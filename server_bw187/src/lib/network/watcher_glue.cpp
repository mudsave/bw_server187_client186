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

#include "watcher_glue.hpp"
#include "cstdmf/watcher.hpp"
#include "cstdmf/debug.hpp"

using namespace std;


DECLARE_DEBUG_COMPONENT2( "Network", 0 )

// -----------------------------------------------------------------------------
// Section: WatcherGlue
// -----------------------------------------------------------------------------

#ifdef _WIN32
#pragma warning (push)
#pragma warning (disable: 4355)
#endif
/**
 * 	This is the constructor.
 */
WatcherGlue::WatcherGlue() :
	WatcherNub(), handler_( *this )
{
	this->setRequestHandler(&handler_);
}
#ifdef _WIN32
#pragma warning (pop)
#endif


/**
 * 	This is the destructor.
 */
WatcherGlue::~WatcherGlue()
{
}


/**
 *	This method returns the singleton WatcherGlue object.
 */
WatcherGlue & WatcherGlue::instance()
{
	static WatcherGlue s_instance;
	return s_instance;
}


/**
 * 	This method is called by Mercury when there is data to read on the
 * 	watcher socket. It calls the receiveRequest method to actually
 * 	process the request.
 */
int WatcherGlue::handleInputNotification( int fd )
{
	// this would be bad
	if (fd != this->getSocketDescriptor())
	{
		ERROR_MSG( "WatcherGlue::handle_input: Got unexpected fd %d!\n", fd );
		return 0;	// don't really want to crash everything
	}

	// ok, go fetch now!
	WatcherGlue::instance().receiveRequest();

	return 0;
}

// watcher_glue.cpp
