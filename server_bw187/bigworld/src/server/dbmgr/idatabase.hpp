/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef IDATABASE_HPP
#define IDATABASE_HPP

#include "network/basictypes.hpp"
#include "common/login_interface.hpp"
#include "server/backup_hash.hpp"

#include <string>
#include <limits>

class EntityDescriptionMap;
class EntityDefs;

typedef LogOnStatus DatabaseLoginStatus;
typedef std::vector<EntityTypeID> TypeIDVec;
typedef std::map< EntityTypeID, EntityTypeID > TypeIDTypeIDMap;

const uint32 INVALID_RESOURCE_VERSION = std::numeric_limits<uint32>::max();

/**
 *	This enum defines the level of modifications we can make when trying to
 * 	sync the defs with the database tables.
 */
enum DefsSyncMode
{
	DefsSyncModeInvalid = -1,
	DefsSyncModeNoSync = 0,
	DefsSyncModeAddOnly = 1,
	DefsSyncModeAddAndUpdate = 2,
	DefsSyncModeFullSync = 3,
	DefsSyncModeMax
};

/**
 *	This struct is a key to an entity record in the database
 */
struct EntityDBKey
{
	EntityDBKey( EntityTypeID _typeID, DatabaseID _dbID,
			const std::string & s = std::string() ) :
		typeID( _typeID ), dbID( _dbID ), name( s ) {}
	EntityTypeID	typeID;	///< always required
	DatabaseID		dbID;	///< preferred but optional
	std::string		name;	///< used if dbID is zero
};

/**
 *	This class is for exchanging EntityMailBoxRef information with
 *	the database using IDatabase. EntityMailBoxRef is optional. If it is not
 *	provided, then is is neither retrieved nor put into the database.
 *	When EntityMailBoxRef is provided, it can also be NULL e.g.
 *	<pre>
 *		EntityMailBoxRef* pNull = 0;
 *		EntityDBRecordBase erec;
 *		erec.provideBaseMB( pNull );
 *	</pre>
 *	This will set the base mailbox to "null" when putting into the database.
 *	When retrieving from the database, a "null" EntityMailBoxRef can be
 *	retrieved, e.g.
 *	<pre>
 *		EntityMailBoxRef mb;
 *		EntityMailBoxRef* pMB = &amp;mb;
 *		EntityDBRecordBase erec;
 *		erec.provideBaseMB( pMB );
 *		idatabase->getEntity( ekey, erec );
 *		if (pMB)
 *			Do something with mb
 *	</pre>
 *	Note that when retrieving information from the database, pMB should not
 *	be NULL otherwise IDatabase does not retrieve the EntityMailBoxRef info.
 *	Note also that pMB can be set to NULL, so you shouldn't make it point to a
 *	"new EntityMailBoxRef", or a memory leak will occur.
 */
class EntityDBRecordBase
{
	EntityMailBoxRef**	ppBaseRef_;

public:
	EntityDBRecordBase() : ppBaseRef_(0)	{}

	void provideBaseMB( EntityMailBoxRef*& pBaseRef )
	{	ppBaseRef_ = &pBaseRef;	}
	void unprovideBaseMB()
	{	ppBaseRef_ = 0;	}
	bool isBaseMBProvided() const
	{	return ppBaseRef_ != NULL;	}
	void setBaseMB( EntityMailBoxRef* pBaseMB )
	{
		MF_ASSERT(isBaseMBProvided());
		if (pBaseMB)
			**ppBaseRef_ = *pBaseMB;
		else
			*ppBaseRef_ = 0;
	}
	EntityMailBoxRef* getBaseMB() const
	{
		MF_ASSERT(isBaseMBProvided());
		return *ppBaseRef_;
	}
};

/**
 *	This class is for exchanging entity property data with the database using
 *	IDatabase. Property data should be provided in a BinaryIStream or
 *	BinaryOStream depending on the direction of the exchange. The stream is
 *	optional. If it is not provided, then the property data of the entity is
 *	neither set nor retrieved.
 */
