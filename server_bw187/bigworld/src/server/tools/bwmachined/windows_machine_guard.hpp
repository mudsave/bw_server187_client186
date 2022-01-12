/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef WINDOWS_MACHINE_GUARD_HPP
#define WINDOWS_MACHINE_GUARD_HPP

#include "messages.h"

//#define openlog if (0) ((int (*)(char *, ...)) 0)
//#define syslog  if (0) ((int (*)(int, char *, ...)) 0)
void openlog( const char * ident, int option, int facility );
void syslog( int priority, const char * format, ... );

#include "common_machine_guard.hpp"

struct ThingData : public CommonThingData
{
	ThingData();
	void init( const MachineGuardMessage& );
	void cleanup();
	HANDLE  hProcess;
	HANDLE  hStopEvent;
};

#endif
