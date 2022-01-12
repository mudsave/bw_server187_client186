/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "Python.h"		// See http://docs.python.org/api/includes.html

#include "mysql_database.hpp"

#include "entity_recoverer.hpp"
#include "mysql_typemapping.hpp"
#include "mysql_wrapper.hpp"
#include "mysql_notprepared.hpp"
#include "database.hpp"
#include "db_interface_utils.hpp"

#include "baseappmgr/baseappmgr_interface.hpp"
#include "cstdmf/debug.hpp"
#include "cstdmf/memory_stream.hpp"
#include "cstdmf/watcher.hpp"
#include "server/bwconfig.hpp"
#include "common/des.h"

DECLARE_DEBUG_COMPONENT(0)

// -----------------------------------------------------------------------------
// Section: Constants
// -----------------------------------------------------------------------------
#define THREAD_TASK_WARNING_DURATION 		stampsPerSecond()
#define THREAD_TASK_TIMING_RESET_DURATION 	(5 * stampsPerSecond())

// -----------------------------------------------------------------------------
// Section: Utility classes
// -----------------------------------------------------------------------------
/**
 *	This is a horrible utility class to get around the fact that you can't
 * 	run some arbitrary code while in a constructor initialiser list. e.g.
 * 		MyClass::MyClass() : member1( param1, param2 ), member2( param3 ) {}
 * 	If you needed to call a function (e.g. to initialise some globals) before
 * 	constructing member1 then you're in trouble. You can either expose that
 * 	requirement to the user of MyClass (horrible), make member1 a pointer (can
 * 	be horrible if member2 depends on member1 being constructed, which means
 * 	you'll can set off an avalanche of turning members into pointers) or you
 * 	can	use this utility class (which is quite horrible in itself).
 *
 * 	To use this utility class, you'll need an ACTIVITY class which has some
 * 	code in it's constructor (that will initialise some globals, for example).
 * 	Then you pass an instance of this class in place of one of the parameters
 * 	to member1's constructor. For example, if param2 is an int:
 * 		MyClass::MyClass() : member1( param1,
 * 			InitialiserHack<int,ActivityClass>( param2, activityParam ) ),
 * 			member2( param3 ) {}
 * 	The constructor of ActivityClass will be called prior to the construction
 * 	of member1 and the destructor will be called after the construction of
 * 	member1 but before the construction of member2.
 */
template <class TARGET, class ACTIVITY>
class InitialiserHack
{
	TARGET		target_;
	ACTIVITY	activity_;

public:
	template <class ACTIVITYPARAM>
	InitialiserHack( TARGET target, ACTIVITYPARAM actParam ) :
		target_( target ), activity_( actParam )
	{}
	operator TARGET () { return target_;	}
};

/**
 *	This is an InitialiserHack specifically to swap Python interpreters
 */
template <class TARGET>
class SwapInterpreterInitialiserHack :
	public InitialiserHack<TARGET,Script::AutoInterpreterSwapper>
{
	typedef InitialiserHack<TARGET,Script::AutoInterpreterSwapper> BaseClass;
public:
	SwapInterpreterInitialiserHack( TARGET target, PyThreadState* pInterpreter )
		: BaseClass( target, pInterpreter ) {}
};

// -----------------------------------------------------------------------------
// Section: class MySqlServerConfig
// -----------------------------------------------------------------------------
/**
 *	This class holds information required to connect to the MySQL database
 * 	and its backup databases.
 */
class MySqlServerConfig
{
public:
	struct ServerInfo
	{
		std::string 			configName;
		MySql::ConnectionInfo	connectionInfo;
	};

private:
	typedef std::vector<ServerInfo> ServerInfos;

	ServerInfos				serverInfos_;
	ServerInfos::iterator	pCurServer_;

public:
	MySqlServerConfig();

	const ServerInfo& getCurServer() const
	{
		return *pCurServer_;
	}

	size_t getNumServers() const
	{
		return serverInfos_.size();
	}

	bool gotoNextServer()
	{
		++pCurServer_;
		if (pCurServer_ == serverInfos_.end())
			pCurServer_ = serverInfos_.begin();
		return (pCurServer_ != serverInfos_.begin());
	}

	void gotoPrimaryServer()
	{
		pCurServer_ = serverInfos_.begin();
	}

private:
	static void readConnectionInfo( MySql::ConnectionInfo& connectionInfo,
			const std::string& parentPath );
};

MySqlServerConfig::MySqlServerConfig()
	: serverInfos_( 1 )
{
	// Default config if <dbMgr> section is missing.
	ServerInfo& primaryServer = serverInfos_.front();
	primaryServer.configName = "<primary>";
	primaryServer.connectionInfo.host = "localhost";
	primaryServer.connectionInfo.username = "bigworld";
	primaryServer.connectionInfo.password = "bigworld";
	primaryServer.connectionInfo.database = "bigworld";

	// Get primary server
	readConnectionInfo( primaryServer.connectionInfo, "dbMgr" );

	// Get backup servers.
	{
		std::vector< std::string > childrenNames;
		BWConfig::getChildrenNames( childrenNames, "dbMgr/backupDatabases" );

		serverInfos_.reserve( serverInfos_.size() + childrenNames.size() );
		const ServerInfo& primaryServer = serverInfos_.front();
		for ( std::vector< std::string >::const_iterator iter =
				childrenNames.begin(); iter != childrenNames.end(); ++iter )
		{
			// By default backups look like the primary server so users
			// can override only those fields that are different.
			serverInfos_.push_back( primaryServer );
			ServerInfo& backupServer = serverInfos_.back();
			backupServer.configName = *iter;
			readConnectionInfo( backupServer.connectionInfo,
					"dbMgr/backupDatabases/" + *iter );
		}
	}

	pCurServer_ = serverInfos_.begin();
}

/**
 *	This method sets the serverInfo from the given data section.
 */
void MySqlServerConfig::readConnectionInfo(
		MySql::ConnectionInfo& connectionInfo, const std::string& parentPath )
{
	BWConfig::update( (parentPath + "/host").c_str(), connectionInfo.host );
	BWConfig::update( (parentPath + "/username").c_str(), connectionInfo.username );
	BWConfig::update( (parentPath + "/password").c_str(), connectionInfo.password );
    //使用加密工具，解密才是正确密码 LuoCD add 2009-08-26 
	if( connectionInfo.password != "")
	{
	    //CPwdDesTool des;
	    CDES des;
	    INFO_MSG("dbmgr before decrypt ,password = %s ....\n",connectionInfo.password.c_str());
	    connectionInfo.password = des.DecryptString(connectionInfo.password);
	    //INFO_MSG("dbmgr after des decrypt ,password = %s ....\n",connectionInfo.password.c_str());
	}
	// LuoCD add 2009-08-26  end
	BWConfig::update( (parentPath + "/name").c_str(), connectionInfo.database );
}

// -----------------------------------------------------------------------------
// Section: class MySqlThreadData
// -----------------------------------------------------------------------------
/**
 *	This struct holds the data necessary for an operation to be run on another
 *	thread.
 */
struct MySqlThreadData
{
	MySql				connection;
	MySqlTypeMapping	typeMapping;
	uint64				startTimestamp;

	ObjectID boundID_;
	int boundLimit_;
	std::auto_ptr<MySqlStatement> putIDStatement_;
	std::auto_ptr<MySqlUnPrep::Statement> getIDsStatement_;
	std::auto_ptr<MySqlUnPrep::Statement> delIDsStatement_;

	std::auto_ptr<MySqlStatement> incIDStatement_;
	std::auto_ptr<MySqlStatement> getIDStatement_;

	TimeStamp gameTime_;
	std::auto_ptr<MySqlStatement> getGameTimeStatement_;
	std::auto_ptr<MySqlStatement> setGameTimeStatement_;

	SpaceID boundSpaceID_;
	uint8 spacesVersion_;
	uint64 boundSpaceEntryID_;
	uint16 boundSpaceDataKey_;
	MySqlBuffer boundSpaceData_;
	std::auto_ptr<MySqlStatement> writeSpaceStatement_;
	std::auto_ptr<MySqlStatement> writeSpaceDataStatement_;

	std::auto_ptr<MySqlStatement> endSpacesStatement_;
	std::auto_ptr<MySqlStatement> endSpaceDataStatement_;

	// Temp variables common to various tasks.
	EntityDBKey			ekey;
	bool				isOK;
	std::string			exceptionStr;

	MySqlThreadData(  const MySql::ConnectionInfo& connInfo,
					  int maxSpaceDataSize,
					  const EntityDefs& entityDefs,
					  const char * tblNamePrefix = TABLE_NAME_PREFIX,
					  const TypeIDSet* pTypes = NULL );
};

MySqlThreadData::MySqlThreadData(  const MySql::ConnectionInfo& connInfo,
								   int maxSpaceDataSize,
                                   const EntityDefs& entityDefs,
                                   const char * tblNamePrefix,
                                   const TypeIDSet* pTypes )
	: connection( connInfo ),
	typeMapping( connection, entityDefs, tblNamePrefix, pTypes ),
	startTimestamp(0),
	putIDStatement_( new MySqlStatement( connection,
							"INSERT INTO bigworldUsedIDs (id) VALUES (?)" ) ),
	// The following two do not work as prepared statements. It
	// appears that the LIMIT arguement cannot be prepared.
	getIDsStatement_( new MySqlUnPrep::Statement( connection,
							"SELECT id FROM bigworldUsedIDs LIMIT ?" ) ),
	delIDsStatement_( new MySqlUnPrep::Statement( connection,
							"DELETE FROM bigworldUsedIDs LIMIT ?" ) ),
	incIDStatement_( new MySqlStatement( connection,
							"UPDATE bigworldNewID SET id=id+?" ) ),
	getIDStatement_( new MySqlStatement( connection,
							"SELECT id FROM bigworldNewID LIMIT 1" ) ),
	getGameTimeStatement_( new MySqlStatement( connection,
								"SELECT * FROM bigworldGameTime LIMIT 1" ) ),
	setGameTimeStatement_( new MySqlStatement( connection,
								"UPDATE bigworldGameTime SET time=?" ) ),
	boundSpaceData_( maxSpaceDataSize ),
	writeSpaceStatement_( new MySqlStatement( connection,
							"REPLACE INTO bigworldSpaces (id, version) "
							"VALUES (?, ?)" ) ),
	writeSpaceDataStatement_( new MySqlStatement( connection,
							"REPLACE INTO bigworldSpaceData "
							"(id, spaceEntryID, entryKey, data, version) "
							"VALUES (?, ?, ?, ?, ?)" ) ),
	endSpacesStatement_( new MySqlStatement( connection,
							"DELETE from bigworldSpaces where version!=?" ) ),
	endSpaceDataStatement_( new MySqlStatement( connection,
							"DELETE from bigworldSpaceData where version!=?" ) ),
	ekey( 0, 0 )
{
	MySqlBindings b;

	b << boundID_;
	putIDStatement_->bindParams( b );
	getIDStatement_->bindResult( b );

	b.clear();
	b << boundLimit_;
	incIDStatement_->bindParams( b );

	b.clear();
	b << gameTime_;
	getGameTimeStatement_->bindResult( b );
	setGameTimeStatement_->bindParams( b );

	b.clear();
	b << boundSpaceID_;
	b << spacesVersion_;
	writeSpaceStatement_->bindParams( b );

	b.clear();
	b << boundSpaceID_;
	b << boundSpaceEntryID_;
	b << boundSpaceDataKey_;
	b << boundSpaceData_;
	b << spacesVersion_;
	writeSpaceDataStatement_->bindParams( b );

	b.clear();
	b << spacesVersion_;
	endSpacesStatement_->bindParams( b );
	endSpaceDataStatement_->bindParams( b );

	// Do unprepared bindings.
	MySqlUnPrep::Bindings b2;
	b2 << boundID_;
	getIDsStatement_->bindResult( b2 );

	b2.clear();
	b2 << boundLimit_;
	getIDsStatement_->bindParams( b2 );
	delIDsStatement_->bindParams( b2 );

	MySqlTransaction transaction( connection ); 
	transaction.execute( "SET character_set_client =  @@character_set_database" );
	transaction.execute( "SET character_set_connection =  @@character_set_database" );
	transaction.execute( "SET character_set_results =  @@character_set_database" );
	transaction.commit();
}

// -----------------------------------------------------------------------------
// Section: class MySqlThreadResPool
// -----------------------------------------------------------------------------
/**
 *	This class is the thread resource pool used by MySqlDatabase to process
 *	incoming requests in parallel.
 */
class MySqlThreadResPool : public Mercury::TimerExpiryHandler
{
	typedef std::vector< MySqlThreadData* > ThreadDataVector;

	WorkerThreadPool					threadPool_;
	auto_container< ThreadDataVector >	threadDataPool_;
	ThreadDataVector					freeThreadData_;
	MySqlThreadData 					mainThreadData_;

	Mercury::Nub&						nub_;
	int									keepAliveTimerID_;

	uint								opCount_;
	uint64								opDurationTotal_;
	uint64								resetTimeStamp_;

	/**
	 *	This class helps to keep the connection to MySQL server alive.
	 */
	class PingTask : public WorkerThread::ITask
	{
		MySqlThreadResPool&	owner_;
		MySqlThreadData* 	pThreadData_;
		bool				pingOK_;

	public:
		PingTask( MySqlThreadResPool& owner ) :
			owner_( owner ), pThreadData_( owner.acquireThreadData() ),
			pingOK_( true )
		{
			MF_ASSERT( pThreadData_ );
			owner_.startThreadTaskTiming( *pThreadData_ );
		}

		virtual ~PingTask()
		{
			uint64 duration = owner_.stopThreadTaskTiming( *pThreadData_ );
			if (duration > THREAD_TASK_WARNING_DURATION)
				WARNING_MSG( "PingTask took %f seconds\n",
							double(duration) / stampsPerSecondD() );
			owner_.releaseThreadData( *pThreadData_ );
		}

		// WorkerThread::ITask overrides
		virtual void run()
		{
			pingOK_ = pThreadData_->connection.ping();
		}
		virtual void onRunComplete()
		{
			if (!pingOK_)
			{
				MySqlError error(pThreadData_->connection.get());
				ERROR_MSG( "MySQL connection ping failed: %s\n",
							error.what() );
				pThreadData_->connection.onFatalError( error );
			}
			delete this;
		}
	};

public:
	MySqlThreadResPool( WorkerThreadMgr& threadMgr,
						Mercury::Nub& nub,
		                int numConnections,
						int maxSpaceDataSize,
		                const MySql::ConnectionInfo& connInfo,
	                    const EntityDefs& entityDefs );
	virtual ~MySqlThreadResPool();

	int	getNumConnections()	{	return int(threadDataPool_.container.size()) + 1;	}
	MySqlThreadData& getMainThreadData()	{ return mainThreadData_; }
	MySqlThreadData* acquireThreadData( int timeoutMicroSeconds = -1 );
	void releaseThreadData( MySqlThreadData& threadData );

	void startThreadTaskTiming( MySqlThreadData& threadData );
	uint64 stopThreadTaskTiming( MySqlThreadData& threadData );

	double getBusyThreadsMaxElapsedSecs() const;
	double getOpCountPerSec() const;
	double getAvgOpDuration() const;

	WorkerThreadPool& threadPool()	{ return threadPool_; }
	const WorkerThreadPool& getThreadPool() const	{ return threadPool_; }

	// Mercury::TimerExpiryHandler overrides
	virtual int handleTimeout( int id, void * arg );

private:
	MySqlThreadData* acquireThreadDataInternal();
};

/**
 *	Constructor. Start worker threads and connect to MySQL database. Also creates
 *	MySqlThreadData for each thread.
 */
MySqlThreadResPool::MySqlThreadResPool( WorkerThreadMgr& threadMgr,
										Mercury::Nub& nub,
									    int numConnections,
										int maxSpaceDataSize,
								        const MySql::ConnectionInfo& connInfo,
                                        const EntityDefs& entityDefs )
	: threadPool_( threadMgr, numConnections - 1 ), threadDataPool_(),
	freeThreadData_(),
	mainThreadData_( connInfo, maxSpaceDataSize, entityDefs ), nub_( nub ),
	opCount_( 0 ), opDurationTotal_( 0 ), resetTimeStamp_( timestamp() )
{
	int numThreads = threadPool_.getNumFreeThreads();
	MF_ASSERT( (numThreads == 0) || ((numThreads > 0) && mysql_thread_safe()) );

	// Do thread specific MySQL initialisation.
	struct InitMySqlTask : public WorkerThread::ITask
	{
		virtual void run()	{	mysql_thread_init();	}
		virtual void onRunComplete() {}
	} initMySqlTask;
	while ( threadPool_.doTask( initMySqlTask ) ) {}
	threadPool_.waitForAllTasks();

	// Create data structures for use in worker threads.
	threadDataPool_.container.reserve( numThreads );
	for ( int i = 0; i < numThreads; ++i )
	{
		threadDataPool_.container.push_back(
			new MySqlThreadData( connInfo, maxSpaceDataSize, entityDefs ) );
	}

	freeThreadData_ = threadDataPool_.container;

	// Set 30-minute keep alive timer to keep connections alive.
	keepAliveTimerID_ = nub_.registerTimer( 1800000000, this );
}

/**
 *	Mercury::TimerExpiryHandler overrides.
 */
int MySqlThreadResPool::handleTimeout( int id, void * arg )
{
	MF_ASSERT( id == keepAliveTimerID_ );
	// Ping all free threads. Busy threads are already doing something so
	// doesn't need pinging to keep alive.
	ThreadDataVector::size_type numFreeConnections = freeThreadData_.size();
	// TRACE_MSG( "Pinging %lu MySQL connections to keep them alive\n",
	//			numFreeConnections + 1 );	// + 1 for main thread.
	for ( ThreadDataVector::size_type i = 0; i < numFreeConnections; ++i )
	{
		PingTask* pTask = new PingTask( *this );
		this->threadPool().doTask( *pTask );
	}
	if (!mainThreadData_.connection.ping())
		ERROR_MSG( "MySQL connection ping failed: %s\n",
					mainThreadData_.connection.getLastError() );

	return 0;
}

MySqlThreadResPool::~MySqlThreadResPool()
{
	nub_.cancelTimer( keepAliveTimerID_ );

	threadPool_.waitForAllTasks();
	// Do thread specific MySQL clean up.
	struct CleanupMySqlTask : public WorkerThread::ITask
	{
		virtual void run()	{	mysql_thread_end();	}
		virtual void onRunComplete() {}
	} cleanupMySqlTask;
	while ( threadPool_.doTask( cleanupMySqlTask ) ) {}
	threadPool_.waitForAllTasks();
}

/**
 *	Gets a free MySqlThreadData from the pool. The returned MySqlThreadData
 *	is now "busy".
 *
 *	@return	Free MySqlThreadData. 0 if all MySqlThreadData are "busy".
 */
inline MySqlThreadData* MySqlThreadResPool::acquireThreadDataInternal()
{
	MySqlThreadData* pThreadData = 0;
	if (freeThreadData_.size() > 0)
	{
		pThreadData = freeThreadData_.back();
		freeThreadData_.pop_back();
	}
	return pThreadData;
}

/**
 *	Gets a free MySqlThreadData from the pool. MySqlThreadData is now "busy".
 *
 *	@return	Free MySqlThreadData. 0 if all MySqlThreadData are "busy".
 */
MySqlThreadData* MySqlThreadResPool::acquireThreadData(int timeoutMicroSeconds)
{
	MySqlThreadData* pThreadData = this->acquireThreadDataInternal();
	if (!pThreadData && (timeoutMicroSeconds != 0))
	{
		threadPool_.waitForOneTask(timeoutMicroSeconds);
		pThreadData = this->acquireThreadDataInternal();
	}
	return pThreadData;
}

/**
 *	Puts a "busy" MySqlThreadData back into the pool.
 */
void MySqlThreadResPool::releaseThreadData( MySqlThreadData& threadData )
{
	freeThreadData_.push_back( &threadData );
	MF_ASSERT( freeThreadData_.size() <= threadDataPool_.container.size() );
}

