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
#include "editor_chunk_terrain.hpp"
#include "big_bang.hpp"
#include "appmgr/options.hpp"
#include "../chunks/editor_chunk.hpp"
#include "moo/terrain_block.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk_obstacle.hpp"
#include "cstdmf/debug.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/data_section_census.hpp"
#include "resmgr/auto_config.hpp"
#include "moo/base_texture.hpp"
#include "moo/shader_manager.hpp"
#include "moo/terrain_renderer.hpp"
#include "item_editor.hpp"
#include "item_properties.hpp"
#include "appmgr/module_manager.hpp"
#include "project/project_module.hpp"
#include "romp/line_helper.hpp"

//These are all helpers to debug / change the shadow calculation rays.
static bool s_debugShadowRays = false;
static std::vector<Vector3> s_lines;
static std::vector<Vector3> s_lineEnds;
static int s_collType = -1;
static float s_raySubdivision = 32.f;
static float s_rayDisplacement = 1.5f;

DECLARE_DEBUG_COMPONENT2( "EditorChunkTerrain", 2 );

// -----------------------------------------------------------------------------
// Section: EditorTerrainBlock
// -----------------------------------------------------------------------------

static AutoConfigString s_notFoundTexture( "system/notFoundBmp" );

/**
 *	This method saves this terrain block using the input resource name.
 *
 *	@return True on success, otherwise false.
 */
bool EditorTerrainBlock::save( const std::string& resourceName )
{
	if ( resourceName.substr(resourceName.size()-6,resourceName.size()) == ".cdata" )
	{
		return this->saveCData( resourceName.substr(0,resourceName.size()-6) );
	}
	else
	{
		return this->saveLegacy( resourceName );
	}
}


DataSectionPtr makeBinarySection( const std::string& fileName )
{
	// create a section
	size_t lastSep = fileName.find_last_of('/');
	std::string parentName = fileName.substr(0, lastSep);
	DataSectionPtr parentSection = BWResource::openSection( parentName );
	MF_ASSERT(parentSection);

	std::string tagName = fileName.substr(lastSep + 1);

	// make it
	DataSectionPtr cData = new BinSection( tagName, new BinaryBlock( NULL, 0 ) );
	cData->setParent( parentSection );
	cData = DataSectionCensus::add( fileName, cData );

	return cData;
}


/**
 *	This method saves the terrain data out to the terrain section
 *	of a chunk's .cdata file.
 */
bool EditorTerrainBlock::saveCData( const std::string& resourceName )
{
	//TODO : stream terrain data directly to BinSection
	std::string tempName( "temp.terrain" );
	this->saveLegacy( tempName );

	//Now copy it into the cdata section
	//Create a binary block copy of this terrainfile.
	DataSectionPtr pTerrainFile = BWResource::openSection( "temp.terrain" );
	BinaryPtr pTerrainData = pTerrainFile->asBinary();
	BinaryPtr binaryBlock = new BinaryBlock(pTerrainData->data(), pTerrainData->len());	

	//Remove temporary data
	pTerrainFile = NULL;
	pTerrainData = NULL;
	BWResource::instance().purge( tempName );
	::DeleteFile( BWResource::resolveFilename(tempName).c_str() );

	//Stick the terrain into a binary section, and save the file to disk.
	std::string cDataName = resourceName + ".cdata";
	DataSectionPtr pSection = BWResource::openSection( cDataName, false );
	if ( !pSection )
		pSection = makeBinarySection( cDataName );
	MF_ASSERT( pSection );
	pSection->delChild( "terrain" );
	DataSectionPtr terrainSection = pSection->openSection( "terrain", true );
	terrainSection->setParent(pSection);

	if ( !pSection->writeBinary( "terrain", binaryBlock ) )
	{
		BigBang::instance().addError( NULL, NULL,
			"EditorTerrainBlock::saveCData - error while writing BinSection in %s/terrain\n",
			cDataName.c_str() );
		return false;
	}

	terrainSection->save();
	pSection->save();

	// We have to update the chunk's cdata section too since it is now out of
	// sync.  This would be 'ok' (though a major hack) because of the 
	// DataSection caching system, but this can get flushed.  This can happen,
	// for example, when reloading models.
	Chunk *chunk = chunkTerrain_->chunk();
	if (chunk != NULL)
	{
		DataSectionPtr cdataSection = 
			EditorChunkCache::instance(*chunk).pCDataSection();
		if (cdataSection)
		{
			cdataSection->delChild("terrain");
			cdataSection->writeBinary("terrain", binaryBlock);
		}
	}

	return true;
}


/**
 *	This method saves the terrain data out to a legacy
 *	.terrain file.
 */
bool EditorTerrainBlock::saveLegacy( const std::string& resourceName )
{
	FILE* outFile = fopen( BWResource::resolveFilename( resourceName ).c_str(), "wb" );

	if ( outFile )
	{
		TerrainBlockHeader tbh;
		ZeroMemory( &tbh, sizeof( TerrainBlockHeader ) );
		tbh.version_ = 0x101;
		tbh.heightMapWidth_ = width_;
		tbh.heightMapHeight_ = height_;
		tbh.spacing_ = spacing_;
		tbh.nTextures_ = textures_.size();
		tbh.textureNameSize_ = 128;
		tbh.detailWidth_ = detailWidth_;
		tbh.detailHeight_ = detailHeight_;

		fwrite( &tbh, sizeof( TerrainBlockHeader ), 1, outFile );

		char buf[256];

		std::vector< Moo::BaseTexturePtr >::iterator it = textures_.begin();
		std::vector< Moo::BaseTexturePtr >::iterator end = textures_.end();

		while ( it != end )
		{
			Moo::BaseTexturePtr& pTex = *it++;

			if ( pTex && pTex->resourceID() != "" )
			{
				sprintf( buf, "%.127s", pTex->resourceID().c_str() );	
			}
			else
			{
				if ( !pTex )
					ERROR_MSG( "EditorTerrainBlock::save - Null texture found in terrain block %s", resourceName.c_str() );
				else
					ERROR_MSG( "EditorTerrainBlock::save - Texture with empty string name found in terrain block %s", resourceName.c_str() );

				sprintf( buf, "%.127s", s_notFoundTexture.value().c_str() );
			}

			fwrite( buf, 128, 1, outFile );
		}

		fwrite( &heightMap_.front(), sizeof(float), heightMap_.size(), outFile );
		fwrite( &blendValues_.front(), sizeof(uint32), blendValues_.size(), outFile );
		fwrite( &shadowValues_.front(), sizeof(uint16), shadowValues_.size(), outFile );
		for ( uint i=0; i<holes_.size(); i++ )
		{
        	uint8 val = (uint8)holes_[i];
			fwrite( &val, sizeof(uint8), 1, outFile );
		}
		fwrite( &detailIDs_.front(), sizeof(uint8), detailIDs_.size(), outFile );

		fclose( outFile );

		return true;
	}

	return false;
}


