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

#include "options.hpp"

#include "cstdmf/debug.hpp"

#ifndef CODE_INLINE
#include "options.ipp"
#endif

#include "pyscript/pyobject_plus.hpp"
#include "pyscript/script_math.hpp"


DECLARE_DEBUG_COMPONENT2( "App", 0 );

// -----------------------------------------------------------------------------
// Section: Statics and globals
// -----------------------------------------------------------------------------

Options Options::instance_;

// -----------------------------------------------------------------------------
// Section: Options
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
Options::Options()
{
}


/**
 *	This method initialises this object from command line options.
 */
bool Options::init( int argc, char * argv[], bool useUserTag /* = true*/ )
{
	// Include the current directory in the default path, incase the current
	// dir changes before save is called; we want to still save to the same
	// file we loaded from
	char buffer[1024];
	::GetCurrentDirectory( sizeof( buffer ), buffer );
	strcat( buffer, "\\options.xml" );

	char * optionsFilename = buffer;

	for (int i = 0; i < argc - 1; i++)
	{
		if (strcmp( "--options", argv[i] ) == 0 ||
			strcmp( "-o", argv[i] ) == 0)
		{
			// TODO:PM Handle filenames with spaces

			if (argv[ i+1 ][0] != '"')
			{
				optionsFilename = argv[ i+1 ];
			}
			else
			{
				CRITICAL_MSG( "Options::init:"
					"Options filename cannot have spaces\n" );

				return false;
			}
		}
	}

	return init( optionsFilename, useUserTag );
}


bool Options::init( const std::string& optionsFilenameString, bool useUserTag /* = true*/ )
{
	const char* optionsFilename = optionsFilenameString.c_str();
	INFO_MSG( "Options file is %s\n", optionsFilename );

	if (instance_.options_.load( optionsFilename ) != DataHandle::DHE_NoError)
	{
		CRITICAL_MSG( "Failed to load \"%s\". Check paths.xml\n",
			optionsFilename );

		return false;
	}

	instance_.pRootSection_ = instance_.options_.getRootSection();
	MF_ASSERT( instance_.pRootSection_ );

	if (instance_.pRootSection_ == static_cast<DataSection *>( NULL ) ||
		instance_.pRootSection_->countChildren() == 0)
	{
		WARNING_MSG( "Options::init: "
			"Options file is empty or nonexistent.\n" );
	}
	else if (useUserTag)
	{
		//create the user name
		char buf[256];
		int num;
		num = ::GetEnvironmentVariable( "USERNAME", buf, 256 );
		if ( num >= 2 )
		{
			char userTag[3];
			userTag[0] = buf[0];
			userTag[1] = buf[num-1];
			userTag[2] = 0;
			strlwr( userTag );
			pRoot()->writeString( "userTag", userTag );
		}
	}

	return true;
}


/**
 *	This static method saves the current options.
 */
bool Options::save( const char * path )
{
	return instance_.options_.save( path ? path : "" ) == DataHandle::DHE_NoError;
}

bool Options::optionExists( const std::string& name )
{
	return instance_.pRoot()->openSection( name );
}

void Options::setOptionString( const std::string& name, const std::string& value )
{
	instance_.pRoot()->writeString( name, value );
	instance_.cache_[ name ] = value;
}

PY_MODULE_STATIC_METHOD( Options, setOptionString, BigBang )

PyObject * Options::py_setOptionString( PyObject * args )
{
	char * name = NULL;
	char * value = NULL;
	if( PyArg_ParseTuple( args, "ss", &name, &value ) )
	{
		setOptionString( name, value );
	}

	Py_Return;
}

std::string Options::getOptionString( const std::string& name )
{
	if( instance_.cache_.find( name ) == instance_.cache_.end() )
		instance_.cache_[ name ] = instance_.pRoot()->readString( name );

	return instance_.cache_[ name ];
}

std::string Options::getOptionString( const std::string& name, const std::string& defaultVal )
{
	if( instance_.cache_.find( name ) == instance_.cache_.end() )
		instance_.cache_[ name ] = instance_.pRoot()->readString( name, defaultVal );

	return instance_.cache_[ name ];
}

PY_MODULE_STATIC_METHOD( Options, getOptionString, BigBang )

PyObject * Options::py_getOptionString( PyObject * args )
{
	char * name = NULL;
	char * defaultVal = NULL;

	if( PyArg_ParseTuple( args, "ss", &name, &defaultVal ) )
	{
		return Py_BuildValue( "s", getOptionString( name, defaultVal ).c_str() );
	}
	else if( PyArg_ParseTuple( args, "s", &name ) )
	{
		PyErr_Clear();
		return Py_BuildValue( "s", getOptionString( name ).c_str() );
	}

	PyErr_Clear();
	return Py_BuildValue( "s", "" );
}

