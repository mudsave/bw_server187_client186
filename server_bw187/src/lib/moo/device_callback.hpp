/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef DEVICE_CALLBACK_HPP
#define DEVICE_CALLBACK_HPP

#include <iostream>
#include <list>

namespace Moo
{

class DeviceCallback
{
public:
	typedef std::list< DeviceCallback* > CallbackList;

	DeviceCallback();
	~DeviceCallback();

	virtual void deleteUnmanagedObjects( );
	virtual void createUnmanagedObjects( );
	virtual void deleteManagedObjects( );
	virtual void createManagedObjects( );

	static void deleteAllUnmanaged( );
	static void createAllUnmanaged( );
	static void deleteAllManaged( );
	static void createAllManaged( );
	
private:
/*	DeviceCallback(const DeviceCallback&);
	DeviceCallback& operator=(const DeviceCallback&);*/

	static CallbackList* callbacks_;
	static CallbackList* deletedList_;

	friend std::ostream& operator<<(std::ostream&, const DeviceCallback&);
};

class GenericUnmanagedCallback : public DeviceCallback
{
public:

	typedef void Function( );

	GenericUnmanagedCallback( Function* createFunction, Function* destructFunction  );
	~GenericUnmanagedCallback();

	void deleteUnmanagedObjects( );
	void createUnmanagedObjects( );
	
private:

	Function* createFunction_;
	Function* destructFunction_;

};

};

#ifdef CODE_INLINE
#include "device_callback.ipp"
#endif




#endif // DEVICE_CALLBACK_HPP
