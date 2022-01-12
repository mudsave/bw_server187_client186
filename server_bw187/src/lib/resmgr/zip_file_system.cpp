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

#include "bwresource.hpp"
#include "zip_file_system.hpp"
#include "zip/zlib.h"
#include "cstdmf/debug.hpp"


DECLARE_DEBUG_COMPONENT2( "ResMgr", 0 )

#ifdef _WIN32
#define PACKED
#pragma pack(1)
#else
#define PACKED __attribute__((packed))
#endif

#include <algorithm>
#define conformSlash(x) (x)

const std::string adjustCase( const std::string& str )
{
#ifdef _WIN32
	char* lwrStr = new char[ str.length() + 1 ];
	strcpy( lwrStr, str.c_str() );
	_strlwr( lwrStr );
	std::string retStr = lwrStr;
	delete [] lwrStr;
	return retStr;
#else
	return str;
#endif
}


/**
 *	This enumeration contains constants related to Zip files.
 */
enum
{
	DIR_FOOTER_SIGNATURE 	= 0x06054b50,
	DIR_ENTRY_SIGNATURE		= 0x02014b50,
	LOCAL_HEADER_SIGNATURE	= 0x04034b50,

	MAX_FIELD_LENGTH		= 1024,

	METHOD_STORE = 0,
	METHOD_DEFLATE = 8
};

/**
 *	This structure represents the footer at the end of a Zip file.
 *	Actually, a zip file may contain a comment after the footer,
 *	but for our purposes we can assume it will not.
 */
struct DirFooter
{
	unsigned long	signature PACKED;
	unsigned short	diskNumber PACKED;
	unsigned short	dirStartDisk PACKED;
	unsigned short	dirEntries PACKED;
	unsigned short	totalDirEntries PACKED;
	unsigned long	dirSize PACKED;
	unsigned long	dirOffset PACKED;
	unsigned short	commentLength PACKED;
};

/**
 *	This structure represents an entry in the directory table at the
 *	end of a zip file.
 */
struct DirEntry
{
	unsigned long	signature PACKED;
	unsigned short	creatorVersion PACKED;
	unsigned short	extractorVersion PACKED;
	unsigned short	mask PACKED;
	unsigned short	compressionMethod PACKED;
	unsigned short	modifiedTime PACKED;
	unsigned short	modifiedDate PACKED;
	unsigned long	crc32 PACKED;
	unsigned long	compressedSize PACKED;
	unsigned long	uncompressedSize PACKED;
	unsigned short	filenameLength PACKED;
	unsigned short	extraFieldLength PACKED;
	unsigned short	fileCommentLength PACKED;
	unsigned short	diskNumberStart PACKED;
	unsigned short	internalFileAttr PACKED;
	unsigned long	externalFileAttr PACKED;
	long			localHeaderOffset PACKED;
};

/**
 *	This structure represents the header that appears directly before
 *	every file within a zip file.
 */
struct LocalHeader
{
	unsigned long	signature PACKED;
	unsigned short	extractorVersion PACKED;
	unsigned short	mask PACKED;
	unsigned short	compressionMethod PACKED;
	unsigned short	modifiedTime PACKED;
	unsigned short	modifiedDate PACKED;
	unsigned long	crc32 PACKED;
	unsigned long	compressedSize PACKED;
	unsigned long	uncompressedSize PACKED;
	unsigned short	filenameLength PACKED;
	unsigned short	extraFieldLength PACKED;
};

/**
 *	This is the constructor.
 *
 *	@param zipFile		Path of the zip file to open
 */
ZipFileSystem::ZipFileSystem( const std::string& zipFile ) :
	path_( zipFile ),
	pFile_(NULL)
{
	SimpleMutexHolder mtx( mutex_ );
	this->openZip(zipFile);
}

/**
 *	This is the destructor.
 */
ZipFileSystem::~ZipFileSystem()
{
	SimpleMutexHolder mtx( mutex_ );
	this->closeZip();
}

/**
 *	This method opens a zipfile, and reads the directory.
 *
 *	@param path		Path of the zipfile.
 *
 *	@return True if successful.
 */
