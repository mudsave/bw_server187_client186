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
#include "space_map.hpp"
#include "space_helpers.hpp"

#include "chunk/base_chunk_space.hpp"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk_manager.hpp"
#include "cstdmf/debug.hpp"
#include "moo/base_texture.hpp"
#include "moo/texture_manager.hpp"
#include "moo/vertex_formats.hpp"
#include "moo/texture_compressor.hpp"
#include "moo/effect_material.hpp"
#include "romp/custom_mesh.hpp"
#include "../big_bang.hpp"
#include "chunks/editor_chunk.hpp"
#include "common/material_utility.hpp"

DECLARE_DEBUG_COMPONENT2( "WorldEditor", 0 )


SpaceMap& SpaceMap::instance()
{
	static SpaceMap sm;
	return sm;
}


SpaceMap::SpaceMap():
	gridWidth_(0),
	gridHeight_(0),
	nTexturesPerFrame_(4),
	nPhotosPerFrame_(2),
	spaceName_( "" ),
	map_( "spaceMap" ),
	material_( NULL ),
	cacheNeedsRetrieval_( true ),
	modifiedThumbnails_( 10 ),
	localToWorld_(0,0),
    deviceReset_(false)
{
	material_ = new Moo::EffectMaterial();
	material_->load( BWResource::openSection( "resources/materials/space_map.mfm" ));
	MaterialUtility::viewTechnique( material_, "spaceMap" );
}


SpaceMap::~SpaceMap()
{
	delete material_;
}


void SpaceMap::setTexture( uint8 textureStage )
{
	Moo::rc().setTexture( textureStage, map_.pTexture() );
}


void SpaceMap::spaceInformation( const std::string& spaceName, const GridCoord& localToWorld, uint16 width, uint16 height )
{
	if ( spaceName == spaceName_ )
		return;

	spaceName_ = spaceName;
	localToWorld_ = localToWorld;
	gridWidth_ = width;
	gridHeight_ = height;

	allThumbnails_.spaceInformation( spaceName_, localToWorld_, gridWidth_, gridHeight_ );
	missingThumbnails_.spaceInformation( spaceName_, localToWorld_, gridWidth_, gridHeight_ );
	modifiedThumbnails_.spaceInformation( spaceName_, localToWorld_, gridWidth_, gridHeight_ );

	this->load();
}


bool SpaceMap::init( DataSectionPtr pSection )
{
	return true;
}


void SpaceMap::createRenderTarget()
{
	if ( !map_.pTexture() )
	{
		//TODO : make configurable and device dependent.
		int width = 2048;
		int height = 2048;
		map_.create( width, height );
        map_.clearOnRecreate(true, 0xffffffff);
	}
}


void SpaceMap::createTextures()
{
	this->createRenderTarget();
    if (!deviceReset_)
        cacheNeedsRetrieval_ = true;
}


void SpaceMap::releaseTextures()
{
	if ( map_.pTexture() != NULL )
	{
        if (!deviceReset_)
		    this->saveTemporaryCache();
		map_.release();
	}
}


void SpaceMap::createUnmanagedObjects()
{
	createTextures();
}


void SpaceMap::deleteUnmanagedObjects()
{
    deviceReset_ = Moo::rc().device()->TestCooperativeLevel() != D3D_OK;
	releaseTextures();
}


void SpaceMap::update( float dTime )
{
	// Rendering is done here, so we need to have a valid device:
	if (Moo::rc().device()->TestCooperativeLevel() != D3D_OK)
	{
		return;
	}

    // If the device was reset then the best we can do is to redraw everything.
    if ( deviceReset_ )
    {
        recreateAfterReset();
        deviceReset_ = false;
    }
    // If the window was resized then load the cached image (this is not 
    // possible on a device reset).
	else if ( cacheNeedsRetrieval_ )
    {
		this->loadTemporaryCache();
        cacheNeedsRetrieval_ = false;
    }

	BigBang::instance().markChunks();
	ChunkManager::instance().tick( dTime );

	// Modified thumbnails are calculated as we go.
	swapInTextures( nTexturesPerFrame_, modifiedThumbnails_, NULL );

	// Inspect tiles finds missing thumbnails.
	if ( !inspectTiles( 1, allThumbnails_ ) )
		allThumbnails_.reset();

	photographChunks( nPhotosPerFrame_, missingThumbnails_ );
}


