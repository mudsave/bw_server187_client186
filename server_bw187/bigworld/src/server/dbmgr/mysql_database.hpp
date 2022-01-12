/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef MYSQL_DATABASE_HPP
#define MYSQL_DATABASE_HPP

#include "entitydef/entity_description_map.hpp"
#include "idatabase.hpp"

class BufferedEntityTasks;

class MySql;
class MySqlTransaction;
class MySqlThreadResPool;
class MySqlThreadData;
class MigrateToNewDefsTask;
class OldMySqlDatabase;
class MySqlServerConfig;

/**
 *	This class is an implementation of IDatabase for the MySQL database.
 */
class MySqlDatabase : public IDatabase, public Mercury::TimerExpiryHandler
{
private:
	MySqlDatabase();
public:
	static MySqlDatabase * create();
	~MySqlDatabase();

	virtual bool startup( const EntityDefs&, bool, bool );
	virtual bool shutDown();

	virtual void mapLoginToEntityDBKey(
		const std::string & logOnName, const std::string & password,
		IDatabase::IMapLoginToEntityDBKeyHandler& handler );
	virtual void setLoginMapping( const std::string & username,
		const std::string & password, const EntityDBKey& ekey,
		ISetLoginMappingHandler& handler );

	virtual void getEntity( IGetEntityHandler& handler );
	virtual void putEntity( const EntityDBKey& ekey, EntityDBRecordIn& erec,
		IPutEntityHandler& handler );
	virtual void delEntity( const EntityDBKey & ekey,
		IDatabase::IDelEntityHandler& handler );

	virtual void executeRawCommand( const std::string & command,
		IExecuteRawCommandHandler& handler );

	virtual void putIDs( int count, const ObjectID * ids );
	virtual void getIDs( int count, IGetIDsHandler& handler );

	// Backing up spaces.
	virtual void startSpaces();
	virtual void endSpaces();
	virtual void writeSpace( SpaceID id );
	virtual void writeSpaceData( SpaceID spaceID, int64 spaceKey,
			uint16 dataKey, const std::string & data );
	uint8 getSpacesVersion() const	{	return spacesVersion_;	}

	virtual bool restoreGameState();

	TimeStamp getGameTime();
	void setGameTime( TimeStamp gameTime );

	virtual void remapEntityMailboxes( const Mercury::Address& srcAddr,
			const BackupHash & destAddrs );

	virtual void migrateToNewDefs( const EntityDefs& newEntityDefs,
		IDatabase::IMigrationHandler& handler );
	virtual IDatabase::IOldDatabase* switchToNewDefs(
		const EntityDefs& oldEntityDefs );

	// Mercury::TimerExpiryHandler override
	virtual int handleTimeout( int id, void * arg );

	MySqlThreadResPool& getThreadResPool()	{	return *pThreadResPool_;	}
	MySqlThreadData& getMainThreadData();

	bool hasFatalConnectionError()	{	return reconnectTimerID_ != -1;	}
	void onConnectionFatalError();
	bool restoreConnectionToDb();

	void onPutEntityOpStarted( EntityTypeID typeID, DatabaseID dbID );
	void onPutEntityOpCompleted( EntityTypeID typeID, DatabaseID dbID );
	void onDelEntityOpStarted( EntityTypeID typeID, DatabaseID dbID );
	void onDelEntityOpCompleted( EntityTypeID typeID, DatabaseID dbID );
	void onWriteSpaceOpStarted()	{	++numWriteSpaceOpsInProgress_;	}
	void onWriteSpaceOpCompleted()	{	--numWriteSpaceOpsInProgress_;	}

	void setupUpdateTemporaryTables();

	void onOldDatabaseDestroy()		{ 	pOldDatabase_ = 0; 	}

	uint watcherGetNumBusyThreads() const;
	double watcherGetBusyThreadsMaxElapsedSecs() const;
	double watcherGetAllOpsCountPerSec() const;
	double watcherGetAllOpsAvgDurationSecs() const;

private:
	MySqlThreadResPool* pThreadResPool_;

	int maxSpaceDataSize_;
	int numConnections_;
	std::auto_ptr<MySqlServerConfig> pServerConfig_;
	int numWriteSpaceOpsInProgress_;
	uint8 spacesVersion_;

	int	reconnectTimerID_;
	size_t reconnectCount_;

	MigrateToNewDefsTask* 	pMigrationTask_;
	OldMySqlDatabase* 		pOldDatabase_;
	BufferedEntityTasks *	pBufferedEntityTasks_;
};

#endif
