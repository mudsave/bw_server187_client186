/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

/**
 *
 * The main application class
 */

#include "pch.hpp"
#include "app.hpp"

#include "ashes/simple_gui.hpp"
#include "cstdmf/debug.hpp"
#include "cstdmf/timestamp.hpp"
#include "moo/render_context.hpp"
#include "moo/animating_texture.hpp"
#include "moo/effect_visual_context.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/auto_config.hpp"
#include "romp/console_manager.hpp"
#include "romp/engine_statistics.hpp"
#include "romp/geometrics.hpp"
#include "romp/lens_effect_manager.hpp"
#include "moo/effect_visual_context.hpp"

#include "romp/flora.hpp"
#include "romp/enviro_minder.hpp"

#include "dev_menu.hpp"
#include "module_manager.hpp"
#include "options.hpp"

extern void setupTextureFeedPropertyProcessors();

static AutoConfigString s_blackTexture( "system/blackBmp" );
static AutoConfigString s_notFoundTexture( "system/notFoundBmp" );
static AutoConfigString s_notFoundModel( "system/notFoundModel" );	
static AutoConfigString s_graphicsSettingsXML("system/graphicsSettingsXML");

DECLARE_DEBUG_COMPONENT2( "App", 0 );

static DogWatch g_watchDrawConsole( "Draw Console" );
static DogWatch g_watchPreDraw( "Predraw" );

static DogWatch g_watchEndScene( "EndScene" );
static DogWatch g_watchPresent( "Present" );
static DogWatch g_watchResetPrimitiveCount( "ResetPrimitiveCount" );

// -----------------------------------------------------------------------------
// Section: Construction/Destruction
// -----------------------------------------------------------------------------

/**
 *	The constructor for the App class. App::init needs to be called to fully
 *	initialise the application.
 *
 *	@see App::init
 */
App::App() :
	hWndInput_( NULL ),
	lastTime_(0),
    paused_(false),
	maxTimeDelta_( 0.5f ),
	timeScale_( 1.f ),
	bInputFocusLastFrame_( false )
{
}


/**
 *	Destructor.
 */
App::~App()
{
}

/**
 *	This method initialises the application.
 *
 *	@param hInstance	The HINSTANCE associated with this application.
 *	@param hWnd			The main window associated with this application.
 *	@param hWndGraphics	The window associated with the 3D device
 *
 *	@return		True if the initialisation succeeded, false otherwise.
 */
bool App::init( HINSTANCE hInstance, HWND hWndInput, HWND hWndGraphics,
			   bool ( *userInit )( HINSTANCE, HWND, HWND ) )
{
	char buf[1024];

	if ( ::GetCurrentDirectory( 1024, buf ) )
	{
		BWResource::instance().addPath( buf );
	}
	else
	{
		CRITICAL_MSG( "Could not find working directory.  terminating\n" );
		return false;
	}


	hWndInput_ = hWndInput;
	maxTimeDelta_ = Options::getOptionFloat( "app/maxTimeDelta", 
		maxTimeDelta_ );
	timeScale_ = Options::getOptionFloat( "app/timeScale", timeScale_ );
	
	bool darkBackground = Options::getOptionBool( "consoles/darkenBackground", false );
	ConsoleManager::instance().setDarkenBackground(darkBackground);

	MF_WATCH( "App/maxTimeDelta", maxTimeDelta_, Watcher::WT_READ_WRITE, "Limits the delta time passed to application tick methods." );
	MF_WATCH( "App/timeScale", timeScale_, Watcher::WT_READ_WRITE, "Multiplies the delta time passed to application tick methods." );


	// Resource glue initialisation
	if (!AutoConfig::configureAllFrom("resources.xml"))
	{
		CRITICAL_MSG( "Unable to find the required file resources.xml at the root of your resource tree.\n"
			"Check whether either the \"BW_RES_PATH\" environmental variable or the \"paths.xml\" files are incorrect." );
		return false;
	}	

	//init the texture feed instance, this registers material section
	//processors.
	setupTextureFeedPropertyProcessors();

	//Initialise the GUI
	SimpleGUI::instance().init( NULL );

	if (userInit)
	{
		if ( !userInit( hInstance, hWndInput, hWndGraphics ) )
			return false;
	}

	if (!ModuleManager::instance().init(
		BWResource::openSection( "resources/data/modules.xml" ) ))
	{
		return false;
	}

	//Check for startup modules
	DataSectionPtr spSection =
		BWResource::instance().openSection( "resources/data/modules.xml/startup");

	std::vector< std::string > startupModules;

	if ( spSection )
	{
		spSection->readStrings( "module", startupModules );
	}

	//Now, create all startup modules.
	std::vector<std::string>::iterator it = startupModules.begin();
	std::vector<std::string>::iterator end = startupModules.end();

	while ( it != end )
	{
		ModuleManager::instance().push( (*it++) );
	}

	DataSectionPtr pSection = BWResource::instance().openSection( s_graphicsSettingsXML );

	// setup far plane options
	DataSectionPtr farPlaneOptionsSec = pSection->openSection("farPlane");
	EnviroMinderSettings::instance().init( farPlaneOptionsSec );

	// setup far flora options
	DataSectionPtr floraOptionsSec = pSection->openSection("flora");
	FloraSettings::instance().init( floraOptionsSec );

	//BUG 4252 FIX: Make sure we start up with focus to allow navigation of view.
	handleSetFocus( true );

	return true;
}


