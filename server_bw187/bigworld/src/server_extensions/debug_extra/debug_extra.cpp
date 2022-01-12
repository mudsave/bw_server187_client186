#include "debug_extra.hpp"

DECLARE_DEBUG_COMPONENT( 0 )

PY_TYPEOBJECT( DebugExtra )

PY_BEGIN_METHODS( DebugExtra )
	PY_METHOD( generateCrashDump )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( DebugExtra )
PY_END_ATTRIBUTES()

const DebugExtra::Instance< DebugExtra >
		DebugExtra::instance( &DebugExtra::s_attributes_.di_ );

bool DebugExtra::enableCrashDump = false;

DebugExtra::DebugExtra( Entity& e ) : EntityExtra( e )
{
	static bool firstTime = true;
	if ( firstTime )
	{
		MF_WATCH( "debugging/enableCrashDump", enableCrashDump, Watcher::WT_READ_WRITE,
			"When enabled, open generateCrashDump()." );
		firstTime = false;
	}
}

DebugExtra::~DebugExtra()
{
}

PyObject * DebugExtra::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();
	return this->EntityExtra::pyGetAttribute( attr );
}

int DebugExtra::pySetAttribute( const char * attr, PyObject * value )
{
	PY_SETATTR_STD();
	return this->EntityExtra::pySetAttribute( attr, value );
}

static pid_t create_dump()
{
	pid_t pid = fork();
	if ( pid == 0 )
	{
		// crash the child process.
		(*((unsigned int*)0xf0005678) = 0x1234);
	}
	return pid;
}

void DebugExtra::generateCrashDump( const std::string& reason )
{
	DEBUG_MSG( "DebugExtra::generateCrashDump entityID %d reason '%s'.\n", entity().id(), reason.c_str() );

	if ( !enableCrashDump )
		return;

	pid_t pid = create_dump();
	DEBUG_MSG( "DebugExtra::generateCrashDump entityID %d child process %d.\n", entity().id(), pid );
}

// debug_extra.cpp
