#ifndef SERVER_CELLEXTRA_PY_ARRAY_OBJ_PROXY
#define SERVER_CELLEXTRA_PY_ARRAY_OBJ_PROXY

#include "Python.h"


class PyArrayProxy
{
public:
	PyArrayProxy( PyObject * );
	~PyArrayProxy();

public:
	Py_ssize_t length( void );
	PyObject * item( Py_ssize_t index );

	PyObject * pop( Py_ssize_t index );
	int clear();

private:
	PyObject * pPyArray_;
};

#endif //SERVER_CELLEXTRA_PY_ARRAY_OBJ_PROXY
