/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

/*
 *	Class: CellApp - represents the Cell application.
 *
 *	This class is used as a singleton. This object represents the entire Cell
 *	application. It's main functionality is to handle the interface to this
 *	application and redirect the calls.
 */

#ifndef CELLAPP_HPP
#define CELLAPP_HPP

#include <Python.h>

#include "cstdmf/memory_stream.hpp"
#include "cstdmf/time_queue.hpp"

#include "entitydef/entity_description_map.hpp"
#include "network/mercury.hpp"
#include "pyscript/pickler.hpp"

#include "server/common.hpp"
#include "server/id_client.hpp"
#include "server/python_server.hpp"

#include "cellapp_interface.hpp"
#include "cellappmgr.hpp"
#include "profile.hpp"
#include "common/doc_watcher.hpp"
#include "updatable.hpp"

#include "cell_viewer_server.hpp"
#include "cellapp_death_listener.hpp"

class Cell;
class Entity;
class SharedData;
class Space;
class TimeKeeper;
struct CellAppInitData;

typedef Mercury::ChannelOwner DBMgr;

class BufferedGhostMessages;

/**
 *	This singleton class represents the entire application.
 */
class CellApp : public Mercury::TimerExpiryHandler
{
private:
	enum TimeOutType
	{
		TIMEOUT_GAME_TICK,
		TIMEOUT_TRIM_HISTORIES,
		TIMEOUT_LOADING_TICK
	};

public:
	/// @name Construction/Initialisation
	//@{
	CellApp();
	virtual ~CellApp();

	bool init( int argc, char *argv[] );
	bool finishInit( const CellAppInitData & initData );

	bool run( int argc, char *argv[] );

	void onGetFirstCell( bool isFromDB );

	//@}

	/// @name Message handlers
	//@{
	void addCell( const Mercury::Address & srcAddr,
			const Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data );
	void startup( const CellAppInterface::startupArgs & args );
	void setGameTime( const CellAppInterface::setGameTimeArgs & args );

	void handleCellAppMgrBirth(
		const CellAppInterface::handleCellAppMgrBirthArgs & args );

	void handleCellAppDeath(
		const CellAppInterface::handleCellAppDeathArgs & args );

	void handleBaseAppDeath( BinaryIStream & data );

	void shutDown( const CellAppInterface::shutDownArgs & args );
	void controlledShutDown(
			const CellAppInterface::controlledShutDownArgs & args );
	void requestShutDown();

	void createSpaceIfNecessary( BinaryIStream & data );

	void resourceVersionControl(
		const CellAppInterface::resourceVersionControlArgs & args );
	void resourceVersionTag(
		const CellAppInterface::resourceVersionTagArgs & args );

	void runScript( BinaryIStream & data );
	void setSharedData( BinaryIStream & data );
	void delSharedData( BinaryIStream & data );

	void setBaseApp( const CellAppInterface::setBaseAppArgs & args );

	void onloadTeleportedEntity( const Mercury::Address & srcAddr,
		const Mercury::UnpackedMessageHeader & header, BinaryIStream & data );

	void cellAppMgrInfo( const CellAppInterface::cellAppMgrInfoArgs & args );

	//@}

	/// @name Utility methods
	//@{
	Entity * findEntity( ObjectID id ) const;

	void entityKeys( PyObject * pList ) const;
	void entityValues( PyObject * pList ) const;
	void entityItems( PyObject * pList ) const;

	std::string pickle( PyObject * args );
	PyObject * unpickle( const std::string & str );
	PyObject * newClassInstance( PyObject * pyClass,
			PyObject * pDictionary );

	bool reloadScript( bool isFullReload );
	//@}

	/// @name Accessors
	//@{
	Cell *	findCell( SpaceID id ) const;
	Space *	findSpace( SpaceID id ) const;
	Space * findOrCreateSpace( SpaceID id );

	static Mercury::Channel & getChannel( const Mercury::Address & addr )
	{
		return CellApp::instance().nub_.findOrCreateChannel( addr );
	}

