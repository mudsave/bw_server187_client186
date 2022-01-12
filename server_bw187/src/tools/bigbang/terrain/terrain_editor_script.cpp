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
#include "big_bang_script.hpp"

#include "../commentary.hpp"
#include "../module_manager.hpp"

// #include "ashes/script_gui.hpp"

#include "cstdmf/debug.hpp"

#include "input/input.hpp"

#include "pyscript/res_mgr_script.hpp"
#include "pyscript/script.hpp"
#include "pyscript/script_math.hpp"
#include "pyscript/py_output_writer.hpp"

#include "resmgr/bwresource.hpp"

#include "romp/particle_system.hpp"
#include "romp/fog_controller.hpp"
#include "romp/weather.hpp"

#include "xactsnd/soundmgr.hpp"
#include "xactsnd/pyfxsound.hpp"

#include "../big_bang.hpp"


#ifndef CODE_INLINE
#include "terrain_editor_script.ipp"
#endif


DECLARE_DEBUG_COMPONENT2( "Script", 0 )

// -----------------------------------------------------------------------------
// Section: 'BigBang' Module
// -----------------------------------------------------------------------------

/**
 *	This function implements a script function. It returns whether or not the
 *	given key is down.
 */
static PyObject * py_bigBang( PyObject * self, PyObject * args )
{
	return &BigBang::instance();
}



/**
 *	This function implements a script function. It returns whether or not the
 *	given key is down.
 */
static PyObject * py_isKeyDown( PyObject * self, PyObject * args )
{
	int	key;
	if (!PyArg_ParseTuple( args, "i", &key ))
	{
		PyErr_SetString( PyExc_TypeError,
			"py_isKeyDown: Argument parsing error." );
		return NULL;
	}

	return PyInt_FromLong( InputDevices::isKeyDown( (KeyEvent::Key)key ) );
}


/**
 *	This function implements a script function. It returns the key given its
 *	name
 */
static PyObject * py_stringToKey( PyObject *, PyObject * args )
{
	char * str;
	if (!PyArg_ParseTuple( args, "s", &str ))
	{
		PyErr_SetString( PyExc_TypeError,
			"py_stringToKey: Argument parsing error." );
		return NULL;
	}

	return PyInt_FromLong( KeyEvent::stringToKey( str ) );
}


/**
 *	This function implements a script function. It returns the name of the given
 *	key.
 */
static PyObject * py_keyToString( PyObject *, PyObject * args )
{
	int	key;
	if (!PyArg_ParseTuple( args, "i", &key ))
	{
		PyErr_SetString( PyExc_TypeError,
			"py_keyToString: Argument parsing error." );
		return NULL;
	}

	return PyString_FromString( KeyEvent::keyToString(
		(KeyEvent::Key) key ) );
}


/**
 *	This function implements a script function. It plays the named sound effect.
 */
static PyObject* py_playFx( PyObject * self, PyObject * args )
{
	char* tag;
	float x, y, z;

	if (!PyArg_ParseTuple( args, "s(fff)", &tag , &x, &y, &z ))
	{
		PyErr_SetString( PyExc_TypeError, "py_playFx: Argument parsing error." );
		return NULL;
	}

	//TRACE_MSG( "py_playFx(%s)\n", tag );

	soundMgr().playFx(tag, Vector3(x, y, z));

	Py_Return;
}


/**
 *	This function implements a script function. It plays the named sound effect
 *	with a delay.
 */
static PyObject * py_playFxDelayed( PyObject * self, PyObject * args )
{
	char* tag;
	float x, y, z, delay;

	if (!PyArg_ParseTuple( args, "s(fff)f", &tag , &x, &y, &z, &delay ))
	{
		PyErr_SetString( PyExc_TypeError, "py_playFxDelayed: Argument parsing error." );
		return NULL;
	}

	//TRACE_MSG( "py_playFxDelayed(%s)\n", tag );

	soundMgr().playFxDelayed(tag, delay, Vector3(x, y, z));

	Py_Return;
}


/**
 *	This function implements a script function. It returns a reference to a
 *	loaded sound.
 */
static PyObject* py_fxSound( PyObject* self, PyObject* args )
{
	char* tag;

	if (!PyArg_ParseTuple( args, "s", &tag ))
	{
		PyErr_SetString( PyExc_TypeError, "py_fxSound: Argument parsing error." );
		return NULL;
	}

	DEBUG_MSG( "py_fxSound: %s\n", tag );

	PyFxSound* snd = new PyFxSound( tag );

	if (!snd->isValid())
	{
		PyErr_Format( PyExc_ValueError, "py_fxSound: No such sound: %s", tag );
		Py_DECREF( snd );
		return NULL;
	}

	return snd;
}


/**
 *	This function implements a script function. It plays the named Simple sound.
 */
static PyObject * py_playSimple( PyObject * self, PyObject * args )
{
	char* tag;

	if (!PyArg_ParseTuple( args, "s", &tag ))
	{
		PyErr_SetString( PyExc_TypeError, "py_playSimple: Argument parsing error." );
		return NULL;
	}

	TRACE_MSG( "py_playSimple(%s)\n", tag );

	soundMgr().playSimple(tag);

	Py_Return;
}



/**
 *	This function implements a script function. It adds a message to the
 *	commentary console.
 */