/**
 *	This function is called to mark the start of a thread task (represented by
 * 	its	MySqlThreadData).
 */
void MySqlThreadResPool::startThreadTaskTiming( MySqlThreadData& threadData )
{
	threadData.startTimestamp = timestamp();
	// Reset our counter every 5 seconds (to catch transient behaviour)
	if ((threadData.startTimestamp - resetTimeStamp_) > THREAD_TASK_TIMING_RESET_DURATION)
	{
		resetTimeStamp_ = threadData.startTimestamp;
		opDurationTotal_ = 0;
		opCount_ = 0;
	}
}

/**
 *	This function is called to mark the end of a thread task (represented by
 * 	its	MySqlThreadData).
 *
 * 	Returns the duration which this thread task took, in number of CPU
 * 	clock cycles (i.e. same unit as the timestamp() function).
 */
uint64 MySqlThreadResPool::stopThreadTaskTiming( MySqlThreadData& threadData )
{
	uint64	opDuration = timestamp() - threadData.startTimestamp;
	opDurationTotal_ += opDuration;
	++opCount_;
	threadData.startTimestamp = 0;

	return opDuration;
}

/**
 *	Gets the largest elapsed time of any thread that is currently busy
 *	(in seconds)
 */
double MySqlThreadResPool::getBusyThreadsMaxElapsedSecs() const
{
	uint64 maxElapsed = 0;
	uint64 curTimestamp = timestamp();
	for ( ThreadDataVector::const_iterator i = threadDataPool_.container.begin();
			i != threadDataPool_.container.end(); ++i )
	{
		if ((*i)->startTimestamp > 0)	// busy thread
		{
			uint64 elapsedTimestamp = curTimestamp - (*i)->startTimestamp;
		 	if (elapsedTimestamp > maxElapsed)
		 		maxElapsed = elapsedTimestamp;
		}
	}
	return double(maxElapsed)/stampsPerSecondD();
}

/**
 *	Gets the number of operations per second. Only completed operations are
 *  counted.
 */
double MySqlThreadResPool::getOpCountPerSec() const
{
	return double(opCount_)/(double(timestamp() - resetTimeStamp_)/stampsPerSecondD());
}

/**
 *	Gets the average duration of an operation (in seconds). Only completed
 * 	operations are counted.
 */
double MySqlThreadResPool::getAvgOpDuration() const
{
	return (opCount_ > 0)
				? double(opDurationTotal_/opCount_)/stampsPerSecondD() : 0;
}

// -----------------------------------------------------------------------------
// Section: class MySqlThreadTask
// -----------------------------------------------------------------------------
/**
 *	This class is the base class for all MySqlDatabase operations that needs
 *	to be executed in a separate thread. It handles to the acquisition and
 *	release of thread resources.
 */
class MySqlThreadTask :	public WorkerThread::ITask
{
public:
	typedef MySqlDatabase OwnerType;

private:
	MySqlDatabase&		owner_;
	MySqlThreadData*	pThreadData_;
	bool				isTaskReady_;

public:
	MySqlThreadTask( MySqlDatabase& owner )
		: owner_(owner),
		pThreadData_( owner_.getThreadResPool().acquireThreadData(0) ),
		isTaskReady_(true)
	{
		if (!pThreadData_)
		{
			if (owner_.getThreadResPool().getNumConnections() < 3)
			{
				// When there are low number of threads, re-use main thread
				// to do work as well
				pThreadData_ = &owner_.getMainThreadData();
			}
			else
			{
				// When number of threads are high, main thread should be kept
				// free to hand out tasks to worker threads.
				pThreadData_ = owner_.getThreadResPool().acquireThreadData();
				MF_ASSERT( pThreadData_ );
			}
		}
	}

	virtual ~MySqlThreadTask()
	{
		if (pThreadData_->connection.hasFatalError())
		{
			ERROR_MSG( "MySqlDatabase: %s\n",
					pThreadData_->connection.getFatalErrorStr().c_str() );
			owner_.onConnectionFatalError();
		}
		if (!this->shouldRunInMainThread())
		{
			owner_.getThreadResPool().releaseThreadData(*pThreadData_);
		}
	}

	MySqlThreadData& getThreadData()	{	return *pThreadData_;	}
	MySqlDatabase& getOwner()			{	return owner_;	}
	void setTaskReady( bool isReady )	{	isTaskReady_ = isReady;	}
	bool isTaskReady() const			{	return isTaskReady_;	}
	bool shouldRunInMainThread() const	{	return (pThreadData_ == &owner_.getMainThreadData());	}

	void startThreadTaskTiming()
	{	owner_.getThreadResPool().startThreadTaskTiming( this->getThreadData() );	}
	uint64 stopThreadTaskTiming()
	{	return owner_.getThreadResPool().stopThreadTaskTiming( this->getThreadData() );	}

	void doTask();
};

inline void MySqlThreadTask::doTask()
{
	if (isTaskReady_ && !pThreadData_->connection.hasFatalError())
	{
		if (this->shouldRunInMainThread())
		{
			WorkerThreadPool::doTaskInCurrentThread(*this);
		}
		else
		{
			bool isDoing = owner_.getThreadResPool().threadPool().doTask(*this);
			MF_ASSERT( isDoing );
		}
	}
	else
	{
		this->onRunComplete();
	}
}

namespace
{
	uint32 initInfoTable( MySql& connection )
	{
		// Need additional detection of new database because pre-1.7 doesn't
		// have bigworldInfo table.
		std::vector<std::string> tableNames;
		connection.getTableNames( tableNames, "bigworldEntityTypes" );
		bool brandNewDb = (tableNames.size() == 0);

		MySqlTransaction transaction( connection );
		transaction.execute( "CREATE TABLE IF NOT EXISTS bigworldInfo "
				 			"(version INT UNSIGNED NOT NULL) TYPE=InnoDB" );

		MySqlStatement	stmtGetVersion( connection,
										"SELECT version FROM bigworldInfo" );
		uint32			version = 0;
		MySqlBindings	b;
		b << version;
		stmtGetVersion.bindResult( b );

		transaction.execute( stmtGetVersion );
		if (stmtGetVersion.resultRows() > 0)
		{
			stmtGetVersion.fetch();
		}
		else
		{
			// If not new DB then must be old version of DB.
			version = brandNewDb ? DBMGR_CURRENT_VERSION : 0;
			std::stringstream ss;
			ss << "INSERT INTO bigworldInfo (version) VALUES (" <<
					version << ")";
			transaction.execute( ss.str() );
		}

		transaction.commit();

		return version;
	}
}

namespace
{
	/**
	 * 	This class is responsible for performing convertion of entity data between
	 * 	old definitions and new definitions.
	 */
	class EntityConverter
	{
		typedef std::map< EntityTypeID, PyObjectPtr > TypeIDPyObjMap;

		static const char * moduleName_;

		PyObjectPtr 	pModule_;
		TypeIDPyObjMap	pyConverters_;
	public:
		EntityConverter( const EntityDefs& newEntityDefs );

		PyObject* getConvFunc( EntityTypeID newTypeID )
		{
			TypeIDPyObjMap::iterator pConvFunc = pyConverters_.find( newTypeID );
			return (pConvFunc != pyConverters_.end()) ?
						pConvFunc->second.getObject() : NULL;
		}

		void convertBindings( MySqlEntityTypeMapping& newEntity,
			EntityTypeID newTypeID, DatabaseID dbID,
			const MySqlEntityTypeMapping& oldEntity );

	private:
		static std::string getConvFuncName( const EntityDescription& desc )
		{
			return "migrate" + desc.name();
		}
		static PyObject* unloadThenReloadModule( const char * module )
		{
			// No need to unload first now that we create a new interpreter
			// on every update.
			// Script::unloadModule( module );
			return PyImport_ImportModule( const_cast<char *>(module) );
		}

		bool convertBindingsByScript( MySqlEntityTypeMapping& newEntity,
			DatabaseID dbID, const MySqlEntityTypeMapping& oldEntity,
			PyObject* pConvFunc ) const;
		void convertBindingsSimple( MySqlEntityTypeMapping& newEntity,
			const MySqlEntityTypeMapping& oldEntity ) const;
	};

	const char * EntityConverter::moduleName_ = "DbMigration";

	EntityConverter::EntityConverter( const EntityDefs& newEntityDefs )
		: pModule_( unloadThenReloadModule( moduleName_),
					PyObjectPtr::STEAL_REFERENCE )
	{
		if (pModule_)
		{
			// Find the migration method for each entity type
			for ( EntityTypeID type = 0; type < newEntityDefs.getNumEntityTypes(); ++type )
			{
				const EntityDescription& desc = newEntityDefs.getEntityDescription( type );
				std::string migrationMethodName = getConvFuncName( desc );

				PyObject* pFunc = PyObject_GetAttrString( pModule_.getObject(),
								const_cast<char*>(migrationMethodName.c_str()) );
				if (pFunc && PyCallable_Check( pFunc ))
				{
					INFO_MSG( "EntityConverter::EntityConverter: Found "
							"migration function %s\n", migrationMethodName.c_str() );
					pyConverters_[type] = PyObjectPtr( pFunc, PyObjectPtr::STEAL_REFERENCE );
				}
				else
				{
					PyErr_Clear();
				}
			}
		}
		else
		{
			PyErr_Print();
			WARNING_MSG( "EntityConverter::EntityConverter: Failed to load module %s. "
						"No data conversion will be performed.\n", moduleName_ );
		}
	}

	/**
	 *	This method sets the bindings in newEntity from the bindings in oldEntity.
	 */
	void EntityConverter::convertBindings( MySqlEntityTypeMapping& newEntity,
		EntityTypeID newTypeID, DatabaseID dbID,
		const MySqlEntityTypeMapping& oldEntity )
	{
		PyObject* pConvFunc = this->getConvFunc( newTypeID );
		if (!pConvFunc ||
			!convertBindingsByScript( newEntity, dbID, oldEntity, pConvFunc ))
		{
			convertBindingsSimple( newEntity, oldEntity );
		}
	}

	/**
	 *	This method sets the bindings in newEntity from the bindings in oldEntity
	 * 	by calling the user provided migration Python function.
	 */
	bool EntityConverter::convertBindingsByScript( MySqlEntityTypeMapping& newEntity,
		DatabaseID dbID, const MySqlEntityTypeMapping& oldEntity,
		PyObject* pConvFunc ) const
	{
		bool isOK = true;
		// Create dictionary from old bindings.
		PyObjectPtr pDict( PyDict_New(), PyObjectPtr::STEAL_REFERENCE );

		const PropertyMappings& oldProps = oldEntity.getPropertyMappings();
		for ( PropertyMappings::const_iterator i = oldProps.begin();
			i < oldProps.end(); ++i )
		{
			PyObjectPtr pValue( (*i)->createPyObject(), PyObjectPtr::STEAL_REFERENCE );
			PyDict_SetItemString( pDict.getObject(),
				const_cast<char*>((*i)->propName().c_str()), pValue.getObject() );
		}
		// Set "databaseID" property. This should be a read-only property as
		// we don't allow changing the databaseID of an entity.
		{
			PyObjectPtr pValue( PyConv::toPyObject( dbID ),
								PyObjectPtr::STEAL_REFERENCE );
			PyDict_SetItemString( pDict.getObject(), "databaseID",
									pValue.getObject() );
		}

		// Call conversion function.
		PyObjectPtr pArgs( PyTuple_New( 1 ), PyObjectPtr::STEAL_REFERENCE );
		Py_INCREF( pDict.getObject() );
		PyTuple_SetItem( pArgs.getObject(), 0, pDict.getObject() );
		PyObjectPtr pResult( PyObject_Call( pConvFunc, pArgs.getObject(), NULL ),
							PyObjectPtr::STEAL_REFERENCE );
		if (pResult)
		{
			// Set new bindings from dictionary
			PropertyMappings& newProps = newEntity.getPropertyMappings();
			for ( PropertyMappings::const_iterator i = newProps.begin();
				i < newProps.end(); ++i )
			{
				PyObject* pPyValue = PyDict_GetItemString( pDict.getObject(),
					const_cast<char*>((*i)->propName().c_str()) );
				if (pPyValue)
				{
					(*i)->setValue( pPyValue );
				}
				else
				{
					(*i)->nullifyBound();
				}
			}
		}
		else
		{
			std::string migrationMethodName =
				getConvFuncName( newEntity.getEntityDescription() );
			ERROR_MSG( "Data conversion function %s failed\n",
				migrationMethodName.c_str() );
			PyErr_Print();
			isOK = false;
		}

		return isOK;
	}

	/**
	 *	This method sets the bindings in newEntity from the bindings in oldEntity
	 * 	by doing a straight copy or by simple type conversion if types don't match.
	 */
	void EntityConverter::convertBindingsSimple( MySqlEntityTypeMapping& newEntity,
		const MySqlEntityTypeMapping& oldEntity ) const
	{
		PropertyMappings& newProps = newEntity.getPropertyMappings();
		for ( PropertyMappings::const_iterator i = newProps.begin();
			i < newProps.end(); ++i )
		{
			const PropertyMapping* pOldProp =
				oldEntity.getPropMapByName( (*i)->propName() );
			if (pOldProp)
			{
				PyObjectPtr pVal( pOldProp->createPyObject() );
				(*i)->setValue( pVal.getObject() );
			}
			else
			{
				(*i)->nullifyBound();
			}
		}
	}
}	// end of anonymous namespace

// -----------------------------------------------------------------------------
// Section: class MigrateToNewDefsTask
// -----------------------------------------------------------------------------
/**
 *	This class handles the migration of data to a newer version of entity
 * 	definitions.
 */
class MigrateToNewDefsTask : public WorkerThread::ITask,
							public ITemporaryMappingsConverter
{
	enum Step
	{
		StepInit,
		StepCollectTables,
		StepPrepareTablesAndStatements,
		StepGetNextEntityData,	// occurs multiple times
		StepGetCurEntityData,	// occurs multiple times - for fixing up mistakes
		StepPutEntityData,		// occurs multiple times
		StepDeleteCurEntity, 	// occurs multiple times - for fixing up mistakes
		StepCreateRemainingTables,
		StepWaitingForSwitch,
		StepSwitched,
		StepCleanedUp
	};

	typedef std::map< EntityTypeID, std::set<DatabaseID> > TypeToDBIDsMap;

	// Init during construction
	Step				step_;
	bool				abortMigration_;
	MySqlDatabase&		owner_;
	IDatabase::IMigrationHandler& handler_;
	const EntityDefs& 	newEntityDefs_;
	MySql::ConnectionInfo connectionInfo_;
	MySql* 				pConnection_;
	WorkerThread		thread_;
	bool				paused_;
	bool				isTaskRunning_;
	PyThreadState* 		pInterpreter_;
	EntityConverter		converter_;
	// Init during StepCollectTables
	TypeMappings		newPropMappings_;
	TableMigrationData	tableMigrationData_;
	SimpleTableCollector changedEntitiesTables_;
	TypeIDTypeIDMap		changedEntitiesTypeMap_;
	TypeIDSet			changedEntitiesOldTypeID_;
	// Init during StepPrepareTablesAndStatements
	TypeMappings		oldPropMappings_;
	MySqlEntityTypeMappings oldEntityMappings_;
	MySqlEntityTypeMappings newEntityMappings_;
	TypeIDTypeIDMap::const_iterator	curEntityType_;
	DatabaseID			curEntityDBID_;
	DatabaseID			nextEntityDBID_;
	// Used during data migration i.e. StepGetNextEntityData, StepPutEntityData
	bool				inCollision_;
	TypeToDBIDsMap		updatingEntities_;
	TypeToDBIDsMap		updatedEntities_;
	TypeToDBIDsMap		deletingEntities_;
	TypeToDBIDsMap		deletedEntities_;
	// Init during StepCreateRemainingTables
	TableMigrationData::NewTableDataMap remainingTables_;

	std::string			exceptionStr_;

	// Delays used for debugging.
	int					initDelay_;
	int					collectTablesDelay_;
	int					prepareTablesAndStatementsDelay_;
	int					getNextEntityDataDelay_;
	int					getCurEntityDataDelay_;
	int					putEntityDataDelay_;
	int					deleteCurEntityDelay_;
	int					createRemainingTablesDelay_;

public:
	MigrateToNewDefsTask( const EntityDefs& newEntityDefs, MySqlDatabase& owner,
		IDatabase::IMigrationHandler& handler );
	virtual ~MigrateToNewDefsTask();

	const EntityDefs& getNewEntityDefs() const	{ 	return newEntityDefs_;	}
	// This function returns the Python interpreter used for migration. But
	// after switchToNewDefs() is called, this will return the ex-main
	// interpreter.
	PyThreadState*	getPyInterpreter()			{	return pInterpreter_;	}
	const TypeIDSet& getChangedEntitiesOldTypes() const
	{ return changedEntitiesOldTypeID_;	}

	MySqlDatabase& getOwner()	{	return owner_;	}
	const MySql::ConnectionInfo& getConnectionInfo() const
	{	return connectionInfo_;	}
	MySql& getConnection()		{	return *pConnection_;	}
	WorkerThread& getThread()	{	return thread_;	}

	void startMigration();
	bool waitForMigrationCompletion();
	void switchToNewDefs();
	void cleanUpTables();
	bool restoreConnectionToDb();

	bool shouldAllowExecRawCmd( const std::string& command );

	void onUpdateEntity( EntityTypeID typeID, DatabaseID dbID );
	void onUpdateEntityComplete( EntityTypeID typeID, DatabaseID dbID );
	void onDeleteEntity( EntityTypeID typeID, DatabaseID dbID );
	void onDeleteEntityComplete( EntityTypeID typeID, DatabaseID dbID );

	// WorkerThread::ITask overrides
	virtual void run();
	virtual void onRunComplete();

	// ITemporaryMappingsConverter overrides
	virtual EntityTypeID getTempTypeID( EntityTypeID oldTypeID );
	virtual void setTempMapping( MySqlEntityTypeMapping& tempMapping,
		EntityTypeID oldTypeID, DatabaseID dbID,
		const MySqlEntityTypeMapping& oldMapping );

private:
	void doStepInThread( Step step );
	void waitForStepCompletion();
	void threadInit();
	void onThreadInitComplete();
	void collectTables();
	void onCollectTablesComplete();
	void prepareTablesAndStatements();
	void onPrepareTablesAndStatementsComplete();
	void getNextEntityData();
	void getCurEntityData();
	void onGetEntityDataComplete();
	void putEntityData();
	void onPutEntityDataComplete();
	void deleteEntity();
	void onDeleteEntityComplete();
	void createRemainingTables();
	void onCreateRemainingTablesComplete();

	bool connectToDb();
	void createOldEntityMappings();
	void createNewEntityMappings();
	void setupUpdateMigrationTables();

	void addToLog( TypeToDBIDsMap& log, EntityTypeID typeID, DatabaseID dbID );
	static void moveToLog( TypeToDBIDsMap& destLog, TypeToDBIDsMap& srcLog,
		EntityTypeID typeID, DatabaseID dbID );
	static bool isInLog( TypeToDBIDsMap& log, EntityTypeID typeID,
		DatabaseID dbID );
	void clearCompletedLogs();
};