template < class STRM_TYPE >
class EntityDBRecord : public EntityDBRecordBase
{
	STRM_TYPE*	pStrm_;	// Optional stream containing entity properties

public:
	EntityDBRecord() : EntityDBRecordBase(), pStrm_(0)	{}

	void provideStrm( STRM_TYPE& strm )
	{	pStrm_ = &strm;	}
	void unprovideStrm()
	{	pStrm_ = 0;	}
	bool isStrmProvided() const
	{	return pStrm_ != 0;	}
	STRM_TYPE& getStrm() const
	{
		MF_ASSERT(isStrmProvided());
		return *pStrm_;
	}

private:
	EntityDBRecord( const EntityDBRecord & );
};

typedef EntityDBRecord<BinaryIStream> EntityDBRecordIn;
typedef EntityDBRecord<BinaryOStream> EntityDBRecordOut;

/**
 *	This class defines an interface to the database. This allows us to support
 *	different database types (such as XML or oracle).
 *
 *	Many functions in this interface are asynchronous i.e. return results
 *	through callbacks. The implementation has the option to call the
 *	callback prior to returning from the function, essentially making it
 *	a synchronous operation. Asynchronous implementations should take a copy
 *	the function parameters as the caller is not required to keep them alive
 *	after the function returns.
 */
class IDatabase
{
public:
	virtual ~IDatabase() {}
	virtual bool startup( const EntityDefs& entityDefs, bool isFaultRecovery,
		bool isUpgrade ) = 0;
	virtual bool shutDown() = 0;

	/**
	 *	This is the callback interface used by mapLoginToEntityDBKey().
	 */
	struct IMapLoginToEntityDBKeyHandler
	{
		/**
		 *	This method is called when mapLoginToEntityDBKey() completes.
		 *
		 *	@param	status	The status of the log in.
		 *	@param	ekey	The entity key if the logon is successful.
		 *	NOTE: Only one of ekey.dbID or ekey.name is required to be set.
		 */
		virtual void onMapLoginToEntityDBKeyComplete(
			DatabaseLoginStatus status, const EntityDBKey& ekey ) = 0;
	};

	/**
	 *	This function turns user/pass login credentials into the EntityDBKey
	 *	associated with them.
	 *
	 *	This method provides the result of the operation by calling
	 *	IMapLoginToEntityDBKeyHandler::onMapLoginToEntityDBKeyComplete().
	 */
	virtual void mapLoginToEntityDBKey(
		const std::string & username, const std::string & password,
		IMapLoginToEntityDBKeyHandler& handler ) = 0;

	/**
	 *	This is the callback interface used by addLoginMapping().
	 */
	struct ISetLoginMappingHandler
	{
		/**
		 *	This method is called when addLoginMapping() completes.
		 */
		virtual void onSetLoginMappingComplete() = 0;
	};

	/**
	 *	This function sets the mapping between user/pass to an entity.
	 *
	 *	This method provides the result of the operation by calling
	 *	ISetLoginMappingHandler::onSetLoginMappingComplete().
	 */
	virtual void setLoginMapping( const std::string & username,
		const std::string & password, const EntityDBKey& ekey,
		ISetLoginMappingHandler& handler ) = 0;

	/**
	 *	This is the callback interface used by getEntity().
	 */
	struct IGetEntityHandler
	{
		/**
		 *	This method is called by getEntity() to obtain the
		 *	EntityDBKey used to identify the entity in the database.
		 *	If EntityDBKey::dbID != 0 then it will be used and
		 *	EntityDBKey::name will be set from entity data.
		 *	If EntityDBKey::dbID == 0 then EntityDBKey::name will be used and
		 *	EntityDBKey::dbID will be set from the entity data.
		 *	This method may be called multiple times and the implementation
		 *	should return the same instance of EntityDBKey each time.
		 *	This method may be called from another thread.
		 */
		virtual EntityDBKey& key() = 0;

