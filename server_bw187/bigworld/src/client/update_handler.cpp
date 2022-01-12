/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "pch.hpp"
#include "update_handler.hpp"
#include "connection_control.hpp"
#include "app_config.hpp"
#include "personality.hpp"
#include "app.hpp"

#include "common/servconn.hpp"

#include "resmgr/bwresource.hpp"
#include "resmgr/bundiff.hpp"
#include "resmgr/xml_section.hpp"
#include "resmgr/multi_file_system.hpp"
#include "romp/progress.hpp"

#include "pyscript/script_math.hpp"

#include <set>
#include <fcntl.h>
#include "shlwapi.h"	// for PathMatchSpec

DECLARE_DEBUG_COMPONENT2( "Server", 0 )

// set whether or not ROMFS is expected to be in CVS, rather than a
// production package or downloading from the server. It is more
// efficient when running the updater to set this to zero.
#define ROMFS_IS_IN_CVS 1

// -----------------------------------------------------------------------------
// Section: ResUpdateHandler
// -----------------------------------------------------------------------------

extern THREADLOCAL( bool ) g_isMainThread;

/// Global version point function pointer
extern void (*g_versionPointPass)( const std::string & path );

/**
 *	Global version point access function for us
 */
static void versionPointPass( const std::string & path )
{
	ResUpdateHandler::instance().versionPointPass( path );
}


/// Template class for version point maps
template <class C> struct VersionPtMapType : public std::map<std::string,C>
{
	iterator get( const std::string & filename, bool mustBeInDyn = true )
	{
		bool versionPtTop = true;
		for (uint32 slpos = filename.find_last_of( '/' );
			slpos < filename.length();	// assumes no empty name dirs at res top
			slpos = filename.find_last_of( '/', slpos-1 ))
		{
			iterator found = this->find( filename.substr( 0, slpos+1 ) );
			if (found != this->end() && (!mustBeInDyn || found->second.inDyn))
			{
				if (!versionPtTop)
					return found;
				// version point meta files are associated with the version pt
				// enclosing the one they define
				std::string nameEnd = filename.substr( slpos+1 );
				if (nameEnd != "version.point" && nameEnd != "space.settings")
					return found;
			}
			versionPtTop = false;
		}
		return this->find( std::string() );
	}
};

/// Version point tracker info
struct VersionPtInfo
{
	VersionPtInfo() : version( uint32(-1) ), inDyn(0), inRom(0) { }

	uint32		version;
	int			shallowPass:1;
	int			inDyn:1;
	int			inRom:1;
	MD5::Digest	romMD5;
	MD5::Digest	dynMD5;

	std::string	updDirPath;
	bool		updInProgress;
	std::vector<bool*>	waiters;
};
/// Version point tracking map
struct VersionPtTracker : public VersionPtMapType<VersionPtInfo> { };

/// Partial update record
struct PartialUpdate
{
	std::string point;
	bool		clearDyn;
};
/// Map of partial updates
struct PartialUpdates : public std::map<std::string,PartialUpdate> { };

// quiet file reader
BinaryPtr readFileQuiet( IFileSystem * pFS, const std::string & name )
{
	if (pFS->getFileType( name ) == IFileSystem::FT_FILE)
		return pFS->readFile( name );
	return NULL;
}

// glob matching function
static bool matchesIgnorePattern( const std::string & needle,
	const std::vector<std::string> & haystacks )
{
	if (needle.empty()) return true;
	// linux version checks for '.' files but that could be exploited here

	for (uint i = 0; i < haystacks.size(); ++i)
	{
		if (PathMatchSpec( needle.c_str(), haystacks[i].c_str() ))
			return true;
	}

	return false;
}


/// Arguments to background downloading functions
struct BackgroundDownloadArgs
{
	SimpleThread	* pThread;
	uint32			version;
	std::string		point;
	bool			clearDyn;
};




/**
 *	Constructor.
 */
ResUpdateHandler::ResUpdateHandler() :
	pDisplay_( NULL ),
	pRomFS_( NULL ),
	pDynFS_( NULL ),
	pVPTracker_( NULL ),
	pPartialUpdates_( new PartialUpdates ),
	pServConn_( NULL ),
	latestVersion_( uint32(-1) ),
	enabled_( AppConfig::instance().pRoot()->readBool("updaterEnabled",false) ),
	blockerDownloadingComplete_( NULL ),
	impendingDownloadingComplete_( NULL )
{
}

/**
 *	Destructor.
 */
ResUpdateHandler::~ResUpdateHandler()
{
	if (g_versionPointPass == &::versionPointPass)
		g_versionPointPass = NULL;

	delete pVPTracker_;

	delete pDynFS_;
	delete pRomFS_;
}

extern THREADLOCAL( bool ) g_doneLoading;	// in WinFileSystem.cpp

/**
 *	Version point access method called when a version point is passed.
 *
 *	If we do not have the required version of the files under the enclosing
 *	version point then this method blocks until we do.
 */
void ResUpdateHandler::versionPointPass( const std::string & path,
	uint32 mustBeVersion )
{
	if (!pServConn_) return;
	if (mustBeVersion == uint32(-1)) mustBeVersion = latestVersion_;

	// find the relevant version point, and return if it is OK
	const std::string pathSlashed = path.empty() ? path : path+"/";
	VersionPtTracker::iterator found = pVPTracker_->find( pathSlashed );
	if (found == pVPTracker_->end())
	{
		if (pPartialUpdates_->empty()) return;

		// see if this goes deeper into a partially performed update
		PartialUpdates::iterator puit = pPartialUpdates_->find( pathSlashed );
		if (puit == pPartialUpdates_->end()) return;

		bool oldGDL = g_doneLoading;
		g_doneLoading = false;

		PartialUpdate & partUpd = puit->second;

		TRACE_MSG( "Traversing deeper into a shallow point: %s at %s\n",
			partUpd.point.c_str(), pathSlashed.c_str() );

		std::string subDir = pathSlashed.substr( partUpd.point.length() );
		std::string pointCopy = partUpd.point; // partUpd erased by fn below

		bool ok = this->versionPointDownload(
			latestVersion_, pointCopy, partUpd.clearDyn, subDir );
		if (ok)
			this->versionPointInstall( latestVersion_, pointCopy, subDir );

		g_doneLoading = oldGDL;
		return;
	}
	if (found->second.version != uint32(-1) &&
		found->second.version >= mustBeVersion) return;

	// ask the server for the version point version if we don't yet have it
	bool clearDyn = false;
	if (found->second.version == uint32(-1))
	{
		uint32	version;
		uint8	index;
		pServConn_->identifyVersionPoint( found->first,
			found->second.dynMD5, found->second.romMD5,
			version, index );
		found->second.version = version;

		if (index == 0)
			(void)0;
		else if (index == 1)
			clearDyn = true;
		else if (index == uint8(-1))
			found->second.version = uint32(-1);
	}

	// prevent complaints about accessing files in main thread
	bool oldGDL = g_doneLoading;
	g_doneLoading = false;

	// check if the version point version is OK now that we know it
	if (found->second.version != uint32(-1) &&
		found->second.version >= mustBeVersion)
	{
		if (clearDyn)
		{
			this->versionPointClearDyn( found->first );

			// relaunch if this happened in the main thread,
			// and resources have already been loaded underneath us
			if (g_isMainThread && found->first.empty())
			{
				// give the personality script a heads up
				Script::call( PyObject_GetAttrString(
					Personality::instance(), "onResUpdateAutoRelaunch" ),
				PyTuple_New(0), "onResUpdateRelaunch: ", /*okIfFnNULL:*/true );

				this->rootInstallAndRelaunch( /*alreadyInstalled:*/true );
			}
		}
	}
	// ok we have to go and update this version point then... yeehah!
	else
	{
		// if this is the main thread then launch a blocker thread to
		// download the update and disable the versionPointPass mechanism
		if (g_isMainThread && found->first.empty())
		{
			BackgroundDownloadArgs * pBDA = new BackgroundDownloadArgs;
			void * backgroundDownloader = new char[ sizeof(SimpleThread) ];
			pBDA->pThread = (SimpleThread*)backgroundDownloader;
			pBDA->version = latestVersion_;
			pBDA->point = found->first;
			pBDA->clearDyn = clearDyn;
			new (backgroundDownloader) SimpleThread(
				&ResUpdateHandler::blockerDownloadThread, pBDA );

			// disable version point checking ... we'll be relaunched
			// when the update started above has completed
			g_versionPointPass = NULL;

			// wait for it to add its entry to the callbacks list
			do { Sleep( 50 ); } while (!this->rootDownloadInProgress());
		}
		// otherwise it is fine to download it in this thread
		// (or we have to do it right now, because the main thread is
		// trying to access a non-root version point that is out of date
		else
		{
			bool ok = this->versionPointDownload(
				latestVersion_, found->first, clearDyn, std::string() );
			if (ok) this->versionPointInstall(
				latestVersion_, found->first, std::string() );
		}
	}

	// restore the ResMgr's g_doneLoading variable
	g_doneLoading = oldGDL;
}