MigrateToNewDefsTask::MigrateToNewDefsTask( const EntityDefs& newEntityDefs,
		MySqlDatabase& owner, IDatabase::IMigrationHandler& handler ) :
	step_(StepInit), abortMigration_( false ), owner_( owner ),
	handler_(handler), newEntityDefs_( newEntityDefs ),
	connectionInfo_(),
	pConnection_( NULL ),
	thread_( Database::instance().getWorkerThreadMgr() ), paused_(false),
	isTaskRunning_(false), pInterpreter_( Script::createInterpreter() ),
	converter_(SwapInterpreterInitialiserHack<const EntityDefs&>(
					newEntityDefs_, pInterpreter_ )),
	newPropMappings_(), tableMigrationData_(),
	changedEntitiesTables_(), changedEntitiesTypeMap_(),
	oldPropMappings_(), oldEntityMappings_(), newEntityMappings_(),
	curEntityType_( changedEntitiesTypeMap_.begin() ),
	curEntityDBID_( 0 ), nextEntityDBID_( 0 ), inCollision_(false),
	exceptionStr_(),
	initDelay_( 0 ), collectTablesDelay_( 0 ),
	prepareTablesAndStatementsDelay_( 0 ), getNextEntityDataDelay_( 0 ),
	getCurEntityDataDelay_( 0 ), putEntityDataDelay_( 0 ),
	deleteCurEntityDelay_( 0 ), createRemainingTablesDelay_( 0 )
{
	this->connectToDb();

	BWConfig::update( "dbMgr/debug/migration/initDelay", initDelay_ );
	BWConfig::update( "dbMgr/debug/migration/collectTablesDelay", collectTablesDelay_ );
	BWConfig::update( "dbMgr/debug/migration/prepareTablesAndStatementsDelay", prepareTablesAndStatementsDelay_ );
	BWConfig::update( "dbMgr/debug/migration/getNextEntityDataDelay", getNextEntityDataDelay_ );
	BWConfig::update( "dbMgr/debug/migration/getCurEntityDataDelay", getCurEntityDataDelay_ );
	BWConfig::update( "dbMgr/debug/migration/putEntityDataDelay", putEntityDataDelay_ );
	BWConfig::update( "dbMgr/debug/migration/deleteCurEntityDelay", deleteCurEntityDelay_ );
	BWConfig::update( "dbMgr/debug/migration/createRemainingTablesDelay", createRemainingTablesDelay_ );
}

bool MigrateToNewDefsTask::connectToDb()
{
	bool isOK = true;
	try
	{
		MySql* pConnection = new MySql( connectionInfo_ );
		delete pConnection_;
		pConnection_ = pConnection;
	}
	catch (std::exception& e)
	{
		ERROR_MSG( "MySqlDatabase: Data migration halted: %s\n", e.what() );
		isOK = false;
	}

	return isOK;
}

MigrateToNewDefsTask::~MigrateToNewDefsTask()
{
	INFO_MSG( "MigrateToNewDefsTask::~MigrateToNewDefsTask\n" );

	if (isTaskRunning_)
		this->waitForStepCompletion();

	if (step_ != StepCleanedUp)
	{
		INFO_MSG( "MigrateToNewDefsTask::~MigrateToNewDefsTask: Cleaning up "
					"after aborted update process at step %d\n", int(step_) );

		MySqlTransaction transaction( this->getConnection() );
		switch (step_)
		{
			case StepInit:
			case StepCollectTables:
			case StepCleanedUp:
				// Nothing to clean up.
				break;
			case StepCreateRemainingTables:
			case StepWaitingForSwitch:
				try
				{
					// Try to remove "remaining tables".
					removeTempEntityTables( transaction, remainingTables_ );
				}
				catch (std::exception& e)
				{
					ERROR_MSG( "MigrateToNewDefsTask::~MigrateToNewDefsTask: %s\n",
								e.what() );
				}
				// Fallthrough and clean up temporary tables.
			case StepPrepareTablesAndStatements:
			case StepGetNextEntityData:
			case StepGetCurEntityData:
			case StepPutEntityData:
			case StepDeleteCurEntity:
				// Try to delete temporary tables;
				try
				{
					removeTempEntityTables( transaction,
						changedEntitiesTables_.getTables() );
				}
				catch (std::exception& e)
				{
					ERROR_MSG( "MigrateToNewDefsTask::~MigrateToNewDefsTask: %s\n",
								e.what() );
				}
				break;
			case StepSwitched:
				// Probably shouldn't try to remove "old" tables since they
				// contain all original the data. Oh well, don't handle
				// this case very well.
#ifdef DBMGR_REMOVE_OLD_TABLES_ANYWAY
					WARNING_MSG( "MigrateToNewDefsTask::~MigrateToNewDefsTask: "
						"Removing old tables anyway... Say goodbye to your data!\n" );
					try
					{
						removeSwappedOutTables( transaction,
							changedEntitiesTables_.getTables(),
							tableMigrationData_.newTables,
							tableMigrationData_.obsoleteTables );
					}
					catch (std::exception& e)
					{
						ERROR_MSG( "MigrateToNewDefsTask::~MigrateToNewDefsTask: %s\n",
									e.what() );
					}
#else
					WARNING_MSG( "MigrateToNewDefsTask::~MigrateToNewDefsTask: "
						"Clean up aborted due to possibility of data loss. "
						"Manual clean up required\n" );
#endif
				break;
			default:
				MF_ASSERT(false);
				break;
		}

		transaction.commit();
	}

	struct ThreadCleanUpTask : public WorkerThread::ITask
	{
		bool done;

		ThreadCleanUpTask() : done( false ) {}

		virtual void run()				{	mysql_thread_end();	}
		virtual void onRunComplete() 	{	done = true;	}
	};
	ThreadCleanUpTask	cleanupTask;
	thread_.doTask( cleanupTask );
	do
	{
		Database::instance().getWorkerThreadMgr().waitForTaskCompletion( 1 );
	} while (!cleanupTask.done);

	destroyEntityPropMappings( oldEntityMappings_ );
	destroyEntityPropMappings( newEntityMappings_ );

	Script::destroyInterpreter( pInterpreter_ );
}

void MigrateToNewDefsTask::startMigration()
{
	this->doStepInThread( StepInit );	// Calls threadInit() in a different thread.
}

/**
 *	WorkerThread::ITask override
 */
void MigrateToNewDefsTask::doStepInThread( Step step )
{
	step_ = step;
	if (!paused_)
	{
		DEBUG_MSG( "MigrateToNewDefsTask::doStepInThread: step=%d\n", int(step) );

		isTaskRunning_ = true;
		thread_.doTask( *this );	// Calls run() in a different thread.
	}
}

/**
 *	The method will block until the current step (executed in a different
 * 	thread) finishes processing. It also prevents the current step from
 * 	starting the next step.
 */
void MigrateToNewDefsTask::waitForStepCompletion()
{
	paused_ = true;

	while (isTaskRunning_)
		Database::instance().getWorkerThreadMgr().waitForTaskCompletion( 1 );

	paused_ = false;
}

/**
 *	WorkerThread::ITask override
 */
void MigrateToNewDefsTask::run()
{
	switch (step_)
	{
		case StepInit:
			if (initDelay_ > 0)
				threadSleep( initDelay_ * 1000000 );
			this->threadInit();
			break;
		case StepCollectTables:
			if (collectTablesDelay_ > 0)
				threadSleep( collectTablesDelay_ * 1000000 );
			this->collectTables();
			break;
		case StepPrepareTablesAndStatements:
			if (prepareTablesAndStatementsDelay_ > 0)
				threadSleep( prepareTablesAndStatementsDelay_ * 1000000 );
			this->prepareTablesAndStatements();
			break;
		case StepGetNextEntityData:
			if (getNextEntityDataDelay_ > 0)
				threadSleep( getNextEntityDataDelay_ * 1000000 );
			this->getNextEntityData();
			break;
		case StepGetCurEntityData:
			if (getCurEntityDataDelay_ > 0)
				threadSleep( getCurEntityDataDelay_ * 1000000 );
			this->getCurEntityData();
			break;
		case StepPutEntityData:
			if (putEntityDataDelay_ > 0)
				threadSleep( putEntityDataDelay_ * 1000000 );
			this->putEntityData();
			break;
		case StepDeleteCurEntity:
			if (deleteCurEntityDelay_ > 0)
				threadSleep( deleteCurEntityDelay_ * 1000000 );
			this->deleteEntity();
			break;
		case StepCreateRemainingTables:
			if (createRemainingTablesDelay_ > 0)
				threadSleep( createRemainingTablesDelay_ * 1000000 );
			this->createRemainingTables();
			break;
		default:
			MF_ASSERT( false );
			break;
	}
}

/**
 *	WorkerThread::ITask override
 */
void MigrateToNewDefsTask::onRunComplete()
{
	DEBUG_MSG( "MigrateToNewDefsTask::onRunComplete: step=%d\n", int(step_) );
	isTaskRunning_ = false;

	if (!pConnection_ || pConnection_->hasFatalError())
	{

		ERROR_MSG( "MySqlDatabase: Data migration halted: %s\n",
					this->getConnection().getFatalErrorStr().c_str() );
		// Almost always the same error as the fatal error. Need to clear this
		// to restart migration.
		exceptionStr_.clear();
		return;
		// restoreConnectionToDb() will be called when the database comes back.
	}

	switch (step_)
	{
		case StepInit:
			this->onThreadInitComplete();
			break;
		case StepCollectTables:
			this->onCollectTablesComplete();
			break;
		case StepPrepareTablesAndStatements:
			this->onPrepareTablesAndStatementsComplete();
			break;
		case StepGetNextEntityData:
			this->onGetEntityDataComplete();
			break;
		case StepGetCurEntityData:
			this->onGetEntityDataComplete();
			break;
		case StepPutEntityData:
			this->onPutEntityDataComplete();
			break;
		case StepDeleteCurEntity:
			this->onDeleteEntityComplete();
			break;
		case StepCreateRemainingTables:
			this->onCreateRemainingTablesComplete();
			break;
		default:
			MF_ASSERT( false );
			break;
	}
}

// ITemporaryMappingsConverter overrides
EntityTypeID MigrateToNewDefsTask::getTempTypeID( EntityTypeID oldTypeID )
{
	TypeIDTypeIDMap::const_iterator i =
		changedEntitiesTypeMap_.find( oldTypeID );
	return ( i != changedEntitiesTypeMap_.end() ) ? i->second : INVALID_TYPEID;
}

void MigrateToNewDefsTask::setTempMapping( MySqlEntityTypeMapping& tempMapping,
	EntityTypeID oldTypeID, DatabaseID dbID,
	const MySqlEntityTypeMapping& oldMapping )
{
	if (step_ < StepSwitched)
	{
		// convertBindings uses migration scripts which are only loaded in the
		// new interpreter.
		Script::AutoInterpreterSwapper interpreterSwapper( pInterpreter_ );

		EntityTypeID newTypeID = getTempTypeID( oldTypeID );
		converter_.convertBindings( tempMapping, newTypeID, dbID, oldMapping );
	}
	else
	{
		// After switching, no need to swap interpreters since the default one
		// is the one with the migration scripts.
		EntityTypeID newTypeID = getTempTypeID( oldTypeID );
		converter_.convertBindings( tempMapping, newTypeID, dbID, oldMapping );
	}
}

/**
 * 	This method does thread specific initialisation for the data migration
 * 	thread.
 */
void MigrateToNewDefsTask::threadInit()
{
	mysql_thread_init();
	// Calls onThreadInitComplete() in main thread.
}

/**
 * 	This method called in the main thread after threadInit() completes.
 */
void MigrateToNewDefsTask::onThreadInitComplete()
{
	{	// createEntityPropMappings may call user data type scripts.
		Script::AutoInterpreterSwapper interpreterSwapper( pInterpreter_ );

		createEntityPropMappings( newPropMappings_, newEntityDefs_,
			TEMP_TABLE_NAME_PREFIX );
	}
	// Calls collectTables() in a different thread.
	this->doStepInThread( StepCollectTables );
}

/**
 *	This method collects all the tables required for the new entity
 * 	definitions.
 */
void MigrateToNewDefsTask::collectTables()
{
	try
	{
		MySqlTransaction	transaction( this->getConnection() );
		BigWorldMetaData	metaData( transaction );
		TableCollector		tableCollector( tableMigrationData_, metaData );
		const EntityDefs&	oldEntityDefs = Database::instance().getEntityDefs();

		for ( EntityTypeID ent = 0; ent < newEntityDefs_.getNumEntityTypes(); ++ent )
		{
			if (!newEntityDefs_.isValidEntityType( ent ))
				continue;
			PropertyMappings& properties = newPropMappings_[ent];
			const EntityDescription& entDes = newEntityDefs_.getEntityDescription(ent);
			std::string tblName = TEMP_TABLE_NAME_PREFIX"_" + entDes.name();
			ITableCollector::NameToColInfoMap cols;
			ITableCollector::NameToColInfoMap cols2;
			size_t	numUpdatedTables = tableMigrationData_.getNumUpdatedTables();
			SimpleTableCollector entityTables; // Tables for this entity only

			for ( PropertyMappings::iterator prop = properties.begin();
					prop != properties.end(); ++prop )
			{
				// This doesn't actually create any tables. Just tells us what
				// tables needs creating.
				(*prop)->createTables( tableCollector, cols );
				(*prop)->createTables( entityTables, cols2 );
			}

			tableCollector.addType( ent, entDes.name() );

			ITableCollector::ColumnInfo& idCol = cols[ ID_COLUMN_NAME ];
			idCol.typeStr = ID_COLUMN_TYPE_NAME;
			idCol.indexType = ITableCollector::IndexTypePrimary;

			tableCollector.requireTable( tblName, cols );
			entityTables.requireTable( tblName, cols ); // Not col2 deliberate

			// Need to migrate if entity table has changed or it has a migration
			// function.
			if ((tableMigrationData_.getNumUpdatedTables() > numUpdatedTables) ||
				converter_.getConvFunc(ent))
			{
				// And it must also exist prior to migration. This is protect
				// against the case where a migration function was provided
				// for an entity type that only exists in the new defs.
				EntityTypeID oldTypeID =
					oldEntityDefs.getEntityType(entDes.name());
				if (oldTypeID != INVALID_TYPEID)
				{
					// Entity needs migration.
					TRACE_MSG( "MigrateToNewDefsTask::collectTables: Entity type %s "
								"needs migration\n", entDes.name().c_str() );
					changedEntitiesTypeMap_[oldTypeID] = ent;
					changedEntitiesOldTypeID_.insert( oldTypeID );
					changedEntitiesTables_ += entityTables;
				}
			}
		}
		transaction.commit();
	}
	catch (std::exception& e)
	{
		exceptionStr_ = e.what();
	}
	// Calls onCollectTablesComplete() in main thread.
}

/**
 *	This method is called after collectTables() completes.
 */
void MigrateToNewDefsTask::onCollectTablesComplete()
{
	if (exceptionStr_.length() > 0)
	{
		ERROR_MSG( "MigrateToNewDefsTask::onCollectTablesComplete: %s\n",
					exceptionStr_.c_str() );
		exceptionStr_.clear();
		// TODO: Stop the migration process somehow.
		return;
	}

	if (tableMigrationData_.maxRequiredSyncMode <=
		Database::instance().getDefsSyncMode())
	{	// DefsSyncMode OK
		if (changedEntitiesTypeMap_.size() > 0)
		{
			// Create entity mappings for changed entity types.
			this->createOldEntityMappings();

			// Calls prepareTablesAndStatements() in a different thread.
			this->doStepInThread( StepPrepareTablesAndStatements );
		}
		else
		{	// Skip data migration step
			// Calls createRemainingTables() in a different thread.
			this->doStepInThread( StepCreateRemainingTables );
		}
	}
	else
	{	// DefsSyncMode not high enough for this operation
		ERROR_MSG( "MySqlDatabase::migrateToNewDefs: Tables need to be "
					"updated but syncTablesToDefs is false\n" );
//		ERROR_MSG( "MySqlDatabase::migrateToNewDefs: Require defsSyncMode=%d, "
//					"currently defsSyncMode=%d\n",
//					int(tableMigrationData_.maxRequiredSyncMode),
//					int(Database::instance().getDefsSyncMode()) );
		abortMigration_ = true;
		handler_.onMigrateToNewDefsComplete( false );
	}
}

/**
 *	This method sets the entity type mappings for reading from existing
 * 	data tables - but only for those entities that need migration.
 */
void MigrateToNewDefsTask::createOldEntityMappings()
{
	// Create entity mappings for changed entity types.
	createEntityMappings( oldEntityMappings_,
		Database::instance().getEntityDefs(), TABLE_NAME_PREFIX,
		this->getConnection(), &changedEntitiesOldTypeID_ );
}

/**
 *	This method prepares for data migration.
 */
void MigrateToNewDefsTask::prepareTablesAndStatements()
{
	MF_ASSERT( changedEntitiesTypeMap_.size() > 0 );

	// Create temporary migration tables.
	try
	{
		MySqlTransaction	transaction( this->getConnection() );
		createTempEntityTables(transaction, changedEntitiesTables_.getTables());
		transaction.commit();
	}
	catch (std::exception& e)
	{
		exceptionStr_ = e.what();
	}

	if (!this->getConnection().hasFatalError())
		// onRunComplete() will pick up the fatal error and restart this
		// operation.
		this->createNewEntityMappings();

	curEntityType_ = changedEntitiesTypeMap_.begin();
	curEntityDBID_ = 0;
	// Calls onStartMigrationComplete() in the main thread.
}

/**
 *	This method sets the entity type mappings for the migration tables.
 */
void MigrateToNewDefsTask::createNewEntityMappings()
{
	// Build up set of changed entity types.
	TypeIDSet changedTypes;
	for ( TypeIDTypeIDMap::const_iterator i = changedEntitiesTypeMap_.begin();
		i != changedEntitiesTypeMap_.end(); ++i )
	{
		changedTypes.insert( i->second );
	}
	// Create entity mappings for use on migration tables.
	createEntityMappings( newEntityMappings_, newPropMappings_, newEntityDefs_,
		TEMP_TABLE_NAME_PREFIX, this->getConnection(), &changedTypes );
}

/**
 *	This method is called in the main thread after startMigration() completes.
 */
void MigrateToNewDefsTask::onPrepareTablesAndStatementsComplete()
{
	if (exceptionStr_.length() == 0)
	{
		this->setupUpdateMigrationTables();

		// Calls getNextEntityData() in a different thread.
		this->doStepInThread( StepGetNextEntityData );
	}
	else
	{
		ERROR_MSG( "MigrateToNewDefsTask::onPrepareTablesAndStatementsComplete: "
					"%s\n", exceptionStr_.c_str() );
		exceptionStr_.clear();
	}
}


/**
 *	This method sets up the main processing threads to also update the migration
 * 	tables if the entity they are adding/updating/deleting is one of the
 * 	entities requiring migration.
 */
void MigrateToNewDefsTask::setupUpdateMigrationTables()
{
	// setupUpdateTemporaryTables() may call user data type scripts.
	Script::AutoInterpreterSwapper interpreterSwapper( pInterpreter_ );

	owner_.setupUpdateTemporaryTables();
}


/**
 * 	This method gets the "next" entity from the old table.
 */
void MigrateToNewDefsTask::getNextEntityData()
{
	try
	{
		MySqlTransaction	transaction( this->getConnection() );
		nextEntityDBID_ = oldEntityMappings_[curEntityType_->first]->
							getNextPropsByID( transaction, curEntityDBID_ );
		transaction.commit();
	}
	catch (std::exception& e)
	{
		exceptionStr_ = e.what();
	}
	// Calls onGetEntityDataComplete() in the main thread.
}

/**
 * 	This method re-gets the current entity from the old table.
 */
void MigrateToNewDefsTask::getCurEntityData()
{
	try
	{
		MySqlTransaction	transaction( this->getConnection() );
		std::string			name;
		nextEntityDBID_ = curEntityDBID_;
		oldEntityMappings_[curEntityType_->first]->
							getPropsByID( transaction, curEntityDBID_, name );
		transaction.commit();
	}
	catch (std::exception& e)
	{
		exceptionStr_ = e.what();
	}
	// Calls onGetEntityDataComplete() in the main thread.
}

/**
 *	This method is called in the main thread after getNextEntityData() or
 * 	getCurEntityData() completes.
 */
