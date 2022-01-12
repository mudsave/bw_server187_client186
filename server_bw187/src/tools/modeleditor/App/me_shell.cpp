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

#include "me_shell.hpp"
#include "romp_harness.hpp"
#include "me_scripter.hpp"

#include "appmgr/application_input.hpp"
#include "appmgr/module_manager.hpp"
#include "appmgr/options.hpp"
#include "resmgr/auto_config.hpp"
#include "ashes/simple_gui.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk_obstacle.hpp"
#include "tools_camera.hpp"
#include "gizmo/tool.hpp"
#include "input/input.hpp"
#include "moo/render_context.hpp"
#include "moo/visual_channels.hpp"
#include "resmgr/bwresource.hpp"
#include "romp/console_manager.hpp"
#include "romp/console.hpp"
#include "romp/engine_statistics.hpp"
#include "romp/lens_effect_manager.hpp"
#include "romp/z_buffer_occluder.hpp"
#include "romp/flora.hpp"
#include "guimanager/gui_input_handler.hpp"

#include "pyscript/pyobject_plus.hpp"


DECLARE_DEBUG_COMPONENT2( "Shell", 0 )

static AutoConfigString s_floraXML( "environment/floraXML" );
static AutoConfigString s_defaultSpace( "environment/defaultEditorSpace" );

MeShell * MeShell::s_instance_ = NULL;

// need to define some things for the libs we link to..
// for ChunkManager
std::string g_specialConsoleString = "";
// for network
const char * compileTimeString = __TIME__ " " __DATE__;
// for gizmos
void findRelevantChunks(class SmartPointer<class Tool>, float buffer = 0.f) { ;}

MeShell::MeShell()
	: hInstance_(NULL)
	, hWndApp_(NULL)
	, hWndGraphics_(NULL)
	, romp_(NULL)
{
	ASSERT(s_instance_ == NULL);
	s_instance_ = this;
}

MeShell::~MeShell()
{
	ASSERT(s_instance_);
	DebugFilter::instance().deleteMessageCallback( &debugMessageCallback_ );
	s_instance_ = NULL;

	std::vector<PhotonOccluder*>::iterator it = occluders_.begin();
	std::vector<PhotonOccluder*>::iterator end = occluders_.end();

	while ( it != end )
	{
		delete *it++;
	}
}


bool MeShell::initApp( HINSTANCE hInstance, HWND hWndInput, HWND hWndGraphics )
{	
	return MeShell::instance().init( hInstance, hWndInput, hWndGraphics );
}


/**
 *	This method intialises global resources for the application.
 *
 *	@param hInstance	The HINSTANCE variable for the application.
 *	@param hWnd			The HWND variable for the application's main window.
 *	@param hWndGraphics	The HWND variable the 3D device will use.
 *
 *	@return		True if initialisation succeeded, otherwise false.
 */
bool
MeShell::init( HINSTANCE hInstance, HWND hWndInput, HWND hWndGraphics )
{
	hInstance_ = hInstance;
	hWndApp_ = hWndInput;
	hWndGraphics_ = hWndGraphics;

	GUI::Win32InputDevice::install();

	initErrorHandling();

	// Create Direct Input devices
	int deviceInitFlags = 0;
	
	if ( Options::getOptionBool( "input/exclusive", false ) )
	{
		deviceInitFlags |= InputDevices::EXCLUSIVE_MODE;	
	}

	if (!InputDevices::init( hInstance, hWndInput,
			deviceInitFlags ))
	{
		CRITICAL_MSG( "MeShell::initApp - Init inputDevices FAILED\n" );
		return false;
	}

	if (!initGraphics())
	{
		return false;
	}

	if (!initScripts())
	{
		return false;
	}

	if (!initConsoles())
	{
		return false;
	}

	/*if (!SoundMgr::instance().init( hWndApp_ ))
	{
		return false;
	}*/
	
	// romp needs chunky
	{
		ChunkManager::instance().init();
		ChunkSpacePtr space = new ChunkSpace(1);

		std::string spacePath = Options::getOptionString( "space", s_defaultSpace );
		ChunkDirMapping* mapping = space->addMapping( SpaceEntryID(), Matrix::identity, spacePath );
		if (!mapping)
		{
			ERROR_MSG( "Couldn't map path \"%s\" as a space.", spacePath.c_str() );
			return false;
		}

		ChunkManager::instance().camera( Matrix::identity, space );

		// Call tick to give it a chance to load the outdoor seed chunk, before
		// we ask it to load the dirty chunks
		ChunkManager::instance().tick( 0.f );
	}

	if (ZBufferOccluder::isAvailable())
		occluders_.push_back( new ZBufferOccluder );
	else
		occluders_.push_back( new ChunkObstacleOccluder );
	LensEffectManager::instance().addPhotonOccluder( occluders_.back() );

	if (!initRomp())
	{
		return false;
	}

	ApplicationInput::disableModeSwitch();

	return true;
}