bool ZipFileSystem::openZip(const std::string& path)
{
	std::string filename, dirComponent, fileComponent;
	DirFooter footer;
	DirEntry entry;
	Directory blankDir;
	char buf[MAX_FIELD_LENGTH];
	unsigned long i, offset;
	int pos;

	// Add a directory node for the root directory.
	dirMap_[""] = blankDir;

	if(!(pFile_ = fopen(conformSlash(path).c_str(), "rb")))
	{
		return false;
	}

	if(fseek(pFile_, -(int)sizeof(footer), SEEK_END) != 0)
	{
		ERROR_MSG("ZipFileSystem::openZip Failed to seek to footer (opening %s)\n",
			path.c_str());
		this->closeZip();
		return false;
	}

	if(fread(&footer, sizeof(footer), 1, pFile_) != 1)
	{
		ERROR_MSG("ZipFileSystem::openZip Failed to read footer (opening %s)\n",
			path.c_str());
		this->closeZip();
		return false;
	}

	if(footer.signature != DIR_FOOTER_SIGNATURE)
	{
		ERROR_MSG("ZipFileSystem::openZip Invalid footer signature (opening %s)\n",
			path.c_str());
		this->closeZip();
		return false;
	}

	if(fseek(pFile_, footer.dirOffset, SEEK_SET) != 0)
	{
		ERROR_MSG("ZipFileSystem::openZip Failed to seek to directory start (opening %s)\n",
			path.c_str());
		this->closeZip();
		return false;
	}

	offset = footer.dirOffset;

	for(i = 0; i < footer.totalDirEntries; i++)
	{
		if(fread(&entry, sizeof(entry), 1, pFile_) != 1)
		{
			ERROR_MSG("ZipFileSystem::openZip Failed to read directory entry (opening %s)\n",
				path.c_str());
			this->closeZip();
			return false;
		}

		if(entry.signature != DIR_ENTRY_SIGNATURE)
		{
			ERROR_MSG("ZipFileSystem::openZip Invalid directory signature (opening %s)\n",
				path.c_str());
			this->closeZip();
			return false;
		}

		if(entry.filenameLength > MAX_FIELD_LENGTH)
		{
			ERROR_MSG("ZipFileSystem::openZip Filename is too long (opening %s)\n",
				path.c_str());
			this->closeZip();
			return false;
		}

		if(entry.extraFieldLength > MAX_FIELD_LENGTH)
		{
			ERROR_MSG("ZipFileSystem::openZip Extra field is too long (opening %s)\n",
				path.c_str());
			this->closeZip();
			return false;
		}

		if(entry.fileCommentLength > MAX_FIELD_LENGTH)
		{
			ERROR_MSG("ZipFileSystem::openZip File comment is too long (opening %s)\n",
				path.c_str());
			this->closeZip();
			return false;
		}

		fread(buf, entry.filenameLength, 1, pFile_);
		filename.assign(buf, entry.filenameLength);
		std::replace(filename.begin(), filename.end(), '\\', '/');
		fread(buf, entry.extraFieldLength, 1, pFile_);
		fread(buf, entry.fileCommentLength, 1, pFile_);

		if(filename[filename.length() - 1] == '/')
		{
			filename.erase(filename.length() - 1);
			dirMap_[ adjustCase( filename ) ] = blankDir;
		}

		fileMap_[ adjustCase( filename ) ] = entry.localHeaderOffset;
		pos = filename.rfind('/');

		if(pos == -1)
		{
			dirComponent = "";
			fileComponent = filename;
		}
		else
		{
			dirComponent = filename.substr(0, pos);
			fileComponent = filename.substr(pos + 1);
		}

		DirMap::iterator dirIter = dirMap_.find( adjustCase( dirComponent ) );

		if(dirIter == dirMap_.end())
		{
			ERROR_MSG("ZipFileSystem::openZip Failed to find directory %s (opening %s)\n",
				dirComponent.c_str(), path.c_str());
		}
		else
		{
			dirIter->second.push_back(fileComponent);
		}

		offset += sizeof(entry);
		offset += entry.filenameLength;
		offset += entry.extraFieldLength;
		offset += entry.fileCommentLength;
	}

	return true;
}

/**
 *	This method reads the contents of a file through readFileInternal.
 *
 *	@param path		Path relative to the base of the filesystem.
 *
 *	@return A BinaryBlock object containing the file data.
 */
