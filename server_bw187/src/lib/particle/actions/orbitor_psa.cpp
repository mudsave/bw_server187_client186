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

#pragma warning (disable:4355)	// 'this' : used in base member initialiser list
#pragma warning (disable:4503)	// 'identifier' : decorated name length exceeded, name was truncated
#pragma warning (disable:4786)	// 'identifier' : identifier was truncated to 'number' characters in the debug information

#include "orbitor_psa.hpp"
#include "particle/particle_system.hpp"

DECLARE_DEBUG_COMPONENT2( "Particle", 0 )

#ifndef CODE_INLINE
#include "orbitor_psa.ipp"
#endif


ParticleSystemActionPtr OrbitorPSA::clone() const
{
    return ParticleSystemAction::clonePSA(*this);
}

// -----------------------------------------------------------------------------
// Section: Overrides to the Particle System Action Interface.
// -----------------------------------------------------------------------------

/**
 *	This method executes the action for the given frame of time. The dTime
 *	parameter is the time elapsed since the last call.
 *
 *	@param particleSystem	The particle system on which to operate.
 *	@param dTime			Elapsed time in seconds.
 */
void OrbitorPSA::execute( ParticleSystem &particleSystem, float dTime )
{
	// Do nothing if the dTime passed was zero or if the particle system
	// is not quite old enough to be active. Or if angular velocity is zero.
	if ( ( age_ < delay() ) || ( dTime <= 0.0f ) || angularVelocity_ == 0.0f )
	{
		age_ += dTime;
		return;
	}
	
	// Update the times of the stored Orbitor axes
	timedAxes_.updateTimes( dTime );

	// Get the position in world space of the particle system.
	Vector3 positionOfPS = particleSystem.objectToWorld().applyToOrigin();

	// Check to see if the PS has moved.
	if ( firstUpdate_ )
	{
		firstUpdate_ = false;
		lastPosPS( positionOfPS );
		timedAxes_.add( lastPosPS() );
	}
	if ( lastPosPS() != positionOfPS )
	{
		timedAxes_.add( positionOfPS );
		lastPosPS( positionOfPS );
	}
	
	float oldestParticle = -1.f;
	Particles::iterator current = particleSystem.begin();
	Particles::iterator endOfParticles = particleSystem.end();
	while ( current != endOfParticles )
	{
		Particle &particle = *current++;

		if (particle.isAlive())
		{
			if ( particle.age() > oldestParticle)
				oldestParticle = particle.age();
			
			// Find the particle's stored PS position
			positionOfPS = timedAxes_.find( particle.age(), positionOfPS );
			
			// transform the rotation by the stored PS position that relates to the particles' age
			Matrix rotation;
			rotation.setRotateY( angularVelocity_ * dTime );
			rotation.preTranslateBy( -1 * positionOfPS );
			rotation.preTranslateBy( -1 * point() );
			rotation.postTranslateBy( point() );
			rotation.postTranslateBy( positionOfPS );
			rotation.applyPoint( particle.position(), particle.position() );
			
			if ( affectVelocity() )
			{
				Vector3 velocity = particle.getTempVelocity();			
				rotation.applyVector( velocity, velocity );
				particle.setVelocity(velocity);
			}
		}
	}
	// remove stored PS positions that are no longer of any use to us.
	timedAxes_.remove( oldestParticle, true );
}


/**
 *	This is the serialiser for OrbitorPSA properties
 */
void OrbitorPSA::serialiseInternal(DataSectionPtr pSect, bool load)
{
	SERIALISE(pSect, point_, Vector3, load);
	SERIALISE(pSect, angularVelocity_, Float, load);
	SERIALISE(pSect, affectVelocity_, Bool, load);
}


// -----------------------------------------------------------------------------
// Section: Python Interface to the PyOrbitorPSA.
// -----------------------------------------------------------------------------

PY_TYPEOBJECT( PyOrbitorPSA )

/*~ function Pixie.OrbitorPSA
 *	Factory function to create and return a new PyOrbitorPSA object. OrbitorPSA is
 *	a ParticleSystemAction that causes particles to orbit around a point.
 *	@return A new PyOrbitorPSA object.
 */
PY_FACTORY_NAMED( PyOrbitorPSA, "OrbitorPSA", Pixie )

PY_BEGIN_METHODS( PyOrbitorPSA )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( PyOrbitorPSA )	
	/*~ attribute PyOrbitorPSA.point
	 *	point specifies the center point around which the particles orbit.
	 *	Default is (0.0, 0.0, 0.0).
	 *	@type Sequence of 3 floats.
	 */
	PY_ATTRIBUTE( point )
	/*~ attribute PyOrbitorPSA.angularVelocity
	 *	angularValocity is used to specify the angular velocity of the orbit
	 *	in degrees per second.
	 *	@type Float.
	 */
	PY_ATTRIBUTE( angularVelocity )
	/*~ attribute PyOrbitorPSA.affectVelocity
	 *	affectVelocity is used to specify whether the OrbitorPSA affects
	 *	a particle's velocity as well as it's position. Default is 0 (false).
	 *	@type Integer as boolean.
	 */
	PY_ATTRIBUTE( affectVelocity )
PY_END_ATTRIBUTES()


/**
 *	This is an automatically declared method for any derived classes of
 *	PyObjectPlus. It connects the readable attributes to their corresponding
 *	C++ components and searches the parent class' attributes if not found.
 *
 *	@param attr		The attribute being searched for.
 */
PyObject *PyOrbitorPSA::pyGetAttribute( const char *attr )
{
	PY_GETATTR_STD();

	return PyParticleSystemAction::pyGetAttribute( attr );
}


/**
 *	This is an automatically declared method for any derived classes of
 *	PyObjectPlus. It connects the writable attributes to their corresponding
 *	C++ components and searches the parent class' attributes if not found.
 *
 *	@param attr		The attribute being searched for.
 *	@param value	The value to assign to the attribute.
 */
int PyOrbitorPSA::pySetAttribute( const char *attr, PyObject *value )
{
	PY_SETATTR_STD();

	return PyParticleSystemAction::pySetAttribute( attr, value );
}


/**
 *	This is a static Python factory method. This is declared through the
 *	factory declaration in the class definition.
 *
 *	@param args		The list of parameters passed from Python. The NodeClamp
 *					action takes no parameters and ignores any passed to this
 *					method.
 */
PyObject *PyOrbitorPSA::pyNew( PyObject *args )
{
	Vector3 newPoint( 0.0f, 0.0f, 0.0f );
	float velocity = 0.0f;

	if ( PyTuple_Size( args ) > 0 )
	{
		if ( Script::setData( PyTuple_GetItem( args, 0 ), newPoint,
			"OrbitorPSA() first argument" ) != 0 )
		{
			return NULL;
		}

		if ( PyTuple_Size( args ) > 1 )
		{
			if ( Script::setData( PyTuple_GetItem( args, 1 ), velocity,
				"OrbitorPSA() second argument" ) != 0 )
			{
				return NULL;
			}
			else
			{
				OrbitorPSAPtr pAction = new OrbitorPSA( newPoint, velocity );
				return new PyOrbitorPSA(pAction);
			}
		}
		else
		{
			OrbitorPSAPtr pAction = new OrbitorPSA( newPoint, velocity );
			return new PyOrbitorPSA(pAction);
		}
	}
	else
	{
		OrbitorPSAPtr pAction = new OrbitorPSA;
		return new PyOrbitorPSA(pAction);
	}
}


PY_SCRIPT_CONVERTERS( PyOrbitorPSA )