void Options::setOptionInt( const std::string& name, int value )
{
	instance_.pRoot()->writeInt( name, value );
	instance_.cache_[ name ] = instance_.pRoot()->readString( name );
}

PY_MODULE_STATIC_METHOD( Options, setOptionInt, BigBang )

PyObject * Options::py_setOptionInt( PyObject * args )
{
	char * name = NULL;
	int value;
	if( PyArg_ParseTuple( args, "si", &name, &value ) )
	{
		setOptionInt( name, value );
	}

	Py_Return;
}

int Options::getOptionInt( const std::string& name )
{
	if( instance_.cache_.find( name ) == instance_.cache_.end() )
		instance_.cache_[ name ] = instance_.pRoot()->readString( name );

	return instance_.cache_[ name ].empty() ? 0 : atoi( instance_.cache_[ name ].c_str() );
}

int Options::getOptionInt( const std::string& name, int defaultVal )
{
	if( instance_.cache_.find( name ) == instance_.cache_.end() )
	{
		char formattedString[512];
		sprintf( formattedString, "%d", defaultVal );
		instance_.cache_[ name ] = instance_.pRoot()->readString( name, formattedString );
	}

	return instance_.cache_[ name ].empty() ? 0 : atoi( instance_.cache_[ name ].c_str() );
}

PY_MODULE_STATIC_METHOD( Options, getOptionInt, BigBang )

PyObject * Options::py_getOptionInt( PyObject * args )
{
	char * name = NULL;
	int defaultVal = 0;

	if( PyArg_ParseTuple( args, "si", &name, &defaultVal ) )
	{
		return Py_BuildValue( "i", getOptionInt( name, defaultVal ) );
	}
	else if( PyArg_ParseTuple( args, "s", &name ) )
	{
		PyErr_Clear();
		return Py_BuildValue( "i", getOptionInt( name ) );
	}

	PyErr_Clear();
	return Py_BuildValue( "i", 0 );
}

void Options::setOptionBool( const std::string& name, bool value )
{
	instance_.pRoot()->writeBool( name, value );
	instance_.cache_[ name ] = instance_.pRoot()->readString( name );
}

bool Options::getOptionBool( const std::string& name )
{
	if( instance_.cache_.find( name ) == instance_.cache_.end() )
		instance_.cache_[ name ] = instance_.pRoot()->readString( name );

	return stricmp( instance_.cache_[ name ].c_str(), "true" ) == 0 ? true : false;
}

bool Options::getOptionBool( const std::string& name, bool defaultVal )
{
	if( instance_.cache_.find( name ) == instance_.cache_.end() )
	{
		char formattedString[512];
		sprintf( formattedString, "%s", defaultVal ? "true" : "false" );
		instance_.cache_[ name ] = instance_.pRoot()->readString( name, formattedString );
	}

	if( stricmp( instance_.cache_[ name ].c_str(), "true" ) == 0 )
		return true;
	if( stricmp( instance_.cache_[ name ].c_str(), "false" ) == 0 )
		return false;
	return defaultVal;
}

void Options::setOptionFloat( const std::string& name, float value )
{
	instance_.pRoot()->writeFloat( name, value );
	instance_.cache_[ name ] = instance_.pRoot()->readString( name );
}

PY_MODULE_STATIC_METHOD( Options, setOptionFloat, BigBang )

PyObject * Options::py_setOptionFloat( PyObject * args )
{
	char * name = NULL;
	float value;
	if( PyArg_ParseTuple( args, "sf", &name, &value ) )
	{
		setOptionFloat( name, value );
	}

	Py_Return;
}

float Options::getOptionFloat( const std::string& name )
{
	if( instance_.cache_.find( name ) == instance_.cache_.end() )
		instance_.cache_[ name ] = instance_.pRoot()->readString( name );

	return (float)atof( instance_.cache_[ name ].c_str() );
}

float Options::getOptionFloat( const std::string& name, float defaultVal )
{
	if( instance_.cache_.find( name ) == instance_.cache_.end() )
	{
		char formattedString[512];
		sprintf( formattedString, "%f", defaultVal );
		instance_.cache_[ name ] = instance_.pRoot()->readString( name, formattedString );
	}

	return (float)atof( instance_.cache_[ name ].c_str() );
}

PY_MODULE_STATIC_METHOD( Options, getOptionFloat, BigBang )

