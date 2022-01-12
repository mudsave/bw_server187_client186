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
#include "chunk_walker.hpp"
#include "space_helpers.hpp"
#include "chunk/base_chunk_space.hpp"
#include "chunk/chunk_space.hpp"
#include "resmgr/bin_section.hpp"
#include "resmgr/binary_block.hpp"

LinearChunkWalker::LinearChunkWalker():
	spaceName_( "" ),
	width_( 0 ),
	height_( 0 ),
	x_( 0 ),
	z_( 0 ),
	localToWorld_(0,0)
{
}


void LinearChunkWalker::spaceInformation( const std::string& spaceName, const GridCoord& localToWorld, uint16 width, uint16 height )
{
	//reset the chunk walker.
	spaceName_ = spaceName;
	localToWorld_ = localToWorld;
	width_ = width;
	height_ = height;
	this->reset();
}


void LinearChunkWalker::reset()
{
	x_ = z_ = 0;
}


bool LinearChunkWalker::nextTile( std::string& retChunkName, int16& gridX, int16& gridZ )
{
	if ( z_ == height_ )
		return false;

	::offsetGrid( localToWorld_, x_, z_, gridX, gridZ );
	::chunkID( retChunkName, gridX, gridZ );

	x_++;
	if ( x_ == width_ )
	{
		x_ = 0;
		z_++;
	}

	return true;
}



CacheChunkWalker::CacheChunkWalker():
localToWorld_(0,0),
width_(0),
height_(0)
{
}


void CacheChunkWalker::spaceInformation( const std::string& spaceName, const GridCoord& localToWorld, uint16 width, uint16 height )
{
	localToWorld_ = localToWorld;
	width_ = width;
	height_ = height;
}



bool CacheChunkWalker::nextTile( std::string& chunkName, int16& gridX, int16& gridZ )
{
	if (xs_.size()==0)
		return false;

	int i = xs_.size() - 1;

	gridX = xs_[i];
	gridZ = zs_[i];
	chunkID( chunkName, gridX, gridZ );

	xs_.pop_back();
	zs_.pop_back();

	return true;
}


void CacheChunkWalker::add( Chunk* pChunk )
{
	int16 gridX, gridZ;
	::gridFromChunk( pChunk, gridX, gridZ );
	this->add( gridX, gridZ );
}


void CacheChunkWalker::add( int16& gridX, int16& gridZ )
{
	xs_.push_back( gridX );
	zs_.push_back( gridZ );
}


bool CacheChunkWalker::added( const Chunk* pChunk ) const
{
	int16 gridX, gridZ;
	::gridFromChunk( pChunk, gridX, gridZ );
    return added(gridX, gridZ );
}


bool CacheChunkWalker::added( int16 gridX, int16 gridZ ) const
{
    for (size_t i = 0; i < xs_.size(); ++i)
    {
        if ((int16)xs_[i] == gridX && (int16)zs_[i] == gridZ)
            return true;
    }
    return false;
}


bool CacheChunkWalker::erase( const Chunk* pChunk )
{
	int16 gridX, gridZ;
	::gridFromChunk( pChunk, gridX, gridZ );
    return erase( gridX, gridZ );
}


bool CacheChunkWalker::erase( int16 gridX, int16 gridZ )
{
    for (size_t i = 0; i < xs_.size(); ++i)
    {
        if ((int16)xs_[i] == gridX && (int16)zs_[i] == gridZ)
        {
            xs_.erase(xs_.begin() + i);
            zs_.erase(zs_.begin() + i);
            return true;
        }
    }
    return false;
}


bool inRange( uint16 tileNum, uint16 minRange, uint16 maxRange )
{
	return ( min( max(tileNum,minRange), maxRange ) == tileNum );
}


void writeRange( DataSectionPtr pSection, uint16 minRange, uint16 maxRange )
{
	if ( minRange > maxRange )
		return;

	DataSectionPtr pSect = pSection->newSection( "tile" );
	if ( minRange == maxRange )
	{
		pSect->writeInt( "min", minRange );
	}
	else
	{
		pSect->writeInt( "min", minRange );
		pSect->writeInt( "max", maxRange );
	}
}


/**
 *	This method writes out a list of all the tiles that we know about.
 *	It converts the grid X,Z to a single tile number and saves them out.
 *	It also concatenates all adjacencies (RLE) because sometimes the
 *	number of entries can be very large ( e.g. 70km space ~ 1/2 million thumbnails )	
 */