bool MeShell::initRomp()
{
	if ( !romp_ )
	{
		romp_ = new RompHarness;

		// set it into the BigBang module
		PyObject * pMod = PyImport_AddModule( "ModelEditor" );	// borrowed
		PyObject_SetAttrString( pMod, "romp", romp_ );

		if ( !romp_->init() )
		{
			CRITICAL_MSG( "MeShell::initRomp: init romp FAILED\n" );
			return false;
		}

		romp_->enviroMinder().activate();
	}
	return true;
}


/**
 *	This method finalises the application. All stuff done in initApp is undone.
 *	@todo make a better sentence
 */
void MeShell::fini()
{
	if ( romp_ )
		romp_->enviroMinder().deactivate();

	ChunkManager::instance().fini();

	if ( romp_ )
	{
		PyObject * pMod = PyImport_AddModule( "ModelEditor" );                  
        PyObject_DelAttrString( pMod, "romp" );                                 
        PyObject_DelAttrString( pMod, "opts" );                                 

        Py_DECREF( romp_ );                                                     
        romp_ = NULL;                                                           
	}

	MeShell::finiGraphics();

	MeShell::finiScripts();

	// Kill winsock
	//WSACleanup();
}


bool MeShellDebugMessageCallback::handleMessage( int componentPriority,
												int messagePriority, 
												const char * format, 
												va_list argPtr )                           
{
	return MeShell::instance().messageHandler( componentPriority, 
												messagePriority, 
												format, 
												argPtr );
}  


/**
 *	This method sets up error handling, by routing error msgs
 *	to our own static function
 *
 *	@see messageHandler
 */
bool MeShell::initErrorHandling()
{
	MF_WATCH( "debug/filterThreshold",
				DebugFilter::instance(),
				MF_ACCESSORS( int, DebugFilter, filterThreshold ) );

	DebugFilter::instance().addMessageCallback( &debugMessageCallback_ );
	return true;
}


// -----------------------------------------------------------------------------
// Section: Graphics
// -----------------------------------------------------------------------------

/**
 *	This method initialises the graphics subsystem.
 *
 *	@param hInstance	The HINSTANCE variable for the application.
 *	@param hWnd			The HWND variable for the application's 3D view window.
 *
 * @return true if initialisation went well
 */
bool MeShell::initGraphics()
{
	// Read render surface options.
	uint32 width = Options::getOptionInt( "graphics/width", 1024 );
	uint32 height = Options::getOptionInt( "graphics/height", 768 );
	uint32 bpp = Options::getOptionInt( "graphics/bpp", 32 );
	bool fullScreen = Options::getOptionBool( "graphics/fullScreen", false );

	const Moo::DeviceInfo& di = Moo::rc().deviceInfo( 0 );

	uint32 modeIndex = 0;
	bool foundMode = false;

	// Go through available modes and try to match a mode from the options file.
	while( foundMode != true && 
		modeIndex != di.displayModes_.size() )
	{
		if( di.displayModes_[ modeIndex ].Width == width &&
			di.displayModes_[ modeIndex ].Height == height )
		{
			if( bpp == 32 &&
				( di.displayModes_[ modeIndex ].Format == D3DFMT_R8G8B8 ||
				di.displayModes_[ modeIndex ].Format == D3DFMT_X8R8G8B8 ) )
			{
				foundMode = true;
			}
			else if( bpp == 16 && 
				( di.displayModes_[ modeIndex ].Format == D3DFMT_R5G6B5 ||
				di.displayModes_[ modeIndex ].Format == D3DFMT_X1R5G5B5 ) )
			{
				foundMode = true;
			}
		}
		if (!foundMode)
			modeIndex++;
	}

	// If the mode could not be found. Set windowed and mode 0.
	if (!foundMode)
	{
		modeIndex = 0;
		fullScreen = false;
	}
	
	// Read shadow options.
	bool useShadows = Options::getOptionBool( "graphics/shadows", false );

	// Initialise the directx device with the settings from the options file.
	if (!Moo::rc().createDevice( hWndGraphics_, 0, modeIndex, !fullScreen, useShadows ))
	{
		CRITICAL_MSG( "MeShell:initApp: Moo::rc().createDevice() FAILED\n" );
		return false;
	}

	if ( Moo::rc().device() )
	{
		Moo::VisualChannel::initChannels();
		::ShowCursor( true );
	}
	else
	{
		CRITICAL_MSG( "MeShell:initApp: Moo::rc().device() FAILED\n" );
		return false;
	}

	//Use no fogging...
	Moo::rc().fogNear( 5000.f );
	Moo::rc().fogFar( 10000.f );

	//Load Ashes
    SimpleGUI::instance().hwnd( hWndGraphics_ );
    
   	// Hide the 3D window to avoid it turning black from the clear device in
	// the following method
	::ShowWindow( hWndGraphics_, SW_HIDE );

	FontManager::instance().preCreateAllFonts();

	::ShowWindow( hWndGraphics_, SW_SHOW );

	return true;
}