BinaryPtr ZipFileSystem::readFile(const std::string& path)
{
	BWResource::checkAccessFromCallingThread( path, "ZipFileSystem::readFile" );

	SimpleMutexHolder mtx( mutex_ );
	return readFileInternal( path );
}

/**
 *	This method reads the contents of a file. It is not thread safe.
 *
 *	@param path		Path relative to the base of the filesystem.
 *
 *	@return A BinaryBlock object containing the file data.
 */
BinaryPtr ZipFileSystem::readFileInternal(const std::string& path)
{
	std::string path2 = path;
	std::replace(path2.begin(), path2.end(), '\\', '/');

	if(path2[0] == '/')
		path2.erase(0,1 );

	LocalHeader hdr;
	char *pCompressed = NULL, *pUncompressed = NULL;
	FileMap::iterator it = fileMap_.find( adjustCase( path2 ) );
	BinaryPtr pBinaryBlock;
	int r;

	if (it == fileMap_.end())
		return static_cast<BinaryBlock *>( NULL );

	if(fseek(pFile_, it->second, SEEK_SET) != 0)
	{
		ERROR_MSG("ZipFileSystem::readFile Failed to seek to local header (%s in %s)\n",
			path.c_str(), path_.c_str());
		return static_cast<BinaryBlock *>( NULL );
	}

	if(fread(&hdr, sizeof(hdr), 1, pFile_) != 1)
	{
		ERROR_MSG("ZipFileSystem::readFile Failed to read local header (%s in %s)\n",
			path.c_str(), path_.c_str());
		return static_cast<BinaryBlock *>( NULL );
	}

	if(hdr.signature != LOCAL_HEADER_SIGNATURE)
	{
		ERROR_MSG("ZipFileSystem::readFile Invalid local header signature (%s in %s)\n",
			path.c_str(), path_.c_str());
		return static_cast<BinaryBlock *>( NULL );
	}

	if(fseek(pFile_, hdr.filenameLength + hdr.extraFieldLength,
		SEEK_CUR) != 0)
	{
		ERROR_MSG("ZipFileSystem::readFile Failed to seek to data (%s in %s)\n",
			path.c_str(), path_.c_str());
		return static_cast<BinaryBlock *>( NULL );
	}

	if(hdr.compressionMethod != METHOD_STORE &&
		hdr.compressionMethod != METHOD_DEFLATE)
	{
		ERROR_MSG("ZipFileSystem::readFile Compression method %d not yet supported (%s in %s)\n",
			hdr.compressionMethod, path.c_str(), path_.c_str());
		return static_cast<BinaryBlock *>( NULL );
	}

	if(!(pCompressed = new char[hdr.compressedSize]))
	{
		ERROR_MSG("ZipFileSystem::readFile Failed to alloc data buffer (%s in %s)\n",
			path.c_str(), path_.c_str());
		return static_cast<BinaryBlock *>( NULL );
	}

	if(fread(pCompressed, 1, hdr.compressedSize, pFile_) != hdr.compressedSize)
	{
		ERROR_MSG("ZipFileSystem::readFile Data read error (%s in %s)\n",
			path.c_str(), path_.c_str());
		delete[] pCompressed;
		return static_cast<BinaryBlock *>( NULL );
	}

	if(hdr.compressionMethod == METHOD_DEFLATE)
	{
		unsigned long uncompressedSize = hdr.uncompressedSize;
		z_stream zs;
		memset( &zs, 0, sizeof(zs) );

		if(!(pUncompressed = new char[hdr.uncompressedSize]))
		{
			ERROR_MSG("ZipFileSystem::readFile Failed to alloc data buffer (%s in %s)\n",
				path.c_str(), path_.c_str());
			delete[] pCompressed;
			return static_cast<BinaryBlock *>( NULL );
		}

		// Note that we dont use the uncompress wrapper function in zlib,
		// because we need to pass in -MAX_WBITS to inflateInit2_ as the
		// window size. This is an "undocumented feature" in zlib that
		// disables the zlib header. This is what we want, since zip files
		// don't contain zlib headers.

		if(inflateInit2_(&zs, -MAX_WBITS, ZLIB_VERSION,
			sizeof(z_stream)) != Z_OK)
		{
			ERROR_MSG("ZipFileSystem::readFile inflateInit2 failed (%s in %s)\n",
				path.c_str(), path_.c_str());
			delete[] pCompressed;
			delete[] pUncompressed;
			return static_cast<BinaryBlock *>( NULL );
		}

		zs.next_in = (unsigned char *)pCompressed;
		zs.avail_in = hdr.compressedSize;
		zs.next_out = (unsigned char *)pUncompressed;
		zs.avail_out = uncompressedSize;
		zs.zalloc = NULL;
		zs.zfree = NULL;

		if((r = inflate(&zs, Z_FINISH)) != Z_STREAM_END)
		{
			ERROR_MSG("ZipFileSystem::readFile Decompression error %d (%s in %s)\n", r,
				path.c_str(), path_.c_str());
			delete[] pCompressed;
			delete[] pUncompressed;
			inflateEnd(&zs);
			return static_cast<BinaryBlock *>( NULL );
		}

		inflateEnd(&zs);
		pBinaryBlock = new BinaryBlock(pUncompressed, hdr.uncompressedSize);
		//HACK_MSG("Read compressed file %s\n", path.c_str());
	}
	else
	{
		pBinaryBlock = new BinaryBlock(pCompressed, hdr.compressedSize);
	}

	delete[] pCompressed;
	delete[] pUncompressed;
	return pBinaryBlock;
}

