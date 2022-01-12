/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef MAILBOX_HPP
#define MAILBOX_HPP


#include "entitydef/entity_description.hpp"
#include "entitydef/mailbox_base.hpp"
#include "network/channel.hpp"
#include "pyscript/pyobject_plus.hpp"
#include "pyscript/script.hpp"
#include "server/backup_hash.hpp"
#include "entity_type.hpp"

class EntityType;


/**
 *	This class implements a mailbox that can send to a server object. This
 *	object may be on a cell or may be a base.
 *
 *	@see CellEntityMailBox
 *	@see BaseEntityMailBox
 */
class ServerEntityMailBox: public PyEntityMailBox
{
	Py_Header( ServerEntityMailBox, PyEntityMailBox )

public:
	ServerEntityMailBox( EntityTypePtr pBaseType,
			const Mercury::Address & addr, ObjectID id,
			PyTypePlus * pType = &s_type_ );
	~ServerEntityMailBox();

	virtual PyObject * pyGetAttribute( const char * attr );
	void sendStream();

	const Mercury::Address		address() const		{ return addr_; }
	void address( const Mercury::Address & addr )	{ addr_ = addr; }

	ObjectID					id() const			{ return id_; }

	PY_RO_ATTRIBUTE_DECLARE( this->componentName(), component );
	PY_RO_ATTRIBUTE_DECLARE( pLocalType_->name(), className );
	PY_RO_ATTRIBUTE_DECLARE( id_, id );

	EntityMailBoxRef ref() const;
	virtual EntityMailBoxRef::Component component() const = 0;
	const char * componentName() const;

	static EntityMailBoxRef static_ref( PyObject * pThis )
		{ return ((const ServerEntityMailBox*)pThis)->ref(); }

	static void migrate();
	static void adjustForDeadBaseApp( const Mercury::Address & deadAddr,
			const BackupHash & backupHash );

protected:
	virtual Mercury::Channel & channel() const;
	Mercury::Bundle & bundle() const { return this->channel().bundle(); }

	Mercury::Address			addr_;
	ObjectID					id_;

	EntityTypePtr	pLocalType_;

private:
	ServerEntityMailBox * pPrev_;
	ServerEntityMailBox * pNext_;
	static ServerEntityMailBox * s_pHead_;
	static ServerEntityMailBox * s_pTail_;
};

PY_SCRIPT_CONVERTERS_DECLARE( ServerEntityMailBox )


/**
 *	This class implements a mailbox that can send to an object on a cell.
 */
class CellEntityMailBox: public ServerEntityMailBox
{
	Py_Header( CellEntityMailBox, ServerEntityMailBox )

public:
	CellEntityMailBox( EntityTypePtr pBaseType,
			const Mercury::Address & addr, ObjectID id,
			PyTypePlus * pType = &s_type_ ) :
		ServerEntityMailBox( pBaseType, addr, id, pType )
	{}

	virtual PyObject * pyGetAttribute( const char * attr );

	virtual BinaryOStream * getStream( const MethodDescription & methodDesc );
	virtual const MethodDescription * findMethod( const char * attr ) const;
	virtual EntityMailBoxRef::Component component() const;
};

PY_SCRIPT_CONVERTERS_DECLARE( CellEntityMailBox )

typedef SmartPointer<CellEntityMailBox> CellEntityMailBoxPtr;


/**
 *	This class is common to all mailboxes that send to the base entity or via
 *	the base entity.
 */
class CommonBaseEntityMailBox : public ServerEntityMailBox
{
	Py_Header( CommonBaseEntityMailBox, ServerEntityMailBox )

public:
	CommonBaseEntityMailBox( EntityTypePtr pBaseType,
			const Mercury::Address & addr, ObjectID id,
			PyTypePlus * pType = &s_type_ ) :
		ServerEntityMailBox( pBaseType, addr, id, pType )
	{}

protected:
	Mercury::Bundle & bundle() const;

private:
	virtual Mercury::Channel & channel() const;
	Mercury::Channel & channel( Entity * pEntity ) const;
};


/**
 *	This class implements a mailbox that can send to a base object.
 */
class BaseEntityMailBox: public CommonBaseEntityMailBox
{
	Py_Header( BaseEntityMailBox, CommonBaseEntityMailBox )

public:
	BaseEntityMailBox( EntityTypePtr pBaseType,
			const Mercury::Address & addr, ObjectID id,
			PyTypePlus * pType = &s_type_ ) :
		CommonBaseEntityMailBox( pBaseType, addr, id, pType )
	{}

	virtual PyObject * pyGetAttribute( const char * attr );

	virtual BinaryOStream * getStream( const MethodDescription & methodDesc );
	virtual const MethodDescription * findMethod( const char * attr ) const;
	virtual EntityMailBoxRef::Component component() const;
};

PY_SCRIPT_CONVERTERS_DECLARE( BaseEntityMailBox )






#endif // MAILBOX_HPP
