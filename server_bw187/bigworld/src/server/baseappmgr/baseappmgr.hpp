/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef BASE_APP_MGR_HPP
#define BASE_APP_MGR_HPP

#include "baseappmgr_interface.hpp"

#include "common/doc_watcher.hpp"
#include "cstdmf/profile.hpp"
#include "network/channel.hpp"
#include "server/common.hpp"

#include <map>
#include <set>
#include <string>

class BaseApp;
class BackupBaseApp;
class TimeKeeper;

typedef Mercury::ChannelOwner CellAppMgr;
typedef Mercury::ChannelOwner DBMgr;

/**
 *	This singleton class is the global object that is used to manage proxies and
 *	bases.
 */
class BaseAppMgr : public Mercury::TimerExpiryHandler
{
public:
	BaseAppMgr( Mercury::Nub & nub );
	virtual ~BaseAppMgr();
	bool init(int argc, char* argv[]);

	// BaseApps register/unregister via the add/del calls.
	void add( const Mercury::Address & srcAddr, Mercury::ReplyID replyID,
		const BaseAppMgrInterface::addArgs & args );
	void addBackup( const Mercury::Address & srcAddr, Mercury::ReplyID replyID,
		const BaseAppMgrInterface::addBackupArgs & args );

	void recoverBaseApp( const Mercury::Address & addr,
			const Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data );
	void old_recoverBackupBaseApp( const Mercury::Address & addr,
			const Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data );

	void del( const BaseAppMgrInterface::delArgs & args,
		const Mercury::Address & addr );
	void informOfLoad( const BaseAppMgrInterface::informOfLoadArgs  & args,
		const Mercury::Address & addr );

	void shutDown( bool shutDownOthers );
	void shutDown( const BaseAppMgrInterface::shutDownArgs & args );
	void startup( const Mercury::Address & addr,
			const Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data );

	void checkStatus( const Mercury::Address & addr,
			const Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data );

	void controlledShutDown(
			const BaseAppMgrInterface::controlledShutDownArgs & args );

	void prepareForRestoreFromDB( const Mercury::Address & addr,
			const Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data );

	void setSharedData( BinaryIStream & data );
	void delSharedData( BinaryIStream & data );

	void useNewBackupHash( const Mercury::Address & addr,
			const Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data );

	BaseApp * findBaseApp( const Mercury::Address & addr );
	Mercury::ChannelOwner * findChannelOwner( const Mercury::Address & addr );

	Mercury::Nub & nub()						{ return nub_; }

	static Mercury::Channel & getChannel( const Mercury::Address & addr )
	{
		return BaseAppMgr::instance().nub().findOrCreateChannel( addr );
	}

	int numBaseApps() const					{ return baseApps_.size(); }
	int numBackupBaseApps() const			{ return backupBaseApps_.size(); }

	int numBases() const;
	int numProxies() const;

	float minBaseAppLoad() const;
	float avgBaseAppLoad() const;
	float maxBaseAppLoad() const;

	static BaseAppMgr & instance();
	static BaseAppMgr * pInstance();

	uint32 latestVersion()		{ return latestVersion_; }
	uint32 impendingVersion()	{ return impendingVersion_; }

	double gameTimeInSeconds() const { return double( time_ )/updateHertz_; }

	CellAppMgr & cellAppMgr()	{ return cellAppMgr_; }
	DBMgr & dbMgr()				{ return *dbMgr_.pChannelOwner(); }

	uint64 lastInformTime() const	{ return lastInformTime_; }

	// ---- Message Handlers ----
	void handleCellAppMgrBirth(
		const BaseAppMgrInterface::handleCellAppMgrBirthArgs & args );
	void handleBaseAppMgrBirth(
		const BaseAppMgrInterface::handleBaseAppMgrBirthArgs & args );

	void handleCellAppDeath( const Mercury::Address & addr,
			const Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data );
	void handleBaseAppDeath(
		const BaseAppMgrInterface::handleBaseAppDeathArgs & args );
	void handleBaseAppDeath( const Mercury::Address & addr );

	void createBaseEntity( const Mercury::Address & addr,
			const Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data );

	void registerBaseGlobally( const Mercury::Address & addr,
			const Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data );

	void deregisterBaseGlobally( const Mercury::Address & addr,
			const Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data );

	void runScript( const Mercury::Address & addr,
			const Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data );

	void resourceVersionControl(
		const BaseAppMgrInterface::resourceVersionControlArgs & args );

	void startAsyncShutDownStage( ShutDownStage stage );

	void controlledShutDownServer();

	bool shutDownServerOnBadState() const { return shutDownServerOnBadState_; }