/*bool EditorTerrainBlock::save( const std::string& resourceName )
{
	std::string fname = resourceName.substr(0,resourceName.size()-14);
	fname += "o.terrain";
	FILE* outFile = fopen( BWResource::resolveFilename( fname ).c_str(), "wb" );

	if ( outFile )
	{
		TerrainBlockHeader tbh;
		ZeroMemory( &tbh, sizeof( TerrainBlockHeader ) );
		tbh.version_ = 0x101;
		tbh.heightMapWidth_ = width_;
		tbh.heightMapHeight_ = height_;
		tbh.spacing_ = spacing_;
		tbh.nTextures_ = textures_.size();
		tbh.textureNameSize_ = 128;
		tbh.detailWidth_ = detailWidth_;
		tbh.detailHeight_ = detailHeight_;

		fwrite( &tbh, sizeof( TerrainBlockHeader ), 1, outFile );

		char buf[256];

		std::vector< Moo::BaseTexturePtr >::iterator it = textures_.begin();
		std::vector< Moo::BaseTexturePtr >::iterator end = textures_.end();

		while ( it != end )
		{
			Moo::BaseTexturePtr& pTex = *it++;

			if ( pTex && pTex->resourceID() != "" )
			{
				sprintf( buf, "%.127s", pTex->resourceID().c_str() );	
			}
			else
			{
				if ( !pTex )
					ERROR_MSG( "EditorTerrainBlock::save - Null texture found in terrain block %s", fname.c_str() );
				else
					ERROR_MSG( "EditorTerrainBlock::save - Texture with empty string name found in terrain block %s", fname.c_str() );

				sprintf( buf, "%.127s", s_notFoundTexture.c_str() );
			}

			fwrite( buf, 128, 1, outFile );
		}

		fwrite( &heightMap_.front(), sizeof(float), heightMap_.size(), outFile );
		fwrite( &blendValues_.front(), sizeof(uint32), blendValues_.size(), outFile );
		fwrite( &shadowValues_.front(), sizeof(uint16), shadowValues_.size(), outFile );
        for ( uint i=0; i<holes_.size(); i++ )
        {
        	uint8 val = (uint8)holes_[i];
			fwrite( &val, sizeof(uint8), 1, outFile );
        }
		fwrite( &detailIDs_.front(), sizeof(uint8), detailIDs_.size(), outFile );

		fclose( outFile );

		//Now copy it into the cdata section
		//Create a binary block copy of this terrainfile.
		DataSectionPtr pTerrainFile = BWResource::openSection( fname );
		BinaryPtr pTerrainData = pTerrainFile->asBinary();
		BinaryPtr binaryBlock = new BinaryBlock(pTerrainData->data(), pTerrainData->len());		

		//Stick the terrain into a binary section, and save the file to disk.
		std::string cDataName = fname.substr(0,fname.size()-7);
		cDataName += "cdata";
		DataSectionPtr pSection = BWResource::openSection( cDataName, false );
		pSection->delChild( "terrain" );
		DataSectionPtr terrainSection = pSection->openSection( "terrain", true );
		terrainSection->setParent(pSection);

		if ( !pSection->writeBinary( "terrain", binaryBlock ) )
		{
			return false;
		}

		terrainSection->save();
		pSection->save();

		return true;
	}

	return false;
}*/



const std::string& EditorTerrainBlock::textureName( int level )
{
	MF_ASSERT( level >= 0 );
	MF_ASSERT( level < (int) textures_.size() );
	
	return textures_[level]->resourceID();
}


void EditorTerrainBlock::textureName( int level, const std::string& newName )
{
	textures_[level] = Moo::TextureManager::instance()->get( newName );
}


/**
 * This method draws the whole terrain block.
 * regardless of whether it contains holes or not.

 * @param setTextures true if you want the method to set the textures on the
 * texture stages
 */
void EditorTerrainBlock::drawIgnoringHoles( bool setTextures )
{
	// Allocate d3d resources if they have not been allocated already.
	if (!vertexBuffer_.pComObject() || !indexBuffer_.pComObject())
	{
		createManagedObjects();
	}
	if (Moo::rc().device())
	{
		// Set the textures on the device.
		if (setTextures)
		{
			for (uint32 i = 0; i < textures_.size(); i++)
			{
				Moo::rc().setTexture( i, textures_[ i ]->pTexture() );
			}
		}

		// Set up buffers and draw.
		uint32 nIndices = TerrainBlockIndicesNoHoles::instance().setIndexBuffer(blocksHeight_,blocksWidth_,verticesWidth_);
		uint32 nPrimitives = nIndices - 2;		
		Moo::rc().device()->SetStreamSource( 0, &*vertexBuffer_, 0, sizeof( Moo::VertexXYZNDS ) );
		Moo::rc().drawIndexedPrimitive( D3DPT_TRIANGLESTRIP,
				0, 0, nVertices_, 0, nPrimitives );
		Moo::rc().addToPrimitiveCount( nPrimitives );
	}
}


/**
 *	This method resizes the variable-sized detail ID section in a terrain block.
 *
 *	@param w the width of the detail ID information.
 *	@param h the height of the detail ID information.
 */
void EditorTerrainBlock::resizeDetailIDs( int w, int h )
{
	detailWidth_ = w;
	detailHeight_ = h;
	detailIDs_.resize( w * h );
}

void EditorTerrainBlock::draw( Moo::TerrainTextureSetter* tts )
{
	if( BigBang::instance().drawSelection() )
		Moo::rc().setRenderState( D3DRS_TEXTUREFACTOR, (DWORD)chunkTerrain_ );
	TerrainBlock::draw( tts );
}

PY_MODULE_STATIC_METHOD( EditorTerrainBlock, createBlankTerrainFile, BigBang )


/**
 *	This method creates a new terrain file, given the full terrain file name.
 *
 *	@return		the terrain file name, or "error" if the function failed.
 */
PyObject * EditorTerrainBlock::py_createBlankTerrainFile( PyObject * args )
{
	char * fileName = NULL;

	if (!PyArg_ParseTuple( args, "s", &fileName ))
	{
		PyErr_SetString( PyExc_ValueError, "BigBang.createBlankTerrainFile() "
			"expects a relative filename" );
		return PyString_FromString( "Error" );
	}

	static EditorTerrainBlock eb;
	static bool loaded = false;

	if ( !loaded )
	{
		eb.load( "universes/blank.terrain" );
		loaded = true;
	}

	eb.save( fileName );

	return PyString_FromString( fileName );
}


// -----------------------------------------------------------------------------
// Section: EditorChunkTerrain
// -----------------------------------------------------------------------------

EditorChunkTerrain::EditorChunkTerrain()
{
	transform_ = Matrix::identity;

	// We want to pretend the origin is near (but not at) the centre so
	// everything rotates correctly when we have an even and an odd amount of 
	// terrain blocks selected

	//transform_.setTranslate( 50.f, 0.f, 50.f );
	transform_.setTranslate( 49.5f, 0.f, 49.5f );

	if (s_collType < 0)
	{
		s_collType = 1;
		MF_WATCH( "Render/Terrain Shadows/debug shadow rays",
			s_debugShadowRays,
			Watcher::WT_READ_WRITE,
			"Enabled, this draws a visual representation of the rays used to "
			"calculate the shadows on the terrain." );

		MF_WATCH( "Render/Terrain Shadows/coll type", s_collType,
			Watcher::WT_READ_WRITE,
			"0 - 500m straight line rays, 1 - <500m curved rays" );

		MF_WATCH( "Render/Terrain Shadows/ray subdv",
			s_raySubdivision,
			Watcher::WT_READ_WRITE,
			"If using curved rays, this amount changes the subdivision.  A "
			"lower value indicates more rays will be used.");

		MF_WATCH( "Render/Terrain Shadows/ray displ",
			s_rayDisplacement,
			Watcher::WT_READ_WRITE,
			"This value changes the lenght of the ray after the subdivisions. "
			"Use it to tweak how long the resultant rays are." );
	}
}

