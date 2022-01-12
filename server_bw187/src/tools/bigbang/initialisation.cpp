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
#include "initialisation.hpp"

#include "appmgr/module_manager.hpp"
#include "appmgr/options.hpp"

#include "terrain/terrain_editor_script.hpp"
#include "big_bang.hpp"

#include "input/input.hpp"
#include "moo/render_context.hpp"
#include "moo/visual_channels.hpp"
#include "resmgr/bwresource.hpp"

#include "romp/console_manager.hpp"
#include "romp/console.hpp"
#include "romp/engine_statistics.hpp"
#include "romp/sky_light_map.hpp"
#include "romp/frame_logger.hpp"

#include "ashes/simple_gui.hpp"

#include "xactsnd/soundmgr.hpp"
#include "network/endpoint.hpp"

#include <WINSOCK.H>


#ifndef CODE_INLINE
#include "initialisation.ipp"
#endif

DECLARE_DEBUG_COMPONENT2( "App", 0 )

HINSTANCE Initialisation::s_hInstance = NULL;
HWND Initialisation::s_hWndInput = NULL;
HWND Initialisation::s_hWndGraphics = NULL;


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
Initialisation::initApp( HINSTANCE hInstance, HWND hWndInput, HWND hWndGraphics )
{
	s_hInstance = hInstance;
	s_hWndInput = hWndInput;
	s_hWndGraphics = hWndGraphics;

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
		ERROR_MSG( "Initialisation::initApp: Init inputDevices FAILED\n" );
		return false;
	}

	if (!Initialisation::initGraphics(hInstance, hWndGraphics))
	{
		return false;
	}
	// Init winsock
	initNetwork();

	if (!Initialisation::initScripts())
	{
		return false;
	}

	if (!Initialisation::initConsoles())
	{
		return false;
	}

	/*if (!SoundMgr::instance().init( s_hWndInput ))
	{
		return false;
	}*/

	if ( !BigBang::instance().init( s_hInstance, s_hWndInput, s_hWndGraphics ) )
	{
		return false;
	}

	return true;
}


/**
 *	This method finalises the application. All stuff done in initApp is undone.
 *	@todo make a better sentence
 */
void Initialisation::finiApp()
{
	BigBang::instance().fini();

	Initialisation::finaliseGraphics();

	Initialisation::finaliseScripts();

	// Kill winsock
	WSACleanup();
}


/**
 *	This method sets up error handling, by routing error msgs
 *	to our own static function
 *
 *	@see messageHandler
 */
bool Initialisation::initErrorHandling()
{
	MF_WATCH( "debug/filterThreshold",
				DebugFilter::instance(),
				MF_ACCESSORS( int, DebugFilter, filterThreshold ) );

	DebugFilter::instance().addMessageCallback( BigBang::instance().getDebugMessageCallback() );
	return true;
}


// -----------------------------------------------------------------------------
// Section: Graphics
// -----------------------------------------------------------------------------

/**
 *	This method initialises the graphics subsystem.
 *
 *	@param hInstance	The HINSTANCE variable for the application.
 *	@param hWnd			The HWND variable for the application's main window.
 *
 * @return true if initialisation went well
 */
bool Initialisation::initGraphics( HINSTANCE hInstance, HWND hWnd )
{
	// Read render surface options.
	bool fullScreen = false;
	uint32 modeIndex = 0;

	// Read shadow options.
	bool useShadows = Options::getOptionBool( "graphics/shadows", false );

	// Initialise the directx device with the settings from the options file.
	if (!Moo::rc().createDevice( hWnd, 0, modeIndex, !fullScreen, useShadows ))
	{
		CRITICAL_MSG( "Initialisation:initApp: Moo::rc().createDevice() FAILED\n" );
		return false;
	}
	Moo::VisualChannel::initChannels();

	//We don't need no fog man!
	Moo::rc().fogNear( 5000.f );
	Moo::rc().fogFar( 10000.f );
    
	//ASHES
    SimpleGUI::instance().hwnd( hWnd );

	// Hide the 3D window to avoid it turning black from the clear device in
	// the following method
	::ShowWindow( hWnd, SW_HIDE );

	FontManager::instance().preCreateAllFonts();

	::ShowWindow( hWnd, SW_SHOW );

	return true;
}


/**
 *	This method finalises the graphics sub system.
 */
void Initialisation::finaliseGraphics()
{
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
bool Initialisation::initScripts()
{
	// as a test, nuke the PYTHONPATH
	_putenv( "PYTHONPATH=" );

	if (!BigBangScript::init( BWResource::instance().rootSection() ))
	{
		CRITICAL_MSG( "Initialisation::initScripts: BigBangScript::init() failed\n" );
		return false;
	}

	return true;
}


/**
 * This method finalises the scripting environment
 */
void Initialisation::finaliseScripts()
{
	BigBangScript::fini();
}


// -----------------------------------------------------------------------------
// Section: Consoles
// -----------------------------------------------------------------------------

/**
 *	This method initialises the consoles.
 */
bool Initialisation::initConsoles()
{
	// Initialise the consoles
	ConsoleManager & mgr = ConsoleManager::instance();

	XConsole * pStatusConsole = new XConsole();
	XConsole * pStatisticsConsole =
		new StatisticsConsole( &EngineStatistics::instance() );

	mgr.add( new HistogramConsole(),	"Histogram",		KeyEvent::KEY_F11 );
	mgr.add( pStatisticsConsole,		"Default",	KeyEvent::KEY_F5 );
	//	The line below is commented until someone finds out the reason of its existence
	//	mgr.add( new XConsole(),			"Special",	KeyEvent::KEY_F5, MODIFIER_CTRL );
	mgr.add( new DebugConsole(),		"Debug",	KeyEvent::KEY_F7 );
	mgr.add( new PythonConsole(),		"Python",	KeyEvent::KEY_P, MODIFIER_CTRL );
	mgr.add( pStatusConsole,			"Status" );

	pStatusConsole->setConsoleColour( 0xFF26D1C7 );
	pStatusConsole->setScrolling( true );
	pStatusConsole->setCursor( 0, 20 );

#if ENABLE_DOG_WATCHERS
	FrameLogger::init();
#endif

	return true;
}


// -----------------------------------------------------------------------------
// Section: Miscellaneous
// -----------------------------------------------------------------------------

/**
 *	Output streaming operator for Initialisation.
 */
std::ostream& operator<<(std::ostream& o, const Initialisation& t)
{
	o << "Initialisation\n";
	return o;
}

// initialisation.cpp
