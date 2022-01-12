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

#include "cstdmf/config.hpp"

#if ENABLE_WATCHERS

#include "Python.h"

#include "cstdmf/debug.hpp"
DECLARE_DEBUG_COMPONENT2( "Script", 0 )


#include "pyobject_plus.hpp"
#include "pywatcher.hpp"
#include "script.hpp"


// ----------------------------------------------------------------------------
// Section: SequenceWatcher and MappingWatcher
// ----------------------------------------------------------------------------


/**
 *	Function to return a watcher for PySequences
 */
Watcher & PySequence_Watcher()
{
	static WatcherPtr pWatchMe = NULL;
	if (pWatchMe == NULL)
	{
		pWatchMe = new SequenceWatcher<PySequenceSTL>( *(PySequenceSTL*)NULL );

		/*pWatchMe->addChild( "*", new DataWatcher<PyObjectPtrRef>(
			*(PyObjectPtrRef*)NULL ) );*/
		pWatchMe->addChild( "*", new PyObjectWatcher(
			*(PyObjectPtrRef*)NULL ) );
	}
	return *pWatchMe;
}


/**
 *	Function to return a watcher for PyMappings
 */
Watcher & PyMapping_Watcher()
{
	static WatcherPtr pWatchMe = NULL;
	if (pWatchMe == NULL)
	{
		pWatchMe = new MapWatcher<PyMappingSTL>( *(PyMappingSTL*)NULL );

		/*pWatchMe->addChild( "*", new DataWatcher<PyObjectPtrRef>(
			*(PyObjectPtrRef*)NULL ) );*/
		pWatchMe->addChild( "*", new PyObjectWatcher(
			*(PyObjectPtrRef*)NULL ) );
	}
	return *pWatchMe;
}




// ----------------------------------------------------------------------------
// Section: PyObjectWatcher
// ----------------------------------------------------------------------------


// Static initialiser of our fallback watcher
SmartPointer<DataWatcher<PyObjectPtrRef> >	PyObjectWatcher::fallback_ = 
	new DataWatcher<PyObjectPtrRef>( *(PyObjectPtrRef*)NULL, Watcher::WT_READ_WRITE );


/**
 *	Constructor
 */
PyObjectWatcher::PyObjectWatcher( PyObjectPtrRef & popr ) : popr_( popr )
{
}


/**
 *	Get method
 */
bool PyObjectWatcher::getAsString( const void * base, const char * path,
	std::string & result, std::string & desc, Type & type ) const
{
	PyObjectPtrRef & popr = *(PyObjectPtrRef*)( ((const uint)&popr_) + ((const uint)base) );

	// get out the PyObject *
	PyObject * pObject = popr;

	bool ret = false;

	// check against our registry to see if this is a directory
	Watcher * pSpecialWatcher = this->getSpecialWatcher( pObject );

	if (pSpecialWatcher != NULL)
	{	// yes it is - let it handle everything then
		ret = pSpecialWatcher->getAsString( &pObject, path, result, desc, type );
	}		// (should retunr "<DIR>" for empty paths
	else
	{
		// the path had better be empty...
		if (isEmptyPath( path ))
		{	// call the datawatcher
			ret = static_cast<Watcher&>(*fallback_).getAsString(
				&popr, path, result, desc, type );
		}
	}

	Py_DECREF( pObject );
	return ret;
}


/*
 *	Set method
 */
bool PyObjectWatcher::setFromString( void * base, const char * path,
	const char * valueStr )
{
	PyObjectPtrRef & popr = *(PyObjectPtrRef*)( ((const uint)&popr_) + ((const uint)base) );

	bool ret = false;

	// see if it's this watcher we're interested in or one of its chillun
	if (isEmptyPath( path ))
	{
		// ok, set away then
		ret = static_cast<Watcher&>(*fallback_).setFromString( &popr, path, valueStr );
	}
	else
	{
		// it had better be a directory...
		PyObject * pObject = popr;
		Watcher * pSpecialWatcher = this->getSpecialWatcher( pObject );

		if (pSpecialWatcher != NULL)
		{
			ret = pSpecialWatcher->setFromString( &pObject, path, valueStr );
		}

		Py_DECREF( pObject );
	}

	return ret;
}


/**
 *	Visit method
 */
bool PyObjectWatcher::visitChildren( const void * base, const char * path,
	WatcherVisitor & visitor )
{
	PyObjectPtrRef & popr = *(PyObjectPtrRef*)( ((const uint)&popr_) + ((const uint)base) );

	// get out the PyObject *
	PyObject * pObject = popr;

	bool ret = false;

	// check against our registry to see if this is a directory
	Watcher * pSpecialWatcher = this->getSpecialWatcher( pObject );

	// can only pass it down (for whatever action) if it's a directory
	if (pSpecialWatcher != NULL)
	{
		ret = pSpecialWatcher->visitChildren( &pObject, path, visitor );
	}
	// the datawatcher can't visit children (as it's not a directory)

	Py_DECREF( pObject );
	return ret;
}



