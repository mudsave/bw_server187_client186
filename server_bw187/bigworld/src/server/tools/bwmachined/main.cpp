/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifdef _WIN32
# include "windows_machine_guard.hpp"
# include <string>

enum
{
	SIGINT,
	SIGQUIT,
	SIGUSR1
};
#else
# include "linux_machine_guard.hpp"
#endif

#include "bwmachined.hpp"

#ifdef _WIN32  // WIN32PORT

void bwStop()
{
	ep.close();
}

char szServiceDependencies[] = "+TDI~+Network~";

#endif // _WIN32

#include "server/bwservice.hpp"

static char usage[] =
	"Usage: %s [args]\n"
	"-f/--foreground   Run machined in the foreground (i.e. not as a daemon)\n";

int BIGWORLD_MAIN_NO_RESMGR( int argc, char * argv[] )
{
	bool daemon = true;
	for (int i = 1; i < argc; ++i)
	{
		if (!strcmp( argv[i], "-f" ) || !strcmp( argv[i], "--foreground" ))
			daemon = false;

		else if (strcmp( argv[i], "--help" ) == 0)
		{
			printf( usage, argv[0] );
			return 0;
		}

		else
		{
			fprintf( stderr, "Invalid argument: '%s'\n", argv[i] );
			return 1;
		}
	}

	openlog( argv[0], 0, LOG_DAEMON );
	initProcessState( daemon );
	srand( (int)timestamp() );

#ifndef _WIN32  // WIN32PORT
	rlimit rlimitData = { RLIM_INFINITY, RLIM_INFINITY };
	setrlimit( RLIMIT_CORE, &rlimitData );
#endif

	BW_SERVICE_UPDATE_STATUS( SERVICE_RUNNING, 0, 0 );

	BWMachined machined;
	if (BWMachined::pInstance())
		return machined.run();
	else
		return 1;
}
