/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "soundmanager.hpp"

#include "resmgr/bwresource.hpp"

#if FMOD_SUPPORT
# include <fmod_errors.h>
#endif

DECLARE_DEBUG_COMPONENT2( "SoundManager", 0 );	// debugLevel for this file

SoundManager SoundManager::s_instance_;

#if FMOD_SUPPORT

SoundManager::SoundManager() :
	errorLevel_( SoundManager::WARNING ),
    lastSet_( false ),
	eventSystem_( NULL ),
	defaultProject_( NULL )
{}

SoundManager::~SoundManager()
{
	FMOD_RESULT result;

	// Free all the event projects
	for (EventProjects::iterator it = eventProjects_.begin();
		 it != eventProjects_.end(); ++it)
	{
		result = it->second->release();

		if (result != FMOD_OK)
		{
			ERROR_MSG( "SoundManager::~SoundManager: "
				"Failed to release project '%s': %s\n",
				it->first.c_str(), FMOD_ErrorString( result ) );
		}
	}
}


/**
 *  Returns the singleton instance of this class.
 */
SoundManager& SoundManager::instance()
{
	return s_instance_;
}


/**
 *  Initialises the sound manager and devices.
 */
bool SoundManager::initialise( DataSectionPtr config )
{
	// check what we're gonna do when play()/get() calls fail
	if (config != NULL)
	{
		std::string errorLevel = config->readString( "errorLevel", "warning" );
		if (errorLevel == "silent")
		{
			this->errorLevel( SoundManager::SILENT );
		}
		else if (errorLevel == "warning")
		{
			this->errorLevel( SoundManager::WARNING );
		}
		else if (errorLevel == "exception")
		{
			this->errorLevel( SoundManager::EXCEPTION );
		}
		else
		{
			ERROR_MSG( "SoundManager::initialise: "
				"Unrecognised value for soundMgr/errorLevel: %s\n",
				errorLevel.c_str() );

			this->errorLevel( SoundManager::WARNING );
		}
	}

	FMOD_RESULT result;

	result = FMOD::EventSystem_Create( &eventSystem_ );

	if (result == FMOD_OK)
	{
		result = eventSystem_->init( FMOD_CHANNELS, FMOD_INIT_NORMAL, NULL );

		if (result != FMOD_OK)
		{
			ERROR_MSG( "SoundManager::initialise: "
				"Couldn't initialise event system: %s\n",
				FMOD_ErrorString( result ) );

			eventSystem_ = NULL;
		}
	}
	else
	{
		ERROR_MSG( "SoundManager::initialise: "
			"Couldn't create event system: %s\n",
			FMOD_ErrorString( result ) );

		eventSystem_ = NULL;
	}

	if (eventSystem_ == NULL)
	{
		NOTICE_MSG( "SoundManager::initialise: "
			"Sound init has failed, suppressing all sound error messages\n" );

		errorLevel_ = SoundManager::SILENT;
		return false;
	}

	// Break out now if XML config wasn't passed in
	if (config == NULL)
		return true;

	this->setPath( config->readString( "mediapath", "" ) );

	DataSectionPtr banks = config->openSection( "soundbanks" );

	if (banks)
	{
		for (int i=0; i < banks->countChildren(); ++i)
		{
			DataSectionPtr file = banks->openChild( i );
			if (file)
			{
				if (this->loadSoundBank( file->asString() ))
				{
					INFO_MSG( "SoundManager::initialise: "
						"Loaded soundbank %s\n",
						file->asString().c_str() );
				}
				else
				{
					ERROR_MSG( "SoundManager::initialise: "
						"Failed to load sound bank %s\n",
						file->asString().c_str() );
				}
			}
		}
	}
	else
	{
		WARNING_MSG( "SoundManager::initialise: "
			"No <soundMgr/soundbanks> config section found, "
			"no sounds have been loaded\n" );
	}

	return true;
}


/**
 *  Call the FMOD update() function which must be called once per main game
 *  loop.
 */