#if 0
/// This struct contains info about the op in progress
struct CurOpInfo
{
	CurOpInfo( CurOpInfo * & grp ) : rp( grp )
	{
		rp = this;
		progress = 0;
		element = NULL;
	};
	~CurOpInfo() { rp = NULL; }

	float progress;
	const std::string * element;
	CurOpInfo * & rp;
};
static THREADLOCAL( CurOpInfo* ) s_pCurOp = NULL;
#endif



/**
 *	Clear all downloaded data from the given version point, since we are
 *	happy with the rom match. In the future these may be saved somewhere
 *	instead of being deleted, but it probably wouldn't be worthwhile.
 */
void ResUpdateHandler::versionPointClearDyn( const std::string & point )
{
	// TODO: Don't clear child version points! We can do that later if need
	// be when/if we encounter them.

	std::string dirname;
	if (!point.empty())
		dirname = point.substr( 0, point.length()-1 );
	if (!pDynFS_->eraseFileOrDirectory( dirname ))
	{
		ERROR_MSG( "ResUpdateHandler::versionPointClearDyn: "
			"Unable to clear version point %s, "
			"continuing with existing resources\n",
			point.c_str() );
	}
}


// Elements of the manifest processing stack
struct ManifestStackElt
{
	std::string		prefix;
	DataSectionPtr	pSect;
	float			progressBeg;
	float			progressEnd;
};

struct ROMMaskStackElt
{
	std::string		prefix;
	std::vector<std::string>	ignorePatterns;
};

/**
 *	Download the given version point from the stored version number to
 *	the latest. If clearDyn is true, then the stored version number is
 *	that of the rom tree not the dyn tree. The dyn tree may however
 *	still be used to avoid redownloading files that have already
 *	been retrieved, based on when their MD5s match.
 *
 *	Note that the stored version number may well be -1, in which case
 *	the whole version history must be traversed, starting from the
 *	assumption of an empty version point. Files that happen to be in the
 *	dyn and rom trees may however still be used if they exist and match.
 *
 *	Note also that we always go to the latest version, even when an
 *	earlier version would suffice. This is for sanity and to avoid
 *	downloading superceeded when we'll probably have to get the later
 *	ones at some time anyway.
 */
bool ResUpdateHandler::versionPointDownload( uint32 targetVersion,
	const std::string & point, bool clearDyn, const std::string & subDir )
{
//	CurOpInfo curOpInfo( s_pCurOp );
	Vector4 & progressInfo =
		*this->addPersonalityCallback( targetVersion, point );
	progressInfo = Vector4::zero();

	// TODO: some locks for pVPTracker_!
	VersionPtTracker::iterator found = pVPTracker_->find( point );
	MF_ASSERT( found != pVPTracker_->end() );
	VersionPtInfo & vpi = found->second;

	// make sure someone else isn't already downloading it
	if (!vpi.updDirPath.empty() && vpi.updInProgress)
	{
		TRACE_MSG( "ResUpdateHandler::versionPointDownload: "
			"Waiting for another thread to complete download of point %s\n",
			point.c_str() );

		bool done = false;
		vpi.waiters.push_back( &done );

		while (!done) Sleep( 100 );
		return true;
	}

	TRACE_MSG( "ResUpdateHandler::versionPointDownload: "
		"Downloading point %s (%s%s) from %d to %d\n",
		point.c_str(), (!vpi.shallowPass) ? "deep" : "shallow: ",
		(!vpi.shallowPass) ? "" : subDir.c_str(),
		int(vpi.version), int(targetVersion) );

	// make a directory for this update
	if (vpi.updDirPath.empty())
	{
		std::string dynParent = BWResource::getPath( 0 );
		dynParent = dynParent.substr(
			0, dynParent.find_last_of('/',dynParent.length()-1) + 1 );
		IFileSystem * pDynParent = NativeFileSystem::create( dynParent );

		std::string updDirName;
		for (int i = 0; i < 4096; i++)
		{
			char buf[32];
			sprintf( buf, "upd/%d", i );
			updDirName = buf;
			if (pDynParent->getFileType( updDirName ) == IFileSystem::FT_NOT_FOUND)
				break;
		}

		MF_ASSERT( !updDirName.empty() );
		pDynParent->makeDirectory( updDirName );
		delete pDynParent; pDynParent = NULL;

		vpi.updDirPath = dynParent + updDirName + "/";
		this->saveUpdIndexXML( dynParent + "upd/" );
	}
	vpi.updInProgress = true;

	IFileSystem * pUpdFS = NativeFileSystem::create( vpi.updDirPath );


	bool err = false;

	// TODO: Put the stuff below into servconn and combine with
	// sendRequestsAndBlock
	uint64	safetySendEvery = stampsPerSecond();
	uint64	safetySendNext = g_isMainThread ?
		timestamp() + safetySendEvery : ~uint64(0);

	std::set<std::string>	seenFiles;

	// TODO: If vpi.version is -1 then don't loop but just get the latest
	// version in full for each file (whose md5 does not match something we
	// already have), indicated by passing -1 as the version
	// to the download method.

	// TODO: Do this loop backwards so we don't download superceeded files...
	// or better yet, get the server to make a custom manifest for us.
	// This is only tricky because we need the previous version to apply patches
	for (uint32 v = vpi.version+1; v != targetVersion+1 && !err; v++)
	{
		DEBUG_MSG( "ResUpdateHandler::versionPointDownload: "
			"Updating version point '%s' to version %d from previous\n",
			point.c_str(), int(v) );
		progressInfo.x = float(v);
		progressInfo.y = 0.f;
		progressInfo.z = 0.f;

		// ask the server for the MD5 of the manifest for this version
		MD5::Digest manifestDigest;
		MD5::Digest completeDigest;
		err = !pServConn_->summariseVersionPoint( v, point,
			manifestDigest, completeDigest );
		if (err) break;

		// download and check the manifest file
		if (!this->download( v, point, "manifest.xml", pUpdFS, manifestDigest,
			&progressInfo.z ))
		{
			err = true;
			break;
		}

		// load in the manifest file
		DataSectionPtr pManifest = XMLSection::createFromBinary(
			"manifest.xml", pUpdFS->readFile( point + "manifest.xml" ) );

		// download and check every file in it
		std::vector<ManifestStackElt> stack;
		ManifestStackElt rmse;
		rmse.pSect = pManifest;
		rmse.progressBeg = 0.f;
		rmse.progressEnd = 1.f;
		stack.push_back( rmse );
		while (!stack.empty() && !err)
		{
			ManifestStackElt mse = stack.back();
			stack.pop_back();

			// skip files and whole directories if this is a shallow pass
			bool skipFiles = false;
			if (vpi.shallowPass)
			{
				if (mse.prefix.length() > subDir.length())
				{
					if (mse.prefix.substr( 0, subDir.length() ) == subDir)
					{
						std::string dirPath = point + mse.prefix;
						pUpdFS->makeDirectory( dirPath );
						PartialUpdate & partUpd = (*pPartialUpdates_)[dirPath];
						partUpd.point = point;
						partUpd.clearDyn = clearDyn;
					}
					continue;
				}
				if (mse.prefix != subDir.substr( 0, mse.prefix.length() ))
					continue;
				skipFiles = mse.prefix.length() < subDir.length();
			}

			// see if we are creating or deleting a version point
			DataSectionPtr pVP = mse.pSect->findChild( "version.point" );
			DataSectionPtr pSS = mse.pSect->findChild( "space.settings" );
			if (pVP || pSS)
			{
				// TODO: If a version point is deleted it must first be updated
				// to the latest version so patches in the enclosing version
				// point (i.e. this one, in this directory) apply correctly.
				// bool vpInOld = ..., vpInNew = ...;
				// bool ssInOld = ..., ssInNew = ...;
				// if (vpInOld && !vpInNew)
				//  if (!ssInNew) mustUpdate
				// elif (vpInOld && vpInNew)
				//  if (ssInNew && vpChanged && downloaded vp says false)
				//   mustUpdate
				//  ... etc
				// bleh ... 50 000 combinations ... too late for this tonight
			}

			// go through all our children in the normal way
			float progressLen = float( mse.pSect->countChildren() );
			for (int i = mse.pSect->countChildren()-1; i >= 0 && !err; i--)
			{
				if (timestamp() > safetySendNext)
				{	// keep the connection alive
					if (pServConn_ != NULL)
					{
						// TODO: very dodgy and should be fixed!
						pServConn_->processInput();
						pServConn_->send();
					}
					safetySendNext = timestamp() + safetySendEvery;
				}

				DataSectionPtr pChild = mse.pSect->openChild( i );
				std::string value = pChild->asString();
				std::string relName = mse.prefix + pChild->sectionName();
				if (value.empty())
				{	// a directory
					ManifestStackElt dmse;
					dmse.prefix = relName + "/";
					dmse.pSect = pChild;
					dmse.progressEnd = mse.progressEnd;
					dmse.progressBeg = mse.progressEnd -= 1.f/progressLen;
					stack.push_back( dmse );
				}
				else if (!skipFiles)
				{	// a file
					MD5::Digest md5;
					md5.unquote( value );

					if (!this->download( v, point, relName, pUpdFS, md5,
						&progressInfo.z ))
					{
						err = true;
						break;
					}

					// record seeing this file if we care
					if (vpi.version == uint32(-1) || clearDyn)
						seenFiles.insert( point + relName );

					progressInfo.y = mse.progressBeg += 1.f/progressLen;
					progressInfo.z = 0.f;
				}
			}
		}

		// TODO: Check that completeDigest matches
		// (means keeping res cat for this version point around)
	}

	// consider version point indicators to have been seen
	if (!point.empty())
	{
		seenFiles.insert( point + "space.settings" );
		seenFiles.insert( point + "version.point" );
	}

	// see if we have any extra files that aren't supposed to be in there,
	// and create an empty masking file in pUpdFS for each of them if we do
	// TODO: fix this for unwanted version point indicators too
	if (!err && (vpi.version == uint32(-1) || clearDyn))
	{
		MD5::Digest emptyFile; emptyFile.clear();

		// there can be files that aren't supposed to be there either if
		// we started updating from version -1 (i.e. started with server
		// assuming that nothing is there) or if we started updating from
		// the version of the files in the rom fs (i.e. started with server
		// assuming that nothing is in dyn fs)

		// see if there's anything undesirable in the rom fs
		std::vector<ROMMaskStackElt> stack1;
		ROMMaskStackElt rrmse;
		rrmse.prefix = point;
		rrmse.ignorePatterns.push_back( "*.fxo" );	// always locally generated
		if (vpi.version == uint32(-1))	// only do the rom fs if we had no
			stack1.push_back( rrmse );	// match at all; leave it intact
		else							// if it was identified and dyn wasn't,
			{ MF_ASSERT( clearDyn ); }	// in which case clearDyn should be set
		if (!stack1.empty() && vpi.shallowPass)
			stack1.back().prefix += subDir;	// override with subDir if shallow
		while (!stack1.empty())	// TODO: fix ignore accum for points & subDirs!
		{
			ROMMaskStackElt rmse = stack1.back();
			stack1.pop_back();
			std::string & path = rmse.prefix;

			if (ROMFS_IS_IN_CVS)	// ignore.opt exists only when in CVS
			{
				BinaryPtr pOptsFile = readFileQuiet( pRomFS_, path + "ignore.opt" );
				if (!pOptsFile) pOptsFile = readFileQuiet( pRomFS_, path + "xbsync.opt" );
				if (pOptsFile)
				{
					DataSectionPtr pOptsSec = DataSection::createAppropriateSection(
						"ignore.opt", pOptsFile );
					if (!pOptsSec->read( "copy", true )) continue;

					pOptsSec->readSeq( "ignore", rmse.ignorePatterns );
				}
			}

			IFileSystem::Directory dir = pRomFS_->readDirectory( path );
			for (uint i = 0; i < dir.size(); i++)
			{
				if (matchesIgnorePattern( dir[i], rmse.ignorePatterns ) ||
					(path.length() == point.length() &&
						dir[i] == "catalogue.md5"))
				{
					continue;
				}

				std::string resName = path + dir[i];
				if (pRomFS_->getFileType( resName ) ==
					IFileSystem::FT_DIRECTORY)
				{
					resName += "/";
					if (pVPTracker_->find( resName ) != pVPTracker_->end())
						continue;
					ROMMaskStackElt nrmse;
					nrmse.prefix = resName;
					nrmse.ignorePatterns = rmse.ignorePatterns;
					stack1.push_back( nrmse );
				}
				else
				{
					if (seenFiles.find( resName ) == seenFiles.end())
					{
						IFileSystem::FileInfo dynfi;
						if (pDynFS_->getFileType( resName, &dynfi ) ==
							IFileSystem::FT_FILE && dynfi.size == 0) continue;

						// 'download' this empty file
						std::string relName = resName.substr( point.length() );
						this->download( -1, point, relName, pUpdFS, emptyFile );
						DEBUG_MSG( "ResUpdateHandler::versionPointDownload: "
							"Masking %s%s from rom since unseen\n",
							point.c_str(), relName.c_str() );
					}
				}
			}

			if (vpi.shallowPass) break;	// only one dir thanks
		}

		// see if there's anything undesirable in the dyn fs
		std::vector<std::string> stack2;
		stack2.push_back( point );
		if (vpi.shallowPass) stack2.back() += subDir;
		while (!stack2.empty())
		{
			std::string path = stack2.back();
			stack2.pop_back();

			IFileSystem::Directory dir = pDynFS_->readDirectory( path );
			for (uint i = 0; i < dir.size(); i++)
			{
				std::string resName = path + dir[i];
				IFileSystem::FileInfo dynfi;
				if (pDynFS_->getFileType( resName, &dynfi ) ==
					IFileSystem::FT_DIRECTORY)
				{
					resName += "/";
					if (pVPTracker_->find( resName ) != pVPTracker_->end())
						continue;
					stack2.push_back( resName );
				}
				else
				{
					if (seenFiles.find( resName ) == seenFiles.end())
					{
						if (resName == "catalogue.md5") continue;
						if (dynfi.size == 0) continue;

						// 'download' this empty file
						std::string relName = resName.substr( point.length() );
						this->download( -1, point, relName, pUpdFS, emptyFile );
						DEBUG_MSG( "ResUpdateHandler::versionPointDownload: "
							"Deleting %s%s from dyn since unseen\n",
							point.c_str(), relName.c_str() );
					}
				}
			}

			if (vpi.shallowPass) break;
		}
		// TODO: delete empty directories in case they turn into files.
		// We can probably safely assume this won't happen though
		// (since directories don't have extensions but files do)
	}

	// see if we had any errors doing all that
	if (err)
	{
		delete pUpdFS;
		ERROR_MSG( "ResUpdateHandler::versionPointDownload: "
			"Failed to update %s to version %d, "
			"continuing with existing resources\n",
			point.c_str(), int(targetVersion) );
		vpi.version = uint32(-1);
		vpi.updDirPath = std::string();
		return false;
	}

	DEBUG_MSG( "ResUpdateHandler::versionPointDownload: "
		"Download complete of %s (%s%s) to version %d\n",
		point.c_str(), (!vpi.shallowPass) ? "deep" : "shallow: ",
		(!vpi.shallowPass) ? "" : subDir.c_str(), int(targetVersion) );

	progressInfo.x = -1.f;
	progressInfo.y = 0.f;
	progressInfo.z = 0.f;

	delete pUpdFS;
	return true;
}


