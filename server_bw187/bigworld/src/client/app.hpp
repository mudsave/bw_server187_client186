/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef APP_HPP
#define APP_HPP

// identifier was truncated to '255' characters in the browser information
#pragma warning(disable: 4786)
#pragma warning(disable: 4503)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
#include <queue>
#include <map>
#include <vector>
#include <list>

#include "cstdmf/stringmap.hpp"
#include "pyscript/script.hpp"

#include "math/angle.hpp"
#include "math/vector3.hpp"
#include "math/matrix.hpp"

#include "input/input.hpp"


// Forward declarations
class ProgressDisplay;
class ProgressTask;
class EnviroMinder;

class PhotonOccluder;
class EntityPhotonOccluder;

class BaseCamera;
class ProjectionAccess;

class FrameWriter;
class ServerConnection;
class InputCursor;
class VersionInfo;



/**
 *	This singleton class is the highest level of the application
 */
class App : public InputHandler
{
public:
	class InitError : public std::runtime_error 
	{
	public:
		InitError( const char* what ) :
			std::runtime_error( what )
		{}
	};
	
	App( const std::string & configFilename );
	~App();

	// Init method to be called first
	bool init( HINSTANCE hInstance, HWND hWnd );	

	// Main loop method called by our owner
	bool updateFrame(bool active);

	// Make us quit
	void quit( bool restart = false );

	// Overrides from InputHandler
	virtual bool handleKeyEvent( const KeyEvent & event );
	virtual bool handleMouseEvent( const MouseEvent & event );
	virtual bool handleAxisEvent( const AxisEvent & event );
	
	InputCursor * activeCursor();
	void activeCursor( InputCursor * handler );

	bool savePreferences();

	// Windows messages
	void resizeWindow();
	void resizeWindow(int width, int height);
	const POINT & windowSize() const;
	
	void moveWindow( int16 x, int16 y );
	void handleSetFocus( bool focusState );
	void setWindowTitleNote( int pos, const std::string & note );

	// Time
	float getTimeDelta( void ) const;
	double getTime( void ) const;

	void checkPython();
	HWND hWnd()	{ return hWnd_; }

	// Camera Methods
	SmartPointer<BaseCamera> camera();
	void camera( SmartPointer<BaseCamera> pCamera );

	ProjectionAccess * projAccess();


	/// Singleton instance accessor
	static App & instance()		{ return *pInstance_; }

	PY_MODULE_STATIC_METHOD_DECLARE( py_setCursor )

private:

	// Init methods
	void runPreloads( ProgressTask & task );

	// Update methods
	void updateScene( float dTime );
	void updateCameras( float dTime );

	// Render methods
	void renderFrame();
	void drawWorld();
	void drawScene();

	// Event handlers
	void handleInputPlayback();
	bool handleKeyDown( const KeyEvent & event );
	bool handleDebugKeyDown( const KeyEvent & event );


	enum eEventDestination
	{
		EVENT_SINK_NONE = 0,
		EVENT_SINK_CONSOLE,
		EVENT_SINK_SCRIPT,
		EVENT_SINK_CAMERA,
		EVENT_SINK_APP,
		EVENT_SINK_DEBUG,
		EVENT_SINK_PERSONALITY
	};

	eEventDestination	keyRouting_[KeyEvent::NUM_KEYS];

	// Helper methods
	void calculateFrameTime();

	// window handle
	HWND hWnd_;

	// Frame statistics
	float fps_;
	float dTime_;
	uint64 lastTime_;
	uint64 lastFrameEndTime_;
	uint64 minFrameTime_;
	double totalTime_;
	float minimumFrameRate_;

	static App *		pInstance_;

	typedef std::map<int,std::string> TitleNotes;
	TitleNotes			titleNotes_;

	bool			debugKeyEnable_;
	InputCursor *	activeCursor_;
	POINT           windowSize_;

	friend class DeviceApp;
	typedef App This;
};

bool isCameraOutside();
bool isPlayerOutside();

#ifdef CODE_INLINE
#include "app.ipp"
#endif


#endif // APP_HPP