bool SoundManager::update()
{
	if (eventSystem_ == NULL)
		return false;

	FMOD_RESULT result = eventSystem_->update();

	if (result != FMOD_OK)
	{
		ERROR_MSG( "SoundManager::update: %s", FMOD_ErrorString( result ) );
	}

	return result == FMOD_OK;
}


/**
 *  Sets the path for the sound system to use when locating sounds.  This is
 *  just an interface to FMOD::EventSystem::setMediaPath().
 */
bool SoundManager::setPath( const std::string &path )
{
	if (eventSystem_ == NULL)
		return false;

	if (path.length() == 0)
	{
		ERROR_MSG( "SoundManager::setPath: "
			"Called with an empty path\n" );
		return false;
	}

	std::string resolvedPath = BWResolver::resolveFilename( path );

	FMOD_RESULT result;

	if ((result = eventSystem_->setMediaPath( resolvedPath.c_str() )) != FMOD_OK)
	{
		ERROR_MSG( "SoundManager::setPath: "
			"Couldn't set media path to '%s': %s\n",
			resolvedPath.c_str(), FMOD_ErrorString( result ) );
	}

	return result == FMOD_OK;
}


/**
 *  Loads a sound bank from an FMOD .fev project file.
 */
bool SoundManager::loadSoundBank( const std::string &fev )
{
	// Check for standard .fev extension
	if (fev.size() <= 3 || fev.rfind( ".fev" ) != fev.size() - 4)
	{
		ERROR_MSG( "SoundManager::loadSoundBank: "
			"Soundbanks must have standard FMOD project .fev extension: %s\n",
			fev.c_str() );

		return false;
	}

	// Prepend leading slash and drop extension to conform to standard syntax
	std::string path = "/";
	path += fev.substr( 0, fev.size() - 4 );

	FMOD::EventProject *pProject;
	return this->parsePath( path, &pProject, NULL, NULL );
}


/**
 *  Helper method for loadWaveData() and unloadWaveData().
 */
bool SoundManager::loadUnload( const std::string &group, bool load )
{
	FMOD::EventProject *pProject;
	FMOD::EventGroup *pGroup;
	FMOD_RESULT result;

	if (!this->parsePath( group, &pProject, &pGroup, NULL ))
	{
		PyErr_Format( PyExc_RuntimeError, "SoundManager::loadUnload: "
			"parsePath() failed for %s, see debug output for more info",
			group.c_str() );
		return false;
	}

	if (load)
	{
		result = pGroup->loadEventData(
			FMOD_EVENT_RESOURCE_SAMPLES, FMOD_EVENT_DEFAULT );
	}
	else
	{
		result = pGroup->freeEventData( NULL );
	}

	if (result != FMOD_OK)
	{
		PyErr_Format( PyExc_RuntimeError, "SoundManager::loadUnload: "
			"%sEventData() failed: %s",
			load ? "load" : "free", FMOD_ErrorString( result ) );
		return false;
	}

	return true;
}


/**
 *  Trigger a sound event, returning a handle to the event if successful, or
 *  NULL on failure.  For details on the semantics of event naming, please see
 *  the Python API documentation for BigWorld.playSound().
 */
SoundManager::Event* SoundManager::play( const std::string &name )
{
	FMOD_RESULT result;

	Event *pEvent = this->get( name );

	if (pEvent == NULL)
		return NULL;

	result = pEvent->start();
	if (result != FMOD_OK)
	{
		PyErr_Format( PyExc_RuntimeError, "SoundManager::play: "
			"Failed to play '%s': %s",
			name.c_str(), FMOD_ErrorString( result ) );

		return NULL;
	}

	return pEvent;
}


/**
 *  Fetch a handle to a sound event.  For details on the semantics of event
 *  naming, please see the Python API documentation for BigWorld.playSound().
 */
