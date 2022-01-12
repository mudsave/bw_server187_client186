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
#include "terrain_block.hpp"

#include "cstdmf/memory_counter.hpp"
#include "resmgr/bwresource.hpp"
#include "texture_manager.hpp"
#include "terrain_texture_setter.hpp"

#ifndef CODE_INLINE
#include "terrain_block.ipp"
#endif

DECLARE_DEBUG_COMPONENT2( "Moo", 0 )

memoryCounterDefine( terrainAc, Environment )

namespace Moo
{

float TerrainBlock::penumbraSize_ = 0.1f;

float TerrainBlock::penumbraSize()
{
	return penumbraSize_;
}

void TerrainBlock::penumbraSize( float f )
{
	penumbraSize_ = f;
}

// -----------------------------------------------------------------------------
// Section: Construction/Destruction
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
TerrainBlock::TerrainBlock() :
	nVertices_( 0 ),
	nIndices_( 0 ),
	nPrimitives_( 0 )
{
}


/**
 *	Destructor.
 */
TerrainBlock::~TerrainBlock()
{
	this->deleteManagedObjects();

	memoryCounterSub( terrainAc );
	memoryClaim( heightMap_ );
	memoryClaim( blendValues_ );
	memoryClaim( shadowValues_ );
	memoryClaim( holes_ );
	memoryClaim( detailIDs_ );
}

// -----------------------------------------------------------------------------
// Section: Rendering
// -----------------------------------------------------------------------------

/**
 * This method draws the terrain block.
 * @param tts optional TerrainTextureSetter object to set textures on the effect
 */
void TerrainBlock::draw( TerrainTextureSetter* tts )
{
	// Allocate d3d resources if they have not been allocated already.
	if (!vertexBuffer_.pComObject() || !indexBuffer_.pComObject())
	{
		createManagedObjects();

		if (!vertexBuffer_.pComObject() || !indexBuffer_.pComObject())
			return;
	}	
	if (rc().device() && !allHoles_)
	{
		if (tts) tts->setTextures(textures_);		

		// Set up buffers and draw.
		rc().setIndices( &*indexBuffer_ );
		rc().device()->SetStreamSource( 0, &*vertexBuffer_, 0, sizeof( VertexXYZNDS ) );
		rc().device()->DrawIndexedPrimitive( D3DPT_TRIANGLESTRIP,
				0, 0, nVertices_, 0, nPrimitives_ );
		rc().addToPrimitiveCount( nPrimitives_ );
	}
}


// -----------------------------------------------------------------------------
// Section: Loading
// -----------------------------------------------------------------------------

/**
 * Implementation of DeviceCallback::createManagedObjects.
 * This method creates the vertex and index buffers.
 */
void TerrainBlock::createManagedObjects()
{
	noHoles_ = true;
	allHoles_ = false;

	if (rc().device())
	{
		// Create the vertex buffer.
		DX::VertexBuffer* pVBuffer = NULL;
		nVertices_ = verticesWidth_ * verticesHeight_;

		HRESULT hr = rc().device()->CreateVertexBuffer( nVertices_ * sizeof( VertexXYZNDS ),
						D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &pVBuffer, NULL );
		if (SUCCEEDED( hr ))
		{
			// Fill the vertex buffer with the heights from the heightmap,
			// the blendvalues from the blend map, and the shadow values from
			// the shadowmap, and generate x and z positions.
			VertexXYZNDS* vertices = NULL;

			hr = pVBuffer->Lock( 0, nVertices_ * sizeof( VertexXYZNDS ),
					(void**)&vertices, 0 );

			if (SUCCEEDED(hr))
			{
				float zPos = 0;
				for (uint32 z = 0; z < verticesHeight_; z++)
				{
					float xPos = 0;
					for (uint32 x = 0; x < verticesWidth_; x++ )
					{
						vertices->pos_.set( xPos, heightPole( x, z ), zPos );
						vertices->normal_ = heightPoleNormal( x, z );
						vertices->colour_ = heightPoleBlend( x, z );
						vertices->specular_ = heightPoleShadow( x, z );
						xPos += spacing_;
						vertices++;
					}
					zPos += spacing_;
				}
				pVBuffer->Unlock();
				vertexBuffer_ = pVBuffer;
			}
			else
			{
				nVertices_ = 0;
				CRITICAL_MSG( "Moo::TerrainBlock::createManagedObjects: Unable to lock vertex buffer, error code %lx\n", hr );
			}
			pVBuffer->Release();
			pVBuffer = NULL;
		}
		else
		{
			WARNING_MSG( "Moo::TerrainBlock::createManagedObjects: Unable to create vertex buffer, error code %lx\n", hr );
		}

		MF_ASSERT( nVertices_ < 65536 );// otherwise, we MUST revise the index buffer usage here
		// Create the indices, the indices are made up of one big strip with degenerate
		// triangles where there is a break in the strip, breaks occur on the edges of the
		// strip and where a hole is encountered.

		std::vector<uint16> indices;
		bool instrip = false;
		uint16 lastIndex = 0;

		for (uint32 z = 0; z < blocksHeight_; z++)
		{
			for (uint32 x = 0; x < blocksWidth_; x++)
			{
				if (instrip)
				{
					if (!holes_[ x + ( z * blocksWidth_ ) ])
					{
						indices.push_back( uint16( x + 1 + ((z + 1) * verticesWidth_) ) );
						indices.push_back( uint16( x + 1 + (z * verticesWidth_) ) );
					}
					else
					{
						instrip = false;
						noHoles_ = false;
					}
				}
				else
				{
					if (!holes_[ x + ( z * blocksWidth_ ) ])
					{
						if (indices.size())
							indices.push_back( indices.back() );
						indices.push_back( uint16( x + ((z + 1) * verticesWidth_) ) );
						indices.push_back( indices.back() );
						indices.push_back( uint16( x + (z * verticesWidth_) ) );
						indices.push_back( uint16( x + 1 + ((z + 1) * verticesWidth_) ) );
						indices.push_back( uint16( x + 1 + (z * verticesWidth_) ) );
						instrip = true;
					}
					else
					{
						noHoles_ = false;
					}
				}
			}
			instrip = false;
		}

		// Create the index buffer and fill it with the generated indices.
		if (indices.size())
		{
			nIndices_ = indices.size();
			nPrimitives_ = nIndices_ - 2;

			DX::IndexBuffer* pIBuffer;			
			hr = rc().device()->CreateIndexBuffer( sizeof( uint16 ) * nIndices_,
				D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED, &pIBuffer, NULL );
			if (SUCCEEDED( hr ))
			{
				uint16* indexData;
				hr = pIBuffer->Lock( 0, 0, (void**)(&indexData), 0 );
				if (SUCCEEDED( hr ))
				{
					memcpy( indexData, &indices.front(), sizeof( uint16 ) * nIndices_ );
					pIBuffer->Unlock();
					indexBuffer_ = pIBuffer;
				}
				else
				{
					CRITICAL_MSG( "Moo::TerrainBlock::createManagedObjects: unable to lock index buffer, error code %lx\n", hr );
				}
				pIBuffer->Release();
				pIBuffer = NULL;
			}
			else
			{
				WARNING_MSG( "Moo::TerrainBlock::createManagedObjects: unable to create index buffer, error code %lx\n", hr );
			}
		}
		else
		{
			//WARNING_MSG( "Moo::TerrainBlock::createManagedObjects: mesh contains nothing but holes\n" );
			allHoles_ = true;
		}
	}
}

/**
 * Implementation of DeviceCallback::deleteManagedObjects.
 * Removes the vertex and index buffers
 */
void TerrainBlock::deleteManagedObjects()
{
	memoryCounterSub( terrainAc );
	if (indexBuffer_ != NULL)
	{
		memoryClaim( indexBuffer_ );
		indexBuffer_ = NULL;
	}
	if (vertexBuffer_ != NULL)
	{
		memoryClaim( vertexBuffer_ );
		vertexBuffer_ = NULL;
	}
}


/**
 * This method creates and loads a terrain block from disk.
 * @param resourceName the name of the terrain resource to load.
 * @return smartpointer to the loaded terrain block, or NULL if the load failed.
 */
TerrainBlockPtr TerrainBlock::loadBlock( const std::string& resourceName )
{
	TerrainBlockPtr block = new TerrainBlock;

	if (block->load( resourceName ))
	{
		return block;
	}

	return NULL;
}


// -----------------------------------------------------------------------------
// Section: TerrainFinder
// -----------------------------------------------------------------------------

// TerrainBlock::TerrainFinder *	TerrainBlock::pTerrainFinder_ = NULL;

} //namespace Moo

// terrain_block.cpp
