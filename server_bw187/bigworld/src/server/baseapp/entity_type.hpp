/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef ENTITY_TYPE_HPP
#define ENTITY_TYPE_HPP

#include "Python.h"

#include "network/basictypes.hpp"
#include "entitydef/entity_description.hpp"
#include "entitydef/method_description.hpp"

class Base;

namespace Mercury
{
	class Nub;
};


class EntityType;
typedef SmartPointer< EntityType > EntityTypePtr;
typedef SmartPointer<Base> BasePtr;

/**
 *	This class is the entity type of a base
 */
class EntityType : public ReferenceCount
{
public:
	EntityType( const EntityDescription & desc, PyTypeObject * pType,
		   bool isProxy );
	~EntityType();

	BasePtr create( ObjectID id, DatabaseID dbID, BinaryIStream & data,
		bool hasPersistentDataOnly, Mercury::Nub * intNub,
		Mercury::Nub * extNub );

	Base * newEntityBase( ObjectID id, DatabaseID dbID,
		Mercury::Nub * intNub, Mercury::Nub * extNub );

	PyObject * createScript( BinaryIStream & data );
	PyObjectPtr createCellDict( BinaryIStream & data,
								bool strmHasPersistentDataOnly );

	const EntityDescription & description() const
											{ return entityDescription_; }

	static EntityTypePtr getType( EntityTypeID typeID );
	static EntityTypePtr getType( const char * className );
	static EntityTypeID nameToIndex( const char * name );

	static bool init( bool isReload = false );
	static bool reloadScript( bool isRecover = false );
	static void migrate( bool isFullReload = true );
	static void cleanupAfterReload( bool isFullReload = true );

	static void clearStatics();

	const char * name() const	{ return entityDescription_.name().c_str(); }

	bool hasBaseScript() const	{ return entityDescription_.hasBaseScript(); }
	bool hasCellScript() const	{ return entityDescription_.hasCellScript(); }

	bool isProxy() const		{ return isProxy_; }

	PyObject * pClass() const		{ return (PyObject*)pClass_; }
	void setClass( PyTypeObject * pClass );

	EntityTypeID id() const	{ return entityDescription_.index(); }

	EntityTypePtr old() const	{ return pOldSelf_; }
	void old( EntityTypePtr pOldType );

private:
	EntityDescription entityDescription_;
	PyTypeObject *	pClass_;

	EntityTypePtr	pOldSelf_;

	bool			isProxy_;

	// static stuff
	static PyObject * s_pInitModules_, * s_pNewModules_;

	typedef std::vector< EntityTypePtr > EntityTypes;
	static EntityTypes s_curTypes_, s_newTypes_;

	static StringMap< EntityTypeID > s_nameToIndexMap_;
};

#endif // ENTITY_TYPE_HPP
