/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef DATABASE_HPP
#define DATABASE_HPP

#include "cstdmf/md5.hpp"
#include "network/channel.hpp"
#include "entitydef/entity_description_map.hpp"
#include "resmgr/datasection.hpp"
#include "common/doc_watcher.hpp"
#include "server/backup_hash.hpp"

#include "db_interface.hpp"
#include "idatabase.hpp"
#include "worker_thread.hpp"

#ifdef USE_ORACLE
#include "oracle_database.hpp"
#endif

#include <map>
#include <set>

class RelogonAttemptHandler;
class DbUpdater;
class BWResource;

typedef Mercury::ChannelOwner BaseAppMgr;

const EntityTypeID INVALID_TYPEID = std::numeric_limits<uint16>::max();

/**
 *	This class represents the entity definitions in DbMgr.
 */
class EntityDefs
{
	EntityDescriptionMap	entityDescriptionMap_;
	std::vector<bool>		entityPasswordBits_;
	MD5::Digest				md5Digest_;
	EntityTypeID			defaultTypeID_;
	std::vector< std::string >	nameProperties_;
	std::string				defaultNameProperty_;

public:
	EntityDefs() :
		entityDescriptionMap_(), entityPasswordBits_(), md5Digest_(),
		defaultTypeID_( INVALID_ENTITY_TYPE_ID ), nameProperties_(),
		defaultNameProperty_()
	{}

	bool init( DataSectionPtr pEntitiesSection,
		const std::string& defaultTypeName,
		const std::string& defaultNameProperty );

	const MD5::Digest getDigest() const 		{	return md5Digest_;	}
	const std::string& getNameProperty( EntityTypeID index ) const
	{	return nameProperties_[ index ];	}
	const std::string& getDefaultNameProperty() const
	{ 	return defaultNameProperty_;	}
	const EntityTypeID getDefaultType() const	{	return defaultTypeID_; 	}
	const std::string& getDefaultTypeName() const;

	bool entityTypeHasPassword( EntityTypeID typeID ) const
	{
		return entityPasswordBits_[typeID];
	}
	bool isValidEntityType( EntityTypeID typeID ) const
	{
		return (typeID < entityDescriptionMap_.size()) &&
				!entityDescriptionMap_.entityDescription( typeID ).
					isClientOnlyType();
	}
	EntityTypeID getEntityType( const std::string& typeName ) const
	{
		EntityTypeID typeID = INVALID_TYPEID;
		entityDescriptionMap_.nameToIndex( typeName, typeID );
		return typeID;
	}
	size_t getNumEntityTypes() const
	{
		return entityDescriptionMap_.size();
	}
	const EntityDescription& getEntityDescription( EntityTypeID typeID ) const
	{
		return entityDescriptionMap_.entityDescription( typeID );
	}
	//	This function returns the type name of the given property.
	std::string getPropertyType( EntityTypeID typeID,
		const std::string& propName ) const;


	void debugDump( int detailLevel ) const;
	bool hasMatchingNameProperties( const EntityDefs& other ) const;
};

/**
 *	This class is used to implement the main singleton object that represents
 *	this application.
 */
class Database : public Mercury::TimerExpiryHandler
{
public:
	enum InitResult
	{
		InitResultSuccess,
		InitResultFailure,
		InitResultAutoShutdown
	};
public:
	Database();
	virtual ~Database();
	InitResult init( bool isRecover, bool isUpgrade, DefsSyncMode defsSyncMode, bool updateSelfIndex = false );
	void run();
	void finalise();

	BaseAppMgr & baseAppMgr() { return baseAppMgr_; }

	// Overrides
	int handleTimeout( int id, void * arg );

	void handleStatusCheck( BinaryIStream & data );

	// Lifetime messages
	void handleBaseAppMgrBirth( DBInterface::handleBaseAppMgrBirthArgs & args );
	void handleDatabaseBirth( DBInterface::handleDatabaseBirthArgs & args );
	void shutDown( DBInterface::shutDownArgs & /*args*/ );
	void shutDown();

	void controlledShutDown( DBInterface::controlledShutDownArgs & args );
	void cellAppOverloadStatus( DBInterface::cellAppOverloadStatusArgs & args );

	// Entity messages
	void logOn( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data );

	void logOn( const Mercury::Address & srcAddr,
		Mercury::ReplyID replyID,
		LogOnParamsPtr pParams,
		const Mercury::Address & addrForProxy );

	void onLogOnLoggedOnUser( EntityTypeID typeID, DatabaseID dbID,
		LogOnParamsPtr pParams,
		const Mercury::Address & proxyAddr, const Mercury::Address& replyAddr,
		Mercury::ReplyID replyID, const EntityMailBoxRef* pExistingBase );