PyObject * Options::py_getOptionFloat( PyObject * args )
{
	char * name = NULL;
	float defaultVal;

	if( PyArg_ParseTuple( args, "sf", &name, &defaultVal ) )
	{
		return Py_BuildValue( "f", getOptionFloat( name, defaultVal ) );
	}
	else if( PyArg_ParseTuple( args, "s", &name ) )
	{
		PyErr_Clear();
		return Py_BuildValue( "f", getOptionFloat( name ) );
	}

	PyErr_Clear();
	return Py_BuildValue( "f", 0 );
}

void Options::setOptionVector2( const std::string& name, const Vector2& value )
{
	instance_.pRoot()->writeVector2( name, value );
	instance_.cache_[ name ] = instance_.pRoot()->readString( name );
}

PY_MODULE_STATIC_METHOD( Options, setOptionVector2, BigBang )

PyObject * Options::py_setOptionVector2( PyObject * args )
{
	char * name = NULL;
	PyObject * pValueObject;
	Vector2 value;
	if( PyArg_ParseTuple( args, "sO", &name, &pValueObject ) &&
		Script::setData( pValueObject, value ) == 0 )
	{
		setOptionVector2( name, value );
	}

	Py_Return;
}

Vector2 Options::getOptionVector2( const std::string& name )
{
	if( instance_.cache_.find( name ) == instance_.cache_.end() )
		instance_.cache_[ name ] = instance_.pRoot()->readString( name );

	Vector2 v;
	if( sscanf( instance_.cache_[ name ].c_str(), "%f%f", &v.x, &v.y ) == 2 )
		return v;
	return Vector2( 0.f, 0.f );
}

Vector2 Options::getOptionVector2( const std::string& name, const Vector2& defaultVal )
{
	if( instance_.cache_.find( name ) == instance_.cache_.end() )
	{
		char formattedString[512];
		sprintf( formattedString, "%f %f", defaultVal[ 0 ], defaultVal[ 1 ] );
		instance_.cache_[ name ] = instance_.pRoot()->readString( name, formattedString );
	}

	Vector2 v;
	if( sscanf( instance_.cache_[ name ].c_str(), "%f%f", &v.x, &v.y ) == 2 )
		return v;
	return defaultVal;
}

PY_MODULE_STATIC_METHOD( Options, getOptionVector2, BigBang )

PyObject * Options::py_getOptionVector2( PyObject * args )
{
	char * name = NULL;
	Vector2 value( 0.f, 0.f );
	if( PyArg_ParseTuple( args, "s", &name ) )
	{
		value = getOptionVector2( name );
	}

	PyObject * result = PyTuple_New( 2 );
	PyTuple_SetItem( result, 0, Script::getData( value[0] ) );
	PyTuple_SetItem( result, 1, Script::getData( value[1] ) );
	return result;
}

void Options::setOptionVector3( const std::string& name, const Vector3& value )
{
	instance_.pRoot()->writeVector3( name, value );
	instance_.cache_[ name ] = instance_.pRoot()->readString( name );
}

PY_MODULE_STATIC_METHOD( Options, setOptionVector3, BigBang )

PyObject * Options::py_setOptionVector3( PyObject * args )
{
	char * name = NULL;
	PyObject * pValueObject;
	Vector3 value;
	if( PyArg_ParseTuple( args, "sO", &name, &pValueObject ) &&
		Script::setData( pValueObject, value ) == 0 )
	{
		setOptionVector3( name, value );
	}

	Py_Return;
}

Vector3 Options::getOptionVector3( const std::string& name )
{
	if( instance_.cache_.find( name ) == instance_.cache_.end() )
		instance_.cache_[ name ] = instance_.pRoot()->readString( name );

	Vector3 v;
	if( sscanf( instance_.cache_[ name ].c_str(), "%f%f%f", &v.x, &v.y, &v.z ) == 3 )
		return v;
	return Vector3( 0.f, 0.f, 0.f );
}

Vector3 Options::getOptionVector3( const std::string& name, const Vector3& defaultVal )
{
	if( instance_.cache_.find( name ) == instance_.cache_.end() )
	{
		char formattedString[512];
		sprintf( formattedString, "%f %f %f", defaultVal[ 0 ], defaultVal[ 1 ], defaultVal[ 2 ] );
		instance_.cache_[ name ] = instance_.pRoot()->readString( name, formattedString );
	}

	Vector3 v;
	if( sscanf( instance_.cache_[ name ].c_str(), "%f%f%f", &v.x, &v.y, &v.z ) == 3 )
		return v;
	return defaultVal;
}

PY_MODULE_STATIC_METHOD( Options, getOptionVector3, BigBang )