		/**
		 *	This method is called by getEntity() to obtain the
		 *	EntityDBRecordOut structure. EntityDBRecordOut structure is used
		 *	by getEntity() to determine what to retrieve from the database, and
		 *	to store the data retrieved from the database. This method may be
		 *	called multiple times and the implementation should return the
		 *	same instance of EntityDBRecordOut each time.
		 *	This method may be called from another thread.
		 */
		virtual EntityDBRecordOut& outrec() = 0;

		/**
		 *	This method is called by getEntity() to get the password that
		 *	overrides the "password" property of the entity. If this
		 *	function returns NULL, then the "password" property is not
		 *	overridden. This method may be called from another thread.
		 */
		virtual const std::string* getPasswordOverride() const {	return 0;	}

		/**
		 *	This method is called when getEntity() completes.
		 *
		 *	@param	isOK	True if sucessful. False if the entity cannot
		 *	be found with ekey.
		 */
		virtual void onGetEntityComplete( bool isOK ) = 0;
	};

	/**
 	 *	This method retrieves an entity's data from the database.
	 *
	 *	@param	handler	This handler will be asked to provide the parameters
	 *	for this operation, and it will be notified when the operation
	 *	completes.
	 */
	virtual void getEntity( IGetEntityHandler& handler ) = 0;

	/**
	 *	This is the callback interface used by putEntity().
	 */
	struct IPutEntityHandler
	{
		/**
		 *	This method is called when putEntity() completes.
		 *
		 *	@param	isOK	Is true if entity was written to the database
		 *	Is false if ekey.dbID != 0 but didn't identify an existing entity.
		 *	@param	dbID	Is the database ID of the entity. Particularly
		 *	useful if ekey.dbID == 0 originally.
		 */
		virtual void onPutEntityComplete( bool isOK, DatabaseID dbID ) = 0;
	};

	/**
	 *	This method writes an entity's data into the database.
	 *
	 *	@param	ekey	This contains the key to find the entity.
	 *					If ekey.dbID != 0 then it will be used to find the entity.
	 *					If ekey.dbID == 0 then a new entity will be inserted.
	 *					ekey.name is not used at all.
	 *	@param	erec	This contains the entity data. See EntityDBRecord
	 *					for description on how to specify what data to write.
	 *	@param	handler	This handler will be notified when the operation
	 *	completes.
	 *
	 */
	virtual void putEntity( const EntityDBKey& ekey,
		EntityDBRecordIn& erec, IPutEntityHandler& handler ) = 0;

	/**
	 *	This is the callback interface used by delEntity().
	 */
	struct IDelEntityHandler
	{
		/**
		 *	This method is called when delEntity() completes.
		 *
		 *	@param	isOK	Is true if entity was deleted from the database.
		 *	Is false if the entity doesn't exist.
		 */
		virtual void onDelEntityComplete( bool isOK ) = 0;
	};

	/**
	 *	This method deletes an entity from the database, if it exists.
	 *
	 *	@param	ekey	The key for indentifying the entity. If ekey.dbID != 0
	 *	then it will be used to identify the entity. Otherwise ekey.name will
	 *	be used.
	 *	@param	handler	This handler will be notified when the operation
	 *	completes.
	 */
	virtual void delEntity( const EntityDBKey & ekey,
		IDelEntityHandler& handler ) = 0;

	//-------------------------------------------------------------------

	virtual void setGameTime( TimeStamp time ) {};
	virtual TimeStamp getGameTime() { return 0; }

	//-------------------------------------------------------------------

	/**
	 *	This is the callback interface used by executeRawCommand().
	 */
	struct IExecuteRawCommandHandler
	{
		/**
		 *	This method is called by executeRawCommand() to get the stream
		 *	in which to store the results of the operation. This method
		 *	may be called from another thread.
		 *
		 *	This function may be called multiple times and the implementation
		 *	should return the stream instance each time.
		 */
		virtual BinaryOStream& response() = 0;

		/**
		 *	This method is called when executeRawCommand() completes.
		 */
		virtual void onExecuteRawCommandComplete() = 0;
	};
	virtual void executeRawCommand( const std::string & command,
		IExecuteRawCommandHandler& handler ) = 0;