/**
 *	Handle ourselves being added to or removed from a chunk
 */
void EditorChunkTerrain::toss( Chunk * pChunk )
{
	Chunk* oldChunk = pChunk_;


	if (pChunk_ != NULL)
	{
		EditorChunkTerrainCache::instance().del(
			pChunk_->identifier() );

		if (pOwnSect_)
		{
			EditorChunkCache::instance( *pChunk_ ).
				pChunkSection()->delChild( pOwnSect_ );

			pOwnSect_ = NULL;
		}
	}
	

	this->ChunkTerrain::toss( pChunk );

	if (pChunk_ != NULL)
	{
		EditorChunkTerrainCache::instance().add(
			pChunk_->identifier(), this );

		std::string newResourceID = pChunk_->mapping()->path() + 
			ChunkTerrain::resourceID( pChunk_->identifier() );

		if (block().resourceID_ != newResourceID)
		{
			block().resourceID_ = newResourceID;
			BigBang::instance().changedTerrainBlock( pChunk_ );
			BigBang::instance().changedChunk( pChunk_ );
		}

		if (!pOwnSect_)
		{
			pOwnSect_ = EditorChunkCache::instance( *pChunk_ ).
				pChunkSection()->newSection( "terrain" );
		}

		this->edSave( pOwnSect_ );
	}

	// If there are any other terrain blocks in our old chunk, let them be the
	// one the terrain item cache knows about
	if (oldChunk)
	{
		EditorChunkCache::instance( *oldChunk ).fixTerrainBlocks();
	}
}


// Used when loading from a prefab, to get the terrain data
extern DataSectionPtr prefabBinarySection;

/**
 *	This method replaces ChunkTerrain::load
 */
bool EditorChunkTerrain::load( DataSectionPtr pSection, Chunk * pChunk, std::string* errorString )
{
	if (!pChunk->isOutsideChunk())
	{
		std::string err = "Can't load terrain block into an indoor chunk";
		if ( errorString )
		{
			*errorString = err;
		}
		else
		{
			ERROR_MSG( "%s\n", err.c_str() );
		}
		return false;
	}

	edCommonLoad( pSection );

	pOwnSect_ = pSection;

	std::string prefabBinKey = pOwnSect_->readString( "prefabBin" );
	if (!prefabBinKey.empty())
	{
		// Loading from a prefab, source our terrain block from there
		if (!prefabBinarySection)
		{
			std::string err = "Unable to load prefab. Section prefabBin found, but no current prefab binary section";
			if ( errorString )
			{
				*errorString = err;
			}
			else
			{
				BigBang::instance().addError( pChunk, NULL, err.c_str() );
			}
			return false;
		}

		BinaryPtr bp = prefabBinarySection->readBinary( prefabBinKey );

		if (!bp)
		{
			std::string err = "Unable to load " + prefabBinKey + "  from current prefab binary section";
			if ( errorString )
			{
				*errorString = err;
			}
			else
			{
				BigBang::instance().addError( pChunk, NULL, err.c_str() );
			}
			return false;
		}

		// Write the binary out to a temp file and read the block in from there
		BWResource::instance().purge( "prefab_temp.terrain" );

		std::string fname = BWResource::resolveFilename( "prefab_temp.terrain" );

		FILE* pFile = fopen( fname.c_str(), "wb" );

		if (!pFile)
		{
			std::string err = "Failed to open " + fname;
			if ( errorString )
			{
				*errorString = err;
			}
			else
			{
				ERROR_MSG( "EditorChunkTerrain::load: %s\n", err.c_str());
			}
			return false;
		}

		fwrite( bp->data(), bp->len(), 1, pFile );	
		fclose( pFile );

		block_ = EditorChunkTerrain::loadBlock( "prefab_temp.terrain", this );

		::DeleteFile( fname.c_str() );

		pOwnSect_->delChild( "prefabBin" );
	}
	else
	{
		// Loading as normal or cloning, check

		EditorChunkTerrain* currentTerrain = (EditorChunkTerrain*)
			ChunkTerrainCache::instance( *pChunk ).pTerrain();

		// If there's already a terrain block there, that implies we're cloning
		// from it.
		if (currentTerrain)
		{
			// Poxy hack to copy to terrain block
			BWResource::instance().purge( "clone_temp.terrain" );

			currentTerrain->block().save( "clone_temp.terrain" );
			block_ = EditorChunkTerrain::loadBlock( "clone_temp.terrain", this );

			std::string fname = BWResource::resolveFilename( "clone_temp.terrain" );
			::DeleteFile( fname.c_str() );

			if (!block_)
			{
				std::string err = "Could not load terrain block clone_temp.terrain";
				if ( errorString )
				{
					*errorString = err;
				}
				CRITICAL_MSG( "%s\n", err.c_str() );
				return false;
			}
		}
		else
		{
			std::string resName = pChunk->mapping()->path() + pSection->readString( "resource" );
			// Allocate the terrainblock.
			block_ = EditorChunkTerrain::loadBlock( resName, this );

			if (!block_)
			{
				std::string err = "Could not load terrain block '" + resName + "'";
				if ( errorString )
				{
					*errorString = err;
				}
				CRITICAL_MSG( "%s\n", err.c_str() );
				return false;
			}
		}
	}

	this->calculateBB();

	return true;
}

bool EditorChunkTerrain::edSave( DataSectionPtr pSection )
{
	edCommonSave( pSection );

	pSection->writeString( "resource", ChunkTerrain::resourceID( pChunk_->identifier() ) );

	return true;
}

bool EditorChunkTerrain::edShouldDraw()
{
	if( !ChunkTerrain::edShouldDraw() )
		return false;
	if (Moo::rc().frameTimestamp() != s_settingsMark_)
	{
		s_shouldDraw_ = Options::getOptionInt( "render/terrain", 1 ) == 1;
		s_settingsMark_ = Moo::rc().frameTimestamp();
	}

	return s_shouldDraw_;
}


/**
 *	This method replaces Moo::TerrainBlock::loadBlock, so we can create an
 *	EditorTerrainBlockPtr.
 *
 *	@param resourceName		The name of the terrain resource to load.
 *
 *	@return		A smart-pointer to the loaded terrain block, or NULL if the load
 *				failed.
 */
Moo::TerrainBlockPtr EditorChunkTerrain::loadBlock( const std::string& resourceName, EditorChunkTerrain* chunkTerrain )
{
	EditorTerrainBlockPtr block = new EditorTerrainBlock;
	block->chunkTerrain_ = chunkTerrain;
	block->resourceID_ = resourceName;

	if (block->load( resourceName ))
	{
		return block;
	}

	return NULL;
}


/**
 *	This method is exclusive to editorChunkTerrian.  It calculates an array
 *	of relative elevation values for each pole.
 *
 *	@param returnedValues	a modified vector of uint8s containing relative
 *							elevation data.
 *	@param w				returned width of the data.
 *	@param h				returned height of the data.
 */
