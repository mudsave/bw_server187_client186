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
#include "terrain_locator.hpp"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk.hpp"
#include "chunk/chunk_manager.hpp"
#include "gizmo/tool.hpp"
#include "../big_bang.hpp"
#include "appmgr/options.hpp"
#include "snaps.hpp"

//----------------------------------------------------
//	Section : TerrainToolLocator
//----------------------------------------------------

PY_TYPEOBJECT( TerrainToolLocator )

PY_BEGIN_METHODS( TerrainToolLocator )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( TerrainToolLocator )
PY_END_ATTRIBUTES()

PY_FACTORY_NAMED( TerrainToolLocator, "TerrainToolLocator", Locator )

LOCATOR_FACTORY( TerrainToolLocator )

/**
 *	constructor.
 */
TerrainToolLocator::TerrainToolLocator( PyTypePlus * pType )
:ChunkObstacleToolLocator( ClosestTerrainObstacle::s_default, pType )
{
}


/**
 *	This method calculates the position of the terrain tool locator,
 *	being defined as the intersection from the world ray to the terrain.
 *
 *	@param	worldRay	The camera's world ray.
 *	@param	tool		The tool we are locating.
 */
void TerrainToolLocator::calculatePosition( const Vector3& worldRay, Tool& tool )
{
	Vector3 groundPos = Snap::toGround(Moo::rc().invView().applyToOrigin(), worldRay, 20000.f);
	if (groundPos != Moo::rc().invView().applyToOrigin())
	{
		transform_.setTranslate( groundPos );
	}

	//snap in the XZ direction.
	if ( BigBang::instance().snapsEnabled() )
	{
		Vector3 snaps = BigBang::instance().movementSnaps();
		snaps.y = 0.f;
		Snap::vector3( *(Vector3*)&transform_._41, snaps );
		Snap::toGround( *(Vector3*)&transform_._41 );
	}
}


/**
 *	Static python factory method
 */
PyObject * TerrainToolLocator::pyNew( PyObject * args )
{
	return new TerrainToolLocator;
}


//----------------------------------------------------
//	Section : TerrainHoleToolLocator
//----------------------------------------------------

PY_TYPEOBJECT( TerrainHoleToolLocator )

PY_BEGIN_METHODS( TerrainHoleToolLocator )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( TerrainHoleToolLocator )
PY_END_ATTRIBUTES()

PY_FACTORY_NAMED( TerrainHoleToolLocator, "TerrainHoleToolLocator", Locator )

LOCATOR_FACTORY( TerrainHoleToolLocator )

/**
 *	constructor.
 */
TerrainHoleToolLocator::TerrainHoleToolLocator( PyTypePlus * pType )
:ChunkObstacleToolLocator( ClosestTerrainObstacle::s_default, pType )
{
}


/**
 *	This method calculates the position of the terrain tool locator,
 *	being defined as the intersection from the world ray to the terrain,
 *	and any holes underneath that area.
 *
 *	@param	worldRay	The camera's world ray.
 *	@param	tool		The tool we are locating.
 */
void TerrainHoleToolLocator::calculatePosition( const Vector3& worldRay, Tool& tool )
{
	Vector3 groundPos = Snap::toGround(Moo::rc().invView().applyToOrigin(), worldRay, 2500.f, true);
	if (groundPos != Moo::rc().invView().applyToOrigin())
	{
		transform_.setTranslate( groundPos );
	}

	//always snap in the XZ direction.	
	Vector3 snaps( 4.f,0.f,4.f );
	Snap::vector3( *(Vector3*)&transform_._41, snaps, Vector3(-2.f,0.f,-2.f) );	
	Snap::toGround( *(Vector3*)&transform_._41, Vector3(0.f,-1.f,0.f), 500.f, true );
}


/**
 *	Static python factory method
 */
PyObject * TerrainHoleToolLocator::pyNew( PyObject * args )
{
	return new TerrainHoleToolLocator;
}



//----------------------------------------------------
//	Section : TerrainChunkLocator
//----------------------------------------------------

PY_TYPEOBJECT( TerrainChunkLocator )

PY_BEGIN_METHODS( TerrainChunkLocator )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( TerrainChunkLocator )
PY_END_ATTRIBUTES()

PY_FACTORY_NAMED( TerrainChunkLocator, "TerrainChunkLocator", Locator )

LOCATOR_FACTORY( TerrainChunkLocator )

/**
 *	constructor.
 */
TerrainChunkLocator::TerrainChunkLocator( PyTypePlus * pType )
:ChunkObstacleToolLocator( ClosestTerrainObstacle::s_default, pType )
{
}


/**
 *	This method calculates the position of the terrain chunk locator,
 *	being defined as the intersection from the world ray to the terrain,
 *	and from there, the chunks surrounding the tool given the tool's size.
 *
 *	@param	worldRay	The camera's world ray.
 *	@param	tool		The tool we are locating.
 */
void TerrainChunkLocator::calculatePosition( const Vector3& worldRay, Tool& tool )
{
	ClosestTerrainObstacle::s_default.dist_ = 0.f;
	Vector3 extent = Moo::rc().invView().applyToOrigin() + ( worldRay * 2500.f );

	ChunkSpacePtr space = ChunkManager::instance().cameraSpace();
	if ( space )
	{
		space->collide( 
			Moo::rc().invView().applyToOrigin(),
			extent,
			*callback_ );
	}

	if ( ClosestTerrainObstacle::s_default.dist_ > 0.f )
	{
		Vector3	pos = Moo::rc().invView().applyToOrigin() +
				( worldRay * ClosestTerrainObstacle::s_default.dist_ );

		if ( ChunkManager::instance().cameraSpace() )
		{
			ChunkSpace::Column* pColumn =
				ChunkManager::instance().cameraSpace()->column( pos, false );

			if ( pColumn )
			{
				if ( pColumn->pOutsideChunk() )
				{
					transform_ = pColumn->pOutsideChunk()->transform();
					transform_[3] += Vector3( 50.f, 0.f, 50.f );
				}
			}
		}
	}
}


/**
 *	Static python factory method
 */
PyObject * TerrainChunkLocator::pyNew( PyObject * args )
{
	return new TerrainChunkLocator;
}