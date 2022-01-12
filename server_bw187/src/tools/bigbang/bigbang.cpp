/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// bigbang2.cpp : Defines the class behaviors for the application.
//

#include "pch.hpp"
#include "bigbang.h"

#include "mainframe.hpp"

#include "bigbangdoc.hpp"
#include "bigbangview.hpp"

#include "python_adapter.hpp"

#include "appmgr/app.hpp"
#include "appmgr/options.hpp"
#include "appmgr/commentary.hpp"
#include "common/tools_common.hpp"
#include "common/directory_check.hpp"
#include "common/cooperative_moo.hpp"
#include "common/command_line.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_loader.hpp"
#include "chunk/chunk_space.hpp"
#include "gizmo/gizmo_manager.hpp"
#include "gizmo/tool_manager.hpp"
#include "big_bang.hpp"
#include "initialisation.hpp"
#include "compile_time.hpp"
#include <afxdhtml.h>

#include "guimanager/gui_manager.hpp"
#include "guimanager/gui_menu.hpp"
#include "guimanager/gui_toolbar.hpp"
#include "guimanager/gui_functor.hpp"
#include "guimanager/gui_functor_option.hpp"

#include "editor_chunk_particle_system.hpp"
#include "editor_chunk_model.hpp"

#include "common/page_messages.hpp"

#include "panel_manager.hpp"

#include "placement_ctrls_dlg.hpp"

#include "../../controls/message_box.hpp"

DECLARE_DEBUG_COMPONENT2( "BigBang2", 0 )


#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// BigBang2App

BEGIN_MESSAGE_MAP(BigBang2App, CWinApp)
	// Standard file based document commands
	ON_COMMAND(ID_FILE_NEW, CWinApp::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, CWinApp::OnFileOpen)
END_MESSAGE_MAP()


// BigBang2App construction
BigBang2App * BigBang2App::s_instance_ = NULL;

BigBang2App::BigBang2App()
: pythonAdapter_(NULL),
s_mfApp( NULL )
{
	ASSERT(s_instance_ == NULL);
	s_instance_ = this;
}

BigBang2App::~BigBang2App()
{
	ASSERT(s_instance_);
	s_instance_ = NULL;

	if (pythonAdapter_)
		delete pythonAdapter_;

	if( updateMailSlot_ != INVALID_HANDLE_VALUE )
		CloseHandle( updateMailSlot_ );
}



// The one and only BigBang2App object

BigBang2App theApp;

// BigBang2App initialization

#include "appmgr/options.hpp"
#include "resmgr/bwresource.hpp"

bool isRunning( const char* uniqueName )
{
    HANDLE mutex = CreateMutex( NULL, FALSE, uniqueName );
    if (mutex != NULL && GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CloseHandle( mutex );
        return true;
    }

    return (mutex == NULL);
}


bool BigBang2App::panelsReady()
{
	return PanelManager::instance()->ready();
}

