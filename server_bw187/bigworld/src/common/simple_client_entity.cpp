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

#include "Python.h"

#include "cstdmf/debug.hpp"

#include "simple_client_entity.hpp"

DECLARE_DEBUG_COMPONENT2( "Connect", 0 )


namespace SimpleClientEntity
{

/**
 *	Helper class to cast entity as a property owner
 */
class EntityPropertyOwner : public PropertyOwnerBase
{
public:
	EntityPropertyOwner( PyObjectPtr e, const EntityDescription & edesc ) :
		e_( e ), edesc_( edesc ) { }

	// called going to the root of the tree
	virtual void propertyChanged( PyObjectPtr val, const DataType & type,
		ChangePath path )
	{
		// unimplemented
	}

	// called going to the leaves of the tree
	virtual int propertyDivisions()
	{
		return edesc_.clientServerPropertyCount();
	}

	virtual PropertyOwnerBase * propertyVassal( int ref )
	{
		DataDescription * pDD = edesc_.clientServerProperty( ref );
		PyObjectPtr pPyObj(
			PyObject_GetAttrString( &*e_, (char*)pDD->name().c_str() ),
			PyObjectPtr::STEAL_REFERENCE );
		if (!pPyObj)
		{
			PyErr_Clear();
			return NULL;
		}

		return pDD->dataType()->asOwner( &*pPyObj );
	}

	virtual PyObjectPtr propertyRenovate( int ref, BinaryIStream & data,
		PyObjectPtr & pValue, DataType *& pType )
	{
		DataDescription * pDD = edesc_.clientServerProperty( ref );
		if (pDD == NULL) return NULL;

		PyObjectPtr pNewObj = pDD->createFromStream( data, false );
		if (!pNewObj)
		{
			ERROR_MSG( "Entity::handleProperty: "
				"Error streaming off new property value\n" );
			return NULL;
		}

		// ok looking good then
		pType = pDD->dataType();
		pValue = pNewObj;

		PyObjectPtr pOldObj(
			PyObject_GetAttrString( &*e_, (char*)pDD->name().c_str() ),
			PyObjectPtr::STEAL_REFERENCE );
		if (!pOldObj)
		{
			PyErr_Clear();
			pOldObj = Py_None;
		}

		int err = PyObject_SetAttrString(
			&*e_, (char*)pDD->name().c_str(), &*pNewObj );
		if (err != 0)
		{
			ERROR_MSG( "Entity::handleProperty: "
				"Failed to set new property into Entity\n" );
			PyErr_PrintEx(0);
		}

		return pOldObj;
	}

private:
	PyObjectPtr e_;
	const EntityDescription & edesc_;
};


/**
 *	Update the identified property on the given entity. Returns true if
 *	the property was found to update.
 */
bool propertyEvent( PyObjectPtr pEntity, const EntityDescription & edesc,
	int messageID, BinaryIStream & data, bool callSetForTopLevel )
{
	EntityPropertyOwner king( pEntity, edesc );

	// find what (sub-)property this message is for
	PropertyOwnerBase::ChangePath path;
	PropertyOwnerBase * pOwner = king.getPathFromStream(
		messageID, data, path );

	if (pOwner == NULL)
	{
		ERROR_MSG( "SimpleClientEntity::propertyEvent: "
			"No property starting with message id %d\n", messageID );
		return false;
	}

	// set the property to its new value
	PyObjectPtr pNewValue;
	DataType * pType;
	PyObjectPtr pOldValue =
		pOwner->propertyRenovate( path[0], data, pNewValue, pType );
	MF_ASSERT( pOldValue );

	// if this was a top-level property then call the set handler for it
	// TODO: Restore functionality prior to hierarchical property.
    // need to improve to inform script which element in a complex
	// property has been updated.
	if (callSetForTopLevel)
	{
		DataDescription * pDataDescription =
			edesc.clientServerProperty( path[path.size()-1] );
		MF_ASSERT( pDataDescription != NULL );

		std::string methodName = "set_" + pDataDescription->name();
		Script::call(
			PyObject_GetAttrString( &*pEntity, (char*)methodName.c_str() ),
			Py_BuildValue( "(O)", &*pOldValue ),
			"Entity::propertyEvent: ",
			/*okIfFunctionNull:*/true );
	}

	return true;
}

/**
 *	Call the identified method on the given entity. Returns true if the
 *	method description was found.
 */
bool methodEvent( PyObjectPtr pEntity, const EntityDescription & edesc,
	int messageID, BinaryIStream & data )
{
	MethodDescription * pMethodDescription =
		edesc.clientMethod( messageID, data );

	if (pMethodDescription == NULL)
	{
		ERROR_MSG( "SimpleClientEntity::methodEvent: "
			"No method starting with message id %d\n", messageID );
		return false;
	}

	pMethodDescription->callMethod( &*pEntity, data );
	return true;
}

} // namespace SimpleClientEntity

// simple_client_entity.cpp
