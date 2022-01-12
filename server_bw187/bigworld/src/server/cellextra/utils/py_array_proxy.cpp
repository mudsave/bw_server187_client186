/**
* 由于引擎的PyArrayDataInstance只在cpp文件中定义，
* 在其他地方无法获得此种类类型，导致进行C++优化时，
* 遇到这种类型就无法方便地进行操作，所以提供这个类
* 类型的代理，封装操作PyArrayDataInstance类实例的
* Python对象的行为。
*/

#include "cstdmf/debug.hpp"
#include "py_array_proxy.hpp"
#include "pyscript/script.hpp"


/**
* Default constructor
*/
PyArrayProxy::PyArrayProxy( PyObject * pPyArray )
{
	pPyArray_ = pPyArray;
	Py_XINCREF( pPyArray_ );
}

/**
* Destructor
*/
PyArrayProxy::~PyArrayProxy()
{
	Py_XDECREF( pPyArray_ );
	pPyArray_ = NULL;
}

/**
* Length of array
*/
Py_ssize_t PyArrayProxy::length()
{
	PyObject * pLen = PyObject_CallMethod( pPyArray_, "__len__", NULL );
	Py_ssize_t len = PyInt_AsLong( pLen );
	Py_XDECREF( pLen );
	return len;
}


/**
* Get array item by index, return a new reference object,
* remember to decrement its reference after used.
*/
PyObject * PyArrayProxy::item( Py_ssize_t index )
{
	return PyObject_CallMethod( pPyArray_, "__getitem__", "i", index );
}


/**
* Pop array item by index, return a new reference object,
* remember to decrement its reference after used.
*/
PyObject * PyArrayProxy::pop( Py_ssize_t index )
{
	return PyObject_CallMethod( pPyArray_, "pop", "i", index );
}


/**
* Clear elements in array.
*/
int PyArrayProxy::clear()
{
	Py_ssize_t length = this->length();
	if( length > 0 )
	{
		PyObjectPtr pTuple( PyTuple_New( 0 ), PyObjectPtr::STEAL_REFERENCE );
		PyObject * pResult = PyObject_CallMethod( pPyArray_, "__setslice__",
												"iiO", 0, length, &*pTuple );
		Py_XDECREF( pResult );
		return 0;
	}
	else
	{
		return 0;
	}
}
