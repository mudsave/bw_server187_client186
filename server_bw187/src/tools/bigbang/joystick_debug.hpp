/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef JOYSTICKDEBUG_HPP
#define JOYSTICKDEBUG_HPP

#include <iostream>

class JoystickDebug
{
public:
	JoystickDebug();
	~JoystickDebug();

	void render();

private:
	JoystickDebug(const JoystickDebug&);
	JoystickDebug& operator=(const JoystickDebug&);

	friend std::ostream& operator<<(std::ostream&, const JoystickDebug&);
};

#ifdef CODE_INLINE
#include "joystick_debug.ipp"
#endif




#endif
/*joystickdebug.hpp*/
