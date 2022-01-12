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
#include "pe_shell.hpp"
#include "floor.hpp"
#include "pe_scripter.hpp"
#include "appmgr/application_input.hpp"
#include "appmgr/module_manager.hpp"
#include "appmgr/options.hpp"
#include "ashes/simple_gui.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_space.hpp"
#include "common/romp_harness.hpp"
#include "common/tools_camera.hpp"
#include "gizmo/tool.hpp"
#include "input/input.hpp"
#include "moo/render_context.hpp"
#include "moo/visual_channels.hpp"
#include "resmgr/bwresource.hpp"
#include "romp/console_manager.hpp"
#include "romp/console.hpp"
#include "romp/engine_statistics.hpp"
#include "guimanager/gui_input_handler.hpp"

DECLARE_DEBUG_COMPONENT2( "Shell", 0 )

PeShell * PeShell::s_instance_ = NULL;

// need to define some things for the libs we link to..
// for ChunkManager
std::string g_specialConsoleString = "";
// for network
const char * compileTimeString = __TIME__ " " __DATE__;
// for gizmos
void findRelevantChunks(class SmartPointer<class Tool>, float buffer = 0.f) { ;}

PeShell::PeShell() : 
	hInstance_(NULL), 
	hWndApp_(NULL), 
	hWndGraphics_(NULL), 
	romp_(NULL), 
	camera_(NULL),
	floor_(NULL)
{
    ASSERT(s_instance_ == NULL);
    s_instance_ = this;
}

PeShell::~PeShell()
{
    ASSERT(s_instance_);
    DebugFilter::instance().deleteMessageCallback( &debugMessageCallback_ );
    s_instance_ = NULL;
}

/*static*/ PeShell &PeShell::instance() 
{ 
    ASSERT(s_instance_); 
    return *s_instance_; 
}

/*static*/ bool PeShell::hasInstance() 
{ 
    return s_instance_ != NULL; 
}

bool PeShell::initApp( HINSTANCE hInstance, HWND hWndInput, HWND hWndGraphics )
{   
    return PeShell::instance().init( hInstance, hWndInput, hWndGraphics );
}


/**
 *  This method intialises global resources for the application.
 *
 *  @param hInstance    The HINSTANCE variable for the application.
 *  @param hWnd         The HWND variable for the application's main window.
 *  @param hWndGraphics The HWND variable the 3D device will use.
 *
 *  @return     True if initialisation succeeded, otherwise false.
 */
bool
PeShell::init( HINSTANCE hInstance, HWND hWndInput, HWND hWndGraphics )
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
        ERROR_MSG( "Error initializing input devices.\n" );
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

    // romp needs chunky
    {
        ChunkManager::instance().init();
        ChunkSpacePtr space = new ChunkSpace(1);

        std::string spacePath = Options::getOptionString( "space" );
        ChunkDirMapping* mapping = space->addMapping( SpaceEntryID(), Matrix::identity, spacePath );
        if (!mapping)
        {
            ERROR_MSG( "Couldn't map path %s as a space.\n", spacePath.c_str() );
            return false;
        }

        ChunkManager::instance().camera( Matrix::identity, space );

        // Call tick to give it a chance to load the outdoor seed chunk, before
        // we ask it to load the dirty chunks
        ChunkManager::instance().tick( 0.f );

        // We don't want our space going anywhere
        space->incRef();
    }

    if (!initRomp())
    {
        return false;
    }

    if (!initCamera())
    {
        return false;
    }

    floor_ = new Floor(Options::getOptionString("settings/floorTexture", "" ));

    ApplicationInput::disableModeSwitch();

    return true;
}


bool PeShell::initCamera()
{
    camera_ = new ToolsCamera();
    camera_->windowHandle( hWndGraphics_ );
    camera_->maxZoomOut(200.0f);
    std::string speedName = Options::getOptionString( "camera/speed" );
    camera_->speed( Options::getOptionFloat( "camera/speed/" + speedName ) );
    camera_->turboSpeed( Options::getOptionFloat( "camera/speed/" + speedName + "/turbo" ) );
    camera_->mode( Options::getOptionInt( "camera/mode", 0 ) );
    camera_->invert( !!Options::getOptionInt( "camera/invert", 0 ) );
    camera_->rotDir( Options::getOptionInt( "camera/rotDir", -1 ) );
    camera_->view( Options::pRoot()->readMatrix34(
        "startup/lastView",
        camera_->view() ));

    return true;
}

bool PeShell::initRomp()
{
    if ( !romp_ )
    {
        romp_ = new RompHarness;

#ifdef PY_ENABLE
        // set it into the BigBang module
        PyObject * pMod = PyImport_AddModule( "BigBang" );  // borrowed
        PyObject_SetAttrString( pMod, "romp", romp_ );
#endif // PY_ENABLE

        if ( !romp_->init() )
        {
            ERROR_MSG( "ROMP Failed to initialize\n" );
            return false;
        }

		romp_->enviroMinder().activate();
    }
    return true;
}


/**
 *  This method finalises the application. All stuff done in initApp is undone.
 *  @todo make a better sentence
 */
void PeShell::fini()
{
	if ( romp_ )
		romp_->enviroMinder().deactivate();                                       

	ChunkManager::instance().fini();

    PeShell::finiGraphics();

    PeShell::finiScripts();

    // Kill winsock
    //WSACleanup();
}