void EditorChunkTerrain::relativeElevations(
		std::vector<float>& returnedValues, int& w, int& h )
{
	EditorTerrainBlock* petb =
			static_cast<EditorTerrainBlock*>(&*block_);

	returnedValues.clear();


	w = petb->polesWidth();
	h = petb->polesWidth();

	//calculate the average height at each corner
	float averageElevation = 0.f;
	int numSamples = 0;

	int z,x;

	for ( z = 0; z < h; z++ )
	{
		for ( x = 0; x < w; x++ )
		{
			averageElevation += petb->heightMap_[ z*w + x ];
			numSamples++;
		}
	}

	MF_ASSERT( numSamples != 0 );

	averageElevation /= numSamples;

	//now we have the average elevation for the whole chunk,
	//calculate the relative elevations
	for ( z = 0; z < h; z++ )
	{
		for ( x = 0; x < w; x++ )
		{
			float dy = petb->heightMap_[ z*w + x ] - averageElevation;
			returnedValues.push_back( dy );
		}
	}
}


/**
 *	This method calculates the slope at a height pole, and returns the
 *	angle in degrees.
 */
float EditorChunkTerrain::slope( int xIdx, int zIdx )
{
	EditorTerrainBlock* petb =
			static_cast<EditorTerrainBlock*>(&*block_);

	return ( RAD_TO_DEG( acosf( petb->heightPoleNormal( xIdx, zIdx ).y ) ) );
}


/**
 * This method should be called if you have changed the dimensions of the chunk.
 * It recalculates the chunk's bounding box.
 */
void EditorChunkTerrain::onAlterDimensions()
{
	// first copy the overlapping poles from neighbouring chunks
	EditorTerrainBlock* petb =
			static_cast<EditorTerrainBlock*>(&*block_);

	int w = petb->blocksWidth();

	Vector3 cen = (bb_.maxBounds() + bb_.minBounds()) * 0.5f;
	Vector3 delt = bb_.maxBounds() - bb_.minBounds();

	// do the back (1 row) and front (2 rows)
	Moo::TerrainBlockPtr bBack = Moo::TerrainBlock::findOutsideBlock(
		pChunk_->transform().applyPoint( cen + Vector3(0,0,-delt.z) )
		).pBlock_;
	Moo::TerrainBlockPtr bFront = Moo::TerrainBlock::findOutsideBlock(
		pChunk_->transform().applyPoint( cen + Vector3(0,0,delt.z) )
		).pBlock_;
	for (int x = -1; x < w+2; x++)
	{
		if (bBack)
		{
			Moo::TerrainBlock::Pole( *petb, x, -1 ).height() =
				Moo::TerrainBlock::Pole( *bBack, x, w-1 ).height();
		}
		if (bFront)
		{
			Moo::TerrainBlock::Pole( *petb, x, w ).height() =
				Moo::TerrainBlock::Pole( *bFront, x, 0 ).height();
			Moo::TerrainBlock::Pole( *petb, x, w+1 ).height() =
				Moo::TerrainBlock::Pole( *bFront, x, 1 ).height();
		}
	}

	// do the left (one row) and right (2 rows)
	Moo::TerrainBlockPtr bLeft = Moo::TerrainBlock::findOutsideBlock(
		pChunk_->transform().applyPoint( cen + Vector3(-delt.x,0,0) )
		).pBlock_;
	Moo::TerrainBlockPtr bRight = Moo::TerrainBlock::findOutsideBlock(
		pChunk_->transform().applyPoint( cen + Vector3(delt.x,0,0) )
		).pBlock_;
	for (int z = -1; z < w+2; z++)
	{
		if (bLeft)
		{
			Moo::TerrainBlock::Pole( *petb, -1, z ).height() =
				Moo::TerrainBlock::Pole( *bLeft, w-1, z ).height();
		}
		if (bRight)
		{
			Moo::TerrainBlock::Pole( *petb, w, z ).height() =
				Moo::TerrainBlock::Pole( *bRight, 0, z ).height();
			Moo::TerrainBlock::Pole( *petb, w+1, z ).height() =
				Moo::TerrainBlock::Pole( *bRight, 1, z ).height();
		}
	}

	// And get the four corners from the diagonally opposite chunks
	//  ('coz the other ones may not have updated their caches yet)
	for (int x = -1; x <= 1; x+=2) for (int z = -1; z <= 1; z+=2)
	{
		Moo::TerrainBlockPtr bCorner = Moo::TerrainBlock::findOutsideBlock(
			pChunk_->transform().applyPoint( cen + Vector3(delt.x*x,0,delt.z*z))
			).pBlock_;
		if (!bCorner) continue;

		// first fix up the furthest away (always unseen) pole
		int xi = x<0 ? -1 : w+1;
		int zi = z<0 ? -1 : w+1;
		Moo::TerrainBlock::Pole ourhp( *petb, xi, zi );
		Moo::TerrainBlock::Pole othhp( *bCorner,
			xi + (x<0?w:-w), zi + (z<0?w:-w) );
		ourhp.height() = othhp.height();

		// now fix up the nearer one on the right since we don't own it
		if (x>0)
		{
			ourhp.move(-1,0);	othhp.move(-1,0);
			ourhp.height() = othhp.height();
			ourhp.move(1,0);	othhp.move(1,0);
		}
		// and the nearer one on the back since we don't own it
		if (z>0)
		{
			ourhp.move(0,-1);	othhp.move(0,-1);
			ourhp.height() = othhp.height();
			ourhp.move(0,1);	othhp.move(0,1);
		}
		// and don't forget the back right corner.
		if (x>0 && z>0)
		{
			ourhp.move(-1,-1);	othhp.move(-1,-1);
			ourhp.height() = othhp.height();
			// don't bother moving pole back...
		}
	}

	// anf finally fix our bounding box
	this->calculateBB();

}

void EditorChunkTerrain::draw()
{
	if( !edShouldDraw() )
		return;
	if (block_->allHoles())
		return;

	// Draw using our transform, to show when the user is dragging the block around
	Matrix m = transform_;
	//m.setTranslate( m[3] - Vector3( 50.f, 0.f, 50.f ) );
	m.setTranslate( m[3] - Vector3( 49.5f, 0.f, 49.5f ) );

	m.preMultiply( Moo::rc().world() );

	bool isWriteable = EditorChunkCache::instance( *chunk() ).edIsWriteable();

	static int renderMiscShadeReadOnlyAreas = 0;
	static uint32 s_settingsMark_ = -16;
	if (Moo::rc().frameTimestamp() != s_settingsMark_)
	{
		renderMiscShadeReadOnlyAreas =
			Options::getOptionInt( "render/misc/shadeReadOnlyAreas", 1 );
		s_settingsMark_ = Moo::rc().frameTimestamp();
	}

	bool shadeReadOnlyBlocks = renderMiscShadeReadOnlyAreas != 0;
	shadeReadOnlyBlocks &= !!Options::getOptionInt("render/misc", 0);
	bool projectModule = ProjectModule::currentInstance() == ModuleManager::instance().currentModule();
	if( !isWriteable && BigBang::instance().drawSelection() )
		return;
	if (!isWriteable && shadeReadOnlyBlocks && !projectModule )
	{
		BigBang::instance().addReadOnlyBlock( m, block_ );
	}
	else
	{
		// Add the terrain block to the terrain's drawlist.
		Moo::TerrainRenderer::instance().addBlock( m, &*block_ );
	}

	if (s_debugShadowRays)
	{
		for (uint32 i=0; i<s_lines.size(); i++)
		{
			LineHelper::instance().addWorldSpace(s_lines[i], s_lineEnds[i], i % 2 ? 0xffffffff : 0xffff8000 );
		}
		LineHelper::instance().purge();
		s_lines.clear();
		s_lineEnds.clear();
	}
}

namespace
{
#define SHADOW_RAY_ERROR_OFFSET 0.001f

