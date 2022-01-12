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

#include "cstdmf/debug.hpp"
#include "db_interface.hpp"
#include "database.hpp"
#include "network/logger_message_forwarder.hpp"
#include "server/bwconfig.hpp"

DECLARE_DEBUG_COMPONENT( 0 )

#ifdef _WIN32  // WIN32PORT

void bwStop()
{
	Database * pDB = Database::pInstance();

	if (pDB != NULL)
	{
		pDB->shutDown();
	}
}

char szServiceDependencies[] = "machined";
#endif

#include "server/bwservice.hpp"

int BIGWORLD_MAIN( int argc, char * argv[] )
{
	{
		bool isRecover = false;
		bool isUpgrade = false;
        bool isUpIndex = false;
		int defsSyncMode = DefsSyncModeInvalid;
		for (int i=1; i<argc; i++)
		{
			if (0 == strcmp(argv[i], "-recover"))
				isRecover = true;
			if (0 == strcmp(argv[i], "-upgrade"))
				isUpgrade = true;
			else if (0 == strncmp(argv[i], "-syncTablesToDefs", 14))
			{
				// __kyl__ (12/12/2005) Used to have defsSyncMode 0,1,2,3 but
				// most users gets very confused about it. Now just have a
				// boolean which maps to mode 0 and 3. Should probably remove
				// redundant code if customers don't show any interest in the
				// other modes.
				defsSyncMode = DefsSyncModeFullSync;
//				defsSyncMode = atoi(&argv[i][14]);
//				if (!Database::isValidDefsSyncMode( defsSyncMode ))
//				{
//					ERROR_MSG( "Invalid defsSyncMode %d\n", defsSyncMode );
//					return 1;
//				}
			}
			if (0 == strcmp(argv[i], "-upindex"))
				isUpIndex = true;

		}

		if (isRecover && isUpgrade)
		{
			ERROR_MSG( "Cannot recover and upgrade at the same time\n" );
			return 1;
		}

		// We don't do the standard static instance trick here because we need
		// to guarentee the shut down order of the objects to avoid an access
		// violation on shut down.
		Database database;
		BW_MESSAGE_FORWARDER( DBMgr, dbMgr, database.nub() );

		START_MSG( "DBMgr" );

		BW_SERVICE_CHECK_POINT( 3000 );

		Database::InitResult result =
			database.init( isRecover, isUpgrade, (DefsSyncMode) defsSyncMode, isUpIndex );
		switch (result)
		{
			case Database::InitResultFailure:
				ERROR_MSG( "Failed to initialise the database\n" );
				return 1;
			case Database::InitResultSuccess:
				BW_SERVICE_UPDATE_STATUS( SERVICE_RUNNING, 0, 0 );
				database.run();
				break;
			case Database::InitResultAutoShutdown:
				database.finalise();
				break;
			default:
				MF_ASSERT(false);
				break;
		}
	}

	INFO_MSG( "DBMgr has shut down.\n" );

	return 0;
}

// main.cpp