static PyObject * py_addCommentaryMsg( PyObject * self, PyObject * args )
{
	int id = Commentary::COMMENT;
	char* tag;

	if (!PyArg_ParseTuple( args, "s|i", &tag, &id ))
	{
		PyErr_SetString( PyExc_TypeError, "py_addCommentaryMsg: Argument parsing error." );
		return NULL;
	}

	if ( stricmp( tag, "" ) )
	{
		Commentary::instance().addMsg( std::string( tag ), id );
	}
	else
	{
		Commentary::instance().addMsg( std::string( "NULL" ), Commentary::WARNING );
	}

	Py_Return;
}


/**
 *	This function implements a script function. It pushes a module
 *	onto the application's module stack.
 */
static PyObject * py_push( PyObject * self, PyObject * args )
{
	char* id;

	if (!PyArg_ParseTuple( args, "s", &id ))
	{
		PyErr_SetString( PyExc_TypeError, "py_push: Argument parsing error." );
		return NULL;
	}

	ModuleManager::instance().push( std::string(id) );

	Py_Return;
}


/**
 *	This function implements a script function. It pops the current
 *	module from the application's module stack.
 */
static PyObject * py_pop( PyObject * self, PyObject * args )
{
	ModuleManager::instance().pop();

	Py_Return;
}


/// Yes, these are globals
struct TimerRecord
{
	/**
	 *	This method returns whether or not the input record occurred later than
	 *	this one.
	 *
	 *	@return True if input record is earlier (higher priority),
	 *		false otherwise.
	 */
	bool operator <( const TimerRecord & b ) const
	{
		return b.time < this->time;
	}

	float		time;			///< The time of the record.
	PyObject	* function;		///< The function associated with the record.
};


typedef std::priority_queue<TimerRecord>	Timers;
Timers	gTimers;


/**
 *	Registers a callback function to be called after a certain time,
 *	 but not before the next tick. (If registered during a tick
 *	 and it has expired then it will go off still - add a miniscule
 *	 amount of time to BigWorld.time() to prevent this if unwanted)
 *	Non-positive times are interpreted as offsets from the current time.
 */
static PyObject * py_callback( PyObject * self, PyObject * args )
{
	float		time = 0.f;
	PyObject *	function = NULL;

	if (!PyArg_ParseTuple( args, "fO", &time, &function ) ||
		function == NULL || !PyCallable_Check( function ) )
	{
		PyErr_SetString( PyExc_TypeError, "py_callback: Argument parsing error." );
		return NULL;
	}

	if (time < 0) time = 0.f;

	//TODO
	//time = EntityManager::getTimeNow() + time;
	Py_INCREF( function );

	TimerRecord		newTR = { time, function };
	gTimers.push( newTR );

	Py_Return;
}


static PyMethodDef s_TerrainEditorScriptMethods[] =
{
	{ "bigBang",			py_bigBang,				METH_VARARGS	},
	{ "isKeyDown",			py_isKeyDown,			METH_VARARGS	},
	{ "stringToKey",		py_stringToKey,			METH_VARARGS	},
	{ "keyToString",		py_keyToString,			METH_VARARGS	},
	{ "playFx",				py_playFx,				METH_VARARGS	},
	{ "playFxDelayed",		py_playFxDelayed,		METH_VARARGS	},
	{ "playSimple",			py_playSimple,			METH_VARARGS	},
	{ "fxSound",			py_fxSound,				METH_VARARGS	},
	{ "addComment",			py_addCommentaryMsg,	METH_VARARGS	},
	{ "push",				py_push,				METH_VARARGS	},
	{ "pop",				py_pop,					METH_VARARGS	},
	{ "callback",			py_callback,			METH_VARARGS	},
	{ NULL,					NULL,					0				}
};



// -----------------------------------------------------------------------------
// Section: TerrainEditorScript namespace functions
// -----------------------------------------------------------------------------

/**
 *	This method initialises the TerrainEditor script.
 */
bool TerrainEditorScript::init( DataSectionPtr pDataSection )
{
	std::string scriptPath = BWResource::resolveFilename( "resources/scripts" );

	// Call the general init function
	if (!Script::init( scriptPath ))
	{
		CRITICAL_MSG( "BigBang::init: Failed to init Script.\n" );
		return false;
	}

	// Initialise the ResMgr module
	if (!ResMgrScript::init())
	{
		CRITICAL_MSG( "BigBang::init: Failed to init ResMgr script.\n" );
		return false;
	}

	// Initialise the TerrainEditor module
	PyObject * pTerrainEditorModule = PyImport_AddModule( "BigBang" );
	Py_InitModule( "BigBang", s_TerrainEditorScriptMethods );
		// pTerrainEditorModule is a 'borrowed reference'

	if (!pTerrainEditorModule)
	{
		return false;
	}

	// We implement our own stderr / stdout so we can see Python's output
	PyObject * pSysModule = PyImport_AddModule( "sys" );	// borrowed

	PyObject_SetAttrString( pSysModule, "stderr", new PyOutputWriter( "stderr: ", "" ) );
	PyObject_SetAttrString( pSysModule, "stdout", new PyOutputWriter( "stdout: ", "" ) );

	return true;
}


/**
 *	This method does the script clean up.
 */
void TerrainEditorScript::fini()
{
	Script::fini();
}


// terraineditor_script.cpp
