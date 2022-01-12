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

#include "resmgr/bwresource.hpp"
#include "pyscript/script.hpp"
#include "pyscript/py_output_writer.hpp"
#include "romp/xconsole.hpp"
#include "romp/console_manager.hpp"

#include "gizmo/undoredo.hpp"

#include "compile_time.hpp"
#include "appmgr/options.hpp"
#include "appmgr/commentary.hpp"

#include "me_module.hpp"
#include "me_app.hpp"
#include "me_shell.hpp"

#include "guimanager/gui_functor_cpp.hpp"
#include "panel_manager.hpp"

DECLARE_DEBUG_COMPONENT( 0 )
#include "me_error_macros.hpp"

#include "me_scripter.hpp"

/**
 *	This class implements a PyOutputWriter with the added functionality of
 *	writing to the Python console.
 */
class BWOutputWriter : public PyOutputWriter
{
public:
	BWOutputWriter( const char * prefix, const char * fileText ) :
		PyOutputWriter( fileText, /*shouldWritePythonLog = */true )
	{
	}

protected:
	virtual void printMessage( const char * msg );
};


/**
 *	This method implements the default behaviour for printing a message. Derived
 *	class should override this to change the behaviour.
 */
void BWOutputWriter::printMessage( const char * msg )
{
	XConsole * pXC = ConsoleManager::instance().find( "Python" );
	if (pXC != NULL) pXC->print( msg );

	this->PyOutputWriter::printMessage( msg );
}

/**
 *	This function implements a script function. It adds a message to the
 *	commentary console.
 */
static PyObject * py_addCommentaryMsg( PyObject * args )
{
	int id = Commentary::COMMENT;
	char* tag;

	if (!PyArg_ParseTuple( args, "s|i", &tag, &id ))
	{
		PyErr_SetString( PyExc_TypeError, "ModelEditor.addCommentaryMsg(): Argument parsing error." );
		return NULL;
	}

	if ( stricmp( tag, "" ) )
	{
		Commentary::instance().addMsg( std::string( tag ), id );
		dprintf( "Commentary: %s\n", tag );
	}
	else
	{
		Commentary::instance().addMsg( std::string( "NULL" ), Commentary::WARNING );
	}

	Py_Return;
}
PY_MODULE_FUNCTION( addCommentaryMsg, ModelEditor )

/**
 *	This function undoes the most recent operation, returning
 *	its description. If it is passed a positive integer argument,
 *	then it just returns the description for that level of the
 *	undo stack and doesn't actually undo anything.
 *	If there is no undo level, the empty string is returned.
 */
static PyObject * py_undo( PyObject * args )
{
	CWaitCursor wait; // This could potentially take a while
	
	int forStep = -1;
	if (!PyArg_ParseTuple( args, "|i", &forStep ))
	{
		PyErr_SetString( PyExc_TypeError, "ModelEditor.undo() "
			"expects an optional integer argument" );
		return NULL;
	}

	std::string what = UndoRedo::instance().undoInfo( max(0,forStep) );

	if (forStep < 0)
	{
		char buf[256];
		sprintf( buf, "Undoing: %s\n", what.c_str() );
		ME_INFO_MSG( buf );
		UndoRedo::instance().undo();
	}

	return Script::getData( what );
}
PY_MODULE_FUNCTION( undo, ModelEditor )

/**
 *	This function works exactly like undo, it just redoes.
 *
 *	@see py_undo
 */
static PyObject * py_redo( PyObject * args )
{
	CWaitCursor wait; // This could potentially take a while
	
	int forStep = -1;
	if (!PyArg_ParseTuple( args, "|i", &forStep ))
	{
		PyErr_SetString( PyExc_TypeError, "ModelEditor.redo() "
			"expects an optional integer argument" );
		return NULL;
	}

	std::string what = UndoRedo::instance().redoInfo( max(0,forStep) );

	if (forStep < 0)
	{
		char buf[256];
		sprintf( buf, "Redoing: %s\n", what.c_str() );
		ME_INFO_MSG( buf );
		UndoRedo::instance().redo();
	}

	return Script::getData( what );
}
PY_MODULE_FUNCTION( redo, ModelEditor )

/**
 * Adds an undo/redo barrier with the given name
 */
static PyObject * py_addUndoBarrier( PyObject * args )
{

	char* name;
	int skipIfNoChange = 0;
	if (!PyArg_ParseTuple( args, "s|i", &name, &skipIfNoChange ))
	{
		PyErr_SetString( PyExc_TypeError, "ModelEditor.addUndoBarrier() "
			"expects a string and an optional int" );
		return NULL;
	}

	// Add the undo barrier
	UndoRedo::instance().barrier( name, (skipIfNoChange != 0) );

	Py_Return;
}
PY_MODULE_FUNCTION( addUndoBarrier, ModelEditor )


