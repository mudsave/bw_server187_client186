/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef WATCHER_GLUE_HPP
#define WATCHER_GLUE_HPP

#include "network/nub.hpp"
#include "network/watcher_nub.hpp"

/**
 *	This class is a singleton version of WatcherNub that receives event
 *	notifications from Mercury and uses these to process watcher events.
 *
 * 	@ingroup watcher
 */
class WatcherGlue : public WatcherNub, public Mercury::InputNotificationHandler
{
public:
	WatcherGlue();
	virtual ~WatcherGlue();

	virtual int handleInputNotification( int fd );

	static WatcherGlue & instance();

private:
	StandardWatcherRequestHandler	handler_;
};

#endif // WATCHER_GLUE_HPP