/**
 *	Add child method
 */
bool PyObjectWatcher::addChild( const char * /*path*/, Watcher * /*pChild*/,
	void * /*withBase*/ )
{
	// Neither this nor any of the sub watchers support the
	// 'addChild' method, sorry [decision may be revised ... maybe
	// the list of special watchers should not be static, but rather
	// stored in each PyObjectWatcher as its children... with the
	// path as the hex of the object type's address ... hmmmm]
	return false;
}


/**
 *	This static method determines whether or not the object in pObject
 *	has its own special watcher. If not, then the default watcher is
 *	used, which just calls the object's 'Str' method. This watcher is
 *	not used for setAsString on the object itself - the default watcher
 *	is always used for that.
 */
Watcher * PyObjectWatcher::getSpecialWatcher( PyObject * pObject )
{
	if (PyString_Check( pObject ))
	{
		return NULL;
	}
	if (PySequenceSTL::Check( pObject ))
	{
		return &PySequence_Watcher();
	}
	else if(PyMappingSTL::Check( pObject ))
	{
		return &PyMapping_Watcher();
	}

	return NULL;
}


// -----------------------------------------------------------------------------
// Section: Misc
// -----------------------------------------------------------------------------

/*~ function BigWorld.setWatcher
 *	@components{ client, base, cell }
 *
 *	This function interacts with the watcher debugging system, allowing the
 *	setting of watcher variables.
 *
 *	The watcher debugging system allows various variables to be displayed and
 *  updated from within the runtime.
 *	When running the engine, pressing CAPSLOCK-F7 brings up the watcher screen.
 *
 *	This displays a list of groups and values.  Groups are in yellow, values
 *	either white (read-write) or grey (read-only).
 *	The currently selected item is highlighted in red.
 *
 *	You can navigate up and down through the list using the PAGEUP and PAGEDOWN
 *	keys.  The HOME key takes you to the top of the list.
 *
 *	If a group is currently selected, the ENTER key closes the current list and
 *	expands that group in its place.  If a read-write item is selected
 *	The enter key allows the value of that item to be edited.  It has no effect
 *	on a read-only item.  Pressing the BACKSPACE key closes the current list,
 *	and returns to the parent list.
 *
 *	Watchers are specified in the C-code using the MF_WATCH macro.  This takes
 *	three arguments, the first is a string specifying the path (parent groups
 *	followed by the item name, separated by forward slashes), the second is the
 *	expression to watch.  The third argument specifies such things as whether
 *	it is read only or not, and has a default of read-write.
 *
 *	The setWatcher function in Python allows the setting of a particular
 *	read-write value.
 *	For example:
 *
 *	@{
 *	>>>	BigWorld.setWatcher( "Client Settings/Terrain/draw", 0 )
 *	@}
 *
 *	This switches off the drawing of the terrain.  You can observe this by expanding
 *	the Client Settings group, expanding the Terrain group, and
 *	looking at the draw item, which will now be zero.
 *
 *	@param	path	the path to the item to modify.
 *	@param	val		the new value for the item.
 */
/**
 *	This script function is used to set watcher values. The script function
 *	takes a string to the watcher value to set and the value to set it too.
 */
static PyObject * py_setWatcher( PyObject * args )
{
	char * path = NULL;
	PyObject * pValue = NULL;

	if (!PyArg_ParseTuple( args, "sO", &path, &pValue ))
	{
		return NULL;
	}

	PyObject * pStr = PyObject_Str( pValue );

	if (pStr)
	{
		char * pCStr = PyString_AsString( pStr );

		if (!Watcher::rootWatcher().setFromString( NULL, path, pCStr ))
		{
			PyErr_Format( PyExc_TypeError, "Failed to set %s to %s.",
				path, pCStr );
			Py_DECREF( pStr );
			return NULL;
		}

		Py_DECREF( pStr );
	}
	else
	{
		PyErr_SetString( PyExc_TypeError, "Invalid second argument." );
		return NULL;
	}

	Py_Return;
}
PY_MODULE_FUNCTION( setWatcher, BigWorld )