	void onEntityLogOff( EntityTypeID typeID, DatabaseID dbID );

	void sendFailure( Mercury::ReplyID replyID, const Mercury::Address & dstAddr,
		DatabaseLoginStatus reason, const char * pDescription = NULL );

	void loadEntity( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data );

	void writeEntity( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data );

	void deleteEntity( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		DBInterface::deleteEntityArgs & args );
	void deleteEntityByName( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data );

	void lookupEntity( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		DBInterface::lookupEntityArgs & args );
	void lookupEntityByName( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data );
	void lookupDBIDByName( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data );
	void lookupEntity( EntityDBKey & ekey, BinaryOStream & bos );

	// Misc messages
	void executeRawCommand( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data );

	void putIDs( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data );

	void getIDs( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data );

	void writeSpaces( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data );

	void writeGameTime( DBInterface::writeGameTimeArgs & args );

	void handleBaseAppDeath( const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data );

	void resourceVersionTag( DBInterface::resourceVersionTagArgs& args );
	void resourceVersionControl(
		DBInterface::resourceVersionControlArgs & args );
	uint32 getBundleResourceVersion() const;


	// Accessors
	static Database & instance();
	static Database * pInstance();

	const EntityDefs& getEntityDefs() const
		{ return *pEntityDefs_; }
	EntityDefs& swapEntityDefs( EntityDefs& entityDefs )
	{
		EntityDefs& curDefs = *pEntityDefs_;
		pEntityDefs_ = &entityDefs;
		return curDefs;
	}

	Mercury::Nub & nub()	{ return nub_; }

	static Mercury::Channel & getChannel( const Mercury::Address & addr )
	{
		return Database::instance().nub().findOrCreateChannel( addr );
	}

	WorkerThreadMgr& getWorkerThreadMgr()	{	return workerThreadMgr_;	}
	IDatabase& getIDatabase()	{ MF_ASSERT(pDatabase_); return *pDatabase_; }
	IDatabase::IOldDatabase* getOldDatabaseForBundle() const;

	// IDatabase pass-through methods. Call these instead of IDatabase methods
	// so that we can intercept and muck around with stuff.
	/**
	 *	This class is meant to replace IDatabase::IGetEntityHandler as the base
	 * 	class for IDatabase::getEntity() callbacks. It allows us to muck around
	 * 	with the results before passing them on to the actual handler.
	 */
	class GetEntityHandler : public IDatabase::IGetEntityHandler
	{
	public:
		// Intercepts the result callback. Derived classes should NOT implement
		// this method.
		virtual void onGetEntityComplete( bool isOK );
		// Derived classes should implement this method instead of
		// onGetEntityComplete() - note the extra 'd'.
		virtual void onGetEntityCompleted( bool isOK ) = 0;
	};
	void getEntity( GetEntityHandler& handler,
			bool shouldCheckBundleVersion = false );
	void putEntity( const EntityDBKey& ekey, EntityDBRecordIn& erec,
			IDatabase::IPutEntityHandler& handler,
			bool shouldCheckBundleVersion = false );

	bool shouldLoadUnknown() const		{ return shouldLoadUnknown_; }
	bool shouldCreateUnknown() const	{ return shouldCreateUnknown_; }
	bool shouldRememberUnknown() const	{ return shouldRememberUnknown_; }

	static bool isValidDefsSyncMode( int mode )
	{	return (mode < DefsSyncModeMax) && (mode > DefsSyncModeInvalid);	}
	DefsSyncMode getDefsSyncMode() const
	{	return DefsSyncMode(defsSyncMode_);	}

	bool clearRecoveryDataOnStartUp() const
										{ return clearRecoveryDataOnStartUp_; }

	// Watcher
	void hasStartBegun( bool hasStartBegun );
	bool hasStartBegun() const			{ return hasStartBegun_; }
	bool hasStarted() const				{ return hasStarted_; }

	bool isReady() const
	{
		return baseAppMgr_.channel().isEstablished();
	}

	void startServerBegin( bool isRecover = false );
	void startServerEnd( bool isRecover = false );

	// Sets baseRef to "pending base creation" state.
	static void setBaseRefToLoggingOn( EntityMailBoxRef& baseRef,
		EntityTypeID entityTypeID )
	{
		baseRef.init( 0, Mercury::Address( 0, 0 ),
			EntityMailBoxRef::BASE, entityTypeID );
	}
	// Checks that pBaseRef is fully checked out i.e. not in "pending base
	// creation" state.
	static bool isValidMailBox( const EntityMailBoxRef* pBaseRef )
	{	return (pBaseRef && pBaseRef->id != 0);	}