	bool onBaseAppGone( const Mercury::Address & addr, bool shouldRestore );
	bool onBackupBaseAppGone( const Mercury::Address & addr );

	void removeControlledShutdownBaseApp( const Mercury::Address & addr );

private:
	enum TimeOutType
	{
		TIMEOUT_GAME_TICK
	};

	virtual int handleTimeout( int /*id*/, void * arg );

	void startTimer();
	void checkBackups(); // Old-style backup
	void adjustBackupLocations( const Mercury::Address & addr, bool isAdd );

	void checkForDeadBaseApps();

	void checkGlobalBases( const Mercury::Address & addr );

	void commonAdd( Mercury::ChannelOwner & channelOwner );

	void updateCreateBaseInfo();

	Mercury::Nub & nub_;

	template <class TYPE>
	class AutoPointer
	{
	public:
		AutoPointer() : pValue_( NULL ) {}
		~AutoPointer()	{ delete pValue_; }

		void set( TYPE * pValue )	{ pValue_ = pValue; }

		TYPE & operator *()						{ return *pValue_; }
		const TYPE & operator *() const			{ return *pValue_; }

		TYPE * operator->()						{ return pValue_; }
		const TYPE * operator->() const			{ return pValue_; }

		TYPE * get()				{ return pValue_; }
		const TYPE * get() const	{ return pValue_; }

	private:
		TYPE * pValue_;
	};

	typedef AutoPointer< BaseApp > BaseAppPtr;
	typedef AutoPointer< BackupBaseApp > BackupBaseAppPtr;

	CellAppMgr				cellAppMgr_;
	bool					cellAppMgrReady_;
	AnonymousChannelClient	dbMgr_;

	typedef std::map< Mercury::Address, BaseAppPtr > BaseApps;
	BaseApps baseApps_;

	typedef std::map< Mercury::Address, BackupBaseAppPtr >
		BackupBaseApps;
	BackupBaseApps backupBaseApps_;

	typedef std::map< std::string, std::string > SharedData;
	SharedData sharedBaseAppData_; // Authoritative copy
	SharedData sharedGlobalData_; // Copy from CellAppMgr

	BaseAppID 	lastBaseAppID_;	//  last id allocated for a BaseApp

	ProfileGroup				pro_;

	BaseApp * findBestBaseApp() const;
	BackupBaseApp * findBestBackup( const BaseApp & baseApp ) const;
	BaseAppID getNextID();

	void sendToBaseApps( const Mercury::InterfaceElement & ifElt,
		MemoryOStream & args, const BaseApp * pExclude = NULL,
		Mercury::ReplyMessageHandler * pHandler = NULL );

	void sendToBackupBaseApps( const Mercury::InterfaceElement & ifElt,
		MemoryOStream & args, const BackupBaseApp * pExclude = NULL,
		Mercury::ReplyMessageHandler * pHandler = NULL );

	void addWatchers();

	uint32				latestVersion_;
	uint32				impendingVersion_;
	uint32				coordinatingVersion_;
	Mercury::Address	updaterAddr_;
	int					updaterStep_;
	bool				allowNewBaseApps_;

	void sendUpdaterStepAck( int step );

	typedef std::map< std::string, EntityMailBoxRef > GlobalBases;
	GlobalBases globalBases_;

	TimeStamp		time_;
	TimeKeeper *	pTimeKeeper_;
	int				syncTimePeriod_;
	int				updateHertz_;

	float			baseAppOverloadLevel_;
	float			createBaseRatio_;
	int				updateCreateBaseInfoPeriod_;

	Mercury::Address bestBaseAppAddr_;

	bool			isRecovery_;
	bool			hasStarted_;
	bool			shouldShutDownOthers_;
	bool			shouldHardKillDeadBaseApps_;
	bool			onlyUseBackupOnSameMachine_;
	bool			useNewStyleBackup_;
	bool			shutDownServerOnBadState_;
	bool			shutDownServerOnBaseAppDeath_;

	TimeStamp		shutDownTime_;
	ShutDownStage	shutDownStage_;

	uint64			lastInformTime_;
	uint64			baseAppTimeoutPeriod_;

	static BaseAppMgr * pInstance_;

	// Whether we've got baseapps on multiple machines
	bool			hasMultipleBaseAppMachines_;

	std::set<int>	reservedBaseApps_;

	friend class CreateEntityIncomingHandler;
	friend class CreateBaseReplyHandler;
};

#ifdef CODE_INLINE
#include "baseappmgr.ipp"
#endif

#endif // BASE_APP_MGR_HPP
