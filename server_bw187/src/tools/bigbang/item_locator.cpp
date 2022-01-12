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
#include "item_locator.hpp"
#include "item_view.hpp"
#include "big_bang.hpp"
#include "selection_filter.hpp"
#include "snaps.hpp"
#include "appmgr/options.hpp"

#include "chunk/chunk_item.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk_obstacle.hpp"
#include "physics2/worldtri.hpp"
#include "chunks/editor_chunk.hpp"



// -----------------------------------------------------------------------------
// Section: ChunkItemLocatorRevealer
// -----------------------------------------------------------------------------

class ChunkItemLocatorRevealer : public ChunkItemRevealer
{
	Py_Header( ChunkItemLocatorRevealer, ChunkItemRevealer )
public:
	ChunkItemLocatorRevealer( SmartPointer<ChunkItemLocator> pLoc,
			PyTypePlus * pType = &s_type_ ) :
		ChunkItemRevealer( pType ),
		pLoc_( pLoc )
	{
	}

	PyObject *		pyGetAttribute( const char * attr );
	int				pySetAttribute( const char * attr, PyObject * value );

	// should really be a C++ virtual method...
	//  with attribute in base class
	PY_RO_ATTRIBUTE_DECLARE( pLoc_->chunkItem() ? 1 : 0, size );

private:
	virtual void reveal( std::vector< ChunkItemPtr > & items )
	{
		items.clear();
		ChunkItemPtr ci = pLoc_->chunkItem();
		if (ci && ci->chunk() && !EditorChunkCache::instance( *ci->chunk() ).edIsDeleted())
			items.push_back( ci );
	}

	SmartPointer<ChunkItemLocator>	pLoc_;
};

PY_TYPEOBJECT( ChunkItemLocatorRevealer )

PY_BEGIN_METHODS( ChunkItemLocatorRevealer )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( ChunkItemLocatorRevealer )
	PY_ATTRIBUTE( size )
PY_END_ATTRIBUTES()



/**
 *	Get an attribute for python
 */
PyObject * ChunkItemLocatorRevealer::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();

	return ChunkItemRevealer::pyGetAttribute( attr );
}

/**
 *	Set an attribute for python
 */
int ChunkItemLocatorRevealer::pySetAttribute( const char * attr, PyObject * value )
{
	PY_SETATTR_STD();

	return ChunkItemRevealer::pySetAttribute( attr, value );
}


// -----------------------------------------------------------------------------
// Section: ChunkItemLocator
// -----------------------------------------------------------------------------

PY_TYPEOBJECT( ChunkItemLocator )

PY_BEGIN_METHODS( ChunkItemLocator )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( ChunkItemLocator )
	PY_ATTRIBUTE( revealer )
	PY_ATTRIBUTE( subLocator )
	PY_ATTRIBUTE( enabled )
PY_END_ATTRIBUTES()

PY_FACTORY( ChunkItemLocator, Locator )


/**
 *	Constructor.
 */
ChunkItemLocator::ChunkItemLocator( ToolLocatorPtr pSub, PyTypePlus * pType ) :
	ToolLocator( pType ),
	subLocator_( pSub ),
	enabled_( true )
{
}


/**
 *	Destructor.
 */
ChunkItemLocator::~ChunkItemLocator()
{
}


/**
 *	A helper class to find and store the closest chunk item
 */
class ClosestObstacleCatcher : public CollisionCallback
{
public:
	ClosestObstacleCatcher( uint8 onFlags, uint8 offFlags) :
		onFlags_( onFlags ), offFlags_( offFlags ), dist_( 1000000000.f ) { }

	ChunkItemPtr	chunkItem_;	// initialises itself

private:
	float dist_;
	virtual int operator()( const ChunkObstacle & obstacle,
		const WorldTriangle & triangle, float dist )
	{
		// We don't want terrain
		if( !Options::getOptionInt( "render/terrain", 1 ) )
		{
			if( ( triangle.flags() & TRIANGLE_TERRAIN ) )
				return COLLIDE_ALL;
			if( obstacle.pItem()->isShellModel() && SelectionFilter::getSelectMode() == SelectionFilter::SELECT_NOSHELLS )
				return COLLIDE_ALL;
		}
		if ((triangle.flags() & onFlags_) != onFlags_ ||
			(triangle.flags() & offFlags_)) return COLLIDE_BEFORE;

		if( !obstacle.pItem()->edShouldDraw() )
			return COLLIDE_ALL;

		const Matrix& obstacleTransform = obstacle.transform_;
		PlaneEq peq( obstacleTransform.applyPoint( triangle.v0() ),
			obstacleTransform.applyVector( triangle.normal() ) );
		bool frontFacing = peq.isInFrontOf( Moo::rc().invView()[3] );

		if ( !(triangle.flags() & TRIANGLE_DOUBLESIDED) )
		{
			// Check that we don't hit back facing triangles,
			// if we can't see it, we don't want to select it (unless it's
			// double-sided)
			if( !frontFacing )
			{
				if ( !obstacle.pItem()->isShellModel() )
					return COLLIDE_ALL;
				if( obstacle.pItem()->isShellModel() && !SelectionFilter::canSelect( &*obstacle.pItem() ) )
					return COLLIDE_ALL;
			}
		}

		if (!SelectionFilter::canSelect( &*obstacle.pItem() ))
			return COLLIDE_BEFORE;

		if( dist < dist_ )
		{
			dist_ = dist;
			if (!SelectionFilter::canSelect( &*obstacle.pItem() ))
				chunkItem_ = NULL;
			else
				chunkItem_ = obstacle.pItem();
		}

		return COLLIDE_BEFORE;
	}