void MigrateToNewDefsTask::onGetEntityDataComplete()
{
	if (exceptionStr_.length() == 0)
	{
		if (nextEntityDBID_ != 0)
		{
			{	// convertBindings uses migration scripts which are only loaded
				// in the new interpreter.
				Script::AutoInterpreterSwapper interpSwapper( pInterpreter_ );

				curEntityDBID_ = nextEntityDBID_;
				converter_.convertBindings(
					*newEntityMappings_[curEntityType_->second],
					curEntityType_->second, curEntityDBID_,
					*oldEntityMappings_[curEntityType_->first] );
			}
			// Calls putEntityData() in a different thread.
			this->doStepInThread( StepPutEntityData );
		}
		else
		{	// Run out of entities for current type. Try next type.
			++curEntityType_;
			curEntityDBID_ = 0;
			if (curEntityType_ != changedEntitiesTypeMap_.end())
			{
				// Calls getEntityData() in a different thread.
				this->doStepInThread( StepGetNextEntityData );
			}
			else
			{
				// Calls createRemainingTables() in a different thread.
				this->doStepInThread( StepCreateRemainingTables );
			}
		}
	}
	else
	{
		// __kyl__ (23/9/2004) Got an error! Not sure what to do here.
		// Hopefully incrementing the DBID will prevent us
		// from going in an infinite loop. But since it's a 64-bit number, it's
		// pretty much close to infinite.
		ERROR_MSG( "MigrateToNewDefsTask::getEntityData: Failed to retrieve "
			"entity data for entity %"FMT_DBID" of type %d: %s\n",
			curEntityDBID_, curEntityType_->first, exceptionStr_.c_str() );
		exceptionStr_.clear();
		++curEntityDBID_;
		this->doStepInThread( step_ );
	}
}

/**
 * 	This method puts a single entity into the new table.
 */
void MigrateToNewDefsTask::putEntityData()
{
	bool retry;
	do
	{
		retry = false;
		try
		{
			MySqlTransaction	transaction( this->getConnection() );
			newEntityMappings_[curEntityType_->second]->
					insertNewWithID( transaction, curEntityDBID_ );
			transaction.commit();
		}
		catch (MySqlRetryTransactionException& e)
		{
			retry = true;
		}
		catch (std::exception& e)
		{
			exceptionStr_ = e.what();
		}
	} while (retry);
	// Calls onPutEntityDataComplete() in the main thread.
}

/**
 *	This function is called in the main thread after putEntityData() completes.
 */
void MigrateToNewDefsTask::onPutEntityDataComplete()
{
	DEBUG_MSG( "MigrateToNewDefsTask::onPutEntityDataComplete: typeID=%d, "
				"dbID = %"FMT_DBID"\n", curEntityType_->first, curEntityDBID_ );

	if (exceptionStr_.length() > 0)
	{
		ERROR_MSG( "MigrateToNewDefsTask::putEntityData: Failed to put entity "
				"%"FMT_DBID" of type %d: %s\n", curEntityDBID_,
				curEntityType_->first, exceptionStr_.c_str() );
		exceptionStr_.clear();
	}

	// Check whether the entity we've just finished migrating was
	// updated/deleted while we were doing the migration.
	EntityTypeID 	typeID = curEntityType_->first;
	DatabaseID		dbID = curEntityDBID_;
	if (this->isInLog( updatingEntities_, typeID, dbID ))
	{
		DEBUG_MSG( "MigrateToNewDefsTask::onPutEntityDataComplete: "
					"Wait and retransfer\n" );
		// Wait for update task to complete before re-transfering entity.
		step_ = StepGetCurEntityData;
		inCollision_ = true;
	}
	else if (this->isInLog( updatedEntities_, typeID, dbID ))
	{
		DEBUG_MSG( "MigrateToNewDefsTask::onPutEntityDataComplete: "
					"Retransfer\n" );
		// Retransfer entity.
		this->doStepInThread( StepGetCurEntityData );
	}
	else if (this->isInLog( deletingEntities_, typeID, dbID ) ||
				this->isInLog( deletedEntities_, typeID, dbID ))
	{
		DEBUG_MSG( "MigrateToNewDefsTask::onPutEntityDataComplete: "
					"Delete\n" );
		// Attempt to delete the entity just in case we wrote the entity into
		// the new table just after the other thread tries to delete it.
		this->doStepInThread( StepDeleteCurEntity );
	}
	else
	{
		// Calls getNextEntityData() in a different thread.
		this->doStepInThread( StepGetNextEntityData );
	}
	this->clearCompletedLogs();
}

/**
 *	This method deletes the current entity from the new table.
 */
void MigrateToNewDefsTask::deleteEntity()
{
	bool retry;
	do
	{
		retry = false;
		try
		{
			MySqlTransaction	transaction( this->getConnection() );
			newEntityMappings_[curEntityType_->second]->
					deleteWithID( transaction, curEntityDBID_ );
			transaction.commit();
		}
		catch (MySqlRetryTransactionException& e)
		{
			retry = true;
		}
		catch (std::exception& e)
		{
			exceptionStr_ = e.what();
		}
	} while (retry);
}

/**
 *	This method is called in the main thread after deleteEntity() completes.
 */
void MigrateToNewDefsTask::onDeleteEntityComplete()
{
	// Delete can potentially fail due to race condition between migration task
	// and normal tasks i.e. the normal task could've deleted it before us. So
	// we ignore the failure
	exceptionStr_.clear();

	// Calls getNextEntityData() in a different thread.
	this->doStepInThread( StepGetNextEntityData );
}

/**
 *	This method is creates the tables (other than those required for migration)
 *  required by the new entity definitions.
 */
void MigrateToNewDefsTask::createRemainingTables()
{
	try
	{
		MySqlTransaction	transaction( this->getConnection() );
		// Create new tables that are not part of the migration set.
		const TableMigrationData::NewTableDataMap& newTables =
			tableMigrationData_.newTables;
		const TableMigrationData::NewTableDataMap& migrationTables =
			changedEntitiesTables_.getTables();
		for ( TableMigrationData::NewTableDataMap::const_iterator i =
				newTables.begin(); i != newTables.end(); ++i )
		{
			if ( migrationTables.find( i->first ) == migrationTables.end() )
			{
				remainingTables_.insert( *i );
			}
		}
		createTempEntityTables( transaction, remainingTables_ );
		transaction.commit();
	}
	catch (std::exception& e)
	{
		exceptionStr_ = e.what();
	}
	// Calls onCreateRemainingTablesComplete() in the main thread.
}

/**
 *	This method is called in the main thread after createRemainingTables()
 * 	completes.
 */
void MigrateToNewDefsTask::onCreateRemainingTablesComplete()
{
	if (exceptionStr_.length() == 0)
	{
		step_ = StepWaitingForSwitch;
		handler_.onMigrateToNewDefsComplete( true );
	}
	else
	{
		ERROR_MSG( "MigrateToNewDefsTask::onCreateRemainingTablesComplete: %s\n",
					exceptionStr_.c_str() );
		exceptionStr_.clear();
	}
}

/**
 *	This method will block until we are ready to switch to the new defs.
 */
bool MigrateToNewDefsTask::waitForMigrationCompletion()
{
	while ( (step_ != StepWaitingForSwitch) && (!abortMigration_) )
	{
		Database::instance().getWorkerThreadMgr().waitForTaskCompletion( 1 );
	}

	return ((step_ == StepWaitingForSwitch) && (!abortMigration_));
}

/**
 *	This method is called to switch over to the new defs.
 */
void MigrateToNewDefsTask::switchToNewDefs()
{
	try
	{
		MySqlTransaction	transaction( this->getConnection() );

		swapTempTablesWithReal( transaction, changedEntitiesTables_.getTables(),
			tableMigrationData_.updatedTables, tableMigrationData_.newTables,
			tableMigrationData_.obsoleteTables );
		BigWorldMetaData::updateEntityTypes( transaction,
			tableMigrationData_.obsoleteTypes, tableMigrationData_.newTypes,
			tableMigrationData_.updatedTypes );

		transaction.commit();

		// Permanently set main interpreter to the migration interpreter,
		// which contains the correct user type modules.
		pInterpreter_ = Script::swapInterpreter( pInterpreter_ );

		step_ = StepSwitched;
	}
	catch (std::exception& e)
	{
		ERROR_MSG( "MigrateToNewDefsTask::switchToNewDefs: %s\n",
					e.what() );
	}
}

/**
 *	This method is called to clean-up all the temporary tables, as well as
 *	tables not needed by the new entity defs.
 */
void MigrateToNewDefsTask::cleanUpTables()
{
	try
	{
		MySqlTransaction	transaction( this->getConnection() );
		removeSwappedOutTables( transaction, changedEntitiesTables_.getTables(),
			tableMigrationData_.newTables, tableMigrationData_.obsoleteTables );
		transaction.commit();

		step_ = StepCleanedUp;
	}
	catch (std::exception& e)
	{
		ERROR_MSG( "MigrateToNewDefsTask::cleanUpTables: %s\n", e.what() );
	}
}

/**
 *	This method is called so that we reconnect to the database.
 */
bool MigrateToNewDefsTask::restoreConnectionToDb()
{
	bool isOK = this->connectToDb();
	if (isOK)
	{
		INFO_MSG( "MySqlDatabase: Data migration restarting\n" );
		// Restart migration process.
		switch (step_)
		{
			case StepInit:
				this->onRunComplete();
				break;
			case StepCollectTables:
				tableMigrationData_.init();
				this->doStepInThread( step_ );
				break;
			case StepPrepareTablesAndStatements:
				destroyEntityPropMappings( oldEntityMappings_ );
				this->createOldEntityMappings();
				destroyEntityPropMappings( newEntityMappings_ );
				this->doStepInThread( step_ );
				break;
			case StepGetNextEntityData:
			case StepGetCurEntityData:
			case StepPutEntityData:
				destroyEntityPropMappings( oldEntityMappings_ );
				this->createOldEntityMappings();
				destroyEntityPropMappings( newEntityMappings_ );
				this->createNewEntityMappings();
				this->setupUpdateMigrationTables();
				this->doStepInThread( StepGetCurEntityData );
				break;
			case StepDeleteCurEntity:
				destroyEntityPropMappings( oldEntityMappings_ );
				this->createOldEntityMappings();
				destroyEntityPropMappings( newEntityMappings_ );
				this->createNewEntityMappings();
				this->setupUpdateMigrationTables();
				this->doStepInThread( step_ );
				break;
			case StepCreateRemainingTables:
				this->setupUpdateMigrationTables();
				this->doStepInThread( step_ );
				break;
			case StepWaitingForSwitch:
				this->setupUpdateMigrationTables();
				break;
			case StepSwitched:
			case StepCleanedUp:
				break;
		}
	}

	return isOK;
}

/**
 *	This method returns true if executeRawCommand() should be allowed to
 * 	proceed.
 */
bool MigrateToNewDefsTask::shouldAllowExecRawCmd( const std::string& command )
{
	// TODO: Probably should have a better criteria for allowing/disallowing
	// raw commands.
	return (step_ <= StepPrepareTablesAndStatements);
}

/**
 *	This method adds an entry into the given log if and operation on
 * 	that particular entity in the database can potentially clash with the
 * 	current migration activity.
 */
void MigrateToNewDefsTask::addToLog( TypeToDBIDsMap& log,
	EntityTypeID typeID, DatabaseID dbID )
{
	if (curEntityType_ != changedEntitiesTypeMap_.end())
	{
		switch (step_)
		{
			case StepGetNextEntityData:
			case StepGetCurEntityData:
			case StepPutEntityData:
				if (((typeID == curEntityType_->first) &&
						(dbID >= curEntityDBID_)) ||
					(typeID > curEntityType_->first))	// map is sorted
				{
					log[typeID].insert( dbID );
				}
				break;
			default:
				break;
		}
	}
}

/**
 *	This utility method moves an entry from srcLog to destLog, if it exists.
 */
void MigrateToNewDefsTask::moveToLog( TypeToDBIDsMap& destLog,
	TypeToDBIDsMap& srcLog, EntityTypeID typeID, DatabaseID dbID )
{
	TypeToDBIDsMap::mapped_type& 			dbIDs = srcLog[typeID];
	TypeToDBIDsMap::mapped_type::iterator 	pDBID = dbIDs.find( dbID );
	if (pDBID != dbIDs.end())
	{
		dbIDs.erase(pDBID);
		destLog[typeID].insert( dbID );
	}
}

/**
 *	This utility method returns true if the given entry is in the log.
 */
bool MigrateToNewDefsTask::isInLog( TypeToDBIDsMap& log, EntityTypeID typeID,
	DatabaseID dbID )
{
	TypeToDBIDsMap::mapped_type& dbIDs = log[typeID];
	return dbIDs.find( dbID ) != dbIDs.end();
}

/**
 *	This method clears the logs which keeps track of updated and deleted
 * 	entities.
 */
void MigrateToNewDefsTask::clearCompletedLogs()
{
	// TODO: Consider just clearing the DBID set of each entity type instead
	// of deleting the set.
	updatedEntities_.clear();
	deletedEntities_.clear();
}

/**
 *	This method may be called during the migration process if an entity in the
 * 	original tables is about to be updated.
 */
void MigrateToNewDefsTask::onUpdateEntity( EntityTypeID typeID, DatabaseID dbID )
{
	this->addToLog( updatingEntities_, typeID, dbID );
}

/**
 *	This method may be called during the migration process if an entity in the
 * 	original tables has been updated.
 */
void MigrateToNewDefsTask::onUpdateEntityComplete( EntityTypeID typeID,
	DatabaseID dbID )
{
	this->moveToLog( updatedEntities_, updatingEntities_, typeID, dbID );
	if (inCollision_)
	{
		clearCompletedLogs();
		// Restart migration if we were waiting for this entity to be updated.
		if ((step_ == StepGetCurEntityData) &&
			(typeID == curEntityType_->first) && (dbID == curEntityDBID_))
		{
			inCollision_ = false;
			this->doStepInThread(step_);
		}
	}
}

/**
 *	This method may be called during the migration process if an entity in the
 * 	original tables is about to be deleted.
 */
void MigrateToNewDefsTask::onDeleteEntity( EntityTypeID typeID, DatabaseID dbID )
{
	this->addToLog( deletingEntities_, typeID, dbID );
}

/**
 *	This method may be called during the migration process if an entity in the
 * 	original tables has been deleted.
 */
void MigrateToNewDefsTask::onDeleteEntityComplete( EntityTypeID typeID,
	DatabaseID dbID )
{
	this->moveToLog( deletedEntities_, deletingEntities_, typeID, dbID );
	if (inCollision_)
	{
		clearCompletedLogs();
	}
}

// -----------------------------------------------------------------------------
// Section: class OldMySqlDatabase
// -----------------------------------------------------------------------------
/**
 *	This class implements the IDatabase::IOldDatabase interface which maintains
 * 	compatibility with entities still using the old definitions for a short
 * 	while after we switch over to using the new definitions.
 */
class OldMySqlDatabase : public IDatabase::IOldDatabase
{
	MigrateToNewDefsTask*	pMigrationTask_;
	MySqlThreadData			threadData_;
	bool					isThreadDataLocked_;

public:
	OldMySqlDatabase( MigrateToNewDefsTask& migrationTask,
		const EntityDefs& oldEntityDefs, int maxSpaceDataSize );
	virtual ~OldMySqlDatabase();

	virtual void getEntity( IDatabase::IGetEntityHandler& handler );
	virtual void putEntity( const EntityDBKey& ekey,
		EntityDBRecordIn& erec, IDatabase::IPutEntityHandler& handler );

	void rollback()
	{
		// pMigrationTask_ destructor has cleanup code;
		delete pMigrationTask_;
		pMigrationTask_ = NULL;
	}

	MySqlDatabase& getOwner()		{	return pMigrationTask_->getOwner();	}
	WorkerThread& getWorkerThread()	{	return pMigrationTask_->getThread();	}
	MySqlThreadData& getThreadData()	{	return threadData_;	}

	void lockThreadData();
	void unlockThreadData()
	{
		MF_ASSERT( isThreadDataLocked_ );
		isThreadDataLocked_ = false;
	}

	// No-ops for the following. Needed for compatibility with MySqlDatabase.
	void onPutEntityOpStarted( EntityTypeID typeID, DatabaseID dbID )	{}
	void onPutEntityOpCompleted( EntityTypeID typeID, DatabaseID dbID )	{}
	void onDelEntityOpStarted( EntityTypeID typeID, DatabaseID dbID )	{}
	void onDelEntityOpCompleted( EntityTypeID typeID, DatabaseID dbID )	{}

	void onWriteSpaceOpStarted()
	{
		this->getOwner().onWriteSpaceOpStarted();
	}

	void onWriteSpaceOpCompleted()
	{
		this->getOwner().onWriteSpaceOpCompleted();
	}
};

/**
 * 	This class mimics MySqlThreadTask, except that is deals with
 * 	OldMySqlDatabase instead of MySqlDatabase.
 */
class OldMySqlThreadTask : public WorkerThread::ITask
{
public:
	typedef OldMySqlDatabase OwnerType;

private:
	OldMySqlDatabase& 	owner_;

public:
	OldMySqlThreadTask( OldMySqlDatabase& owner ) : owner_( owner )
	{
		owner_.lockThreadData();	// Blocks until thread data is available.
	}
	virtual ~OldMySqlThreadTask()
	{
		owner_.unlockThreadData();
	}

	MySqlThreadData& getThreadData()	{	return owner_.getThreadData();	}
	OldMySqlDatabase& getOwner()		{	return owner_;	}

	void doTask()
	{
		owner_.getWorkerThread().doTask( *this );
	}

	// Dummy methods to make compiler happy. We don't do any timings for
	// operations on OldMySqlDatabase.
	void startThreadTaskTiming()	{	/* do nothing */	}
	uint64 stopThreadTaskTiming()	{	return 0;	}
};

/**
 *	Constructor. This should only be called after temp tables are renamed to
 * 	real tables and previously real tables have become "old" tables.
 */
OldMySqlDatabase::OldMySqlDatabase( MigrateToNewDefsTask& migrationTask,
		const EntityDefs& oldEntityDefs, int maxSpaceDataSize ) :
	pMigrationTask_( &migrationTask ),
	threadData_( migrationTask.getConnectionInfo(), maxSpaceDataSize,
				oldEntityDefs, OLD_TABLE_NAME_PREFIX,
				SwapInterpreterInitialiserHack<const TypeIDSet*>(
					&migrationTask.getChangedEntitiesOldTypes(),
					migrationTask.getPyInterpreter() ) ),
	isThreadDataLocked_( false )
{
	threadData_.typeMapping.setTemporaryMappings(
		migrationTask.getConnection(), migrationTask.getNewEntityDefs(),
		TABLE_NAME_PREFIX, migrationTask );
}

OldMySqlDatabase::~OldMySqlDatabase()
{
	if (pMigrationTask_)
	{	// not rolled back.
		this->getOwner().onOldDatabaseDestroy();
		pMigrationTask_->cleanUpTables();
		delete pMigrationTask_;
	}
}

/**
 *	This marks the thread data as being "busy". It will block until the thread
 * 	data is not "busy".
 */
void OldMySqlDatabase::lockThreadData()
{
	while (isThreadDataLocked_)
	{
		TRACE_MSG( "Waiting for OldMySqlDatabase thread to become ready\n" );
		Database::instance().getWorkerThreadMgr().waitForTaskCompletion( 1 );
	}
	TRACE_MSG( "OldMySqlDatabase thread ready!\n" );
	isThreadDataLocked_ = true;
}


// -----------------------------------------------------------------------------
// Section: BufferedEntityTasks
// -----------------------------------------------------------------------------

/**
 *	This class is used to represent store a buffered database operation.
 */
class BufferedEntityTask
{
public:
	BufferedEntityTask( DatabaseID dbID ) : dbID_( dbID )
	{
	}

	DatabaseID dbID() const	{ return dbID_; }

	virtual void play( BufferedEntityTasks * pBufferedEntityTasks ) = 0;

private:
	DatabaseID dbID_;
};


/**
 *	This class is responsible for ensuring there is only a single database
 *	operation outstanding for a specific entity. If there are two writeToDB
 *	calls for the same entity outstanding, these cannot be sent to different
 *	threads as the order these are applied to the database are not guaranteed.
 */
