/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

/**
 *	@file
 */

#ifndef WORLDTRIANGLE_HPP
#define WORLDTRIANGLE_HPP

#include <vector>

#include "cstdmf/stdmf.hpp"
#include "math/vector2.hpp"
#include "math/vector3.hpp"


enum
{
	// Nothing will ever collide with this triangle
	TRIANGLE_NOT_IN_BSP			= 0xff,

	TRIANGLE_UNUSED_0			= 1 << 0,
	TRIANGLE_TRANSPARENT		= 1 << 1,
	TRIANGLE_BLENDED			= 1 << 2,
	TRIANGLE_TERRAIN			= 1 << 3,

	// Player does not collide. Camera still does
	TRIANGLE_NOCOLLIDE			= 1 << 4,

	TRIANGLE_DOUBLESIDED		= 1 << 5,
	TRIANGLE_DOOR				= 1 << 6,
	TRIANGLE_UNUSED_7			= 1 << 7,

	TRIANGLE_COLLISIONFLAG_MASK = 0xff,
	TRIANGLE_MATERIALKIND_MASK	= 0xff << 8,
	TRIANGLE_MATERIALKIND_SHIFT	= 8		// how many bits to shift to get to the byte we want
};




/**
 *	This class is used to represent a triangle in the collision scene.
 */
class WorldTriangle
{
public:
	typedef uint16 Flags;
	typedef uint16 Padding;

	WorldTriangle( const Vector3 & v0,
		const Vector3 & v1,
		const Vector3 & v2,
		Flags flags = 0 );
	WorldTriangle();

	bool intersects(const WorldTriangle & triangle) const;
	bool intersects(const Vector3 & start,
		const Vector3 & dir,
		float & dist) const;
/* Seems to be slower
	bool intersects2(const Vector3 & start,
		const Vector3 & end,
		float & dist) const;
*/
	bool intersects(const WorldTriangle & triangle,
		const Vector3 & translation ) const;

	template <class C> C interpolate( const Vector3 & onTri,
		const C & a, const C & b, const C & c ) const;
	Vector2 project( const Vector3 & onTri ) const;

	const Vector3 & v0() const;
	const Vector3 & v1() const;
	const Vector3 & v2() const;

	Vector3 normal() const;

	void bounce(Vector3 & v,
		float elasticity = 1.f) const;

	static Flags packFlags( int collisionFlags, int materialKind );
	static int collisionFlags( Flags flags )
	{ return (flags & TRIANGLE_COLLISIONFLAG_MASK); }
	static int materialKind( Flags flags )
	{ return (flags & TRIANGLE_MATERIALKIND_MASK) >> TRIANGLE_MATERIALKIND_SHIFT; }
	Flags	flags() const;
	void	flags( Flags newFlags );
	bool	isTransparent() const;
	bool	isBlended() const;

	uint	collisionFlags() const	{ return collisionFlags( flags_ ); }
	uint	materialKind() const	{ return materialKind( flags_ ); }

	bool operator==(const WorldTriangle& rhs) const
	{
		return v0_ == rhs.v0_ && v1_ == rhs.v1_ && v2_ == rhs.v2_
			&& flags_ == rhs.flags_ && padding_ == rhs.padding_;
	}

private:
	Vector3 v0_;
	Vector3 v1_;
	Vector3 v2_;
	Flags	flags_;
	Padding padding_;
};

/// This type stores a collection of triangles.
typedef std::vector<const WorldTriangle *> WTriangleSet;
typedef std::vector<WorldTriangle> RealWTriangleSet;

#ifdef CODE_INLINE
#include "worldtri.ipp"
#endif

#endif // WORLDTRIANGLE_HPP