void CacheChunkWalker::save( DataSectionPtr pSection )
{
	std::vector<uint16> tileNumbers;
	
	int num = xs_.size();
	for ( int i=0; i<num; i++ )
	{
		uint16 biasedX, biasedZ;
		int16 x = xs_[i];
		int16 z = zs_[i];
		::biasGrid( localToWorld_, x, z, biasedX, biasedZ );
		uint16 tileNum = biasedX + biasedZ*width_;
		tileNumbers.push_back(tileNum);
	}

	if ( num>0 )
		this->save( pSection, tileNumbers );
}


void CacheChunkWalker::save( DataSectionPtr pSection, std::vector<uint16>& modifiedTileNumbers )
{
	//to make the most of our span range algortithm, we sort the tiles.
	std::sort( modifiedTileNumbers.begin(), modifiedTileNumbers.end() );

	pSection->writeInt( "minX", localToWorld_.x );
	pSection->writeInt( "minZ", localToWorld_.y );
	pSection->writeInt( "width", width_ );
	pSection->writeInt( "height", height_ );

	uint16 minRange = 0xffff;
	uint16 maxRange = 0;

	int num = modifiedTileNumbers.size();
	for ( int i=0; i<num; i++ )
	{
		uint16 tileNum = modifiedTileNumbers[i];

		//allow new tiles to be in range, or adjacent to it.
		uint16 adjMin = (minRange>0) ? minRange-1 : (uint16)0;
		if ( ::inRange(tileNum,adjMin,maxRange+1) )
		{
			minRange = min(tileNum,minRange);
			maxRange = max(tileNum,maxRange);
			continue;
		}

		//new tile number doesn't fit in range.
		//write out the previous range.
		//note the writeRange method rejects invalid ranges,
		//which will always happen for the first tile.
		::writeRange( pSection, minRange, maxRange );
		
		//...and begin the new one
		minRange = tileNum;
		maxRange = tileNum;
	}

	//write out the last one.
	::writeRange( pSection, minRange, maxRange );
}


void CacheChunkWalker::load( DataSectionPtr pSection )
{
	int16 offsetX;
	int16 offsetZ;
	uint16 minRange,maxRange;

	std::string chunkName;
	localToWorld_.x = pSection->readInt( "minX", localToWorld_.x );
	localToWorld_.y = pSection->readInt( "minZ", localToWorld_.y );
	width_ = pSection->readInt( "width", width_ );
	height_ = pSection->readInt( "height", height_ );

	int num = pSection->countChildren();
	for ( int i=0; i<num; i++ )
	{
		DataSectionPtr pSect = pSection->openChild(i);
		
		if ( pSect->findChild("min" ) )
		{
			minRange = pSect->readInt("min");
			maxRange = pSect->readInt("max",minRange);
		
			while ( minRange <= maxRange )
			{
				uint16 x = (minRange%width_);
				uint16 z = (minRange/width_);
				::offsetGrid(localToWorld_,x,z,offsetX,offsetZ);
				xs_.push_back(offsetX);
				zs_.push_back(offsetZ);			
				minRange++;
			} 
		}
	}
}


size_t CacheChunkWalker::size() const
{
    return xs_.size();
}


/**
 *	Section - FileChunkWalker
 */
FileChunkWalker::FileChunkWalker( uint16 numIrrelevantPerFind ):
	fileHandle_( INVALID_HANDLE_VALUE ),
	folder_( "" ),
	root_( "" ),
	width_( 0 ),
	height_( 0 ),
	numIrrelevantPerFind_( numIrrelevantPerFind ),
	reentryFolder_( "" ),
	localToWorld_(0,0)
{
}


FileChunkWalker::~FileChunkWalker()
{
	releaseFileHandle();
}


void FileChunkWalker::spaceInformation( const std::string& spaceName, const GridCoord& localToWorld, uint16 width, uint16 height )
{
	ChunkDirMapping* dirMap = BigBang::instance().chunkDirMapping();
	space_ = dirMap->path();
	root_ = BWResource::resolveFilename( space_ );
	folder_ = "";
	localToWorld_ = localToWorld;
	width_ = width;
	height_ = height;
	this->reset();
}


void FileChunkWalker::reset()
{
	this->releaseFileHandle();
	folders_.clear();
	folder_ = "";
	xs_.clear();
	zs_.clear();
}