/**
 *	This function saves the options file.
 */
static PyObject * py_saveOptions( PyObject * args )
{
	char * filename = NULL;

	if (!PyArg_ParseTuple( args, "|s", &filename ))
	{
		PyErr_SetString( PyExc_TypeError, "ModelEditor.saveOptions() "
			"expects an optional string argument." );
		return NULL;
	}

	if (filename)
		return Script::getData( Options::save( filename ) );
	else
		return false;
}
PY_MODULE_FUNCTION( saveOptions, ModelEditor )

/**
 *	This function shows or hides a Tool Panel
 */
static PyObject * py_showPanel( PyObject * args )
{
	char * panel = NULL;
	int show = -1;
	if ( !PyArg_ParseTuple( args, "si", &panel, &show ) )
	{
		PyErr_SetString( PyExc_TypeError, "ModelEditor.showPanel() "
			"expects a string and an int argument." );
		return NULL;
	}

	if ( (panel != NULL) && (show != -1) )
		PanelManager::instance()->showPanel( (std::string)(panel), show );

	Py_Return;
}
PY_MODULE_FUNCTION( showPanel, ModelEditor )


/**
 *	This function asks about a tool panel visibility
 */
static PyObject * py_isPanelVisible( PyObject * args )
{
	char * panel = NULL;
	if ( !PyArg_ParseTuple( args, "s", &panel ) )
	{
		PyErr_SetString( PyExc_TypeError, "ModelEditor.isPanelVisible() "
			"expects a string argument." );
		return NULL;
	}

	if ( panel )
		return PyInt_FromLong( PanelManager::instance()->isPanelVisible( (std::string)(panel) ) );

	Py_Return;
}
PY_MODULE_FUNCTION( isPanelVisible, ModelEditor )

/**
 *	AddItemToHistory
 */
static PyObject * py_addItemToHistory( PyObject * args )
{
	char* filePath = NULL;
	if ( !PyArg_ParseTuple( args, "s", &filePath ) )
	{
		PyErr_SetString( PyExc_TypeError, "ModelEditor.addItemToHistory()"
			"expects a string argument." );
		return NULL;
	}

	if ( filePath )
	{
		PanelManager::instance()->ualAddItemToHistory( filePath );
	}

	Py_Return;
}
PY_MODULE_FUNCTION( addItemToHistory, ModelEditor )

static PyObject * py_makeThumbnail( PyObject * args )
{
	const char* filePath = NULL;

	if ( !PyArg_ParseTuple( args, "|s", &filePath ) )
	{
		PyErr_SetString( PyExc_TypeError, "ModelEditor.makeThumbnail()"
			"expects an optional string argument." );
		return NULL;
	}

	if (filePath == NULL)
	{
		filePath = MeApp::instance().mutant()->modelName().c_str();
	}

	if ( strcmp( filePath, "") )
	{
		if (MeModule::instance().renderThumbnail( filePath ))
		{
			char buf[256];
			sprintf( buf, "Taking Thumbnail of \"%s\"\n", MeApp::instance().mutant()->modelName().c_str() );
			ME_INFO_MSG( buf );
		}
	}
	else
	{
		ME_WARNING_MSG( "No thumbnail was created since there is no model loaded.\n" );
	}


	Py_Return;
}
PY_MODULE_FUNCTION( makeThumbnail, ModelEditor )

/**
 *  BW script interface.
 */

bool Scripter::init(DataSectionPtr pDataSection )
{
	std::string scriptPath = BWResource::resolveFilename( "resources/scripts" );

	// Call the general init function
	if (!Script::init( scriptPath ))
	{
		ERROR_MSG( "BigBang::init: Failed to init Script.\n" );
		return false;
	}

	// We implement our own stderr / stdout so we can see Python's output
	PyObject * pSysModule = PyImport_AddModule( "sys" );	// borrowed


	char fileText[ 256 ];
	
#ifdef _DEBUG
		const char * config = "Debug";
#elif defined( _HYBRID )
		const char * config = "Hybrid";
#else
		const char * config = "Release";
#endif

	time_t aboutTime = time( NULL );

	sprintf( fileText, "ModelEditor %s (compiled at %s) starting on %s",
		config,
		aboutCompileTimeString,
		ctime( &aboutTime ) );


	PyObject_SetAttrString( pSysModule,
		"stderr", new BWOutputWriter( "stderr: ", fileText ) );
	PyObject_SetAttrString( pSysModule,
		"stdout", new BWOutputWriter( "stdout: ", fileText ) );

	PyObject * pInit = PyObject_GetAttrString( PyImport_AddModule("keys"), "init" );
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
