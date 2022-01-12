/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef BASE_HPP
#define BASE_HPP

#include "Python.h"

#include "baseapp_int_interface.hpp"
#include "entity_type.hpp"
#include "mailbox.hpp"

#include "cstdmf/time_queue.hpp"

#include "entitydef/entity_description.hpp"

#include "network/channel.hpp"
#include "network/mercury.hpp"

#include "pyscript/pyobject_plus.hpp"
#include "pyscript/script.hpp"

#include "server/auto_backup_and_archive.hpp"

class Timers;

class Base;
typedef SmartPointer<PyObject> PyObjectPtr;

class WriteToDBReplyHandler
{
public:
	virtual ~WriteToDBReplyHandler() {};
	virtual void onWriteToDBComplete( bool succeeded ) = 0;
};

class WriteToDBReplyStruct
{
public:
	PyObjectPtr pScriptHandler_;
	WriteToDBReplyHandler * pCPPHandler_;
};


/*~ class BigWorld.Base
 *	@components{ base }
 *	The class Base represents an entity residing on a base application. Base
 *	entities can be created via the BigWorld.createBase function (and functions
 *	with createBase prefix). An base entity instance can also be created from a
 *	remote cellApp BigWorld.createEntityOnBaseApp function call.
 *
 *	A base entity can be linked to an entity within one of the active cells,
 *	and can be used to create an associated entity on an appropriate cell. The
 *	functionality of this class allows you to create and destroy the entity on
 *	existing cells, register a timer callback function to be called on the base
 *	entity, access contact information for this object,	and also access a
 *	CellEntityMailBox through which the base entity can	communicate with its cell
 *	entity (the associated cell entity can move to different cells as a result
 *	of movement of the cell entity, or load balancing).
 */

/**
 *	Instances of this class are used to represent a generic base.
 */
class Base: public PyInstancePlus
{
	Py_InstanceHeader( Base )

public:
	Base( ObjectID id, DatabaseID dbID, EntityTypePtr pType );
	~Base();
	bool init( PyObject * pDict, PyObject * pCellArgs,
				bool isRestore = false );

	PyObject * dictFromStream( BinaryIStream & data ) const;

	ObjectID id() const								{ return id_; }
	EntityMailBoxRef baseEntityMailBoxRef() const;

	Mercury::Channel& channel() { return *pChannel_; }

	const Mercury::Address & cellAddr() const	{ return pChannel_->addr(); }

	void databaseID( DatabaseID );
	DatabaseID databaseID()							{ return databaseID_; }

	CellEntityMailBox * pCellEntityMailBox()		{ return pCellEntityMailBox_; }
	SpaceID spaceID() const							{ return spaceID_; }

	void destroy( bool deleteFromDB, bool writeToDB, bool logOffFromDB = true );
	bool writeToDB( int8 flags,
			WriteToDBReplyStruct replyStruct, PyObjectPtr pCellData = NULL );
	bool writeToDB( int8 flags, WriteToDBReplyHandler * pHandler,
			PyObjectPtr pCellData = NULL );
	bool writeToDB( int8 flags, PyObjectPtr pHandler = NULL,
			PyObjectPtr pCellData = NULL );
	void backup( BinaryOStream & stream, bool isNewStyle );
	void restore( BinaryIStream & stream, bool isNewStyle );

	bool hasCellEntity() const;
	bool shouldSendToCell() const;

	bool isCreateCellPending() const	{ return isCreateCellPending_; }
	bool isGetCellPending() const		{ return isGetCellPending_; }
	bool isDestroyCellPending() const 	{ return isDestroyCellPending_; }

	void reloadScript();
	bool migrate();
	void migratedAll();

	// virtual methods
	bool isProxy() const							{ return isProxy_; }

	bool isDestroyed() const						{ return isDestroyed_; }

	// tell the base that its cell entity is in a new cell
	void setCurrentCell( SpaceID spaceID,
		const Mercury::Address & cellAppAddr,
		const Mercury::Address * pSrcAddr = NULL,
		bool shouldReset = false );

	void currentCell( const BaseAppIntInterface::currentCellArgs & args,
		   const Mercury::Address & srcAddr );

	void emergencySetCurrentCell( const Mercury::Address & srcAddr,
		const Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data );

	void backupCellEntity( BinaryIStream & data );

	void writeToDB( BinaryIStream & data );

	void cellEntityLost( const Mercury::Address & srcAddr,
		   const Mercury::UnpackedMessageHeader & header,
		   BinaryIStream & data );

	void callBaseMethod( const Mercury::Address & srcAddr,
		   const Mercury::UnpackedMessageHeader & header,
		   BinaryIStream & data );

	void callCellMethod( BinaryIStream & data );

	// Script related methods
	PyObject * pyGetAttribute( const char * attr );
	int pySetAttribute( const char * attr, PyObject * value );

