/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef PERSONALITY_HPP
#define PERSONALITY_HPP

#include "moo/device_callback.hpp"
#include <Python.h>

/**
 *	This class manages the personality script.
 */
class Personality : public Moo::DeviceCallback
{
public:
	Personality();
	~Personality();

	static Personality & instance();

	operator PyObject * ()
	{
		return pPersonality_;
	}

	void set( PyObject * pObject )
	{
		pPersonality_ = pObject;
	}

	void createUnmanagedObjects();

private:
	Personality( const Personality & );
	Personality & operator =( const Personality & );

	PyObject * pPersonality_;
};



#endif // PERSONALITY_HPP
