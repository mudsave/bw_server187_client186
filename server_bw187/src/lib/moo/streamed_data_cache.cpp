/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/


#include "pch.hpp"
#include "resmgr/multi_file_system.hpp"
#include "resmgr/bwresource.hpp"
#include "cstdmf/diary.hpp"
#include "cstdmf/memory_counter.hpp"
#include "cstdmf/binaryfile.hpp"
#include "streamed_data_cache.hpp"

#include <io.h>

DECLARE_DEBUG_COMPONENT2( "Moo", 0 )

/** 
 *	The SEPARATE_READ_THREAD token specifies whether streamed animations
 *	should use a separate thread for file read operations.
 *
 *	The previous version of this class used window's overlapped (asyncronous)
 *  file access, but has been replaced with the BWResource file system posix
 *  file methods.
 */
#define SEPARATE_READ_THREAD	1

namespace Moo
{

#ifdef SEPARATE_READ_THREAD
// -----------------------------------------------------------------------------
// Section: ReadThread
// -----------------------------------------------------------------------------

#include "process.h"


#define READREQMAX 32
static HANDLE s_readThread = INVALID_HANDLE_VALUE;

/**
 * This structure holds a read request for a file.
 */
struct ReadReq
{
	FILE* file;
	void * data;
	uint32 size;
	StreamedDataCache::Tracker* tracker;
};
static ReadReq readReqs[READREQMAX];
uint readReqIn = 0;
uint readReqOut = 0;
SimpleMutex readReqMtx;
SimpleSemaphore	readReqListIn;
SimpleSemaphore	readReqListOut;

void addReadReq( const ReadReq & rr )
{
	readReqListIn.pull();

	readReqs[ (readReqIn++) & (READREQMAX-1) ] = rr;

	readReqListOut.push();
}


void __cdecl read_start( void * arg )
{
	for (uint i = 0; i < READREQMAX; i++) readReqListIn.push();

	while (1)
	{
		readReqListOut.pull();

		ReadReq & rr = readReqs[ (readReqOut++) & (READREQMAX-1) ];
		readReqMtx.grab();
		rr.tracker->finished_ = 0;
		readReqMtx.give();

		clearerr( rr.file );
		fseek( rr.file, rr.tracker->offset_, SEEK_SET );
		DWORD numRead = fread( rr.data, 1, rr.size, rr.file );
		if ( !ferror( rr.file ) )
		{
			readReqMtx.grab();
			rr.tracker->finished_ = 1;
			readReqMtx.give();
		}
		else
		{
			ERROR_MSG( "StreamedDataCache::load: "
				"Read error: (off 0x%08X siz %d data 0x%08X)\n",
				rr.tracker->offset_, rr.size, rr.data );

			readReqMtx.grab();
			rr.tracker->finished_ = -1;
			readReqMtx.give();
		}

		readReqListIn.push();
	}
}


#endif // SEPARATE_READ_THREAD

// -----------------------------------------------------------------------------
// Section: StreamedDataCache
// -----------------------------------------------------------------------------

bool StreamedDataCache::fileAccessDone( Tracker* tracker )
{
#ifdef SEPARATE_READ_THREAD
	readReqMtx.grab();
	bool result = tracker->finished_ != 0;
	readReqMtx.give();
	return result;
#else
	tracker->finished_ = 1;
	return true;
#endif // !SEPARATE_READ_THREAD
}

/**
 *	Constructor
 */
StreamedDataCache::StreamedDataCache( const std::string & resourceID,
		bool createIfAbsent ) :
	lastEntry_( NULL ),
	deleteOnClose_( false )
{
	memoryCounterAdd( animation );
	memoryClaim( this );

#ifdef SEPARATE_READ_THREAD
	if (s_readThread == INVALID_HANDLE_VALUE)
		s_readThread = HANDLE( _beginthread( read_start, 0, NULL ) );
#endif

	fileName_ = resourceID;

	memoryClaim( fileName_ );
	memoryClaim( directory_ );

	// first try r/w for an existing
	file_ = BWResource::instance().fileSystem()->posixFileOpen( fileName_.c_str(), "r+b" );
	if ( createIfAbsent )
	{
		// if it didn't work, try to create the file and open it
		if (!file_)
			file_ = BWResource::instance().fileSystem()->posixFileOpen( fileName_.c_str(), "w+b" );
	}
	// if it didn't work still, then try read-only
	if (!file_)
		file_ = BWResource::instance().fileSystem()->posixFileOpen( fileName_.c_str(), "rb" );
 
	// ok, we've opened the file.
	// now read in the directory
	if (file_)
		this->loadSelf();
}

/**
 *	Destructor
 */
StreamedDataCache::~StreamedDataCache()
{
	if (file_)
	{
		fclose( file_ );
	}

	if (deleteOnClose_)
	{
		BWResource::instance().fileSystem()->eraseFileOrDirectory( fileName_ );
	}

	memoryCounterSub( animation );
	while (!directory_.empty())
	{
		memoryClaim( directory_, directory_.begin() );
		directory_.erase( directory_.begin() );
	}
	memoryClaim( directory_ );
	memoryClaim( fileName_ );
	memoryClaim( this );
}


/**
 *	Find the data for the given entry name.
 *
 *	If either minVersion or minModified is nonzero, then the version
 *	or modification time must be at least the value passed in.
 */
const StreamedDataCache::EntryInfo * StreamedDataCache::findEntryData(
	const std::string & name, uint32 minVersion, uint64 minModified )
{
	EntryInfos::iterator found = directory_.find( name );
	if (found != directory_.end())
	{
		if (minVersion == 0 || minVersion <= found->second.version_)
		{
			if (minModified == 0 || minModified <= found->second.modified_)
			{
				return &found->second;
			}
		}
	}
	
	return NULL;
}

/**
 *	Add an entry under the given name.
 *	Returns the offset at which to begin writing.
 */
uint32 StreamedDataCache::addEntryData( const std::string & name,
	uint32 version, uint64 modified )
{
	uint32 dataEnd = (lastEntry_ != NULL) ? lastEntry_->offset_ +
		((lastEntry_->psize_+lastEntry_->ssize_ + 3) & ~3L) : 0;

	EntryInfo * ei = const_cast<EntryInfo*>(this->findEntryData( name, 0, 0 ));
	if (ei == NULL)
	{
		// it's really easy if there's not an entry already there
		EntryInfo entryInfo = { 0, 0, dataEnd, version, modified };
		lastEntry_ = &directory_.insert(
			std::make_pair( name, entryInfo ) ).first->second;
	}
	else
	{
		INFO_MSG( "StreamedDataCache::addEntryData: "
			"Replacing existing entry %s in cache.\n", name.c_str() );

		// first move up all the data
		const uint32 SHUFFLE_BUFFER_SIZE = 256*1024;
		char * dataBuffer = new char[SHUFFLE_BUFFER_SIZE];

		uint32 oldLen = ((ei->psize_+ei->ssize_ + 3) & ~3L);
		uint32 dataOffset = ei->offset_ + oldLen;
		while (dataOffset < dataEnd)
		{
			uint32 thisLen = std::min( dataEnd - dataOffset, SHUFFLE_BUFFER_SIZE );
			this->preload( dataOffset, thisLen, dataBuffer );
			this->immsave( dataOffset - oldLen, thisLen, dataBuffer );
			dataOffset += thisLen;
		}
		dataEnd -= oldLen;

		delete [] dataBuffer;

		// now fix up all the entry infos after us
		for (EntryInfos::iterator it = directory_.begin();
			it != directory_.end();
			it++)
		{
			EntryInfo * aei = &it->second;
			if (aei->offset_ > ei->offset_) aei->offset_ -= oldLen;
		}

		// and finally fix up our entry info
		ei->psize_ = 0;
		ei->ssize_ = 0;
		ei->offset_ = dataEnd;
		ei->version_ = version;
		ei->modified_ = modified;

		lastEntry_ = ei;	// we are now the last entry
	}

	return dataEnd;
}

/**
 *	Mark the end of the data for a newly added entry.
 *	The argument is the size of the entry data written.
 */
void StreamedDataCache::endEntryData( uint32 psize, uint32 ssize )
{
	lastEntry_->psize_ = psize;
	lastEntry_->ssize_ = ssize;
}


/**
 *	Load the given data spec immediately
 */
bool StreamedDataCache::preload( uint32 offset, uint32 size, void * data )
{
	Tracker* tracker = this->load( offset, size, data );
	while ( !fileAccessDone( tracker ) )
		Sleep( 5 );
	bool result = ( tracker->finished_ == 1 );
	delete tracker;
	return result;
}


/**
 *	Save the given data spec immediately
 */
bool StreamedDataCache::immsave( uint32 offset, uint32 size, const void * data )
{
	Tracker* tracker = this->save( offset, size, data );
	while ( !fileAccessDone( tracker ) )
		Sleep( 5 );
	bool result = ( tracker->finished_ == 1 );
	delete tracker;
	return result;
}


/**
 *	Start loading the given data spec
 */
StreamedDataCache::Tracker* StreamedDataCache::load( uint32 offset, uint32 size, void * data )
{
	//	DiaryEntry & deSLA = Diary::instance().add( "slA" );
	Tracker* tracker = new Tracker( offset );
//	deSLA.stop();

//	DiaryEntry & deSLB = Diary::instance().add( "slB" );
	DWORD numRead = 0;
	BOOL ok;
#ifdef SEPARATE_READ_THREAD
	ReadReq rr = { file_, data, size, tracker };
	addReadReq( rr );
	ok = true;
#else	
	clearerr( file_ );
	fseek( file_, offset, SEEK_SET );
	numRead = fread( data, 1, size, file_ );
	ok = !ferror( file_ );
#endif	//!SEPARATE_READ_THREAD
//	deSLB.stop();

	if ( ok )
	{
		return tracker;
	}
	else
	{
		clearerr( file_ );
		ERROR_MSG( "StreamedDataCache::load: "
			"Read error (off 0x%08X siz %d data 0x%08X)\n",
			offset, size, data );

		delete tracker;
		return NULL;
	}
}

/**
 *	Start saving the given data spec
 */
StreamedDataCache::Tracker* StreamedDataCache::save(
	uint32 offset, uint32 size, const void * data )
{
	DWORD numSaved = 0;

	clearerr( file_ );
	fseek( file_, offset, SEEK_SET );
	numSaved = fwrite( data, 1, size, file_ );
	if (!ferror( file_ ))
	{
		return new Tracker( offset, 1 );
	}
	else
	{
		clearerr( file_ );
		ERROR_MSG( "StreamedDataCache::save: "
			"Write error\n" );

		return false;
	}
}


/**
 *	Read in our directory
 */
void StreamedDataCache::loadSelf()
{
	// (note that this is in exactly the same format as a PrimitiveFile)
	long oldPos = ftell( file_ );
	fseek( file_, 0, SEEK_END );
	long len = ftell( file_ );
	fseek( file_, oldPos, SEEK_SET );
	if (len == 0)
	{
		// new empty file
		lastEntry_ = NULL;
		return;
	}

	memoryCounterAdd( animation );

	uint32 dirLen = 0;
	this->preload( len-sizeof(dirLen), sizeof(dirLen), &dirLen );

	char * dirBuf = new char[dirLen];
	this->preload( len-sizeof(dirLen)-dirLen, dirLen, dirBuf );

	uint32 entryDataOffset = 0;
	uint32 offset = 0;
	while (offset < dirLen)
	{
		// read this directory entry
		uint32 entryDataLen = *(uint32*)(dirBuf+offset);
		offset += sizeof(uint32);
		uint32 entryPreloadLen = 0;
		uint32 entryVersion = 0;
		uint64 entryModified = 0;
		if (entryDataLen & (1<<31))
		{
			entryDataLen &= ~(1<<31);
			entryPreloadLen = *(uint32*)(dirBuf+offset);
			offset += sizeof(uint32);
			entryVersion = *(uint32*)(dirBuf+offset);
			offset += sizeof(uint32);
			entryModified = *(uint64*)(dirBuf+offset);
			offset += sizeof(uint64);
		}
		uint32 entryNameLen = *(uint32*)(dirBuf+offset);
		offset += sizeof(uint32);
		std::string entryStr( dirBuf+offset, entryNameLen );
		offset += (entryNameLen + 3) & ~3L;

		// insert it into our map
		EntryInfo entryInfo = { entryPreloadLen, entryDataLen - entryPreloadLen,
			entryDataOffset, entryVersion, entryModified };
		EntryInfos::iterator it = directory_.insert(
			std::make_pair( entryStr, entryInfo ) ).first;
		memoryClaim( it->first );
		lastEntry_ = &it->second;
		memoryClaim( directory_, it );

		// move on the data offset
		entryDataOffset += (entryDataLen + 3) & (~3L);
	}
	
	// If the entry data is not valid, we start afresh
	if (!validateEntryData(len != 0 ? (len - sizeof(dirLen) - dirLen) : 0 ))
	{
		directory_.clear();
		lastEntry_ = NULL;
	}

	//dataEnd_ = entryDataOffset;

	delete [] dirBuf;
}

/**
 *	Write out our directory
 */
void StreamedDataCache::saveSelf()
{
	// figure out the end of our data
	uint32 dataEnd = (lastEntry_ != NULL) ? lastEntry_->offset_ +
		((lastEntry_->psize_+lastEntry_->ssize_ + 3) & ~3L) : 0;

	// sort the directory by increasing order of offset
	std::map< uint32, EntryInfos::iterator > revEntries;
	{
		for (EntryInfos::iterator it = directory_.begin();
			it != directory_.end();
			it++)
		{
			revEntries.insert( std::make_pair( it->second.offset_, it ) );
		}
	}
	// copy everything into a string
	std::string	dirFile;
	std::map< uint32, EntryInfos::iterator >::iterator it, nit;
	for (it = revEntries.begin();
		it != revEntries.end();
		it = nit)
	{
		nit = it;
		nit++;
		EntryInfos::const_iterator eit = it->second;
		const EntryInfo & ei = eit->second;
		uint32 entryDataLen = ei.ssize_ + ei.psize_;
//			((nit != revEntries.end()) ? nit->first : dataEnd) - it->first;
		entryDataLen |= 1 << 31;
		dirFile.append( (char*)&entryDataLen, sizeof(entryDataLen) );
		dirFile.append( (char*)&ei.psize_, sizeof(ei.psize_) );
		dirFile.append( (char*)&ei.version_, sizeof(ei.version_) );
		dirFile.append( (char*)&ei.modified_, sizeof(ei.modified_) );
		uint32 entryNameLen = eit->first.length();
		dirFile.append( (char*)&entryNameLen, sizeof(entryNameLen) );
		dirFile.append( eit->first );
		uint32 zero = 0;	// VC7 dislikes unary minus on unsigned types
		dirFile.append( (char*)&zero, (0-entryNameLen) & (sizeof(zero)-1) );
	}

	uint32 dirLen = dirFile.length();
	dirFile.append( (char*)&dirLen, sizeof(dirLen) );

	// now write it out to the file
	this->immsave( dataEnd, dirFile.length(), dirFile.c_str() );
	fflush( file_ );

	// Make sure we are truncating the file to the size of the data + directory
	// Seek to the right position
	fseek( file_, dataEnd + dirFile.length(), SEEK_SET );

	// Truncate the file, we use win32 functions as there is no posix equivalent
	HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(file_));
	if (hFile != (HANDLE)-1)
	{
		SetEndOfFile(hFile);
	}

}

