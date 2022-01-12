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

#include "pch.hpp"

#include "worldtri.hpp"

#include "math/vector2.hpp"
#include "cstdmf/debug.hpp"

#ifndef CODE_INLINE
#include "worldtri.ipp"
#endif



// -----------------------------------------------------------------------------
// Section: WorldTriangle
// -----------------------------------------------------------------------------
/**
 *	This method packs collision flags + materialKind into a Flags type.
 */
WorldTriangle::Flags WorldTriangle::packFlags( int collisionFlags, int materialKind )
{
	return collisionFlags |
		(
			(materialKind << TRIANGLE_MATERIALKIND_SHIFT) & 
			TRIANGLE_MATERIALKIND_MASK
		);
}


/**
 *	This method returns whether the input triangle intersects this triangle.
 */
bool WorldTriangle::intersects(const WorldTriangle & triangle) const
{
	// How it works:
	//	Two (non-coplanar) triangles can only intersect in 3D space along an
	//	interval that lies on the line that is the intersection of the two
	//	planes that the triangles lie on. What we do is find the interval of
	//	intersecton each triangle with this line. If these intervals overlap,
	//	the triangles intersect. We only need to test if they overlap in one
	//	coordinate.

	const WorldTriangle & triA = *this;
	const WorldTriangle & triB = triangle;

	PlaneEq planeEqA( triA.v0(), triA.v1(), triA.v2(),
		PlaneEq::SHOULD_NOT_NORMALISE );

	float dB0 = planeEqA.distanceTo(triB.v0());
	float dB1 = planeEqA.distanceTo(triB.v1());
	float dB2 = planeEqA.distanceTo(triB.v2());

	float dB0dB1 = dB0 * dB1;
	float dB0dB2 = dB0 * dB2;

	// Is triangle B on one side of the plane equation A.
	if (dB0dB1 > 0.f && dB0dB2 > 0.f)
		return false;

	PlaneEq planeEqB( triB.v0(), triB.v1(), triB.v2(),
		PlaneEq::SHOULD_NOT_NORMALISE );

	float dA0 = planeEqB.distanceTo(triA.v0());
	float dA1 = planeEqB.distanceTo(triA.v1());
	float dA2 = planeEqB.distanceTo(triA.v2());

	float dA0dA1 = dA0 * dA1;
	float dA0dA2 = dA0 * dA2;

	// Is triangle A on one side of the plane equation B.
	if (dA0dA1 > 0.f && dA0dA2 > 0.f)
		return false;

	Vector3 D(planeEqA.normal().crossProduct(planeEqB.normal()));

	float max = absValue(D.x);
	int index = X_AXIS;

	float temp = absValue(D.y);

	if (temp > max)
	{
		max = temp;
		index = Y_AXIS;
	}

	temp = absValue(D.z);

	if (temp > max)
	{
		index = Z_AXIS;
	}

	Vector3 projA(
		triA.v0()[index],
		triA.v1()[index],
		triA.v2()[index]);

	Vector3 projB(
		triB.v0()[index],
		triB.v1()[index],
		triB.v2()[index]);

	float isect1[2], isect2[2];

	/* compute interval for triangle A */
	COMPUTE_INTERVALS(projA.x, projA.y, projA.z,
		dA0, dA1,dA2,
		dA0dA1, dA0dA2,
		isect1[0], isect1[1]);

	/* compute interval for triangle B */
	COMPUTE_INTERVALS(projB.x, projB.y, projB.z,
		dB0, dB1,dB2,
		dB0dB1, dB0dB2,
		isect2[0], isect2[1]);

	sort(isect1[0],isect1[1]);
	sort(isect2[0],isect2[1]);

	return(isect1[1] >= isect2[0] &&
		isect2[1] >= isect1[0]);
}

