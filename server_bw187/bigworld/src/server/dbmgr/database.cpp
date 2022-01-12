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

#include "database.hpp"
#include "db_interface.hpp"
#include "db_interface_utils.hpp"

#include "baseappmgr/baseappmgr_interface.hpp"
#include "baseapp/baseapp_int_interface.hpp"
#include "updater/updater_interface.hpp"

#include "cstdmf/memory_stream.hpp"

#include "resmgr/bwresource.hpp"
#include "resmgr/dataresource.hpp"
#include "resmgr/bwresource.hpp"

#include "server/bwconfig.hpp"
#include "server/reviver_subject.hpp"
#include "server/writedb.hpp"

#include "network/watcher_glue.hpp"

#include "entitydef/entity_description_debug.hpp"

#include "pyscript/py_output_writer.hpp"

#ifdef USE_ORACLE
#include "oracle_database.hpp"
#endif

#ifdef USE_MYSQL
#include "mysql_database.hpp"
#endif

#ifdef USE_XML
#include "xml_database.hpp"
#endif

#include "resmgr/xml_section.hpp"

#include <signal.h>

#ifndef CODE_INLINE
#include "database.ipp"
#endif

#include "custom.hpp"

DECLARE_DEBUG_COMPONENT( 0 )

// -----------------------------------------------------------------------------
// Section: Constants
// -----------------------------------------------------------------------------
static const char * DEFAULT_ENTITY_TYPE_STR = "Avatar";
static const char * DEFAULT_NAME_PROPERTY_STR = "playerName";
static const char * UNSPECIFIED_ERROR_STR = "Unspecified error";
static const std::string EMPTY_STR;

// -----------------------------------------------------------------------------
// Section: Functions
// -----------------------------------------------------------------------------
// Handles SIGINT and SIGHUP.
static void signalHandler( int /*sigNum*/ )
{
	Database * pDB = Database::pInstance();

	if (pDB != NULL)
	{
		pDB->shutDown();
	}
}

// extern int Math_token;
extern int ResMgr_token;
static int s_moduleTokens =
	/* Math_token |*/ ResMgr_token;
extern int PyPatrolPath_token;
static int s_patrolToken = PyPatrolPath_token;

// -----------------------------------------------------------------------------
// Section: EntityDefs
// -----------------------------------------------------------------------------
/**
 * 	This function initialises EntityDefs. Must be called once and only once for
 * 	each instance of EntityDefs.
 *
 * 	@param	pEntitiesSection	The entities.xml data section.
 * 	@param	defaultTypeName		The default entity type.
 * 	@param	defaultNameProperty		The default name property.
 * 	@return	True if successful.
 */
bool EntityDefs::init( DataSectionPtr pEntitiesSection,
	const std::string& defaultTypeName, const std::string& defaultNameProperty )
{
	struct LocalHelperFunctions
	{
		// Sets output to dataDesc.name() only if it is a STRING or BLOB.
		void setNameProperty( std::string& output,
				const DataDescription& dataDesc,
				const EntityDescription& entityDesc )
		{
			const DataType* pDataType = dataDesc.dataType();
			if ((strcmp( pDataType->pMetaDataType()->name(), "BLOB" ) == 0) ||
				(strcmp( pDataType->pMetaDataType()->name(), "STRING" ) == 0))
			{
				output = dataDesc.name();
			}
			else
			{
				ERROR_MSG( "EntityDefs::init: Identifier must be of "
						"STRING or BLOB type. Identifier '%s' "
						"for entity type '%s' is ignored\n",
						dataDesc.name().c_str(),
						entityDesc.name().c_str() );
			}
		}
	} helper;

	MF_ASSERT( (entityDescriptionMap_.size() == 0) && pEntitiesSection );
	if (!entityDescriptionMap_.parse( pEntitiesSection ))
	{
		ERROR_MSG( "EntityDefs::init: "
				"Could not parse 'entities/entities.xml'\n" );
		return false;
	}

	defaultNameProperty_ = defaultNameProperty;

	// Set up some entity def stuff specific to DbMgr
	nameProperties_.resize( entityDescriptionMap_.size() );
	entityPasswordBits_.resize( entityDescriptionMap_.size() );
	for ( EntityTypeID i = 0; i < entityDescriptionMap_.size(); ++i )
	{
		const EntityDescription& entityDesc =
				entityDescriptionMap_.entityDescription(i);
		// Remember whether it has a password property.
		entityPasswordBits_[i] = (entityDesc.findProperty( "password" ) != 0);

		// Find its name property.
		const DataDescription* 	pDefaultNameDataDesc = NULL;
		std::string&			nameProperty = nameProperties_[i];
		for ( unsigned int i = 0; i < entityDesc.propertyCount(); ++i)
		{
			const DataDescription* pDataDesc = entityDesc.property( i );
			if (pDataDesc->isIdentifier())
			{
				if (nameProperty.empty())
				{
					helper.setNameProperty( nameProperty, *pDataDesc,
							entityDesc );
				}
				else // We don't support having multiple to name columns.
				{
					ERROR_MSG( "EntityDefs::init: Multiple identifiers for "
							"one entity type not supported. Identifier '%s' "
							"for entity type '%s' is ignored\n",
							pDataDesc->name().c_str(),
							entityDesc.name().c_str() );
				}
			}
			else if ( pDataDesc->name() == defaultNameProperty )
			{	// For backward compatiblity, we use the default name property
				// if none of the properties of the entity is an identifier.
				pDefaultNameDataDesc = pDataDesc;
			}
		}
		// Backward compatibility.
		if (nameProperty.empty() && pDefaultNameDataDesc)
		{
			helper.setNameProperty( nameProperty, *pDefaultNameDataDesc,
					entityDesc );
		}
	}

	entityDescriptionMap_.nameToIndex( defaultTypeName, defaultTypeID_ );

	MD5 md5;
	entityDescriptionMap_.addToMD5( md5 );
	md5.getDigest( md5Digest_ );

	return true;
}

const std::string& EntityDefs::getDefaultTypeName() const
{
	return (this->isValidEntityType( this->getDefaultType() )) ?
				this->getEntityDescription( this->getDefaultType() ).name() :
				EMPTY_STR;
}

/**
 *	This function returns the type name of the given property.
 */
std::string EntityDefs::getPropertyType( EntityTypeID typeID,
	const std::string& propName ) const
{
	const EntityDescription& entityDesc = this->getEntityDescription(typeID);
	DataDescription* pDataDesc = entityDesc.findProperty( propName );
	return ( pDataDesc ) ? pDataDesc->dataType()->typeName() : std::string();
}

/**
 *	This function prints out information about the entity defs.
 */
void EntityDefs::debugDump( int detailLevel ) const
{
	EntityDescriptionDebug::dump( entityDescriptionMap_, detailLevel );
}

/**
 *	This function checks that all the entities with name properties has the
 * 	same name properties in the other entity defs. Mainly used in updater
 * 	because we currently do not support migrating the name property.
 */
bool EntityDefs::hasMatchingNameProperties( const EntityDefs& other ) const
{
	EntityTypeID ourMaxTypeID = (EntityTypeID) this->getNumEntityTypes();
	for ( EntityTypeID ourTypeID = 0; ourTypeID < ourMaxTypeID; ++ourTypeID )
	{
		const EntityDescription& ourEntityDesc =
				entityDescriptionMap_.entityDescription( ourTypeID );
		EntityTypeID theirTypeID = other.getEntityType( ourEntityDesc.name() );
		if ((theirTypeID != INVALID_TYPEID) &&
			(this->getNameProperty( ourTypeID ) !=
				other.getNameProperty( theirTypeID )))
		{
			return false;
		}
	}

	return true;
}



// -----------------------------------------------------------------------------
// Section: DbUpdater
// -----------------------------------------------------------------------------
/**
 *	This class handles most of the updater functions in DbMgr.
 */
class DbUpdater : // public Mercury::BundleFinishHandler,
					public IDatabase::IMigrationHandler
{
	Mercury::Address	updaterAddr_;
	Mercury::Nub&		nub_;	// Share with Database
	EntityDefs*			pEntityDefs_;
	uint32				oldVersion_;
	uint32				impendingVersion_;

	int					bundleFinishHandlerId_;
	uint32				bundleResourceVersionTag_;
	uint32				defaultBundleResourceVersionTag_;

	IDatabase::IOldDatabase*	pOldDatabase_;
	bool				hasSwitchedToNewRes_;

public:
	// Updater steps
	enum ResVerCtrlActivity
	{
		// Updater started.
		ResVerCtrlStart,
		// Updater is gathering list of servers participating in update.
		ResVerCtrlGatherParticipatingServers,
		// Updater announces new version of resources. New resources sent to
		// clients.
		ResVerCtrlNewResouceAvailable,
		// Client logins prevented.
		ResVerCtrlStopClientLogins,
		// Stop creation of any additional server components.
		ResVerCtrlStopServerSpawns,
		// Begin loading new resources.
		ResVerCtrlLoadNewResource,
		// Switch over to new resources. Entity of different versions may be
		// on the network. Assume untagged entities to be old version.
		ResVerCtrlSwitchToNewResource,
		// Completed switch to new resources. Entities on the network are
		// from the new version (except for some clients). Assume untagged
		// entities to be new version. Allow new client logins.
		ResVerCtrlSwitchToNewResourceComplete,
		// End support for any clients still not using the resources.
		ResVerCtrlEndOldResourceSupport,
		// End of update process. Allow creation of new server components.
		ResVerCtrlEnd
	};

	// Constructor.
	DbUpdater( const Mercury::Address& updaterAddr, Mercury::Nub& nub,
			const DBInterface::resourceVersionControlArgs& args ) :
		updaterAddr_( updaterAddr ), nub_(nub),
		pEntityDefs_( 0 ), oldVersion_( args.version ),
		impendingVersion_( INVALID_RESOURCE_VERSION ),
		bundleFinishHandlerId_( -1 ),
		bundleResourceVersionTag_( INVALID_RESOURCE_VERSION ),
		defaultBundleResourceVersionTag_( INVALID_RESOURCE_VERSION ),
		pOldDatabase_( 0 ), hasSwitchedToNewRes_(false)
	{
		MF_ASSERT( args.activity == ResVerCtrlStart );
	}

	virtual ~DbUpdater()
	{
		delete pEntityDefs_;
		// __kyl__ (7/10/2005) Don't delete pOldDatabase_ even though it could
		// be non-NULL if we are deleted in the middle of an update. Deleting
		// pOldDatabase_ indicates completion of migration process and causes
		// old entity data to be cleaned - probably not what we want if we are
		// deleted prior to completing the update.
		// delete pOldDatabase_;	// Should be NULL normally
	}

	// Because DbUpdater doesn't really do anything for many updater steps,
	// it's probably OK to restart it if it hasn't started migrating.
	bool isOKToRestart() const	{	return !pEntityDefs_;	}

	void sendAckToUpdater( uint32 stepNum );

	void onRcvResourceVersionControl(
		const DBInterface::resourceVersionControlArgs& args );

	bool hasRegisteredBundleFinishHandler() const
	{	return bundleFinishHandlerId_ >= 0;	}
	bool registerBundleFinishHandler();
	bool deregisterBundleFinishHandler();

	bool hasReceivedResourceVersionTag() const
	{	return bundleResourceVersionTag_ != INVALID_RESOURCE_VERSION;	}
	// Returns the version of the current bundle. Returns
	// INVALID_RESOURCE_VERSION there is no need to care about the bundle
	// version (e.g. we haven't begun switching resources).
	uint32 getBundleResourceVersion() const
	{
		return (hasReceivedResourceVersionTag())
				? bundleResourceVersionTag_ : defaultBundleResourceVersionTag_;
	}

	// Returns IOldDatabase if the current bundle has old version. Returns
	// NULL if bundle is new version or if we don't care about bundle version
	// yet (e.g. we haven't begun switching resources)
	IDatabase::IOldDatabase* getOldDatabaseForBundle() const
	{
		return ( getBundleResourceVersion() == oldVersion_ )
					? pOldDatabase_ : 0;
	}

	// Database calls this function when it receives a resource version tag.
	void onRcvResourceVersionTag( uint32 resourceVersion );

	// Mercury::BundleFinishHandler override
	virtual void bundleFinished();

	// IMigrationHandler override
	virtual void onMigrateToNewDefsComplete( bool isOK );

private:
	bool switchToNewRes();
};

/**
 *	This function handles a control message from the updater.
 */
void DbUpdater::onRcvResourceVersionControl(
	const DBInterface::resourceVersionControlArgs& args )
{
	switch (args.activity)
	{
		case ResVerCtrlGatherParticipatingServers:
			this->sendAckToUpdater( ResVerCtrlGatherParticipatingServers );
			break;
		case ResVerCtrlNewResouceAvailable:
			impendingVersion_ = args.version;
			this->sendAckToUpdater( ResVerCtrlNewResouceAvailable );
			break;
		case ResVerCtrlStopClientLogins:
			this->sendAckToUpdater( ResVerCtrlStopClientLogins );
			break;
		case ResVerCtrlStopServerSpawns:
			// no ack need be sent here
			break;
		case ResVerCtrlLoadNewResource:
		{
			// BWResource should be set correctly before commencing this step.
			DataSectionPtr pSection =
				BWResource::openSection( "entities/entities.xml" );
			const EntityDefs& oldEntityDefs =
				Database::instance().getEntityDefs();
			pEntityDefs_ = new EntityDefs();
			if ( pSection &&
				pEntityDefs_->init( pSection, oldEntityDefs.getDefaultTypeName(),
									oldEntityDefs.getDefaultNameProperty() ) )
			{
				// NOTE: Need to register this before
				// ResVerCtrlSwitchToNewResource because there is a race
				// condition where some components may
				// receive ResVerCtrlSwitchToNewResource notification before
				// us so multiple version of entities may be on the network.
				this->registerBundleFinishHandler();

				Database::instance().getIDatabase().
					migrateToNewDefs( *pEntityDefs_, *this );
				// onMigrateToNewDefsComplete() will be called when
				// migrateToNewDefs() completes.
			}
			else
			{
				ERROR_MSG( "DbUpdater::onRcvResourceVersionControl: "
					"Failed to load new entity definitions\n" );
				// Don't send ack. Hopefully updater will timeout and do
				// something sensible.
			}
			break;
		}
		case ResVerCtrlSwitchToNewResource:
			if (this->switchToNewRes())
				this->sendAckToUpdater( ResVerCtrlSwitchToNewResource );
			// else
				// Don't send ack. Hopefully updater will timeout and do
				// something sensible.
			break;
		case ResVerCtrlSwitchToNewResourceComplete:
			defaultBundleResourceVersionTag_ = impendingVersion_;
			this->sendAckToUpdater( ResVerCtrlSwitchToNewResourceComplete );
			break;
		case ResVerCtrlEndOldResourceSupport:
			// NOTE: Need to deregister after previous step due to race
			// conditions with other components. We may still receive messages
			// with version tags after notification of
			// ResVerCtrlSwitchToNewResourceComplete because other components
			// may not have received it yet.
			this->deregisterBundleFinishHandler();
			defaultBundleResourceVersionTag_ = INVALID_RESOURCE_VERSION;
			delete pOldDatabase_;
			pOldDatabase_ = 0;
			this->sendAckToUpdater( ResVerCtrlEndOldResourceSupport );
			break;
		case ResVerCtrlEnd:
			this->sendAckToUpdater( ResVerCtrlEnd );
			break;
		default:
			ERROR_MSG( "DbUpdater::onRcvResourceVersionControl: "
				"Received unknown activity %lu\n", args.activity );
			break;
	}
}

/**
 *	This method sends an ack message to the updater.
 */
void DbUpdater::sendAckToUpdater( uint32 stepNum )
{
	Mercury::Bundle bundle;
	UpdaterInterface::ackStepArgs msg = { stepNum };
	bundle << msg;
	nub_.send( updaterAddr_, bundle );
}

/**
 *	This function registers this DbUpdater as a BundleFinishHandler to
 * 	the Nub.
 */
bool DbUpdater::registerBundleFinishHandler()
{
	bool isSuccess = !this->hasRegisteredBundleFinishHandler();
	if (isSuccess)
	{
		// bundleFinishHandlerId_ = nub_.registerBundleFinishHandler( this );
	}
	else
		ERROR_MSG( "DbUpdater::registerBundleFinishHandler: "
			"Cannot register twice\n" );
	return isSuccess;
}

