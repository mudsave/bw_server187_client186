/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CELLAPP_PROFILE_HPP
#define CELLAPP_PROFILE_HPP

#include "cellapp.hpp"
#include "cstdmf/profile.hpp"

extern ProfileGroup g_profileGroup;

/**
 *	This class stores a value in stamps but has access functions in seconds.
 */
class CPUStamp
{
	uint64	stamp_;
public:
	CPUStamp( uint64 stamp = 0 ) : stamp_( stamp ) {}
	CPUStamp( double seconds ) : stamp_( fromSeconds( seconds ) ) {}

	uint64 get() const 			{ return stamp_; }
	void set( uint64 stamp ) 	{ stamp_ = stamp; }

	double getInSeconds() const 		{ return toSeconds( stamp_ ); }
	void setInSeconds( double seconds ) { stamp_ = fromSeconds( seconds ); }

private:
	// Utility methods
	static double toSeconds( uint64 stamp )
	{
		return double( stamp )/stampsPerSecondD();
	}
	static uint64 fromSeconds( double seconds )
	{
		return uint64( seconds * stampsPerSecondD() );
	}
};

// __kyl__(30/7/2007) Special profile thresholds for T2CN
extern CPUStamp 	g_profileInitGhostTimeLevel;
extern int			g_profileInitGhostSizeLevel;
extern CPUStamp 	g_profileInitRealTimeLevel;
extern int			g_profileInitRealSizeLevel;
extern CPUStamp 	g_profileOnloadTimeLevel;
extern int			g_profileOnloadSizeLevel;
extern int			g_profileBackupSizeLevel;

#define START_PROFILE( PROFILE )		g_profileGroup[ PROFILE ].start();

#define IF_PROFILE_LONG( PROFILE )											\
	if (!g_profileGroup[ PROFILE ].running() &&								\
		g_profileGroup[ PROFILE ].lastTime * 								\
			CellApp::instance().updateHertz() > stampsPerSecond())			\

#define STOP_PROFILE( PROFILE )												\
{																			\
	g_profileGroup[ PROFILE ].stop();										\
	IF_PROFILE_LONG( PROFILE )												\
	{																		\
		WARNING_MSG( "%s:%d: Profile " #PROFILE " took %.2f seconds\n",		\
			__FILE__, __LINE__,												\
			double(g_profileGroup[PROFILE].lastTime) / stampsPerSecondD());	\
	}																		\
}

class AutoProfile
{
public:
	AutoProfile( int profile ) : profile_( profile )
	{ g_profileGroup[ profile ].start(); }

	~AutoProfile()
	{
		g_profileGroup[ profile_ ].stop();
	}

private:
	int profile_;
};

#define AUTO_PROFILE( PROFILE )												\
	AutoProfile localProfile( PROFILE );


#define STOP_PROFILE_WITH_CHECK( PROFILE )									\
	STOP_PROFILE( PROFILE )													\
	IF_PROFILE_LONG( PROFILE )


#define STOP_PROFILE_WITH_DATA( PROFILE, DATA )	\
									g_profileGroup[ PROFILE ].stop( DATA );

#define IS_PROFILE_RUNNING( PROFILE )	g_profileGroup[ PROFILE ].running();

namespace CellProfileGroup
{
	void init();
}

enum CellProfile
{
	RUNNING,

	CREATE_ENTITY,
	CREATE_GHOST,
	ONLOAD_ENTITY,
	UPDATE_CLIENT,
	UPDATE_CLIENT_PREPARE,
	UPDATE_CLIENT_LOOP,
	UPDATE_CLIENT_POP,
	UPDATE_CLIENT_APPEND,
	UPDATE_CLIENT_PUSH,
	UPDATE_CLIENT_SEND,
	UPDATE_CLIENT_UNSEEN,
	OFFLOAD_ENTITY,
	DELETE_GHOST,

	AVATAR_UPDATE,
	GHOST_AVATAR_UPDATE,
	GHOST_OWNER,
	SCRIPT_MESSAGE,
	SCRIPT_CALL,

	LOAD_BALANCE,
	BOUNDARY_CHECK,
	DELIVER_GHOSTS,

	INIT_REAL,
	INIT_GHOST,
	FORWARD_TO_REAL,
	POPULATE_KNOWN_LIST,
	FIND_ENTITY,
	PICKLE,
	UNPICKLE,

	ON_TIMER,
	ON_MOVE,
	ON_NAVIGATE,
	NAVIGATE_MOVE,
	CAN_NAVIGATE_TO,
	FIND_PATH,
	SHUFFLE_ENTITY,
	SHUFFLE_TRIGGERS,
	SHUFFLE_AOI_TRIGGERS,
	VISION_UPDATE,
	COLLISION_TEST,
	ENTITIES_IN_RANGE,

	CHUNKS_MAIN_THREAD,

	TICK_SLUSH,

	GAME_TICK,

	CALC_BOUNDARY,
	CALL_TIMERS,
	CALL_UPDATES,

	WRITE_TO_DB,

	BACKUP,

	PROFILE_SIZE // Must be last
};

// ifdef like this doesn't work with pch
//#ifdef CELLAPP_PROFILE_INTERNAL
inline const char ** gProfileLabels()
{
static const char *gProfileLabels_mem[] = {
	"running",

	"create entity",
	"create ghost",
	"onload entity",
	"update client",
	"update client prepare",
	"update client loop",
	"update client pop",
	"update client append",
	"update client push",
	"update client send",
	"update client unseen",
	"offload entity",
	"delete ghost",

	"avatar update",
	"ghost avatar update",
	"ghost owner",
	"script message",
	"script call",

	"load balance",
	"boundary check",
	"deliver ghosts",
	"init real",
	"init ghost",
	"forward to real",
	"populate known list",
	"find entity",
	"pickle",
	"unpickle",

	"on timer",
	"on move",
	"on navigate",
	"navigate move",
	"can navigate to",
	"find path",

	"shuffle entity",
	"shuffle triggers",
	"shuffle AoI triggers",

	"update vision",

	"collision test",
	"entities in range",

	"chunks main thread",

	"tick slush",

	"game tick",

	"calc boundary",
	"call timers",
	"call updates",

	"write to db",

	"backup",

	NULL
};
return gProfileLabels_mem;
}
//#endif

/**
 *	This function stops the given profile and returns the duration of the
 * 	last run (in stamps). Will return 0 if the profile is still running i.e.
 * 	there were nested starts.
 */
inline uint64 STOP_PROFILE_GET_TIME( CellProfile profile )
{
	ProfileVal& profileObj = g_profileGroup[ profile ];
	profileObj.stop();
	return (profileObj.running()) ? 0 : profileObj.lastTime;
}

#ifdef CODE_INLINE
#include "profile.ipp"
#endif

#endif // CELLAPP_PROFILE_HPP