/*
inline float Abs(float fValue)
{
	return (fValue >= 0.f) ? fValue : -fValue;
}


bool WorldTriangle::intersects2(const Vector3 & start,
							   const Vector3 & dir,
							   float & dist) const
{
    const float fTolerance = 1e-06f;

	Vector3 e1(this->v1() - this->v0());
	Vector3 e2(this->v2() - this->v0());
	Vector3 normal; normal.crossProduct(e1, e2);
//	Vector3 dir = end - start;
	float fDenominator = normal.dotProduct(dir);

    float fLLenSqr = dir.dotProduct(dir);
    float fNLenSqr = normal.dotProduct(normal);

    if ( fDenominator*fDenominator <= fTolerance*fLLenSqr*fNLenSqr )
    {
        // Line and triangle are parallel.  I assume that only transverse
        // intersections are sought.  If the line and triangle are coplanar
        // and they intersect, then you will need to put code here to
        // compute the intersection set.
        return false;
    }

    // The line is X(t) = line.b + t*line.m.  Compute line parameter t for
    // intersection of line and plane of triangle.  Substitute in the plane
    // equation to get Dot(normal,line.b-tri.b) + t*Dot(normal,line.m)
    Vector3 kDiff0 = start - this->v0();
	float fTime = -normal.dotProduct(kDiff0)/fDenominator;

	if (fTime < 0.f || fTime > 1.f)
		return false;

    // Find difference of intersection point of line with plane and vertex
    // of triangle.
    Vector3 kDiff1 = kDiff0 + fTime*dir;

    // Compute if intersection point is inside triangle.  Write
    // kDiff1 = s0*E0 + s1*E1 and solve for s0 and s1.
    float fE00 = e1.dotProduct(e1);
    float fE01 = e1.dotProduct(e2);
    float fE11 = e2.dotProduct(e2);
    float fDet = Abs(fE00*fE11-fE01*fE01);  // = |normal|^2 > 0
    float fR0 = e1.dotProduct(kDiff1);
    float fR1 = e2.dotProduct(kDiff1);

    float fS0 = fE11*fR0 - fE01*fR1;
    float fS1 = fE00*fR1 - fE01*fR0;

    if ( fS0 >= 0.0 && fS1 >= 0.0 && fS0 + fS1 <= fDet )
    {
		if (fTime < dist)
			dist = fTime;

        return true;
    }
    else
    {
        // intersection is outside triangle
        return false;
    }
}
*/

/// This constant stores that value that if two floats are within this value,
///	they are considered equal.
const float EPSILON = 0.000001f;

/**
 *	This method tests whether the input interval intersects this triangle.
 *	The interval is from start to (start + length * dir). If it intersects, the
 *	input float is set to the value such that (start + length * dir) is the
 *	intersection point.
 */
bool WorldTriangle::intersects(const Vector3 & start,
								const Vector3 & dir,
								float & length) const
{
	// Find the vectors for the edges sharing v0.
	const Vector3 edge1(v1() - v0());
	const Vector3 edge2(v2() - v0());

	// begin calculating determinant - also used to calculate U parameter
	const Vector3 p(dir.crossProduct(edge2));

	// if determinant is near zero, ray lies in plane of triangle
	float det = edge1.dotProduct(p);

	if ( almostZero( det, EPSILON ) )
		return false;

	float inv_det = 1.f / det;

	// calculate distance from vert0 to ray origin
	const Vector3 t(start - v0());

	// calculate U parameter and test bounds
	float u = t.dotProduct(p) * inv_det;

	if (u < 0.f || 1.f < u)
		return false;

	// prepare to test V parameter
	const Vector3 q(t.crossProduct(edge1));

	// calculate V parameter and test bounds
	float v = dir.dotProduct(q) * inv_det;

	if (v < 0.f || 1.f < u + v)
		return false;

	// calculate intersection point = start + dist * dir
	float distanceToPlane = edge2.dotProduct(q) * inv_det;

	if (0.f < distanceToPlane && distanceToPlane < length)
	{
		length = distanceToPlane;
		return true;
	}
	else
	{
		return false;
	}
}



/**
 *	Helper function to determine which side a given vector is
 *	of another vector. Returns true if the second vector is in
 *	the half-plane on the anticlockwise side of the first vector.
 *	Colinear vectors are considered to be on that side (returns true)
 */
inline bool inside( const Vector2 & va, const Vector2 & vb )
{
	return va.x * vb.y - va.y * vb.x >= 0;
}


/**
 *	This method returns whether the volume formed by moving the input
 *	triangle by the input translation intersects this triangle.
 *
 *	Assumes that 'translation' is not parallel to the plane of the triangle.
 */