/**
 *	This function deregisters this DbUpdater as a BundleFinishHandler from
 * 	the Nub.
 */
bool DbUpdater::deregisterBundleFinishHandler()
{
	bool isOK = this->hasRegisteredBundleFinishHandler();
	if (isOK)
	{
		// nub_.deregisterBundleFinishHandler( bundleFinishHandlerId_ );
		bundleFinishHandlerId_ = -1;
		bundleResourceVersionTag_ = INVALID_RESOURCE_VERSION;
	}
	else
	{
		ERROR_MSG( "DbUpdater::deregisterBundleFinishHandler: "
			"Cannot deregister twice\n" );
	}
	return isOK;
}

/**
 *	This function is called by the Database when it receives a resource
 * 	version tag.
 */
void DbUpdater::onRcvResourceVersionTag( uint32 resourceVersion )
{
	// Shouldn't be receiving version Tags any more after we deregister since
	// there is no one to reset the version after the end of the bundle.
	MF_ASSERT( this->hasRegisteredBundleFinishHandler() );
	bundleResourceVersionTag_ = resourceVersion;
	if ( !hasSwitchedToNewRes_ && (resourceVersion == impendingVersion_) )
	{
		// New format messages must've overtaken the
		// ResVerCtrlSwitchToNewResource notification
		TRACE_MSG( "Switching to new resource due to message with new version!\n" );
		this->switchToNewRes();
	}
}

/**
 *	BundleFinishHandler override
 */
void DbUpdater::bundleFinished()
{
	bundleResourceVersionTag_ = INVALID_RESOURCE_VERSION;
}

/**
 *	IMigrationHandler override
 */
void DbUpdater::onMigrateToNewDefsComplete( bool isOK )
{
	if (isOK)
	{
		this->sendAckToUpdater( ResVerCtrlLoadNewResource );
	}
	else
	{
		ERROR_MSG( "DbUpdater::onMigrateToNewDefsComplete: "
			"Failed to migrate to new entity definitions\n" );
		// Don't send ack. Hopefully updater will timeout and do
		// something sensible.
	}
}

/**
 *	This function switches database to use the new resources.
 *
 * 	@return	True if successful.
 */
bool DbUpdater::switchToNewRes()
{
	if (!hasSwitchedToNewRes_)
	{
		hasSwitchedToNewRes_ = true;

		defaultBundleResourceVersionTag_ = oldVersion_;

		pOldDatabase_ = Database::instance().getIDatabase().switchToNewDefs(
							Database::instance().getEntityDefs() );
		if ( !pOldDatabase_ )
			ERROR_MSG( "DbUpdater::onRcvResourceVersionControl:	Failed to "
				"switch to new resources\n" );
		else
			pEntityDefs_ = &Database::instance().swapEntityDefs( *pEntityDefs_ );
		// TODO: Switch BWResource to new resources?
		// TODO: Re-init BWConfig with new resources?
	}
	return pOldDatabase_ ? true : false;
}

// -----------------------------------------------------------------------------
// Section: Database
// -----------------------------------------------------------------------------

Database * Database::pInstance_ = NULL;

/**
 *	Constructor.
 */
Database::Database() :
	nub_( 0, BW_INTERNAL_INTERFACE( dbMgr ) ),
	workerThreadMgr_( nub_ ),
	pEntityDefs_( NULL ),
	pDatabase_( NULL ),
	pDbUpdater_( NULL ),
	pUpdatedBWResource_( NULL ),
	baseAppMgr_( nub_ ),
	shouldLoadUnknown_( true ),
	shouldCreateUnknown_( true ),
	shouldRememberUnknown_( true ),
	defsSyncMode_( DefsSyncModeNoSync ),
	allowEmptyDigest_( true ), // Should probably default to false.
	shutDownBaseAppMgrOnExit_( false ),
	hasStartBegun_( false ),
	hasStarted_( false ),
	desiredBaseApps_( 1 ),
	desiredCellApps_( 1 ),
	statusCheckTimerId_( -1 ),
	clearRecoveryDataOnStartUp_( true ),
	writeEntityTimer_( 5 ),
	curLoad_( 1.f ),
	maxLoad_( 1.f ),
	anyCellAppOverloaded_( true )
{
	// Can only have 1 instance of this class.
	MF_ASSERT( pInstance_ == NULL );
	pInstance_ = this;

	// The channel to the BaseAppMgr is irregular
	baseAppMgr_.channel().isIrregular( true );
}


/**
 *	Destructor.
 */
Database::~Database()
{
	delete pDatabase_;
	// Destroy entity descriptions before calling Script::fini() so that it
	// can clean up any PyObjects that it may have.
	delete pEntityDefs_;
	DataType::clearStaticsForReload();
	Script::fini();
	pDatabase_ = NULL;
	pInstance_ = NULL;
}


/**
 *	This method initialises this object. It should be called before any other
 *	method.
 */
Database::InitResult Database::init( bool isRecover, bool isUpgrade,
		DefsSyncMode defsSyncMode, bool updateSelfIndex )
{
    updateSelfIndex_ = updateSelfIndex;

	if (nub_.socket() == -1)
	{
		ERROR_MSG( "Failed to create Nub on internal interface.\n");
		return InitResultFailure;
	}

	ReviverSubject::instance().init( &nub_, "dbMgr" );

	if (!Script::init( "entities/db", "database" ))
	{
		return InitResultFailure;
	}

	PyObject * module = PyImport_ImportModule( "encodings" );

	if (module)
	{
		Py_DECREF( module );
	}

	std::string defaultTypeName = DEFAULT_ENTITY_TYPE_STR;
	std::string nameProperty;

	int dumpLevel = 0;

	BWConfig::update( "dbMgr/allowEmptyDigest", allowEmptyDigest_ );
	BWConfig::update( "dbMgr/shutDownBaseAppMgrOnExit",
			shutDownBaseAppMgrOnExit_ );
	BWConfig::update( "dbMgr/loadUnknown", shouldLoadUnknown_ );
	BWConfig::update( "dbMgr/createUnknown", shouldCreateUnknown_ );
	BWConfig::update( "dbMgr/rememberUnknown", shouldRememberUnknown_ );

	BWConfig::update( "dbMgr/entityType", defaultTypeName );
	BWConfig::update( "dbMgr/nameProperty", nameProperty );

	if (nameProperty.empty())
	{
		nameProperty = DEFAULT_NAME_PROPERTY_STR;
	}
	else
	{
		INFO_MSG( "dbMgr/nameProperty has been deprecated. Please add the "
					"attribute <Identifier> true </Identifier> to the name "
					"property of the entity\n" );
	}

	BWConfig::update( "dbMgr/dumpEntityDescription", dumpLevel );

	BWConfig::update( "desiredBaseApps", desiredBaseApps_ );
	BWConfig::update( "desiredCellApps", desiredCellApps_ );

	BWConfig::update( "dbMgr/clearRecoveryData", clearRecoveryDataOnStartUp_ );

	bool isAutoShutdown = false;

	if (isUpgrade)
		defsSyncMode = DefsSyncModeFullSync;

	if (defsSyncMode == DefsSyncModeInvalid)
	{
		bool isSyncTablesToDefs = true;
		BWConfig::update( "dbMgr/syncTablesToDefs", isSyncTablesToDefs );
		defsSyncMode_ = isSyncTablesToDefs ? DefsSyncModeFullSync : DefsSyncModeNoSync;

		// __kyl__ (12/12/2005) Used to have defsSyncMode 0,1,2,3 but most users
		// gets very confused about it. Now just have a boolean which maps to
		// mode 0 and 3. Should probably remove redundant code if customers
		// don't show any interest in the other modes.
//		int newDefsSyncMode = defsSyncMode_;
//		BWConfig::update( "dbMgr/defsSyncMode", newDefsSyncMode );
//		if (!Database::isValidDefsSyncMode( newDefsSyncMode ))
//		{
//			WARNING_MSG( "Invalid defsSyncMode %d. Using mode %d\n",
//						newDefsSyncMode, defsSyncMode_ );
//		}
//		else
//		{
//			defsSyncMode_ = newDefsSyncMode;
//		}
	}
	else
	{
		defsSyncMode_ = defsSyncMode;

		// Shutdown DBMgr right after starting if any of the command-line
		// parameters "-upgrade" or "-syncTablesToDefs" are used.
		isAutoShutdown = true;
	}

	BWConfig::update( "dbMgr/overloadLevel", maxLoad_ );

	PyOutputWriter::overrideSysMembers(
		BWConfig::get( "dbMgr/writePythonLog", false ) );

	BW_INIT_WATCHER_DOC( "dbmgr" );

	MF_WATCH( "allowEmptyDigest",	allowEmptyDigest_ );
	MF_WATCH( "shutDownBaseAppMgrOnExit", shutDownBaseAppMgrOnExit_ );
	MF_WATCH( "createUnknown",		shouldCreateUnknown_ );
	MF_WATCH( "rememberUnknown",	shouldRememberUnknown_ );
	MF_WATCH( "loadUnknown",		shouldLoadUnknown_ );
	MF_WATCH( "isReady", *this, &Database::isReady );

	MF_WATCH( "hasStartBegun",
			*this, MF_ACCESSORS( bool, Database, hasStartBegun ) );
	MF_WATCH( "hasStarted", *this, &Database::hasStarted );

	MF_WATCH( "desiredBaseApps", desiredBaseApps_ );
	MF_WATCH( "desiredCellApps", desiredCellApps_ );

	MF_WATCH( "clearRecoveryDataOnStartUp", clearRecoveryDataOnStartUp_ );

	MF_WATCH( "performance/writeEntity/performance", writeEntityTimer_,
		Watcher::WT_READ_ONLY );
	MF_WATCH( "performance/writeEntity/rate", writeEntityTimer_,
		&Mercury::Nub::TransientMiniTimer::getCountPerSec );
	MF_WATCH( "performance/writeEntity/duration", (Mercury::Nub::MiniTimer&) writeEntityTimer_,
		&Mercury::Nub::MiniTimer::getAvgDurationSecs );

	MF_WATCH( "load", curLoad_, Watcher::WT_READ_ONLY );
	MF_WATCH( "overloadLevel", maxLoad_ );

	DataSectionPtr pSection =
		BWResource::openSection( "entities/entities.xml" );

	if (!pSection)
	{
		ERROR_MSG( "Database::init: Failed to open "
				"<res>/entities/entities.xml\n" );
		return InitResultFailure;
	}

	pEntityDefs_ = new EntityDefs();
	if ( !pEntityDefs_->init( pSection, defaultTypeName, nameProperty ) )
	{
		return InitResultFailure;
	}

	// Check that dbMgr/entityType is valid. Unless dbMgr/shouldLoadUnknown and
	// dbMgr/shouldCreateUnknown is false, in which case we don't use
	// dbMgr/entityType anyway so don't care if it's invalid.
	if ( !pEntityDefs_->isValidEntityType( pEntityDefs_->getDefaultType() ) &&
	     (shouldLoadUnknown_ || shouldCreateUnknown_) )
	{
		ERROR_MSG( "Database::init: Invalid dbMgr/entityType '%s'. "
				"Consider changing dbMgr/entityType in bw.xml\n",
				defaultTypeName.c_str() );
		return InitResultFailure;
	}

	pEntityDefs_->debugDump( dumpLevel );

	INFO_MSG( "\tNub address         = %s\n", (char *)nub_.address() );
	INFO_MSG( "\tAllow empty digest  = %s\n",
		allowEmptyDigest_ ? "True" : "False" );
	INFO_MSG( "\tLoad unknown user = %s\n",
		shouldLoadUnknown_ ? "True" : "False" );
	INFO_MSG( "\tCreate unknown user = %s\n",
		shouldCreateUnknown_ ? "True" : "False" );
	INFO_MSG( "\tRemember unknown user = %s\n",
		shouldRememberUnknown_ ? "True" : "False" );
	INFO_MSG( "\tRecover database = %s\n",
		isRecover ? "True" : "False" );

	// Initialise the watcher
	BW_REGISTER_WATCHER( 0, "dbmgr", "DBMgr", "dbMgr", nub_ );

	Watcher::rootWatcher().addChild( "nub", Mercury::Nub::pWatcher(),
		&this->nub_ );

	std::string databaseType = BWConfig::get( "dbMgr/type", "xml" );

#ifdef USE_XML
	if (databaseType == "xml")
	{
		pDatabase_ = new XMLDatabase();
	} else
#endif
#ifdef USE_ORACLE
	if (databaseType == "oracle")
	{
		char * oracle_home = getenv( "ORACLE_HOME" );

		if (oracle_home == NULL)
		{
			INFO_MSG( "ORACLE_HOME not set. Setting to /home/local/oracle\n" );
			putenv( "ORACLE_HOME=/home/local/oracle" );
		}

		pDatabase_ = new OracleDatabase();
	}
	else
#endif
#ifdef USE_MYSQL
	if (databaseType == "mysql")
	{
		// pDatabase_ = new MySqlDatabase();
		pDatabase_ = MySqlDatabase::create();

		if (pDatabase_ == NULL)
		{
			return InitResultFailure;
		}
	}
	else
#endif
	{
		ERROR_MSG("Unknown database type: %s\n", databaseType.c_str());
#ifndef USE_MYSQL
		if (databaseType == "mysql")
		{
			INFO_MSG( "DBMgr needs to be rebuilt with MySQL support. See "
					"the Server Installation Guide for more information\n" );
		}
#endif
		return InitResultFailure;
	}

	INFO_MSG( "\tDatabase layer      = %s\n", databaseType.c_str() );
	if (databaseType == "xml")
	{
		NOTICE_MSG(
			"The XML database is suitable for demonstrations and "
			"evaluations only.\n"
			"Please use the MySQL database for serious development and "
			"production systems.\n"
			"See the Server Operations Guide for instructions on how to switch"
		   	"to the MySQL database.\n" );
	}

	if (!pDatabase_->startup( this->getEntityDefs(), isRecover, isUpgrade ))
		return InitResultFailure;

	if (isAutoShutdown)
		return InitResultAutoShutdown;

	signal( SIGINT, ::signalHandler );
#ifndef _WIN32  // WIN32PORT
	signal( SIGHUP, ::signalHandler );
#endif //ndef _WIN32  // WIN32PORT

	{
		nub_.registerBirthListener( DBInterface::handleBaseAppMgrBirth,
									"BaseAppMgrInterface" );

		// find the BaseAppMgr interface
		Mercury::Address baseAppMgrAddr;
		Mercury::Reason reason =
			nub_.findInterface( "BaseAppMgrInterface", 0, baseAppMgrAddr );

		if (reason == Mercury::REASON_SUCCESS)
		{
			baseAppMgr_.addr( baseAppMgrAddr );

			INFO_MSG( "Database::init: BaseAppMgr at %s\n",
				baseAppMgr_.c_str() );
		}
		else if (reason == Mercury::REASON_TIMER_EXPIRED)
		{
			INFO_MSG( "Database::init: BaseAppMgr is not ready yet.\n" );
		}
		else
		{
			CRITICAL_MSG("login::init: "
				"findInterface( BaseAppMgrInterface ) failed! (%s)\n",
				Mercury::reasonToString( (Mercury::Reason)reason ) );

			return InitResultFailure;
		}
	}

	DBInterface::registerWithNub( nub_ );

	Mercury::Reason reason =
		DBInterface::registerWithMachined( nub_, 0 );

	if (reason != Mercury::REASON_SUCCESS)
	{
		ERROR_MSG( "Database::init: Unable to register with nub. "
				"Is machined running?\n" );
		return InitResultFailure;
	}

	nub_.registerBirthListener( DBInterface::handleDatabaseBirth,
								"DBInterface" );

	if (isRecover)
		this->startServerBegin( true );
	else
		// A one second timer to check all sorts of things.
		statusCheckTimerId_ = nub_.registerTimer( 1000000, this );

#ifdef DBMGR_SELFTEST
		this->runSelfTest();
#endif

	return InitResultSuccess;
}


/**
 *	This method runs the database.
 */
void Database::run()
{
	INFO_MSG( "---- DBMgr is running ----\n" );
	nub_.processUntilBreak();

	this->finalise();
}

/**
 *	This method performs some clean-up at the end of the shut down process.
 */