class BufferedEntityTasks
{
public:
	/**
	 *	This method attempts to "grab a lock" for writing an entity. This class
	 *	stores a list of the outstanding tasks for an entity. If there are any
	 *	outstanding tasks, this method will return false.
	 *
	 *	If there are no tasks, a new NULL task is added, blocking others from
	 *	grabbing the lock until that task has finished.
	 */
	bool grabLock( DatabaseID dbID )
	{
		// TODO: This should really take both the DatabaseID and EntityTypeID
		// but it is not too bad when these collide and should be rare.

		// Must be associated with an entity
		MF_ASSERT( dbID != 0 );

		std::pair< Map::iterator, Map::iterator > range =
			tasks_.equal_range( dbID );

		if (range.first == range.second)
		{
			// If no tasks existed, a NULL task is added. This represents this
			// new outstanding task
			tasks_.insert( range.second, std::make_pair( dbID,
						(BufferedEntityTask *)NULL ) );
			return true;
		}

		return false;
	}

	/**
	 *	This method buffers a task to be performed once the other outstanding
	 *	tasks for this entity have completed.
	 */
	void buffer( BufferedEntityTask * pBufferedTask )
	{
		MF_ASSERT( pBufferedTask->dbID() != 0 );

		INFO_MSG( "BufferedEntityTasks::buffer: Buffering for %"FMT_DBID"\n",
				pBufferedTask->dbID() );
		DatabaseID dbID = pBufferedTask->dbID();

		std::pair< Map::iterator, Map::iterator > range =
			tasks_.equal_range( dbID );

		MF_ASSERT( range.first != range.second );
		MF_ASSERT( range.first->second == NULL );

		// Make sure that it is inserted at the end.
		tasks_.insert( range.second, std::make_pair( dbID, pBufferedTask ) );
	}

	/**
	 *	This method should be called when the currently performing task has
	 *	completed. It will remove its entry from the collection and will perform
	 *	the next task (if any).
	 */
	void onFinished( DatabaseID dbID )
	{
		MF_ASSERT( dbID != 0 );

		std::pair< Map::iterator, Map::iterator > range =
			tasks_.equal_range( dbID );

		// There must be tasks and the first (the one being deleted) must be
		// NULL.
		MF_ASSERT( range.first != range.second );
		MF_ASSERT( range.first->second == NULL );

		Map::iterator nextIter = range.first;
		++nextIter;
		BufferedEntityTask * pNextTask = NULL;

		if (nextIter != range.second)
		{
			pNextTask = nextIter->second;

			nextIter->second = NULL;
		}

		tasks_.erase( range.first );

		if (pNextTask)
		{
			NOTICE_MSG( "Playing buffered task for %"FMT_DBID"\n",
					pNextTask->dbID() );
			pNextTask->play( this );
			delete pNextTask;
		}
	}

private:
	typedef std::multimap< DatabaseID, BufferedEntityTask * > Map;
	Map tasks_;
};


// -----------------------------------------------------------------------------
// Section: class MySqlDatabase
// -----------------------------------------------------------------------------
MySqlDatabase::MySqlDatabase() :
	pThreadResPool_( 0 ),
	maxSpaceDataSize_( 2048 ),
	numConnections_( 5 ),
	pServerConfig_( new MySqlServerConfig() ),
	numWriteSpaceOpsInProgress_( 0 ),
	spacesVersion_( 42 ),
	reconnectTimerID_( -1 ),
	reconnectCount_( 0 ),
	pMigrationTask_( 0 ),
	pOldDatabase_( 0 ),
	pBufferedEntityTasks_( new BufferedEntityTasks )
{
	MF_WATCH( "performance/numBusyThreads", *this,
				&MySqlDatabase::watcherGetNumBusyThreads );
	MF_WATCH( "performance/busyThreadsMaxElapsed", *this,
				&MySqlDatabase::watcherGetBusyThreadsMaxElapsedSecs );
	MF_WATCH( "performance/allOperations/rate", *this,
				&MySqlDatabase::watcherGetAllOpsCountPerSec );
	MF_WATCH( "performance/allOperations/duration", *this,
				&MySqlDatabase::watcherGetAllOpsAvgDurationSecs );
}

MySqlDatabase * MySqlDatabase::create()
{
	try
	{
		MySqlDatabase * pDatabase = new MySqlDatabase();
		return pDatabase;
	}
	catch (std::exception & e)
	{
		return NULL;
	}
}

MySqlDatabase::~MySqlDatabase()
{
	delete pBufferedEntityTasks_;
	pBufferedEntityTasks_ = NULL;
}

bool MySqlDatabase::startup( const EntityDefs& entityDefs,
		bool isFaultRecovery, bool isUpgrade )
{
	MF_ASSERT( !(isFaultRecovery && isUpgrade) );	// Can't do both at once.

#ifdef USE_MYSQL_PREPARED_STATEMENTS
		INFO_MSG( "\tMySql: Compiled for prepared statements = True.\n" );
#else
		INFO_MSG( "\tMySql: Compiled for prepared statements = False.\n" );
#endif

	// Print out list of configured servers
	INFO_MSG( "\tMySql: Configured MySQL servers:\n" );
	do
	{
		const MySqlServerConfig::ServerInfo& serverInfo =
				pServerConfig_->getCurServer();
		// Test connection to server
		const char * failedString;
		try
		{
			MySql connection( serverInfo.connectionInfo );
			failedString = "";
		}
		catch (std::exception& e)
		{
			failedString = " - FAILED!";
		}
		INFO_MSG( "\t\t%s: %s (%s)%s\n", serverInfo.configName.c_str(),
					serverInfo.connectionInfo.host.c_str(),
//					(serverInfo.connectionInfo.host + ";" +
//					serverInfo.connectionInfo.username + ";" +
//					serverInfo.connectionInfo.password).c_str(),
					serverInfo.connectionInfo.database.c_str(),
					failedString );
	} while (pServerConfig_->gotoNextServer());

	try
	{
		const MySql::ConnectionInfo& connectionInfo =
				pServerConfig_->getCurServer().connectionInfo;
		MySql connection( connectionInfo );

        /* huangshanquan 2010-02-22 add end*/
        /* Check mysql server version */
        unsigned long dbversion = connection.getVersion();
        if ( dbversion < SUPPORTED_MYSQL_VERSION )
        {
			ERROR_MSG( "Mysql require mysql version than %d current use version is %d\n",
                    SUPPORTED_MYSQL_VERSION, dbversion );
			return false;
        }

        /* Check index */
        MemoryOStream tabExist;
        int trows;
        connection.execute("show tables like 'bigworldTableMetadata'", &tabExist);
        tabExist >> trows;
        if( trows == 1 && Database::instance().updateSelfIndex() )
        {
            MemoryOStream indexData;
            std::stringstream bwIndexTable;
            bwIndexTable << "select `tbl`, `col`, `idx` from `bigworldTableMetadata` where `idx` = " 
                << ITableCollector::IndexTypeUnique << " or `idx` = " 
                << ITableCollector::IndexTypeNormal ;
            connection.execute(bwIndexTable.str(), &indexData);

            int nrows;
            int nfields;
            indexData >> nrows >> nfields;

            for(int i = 0; i < nrows; i++)
            {
                std::string tbl, col, idx;
                indexData >> tbl >> col >> idx;
                MemoryOStream showIndexData;
                std::stringstream showIndexStr;
                std::string ketType;
                if(idx == "5") 
                    ketType = "1";
                else
                    ketType = "0";


                showIndexStr << "show index from `" << tbl << "` where `Column_name` = '" << col <<
                    "' and `Non_unique` = " << ketType;

                connection.execute(showIndexStr.str(), &showIndexData);
                int rows;
                showIndexData >> rows;

                if(rows == 0)
                {
                    std::stringstream stmtStrm;
                    stmtStrm << "UPDATE `bigworldTableMetadata` SET " << "`idx`=0" 
                        << " WHERE `tbl`='" << tbl << "' AND `col`='" << col << "'";
                    std::string t = stmtStrm.str();

                    MemoryOStream keyNameData;
                    std::stringstream keyNameStr;
                    int nKeyName;

                    std::string::size_type scorePos = col.find( '_' );
                    std::string keyName = 
                        (scorePos == std::string::npos) ? col + "Index" : col.substr( scorePos + 1 ) + "Index";

                    keyNameStr << "show index from " << tbl << " where `Key_name` = '" << 
                        keyName  << "'";

                    connection.execute(keyNameStr.str(), &keyNameData);
                    keyNameData >> nKeyName;
                    if(nKeyName != 0)
                    {
                        connection.execute( "ALTER TABLE " + tbl + " DROP INDEX " + keyName );
                    }
                }
            }
        }

	    //return false;
        /* huangshanquan 2010-02-22 add end*/

		uint32 version = initInfoTable( connection );
		if (version < DBMGR_OLDEST_SUPPORTED_VERSION)
		{
			ERROR_MSG( "Cannot use database created by an ancient version "
					"of BigWorld\n" );
			return false;
		}
		else if ( (version < DBMGR_CURRENT_VERSION) && !isUpgrade )
		{
			ERROR_MSG( "Cannot use database from previous versions of BigWorld "
						"without upgrade\n" );
			INFO_MSG( "Database can be upgraded by running dbmgr -upgrade\n" );
			return false;
		}
		else if (version > DBMGR_CURRENT_VERSION)
		{
			ERROR_MSG( "Cannot use database from newer version of BigWorld\n" );
			return false;
		}
		else if ( (version == DBMGR_CURRENT_VERSION) && isUpgrade )
		{
			WARNING_MSG( "Database version is current, ignoring -upgrade option\n" );
		}

		maxSpaceDataSize_ = std::max( BWConfig::get( "dbMgr/maxSpaceDataSize",
										maxSpaceDataSize_ ), 1 );

		if (!isFaultRecovery)
		{
			initEntityTables( connection, entityDefs, version );

			MySqlTransaction t( connection );
			t.execute( "CREATE TABLE IF NOT EXISTS bigworldNewID "
					 "(id INT NOT NULL) TYPE=InnoDB" );
			t.execute( "CREATE TABLE IF NOT EXISTS bigworldUsedIDs "
					 "(id INT NOT NULL) TYPE=InnoDB" );
			t.execute( "DELETE FROM bigworldUsedIDs" );
			t.execute( "DELETE FROM bigworldNewID" );
			t.execute( "INSERT INTO bigworldNewID (id) VALUES (1)" );

			t.execute( "CREATE TABLE IF NOT EXISTS bigworldGameTime "
					"(time INT NOT NULL)" );

			char * blobTypeName;
			if (maxSpaceDataSize_ < 1<<8)
				blobTypeName = "TINYBLOB";
			else if (maxSpaceDataSize_ < 1<<16)
				blobTypeName = "BLOB";
			else if (maxSpaceDataSize_ < 1<<24)
				blobTypeName = "MEDIUMBLOB";
			else
				blobTypeName = "LONGBLOB";

			// Copy the old space tables to recovery tables and create new ones.
			t.execute( "CREATE TABLE IF NOT EXISTS bigworldSpaces "
					"(id INT NOT NULL UNIQUE, version TINYINT NOT NULL)" );
			char buffer[512];
			sprintf( buffer, "CREATE TABLE IF NOT EXISTS bigworldSpaceData "
					"(id INT, "
					"spaceEntryID BIGINT NOT NULL, "
					"entryKey SMALLINT UNSIGNED NOT NULL, "
					"data %s NOT NULL, "
					"version TINYINT NOT NULL)",
					blobTypeName );
			t.execute( buffer );
			// Just in case the table already exists and have a different BLOB
			// type for the data column.
			sprintf( buffer, "ALTER TABLE bigworldSpaceData MODIFY data %s",
					 blobTypeName );
			t.execute( buffer );

			// End space data tables

			if (Database::instance().clearRecoveryDataOnStartUp())
			{
				t.execute( "DELETE FROM bigworldLogOns" );

				t.execute( "DELETE FROM bigworldSpaces" );
				t.execute( "DELETE FROM bigworldSpaceData" );

				t.execute( "UPDATE bigworldGameTime SET time=0" );
			}

			t.commit();
		}

		numConnections_ = std::max( BWConfig::get( "dbMgr/numConnections",
													numConnections_ ), 1 );

		INFO_MSG( "\tMySql: Number of connections = %d.\n", numConnections_ );

		// Create threads and thread resources.
		pThreadResPool_ =
			new MySqlThreadResPool( Database::instance().getWorkerThreadMgr(),
				Database::instance().nub(),
				numConnections_, maxSpaceDataSize_, connectionInfo, entityDefs );

		MySqlBindings b;

		MySqlThreadData& mainThreadData = this->getMainThreadData();
		mainThreadData.connection.execute(
			*mainThreadData.getGameTimeStatement_ );
		if (mainThreadData.getGameTimeStatement_->resultRows() == 0)
		{
			mainThreadData.connection.execute( "INSERT INTO bigworldGameTime VALUES (0)" );
		}
	}
	catch (std::exception& e)
	{
		ERROR_MSG( "MySqlDatabase::startup: %s\n", e.what() );
		return false;
	}

	return true;
}

bool MySqlDatabase::shutDown()
{
	try
	{
		delete pThreadResPool_;
		pThreadResPool_ = NULL;
		if (pOldDatabase_)
		{
			pOldDatabase_->rollback();
			delete pOldDatabase_;
			pOldDatabase_ = NULL;
		}
		else
		{
			delete pMigrationTask_;	// Should be NULL, but just in case...
			pMigrationTask_ = NULL;
		}

		if (reconnectTimerID_ != -1)
		{
			Database::instance().nub().cancelTimer( reconnectTimerID_ );
			reconnectTimerID_ = -1;
		}

		return true;
	}
	catch (std::exception& e)
	{
		ERROR_MSG( "MySqlDatabase::shutDown: %s\n", e.what() );
		return false;
	}
}

void printSection( const char * msg, DataSectionPtr pSection )
{
	BinaryPtr pBinary = pSection->asBinary();
	DEBUG_MSG( "printSection: %s\n", msg );
	size_t len = pBinary->len();
	const char * data = static_cast<const char *>( pBinary->data() );
	for ( size_t i=0; i<len; ++i )
	{
		putchar( data[i] );
	}
	puts("");
}

inline MySqlThreadData& MySqlDatabase::getMainThreadData()
{
	return pThreadResPool_->getMainThreadData();
}

/**
 *	This method is called when one of our connections to the database drops out.
 * 	We assume that if one drops out, then all are in trouble.
 */
void MySqlDatabase::onConnectionFatalError()
{
	if (!this->hasFatalConnectionError())
	{
		if (pServerConfig_->getNumServers() == 1)
		{
			// Poll every second.
			reconnectTimerID_ =
				Database::instance().nub().registerTimer( 1000000, this );
		}
		else
		{
			// Switch servers straight away.
			reconnectTimerID_ =
				Database::instance().nub().registerTimer( 1, this );
		}
		reconnectCount_ = 0;
	}
}

/**
 *	This method is attempts to restore all our connections to the database.
 */
bool MySqlDatabase::restoreConnectionToDb()
{
	MF_ASSERT( this->hasFatalConnectionError() );

	++reconnectCount_;

	bool isSuccessful;
	if (pServerConfig_->getNumServers() == 1)
	{
		// Try to reconnect to the same server. Quickly test if it's worthwhile
		// reconnecting.
		isSuccessful = this->getMainThreadData().connection.ping();
	}
	else
	{
		pServerConfig_->gotoNextServer();
		// Assume it's OK to connect to it. If it's down then we'd have done
		// a whole lot of useless work but hopefully it's a rare case.
		isSuccessful = true;

		if (reconnectCount_ == pServerConfig_->getNumServers())
		{
			// Go back to polling every second.
			Database::instance().nub().cancelTimer( reconnectTimerID_ );
			reconnectTimerID_ =
				Database::instance().nub().registerTimer( 1000000, this );
		}
	}

	if (isSuccessful)
	{
		MySqlThreadResPool* pOldThreadResPool = pThreadResPool_;
		// Wait for all tasks to finish because we are about to swap the global
		// thread resource pool. Tasks generally assume that this doesn't
		// change while they are executing.
		pOldThreadResPool->threadPool().waitForAllTasks();
		const MySqlServerConfig::ServerInfo& curServer =
				pServerConfig_->getCurServer();
		try
		{
			pThreadResPool_ =
				new MySqlThreadResPool( Database::instance().getWorkerThreadMgr(),
					Database::instance().nub(),
					numConnections_, maxSpaceDataSize_,
					curServer.connectionInfo,
					Database::instance().getEntityDefs() );

			if (pMigrationTask_)
				isSuccessful = pMigrationTask_->restoreConnectionToDb();
			// TODO: Have to restore connection in pOldDatabase_ as well but
			// chances of disconnection and recovery happening during
			// pOldDatabase_'s lifetime very small... perhaps too small to worth
			// bothering about.

			if (isSuccessful)
			{
				Database::instance().nub().cancelTimer( reconnectTimerID_ );
				reconnectTimerID_ = -1;

				delete pOldThreadResPool;

				INFO_MSG( "MySqlDatabase: %s - Reconnected to database\n",
						 curServer.configName.c_str() );
			}
			else
			{
				delete pThreadResPool_;
				pThreadResPool_ = pOldThreadResPool;
			}
		}
		catch (std::exception& e)
		{
			ERROR_MSG( "MySqlDatabase::restoreConnectionToDb: %s - %s\n",
						curServer.configName.c_str(),
						e.what() );

			if (pThreadResPool_ != pOldThreadResPool)
				delete pThreadResPool_;
			pThreadResPool_ = pOldThreadResPool;

			isSuccessful = false;
		}
	}

	return isSuccessful;
}

/**
 *	Timer callback.
 */
int MySqlDatabase::handleTimeout( int id, void * arg )
{
	this->restoreConnectionToDb();

	return 0;
}

/**
 *	Watcher interface. Gets the number of threads currently busy.
 */
uint MySqlDatabase::watcherGetNumBusyThreads() const
{
	return (pThreadResPool_) ?
				(uint) pThreadResPool_->threadPool().getNumBusyThreads() : 0;
}

/**
 *	Watcher interface. For busy threads, get the duration of the thread that has
 *  been running the longest (in seconds).
 */
double MySqlDatabase::watcherGetBusyThreadsMaxElapsedSecs() const
{
	return (pThreadResPool_) ?
				pThreadResPool_->getBusyThreadsMaxElapsedSecs() : 0;
}

/**
 *	Watcher interface. Get the number of operations per second.
 */
double MySqlDatabase::watcherGetAllOpsCountPerSec() const
{
	return (pThreadResPool_) ?
				pThreadResPool_->getOpCountPerSec() : 0;
}

/**
 *	Watcher interface. Get the average duration of operations (in seconds).
 */
double MySqlDatabase::watcherGetAllOpsAvgDurationSecs() const
{
	return (pThreadResPool_) ? pThreadResPool_->getAvgOpDuration() : 0;
}


// -----------------------------------------------------------------------------
// Section: class MapLoginToEntityDBKeyTask
// -----------------------------------------------------------------------------
/**
 *	This class encapsulates a mapLoginToEntityDBKey() operation so that it can
 *	be executed in a separate thread.
 */
class MapLoginToEntityDBKeyTask : public MySqlThreadTask
{
	std::string			logOnName_;
	std::string			password_;
	DatabaseLoginStatus	loginStatus_;
	IDatabase::IMapLoginToEntityDBKeyHandler& handler_;

public:
	MapLoginToEntityDBKeyTask( MySqlDatabase& owner,
		const std::string& logOnName, const std::string& password,
		IDatabase::IMapLoginToEntityDBKeyHandler& handler );

	// WorkerThread::ITask overrides
	virtual void run();
	virtual void onRunComplete();
};

/**
 *	Constructor. Stores new entity information in MySQL bindings.
 */
MapLoginToEntityDBKeyTask::MapLoginToEntityDBKeyTask( MySqlDatabase& owner,
	const std::string& logOnName, const std::string& password,
	IDatabase::IMapLoginToEntityDBKeyHandler& handler )
	: MySqlThreadTask(owner), logOnName_(logOnName), password_(password),
	loginStatus_( DatabaseLoginStatus::LOGGED_ON ), handler_(handler)
{
	this->startThreadTaskTiming();

	MySqlThreadData& threadData = this->getThreadData();
	threadData.ekey.typeID = 0;
	threadData.ekey.dbID = 0;
	threadData.ekey.name.clear();
	threadData.exceptionStr.clear();
}