	uint8			onFlags_;
	uint8			offFlags_;
};


namespace
{
/**
 * Get a vector pointing from the camera position to the item's origin
 */
Vector3 vectorToItem( ChunkItemPtr item )
{
	Vector3 itemPos = item->chunk()->transform().applyPoint(
		item->edTransform().applyToOrigin()
		);

	Vector3 cameraPos = Moo::rc().invView().applyToOrigin();

	return itemPos - cameraPos;
}

/**
 * Calculate the distance between the origin of item and the ray
 */
float distToWorldRay ( ChunkItemPtr item, Vector3 worldRay )
{
	Vector3 diff = vectorToItem( item );

	float f = diff.dotProduct( worldRay );

	diff -= f * worldRay;

	// We're only using this for comparisons, so we don't need the actual length
	return diff.lengthSquared();
}
}

/** Get the ChunkItem closest to the worldRay */
class BestObstacleCatcher : public CollisionCallback
{
public:
	BestObstacleCatcher( ChunkItemPtr initialChunkItem, const Vector3& worldRay ) :
		chunkItem( initialChunkItem ), worldRay_( worldRay ) { }

	ChunkItemPtr	chunkItem;
private:
	virtual int operator()( const ChunkObstacle & obstacle,
		const WorldTriangle & triangle, float dist )
	{
		// We don't want terrain
		if (triangle.flags() & TRIANGLE_TERRAIN)
			return Options::getOptionInt( "render/terrain", 1 ) ? COLLIDE_BEFORE : COLLIDE_ALL;

		if( !Options::getOptionInt( "render/terrain", 1 ) )
		{
			if( obstacle.pItem()->isShellModel() && SelectionFilter::getSelectMode() == SelectionFilter::SELECT_NOSHELLS )
				return COLLIDE_ALL;
		}
		// We don't want non loaded items, either
		if (!obstacle.pItem()->chunk())
			return COLLIDE_BEFORE;

		if( !obstacle.pItem()->edShouldDraw() )
			return COLLIDE_ALL;

		if ( !(triangle.flags() & TRIANGLE_DOUBLESIDED) )
		{
			// Check that we don't hit back facing triangles,
			// if we can't see it we don't want to select it (unless it's
			// double-sided)
			Vector3 trin = obstacle.transform_.applyVector( triangle.normal() );
			if ( trin.dotProduct( Moo::rc().invView()[2] ) >= 0.f )
				return COLLIDE_ALL;
		}

		if (!SelectionFilter::canSelect( &*obstacle.pItem() ))
			return COLLIDE_BEFORE;


		// Make sure that the item is within a reasonable distance to the cursor
		// (Can be a problem for close objects)
		// We do this by checking the angle between the camerapos to item and worldRay
		// isn't too large
		Vector3 itemRay = vectorToItem( obstacle.pItem() );
		itemRay.normalise();

		if (acosf( worldRay_.dotProduct( itemRay ) ) > DEG_TO_RAD( 5.f ) )
			return COLLIDE_BEFORE;


		// Set the chunk item if this one is closer to the ray
		float d = distToWorldRay( obstacle.pItem(), worldRay_ );
		if (!chunkItem ||
				distToWorldRay( obstacle.pItem(), worldRay_ ) <
				distToWorldRay( chunkItem, worldRay_ ))

			chunkItem = obstacle.pItem();

		// Keep on searching, we want to check everything
		return COLLIDE_BEFORE;
	}

	const Vector3& worldRay_;
};

/**
 *	Returns the chunk item. In the cpp so the header needn't know about
 *	chunk items
 */
ChunkItemPtr ChunkItemLocator::chunkItem()
{
	return chunkItem_;
}


/**
 *	Calculate the locator's position
 */