void Database::finalise()
{
	nub_.processUntilChannelsEmpty();

	if (pDbUpdater_)
	{
		// Shutting down in the middle of an update! This is not going to be
		// pretty. Hopefully pDatabase_->shutDown() does something sensible.
		delete pDbUpdater_;
		pDbUpdater_ = NULL;
	}
	if (pUpdatedBWResource_)
	{
		BWResource::instance( NULL );
		delete pUpdatedBWResource_;
		pUpdatedBWResource_ = NULL;
	}
	if (pDatabase_)
	{
		pDatabase_->shutDown();
	}
}


/**
 *	This method handles the replies from the checkStatus requests.
 */
void Database::handleStatusCheck( BinaryIStream & data )
{
	int numBaseApps;
	int numCellApps;
	data >> numBaseApps >> numCellApps;
	INFO_MSG( "Database::handleStatusCheck: "
				"baseApps = %d/%d. cellApps = %d/%d\n",
			std::max( 0, numBaseApps ), desiredBaseApps_,
			std::max( 0, numCellApps ), desiredCellApps_ );

	if (!hasStartBegun_ &&
			(numBaseApps >= desiredBaseApps_) &&
			(numCellApps >= desiredCellApps_))
	{
		this->startServerBegin();
	}
}


/**
 *	This method handles the checkStatus request's reply.
 */
class CheckStatusReplyHandler : public Mercury::ReplyMessageHandler
{
private:
	virtual void handleMessage( const Mercury::Address & /*srcAddr*/,
			Mercury::UnpackedMessageHeader & /*header*/,
			BinaryIStream & data, void * /*arg*/ )
	{
		Database::instance().handleStatusCheck( data );
		delete this;
	}

	virtual void handleException( const Mercury::NubException & /*ne*/,
		void * /*arg*/ )
	{
		delete this;
	}
};


/**
 *	This method handles timer events. It is called every second.
 */
int Database::handleTimeout( int id, void * arg )
{
	// See if we are ready to start.
	if (!hasStartBegun_ && this->isReady())
	{
		Mercury::Bundle & bundle = baseAppMgr_.bundle();

		bundle.startRequest( BaseAppMgrInterface::checkStatus,
			   new CheckStatusReplyHandler() );

		baseAppMgr_.send();

		nub_.clearSpareTime();
	}

	// Update our current load so we can know whether or not we are overloaded.
	if (hasStartBegun_)
	{
		uint64 spareTime = nub_.getSpareTime();
		nub_.clearSpareTime();

		curLoad_ = 1.f - float( double(spareTime) / stampsPerSecondD() );

		// TODO: Consider asking DB implementation if it is overloaded too...
	}

	// Check whether we should end our remapping of mailboxes for a dead BaseApp
	if (--mailboxRemapCheckCount_ == 0)
		this->endMailboxRemapping();

	return 0;
}


// -----------------------------------------------------------------------------
// Section: Database lifetime
// -----------------------------------------------------------------------------

/**
 *	This method is called when a new BaseAppMgr is started.
 */
void Database::handleBaseAppMgrBirth( DBInterface::handleBaseAppMgrBirthArgs & args )
{
	baseAppMgr_.addr( args.addr );

	INFO_MSG( "Database::handleBaseAppMgrBirth: BaseAppMgr is at %s\n",
		baseAppMgr_.c_str() );
}

/**
 *	This method is called when a new DbMgr is started.
 */
void Database::handleDatabaseBirth( DBInterface::handleDatabaseBirthArgs & args )
{
	if (args.addr != nub_.address())
	{
		WARNING_MSG( "Database::handleDatabaseBirth: %s\n", (char *)args.addr );
		this->shutDown();
	}
}

/**
 *	This method handles the shutDown message.
 */
void Database::shutDown( DBInterface::shutDownArgs & /*args*/ )
{
	this->shutDown();
}

/**
 *	This method shuts this process down.
 *
 *	@warning This process needs to be reentrant as it is called from the signal
 *				handlers.
 */
void Database::shutDown()
{
	INFO_MSG( "Database::shutDown: Shutting down\n" );

	if (this->isReady())
	{
		if (shutDownBaseAppMgrOnExit_)
		{
			baseAppMgr_.bundle() << BaseAppMgrInterface::shutDownArgs();
			baseAppMgr_.send();
		}
	}
	else
	{
		WARNING_MSG( "Database::shutDown: No known BaseAppMgr\n" );
	}

	nub_.breakProcessing();
}


/**
 *	This method handles telling us to shut down in a controlled manner.
 */
void Database::controlledShutDown( DBInterface::controlledShutDownArgs & args )
{
	DEBUG_MSG( "Database::controlledShutDown: stage = %d\n", args.stage );

	switch (args.stage)
	{
	case SHUTDOWN_REQUEST:
	{
		if (this->hasStarted() && this->isReady())
		{
			BaseAppMgrInterface::controlledShutDownArgs args;
			args.stage = SHUTDOWN_REQUEST;
			args.shutDownTime = 0;
			baseAppMgr_.bundle() << args;
			baseAppMgr_.send();
		}
		else
		{
			this->shutDown();
		}
	}
	break;

	case SHUTDOWN_PERFORM:
		this->shutDown();
		break;

	default:
		ERROR_MSG( "Database::controlledShutDown: Stage %d not handled.\n",
				args.stage );
		break;
	}
}


/**
 *	This method handles telling us that a CellApp is or isn't overloaded.
 */
void Database::cellAppOverloadStatus(
	DBInterface::cellAppOverloadStatusArgs & args )
{
	anyCellAppOverloaded_ = args.anyOverloaded;
}


// -----------------------------------------------------------------------------
// Section: IDatabase intercept methods
// -----------------------------------------------------------------------------
/**
 *	This method intercepts the result of IDatabase::getEntity() operations and
 *	mucks around with it before passing it to onGetEntityCompleted().
 */
void Database::GetEntityHandler::onGetEntityComplete( bool isOK )
{
	// Update mailbox for dead BaseApps.
	if (Database::instance().hasMailboxRemapping() &&
			this->outrec().isBaseMBProvided() &&
			this->outrec().getBaseMB())
	{
		Database::instance().remapMailbox( *this->outrec().getBaseMB() );
	}

	// Give results to real handler.
	this->onGetEntityCompleted( isOK );
}

/**
 *	This method is meant to be called instead of IDatabase::getEntity() so that
 * 	we can muck around with stuff before passing it to IDatabase.
 */
void Database::getEntity( GetEntityHandler& handler,
		bool shouldCheckBundleVersion )
{
	if (!shouldCheckBundleVersion)
	{
		pDatabase_->getEntity( handler );
	}
	else
	{
		// Updater stuff. Get entity request came from a component that may not
		// match our current version. Checking last bundle version to see if it
		// requires special processing.
		IDatabase::IOldDatabase* pOldDatabase = this->getOldDatabaseForBundle();
		if (pOldDatabase)
			pOldDatabase->getEntity( handler );
		else
			pDatabase_->getEntity( handler );
	}
}

/**
 *	This method is meant to be called instead of IDatabase::putEntity() so that
 * 	we can muck around with stuff before passing it to IDatabase.
 */
void Database::putEntity( const EntityDBKey& ekey, EntityDBRecordIn& erec,
		IDatabase::IPutEntityHandler& handler, bool shouldCheckBundleVersion )
{
	// Update mailbox for dead BaseApps.
	if (this->hasMailboxRemapping() && erec.isBaseMBProvided() &&
			erec.getBaseMB())
		this->remapMailbox( *erec.getBaseMB() );

	if (!shouldCheckBundleVersion)
	{
		pDatabase_->putEntity( ekey, erec, handler );
	}
	else
	{
		// Updater stuff. erec.pStrm_ is a stream that was just extracted from
		// a bundle. Checking last bundle version to see if it requires special
		// processing.
		IDatabase::IOldDatabase* pOldDatabase = this->getOldDatabaseForBundle();
		if (pOldDatabase)
			pOldDatabase->putEntity( ekey, erec, handler );
		else
			pDatabase_->putEntity( ekey, erec, handler );
	}
}

// -----------------------------------------------------------------------------
// Section: LoginHandler
// -----------------------------------------------------------------------------

/**
 *	This class is used to receive the reply from a createEntity call to
 *	BaseAppMgr.
 */
class LoginHandler : public Mercury::ReplyMessageHandler,
                     public IDatabase::IMapLoginToEntityDBKeyHandler,
					 public IDatabase::ISetLoginMappingHandler,
                     public Database::GetEntityHandler,
                     public IDatabase::IPutEntityHandler
{
	enum State
	{
		StateInit,
		StateWaitingForLoadUnknown,
		StateWaitingForLoad,
		StateWaitingForPutNewEntity,
		StateWaitingForSetLoginMappingForLoadUnknown,
		StateWaitingForSetLoginMappingForCreateUnknown,
		StateWaitingForSetBaseToLoggingOn,
		StateWaitingForSetBaseToFinal,
	};

public:
	LoginHandler(
			LogOnParamsPtr pParams,
			const Mercury::Address& clientAddr,
			const Mercury::Address & replyAddr,
			Mercury::ReplyID replyID ) :
		state_( StateInit ),
		ekey_( 0, 0 ),
		pParams_( pParams ),
		clientAddr_( clientAddr ),
		replyAddr_( replyAddr ),
		replyID_( replyID ),
		pStrmDbID_( 0 )
	{
	}
	virtual ~LoginHandler() {}

	void login();

	// Mercury::ReplyMessageHandler overrides
	virtual void handleMessage( const Mercury::Address & source,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data, void * arg );

	virtual void handleException( const Mercury::NubException & ne,
		void * arg );

	// IDatabase::IMapLoginToEntityDBKeyHandler override
	virtual void onMapLoginToEntityDBKeyComplete( DatabaseLoginStatus status,
		const EntityDBKey& ekey );

	// IDatabase::ISetLoginMappingHandler override
	virtual void onSetLoginMappingComplete();

	// IDatabase::IGetEntityHandler/Database::GetEntityHandler overrides
	virtual EntityDBKey& key()					{	return ekey_;	}
	virtual EntityDBRecordOut& outrec()			{	return outRec_;	}

	virtual const std::string * getPasswordOverride() const
	{
		return Database::instance().getEntityDefs().
			entityTypeHasPassword( ekey_.typeID ) ? &pParams_->password() : 0;
	}

	virtual void onGetEntityCompleted( bool isOK );

	// IDatabase::IPutEntityHandler override
	virtual void onPutEntityComplete( bool isOK, DatabaseID dbID );

private:
	void handleFailure( BinaryIStream * pData, uint16 why );
	void checkOutEntity();
	void createNewEntity( bool isBundlePrepared = false );
	void sendCreateEntityMsg();
	void sendReply();
	void sendFailureReply( DatabaseLoginStatus status, char * msg = NULL );

	State				state_;
	EntityDBKey			ekey_;
	LogOnParamsPtr		pParams_;
	Mercury::Address	clientAddr_;
	Mercury::Address	replyAddr_;
	Mercury::ReplyID	replyID_;
	Mercury::Bundle		bundle_;
	EntityMailBoxRef	baseRef_;
	EntityMailBoxRef*	pBaseRef_;
	EntityDBRecordOut	outRec_;
	DatabaseID*			pStrmDbID_;
};

/**
 *	Start the login process
 */
void LoginHandler::login()
{
	// __glenc__ TODO: See if this needs to be converted to use params
	Database::instance().getIDatabase().mapLoginToEntityDBKey(
		pParams_->username(), pParams_->password(), *this );

	// When mapLoginToEntityDBKey() completes, onMapLoginToEntityDBKeyComplete()
	// will be called.
}

/**
 *	IDatabase::IMapLoginToEntityDBKeyHandler override
 */
void LoginHandler::onMapLoginToEntityDBKeyComplete( DatabaseLoginStatus status,
												   const EntityDBKey& ekey )
{
	bool shouldLoadEntity = false;
	bool shouldCreateEntity = false;

	if (status == DatabaseLoginStatus::LOGGED_ON)
	{
		ekey_ = ekey;
		shouldLoadEntity = true;
		state_ = StateWaitingForLoad;
	}
	else if (status == DatabaseLoginStatus::LOGIN_REJECTED_NO_SUCH_USER)
	{
		if (Database::instance().shouldLoadUnknown())
		{
			ekey_.typeID = Database::instance().getEntityDefs().getDefaultType();
			ekey_.name = pParams_->username();
			shouldLoadEntity = true;
			state_ = StateWaitingForLoadUnknown;
		}
		else if (Database::instance().shouldCreateUnknown())
		{
			shouldCreateEntity = true;
		}
	}

	if (shouldLoadEntity)
	{
		// Start "create new base" message even though we're not sure entity
		// exists. This is to take advantage of getEntity() streaming properties
		// into the bundle directly.
		pStrmDbID_ = Database::prepareCreateEntityBundle( ekey_.typeID,
			ekey_.dbID, clientAddr_, this, bundle_, pParams_ );

		// Get entity data
		pBaseRef_ = &baseRef_;
		outRec_.provideBaseMB( pBaseRef_ );		// Get entity mailbox
		outRec_.provideStrm( bundle_ );			// Get entity data into bundle

		Database::instance().getEntity( *this );
		// When getEntity() completes, onGetEntityCompleted() is called.
	}
	else if (shouldCreateEntity)
	{
		this->createNewEntity();
	}
	else
	{
		const char * msg;
		bool isError = false;
		switch (status)
		{
			case DatabaseLoginStatus::LOGIN_REJECTED_NO_SUCH_USER:
				msg = "Unknown user."; break;
			case DatabaseLoginStatus::LOGIN_REJECTED_INVALID_PASSWORD:
				msg = "Invalid password."; break;
			case DatabaseLoginStatus::LOGIN_ACCOUNT_BLOCKED:
				msg = "account blocked."; 
                break;
			case DatabaseLoginStatus::LOGIN_CONTROL_BY_GM:
				msg = "control by GM."; 
                break;
			case DatabaseLoginStatus::LOGIN_REJECTED_DB_GENERAL_FAILURE:
				msg = "Unexpected database failure.";
				isError = true;
				break;
			default:
				msg = UNSPECIFIED_ERROR_STR;
				isError = true;
				break;
		}
		if (isError)
		{
			ERROR_MSG( "Database::logOn: mapLoginToEntityDBKey for %s failed: "
				"(%d) %s\n", pParams_->username().c_str(), int(status), msg );
		}
		else
		{
			NOTICE_MSG( "Database::logOn: mapLoginToEntityDBKey for %s failed: "
				"(%d) %s\n", pParams_->username().c_str(), int(status), msg );
		}
		Database::instance().sendFailure( replyID_, replyAddr_, status, msg );
		delete this;
	}

}

/**
 *	Database::GetEntityHandler override
 */
void LoginHandler::onGetEntityCompleted( bool isOK )
{
	if (!isOK)
	{	// Entity doesn't exist.
		if ( (state_ == StateWaitingForLoadUnknown) &&
			 Database::instance().shouldCreateUnknown() )
		{
			this->createNewEntity( true );
		}
		else
		{
			ERROR_MSG( "Database::logOn: Entity %s does not exist\n",
				ekey_.name.c_str() );

			this->sendFailureReply(
				DatabaseLoginStatus::LOGIN_REJECTED_NO_SUCH_USER,
				"Failed to load entity." );
		}
	}
	else
	{
		if (pStrmDbID_)
		{	// Means ekey.dbID was 0 when we called prepareCreateEntityBundle()
			// Now fix up everything.
			*pStrmDbID_ = ekey_.dbID;
		}

		if ( (state_ == StateWaitingForLoadUnknown) &&
			 (Database::instance().shouldRememberUnknown()) )
		{	// Need to remember this login mapping.
			state_ = StateWaitingForSetLoginMappingForLoadUnknown;

			Database::instance().getIDatabase().setLoginMapping(
				pParams_->username(), pParams_->password(), ekey_, *this );

			// When setLoginMapping() completes onSetLoginMappingComplete() is called
		}
		else
		{
			this->checkOutEntity();
		}
	}
}

/**
 *	This function checks out the login entity. Must be called after
 *	entity has been successfully retrieved from the database.
 */