/**
 *	This method looks at all the tiles provided by the ChunkWalker,
 *	and checks for existence, and whether it was modified after the
 *	cache map.
 *
 *	Modified maps go into the modifiedThumbnails cache,
 *	missing maps go into the missingThumbnails cache.
 */
bool SpaceMap::inspectTiles( uint32 n, IChunkWalker& chunkWalker )
{
	int16 gridX, gridZ;
	std::string mapName;
	std::string pathName, chunkName;
	uint32 num = n;

	ChunkDirMapping* dirMap = BigBang::instance().chunkDirMapping();
	pathName= dirMap->path();

	while ( num > 0 )
	{
		if (!chunkWalker.nextTile(chunkName, gridX, gridZ))
			break;

		if ( !::thumbnailExists( pathName, chunkName ) )
		{
            if (!missingThumbnails_.added( gridX, gridZ ))
			    missingThumbnails_.add( gridX, gridZ );
		}
		num--;
	}

	return (num<n);
}


/**
 *	This method swaps n textures into the large bitmap, given
 *	a chunk walker.
 */
bool SpaceMap::swapInTextures
( 
    uint32              n, 
    IChunkWalker        &chunkWalker, 
    CacheChunkWalker    *removeCache 
)
{
	std::string mapName;
	std::string pathName, chunkName;
	int16 gridX, gridZ;

	ChunkDirMapping* dirMap = BigBang::instance().chunkDirMapping();
	pathName= dirMap->path();

	uint32 num = n;

	Moo::rc().beginScene();
	if (Moo::rc().mixedVertexProcessing())
		Moo::rc().device()->SetSoftwareVertexProcessing( TRUE );

	bool didOne = false;
	if ( map_.pTexture() && map_.push() )
	{
		material_->begin();
		for ( uint32 i=0; i<material_->nPasses(); i++ )
		{
			material_->beginPass(i);
			
			while ( num > 0 )
			{
				if (!chunkWalker.nextTile( chunkName, gridX, gridZ ))
					break;
				
				didOne = true;

				if (::thumbnailExists( pathName, chunkName ))
				{
					mapName = ::thumbnailFilename( pathName, chunkName );
					Moo::BaseTexturePtr pTexture = Moo::TextureManager::instance()->get(
						mapName, true, false );

					if ( pTexture )
					{
						Moo::rc().setTexture(0,pTexture->pTexture());
						this->drawGridSquare( gridX, gridZ );
					}
					else
					{
						Moo::rc().setTexture(0,NULL);
						ERROR_MSG( "SpaceMap::update - Could not load bmp %s, even though the resource exists\n", mapName.c_str() );
					}
				}
				else
				{
					Moo::rc().setTexture(0,NULL);
					ERROR_MSG( "SpaceMap::update - Could not load cdata file %s%s.cdata\n", pathName.c_str(), chunkName.c_str() );
				}
				if (removeCache != NULL)
					removeCache->erase( gridX, gridZ );
					
				--num;
			}
			material_->endPass();
		}
		material_->end();

		map_.pop();
	}

    Moo::rc().endScene();

	return didOne;
}

