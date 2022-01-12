/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef PYWATCHER_HPP
#define PYWATCHER_HPP


#include "cstdmf/config.hpp"

#if ENABLE_WATCHERS

#include "Python.h"

#include <iostream>

#include "cstdmf/watcher.hpp"
#include "py_to_stl.hpp"


// Get a watcher for a PySequence (using PySequenceSTL)
Watcher & PySequence_Watcher();

// Get a watcher for a PyMapping (using PyMappingSTL)
Watcher & PyMapping_Watcher();



/**
 *	This class is used to watch a PyObjectPtrRef
 *
 *	It uses a registry to more specialised watchers to dispatch
 *	the handling to the appropriate watcher.
 *
 *	Note that these watchers all work with references of pointers,
 *	not references of PyObjects themselves. This is so that a
 *	'set' operation could conceivably work ... as many PyObjects
 *	are not mutable. e.g. changing the '1' object to actually be
 *	'2' would have dire consequences for the Python system - every
 *	other '1' would also change to a '2'! In 'set' operations,
 *	DECREF is used on the old object pointer, and a a new one
 *	(always of the same type) is put in its place.
 */
class PyObjectWatcher : public Watcher
{
public:
	/// @name Construction/Destruction
	//@{
	PyObjectWatcher( PyObjectPtrRef & popr );
	//@}

	/// @name Overrides from Watcher
	//@{
	virtual bool getAsString( const void * base, const char * path,
		std::string & result, std::string & desc, Type & type ) const;

	virtual bool setFromString( void * base, const char * path,
		const char * valueStr );

	virtual bool visitChildren( const void * base, const char * path,
		WatcherVisitor & visitor );

	virtual bool addChild( const char * path, Watcher * pChild,
		void * withBase = NULL );
	//@}

private:
	PyObjectPtrRef & popr_;

	static SmartPointer<DataWatcher<PyObjectPtrRef> >	fallback_;

	static Watcher * getSpecialWatcher( PyObject * pObject );

	static const char * tail( const char * path )
	{
		return DirectoryWatcher::tail( path );
	}
};


#endif // ENABLE_WATCHERS

#endif // PYWATCHER_HPP