PyObject * Options::py_getOptionVector3( PyObject * args )
{
	char * name = NULL;
	Vector3 value( 0.f, 0.f, 0.f );
	if( PyArg_ParseTuple( args, "s", &name ) )
	{
		value = getOptionVector3( name );
	}

	PyObject * result = PyTuple_New( 3 );
	PyTuple_SetItem( result, 0, Script::getData( value[0] ) );
	PyTuple_SetItem( result, 1, Script::getData( value[1] ) );
	PyTuple_SetItem( result, 2, Script::getData( value[2] ) );
	return result;
}

void Options::setOptionVector4( const std::string& name, const Vector4& value )
{
	instance_.pRoot()->writeVector4( name, value );
	instance_.cache_[ name ] = instance_.pRoot()->readString( name );
}

PY_MODULE_STATIC_METHOD( Options, setOptionVector4, BigBang )

PyObject * Options::py_setOptionVector4( PyObject * args )
{
	char * name = NULL;
	PyObject * pValueObject;
	Vector4 value;
	if( PyArg_ParseTuple( args, "sO", &name, &pValueObject ) &&
		Script::setData( pValueObject, value ) == 0 )
	{
		setOptionVector4( name, value );
	}

	Py_Return;
}

Vector4 Options::getOptionVector4( const std::string& name )
{
	if( instance_.cache_.find( name ) == instance_.cache_.end() )
		instance_.cache_[ name ] = instance_.pRoot()->readString( name );

	Vector4 v;
	if( sscanf( instance_.cache_[ name ].c_str(), "%f%f%f%f", &v.x, &v.y, &v.z, &v.w ) == 4 )
		return v;
	return Vector4( 0.f, 0.f, 0.f, 0.f );
}

Vector4 Options::getOptionVector4( const std::string& name, const Vector4& defaultVal )
{
	if( instance_.cache_.find( name ) == instance_.cache_.end() )
	{
		char formattedString[512];
		sprintf( formattedString, "%f %f %f %f", defaultVal[ 0 ], defaultVal[ 1 ], defaultVal[ 2 ],
			defaultVal[ 3 ] );
		instance_.cache_[ name ] = instance_.pRoot()->readString( name, formattedString );
	}

	Vector4 v;
	if( sscanf( instance_.cache_[ name ].c_str(), "%f%f%f%f", &v.x, &v.y, &v.z, &v.w ) == 4 )
		return v;
	return defaultVal;
}

PY_MODULE_STATIC_METHOD( Options, getOptionVector4, BigBang )

PyObject * Options::py_getOptionVector4( PyObject * args )
{
	char * name = NULL;
	Vector4 value( 0.f, 0.f, 0.f, 0.f );
	if( PyArg_ParseTuple( args, "s", &name ) )
	{
		value = getOptionVector4( name );
	}

	PyObject * result = PyTuple_New( 4 );
	PyTuple_SetItem( result, 0, Script::getData( value[0] ) );
	PyTuple_SetItem( result, 1, Script::getData( value[1] ) );
	PyTuple_SetItem( result, 2, Script::getData( value[2] ) );
	PyTuple_SetItem( result, 3, Script::getData( value[3] ) );
	return result;
}

PY_MODULE_STATIC_METHOD( Options, setOption, BigBang )

PyObject * Options::py_setOption( PyObject * args )
{
	char * path;
	PyObject * pValueObject;

	if (!PyArg_ParseTuple( args, "sO", &path, &pValueObject ))
	{
		PyErr_SetString( PyExc_TypeError, "Expected string and an object." );

		return NULL;
	}

	if (PyInt_Check( pValueObject ))
	{
		return py_setOptionInt( args );
	}
	else if (PyFloat_Check( pValueObject ))
	{
		return py_setOptionFloat( args );
	}
	else if (PyString_Check( pValueObject ))
	{
		return py_setOptionString( args );
	}
	else if (PyTuple_Check( pValueObject ))
	{
		const int size = PyTuple_Size( pValueObject );

		if (size == 3)
		{
			return py_setOptionVector3( args );
		}
		else if (size == 4)
		{
			return py_setOptionVector4( args );
		}
	}
	else if (PyVector<Vector3>::Check( pValueObject ))
	{
		return py_setOptionVector3( args );
	}
	else if (PyVector<Vector4>::Check( pValueObject ))
	{
		return py_setOptionVector4( args );
	}

	PyErr_Format( PyExc_TypeError, "BigBang.setOption: "
		"Unrecognised object type %s", pValueObject->ob_type->tp_name );

	return NULL;
}

// options.cpp