	class ShadowObstacleCatcher : public CollisionCallback
	{
	public:
		ShadowObstacleCatcher() : hit_( false ) {}

		void direction( const Vector3& dir )	{ dir_ = dir; }
		bool hit() { return hit_; }
		void hit( bool h ) { hit_ = h; }
		float dist() const	{	return dist_;	}
	private:
		virtual int operator()( const ChunkObstacle & obstacle,
			const WorldTriangle & triangle, float dist )
		{
			dist_ = dist;
			if (!(triangle.flags() & TRIANGLE_TERRAIN))
			{
				// Don't collide with models that are really close
				//if (dist < .5f)
				//	return COLLIDE_ALL;

				// protect vs. deleted items
				if (!obstacle.pItem()->chunk())
					return COLLIDE_ALL;				

				// Make sure it's a model, ie, we don't want entities casting
				// shadows
				/*DataSectionPtr ds = obstacle.pItem()->pOwnSect();
				if (!ds || ds->sectionName() != "model")
					return COLLIDE_ALL;*/

				if( !obstacle.pItem()->edAffectShadow() )
					return COLLIDE_ALL;

				// Check that we don't hit back facing triangles.				
				//Vector3 trin = obstacle.transform_.applyVector( triangle.normal() );
				//if ( trin.dotProduct( dir_ ) >= 0.f )
				//	return COLLIDE_ALL;
			}

			// Transparent BSP does not cast shadows
			if( triangle.flags() & TRIANGLE_TRANSPARENT )
				return COLLIDE_ALL;

			hit_ = true;

			return COLLIDE_BEFORE;
		}

		bool hit_;
		float dist_;
		Vector3 dir_;
	};

	std::pair<int, Vector3> getMinVisibleAngle0( Chunk* pChunk, Moo::TerrainBlock::Pole& pole, const Vector3& lastPos, bool reverse )
	{
		// cast up to 256 rays, from horiz over a 180 deg range, finding the 1st one 
		// that doesn't collide (do it twice starting from diff directions each time)

		// We've gotta add a lil to the z, or when checking along chunk boundaries
		// we don't hit the terrain block
		Vector3 polePos( pole.x() * pole.block().spacing(), pole.height() + 0.1f, pole.z() * pole.block().spacing() + 0.0001f);

		// apply the chunk's xform to polePos to get it's world pos
		polePos = pChunk->transform().applyPoint( polePos );

		// adding an offset to avoid errors in the collisions at chunk seams
		polePos.z += SHADOW_RAY_ERROR_OFFSET;

		Vector3 lastHit = lastPos;
		float xdiff = reverse ? polePos.x - lastPos.x : lastPos.x - polePos.x;
		float ydiff = lastPos.y - polePos.y;
		bool valid = lastPos.y > polePos.y;
		float lastAngle = atan2f( ydiff, xdiff );
		if( lastAngle < 0 )
			lastAngle += MATH_PI / 2;

		for (int i = 0; i < 256; i++)
		{
			// sun travels along the x axis (thus around the z axis)

			// Make a ray that points the in correct direction
			float angle = MATH_PI * (i / 255.f);

			if( valid && angle < lastAngle )
				continue;

			Vector3 ray ( cosf( angle ), sinf( angle ), 0.f );

			if (reverse)
				ray.x *= -1;

			if (s_debugShadowRays)
			{
				s_lines.push_back( polePos );
				s_lineEnds.push_back( polePos + (ray * MAX_TERRAIN_SHADOW_RANGE) );
			}

			ShadowObstacleCatcher soc;
			soc.direction(ray);
			ChunkManager::instance().cameraSpace()->collide( 
				polePos,
				polePos + (ray * MAX_TERRAIN_SHADOW_RANGE),
				soc );

			if (!soc.hit())
				return std::make_pair( i, lastHit );
			else
				lastHit = Vector3( polePos + soc.dist() * ray );
		}

		return std::make_pair( 0xff, lastHit );
	}

	std::pair<int, Vector3> getMinVisibleAngle1( Chunk* pChunk, Moo::TerrainBlock::Pole& pole, const Vector3& lastPos, bool reverse )
	{
		// cast up to 256 rays, from horiz over a 90 deg range, finding the 1st one 
		// that doesn't collide (do it twice starting from diff directions each time)

		// We've gotta add a lil to the z, or when checking along chunk boundaries
		// we don't hit the terrain block
		Vector3 polePos( pole.x() * pole.block().spacing(), pole.height() + 0.1f, pole.z() * pole.block().spacing() + 0.0001f);

		// apply the chunk's xform to polePos to get it's world pos
		polePos = pChunk->transform().applyPoint( polePos );

		// adding an offset to avoid errors in the collisions at chunk seams
		polePos.z += SHADOW_RAY_ERROR_OFFSET;

		Vector3 lastHit = lastPos;
		float xdiff = reverse ? polePos.x - lastPos.x : lastPos.x - polePos.x;
		float ydiff = lastPos.y - polePos.y;
		bool valid = lastPos.y > polePos.y;
		float lastAngle = atan2f( ydiff, xdiff );
		if( lastAngle < 0 )
			lastAngle += MATH_PI / 2;

		for (int i = 0; i < 256; i++)
		{			
			// sun travels along the x axis (thus around the z axis)

			// Make a ray that points the in correct direction
			float angle = MATH_PI * (i / 255.f);			

			//if( valid && angle < lastAngle )
			//	continue;

			float nRays = max(1.f, (255 - i) / s_raySubdivision);
			float dAngle = ((MATH_PI/2.f) - angle) / nRays;
			float dDispl = (MAX_TERRAIN_SHADOW_RANGE / nRays) * s_rayDisplacement;

			Vector3 start( polePos );
			ShadowObstacleCatcher soc;			
			
			for (int r=0; r < nRays; r++)
			{
				angle = (MATH_PI * (i/255.f)) + (r * dAngle);
				Vector3 ray ( cosf(angle), sinf(angle), 0.f );

				if (reverse)
					ray.x *= -1;				

				if (s_debugShadowRays)
				{
					s_lines.push_back( start );
					s_lineEnds.push_back( start + (ray * dDispl) );
				}

				soc.direction(ray);
				ChunkManager::instance().cameraSpace()->collide( 
					start,
					start + (ray * dDispl),
					soc );

				if (soc.hit())
				{
					lastHit = start + soc.dist() * ray;
					break;
				}

				//always start slightly back along the line, so as
				//not to create a miniscule break in the line (i mean curve)
				start += (ray * dDispl * 0.999f);
			}			

			if (!soc.hit())
			{				
				return std::make_pair( i, lastHit );
			}			
		}

		return std::make_pair( 0xff, lastHit );
	}

}


/**
 *  This is called to recalculate the self-shadowing of the terrain.
 *
 *	@param canExitEarly		If true then early exit due to a change in the 
 *							working chunk is allowed.  If false then the shadow 
 *							must be fully calculated.
 */