/**
 *	Install the given downloading version point into our dynamic filesystem.
 *	@see versionPointUpdate
 */
void ResUpdateHandler::versionPointInstall( uint32 targetVersion,
	const std::string & point, const std::string & subDir )
{
	DEBUG_MSG( "ResUpdateHandler::versionPointInstall: "
		"Installing %s to version %lu\n", point.c_str(), targetVersion );

	VersionPtTracker::iterator found = pVPTracker_->find( point );
	MF_ASSERT( found != pVPTracker_->end() );
	VersionPtInfo & vpi = found->second;

	MF_ASSERT( !vpi.updDirPath.empty() );
	IFileSystem * pUpdFS = NativeFileSystem::create( vpi.updDirPath );

	std::string startPath = point + ((!vpi.shallowPass) ? std::string():subDir);
	for (uint je = startPath.find_first_of( '/' );
		je < startPath.length();
		je = startPath.find_first_of( '/', je+1 ))
			pDynFS_->makeDirectory( startPath.substr( 0, je ) );

	// copy pUpdFS over pDynFS and delete pUpdFS
	std::string manifestLocn = point + "manifest.xml";
	char * rbuf = new char[128*1024];
	std::vector<std::string> stack;
	stack.push_back( startPath );
	while (!stack.empty())
	{
		std::string path = stack.back();
		stack.pop_back();

		IFileSystem::Directory dir = pUpdFS->readDirectory( path );
		for (uint i = 0; i < dir.size(); i++)
		{
			std::string file = path + dir[i];
			if (file == manifestLocn) continue;

			IFileSystem::FileInfo fi;
			if (pUpdFS->getFileType( file, &fi ) == IFileSystem::FT_DIRECTORY)
			{		// it's a directory; create it and recurse
				pDynFS_->makeDirectory( file );
				stack.push_back( file + "/" );
			}
			else if (fi.size == 0 &&
				pRomFS_->getFileType( file ) == IFileSystem::FT_NOT_FOUND)
			{		// it's an empty file that we can just delete
				pDynFS_->eraseFileOrDirectory( file );
				pUpdFS->eraseFileOrDirectory( file );
			}
			else
			{		// it's a file we have to copy over
				FILE * pSrc = pUpdFS->posixFileOpen( file, "rb" );
				FILE * pDst = pDynFS_->posixFileOpen( file, "wb" );

				// TODO: investigate whether using moveFileOrDirectory is safe

				int ferr = 1;
				uint32 full = uint32(fi.size);
				uint32 more;
				for (uint32 sofa = 0; sofa < full; sofa += more)
				{
					more = std::min( full-sofa, 128*1024UL );
					ferr = fread( rbuf, 1, more, pSrc );
					if (ferr == 0) break;
					ferr = fwrite( rbuf, 1, more, pDst );
					if (ferr == 0) break;
				}

				fclose( pDst );
				fclose( pSrc );

				// TODO: Handle this condition more gracefully
				MF_ASSERT( ferr != 0 );

				pUpdFS->eraseFileOrDirectory( file );
			}
		}

		if (vpi.shallowPass) break;
	}
	delete [] rbuf;

	bool updFinished = true;
	if (vpi.shallowPass)
	{
		pPartialUpdates_->erase( point + subDir );
		PartialUpdates::iterator it = pPartialUpdates_->lower_bound( point );
		if (it != pPartialUpdates_->end() &&
			it->first.substr( 0, point.length() ) == point)
				updFinished = false;	// more partial stuff remains
	}

	std::string updTopDir = vpi.updDirPath.substr( 0,
		vpi.updDirPath.find_last_of( '/', vpi.updDirPath.length()-2 )+1 );

	if (updFinished)
	{
		// delete anything left in pUpdFS (the manifest and the directory
		// structure) the manifest is intentionally left there until now
		// so we can figure out where we're up to if we have to restart
		// this update (another TODO)
		pUpdFS->eraseFileOrDirectory( std::string() );
		delete pUpdFS;

		vpi.updDirPath = std::string();
		vpi.version = targetVersion;
	}
	else
	{
		vpi.updInProgress = false;	
	}
	this->saveUpdIndexXML( updTopDir );

	std::vector<bool*> waitersCopy;
	waitersCopy.swap( vpi.waiters );
	for (uint i = 0; i < waitersCopy.size(); ++i)
		*waitersCopy[i] = true;
	//std::for_each( waitersCopy.begin(), waitersCopy.end(),
	//	void f( bool * c ) { *c = true; } );
	// local fn defns are illegal in C++ :(

	INFO_MSG( "ResUpdateHandler::versionPointInstall: "
		"Successfully updated %s to version %d (%s)!\n",
		point.c_str(), int(latestVersion_),
		updFinished ? "complete" : "partial" );

	// delete the catalogue in the dyn FS
	// TODO: update it using the manifest instead!
	pDynFS_->eraseFileOrDirectory( "catalogue.md5" );
}




