/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// AssetProcessorDLL.cpp : Defines the entry point for the DLL application.
//

#include "pch.hpp"
#include "assetprocessor.hpp"
#include "asset_processor_script.hpp"
#include "resmgr/bwresource.hpp"
#include "pyscript/script.hpp"
#include <direct.h>

extern int ResMgr_token;


#ifdef _MANAGED
#pragma managed(push, off)
#endif

ASSETPROCESSOR_API void init_AssetProcessor()
{	
	if (!BWResource::init( 0, NULL ))
	{
		return;
	}

	int tokens = ResMgr_token;

	if (!Script::init( "entities/client", "client" + tokens ) )
	{
		return;
	}

	AssetProcessorScript::init();

	PyErr_Clear();

    return;
}

#ifdef _MANAGED
#pragma managed(pop)
#endif