void EditorChunkTerrain::calculateShadows(bool canExitEarly /*=true*/)
{
	Chunk* pChunk = chunk();

	MF_ASSERT( pChunk->online() );

	TRACE_MSG( "Calculating shadows for chunk %s\n", pChunk->identifier().c_str() );

	for (int z = -1; z < block_->blocksWidth() + 2; z++)
	{		
		Vector3 lastPos( 0.f, -2000000.f, 0.f );
		for (int x = block_->blocksWidth() + 1; x >= -1; x--)		
		{			
			Moo::TerrainBlock::Pole pole( *block_, x, z );

			std::pair<int, Vector3> result = s_collType ?  getMinVisibleAngle1( pChunk, pole, lastPos, false ) : getMinVisibleAngle0( pChunk, pole, lastPos, false );
			int posAngle = result.first;
			lastPos = result.second;

			MF_ASSERT(posAngle >= 0 && posAngle <= 255);

			uint16& shadow = pole.shadow();
			shadow = ( shadow & 0xff00 ) | (255 - posAngle);
			BigBang::instance().fiberPause();
			if( canExitEarly && !BigBang::instance().isWorkingChunk( pChunk ) )
				return;			
		}
		lastPos = Vector3( 0.f, -2000000.f, 0.f );
		for (int x = -1; x < block_->blocksWidth() + 2; x++)		
		{			
			Moo::TerrainBlock::Pole pole( *block_, x, z );

			std::pair<int, Vector3> result = s_collType ?  getMinVisibleAngle1( pChunk, pole, lastPos, true ) : getMinVisibleAngle0( pChunk, pole, lastPos, true );
			int negAngle = result.first;
			lastPos = result.second;

			negAngle = 255 - negAngle;

			MF_ASSERT(negAngle >= 0 && negAngle <= 255);

			uint16& shadow = pole.shadow();
			shadow = ((255 - negAngle) << 8) | (shadow & 0xff);
			BigBang::instance().fiberPause();
			if( canExitEarly && !BigBang::instance().isWorkingChunk( pChunk ) )
				return;			
		}		
	}

	BigBang::instance().changedTerrainBlock( pChunk, /*rebuildNavmesh=*/false );
	BigBang::instance().chunkShadowUpdated( pChunk );

	block_->deleteManagedObjects();
	block_->createManagedObjects();
}

class BlockState
{
public:
	BlockState( EditorTerrainBlockPtr block )
	{
		extractPoleData( block, data_ );
	}

	struct PoleData
	{
		PoleData( const Moo::TerrainBlock::Pole& p )
			: height_( p.height() )
			, blend_( p.blend() )
			, shadow_( p.shadow() )
		{
			int bi = p.x() + p.z() * p.block().blocksWidth();

			if (bi < 0 || bi >= p.block().blocksWidth() * p.block().blocksHeight())
			{
				hole_ = false;
			}
			else
			{
				hole_ = p.hole();
			}
		}

		void apply( Moo::TerrainBlock::Pole& p ) const
		{
			p.height() = height_;
			p.blend() = blend_;
			p.shadow() = shadow_;

			int bi = p.x() + p.z() * p.block().blocksWidth();

			if (bi >= 0 && bi < p.block().blocksWidth() * p.block().blocksHeight())
				p.hole( hole_ );
		}

		bool operator==( const PoleData& rhs ) const
		{
			return height_ == rhs.height_ && blend_ == rhs.blend_
				&& shadow_ == rhs.shadow_ && hole_ == rhs.hole_;
		}

		float height_;
		uint32 blend_;
		uint16 shadow_;
		bool hole_;
	};

	std::vector<PoleData>	data_;

	void extractPoleData( EditorTerrainBlockPtr block, std::vector<PoleData>& data )
	{
		int poleCount = block->polesWidth() * block->polesHeight();
		data.clear();
		data.reserve( poleCount );
		for (int i = 0; i < poleCount; ++i)
		{
			data_.push_back( PoleData( Moo::TerrainBlock::Pole( *block, i ) ) );
		}
	}

	void applyPoleData( EditorTerrainBlockPtr block )
	{
		applyPoleData( block, data_ );
	}

	void applyPoleData( EditorTerrainBlockPtr block, const std::vector<PoleData>& data )
	{
		MF_ASSERT( data.size() == block->polesWidth() * block->polesHeight() );
		for (int i = 0; i < (block->polesWidth() * block->polesHeight()); ++i)
		{
			data[i].apply( Moo::TerrainBlock::Pole( *block, i ) );
		}
	}
};


class BlockUndo : public UndoRedo::Operation
{
public:
	BlockUndo( EditorTerrainBlockPtr pBlock, ChunkPtr pChunk ) :
		UndoRedo::Operation( int(typeid(BlockUndo).name()) ),
		pBlock_( pBlock ),
		pChunk_( pChunk ),
		state_( pBlock )
	{
		addChunk( pChunk_ );
	}

	virtual void undo()
	{
		// first add the current state of this block to the undo/redo list
		UndoRedo::instance().add( new BlockUndo( pBlock_, pChunk_ ) );

		// now apply our stored change
		state_.applyPoleData( pBlock_ );

		// and let everyone know it's changed
		BigBang::instance().changedTerrainBlock( pChunk_ );
		pBlock_->deleteManagedObjects();
		pBlock_->createManagedObjects();
	}

	virtual bool iseq( const UndoRedo::Operation & oth ) const
	{
		return state_.data_ == static_cast<const BlockUndo&>( oth ).state_.data_;
	}

private:
	EditorTerrainBlockPtr	pBlock_;
	ChunkPtr				pChunk_;
	BlockState				state_;
};