bool FileChunkWalker::nextTile( std::string& retChunkName, int16& gridX, int16& gridZ )
{
	if ( reentryFolder_ != "" )
	{
		this->findReentryFolder();
	}

	if ( !xs_.size() )
	{
		this->findNextFile();
	}

	if ( xs_.size() )
	{
		gridX = xs_.back();
		gridZ = zs_.back();
		xs_.pop_back();
		zs_.pop_back();
		::chunkID( retChunkName, gridX, gridZ );
		return true;
	}

	return false;
}


void FileChunkWalker::findNextFile()
{
	for ( int i=0; i<numIrrelevantPerFind_; i++ )
	{
		this->advanceFileHandle();
		
		if (fileHandle_ != INVALID_HANDLE_VALUE)
		{
			if ( searchingFiles_ )
			{
				if ( this->isRelevant( findFileData_ ) )
				{
					int16 x,z;
					if (::gridFromThumbnailFileName((unsigned char*)&findFileData_.cFileName[0],x,z))
					{
						xs_.push_back( x );
						zs_.push_back( z );
					}
					return;
				}
			}
			else
			{
				if ( isValidFolder( findFileData_ ) )
				{
					folders_.push_back( folder_ + std::string(findFileData_.cFileName) + "/sep/" );
				}
				return;
			}
		}
	}
}


void FileChunkWalker::releaseFileHandle()
{
	if ( fileHandle_ != INVALID_HANDLE_VALUE )
		FindClose(fileHandle_);
	fileHandle_ = INVALID_HANDLE_VALUE;
}


/**
 *	This special case method is used to kick-start
 *	the chunk walking process off from a given folder.
 *
 *	This allows the file scanning process to be
 *	interruptible, without favouring files towards
 *	the start of the default search.
 */
void FileChunkWalker::findReentryFolder()
{
	this->releaseFileHandle();

	if ( reentryFolder_ == "" )
		return;

	this->findFolder( reentryFolder_ );

	reentryFolder_ = "";
}


//returns true if the two folders match to as many partial paths as exist
//
//e.g.	"n1/n2" and "n1/n2/n3/n4" returns true
//		"n1/n2" and "n1/n3/n2/n4" returns false
bool FileChunkWalker::partialFolderMatch( const std::string& folder1, const std::string& folder2 )
{
	return ( folder1 == folder2.substr(0,folder1.size()) );
}


bool FileChunkWalker::findFolder( const std::string& target )
{
	this->findFolders( "" );

	//Now search the folders just as we would searching for thumbnails.
	while ( folders_.size() )
	{
		std::string folderName = folders_.back();
		folders_.pop_back();

		if ( folderName == target )
		{
			folders_.push_back( folderName );
			return true;
		}

		if ( partialFolderMatch( folderName, target ) )
			this->findFolders( folderName );
	}

	//Bad luck.  No folders in the current folder match our criteria.
	return false;
}


//This method advances the search 
void FileChunkWalker::advanceFileHandle()
{
	std::string fullName;

	//If this is a new search, or we have no more files,
	//advance the file handle to the next folder.
	if (fileHandle_ == INVALID_HANDLE_VALUE )
	{
		if ( folders_.size() )
		{
			folder_ = folders_.back();
			folders_.pop_back();
		}
		else
		{
			folder_ = "";
		}

		searchingFiles_ = true;
		fullName = root_ + folder_ + "*.cdata";
		fileHandle_ = FindFirstFile( fullName.c_str(), &findFileData_ );
		return;
	}

	//Next!
	if ( FindNextFile(fileHandle_,&findFileData_) )
	{
		//found another one ( file or folder ).
		return;
	}
	
	//No more found.
	//If we were doing a file search, begin a folder search.
	this->releaseFileHandle();
	if ( searchingFiles_ )
	{
		searchingFiles_ = false;
		std::string fullName = root_ + folder_ + "*.";
		fileHandle_ = FindFirstFile( fullName.c_str(), &findFileData_ );
	}
}