void LoginHandler::checkOutEntity()
{
	if ( !outRec_.getBaseMB() &&
		Database::instance().onStartEntityCheckout( ekey_.typeID, ekey_.dbID ) )
	{	// Not checked out and not in the process of being checked out.
		state_ = StateWaitingForSetBaseToLoggingOn;
		Database::setBaseRefToLoggingOn( baseRef_, ekey_.typeID );
		pBaseRef_ = &baseRef_;
		EntityDBRecordIn	erec;
		erec.provideBaseMB( pBaseRef_ );
		Database::instance().putEntity( ekey_, erec, *this );
		// When putEntity() completes, onPutEntityComplete() is called.
	}
	else	// Checked out
	{
		Database::instance().onLogOnLoggedOnUser( ekey_.typeID, ekey_.dbID,
			pParams_, clientAddr_, replyAddr_, replyID_, outRec_.getBaseMB() );

		delete this;
	}
}

/**
 *	IDatabase::IPutEntityHandler override
 */
void LoginHandler::onPutEntityComplete( bool isOK, DatabaseID dbID )
{
	switch (state_)
	{
		case StateWaitingForPutNewEntity:
			MF_ASSERT( pStrmDbID_ );
			*pStrmDbID_ = dbID;
			if (isOK)
			{
				ekey_.dbID = dbID;

				state_ = StateWaitingForSetLoginMappingForCreateUnknown;
				Database::instance().getIDatabase().setLoginMapping(
					pParams_->username(), pParams_->password(),	ekey_, *this );

				// When setLoginMapping() completes onSetLoginMappingComplete() is called
				break;
			}
			else
			{	// Failed "rememberEntity" function.
				ERROR_MSG( "Database::logOn: Failed to write default entity "
					"for %s\n", pParams_->username().c_str());

				// Let them log in anyway since this is meant to be a
				// convenience feature during product development.
				// Fallthrough
			}
		case StateWaitingForSetBaseToLoggingOn:
			if (isOK)
				this->sendCreateEntityMsg();
			else
			{
				Database::instance().onCompleteEntityCheckout( ekey_.typeID,
						ekey_.dbID, NULL );
				// Something horrible like database disconnected or something.
				this->sendFailureReply(
						DatabaseLoginStatus::LOGIN_REJECTED_DB_GENERAL_FAILURE,
						"Unexpected database failure." );
			}
			break;
		case StateWaitingForSetBaseToFinal:
			Database::instance().onCompleteEntityCheckout( ekey_.typeID,
					ekey_.dbID, (isOK) ? &baseRef_ : NULL );
			if (isOK)
				this->sendReply();
			else
				// Something horrible like database disconnected or something.
				this->sendFailureReply(
						DatabaseLoginStatus::LOGIN_REJECTED_DB_GENERAL_FAILURE,
						"Unexpected database failure." );
			break;
		default:
			MF_ASSERT(false);
			delete this;
	}
}

/**
 *	This method sends the BaseAppMgrInterface::createEntity message.
 *	Assumes bundle has the right data.
 */
inline void LoginHandler::sendCreateEntityMsg()
{
	INFO_MSG( "Database::logOn: %s\n", pParams_->username().c_str() );

	Database::instance().baseAppMgr().send( &bundle_ );
}

/**
 *	This method sends the reply to the LoginApp. Assumes bundle already
 *	has the right data.
 *
 *	This should also be the last thing this object is doing so this
 *	also destroys this object.
 */
inline void LoginHandler::sendReply()
{
	Database::getChannel( replyAddr_ ).send( &bundle_ );
	delete this;
}

/**
 *	This method sends a failure reply to the LoginApp.
 *
 *	This should also be the last thing this object is doing so this
 *	also destroys this object.
 */
inline void LoginHandler::sendFailureReply( DatabaseLoginStatus status, char * msg )
{
	Database::instance().sendFailure( replyID_, replyAddr_, status, msg );
	delete this;
}

/**
 *	IDatabase::ISetLoginMappingHandler override
 */
void LoginHandler::onSetLoginMappingComplete()
{
	if (state_ == StateWaitingForSetLoginMappingForLoadUnknown)
	{
		this->checkOutEntity();
	}
	else
	{
		MF_ASSERT( state_ == StateWaitingForSetLoginMappingForCreateUnknown );
		this->sendCreateEntityMsg();
	}
}

/**
 *	This function creates a new login entity for the user.
 */
void LoginHandler::createNewEntity( bool isBundlePrepared )
{
	ekey_.typeID = Database::instance().getEntityDefs().getDefaultType();
	ekey_.name = pParams_->username();

	if (!isBundlePrepared)
	{
		pStrmDbID_ = Database::prepareCreateEntityBundle( ekey_.typeID,
			0, clientAddr_, this, bundle_, pParams_ );
	}

	bool isDefaultEntityOK;

	if (Database::instance().shouldRememberUnknown())
	{
		// __kyl__ (13/7/2005) Need additional MemoryOStream because I
		// haven't figured out how to make a BinaryIStream out of a
		// Mercury::Bundle.
		MemoryOStream strm;
		isDefaultEntityOK = Database::instance().defaultEntityToStrm(
			ekey_.typeID, pParams_->username(), strm, &pParams_->password() );

		if (isDefaultEntityOK)
		{
			bundle_.transfer( strm, strm.size() );
			strm.rewind();

			// Put entity data into DB and set baseref to "logging on".
			state_ = StateWaitingForPutNewEntity;
			Database::setBaseRefToLoggingOn( baseRef_, ekey_.typeID );
			pBaseRef_ = &baseRef_;
			EntityDBRecordIn	erec;
			erec.provideBaseMB( pBaseRef_ );
			erec.provideStrm( strm );
			Database::instance().putEntity( ekey_, erec, *this );
			// When putEntity() completes, onPutEntityComplete() is called.
		}
	}
	else
	{
		*pStrmDbID_ = 0;

		// No need for additional MemoryOStream. Just stream into bundle.
		isDefaultEntityOK = Database::instance().defaultEntityToStrm(
			ekey_.typeID, pParams_->username(), bundle_,
			&pParams_->password() );

		if (isDefaultEntityOK)
			this->sendCreateEntityMsg();
	}

	if (!isDefaultEntityOK)
	{
		ERROR_MSG( "Database::logOn: Failed to create default entity for %s\n",
			pParams_->username().c_str());

		this->sendFailureReply( DatabaseLoginStatus::LOGIN_CUSTOM_DEFINED_ERROR,
				"Failed to create default entity" );
	}
}

/**
 *	Mercury::ReplyMessageHandler override.
 */
void LoginHandler::handleMessage( const Mercury::Address & source,
	Mercury::UnpackedMessageHeader & header,
	BinaryIStream & data,
	void * arg )
{
	Mercury::Address proxyAddr;

	data >> proxyAddr;

	if (proxyAddr.ip == 0)
	{
		this->handleFailure( &data, proxyAddr.port );
	}
	else
	{
		data >> baseRef_;

		bundle_.clear();
		bundle_.startReply( replyID_ );

		// Assume success.
		bundle_ << (uint8)DatabaseLoginStatus::LOGGED_ON;
		bundle_ << proxyAddr;
		bundle_.transfer( data, data.remainingLength() );

		if ( ekey_.dbID )
		{
			state_ = StateWaitingForSetBaseToFinal;
			pBaseRef_ = &baseRef_;
			EntityDBRecordIn erec;
			erec.provideBaseMB( pBaseRef_ );
			Database::instance().putEntity( ekey_, erec, *this );
			// When putEntity() completes, onPutEntityComplete() is called.
		}
		else
		{	// Must be either "loadUnknown" or "createUnknown", but
			// "rememberUnknown" is false.
			this->sendReply();
		}
	}
}

/**
 *	Mercury::ReplyMessageHandler override.
 */
void LoginHandler::handleException( const Mercury::NubException & ne,
	void * arg )
{
	MemoryOStream mos;
	mos << "BaseAppMgr timed out creating entity.";
	this->handleFailure( &mos, 1024 );
}

/**
 *	Handles a failure to create entity base.
 */
void LoginHandler::handleFailure( BinaryIStream * pData, uint16 why )
{
	bundle_.clear();
	bundle_.startReply( replyID_ );

	DatabaseLoginStatus::Status logonstatus;
	switch (why)
	{
		case 1:	logonstatus =
			DatabaseLoginStatus::LOGIN_REJECTED_UPDATER_NOT_READY;	break;
		case 2: logonstatus =
			DatabaseLoginStatus::LOGIN_REJECTED_NO_BASEAPPS;		break;
		case 3: logonstatus =
			DatabaseLoginStatus::LOGIN_REJECTED_BASEAPP_OVERLOAD;	break;
		case 1024: logonstatus =
			DatabaseLoginStatus::LOGIN_REJECTED_BASEAPPMGR_TIMEOUT;	break;
		default: logonstatus =
			DatabaseLoginStatus::LOGIN_CUSTOM_DEFINED_ERROR;		break;
	}
	bundle_ << (uint8)logonstatus;

	bundle_.transfer( *pData, pData->remainingLength() );

	if ( ekey_.dbID )
	{
		state_ = StateWaitingForSetBaseToFinal;
		pBaseRef_ = 0;
		EntityDBRecordIn erec;
		erec.provideBaseMB( pBaseRef_ );
		Database::instance().putEntity( ekey_, erec, *this );
		// When putEntity() completes, onPutEntityComplete() is called.
	}
	else
	{	// Must be either "loadUnknown" or "createUnknown", but
		// "rememberUnknown" is false.
		this->sendReply();
	}
}



// -----------------------------------------------------------------------------
// Section: RelogonAttemptHandler
// -----------------------------------------------------------------------------

/**
 *	This class is used to receive the reply from a createEntity call to
 *	BaseAppMgr during a re-logon operation.
 */
class RelogonAttemptHandler : public Mercury::ReplyMessageHandler,
                            public IDatabase::IPutEntityHandler
{
	enum State
	{
		StateWaitingForOnLogOnAttempt,
		StateWaitingForSetBaseToFinal,
		StateWaitingForSetBaseToNull,
		StateAborted
	};

public:
	RelogonAttemptHandler( EntityTypeID entityTypeID,
			DatabaseID dbID,
			const Mercury::Address & replyAddr,
			Mercury::ReplyID replyID,
			LogOnParamsPtr pParams,
			const Mercury::Address & addrForProxy ) :
		state_( StateWaitingForOnLogOnAttempt ),
		ekey_( entityTypeID, dbID ),
		replyAddr_( replyAddr ),
		replyID_( replyID ),
		pParams_( pParams ),
		addrForProxy_( addrForProxy )
	{
		Database::instance().onStartRelogonAttempt( entityTypeID, dbID, this );
	}

	virtual ~RelogonAttemptHandler()
	{
		if (state_ != StateAborted)
		{
			Database::instance().onCompleteRelogonAttempt(
					ekey_.typeID, ekey_.dbID );
		}
	}

	virtual void handleMessage( const Mercury::Address & source,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data,
		void * arg )
	{
		uint8 result;
		data >> result;

		if (state_ != StateAborted)
		{
			switch (result)
			{
				case BaseAppIntInterface::LOG_ON_ATTEMPT_TOOK_CONTROL:
				{
					INFO_MSG( "RelogonAttemptHandler: It's taken over.\n" );
					Mercury::Address proxyAddr;
					data >> proxyAddr;

					EntityMailBoxRef baseRef;
					data >> baseRef;

					replyBundle_.startReply( replyID_ );

					// Assume success.
					replyBundle_ << (uint8)DatabaseLoginStatus::LOGGED_ON;
					replyBundle_ << proxyAddr;
					replyBundle_.transfer( data, data.remainingLength() );

					// Set base mailbox.
					// __kyl__(6/6/2006) Not sure why we need to do this. Can
					// they return a different one to the one we tried to
					// relogon to?
					state_ = StateWaitingForSetBaseToFinal;
					EntityMailBoxRef*	pBaseRef = &baseRef;
					EntityDBRecordIn	erec;
					erec.provideBaseMB( pBaseRef );
					Database::instance().putEntity( ekey_, erec, *this );
					// When putEntity() completes, onPutEntityComplete() is
					// called.

					return;	// Don't delete ourselves just yet.
				}
				case BaseAppIntInterface::LOG_ON_ATTEMPT_NOT_EXIST:
				{
					INFO_MSG( "RelogonAttemptHandler: Entity does not exist. "
						"Attempting to log on normally.\n" );
					// Log off entity from database since base no longer
					// exists.
					state_ = StateWaitingForSetBaseToNull;
					EntityDBRecordIn	erec;
					EntityMailBoxRef* 	pBaseRef = 0;
					erec.provideBaseMB( pBaseRef );
					Database::instance().putEntity( ekey_, erec, *this );
					// When putEntity() completes, onPutEntityComplete() is
					// called.

					return;	// Don't delete ourselves just yet.
				}
				case BaseAppIntInterface::LOG_ON_ATTEMPT_REJECTED:
				{
					INFO_MSG( "RelogonAttemptHandler: Re-login not allowed for %s.\n",
						pParams_->username().c_str() );

					Database::instance().sendFailure( replyID_, replyAddr_,
						DatabaseLoginStatus::LOGIN_REJECTED_ALREADY_LOGGED_IN,
						"Relogin denied." );

					break;
				}
				default:
					CRITICAL_MSG( "RelogonAttemptHandler: Invalid result %d\n",
							int(result) );
					break;
			}
		}

		delete this;
	}

	virtual void handleException( const Mercury::NubException & exception,
		void * arg )
	{
		if (state_ != StateAborted)
		{
			const char * errorMsg = Mercury::reasonToString( exception.reason() );
			ERROR_MSG( "RelogonAttemptHandler: %s.\n", errorMsg );
			Database::instance().sendFailure( replyID_, replyAddr_,
					DatabaseLoginStatus::LOGIN_REJECTED_BASEAPP_TIMEOUT,
					errorMsg );
		}

		delete this;
	}

	// IDatabase::IPutEntityHandler override
	virtual void onPutEntityComplete( bool isOK, DatabaseID )
	{
		switch (state_)
		{
			case StateWaitingForSetBaseToFinal:
				if (isOK)
					Database::getChannel( replyAddr_ ).send( &replyBundle_ );
				else
					this->sendEntityDeletedFailure();
				break;
			case StateWaitingForSetBaseToNull:
				if (isOK)
					this->onEntityLogOff();
				else
					this->sendEntityDeletedFailure();
				break;
			case StateAborted:
				break;
			default:
				MF_ASSERT( false );
				break;
		}

		delete this;
	}

	void sendEntityDeletedFailure()
	{
		// Someone deleted the entity while we were logging on.
		ERROR_MSG( "Database::logOn: Entity %s was deleted during logon.\n",
			ekey_.name.c_str() );

		Database::instance().sendFailure( replyID_, replyAddr_,
				DatabaseLoginStatus::LOGIN_REJECTED_NO_SUCH_USER,
				"Entity deleted during login." );
	}

	// This function is called when the entity that we're trying to re-logon
	// to suddenly logs off.
	void onEntityLogOff()
	{
		if (state_ != StateAborted)
		{
			// Abort our re-logon attempt... actually, just flag it as aborted.
			// Still need to wait for callbacks.
			state_ = StateAborted;
			Database::instance().onCompleteRelogonAttempt(
					ekey_.typeID, ekey_.dbID );

			// Log on normally
			Database::instance().logOn( replyAddr_, replyID_, pParams_,
				addrForProxy_ );
		}
	}

private:
	State state_;
	EntityDBKey	ekey_;
	const Mercury::Address replyAddr_;
	Mercury::ReplyID replyID_;
	LogOnParamsPtr pParams_;
	const Mercury::Address addrForProxy_;
	Mercury::Bundle	replyBundle_;
};


// -----------------------------------------------------------------------------
// Section: Entity entry database requests
// -----------------------------------------------------------------------------

/**
 *	This method handles a logOn request.
 */
void Database::logOn( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data )
{
	// __kyl__ (9/9/2005) There are no specific code in logOn to handle the
	// update process because it is assumed that logins are disabled during
	// update - or atleast during the part where we need to consider multiple
	// entity versions.
	Mercury::Address addrForProxy;
	LogOnParamsPtr pParams = new LogOnParams();

	data >> addrForProxy >> *pParams;

	const MD5::Digest & digest = pParams->digest();
	bool goodDigest = (digest == this->getEntityDefs().getDigest());

	if (!goodDigest && allowEmptyDigest_)
	{
		goodDigest = true;

		// Bots and egclient send an empty digest
		for (uint32 i = 0; i < sizeof( digest ); i++)
		{
			if (digest.bytes[i] != '\0')
			{
				goodDigest = false;
				break;
			}
		}

		if (goodDigest)
		{
			WARNING_MSG( "Database::logOn: %s logged on with empty digest.\n",
				pParams->username().c_str() );
		}
	}

	if (!goodDigest)
	{
		ERROR_MSG( "Database::logOn: Incorrect digest\n" );
		this->sendFailure( header.replyID, srcAddr,
			DatabaseLoginStatus::LOGIN_REJECTED_BAD_DIGEST,
			"Defs digest mismatch." );
		return;
	}

	this->logOn( srcAddr, header.replyID, pParams, addrForProxy );
}