SoundManager::Event* SoundManager::get( const std::string &name )
{
	FMOD::EventProject *pProject;
	FMOD::EventGroup *pGroup;
	FMOD::Event *pEvent;

	if (this->parsePath( name, &pProject, &pGroup, &pEvent ))
		return pEvent;
	else
	{
		PyErr_Format( PyExc_LookupError, "SoundManager::get: "
			"parsePath() failed for %s", name.c_str() );
		return NULL;
	}
}


/**
 *  Set the 3D position of a sound event.
 */
bool SoundManager::set3D( Event *pEvent, const Vector3& pos, bool silent )
{
	FMOD_RESULT result;

	if (pEvent == NULL)
	{
		ERROR_MSG( "SoundManager::set3D: "
			"NULL event handle passed\n" );
		return false;
	}

	result = pEvent->set3DAttributes( (FMOD_VECTOR*)&pos, NULL, NULL );

	if (result != FMOD_OK && !silent)
	{
		ERROR_MSG( "SoundManager::set3D: "
			"Failed to set 3D attributes for %s: %s\n",
			SoundManager::name( pEvent ), FMOD_ErrorString( result ) );
	}

	return result == FMOD_OK;
}


/**
 *  Set the project that will be used to resolve relatively-named sound events.
 */
bool SoundManager::setDefaultProject( const std::string &name )
{
	FMOD::EventProject *pProject;
	std::string path = "/";
	path.append( name );

	if (!this->parsePath( path, &pProject, NULL, NULL ))
		return false;

	defaultProject_ = pProject;
	return true;
}


/**
 *  Sets the microphone position of the listener.
 *
 *  @param position		The listener's position
 *  @param forward		The forward vector
 *  @param forward		The up vector
 *  @param deltaTime	Time since last call to this method
 */
bool SoundManager::setListenerPosition( const Vector3& position,
	const Vector3& forward, const Vector3& up, float deltaTime )
{
	if (eventSystem_ == NULL)
		return false;

    // if the listener position has been set before
    if( lastSet_ )
    {
        if( deltaTime > 0 )
        {
            lastVelocity_ = ( position - lastPosition_ ) / deltaTime;
        }
        else
        {
            lastVelocity_ = Vector3( 0, 0, 0 );
        }

        lastPosition_ = position;
    }
    else
    {
        lastSet_ = true;
        lastPosition_ = position;
        lastVelocity_ = Vector3( 0, 0, 0 );
    }

	eventSystem_->set3DListenerAttributes( 0,
		(FMOD_VECTOR*)&lastPosition_,
		(FMOD_VECTOR*)&lastVelocity_,
		(FMOD_VECTOR*)&forward,
		(FMOD_VECTOR*)&up );

    return this->update();
}


/**
 *  Set the master volume.  Returns true on success.  All errors are reported as
 *  Python errors, so if you are calling this from C++ you will need to extract
 *  error messages with PyErr_PrintEx(0).
 */
bool SoundManager::setMasterVolume( float vol )
{
	FMOD_RESULT result;
	FMOD::EventCategory *pCategory;

	if (eventSystem_ == NULL)
	{
		PyErr_Format( PyExc_RuntimeError, "SoundManager::setMasterVolume: ",
			"No sound subsystem, can't set master volume" );
		return false;
	}

	result = eventSystem_->getCategory( "master", &pCategory );
	if (result != FMOD_OK)
	{
		PyErr_Format( PyExc_RuntimeError, "SoundManager::setMasterVolume: ",
			"Couldn't get master EventCategory: %s\n",
			FMOD_ErrorString( result ) );

		return false;
	}

	result = pCategory->setVolume( vol );
	if (result != FMOD_OK)
	{
		PyErr_Format( PyExc_RuntimeError, "SoundManager::setMasterVolume: ",
			"Couldn't set master channel group volume: %s\n",
			FMOD_ErrorString( result ) );

		return false;
	}

	return true;
}