HINSTANCE &PeShell::hInstance() 
{ 
    return hInstance_; 
}

HWND &PeShell::hWndApp() 
{ 
    return hWndApp_; 
}

HWND &PeShell::hWndGraphics() 
{ 
    return hWndGraphics_; 
}

RompHarness & PeShell::romp() 
{ 
    return *romp_; 
}

ToolsCamera &PeShell::camera() 
{ 
    return *camera_; 
}

Floor& PeShell::floor()
{
	return *floor_;
}

bool PeShellDebugMessageCallback::handleMessage( int componentPriority,
                                                int messagePriority, 
                                                const char * format, 
                                                va_list argPtr )                           
{
    return PeShell::instance().messageHandler( componentPriority, 
                                                messagePriority, 
                                                format, 
                                                argPtr );
}  


/**
 *  This method sets up error handling, by routing error msgs
 *  to our own static function
 *
 *  @see messageHandler
 */
bool PeShell::initErrorHandling()
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
 *  This method initialises the graphics subsystem.
 *
 *  @param hInstance    The HINSTANCE variable for the application.
 *  @param hWnd         The HWND variable for the application's 3D view window.
 *
 * @return true if initialisation went well
 */
bool PeShell::initGraphics()
{
    // Read render surface options.
    uint32 width = Options::pRoot()->readInt( "graphics/width", 1024 );
    uint32 height = Options::pRoot()->readInt( "graphics/height", 768 );
    uint32 bpp = Options::pRoot()->readInt( "graphics/bpp", 32 );
    bool fullScreen = Options::pRoot()->readBool( "graphics/fullScreen", false );

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
    bool useShadows = Options::pRoot()->readBool( "graphics/shadows", false );

    //for (uint32 i = 0; i < Moo::rc().nDevices(); i++)
	//{
	//    if (std::string(Moo::rc().deviceInfo(i).identifier_.Description) == std::string("NVIDIA NVPerfHUD"))
	//    {
	//         modeIndex = i;
	//	  }
	//} 

    // Initialise the directx device with the settings from the options file.
    if (!Moo::rc().createDevice( hWndGraphics_, 0, modeIndex, !fullScreen, useShadows ))
    {
        CRITICAL_MSG( "Creation of DirectX device failed\n" );
        return false;
    }

    if ( Moo::rc().device() )
    {
        Moo::VisualChannel::initChannels();
        ::ShowCursor( true );
    }
    else
    {
        CRITICAL_MSG( "Unable to use DirectX device\n" );
        return false;
    }

    //We don't need no fog man!
    Moo::rc().fogNear( 5000.f );
    Moo::rc().fogFar( 10000.f );

    //ASHES
    SimpleGUI::instance().hwnd( hWndGraphics_ );

	// Hide the 3D window to avoid it turning black from the clear device in
	// the following method
	::ShowWindow( hWndGraphics_, SW_HIDE );

	FontManager::instance().preCreateAllFonts();

	::ShowWindow( hWndGraphics_, SW_SHOW );

    // camera


    return true;
}


/**
 *  This method finalises the graphics sub system.
 */
void PeShell::finiGraphics()
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
bool PeShell::initScripts()
{
    if (!Scripter::init( BWResource::instance().rootSection() ))
    {
        ERROR_MSG( "Scripting failed to initialize\n" );
        return false;
    }

    return true;
}


/**
 * This method finalises the scripting environment
 */
void PeShell::finiScripts()
{
    Scripter::fini();
}

POINT PeShell::currentCursorPosition() const
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
 *  This method initialises the consoles.
 */
bool PeShell::initConsoles()
{
    // Initialise the consoles
    ConsoleManager & mgr = ConsoleManager::instance();

    XConsole * pStatusConsole = new XConsole();
    XConsole * pStatisticsConsole =
        new StatisticsConsole( &EngineStatistics::instance() );

    mgr.add( pStatisticsConsole,        "Default",  KeyEvent::KEY_F5 );
	//	The line below is commented until someone finds out the reason of its existence
	//	mgr.add( new XConsole(),            "Special",  KeyEvent::KEY_F5, MODIFIER_CTRL );
    mgr.add( new DebugConsole(),        "Debug",    KeyEvent::KEY_F7 );
    mgr.add( new PythonConsole(),       "Python",   KeyEvent::KEY_P, MODIFIER_CTRL );
    mgr.add( pStatusConsole,            "Status" );

    pStatusConsole->setConsoleColour( 0xFF26D1C7 );
    pStatusConsole->toggleFontScale( );
    pStatusConsole->setScrolling( true );
    pStatusConsole->setCursor( 0, 20 );

    return true;
}


// -----------------------------------------------------------------------------
// Section: Miscellaneous
// -----------------------------------------------------------------------------


/**
 *  This static function implements the callback that will be called for each
 *  *_MSG.
 */
bool PeShell::messageHandler( int componentPriority,
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
            AfxMessageBox(buf, MB_ICONERROR | MB_OK);
        }
    }
    return false;
}

/**
 *  Output streaming operator for PeShell.
 */
std::ostream& operator<<(std::ostream& o, const PeShell& t)
{
    o << "PeShell\n";
    return o;
}
