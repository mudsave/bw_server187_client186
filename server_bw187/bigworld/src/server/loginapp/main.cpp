/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "loginapp.hpp"

#include "cstdmf/debug.hpp"

#include "network/logger_message_forwarder.hpp"
#include "network/portmap.hpp"

#include "resmgr/bwresource.hpp"

#include "server/bwconfig.hpp"

DECLARE_DEBUG_COMPONENT( 0 )

// Needed to make a function out of this because g++ was complaining if htons
// was used outside of a function.
static unsigned short loginPort( int argc, char * argv[] )
{
	uint16 loginPort = BWConfig::get( "loginApp/port", PORT_LOGIN );

	for (int i = 1; i < argc - 1; ++i)
	{
		if (strcmp( argv[i], "-loginPort" ) == 0)
		{
			loginPort = atoi( argv[ i + 1 ] );
		}
	}

	return loginPort;
}


/**
 *	This function is the entry point to this process.
 */
#ifdef _WIN32  // WIN32PORT
#include <signal.h>

void bwStop()
{
	raise( SIGINT );
}

char szServiceDependencies[] = "machined";

#endif // _WIN32

#include "server/bwservice.hpp"

int BIGWORLD_MAIN( int argc, char * argv[] )
{
	// Initialise the networking stuff
	uint16 loginPort = ::loginPort( argc, argv );
	LoginApp loginApp( loginPort );

	BW_MESSAGE_FORWARDER( LoginApp, loginApp, loginApp.intNub() );

	START_MSG( "LoginApp" );
	BW_SERVICE_CHECK_POINT( 3000 );


	if (!loginApp.init( argc, argv, loginPort ))
	{
		ERROR_MSG( "Failed to initialise Login App\n" );
		return 1;
	}

	INFO_MSG( "---- LoginApp is running ----\n" );

	if (!loginApp.isDBReady())
	{
		INFO_MSG( "Database is not ready yet\n" );
	}

	BW_SERVICE_UPDATE_STATUS( SERVICE_RUNNING, 0, 0 );

	loginApp.run();

	INFO_MSG( "LoginApp has shut down.\n" );

	return 0;
}


/**
 *	An instance of this class is used to handle resource version control
 *	messages.
 */
/*
// Not doing this ... yet...
class ResourceVersionControlHandler : public Mercury::InputMessageHandler
{
public:
	ResourceVersionControlHandler() { }
	virtual ~ResourceVersionControlHandler() {}

	virtual void handleMessage( const Mercury::Address & source,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data )
	{
		LoginInterface::resourceVersionControlArgs & args =
			*(LoginInterface::resourceVersionControlArgs*)
			data.retrieve( header.length );

		switch (args.activity)
		{
		case 0:
			g_latestVersion = args.version;
			g_impendingVersion = args.version;
			INFO_MSG( "ResourceVersionControlHandler: Received version %d\n",
				int(g_latestVersion) );
			break;
		default:
			ERROR_MSG( "ResourceVersionControlHandler: Unknown activity %d\n",
				int(args.activity) );
			break;
		}
	}
};
*/


// main.cpp
