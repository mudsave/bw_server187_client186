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
#include "collision_callbacks.hpp"

#include "big_bang.hpp"

DECLARE_DEBUG_COMPONENT2( "editor", 0 )

ClosestForwardFacingPoly ClosestForwardFacingPoly::s_default;

int ClosestForwardFacingPoly::operator() ( const ChunkObstacle & obstacle,
		const WorldTriangle & triangle, float dist )
{
	// ignore selected
	if ( BigBang::instance().isItemSelected(obstacle.pItem()) ||
		!obstacle.pItem()->edIsSnappable() )
		return COLLIDE_ALL;

	// ignore back facing
	Vector3 trin = obstacle.transform_.applyVector( triangle.normal() );
	trin.normalise();
	Vector3 lookv = obstacle.transform_.applyPoint( triangle.v0() ) -
		Moo::rc().invView()[3];
	lookv.normalise();
	const float dotty = trin.dotProduct( lookv );
	if ( dotty > 0.f )
		return COLLIDE_ALL;

	// choose closer
	if ( dist < dist_ )
	{
		dist_ = dist;
		normal_ = trin;
		pItem_ = obstacle.pItem();
		triangleNormal_ = triangle.normal();
	}

	//return COLLIDE_BEFORE;
	return COLLIDE_ALL;
}