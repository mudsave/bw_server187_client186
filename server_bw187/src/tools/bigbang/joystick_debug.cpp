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

#include "joystick_debug.hpp"
#include "input/input.hpp"
#include "romp/xconsole.hpp"

#ifndef CODE_INLINE
#include "joystick_debug.ipp"
#endif

JoystickDebug::JoystickDebug()
{
}

JoystickDebug::~JoystickDebug()
{
}

void JoystickDebug::render()
{
	static XConsole console;

	char buf[ 256 ];
	sprintf( buf, "X = %f. Y = %f",
		InputDevices::joystick().getAxis( AxisEvent::AXIS_LX ).value(),
		InputDevices::joystick().getAxis( AxisEvent::AXIS_LY ).value() );

	console.setCursor( 0, 0 );
	console.print( buf );
	console.draw( 0.1f );
}

std::ostream& operator<<(std::ostream& o, const JoystickDebug& t)
{
	o << "JoystickDebug\n";
	return o;
}


/*joystickdebug.cpp*/