static THREADLOCAL( char* ) s_scratchBuffer = NULL;
struct ScratchBufferFiller
{
	static const uint32 SIZE;
	ScratchBufferFiller() { s_scratchBuffer = new char[SIZE]; }
	~ScratchBufferFiller() { delete [] s_scratchBuffer; }
};
const uint32 ScratchBufferFiller::SIZE = 128*1024;


// helper method to set text mode on a file if it is a text file from CVS
// returns true if the file was set to text mode
bool setTextModeIfTextFile( IFileSystem * pFS, const std::string & name,
	FILE * pFile )
{
	bool ret = false;

	// see if it is a text file
	uint npos = name.find_last_of('/');
	std::string nameDir = (npos < name.size()) ?
		name.substr( 0, npos+1 ) : std::string();
	if (pFS->getFileType( nameDir + "CVS" ) == IFileSystem::FT_DIRECTORY)
	{
		// don't look in CVS dir in case this file is newly added...
		// ... then again, maybe that would be OK since server not likely
		// to have files that aren't in CVS...
		// TODO: Do look in CVS/Entries to decide if file is text or binary
		char first = 0;
		fread( &first, 1, 1, pFile );	// ok to fail
		std::string ext = name.substr( name.length()-3 );
		if (first == '<' || ext == ".py" || ext == ".fx" || ext == "txt" ||
			ext == "vsh" || ext == "psh" || ext == "fxh")
		{
			_setmode( _fileno( pFile ), _O_TEXT );
			ret = true;
		}
		fseek( pFile, 0, SEEK_SET );
	}

	return ret;
}


// helper method to get the MD5 of a file
bool getFileMD5( IFileSystem * pFS, const std::string & name, MD5::Digest & md5,
	bool crlfFileSystem = false )
{
	md5.clear();

	// could look in catalogue but this is almost certainly faster!
	// (since we don't have the RAM to keep the catalogue in memory)

	FILE * pFile = pFS->posixFileOpen( name, "rb" );
	if (pFile == NULL) return false;
	char * rbuf = s_scratchBuffer;

	fseek( pFile, 0, SEEK_END );
	uint32 full = ftell( pFile );
	fseek( pFile, 0, SEEK_SET );

	if (full == 0) { fclose( pFile ); return true; }

	bool earlyOK = false;
	if (crlfFileSystem)
		earlyOK = setTextModeIfTextFile( pFS, name, pFile );

	MD5 md5Calc;

	int ferr = 1;
	uint32 more;
	for (uint32 sofa = 0; sofa < full; sofa += more)
	{
		more = std::min( full-sofa, ScratchBufferFiller::SIZE );
		ferr = fread( rbuf, 1, more, pFile );
		if (ferr == 0) break;
		if (more != ferr && earlyOK) more = ferr;
		md5Calc.append( rbuf, more );
	}

	if (earlyOK) ferr = feof( pFile );	// might have legit ferr == 0

	md5 = md5Calc;
	fclose( pFile );

	return ferr != 0;
}


// helper method to copy a file between filesystems
bool copyFile( IFileSystem * pSrcFS, const std::string & srcName,
	IFileSystem * pDstFS, const std::string & dstName,
	bool crlfSrcFileSystem = false )
{
	FILE * pSrcFile = pSrcFS->posixFileOpen( srcName, "rb" );
	if (pSrcFile == NULL) return false;
	char * rbuf = s_scratchBuffer;

	fseek( pSrcFile, 0, SEEK_END );
	uint32 full = ftell( pSrcFile );
	fseek( pSrcFile, 0, SEEK_SET );

	FILE * pDstFile = pDstFS->posixFileOpen( dstName, "wb" );
	if (pDstFile == NULL)
	{
		fclose( pSrcFile );
		return false;
	}

	bool earlyOK = false;
	if (full > 0 && crlfSrcFileSystem)
		earlyOK = setTextModeIfTextFile( pSrcFS, srcName, pSrcFile );

	int ferr = 1;
	uint32 more;
	for (uint32 sofa = 0; sofa < full; sofa += more)
	{
		more = std::min( full-sofa, ScratchBufferFiller::SIZE );
		ferr = fread( rbuf, 1, more, pSrcFile );
		if (ferr == 0) break;
		if (more != ferr && earlyOK) more = ferr;
		if (ferr > 0 && earlyOK) more = ferr;
		ferr = fwrite( rbuf, 1, more, pDstFile );
		if (ferr == 0) break;
	}

	if (earlyOK) ferr = feof( pSrcFile );	// might have legit ferr == 0

	fclose( pDstFile );
	fclose( pSrcFile );

	return ferr != 0;
}

#include "zip/zlib.h"

// helper method to unzip a file
bool unzipFile( IFileSystem * pFS, const std::string & srcName,
	const std::string & dstName )
{
	z_stream zs;
	zs.zalloc = Z_NULL;
	zs.zfree = Z_NULL;
	zs.avail_in = 0;
	if (inflateInit( &zs ) != Z_OK) return false;

	FILE * pSrcFile = pFS->posixFileOpen( srcName, "rb" );
	if (pSrcFile == NULL)
	{
		inflateEnd( &zs );
		return false;
	}
	char * scratchBuf = s_scratchBuffer;
	uint8 * rbuf = (uint8*)scratchBuf;

	fseek( pSrcFile, 0, SEEK_END );
	uint32 full = ftell( pSrcFile );
	fseek( pSrcFile, 0, SEEK_SET );

	FILE * pDstFile = pFS->posixFileOpen( dstName, "wb" );
	if (pDstFile == NULL)
	{
		inflateEnd( &zs );
		fclose( pSrcFile );
		return false;
	}

	uint32 rbufSize = ScratchBufferFiller::SIZE/2-4096;
	uint32 wbufSize = ScratchBufferFiller::SIZE - rbufSize;
	uint8 * wbuf = rbuf + rbufSize;

	int ferr = 1;
	int zres = Z_OK;
	uint32 more;
	for (uint32 sofa = 0; sofa < full; sofa += more)
	{
		more = std::min( full-sofa, rbufSize );
		ferr = fread( rbuf, 1, more, pSrcFile );
		if (ferr == 0) break;
		bool readingFinished = sofa+more == full;

		// unzip it
		zs.next_in = rbuf;
		zs.avail_in = more;	// wait for Z_STREAM_END when readingFinished
		while (zs.avail_in != 0 || (readingFinished && zres == Z_OK))
		{
			zs.next_out = wbuf;			// Z_FINISH would need enough buffer
			zs.avail_out = wbufSize;	// to complete in a single call
			zres = inflate( &zs, (zs.avail_in != 0) ? 0 : Z_SYNC_FLUSH );
			if (zres != Z_OK && zres != Z_STREAM_END) break;

			ferr = fwrite( wbuf, 1, wbufSize-zs.avail_out, pDstFile );
			if (ferr == 0) break;
		}
		if (zs.avail_in != 0) break;
	}

	fclose( pDstFile );
	fclose( pSrcFile );

	inflateEnd( &zs );

	return ferr != 0 && zres == Z_STREAM_END;
}

