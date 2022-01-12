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
#include "personality.hpp"
#include "script_bigworld.hpp"

// -----------------------------------------------------------------------------
// Section: Personality
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
Personality::Personality() :
	pPersonality_( NULL )
{
}


/**
 *	Destructor.
 */
Personality::~Personality()
{
}


/*~ callback Personality.createUnmanagedObjects
 *
 *	This callback method is called on the personality script when
 *	the Direct3D device has asked the engine to recreate all
 *	resources  not managed by Direct3D itself.  
 */
/**
 *	Called when the d3d device is recreated.
 */
void Personality::createUnmanagedObjects()
{
	if (pPersonality_)
	{
		PyObject * pfn = PyObject_GetAttrString( pPersonality_,
			"onRecreateDevice" );

		if ( pfn )
		{
			BigWorldClientScript::callNextFrame(
				pfn, PyTuple_New(0),
				"Personality::createUnmanagedObjects: " );
		}
		else
		{
			PyErr_Clear();
		}
	}
}

/**
 *	Static method to return the Personality instance
 */
Personality & Personality::instance()
{
	static Personality instance;
	return instance;
}

// personality.cpp
