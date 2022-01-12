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

#include "chunk_obstacle_locator.hpp"
#include "collision_callbacks.hpp"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk_manager.hpp"
#include "snaps.hpp"

//------------------------------------------------------------
//Section : ChunkObstacleToolLocator
//------------------------------------------------------------


#undef PY_ATTR_SCOPE
#define PY_ATTR_SCOPE ChunkObstacleToolLocator::

PY_TYPEOBJECT( ChunkObstacleToolLocator )

PY_BEGIN_METHODS( ChunkObstacleToolLocator )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( ChunkObstacleToolLocator )
PY_END_ATTRIBUTES()

PY_FACTORY( ChunkObstacleToolLocator, Locator )

/**
 *	constructor.
 */
ChunkObstacleToolLocator::ChunkObstacleToolLocator(
	CollisionCallback& collisionRoutine,
	PyTypePlus * pType ):
		ChunkToolLocator( pType ),
		callback_( &collisionRoutine )
{
}


/**
 *	This method calculates the position of the tool, defined
 *	as the nearest solid object along the camera's world ray.
 *
 *	@param	worldRay	The camera's world ray.
 *	@param	tool		The tool this locator applies to.
 */
void ChunkObstacleToolLocator::calculatePosition( const Vector3& worldRay, Tool& tool )
{
	ChunkSpacePtr space = ChunkManager::instance().cameraSpace();
	if ( !space )
		return;

	if ( callback_ == &ClosestForwardFacingPoly::s_default )
	{
		ClosestForwardFacingPoly* pCallback = &ClosestForwardFacingPoly::s_default;

		Vector3 pos = Snap::toObstacle(Moo::rc().invView().applyToOrigin(),
						worldRay);

		if (pos != Moo::rc().invView().applyToOrigin())
		{
			// found something

			// set transform so that the y-axis is aligned with the surface normal,
			// and the x and z axes point along the surface
			pCallback->normal_.normalise();
			if ( fabsf(pCallback->normal_.y) < 0.95f )
			{
				transform_.lookAt( pos, pCallback->normal_, Vector3( 0.f, 1.f, 0.f ) );			
			}
			else
			{
				transform_.lookAt( pos, pCallback->normal_, Vector3( 0.f, 0.f, 1.f ) );				
			}
			transform_.invert();
			transform_.translation( Vector3(0,0,0) );
			transform_.preRotateX( DEG_TO_RAD(90.f) );
			transform_.translation( pos );
		}

		pCallback->pItem_ = NULL;
	}
	else
	{
		float distance = 0.f;
		Vector3 extent = Moo::rc().invView().applyToOrigin() + ( worldRay * 2500.f );

		distance = space->collide( 
			Moo::rc().invView().applyToOrigin(),
			extent,
			*callback_ );

		if ( distance > 0.f )
		{
			Vector3	pos = Moo::rc().invView().applyToOrigin() + ( worldRay * distance );
			transform_.setTranslate( pos );
		}
	}
}



/**
 *	Python factory method
 */
PyObject * ChunkObstacleToolLocator::pyNew( PyObject * pArgs )
{
	if (PyTuple_Size( pArgs ) != 0)
	{
		PyErr_SetString( PyExc_TypeError, "ChunkObstacleToolLocator() "
			"expects no arguments" );
		return NULL;
	}

	return new ChunkObstacleToolLocator( ClosestForwardFacingPoly::s_default );
}