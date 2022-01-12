/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CHUNK_OBSTACLE_LOCATOR_HPP
#define CHUNK_OBSTACLE_LOCATOR_HPP

#include "gizmo/tool_locator.hpp"

/**
 *	This class implements a tool locator that sits on any arbitrary
 *	chunk obstacle type.  It takes any Chunk CollisionCallback
 *	class as a parameter and uses it to locate the tool.
 */
class ChunkObstacleToolLocator : public ChunkToolLocator
{
	Py_Header( ChunkObstacleToolLocator, ChunkToolLocator )
public:
	ChunkObstacleToolLocator( class CollisionCallback& collisionRoutine,
			PyTypePlus * pType = &s_type_ );
	virtual void calculatePosition( const Vector3& worldRay, Tool& tool );

	//class CollisionCallback& callback()				{return *callback_;}
	//void callback( class CollisionCallback& c )		{callback_ = &c;}

	PY_FACTORY_DECLARE()
protected:
	class CollisionCallback* callback_;
private:
};


#endif	// CHUNK_OBSTACLE_LOCATOR_HPP