/**
 *	This method attempts to log on a player.
 */
void Database::logOn( const Mercury::Address & srcAddr,
	Mercury::ReplyID replyID,
	LogOnParamsPtr pParams,
	const Mercury::Address & addrForProxy )
{
	if (!this->hasStarted())
	{
		INFO_MSG( "Database::logOn: "
			"Login failed for %s. BaseAppMgr not ready.\n",
			pParams->username().c_str() );

		this->sendFailure( replyID, srcAddr,
			DatabaseLoginStatus::LOGIN_REJECTED_BASEAPPMGR_NOT_READY,
			"BaseAppMgr not ready." );

		return;
	}

	if (curLoad_ > maxLoad_)
	{
		INFO_MSG( "Database::logOn: "
				"Login failed for %s. We are overloaded "
				"(load=%.02f > max=%.02f)\n",
			pParams->username().c_str(), curLoad_, maxLoad_ );

		this->sendFailure( replyID, srcAddr,
			DatabaseLoginStatus::LOGIN_REJECTED_DBMGR_OVERLOAD,
			"DBMgr is overloaded." );

		return;
	}

	if (anyCellAppOverloaded_)
	{
		INFO_MSG( "Database::logOn: "
			"Login failed for %s. At least one CellApp is overloaded.\n",
			pParams->username().c_str() );

		this->sendFailure( replyID, srcAddr,
			DatabaseLoginStatus::LOGIN_REJECTED_CELLAPP_OVERLOAD,
			"At least one CellApp is overloaded." );

		return;
	}

	LoginHandler * pHandler =
		new LoginHandler( pParams, addrForProxy, srcAddr, replyID );

	pHandler->login();
}

/**
 *	This method is called when there is a log on request for an entity
 *	that is already logged on.
 */
void Database::onLogOnLoggedOnUser( EntityTypeID typeID, DatabaseID dbID,
	LogOnParamsPtr pParams,
	const Mercury::Address & clientAddr, const Mercury::Address & replyAddr,
	Mercury::ReplyID replyID, const EntityMailBoxRef* pExistingBase )
{
	// TODO: Make this a member
	bool shouldAttemptRelogon_ = true;

	if (shouldAttemptRelogon_ &&
		(Database::instance().getInProgRelogonAttempt( typeID, dbID ) == NULL))
	{
		if (Database::isValidMailBox(pExistingBase))
		{
			// Logon to existing base
			Mercury::ChannelSender sender(
				Database::getChannel( pExistingBase->addr ) );

			Mercury::Bundle & bundle = sender.bundle();
			bundle.startRequest( BaseAppIntInterface::logOnAttempt,
				new RelogonAttemptHandler( pExistingBase->type(), dbID,
					replyAddr, replyID, pParams, clientAddr )
				);

			bundle << pExistingBase->id;
			bundle << clientAddr;
			bundle << pParams->encryptionKey();

			bool hasPassword =
				this->getEntityDefs().entityTypeHasPassword( typeID );

			bundle << hasPassword;

			if (hasPassword)
			{
				bundle << pParams->password();
			}
		}
		else
		{
			// Another logon still in progress.
			WARNING_MSG( "Database::logOn: %s already logging in\n",
				pParams->username().c_str() );

			this->sendFailure( replyID, replyAddr,
				DatabaseLoginStatus::LOGIN_REJECTED_ALREADY_LOGGED_IN,
			   "Another login of same name still in progress." );
		}
	}
	else
	{
		// Another re-logon already in progress.
		INFO_MSG( "Database::logOn: %s already logged on\n",
			pParams->username().c_str() );

		this->sendFailure( replyID, replyAddr,
				DatabaseLoginStatus::LOGIN_REJECTED_ALREADY_LOGGED_IN,
				"A relogin of same name still in progress." );
	}
}

/*
 *	This method creates a default entity (via createNewEntity() in
 *	custom.cpp) and serialises it into the stream.
 *
 *	@param	The type of entity to create.
 *	@param	The name of the entity (for entities with a name property)
 *	@param	The stream to serialise entity into.
 *	@param	If non-NULL, this will override the "password" property of
 *	the entity.
 *	@return	True if successful.
 */
bool Database::defaultEntityToStrm( EntityTypeID typeID,
	const std::string& name, BinaryOStream& strm,
	const std::string* pPassword ) const
{
	DataSectionPtr pSection = createNewEntity( typeID, name );
	bool isCreated = pSection.exists();
	if (isCreated)
	{
		if (pPassword)
		{
			if (this->getEntityDefs().getPropertyType( typeID, "password" ) == "BLOB")
				pSection->writeBlob( "password", *pPassword );
			else
				pSection->writeString( "password", *pPassword );
		}

		const EntityDescription& desc =
			this->getEntityDefs().getEntityDescription( typeID );
		desc.addSectionToStream( pSection, strm,
			EntityDescription::BASE_DATA | EntityDescription::CELL_DATA |
			EntityDescription::ONLY_PERSISTENT_DATA );
		if (desc.hasCellScript())
		{
			Vector3	defaultVec( 0, 0, 0 );

			strm << defaultVec;	// position
			strm << defaultVec;	// direction
			strm << SpaceID(0);	// space ID
		}
	}

	return isCreated;
}

/*
 *	This method inserts the "header" info into the bundle for a
 *	BaseAppMgrInterface::createEntity message, up till the point
 *	where entity properties should begin.
 *
 *	@return	If dbID is 0, then this function returns the position in the
 *	bundle where you should put the DatabaseID.
 */
DatabaseID* Database::prepareCreateEntityBundle( EntityTypeID typeID,
		DatabaseID dbID, const Mercury::Address& addrForProxy,
		Mercury::ReplyMessageHandler* pHandler, Mercury::Bundle& bundle,
		LogOnParamsPtr pParams )
{
	bundle.startRequest( BaseAppMgrInterface::createEntity, pHandler, 0,
		Mercury::DEFAULT_REQUEST_TIMEOUT + 1000000 ); // 1 second extra

	// This data needs to match BaseAppMgr::createBaseWithCellData.
	bundle	<< ObjectID( 0 )
			<< typeID;

	DatabaseID*	pDbID = 0;
	if (dbID)
		bundle << dbID;
	else
		pDbID = reinterpret_cast<DatabaseID*>(bundle.reserve( sizeof(*pDbID) ));

	// This is the client address. It is used if we are making a proxy.
	bundle << addrForProxy;

	bundle << ((pParams != NULL) ? pParams->encryptionKey() : "");

	bundle << true;		// Has persistent data only

	return pDbID;
}

/**
 *	This helper method sends a failure reply.
 */
void Database::sendFailure( Mercury::ReplyID replyID,
	const Mercury::Address & dstAddr,
	DatabaseLoginStatus reason, const char * pDescription )
{
	MF_ASSERT( reason != DatabaseLoginStatus::LOGGED_ON );

	Mercury::ChannelSender sender( Database::getChannel( dstAddr ) );
	sender.bundle().startReply( replyID );
	sender.bundle() << uint8( reason );

	if (pDescription == NULL)
		pDescription = UNSPECIFIED_ERROR_STR;

	sender.bundle() << pDescription;
}

/**
 *	This class is used by Database::writeEntity() to write entities into
 *	the database and wait for the result.
 */
class WriteEntityHandler : public IDatabase::IPutEntityHandler,
                           public IDatabase::IDelEntityHandler
{
	EntityDBKey				ekey_;
	int8					flags_;
	bool					shouldReply_;
	Mercury::ReplyID		replyID_;
	const Mercury::Address	srcAddr_;

public:
	WriteEntityHandler( const EntityDBKey ekey, int8 flags, bool shouldReply,
		Mercury::ReplyID replyID, const Mercury::Address & srcAddr )
		: ekey_(ekey), flags_(flags), shouldReply_(shouldReply),
		replyID_(replyID), srcAddr_(srcAddr)
	{}
	virtual ~WriteEntityHandler() {}

	void writeEntity( BinaryIStream & data, ObjectID objectID );

	void deleteEntity();

	// IDatabase::IPutEntityHandler override
	virtual void onPutEntityComplete( bool isOK, DatabaseID );

	// IDatabase::IDelEntityHandler override
	virtual void onDelEntityComplete( bool isOK );

private:
	void putEntity( EntityDBRecordIn& erec )
	{
		Database::instance().putEntity( ekey_, erec, *this, true );
	}
	void finalise(bool isOK);
};

/**
 *	This method writes the entity data into the database.
 *
 *	@param	data	Stream should be currently at the start of the entity's
 *	data.
 *	@param	objectID	The entity's base mailbox object ID.
 */

void WriteEntityHandler::writeEntity( BinaryIStream & data, ObjectID objectID )
{
	EntityDBRecordIn erec;
	if (flags_ & WRITE_ALL_DATA)
		erec.provideStrm( data );

	if (flags_ & WRITE_LOG_OFF)
	{
		EntityMailBoxRef* pBaseRef = 0;
		erec.provideBaseMB( pBaseRef );
		this->putEntity( erec );
	}
	else if (!ekey_.dbID)
	{	// New entity is checked out straight away
		EntityMailBoxRef	baseRef;
		baseRef.init( objectID, srcAddr_, EntityMailBoxRef::BASE,
			ekey_.typeID );
		EntityMailBoxRef* pBaseRef = &baseRef;
		erec.provideBaseMB( pBaseRef );
		this->putEntity( erec );
	}
	else
	{
		this->putEntity( erec );
	}
	// When putEntity() completes onPutEntityComplete() is called.
}

/**
 *	IDatabase::IPutEntityHandler override
 */
void WriteEntityHandler::onPutEntityComplete( bool isOK, DatabaseID dbID )
{
	ekey_.dbID = dbID;
	if (!isOK)
	{
		ERROR_MSG( "Database::writeEntity: Failed to update entity %"FMT_DBID
			" of type %d.\n", dbID, ekey_.typeID );
	}

	this->finalise(isOK);
}

/**
 *	Deletes the entity from the database.
 */
void WriteEntityHandler::deleteEntity()
{
	MF_ASSERT( flags_ & WRITE_DELETE_FROM_DB );
	Database::instance().getIDatabase().delEntity( ekey_, *this );
	// When delEntity() completes, onDelEntityComplete() is called.
}

/**
 *	IDatabase::IDelEntityHandler override
 */
void WriteEntityHandler::onDelEntityComplete( bool isOK )
{
	if (!isOK)
	{
		ERROR_MSG( "Database::writeEntity: Failed to delete entity %"FMT_DBID" of type %d.\n",
			ekey_.dbID, ekey_.typeID );
	}

	this->finalise(isOK);
}

/**
 *	This function does some common stuff at the end of the operation.
 */
void WriteEntityHandler::finalise( bool isOK )
{
	if (shouldReply_)
	{
		Mercury::ChannelSender sender( Database::getChannel( srcAddr_ ) );
		sender.bundle().startReply( replyID_ );
		sender.bundle() << isOK << ekey_.dbID;
	}

	if (isOK && (flags_ & WRITE_LOG_OFF))
	{
		Database::instance().onEntityLogOff( ekey_.typeID, ekey_.dbID );
	}

	delete this;
}

/**
 *	This method handles the writeEntity mercury message.
 */
void Database::writeEntity( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data )
{
	Mercury::Nub::TransientMiniTimer::Op< Mercury::Nub::TransientMiniTimer >
		writeEntityTimerOp(writeEntityTimer_);

	int8 flags;
	data >> flags;
	// if this fails then the calling component had no need to call us
	MF_ASSERT( flags & (WRITE_ALL_DATA|WRITE_LOG_OFF) );

	EntityDBKey	ekey( 0, 0 );
	data >> ekey.typeID >> ekey.dbID;

	// TRACE_MSG( "Database::writeEntity: %lld flags=%i\n",
	//		   ekey.dbID, flags );

	bool isOkay = this->getEntityDefs().isValidEntityType( ekey.typeID );
	if (!isOkay)
	{
		ERROR_MSG( "Database::writeEntity: Invalid entity type %d\n",
			ekey.typeID );

		if (header.flags & Mercury::Packet::FLAG_HAS_REQUESTS)
		{
			Mercury::ChannelSender sender( Database::getChannel( srcAddr ) );
			sender.bundle().startReply( header.replyID );
			sender.bundle() << isOkay << ekey.dbID;
		}
	}
	else
	{
		WriteEntityHandler* pHandler =
			new WriteEntityHandler( ekey, flags,
				(header.flags & Mercury::Packet::FLAG_HAS_REQUESTS),
				header.replyID, srcAddr );
		if (flags & WRITE_DELETE_FROM_DB)
		{
			pHandler->deleteEntity();
		}
		else
		{
			ObjectID objectID;
			data >> objectID;

			pHandler->writeEntity( data, objectID );
		}
	}
}

/**
 *	This method is called when we've just logged off an entity.
 *
 *	@param	typeID The type ID of the logged off entity.
 *	@param	dbID The database ID of the logged off entity.
 */
void Database::onEntityLogOff( EntityTypeID typeID, DatabaseID dbID )
{
	// Notify any re-logon handler waiting on this entity that it has gone.
	RelogonAttemptHandler* pHandler =
			this->getInProgRelogonAttempt( typeID, dbID );
	if (pHandler)
		pHandler->onEntityLogOff();
}

/**
 *	This class is used by Database::loadEntity() to load an entity from
 *	the database and wait for the results.
 */
class LoadEntityHandler : public Database::GetEntityHandler,
                          public IDatabase::IPutEntityHandler,
                          public Database::ICheckoutCompletionListener
{
	EntityDBKey			ekey_;
	EntityMailBoxRef	baseRef_;
	EntityMailBoxRef*	pBaseRef_;
	EntityDBRecordOut	outRec_;
	Mercury::Address	srcAddr_;
	ObjectID			objID_;
	Mercury::ReplyID	replyID_;
	Mercury::Bundle		replyBundle_;
	DatabaseID*			pStrmDbID_;

public:
	LoadEntityHandler( const EntityDBKey& ekey,
			const Mercury::Address& srcAddr, ObjectID objID,
			Mercury::ReplyID replyID ) :
		ekey_(ekey), baseRef_(), pBaseRef_(0), outRec_(), srcAddr_(srcAddr),
		objID_(objID), replyID_(replyID), pStrmDbID_(0)
	{}
	virtual ~LoadEntityHandler() {}

	void loadEntity();

	// IDatabase::IGetEntityHandler/Database::GetEntityHandler overrides
	virtual EntityDBKey& key()					{	return ekey_;	}
	virtual EntityDBRecordOut& outrec() 		{	return outRec_;	}
	virtual void onGetEntityCompleted( bool isOK );

	// IDatabase::IPutEntityHandler override
	virtual void onPutEntityComplete( bool isOK, DatabaseID );

	// Database::ICheckoutCompletionListener override
	virtual void onCheckoutCompleted( const EntityMailBoxRef* pBaseRef );

private:
	void sendAlreadyCheckedOutReply( const EntityMailBoxRef& baseRef );
};



void LoadEntityHandler::loadEntity()
{
	// Start reply bundle even though we're not sure the entity exists.
	// This is to take advantage of getEntity() streaming directly into bundle.
	replyBundle_.startReply( replyID_ );
	replyBundle_ << (uint8) DatabaseLoginStatus::LOGGED_ON;

	if (ekey_.dbID)
	{
		replyBundle_ << ekey_.dbID;
	}
	else
	{
		// Reserve space for a DBId since we don't know what it is yet.
		pStrmDbID_ = reinterpret_cast<DatabaseID*>(replyBundle_.reserve(sizeof(*pStrmDbID_)));
	}

	pBaseRef_ = &baseRef_;
	outRec_.provideBaseMB( pBaseRef_ );		// Get base mailbox into baseRef_
	outRec_.provideStrm( replyBundle_ );	// Get entity data into bundle
	Database::instance().getEntity( *this, true );
	// When getEntity() completes, onGetEntityCompleted() is called.
}