/**
 *	This method closes the zip file and frees all resources associated
 *	with it.
 */
void ZipFileSystem::closeZip()
{
	if(pFile_)
	{
		fclose(pFile_);
		pFile_ = NULL;
	}

	fileMap_.clear();
	dirMap_.clear();
}

/**
 *	This method reads the contents of a directory.
 *
 *	@param path		Path relative to the base of the filesystem.
 *
 *	@return A vector containing all the filenames in the directory.
 */
IFileSystem::Directory ZipFileSystem::readDirectory(const std::string& path)
{
	BWResource::checkAccessFromCallingThread( path, "ZipFileSystem::readDirectory" );

	SimpleMutexHolder mtx( mutex_ );
	std::string path2 = path;
	std::replace(path2.begin(), path2.end(), '\\', '/');

	if(path2[0] == '/')
		path2.erase(0, 1);

	DirMap::iterator it = dirMap_.find( adjustCase( path2 ) );

	if(it == dirMap_.end())
	{
		Directory blankDir;
		return blankDir;
	}

	return it->second;
}

/**
 *	This method gets the file type, and optionally additional info.
 *
 *	@param path		Path relative to the base of the filesystem.
 *	@param pFI		If not NULL, this is set to the type of this file.
 *
 *	@return File type enum.
 */
IFileSystem::FileType ZipFileSystem::getFileType(const std::string& path,
	FileInfo * pFI )
{
	BWResource::checkAccessFromCallingThread( path, "ZipFileSystem::getFileType" );

	SimpleMutexHolder mtx( mutex_ );
	std::string path2 = path;
	std::replace(path2.begin(), path2.end(), '\\', '/');

	if(path2[0] == '/')
		path2.erase(0, 1);

	FileMap::iterator ffound = fileMap_.find( adjustCase( path2 ) );
	if (ffound == fileMap_.end())
		return FT_NOT_FOUND;

	FileType ft = (dirMap_.find( adjustCase( path2 ) ) != dirMap_.end()) ?
		FT_DIRECTORY : FT_FILE;

	if (pFI != NULL)
	{
		LocalHeader hdr;

		if (fseek(pFile_, ffound->second, SEEK_SET) != 0)
		{
			ERROR_MSG("ZipFileSystem::getFileType Failed to seek to local header (%s in %s)\n",
				path.c_str(), path_.c_str());
			return FT_NOT_FOUND;
		}

		if (fread(&hdr, sizeof(hdr), 1, pFile_) != 1)
		{
			ERROR_MSG("ZipFileSystem::getFileType Failed to read local header (%s in %s)\n",
				path.c_str(), path_.c_str());
			return FT_NOT_FOUND;
		}

		if (hdr.signature != LOCAL_HEADER_SIGNATURE)
		{
			ERROR_MSG("ZipFileSystem::getFileType Invalid local header signature (%s in %s)\n",
				path.c_str(), path_.c_str());
			return FT_NOT_FOUND;
		}

		pFI->size = hdr.uncompressedSize;
		uint32 alltime =
			uint32(hdr.modifiedDate) << 16 | uint32(hdr.modifiedTime);
		pFI->created = alltime;
		pFI->modified = alltime;
		pFI->accessed = alltime;
		// TODO: Look for NTFS/Unix extra fields
	}

	return ft;
}