	bool defaultEntityToStrm( EntityTypeID typeID, const std::string& name,
		BinaryOStream& strm, const std::string* pPassword = 0 ) const;

	static DatabaseID* prepareCreateEntityBundle( EntityTypeID typeID,
		DatabaseID dbID, const Mercury::Address& addrForProxy,
		Mercury::ReplyMessageHandler* pHandler, Mercury::Bundle& bundle,
		LogOnParamsPtr pParams = NULL );

	RelogonAttemptHandler* getInProgRelogonAttempt( EntityTypeID typeID,
			DatabaseID dbID )
	{
		PendingAttempts::iterator iter =
				pendingAttempts_.find( EntityID( typeID, dbID ) );
		return (iter != pendingAttempts_.end()) ? iter->second : NULL;
	}
	void onStartRelogonAttempt( EntityTypeID typeID, DatabaseID dbID,
		 	RelogonAttemptHandler* pHandler )
	{
		MF_VERIFY( pendingAttempts_.insert(
				PendingAttempts::value_type( EntityID( typeID, dbID ),
						pHandler ) ).second );
	}
	void onCompleteRelogonAttempt( EntityTypeID typeID, DatabaseID dbID )
	{
		MF_VERIFY( pendingAttempts_.erase( EntityID( typeID, dbID ) ) > 0 );
	}

	bool onStartEntityCheckout( EntityTypeID typeID, DatabaseID dbID )
	{
		return inProgCheckouts_.insert( EntityID( typeID, dbID ) ).second;
	}
	bool onCompleteEntityCheckout( EntityTypeID typeID, DatabaseID dbID,
		const EntityMailBoxRef* pBaseRef );

	struct ICheckoutCompletionListener
	{
		// This method is called when the onCompleteEntityCheckout() is
		// called for the entity that you've registered for via
		// registerCheckoutCompletionListener(). After this call, this callback
		// will be automatically deregistered.
		virtual void onCheckoutCompleted( const EntityMailBoxRef* pBaseRef ) = 0;
	};
	bool registerCheckoutCompletionListener( EntityTypeID typeID,
			DatabaseID dbID, ICheckoutCompletionListener& listener );

	bool hasMailboxRemapping() const	{ return !remappingDestAddrs_.empty(); }
	void remapMailbox( EntityMailBoxRef& mailbox ) const;
    bool updateSelfIndex(){ return updateSelfIndex_; }

private:
	void endMailboxRemapping();

#ifdef DBMGR_SELFTEST
		void runSelfTest();
#endif

	Mercury::Nub		nub_;
	WorkerThreadMgr		workerThreadMgr_;
	EntityDefs*			pEntityDefs_;
	IDatabase*			pDatabase_;
	DbUpdater*			pDbUpdater_;
	BWResource*			pUpdatedBWResource_;

	BaseAppMgr			baseAppMgr_;

	bool				shouldLoadUnknown_;
	bool 				shouldCreateUnknown_;
	bool				shouldRememberUnknown_;
	int					defsSyncMode_;

	bool				allowEmptyDigest_;
	bool				shutDownBaseAppMgrOnExit_;

	bool				hasStartBegun_;
	bool				hasStarted_;

	int					desiredBaseApps_;
	int					desiredCellApps_;

	int					statusCheckTimerId_;

	bool				clearRecoveryDataOnStartUp_;

	Mercury::Nub::TransientMiniTimer	writeEntityTimer_;

	friend class RelogonAttemptHandler;
	typedef std::pair< EntityTypeID, DatabaseID > EntityID;
	typedef std::map< EntityID, RelogonAttemptHandler * > PendingAttempts;
	PendingAttempts pendingAttempts_;
	typedef std::set< EntityID > EntityIDSet;
	EntityIDSet	inProgCheckouts_;

	typedef std::multimap< std::pair< EntityTypeID, DatabaseID >,
			ICheckoutCompletionListener* > CheckoutCompletionListeners;
	CheckoutCompletionListeners checkoutCompletionListeners_;

	float				curLoad_;
	float				maxLoad_;
	bool				anyCellAppOverloaded_;

	Mercury::Address	remappingSrcAddr_;
	BackupHash			remappingDestAddrs_;
	int					mailboxRemapCheckCount_;

    bool                updateSelfIndex_;
	static Database *	pInstance_;
};

#ifdef CODE_INLINE
#include "database.ipp"
#endif

#endif // DATABASE_HPP

// database.hpp