	virtual void putIDs( int count, const ObjectID * ids ) = 0;

	/**
	 *	This is the callback interface used by getIDs().
	 */
	struct IGetIDsHandler
	{
		/**
		 *	This method is called by getIDs() to get the stream	in which to
		 *	store the IDs. This method may be called from another thread.
		 *
		 *	This function may be called multiple times and the implementation
		 *	should return the stream instance each time.
		 */
		virtual BinaryOStream& idStrm() = 0;

		/**
		 *	This method is called when getIDs() completes.
		 */
		virtual void onGetIDsComplete() = 0;
	};
	virtual void getIDs( int count, IGetIDsHandler& handler ) = 0;

	virtual void startSpaces()	{};
	virtual void endSpaces()	{};
	virtual void writeSpace( SpaceID id ) {};
	virtual void writeSpaceData( SpaceID spaceID, int64 spaceKey,
			uint16 dataKey, const std::string & data ) {};

	virtual bool restoreGameState() { return true; }

	/**
	 *	This method converts all the entity mailboxes in the
	 * 	database from srcAddr to destAddrs using the following algorithm:
	 * 		IF mailbox.addr == srcAddr
	 *			mailbox.addr = destAddrs[ mailbox.id % destAddrs.size() ]
	 * 	NOTE: The salt of the addr must not be modified.
	 */
	virtual void remapEntityMailboxes( const Mercury::Address& srcAddr,
			const BackupHash & destAddrs ) = 0;

	//-------------------------------------------------------------------

	/**
	 *	This is the callback interface used by migrateToNewDefs().
	 */
	struct IMigrationHandler
	{
		/**
		 *	This method is called when migrateToNewDefs() completes.
		 *
		 * 	@param	isOK	Whether the migration completed successfully.
		 */
		virtual void onMigrateToNewDefsComplete( bool isOK ) = 0;
	};

	/**
	 * 	This method starts process of migrating the data so that it is
	 * 	compatible with the new entity definitions. While migration is occurring
	 * 	entity data must be presented in the old format. When the migration
	 * 	completes IMigrationHandler::onMigrateToNewDefsComplete() is called.
	 * 	If the migration is successful, switchToNewDefs() must be called so
	 * 	that all the interface functions now expect entity data in the new
	 * 	format.
	 *
	 * 	@param	newEntityDefs	The new entity definitions.
	 * 	@param	handler			This handler will be notified when the operation
	 * 	completes.
	 */
	virtual void migrateToNewDefs( const EntityDefs& newEntityDefs,
		IMigrationHandler& handler ) = 0;

	/**
	 *	This interface is returned by switchToNewDefs() so that entity data
	 * 	in the old format can still be retrieved and written to the database.
	 * 	The interface pointer should be deleted as soon as it is not needed
	 * 	to free up resources.
	 * 	Methods should behave identically to those in IDatabase.
	 */
	struct IOldDatabase
	{
		virtual ~IOldDatabase() {};
		virtual void getEntity( IGetEntityHandler& handler ) = 0;
		virtual void putEntity( const EntityDBKey& ekey,
			EntityDBRecordIn& erec, IPutEntityHandler& handler ) = 0;
	};

	/**
	 *	This method should be called after migrateToNewDefs() has completed
	 * 	successfully so that interface functions now expect entity data
	 * 	in the new format. If this function is called prior to completion
	 * 	of migrateToNewDefs(), then switchToNewDefs() will block until
	 *  migrateToNewDefs() completes. Even in this case,
	 * 	IMigrationHandler::onMigrateToNewDefsComplete() is still called.
	 *
	 * 	@param oldEntityDefs	The old entity definitions. This object is
	 * 	referenced until the returned IOldDatabase object is destroyed.
	 * 	@return	Old interface that can still process entities in the old format.
	 * 	NULL if there was an error in migrateToNewDefs() or if it hasn't been
	 * 	called.
	 */
	virtual IOldDatabase* switchToNewDefs( const EntityDefs& oldEntityDefs ) = 0;
};

#endif // IDATABASE_HPP
