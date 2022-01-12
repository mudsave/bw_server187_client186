/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "Python.h"		// See http://docs.python.org/api/includes.html

#include <signal.h>
#include <set>

//#include "common/syslog.hpp"
#include "baseapp.hpp"
#include "cstdmf/timestamp.hpp"
#include "network/logger_message_forwarder.hpp"
#include "resmgr/bwresource.hpp"
#include "server/bwconfig.hpp"
#include "network/watcher_glue.hpp"

DECLARE_DEBUG_COMPONENT( 0 )

#ifdef _WIN32  // WIN32PORT

void bwStop()
{
	raise( SIGINT );
}

char szServiceDependencies[] = "machined";

#endif // _WIN32

#include "server/bwservice.hpp"


// ----------------------------------------------------------------------------
// Section: Main
// ----------------------------------------------------------------------------

int BIGWORLD_MAIN( int argc, char * argv[] )
{
	Mercury::Nub nub( 0, BW_INTERNAL_INTERFACE( baseApp ) );

	BW_MESSAGE_FORWARDER( BaseApp, baseApp, nub );
	START_MSG( "BaseApp" );

	BaseApp baseApp( nub );

	/*
	// Syslogger must be initialised first
	SysloggerCallback syscb(argc, argv);
	syscb.register_me();
	*/

	// calculate the clock speed
	stampsPerSecond();

	BW_SERVICE_CHECK_POINT( 3000 );

//	INFO_MSG( "BaseApp: starting server...\n" );

	if (!baseApp.init( argc, argv ))
	{
		ERROR_MSG( "main: init failed.\n" );
		return 1;
	}

	INFO_MSG( "---- BaseApp is running ----\n" );

	BW_SERVICE_UPDATE_STATUS( SERVICE_RUNNING, 0, 0 );

	do
	{
		try
		{
			baseApp.intNub().processContinuously();
			break;
		}
		catch (Mercury::NubException & ne)
		{
			Mercury::Address addr = Mercury::Address::NONE;
			bool hasAddr = ne.getAddress( addr );

			switch (ne.reason())
			{
				// REASON_WINDOW_OVERFLOW is omitted here because that case is
				// checked for during sending.
				case Mercury::REASON_INACTIVITY:
				case Mercury::REASON_NO_SUCH_PORT:

					baseApp.addressDead( addr, ne.reason() );
					break;

				default:

					if (hasAddr)
					{
						char buf[ 256 ];
						snprintf( buf, sizeof( buf ),
							"processContinuously( %s )", addr.c_str() );

						baseApp.intNub().reportException( ne , buf );
					}
					else
					{
						baseApp.intNub().reportException(
							ne, "processContinuously" );
					}
					break;
			}
		}
	}
	while (!baseApp.intNub().processingBroken());

	baseApp.intNub().reportPendingExceptions( true /* reportBelowThreshold */ );
	baseApp.extNub().reportPendingExceptions( true /* reportBelowThreshold */ );

	// Make sure all packets have been ACKed.
	baseApp.intNub().processUntilChannelsEmpty();

	INFO_MSG( "BaseApp has shut down.\n" );

	return 0;
}

// main.cpp
