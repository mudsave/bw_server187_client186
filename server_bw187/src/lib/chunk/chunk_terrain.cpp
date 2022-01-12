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
#include "chunk_terrain.hpp"

#include "chunk.hpp"
#include "chunk_space.hpp"
#include "chunk_obstacle.hpp"
#include "chunk_manager.hpp"
#include "chunk_terrain_common.hpp"

#include "grid_traversal.hpp"

#include "physics2/hulltree.hpp"
#include "resmgr/datasection.hpp"
#include "moo/terrain_block.hpp"
#include "moo/terrain_renderer.hpp"
#include "moo/render_context.hpp"

#include "romp/water_scene_renderer.hpp"

#include "umbra_config.hpp"
#include "romp/line_helper.hpp"

#if UMBRA_ENABLE
#include <umbraModel.hpp>
#include <umbraObject.hpp>
#endif

#ifndef CODE_INLINE
#include "chunk_terrain.ipp"
#endif

int ChunkTerrain_token;

bool use_water_culling = true;


DECLARE_DEBUG_COMPONENT2( "Chunk", 0 )

// -----------------------------------------------------------------------------
// Section: ChunkTerrain
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
ChunkTerrain::ChunkTerrain() :
	ChunkItem( 1 ),
	bb_( Vector3::zero(), Vector3::zero() )
#if UMBRA_ENABLE
	, terrainVertices_()
	, terrainIndices_()
#endif // UMBRA_ENABLE
{
	static bool firstTime = true;
	if (firstTime)
	{
		MF_WATCH( "Render/Terrain/Use water culling",
				use_water_culling, Watcher::WT_READ_WRITE,
				"Perform extra culling for the terrain blocks in the water scene. " );
		firstTime=false;
	}
}


/**
 *	Destructor.
 */
ChunkTerrain::~ChunkTerrain()
{
}


/**
 *	Draw method
 */
void ChunkTerrain::draw()
{
	static DogWatch drawWatch( "ChunkTerrain" );
	ScopedDogWatch watcher( drawWatch );

	if (!block_->allHoles())
	{
		if (Moo::rc().reflectionScene() && use_water_culling)
		{
			float height = WaterSceneRenderer::currentScene()->waterHeight();

			Matrix world( pChunk_->transform() );
			BoundingBox bounds( this->bb() );

			//TODO: check to see if this transform is needed at all to get the height range info..
			bounds.transformBy( world );

			bool onPlane = bounds.minBounds().y == height || bounds.maxBounds().y == height;
			bool underWater = ( WaterSceneRenderer::currentCamHeight() < height);

			bool minAbovePlane = bounds.minBounds().y > height;
			bool maxAbovePlane = bounds.maxBounds().y > height;

			bool abovePlane = minAbovePlane && maxAbovePlane;
			bool belowPlane = !minAbovePlane && !maxAbovePlane;

			if (!onPlane)
			{
				if (underWater)
				{
					if (Moo::rc().mirroredTransform() && abovePlane) //reflection
						return;
					if (!Moo::rc().mirroredTransform() && belowPlane) //refraction
						return;
				}
				else
				{
					if (Moo::rc().mirroredTransform() && belowPlane) //reflection
						return;
					if (!Moo::rc().mirroredTransform() && abovePlane) //refraction
						return;
				}
			}
		}
		// Add the terrain block to the terrain's drawlist.
		Moo::TerrainRenderer::instance().addBlock(
			Moo::rc().world(), &*block_ );
	}
}


#if UMBRA_ENABLE

/**
 * Disable the Umbra occluder model
 */
void ChunkTerrain::disableOccluder()
{
	pUmbraObject_->object()->setWriteModel( NULL );
}


/**
 * Enable the Umbra occluder model.
 */
void ChunkTerrain::enableOccluder()
{
	Umbra::Model* model = pUmbraModel_->model();
	pUmbraObject_->object()->setWriteModel( model );
}

#endif //UMBRA_ENABLE


/**
 *	This method calculates the block's bounding box, and sets into bb_.
 */
