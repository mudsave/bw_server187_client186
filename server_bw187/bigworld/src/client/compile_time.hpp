/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef COMPILE_TIME_HPP
#define COMPILE_TIME_HPP

/*
	These need to be instantiated in the exe. Something link the following.

	compile_time.cpp
		const char * compileTimeString	= __TIME__ " " __DATE__;

#include "../../build_id.txt"
		const char * buildIdString		= BUILD_ID_STRING;

	Have a Pre-Link Event something like:
		cl.exe /c /Fo$(OutDir) /nologo compile_time.cpp
*/

extern const char * compileTimeString;
extern const char * buildIdString;

#endif