/**
 *	Database::GetEntityHandler override
 */
void LoadEntityHandler::onGetEntityCompleted( bool isOK )
{
	if (isOK)
	{
		if ( !pBaseRef_ &&	// same as outRec_.getBaseMB()
			 Database::instance().onStartEntityCheckout( ekey_.typeID, ekey_.dbID ) )
		{	// Not checked out and not in the process of being checked out.
			if (pStrmDbID_)
			{
				// Now patch the dbID in the stream.
				*pStrmDbID_ = ekey_.dbID;
			}

			// Check out entity.
			baseRef_.init( objID_, srcAddr_, EntityMailBoxRef::BASE, ekey_.typeID );

			pBaseRef_ = &baseRef_;
			EntityDBRecordIn inrec;
			inrec.provideBaseMB( pBaseRef_ );
			Database::instance().putEntity( ekey_, inrec, *this );
			// When putEntity() completes, onPutEntityComplete() is called.
			// __kyl__ (24/6/2005) Race condition when multiple check-outs of the same entity
			// at the same time. More than one can succeed. Need to check for checking out entity?
			// Also, need to prevent deletion of entity which checking out entity.
			return;	// Don't delete ourselves just yet.
		}
		else if (pBaseRef_) // Already checked out
		{
			this->sendAlreadyCheckedOutReply( *pBaseRef_ );
		}
		else // In the process of being checked out.
		{
			MF_VERIFY( Database::instance().registerCheckoutCompletionListener(
					ekey_.typeID, ekey_.dbID, *this ) );
			// onCheckoutCompleted() will be called when the entity is fully
			// checked out.
			return;	// Don't delete ourselves just yet.
		}
	}
	else
	{
		if (ekey_.dbID)
			ERROR_MSG( "Database::loadEntity: No such entity %"FMT_DBID" of type %d.\n",
					ekey_.dbID, ekey_.typeID );
		else
			ERROR_MSG( "Database::loadEntity: No such entity %s of type %d.\n",
					ekey_.name.c_str(), ekey_.typeID );
		Database::instance().sendFailure( replyID_, srcAddr_,
			DatabaseLoginStatus::LOGIN_REJECTED_NO_SUCH_USER,
			"No such user." );
	}
	delete this;
}

/**
 *	IDatabase::IPutEntityHandler override
 */
void LoadEntityHandler::onPutEntityComplete( bool isOK, DatabaseID )
{
	if (isOK)
	{
		Database::getChannel( srcAddr_ ).send( &replyBundle_ );
	}
	else
	{
		// Something horrible like database disconnected or something.
		Database::instance().sendFailure( replyID_, srcAddr_,
			DatabaseLoginStatus::LOGIN_REJECTED_DB_GENERAL_FAILURE,
			"Unexpected failure from database layer." );
	}
	// Need to call onCompleteEntityCheckout() after we send reply since
	// onCompleteEntityCheckout() can trigger other tasks to send their
	// own replies which assumes that the entity creation has already
	// succeeded or failed (depending on the parameters we pass here).
	// Obviously, if they sent their reply before us, then the BaseApp will
	// get pretty confused since the entity creation is not completed until
	// it gets our reply.
	Database::instance().onCompleteEntityCheckout( ekey_.typeID,
			ekey_.dbID, (isOK) ? &baseRef_ : NULL );

	delete this;
}

/**
 *	Database::ICheckoutCompletionListener override
 */
void LoadEntityHandler::onCheckoutCompleted( const EntityMailBoxRef* pBaseRef )
{
	if (pBaseRef)
	{
		this->sendAlreadyCheckedOutReply( *pBaseRef );
	}
	else
	{
		// __kyl__ (11/8/2006) Currently there are no good reason that a
		// checkout would fail. This usually means something has gone horribly
		// wrong. We'll just return an error for now. We could retry the
		// operation from scratch but that's just too much work for now.
		Database::instance().sendFailure( replyID_, srcAddr_,
				DatabaseLoginStatus::LOGIN_REJECTED_DB_GENERAL_FAILURE,
				"Unexpected failure from database layer." );
	}

	delete this;
}

/**
 *	This method sends back a reply that says that the entity is already
 * 	checked out.
 */
void LoadEntityHandler::sendAlreadyCheckedOutReply(
		const EntityMailBoxRef& baseRef )
{
	Mercury::ChannelSender sender( Database::getChannel( srcAddr_ ) );
	Mercury::Bundle & bundle = sender.bundle();

	bundle.startReply( replyID_ );
	bundle << uint8( DatabaseLoginStatus::LOGIN_REJECTED_ALREADY_LOGGED_IN );
	bundle << ekey_.dbID;
	bundle << baseRef;
}

/**
 *	This method handles a message to load an entity from the database.
 */
void Database::loadEntity( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream& input )
{
	EntityDBKey	ekey( 0, 0 );
	DataSectionPtr pSection;
	bool byName;
	ObjectID objID;
	input >> ekey.typeID >> objID >> byName;

	if (!this->getEntityDefs().isValidEntityType( ekey.typeID ))
	{
		ERROR_MSG( "Database::loadEntity: Invalid entity type %d\n",
			ekey.typeID );
		this->sendFailure( header.replyID, srcAddr,
			DatabaseLoginStatus::LOGIN_CUSTOM_DEFINED_ERROR,
			"Invalid entity type" );
		return;
	}

	if (byName)
		input >> ekey.name;
	else
		input >> ekey.dbID;

	LoadEntityHandler* pHandler = new LoadEntityHandler( ekey, srcAddr, objID,
		header.replyID );
	pHandler->loadEntity();
}

/**
 *	This class processes a request to delete an entity from the database.
 */
class DeleteEntityHandler : public Database::GetEntityHandler
                          , public IDatabase::IDelEntityHandler
{
	Mercury::Bundle		replyBundle_;
	Mercury::Address	srcAddr_;
	EntityDBKey			ekey_;
	EntityMailBoxRef	baseRef_;
	EntityMailBoxRef*	pBaseRef_;
	EntityDBRecordOut	outRec_;

public:
	DeleteEntityHandler( EntityTypeID typeID, DatabaseID dbID,
		const Mercury::Address& srcAddr, Mercury::ReplyID replyID );
	DeleteEntityHandler( EntityTypeID typeID, const std::string& name,
		const Mercury::Address& srcAddr, Mercury::ReplyID replyID );
	virtual ~DeleteEntityHandler() {}

	void deleteEntity();

	// IDatabase::IGetEntityHandler/Database::GetEntityHandler overrides
	virtual EntityDBKey& key()					{	return ekey_;	}
	virtual EntityDBRecordOut& outrec()			{	return outRec_;	}
	virtual void onGetEntityCompleted( bool isOK );

	// IDatabase::IDelEntityHandler override
	virtual void onDelEntityComplete( bool isOK );
};

/**
 *	Constructor. For deleting entity by database ID.
 */
DeleteEntityHandler::DeleteEntityHandler( EntityTypeID typeID,
	DatabaseID dbID, const Mercury::Address& srcAddr, Mercury::ReplyID replyID )
	: replyBundle_(), srcAddr_(srcAddr), ekey_(typeID, dbID),
	baseRef_(), pBaseRef_(0), outRec_()
{
	replyBundle_.startReply(replyID);
}

/**
 *	Constructor. For deleting entity by name.
 */
DeleteEntityHandler::DeleteEntityHandler( EntityTypeID typeID,
		const std::string& name, const Mercury::Address& srcAddr,
		Mercury::ReplyID replyID )
	: replyBundle_(), srcAddr_(srcAddr), ekey_(typeID, 0, name),
	baseRef_(), pBaseRef_(0), outRec_()
{
	replyBundle_.startReply(replyID);
}

/**
 *	Starts the process of deleting the entity.
 */
void DeleteEntityHandler::deleteEntity()
{
	if (Database::instance().getEntityDefs().isValidEntityType( ekey_.typeID ))
	{
		// See if it is checked out
		pBaseRef_ = &baseRef_;
		outRec_.provideBaseMB(pBaseRef_);
		Database::instance().getEntity( *this );
		// When getEntity() completes, onGetEntityCompleted() is called.
	}
	else
	{
		ERROR_MSG( "DeleteEntityHandler::deleteEntity: Invalid entity type "
				"%d\n", int(ekey_.typeID) );
		replyBundle_ << int32(-1);

		Database::getChannel( srcAddr_ ).send( &replyBundle_ );
		delete this;
	}
}

/**
 *	Database::GetEntityHandler overrides
 */
void DeleteEntityHandler::onGetEntityCompleted( bool isOK )
{
	if (isOK)
	{
		if (Database::isValidMailBox(outRec_.getBaseMB()))
		{
			TRACE_MSG( "Database::deleteEntity: entity checked out\n" );
			// tell the caller where to find it
			replyBundle_ << *outRec_.getBaseMB();
		}
		else
		{	// __kyl__ TODO: Is it a problem if we delete the entity when it's awaiting creation?
			Database::instance().getIDatabase().delEntity( ekey_, *this );
			// When delEntity() completes, onDelEntityComplete() is called.
			return;	// Don't send reply just yet.
		}
	}
	else
	{	// Entity doesn't exist
		TRACE_MSG( "Database::deleteEntity: no such entity\n" );
		replyBundle_ << int32(-1);
	}

	Database::getChannel( srcAddr_ ).send( &replyBundle_ );

	delete this;
}

/**
 *	IDatabase::IDelEntityHandler overrides
 */
void DeleteEntityHandler::onDelEntityComplete( bool isOK )
{
	if ( isOK )
	{
		TRACE_MSG( "Database::deleteEntity: succeeded\n" );
	}
	else
	{
		ERROR_MSG( "Database::deleteEntity: Failed to delete entity '%s' "
					"(%"FMT_DBID") of type %d\n",
					ekey_.name.c_str(), ekey_.dbID, ekey_.typeID );
		replyBundle_ << int32(-1);
	}

	Database::getChannel( srcAddr_ ).send( &replyBundle_ );

	delete this;
}

/**
 *	This message deletes the specified entity if it exists and is not
 *	checked out. If it is checked out, it returns a mailbox to it instead.
 *	If it does not exist, it returns -1 as an int32.
 */
void Database::deleteEntity( const Mercury::Address & srcAddr,
	Mercury::UnpackedMessageHeader & header,
	DBInterface::deleteEntityArgs & args )
{
	DeleteEntityHandler* pHandler = new DeleteEntityHandler( args.entityTypeID,
		args.dbid, srcAddr, header.replyID );
	pHandler->deleteEntity();
}

/**
 *	This message deletes the specified entity if it exists and is not
 *	checked out, and returns an empty message. If it is checked out,
 *	it returns a mailbox to it instead. If it does not exist,
 *	it returns -1 as an int32.
 */
void Database::deleteEntityByName( const Mercury::Address & srcAddr,
	Mercury::UnpackedMessageHeader & header,
	BinaryIStream & data )
{
	EntityTypeID	entityTypeID;
	std::string		name;
	data >> entityTypeID >> name;

	DeleteEntityHandler* pHandler = new DeleteEntityHandler( entityTypeID,
		name, srcAddr, header.replyID );
	pHandler->deleteEntity();
}

/**
 *	This class processes a request to retrieve the base mailbox of a
 *	checked-out entity from the database.
 */
class LookupEntityHandler : public Database::GetEntityHandler
{
	Mercury::Bundle		replyBundle_;
	Mercury::Address	srcAddr_;
	EntityDBKey			ekey_;
	EntityMailBoxRef	baseRef_;
	EntityMailBoxRef*	pBaseRef_;
	EntityDBRecordOut	outRec_;

public:
	LookupEntityHandler( EntityTypeID typeID, DatabaseID dbID,
		const Mercury::Address& srcAddr, Mercury::ReplyID replyID );
	LookupEntityHandler( EntityTypeID typeID, const std::string& name,
		const Mercury::Address& srcAddr, Mercury::ReplyID replyID );
	virtual ~LookupEntityHandler() {}

	void lookupEntity();

	// IDatabase::IGetEntityHandler/Database::GetEntityHandler overrides
	virtual EntityDBKey& key()					{	return ekey_;	}
	virtual EntityDBRecordOut& outrec()			{	return outRec_;	}
	virtual void onGetEntityCompleted( bool isOK );

};

/**
 *	Constructor. For looking up entity by database ID.
 */
LookupEntityHandler::LookupEntityHandler( EntityTypeID typeID,
		DatabaseID dbID, const Mercury::Address& srcAddr,
		Mercury::ReplyID replyID ) :
	replyBundle_(), srcAddr_(srcAddr), ekey_(typeID, dbID),
	baseRef_(), pBaseRef_(0), outRec_()
{
	replyBundle_.startReply(replyID);
}

/**
 *	Constructor. For looking up entity by name.
 */
LookupEntityHandler::LookupEntityHandler( EntityTypeID typeID,
		const std::string& name, const Mercury::Address& srcAddr,
		Mercury::ReplyID replyID ) :
	replyBundle_(), srcAddr_(srcAddr), ekey_(typeID, 0, name),
	baseRef_(), pBaseRef_(0), outRec_()
{
	replyBundle_.startReply(replyID);
}

/**
 *	Starts the process of looking up the entity.
 */
void LookupEntityHandler::lookupEntity()
{
	if (Database::instance().getEntityDefs().isValidEntityType( ekey_.typeID ))
	{
		pBaseRef_ = &baseRef_;
		outRec_.provideBaseMB(pBaseRef_);
		Database::instance().getEntity( *this );
		// When getEntity() completes, onGetEntityCompleted() is called.
	}
	else
	{
		ERROR_MSG( "LookupEntityHandler::lookupEntity: Invalid entity type "
				"%d\n", ekey_.typeID );
		replyBundle_ << int32(-1);

		Database::getChannel( srcAddr_ ).send( &replyBundle_ );

		delete this;
	}
}

void LookupEntityHandler::onGetEntityCompleted( bool isOK )
{
	if (isOK)
	{
		if (Database::isValidMailBox(outRec_.getBaseMB()))
		{	// Entity is checked out.
			replyBundle_ << *outRec_.getBaseMB();
		}
		else
		{
			// not checked out so empty message
		}
	}
	else
	{	// Entity doesn't exist
		replyBundle_ << int32(-1);
	}

	Database::getChannel( srcAddr_ ).send( &replyBundle_ );

	delete this;
}

/**
 *	This message looks up the specified entity if it exists and is checked
 *	out and returns a mailbox to it. If it is not checked out it returns
 *	an empty message. If it does not exist, it returns -1 as an int32.
 */
void Database::lookupEntity( const Mercury::Address & srcAddr,
	Mercury::UnpackedMessageHeader & header,
	DBInterface::lookupEntityArgs & args )
{
	LookupEntityHandler* pHandler = new LookupEntityHandler( args.entityTypeID,
		args.dbid, srcAddr, header.replyID );
	pHandler->lookupEntity();
}

/**
 *	This message looks up the specified entity if it exists and is checked
 *	out and returns a mailbox to it. If it is not checked out it returns
 *	an empty message. If it does not exist, it returns -1 as an int32.
 */
void Database::lookupEntityByName( const Mercury::Address & srcAddr,
	Mercury::UnpackedMessageHeader & header,
	BinaryIStream & data )
{
	EntityTypeID	entityTypeID;
	std::string		name;
	data >> entityTypeID >> name;

	LookupEntityHandler* pHandler = new LookupEntityHandler( entityTypeID,
		name, srcAddr, header.replyID );
	pHandler->lookupEntity();
}


/**
 *	This class processes a request to retrieve the DBID of an entity from the
 * 	database.
 */
class LookupDBIDHandler : public Database::GetEntityHandler
{
	Mercury::Bundle		replyBundle_;
	Mercury::Address	srcAddr_;
	EntityDBKey			ekey_;
	EntityDBRecordOut	outRec_;

public:
	LookupDBIDHandler( EntityTypeID typeID, const std::string& name,
			const Mercury::Address& srcAddr, Mercury::ReplyID replyID ) :
		replyBundle_(), srcAddr_( srcAddr ), ekey_( typeID, 0, name ), outRec_()
	{
		replyBundle_.startReply( replyID );
	}
	virtual ~LookupDBIDHandler() {}

	void lookupDBID();

	// IDatabase::IGetEntityHandler/Database::GetEntityHandler overrides
	virtual EntityDBKey& key()					{	return ekey_;	}
	virtual EntityDBRecordOut& outrec()			{	return outRec_;	}
	virtual void onGetEntityCompleted( bool isOK );

};

