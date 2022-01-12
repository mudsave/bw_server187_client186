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
#include "py_output_writer.hpp"

DECLARE_DEBUG_COMPONENT2( "Script", 0 )

#ifndef CODE_INLINE
#include "py_output_writer.ipp"
#endif

static FILE *			s_file = NULL;
static int				s_fileRefCount = 0;
/*static*/ std::ostream*	PyOutputWriter::s_outFile_ = NULL;

// -----------------------------------------------------------------------------
// Section: PyOutputWriter
// -----------------------------------------------------------------------------

PY_TYPEOBJECT( PyOutputWriter )

/*~ function PyOutputWriter write
 *  Write a string to this writer's outputs. The Python io system calls this.
 *  @param string The string to write.
 *  @return None
 */
PY_BEGIN_METHODS( PyOutputWriter )
	PY_METHOD( write )
PY_END_METHODS()

/*~ attribute PyOutputWriter softspace
 *  This is required for use by the Python io system so
 *  that instances of PyOutputWriter can be used as streams.
 *  @type Read-Write String
 */
PY_BEGIN_ATTRIBUTES( PyOutputWriter )
	PY_ATTRIBUTE( softspace )
PY_END_ATTRIBUTES()



/**
 *	Constructor
 */
PyOutputWriter::PyOutputWriter( const char * fileText,
		bool shouldWritePythonLog,
		PyTypePlus * pType ) :
	PyObjectPlus( pType ),
	softspace_( false ),
	shouldWritePythonLog_( shouldWritePythonLog )
{
	if (shouldWritePythonLog_)
	{
		if (s_fileRefCount == 0)
		{
			if (s_outFile_ == NULL) // If a log file has not been specified then create one
			{
				const char * PYTHON_LOG = "python.log";

				s_file = fopen( PYTHON_LOG, "a" );

				if (s_file != NULL)
				{
					fprintf( s_file,
						"\n/------------------------------------------------------------------------------\\\n" );

					if (fileText)
					{
						fprintf( s_file, fileText );
					}
				}
				else
				{
					ERROR_MSG( "PyOutputWriter::PyOutputWriter: Could not open '%s'\n",
						PYTHON_LOG );
				}
			}
		}

		s_fileRefCount++;
	}
}


/**
 *	Destructor
 */
PyOutputWriter::~PyOutputWriter()
{
	if ((shouldWritePythonLog_) && (--s_fileRefCount == 0))
	{
		fini();
	}
}

/*static*/ void PyOutputWriter::fini()
{
	flush();
	if ((s_outFile_ == NULL) && (s_file != NULL))
	{
		fprintf( s_file, "\\--------------------------------------------------------------------------------/\n" );
		fclose( s_file );
		s_file = NULL;
		s_fileRefCount = 0;
	}
}

/**
 *	This static method is used to flush the log file.
 */
/*static*/ void PyOutputWriter::flush()
{
	if (s_outFile_ != NULL)
	{
		s_outFile_->flush();
	}
	else if (s_file != NULL)
	{
		fflush( s_file );
	}
}


/**
 *	This method returns the attributes associated with this object.
 */
PyObject * PyOutputWriter::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();

	return PyObjectPlus::pyGetAttribute( attr );
}


/**
 *	This method sets the attributes associated with this object.
 */
int PyOutputWriter::pySetAttribute( const char * attr, PyObject * value )
{
	PY_SETATTR_STD();

	return PyObjectPlus::pySetAttribute( attr, value );
}


/**
 *	This method implements the Python write method. It is used to redirect the
 *	write calls to this object's printMessage method.
 */
PyObject * PyOutputWriter::py_write( PyObject * args )
{
	char * msg;
	Py_ssize_t msglen;

	if (!PyArg_ParseTuple( args, "s#", &msg, &msglen ))
	{
		ERROR_MSG( "PyOutputWriter::py_write: Bad args\n" );
		return NULL;
	}

	// fix null chars in msg as we're going to output using C functions
	for( int i = 0; i < msglen; ++i )
		if( msg[i] == 0 )
			msg[i] = ' ';

	this->printMessage( msg );

	if (s_outFile_ != NULL)
	{
		for (unsigned i=0; i<strlen( msg ); i++)
		{
			if (msg[i] == '\n') msg[i] = ' ';
		}

		*s_outFile_ << "SCRIPT: " << msg << std::endl;
	}
	else if (s_file != NULL)
	{
		fwrite( msg, 1, strlen(msg), s_file );
	}

	Py_Return;
}


/**
 *	This method implements the default behaviour for printing a message. Derived
 *	class should override this to change the behaviour.
 */
void PyOutputWriter::printMessage( const char * msg )
{
	int len = strlen( msg );

	if (msg[ len - 1 ] == '\n')
	{
		// This is done so that the hack to prefix the time in cell and the base
		// applications works.
		msg_.append( msg, len - 1 );
		SCRIPT_MSG( "%s\n", msg_.c_str() );
		msg_ = "";
	}
	else
	{
		msg_ += msg;
	}
}


/**
 *	This static method overrides the stdout and stderr members of the sys module
 *	with a new PyOutputWriter.
 */
bool PyOutputWriter::overrideSysMembers( bool shouldWritePythonLog )
{
	PyObject * pSysModule = PyImport_ImportModule( "sys" );

	if (pSysModule != NULL)
	{
		PyObject * pOutputWriter =
				new PyOutputWriter( "", shouldWritePythonLog );
		PyObject_SetAttrString( pSysModule, "stdout", pOutputWriter );
		PyObject_SetAttrString( pSysModule, "stderr", pOutputWriter );
		Py_DECREF( pOutputWriter );

		Py_DECREF( pSysModule );
	}

	return true;
}


// -----------------------------------------------------------------------------
// Section: PyInputSubstituter
// -----------------------------------------------------------------------------

/**
 *	Static method that performs dollar substitution on this line
 */
std::string PyInputSubstituter::substitute( const std::string & line,
	char * moduleName, char * macroDictName )
{
	if (line.empty()) return line;

	std::string	ret;

	PyObject * pMacros = PyObject_GetAttrString(
		PyImport_AddModule( moduleName ), macroDictName );
	if (pMacros == NULL)
	{
		PyErr_Clear();
		return line;
	}

	uint32	nextpos = 0,	lastpos = 0;

	while( (nextpos = line.find( '$', lastpos )) < line.length()-1)
	{
		ret.append( line, lastpos, nextpos-lastpos );

		char	macroName[2] = { line[ nextpos+1 ], 0 };

		PyObject * pExpansion = PyDict_GetItemString( pMacros, macroName );
		// returns a borrowed reference, and _doesn't_ set an exception.
		if (pExpansion != NULL && PyString_Check( pExpansion ))
		{
			ret.append( PyString_AsString( pExpansion ) );
		}

		lastpos = nextpos + 2;
	}

	ret.append( line, lastpos, line.length() );

	Py_DECREF( pMacros );

	return ret;
}

// py_output_writer.cpp