	PyObject * createCellEntity( CellEntityMailBoxPtr cell );
	PY_AUTO_METHOD_DECLARE( RETOWN, createCellEntity,
		OPTARG( CellEntityMailBoxPtr, NULL, END ) )

	PyObject * createInDefaultSpace();
	PY_AUTO_METHOD_DECLARE( RETOWN, createInDefaultSpace, END )

	PyObject * createInNewSpace();
	PY_AUTO_METHOD_DECLARE( RETOWN, createInNewSpace, END )

	PyObject * createInSpace( SpaceID spaceID, const char * pyErrorPrefix );

	void cellCreationResult( bool success );

	bool restoreTo( SpaceID spaceID, const Mercury::Address & cellAppAddr );

	Mercury::Bundle & cellBundle()	{ return pChannel_->bundle(); }
	void sendToCell();

	PY_KEYWORD_METHOD_DECLARE( py_destroy )
	PY_METHOD_DECLARE( py_destroyCellEntity )
	PY_METHOD_DECLARE( py_writeToDB )
	PY_METHOD_DECLARE( py_addTimer )
	PY_METHOD_DECLARE( py_delTimer )

	PY_METHOD_DECLARE( py_registerGlobally )
	PY_METHOD_DECLARE( py_deregisterGlobally )

	PY_PICKLING_METHOD_DECLARE( MailBox )

	PY_RO_ATTRIBUTE_DECLARE( (int)id_, id )
	PY_RO_ATTRIBUTE_DECLARE( isDestroyed_, isDestroyed )

	PY_RO_ATTRIBUTE_DECLARE( (int)pType_->description().index(), baseType )

	PyObject * pyGet_cell();
	PY_RO_ATTRIBUTE_SET( cell )

	PyObject * pyGet_contactDetails();
	PY_RO_ATTRIBUTE_SET( contactDetails )

	PyObject * pyGet_databaseID();
	PY_RO_ATTRIBUTE_SET( databaseID )
	
	AutoBackupAndArchive::Policy shouldAutoBackup() const 
		{ return shouldAutoBackup_; }

	void shouldAutoBackup( AutoBackupAndArchive::Policy value )
		{ shouldAutoBackup_  = value; }

	AutoBackupAndArchive::Policy shouldAutoArchive() const 
		{ return shouldAutoArchive_; }

	void shouldAutoArchive( AutoBackupAndArchive::Policy value )
		{ shouldAutoArchive_  = value; }

	PY_RW_ATTRIBUTE_DECLARE( shouldAutoBackup_, shouldAutoBackup )
	PY_RW_ATTRIBUTE_DECLARE( shouldAutoArchive_, shouldAutoArchive )

	EntityTypePtr pType() const								{ return pType_; }

	// Static methods
	// static Watcher & watcher();
	static bool init();

protected:
	void onDestroy();
	void createCellData( BinaryIStream & data );

private:
	bool addToStream( int8 flags, BinaryOStream & stream,
			PyObjectPtr pCellData );

	std::auto_ptr< Mercury::ReplyMessageHandler >
		prepareForCellCreate( const char * errorPrefix );
	bool addCellCreationData( Mercury::Bundle & bundle,
		const char * errorPrefix );
	bool checkAssociatedCellEntity( bool havingEntityGood,
		const char * errorPrefix = NULL );

protected:
	Mercury::Channel *	pChannel_;

	ObjectID			id_;
	DatabaseID			databaseID_;
	EntityTypePtr		pType_;

	CellEntityMailBox *	pCellEntityMailBox_;
	SpaceID				spaceID_;

	bool				isProxy_;
	bool				isDestroyed_;

	/// True if a 'create' request has been sent to the CellApp but no reply has
	/// yet been received.
	bool				isCreateCellPending_;

	/// True if a 'create' request has been sent but setCurrentCell has yet to
	/// be called.
	bool				isGetCellPending_;

	/// True if a 'destroy' request has been sent but cellEntityLost has yet to
	/// be called.
	bool				isDestroyCellPending_;

	class TimerHandler : public TimeQueueHandler
	{
	public:
		TimerHandler( Base & base ) : base_( base ) {}

	private:
		// Overrides
		virtual void handleTimeout( TimeQueueId id, void * pUser );
		virtual void onRelease( TimeQueueId id, void  * pUser );

		Base & base_;
	};

	void handleTimeout( TimeQueueId id, void * pUser );
	void onTimerReleased( TimeQueueId id );
	friend class TimerHandler;

	TimerHandler timerHandler_;

	Timers * pTimers_;

	std::string		cellBackupData_;

	AutoBackupAndArchive::Policy shouldAutoBackup_;
	AutoBackupAndArchive::Policy shouldAutoArchive_;

	friend class EntityType;
};

typedef SmartPointer<Base> BasePtr;


#endif // BASE_HPP