/**
 *  This is the catch-all method for parsing soundbank paths.  The general
 *  semantics are similar to those for filesystem paths, which gives two general
 *  forms of event name:
 *
 *  absolute: /project/group1/group2/event
 *  relative: group1/group2/event
 *
 *  The default project is used to look up relative paths.
 *
 *  The caller must pass in FMOD pointer pointers which the return values are
 *  written to, or NULL if the caller isn't interested in a particular pointer.
 *
 *  You cannot pass a non-NULL pointer after you have passed a NULL one,
 *  i.e. you can't pass NULL for ppProject and then pass a non-NULL pointer for
 *  ppGroup.
 *
 *  If you pass NULL for ppEvent, then the entire path is considered to be the
 *  name of the event group, rather than the usual 'path/to/group/eventname'
 *  semantics.
 */
bool SoundManager::parsePath( const std::string &path,
	FMOD::EventProject **ppProject, FMOD::EventGroup **ppGroup,
	Event **ppEvent )
{
	FMOD_RESULT result;
	ssize_t groupStart = 0;
	std::string groupName, eventName;

	if (eventSystem_ == NULL)
		return false;

	// Sanity check for the path
	if (path.size() == 0)
	{
		ERROR_MSG( "SoundManager::parsePath: Invalid path '%s'\n",
			path.c_str() );

		return false;
	}

	// If the project isn't wanted, bail now
	if (!ppProject)
		return true;

	// If the leading character is a '/', then the project has been manually
	// specified
	if (path[0] == '/')
	{
		ssize_t firstSlash = path.find( '/', 1 );
		bool gotFirstSlash = (firstSlash != std::string::npos);

		groupStart = gotFirstSlash ? firstSlash + 1 : path.size();

		std::string projectName( path, 1,
			gotFirstSlash ? firstSlash - 1 : path.size() - 1 );

		EventProjects::iterator it = eventProjects_.find( projectName );

		// If the project isn't loaded, do it now
		if (it == eventProjects_.end())
		{
			result = eventSystem_->load(
				(projectName + ".fev").c_str(), NULL, ppProject );

			if (result == FMOD_OK)
			{
				eventProjects_[ projectName ] = *ppProject;

				// Set the default project if there isn't one already
				if (defaultProject_ == NULL)
					defaultProject_ = *ppProject;
			}
			else
			{
				ERROR_MSG( "SoundManager::parsePath: "
					"Failed to load project %s: %s\n",
					projectName.c_str(), FMOD_ErrorString( result ) );

				return false;
			}
		}

		// Otherwise just pass back handle to already loaded project
		else
		{
			*ppProject = it->second;
		}
	}

	// If no leading slash, then we're talking about the default project
	else
	{
		groupStart = 0;

		if (defaultProject_ == NULL)
		{
			PyErr_Format( PyExc_LookupError, "SoundManager::parsePath: "
				"No project specified and no default project loaded: %s",
				path.c_str() );

			return false;
		}
		else
		{
			*ppProject = defaultProject_;
		}
	}

	// If the group isn't wanted, bail now
	if (!ppGroup)
		return true;

	// If ppEvent isn't provided, then the group name is the rest of the path
	if (!ppEvent)
	{
		groupName = path.substr( groupStart );
	}

	// Otherwise, we gotta split on the final slash
	else
	{
		ssize_t lastSlash = path.rfind( '/' );

		if (lastSlash == std::string::npos || lastSlash < groupStart)
		{
			PyErr_Format( PyExc_SyntaxError, "SoundManager::parsePath: "
				"Asked for illegal top-level event '%s'", path.c_str() );
			return false;
		}

		ssize_t eventStart = lastSlash + 1;
		groupName = path.substr( groupStart, lastSlash - groupStart );
		eventName = path.substr( eventStart );
	}

	// If the event group hasn't been loaded yet, do it now.
	Group g( *ppProject, groupName );
	EventGroups::iterator it = eventGroups_.find( g );

	if (it != eventGroups_.end())
	{
		*ppGroup = it->second;
	}
	else
	{
		result = (*ppProject)->getGroup(
			groupName.c_str(), true, ppGroup );

		if (result == FMOD_OK)
		{
			eventGroups_[ g ] = *ppGroup;
		}
		else
		{
			PyErr_Format( PyExc_LookupError, "SoundManager::get: "
				"Couldn't get event group '%s': %s",
				groupName.c_str(), FMOD_ErrorString( result ) );

			return false;
		}
	}

	// If the event isn't wanted, bail now
	if (!ppEvent)
		return true;

	// Get event handle
	result = (*ppGroup)->getEvent( eventName.c_str(), FMOD_EVENT_DEFAULT, ppEvent );

	if (result != FMOD_OK)
	{
		PyErr_Format( PyExc_LookupError, "SoundManager::get: "
			"Couldn't get event %s from group %s: %s",
			eventName.c_str(), groupName.c_str(), FMOD_ErrorString( result ) );

		return false;
	}

	return true;
}