	Mercury::Nub & nub()						{ return nub_; }

	CellAppMgr & cellAppMgr()				{ return cellAppMgr_; }
	DBMgr & dbMgr()							{ return *dbMgr_.pChannelOwner(); }

	TimeQueue & timeQueue()						{ return timeQueue_; }
	const std::string & exeName()				{ return exeName_; }

	TimeStamp time() const						{ return time_; }
	double gameTimeInSeconds() const
									{ return double(time_)/updateHertz_; }
	float getLoad() const						{ return load_; }
	float spareTime() const						{ return spareTime_; }

	int updateHertz() const						{ return updateHertz_; }

	uint64 lastGameTickTime() const				{ return lastGameTickTime_; }

	typedef std::vector< Cell * > Cells;
	Cells & cells()								{ return cells_; }
	const Cells & cells() const					{ return cells_; }

	typedef std::map< SpaceID, Space * > Spaces;
	Spaces & spaces()							{ return spaces_; }
	const Spaces & spaces() const				{ return spaces_; }

	bool hasStarted() const						{ return gameTimerID_ != 0; }
	bool isShuttingDown() const					{ return shutDownTime_ != 0; }

	int numRealEntities() const;

	int numStartupRetries() const				{ return numStartupRetries_; }

	float noiseStandardRange() const			{ return noiseStandardRange_; }
	float noiseVerticalSpeed() const			{ return noiseVerticalSpeed_; }
	float noiseHorizontalSpeedSqr() const
											{ return noiseHorizontalSpeedSqr_; }

	float maxCellAppLoad() const				{ return maxCellAppLoad_; }

	int maxGhostsToDelete() const				{ return maxGhostsToDelete_; }
	int minGhostLifespanInTicks() const		{ return minGhostLifespanInTicks_; }

	/// Amount to scale back CPU usage: 256 = none, 0 = fully
	// int scaleBack() const						{ return scaleBack_; }
	float emergencyThrottle() const			{ return emergencyThrottle_; }

	const Mercury::InterfaceElement & entityMessage( int index ) const;

	Entity * pTeleportingEntity() const		{ return pTeleportingEntity_; }
	void pTeleportingEntity( Entity * pEntity )	{ pTeleportingEntity_ = pEntity; }

	const Mercury::Address & baseAppAddr() const	{ return baseAppAddr_; }

	int getExecRawDbCmdTimeout() const	{ return execRawDbCmdTimeout_; }

	uint32 latestVersion() const		{ return latestVersion_; }
	uint32 previousVersion() const		{ return previousVersion_; }

	bool sendResourceVersionTag() const	{ return sendResourceVersionTag_; }
	bool versForCallIsOld() const		{ return versForCallIsOld_; }

	bool shouldLoadAllChunks() const	{ return shouldLoadAllChunks_; }
	bool shouldUnloadChunks() const		{ return shouldUnloadChunks_; }

	bool shouldResolveMailBoxes() const	{ return shouldResolveMailBoxes_; }

	int entitySpamSize() const			{ return entitySpamSize_; }

	bool extrapolateLoadFromPendingRealTransfers() const
					{ return extrapolateLoadFromPendingRealTransfers_; }

	IDClient & idClient()				{ return idClient_; }

	static CellApp & instance();
	static CellApp * pInstance();
	//@}

	/// @name Update methods
	//@{
	bool registerForUpdate( Updatable * pObject, int level = 0 );
	bool deregisterForUpdate( Updatable * pObject );

	bool nextTickPending() const;	// are we running out of time?
	//@}

	/// @name Misc
	//@{
	void killCell( Cell * pCell );

	static CellApp * findMessageHandler( BinaryIStream & /*data*/ )
													{ return pInstance_; }
	void detectDeadCellApps(
		const std::vector< Mercury::Address > & addrs );

	static void reloadResources( void * arg );
	//@}