// helper function to undiff a file (and find the appropriate bits)
bool undiffFile( IFileSystem * pFS, IFileSystem * pRomFS, IFileSystem * pDynFS,
				 const std::string& diffName, const std::string& srcName, 
				 const std::string& destName )
{
	FILE * pDiff;
	FILE * pSrc;
	FILE * pDest;

	pDiff = pFS->posixFileOpen( diffName, "rb" );
	if (!pDiff)
	{
		ERROR_MSG( "undiffFile: unable to find diff file %s\n", diffName.c_str() );
		return false;
	}
	pSrc = pFS->posixFileOpen( srcName, "rb" );
	if (!pSrc)
		pSrc = pDynFS->posixFileOpen( srcName, "rb" );
	if (!pSrc)
	{
		pSrc = pRomFS->posixFileOpen( srcName, "rb" );
		if (pSrc && ROMFS_IS_IN_CVS)
		{
			if (setTextModeIfTextFile( pRomFS, srcName, pSrc ))
			{
				// fseek doesn't work with text mode files thus bundiff
				// doesn't work either... so translate the file locally first
				fclose( pSrc );

				for (uint je = srcName.find_first_of( '/' );
					je < srcName.length();
					je = srcName.find_first_of( '/', je+1 ))
						pFS->makeDirectory( srcName.substr( 0, je ) );
				bool ok = copyFile( pRomFS, srcName, pFS, srcName, true );
				pSrc = pFS->posixFileOpen( srcName, "rb" );
				if (!ok || !pSrc)
				{
					if (pSrc) fclose( pSrc );	// unlikely
					ERROR_MSG( "undiffFile: unable to copy source file from "
						"text mode ROM FS to temp FS: %s\n", srcName.c_str() );
					return false;
				}
			}
		}
	}
	if (!pSrc)
	{
		ERROR_MSG( "undiffFile: unable to find source file %s\n", srcName.c_str() );
		fclose( pDiff );
		return false;
	}
	pDest = pFS->posixFileOpen( destName, "wb" );
	if (!pDest)
	{
		ERROR_MSG( "undiffFile: unable to find dest file %s\n", destName.c_str() );
		fclose( pSrc );
		fclose( pDiff );
		return false;
	}

	bool ok = performUndiff( pDiff, pSrc, pDest );

	fclose( pSrc );
	fclose( pDest );
	fclose( pDiff );

	return ok;
}

/// helper struct that we only want for its structors
struct CopyNameExposer
{
#if 0
	CopyNameExposer( const std::string & s ) { s_pCurOp->element = &s; }
	~CopyNameExposer() { s_pCurOp->element = NULL; }
#else
	CopyNameExposer( const std::string & s ) { }
#endif
};

/**
 *	This method downloads the given version of the given file in the given
 *	version point. Note that the file must have changed in that version.
 */
bool ResUpdateHandler::download( uint32 version, const std::string & point,
	const std::string & file, IFileSystem * pFS, const MD5::Digest & md5,
	float * progressVal )
{
	ScratchBufferFiller sbf;
	if (s_scratchBuffer == NULL)
	{
		ERROR_MSG( "ResUpdateHandler::download: "
			"Out of memory allocating scratch buffer\n" );
		return false;
	}

	std::string copyName = point + file;
	CopyNameExposer cnm( copyName );

	MD5::Digest emptyFileMD5; emptyFileMD5.clear();
	MD5::Digest oldMD5;

	// if the file is already there with the correct MD5 then great!
	// (and we must be continuing an earlier interrupted download)
	// TODO: continue partial downloads somehow...
	if (getFileMD5( pFS, copyName, oldMD5 ) && md5 == oldMD5) return true;

	// erase temporary files
	pFS->eraseFileOrDirectory( ".dltemp" );
	pFS->eraseFileOrDirectory( ".litemp" );
	pFS->eraseFileOrDirectory( copyName );	// likely to fail

	uint8 method = 0xFE;
	if (md5 == emptyFileMD5)
	{
		pFS->writeFile( ".dltemp", new BinaryBlock( this, 0 ), true );
		method = 0;	// whole empty file
	}
	else if (getFileMD5( pDynFS_, copyName, oldMD5 ) && md5 == oldMD5)
	{		// see if we were going to copy it here anyway
		if (version == latestVersion_) return true;
		copyFile( pDynFS_, copyName, pFS, ".dltemp" );
		method = 0;	// whole file
	}
	else if (getFileMD5( pRomFS_, copyName, oldMD5, ROMFS_IS_IN_CVS ) &&
		md5 == oldMD5)
	{		// see if we would be liking to copy it here anyway
		if (version == latestVersion_ &&
			pDynFS_->getFileType( copyName ) == IFileSystem::FT_NOT_FOUND)
				return true;
		copyFile( pRomFS_, copyName, pFS, ".dltemp", ROMFS_IS_IN_CVS );
		method = 0;	// whole file
	}
	else
	{
		// download whatever the server wants to send us for that file
		TRACE_MSG( "ResUpdateHandler::download: "
			"Starting download of %s...\n", file.c_str() );
		FILE * pFile = pFS->posixFileOpen( ".dltemp", "wb" );
		method = pServConn_->downloadResourceVersion(
			version, point, file, pFile, progressVal );
		fclose( pFile );
		TRACE_MSG( "ResUpdateHandler::download: "
			"Finished download of %s\n", file.c_str() );
	}

	// rebuild the file from the given data
	switch (method)
	{
	case 0:	// whole file
		pFS->moveFileOrDirectory( ".dltemp", ".litemp" );
		break;
	case 1:	// whole file, zipped
		if (!unzipFile( pFS, ".dltemp", ".litemp" ))
		{
			ERROR_MSG( "ResUpdateHandler::download: "
				"Error unzipping %s\n", copyName.c_str() );
			return false;
		}
		pFS->eraseFileOrDirectory( ".dltemp" );
		break;
	case 2:	// binary diff
		// Note: for the existing file to merge with the diff,
		//  should look first in pFS then in pDynFS_ then in pRomFS_
		if (!undiffFile( pFS, pRomFS_, pDynFS_, ".dltemp", copyName, ".litemp" ))
		{
			ERROR_MSG( "ResUpdateHandler::download: "
				"Error undiffing %s\n", copyName.c_str() );
			return false;
		}
		pFS->eraseFileOrDirectory( ".dltemp" );
		break;
	case 3:	// binary diff, zipped
		if (!unzipFile( pFS, ".dltemp", ".bdtemp" ))
		{
			ERROR_MSG( "ResUpdateHandler::download: "
				"Error unzipping %s\n", copyName.c_str() );
			return false;
		}
		pFS->eraseFileOrDirectory( ".dltemp" );
		if (!undiffFile( pFS, pRomFS_, pDynFS_, ".bdtemp", copyName, ".litemp" ))
		{
			ERROR_MSG( "ResUpdateHandler::download: "
				"Error undiffing %s\n", copyName.c_str() );
			return false;
		}
		pFS->eraseFileOrDirectory( ".bdtemp" );
		break;
	default:
		ERROR_MSG( "ResUpdateHandler::download: "
			"Unknown packing method after download of %s\n",
			copyName.c_str() );
		return false;
	case 0xFE:
		ERROR_MSG( "ResUpdateHandler::download: "
			"Connection problems receiving %s\n",
			copyName.c_str() );
		return false;
		break;
	case 0xFF:
		ERROR_MSG( "ResUpdateHandler::download: "
			"Server could not send %s\n",
			copyName.c_str() );
		return false;
		break;
	}

	// calculate its MD5 ... carefully
	MD5::Digest actMD5;
	if (!getFileMD5( pFS, ".litemp", actMD5 ))
	{
		ERROR_MSG( "ResUpdateHandler::download: "
			"Read error after reconstruction of %s\n",
			copyName.c_str() );
		return false;
	}

	// see if it matches
	if (actMD5 != md5)
	{
		ERROR_MSG( "ResUpdateHandler::download: "
			"MD5 mismatch after reconstruction of %s\n",
			copyName.c_str() );
		DEBUG_MSG( "\tdesired %s actual %s\n",
			md5.quote().c_str(), actMD5.quote().c_str() );
		return false;
	}

	// yay we got the file OK, so make a home for it
	for (uint je = copyName.find_first_of( '/' );
		je < copyName.length();
		je = copyName.find_first_of( '/', je+1 ))
			pFS->makeDirectory( copyName.substr( 0, je ) );

	// and move it in
	bool finalMoveSucceeded =
		pFS->moveFileOrDirectory( ".litemp", copyName );
	if (!finalMoveSucceeded)
	{
		ERROR_MSG( "ResUpdateHandler::download: "
			"Error doing final move after MD5 check of %s\n",
			copyName.c_str() );
	}
	return finalMoveSucceeded;
}



/**
 *	This method saves out the current state of our upd directory
 *	into the file named index.xml in that directory.
 */
