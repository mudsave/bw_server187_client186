#include "apex_game_proxy.h"
#include "Python.h"
#include "pyscript/pyobject_plus.hpp"
#include "pyscript/script.hpp"
#include <mysql/mysql.h>
#include <mysql/mysqld_error.h>

PyObject* escape_string(std::string message)
{
    PyObject *res;
    char *buf = (char *)malloc(message.size() * 2 + 1); 

    if (buf == NULL)
        return PyErr_NoMemory();

    mysql_escape_string(buf, message.c_str(), message.size());
    res = PyString_FromString(buf);

    free(buf); 

    return res;
}

PY_AUTO_MODULE_FUNCTION(RETOWN, escape_string, ARG(std::string, END), BigWorld)
// execute_escape_script.cpp