	PyObjectPtr getPersonalityModule() const;

	BufferedGhostMessages & bufferedGhostMessages()
		{ return *pBufferedGhostMessages_; }

	bool shouldOffload() const { return shouldOffload_; }
	void shouldOffload( bool b ) { shouldOffload_ = b; }

	int id() const				{ return id_; }

private:
	// Methods
	void initExtensions();
	bool initScript();

	void addWatchers();

	void callUpdates();
	void adjustUpdatables();

	void checkPython();

	void bindNewlyLoadedChunks();

	void sendAckForStep( int step );
	void reloadResources();
	void migrateToNextVersion();

	int secondsToTicks( float seconds, int lowerBound );

	void startGameTime();

	/// @name Overrides
	//@{
	int handleTimeout( int id, void * arg );
	//@}
	int handleGameTickTimeSlice();
	int handleTrimHistoriesTimeSlice();

	// Data
	Cells			cells_;
	Spaces			spaces_;

	Mercury::Nub	nub_;
	CellAppMgr		cellAppMgr_;
	AnonymousChannelClient dbMgr_;

	TimeStamp		time_;
	TimeStamp		shutDownTime_;
	TimeQueue		timeQueue_;
	TimeKeeper * 	pTimeKeeper_;

	Pickler * pPickler_;

	Entity * pTeleportingEntity_;

	typedef std::vector< Updatable * > UpdatableObjects;

	UpdatableObjects updatableObjects_;
	std::vector< int > updatablesLevelSize_;
	bool		inUpdate_;
	int			deletedUpdates_;

	// Used for throttling back
	float		emergencyThrottle_;
	float		spareTime_;

	// Throttling configuration
	float					throttleSmoothingBias_;
	float					throttleBackTrigger_;
	float					throttleForwardTrigger_;
	float					throttleForwardStep_;
	float					minThrottle_;
	float					throttleEstimatedEffect_;

	bool					extrapolateLoadFromPendingRealTransfers_;

	uint64					lastGameTickTime_;

	int						updateHertz_;

	PythonServer *			pPythonServer_;

	SharedData *			pCellAppData_;
	SharedData *			pGlobalData_;

	bool					isShuttingDown_;
	bool					shouldRequestShutDown_;
	std::string				exeName_;
	IDClient				idClient_;

	Mercury::Address		baseAppAddr_;

	int						backupIndex_;
	int						backupPeriod_;
	int						checkOffloadsPeriod_;

	int						gameTimerID_;
	uint64					reservedTickTime_;

	int						execRawDbCmdTimeout_;

	uint32					latestVersion_;
	uint32					impendingVersion_;
	uint32					previousVersion_;
	Mercury::Address		updaterAddr_;
	bool					sendResourceVersionTag_;
	bool					versForCallIsOld_;

	CellViewerServer *		pViewerServer_;
	CellAppID				id_;

	// TODO: We could put all "global" configuration options in their own object
	// or namespace.
	float					demoNumEntitiesPerCell_;
	float					load_;
	float					loadSmoothingBias_;

	bool					demoLoadBalancing_;
	bool					shouldLoadAllChunks_;
	bool					shouldUnloadChunks_;
	bool					shouldOffload_;
	bool					fastShutdown_;
	bool					isFromMachined_;

	bool					shouldResolveMailBoxes_;

	int						entitySpamSize_;

	int						maxGhostsToDelete_;
	int						minGhostLifespanInTicks_;

	float					maxCPUOffload_;
	int						minEntityOffload_;

	int						numStartupRetries_;

	float					noiseStandardRange_;
	float					noiseVerticalSpeed_;
	float					noiseHorizontalSpeedSqr_;

	float					maxCellAppLoad_;

 	BufferedGhostMessages * pBufferedGhostMessages_;

	static CellApp *		pInstance_;
	friend class CellAppResourceReloader;
};


#ifdef CODE_INLINE
#include "cellapp.ipp"
#endif

#endif // CELLAPP_HPP