void SpaceMap::regenerateAllDirty(bool showProgress)
{
    BigBangProgressBar *progress  = NULL;
	ProgressTask       *genHMTask = NULL;
    
    try
    { 
        size_t numOperationsDone = 0; // Number of steps calculated so far.
        size_t numOperations     = 0; // Number of steps in total.

        // Create a progress indicator if requested:
        if (showProgress)
        {
            progress = new BigBangProgressBar();
            // Estimate the number of operations.  Unfortunately the 
            // modifiedThumbnails_ object cannot get the number of operations
            // it needs to do without a lot of work, so we take a guess that
            // it is about the same size as missingThumbnails_.
            numOperations = 
                missingThumbnails_.size()/nPhotosPerFrame_
                +
                missingThumbnails_.size()/nTexturesPerFrame_;
            genHMTask =
                new ProgressTask
                (
                    progress, 
                    "Updating project view", 
                    (float)(numOperations)
                );
        }

        // Swap in textures:
        bool swappedTexture = true;
        while (swappedTexture)
        {
            swappedTexture = 
                swapInTextures
                (
                    nTexturesPerFrame_, 
                    modifiedThumbnails_, 
                    &missingThumbnails_ 
                );            
            if (genHMTask != NULL)
            {
                genHMTask->step();
                ++numOperationsDone;
            }
        }

        // Generate missing thumbnails.  This should be after swapping in
        // textures as the textures stored in the .cdata files could be out
        // of date.
        while (missingThumbnails_.size() != 0)
        {
            photographChunks( nPhotosPerFrame_, missingThumbnails_ );            
            if (genHMTask != NULL)
            {
                genHMTask->step();
                ++numOperationsDone;
            }
        }

        // Update the progress indicator to look as though everything was done:
        if (genHMTask != NULL)
        {
            while (numOperationsDone < numOperations)
            {
                genHMTask->step();
                ++numOperationsDone;
            }
        }

        // Cleanup:
        delete progress ; progress  = NULL;
        delete genHMTask; genHMTask = NULL;

        // Save the result:
        save();
    }
    catch (...)
    {
        delete progress ; progress  = NULL;
        delete genHMTask; genHMTask = NULL;
        throw;
    }


}

void SpaceMap::swapInTextures( Chunk* chunk )
{
	if( ! chunk->isOutsideChunk() )
		return;
	std::string mapName;
	std::string pathName, chunkName;
	int16 gridX, gridZ;

	ChunkDirMapping* dirMap = BigBang::instance().chunkDirMapping();
	pathName= dirMap->path();

	Moo::rc().beginScene();
	if (Moo::rc().mixedVertexProcessing())
		Moo::rc().device()->SetSoftwareVertexProcessing( TRUE );

	if ( map_.pTexture() && map_.push() )
	{
		material_->begin();
		for ( uint32 i=0; i<material_->nPasses(); i++ )
		{
			material_->beginPass(i);

			if( !gridFromChunk( chunk, gridX, gridZ ) )
				break;
			chunkID( chunkName, gridX, gridZ );
				
			if (::thumbnailExists( pathName, chunkName ))
			{
				mapName = ::thumbnailFilename( pathName, chunkName );
				Moo::BaseTexturePtr pTexture = Moo::TextureManager::instance()->get(
					mapName, true, false );

				if ( pTexture )
				{
					Moo::rc().setTexture(0,pTexture->pTexture());
					this->drawGridSquare( gridX, gridZ );
				}
				else
				{
					Moo::rc().setTexture(0,NULL);
					ERROR_MSG( "SpaceMap::update - Could not load bmp %s, even though the resource exists\n", mapName.c_str() );
				}
			}
			else
			{
				Moo::rc().setTexture(0,NULL);
				ERROR_MSG( "SpaceMap::update - Could not load cdata file %s%s.cdata\n", pathName.c_str(), chunkName.c_str() );
			}

			material_->endPass();
		}
		material_->end();

		map_.pop();
	}

	Moo::rc().endScene();
}

/**
 *	This method photographs the next n appropriate chunks and saves
 *	their thumbnails.
 *
 *	For each chunk photographed, the tile is swapped in immediately.
 */
bool SpaceMap::photographChunks( uint32 n, IChunkWalker& chunkWalker )
{
	CacheChunkWalker ccw;

	std::string pathName, chunkName, mapName;
	int16 gridX, gridZ;

	ChunkDirMapping* dirMap = BigBang::instance().chunkDirMapping();
	pathName= dirMap->path();

	ChunkSpacePtr pSpace = ChunkManager::instance().cameraSpace();

	uint32 num = n;
	while ( num > 0 )
	{
		if (!chunkWalker.nextTile( chunkName, gridX, gridZ ))
			break;

		Chunk* pChunk = pSpace->findChunk( chunkName, "" );
		if ( pChunk && pChunk->online() )
		{
			bool ok = EditorChunkCache::instance(*pChunk).calculateThumbnail();
			if (ok)
				ccw.add( gridX, gridZ );
		}
		else
		{
			BigBang::instance().loadChunkForThumbnail( chunkName );
			++num;
		}

		num--;
	}

	if ( num<n )
	{
		swapInTextures( (n-num), ccw, NULL );
		return true;
	}
	return false;
}


