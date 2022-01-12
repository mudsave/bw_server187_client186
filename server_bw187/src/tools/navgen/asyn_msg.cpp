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
#include "asyn_msg.hpp"
#include "resource.h"
#include <process.h>

static volatile HWND hwnd = NULL;
static char logFileName[1024];
FILE * logFile = NULL;

static const UINT WM_ADDERROR = WM_USER + 0x345;

INT_PTR CALLBACK DialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	BOOL result = TRUE;
	switch( uMsg )
	{
	case WM_INITDIALOG:
		hwnd = hwndDlg;
		ShowWindow( hwnd, SW_HIDE );
		if( logFile == NULL )
			AsyncMessage().reportMessage( 
				( std::string( "cannot create log file " ) + logFileName ).c_str(), true );
		break;
	case WM_CLOSE:
		ShowWindow( hwndDlg, SW_HIDE );
		break;
	case WM_DESTROY:
		PostQuitMessage( 0 );
		break;
	case WM_ADDERROR:
		{
			std::string str = (char*)wParam;
			free( (char*)wParam );
			wParam = 0;
			// remove newline and carriage return characters
			std::replace( str.begin(), str.end(), '\r', ' ' );
			std::replace( str.begin(), str.end(), '\n', ' ' );
			HWND message = GetDlgItem( hwndDlg, IDC_MESSAGELIST );
			SendMessage( message, LB_SETTOPINDEX,
				SendMessage( message, LB_ADDSTRING, 0, (LPARAM)str.c_str() ), 0 );
		}
		break;
	default:
		result = FALSE;
	}
	return result;
}

void AsyncMessageThread( LPVOID )
{
	// get the log file name
	GetModuleFileName( NULL, logFileName, sizeof( logFileName ) );
	char* end = logFileName + strlen( logFileName ) - 1;
	while( *end != '.' )
		--end;
	strcpy( end + 1, "log" );

	// create the log file
	logFile = fopen( logFileName, "a+" );

	// create the error log dialog
	DialogBox( GetModuleHandle( NULL ),
		MAKEINTRESOURCE( IDD_MESSAGEDIALOG ), NULL /*GetDesktopWindow()*/, DialogProc );

	// close if the file is opened succefully
	if( logFile != NULL )
		fclose( logFile );
}

void AsyncMessage::reportMessage( const char* msg, bool severity )
{
	SYSTEMTIME time;
	GetLocalTime( &time );
	char dateTime[1023];
	sprintf( dateTime, "%02d/%02d/%04d - %02d:%02d:%02d : ",
		time.wMonth, time.wDay, time.wYear,
		time.wHour, time.wMinute, time.wSecond );

	std::string datedMsg = dateTime;
	datedMsg += msg;

	PostMessage( hwnd, WM_ADDERROR,
		(WPARAM)_strdup( datedMsg.c_str() ), 0 );
	if( logFile != NULL )
	{
		fputs( datedMsg.c_str(), logFile );
		fflush( logFile );
	}
	if( !isShow() )
		show();
}

void AsyncMessage::show()
{
	ShowWindow( hwnd, SW_SHOW );
	SetForegroundWindow( hwnd );
}

void AsyncMessage::hide()
{
	ShowWindow( hwnd, SW_HIDE );
}

bool AsyncMessage::isShow() const
{
	return ( GetWindowLong( hwnd, GWL_STYLE ) & WS_VISIBLE ) != 0;
}

const char* AsyncMessage::getLogFileName() const
{
	return logFileName;
}

namespace
{

struct ThreadCreator
{
	ThreadCreator()
	{
		_beginthread( AsyncMessageThread, 0, NULL );
		while( !hwnd )
			Sleep( 1 );
	}
	~ThreadCreator()
	{
		EndDialog( hwnd, 0 );
	}
}
ThreadCreator;

}
