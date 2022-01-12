/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef snaps_hpp
#define snaps_hpp

namespace Snap
{
	float value( float& v, float snapSize );
	Vector3& vector3( Vector3& v, const Vector3& snaps, const Vector3& origin = Vector3(0,0,0) );
	Vector3 vector3( const Vector3& v, const Vector3& snaps, const Vector3& origin = Vector3(0,0,0) );
	Vector3& angle( Vector3& vector, float snapAngle );

	// snap an arbitrary ray to the ground
	Vector3 toGround( const Vector3& startPosition, const Vector3& direction, 
						float distance = 2500.f, bool ignoreHoles = false );

	// snap a point's Y value to the ground
	Vector3 toGround( const Vector3& v );

	// snap an arbitrary ray to the closest obstacle
	Vector3 toObstacle( const Vector3& startPosition, const Vector3& direction, 
						bool snapToBoundingBox = false, float distance = 2500.f,
                        bool *snapped = NULL );

	Vector3 toObstacleNormal( const Vector3& startPosition, const Vector3& direction,
		float distance = 2500.f);

	// snap a point's Y value to the nearest obstacle
	Vector3 toObstacle( const Vector3& v );
	
	Matrix& angles( Matrix& v, float snapSize );

	// calculate a value that will satisfy both snap values a and b
	float satisfy( float a, float b );
};

#endif