// -----------------------------------------------------------------------------
// Section: EventList
// -----------------------------------------------------------------------------

SoundManager::EventList::EventList() :
	std::list< Event* >(),
	stopOnDestroy_( true )
{
}

/**
 *  If required, the destructor cleans up any sounds still remaining in the
 *  list.
 */
SoundManager::EventList::~EventList()
{
	if (stopOnDestroy_)
	{
		this->stopAll();
	}

	// If we're allowing sounds to play after this list is destroyed, then we
	// need to make sure their callbacks are disabled so they don't try to
	// delete themselves from this list after it has been destroyed
	else
	{
		for (iterator it = this->begin(); it != this->end(); ++it)
		{
			(*it)->setCallback( NULL, NULL );
		}
	}
}


/**
 *  FMOD callback called for sounds attached to models.  Just removes this
 *  Event* from the model's EventList.
 */
FMOD_RESULT F_CALLBACK attachedEventCallback( FMOD::Event * pEvent,
	FMOD_EVENT_CALLBACKTYPE type, void *p1, void *p2, void *userData )
{
	if (type == FMOD_EVENT_CALLBACKTYPE_EVENTFINISHED ||
		type == FMOD_EVENT_CALLBACKTYPE_STOLEN)
	{
		SoundManager::EventList *pList = (SoundManager::EventList*)userData;
		SoundManager::EventList::iterator iter = std::find(
			pList->begin(), pList->end(), pEvent );

		// It's fine for an event to not be in the list ... that just means it
		// was erased during an update(), probably because its Event* had been
		// stolen by a newer Event.
		if (iter != pList->end())
		{
			pList->erase( iter );
		}

		// Avoid needless callback on EVENTFINISHED for this event.
		if (type == FMOD_EVENT_CALLBACKTYPE_STOLEN)
		{
			pEvent->setCallback( NULL, NULL );
		}
	}

	return FMOD_OK;
}


/**
 *  Appends an Event to this list, and sets the FMOD callback for the Event so
 *  it will automatically remove itself from the list once it has finished
 *  playing.
 */
void SoundManager::EventList::push_back( Event *pEvent )
{
	pEvent->setCallback( (FMOD_EVENT_CALLBACK)attachedEventCallback, this );
	std::list< Event* >::push_back( pEvent );
}


/**
 *  Update positions for any sounds that are still playing.
 */
bool SoundManager::EventList::update( const Vector3 &pos )
{
	bool ok = true;

	iterator it = this->begin();
	while (it != this->end())
	{
		Event * pEvent = *it;

		if (SoundManager::instance().set3D( pEvent, pos, true ))
		{
			++it;
		}
		else
		{
			// If we get to here, the event must have had its channel stolen
			// and so you'd think that it's EVENTFINISHED callback would never
			// be called, but sometimes it seems to be.  Strange.  Make sure the
			// callback won't be called so we don't segfault in the callback if
			// this EventList has been deleted in the meantime.
			pEvent->setCallback( NULL, NULL );

			it = this->erase( it );
		}
	}

	return ok;
}

/**
 *  Stop and clear all sound events.
 */
