/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef UPDATE_HANDLER_HPP
#define UPDATE_HANDLER_HPP

#include <string>
#include "math/vector4.hpp"
#include "cstdmf/md5.hpp"
#include "cstdmf/concurrency.hpp"

class ProgressDisplay;
class ServerConnection;
class IFileSystem;

struct ResCat;
struct VersionPtList;
struct VersionPtTracker;
struct PartialUpdates;
struct PersonalityCallback;

/**
 *	This class handles resource updates from the server.
 *	It may become a MainLoopTask in the future.
 */
class ResUpdateHandler
{
public:
	ResUpdateHandler();
	~ResUpdateHandler();

	void versionPointPass( const std::string & path,
		uint32 mustBeVersion = uint32(-1) );

	void init();
	void stocktake( ProgressDisplay * pDisplay );

	void serverConnection( ServerConnection * pServConn );

	void impendingVersion( int proximity );

	bool rootDownloadInProgress();
	void rootInstallAndRelaunch( bool alreadyInstalled );

	void tick();

	static ResUpdateHandler & instance();

private:
	ResUpdateHandler( const ResUpdateHandler& );
	ResUpdateHandler& operator=( const ResUpdateHandler& );

	void versionPointClearDyn( const std::string & point );
	bool versionPointDownload( uint32 targetVersion,
		const std::string & point, bool clearDyn, const std::string & subDir );
	void versionPointInstall( uint32 targetVersion,
		const std::string & point, const std::string & subDir );

	bool download( uint32 version, const std::string & point,
		const std::string & file, IFileSystem * pFS, const MD5::Digest & md5,
		float * progressVal = NULL );

	void saveUpdIndexXML( const std::string & updTopDir );

	void stocktakeOne( IFileSystem * pFS, bool trustCache, bool trustModDate,
		ResCat & resCat, VersionPtList & vpts );

	static void blockerDownloadThread( void * pThread );
	static void impendingDownloadThread( void * pThread );

	Vector4 * addPersonalityCallback(
		uint32 version, const std::string & point );

	ProgressDisplay		* pDisplay_;
	IFileSystem			* pRomFS_;
	IFileSystem			* pDynFS_;
	VersionPtTracker	* pVPTracker_;
	PartialUpdates		* pPartialUpdates_;

	ServerConnection	* pServConn_;
	uint32				latestVersion_;

	bool				enabled_;

	SimpleThread			* blockerDownloadingComplete_;
	SimpleThread			* impendingDownloadingComplete_;

	typedef std::vector<PersonalityCallback> PersonalityCallbacks;
	PersonalityCallbacks	personalityCallbacks_;
	SimpleMutex				personalityCallbacksLock_;
};


#endif // UPDATE_HANDLER_HPP
