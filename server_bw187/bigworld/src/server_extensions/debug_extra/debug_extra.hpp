#ifndef DEBUG_EXTRA_HPP
#define DEBUG_EXTRA_HPP

#include "../server/cellapp/entity_extra.hpp"

#undef PY_METHOD_ATTRIBUTE
#define PY_METHOD_ATTRIBUTE PY_METHOD_ATTRIBUTE_ENTITY_EXTRA

class DebugExtra : public EntityExtra
{
	Py_EntityExtraHeader( DebugExtra );

public:
	DebugExtra( Entity& e );
	~DebugExtra();

	PyObject * pyGetAttribute( const char * attr );
	int pySetAttribute( const char * attr, PyObject * value );

	PY_AUTO_METHOD_DECLARE( RETVOID, generateCrashDump, ARG( std::string, END ) );
	void generateCrashDump( const std::string& reason );

	static const Instance<DebugExtra> instance;
	static bool enableCrashDump;
};

#undef PY_METHOD_ATTRIBUTE
#define PY_METHOD_ATTRIBUTE PY_METHOD_ATTRIBUTE_BASE

#endif // DEBUG_EXTRA_HPP
