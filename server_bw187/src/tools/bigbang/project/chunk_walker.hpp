/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#pragma once

#include "grid_coord.hpp"


class   Chunk;


/** 
 *	Interface to a helper class that provides chunk names / grid positions
 *	in any appropriate order.  Used by the space map to update itself.
 */
class IChunkWalker
{
public:
	virtual void reset() = 0;
	virtual void spaceInformation( const std::string& spaceName, const GridCoord& localToWorld, uint16 width, uint16 height ) = 0;
	virtual bool nextTile( std::string& chunkName, int16& gridX, int16& gridZ ) = 0;
};


//This chunk walker creates tile information in a linear fashion.
class LinearChunkWalker : public IChunkWalker
{
public:
	LinearChunkWalker();
	void reset();
	void spaceInformation( const std::string& spaceName, const GridCoord& localToWorld, uint16 width, uint16 height );
	bool nextTile( std::string& chunkName, int16& gridX, int16& gridZ );
private:
	std::string spaceName_;
	GridCoord localToWorld_;
	uint16 width_;
	uint16 height_;
	uint16 x_;
	uint16 z_;
};


//This chunk walker just gives up information that is added to it.
//Most often used by others just to store a list of chunks.
//It can serialise to xml.
class CacheChunkWalker : public IChunkWalker
{
public:
	CacheChunkWalker();
	void reset()	{};
	void spaceInformation( const std::string& spaceName, const GridCoord& localToWorld, uint16 width, uint16 height );
	bool nextTile( std::string& chunkName, int16& gridX, int16& gridZ );
	void add( Chunk* pChunk );
	void add( int16& gridX, int16& gridZ );
	void save( DataSectionPtr pSection );
	void save( DataSectionPtr pSection, std::vector<uint16>& modifiedTileNumbers );
	void load( DataSectionPtr pSection );
    bool added( const Chunk* pChunk ) const;
    bool added( int16 gridX, int16 gridZ ) const;
    bool erase( const Chunk* pChunk );
    bool erase( int16 gridX, int16 gridZ );
    size_t size() const;
private:
	std::vector<uint16>		 xs_;
	std::vector<uint16>		 zs_;
	GridCoord localToWorld_;
	uint16	width_;
	uint16	height_;
};


/**
 *	This class scans the space on disk for all thumbnail files,
 *	and returns chunks identified as relevant by derived classes.
 *
 *	This base class handles file searching and tile storage.
 *	Derived classes need only implement the isRelevant interface.
 */
class FileChunkWalker : public IChunkWalker
{
public:
	FileChunkWalker( uint16 numIrrelevantPerFind = 10 );
	virtual ~FileChunkWalker();
protected:
	void reset();
	void spaceInformation( const std::string& spaceName, const GridCoord& localToWorld, uint16 width, uint16 height );
	bool nextTile( std::string& chunkName, int16& gridX, int16& gridZ );
	void load();
	void save();

	virtual bool isRelevant( const WIN32_FIND_DATA& fileInfo ) = 0;

	GridCoord localToWorld_;
	uint16	width_;
	uint16	height_;
	std::string	reentryFolder_;
private:
	void findNextFile();
	void findFolders( const std::string& folder );
	void advanceFileHandle();
	void releaseFileHandle();

	void findReentryFolder();
	bool partialFolderMatch( const std::string& folder, const std::string& target );
	bool findFolder( const std::string& target );

	//disregard ., .., CVS folders
	bool isValidFolder( const WIN32_FIND_DATA& fileInfo );

	HANDLE fileHandle_;
	WIN32_FIND_DATA findFileData_;
	std::string root_;
	std::string space_;
	std::string folder_;
	bool searchingFiles_;
	std::vector<std::string> folders_;
	std::vector<uint16> xs_;
	std::vector<uint16>	zs_;
	uint16	numIrrelevantPerFind_;
};



//This chunk walker finds thumbnails with a modification date
//later than the modification date stored in our binary cache.
class ModifiedFileChunkWalker : public FileChunkWalker
{
public:
	ModifiedFileChunkWalker( uint16 numIrrelevantPerFind = 10 );
	~ModifiedFileChunkWalker();
	void spaceInformation( const std::string& spaceName, const GridCoord& localToWorld, uint16 width, uint16 height );
	void load();
	void save();
private:
	bool isRelevant( const WIN32_FIND_DATA& fileInfo );
	uint64 cacheModifyTime( uint32 tileNum );
	void cacheModifyTime( uint32 tileNum, uint64 modTime );

	std::string cacheName_;
	BinaryPtr	pLastModified_;
	
};
