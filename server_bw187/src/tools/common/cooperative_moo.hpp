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
 *	Class to help make Moo windowed apps cooperative with each other
 */

#ifndef _COOPERATIVE_MOO_HPP_
#define _COOPERATIVE_MOO_HPP_

class CooperativeMoo
{
public:
	static bool beginOnPaint();
	static void endOnPaint();
	static void deactivate();
	static bool activate( int minTextureMem = 32 );

private:
	static int wasPaused_;
};

#endif _COOPERATIVE_MOO_HPP_