/**
 *	This method writes the new default entity into the database.
 *	May be run in another thread.
 */
void MapLoginToEntityDBKeyTask::run()
{
	bool retry;
	do
	{
		retry = false;
		MySqlThreadData& threadData = this->getThreadData();
		try
		{
			MySqlTransaction transaction( threadData.connection );
			std::string actualPassword;
            int isblock ,istakeover;

			bool entryExists = threadData.typeMapping.getLogOnMapping( transaction,
				logOnName_, actualPassword, threadData.ekey.typeID,
				threadData.ekey.name, isblock, istakeover );

			if (entryExists)
			{
				if ( !actualPassword.empty() && password_ != actualPassword )
                {
                    if(istakeover)
                    {
                        loginStatus_ = DatabaseLoginStatus::LOGIN_CONTROL_BY_GM ;
                    }
                    else
                    {
					    loginStatus_ = DatabaseLoginStatus::LOGIN_REJECTED_INVALID_PASSWORD;
                    }
                }
                else if( isblock && !istakeover )
                {
                    loginStatus_ = DatabaseLoginStatus::LOGIN_ACCOUNT_BLOCKED ;
                }
			}
			else
			{
				loginStatus_ = DatabaseLoginStatus::LOGIN_REJECTED_NO_SUCH_USER;
			}
			transaction.commit();
		}
		catch (MySqlRetryTransactionException& e)
		{
			retry = true;
		}
		catch (std::exception& e)
		{
			threadData.exceptionStr = e.what();
			loginStatus_ = DatabaseLoginStatus::LOGIN_REJECTED_DB_GENERAL_FAILURE;
		}
	} while (retry);
}

/**
 *	This method is called in the main thread after run() is complete.
 */
void MapLoginToEntityDBKeyTask::onRunComplete()
{
	MySqlThreadData& threadData = this->getThreadData();
	if (threadData.exceptionStr.length())
	{
		ERROR_MSG( "MySqlDatabase::mapLoginToEntityDBKey: %s\n",
		           threadData.exceptionStr.c_str() );
	}
	else if (threadData.connection.hasFatalError())
	{
		loginStatus_ = DatabaseLoginStatus::LOGIN_REJECTED_DB_GENERAL_FAILURE;
	}

	uint64 duration = this->stopThreadTaskTiming();
	if (duration > THREAD_TASK_WARNING_DURATION)
		WARNING_MSG( "MapLoginToEntityDBKeyTask for '%s' took %f seconds\n",
					logOnName_.c_str(), double(duration)/stampsPerSecondD() );

	// Release thread resources before callback, so that if callback decides
	// to do another operation that requires thread resource, it is not
	// deadlocked.
	DatabaseLoginStatus	loginStatus = loginStatus_;
	EntityDBKey			ekey = threadData.ekey;
	IDatabase::IMapLoginToEntityDBKeyHandler& handler = handler_;
	delete this;

	handler.onMapLoginToEntityDBKeyComplete( loginStatus, ekey );
}


/**
 *	IDatabase override
 */
void MySqlDatabase::mapLoginToEntityDBKey(
		const std::string & logOnName,
		const std::string & password,
		IDatabase::IMapLoginToEntityDBKeyHandler& handler )
{
	MapLoginToEntityDBKeyTask* pTask =
		new MapLoginToEntityDBKeyTask( *this, logOnName, password, handler );
	pTask->doTask();
}

/**
 *	This class encapsulates the setLoginMapping() operation so that it can be
 *	executed in a separate thread.
 */
class SetLoginMappingTask : public MySqlThreadTask
{
	IDatabase::ISetLoginMappingHandler& handler_;

public:
	SetLoginMappingTask( MySqlDatabase& owner, const std::string& username,
		const std::string & password, const EntityDBKey& ekey,
		IDatabase::ISetLoginMappingHandler& handler )
		: MySqlThreadTask(owner), handler_(handler)
	{
		this->startThreadTaskTiming();

		MySqlThreadData& threadData = this->getThreadData();

		threadData.typeMapping.logOnMappingToBound( username, password,
			ekey.typeID, ekey.name );

		threadData.exceptionStr.clear();
	}

	// WorkerThread::ITask overrides
	virtual void run();
	virtual void onRunComplete();
};

/**
 *	This method writes the log on mapping into the table.
 *	May be run in another thread.
 */
void SetLoginMappingTask::run()
{
	bool retry;
	do
	{
		retry = false;
		MySqlThreadData& threadData = this->getThreadData();
		try
		{
			MySqlTransaction transaction( threadData.connection );
			threadData.typeMapping.setLogOnMapping( transaction );
			transaction.commit();
		}
		catch (MySqlRetryTransactionException& e)
		{
			retry = true;
		}
		catch (std::exception& e)
		{
			threadData.exceptionStr = e.what();
		}
	} while (retry);
}

/**
 *	This method is called in the main thread after run() is complete.
 */
void SetLoginMappingTask::onRunComplete()
{
	MySqlThreadData& threadData = this->getThreadData();
	if (threadData.exceptionStr.length())
	{
		// __kyl__ (13/7/2005) At the moment there is no good reason
		// why this operation should fail. Possibly something disasterous
		// like MySQL going away.
		ERROR_MSG( "MySqlDatabase::setLoginMapping: %s\n",
		           threadData.exceptionStr.c_str() );
	}

	uint64 duration = this->stopThreadTaskTiming();
	if (duration > THREAD_TASK_WARNING_DURATION)
		WARNING_MSG( "SetLoginMappingTask for '%s' took %f seconds\n",
					threadData.typeMapping.getBoundLogOnName().c_str(),
					double(duration)/stampsPerSecondD() );

	// Release thread resources before callback, so that if callback decides
	// to do another operation that requires thread resource, it is not
	// deadlocked.
	IDatabase::ISetLoginMappingHandler& handler = handler_;
	delete this;

	handler.onSetLoginMappingComplete();
}

/**
 *	IDatabase override
 */
void MySqlDatabase::setLoginMapping( const std::string& username,
	const std::string& password, const EntityDBKey& ekey,
	IDatabase::ISetLoginMappingHandler& handler )
{
	SetLoginMappingTask* pTask =
		new SetLoginMappingTask( *this, username, password, ekey, handler );
	pTask->doTask();
}

/**
 *	This class encapsulates the MySqlDatabase::getEntity() operation so that
 *	it can be executed in a separate thread.
 *
 * 	THREADTASK must be a MySqlThreadTask-like class.
 */
template <class THREADTASK>
class GetEntityTask : public THREADTASK
{
	IDatabase::IGetEntityHandler& handler_;

public:
	GetEntityTask( typename THREADTASK::OwnerType& owner,
		IDatabase::IGetEntityHandler& handler );

	// WorkerThread::ITask overrides
	virtual void run();
	virtual void onRunComplete();

private:
	bool fillKey( MySqlTransaction& transaction, EntityDBKey& ekey );
};

/**
 *	Constructor.
 */
template <class THREADTASK>
GetEntityTask<THREADTASK>::GetEntityTask( typename THREADTASK::OwnerType& owner,
							 IDatabase::IGetEntityHandler& handler )
	: THREADTASK(owner), handler_(handler)
{
	this->startThreadTaskTiming();
}

/**
 *	This method reads the entity data into the MySQL bindings. May be executed
 *	in a separate thread.
 */
template <class THREADTASK>
void GetEntityTask<THREADTASK>::run()
{
	MySqlThreadData& threadData = this->getThreadData();
	bool isOK = true;
	threadData.exceptionStr.clear();
	try
	{
		MySqlTransaction	transaction( threadData.connection );
		MySqlTypeMapping&	typeMapping = threadData.typeMapping;
		EntityDBKey&		ekey = handler_.key();
		EntityDBRecordOut&	erec = handler_.outrec();
		bool				definitelyExists = false;
		if (erec.isStrmProvided())
		{	// Get entity props
			definitelyExists = typeMapping.getEntityToBound( transaction, ekey );
			isOK = definitelyExists;
		}

		if (isOK && erec.isBaseMBProvided() && erec.getBaseMB())
		{	// Need to get base mail box
			if (!definitelyExists)
				isOK = this->fillKey( transaction, ekey );

			if (isOK)
			{	// Try to get base mailbox
				definitelyExists = true;
				if (!typeMapping.getLogOnRecord( transaction, ekey.typeID,
					ekey.dbID, *erec.getBaseMB() ) )
					erec.setBaseMB( 0 );

			}
		}

		if (isOK && !definitelyExists)
		{	// Caller hasn't asked for anything except the missing member of
			// ekey.
			isOK = this->fillKey( transaction, ekey );
		}
		transaction.commit();
	}
	catch (std::exception& e)
	{
		threadData.exceptionStr = e.what();
		isOK = false;
	}

	threadData.isOK = isOK;
}

/**
 *	This method is called in the main thread after run() is complete.
 */
template <class THREADTASK>
void GetEntityTask<THREADTASK>::onRunComplete()
{
	MySqlThreadData& threadData = this->getThreadData();
	if (threadData.exceptionStr.length() > 0)
		ERROR_MSG( "MySqlDatabase::getEntity: %s\n",
			threadData.exceptionStr.c_str() );
	else if (threadData.connection.hasFatalError())
		threadData.isOK = false;

	if ( threadData.isOK )
	{
		EntityDBRecordOut&	erec = handler_.outrec();
		if (erec.isStrmProvided())
		{
			EntityDBKey& ekey = handler_.key();
			// NOTE: boundToStream() shouldn't be run in a separate thread
			// because is uses Python do to some operations.
			threadData.typeMapping.boundToStream( ekey.typeID, erec.getStrm(),
				handler_.getPasswordOverride() );
		}
	}

	const EntityDBKey& ekey = handler_.key();
	uint64 duration = this->stopThreadTaskTiming();
	if (duration > THREAD_TASK_WARNING_DURATION)
		WARNING_MSG( "GetEntityTask for entity %"FMT_DBID" of type %d "
					"named '%s' took %f seconds\n",
					ekey.dbID, ekey.typeID, ekey.name.c_str(),
					double(duration)/stampsPerSecondD() );

	// Release thread resources before callback, so that if callback decides
	// to do another operation that requires thread resource, it is not
	// deadlocked.
	IDatabase::IGetEntityHandler& handler = handler_;
	bool isOK = threadData.isOK;
	delete this;

	handler.onGetEntityComplete( isOK );
}

/**
 *	This method set the missing member of the EntityDBKey. If entity doesn't
 *	have a name property then ekey.name is set to empty.
 *
 *	This method may be called from another thread.
 *
 *	@return	True if successful. False if entity doesn't exist.
 */
template <class THREADTASK>
bool GetEntityTask<THREADTASK>::fillKey( MySqlTransaction& transaction,
	EntityDBKey& ekey )
{
	MySqlTypeMapping& typeMapping = this->getThreadData().typeMapping;
	bool isOK;
	if (typeMapping.hasNameProp( ekey.typeID ))
	{
		if (ekey.dbID)
		{
			isOK = typeMapping.getEntityName( transaction, ekey.typeID,
				ekey.dbID, ekey.name );
		}
		else
		{
			ekey.dbID = typeMapping.getEntityDbID( transaction,
						ekey.typeID, ekey.name );
			isOK = ekey.dbID != 0;
		}
	}
	else
	{	// Entity doesn't have a name property. Check for entity existence if
		// DBID is provided
		if (ekey.dbID)
		{
			isOK = typeMapping.checkEntityExists( transaction, ekey.typeID,
				ekey.dbID );
			ekey.name.clear();
		}
		else
		{
			isOK = false;
		}
	}

	return isOK;
}

/**
 *	Override from IDatabase
 */
void MySqlDatabase::getEntity( IDatabase::IGetEntityHandler& handler )
{
	GetEntityTask<MySqlThreadTask>*	pGetEntityTask =
		new GetEntityTask<MySqlThreadTask>( *this, handler );
	pGetEntityTask->doTask();
}

/**
 *	Override from IOldDatabase
 */
void OldMySqlDatabase::getEntity( IDatabase::IGetEntityHandler& handler )
{
	// Need backward compatibility for this entity only if it is one of those
	// that we've migrated.
	const TypeIDSet& changedTypes = pMigrationTask_->getChangedEntitiesOldTypes();
	if ( changedTypes.find( handler.key().typeID ) != changedTypes.end() )
	{
		GetEntityTask<OldMySqlThreadTask>*	pGetEntityTask =
			new GetEntityTask<OldMySqlThreadTask>( *this, handler );
		pGetEntityTask->doTask();
	}
	else
	{
		// Otherwise, just use normal path because entity tables are unchanged
		// ... plus we don't have the "old" tables to load this entity from.
		this->getOwner().getEntity( handler );
	}
}


/**
 *	This class encapsulates the MySqlDatabase::putEntity() operation so that
 *	it can be executed in a separate thread.
 *
 * 	THREADTASK must be a MySqlThreadTask-like class.
 */
template <class THREADTASK>
class PutEntityTask : public THREADTASK
{
	enum BaseRefAction
	{
		BaseRefActionNone,
		BaseRefActionWrite,
		BaseRefActionRemove
	};

	bool							writeEntityData_;
	BaseRefAction					baseRefAction_;
	IDatabase::IPutEntityHandler&	handler_;
	BufferedEntityTasks *			pBufferedEntityTasks_;

public:
	PutEntityTask( typename THREADTASK::OwnerType& owner,
				   const EntityDBKey& ekey, EntityDBRecordIn& erec,
				   IDatabase::IPutEntityHandler& handler,
				   BufferedEntityTasks * pBufferedEntityTasks );

	// WorkerThread::ITask overrides
	virtual void run();
	virtual void onRunComplete();
};

/**
 *	Constructor. Stores all required information from ekey and erec so that
 *	operation can be executed in a separate thread.
 */
template <class THREADTASK>
PutEntityTask<THREADTASK>::PutEntityTask( typename THREADTASK::OwnerType& owner,
							  const EntityDBKey& ekey,
							  EntityDBRecordIn& erec,
							  IDatabase::IPutEntityHandler& handler,
							  BufferedEntityTasks * pBufferedEntityTasks )
	: THREADTASK(owner), writeEntityData_(false),
	baseRefAction_(BaseRefActionNone), handler_(handler),
	pBufferedEntityTasks_( pBufferedEntityTasks )
{
	MF_ASSERT( (ekey.dbID != 0) || (pBufferedEntityTasks_ == NULL) );
	this->startThreadTaskTiming();

	MySqlThreadData& threadData = this->getThreadData();

	threadData.ekey = ekey;
	threadData.isOK = true;
	threadData.exceptionStr.clear();

	// Store entity data inside bindings, ready to be put into the database.
	if (erec.isStrmProvided())
	{
		threadData.typeMapping.streamToBound( ekey.typeID, ekey.dbID,
												erec.getStrm() );
		writeEntityData_ = true;
		owner.onPutEntityOpStarted( ekey.typeID, ekey.dbID );
	}

	if (erec.isBaseMBProvided())
	{
		EntityMailBoxRef* pBaseMB = erec.getBaseMB();
		if (pBaseMB)
		{
			threadData.typeMapping.baseRefToBound( *pBaseMB );
			baseRefAction_ =  BaseRefActionWrite;
		}
		else
		{
			baseRefAction_ = BaseRefActionRemove;
		}
	}
}

/**
 *	This method writes the entity data into the database. May be executed in
 *	a separate thread.
 */
template <class THREADTASK>
void PutEntityTask<THREADTASK>::run()
{
	bool retry;
	do
	{
		retry = false;
		MySqlThreadData& threadData = this->getThreadData();
		try
		{
			DatabaseID			dbID = threadData.ekey.dbID;
			EntityTypeID		typeID = threadData.ekey.typeID;
			bool				isOK = threadData.isOK;
			MySqlTransaction	transaction( threadData.connection );
			bool				definitelyExists = false;
			if (writeEntityData_)
			{
				if (dbID)
				{
					isOK = threadData.typeMapping.updateEntity( transaction,
																typeID, dbID );
				}
				else
				{
					dbID = threadData.typeMapping.newEntity( transaction, typeID );
					isOK = (dbID != 0);
				}

				definitelyExists = isOK;
			}

			if (isOK && baseRefAction_ != BaseRefActionNone)
			{
				if (!definitelyExists)
				{	// Check for existence to prevent adding crap LogOn records
					isOK = threadData.typeMapping.checkEntityExists( transaction,
								typeID, dbID );
				}

				if (isOK)
				{
					if (baseRefAction_ == BaseRefActionWrite)
					{
						// Add or update the log on record.
						threadData.typeMapping.addLogOnRecord( transaction,
								typeID, dbID );
					}
					else
					{	// Try to set BaseRef to "NULL" by removing the record
						threadData.typeMapping.removeLogOnRecord( transaction,
								typeID, dbID );
						if (transaction.affectedRows() == 0)
						{
							// Not really an error. If it doesn't exist then
							// it is effectively "NULL" already. Want to print
							// out a warning but no easy way to to that.
							// So doing something a little naughty and setting
							// exception string but leaving isOK as true.
							threadData.exceptionStr = "Failed to remove logon record";
						}
					}
				}
			}
			transaction.commit();

			threadData.ekey.dbID = dbID;
			threadData.isOK = isOK;
		}
		catch (MySqlRetryTransactionException& e)
		{
			retry = true;
		}
		catch (std::exception& e)
		{
			threadData.exceptionStr = e.what();
			threadData.isOK = false;
		}
	} while (retry);
}

/**
 *	This method is called in the main thread after run() is complete.
 */
template <class THREADTASK>
void PutEntityTask<THREADTASK>::onRunComplete()
{
	MySqlThreadData& threadData = this->getThreadData();
	if (threadData.exceptionStr.length())
		ERROR_MSG( "MySqlDatabase::putEntity: %s\n", threadData.exceptionStr.c_str() );
	else if (threadData.connection.hasFatalError())
		threadData.isOK = false;
	else if (!threadData.isOK)
		WARNING_MSG( "MySqlDatabase::putEntity: Failed to write entity %"FMT_DBID
					" of type %d into MySQL database.\n",
					threadData.ekey.dbID, threadData.ekey.typeID  );

	if (writeEntityData_)
		this->getOwner().onPutEntityOpCompleted( threadData.ekey.typeID,
			threadData.ekey.dbID );

	uint64 duration = this->stopThreadTaskTiming();
	if (duration > THREAD_TASK_WARNING_DURATION)
		WARNING_MSG( "PutEntityTask for entity %"FMT_DBID" of type %d "
					"took %f seconds\n",
					threadData.ekey.dbID, threadData.ekey.typeID,
					double(duration)/stampsPerSecondD() );

	// Release thread resources before callback, so that if callback decides
	// to do another operation that requires thread resource, it is not
	// deadlocked.
	bool isOK = threadData.isOK;
	DatabaseID dbID = threadData.ekey.dbID;
	BufferedEntityTasks * pBufferedEntityTasks = pBufferedEntityTasks_;

	IDatabase::IPutEntityHandler& handler = handler_;
	delete this;

	handler.onPutEntityComplete( isOK, dbID );

	// Perform this last to ensure others return earlier. Note that 'this' has
	// already been deleted
	if (pBufferedEntityTasks)
	{
		pBufferedEntityTasks->onFinished( dbID );
	}
}


/**
 *	This class buffers a PutEntity task that will be performed later.
 */
class BufferedPutEntityTask : public BufferedEntityTask
{
public:
	/**
	 *	This static method creates a BufferedPutEntityTask.
	 */
	static BufferedEntityTask * create(
			MySqlDatabase & owner,
			const EntityDBKey & ekey,
			EntityDBRecordIn & erec,
			IDatabase::IPutEntityHandler & handler )
	{
		return new BufferedPutEntityTask( owner, ekey, erec, handler );
	}