/**
 *	This method finalises the application.
 */
void App::fini()
{
	LensEffectManager::instance().finz();
	ModuleManager::instance().popAll();
	SimpleGUI::instance().fini();
}

// -----------------------------------------------------------------------------
// Section: Framework
// -----------------------------------------------------------------------------

/**
 *	This method calculates the time between this frame and last frame.
 */
float App::calculateFrameTime()
{
	uint64 thisTime = timestamp();

	float dTime = (float)(((int64)(thisTime - lastTime_)) / stampsPerSecondD());

	if (dTime > maxTimeDelta_)
	{
		dTime = maxTimeDelta_;
	}

	dTime *= timeScale_;
	lastTime_ = thisTime;

    if (paused_)
        dTime = 0.0f;

	return dTime;
}

/**
 *  This method allows pausing.  The time between pausing and unpausing is,
 *  as far as rendering is concerned, zero.
 */
void App::pause(bool paused)
{
    // If we are unpausing, update the current time so that there isn't a 
    // 'jump'.
    if (paused != paused_ && !paused)
    {
        lastTime_ = timestamp();
    }
    paused_ = paused;
}

/**
 *  Are we paused?
 */
bool App::isPaused() const
{
    return paused_;
}

/**
 *	This method is called once per frame to do all the application processing
 *	for that frame.
 */
void App::updateFrame( bool tick )
{
	g_watchPreDraw.start();
	
	float dTime = this->calculateFrameTime();
	if (!tick) dTime = 0.f;

	if ( InputDevices::hasFocus() )
	{
		if ( bInputFocusLastFrame_ )
		{
			InputDevices::processEvents( this->inputHandler_ );
		}
		else
		{
			InputDevices::processEvents( this->nullInputHandler_ );
			bInputFocusLastFrame_ = true;
		}
	}
	else bInputFocusLastFrame_ = false;

	EngineStatistics::instance().tick( dTime );
	Moo::AnimatingTexture::tick( dTime );
	Moo::Material::tick( dTime );
	Moo::EffectVisualContext::instance().tick( dTime );
	Moo::EffectMaterial::finishEffectInits();
	
	ModulePtr pModule = ModuleManager::instance().currentModule();

	g_watchPreDraw.stop();
	
	if (pModule)
	{
        // If the device cannot be used don't bother rendering
	    if (!Moo::rc().checkDevice())
	    {
		    Sleep(100);
		    return;
	    }

		pModule->updateFrame( dTime );

		HRESULT hr = Moo::rc().beginScene();
        if (SUCCEEDED(hr))
        {
		    if (Moo::rc().mixedVertexProcessing())
			    Moo::rc().device()->SetSoftwareVertexProcessing( TRUE );
		    Moo::rc().nextFrame();

		    pModule->render( dTime );

		    g_watchDrawConsole.start();
		    if (ConsoleManager::instance().pActiveConsole() != NULL)
		    {
			    ConsoleManager::instance().draw( dTime );
		    }
		    g_watchDrawConsole.stop();
    		
		    // End rendering
		    g_watchEndScene.start();
		    Moo::rc().endScene();
		    g_watchEndScene.stop();
        }

		g_watchPresent.start();
		if (Moo::rc().device()->Present( NULL, NULL, NULL, NULL ) != D3D_OK)
		{
			DEBUG_MSG( "An error occured while presenting the new frame.\n" );
		}
		g_watchPresent.stop();

		g_watchResetPrimitiveCount.start();
		Moo::rc().resetPrimitiveCount();
		g_watchResetPrimitiveCount.stop();

		
	}
	else
	{
		//TEMP! WindowsMain::shutDown();
	}
}

/**
 *	flush input queue
 */

void App::consumeInput()
{
	InputDevices::processEvents( this->nullInputHandler_ );
}

/**
 *	This method is called when the window has resized. It is called in response
 *	to the Windows event.
 */
void App::resizeWindow()
{
	if (Moo::rc().windowed())
	{
		Moo::rc().changeMode( Moo::rc().modeIndex(), Moo::rc().windowed() );
	}
}


/**
 *	This method is called when the application gets the focus.
 */
void App::handleSetFocus( bool state )
{
	//DEBUG_MSG( "App::handleSetFocus: %d\n", state );

	InputDevices::setFocus( state );
}

// app.cpp