bool WorldTriangle::intersects( const WorldTriangle & triangle,
	const Vector3 & translation ) const
{
	// first find the distance along 'translation' of the points of
	// 'triangle' from our plane eq.
	PlaneEq planeEq( v0_, v1_, v2_, PlaneEq::SHOULD_NOT_NORMALISE );
	float ndt = planeEq.normal().dotProduct( translation );
	if ( almostZero( ndt, 0.0001f ) ) return false;

	float vd0 = planeEq.intersectRayHalf( triangle.v0_, ndt );
	float vd1 = planeEq.intersectRayHalf( triangle.v1_, ndt );
	float vd2 = planeEq.intersectRayHalf( triangle.v2_, ndt );

	// if they are all too near or too close, then we consider the
	// volume to not cross the plane.
	if (vd0 <  0.f && vd1 <  0.f && vd2 <  0.f) return false;
	if (vd0 >= 1.f && vd1 >= 1.f && vd2 >= 1.f) return false;

	// figure out a good cartesian plane to project everything onto
	const Vector3 & norm = planeEq.normal();
	float max = absValue(norm.x);
	int indexU = Y_AXIS;
	int indexV = Z_AXIS;

	float temp = absValue(norm.y);
	if (temp > max)   { max = temp; indexU = X_AXIS; indexV = Z_AXIS; }
	if (absValue(norm.z) > max) { indexU = X_AXIS; indexV = Y_AXIS; }

	// now project both triangles onto it
	Vector3 triB3D[3] = {
		triangle.v0_ + translation * vd0,
		triangle.v1_ + translation * vd1,
		triangle.v2_ + translation * vd2 };
	Vector2	triA[3] = {
		Vector2( v0_[indexU], v0_[indexV] ),
		Vector2( v1_[indexU], v1_[indexV] ),
		Vector2( v2_[indexU], v2_[indexV] ) };
	Vector2 triB[3] = {
		Vector2( triB3D[0][indexU], triB3D[0][indexV] ),
		Vector2( triB3D[1][indexU], triB3D[1][indexV] ),
		Vector2( triB3D[2][indexU], triB3D[2][indexV] ) };

	// now intersect the two triangles (yay!)
	// we make sure at least one point of each triangle is on the inside
	//  side of each segment of the other triangle
	bool signA = inside( triA[1] - triA[0], triA[2] - triA[0] );
	bool signB = inside( triB[1] - triB[0], triB[2] - triB[0] );
	Vector2 segmentA, segmentB;

	for (int i1 = 0, i2 = 2; i1 <= 2; i2 = i1, i1++)
	{
		segmentA = triA[i1] - triA[i2];
		if (inside( segmentA, triB[0] - triA[i2] ) != signA &&
			inside( segmentA, triB[1] - triA[i2] ) != signA &&
			inside( segmentA, triB[2] - triA[i2] ) != signA) return false;

		segmentB = triB[i1] - triB[i2];
		if (inside( segmentB, triA[0] - triB[i2] ) != signB &&
			inside( segmentB, triA[1] - triB[i2] ) != signB &&
			inside( segmentB, triA[2] - triB[i2] ) != signB) return false;
	}

	// so they both have at least one point on the inside side of every
	// segment of the other triangle. this means they intersect

	return true;
}


/**
 *	This method <i>bounces</i> the input vector off a plane parallel to this
 *	triangle.
 *
 *	@param v			The vector to be bounced. This is passed by reference
 *						and obtains the result.
 *	@param elasticity	The elasticity of the bounce. 0 indicates that the
 *						resulting vector has no component in the direction of
 *						the plane's normal. 1 indicates that the the component
 *						in the direction of the plane's normal is reversed.
 */
void WorldTriangle::bounce( Vector3 & v,
							float elasticity ) const
{
	Vector3 normal = this->normal();
	normal.normalise();

	float proj = normal.dotProduct(v);
	v -= (1 + elasticity) * proj * normal;
}



/**
 *	This method project the given 3D point to the basis defined by
 *	the sides of this triangle. (0,0) is at v0, (1,0) is at v1,
 *	(0,1) is at v2, etc.
 *
 *	Currently it only uses a vertical projection.
 */
Vector2 WorldTriangle::project( const Vector3 & onTri ) const
{
	// always use a vertical projection
	Vector2 vs( v1_[0]		- v0_[0], v1_[2]	- v0_[2] );
	Vector2 vt( v2_[0]		- v0_[0], v2_[2]	- v0_[2] );
	Vector2 vp( onTri[0]	- v0_[0], onTri[2]	- v0_[2] );

	// do that funky linear interpolation
	float sXt = vs[0]*vt[1] - vs[1]*vt[0];
	float ls = (vp[0]*vt[1] - vp[1]*vt[0])/sXt;
	float lt = (vp[1]*vs[0] - vp[0]*vs[1])/sXt;

	return Vector2( ls, lt );
}

// worldtri.cpp