/**
 *	This method calcualtes the screen-space area
 *	for the given grid square, and calls drawTile with those
 *	values.
 */
void SpaceMap::drawGridSquare( int16 gridX, int16 gridZ )
{
	uint16 biasedX,biasedZ;
	::biasGrid(localToWorld_,gridX,gridZ,biasedX,biasedZ);	

	float dx = Moo::rc().screenWidth() / float(gridWidth_);
	float dy = Moo::rc().screenHeight() / float(gridHeight_);

	float x = (float(biasedX) / float(gridWidth_)) * (Moo::rc().screenWidth());
	float y = (float(biasedZ) / float(gridHeight_))* (Moo::rc().screenHeight());

	drawTile( x, y, dx, dy );

}


/**
 *	Draws a single tile ( i.e quad ) in screen space.
 *
 *	Pass in non-texel aligned screen space coordinates.
 */
void SpaceMap::drawTile( float x, float y, float dx, float dy )
{
	static CustomMesh< Moo::VertexTUV > s_gridSquare( D3DPT_TRIANGLEFAN );

	Vector2 uvStart( 0.f, 0.f );
	Vector2 uvEnd( 1.f, 1.f );

	if ( dy < 0.f )
	{
		//handle case of flipped textures.
		uvEnd.y = 0.f;
		uvStart.y = 1.f;
		y += dy;
		dy = -dy;
	}

	//screen/texel alignment	
	x -= 0.5f;
	y -= 0.5f;	

	s_gridSquare.clear();
	Moo::VertexTUV v;
	v.pos_.set(x,y,1.f,1.f);
	v.uv_.set(uvStart.x,uvEnd.y);
	s_gridSquare.push_back(v);

	v.pos_.set(x+dx,y,1.f,1.f);
	v.uv_.set(uvEnd.x,uvEnd.y);
	s_gridSquare.push_back(v);

	v.pos_.set(x+dx,y+dy,1.f,1.f);
	v.uv_.set(uvEnd.x,uvStart.y);
	s_gridSquare.push_back(v);

	v.pos_.set(x,y+dy,1.f,1.f);
	v.uv_.set(uvStart.x,uvStart.y);
	s_gridSquare.push_back(v);

	s_gridSquare.drawEffect();
}


bool SpaceMap::cacheName( std::string& ret )
{
	ChunkDirMapping* dirMap = BigBang::instance().chunkDirMapping();
	if ( dirMap )
	{
		ret = dirMap->path() + "space";
		return true;
	}
	return false;
}


void SpaceMap::load()
{
	CWaitCursor wait;

	std::string name;
	if ( !cacheName( name ) )
		return;

	// Ensure that the render target is ok.
	this->createRenderTarget();

	Moo::rc().beginScene();
	if (Moo::rc().mixedVertexProcessing())
		Moo::rc().device()->SetSoftwareVertexProcessing( TRUE );

	// First load the cache texture.
	if ( map_.pTexture() && map_.push() )
	{
		material_->begin();
		for ( uint32 i=0; i<material_->nPasses(); i++ )
		{
			material_->beginPass(i);
			Moo::rc().setTexture(0,NULL);

			std::string mapName = name + ".thumbnail.dds";

			if (BWResource::openSection(mapName,false))
			{
				Moo::BaseTexturePtr pTexture = Moo::TextureManager::instance()->get(
						mapName, true, false );

				if ( pTexture )
				{
					Moo::rc().setTexture(0,pTexture->pTexture());
				}
			}

			this->drawTile( 0.f, Moo::rc().screenHeight(), Moo::rc().screenWidth(), -Moo::rc().screenHeight() );

			material_->endPass();
		}
		material_->end();
		map_.pop();
	}

	Moo::rc().endScene();

	cacheNeedsRetrieval_ = false;

	// Now load the thumbnail modification date cache.
	modifiedThumbnails_.load();
}