	/**
	 *	This method dispatches a PutEntity task to be performed by a thread.
	 */
	static void run( MySqlDatabase & owner,
			const EntityDBKey & ekey,
			EntityDBRecordIn & erec,
			IDatabase::IPutEntityHandler & handler,
			BufferedEntityTasks * pBufferedEntityTasks )
	{
		PutEntityTask<MySqlThreadTask>* pTask =
			new PutEntityTask<MySqlThreadTask>( owner, ekey, erec, handler,
				   pBufferedEntityTasks );

		pTask->doTask();
	}

	/**
	 *	This method is called to perform this buffered task.
	 */
	virtual void play( BufferedEntityTasks * pBufferedEntityTasks )
	{
		BufferedPutEntityTask::run( owner_,
				ekey_, erec_, handler_, pBufferedEntityTasks );
	}

private:
	BufferedPutEntityTask( MySqlDatabase & owner, const EntityDBKey & ekey,
			const EntityDBRecordIn & erec,
			IDatabase::IPutEntityHandler & handler ) :
		BufferedEntityTask( ekey.dbID ),
		owner_( owner ),
		ekey_( ekey ),
		erec_(),
		handler_( handler ),
		baseMB_(),
		pBaseMB_( NULL ),
		stream_()
	{
		if (erec.isBaseMBProvided())
		{
			const EntityMailBoxRef * pBaseMB = erec.getBaseMB();

			if (pBaseMB)
			{
				baseMB_ = *pBaseMB;
				pBaseMB_ = &baseMB_;
			}
			else
			{
				WARNING_MSG( "BufferedEntityTask::BufferedEntityTask: "
						"Unexpected NULL base mailbox\n" );
			}

			erec_.provideBaseMB( pBaseMB_ );
		}

		if (erec.isStrmProvided())
		{
			BinaryIStream & stream = erec.getStrm();
			stream_.transfer( stream, stream.remainingLength() );
			erec_.provideStrm( stream_ );
		}
	}

	MySqlDatabase & owner_;
	EntityDBKey ekey_;
	EntityDBRecordIn erec_;
	IDatabase::IPutEntityHandler & handler_;

	EntityMailBoxRef baseMB_;
	EntityMailBoxRef * pBaseMB_;
	MemoryOStream stream_;
};


/**
 *	Override from IDatabase
 */
void MySqlDatabase::putEntity( const EntityDBKey& ekey, EntityDBRecordIn& erec,
							   IPutEntityHandler& handler )
{
	MF_ASSERT( erec.isStrmProvided() || erec.isBaseMBProvided() );

	if (ekey.dbID == 0)
	{
		// Do not buffer since this is creating a new record.
		BufferedPutEntityTask::run( *this, ekey, erec, handler,
			   /*pBufferedEntityTasks:*/ NULL );
	}
	else if (pBufferedEntityTasks_->grabLock( ekey.dbID ))
	{
		BufferedPutEntityTask::run( *this, ekey, erec, handler,
			   pBufferedEntityTasks_ );
	}
	else
	{
		pBufferedEntityTasks_->buffer(
				BufferedPutEntityTask::create( *this, ekey, erec, handler ) );
	}
}


/**
 *	Override from IOldDatabase
 */
void OldMySqlDatabase::putEntity( const EntityDBKey& ekey,
	EntityDBRecordIn& erec, IDatabase::IPutEntityHandler& handler )
{
	MF_ASSERT( erec.isStrmProvided() || erec.isBaseMBProvided() );

	// Need backward compatibility for this entity only if it is one of those
	// that we've migrated.
	const TypeIDSet& changedTypes = pMigrationTask_->getChangedEntitiesOldTypes();
	if ( changedTypes.find( ekey.typeID ) != changedTypes.end() )
	{
		PutEntityTask<OldMySqlThreadTask>* pTask =
			new PutEntityTask<OldMySqlThreadTask>( *this, ekey, erec, handler, NULL );
		pTask->doTask();
	}
	else
	{
		// Otherwise, just use normal path because entity tables are unchanged
		// ... plus we don't have the migration scripts to migrate this entity
		// anyway.
		this->getOwner().putEntity( ekey, erec, handler );
	}
}

/**
 *	This class encapsulates the MySqlDatabase::delEntity() operation so that
 *	it can be executed in a separate thread.
 */
class DelEntityTask : public MySqlThreadTask
{
	IDatabase::IDelEntityHandler&	handler_;

public:
	DelEntityTask( MySqlDatabase& owner, const EntityDBKey& ekey,
		           IDatabase::IDelEntityHandler& handler );

	// WorkerThread::ITask overrides
	virtual void run();
	virtual void onRunComplete();
};

/**
 *	Constructor.
 */
DelEntityTask::DelEntityTask( MySqlDatabase& owner, const EntityDBKey& ekey,
							  IDatabase::IDelEntityHandler& handler )
	: MySqlThreadTask(owner), handler_(handler)
{
	this->startThreadTaskTiming();

	MySqlThreadData& threadData = this->getThreadData();

	threadData.ekey = ekey;
	threadData.isOK = true;
	threadData.exceptionStr.clear();

	// __kyl__ (27/9/2005) Due to the migration task needing the DBID, we
	// need to fetch it here prior to notifying the migration task.
	if (threadData.ekey.dbID == 0)
	{
		MySqlTransaction transaction( threadData.connection );
		threadData.ekey.dbID = threadData.typeMapping.getEntityDbID( transaction,
			ekey.typeID, ekey.name );
		transaction.commit();
	}

	owner.onDelEntityOpStarted( threadData.ekey.typeID, threadData.ekey.dbID );
}

/**
 *	This method deletes an entity from the database. May be executed in a
 *	separate thread.
 */
void DelEntityTask::run()
{
	bool retry;
	do
	{
		retry = false;
		MySqlThreadData&	threadData = this->getThreadData();
		EntityDBKey&		ekey = threadData.ekey;
		MySqlTypeMapping&	typeMapping = threadData.typeMapping;
		try
		{
			MySqlTransaction transaction( threadData.connection );
			if (ekey.dbID)
			{
				if (typeMapping.deleteEntityWithID( transaction, ekey.typeID,
					ekey.dbID ))
				{
						typeMapping.removeLogOnRecord( transaction, ekey.typeID, ekey.dbID );
				}
				else
				{
					threadData.isOK = false;
				}

				transaction.commit();
			}
			else
			{
				threadData.isOK = false;
			}
		}
		catch (MySqlRetryTransactionException& e)
		{
			retry = true;
		}
		catch (std::exception& e)
		{
			threadData.exceptionStr = e.what();
			threadData.isOK = false;
		}
	} while (retry);
}

/**
 *	This method is called in the main thread after run() completes.
 */
void DelEntityTask::onRunComplete()
{
	MySqlThreadData& threadData = this->getThreadData();

	if (threadData.exceptionStr.length())
		ERROR_MSG( "MySqlDatabase::delEntity: %s\n", threadData.exceptionStr.c_str() );
	else if (threadData.connection.hasFatalError())
		threadData.isOK = false;

	this->getOwner().onDelEntityOpCompleted( threadData.ekey.typeID,
		threadData.ekey.dbID );

	uint64 duration = this->stopThreadTaskTiming();
	if (duration > THREAD_TASK_WARNING_DURATION)
		WARNING_MSG( "DelEntityTask for entity %"FMT_DBID" of type %d "
					"took %f seconds\n",
					threadData.ekey.dbID, threadData.ekey.typeID,
					double(duration)/stampsPerSecondD() );

	// Release thread resources before callback, so that if callback decides
	// to do another operation that requires thread resource, it is not
	// deadlocked.
	bool isOK = threadData.isOK;
	IDatabase::IDelEntityHandler& handler = handler_;
	delete this;

	handler.onDelEntityComplete(isOK);
}

/**
 *	IDatabase override
 */
void MySqlDatabase::delEntity( const EntityDBKey & ekey,
	IDatabase::IDelEntityHandler& handler )
{
	DelEntityTask* pTask = new DelEntityTask( *this, ekey, handler );
	pTask->doTask();
}

/**
 *	This class encapsulates the MySqlDatabase::executeRawCommand() operation
 *	so that it can be executed in a separate thread.
 */
class ExecuteRawCommandTask : public MySqlThreadTask
{
	std::string									command_;
	IDatabase::IExecuteRawCommandHandler&		handler_;

public:
	ExecuteRawCommandTask( MySqlDatabase& owner, const std::string& command,
		IDatabase::IExecuteRawCommandHandler& handler )
		: MySqlThreadTask(owner), command_(command), handler_(handler)
	{
		this->startThreadTaskTiming();

		MySqlThreadData& threadData = this->getThreadData();

		threadData.isOK = true;
		threadData.exceptionStr.clear();
	}

	// WorkerThread::ITask overrides
	virtual void run();
	virtual void onRunComplete();
};

/**
 *	This method executes a raw database command. May be executed in a
 *	separate thread.
 */
void ExecuteRawCommandTask::run()
{
	MySqlThreadData& threadData = this->getThreadData();

	bool retry;
	do
	{
		retry = false;

		int errorNum;
		MySqlTransaction transaction( threadData.connection, errorNum );
		if (errorNum == 0)
		{
			errorNum = transaction.query( command_ );
			if (errorNum == 0)
			{
				MYSQL_RES* pResult = transaction.storeResult();
				if (pResult)
				{
					MySqlResult result( *pResult );
					BinaryOStream& stream = handler_.response();
					stream << std::string();	// no error.
					stream << uint32( result.getNumFields() );
					stream << uint32( result.getNumRows() );
					while ( result.getNextRow() )
					{
						for ( uint i = 0; i < result.getNumFields(); ++i )
						{
							DBInterfaceUtils::addPotentialNullBlobToStream(
									stream, DBInterfaceUtils::Blob(
										result.getField( i ),
										result.getFieldLen( i ) ) );
						}
					}
				}
				else
				{
					errorNum = transaction.getLastErrorNum();
					if (errorNum == 0)
					{
						// Result is empty. Return affected rows instead.
						BinaryOStream& stream = handler_.response();
						stream << std::string();	// no error.
						stream << int32( 0 ); 		// no fields.
						stream << uint64( transaction.affectedRows() );
					}
				}
			}
		}

		if (errorNum != 0)
		{
			if (transaction.shouldRetry())
			{
				retry = true;
			}
			else
			{
				threadData.exceptionStr = transaction.getLastError();
				threadData.isOK = false;

				handler_.response() << threadData.exceptionStr;
			}
		}
		else
		{
			transaction.commit();
		}
	} while (retry);
}

/**
 *	This method is called in the main thread after run() completes.
 */
void ExecuteRawCommandTask::onRunComplete()
{
	MySqlThreadData& threadData = this->getThreadData();
	if (threadData.exceptionStr.length())
		ERROR_MSG( "MySqlDatabase::executeRawCommand: %s\n",
			threadData.exceptionStr.c_str() );
	// __kyl__ (2/8/2006) Following 2 lines not necessary but nice for sanity.
//	else if (threadData.connection.hasFatalError())
//		threadData.isOK = false;

	uint64 duration = this->stopThreadTaskTiming();
	if (duration > THREAD_TASK_WARNING_DURATION)
		WARNING_MSG( "ExecuteRawCommandTask for '%s' took %f seconds\n",
					command_.c_str(), double(duration)/stampsPerSecondD() );

	// Release thread resources before callback, so that if callback decides
	// to do another operation that requires thread resource, it is not
	// deadlocked.
	IDatabase::IExecuteRawCommandHandler& handler = handler_;
	delete this;

	handler.onExecuteRawCommandComplete();
}

void MySqlDatabase::executeRawCommand( const std::string & command,
	IDatabase::IExecuteRawCommandHandler& handler )
{
	if (!pMigrationTask_ ||
		(pMigrationTask_ && pMigrationTask_->shouldAllowExecRawCmd( command )))
	{
		ExecuteRawCommandTask* pTask =
			new ExecuteRawCommandTask( *this, command, handler );
		pTask->doTask();
	}
	else
	{
		handler.response() << std::string( "Migration in progress" );
		handler.onExecuteRawCommandComplete();
	}
}

/**
 *	This class encapsulates the MySqlDatabase::putIDs() operation
 *	so that it can be executed in a separate thread.
 */
class PutIDsTask : public MySqlThreadTask
{
	int			numIDs_;
	ObjectID*	ids_;

public:
	PutIDsTask( MySqlDatabase& owner, int numIDs, const ObjectID * ids )
		: MySqlThreadTask(owner), numIDs_(numIDs), ids_(new ObjectID[numIDs])
	{
		this->startThreadTaskTiming();

		memcpy( ids_, ids, sizeof(ObjectID)*numIDs );

		this->getThreadData().exceptionStr.clear();
	}
	virtual ~PutIDsTask()
	{
		delete [] ids_;
	}

	// WorkerThread::ITask overrides
	virtual void run();
	virtual void onRunComplete();
};

/**
 *	This method puts unused IDs into the database. May be executed in a
 *	separate thread.
 */
void PutIDsTask::run()
{
	MySqlThreadData& threadData = this->getThreadData();
	try
	{
		MySqlLockedTables t( threadData.connection,
				"bigworldNewID WRITE, bigworldUsedIDs WRITE" );

		const ObjectID * ids = ids_;
		const ObjectID * end = ids_ + numIDs_;
		// TODO: ugh... make this not a loop somehow!
		while (ids != end)
		{
			threadData.boundID_ = *ids++;
			threadData.connection.execute( *threadData.putIDStatement_ );
		}
	}
	catch (std::exception& e)
	{
		threadData.exceptionStr = e.what();
	}
}

/**
 *	This method is called in the main thread after run() completes.
 */
void PutIDsTask::onRunComplete()
{
	MySqlThreadData& threadData = this->getThreadData();
	if (threadData.exceptionStr.length())
		// Oh crap! We just lost some IDs.
		// TODO: Store these IDs somewhere and retry later?
		ERROR_MSG( "MySqlDatabase::putIDs: %s\n",
			threadData.exceptionStr.c_str() );

	uint64 duration = this->stopThreadTaskTiming();
	if (duration > THREAD_TASK_WARNING_DURATION)
		WARNING_MSG( "PutIDsTask for %d IDs took %f seconds\n",
					numIDs_, double(duration)/stampsPerSecondD() );

	delete this;
}

void MySqlDatabase::putIDs( int numIDs, const ObjectID * ids )
{
	PutIDsTask* pTask = new PutIDsTask( *this, numIDs, ids );
	pTask->doTask();
}

/**
 *	This class encapsulates the MySqlDatabase::getIDs() operation
 *	so that it can be executed in a separate thread.
 */
class GetIDsTask : public MySqlThreadTask
{
	int							numIDs_;
	IDatabase::IGetIDsHandler&	handler_;

	int rangeSize_;
	int rangeEnd_;
	int numUsed_;

public:
	GetIDsTask( MySqlDatabase& owner, int numIDs,
		IDatabase::IGetIDsHandler& handler )
		: MySqlThreadTask(owner), numIDs_(numIDs), handler_(handler),
		rangeSize_( 0 ),
		rangeEnd_( 0 ),
		numUsed_( 0 )
	{
		this->startThreadTaskTiming();
		this->getThreadData().exceptionStr.clear();
	}

	// WorkerThread::ITask overrides
	virtual void run();
	virtual void onRunComplete();
};

/**
 *	This method gets some unused IDs from the database. May be executed in a
 *	separate thread.
 */
void GetIDsTask::run()
{
	MySqlThreadData& threadData = this->getThreadData();
	try
	{
		MySqlLockedTables t( threadData.connection,
				"bigworldNewID WRITE, bigworldUsedIDs WRITE" );

		BinaryOStream& strm = handler_.idStrm();

		// step 1. reuse any id's we can get our hands on
		threadData.boundLimit_ = numIDs_;
		threadData.connection.execute( *threadData.getIDsStatement_ );
		int numIDsRetrieved = threadData.getIDsStatement_->resultRows();

		numUsed_ = numIDsRetrieved;

		while (threadData.getIDsStatement_->fetch())
			strm << threadData.boundID_;

		if (numIDsRetrieved > 0)
			threadData.connection.execute( *threadData.delIDsStatement_ );

		threadData.boundLimit_ = numIDs_ - numIDsRetrieved;

		if (threadData.boundLimit_)
		{
			rangeSize_ = threadData.boundLimit_;

			threadData.connection.execute( *threadData.incIDStatement_ );
			threadData.connection.execute( *threadData.getIDStatement_ );
			threadData.getIDStatement_->fetch();

			rangeEnd_ = threadData.boundID_;

			while (numIDsRetrieved++ < numIDs_)
				strm << --threadData.boundID_;
		}
	}
	catch (std::exception& e)
	{
		threadData.exceptionStr = e.what();
	}
}

/**
 *	This method is called in the main thread after run() completes.
 */
void GetIDsTask::onRunComplete()
{
	MySqlThreadData& threadData = this->getThreadData();
	if (threadData.exceptionStr.length())
		ERROR_MSG( "MySqlDatabase::getIDs: %s\n",
			threadData.exceptionStr.c_str() );

	uint64 duration = this->stopThreadTaskTiming();
	if (duration > THREAD_TASK_WARNING_DURATION)
		WARNING_MSG( "GetIDsTask for %d IDs took %f seconds\n",
					numIDs_, double(duration)/stampsPerSecondD() );

	// Release thread resources before callback, so that if callback decides
	// to do another operation that requires thread resource, it is not
	// deadlocked.
	IDatabase::IGetIDsHandler& handler = handler_;
	delete this;

	INFO_MSG( "Got IDs: Num used: %d. New: From %d to %d\n",
			numUsed_, rangeEnd_ - rangeSize_, rangeEnd_ - 1 );

	handler.onGetIDsComplete();
}

void MySqlDatabase::getIDs( int numIDs, IDatabase::IGetIDsHandler& handler )
{
	GetIDsTask* pTask = new GetIDsTask( *this, numIDs, handler );
	pTask->doTask();
}


// -----------------------------------------------------------------------------
// Section: Space related
// -----------------------------------------------------------------------------

/**
 *	This method is called when a new round of spaces are going to be written.
 */
void MySqlDatabase::startSpaces()
{
	if (numWriteSpaceOpsInProgress_ > 0)
		this->endSpaces();
}

/**
 *	This class encapsulates the MySqlDatabase::writeSpace() operation
 *	so that it can be executed in a separate thread.
 */
class EndSpacesTask : public MySqlThreadTask
{
public:
	EndSpacesTask( MySqlDatabase& owner, uint8 spacesVersion )
		: MySqlThreadTask(owner)
	{
		this->startThreadTaskTiming();

		MySqlThreadData& threadData = this->getThreadData();
		threadData.spacesVersion_ = spacesVersion;
		threadData.exceptionStr.clear();
	}

	// WorkerThread::ITask overrides
	virtual void run();
	virtual void onRunComplete();
};

/**
 *	This method adds a space ID into the current list of space IDs.
 *	May be executed in a separate thread.
 */
void EndSpacesTask::run()
{
	MySqlThreadData& threadData = this->getThreadData();
	try
	{
		threadData.connection.execute( *threadData.endSpacesStatement_ );
		threadData.connection.execute( *threadData.endSpaceDataStatement_ );
	}
	catch (std::exception & e)
	{
		threadData.exceptionStr = e.what();
	}
}

/**
 *	This method is called in the main thread after run() completes.
 */
void EndSpacesTask::onRunComplete()
{
	MySqlThreadData& threadData = this->getThreadData();
	if (threadData.exceptionStr.length())
		// Will end up having some old space data in the table but should be
		// OK... just takes up more space. It will be deleted on the next
		// round of updating space data.
		ERROR_MSG( "MySqlDatabase::endSpaces: execute failed (%s)\n",
				   threadData.exceptionStr.c_str() );

	uint64 duration = this->stopThreadTaskTiming();
	if (duration > THREAD_TASK_WARNING_DURATION)
		WARNING_MSG( "EndSpacesTask for space version %d took %f seconds\n",
					threadData.spacesVersion_, double(duration)/stampsPerSecondD() );

	delete this;
}

/**
 *	This method ends the writing of space data.
 */
