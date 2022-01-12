/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CHUNK_TERRAIN_HPP
#define CHUNK_TERRAIN_HPP

#include "chunk_item.hpp"
#include "chunk.hpp"
#include "cstdmf/smartpointer.hpp"

namespace Moo
{
	class TerrainBlock;
	typedef SmartPointer<TerrainBlock> TerrainBlockPtr;
}


/**
 *	This class is the chunk item for a terrain block
 */
class ChunkTerrain : public ChunkItem
{
	DECLARE_CHUNK_ITEM( ChunkTerrain )

public:
	ChunkTerrain();
	~ChunkTerrain();

	virtual void	toss( Chunk * pChunk );
	virtual void	draw();

	Moo::TerrainBlockPtr block()	{ return block_; }
	const BoundingBox & bb() const	{ return bb_; }

    void calculateBB();

	static std::string	resourceID( const std::string& chunkResourceID )
	{
		return chunkResourceID + ".cdata/terrain";
	}

#if UMBRA_ENABLE
public:
	void disableOccluder();
	void enableOccluder();

private:
	typedef std::vector<Vector3> Vector3Vector;
	Vector3Vector terrainVertices_;
	
	typedef std::vector<uint32> IntVector;
	IntVector terrainIndices_;
#endif // UMBRA_ENABLE

protected:
	ChunkTerrain( const ChunkTerrain& );
	ChunkTerrain& operator=( const ChunkTerrain& );

	Moo::TerrainBlockPtr		block_;
	BoundingBox					bb_;		// in local coords

	bool load( DataSectionPtr pSection, Chunk * pChunk, std::string* errorString = NULL );

	friend class ChunkTerrainCache;

	virtual void syncInit();
	virtual bool addYBounds( BoundingBox& bb ) const;
};



class ChunkTerrainObstacle;

/**
 *	This class is a one-per-chunk cache of the chunk terrain item
 *	for that chunk (if it has one). It allows easy access to the
 *	terrain block given the chunk, and adds the terrain obstacle.
 */
class ChunkTerrainCache : public ChunkCache
{
public:
	ChunkTerrainCache( Chunk & chunk );
	~ChunkTerrainCache();

	virtual int focus();

	void pTerrain( ChunkTerrain * pT );

	ChunkTerrain * pTerrain()				{ return pTerrain_; }
	const ChunkTerrain * pTerrain() const	{ return pTerrain_; }

	static Instance<ChunkTerrainCache>	instance;

private:
	Chunk * pChunk_;
	ChunkTerrain * pTerrain_;
	SmartPointer<ChunkTerrainObstacle>	pObstacle_;
};





#ifdef CODE_INLINE
#include "chunk_terrain.ipp"
#endif

#endif // CHUNK_TERRAIN_HPP