void SpaceMap::save()
{
	CWaitCursor wait;

	if ( !map_.pTexture() )
	{
		return;
	}

	std::string name;
	if (!cacheName( name ))
		return;
	
	std::string mapName = name + ".thumbnail.dds";

	//save the thumbnail modification date cache.
	modifiedThumbnails_.save();

	//Then save out the texture
	TextureCompressor tc( static_cast<DX::Texture*>(map_.pTexture()), D3DFMT_DXT1, 1 );
	tc.save( mapName );
}


/**
 *	Load temporary cached map off disk, and apply to render target.
 */
void SpaceMap::loadTemporaryCache()
{
	CWaitCursor wait;

	std::string name;
	if ( !cacheName( name ) )
		return;

	Moo::rc().beginScene();
	if (Moo::rc().mixedVertexProcessing())
		Moo::rc().device()->SetSoftwareVertexProcessing( TRUE );

	//First load the cache texture
	if ( map_.pTexture() && map_.push() )
	{
		material_->begin();
		for ( uint32 i=0; i<material_->nPasses(); i++ )
		{
			material_->beginPass(i);
			Moo::rc().setTexture(0,NULL);

			std::string mapName = name + ".temp_thumbnail.dds";
			
			//try retrieving our temporary cache
			if (BWResource::openSection(mapName,false))
			{
				Moo::BaseTexturePtr pTexture = Moo::TextureManager::instance()->get(
						mapName, true, false );

				if ( pTexture )
				{
					Moo::rc().setTexture(0,pTexture->pTexture());
				}

				::DeleteFile( BWResource::resolveFilename( mapName ).c_str() );
			}

			this->drawTile( 0.f, Moo::rc().screenHeight(), Moo::rc().screenWidth(), -Moo::rc().screenHeight() );
			material_->endPass();
		}
		material_->end();
		map_.pop();
	}

	Moo::rc().endScene();

	cacheNeedsRetrieval_ = false;
}


/**
 *	Save cached large space map temporarily to disk, because
 *	we are about to recreate the underlying texture.
 */
void SpaceMap::saveTemporaryCache()
{
	if ( cacheNeedsRetrieval_ )
	{
		return;
	}

	CWaitCursor wait;

	std::string name;
	if (!cacheName( name ))
		return;
	std::string tempMapName = name + ".temp_thumbnail.dds";

	//Then save out the texture
	TextureCompressor tc( static_cast<DX::Texture*>(map_.pTexture()), D3DFMT_DXT1, 1 );
	tc.save( tempMapName );
}


void SpaceMap::dirtyThumbnail( Chunk* pChunk )
{
	// Add to list of missing thumbnails, so we eventually photograph the 
    // chunk.
    // Be careful not to re-add an existing chunk.
    if (!missingThumbnails_.added( pChunk ))
		missingThumbnails_.add( pChunk );
}


void SpaceMap::invalidateAllChunks()
{
	CWaitCursor wait;
	allThumbnails_.reset();

	int16 gridX, gridZ;
	std::string mapName;
	std::string pathName, chunkName;	

	ChunkDirMapping* dirMap = BigBang::instance().chunkDirMapping();
	pathName= dirMap->path();

	while ( 1 )
	{
		if (!allThumbnails_.nextTile(chunkName, gridX, gridZ))
			break;
		
		DataSectionPtr pSection = BWResource::openSection( pathName + chunkName + ".cdata", false );
		if ( pSection )
		{
			pSection->delChild( "thumbnail.dds" );
			pSection->save();
		}		
	}	
	
	allThumbnails_.reset();
}


void SpaceMap::recreateAfterReset()
{
    allThumbnails_.reset();
    missingThumbnails_.reset();

	allThumbnails_.spaceInformation( spaceName_, localToWorld_, gridWidth_, gridHeight_ );
	missingThumbnails_.spaceInformation( spaceName_, localToWorld_, gridWidth_, gridHeight_ );
	modifiedThumbnails_.spaceInformation( spaceName_, localToWorld_, gridWidth_, gridHeight_ );

	this->load();

    for (uint16 y = 0; y < gridHeight_; ++y)
    {
        int16 sy = y + localToWorld_.y;
        for (uint16 x = 0; x < gridWidth_; ++x)
        {
            int16 sx = x + localToWorld_.x;            
            if (!missingThumbnails_.added(sx, sy))
				missingThumbnails_.add(sx, sy);
        }
    }
}
