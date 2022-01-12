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


#include "pch.hpp"
#include "cooperative_moo.hpp"
#include "moo/render_context.hpp"


int CooperativeMoo::wasPaused_ = false;


//#define DONT_USE_COOPERATIVE_MOO 1


bool CooperativeMoo::beginOnPaint()
{
#ifdef DONT_USE_COOPERATIVE_MOO
	return true;
#else // DONT_USE_COOPERATIVE_MOO
	
	if ( AfxGetMainWnd()->IsIconic() )
		return false;
	
	wasPaused_ = Moo::rc().paused();
	if ( wasPaused_ )
		Moo::rc().resume();

	return !Moo::rc().paused();

#endif // DONT_USE_COOPERATIVE_MOO
}


void CooperativeMoo::endOnPaint()
{
#ifndef DONT_USE_COOPERATIVE_MOO

	if ( AfxGetMainWnd()->IsIconic() )
		return;

	if ( wasPaused_ )
		Moo::rc().pause();

#endif // DONT_USE_COOPERATIVE_MOO
}


void CooperativeMoo::deactivate()
{
#ifndef DONT_USE_COOPERATIVE_MOO
	
	Moo::rc().pause();

#endif // DONT_USE_COOPERATIVE_MOO
}


bool CooperativeMoo::activate( int minTextureMem )
{
#ifdef DONT_USE_COOPERATIVE_MOO
	return true;
#else // DONT_USE_COOPERATIVE_MOO

	if ( !Moo::rc().paused() )
		return true;

	if ( AfxGetMainWnd()->IsIconic() )
		return false;

	CWaitCursor wait;

	if ( Moo::rc().checkDevice() &&
		int(Moo::rc().getAvailableTextureMem()) <= minTextureMem * 1024*1024 )
	{
		// not enough free video memory to resume the context, so fail
		Sleep( 500 );
		return false;
	}
	Moo::rc().resume();

	return !Moo::rc().paused();

#endif // DONT_USE_COOPERATIVE_MOO
}
