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
#include "projection_access.hpp"

#include "moo/render_context.hpp"

#include "direction_cursor.hpp"


// -----------------------------------------------------------------------------
// Section: ProjectionAccess
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
ProjectionAccess::ProjectionAccess( PyTypePlus * pType ) :
	PyObjectPlus( pType ),
	smoothFovTransition_( false )
{
}


/**
 *	Destructor.
 */
ProjectionAccess::~ProjectionAccess()
{
}

/**
 *	Update any timed effects on the camera
 */
void ProjectionAccess::update( float dTime )
{
	if (!smoothFovTransition_) return;

	// All angles in this method are in radians
	Moo::Camera & camera = Moo::rc().camera();
	float newFOV;

	// Update the FOV and elapsed time
	fovTransitionTimeElapsed_ += dTime;
	if ( fovTransitionTimeElapsed_ < fovTransitionTime_ )
	{
		float percent = fovTransitionTimeElapsed_ / fovTransitionTime_;
		newFOV = fovTransitionStart_ +
			percent * ( fovTransitionEnd_ - fovTransitionStart_ );
	}
	else
	{
		newFOV = fovTransitionEnd_;
		smoothFovTransition_ = false;
	}

	// Set the new FOV.
	camera.fov( newFOV );
}


/*~ function ProjectionAccess.rampFov
 *
 *	This function changes from the current fov (field of view, in radians) to
 *  the specified one over the specified time period.  It does this
 *	using linear interpolation.  Note the fov is measured vertically.
 *
 *	@param	newFov		The fov to move toward
 *	@param	time		The time in seconds to take to move between the current
 *						fov and the specified fov
 */
/**
 *	Start ramping the fov between the given values
 */
void ProjectionAccess::rampFov( float newFOV, float timeAllowed )
{
	smoothFovTransition_ = true;

	fovTransitionStart_ = this->fov();
	fovTransitionEnd_ = newFOV;
	fovTransitionTime_= timeAllowed;
	fovTransitionTimeElapsed_ = 0.0f;
}


/**
 *	Get the near plane
 */
float ProjectionAccess::nearPlane() const
{
	return Moo::rc().camera().nearPlane();
}

/**
 *	Set the near plane
 */
void ProjectionAccess::nearPlane( float val )
{
	Moo::rc().camera().nearPlane( val );
}


/**
 *	Get the far plane
 */
float ProjectionAccess::farPlane() const
{
	return Moo::rc().camera().farPlane();
}

/**
 *	Set the far plane
 */
void ProjectionAccess::farPlane( float val )
{
	Moo::rc().camera().farPlane( val );
}


/**
 *	Get the vertical field of view angle
 */
float ProjectionAccess::fov() const
{
	return Moo::rc().camera().fov();
}

/**
 *	Set the vertical field of view angle
 */
void ProjectionAccess::fov( float val )
{
	smoothFovTransition_ = false;

	Moo::rc().camera().fov( val );
}





// -----------------------------------------------------------------------------
// Section: Python stuff
// -----------------------------------------------------------------------------



PY_TYPEOBJECT( ProjectionAccess )

PY_BEGIN_METHODS( ProjectionAccess )
	PY_METHOD( rampFov )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( ProjectionAccess )
	/*~ attribute ProjectionAccess.nearPlane
	 *
	 *	This attribute is the distance to the near clipping plane for the
	 *	current camera.  All geometry closer to the	eye than this will be
	 *	clipped out.
	 *	
	 *	@type	Float
	 */
	PY_ATTRIBUTE( nearPlane )
	/*~ attribute ProjectionAccess.farPlane
	 *
	 *	This attribute is the distance to the far clipping plane for the
	 *	current camera.  All geometry further from the eye than this will be
	 *	clipped out.
	 *	
	 *	@type	Float
	 */
	PY_ATTRIBUTE( farPlane )
	/*~ attribute ProjectionAccess.fov
	 *
	 *	The vertical field of view of the current camera, measured in radians. This
	 *	number must be between 0 and pi.  Setting to a number outside of this
	 *	is likely to cause video driver crashes.
	 *
	 *	@type	Float
	 */
	PY_ATTRIBUTE( fov )
PY_END_ATTRIBUTES()

PY_SCRIPT_CONVERTERS( ProjectionAccess )


/**
 *	Get an attribute for python
 */
PyObject * ProjectionAccess::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();

	return PyObjectPlus::pyGetAttribute( attr );
}

/**
 *	Set an attribute for python
 */
int ProjectionAccess::pySetAttribute( const char * attr, PyObject * value )
{
	PY_SETATTR_STD();

	return PyObjectPlus::pySetAttribute( attr, value );
}

// projection_access.cpp
