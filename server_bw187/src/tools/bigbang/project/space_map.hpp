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

#include "moo/render_target.hpp"
#include "moo/device_callback.hpp"
#include "resmgr/datasection.hpp"
#include "grid_coord.hpp"
#include "chunk_walker.hpp"
#include <set>

namespace Moo
{
	class EffectMaterial;
};


/**
 *	This class manages the rendering of an automatically
 *	generated map of the whole space.
 *
 *	It uses small tiles that are comitted to CVS by various
 *	WorldEditor users.
 *
 *	When BigBang does a full save, the SpaceMap recreates
 *	any bitmap tiles not up to date by photographing the
 *	appropriate chunks from above.
 */
class SpaceMap : public Moo::DeviceCallback
{
public:
	static SpaceMap& instance();
	~SpaceMap();

	void spaceInformation( const std::string& spaceName, const GridCoord& localToWorld, uint16 width, uint16 height );
	void setTexture( uint8 textureStage );
	bool init( DataSectionPtr pSection );
	void update( float dTime );

	//this method forces a re-photograph of the whole space.
	void invalidateAllChunks();

	//load cached, saved map off disk.
	void load();
	void save();

	//notification that something has changed in memory, that
	//means the current thumbnail for the chunk is now invalid.
	void dirtyThumbnail( Chunk* pChunk );

	void swapInTextures( Chunk* chunk );

    void regenerateAllDirty(bool showProgress);

private:
	SpaceMap();
	void createTextures();
	void createRenderTarget();
	void releaseTextures();
	void createUnmanagedObjects();
	void deleteUnmanagedObjects();

	void drawGridSquare( int16 gridX, int16 gridZ );
	void drawTile( float x, float y, float dx, float dy );

	bool inspectTiles( uint32 n, IChunkWalker& chunkWalker );
	bool swapInTextures( uint32 n, IChunkWalker& chunkWalker, CacheChunkWalker *removeCache);
	bool photographChunks( uint32 n, IChunkWalker& chunkWalker );

	bool cacheName( std::string& ret );

	//temporary cache ( used only from create/delete unmanaged )
	void saveTemporaryCache();
	void loadTemporaryCache();

    void recreateAfterReset();

private:
	Moo::EffectMaterial         *material_;
	std::string                 spaceName_;
	GridCoord                   localToWorld_; // transform from origin at (0,0) to origin at middle of map
	uint16	                    gridWidth_;
	uint16	                    gridHeight_;
	int	                        nTexturesPerFrame_;
	int	                        nPhotosPerFrame_;
	Moo::RenderTarget           map_;
	LinearChunkWalker           allThumbnails_;
	CacheChunkWalker            missingThumbnails_;
	ModifiedFileChunkWalker     modifiedThumbnails_;
	std::vector<uint16>         modifiedTileNumbers_;
	bool                        cacheNeedsRetrieval_;
	std::set<Chunk *>           changedThumbnailChunks_;
    bool                        deviceReset_;
};