bool StreamedDataCache::good()
{
	return !!file_;
}

namespace
{
#pragma pack(push,1)
	struct ValidationData
	{
		float totalTime;
		uint32 stringLength;
	};
#pragma pack(pop)
};

/**
 *	This method validates the entry data in the anca file by opening up each 
 *	individiual animation and checking if the header is ok.
 *	It also makes sure all the space is used up from the start of the file
 *	to the start of the header as this can be another sign of corruption.
 *	@param headerStart the location in the anca file for the header
 *	@return true if there was no error, false if an error occured
 */
bool StreamedDataCache::validateEntryData( uint32 headerStart )
{
	// maximum allowed name for the animation in the anca file
	const uint32 MAX_ANIM_NAME_LENGTH = 1024;

	// forward declaration of variable that tells us if the
	// whole file is filled
	bool hitEnd = false;

	// Iterate over the entries in the .anca file
	EntryInfos::const_iterator it = directory_.begin();
	while (it != directory_.end())
	{
		// Calculate the end of this entry's data
		const EntryInfo& ei = it->second;
		uint32 dataEnd = ((ei.offset_ + ei.psize_ + ei.ssize_ + 3) & ~3L);
		
		// If the data goes past the header report an error
		if ( dataEnd > headerStart)
		{
			ERROR_MSG( "StreamedDataCache::validateEntryData: "
				"invalid entry %s, overshooting end of data in file %s by "
				"%d bytes\n", 
				it->first.c_str(), fileName_.c_str(), dataEnd - headerStart );
			return false;
		}
		// If the data goes up to the header we assume that we are utilising
		// all the data in the .anca file
		else if (headerStart == dataEnd)
		{
			hitEnd = true;
		}

		// Validate the start of the animation record in the .anca file
		ValidationData validationData;
		this->preload( ei.offset_, sizeof(ValidationData), &validationData );

		if (validationData.stringLength > MAX_ANIM_NAME_LENGTH ||
			_isnan(validationData.totalTime))
		{
			ERROR_MSG( "StreamedDataCache::validateEntryData: "
				"invalid entry %s in file %s, corrupted header\n",
				it->first.c_str(), fileName_.c_str() );
			return false;
		}
		++it;
	}
	
	// If we didn't hit the header with all our data, report an error
	if (headerStart != 0 && !hitEnd)
	{
		ERROR_MSG( "StreamedDataCache::validateEntryData - data in %s "
			"not taking up all available space in the file, there is a gap "
			"between the last entry and the file index\n", fileName_.c_str() );
		return false;
	}

	return true;
}

}	// namespace Moo

// streamed_data_cache.cpp
