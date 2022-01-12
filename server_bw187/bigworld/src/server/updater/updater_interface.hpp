/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

/*
#ifndef UPDATER_INTERFACE_HPP_ONCE
#define UPDATER_INTERFACE_HPP_ONCE

#include "network/basictypes.hpp"

#endif
*/

#if defined( DEFINE_INTERFACE_HERE ) || defined( DEFINE_SERVER_HERE )
#undef UPDATER_INTERFACE_HPP
#endif

#ifndef UPDATER_INTERFACE_HPP
#define UPDATER_INTERFACE_HPP


#include "server/common.hpp"
#include "network/interface_minder.hpp"
// #include "server/reviver_subject.hpp"


// -----------------------------------------------------------------------------
// Section: Helper macro
// -----------------------------------------------------------------------------

#define BEGIN_UPDATER_MSG( NAME )											\
	BEGIN_HANDLED_STRUCT_MESSAGE( NAME,										\
			UpdaterMessageHandler< UpdaterInterface::NAME##Args >,			\
			&UpdateManager::NAME )											\


// -----------------------------------------------------------------------------
// Section: Updater interface
// -----------------------------------------------------------------------------

#pragma pack(push, 1)
BEGIN_MERCURY_INTERFACE( UpdaterInterface )

	BEGIN_UPDATER_MSG( launchNextVersion )
		char patchPath[1024];
		char cvsInfo[128];
		bool acceptVolatileRes;
		int  downloadWindow;	// in seconds
	END_STRUCT_MESSAGE()

	BEGIN_UPDATER_MSG( ackStep )
		int step;
	END_STRUCT_MESSAGE()

	// askStep

	// axeStep


	MERCURY_HANDLED_VARIABLE_MESSAGE( returnResourceFragment, 2,
		ReturnResourceFragmentHandler, NULL )

	//MF_REVIVER_PING_MSG()

END_MERCURY_INTERFACE()
#pragma pack(pop)

// Cleanup
#undef BEGIN_UPDATER_MSG

#endif // UPDATER_INTERFACE_HPP
