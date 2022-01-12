#include "apex_game_proxy.h"
#include "Python.h"
#include "pyscript/pyobject_plus.hpp"
#include "pyscript/script.hpp"

PyObject* getApexProxy( )
{
    INFO_MSG("PyAPexClient::getApexClient() ....\n");

    PyAPexProxy* pApexProxy = GetUniqueAPexProxyInstance();
	Py_INCREF(pApexProxy);
	return pApexProxy;
}
PY_AUTO_MODULE_FUNCTION( RETOWN, getApexProxy,END, BigWorld )

// execute_apex_script.cpp