BOOL BigBang2App::InitInstance()
{
	// InitCommonControls() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	InitCommonControls();

	CWinApp::InitInstance();

    if (isRunning( "Global\\BigBang" ))
    {
		HWND hwnd = NULL;
		while( hwnd = FindWindowEx( NULL, hwnd, NULL, NULL ) )
		{
			CWnd* wnd = CWnd::FromHandle( hwnd );
			CString hint;
			hint.Format( "Global\\BigBang%d", hwnd );
			HANDLE mutex = OpenMutex( MUTEX_ALL_ACCESS, FALSE, hint );
			if( mutex )
			{
				CloseHandle( mutex );
				if( wnd->IsIconic() )
					wnd->ShowWindow( SW_RESTORE );
				wnd->GetLastActivePopup()->SetForegroundWindow();
				return 1;
			}
		}
		return 1;
    }

	// Check the use-by date
	if (!ToolsCommon::canRun())
	{
		ToolsCommon::outOfDateMessage( "WorldEditor" );
		return FALSE;
	}

	// Initialize OLE libraries
	if (!AfxOleInit())
	{
		AfxMessageBox(IDP_OLE_INIT_FAILED);
		return FALSE;
	}
	AfxEnableControlContainer();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	// of your final executable, you should remove from the following
	// the specific initialization routines you do not need

	SetRegistryKey(_T("BigWorld-WorldEditor"));
	LoadStdProfileSettings(4);  // Load standard INI file options (including MRU)

	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views
	CSingleDocTemplate* pDocTemplate;
	pDocTemplate = new CSingleDocTemplate(
		IDR_MAINFRAME,
		RUNTIME_CLASS(BigBang2Doc),
		RUNTIME_CLASS(MainFrame),       // main SDI frame window
		RUNTIME_CLASS(BigBang2View));
	AddDocTemplate(pDocTemplate);

	// Parse command line for standard shell commands, DDE, file open
	MFCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);

	// initialise the MF file services (read in the command line arguments too)
	if (!parseCommandLineMF())
		return FALSE;

	// Dispatch commands specified on the command line.  Will return FALSE if
	// app was launched with /RegServer, /Register, /Unregserver or /Unregister.
	// This also creates all the GUI windows
	if (!ProcessShellCommand(cmdInfo))
	{
		return FALSE;
	}

	// The one and only window has been initialized, so show and update it
	CString hint;
	hint.Format( "Global\\BigBang%d", m_pMainWnd->m_hWnd );
	isRunning( hint );
	m_pMainWnd->ShowWindow(SW_SHOWMAXIMIZED);
	m_pMainWnd->UpdateWindow();

	// call DragAcceptFiles only if there's a suffix
	//  In an SDI app, this should occur after ProcessShellCommand

	// initialise the MF App components
	ASSERT( !s_mfApp );
	s_mfApp = new App;

	HINSTANCE hInst = AfxGetInstanceHandle();
	MainFrame * mainFrame = (MainFrame *)(m_pMainWnd);
	if (!s_mfApp->init( hInst, m_pMainWnd->m_hWnd,
		mainFrame->GetActiveView()->m_hWnd, 
		Initialisation::initApp ))
	{
		return FALSE;
	}

	// need to load the adapter before the load thread begins, but after the modules
	pythonAdapter_ = new PythonAdapter();

	bool loadStarted = ChunkManager::instance().chunkLoader()->start();
	MF_ASSERT( loadStarted );

	BigBang::instance().postLoadThreadInit();

	// toolbar / menu initialization
	GUI::Manager::instance()->add( new GUI::Menu( "MainMenu", mainFrame->GetSafeHwnd() ) );
	AfxGetMainWnd()->DrawMenuBar();

	// create Toolbars through the BaseMainFrame createToolbars method
	mainFrame->createToolbars( "AppToolbars" );

	PlacementPresets::instance()->init( "resources/data/placement.xml" );

	// GUITABS Tearoff tabs system init and setup
	PanelManager::instance()->initDock( mainFrame, mainFrame->GetActiveView() );
	PanelManager::instance()->initPanels();

	PlacementPresets::instance()->readPresets(); // to fill all placement combo boxes

	updateMailSlot_ = CreateMailslot( "\\\\.\\mailslot\\BigBangUpdate", 0, 0, NULL );

	return TRUE;
}


BOOL BigBang2App::parseCommandLineMF()
{
	DirectoryCheck( "WorldEditor" );
	
	// parse command line
	const int MAX_ARGS = 20;
	const char seps[]   = " \t";
	char * argv[ MAX_ARGS ];
	int argc = 0;

    // add argv[0]: EXE path and name
	char buffer[ MAX_PATH + 1 ];
	GetModuleFileName( NULL, buffer, sizeof( buffer ) );
    argv[ argc++ ] = buffer;

    // parse the arguments
	argv[ argc ] = strtok( m_lpCmdLine, seps );

	while (argv[ argc ] != NULL)
	{
		if (++argc >= MAX_ARGS)
		{
			ERROR_MSG( "BigBang2::parseCommandLineMF: Too many arguments!!\n" );
			return FALSE;
		}

		argv[ argc ] = strtok( NULL, seps );
	}

	return Options::init( argc, argv ) && BWResource::init( argc, argv );
}


// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
	CBitmap mBackground;
	CFont mFont;
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	mBackground.LoadBitmap( IDB_ABOUTBOX );
	mFont.CreatePointFont( 90, "Arial", NULL );
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_LBUTTONDOWN()
	ON_WM_RBUTTONDOWN()
END_MESSAGE_MAP()

class ShortcutsDlg: public CDHtmlDialog
{
public:
	ShortcutsDlg( int ID ): CDHtmlDialog( ID ) {}