/*~ function BigWorld.getWatcher
 *	@components{ client, base, cell }
 *
 *	This function interacts with the watcher debugging system, allowing the
 *	retrieval of watcher variables.
 *
 *	The watcher debugging system allows various variables to be displayed and
 *	updated from within the runtime.
 *	When running the engine, pressing CAPSLOCK-F7 brings up the watcher screen.
 *
 *	This displays a list of groups and values.  Groups are in yellow, values
 *	either white (read-write) or grey (read-only).
 *	The currently selected item is highlighted in red.
 *
 *	You can navigate up and down through the list using the PAGEUP and PAGEDOWN
 *	keys.  The HOME key takes you to the top of the list.
 *
 *	If a group is currently selected, the ENTER key closes the current list and
 *	expands that group in its place.  If a read-write item is selected
 *	the enter key allows the value of that item to be edited.  It has no effect
 *	on a read-only item.  Pressing the BACKSPACE key closes the current list,
 *	and returns to the parent list.
 *
 *	Watchers are specified in the C-code using the MF_WATCH macro.  This takes
 *	three arguments, the first is a string specifying the path (parent groups
 *	followed by the item name, separated by forward slashes),
 *	the second is the expression to watch.  The third argument specifies such
 *	things as whether it is read only or not, and has a default of read-write.
 *
 *	The getWatcher function in Python allows the getting of a particular value.
 *	For example:
 *
 *	@{
 *	>>>	BigWorld.getWatcher( "Client Settings/Terrain/draw" )
 *	1
 *	@}
 *
 *	determines whether or not terrain is being drawn.  You can observe this by
 *	expanding the Client Settings group, expanding the Terrain group, and
 *	looking at the draw item, which will now be one, by default.
 *
 *	@param	path	the path to the item to modify.
 *
 *	@return			the value of that watcher variable.
 */
/**
 *	This script function is used to get watcher values. The script function
 *	takes a string to the watcher value to get. It returns the associated value
 *	as a string.
 */
static PyObject * py_getWatcher( PyObject * args )
{
	char * path = NULL;

	if (!PyArg_ParseTuple( args, "s", &path ))
	{
		return NULL;
	}

	std::string result;
	std::string desc;
	Watcher::Type type;

	if (!Watcher::rootWatcher().getAsString( NULL, path, result, desc, type ))
	{
		PyErr_Format( PyExc_TypeError, "Failed to get %s.", path );
		return NULL;
	}

	return PyString_FromString( result.c_str() );
}
PY_MODULE_FUNCTION( getWatcher, BigWorld )


/*~ function BigWorld.getWatcherDir
 *	@components{ client, base, cell }
 *
 *	This function interacts with the watcher debugging system, allowing the
 *	inspection of a directory watcher.
 *
 *	@param	path	the path to the item to modify.
 *
 *	@return			a list containing an entry for each child watcher. Each
 *					entry is a tuple of child type, label and value. The child
 *					type is 1 for read-only, 2 for read-write and 3 for a
 *					directory.
 */
namespace
{

	class MyVisitor : public WatcherVisitor
	{
	public:
		MyVisitor() : pList_( PyList_New( 0 ) )	{}
		~MyVisitor()							{ Py_DECREF( pList_ ); }

		virtual bool visit( Watcher::Type type,
			const std::string & label,
			const std::string & desc,
			const std::string & valueStr )
		{
			PyObject * pTuple = PyTuple_New( 3 );
			PyTuple_SET_ITEM( pTuple, 0, Script::getData( int(type) ) );
			PyTuple_SET_ITEM( pTuple, 1, Script::getData( label ) );
			PyTuple_SET_ITEM( pTuple, 2, Script::getData( valueStr ) );

			PyList_Append( pList_, pTuple );
			Py_DECREF( pTuple );

			return true;
		}

		PyObject * pList_;
	};

}

static PyObject * py_getWatcherDir( PyObject * args )
{
	char * path = NULL;

	if (!PyArg_ParseTuple( args, "s", &path ))
	{
		return NULL;
	}

	MyVisitor visitor;

	if (!Watcher::rootWatcher().visitChildren( NULL, path, visitor ))
	{
		PyErr_Format( PyExc_TypeError, "Failed to get %s.", path );
		return NULL;
	}

	Py_INCREF( visitor.pList_ );

	return visitor.pList_;

}
PY_MODULE_FUNCTION( getWatcherDir, BigWorld )


/**
 *	This class is used to associate a watcher variable with
 *	Python scripts. The functionality is similar to FunctionWatcher.
 *
 *	The getFunction is called to retrieve the value of watcher
 *	variable, while setFunction, if exists, is called when
 *	watcher variable is modified.
 *
 * 	If setFunction is absent the watcher will be read-only.
 *
 */
class PyFunctionWatcher : public Watcher
{
public:
	/// @name Construction/Destruction
	//@{
	PyFunctionWatcher( const char * path, PyObject * getFunction,
			PyObject * setFunction = NULL, const char * desc = NULL ) :
		getFunction_( getFunction ),
		setFunction_( setFunction )
	{
		Py_INCREF( getFunction_ );
		Py_XINCREF( setFunction_ );
		if (desc)
		{
			std::string s( desc );
			comment_ = s;
		}
	}

	~PyFunctionWatcher()
	{
		Py_XDECREF( getFunction_ );
		Py_XDECREF( setFunction_ );
	}