extern const char * g_daysOfWeek[];
extern const char * g_monthsOfYear[];

/**
 *	Translate an event time from a zip file to a string
 */
std::string	ZipFileSystem::eventTimeToString( uint64 eventTime )
{
	uint16 zDate = uint16(eventTime>>16);
	uint16 zTime = uint16(eventTime);

	char buf[32];
	sprintf( buf, "%s %s %02d %02d:%02d:%02d %d",
		"Unk", g_monthsOfYear[(zDate>>5)&15], zDate&31,
		zTime>>11, (zTime>>5)&15, (zTime&31)*2, 1980 + (zDate>>9) );
	return buf;
}


/**
 *	This method creates a new directory.
 *
 *	@param path		Path relative to the base of the filesystem.
 *
 *	@return True if successful
 */
bool ZipFileSystem::makeDirectory(const std::string& /*path*/)
{
	return false;
}

/**
 *	This method writes a file to disk.
 *
 *	@param path		Path relative to the base of the filesystem.
 *	@param pData	Data to write to the file
 *	@param binary	Write as a binary file (Windows only)
 *	@return	True if successful
 */
bool ZipFileSystem::writeFile(const std::string& /*path*/,
		BinaryPtr /*pData*/, bool /*binary*/)
{
	return false;
}

/**
 *	This method moves a file around.
 *
 *	@param oldPath		The path to move from.
 *	@param newPath		The path to move to.
 *
 *	@return	True if successful
 */
bool ZipFileSystem::moveFileOrDirectory( const std::string & oldPath,
	const std::string & newPath )
{
	return false;
}


/**
 *	This method erases a file or directory.
 *
 *	@param path		Path relative to the base of the filesystem.
 *	@return	True if successful
 */
bool ZipFileSystem::eraseFileOrDirectory( const std::string & path )
{
	return false;
}


/**
 *	This method opens a file using POSIX semantics. It first extracts the file
 *  from the Zip file, and then saves it to a temporary file
 *
 *	@param path		Path relative to the base of the filesystem.
 *	@param mode		The mode that the file should be opened in.
 *	@return	True if successful
 */
FILE * ZipFileSystem::posixFileOpen( const std::string& path,
		const char * mode )
{
	SimpleMutexHolder mtx( mutex_ );
#ifdef _WIN32

	char buf[MAX_PATH+1];
	char fname[MAX_PATH+1];

	GetTempPath( MAX_PATH, buf );
	GetTempFileName( buf, "bwt", 0, fname );

	FILE * pFile = fopen( fname, "w+bD" );

	if (!pFile)
	{
		ERROR_MSG("ZipFileSystem::posixFileOpen: Unable to create temp file (%s in %s)\n",
			path.c_str(), path_.c_str());
		return NULL;
	}

	BinaryPtr bin = readFileInternal( path );
	if ( !bin )
	{
		fclose( pFile );
		return NULL;
	}

	fwrite( bin->cdata(), 1, bin->len(), pFile );
	fseek( pFile, 0, SEEK_SET );
	return pFile;

#else // _WIN32

	// TODO: Unix guys should use fmemopen instead here.
	FILE * pFile = tmpfile();

	if (!pFile)
	{
		ERROR_MSG("ZipFileSystem::posixFileOpen: Unable to create temp file (%s in %s)\n",
			path.c_str(), path_.c_str());
		return NULL;
	}

	BinaryPtr bin = readFileInternal( path );
	if ( !bin )
	{
		fclose( pFile );
		return NULL;
	}

	fwrite( bin->cdata(), 1, bin->len(), pFile );
	fseek( pFile, 0, SEEK_SET );
	return pFile;

#endif // _WIN32
}

/**
 *	This method returns a IFileSystem pointer to a copy of this object.
 *
 *	@return	a copy of this object
 */
IFileSystem* ZipFileSystem::clone()
{
	return new ZipFileSystem( path_ );
}

// zip_file_system.cpp
