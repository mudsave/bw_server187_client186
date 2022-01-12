#ifndef SERVER_CELLEXTRA_AIINTERFAC_HPP
#define SERVER_CELLEXTRA_AIINTERFAC_HPP

#include "cellapp/entity_extra.hpp"

#undef PY_METHOD_ATTRIBUTE
#define PY_METHOD_ATTRIBUTE PY_METHOD_ATTRIBUTE_ENTITY_EXTRA

class AIInterface : public EntityExtra
{
	Py_EntityExtraHeader( AIInterface );

public:
	AIInterface( Entity &e );
	~AIInterface();

	PyObject *pyGetAttribute( const char *attr );
	int pySetAttribute( const char *attr, PyObject *value );

	PY_AUTO_METHOD_DECLARE( RETVOID, onFightAIHeartbeat_AIInterface_cpp, END );
	void onFightAIHeartbeat_AIInterface_cpp();

	PY_AUTO_METHOD_DECLARE( RETDATA, aiCommonCheck_AIInterface_cpp, ARG( PyObject*, END ) );
	bool aiCommonCheck_AIInterface_cpp( PyObject* );

	static const Instance<AIInterface> instance;
};

#undef PY_METHOD_ATTRIBUTE
#define PY_METHOD_ATTRIBUTE PY_METHOD_ATTRIBUTE_BASE

#endif //AIINTERFACEEXTRA_HPP