void ResUpdateHandler::saveUpdIndexXML( const std::string & updTopDir )
{
	DataSectionPtr pSect = new XMLSection( "index.xml" );

	VersionPtTracker::iterator it;
	for (it = pVPTracker_->begin(); it != pVPTracker_->end(); ++it)
	{
		VersionPtInfo & vpi = it->second;
		if (!vpi.updDirPath.empty())
		{
			std::string updDir = vpi.updDirPath.substr(
				vpi.updDirPath.find_last_of( '/', vpi.updDirPath.length()-2 )+1,
				vpi.updDirPath.length()-1 );
			pSect->writeString( updDir, it->first );
		}
	}

	// TODO: Save out partial updates too

	IFileSystem * pUpdTopFS = NativeFileSystem::create( updTopDir );
	pUpdTopFS->writeFile( "index.xml", pSect->asBinary(), /*binary:*/false );
	delete pUpdTopFS;
}


#if 0
/**
 *	This method reports the progress of the operation in the current thread.
 *
 *	Returns a float between 0 and 1 for long operations, and sets element
 *	to a description of the part of the operation it is doing now
 *	(typically a file name, never hardcoded english text).
 */
float ResUpdateHandler::currentThreadOperationProgress( std::string & element )
{
	float ret = -1.f;
	element.clear();

	CurOpInfo * pCOI = s_pCurOp;
	if (pCOI != NULL)
	{
		ret = pCOI->progress;
		if (pCOI->element != NULL)
			element = *pCOI->element;
	}

	return ret;
}
#endif


/// Element of the resource catalogue
struct ResCatEntry
{
	ResCatEntry() : modDate( ~uint64(0) ) { }

	MD5::Digest	md5;
	uint64		modDate;
};
struct ResCat : public std::map<std::string,ResCatEntry> { };
// TODO: Use something much more efficient!


/// Element of version point calculator
struct VersionPtCalcEntry
{
	VersionPtCalcEntry() :
		shallowPass( false ), inRom( false ), inDyn( false ) { }

	bool		shallowPass;
	bool		inRom;
	bool		inDyn;
	MD5			romMD5Calc;
	MD5			dynMD5Calc;
};
struct VersionPtCalcs : public VersionPtMapType<VersionPtCalcEntry> { };

/// Element of simple version point presence list
struct VPLValue { VPLValue() : e(NO) {} enum { NO, DEEP, SHALLOW } e; };
struct VersionPtList : public std::map<std::string,VPLValue> { };


/**
 *	Basic init method that changes res paths.
 */
void ResUpdateHandler::init()
{
	if (!enabled_) return;

	// add the dyn directory if there isn't already one
	std::string topPath = BWResource::getPath( 0 );
	int topSlash = topPath.find_last_of('/',topPath.length()-1);
	if (topPath.substr( topSlash+1 ) != "dyn/")
	{
		// create it if necessary
		std::string dynDir = topPath.substr( 0, topSlash+1 ) + "dyn/";
		BWResource::ensurePathExists( dynDir );
		// make it the first path
		BWResource::addPath( dynDir, 0 );
	}
}


/**
 *	Take stock of our surroundings
 */
void ResUpdateHandler::stocktake( ProgressDisplay * pDisplay )
{
	if (!enabled_) return;

	pDisplay_ = pDisplay;

	// look at all the files we can't change
	pRomFS_ = new MultiFileSystem( *BWResource::instance().fileSystem() );

	ResCat romResCat;
	VersionPtList romVPts;
	this->stocktakeOne( pRomFS_, true/*trustCache*/, true/*trustModDate*/,
		romResCat, romVPts );

	// look at all the files we can change
	std::string dynPath = BWResource::getPath( 0 );
	if (*dynPath.rbegin() != '/') dynPath.append( "/" );
	pDynFS_ = NativeFileSystem::create( dynPath );
	ResCat dynResCat;
	VersionPtList dynVPts;
	bool trustModDateOnDynFS = true;
	this->stocktakeOne( pDynFS_, false/*trustCache*/, trustModDateOnDynFS,
		dynResCat, dynVPts );

	// construct the version point MD5s
	VersionPtCalcs vpts;
	MD5::Digest emptyFileMD5; emptyFileMD5.clear();

	// add the rom version pts
	for (VersionPtList::iterator it = romVPts.begin(); it != romVPts.end();it++)
	{		// all valid version points in rom
		if (it->second.e != it->second.NO)
		{
			vpts[it->first].inRom = true;
			vpts[it->first].shallowPass = (it->second.e == VPLValue::SHALLOW);
		}
	}
	// figure out their MD5s
	for (ResCat::iterator it = romResCat.begin(); it != romResCat.end(); it++)
	{
		if (it->second.md5 == emptyFileMD5) continue;
		VersionPtCalcs::iterator found = vpts.get( it->first, /*inDyn*/false );
		found->second.romMD5Calc.append(
			&it->second.md5, sizeof(it->second.md5) );
	}
	// add the dyn version pts
	for (VersionPtCalcs::iterator it = vpts.begin(); it != vpts.end();it++)
	{		// all version points from rom that are not cancelled by dyn
		VersionPtList::iterator found = dynVPts.find( it->first );
		if (found == dynVPts.end() || found->second.e != found->second.NO)
		{
			it->second.inDyn = true;
			// if not in dynVPts then leave shallowPass alone, o/wise set below
		}
	}
	for (VersionPtList::iterator it = dynVPts.begin(); it != dynVPts.end();it++)
	{		// all new and updated version points from dyn
		if (it->second.e != it->second.NO)
		{
			vpts[it->first].inDyn = true;
			vpts[it->first].shallowPass = (it->second.e == VPLValue::SHALLOW);
		}
	}
	// figure out their MD5s
	ResCat::iterator itDyn = dynResCat.begin();		// have to go through both
	ResCat::iterator itRom = romResCat.begin();		// maps simultaneously to
	ResCat::iterator ndDyn = dynResCat.end();		// preserve the same sort
	ResCat::iterator ndRom = romResCat.end();		// order as the server uses
	while (itDyn != ndDyn || itRom != ndRom)
	{
		VersionPtCalcs::iterator found;
		if (itDyn != ndDyn && (itRom == ndRom || itDyn->first <= itRom->first))
		{	// a resource in the dyn tree
			if (itDyn->second.md5 != emptyFileMD5)
			{
				found = vpts.get( itDyn->first, /*inDyn*/true );
				found->second.dynMD5Calc.append(
					&itDyn->second.md5, sizeof(itDyn->second.md5) );
			}
			if (itRom != ndRom && itDyn->first == itRom->first)
				++itRom;	// rom file masked if both exist
			++itDyn;
		}
		else
		{	// a resource in the rom tree
			if (itRom->second.md5 != emptyFileMD5)
			{
				found = vpts.get( itRom->first, /*inDyn*/true );
				found->second.dynMD5Calc.append(
					&itRom->second.md5, sizeof(itRom->second.md5) );
			}
			++itRom;
		}
	}
	// extract the digest from the accumulators
	pVPTracker_ = new VersionPtTracker();
	for (VersionPtCalcs::iterator it = vpts.begin(); it != vpts.end(); it++)
	{
		VersionPtInfo & vpi = (*pVPTracker_)[ it->first ];
		vpi.romMD5.clear();
		if (it->second.inRom)
			it->second.romMD5Calc.getDigest( vpi.romMD5 );
		vpi.inRom = int(it->second.inRom);
		vpi.dynMD5.clear();
		if (it->second.inDyn)
			it->second.dynMD5Calc.getDigest( vpi.dynMD5 );
		vpi.inDyn = int(it->second.inDyn);

		vpi.shallowPass = int(it->second.shallowPass);

		TRACE_MSG( "Version point '%s' %s pass:\n",
			it->first.c_str(), !(vpi.shallowPass) ? "deep" : "shallow" );
		TRACE_MSG( "\tromMD5 %s dynMD5 %s\n",
			vpi.romMD5.quote().c_str(),
			vpi.dynMD5.quote().c_str() );
	}

	// OK, we have version point info! Yay!
	g_versionPointPass = &::versionPointPass;
}


/// Element of the stocktake stack
struct StocktakeElt
{
	std::string		prefix;
	float	progressBeg;
	float	progressEnd;
	std::vector<std::string>	ignorePatterns;
};

static const uint32 MD5_CACHE_FORMAT_VERSION = 0x01010102;




/**
 *	Examine the given file system
 */
