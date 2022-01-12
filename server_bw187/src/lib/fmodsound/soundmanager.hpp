/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef SOUND_MANAGER_HPP
#define SOUND_MANAGER_HPP

const int FMOD_CHANNELS = 64;
const int VOLUME_MAXIMUM = 255;

#include "pyscript/script.hpp"
#include "math/vector3.hpp"
#include "resmgr/datasection.hpp"
#include "fmod_config.hpp"

#include <map>
#include <string>
#include <vector>
#include <list>

#if FMOD_SUPPORT
# include <fmod_event.hpp>
#endif

class PySound;

// Shadow declarations of FMOD_EVENT_STATE values, since we need these in some
// header files.
#if !FMOD_SUPPORT
#define FMOD_EVENT_STATE_READY		0x00000001
#define FMOD_EVENT_STATE_PLAYING	0x00000008
#endif


class SoundManager
{
public:
	// Aliases for FMOD types
#if FMOD_SUPPORT
	typedef FMOD::Event Event;
	typedef FMOD::EventGroup EventGroup;
	typedef FMOD::EventParameter EventParameter;
	typedef FMOD_EVENT_STATE EventState;
#else
	typedef void Event;
	typedef void EventGroup;
	typedef void EventParameter;
	typedef uint32 EventState;
#endif

	static SoundManager& instance();
	bool initialise( DataSectionPtr config = NULL );
	bool update();
	bool setPath( const std::string &path );

	// Controls whether exceptions are raised on missing sounds
	void errorLevel( int lvl ) { errorLevel_ = lvl; }
	int errorLevel() const { return errorLevel_; }

	static const int SILENT = 0;
	static const int WARNING = 1;
	static const int EXCEPTION = 2;

	static PyObject* error();
	static const char* name( Event *pEvent );

	bool loadSoundBank( const std::string &projectName );
	bool loadWaveData( const std::string &group );
	bool unloadWaveData( const std::string &group );

	Event* play( const std::string &name );
	Event* get( const std::string &name );
	bool set3D( Event *pEvent, const Vector3 &pos, bool silent = false );
	bool setDefaultProject( const std::string &name );

	bool setListenerPosition( const Vector3& position, const Vector3& forward,
		const Vector3& up, float deltaTime );

	bool setMasterVolume( float vol );

protected:
	bool loadUnload( const std::string &group, bool load );

	// Controls what happen when we hit an error
	int errorLevel_;

	static SoundManager s_instance_;

#if FMOD_SUPPORT

	SoundManager();
	~SoundManager();

	bool parsePath( const std::string &path, FMOD::EventProject **ppProject,
		FMOD::EventGroup **ppGroup, Event **ppEvent );

	// Position listener was last set at
    Vector3 lastPosition_;

	// Velocity the listener was last going at (in units per second)
    Vector3 lastVelocity_;

	// Has the last position been set?
    bool lastSet_;

	FMOD::EventSystem* eventSystem_;

	// The FMOD Project used for resolving relatively-named sounds
	FMOD::EventProject* defaultProject_;

	// Mapping used for caching FMOD::EventGroups
	typedef std::pair< FMOD::EventProject*, std::string > Group;
	typedef std::map< Group, EventGroup* > EventGroups;
	EventGroups eventGroups_;

	// Mapping of loaded FMOD::EventProjects
	typedef std::map< std::string, FMOD::EventProject* > EventProjects;
	EventProjects eventProjects_;

public:
	// A list of 3D sound events used for tracking sounds associated with models
	class EventList : public std::list< Event* >
	{
	public:
		EventList();
		~EventList();

		void push_back( Event* pEvent );
		bool update( const Vector3 &pos );
		bool stopAll();
		void stopOnDestroy( bool enable ) { stopOnDestroy_ = enable; }

	protected:
		bool stopOnDestroy_;
	};

#else // FMOD_SUPPORT

public:
	typedef std::list< Event* > EventList;

#endif // FMOD_SUPPORT
};


#endif // SOUND_MANAGER_HPP