void MySqlDatabase::endSpaces()
{
	MF_ASSERT( numWriteSpaceOpsInProgress_ >= 0 );

	if (numWriteSpaceOpsInProgress_ > 0)
	{
		uint64 startTimestamp = timestamp();

		do
		{
			MF_ASSERT( pThreadResPool_->threadPool().getNumBusyThreads() > 0 );
			pThreadResPool_->threadPool().waitForOneTask();
		} while (numWriteSpaceOpsInProgress_ > 0);

		uint64 duration = timestamp() - startTimestamp;
		if (duration > THREAD_TASK_WARNING_DURATION)
			WARNING_MSG( "MySqlDatabase::endSpaces: Waited %f seconds for "
						"space version %d to finish writing\n",
						double(duration)/stampsPerSecondD(), spacesVersion_ );
	}

	EndSpacesTask* pTask = new EndSpacesTask( *this, spacesVersion_ );
	pTask->doTask();

	++spacesVersion_;
}

/**
 *	This class encapsulates the MySqlDatabase::writeSpace() operation
 *	so that it can be executed in a separate thread.
 */
class WriteSpaceTask : public MySqlThreadTask
{
public:
	WriteSpaceTask( MySqlDatabase& owner, SpaceID id, uint8 spacesVersion )
		: MySqlThreadTask(owner)
	{
		this->startThreadTaskTiming();

		MySqlThreadData& threadData = this->getThreadData();

		threadData.exceptionStr.clear();
		threadData.spacesVersion_ = spacesVersion;
		threadData.boundSpaceID_ = id;

		owner.onWriteSpaceOpStarted();
	}

	// WorkerThread::ITask overrides
	virtual void run();
	virtual void onRunComplete();
};

/**
 *	This method adds a space ID into the current list of space IDs.
 *	May be executed in a separate thread.
 */
void WriteSpaceTask::run()
{
	MySqlThreadData& threadData = this->getThreadData();
	try
	{
		threadData.connection.execute( *threadData.writeSpaceStatement_ );
	}
	catch (std::exception & e)
	{
		threadData.exceptionStr = e.what();
	}
}

/**
 *	This method is called in the main thread after run() completes.
 */
void WriteSpaceTask::onRunComplete()
{
	MySqlThreadData& threadData = this->getThreadData();
	if (threadData.exceptionStr.length())
		ERROR_MSG( "MySqlDatabase::writeSpace: "
						"execute failed for space %d (%s)\n",
				   threadData.boundSpaceID_, threadData.exceptionStr.c_str() );

	uint64 duration = this->stopThreadTaskTiming();
	if (duration > THREAD_TASK_WARNING_DURATION)
		WARNING_MSG( "WriteSpaceTask for space id %d, version %d took %f seconds\n",
					threadData.boundSpaceID_, threadData.spacesVersion_,
					double(duration)/stampsPerSecondD() );

	// Release thread resources before callback, so that if callback decides
	// to do another operation that requires thread resource, it is not
	// deadlocked.
	MySqlDatabase& owner = this->getOwner();
	delete this;

	owner.onWriteSpaceOpCompleted();
}

/**
 *	This method writes a space to the database.
 */
void MySqlDatabase::writeSpace( SpaceID id )
{
	WriteSpaceTask* pTask = new WriteSpaceTask( *this, id, spacesVersion_ );
	pTask->doTask();
}

/**
 *	This class encapsulates the MySqlDatabase::writeSpaceData() operation
 *	so that it can be executed in a separate thread.
 */
class WriteSpaceDataTask : public MySqlThreadTask
{
public:
	WriteSpaceDataTask( MySqlDatabase& owner, SpaceID spaceID, uint8 spacesVersion,
		int64 spaceKey, uint16 dataKey, const std::string & data )
		: MySqlThreadTask( owner )
	{
		this->startThreadTaskTiming();

		MySqlThreadData& threadData = this->getThreadData();

		threadData.exceptionStr.clear();
		threadData.spacesVersion_ = spacesVersion;
		threadData.boundSpaceID_ = spaceID;
		threadData.boundSpaceEntryID_ = spaceKey;
		threadData.boundSpaceDataKey_ = dataKey;
		threadData.boundSpaceData_.set( data.data(), data.size() );

		owner.onWriteSpaceOpStarted();
	}

	// WorkerThread::ITask overrides
	virtual void run();
	virtual void onRunComplete();
};

/**
 *	This method writes the space data into the database.
 *	May be executed in a separate thread.
 */
void WriteSpaceDataTask::run()
{
	MySqlThreadData& threadData = this->getThreadData();
	try
	{
		threadData.connection.execute( *threadData.writeSpaceDataStatement_  );
	}
	catch (std::exception & e)
	{
		threadData.exceptionStr = e.what();
	}
}

/**
 *	This method is called in the main thread after run() completes.
 */
void WriteSpaceDataTask::onRunComplete()
{
	MySqlThreadData& threadData = this->getThreadData();
	if (threadData.exceptionStr.length())
		ERROR_MSG( "MySqlDatabase::writeSpaceData: execute failed (%s)\n",
				   threadData.exceptionStr.c_str() );

	uint64 duration = this->stopThreadTaskTiming();
	if (duration > THREAD_TASK_WARNING_DURATION)
		WARNING_MSG( "WriteSpaceDataTask for space id %d, version %d, "
					"spacekey %"FMT_DBID" datakey %d took %f seconds\n",
					threadData.boundSpaceID_, threadData.spacesVersion_,
					threadData.boundSpaceEntryID_, threadData.boundSpaceDataKey_,
					double(duration)/stampsPerSecondD() );

	// Release thread resources before callback, so that if callback decides
	// to do another operation that requires thread resource, it is not
	// deadlocked.
	MySqlDatabase& owner = this->getOwner();
	delete this;

	owner.onWriteSpaceOpCompleted();
}


/**
 *	This method writes data associated with a space to the database.
 */
void MySqlDatabase::writeSpaceData( SpaceID spaceID,
		int64 spaceKey, uint16 dataKey, const std::string & data )
{
	WriteSpaceDataTask* pTask = new WriteSpaceDataTask( *this, spaceID,
			spacesVersion_, spaceKey, dataKey, data );
	pTask->doTask();
}


/**
 *	This method loads game state into the server during a controlled start-up.
 */
bool MySqlDatabase::restoreGameState()
{
	MySql& connection = this->getMainThreadData().connection;

	try
	{
		MySqlBindings paramBindings;
		MySqlBindings resultBindings;

		// TODO: Make this handle the case where we are halfway through
		// updating the space data i.e. there are multiple versions present.
		// In that case we should probably use the last complete version
		// instead of the latest incomplete version.
		MySqlStatement spacesStmt( connection,
								   "SELECT id from bigworldSpaces" );
		SpaceID spaceID;
		resultBindings << spaceID;
		spacesStmt.bindResult( resultBindings );

		MySqlStatement spaceDataStmt( connection,
			"SELECT spaceEntryID, entryKey, data "
					"FROM bigworldSpaceData where id = ?" );
		paramBindings << spaceID;
		spaceDataStmt.bindParams( paramBindings );

		uint64 boundSpaceEntryID;
		uint16 boundSpaceDataKey;
		MySqlBuffer boundSpaceData( maxSpaceDataSize_ );
		resultBindings.clear();
		resultBindings << boundSpaceEntryID;
		resultBindings << boundSpaceDataKey;
		resultBindings << boundSpaceData;
		spaceDataStmt.bindResult( resultBindings );

		connection.execute( spacesStmt );

		int numSpaces = spacesStmt.resultRows();
		std::vector< SpaceID > spaceIDs;
		spaceIDs.reserve( numSpaces );

		// Given that we're in a try block, I think it's prudent to assume that an
		// exception might come at any time, in which case the bundle shouldn't
		// be sent.  Therefore, we don't use a ChannelSender.
		Mercury::Bundle bundle;

		bundle.startMessage( BaseAppMgrInterface::prepareForRestoreFromDB );
		bundle << this->getGameTime();;
		bundle << numSpaces;

		INFO_MSG( "MySqlDatabase::restoreGameState: numSpaces = %d\n", numSpaces );

		for (int i = 0; i < numSpaces; ++i)
		{
			spacesStmt.fetch();
			spaceIDs.push_back( spaceID );
		}

		for (size_t i = 0; i < spaceIDs.size(); ++i)
		{
			spaceID = spaceIDs[i];
			bundle << spaceID;
			connection.execute( spaceDataStmt );

			int numData = spaceDataStmt.resultRows();
			bundle << numData;

			for (int dataIndex = 0; dataIndex < numData; ++dataIndex)
			{
				spaceDataStmt.fetch();
				bundle << boundSpaceEntryID;
				bundle << boundSpaceDataKey;
				bundle << boundSpaceData.getString();
			}
		}

		// We're clear of possible exceptions, so send the bundle now.
		Database::instance().baseAppMgr().send( &bundle );
	}
	catch (std::exception & e)
	{
		ERROR_MSG( "MySqlDatabase::restoreGameState: Restore spaces failed (%s)\n",
				e.what() );
		return false;
	}

	bool hasStarted = false;

	try
	{
		std::map< int, EntityTypeID > typeTranslation;

		// TODO: Should be able to do this in SQL.
		{
			// I'm not too sure why we keep two different values for entity type
			// id. Best guess is for updating. If the entity types change
			// indexes, the values in bigworldLogOns do not need to be changed.
			// It may be easier to just modify these values if this occurs.
			MySqlStatement typeStmt( connection,
					"SELECT typeID, bigworldID FROM bigworldEntityTypes" );
			MySqlBindings resultBindings;
			int dbTypeID;
			EntityTypeID bigworldTypeID;
			resultBindings << dbTypeID << bigworldTypeID;
			typeStmt.bindResult( resultBindings );
			connection.execute( typeStmt );

			int numResults = typeStmt.resultRows();

			for (int i = 0; i < numResults; ++i)
			{
				typeStmt.fetch();
				typeTranslation[ dbTypeID ] = bigworldTypeID;
			}
		}

		{
			MySqlStatement logOnsStmt( connection,
					"SELECT databaseID, typeID from bigworldLogOns" );
			MySqlBindings resultBindings;
			DatabaseID dbID;
			int dbTypeID;
			resultBindings << dbID << dbTypeID;
			logOnsStmt.bindResult( resultBindings );

			connection.execute( logOnsStmt );

			int numResults = logOnsStmt.resultRows();

			if (numResults > 0)
			{
				// Get the entities that have to be recovered.
				EntityRecoverer * pRecoverer =
					new EntityRecoverer( numResults );

				for (int i = 0; i < numResults; ++i)
				{
					logOnsStmt.fetch();
					EntityTypeID bwTypeID = typeTranslation[ dbTypeID ];
					pRecoverer->addEntity( bwTypeID, dbID );
				}

				connection.execute( "DELETE FROM bigworldLogOns" );
				pRecoverer->start();
			}
			else
			{
				hasStarted = true;
			}
		}
	}
	catch (std::exception & e)
	{
		ERROR_MSG( "MySqlDatabase::restoreGameState: Restore entities failed (%s)\n",
				e.what() );
	}

	return hasStarted;
}


/**
 *	This method returns the game time stored in the database.
 */
TimeStamp MySqlDatabase::getGameTime()
{
	MySqlThreadData& mainThreadData = this->getMainThreadData();
	mainThreadData.gameTime_ = 0;
	mainThreadData.connection.execute( *mainThreadData.getGameTimeStatement_ );
	mainThreadData.getGameTimeStatement_->fetch();
	return mainThreadData.gameTime_;
}

class SetGameTimeTask : public MySqlThreadTask
{
public:
	SetGameTimeTask( MySqlDatabase& owner, TimeStamp gameTime )
		: MySqlThreadTask(owner)
	{
		this->startThreadTaskTiming();

		MySqlThreadData& threadData = this->getThreadData();
		threadData.gameTime_ = gameTime;
		threadData.exceptionStr.clear();
	}

	// WorkerThread::ITask overrides
	virtual void run();
	virtual void onRunComplete();
};

/**
 *	This method updates the game time in the database. May be executed in a
 *	separate thread.
 */
void SetGameTimeTask::run()
{
	MySqlThreadData& threadData = this->getThreadData();
	try
	{
		threadData.connection.execute( *threadData.setGameTimeStatement_ );
	}
	catch (std::exception & e)
	{
		threadData.exceptionStr = e.what();
	}
}

void SetGameTimeTask::onRunComplete()
{
	MySqlThreadData& threadData = this->getThreadData();
	if (threadData.exceptionStr.length())
		ERROR_MSG( "MySqlDatabase::setGameTime: "
					"execute failed for time %d (%s)\n",
					threadData.gameTime_, threadData.exceptionStr.c_str() );

	uint64 duration = this->stopThreadTaskTiming();
	if (duration > THREAD_TASK_WARNING_DURATION)
		WARNING_MSG( "SetGameTimeTask for game time %u took %f seconds\n",
					threadData.gameTime_, double(duration)/stampsPerSecondD() );

	delete this;
}

/**
 *	This method sets the game time stored in the database.
 */
void MySqlDatabase::setGameTime( TimeStamp gameTime )
{
	SetGameTimeTask* pTask = new SetGameTimeTask( *this, gameTime );
	pTask->doTask();
}

/**
 *	Override from IDatabase.
 */
void MySqlDatabase::remapEntityMailboxes( const Mercury::Address& srcAddr,
		const BackupHash & destAddrs )
{
	try
	{
		MySql& connection = this->getMainThreadData().connection;

		std::stringstream updateStmtStrm;
		updateStmtStrm << "UPDATE bigworldLogOns SET ip=?, port=? WHERE ip="
				<< ntohl( srcAddr.ip ) << " AND port=" << ntohs( srcAddr.port )
				<< " AND ((((objectID * " << destAddrs.prime()
				<< ") % 0x100000000) >> 8) % " << destAddrs.virtualSize()
				<< ")=?";

//		DEBUG_MSG( "MySqlDatabase::remapEntityMailboxes: %s\n",
//				updateStmtStrm.str().c_str() );

		MySqlStatement updateStmt( connection, updateStmtStrm.str() );
		uint32 	boundAddress;
		uint16	boundPort;
		int		i;

		MySqlBindings params;
		params << boundAddress << boundPort << i;

		updateStmt.bindParams( params );

		// Wait for all tasks to complete just in case some of them updates
		// bigworldLogOns.
		MySqlThreadResPool& threadResPool = this->getThreadResPool();
		threadResPool.threadPool().waitForAllTasks();

		for (i = 0; i < int(destAddrs.size()); ++i)
		{
//			DEBUG_MSG( "MySqlDatabase::remapEntityMailboxes: %s\n",
//					(char *) destAddrs[i] );
			boundAddress = ntohl( destAddrs[i].ip );
			boundPort = ntohs( destAddrs[i].port );

			connection.execute( updateStmt );
		}
		for (i = int(destAddrs.size()); i < int(destAddrs.virtualSize()); ++i)
		{
			int realIdx = i/2;
//			DEBUG_MSG( "MySqlDatabase::remapEntityMailboxes (round 2): %s\n",
//					(char *) destAddrs[realIdx] );
			boundAddress = ntohl( destAddrs[realIdx].ip );
			boundPort = ntohs( destAddrs[realIdx].port );

			connection.execute( updateStmt );
		}
	}
	catch (std::exception& e)
	{
		ERROR_MSG( "MySqlDatabase::remapEntityMailboxes: Remap entity "
				"mailboxes failed (%s)\n", e.what() );
	}
}

/**
 *	This method starts the migration to new entity definitions.
 */
void MySqlDatabase::migrateToNewDefs( const EntityDefs& newEntityDefs,
	IDatabase::IMigrationHandler& handler )
{
	if (!pMigrationTask_ && !pOldDatabase_)
	{
		pMigrationTask_ = new MigrateToNewDefsTask( newEntityDefs, *this,
								handler );
		pMigrationTask_->startMigration();
	}
	else
	{
		handler.onMigrateToNewDefsComplete( false );
	}
}

/**
 *	This method completes the migration to new entity definitions.
 */
IDatabase::IOldDatabase* MySqlDatabase::switchToNewDefs(
	const EntityDefs& oldEntityDefs )
{
	MF_ASSERT( !pOldDatabase_ );
	if (pMigrationTask_ && pMigrationTask_->waitForMigrationCompletion())
	{
		// Wait for all other outstanding tasks to complete. This will free up
		// all the thread resources.
		MySqlThreadResPool& threadResPool = this->getThreadResPool();
		threadResPool.threadPool().waitForAllTasks();

		// Swap temp tables with real tables. And swap Python interpreter
		// with one that has loaded the new user type modules.
		pMigrationTask_->switchToNewDefs();

		// Loop through thread resources and switch them to the new definitions.
		const EntityDefs& newEntityDefs = pMigrationTask_->getNewEntityDefs();
		std::vector<MySqlThreadData*> threadDataList;
		MySqlThreadData* pCurThreadData;
		while ((pCurThreadData = threadResPool.acquireThreadData(0)) != 0)
		{
			pCurThreadData->typeMapping.clearTemporaryMappings();
			pCurThreadData->typeMapping.setEntityMappings(
				pCurThreadData->connection, newEntityDefs );
			threadDataList.push_back( pCurThreadData );
		}
		for ( std::vector<MySqlThreadData*>::iterator i = threadDataList.begin();
				i < threadDataList.end(); ++i )
		{
			threadResPool.releaseThreadData( **i );
		}
		pCurThreadData = &this->getMainThreadData();
		pCurThreadData->typeMapping.clearTemporaryMappings();
		pCurThreadData->typeMapping.setEntityMappings(
			pCurThreadData->connection, newEntityDefs );

		pOldDatabase_ = new OldMySqlDatabase( *pMigrationTask_,
			oldEntityDefs, maxSpaceDataSize_ );
		pMigrationTask_ = NULL;	// OldMySqlDatabase will delete this.
	}

	return pOldDatabase_;
}

/**
 *	This method sets up the threads to also update the temporary tables.
 */
void MySqlDatabase::setupUpdateTemporaryTables()
{
	// Wait for all other outstanding tasks to complete. This will free up
	// all the thread resources.
	MySqlThreadResPool& threadResPool = this->getThreadResPool();
	threadResPool.threadPool().waitForAllTasks();

	MF_ASSERT( pMigrationTask_ );

	// Loop through thread resources and set up the temporary mappings.
	std::vector<MySqlThreadData*> threadDataList;
	MySqlThreadData* pCurThreadData;
	while ((pCurThreadData = threadResPool.acquireThreadData(0)) != 0)
	{
		pCurThreadData->typeMapping.setTemporaryMappings(
			pCurThreadData->connection, pMigrationTask_->getNewEntityDefs(),
			TEMP_TABLE_NAME_PREFIX, *pMigrationTask_ );
		threadDataList.push_back( pCurThreadData );
	}
	for ( std::vector<MySqlThreadData*>::iterator i = threadDataList.begin();
			i < threadDataList.end(); ++i )
	{
		threadResPool.releaseThreadData( **i );
	}
	pCurThreadData = &this->getMainThreadData();
	pCurThreadData->typeMapping.setTemporaryMappings(
			pCurThreadData->connection, pMigrationTask_->getNewEntityDefs(),
			TEMP_TABLE_NAME_PREFIX, *pMigrationTask_ );
}

inline void MySqlDatabase::onPutEntityOpStarted( EntityTypeID typeID,
	DatabaseID dbID )
{
	if (pMigrationTask_ && dbID)
		pMigrationTask_->onUpdateEntity( typeID, dbID );
}

inline void MySqlDatabase::onPutEntityOpCompleted( EntityTypeID typeID,
	DatabaseID dbID )
{
	if (pMigrationTask_ && dbID)
		pMigrationTask_->onUpdateEntityComplete( typeID, dbID );
}

inline void MySqlDatabase::onDelEntityOpStarted( EntityTypeID typeID,
	DatabaseID dbID )
{
	if (pMigrationTask_)
		pMigrationTask_->onDeleteEntity( typeID, dbID );
}

inline void MySqlDatabase::onDelEntityOpCompleted( EntityTypeID typeID,
	DatabaseID dbID )
{
	if (pMigrationTask_)
		pMigrationTask_->onDeleteEntityComplete( typeID, dbID );
}

// mysql_database.cpp