/**
 *	Starts the process of looking up the DBID.
 */
void LookupDBIDHandler::lookupDBID()
{
	if (Database::instance().getEntityDefs().isValidEntityType( ekey_.typeID ))
	{
		Database::instance().getEntity( *this );
		// When getEntity() completes, onGetEntityCompleted() is called.
	}
	else
	{
		ERROR_MSG( "LookupDBIDHandler::lookupDBID: Invalid entity type "
				"%d\n", ekey_.typeID );
		replyBundle_ << DatabaseID( 0 );
		Database::getChannel( srcAddr_ ).send( &replyBundle_ );
		delete this;
	}
}

/**
 *	Database::GetEntityHandler override.
 */
void LookupDBIDHandler::onGetEntityCompleted( bool isOK )
{
	replyBundle_ << ekey_.dbID;
	Database::getChannel( srcAddr_ ).send( &replyBundle_ );

	delete this;
}


/**
 *	This message looks up the DBID of the entity. The DBID will be 0 if the
 * 	entity does not exist.
 */
void Database::lookupDBIDByName( const Mercury::Address & srcAddr,
	Mercury::UnpackedMessageHeader & header, BinaryIStream & data )
{
	EntityTypeID	entityTypeID;
	std::string		name;
	data >> entityTypeID >> name;

	LookupDBIDHandler* pHandler = new LookupDBIDHandler( entityTypeID,
		name, srcAddr, header.replyID );
	pHandler->lookupDBID();
}


// -----------------------------------------------------------------------------
// Section: Miscellaneous database requests
// -----------------------------------------------------------------------------

/**
 *	This class represents a request to execute a raw database command.
 */
class ExecuteRawCommandHandler : public IDatabase::IExecuteRawCommandHandler
{
	Mercury::Bundle		replyBundle_;
	Mercury::Address	srcAddr_;

public:
	ExecuteRawCommandHandler( const Mercury::Address srcAddr,
			Mercury::ReplyID replyID ) :
		replyBundle_(), srcAddr_(srcAddr)
	{
		replyBundle_.startReply(replyID);
	}
	virtual ~ExecuteRawCommandHandler() {}

	void executeRawCommand( const std::string& command )
	{
		Database::instance().getIDatabase().executeRawCommand( command, *this );
	}

	// IDatabase::IExecuteRawCommandHandler overrides
	virtual BinaryOStream& response()	{	return replyBundle_;	}
	virtual void onExecuteRawCommandComplete()
	{
		Database::getChannel( srcAddr_ ).send( &replyBundle_ );

		delete this;
	}
};

/**
 *	This method executaes a raw database command specific to the present
 *	implementation of the database interface.
 */
void Database::executeRawCommand( const Mercury::Address & srcAddr,
	Mercury::UnpackedMessageHeader & header,
	BinaryIStream & data )
{
	std::string command( (char*)data.retrieve( header.length ), header.length );
	ExecuteRawCommandHandler* pHandler =
		new ExecuteRawCommandHandler( srcAddr, header.replyID );
	pHandler->executeRawCommand( command );
}

/**
 *  This method stores some previously used ID's into the database
 */
void Database::putIDs( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream& input )
{
	int numIDs = input.remainingLength() / sizeof(ObjectID);
	INFO_MSG( "Database::putIDs: storing %d id's\n", numIDs);
	pDatabase_->putIDs( numIDs,
			static_cast<const ObjectID*>( input.retrieve( input.remainingLength() ) ) );
}

/**
 *	This class represents a request to get IDs from the database.
 */
class GetIDsHandler : public IDatabase::IGetIDsHandler
{
	Mercury::Address	srcAddr_;
	Mercury::Bundle		replyBundle_;

public:
	GetIDsHandler( const Mercury::Address& srcAddr, Mercury::ReplyID replyID ) :
		srcAddr_(srcAddr), replyBundle_()
	{
		replyBundle_.startReply( replyID );
	}
	virtual ~GetIDsHandler() {}

	void getIDs( int numIDs )
	{
		Database::instance().getIDatabase().getIDs( numIDs, *this );
	}

	virtual BinaryOStream& idStrm()	{ return replyBundle_; }
	virtual void onGetIDsComplete()
	{
		INFO_MSG( "Sending IDs to %s\n", srcAddr_.c_str() );
		Database::getChannel( srcAddr_ ).send( &replyBundle_ );
		delete this;
	}
};

/**
 *  This methods grabs some more ID's from the database
 */
void Database::getIDs( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream& input )
{
	int numIDs;
	input >> numIDs;
	INFO_MSG( "Database::getIDs: fetching %d id's\n", numIDs);

	GetIDsHandler* pHandler = new GetIDsHandler( srcAddr, header.replyID );
	pHandler->getIDs( numIDs );
}


/**
 *	This method writes information about the spaces to the database.
 */
void Database::writeSpaces( const Mercury::Address & /*srcAddr*/,
		Mercury::UnpackedMessageHeader & /*header*/,
		BinaryIStream & data )
{
	int numSpaces;
	data >> numSpaces;

	pDatabase_->startSpaces();

	for (int spaceIndex = 0; spaceIndex < numSpaces; ++spaceIndex)
	{
		SpaceID spaceID;
		data >> spaceID;

		pDatabase_->writeSpace( spaceID );

		int numData;
		data >> numData;

		for (int dataIndex = 0; dataIndex < numData; ++dataIndex)
		{
			int64 spaceKey;
			uint16 dataKey;
			std::string spaceData;

			data >> spaceKey >> dataKey >> spaceData;

			pDatabase_->writeSpaceData( spaceID, spaceKey, dataKey, spaceData );
		}
	}

	pDatabase_->endSpaces();
}


/**
 *	This method handles a message from the BaseAppMgr informing us that a
 *	BaseApp has died.
 */
void Database::handleBaseAppDeath( const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data )
{
	if (this->hasMailboxRemapping())
	{
		ERROR_MSG( "Database::handleBaseAppDeath: Multiple BaseApp deaths "
				"not supported. Some mailboxes may not be updated\n" );
		this->endMailboxRemapping();
	}

	data >> remappingSrcAddr_ >> remappingDestAddrs_;

	INFO_MSG( "Database::handleBaseAppDeath: %s\n", (char *)remappingSrcAddr_ );

	pDatabase_->remapEntityMailboxes( remappingSrcAddr_, remappingDestAddrs_ );

	mailboxRemapCheckCount_ = 5;	// do remapping for 5 seconds.
}

/**
 *	This method ends the mailbox remapping for a dead BaseApp.
 */
void Database::endMailboxRemapping()
{
//	DEBUG_MSG( "Database::endMailboxRemapping: End of handleBaseAppDeath\n" );
	remappingDestAddrs_.clear();
}

/**
 *	This method changes the address of input mailbox to cater for recent
 * 	BaseApp death.
 */
void Database::remapMailbox( EntityMailBoxRef& mailbox ) const
{
	if (mailbox.addr == remappingSrcAddr_)
	{
		const Mercury::Address& newAddr =
				remappingDestAddrs_.addressFor( mailbox.id );
		// Mercury::Address::salt must not be modified.
		mailbox.addr.ip = newAddr.ip;
		mailbox.addr.port = newAddr.port;
	}
}

/**
 *	This method writes the game time to the database.
 */
void Database::writeGameTime( DBInterface::writeGameTimeArgs & args )
{
	pDatabase_->setGameTime( args.gameTime );
}

/**
 *	This method handles a resource version tag message. This is sent as the
 *	first message in a bundle if the rest of the messages in the bundle
 * 	needs versioning (e.g. during updater operation).
 */
void Database::resourceVersionTag( DBInterface::resourceVersionTagArgs& args )
{
	if ( pDbUpdater_ )
		pDbUpdater_->onRcvResourceVersionTag( uint32(args.tag) );
	else
		ERROR_MSG( "Database::resourceVersionTag: Version tag ignored when "
					"update not in progress\n" );
}

/**
 *	This method handles updater control messages.
 */
void Database::resourceVersionControl(
	DBInterface::resourceVersionControlArgs & args )
{
	DEBUG_MSG( "Database::resourceVersionControl: "
		"Step=%lu, version=%lu\n", args.activity, args.version );
	switch (args.activity)
	{
	case DbUpdater::ResVerCtrlStart:
		{
			if (pDbUpdater_)
			{
				if (pDbUpdater_->isOKToRestart())
				{
					delete pDbUpdater_;
				}
				else
				{
					ERROR_MSG( "Database::resourceVersionControl: Update "
								"already in progress - ignoring second update "
								"request\n" );
					break;
				}
			}
			// TODO: get from srcAddr instead of looking it up!
			Mercury::Address updaterAddr;
			this->nub().findInterface( "UpdaterInterface", 0, updaterAddr );
			pDbUpdater_ = new DbUpdater( updaterAddr, this->nub(), args );
		}
		break;
	case DbUpdater::ResVerCtrlLoadNewResource:
		{
			// Set up global BWResource before calling DBUpdater
			// figure out the new res path
			std::string verPath = BWResource::getPath( 0 );
			verPath = verPath.substr( 0, verPath.find_last_of( '/' )+1 );
			std::stringstream	verPathStrm( verPath );
			verPathStrm << "ver/" << args.version << "/";
			verPath = verPathStrm.str();
			// Delete the last updated resource before replacing with new one.
			if (pUpdatedBWResource_)
				delete pUpdatedBWResource_;
			pUpdatedBWResource_ = new BWResource( &verPath );
			// Set global resource to the one with the new res.
			BWResource::instance( pUpdatedBWResource_ );
			// TODO: __kyl__(28/10/2005) Setting the global BWResource
			// permanently is probably a bad thing. We should do a similar thing
			// to the Python interpreter where we swap between the "old" and the
			// "new" when we need it (and probably at the exact same places
			// where we swap Python interpreter) but this is a bit too expensive
			// at the moment because DataSectionCensus is cleared every time we
			// swap in a BWResource.

			pDbUpdater_->onRcvResourceVersionControl( args );
		}
		break;
	case DbUpdater::ResVerCtrlEnd:
		MF_ASSERT( pDbUpdater_ );
		pDbUpdater_->onRcvResourceVersionControl( args );
		delete pDbUpdater_;
		pDbUpdater_ = 0;
		break;
	default:
		MF_ASSERT( pDbUpdater_ );
		pDbUpdater_->onRcvResourceVersionControl( args );
		break;
	}
}

/**
 *	See DbUpdater::getBundleResourceVersion.
 */
inline uint32 Database::getBundleResourceVersion() const
{
	return (pDbUpdater_) ?
		pDbUpdater_->getBundleResourceVersion() : INVALID_RESOURCE_VERSION;
}

/**
 *	See DbUpdater::getOldDatabaseForBundle.
 */
inline IDatabase::IOldDatabase* Database::getOldDatabaseForBundle() const
{
	return (pDbUpdater_) ? pDbUpdater_->getOldDatabaseForBundle() : 0;
}

/**
 *	This method sets whether we have started. It is used so that the server can
 *	be started from a watcher.
 */
void Database::hasStartBegun( bool hasStartBegun )
{
	if (!hasStartBegun_ && hasStartBegun)
	{
		this->startServerBegin();
	}
}


/**
 *	This method starts the process of starting the server.
 */
void Database::startServerBegin( bool isRecover )
{
	if (hasStartBegun_)
		return;

	// Don't restore game state if we are being revived.
	bool hasFinished = (!isRecover) ? pDatabase_->restoreGameState() : true;

	hasStartBegun_ = true;

	if (hasFinished)
	{
		this->startServerEnd( isRecover );
	}
}

/**
 *	This method completes the starting process for the DBMgr and starts all of
 *	the other processes in the system.
 */
void Database::startServerEnd( bool isRecover )
{
	if (!hasStarted_)
	{
		hasStarted_ = true;
		if (!isRecover)
		{
			Mercury::ChannelSender sender(
				Database::instance().baseAppMgr().channel() );

			sender.bundle().startMessage( BaseAppMgrInterface::startup );
		}
	}
	else
	{
		ERROR_MSG( "Database::startServerEnd: Already started.\n" );
	}
}

/**
 *	This method is the called when an entity that is being checked out has
 * 	completed the checkout process. onStartEntityCheckout() should've been
 * 	called to mark the start of the operation. typeID and dbID identifies
 * 	the entity, while pBaseRef is the base mailbox of the now checked out
 * 	entity. pBaseRef should be NULL if the checkout operation failed.
 */
bool Database::onCompleteEntityCheckout( EntityTypeID typeID, DatabaseID dbID,
	const EntityMailBoxRef* pBaseRef )
{
	EntityID key( typeID, dbID );
	bool isErased = (inProgCheckouts_.erase( key ) > 0);
	if (isErased && (checkoutCompletionListeners_.size() > 0))
	{
		std::pair< CheckoutCompletionListeners::iterator,
				CheckoutCompletionListeners::iterator > listeners =
			checkoutCompletionListeners_.equal_range( key );
		for ( CheckoutCompletionListeners::const_iterator iter =
				listeners.first; iter != listeners.second; ++iter )
		{
			iter->second->onCheckoutCompleted( pBaseRef );
		}
		checkoutCompletionListeners_.erase( listeners.first,
				listeners.second );
	}
	return isErased;
}

/**
 *	This method registers listener to be called when the entity identified
 * 	by typeID and dbID completes its checkout process. This function will
 * 	false and not register the listener if the entity is not currently
 * 	in the process of being checked out.
 */
bool Database::registerCheckoutCompletionListener( EntityTypeID typeID,
		DatabaseID dbID, Database::ICheckoutCompletionListener& listener )
{
	EntityIDSet::key_type key( typeID, dbID );
	bool isFound = (inProgCheckouts_.find( key ) != inProgCheckouts_.end());
	if (isFound)
	{
		CheckoutCompletionListeners::value_type item( key, &listener );
		checkoutCompletionListeners_.insert( item );
	}
	return isFound;
}


// -----------------------------------------------------------------------------
// Section: Message handling glue templates
// -----------------------------------------------------------------------------

/**
 *	This class is used to handle a fixed length request made of the database.
 */
template <class ARGS_TYPE>
class SimpleDBMessageHandler : public Mercury::InputMessageHandler
{
	public:
		typedef void (Database::*Handler)( ARGS_TYPE & args );

		SimpleDBMessageHandler( Handler handler ) : handler_( handler ) {}

	private:
		virtual void handleMessage( const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data )
		{
			ARGS_TYPE args;
			if (sizeof(args) != 0)
				data >> args;
			(Database::instance().*handler_)( args );
		}

		Handler handler_;
};


/**
 *	This class is used to handle a fixed length request made of the database.
 */
template <class ARGS_TYPE>
class ReturnDBMessageHandler : public Mercury::InputMessageHandler
{
	public:
		typedef void (Database::*Handler)( const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			ARGS_TYPE & args );

		ReturnDBMessageHandler( Handler handler ) : handler_( handler ) {}

	private:
		virtual void handleMessage( const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data )
		{
			ARGS_TYPE & args = *(ARGS_TYPE*)data.retrieve( sizeof(ARGS_TYPE) );
			(Database::instance().*handler_)( srcAddr, header, args );
		}

		Handler handler_;
};



/**
 *	This class is used to handle a variable length request made of the database.
 */
class DBVarLenMessageHandler : public Mercury::InputMessageHandler
{
	public:
		typedef void (Database::*Handler)( const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data );

		DBVarLenMessageHandler( Handler handler ) : handler_( handler ) {}

	private:
		virtual void handleMessage( const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data )
		{
			(Database::instance().*handler_)( srcAddr, header, data );
		}

		Handler handler_;
};

#ifdef DBMGR_SELFTEST
#include "cstdmf/memory_stream.hpp"