bool EditorChunkTerrain::edTransform( const Matrix & m, bool transient )
{
	Matrix newWorldPos;
	newWorldPos.multiply( m, pChunk_->transform() );


	// it's permanent, so find out where we belong now
	Chunk * pOldChunk = pChunk_;
	Chunk * pNewChunk = EditorChunk::findOutsideChunk( newWorldPos.applyToOrigin() );
	if (pNewChunk == NULL)
		return false;

	// if this is only a temporary change, do nothing
	if (transient)
	{
		transform_ = m;
		return true;
	}

	EditorChunkTerrain* newChunkTerrain = (EditorChunkTerrain*)
		ChunkTerrainCache::instance( *pNewChunk ).pTerrain();

	// Don't allow two terrain blocks in a single chunk
	// We allow an exception for ourself to support tossing into our current chunk
	// We allow an exception for a selected item as it implies we're moving a group of
	// terrain items in a direction, and the current one likely won't be there any longer.
	if (newChunkTerrain && newChunkTerrain != this &&
			!BigBang::instance().isItemSelected( newChunkTerrain ) )
		return false;


	// make sure the chunks aren't readonly
	if (!EditorChunkCache::instance( *pOldChunk ).edIsWriteable() 
		|| !EditorChunkCache::instance( *pNewChunk ).edIsWriteable())
		return false;

	transform_ = Matrix::identity;
	//transform_.setTranslate( 50.f, 0.f, 50.f );
	transform_.setTranslate( 49.5f, 0.f, 49.5f );

	// note that both affected chunks have seen changes
	BigBang::instance().changedChunk( pOldChunk );
	BigBang::instance().changedChunk( pNewChunk );

	BigBang::instance().markTerrainShadowsDirty( pOldChunk );
	BigBang::instance().markTerrainShadowsDirty( pNewChunk );

	// and move ourselves into the right chunk. we have to do this
	// even if it's the same chunk so the col scene gets recreated
	pOldChunk->delStaticItem( this );
	pNewChunk->addStaticItem( this );


	// Ok, now apply any rotation

	// Calculate rotation angle
	Vector3 newPt = m.applyVector( Vector3( 1.f, 0.f, 0.f ) );
	newPt.y = 0.f;

	float angle = acosf( newPt.dotProduct( Vector3( 1.f, 0.f, 0.f ) ) );

	if (!almostZero( angle, 0.0001f) )
	{
		// dp will only give an angle up to 180 degs, make it from 0..2PI
		if (newPt.crossProduct(Vector3( 1.f, 0.f, 0.f )).y > 0.f)
			angle = (2 * MATH_PI) - angle;
	}

	

	// Turn the rotation angle into an amount of 90 degree rotations
	int rotateAmount = 0;

	if (angle > (DEG_TO_RAD(90.f) / 2.f))
		++rotateAmount;
	if (angle > (DEG_TO_RAD(90.f) + (DEG_TO_RAD(90.f) / 2.f)))
		++rotateAmount;
	if (angle > (DEG_TO_RAD(180.f) + (DEG_TO_RAD(90.f) / 2.f)))
		++rotateAmount;
	if (angle > (DEG_TO_RAD(270.f) + (DEG_TO_RAD(90.f) / 2.f)))
		rotateAmount = 0;

	if (rotateAmount > 0)
	{
		MF_ASSERT( rotateAmount < 4 );

		UndoRedo::instance().add( new BlockUndo( &block(), pChunk_ ) );

		// Rotate terrain 90 degress rotateAmount amount of times

		BlockState dst( &block() );

		int width = block().polesWidth();
		int height = block().polesHeight();

		int halfWidth = width / 2;
		int halfHeight = height / 2;

		MF_ASSERT( dst.data_.size() == width * height );

		for (int r = 0; r < rotateAmount; ++r)
		{
			BlockState src( &block() );

			MF_ASSERT( src.data_.size() == dst.data_.size() );

			for (int x = 0; x < width; ++x)
			{
				for (int z = 0; z < height; ++z)
				{
					// Not pretty, but not complex - just working out the dst
					// position of the height pole.

					// Worse than it could be as the amount of poles in even
					// and we only deal with integer coords, so there's no pole
					// at the centre of the block.

					int offsetX;
					if (x < halfWidth)
						offsetX = x - halfWidth;
					else
						offsetX = x - (halfWidth - 1);

					int offsetZ;
					if (z < halfHeight)
						offsetZ = z - halfHeight;
					else
						offsetZ = z - (halfHeight - 1);


					int dstOffsetX = -offsetZ;
					int dstOffsetZ = offsetX;

					int dstX;
					if (dstOffsetX > 0)
						dstX = dstOffsetX + halfWidth - 1;
					else
						dstX = dstOffsetX + halfWidth;

					int dstZ;
					if (dstOffsetZ > 0)
						dstZ = dstOffsetZ + halfHeight - 1;
					else
						dstZ = dstOffsetZ + halfHeight;

					int dstPos = dstX * width + dstZ;

					int srcPos = x * width + z;

					MF_ASSERT( dstPos >= 0 && dstPos < (width * height) );
					MF_ASSERT( srcPos >= 0 && srcPos < (width * height) );


					dst.data_[dstPos] = src.data_[srcPos];
				}
			}

			dst.applyPoleData( &block() );
		}


		BigBang::instance().changedTerrainBlock( pChunk_ );
		block().deleteManagedObjects();
		block().createManagedObjects();
	}

	return true;
}

class TerrainItemMatrix : public ChunkItemMatrix
{
public:
	TerrainItemMatrix( ChunkItemPtr pItem ) : ChunkItemMatrix( pItem )
	{
		// Remove snaps, we don't want them as we're pretending the origin is
		// at 50,0,50
		movementSnaps( Vector3( 0.f, 0.f, 0.f ) );
	}

	/**
	 *	Overriding base class to not check the bounding box is < 100m, we just
	 *	don't care for terrain items
	 */
	bool EDCALL setMatrix( const Matrix & m )
	{
		pItem_->edTransform( m, true );
	}
};

/**
 *	This method adds this item's properties to the given editor
 */
bool EditorChunkTerrain::edEdit( ChunkItemEditor & editor )
{
	//PJ - not really any need to move or rotate terrain blocks.

	//MatrixProxy * pMP = new TerrainItemMatrix( this );
	//editor.addProperty( new GenPositionProperty( "position", pMP ) );
	//editor.addProperty( new GenRotationProperty( "rotation", pMP ) );

	return true;
}

void EditorChunkTerrain::edBounds( BoundingBox& bbRet ) const
{
	bbRet.setBounds(	bb_.minBounds() + Vector3( -49.5f, -0.25f, -49.5f ),
						bb_.maxBounds() + Vector3( -50.5f, 0.25f, -50.5f ) );
}

void EditorChunkTerrain::edPostClone( EditorChunkItem* srcItem )
{
	// Can't clone outside chunks, and terrain can only be in outside chunks,
	// hence this can only be null
	//MF_ASSERT( srcItem );

	// The exception being if we're being placed from a prefab, that'll call
	// this with NULL
}

BinaryPtr EditorChunkTerrain::edExportBinaryData()
{
	// Return a copy of the terrain block
	BWResource::instance().purge( "export_temp.terrain" );

	block().save( "export_temp.terrain" );

	BinaryPtr bp;
	{
		DataSectionPtr ds = BWResource::openSection( "export_temp.terrain" );
		bp = ds->asBinary();
	}
	
	std::string fname = BWResource::resolveFilename( "export_temp.terrain" );
	::DeleteFile( fname.c_str() );

	return bp;
}

/**
 *	The 200 y value is pretty much just for prefabs, so that when placing it,
 *	it'll most likely end up at y = 0.f, if it's anything else items will not
 *	be sitting on the ground.
 */
Vector3 EditorChunkTerrain::edMovementDeltaSnaps()
{
	return Vector3( 100.f, 200.f, 100.f);
}

float EditorChunkTerrain::edAngleSnaps()
{
	return 90.f;
}


bool EditorChunkTerrain::s_shouldDraw_ = true;
uint32 EditorChunkTerrain::s_settingsMark_ = -16;


// Use macros to write EditorChunkTerrain's static create method
#undef IMPLEMENT_CHUNK_ITEM_ARGS
#define IMPLEMENT_CHUNK_ITEM_ARGS (pSection, pChunk, &errorString)
IMPLEMENT_CHUNK_ITEM( EditorChunkTerrain, terrain, 1 )



// -----------------------------------------------------------------------------
// Section: EditorChunkTerrainCache
// -----------------------------------------------------------------------------




EditorChunkTerrainCache::EditorChunkTerrainCache()
{
}


EditorChunkTerrainCache& EditorChunkTerrainCache::instance()
{
	static EditorChunkTerrainCache s_cache;

	return s_cache;
}


EditorChunkTerrainPtr EditorChunkTerrainCache::findChunkFromPoint(
		const Vector3& pos )
{
	SimpleMutexHolder terrainChunksMutexHolder(terrainChunksMutex_);

	EditorTerrainChunkMap::iterator it = 
		terrainChunks_.find( this->chunkName(pos) );

	if ( it != terrainChunks_.end() )
	{
		return it->second;
	}

	return NULL;
}


const std::string& EditorChunkTerrainCache::chunkName( const Vector3& pos )
{
	static std::string s_chunkName;

	int x = (int) floorf(pos.x/100.f);
	int y = (int) floorf(pos.z/100.f);

	char	chunkName[64];
	sprintf( chunkName, "%04X%04Xo", int(uint16( x )), int(uint16( y )) );

	s_chunkName = chunkName;

	return s_chunkName;
}


