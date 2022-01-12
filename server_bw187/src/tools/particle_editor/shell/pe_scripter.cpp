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
#include "pe_scripter.hpp"
#include "resmgr/bwresource.hpp"
#include "pyscript/script.hpp"
#include "pyscript/py_output_writer.hpp"
#include "romp/xconsole.hpp"
#include "romp/console_manager.hpp"
#include "compile_time.hpp"

DECLARE_DEBUG_COMPONENT2( "Shell", 0 )

/**
 *	This class implements a PyOutputWriter with the added functionality of
 *	writing to the Python console.
 */
class BWOutputWriter : public PyOutputWriter
{
public:
	BWOutputWriter
    ( 
        const char      *prefix, 
        const char      *fileText 
    ) 
    :
	PyOutputWriter(fileText, true)
	{
	}

protected:
	/*virtual*/ void printMessage(char const *msg);
};

void BWOutputWriter::printMessage(char const *msg)
{
	XConsole * pXC = ConsoleManager::instance().find( "Python" );
	if (pXC != NULL) pXC->print( msg );
	this->PyOutputWriter::printMessage( msg );
}

/**
 *  BW script interface.
 */
bool Scripter::init(DataSectionPtr pDataSection)
{
	std::string scriptPath = 
        BWResource::resolveFilename("resources/scripts");

	// Call the general init function
	if (!Script::init(scriptPath))
	{
		return false;
	}

	// We implement our own stderr / stdout so we can see Python's output
	PyObject * pSysModule = PyImport_AddModule( "sys" );	// borrowed

	char fileText[256];
	
#ifdef _DEBUG
		const char * config = "Debug";
#elif defined( _HYBRID )
		const char * config = "Hybrid";
#else
		const char * config = "Release";
#endif

	time_t aboutTime = time( NULL );

	sprintf( fileText, "ParticleEditor %s (compiled at %s) starting on %s",
		config,
		aboutCompileTimeString,
		ctime( &aboutTime ) );

	PyObject_SetAttrString( pSysModule,
		"stderr", new BWOutputWriter( "stderr: ", fileText ) );
	PyObject_SetAttrString( pSysModule,
		"stdout", new BWOutputWriter( "stdout: ", fileText ) );

	PyObject *pInit = 
        PyObject_GetAttrString(PyImport_AddModule("keys"), "init");
	if (pInit != NULL)
	{
		PyRun_SimpleString( PyString_AsString(pInit) );
	}
	PyErr_Clear();

	return true;
}

void Scripter::fini()
{
	Script::fini();
}
