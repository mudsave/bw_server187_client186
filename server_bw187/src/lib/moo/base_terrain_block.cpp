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
#include "base_terrain_block.hpp"

#include "cstdmf/memory_counter.hpp"
#include "material_kinds.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/auto_config.hpp"

#ifndef MF_SERVER
#include "texture_manager.hpp"
#endif // MF_SERVER

#ifndef CODE_INLINE
#include "base_terrain_block.ipp"
#endif

DECLARE_DEBUG_COMPONENT2( "Moo", 0 )

memoryCounterDeclare( terrainAc );

static AutoConfigString s_notFoundBmp( "system/notFoundBmp" );

namespace Moo
{
BaseTerrainBlock::Dummy BaseTerrainBlock::NO_TERRAIN;


// -----------------------------------------------------------------------------
// Section: Construction/Destruction
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
BaseTerrainBlock::BaseTerrainBlock() :
	width_( 0 ),
	height_( 0 ),
	referenceCount_( 0 ),
	noHoles_( false ),
	allHoles_( false )
{
}


/**
 *	Destructor.
 */
BaseTerrainBlock::~BaseTerrainBlock()
{
}


// -----------------------------------------------------------------------------
// Section: Loading
// -----------------------------------------------------------------------------
/**
 * This method loads a terrainblock.
 * @param resourceName the name of the terrainresource to load.
 * @return whether the load was successful or not.
 */
bool BaseTerrainBlock::load( const std::string& resourceName )
{
	// Open the binary resource of the terrain block.
	BinaryPtr pTerrainData =
			BWResource::instance().rootSection()->readBinary( resourceName );

	if (!pTerrainData) return false;

	bool res = false;

	// Read the header and calculate statistics for the terrain block.
	const TerrainBlockHeader* pBlockHeader =
		reinterpret_cast<const TerrainBlockHeader*>(pTerrainData->data());
	spacing_      = pBlockHeader->spacing_;
	width_        = pBlockHeader->heightMapWidth_;
	height_       = pBlockHeader->heightMapHeight_;
	detailWidth_  = pBlockHeader->detailWidth_;
	detailHeight_ = pBlockHeader->detailHeight_;

	blocksWidth_    = width_ - 3;
	blocksHeight_   = height_ - 3;
	verticesWidth_  = width_ - 2;
	verticesHeight_ = height_ - 2;

	// Load the texture names.
	const char* pTextureNames =
			reinterpret_cast< const char* >( pBlockHeader + 1 );
	uint32 nHeightValues =
			pBlockHeader->heightMapWidth_ * pBlockHeader->heightMapHeight_;

	for (uint32 i = 0; i < pBlockHeader->nTextures_; i++)
	{
#ifndef MF_SERVER
		BaseTexturePtr pTexture =
						TextureManager::instance()->get( pTextureNames );
		if (pTexture)
		{
			textures_.push_back( pTexture );
		}
		else
		{
			WARNING_MSG( "Moo::BaseTerrainBlock::Load: "
					"Unable to open texture resource %s in block: %s \n",
				pTextureNames, resourceName.c_str() );
			textures_.push_back(
				TextureManager::instance()->get( s_notFoundBmp ) );
		}
#endif // MF_SERVER

		materialKinds_.push_back(MaterialKinds::instance().get(pTextureNames));
		pTextureNames += pBlockHeader->textureNameSize_;
	}


	// Load the height values .
	const float* pHeights = reinterpret_cast< const float* >( pTextureNames );
	heightMap_.assign( pHeights, pHeights + nHeightValues );

	// Load the blend values.
	const uint32* pBlends =
			reinterpret_cast< const uint32* >( pHeights + nHeightValues );
	blendValues_.assign( pBlends, pBlends + nHeightValues );

	// Load the shadow values.
	const uint16* pShadows =
			reinterpret_cast< const uint16* >( pBlends + nHeightValues );
	shadowValues_.assign( pShadows, pShadows + nHeightValues );

	// load the terrain block holes.
	const bool* pHoles =
			reinterpret_cast< const bool* >( pShadows + nHeightValues );
	uint32 nTerrainSquares = blocksWidth_ * blocksHeight_;
	holes_.assign( pHoles, pHoles + nTerrainSquares );

	// load the detail
	if (detailWidth_ == 0)
	{
		//remove when terrain blocks have the right dimensions
		detailWidth_ = 10;
		detailHeight_ = 10;

		for ( int i = 0; i < 10; i++ )
		{
			for ( int j = 0; j < 10; j++ )
			{
				detailIDs_.push_back( rand() % 4 );
			}
		}
	}
	else
	{
		const uint8* detail =
				reinterpret_cast< const uint8* >( pHoles + nTerrainSquares );
		uint32 nDetailIDs = detailWidth_ * detailHeight_;
		detailIDs_.assign( detail, detail + nDetailIDs );
	}

	res = true;

	memoryCounterAdd( terrainAc );

	memoryClaim( heightMap_ );
	memoryClaim( blendValues_ );
	memoryClaim( shadowValues_ );
	memoryClaim( holes_ );
	memoryClaim( detailIDs_ );

	return res;
}



/**
 *	This is a static helper method that returns the terrain height at a given
 *	x/z position.
 */
float BaseTerrainBlock::getHeight(float x, float z)
{
	Vector3 pos( x, 0, z );
	FindDetails findDetails = BaseTerrainBlock::findOutsideBlock( pos );

	if (findDetails.pBlock_)
	{
		Vector3 terPos = findDetails.pInvMatrix_->applyPoint( pos );
		float height = findDetails.pBaseBlock_->heightAt( terPos[0], terPos[2] );

		if (height != NO_TERRAIN)
		{
			return height - findDetails.pInvMatrix_->applyToOrigin()[1];
		}
	}

	return NO_TERRAIN;
}


// -----------------------------------------------------------------------------
// Section: TerrainFinder
// -----------------------------------------------------------------------------

BaseTerrainBlock::TerrainFinder *	BaseTerrainBlock::pTerrainFinder_ = NULL;

} //namespace Moo

// base_terrain_block.cpp
