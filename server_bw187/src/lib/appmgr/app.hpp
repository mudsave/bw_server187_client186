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

#define WIN32_LEAN_AND_MEAN

#include <list>
#include <windows.h>
#include <string>
#include "input_manager.hpp"
#include "scripted_module.hpp"


/**
 *	This is a singleton class that is used to represent the entire application.
 */
class App
{
public:
	App();
	~App();

	bool init( HINSTANCE hInstance, HWND hWndInput, HWND hwndGraphics,
		 bool ( *userInit )( HINSTANCE, HWND, HWND ) );
	void fini();
	void updateFrame( bool tick = true );
	void consumeInput();

	HWND hwndInput();

	// Windows messages
	void resizeWindow( );
	void handleSetFocus( bool focusState );

	// Application Tasks
	float	calculateFrameTime();
    void    pause(bool paused);
    bool    isPaused() const;

private:
	// Properties
	HWND				hWndInput_;

	// Timing
	uint64				lastTime_;
    bool                paused_;

	// Input
	InputManager		inputHandler_;
	NullInputManager	nullInputHandler_;
	bool				bInputFocusLastFrame_;

	// General
	float				maxTimeDelta_;
	float				timeScale_;
};

#ifdef CODE_INLINE
#include "app.ipp"
#endif


#endif // APP_HPP