void ChunkItemLocator::calculatePosition( const Vector3& worldRay, Tool& tool )
{
	// first call our sublocator to set the matrix
	if (subLocator_)
	{
		subLocator_->calculatePosition( worldRay, tool );
		transform_ = subLocator_->transform();
	}
	else
	{
		transform_ = Matrix::identity;
	}

	// now find the chunk item
	if (enabled_)
	{
		// if we're enabled
		float distance = -1.f;
		Vector3 extent = Moo::rc().invView().applyToOrigin() +
			worldRay * Moo::rc().camera().farPlane();

		ClosestObstacleCatcher coc( 0, TRIANGLE_TERRAIN );


		distance = ChunkManager::instance().cameraSpace()->collide( 
			Moo::rc().invView().applyToOrigin(),
			extent,
			coc );

		chunkItem_ = coc.chunkItem_;

		// ok, nothing is at the subLocator, try a fuzzy search aroung the worldRay
		if (!chunkItem_)
		{
			const float SELECTION_SIZE = 5.f;

			// The points for a square around the view position, on the camera plane
			Vector3 tr = Moo::rc().invView().applyPoint( Vector3( SELECTION_SIZE, SELECTION_SIZE, 0.f ) );
			Vector3 br = Moo::rc().invView().applyPoint( Vector3( SELECTION_SIZE, -SELECTION_SIZE, 0.f ) );
			Vector3 bl = Moo::rc().invView().applyPoint( Vector3( -SELECTION_SIZE, -SELECTION_SIZE, 0.f ) );
			Vector3 tl = Moo::rc().invView().applyPoint( Vector3( -SELECTION_SIZE, SELECTION_SIZE, 0.f ) );

			// Our two triangles which will be projected through the world
			WorldTriangle tri1(tl, tr, br);
			WorldTriangle tri2(br, bl, tl);

			// Get the best chunk for tri1, then get the best for tri2, the result
			// will be the best out of both of them
			BestObstacleCatcher boc( NULL, worldRay );
			ChunkManager::instance().cameraSpace()->collide( 
				tri1,
				extent,
				boc );
			ChunkManager::instance().cameraSpace()->collide( 
				tri2,
				extent,
				boc );

			chunkItem_ = boc.chunkItem;
		}

		// Nothing near the locator, lets try again with terrain now
		if (!chunkItem_)
		{
			coc = ClosestObstacleCatcher( 0, 0 );

			distance = ChunkManager::instance().cameraSpace()->collide( 
				Moo::rc().invView().applyToOrigin(),
				extent,
				coc );

			chunkItem_ = coc.chunkItem_;
		}
	}
	else
	{
		chunkItem_ = NULL;
	}
}


/**
 *	Get an attribute for python
 */
PyObject * ChunkItemLocator::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();

	return ToolLocator::pyGetAttribute( attr );
}

/**
 *	Set an attribute for python
 */
int ChunkItemLocator::pySetAttribute( const char * attr, PyObject * value )
{
	PY_SETATTR_STD();

	return ToolLocator::pySetAttribute( attr, value );
}

/**
 *	Get a revealer object for the chunkitem in this locator
 */
PyObject * ChunkItemLocator::pyGet_revealer()
{
	return new ChunkItemLocatorRevealer( this );
}

/**
 *	Python factory method
 */
PyObject * ChunkItemLocator::pyNew( PyObject * args )
{
	PyObject * pSubLoc = NULL;
	if (!PyArg_ParseTuple( args, "|O", &pSubLoc ) ||
		pSubLoc && !ToolLocator::Check( pSubLoc ))
	{
		PyErr_SetString( PyExc_TypeError, "ChunkItemLocator() "
			"expects an optional ToolLocator argument" );
		return NULL;
	}

	ToolLocatorPtr spSubLoc = static_cast<ToolLocator*>( pSubLoc );

	return new ChunkItemLocator( spSubLoc );
}


//------------------------------------------------------------
//Section : ItemToolLocator
//------------------------------------------------------------


//#undef PY_ATTR_SCOPE
//#define PY_ATTR_SCOPE ItemToolLocator::

PY_TYPEOBJECT( ItemToolLocator )

PY_BEGIN_METHODS( ItemToolLocator )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( ItemToolLocator )
PY_END_ATTRIBUTES()

PY_FACTORY_NAMED( ItemToolLocator, "ItemToolLocator", Locator )

LOCATOR_FACTORY( ItemToolLocator )
/**
 *	Constructor.
 */
ItemToolLocator::ItemToolLocator( PyTypePlus * pType ):
	ToolLocator( pType )
{
	planeEq_ = PlaneEq( Vector3(0,1,0),
		Options::getOptionFloat( "itemEditor/lastY" ) );
}


/**
 *	This method calculates the tool's position, by intersecting it with
 *	the XZ plane.
 *
 *	@param	worldRay	The camera's world ray.
 *	@param	tool		The tool for this locator.
 */
void ItemToolLocator::calculatePosition( const Vector3& worldRay, Tool& tool )
{
	planeEq_ = PlaneEq( Vector3(0,1,0),
		Options::getOptionFloat( "itemEditor/lastY" ) );

	transform_.setTranslate( planeEq_.intersectRay(
		Moo::rc().invView().applyToOrigin(), worldRay ) );

	//snap in the XZ direction.
	if ( BigBang::instance().snapsEnabled() )
	{
		Vector3 snaps = BigBang::instance().movementSnaps();
		snaps.y = 0.f;
		Snap::vector3( *(Vector3*)&transform_._41, snaps );
	}
}


/**
 *	Static python factory method
 */
PyObject * ItemToolLocator::pyNew( PyObject * args )
{
	return new ItemToolLocator;
}


// item_locator.cpp