	//@}

protected:
	/// @name Overrides from Watcher.
	//@{
	// Override from Watcher.
	virtual bool getAsString( const void * base, const char * path,
		std::string & result, std::string & desc, Type & type ) const
	{
		if (isEmptyPath( path ))
		{
			Py_INCREF( getFunction_ );
			PyObject * pValue = Script::ask( getFunction_,
				PyTuple_New( 0 ), "PyFunctionWatcher" );

			if (pValue)
			{
				PyObject * pValStr = PyObject_Str( pValue );

				if (pValStr)
				{
					result = PyString_AsString( pValStr );
					type =
						(setFunction_ != NULL) ? WT_READ_WRITE : WT_READ_ONLY;
					Py_DECREF( pValStr );

					return true;
				}
				else
				{
					PyErr_PrintEx(0);
				}
				Py_DECREF( pValue );
			}
			else
			{
				PyErr_PrintEx(0);
			}
		}

		return false;
	}

	// Override from Watcher.
	virtual bool setFromString( void * base, const char * path,
		const char * valueStr )
	{
		bool result = false;

		if (isEmptyPath( path ) && (setFunction_ != NULL))
		{
			Py_INCREF( setFunction_ );
			result = Script::call( setFunction_,
				Py_BuildValue( "(s)", valueStr ), "PyFunctionWatcher" );
		}

		return result;
	}
	//@}

private:

	PyObject * getFunction_;
	PyObject * setFunction_;
};



/*~ function BigWorld.addWatcher
 *	@components{ client, base, cell }
 * 
 *
 *	This function interacts with the watcher debugging system, allowing the
 *	creation of watcher variables.
 *
 *	The addWatcher function in Python allows the creation of a particular
 *	watcher variable.
 *	For example:
 *
 *	@{
 *	>>> maxBandwidth = 20000
 *	>>> def getMaxBps( ):
 *	>>>     return str(maxBandwidth)
 *	>>> def setMaxBps( bps ):
 *	>>>     maxBandwidth = int(bps)
 *	>>>     setBandwidthPerSecond( int(bps) )
 *	>>>
 *	>>> BigWorld.addWatcher( "Comms/Max bandwidth per second", getMaxBps, setMaxBps )
 *	@}
 *
 *	This adds a watcher variable under "Comms" watcher directory. The function getMaxBps
 *  is called to obtain the watcher value and setMaxBps is called when the watcher is modified.
 *
 *	@param	path	the path to the item to create.
 *	@param	getFunction	the function to call when retrieving the watcher variable.
 *						This function takes no argument and returns a string representing
 *						the watcher value.
 *	@param	setFunction	(optional)the function to call when setting the watcher variable
 *						This function takes a string argument as the new watcher value.
 *						This function does not return a value. If this function does not
 * 						exist the created watcher is a read-only watcher variable.
 */
static PyObject * py_addWatcher( PyObject * args )
{
	char * path = NULL;
	char * desc = NULL;
	PyObject * getFunction = NULL;
	PyObject * setFunction = NULL;

	if (!PyArg_ParseTuple( args, "sO|Os", &path, &getFunction, &setFunction, &desc ))
	{
		return NULL;
	}

	if (!PyCallable_Check( getFunction ) ||
		( (setFunction != NULL) && !PyCallable_Check( setFunction ) ))
	{
		return NULL;
	}

	Watcher::rootWatcher().addChild( 
		path, new PyFunctionWatcher( path, getFunction, setFunction, desc ) );

	Py_Return;

}
PY_MODULE_FUNCTION( addWatcher, BigWorld )


/*~ function BigWorld.delWatcher
 *	@components{ client, base, cell }
 *	This function interacts with the watcher debugging system, allowing the
 *	deletion of watcher variables.
 *
 *	@param path the path of the watcher to delete.
 */
static PyObject * py_delWatcher( PyObject * args )                              
{                                                                               
	char * path = NULL;                                                     
	if (!PyArg_ParseTuple( args, "s", &path ))                              
	{                                                                       
		return NULL;                                                    
	}                                                                       
	if (!Watcher::rootWatcher().removeChild(path))                          
	{                                                                       
		PyErr_Format( PyExc_ValueError, "Watcher path not found: %s.", path);
		return NULL;                                                    
	}                                                                       
	Py_Return;                                                              
}                                                                               
PY_MODULE_FUNCTION( delWatcher, BigWorld )                                      


int PyWatcher_token = 1;
int PyScript_token = PyWatcher_token;



#else // ENABLE_WATCHERS



static PyObject * py_addWatcher( PyObject * args )
{
	Py_Return;
}

PY_MODULE_FUNCTION( addWatcher, BigWorld )


int PyScript_token = 1;


#endif // ENABLE_WATCHERS

// pywatcher.cpp