/**
 *	This method finalises the graphics sub system.
 */
void MeShell::finiGraphics()
{
	Moo::VertexDeclaration::fini();

	Moo::rc().releaseDevice();
}



// -----------------------------------------------------------------------------
// Section: Scripts
// -----------------------------------------------------------------------------

/**
 * This method initialises the scripting environment
 *
 * @param hInstance the HINSTANCE variable for the application
 * @param hWnd the HWND variable for the application's main window
 *
 * @return true if initialisation went well
 */
bool MeShell::initScripts()
{
	if (!Scripter::init( BWResource::instance().rootSection() ))
	{
		CRITICAL_MSG( "MeShell::initScripts: failed\n" );
		return false;
	}

	return true;
}


/**
 * This method finalises the scripting environment
 */
void MeShell::finiScripts()
{
	Scripter::fini();
}

POINT MeShell::currentCursorPosition() const
{
	POINT pt;
	::GetCursorPos( &pt );
	::ScreenToClient( hWndGraphics_, &pt );

	return pt;
}


// -----------------------------------------------------------------------------
// Section: Consoles
// -----------------------------------------------------------------------------

/**
 *	This method initialises the consoles.
 */
bool MeShell::initConsoles()
{
	// Initialise the consoles
	ConsoleManager & mgr = ConsoleManager::instance();

	XConsole * pStatusConsole = new XConsole();
	XConsole * pStatisticsConsole =
		new StatisticsConsole( &EngineStatistics::instance() );

	mgr.add( pStatisticsConsole,		"Default",	KeyEvent::KEY_F5 );
	//	The line below is commented until someone finds out the reason of its existence
	//	mgr.add( new XConsole(),			"Special",	KeyEvent::KEY_F5, MODIFIER_CTRL );
	mgr.add( new DebugConsole(),		"Debug",	KeyEvent::KEY_F7 );
	mgr.add( new PythonConsole(),		"Python",	KeyEvent::KEY_P, MODIFIER_CTRL );
	mgr.add( pStatusConsole,			"Status" );

	pStatusConsole->setConsoleColour( 0xFF26D1C7 );
	pStatusConsole->setScrolling( true );
	pStatusConsole->setCursor( 0, 20 );

	return true;
}


// -----------------------------------------------------------------------------
// Section: Miscellaneous
// -----------------------------------------------------------------------------


/**
 *	This static function implements the callback that will be called for each
 *	*_MSG.
 */
bool MeShell::messageHandler( int componentPriority,
		int messagePriority,
		const char * format, va_list argPtr )
{
	if ( componentPriority >= DebugFilter::instance().filterThreshold() &&
		messagePriority == MESSAGE_PRIORITY_ERROR )
	{
		bool fullScreen = !Moo::rc().windowed();

		//don't display message boxes in full screen mode.
		char buf[2*BUFSIZ];
		vsprintf( buf, format, argPtr );

		if ( DebugMsgHelper::showErrorDialogs() && !fullScreen )
		{
			::MessageBox( AfxGetApp()->m_pMainWnd->GetSafeHwnd(),
				buf, "Error", MB_ICONERROR | MB_OK );
		}
	}
	return false;
}

/**
 *	Output streaming operator for MeShell.
 */
std::ostream& operator<<(std::ostream& o, const MeShell& t)
{
	o << "MeShell\n";
	return o;
}