	BOOL ShortcutsDlg::OnInitDialog() 
	{
		std::string shortcutsHtml = Options::getOptionString(
			"help/shortcutsHtml",
			"resources/html/shortcuts.html");
		std::string shortcutsUrl = BWResource::resolveFilename( shortcutsHtml );
		CDHtmlDialog::OnInitDialog();
		Navigate( shortcutsUrl.c_str() );
		return TRUE; 
	}

    /*virtual*/ void OnCancel()
    {
        DestroyWindow();
        s_instance = NULL;
    }

    static ShortcutsDlg *instance()
    {
        if (s_instance == NULL)
        {
            s_instance = new ShortcutsDlg(IDD_HTMLDIALOG);
            s_instance->Create(IDD_HTMLDIALOG);
        }
        return s_instance;
    }

    static void cleanup()
    {
        if (s_instance != NULL)
            s_instance->OnCancel();
    }

private:
    static ShortcutsDlg    *s_instance;
};

ShortcutsDlg *ShortcutsDlg::s_instance = NULL;

static struct AppFunctor : GUI::ActionMaker<AppFunctor>
	, GUI::ActionMaker<AppFunctor, 1>
	, GUI::ActionMaker<AppFunctor, 2>
	, GUI::ActionMaker<AppFunctor, 3>
{
	AppFunctor() : GUI::ActionMaker<AppFunctor>( "doAboutBigBang", &AppFunctor::doAboutBigBang ),
		GUI::ActionMaker<AppFunctor, 1>( "doToolsReferenceGuide", &AppFunctor::OnToolsReferenceGuide ),
		GUI::ActionMaker<AppFunctor, 2>( "doContentCreation", &AppFunctor::OnContentCreation ),
		GUI::ActionMaker<AppFunctor, 3>( "doShortcuts", &AppFunctor::OnShortcuts )
	{}

	bool doAboutBigBang( GUI::ItemPtr )
	{
		CAboutDlg().DoModal();
		return true;
	}
	
	bool openHelpFile( const std::string& name, const std::string& defaultFile )
	{
		CWaitCursor wait;
		
		std::string helpFile = Options::getOptionString(
			"help/" + name,
			"..\\..\\doc\\" + defaultFile );

		int result = (int)ShellExecute( AfxGetMainWnd()->GetSafeHwnd(), "open", helpFile.c_str() , NULL, NULL, SW_SHOWNORMAL );
		if ( result < 32 )
		{
			Commentary::instance().addMsg( "Could not find help file:\n \"" + helpFile + "\"\nPlease check options.xml/help/" + name, Commentary::WARNING );
		}

		return result >= 32;
	}

	// Open the Tools Reference Guide
	bool OnToolsReferenceGuide( GUI::ItemPtr item )
	{
		return openHelpFile( "toolsReferenceGuide" , "content_tools_reference_guide.pdf" );
	}

	// Open the Content Creation Manual (CCM)
	bool OnContentCreation( GUI::ItemPtr item )
	{
		return openHelpFile( "contentCreationManual" , "content_creation.chm" );
	}
	
	bool OnShortcuts( GUI::ItemPtr )
	{
        ShortcutsDlg::instance()->ShowWindow(SW_SHOW);
		return true;
	}
}
AppFunctorInstance;

// BigBang2App message handlers