void ResUpdateHandler::stocktakeOne( IFileSystem * pFS,
	bool trustCache, bool trustModDate, ResCat & resCat, VersionPtList & vpts )
{
	resCat.clear();
	vpts.clear();
	bool catChanged = true;
	std::set<std::string> seenEntries;

	// look at the cache file unless we trust nothing
	FILE * pMD5CacheFile = NULL;
	if (trustCache || trustModDate)
		pMD5CacheFile = pFS->posixFileOpen( "catalogue.md5", "rb" );

	if (pMD5CacheFile != NULL)
	{
		BinaryFile bf( pMD5CacheFile );
		uint32 formatVersion = uint32(-1);
		bf >> formatVersion;
		if (formatVersion == MD5_CACHE_FORMAT_VERSION)
		{
			bf.readMap( resCat );
			bf.readMap( vpts );
			catChanged = false;

			if (trustCache) return;
			vpts.clear();	// will completely rebuild it now
		}
	}
	if (trustCache)
	{
		WARNING_MSG( "ResUpdateHandler::stocktakeOne: "
			"Asked to trust missing/corrupted cache file!\n" );
	}

	ProgressTask stocktakeTask =
		ProgressTask( pDisplay_, "Validating Resources", 1.f );

	vpts[ "" ].e = VPLValue::DEEP;
	MD5::Digest emptyFileMD5; emptyFileMD5.clear();

	ScratchBufferFiller sbf;

	uint32 numFiles = 0;
	uint32 numDirs = 0;

	std::vector<StocktakeElt> stack;
	StocktakeElt rse;
	rse.progressBeg = 0.f;
	rse.progressEnd = 1.f;
	rse.ignorePatterns.push_back( "*.fxo" );	// always locally generated
	stack.push_back( rse );
	while (!stack.empty())
	{
		StocktakeElt se = stack.back();
		stack.pop_back();

		if (ROMFS_IS_IN_CVS)					// ignore.opt only when in CVS
		{
			// figure out what we want ought to ignore, i.e. locally generated files
			BinaryPtr pOptsFile = readFileQuiet( pRomFS_, se.prefix + "ignore.opt" );
			if (!pOptsFile) pOptsFile = readFileQuiet( pRomFS_, se.prefix + "xbsync.opt" );
			if (pOptsFile)
			{
				DataSectionPtr pOptsSec = DataSection::createAppropriateSection(
					"ignore.opt", pOptsFile );
				if (!pOptsSec->read( "copy", true ))
				{
					stocktakeTask.set( se.progressEnd );
					continue;
				}

				pOptsSec->readSeq( "ignore", se.ignorePatterns );
			}
		}

		// read the directory and process each file in it
		IFileSystem::Directory dir = pFS->readDirectory( se.prefix );
		int dirLen = dir.size();
		float progressStep = (se.progressEnd-se.progressBeg)/dirLen;
		int progRHS = dirLen-1;
		int progLHS = 0;
		stocktakeTask.set( se.progressBeg );
		for (int i = dirLen-1; i >= 0; i--)
		{
			if (matchesIgnorePattern( dir[i], se.ignorePatterns ) ||
				(se.prefix.empty() && dir[i] == "catalogue.md5"))
			{
				progLHS++;
				continue;
			}

			std::string resName = se.prefix + dir[i];

			IFileSystem::FileInfo fi;
			IFileSystem::FileType ft = pFS->getFileType( resName, &fi );

			if (ft == IFileSystem::FT_DIRECTORY)
			{
				StocktakeElt dse;
				dse.prefix = resName + "/";
				dse.progressBeg = se.progressBeg + progRHS*progressStep;
				dse.progressEnd = se.progressBeg + (progRHS+1)*progressStep;
				dse.ignorePatterns = se.ignorePatterns;
				stack.push_back( dse );
				progRHS--;
			}
			else
			{
				ResCatEntry & rce = resCat[ resName ];
				if (!trustModDate || fi.modified != rce.modDate)
				{
					if (fi.size != 0)
					{
						getFileMD5( pFS, resName, rce.md5,
							(pFS == pRomFS_) ? ROMFS_IS_IN_CVS : false );
#if 0
						BinaryPtr pResBin = pFS->readFile( resName );
						MF_ASSERT( pResBin );
						MD5 calcMD5;
						calcMD5.append( pResBin->data(), pResBin->len() );
						calcMD5.getDigest( rce.md5 );
#endif
						// we don't care if we set catChanged unnecessarily
						// if we're not trusting mod dates, since we're
						// never going to save it out in that case anyway
					}
					else
					{
						rce.md5 = emptyFileMD5;
					}
					if (trustModDate)
					{
//						TRACE_MSG( "ResUpdateHandler::stocktakeOne: "
//							"Catalogue changed by %s\n", resName.c_str() );
						//TRACE_MSG( "\tOld mod date %s new %s\n",
						//	pFS->eventTimeToString( rce.modDate ).c_str(),
						//	pFS->eventTimeToString( fi.modified ).c_str() );
					}
					rce.modDate = fi.modified;
					catChanged = true;
				}
				seenEntries.insert( resName );

				// see if it is a version point file (no need to check root)
				if (dir[i] == "version.point" && !se.prefix.empty())
				{
					VPLValue there;
					// if corrupted or deleted then ignore,
					// otherwise set version pt presence
					if (fi.size != 0)
					{
						DataSectionPtr pDS = XMLSection::createFromBinary(
							"version.point", pFS->readFile( resName ) );
						//bool there;
						//if (pDS && (there=pDS->as( false )) == pDS->as( true ))
						//	vpts[ se.prefix ].v = int(there);
						if (pDS && pDS->as( false ))
							there.e = (!pDS->readBool( "shallow", false )) ?
								there.DEEP : there.SHALLOW;
					}
					// always want to make a version point elt so we can tell
					// when one present in rom fs has been deleted in dyn
					vpts[ se.prefix ] = there;
				}
				if (dir[i] == "space.settings" && !se.prefix.empty())
				{
					// if not deleted and we haven't seen a version.point file,
					// then by default there is a version pt here
					//if (fi.size != 0)
					//{
					//	VPLValue & vplv = vpts[ se.prefix ];
					//	if (vplv.v == -1) vplv.v = 1;
					//}
					VPLValue there;
					there.e = (fi.size != 0) ? there.SHALLOW : there.NO;
					VPLValue & vplv = vpts[ se.prefix ];
					// always want to make a version point elt so we can tell
					// when one present in rom fs has been deleted in dyn
					if (vplv.e == vplv.NO) vplv = there;
				}

				stocktakeTask.set( se.progressBeg + progLHS*progressStep );
				progLHS++;
				numFiles++;
			}
		}
		numDirs++;
	}

	// see if anything is gone from the cache, if we loaded it
	if (pMD5CacheFile != NULL)
	{
		for (ResCat::iterator it = resCat.begin(); it != resCat.end(); it++)
		{
			if (seenEntries.find( it->first ) == seenEntries.end())
			{
				TRACE_MSG( "ResUpdateHandler::stocktakeOne: "
					"Catalogue changed by deletion of %s\n", it->first.c_str());
				it = resCat.erase( it );
				catChanged = true;
			}
		}
	}

	// save out the cache if it is worthwhile
	if (trustModDate)
	{
		if (catChanged)
		{
			INFO_MSG( "ResUpdateHandler::stocktakeOne: "
				"Caching changed catalogue to disk\n" );
			// TODO: Should probably not do this if the filesystem is read-only.
			BinaryFile bf( pFS->posixFileOpen( "catalogue.md5", "wb" ) );

			if (bf.file())
			{
				uint32 cacheFormatVersion = MD5_CACHE_FORMAT_VERSION;
				bf << cacheFormatVersion;
				bf.writeMap( resCat );
				bf.writeMap( vpts );
			}
			else
			{
				WARNING_MSG( "ResUpdateHandler::stocktakeOne: "
						"Could not open catalogue.md5 for writing\n" );
			}
		}
	}

	INFO_MSG( "Resource stocktake: %d files and %d directories\n",
		numFiles, numDirs );

	stocktakeTask.set( 1.f );
}



/**
 *	Set the server connection.
 *	This should be called after a connection has been made.
 */
void ResUpdateHandler::serverConnection( ServerConnection * pServConn )
{
	if (!enabled_) return;

	pServConn_ = pServConn;
	if (pServConn_ == NULL) return;

	latestVersion_ = pServConn->latestVersion();

	std::string dynParent = BWResource::getPath( 0 );
	dynParent = dynParent.substr(
		0, dynParent.find_last_of('/',dynParent.length()-1) + 1 );
	IFileSystem * pDynParent = NativeFileSystem::create( dynParent );

	switch (pDynParent->getFileType( "upd" ))
	{
	case IFileSystem::FT_DIRECTORY:
		break;
	case IFileSystem::FT_FILE:
		pDynParent->eraseFileOrDirectory( "upd" );
		// fall through intentional
	default:
		pDynParent->makeDirectory( "upd" );
		break;
	}

	// go through any updates that were in progress
	BinaryPtr pUpdMapFile = pDynParent->readFile( "upd/index.xml" );
	DataSectionPtr pUpdMap;
	if (pUpdMapFile)
		pUpdMap = XMLSection::createFromBinary( "index.xml", pUpdMapFile );

	IFileSystem::Directory oldUpds = pDynParent->readDirectory( "upd" );
	for (uint i = 0; i < oldUpds.size(); i++)
	{
		if (oldUpds[i] == "index.xml") continue;

		switch (0)
		{
		case 0:
			if (!pUpdMap) break;

			DataSectionPtr pPointSect = pUpdMap->openSection( oldUpds[i] );
			if (!pPointSect) break;

			std::string point = pPointSect->asString();
			VersionPtTracker::iterator found = pVPTracker_->find( point );
			if (found == pVPTracker_->end()) break;

			VersionPtInfo & vpi = found->second;
			vpi.updDirPath = dynParent + "upd/" + oldUpds[i] + "/";
			vpi.updInProgress = false;
			continue;
		}

		pDynParent->eraseFileOrDirectory( "upd/" + oldUpds[i] );
	}

	delete pDynParent;

	// go past the root version point
	// (this may launch a blocker update)
	this->versionPointPass( std::string() );
}



/**
 *	Static method to download the current version root point in its own thread.
 *	Not having the current version blocks enabling entities and actually
 *	playing the game.
 */