int	EditorChunkTerrainCache::findChunksWithinBox(
		const BoundingBox& bb,
		EditorChunkTerrainPtrs& outVector )
{
	int startX = (int) floorf( bb.minBounds().x/100.f );
	int endX = (int) floorf( bb.maxBounds().x/100.f );

	int startZ = (int) floorf( bb.minBounds().z/100.f );
	int endZ = (int) floorf( bb.maxBounds().z/100.f );

	for ( int x = startX; x <= endX; x++ )
	{
		for ( int z = startZ; z <= endZ; z++ )
		{
			if ( ChunkManager::instance().cameraSpace() )
			{
				ChunkSpace::Column* pColumn =
					ChunkManager::instance().cameraSpace()->column( Vector3( x * 100.f + 50.f, bb.minBounds().y, z * 100.f + 50.f ), false );

				if ( pColumn )
				{
					if ( pColumn->pOutsideChunk() )
					{
						outVector.push_back( static_cast<EditorChunkTerrain*>( ChunkTerrainCache::instance( *pColumn->pOutsideChunk() ).pTerrain() ) );
					}
				}
			}
		}
	}

	return outVector.size();
}


void EditorChunkTerrainCache::add(
		const std::string& name,
		EditorChunkTerrainPtr pChunkItem )
{
	SimpleMutexHolder terrainChunksMutexHolder(terrainChunksMutex_);

	terrainChunks_.insert( std::make_pair( name, pChunkItem ) );
}

void EditorChunkTerrainCache::del(
		const std::string& name )
{
	SimpleMutexHolder terrainChunksMutexHolder(terrainChunksMutex_);

	terrainChunks_.erase( name.c_str() );
}


/**
 *	Section - TerrainBlockIndicesNoHoles
 */

/**
 *	Constructor.
 */
TerrainBlockIndicesNoHoles::TerrainBlockIndicesNoHoles()
{
}

/**
 *	Singleton instance accessor.
 */
TerrainBlockIndicesNoHoles& TerrainBlockIndicesNoHoles::instance()
{
	static TerrainBlockIndicesNoHoles s_instance;
	return s_instance;
}

/**
 *	Destructor.
 */
TerrainBlockIndicesNoHoles::~TerrainBlockIndicesNoHoles()
{
	IndicesMap::iterator it = indices_.begin();
	IndicesMap::iterator end = indices_.end();
	while ( it != end )
	{
		delete it->second;
		it++;
	}
}

/**
 *	This method sets the index buffer onto the device.
 *	If an index buffer doesn't yet exist for the given dimensions,
 *	one is created.
 *
 *	@param blocksHeight	 - must be the same as the terrain block
 *	@param blocksWidth	 - must be the same as the terrain block
 *	@param verticesWidth - must be the same as the terrain block
 *
 *	@return nIndices	 - the number of indices to be drawn
 */
uint32 TerrainBlockIndicesNoHoles::setIndexBuffer( uint32 blocksHeight, uint32 blocksWidth, uint32 verticesWidth )
{
	TerrainBlockIndicesNoHoles::Dimensions d(blocksHeight,blocksWidth,verticesWidth);

	IndicesMap::iterator it = indices_.find( d );

	if ( it == indices_.end() )
	{
		indices_.insert( std::make_pair(d, new Indices(d)) );
		it = indices_.find( d );
		MF_ASSERT(it != indices_.end());
	}

	Indices* indices = it->second;
	return indices->setIndexBuffer();
}

/**
 *	This method is an implementation of a DeviceCallback interface.
 *	It simply enpasses the event to all known indices objects.
 */
void TerrainBlockIndicesNoHoles::createManagedObjects()
{
	IndicesMap::iterator it = indices_.begin();
	IndicesMap::iterator end = indices_.end();
	while ( it != end )
	{
		it->second->createManagedObjects();
		it++;
	}
}


/**
 *	This method is an implementation of a DeviceCallback interface.
 *	It simply enpasses the event to all known indices objects.
 */
void TerrainBlockIndicesNoHoles::deleteManagedObjects()
{
	IndicesMap::iterator it = indices_.begin();
	IndicesMap::iterator end = indices_.end();
	while ( it != end )
	{
		it->second->deleteManagedObjects();
		it++;
	}
}


/**
 *	Constructor.
 */
TerrainBlockIndicesNoHoles::Indices::Indices(const Dimensions& d):
dimensions_( d ),
nIndices_( 0 ),
nPrimitives_( 0 )
{}


/**
 *	This method is an implementation of a DeviceCallback interface.
 *	It creates device dependent objects.
 */
void TerrainBlockIndicesNoHoles::Indices::createManagedObjects()
{
	MF_ASSERT( !indexBuffer_.valid() )

	if (Moo::rc().device())
	{
		// Create the indices, the indices are made up of one big strip with degenerate
		// triangles where there is a break in the strip, breaks occur on the edges of the
		// strip and where a hole is encountered.

		std::vector<uint16> indices;
		bool instrip = false;
		uint16 lastIndex = 0;

		for (uint32 z = 0; z < dimensions_.blocksHeight_; z++)
		{
			for (uint32 x = 0; x < dimensions_.blocksWidth_; x++)
			{
				if (instrip)
				{
					indices.push_back( uint16( x + 1 + ((z + 1) * dimensions_.verticesWidth_) ) );
					indices.push_back( uint16( x + 1 + (z * dimensions_.verticesWidth_) ) );
				}
				else
				{
					if (indices.size())
						indices.push_back( indices.back() );
					indices.push_back( uint16( x + ((z + 1) * dimensions_.verticesWidth_) ) );
					indices.push_back( indices.back() );
					indices.push_back( uint16( x + (z * dimensions_.verticesWidth_) ) );
					indices.push_back( uint16( x + 1 + ((z + 1) * dimensions_.verticesWidth_) ) );
					indices.push_back( uint16( x + 1 + (z * dimensions_.verticesWidth_) ) );
					instrip = true;
				}
			}
			instrip = false;
		}

		// Create the index buffer and fill it with the generated indices.
		if (indices.size())
		{
			nIndices_ = indices.size();
			nPrimitives_ = nIndices_ - 2;

			// on the D3DFMT_INDEX16 being used here
			// it is possible that the terrain vertex count will exceed 65536 when we use larger terrain
			// however, the whole chunk of code here are not compatible with 32 bits index buffer,
			// just see the std::vector<uint16> 
			HRESULT hr = indexBuffer_.create( nIndices_, D3DFMT_INDEX16, D3DUSAGE_WRITEONLY, D3DPOOL_MANAGED );
			if (SUCCEEDED( hr ))
			{
				Moo::IndicesReference index = indexBuffer_.lock();
				index.fill( &indices.front(), nIndices_ );
				indexBuffer_.unlock();
			}
			else
			{
				CRITICAL_MSG( "Moo::TerrainBlockIndicesNoHoles::createManagedObjects: unable to create index buffer\n" );
			}
		}
	}
}


/**
 *	This method is an implementation of a DeviceCallback interface.
 *	It deletes device dependent objects.
 */
void TerrainBlockIndicesNoHoles::Indices::deleteManagedObjects()
{
	if (indexBuffer_.valid())
	{
		memoryClaim( indexBuffer_ );
		indexBuffer_.release();
	}
}


/**
 *	This method sets the index buffer onto the device.
 */
uint32 TerrainBlockIndicesNoHoles::Indices::setIndexBuffer()
{
	if ( !indexBuffer_.valid() )
		this->createManagedObjects();
	if (!indexBuffer_.valid() )
		return 0;
	indexBuffer_.set();
	return nIndices_;
}

// editor_chunk_terrain.cpp