BOOL BigBang2App::OnIdle(LONG lCount)
{
	// The following two lines need to be uncommented for toolbar docking to
	// work properly, and the application's toolbars have to inherit from
	// GUI::CGUIToolBar
	if ( CWinApp::OnIdle( lCount ) )
		return TRUE; // give priority to windows GUI as MS says it should be.

	// check to see the window is not active
	MainFrame * mainFrame = (MainFrame *)(m_pMainWnd);
	HWND fgWin = GetForegroundWindow();
	DWORD processID;
	GetWindowThreadProcessId( fgWin, &processID );

	if( ::IsWindow( fgWin ) && processID == GetCurrentProcessId() &&
		fgWin != mainFrame->m_hWnd && GetParent( fgWin ) != mainFrame->m_hWnd &&
		( GetWindowLong( fgWin, GWL_STYLE ) & WS_VISIBLE ) == 0 )
	{
		mainFrame->SetForegroundWindow();
		fgWin = GetForegroundWindow();
	}

	if (mainFrame->IsIconic()								// is minimised
		|| (fgWin != mainFrame->m_hWnd && GetParent( fgWin ) != mainFrame->m_hWnd )		// is not currently active
		|| !CooperativeMoo::activate() ) // failed, maybe there's not enough memory to reactivate moo
	{
		CooperativeMoo::deactivate();
		Sleep(100);
		s_mfApp->calculateFrameTime(); // Do this to effectively freeze time
	}
	else
	{
		// measure the update time
		uint64 beforeTime = timestamp();
	
		// do the real update!!
		s_mfApp->updateFrame();

		// update any gui
		mainFrame->frameUpdate();

		uint64 afterTime = timestamp();
		float lastUpdateMilliseconds = (float) (((int64)(afterTime - beforeTime)) / stampsPerSecondD()) * 1000.f;

		float desiredFrameRate_ = 60.f;
		const float desiredFrameTime = 1000.f / desiredFrameRate_;	// milliseconds

		if (desiredFrameTime > lastUpdateMilliseconds)
		{
			float compensation = desiredFrameTime - lastUpdateMilliseconds;
			Sleep((int)compensation);
		}
	
	}

	DWORD size, num;
	if( updateMailSlot_ != INVALID_HANDLE_VALUE &&
		GetMailslotInfo( updateMailSlot_, 0, &size, &num, 0 ) &&
		num )
	{
		char s[ 1024 * 128 ];// big enough for a file name???
		DWORD bytesRead;
		if( ReadFile( updateMailSlot_, s, size, &bytesRead, 0 ) )
		{
			ChunkManager::instance().switchToSyncMode( true );
			EditorChunkParticleSystem::reload( s );
			EditorChunkModel::reload( s );
			ChunkManager::instance().switchToSyncMode( false );
			// Ensure stale columns get recreated, otherwise a crash can occur.
			ChunkManager::instance().cameraSpace()->focus( Moo::rc().invView().applyToOrigin() );
		}
	}

	return TRUE;
}


int BigBang2App::ExitInstance()
{
	if ( s_mfApp )
	{
        ShortcutsDlg::cleanup();

		GizmoManager::instance().removeAllGizmo();
		while ( ToolManager::instance().tool() )
			ToolManager::instance().popTool();

		s_mfApp->fini();
		delete s_mfApp;
		s_mfApp = NULL;

		// fini bigbang
		Initialisation::finiApp();

		PanelManager::instance()->killDock();
	}

	return CWinApp::ExitInstance();
}

BOOL BigBang2App::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
//	if (nID == WM_SYSKEYDOWN)
//		return TRUE;

	return CWinApp::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

BOOL BigBang2App::PreTranslateMessage(MSG* pMsg)
{
	return CWinApp::PreTranslateMessage(pMsg);
}

BOOL CAboutDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	BITMAP bitmap;
	mBackground.GetBitmap( &bitmap );
	RECT rect = { 0, 0, bitmap.bmWidth, bitmap.bmHeight };
	AdjustWindowRect( &rect, GetWindowLong( m_hWnd, GWL_STYLE ), FALSE );
	MoveWindow( &rect, FALSE );
	CenterWindow();

	SetCapture();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CAboutDlg::OnPaint()
{
	CPaintDC dc(this); // device context for painting
	CDC memDC;
	memDC.CreateCompatibleDC( &dc);
	CBitmap* saveBmp = memDC.SelectObject( &mBackground );
	CFont* saveFont = memDC.SelectObject( &mFont );

	RECT client;
	GetClientRect( &client );

	dc.SetTextColor( 0x00808080 );
	dc.BitBlt( 0, 0, client.right, client.bottom, &memDC, 0, 0, SRCCOPY );

	memDC.SelectObject( &saveBmp );
	memDC.SelectObject( &saveFont );

	CString builtOn = CString( "Version " ) + aboutVersionString;
	if (ToolsCommon::isEval())
		builtOn += CString( " Eval" );
#ifdef _DEBUG
    builtOn += CString( " Debug" );
#endif
	builtOn += CString( ": built " ) + aboutCompileTimeString;

	dc.SetBkMode( TRANSPARENT );
	dc.ExtTextOut( 72, 290, 0, NULL, builtOn, NULL );
}

void CAboutDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
	CDialog::OnLButtonDown(nFlags, point);
	OnOK();
}

void CAboutDlg::OnRButtonDown(UINT nFlags, CPoint point)
{
	CDialog::OnRButtonDown(nFlags, point);
	OnOK();
}