void ResUpdateHandler::blockerDownloadThread( void * pArgs )
{
	BackgroundDownloadArgs & bda = *(BackgroundDownloadArgs*)pArgs;

	ResUpdateHandler & ruh = ruh.instance();
	bool ok = ruh.versionPointDownload(
		bda.version, bda.point, bda.clearDyn, std::string() );
	if (ok)
		ruh.versionPointInstall( bda.version, bda.point, std::string() );

	// signal main thread to disconnect and relaunch itself (and delete us)
	ruh.blockerDownloadingComplete_ = bda.pThread;

	delete &bda;
}


/**
 *	Static method to download the impending version root point
 *	in its own thread.
 */
void ResUpdateHandler::impendingDownloadThread( void * pArgs )
{
	BackgroundDownloadArgs & bda = *(BackgroundDownloadArgs*)pArgs;

	ResUpdateHandler & ruh = ruh.instance();
	bool ok = ruh.versionPointDownload( bda.version, bda.point,
		/*clearDyn:*/false, std::string() );

	// signal main thread to call enable entities again and delete us
	if (ok)
		ruh.impendingDownloadingComplete_ = bda.pThread;

	// if not ok then wait for relogin when migration occurs...
	// don't signal main thread or it'll install what we've got

	delete &bda;
}

/**
 *	This method lets us know that the next version is impending and how
 *	close it is.
 */
void ResUpdateHandler::impendingVersion( int proximity )
{
	Personality & pPerso = Personality::instance();

	if (proximity == ServerMessageHandler::DOWNLOAD)
	{
		DEBUG_MSG( "ResUpdateHandler::impendingVersion: "
			"Got order to commence download of %lu\n", latestVersion_+1 );

		// launch another thread to...
		// figure out where we're up to in the download and continue it
		BackgroundDownloadArgs * pBDA = new BackgroundDownloadArgs;
		void * backgroundDownloader = new char[ sizeof(SimpleThread) ];
		pBDA->pThread = (SimpleThread*)backgroundDownloader;
		pBDA->version = latestVersion_+1;
		pBDA->point = std::string();
		pBDA->clearDyn = false;	// unused
		new (backgroundDownloader) SimpleThread(
			&ResUpdateHandler::impendingDownloadThread, pBDA );
	}
	else if (proximity == ServerMessageHandler::LOADIN)
	{
		DEBUG_MSG( "ResUpdateHandler::impendingVersion: "
			"Got order to commence loadin of %lu\n", latestVersion_+1 );

		// assert that download is complete (it should be! or else the
		// server would have kicked us off by now)
		MF_ASSERT( !this->rootDownloadInProgress() );

		// TODO: start background reload

		Script::call( PyObject_GetAttrString( pPerso, "onResUpdateLoadin" ),
			PyTuple_New(0), "impendingVersion LOADIN:", /*okIfFnNULL:*/true );

		// for BW1.7, personality script should give the user
		// some notice (like 30s/1min) then call resUpdateInstallAndRelaunch.
	}
	else if (proximity == ServerMessageHandler::MIGRATE)
	{
		DEBUG_MSG( "ResUpdateHandler::impendingVersion: "
			"Got order to migrate immediately to %lu\n", latestVersion_+1 );
		// TODO: block until loadin is complete when initiated above

		// migrate to new resources
		// TODO: Link pUpdFS into res path instead of installing here
		uint32 imminentVersion = latestVersion_+1;
		this->versionPointInstall( imminentVersion,
			std::string(), std::string() );
		latestVersion_ = imminentVersion;	// da-da!

		// TODO: Run migration/update code

		// TODO: copy new resources over old ones in background thread
		// instead of in main thread right here
	}
}


/// Structure of an personality callback
struct PersonalityCallback
{
	uint32						version;
	std::string					point;
	SmartPointer<Vector4Basic>	progressInfo;
	bool						called;
};


/**
 *	Method to determine whether or not any root download is in progress.
 */
bool ResUpdateHandler::rootDownloadInProgress()
{
	SimpleMutexHolder smh( personalityCallbacksLock_ );

	for (PersonalityCallbacks::iterator it = personalityCallbacks_.begin();
		it != personalityCallbacks_.end();
		++it)
	{
		if (it->point.empty()) return true;
	}

	return false;
}


/**
 *	This method is called by script when it wants to install the update
 *	for the root version point which it has been downloading. Thsi requires the
 *	client to disconnect and relaunch.
 */
void ResUpdateHandler::rootInstallAndRelaunch( bool alreadyInstalled )
{
	// first disconnect if we have not already done so -
	// or if this has not already been done for us
	ConnectionControl::instance().disconnect();

	// if the download has completed, then install it now. otherwise we
	// will finish it off and install it (and relaunch again) when we reconnect
	if (!this->rootDownloadInProgress() && !alreadyInstalled)
	{
		// alreadyInstalled is only false when it is an impending download
		uint32 impendingVersion = latestVersion_+1;
		this->versionPointInstall(
			impendingVersion, std::string(), std::string() );
	}

	// now do the equivalent of a unix 'exec'
	/*
	STARTUPINFO si;
	memset( &si, 0, sizeof(si) );
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi;
	memset( &pi, 0, sizeof(pi) );
	CreateProcess( NULL, GetCommandLine(),
		NULL, NULL, FALSE, 0, 0, NULL, &si, &pi );

	ExitProcess( 0 );
	*/
	App::instance().quit( /*restart:*/true );
}

/*~ function BigWorld.resUpdateInstallAndRelaunch
 *	@components{ client }
 *
 *	Placeholder for future functionality.
 */
static void resUpdateInstallAndRelaunch()
{
	ResUpdateHandler::instance().rootInstallAndRelaunch(
		/*alreadyInstalled:*/false );
}
PY_AUTO_MODULE_FUNCTION( RETVOID, resUpdateInstallAndRelaunch, END, BigWorld )





/**
 *	Add a callback to our list of callbacks.
 */
Vector4 * ResUpdateHandler::addPersonalityCallback(
	uint32 version, const std::string & point )
{
	SimpleMutexHolder smh( personalityCallbacksLock_ );

	PersonalityCallback pc;
	pc.point = point;
	pc.version = version;
	pc.progressInfo = new Vector4Basic();	// should be ok in non-main thread
	pc.progressInfo->decRef();
	pc.called = false;
	personalityCallbacks_.push_back( pc );
	return pc.progressInfo->pValue();
}


/**
 *	Tick method.
 */
void ResUpdateHandler::tick()
{
	// did we finish downloading and installing the blocker current version
	// in the background?
	if (blockerDownloadingComplete_ != NULL)
	{
		delete blockerDownloadingComplete_;
		blockerDownloadingComplete_ = NULL;

		// since we have already loaded entities and other resources well
		// before this point, we actually have to relaunch here so we can
		// get the new ones...

		// give the personality script a heads up
		Script::call( PyObject_GetAttrString(
				Personality::instance(), "onResUpdateAutoRelaunch" ),
			PyTuple_New(0), "onResUpdateRelaunch: ", /*okIfFnNULL:*/true );

		// and relaunch!
		this->rootInstallAndRelaunch( /*alreadyInstalled:*/true );
		// relaunch is not immediate...
	}

	// did we finish downloading the impending version in the background?
	if (impendingDownloadingComplete_ != NULL)
	{
		delete impendingDownloadingComplete_;
		impendingDownloadingComplete_ = NULL;

		// tell the server we are ready for the update!
		pServConn_->enableEntities(
			/*impendingVersionDownloaded:*/true );
	}

	// see if there are any personality callbacks we want to call
	PersonalityCallbacks toCall;
	personalityCallbacksLock_.grab();
	for (PersonalityCallbacks::iterator it = personalityCallbacks_.begin();
		it != personalityCallbacks_.end();
		++it)
	{
		if (!it->called)
		{
			toCall.push_back( *it );
			it->called = true;
		}
		else
		{
			if (it->progressInfo->pValue()->x == -1.f)
			{
				toCall.push_back( *it );
				it = personalityCallbacks_.erase( it );
				--it;
			}
		}
	}
	personalityCallbacksLock_.give();

	// call them outside the lock
	// (in case they access unupdated resources or something else nasty)
	if (!toCall.empty())
	{
		for (PersonalityCallbacks::iterator it = toCall.begin();
			it != toCall.end();
			++it)
		{
			PyObject * pArgs = Py_BuildValue( "(isO)",
				it->version, it->point.c_str(), &*it->progressInfo );

			if (!it->called)
				Script::call( PyObject_GetAttrString(
						Personality::instance(), "onResUpdateDownloadBegin" ),
					pArgs, "onResUpdateDownloadBegin: ", /*okIfFnNULL:*/true );
			else
				Script::call( PyObject_GetAttrString(
						Personality::instance(), "onResUpdateDownloadEnd" ),
					pArgs, "onResUpdateDownloadEnd: ", /*okIfFnNULL:*/true );
		}
	}
}

/**
 *	Static instance method
 */
ResUpdateHandler & ResUpdateHandler::instance()
{
	static ResUpdateHandler instance;
	return instance;
}

// update_handler.cpp