class SelfTest : public IDatabase::IGetEntityHandler,
	             public IDatabase::IPutEntityHandler,
				 public IDatabase::IDelEntityHandler
{
	int				stepNum_;
	IDatabase&		db_;

	std::string		entityName_;
	std::string		badEntityName_;
	EntityTypeID	entityTypeID_;
	DatabaseID		newEntityID_;
	DatabaseID		badEntityID_;
	MemoryOStream	entityData_;
	EntityMailBoxRef entityBaseMB_;

	EntityDBKey		ekey_;
	EntityDBRecordOut outRec_;
	MemoryOStream	tmpEntityData_;
	EntityMailBoxRef tmpEntityBaseMB_;
	EntityMailBoxRef* pTmpEntityBaseMB_;

public:
	SelfTest( IDatabase& db ) :
		stepNum_(0), db_(db),
		entityName_("test_entity"), badEntityName_("Ben"), entityTypeID_(0),
		newEntityID_(0), badEntityID_(0), entityData_(), entityBaseMB_(),
		ekey_( 0, 0 ), outRec_(), tmpEntityData_(), tmpEntityBaseMB_(),
		pTmpEntityBaseMB_(0)
	{
		entityBaseMB_.init( 123, Mercury::Address( 7654321, 1234 ), EntityMailBoxRef::CLIENT_VIA_CELL, 1 );
	}
	virtual ~SelfTest() {}

	void nextStep();

	// IDatabase::IGetEntityHandler overrides
	virtual EntityDBKey& key()					{	return ekey_;	}
	virtual EntityDBRecordOut& outrec()			{	return outRec_;	}
	virtual void onGetEntityComplete( bool isOK );

	// IDatabase::IPutEntityHandler override
	virtual void onPutEntityComplete( bool isOK, DatabaseID );

	// IDatabase::IDelEntityHandler override
	virtual void onDelEntityComplete( bool isOK );
};

void SelfTest::nextStep()
{
	TRACE_MSG( "SelfTest::nextStep - step %d\n", stepNum_ + 1);
	switch (++stepNum_)
	{
		case 1:
		{	// Create new entity
			MemoryOStream strm;
			bool isDefaultEntityOK =
				Database::instance().defaultEntityToStrm( entityTypeID_, entityName_, strm );
			MF_ASSERT( isDefaultEntityOK );

			EntityDBRecordIn erec;
			erec.provideStrm( strm );
			EntityDBKey	ekey( entityTypeID_, 0);
			db_.putEntity( ekey, erec, *this );
			break;
		}
		case 2:
		{	// Check entity exists by name
			outRec_.unprovideBaseMB();
			outRec_.unprovideStrm();
			ekey_ = EntityDBKey( entityTypeID_, 0, entityName_ );
			db_.getEntity( *this );
			break;
		}
		case 3:
		{	// Check entity not exists by name
			outRec_.unprovideBaseMB();
			outRec_.unprovideStrm();
			ekey_ = EntityDBKey( entityTypeID_, 0, badEntityName_ );
			db_.getEntity( *this );
			break;
		}
		case 4:
		{	// Check entity exists by ID
			outRec_.unprovideBaseMB();
			outRec_.unprovideStrm();
			ekey_ = EntityDBKey( entityTypeID_, newEntityID_ );
			db_.getEntity( *this );
			break;
		}
		case 5:
		{	// Check entity not exists by ID
			outRec_.unprovideBaseMB();
			outRec_.unprovideStrm();
			ekey_ = EntityDBKey( entityTypeID_, badEntityID_ );
			db_.getEntity( *this );
			break;
		}
		case 6:
		{	// Get entity data by ID.
			outRec_.unprovideBaseMB();
			outRec_.provideStrm( entityData_ );
			ekey_ = EntityDBKey( entityTypeID_, newEntityID_ );
			db_.getEntity( *this );
			break;
		}
		case 7:
		{	// Get entity data by non-existent ID.
			outRec_.unprovideBaseMB();
			tmpEntityData_.reset();
			outRec_.provideStrm( tmpEntityData_ );
			ekey_ = EntityDBKey( entityTypeID_, badEntityID_ );
			db_.getEntity( *this );
			break;
		}
		case 8:
		{	// Overwrite entity data
			EntityDBRecordIn erec;
			erec.provideStrm( entityData_ );
			EntityDBKey	ekey( entityTypeID_, newEntityID_ );
			db_.putEntity( ekey, erec, *this );
			break;
		}
		case 9:
		{	// Get entity data by name.
			outRec_.unprovideBaseMB();
			tmpEntityData_.reset();
			outRec_.provideStrm( tmpEntityData_ );
			ekey_ = EntityDBKey( entityTypeID_, 0, entityName_ );
			db_.getEntity( *this );
			break;
		}
		case 10:
		{	// Overwrite non-existent entity with new data
			EntityDBRecordIn erec;
			erec.provideStrm( entityData_ );
			EntityDBKey	ekey( entityTypeID_, badEntityID_ );
			db_.putEntity( ekey, erec, *this );
			break;
		}
		case 11:
		{	// Get entity data with bad name.
			outRec_.unprovideBaseMB();
			tmpEntityData_.reset();
			outRec_.provideStrm( tmpEntityData_ );
			ekey_ = EntityDBKey( entityTypeID_, 0, badEntityName_ );
			db_.getEntity( *this );
			break;
		}
		case 12:
		{	// Get NULL base MB by ID
			pTmpEntityBaseMB_ = &tmpEntityBaseMB_;
			outRec_.provideBaseMB( pTmpEntityBaseMB_ );
			outRec_.unprovideStrm();
			ekey_ = EntityDBKey( entityTypeID_, newEntityID_ );
			db_.getEntity( *this );
			break;
		}
		case 13:
		{	// Get NULL base MB by bad ID
			pTmpEntityBaseMB_ = &tmpEntityBaseMB_;
			outRec_.provideBaseMB( pTmpEntityBaseMB_ );
			outRec_.unprovideStrm();
			ekey_ = EntityDBKey( entityTypeID_, badEntityID_ );
			db_.getEntity( *this );
			break;
		}
		case 14:
		{	// Put base MB with ID
			EntityMailBoxRef* pBaseRef = &entityBaseMB_;
			EntityDBRecordIn erec;
			erec.provideBaseMB( pBaseRef );
			EntityDBKey	ekey( entityTypeID_, newEntityID_ );
			db_.putEntity( ekey, erec, *this );
			break;
		}
		case 15:
		{	// Put base MB with bad ID
			EntityMailBoxRef baseMB;
			EntityMailBoxRef* pBaseRef = &baseMB;
			EntityDBRecordIn erec;
			erec.provideBaseMB( pBaseRef );
			EntityDBKey	ekey( entityTypeID_, badEntityID_ );
			db_.putEntity( ekey, erec, *this );
			break;
		}
		case 16:
		{	// Get base MB by ID
			tmpEntityBaseMB_.init( 666, Mercury::Address( 66666666, 666 ), EntityMailBoxRef::CLIENT_VIA_BASE, 1 );
			pTmpEntityBaseMB_ = &tmpEntityBaseMB_;
			outRec_.provideBaseMB( pTmpEntityBaseMB_ );
			outRec_.unprovideStrm();
			ekey_ = EntityDBKey( entityTypeID_, newEntityID_ );
			db_.getEntity( *this );
			break;
		}
		case 17:
		{	// Update base MB by ID
			entityBaseMB_.id = 999;
			EntityMailBoxRef* pBaseRef = &entityBaseMB_;
			EntityDBRecordIn erec;
			erec.provideBaseMB( pBaseRef );
			EntityDBKey	ekey( entityTypeID_, newEntityID_ );
			db_.putEntity( ekey, erec, *this );
			break;
		}
		case 18:
		{	// Get entity data and base MB by ID.
			tmpEntityBaseMB_.init( 666, Mercury::Address( 66666666, 666 ), EntityMailBoxRef::CLIENT_VIA_BASE, 1 );
			pTmpEntityBaseMB_ = &tmpEntityBaseMB_;
			outRec_.provideBaseMB( pTmpEntityBaseMB_ );
			tmpEntityData_.reset();
			outRec_.provideStrm( tmpEntityData_ );
			ekey_ = EntityDBKey( entityTypeID_, newEntityID_ );
			db_.getEntity( *this );
			break;
		}
		case 19:
		{	// Put NULL base MB with ID
			EntityMailBoxRef* pBaseRef = 0;
			EntityDBRecordIn erec;
			erec.provideBaseMB( pBaseRef );
			EntityDBKey	ekey( entityTypeID_, newEntityID_ );
			db_.putEntity( ekey, erec, *this );
			break;
		}
		case 20:
		{	// Get NULL base MB by name
			pTmpEntityBaseMB_ = &tmpEntityBaseMB_;
			outRec_.provideBaseMB( pTmpEntityBaseMB_ );
			outRec_.unprovideStrm();
			ekey_ = EntityDBKey( entityTypeID_, 0, entityName_ );
			db_.getEntity( *this );
			break;
		}
		case 21:
		{	// Get NULL base MB by bad name
			pTmpEntityBaseMB_ = &tmpEntityBaseMB_;
			outRec_.provideBaseMB( pTmpEntityBaseMB_ );
			outRec_.unprovideStrm();
			ekey_ = EntityDBKey( entityTypeID_, 0, badEntityName_ );
			db_.getEntity( *this );
			break;
		}
		case 22:
		{	// Delete entity by name
			EntityDBKey	ekey( entityTypeID_, 0, entityName_ );
			db_.delEntity( ekey, *this );
			break;
		}
		case 23:
		{	// Create new entity by stream.
			EntityDBRecordIn erec;
			erec.provideStrm( entityData_ );
			EntityDBKey	ekey( entityTypeID_, 0 );
			db_.putEntity( ekey, erec, *this );
			break;
		}
		case 24:
		{	// Get entity data by name (again)
			outRec_.unprovideBaseMB();
			tmpEntityData_.reset();
			outRec_.provideStrm( tmpEntityData_ );
			ekey_ = EntityDBKey( entityTypeID_, 0, entityName_ );
			db_.getEntity( *this );
			break;
		}
		case 25:
		{	// Delete entity by ID
			EntityDBKey	ekey( entityTypeID_, newEntityID_ );
			db_.delEntity( ekey, *this );
			break;
		}
		case 26:
		{	// Delete non-existent entity by name
			EntityDBKey	ekey( entityTypeID_, 0, entityName_ );
			db_.delEntity( ekey, *this );
			break;
		}
		case 27:
		{	// Delete non-existent entity by ID
			EntityDBKey	ekey( entityTypeID_, newEntityID_ );
			db_.delEntity( ekey, *this );
			break;
		}
		default:
			TRACE_MSG( "SelfTest::nextStep - completed\n", stepNum_ );
			delete this;
			break;
	}
}

void SelfTest::onGetEntityComplete( bool isOK )
{
	switch (stepNum_)
	{
		case 2:
		{
			bool checkEntityWithName = isOK;
			MF_ASSERT( checkEntityWithName && (ekey_.dbID == newEntityID_) );
			break;
		}
		case 3:
		{
			bool checkEntityWithBadName = isOK;
			MF_ASSERT( !checkEntityWithBadName );
			break;
		}
		case 4:
		{
			bool checkEntityWithID = isOK;
			MF_ASSERT( checkEntityWithID && (ekey_.name == entityName_) );
			break;
		}
		case 5:
		{
			bool checkEntityWithBadID = isOK;
			MF_ASSERT( !checkEntityWithBadID );
			break;
		}
		case 6:
		{
			bool getEntityDataWithID = isOK;
			MF_ASSERT( getEntityDataWithID );
			break;
		}
		case 7:
		{
			bool getEntityDataWithBadID = isOK;
			MF_ASSERT( !getEntityDataWithBadID );
			break;
		}
		case 9:
		{
			bool getEntityDataWithName = isOK;
			MF_ASSERT( getEntityDataWithName &&
					(entityData_.size() == tmpEntityData_.size()) &&
					(memcmp( entityData_.data(), tmpEntityData_.data(), entityData_.size()) == 0) );
			break;
		}
		case 11:
		{
			bool getEntityDataWithBadName = isOK;
			MF_ASSERT( !getEntityDataWithBadName );
			break;
		}
		case 12:
		{
			bool getNullBaseMBWithID = isOK;
			MF_ASSERT( getNullBaseMBWithID && outRec_.getBaseMB() == 0 );
			break;
		}
		case 13:
		{
			bool getNullBaseMBWithBadID = isOK;
			MF_ASSERT( !getNullBaseMBWithBadID );
			break;
		}
		case 16:
		{
			bool getBaseMBWithID = isOK;
			MF_ASSERT( getBaseMBWithID && outRec_.getBaseMB() &&
					(tmpEntityBaseMB_.id == entityBaseMB_.id) &&
					(tmpEntityBaseMB_.type() == entityBaseMB_.type()) &&
					(tmpEntityBaseMB_.component() == entityBaseMB_.component()) &&
					(tmpEntityBaseMB_.addr == entityBaseMB_.addr) );
			break;
		}
		case 18:
		{
			bool getEntityWithID = isOK;
			MF_ASSERT( getEntityWithID &&
					(entityData_.size() == tmpEntityData_.size()) &&
					(memcmp( entityData_.data(), tmpEntityData_.data(), entityData_.size()) == 0) &&
					outRec_.getBaseMB() &&
					(tmpEntityBaseMB_.id == entityBaseMB_.id) &&
					(tmpEntityBaseMB_.type() == entityBaseMB_.type()) &&
					(tmpEntityBaseMB_.component() == entityBaseMB_.component()) &&
					(tmpEntityBaseMB_.addr == entityBaseMB_.addr) );
			break;
		}
		case 20:
		{
			bool getNullBaseMBWithName = isOK;
			MF_ASSERT( getNullBaseMBWithName && outRec_.getBaseMB() == 0 );
			break;
		}
		case 21:
		{
			bool getNullBaseMBWithBadName = isOK;
			MF_ASSERT( !getNullBaseMBWithBadName );
			break;
		}
		case 24:
		{
			bool getEntityDataWithName = isOK;
			MF_ASSERT( getEntityDataWithName &&
					(entityData_.size() == tmpEntityData_.size()) &&
					(memcmp( entityData_.data(), tmpEntityData_.data(), entityData_.size()) == 0) );
			break;
		}
		default:
			MF_ASSERT( false );
			break;
	}

	this->nextStep();
}

void SelfTest::onPutEntityComplete( bool isOK, DatabaseID dbID )
{
	switch (stepNum_)
	{
		case 1:
		{
			bool createNewEntity = isOK;
			MF_ASSERT( createNewEntity );
			newEntityID_ = dbID;
			badEntityID_ = newEntityID_ + 9999;
			break;
		}
		case 8:
		{
			bool overwriteEntityData = isOK;
			MF_ASSERT( overwriteEntityData );
			entityData_.rewind();
			break;
		}
		case 10:
		{
			bool putNonExistEntityData = isOK;
			MF_ASSERT( !putNonExistEntityData );
			entityData_.rewind();
			break;
		}
		case 14:
		{
			bool putBaseMBwithID = isOK;;
			MF_ASSERT( putBaseMBwithID );
			break;
		}
		case 15:
		{
			bool putBaseMBwithBadID = isOK;
			MF_ASSERT( !putBaseMBwithBadID );
			break;
		}
		case 17:
		{
			bool updateBaseMBwithID = isOK;
			MF_ASSERT( updateBaseMBwithID );
			break;
		}
		case 19:
		{
			bool putNullBaseMBwithID = isOK;
			MF_ASSERT( putNullBaseMBwithID );
			break;
		}
		case 23:
		{
			bool putEntityData = isOK;
			MF_ASSERT( putEntityData && (dbID != 0) && (newEntityID_ != dbID) );
			newEntityID_ = dbID;

			entityData_.rewind();
			break;
		}
		default:
			MF_ASSERT( false );
			break;
	}

	this->nextStep();
}

void SelfTest::onDelEntityComplete( bool isOK )
{
	switch (stepNum_)
	{
		case 22:
		{
			bool delEntityByName = isOK;
			MF_ASSERT( delEntityByName );
			break;
		}
		case 25:
		{
			bool delEntityByID = isOK;
			MF_ASSERT( delEntityByID );
			break;
		}
		case 26:
		{
			bool delEntityByBadName = isOK;
			MF_ASSERT( !delEntityByBadName );
			break;
		}
		case 27:
		{
			bool delNonExistEntityByID = isOK;
			MF_ASSERT( !delNonExistEntityByID );
			break;
		}
		default:
			MF_ASSERT( false );
			break;
	}
	this->nextStep();
}

void Database::runSelfTest()
{
	SelfTest* selfTest = new SelfTest(*pDatabase_);
	selfTest->nextStep();
}
#endif	// DBMGR_SELFTEST

// -----------------------------------------------------------------------------
// Section: Served interfaces
// -----------------------------------------------------------------------------

#define DEFINE_SERVER_HERE
#include "db_interface.hpp"

// database.cpp