bool FileChunkWalker::isValidFolder( const WIN32_FIND_DATA& fileInfo )
{
	if ( findFileData_.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
	{
		//don't allow "." and ".." folders.  that be bad.  also don't bother looking in the "CVS" folder
		if ( !findFileData_.cFileName[0] )
			return false;
		if ( !findFileData_.cFileName[1] )
			return false;
		if ( !findFileData_.cFileName[2] )
			return false;
		if ( findFileData_.cFileName[3]  )
		{
			return true;
		}

		//3 letter folder name : could be "CVS" or "sep".
		if ( findFileData_.cFileName[0] == 's' )
			return true;

		//first letter of 3 letter folder name is not "s" - assume it is "CVS"
		return false;
	}
	return false;
}


/**
 *	This method adds all of the folders contained in the given
 *	directory to the folders_ vector.
 */
void FileChunkWalker::findFolders( const std::string& folder )
{
	//Now add all sub-folders to our list of folders to search.
	std::string fullName = root_ + folder + "*.";
	fileHandle_ = FindFirstFile( fullName.c_str(), &findFileData_ );
	while (fileHandle_ != INVALID_HANDLE_VALUE)
	{
		if ( isValidFolder( findFileData_ ) )
		{
			folders_.push_back( folder + std::string(findFileData_.cFileName) + "/" );
		}
		
		if ( !FindNextFile(fileHandle_,&findFileData_) )
		{
			this->releaseFileHandle();
			break;
		}
	}
}


void FileChunkWalker::load()
{
	DataSectionPtr pSection = BWResource::openSection( space_ + "space.localsettings" );
	if ( pSection )
		reentryFolder_ = pSection->readString( "reentryFolder", "" );
	else
		reentryFolder_ = "";
}


void FileChunkWalker::save()
{
	DataSectionPtr pSection = BWResource::openSection( space_ + "space.localsettings", true );
	MF_ASSERT( pSection );
	
	pSection->writeString( "reentryFolder", folder_ );
	pSection->save();
}



/**
 *	Section - ModifiedFileChunkWalker
 */
ModifiedFileChunkWalker::ModifiedFileChunkWalker( uint16 numIrrelevantPerFind ):
FileChunkWalker( numIrrelevantPerFind ),
pLastModified_( NULL ),
cacheName_( "" )
{
}


ModifiedFileChunkWalker::~ModifiedFileChunkWalker()
{
}


void ModifiedFileChunkWalker::spaceInformation( const std::string& spaceName, const GridCoord& localToWorld, uint16 width, uint16 height )
{
	cacheName_ = spaceName + "/space.thumbnail.timestamps";
	FileChunkWalker::spaceInformation( spaceName, localToWorld, width, height );
}


bool ModifiedFileChunkWalker::isRelevant( const WIN32_FIND_DATA& fileInfo )
{
	int16 x,z;
	if (::gridFromThumbnailFileName((unsigned char*)&fileInfo.cFileName[0],x,z))
	{
		std::string pathName = BigBang::instance().chunkDirMapping()->path();
		std::string chunkName;
		chunkID( chunkName, x, z );
		if( ::thumbnailExists( pathName, chunkName ) )
		{
			MF_ASSERT( x < width_ );
			MF_ASSERT( z < height_ );
			uint16 biasedX, biasedZ;
			::biasGrid(localToWorld_,x,z,biasedX,biasedZ);

			uint32 tileNum = biasedX+biasedZ*width_;
			uint64 tileModifyTime = *(uint64*)&fileInfo.ftLastWriteTime;

			if ( cacheModifyTime(tileNum) < tileModifyTime )
			{		
				cacheModifyTime(tileNum, tileModifyTime);
				return true;
			}
		}
	}

	return false;
}


void ModifiedFileChunkWalker::save()
{
	if ( pLastModified_ )
	{
		BinSectionPtr pSection = new BinSection( cacheName_, pLastModified_ );
		pSection->save( cacheName_ );
		FileChunkWalker::save();
	}
}


void ModifiedFileChunkWalker::load()
{
	if ( cacheName_ == "" )
		return;

	FileChunkWalker::load();

	pLastModified_ = BWResource::instance().rootSection()->readBinary( cacheName_ );

	if ( !pLastModified_ )
	{
		pLastModified_ = new BinaryBlock( NULL, (1+width_*height_)*sizeof(uint64) );
		//make sure the first bit of data does not contain ascii "<" and thus
		//be mistaken by the filesystem for text xml.
		uint32* pData = (uint32*)pLastModified_->data();
		pData[0] = 0xffffffff;
		pData[1] = 0xffffffff;
	}
}


uint64 ModifiedFileChunkWalker::cacheModifyTime( uint32 tileNum )
{
	uint64* pData = (uint64*)(pLastModified_->data());
	return pData[tileNum+1];
}


void ModifiedFileChunkWalker::cacheModifyTime( uint32 tileNum, uint64 modTime )
{
	uint64* pData = (uint64*)(pLastModified_->data());
	pData[tileNum+1] = modTime;
}