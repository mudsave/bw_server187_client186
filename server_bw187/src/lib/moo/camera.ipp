/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// camera.ipp

#include "cstdmf/debug.hpp"
#include "math/mathdef.hpp"

#ifdef CODE_INLINE
#define INLINE    inline
#else
#define INLINE
#endif

namespace Moo
{


INLINE Camera::Camera( float nP, float fP, float f, float aR ) :
	nearPlane_( nP ),
	farPlane_( fP ),
	fov_( f ),
	aspectRatio_( aR ),
	viewHeight_( 100 ),
	ortho_( false )
{
	MF_ASSERT( nP < fP );
	MF_ASSERT( nP > 0 );
	MF_ASSERT( f > 0 && f < MATH_PI );
	MF_ASSERT( aR != 0 );
}

INLINE Camera::Camera( const Camera& camera)
{
	*this=camera;
}

INLINE Camera& Camera::operator =( const Camera& camera )
{
	nearPlane_=camera.nearPlane_;
	farPlane_=camera.farPlane_;
	fov_=camera.fov_;
	aspectRatio_=camera.aspectRatio_;
	viewHeight_=camera.viewHeight_;
	ortho_=camera.ortho_;
	return *this;
}

INLINE Camera::~Camera()
{
}


INLINE float Camera::nearPlane() const
{
	MF_ASSERT(nearPlane_<farPlane_);
	return nearPlane_;
}

INLINE void Camera::nearPlane( float f )
{
	MF_ASSERT(f>0);
	nearPlane_=f;
}


INLINE float Camera::farPlane() const
{
	MF_ASSERT(nearPlane_<farPlane_);
	return farPlane_;
}

INLINE void Camera::farPlane( float f )
{
	MF_ASSERT(f>0);
	farPlane_=f;
}


INLINE float Camera::fov() const
{
	return fov_;
}

INLINE void Camera::fov( float f )
{
	MF_ASSERT(f>0&&f<MATH_PI);
	fov_=f;
}


INLINE float Camera::aspectRatio() const
{
	return aspectRatio_;
}

INLINE void Camera::aspectRatio( float f )
{
	MF_ASSERT(f!=0);
	aspectRatio_=f;
}


INLINE float Camera::viewHeight() const
{
	return viewHeight_;
}

INLINE void Camera::viewHeight( float height )
{
	viewHeight_ = height;
}


INLINE float Camera::viewWidth() const
{
	return aspectRatio_ * viewHeight_;
}


INLINE bool Camera::ortho() const
{
	return ortho_;
}

INLINE void Camera::ortho( bool b )
{
	ortho_ = b;
}


} // namespace Moo

// camera.ipp