void ChunkTerrain::calculateBB()
{
	MF_ASSERT( block_ );

	float minh =  10000.f;
	float maxh = -10000.f;
	const std::vector<float> & hm = block_->heightMap();
	for (uint i = 0; i < hm.size(); i++)
	{
		float h = hm[i];
		minh = min( minh, h );
		maxh = max( maxh, h );
	}

	bb_.setBounds(	Vector3( 0.f, minh, 0.f ),
					Vector3( 100.f, maxh, 100.f ) );

	// regenerate the collision scene since our bb is different now
	if (pChunk_ != NULL)
	{
		BoundingBox localBB = pChunk_->localBB();
		if( addYBounds( localBB ) )
		{
			pChunk_->localBB( localBB );
			localBB.transformBy( pChunk_->transform() );
			pChunk_->boundingBox( localBB );
		}

#ifndef MF_SERVER
		// update visiblity bounding box
		pChunk_->addYBoundsToVisibilityBox(
			this->bb().minBounds().y, 
			this->bb().maxBounds().y);
#endif // MF_SERVER

		//this->toss( pChunk_ );
		// too slow for now
	}
}


/**
 *	This method loads this terrain block.
 */
bool ChunkTerrain::load( DataSectionPtr pSection, Chunk * pChunk, std::string* errorString )
{
	std::string resName = pSection->readString( "resource" );

	// Allocate the terrainblock.
	block_ = Moo::TerrainBlock::loadBlock( pChunk->mapping()->path() + resName );
	if (!block_)
	{
		std::string err = "Could not load terrain block " + resName;
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

	this->calculateBB();

#if UMBRA_ENABLE
	this->terrainIndices_.reserve( 25 * 25 * 2 * 3 );

	// Create the terrain indices making sure we do not
	// make occlusion geometry for any holes in the terrain
	for (uint32 z = 0; z < 25; z++)
	{
		Moo::BaseTerrainBlock::Pole p(*block_, 0, z);
		uint32 zRow = z * 26;
		for (uint32 x = 0; x < 25; x++)
		{
			if (block_->noHoles() ||
				!p.hole())
			{
				this->terrainIndices_.push_back( zRow + x );
				this->terrainIndices_.push_back( zRow + x + 26 );
				this->terrainIndices_.push_back( zRow + x + 26 + 1 );
				this->terrainIndices_.push_back( zRow + x  );
				this->terrainIndices_.push_back( zRow + x + 26 + 1 );
				this->terrainIndices_.push_back( zRow + x + 1 );
			}
			p.move(1);
		}
	}

	this->terrainVertices_.reserve( 26 * 26 );

	// Create the terrain vertices using the height map
	const std::vector<float>& heightMap = block_->heightMap();
	for (uint32 z = 0; z < 26; z++)
	{
		float zPos = float(z) * 4.f;
		std::vector<float>::const_iterator it = heightMap.begin() + ((z + 1) * 28) + 1;
		for (uint32 x = 0; x < 26; x++)
		{
			float xPos = float(x) * 4.f;
			this->terrainVertices_.push_back( Vector3( xPos, *(it++), zPos ) );
		}
	}
#endif
	return true;
}


/**
 *	Called when we are put in or taken out of a chunk
 */
void ChunkTerrain::toss( Chunk * pChunk )
{
	if (pChunk_ != NULL)
	{
		ChunkTerrainCache::instance( *pChunk_ ).pTerrain( NULL );
	}

	this->ChunkItem::toss( pChunk );

	if (pChunk_ != NULL)
	{
		ChunkTerrainCache::instance( *pChunk ).pTerrain( this );
	}
}


void ChunkTerrain::syncInit()
{
#if UMBRA_ENABLE
	// If we have terrain indices, create the umbra resources for the terrain.
	if ( !this->terrainIndices_.empty() )
	{
		pUmbraModel_ = UmbraModelProxy::getMeshModel( &this->terrainVertices_.front(),
			&this->terrainIndices_.front(), this->terrainVertices_.size(), this->terrainIndices_.size() /3 );

		pUmbraObject_ = UmbraObjectProxy::get( pUmbraModel_, pUmbraModel_ );

		pUmbraObject_->object()->setUserPointer( (void*)this );

		Matrix m = this->pChunk_->transform();
		pUmbraObject_->object()->setObjectToCellMatrix( (Umbra::Matrix4x4&)m );
		pUmbraObject_->object()->setCell( this->pChunk_->getUmbraCell() );
		this->terrainIndices_.swap(IntVector());
		this->terrainVertices_.swap(Vector3Vector());
	}
	else
	{
		pUmbraModel_ = NULL;
		pUmbraObject_ = NULL;
	}
#endif
}

bool ChunkTerrain::addYBounds( BoundingBox& bb ) const
{
	bb.addYBounds( this->bb().minBounds().y );
	bb.addYBounds( this->bb().maxBounds().y );
	return true;
}

#undef IMPLEMENT_CHUNK_ITEM_ARGS
#define IMPLEMENT_CHUNK_ITEM_ARGS (pSection, pChunk, &errorString)
IMPLEMENT_CHUNK_ITEM( ChunkTerrain, terrain, 0 )


// -----------------------------------------------------------------------------
// Section: ChunkTerrainCache
// -----------------------------------------------------------------------------

#include "physics2/worldtri.hpp"


/**
 *	This class treats a terrain block as an obstacle
 */
class ChunkTerrainObstacle : public ChunkObstacle
{
public:
	ChunkTerrainObstacle( const Moo::TerrainBlock & tb,
			const Matrix & transform, const BoundingBox * bb,
			ChunkItemPtr pItem ) :
		ChunkObstacle( transform, bb, pItem ),
		tb_( tb )
	{
	}

	virtual bool collide( const Vector3 & source, const Vector3 & extent,
		CollisionState & state ) const;
	virtual bool collide( const WorldTriangle & source, const Vector3 & extent,
		CollisionState & state ) const;

private:
	bool callHit( float prop, CollisionState & cs, WorldTriangle & tri ) const;

	const Moo::TerrainBlock & tb_;
};





/**
 *	Another helper function for the collide method.
 *	Returns true if the whole collision test is to stop
 *	 (i.e. more than just our terrain block)
 */
bool ChunkTerrainObstacle::callHit( float prop, CollisionState & cs,
	WorldTriangle & tri ) const
{
	// see how far we really travelled (handles scaling, etc.)
	float ndist = cs.sTravel_ + (cs.eTravel_-cs.sTravel_) * prop;

	if (cs.onlyLess_ && ndist > cs.dist_) return false;
	if (cs.onlyMore_ && ndist < cs.dist_) return false;
	cs.dist_ = ndist;

	// call the callback function
	int say = cs.cc_( *this, tri, cs.dist_ );

	// see if any other collisions are wanted
	if (!say) return true;	// nope, we're outta here!

	// some are wanted ... see if it's only one side
	cs.onlyLess_ = !(say & 2);
	cs.onlyMore_ = !(say & 1);

	// but we can't stop completely yet regardless
	return false;

	// The collide function will check onlyLess and stop right after
	// it's had a look at the other triangle in the pair (to make sure
	// that if the line collides with it too then it isn't closer)
}


/**
 *	This method collides the input ray with the terrain block.
 *	source and extent should be inside (or on the edges of)
 *	our bounding box.
 */
#ifdef EDITOR_ENABLED
bool s_collideTerrainHoles = false;
#endif

bool ChunkTerrainObstacle::collide( const Vector3 & source,
	const Vector3 & extent, CollisionState & cs ) const
{
	Vector3	dir = extent - source;

	// Figure out which way we're going
	int		xsense = dir.x < 0 ? -1 : dir.x > 0 ? 1 : 0;
	int		zsense = dir.z < 0 ? -1 : dir.z > 0 ? 1 : 0;

	// OK, away we go then
	Vector3	cast = source;
	dir.normalise();

	int polesWidth = tb_.polesWidth();
	int blocksWidth = tb_.blocksWidth();
	float spacing = tb_.spacing();
	int32 xi = int32( floorf( source.x / spacing ) );
	int32 zi = int32( floorf( source.z / spacing ) );

	// Check the case where we hit the right edge exactly.
	if ((xi == blocksWidth) && (xsense == 0) && (spacing * xi == source.x))
	{
		--xi;
	}

	// Check the case where we hit the top edge exactly.
	if ((zi == blocksWidth) && (zsense == 0) && (spacing * zi == source.z))
	{
		--zi;
	}

	WorldTriangle		triA,	triB;

	float	travelled = 0;
	float	interval = (source - extent).length();
	while (travelled < interval)
	{
		// find the height at the next edge
		Vector3		next = extent;
		int32		nxi = xi;
		int32		nzi = zi;

		Vector3	tryHit;
		float	dist;
		float	otravelled = travelled;
		if (xsense != 0)
		{		// check the left/right edge of this cell
			float	xedge = ( xsense < 0 ? xi : xi+1 ) * spacing;
			dist = (xedge - cast.x) / dir.x;
			tryHit = cast + dist * dir;
			if (tryHit.z >= zi*spacing && tryHit.z < (zi+1)*spacing)
			{
				next = tryHit;
				nxi = xsense < 0 ? xi-1 : xi+1;
				travelled += dist;
			}
		}
		if (zsense != 0)
		{		// check the top/bottom edge of this cell
			float	zedge = ( zsense < 0 ? zi : zi+1 ) * spacing;
			dist = (zedge - cast.z) / dir.z;
			tryHit = cast + dist * dir;
			if (tryHit.x >= xi*spacing && tryHit.x < (xi+1)*spacing)
			{
				next = tryHit;
				nzi = zsense < 0 ? zi-1 : zi+1;
				travelled += dist;
			}
		}
		// yes, both of these cases can occur at once, and it is good.

		// make sure it's in range
		if (!(xi < 0 || zi < 0 || xi >= blocksWidth || zi >= blocksWidth))
		{
			const Moo::TerrainBlock::Pole pole(
				const_cast<Moo::TerrainBlock&>(tb_), xi, zi );

			// find the height of this cell's poles
			float haa = pole.height(0);
			float hba = pole.height(1);
			float hab = pole.height(polesWidth);
			float hbb = pole.height(1+polesWidth);

			// see if we're anywhere near this cell (and that it's not a hole)
			float cy = cast.y;
			float ny = next.y;

			bool aAbove = cy > haa && cy > hba && cy > hab && cy > hbb;
			bool bAbove = ny > haa && ny > hba && ny > hab && ny > hbb;
			bool aBelow = cy < haa && cy < hba && cy < hab && cy < hbb;
			bool bBelow = ny < haa && ny < hba && ny < hab && ny < hbb;

#ifdef EDITOR_ENABLED
			if (!(aAbove && bAbove) && !(aBelow && bBelow) && (!pole.hole() || s_collideTerrainHoles))
#else
			if (!(aAbove && bAbove) && !(aBelow && bBelow) && !pole.hole())
#endif
			{
				// ok, we can't exclude this cell outright
				float x = xi * spacing, z = zi * spacing;

				// find the first triangle
				triA = WorldTriangle(
					Vector3( x, haa, z ),
					Vector3( x, hab, z + spacing ),
					Vector3( x + spacing, hbb, z + spacing ),
					TRIANGLE_TERRAIN );

				// see if we intersect with it
				float dist = interval - otravelled;
				if (triA.intersects( cast, dir, dist ))	// dist is modified
				{
					uint32 blendaa = pole.blend(0);
					uint32 blendab = pole.blend(polesWidth);
					uint32 blendbb = pole.blend(1+polesWidth);
					uint32 dominant = calcDominantBlend( blendaa, blendab, blendbb );
					triA.flags( WorldTriangle::packFlags(TRIANGLE_TERRAIN, tb_.materialKind(dominant)) );

					if (callHit( (otravelled+dist)/interval, cs, triA ))
						return true;
				}

				// find the other triangle
				triB = WorldTriangle(
					Vector3( x, haa, z ),
					Vector3( x + spacing, hbb, z + spacing ),
					Vector3( x + spacing, hba, z ),
					TRIANGLE_TERRAIN );

				// and see if we intersect with that one
				//  (we try the second triangle even if the first call
				//  told us onlyLess, because we can't be bothered
				//  to check then in order of closeness (to source))
				if (triB.intersects( cast, dir, dist ))	// dist is modified
				{
					uint32 blendaa = pole.blend(0);
					uint32 blendba = pole.blend(1);
					uint32 blendbb = pole.blend(1+polesWidth);
					uint32 dominant = calcDominantBlend( blendaa, blendba, blendbb );
					triB.flags( WorldTriangle::packFlags(TRIANGLE_TERRAIN, tb_.materialKind(dominant)) );

					if (callHit( (otravelled+dist)/interval, cs, triB ))
						return true;
				}

				// if only closer collisions are wanted, we're done
				if (cs.onlyLess_) return false;
			}
		}

		// safety check
		if (xi == nxi && zi == nzi) break;

		// ok, we haven't hit anything yet, try the next cell then
		cast = next;
		xi = nxi;
		zi = nzi;
	}

	return false;
}


/**
 *	This method collides the input prism with the terrain block.
 *
 *	@see collide
 */
bool ChunkTerrainObstacle::collide( const WorldTriangle & sourceTri,
	const Vector3 & extentLeader, CollisionState & cs ) const
{
	const Vector3 delta = extentLeader - sourceTri.v0();

	// Create a bounding box based on input prism
	BoundingBox		tribb( sourceTri.v0(), sourceTri.v0() );
	tribb.addBounds( sourceTri.v1() );
	tribb.addBounds( sourceTri.v2() );
	// add swept end of prism
	tribb.addBounds( sourceTri.v0() + delta );
	tribb.addBounds( sourceTri.v1() + delta );
	tribb.addBounds( sourceTri.v2() + delta );

	// Choose a rectangular grid of poles to check, based on the bounds of the
	// input prism.
	const int		polesWidth	=	tb_.polesWidth();
	const int		blocksWidth	=	tb_.blocksWidth();
	const float		spacing		=	tb_.spacing();

	const float		ooSpace	= 1.f / spacing;
	const Vector3	bsource = tribb.minBounds();
	const Vector3	bextent = tribb.maxBounds();

	const int		startx	= std::max( 0,				int(floorf(bsource.x * ooSpace) ) );
	const int		endx	= std::min( blocksWidth, int(ceilf(bextent.x * ooSpace)) );
	const int		startz	= std::max( 0,				int(floorf( bsource.z * ooSpace) ));
	const int		endz	= std::min( blocksWidth, int(ceilf(bextent.z * ooSpace)) );
	
	for ( int sx = startx; sx < endx; sx++ )
	{
		for ( int sz = startz; sz < endz; sz++ )
		{
			// get the pole
			const Moo::TerrainBlock::Pole pole(
				const_cast<Moo::TerrainBlock&>(tb_), sx, sz );
			
			if (!pole.hole())
			{
				// get the heights to make the triangles with
				float haa = pole.height(0);
				float hba = pole.height(1);
				float hab = pole.height(polesWidth);
				float hbb = pole.height(1+polesWidth);

				// I'm too rushed to think up a simple throwaway condition,
				//  so we'll just make the triangles and ask them to decide
				float x = sx * spacing, z = sz * spacing;

				WorldTriangle triA(
					Vector3( x, haa, z ),
					Vector3( x, hab, z + spacing ),
					Vector3( x + spacing, hbb, z + spacing ),
					TRIANGLE_TERRAIN );

				// Note, as in 1.9, distance is set to 0.

				// see if we intersect with it
				if (triA.intersects( sourceTri, delta ))
				{
					if (callHit( 0.0f, cs, triA ))
						return true;
				}

				// find the other triangle
				WorldTriangle triB(
					Vector3( x, haa, z ),
					Vector3( x + spacing, hbb, z + spacing ),
					Vector3( x + spacing, hba, z ),
					TRIANGLE_TERRAIN );

				// and see if we intersect with that one
				if (triB.intersects( sourceTri, delta ))
				{
					if (callHit( 0.0f, cs, triB ))
						return true;
				}
			}
		}
	}

	return false;
}


/**
 *	Constructor
 */
ChunkTerrainCache::ChunkTerrainCache( Chunk & chunk ) :
	pChunk_( &chunk ),
	pTerrain_( NULL ),
	pObstacle_( NULL )
{
}

/**
 *	Destructor
 */
ChunkTerrainCache::~ChunkTerrainCache()
{
}


/**
 *	This method is called when our chunk is focussed. We add ourselves to the
 *	chunk space's obstacles at that point.
 */
int ChunkTerrainCache::focus()
{
	if (!pTerrain_ || !pObstacle_) return 0;

	const Vector3 & mb = pObstacle_->bb_.minBounds();
	const Vector3 & Mb = pObstacle_->bb_.maxBounds();

/*
	// figure out the border
	HullBorder	border;
	for (int i = 0; i < 6; i++)
	{
		// calculate the normal and d of this plane of the bb
		int ax = i >> 1;

		Vector3 normal( 0, 0, 0 );
		normal[ ax ] = (i&1) ? -1.f : 1.f;;

		float d = (i&1) ? -Mb[ ax ] : mb[ ax ];

		// now apply the transform to the plane
		Vector3 ndtr = pObstacle_->transform_.applyPoint( normal * d );
		Vector3 ntr = pObstacle_->transform_.applyVector( normal );
		border.push_back( PlaneEq( ntr, ntr.dotProduct( ndtr ) ) );
	}
*/

	// we assume that we'll be in only one column
	Vector3 midPt = pObstacle_->transform_.applyPoint( (mb + Mb) / 2.f );

	ChunkSpace::Column * pCol = pChunk_->space()->column( midPt );
	MF_ASSERT( pCol );

	// ok, just add the obstacle then
	pCol->addObstacle( *pObstacle_ );

	//dprintf( "ChunkTerrainCache::focus: "
	//	"Adding hull of terrain (%f,%f,%f)-(%f,%f,%f)\n",
	//	mb[0],mb[1],mb[2], Mb[0],Mb[1],Mb[2] );

	// which counts for just one
	return 1;
}


/**
 *	This method sets the terrain pointer
 */
void ChunkTerrainCache::pTerrain( ChunkTerrain * pT )
{
	if (pT != pTerrain_)
	{
		if (pObstacle_)
		{
			// flag column as stale first
			const Vector3 & mb = pObstacle_->bb_.minBounds();
			const Vector3 & Mb = pObstacle_->bb_.maxBounds();
			Vector3 midPt = pObstacle_->transform_.applyPoint( (mb + Mb) / 2.f );
			ChunkSpace::Column * pCol = pChunk_->space()->column( midPt, false );
			if (pCol != NULL) pCol->stale();

			pObstacle_ = NULL;
		}

		pTerrain_ = pT;

		if (pTerrain_ != NULL)
		{
            // Completely flat terrain will not work with the collision system.
            // In this case offset the y coordinates a little.
            if (pTerrain_->bb_.minBounds().y == pTerrain_->bb_.maxBounds().y)
            {
                pTerrain_->bb_.addYBounds(pTerrain_->bb_.minBounds().y + 1.0f);
            }
			pObstacle_ = new ChunkTerrainObstacle( *pTerrain_->block_,
				pChunk_->transform(), &pTerrain_->bb_, pT );

			if (pChunk_->focussed()) this->focus();
		}
	}
}


// -----------------------------------------------------------------------------
// Section: Static initialisers
// -----------------------------------------------------------------------------

/// Static cache instance accessor initialiser
ChunkCache::Instance<ChunkTerrainCache> ChunkTerrainCache::instance;



// -----------------------------------------------------------------------------
// Section: TerrainFinder
// -----------------------------------------------------------------------------

#include "chunk_manager.hpp"

/**
 *	This class implements the TerrainFinder interface. Its purpose is to be an
 *	object that Moo can use to access the terrain. It is implemented like this
 *	so that other libraries do not need to know about the Chunk library.
 */
class TerrainFinderInstance : public Moo::TerrainBlock::TerrainFinder
{
public:
	TerrainFinderInstance()
	{
		Moo::TerrainBlock::setTerrainFinder( *this );
	}

	virtual Moo::TerrainBlock::FindDetails findOutsideBlock( const Vector3 & pos )
	{
		Moo::TerrainBlock::FindDetails details;

		// TODO: At the moment, assuming the space the camera is in.
		ChunkSpacePtr pSpace = ChunkManager::instance().cameraSpace();

		//find the chunk
		if (pSpace)
		{
			ChunkSpace::Column* pColumn = pSpace->column( pos, false );

			if ( pColumn != NULL )
			{
				Chunk * pChunk = pColumn->pOutsideChunk();

				if (pChunk != NULL)
				{
					//find the terrain block
					ChunkTerrain * pChunkTerrain =
						ChunkTerrainCache::instance( *pChunk ).pTerrain();

					if (pChunkTerrain != NULL)
					{
						details.pBlock_ = pChunkTerrain->block().getObject();
						details.pInvMatrix_ = &pChunk->transformInverse();
						details.pMatrix_ = &pChunk->transform();
					}
				}
			}
		}

		return details;
	}
};

static TerrainFinderInstance s_terrainFinder;

// chunk_terrain.cpp