bool SoundManager::EventList::stopAll()
{
	bool ok = true;

	for (iterator it = this->begin(); it != this->end(); ++it)
	{
		Event *pEvent = *it;

		// Nullify the callback now, since we don't want the callback to be
		// called when we stop() the event below.
		pEvent->setCallback( NULL, NULL );

		FMOD_RESULT result = pEvent->stop();

		if (result != FMOD_OK)
		{
			ERROR_MSG( "SoundManager::EventList::stopAll: "
				"Couldn't stop %s: %s\n",
				SoundManager::name( pEvent ), FMOD_ErrorString( result ) );

			ok = false;
		}
	}

	if (!ok)
	{
		ERROR_MSG( "SoundManager::EventList::stopAll: "
			"Some events failed to stop\n" );
	}

	this->clear();

	return ok;
}


/**
 *  Returns the name of the provided Event.  Uses memory managed by FMOD so
 *  don't expect the pointer to be valid for long.
 */
const char* SoundManager::name( Event *pEvent )
{
	static const char *err = "<error>";
	char *name;
	FMOD_RESULT result = pEvent->getInfo( NULL, &name, NULL );

	if (result == FMOD_OK)
	{
		return name;
	}
	else
	{
		ERROR_MSG( "SoundManager::name: %s\n", FMOD_ErrorString( result ) );
		return err;
	}
}


// -----------------------------------------------------------------------------
// Section: Stubs for disabled sound support
// -----------------------------------------------------------------------------

#else // FMOD_SUPPORT

/**
 *  All the operations here are no-ops and will fail.
 */

SoundManager& SoundManager::instance()
{
	return s_instance_;
}

bool SoundManager::initialise( DataSectionPtr config )
{
	return false;
}

bool SoundManager::update()
{
	return false;
}

bool SoundManager::setPath( const std::string &path )
{
	return false;
}

bool SoundManager::loadSoundBank( const std::string &fev )
{
	return false;
}

SoundManager::Event* SoundManager::play( const std::string &name )
{
	PyErr_SetString( PyExc_NotImplementedError,
		"FMOD support disabled, all sound calls will fail" );
	return NULL;
}

SoundManager::Event* SoundManager::get( const std::string &name )
{
	PyErr_SetString( PyExc_NotImplementedError,
		"FMOD support disabled, all sound calls will fail" );
	return NULL;
}

bool SoundManager::set3D( Event *pEvent, const Vector3& position, bool silent )
{
	return false;
}

bool SoundManager::setListenerPosition( const Vector3& position,
	const Vector3& forward, const Vector3& up, float deltaTime )
{
	return false;
}

bool SoundManager::setDefaultProject( const std::string &name )
{
	return false;
}

bool SoundManager::loadUnload( const std::string &group, bool load )
{
	return false;
}

const char *SoundManager::name( Event *pEvent )
{
	static const char * err =
		"<FMOD support disabled, all sounds calls will fail>";

	return err;
}

bool SoundManager::setMasterVolume( float vol )
{
	return false;
}

#endif // FMOD_SUPPORT


/**
 *  Precache the wavedata for a particular event group (and all groups and
 *  events below it).  See the documentation for
 *  FMOD::EventGroup::loadEventData() for more information.
 */
bool SoundManager::loadWaveData( const std::string &group )
{
	return this->loadUnload( group, true );
}


/**
 *  Unload the wavedata and free the Event* handles for an event group.  See the
 *  documentation for FMOD::EventGroup::freeEventData() for more info.
 */
bool SoundManager::unloadWaveData( const std::string &group )
{
	return this->loadUnload( group, false );
}


/**
 *  Return this if you are supposed to return an Event* from a function that is
 *  exposed to script and something goes wrong.
 */
PyObject* SoundManager::error()
{
	switch (instance().errorLevel())
	{
		case EXCEPTION:
			return NULL;

		case WARNING:
			PyErr_PrintEx(0);
			Py_RETURN_NONE;

		case SILENT:
		default:
			PyErr_Clear();
			Py_RETURN_NONE;
	}
}
