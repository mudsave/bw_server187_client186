/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// ModelEditor.cpp : Defines the class behaviors for the application.
//
#include "pch.hpp"

#include "main_frm.h"

#include "model_editor_doc.h"
#include "model_editor_view.h"
#include "about_box.hpp"
#include "prefs_dialog.hpp"

#include "tools_camera.hpp"
#include "utilities.hpp"

#include "me_module.hpp"
#include "me_app.hpp"
#include "me_shell.hpp"
#include "python_adapter.hpp"

#include "pyscript/py_output_writer.hpp"

#include "compile_time.hpp"
#include "appmgr/app.hpp"
#include "appmgr/options.hpp"
#include "common/tools_common.hpp"
#include "common/directory_check.hpp"
#include "common/cooperative_moo.hpp"
#include "common/command_line.hpp"
#include "resmgr/bwresource.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_loader.hpp"

#include "cstdmf/bgtask_manager.hpp"

#include "gizmo/gizmo_manager.hpp"
#include "gizmo/tool_manager.hpp"

#include "guitabs/guitabs.hpp"

#include "guimanager/gui_manager.hpp"
#include "guimanager/gui_menu.hpp"
#include "guimanager/gui_toolbar.hpp"
#include "guimanager/gui_functor.hpp"
#include "guimanager/gui_functor_cpp.hpp"
#include "guimanager/gui_functor_option.hpp"

#include "ual/ual_drop_manager.hpp"

#include "page_lights.hpp"
#include "page_messages.hpp"

#include "panel_manager.hpp"

#include "loading_dialog.hpp"

#include "material_preview.hpp"

DECLARE_DEBUG_COMPONENT( 0 )
#include "me_error_macros.hpp"

#include "model_editor.h"

#include <afxdhtml.h>
#include <string>
#include <fstream>

DogWatch CModelEditorApp::s_updateWatch( "Page Updates" );

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
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
                s_instance = new ShortcutsDlg(IDD_SHORTCUTS);
                s_instance->Create(IDD_SHORTCUTS);
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
}

/*static*/ CMainFrame* CMainFrame::s_instance = NULL;

// The one and only CModelEditorApp object

CModelEditorApp theApp;

// CModelEditorApp

BEGIN_MESSAGE_MAP(CModelEditorApp, CWinApp)
END_MESSAGE_MAP()


// BigBang2App construction
CModelEditorApp * CModelEditorApp::s_instance_ = NULL;

CModelEditorApp::CModelEditorApp():
	mfApp_(NULL),
	initDone_( false ),
	pythonAdapter_(NULL),
	GUI::ActionMaker<CModelEditorApp,0>( "recent_models", &CModelEditorApp::recent_models ),
	GUI::ActionMaker<CModelEditorApp,1>( "recent_lights", &CModelEditorApp::recent_lights ),
	GUI::ActionMaker<CModelEditorApp,2>( "doAboutApp", &CModelEditorApp::OnAppAbout ),
	GUI::ActionMaker<CModelEditorApp,3>( "doToolsReferenceGuide", &CModelEditorApp::OnToolsReferenceGuide ),
	GUI::ActionMaker<CModelEditorApp,4>( "doContentCreation", &CModelEditorApp::OnContentCreation ),
	GUI::ActionMaker<CModelEditorApp,5>( "doShortcuts", &CModelEditorApp::OnShortcuts )
{
	ASSERT(s_instance_ == NULL);
	s_instance_ = this;

	EnableHtmlHelp();

	char dateStr [9];
    char timeStr [9];
    _strdate( dateStr );
    _strtime( timeStr );

	static const int MAX_LOG_NAME = 8192;
	static char logName[ MAX_LOG_NAME + 1 ] = "";
	if( logName[0] == 0 )
	{
		DWORD len = GetModuleFileName( NULL, logName, MAX_LOG_NAME );
		while( len && logName[ len - 1 ] != '.' )
			--len;
		strcpy( logName + len, "log" );
	}

	std::ostream* logFile = NULL;
		
	if( logName[0] != 0 )
	{
		logFile = new std::ofstream( logName, std::ios::app );

		*logFile << "\n/-----------------------------------------------"
			"-------------------------------------------\\\n";

		*logFile << "BigWorld World Editor " << aboutVersionString
			<< " (compiled at " << aboutCompileTimeString << ") starting on "
			<< dateStr << " " << timeStr << "\n\n";
		
		logFile->flush();
	}

	//Instanciate the Message handler to catch BigWorld messages
	MsgHandler::instance().logFile( logFile );
}

void CModelEditorApp::fini()
{
	initDone_ = false;

	meShell_->fini();
	delete meShell_;
	meShell_ = NULL;

	delete meApp_;
	meApp_ = NULL;
	
	delete pythonAdapter_;
	pythonAdapter_ = NULL;

	s_instance_ = NULL;
}

CModelEditorApp::~CModelEditorApp()
{
	if (s_instance_)
	{
		fini();
	}
}

//A method to allow the load error dialog to not interfere with loading
/*static*/ UINT CModelEditorApp::loadErrorMsg( LPVOID lpvParam )
{
	const char* modelName = (char*)lpvParam;

	char msg[256];
	sprintf(msg, 
		"The last time ModelEditor was run it exited unexpectedly.\n"
		"The model \"%s\" will not be loaded.", modelName);

	::MessageBox( AfxGetApp()->m_pMainWnd->GetSafeHwnd(),
		msg, "Model Load Error", MB_OK | MB_ICONERROR);
	return 0;
}

// CModelEditorApp initialization

BOOL CModelEditorApp::InitInstance()
{
	//Let the user know something is going on
	SetCursor( ::LoadCursor( NULL, IDC_WAIT ));
	
	CWinApp::InitInstance();

	// Check the use-by date
	if (!ToolsCommon::canRun())
	{
		ToolsCommon::outOfDateMessage( "ModelEditor" );
		return FALSE;
	}

	// Initialize OLE libraries
	if (!AfxOleInit())
	{
		AfxMessageBox(IDP_OLE_INIT_FAILED);
		return FALSE;
	}
	AfxEnableControlContainer();

	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views
	CSingleDocTemplate* pDocTemplate;
	pDocTemplate = new CSingleDocTemplate(
		IDR_MAINFRAME,
		RUNTIME_CLASS(CModelEditorDoc),
		RUNTIME_CLASS(CMainFrame),       // main SDI frame window
		RUNTIME_CLASS(CModelEditorView));
	AddDocTemplate(pDocTemplate);

	//Assume there will be nothing to load initially,
	//Do it now since "parseCommandLineMF" may set it
	modelToLoad_ = "";
	modelToAdd_ = "";
	
	// Parse command line for standard shell commands, DDE, file open
	MFCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);

	// initialise the MF file services (read in the command line arguments too)
	if (!parseCommandLineMF())
		return FALSE;

	DataSectionPtr testOptions = BWResource::openSection( "options.xml");
	if (testOptions == NULL)
	{
		::MessageBox( AfxGetApp()->m_pMainWnd->GetSafeHwnd(),
			"Unable to locate \"options.xml\" in the modeleditor application directory.\n"
			"Please ensure it present and editable before running again.\n"
			"ModelEditor will exit now.",
			"Unable to locate \"options.xml\"", MB_ICONERROR | MB_OK );
		return FALSE;
	}

	// Dispatch commands specified on the command line.  Will return FALSE if
	// app was launched with /RegServer, /Register, /Unregserver or /Unregister.
	// This also creates all the GUI windows
	if (!ProcessShellCommand(cmdInfo))
		return FALSE;

	// The one and only window has been initialized, so show and update it
	m_pMainWnd->ShowWindow(SW_SHOWMAXIMIZED);
	m_pMainWnd->UpdateWindow();

	// initialise the MF App components
	ASSERT( !mfApp_ );
	mfApp_ = new App;

	ASSERT( !meShell_ );
	meShell_ = new MeShell;

	HINSTANCE hInst = AfxGetInstanceHandle();

	CMainFrame * mainFrame = (CMainFrame *)(m_pMainWnd);

	if (!mfApp_->init( hInst, m_pMainWnd->GetSafeHwnd(), mainFrame->GetActiveView()->GetSafeHwnd(), MeShell::initApp ))
	{
		ERROR_MSG( "CModelEditorApp::InitInstance - init failed\n" );
		return FALSE;
	}

	ASSERT( !meApp_ );
	meApp_ = new MeApp;

	// need to load the adapter before the load thread begins, but after the modules
	pythonAdapter_ = new PythonAdapter();

	// Prepare the GUI
	GUI::OptionFunctor::instance()->setOption( this );

	DataSectionPtr section = BWResource::openSection( "resources/data/gui.xml" );
	for( int i = 0; i < section->countChildren(); ++i )
		GUI::Manager::instance()->add( new GUI::Item( section->openChild( i ) ) );

	//Setup the main menu
	GUI::Manager::instance()->add(
		new GUI::Menu( "MainMenu", AfxGetMainWnd()->GetSafeHwnd() ));
	AfxGetMainWnd()->DrawMenuBar();

	// Add the toolbar(s) through the BaseMainFrame base class
	( (CMainFrame*) AfxGetMainWnd() )->createToolbars( "AppToolbars" );

	//Add some drop acceptance functors
	UalDropManager::instance().add(
		new UalDropFunctor< CModelEditorApp >( mainFrame->GetActiveView(), "model", this, &CModelEditorApp::loadFile, true ) );

	UalDropManager::instance().add(
		new UalDropFunctor< CModelEditorApp >( mainFrame->GetActiveView(), "mvl", this, &CModelEditorApp::loadFile ) );

	UalDropManager::instance().add(
		new UalDropFunctor< CModelEditorApp >( mainFrame->GetActiveView(), "", this, &CModelEditorApp::loadFile ) );

	//CView* oldView = mainFrame->GetActiveView();
	
	// GUITABS Tearoff tabs system init and setup
	PanelManager::instance()->initDock( mainFrame, mainFrame->GetActiveView() );
	PanelManager::instance()->initPanels();
	
	bool loadStarted = ChunkManager::instance().chunkLoader()->start();
	MF_ASSERT( loadStarted );
	
	if (modelToLoad_ != "")
	{
		modelToLoad_ = BWResource::dissolveFilename( modelToLoad_ );
	}
	else if (Options::getOptionInt( "startup/loadLastModel", 1 ))
	{
		modelToLoad_ = Options::getOptionString( "models/file0", "" );
		
		if (!Options::getOptionBool( "startup/lastLoadOK", true ))
		{
			static char modelName[256];
			strcpy( modelName, modelToLoad_.c_str() );
			AfxBeginThread( &CModelEditorApp::loadErrorMsg, modelName );

			//Remove this model from the MRU models list
			MRU::instance().update( "models", modelToLoad_, false );

			Options::setOptionBool( "startup/lastLoadOK", true );
			Options::save();

			modelToLoad_ = "";
		}
	}

	//If there is no model to load, restore the cursor
	if (modelToLoad_ == "")
	{
		updateRecentList("models");
		SetCursor( ::LoadCursor( NULL, IDC_ARROW ));
	}

	updateRecentList("lights");

	initDone_ = true;

	//oldView->SetFocus();

	//mainFrame->SetForegroundWindow();

	// call DragAcceptFiles only if there's a suffix
	//  In an SDI app, this should occur after ProcessShellCommand
	return TRUE;
}

BOOL CModelEditorApp::parseCommandLineMF()
{
	DirectoryCheck( "ModelEditor" );
	
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
		
		char* openFile;
		if( ( openFile = strstr( argv[ argc ], "-o" ) ) ||
			( openFile = strstr( argv[ argc ], "-O" ) ) )
		{
			openFile += 2;
			modelToLoad_ = std::string( openFile );
		}

		if (++argc >= MAX_ARGS)
		{
			ERROR_MSG( "ModelEditor::parseCommandLineMF: Too many arguments!!\n" );
			return FALSE;
		}

		argv[ argc ] = strtok( NULL, seps );
		
	}
	
	return Options::init( argc, argv, false ) && BWResource::init( argc, argv );
}

bool CModelEditorApp::recent_models( GUI::ItemPtr item )
{
	if (!MeApp::instance().canExit( false ))
		return false;
	
	modelToLoad_ = (*item)[ "fileName" ];

	return true;
}

bool CModelEditorApp::recent_lights( GUI::ItemPtr item )
{
	PageLights* lightPage = (PageLights*)GUITABS::Manager::instance()->getContent( PageLights::contentID );
	
	bool loaded = lightPage->openLightFile( (*item)[ "fileName" ] );

	updateRecentList( "lights" );

	return loaded;
}

void CModelEditorApp::updateRecentList( const std::string& kind )
{
	GUI::ItemPtr recentFiles = ( *GUI::Manager::instance() )( "/MainMenu/File/Recent_" + kind );
	if( recentFiles )
	{
		while( recentFiles->num() )
			recentFiles->remove( 0 );

		std::vector<std::string> files;
		MRU::instance().read(kind, files);

		for( unsigned i=0; i < files.size(); i++ )
		{
			std::stringstream name, displayName;
			name << kind << i;
			if (i <= 9)
				displayName << '&' << i << "  " << files[i];
			else
				displayName << "    " << files[i];
			GUI::ItemPtr item = new GUI::Item( "ACTION", name.str(), displayName.str(),
				"",	"", "", "recent_"+kind, "", "" );
			item->set( "fileName", files[i] );
			recentFiles->add( item );
		}
	}
}

BOOL CModelEditorApp::OnIdle(LONG lCount)
{
	static bool s_justLoaded = false;
	
	// The following two lines need to be uncommented for toolbar docking to
	// work properly, and the application's toolbars have to inherit from
	// GUI::CGUIToolBar
	if ( CWinApp::OnIdle( lCount ) )
		return TRUE; // give priority to windows GUI as MS says it should be.

	if (CMainFrame::instance().cursorOverGraphicsWnd())
	{
		::SetFocus( MeShell::instance().hWndGraphics() );
	}

	if (s_justLoaded)
	{
		Options::setOptionBool( "startup/lastLoadOK", true );
		Options::save();
		s_justLoaded = false;
	}

	if (modelToLoad_ != "")
	{
		SetCursor( ::LoadCursor( NULL, IDC_WAIT ));

		std::string::size_type first = modelToLoad_.rfind("/") + 1;
		std::string::size_type last = modelToLoad_.rfind(".");
		std::string modelName = modelToLoad_.substr( first, last-first );

		int numAnim = MeApp::instance().mutant()->animCount( modelToLoad_ );
		bool needsBBCalc = Options::getOptionInt( "settings/regenBBOnLoad", 1 ) &&
			(!MeApp::instance().mutant()->hasVisibilityBox( modelToLoad_ ));
		
		CLoadingDialog* load = NULL;
		if (numAnim > 4)
		{
			load = new CLoadingDialog( std::string( "Loading " + modelName + "..." ));
			load->setRange( needsBBCalc ? 2*numAnim + 1 : numAnim + 1 );
			Model::setAnimLoadCallback(
				new AnimLoadFunctor< CLoadingDialog >(load, &CLoadingDialog::step) );
		}
			
		char buf[256];
		sprintf( buf, "Loading Model: \"%s\"\n", modelToLoad_.c_str() );
		ME_INFO_MSG( buf );
			
		if (MeApp::instance().mutant()->loadModel( modelToLoad_ ))
		{
			s_justLoaded = true;
			Options::setOptionBool( "startup/lastLoadOK", false );
			Options::save();
			
			if (needsBBCalc)
			{
				MeApp::instance().mutant()->recreateModelVisibilityBox(
					new AnimLoadFunctor< CLoadingDialog >(load, &CLoadingDialog::step), false );

				//Let the user know
				ME_WARNING_MSG( "The model's visibility box has been automatically recalculated.\n"
					"A save will be required.\n");

				UndoRedo::instance().forceSave();
			}

			// Make sure the GUI reflects the changes
			CMainFrame::instance().updateGUI( true ); 

			MRU::instance().update( "models", modelToLoad_, true );
			updateRecentList( "models" );

			MeModule::instance().materialPreviewMode( false );
			
			MeApp::instance().camera()->boundingBox(
				MeApp::instance().mutant()->zoomBoundingBox() );

			if (!!Options::getOptionInt( "settings/zoomOnLoad", 0 ))
			{
				MeApp::instance().camera()->zoomToExtents( false );
			}

			MeApp::instance().camera()->render( 0.f );

			PanelManager::instance()->ualAddItemToHistory( modelToLoad_ );

			// Forcefully update any gui stuff
			CMainFrame::instance().updateGUI( true );
		}
		else
		{
			ERROR_MSG( "Unable to load \"%s\" since an error occured.\n", modelToLoad_.c_str() );
			
			//Remove this model from the MRU models list
			MRU::instance().update( "models", modelToLoad_, false );
			updateRecentList( "models" );
		}
		SetCursor( ::LoadCursor( NULL, IDC_ARROW ));
		modelToLoad_ = "";
		if (load)
		{
			Model::setAnimLoadCallback( NULL );
			delete load;
		}
	}
	else if (modelToAdd_ != "")
	{
		if (MeApp::instance().mutant()->addModel( modelToAdd_ ))
		{
			MRU::instance().update( "models", modelToAdd_, true );
			updateRecentList( "models" );
				
			MeApp::instance().camera()->boundingBox(
					MeApp::instance().mutant()->zoomBoundingBox() );

			if (!!Options::getOptionInt( "settings/zoomOnLoad", 0 ))
			{
				MeApp::instance().camera()->zoomToExtents( false );
			}

			MeApp::instance().camera()->render( 0.f );

			char buf[256];
			sprintf( buf, "Added Model: \"%s\"\n", modelToAdd_.c_str() );
			ME_INFO_MSG( buf );

			PanelManager::instance()->ualAddItemToHistory( modelToAdd_ );
		}
		else
		{
			char buf[256];
			sprintf( buf, "Unable to add the model \"%s\" since it is already used.\n", modelToAdd_.c_str() );
			ME_WARNING_MSG( buf );
		}
		modelToAdd_ = "";
	}
	
	CMainFrame * mainFrame = (CMainFrame *)(m_pMainWnd);
	HWND fgWin = GetForegroundWindow();
	if (mainFrame->IsIconic() // is minimised
		|| ((fgWin != mainFrame->GetSafeHwnd()) && (GetParent( fgWin ) != mainFrame->GetSafeHwnd()) )  // is not currently active
		|| !CooperativeMoo::activate() ) // failed, maybe there's not enough memory to reactivate moo
	{
		CooperativeMoo::deactivate();
		Sleep(100);
		mfApp_->calculateFrameTime();
	} 
	else
	{
		/*
		// measure the update time
		uint64 beforeTime = timestamp();
		//*/

		if ( MeApp::instance().mutant()->texMemUpdate() )
		{
			char buf[256];
			sprintf( buf, "%s texture mem", Utilities::memorySizeToStr(
				MeApp::instance().mutant()->texMem() ).c_str() );
			CMainFrame::instance().setStatusText( ID_INDICATOR_TETXURE_MEM, buf );
		}
	
		mfApp_->updateFrame();

		MaterialPreview::instance().update();

		// update any gui stuff
		CModelEditorApp::s_updateWatch.start();
		CMainFrame::instance().updateGUI();
		CModelEditorApp::s_updateWatch.stop();
	
		/*
		uint64 afterTime = timestamp();
		float lastUpdateMilliseconds = (float) (((int64)(afterTime - beforeTime)) / stampsPerSecondD()) * 1000.f;

		float desiredFrameRate_ = 60.f;
		const float desiredFrameTime = 1000.f / desiredFrameRate_;	// milliseconds

		if (desiredFrameTime > lastUpdateMilliseconds)
		{
			float compensation = desiredFrameTime - lastUpdateMilliseconds;
			Sleep((int)compensation);
		}
		//*/
	}

	return TRUE;
}

int CModelEditorApp::ExitInstance()
{
	if ( mfApp_ )
	{
		ShortcutsDlg::cleanup(); 

		GizmoManager::instance().removeAllGizmo();
		while ( ToolManager::instance().tool() )
		{
			ToolManager::instance().popTool();
		}

		mfApp_->fini();
		delete mfApp_;
		mfApp_ = NULL;

        PanelManager::instance()->killDock();                                   

		CModelEditorApp::instance().fini();                                     
	}

	return CWinApp::ExitInstance();
}

std::string CModelEditorApp::get( const std::string& key ) const
{
	return Options::getOptionString( key );
}

bool CModelEditorApp::exist( const std::string& key ) const
{
	return Options::optionExists( key );
}

void CModelEditorApp::set( const std::string& key, const std::string& value )
{
	Options::setOptionString( key, value );
}

bool CModelEditorApp::loadFile( UalItemInfo* ii )
{
	if ( PythonAdapter::instance() )
			PythonAdapter::instance()->callString( "openFile", 
				BWResource::dissolveFilename( ii->longText() ) );

	return true;
}

// App command to run the about dialog
bool CModelEditorApp::OnAppAbout( GUI::ItemPtr item )
{
	CAboutDlg().DoModal();
	return true;
}

bool CModelEditorApp::openHelpFile( const std::string& name, const std::string& defaultFile )
{
	CWaitCursor wait;
	
	std::string helpFile = Options::getOptionString(
		"help/" + name,
		"..\\..\\doc\\" + defaultFile );

	int result = (int)ShellExecute( AfxGetMainWnd()->GetSafeHwnd(), "open", helpFile.c_str() , NULL, NULL, SW_SHOWNORMAL );
	if ( result < 32 )
	{
		char buf[256];
		sprintf( buf, "Could not find help file:\n \"%s\"\nPlease check options.xml/help/%s\n", helpFile.c_str(), name.c_str() );
		ME_WARNING_MSG( buf );
	}

	return result >= 32;
}

// Open the Tools Reference Guide
bool CModelEditorApp::OnToolsReferenceGuide( GUI::ItemPtr item )
{
	return openHelpFile( "toolsReferenceGuide" , "content_tools_reference_guide.pdf" );
}

// Open the Content Creation Manual (CCM)
bool CModelEditorApp::OnContentCreation( GUI::ItemPtr item )
{
	return openHelpFile( "contentCreationManual" , "content_creation.chm" );
}

// App command to show the keyboard shortcuts
bool CModelEditorApp::OnShortcuts( GUI::ItemPtr item )
{
    ShortcutsDlg::instance()->ShowWindow(SW_SHOW);
	return true;
}

//------------------------------------------------

void CModelEditorApp::loadModel( const char* modelName )
{
	modelToLoad_ = modelName;
}

static PyObject * py_loadModel( PyObject * args )
{
	char* modelName;

	if (!PyArg_ParseTuple( args, "s", &modelName ))
	{
		PyErr_SetString( PyExc_TypeError, "py_openModel: Expected a string argument." );
		return NULL;
	}
	
	//Either Ctrl or Alt will result in the model being added
	if ((GetAsyncKeyState(VK_LCONTROL) < 0) ||
		(GetAsyncKeyState(VK_RCONTROL) < 0) ||
		(GetAsyncKeyState(VK_LMENU) < 0) ||
		(GetAsyncKeyState(VK_RMENU) < 0))
	{
		CModelEditorApp::instance().addModel( modelName );
	}
	else if (MeApp::instance().canExit( false ))
	{
		CModelEditorApp::instance().loadModel( modelName );
	}

	Py_Return;
}
PY_MODULE_FUNCTION( loadModel, ModelEditor )

//------------------------------------------------

void CModelEditorApp::addModel( const char* modelName )
{
	//If there is no model loaded then load this one...
	if (MeApp::instance().mutant()->modelName() == "")
	{
		loadModel( modelName );
	}
	else if (!MeApp::instance().mutant()->nodefull())
	{
		ERROR_MSG( "Models can only be added to nodefull models.\n\n"
			"  \"%s\"\n\n"
			"is not a nodefull model.\n",
			MeApp::instance().mutant()->modelName().c_str() );
	}
	else if (!MeApp::instance().mutant()->nodefull( modelName ))
	{
		ERROR_MSG( "Only nodefull models can be added to other models.\n\n"
			"  \"%s\"\n\n"
			"is not a nodefull model.\n",
			modelName );
	}
	else if ( !MeApp::instance().mutant()->canAddModel( modelName ))
	{
		ERROR_MSG( "The model cannot be added since it shares no nodes in common with the loaded model.\n" );
	}
	else
	{
		modelToAdd_ = modelName;
	}
}

static PyObject * py_addModel( PyObject * args )
{
	char* modelName;

	if (!PyArg_ParseTuple( args, "s", &modelName ))
	{
		PyErr_SetString( PyExc_TypeError, "py_addModel: Expected a string argument." );
		return NULL;
	}

	if ( BWResource::openSection( modelName ) == NULL )                         
    {                                                                           
        PyErr_SetString( PyExc_IOError, "py_addModel: The model was not found." );
        return NULL;                                                            
    }                                                                           
	
	CModelEditorApp::instance().addModel( modelName );

	Py_Return;
}
PY_MODULE_FUNCTION( addModel, ModelEditor )

//------------------------------------------------

static PyObject * py_removeAddedModels( PyObject * args )
{
	if (MeApp::instance().mutant())
		MeApp::instance().mutant()->removeAddedModels();

	Py_Return;
}
PY_MODULE_FUNCTION( removeAddedModels, ModelEditor )


//------------------------------------------------
	
static PyObject * py_hasAddedModels( PyObject * args )
{
	if (MeApp::instance().mutant())
		return PyInt_FromLong( MeApp::instance().mutant()->hasAddedModels() );

	return PyInt_FromLong( 0 );
}
PY_MODULE_FUNCTION( hasAddedModels, ModelEditor )


//------------------------------------------------

void CModelEditorApp::loadLights( const char* lightsName )
{
	PageLights* lightPage = (PageLights*)GUITABS::Manager::instance()->getContent( PageLights::contentID );
	lightPage->openLightFile( lightsName );
	updateRecentList( "lights" );
}

static PyObject * py_loadLights( PyObject * args )
{
	char* lightsName;

	if (!PyArg_ParseTuple( args, "s", &lightsName ))
	{
		PyErr_SetString( PyExc_TypeError, "py_openModel: Expected a string argument." );
		return NULL;
	}
	
	CModelEditorApp::instance().loadLights( lightsName );

	Py_Return;
}
PY_MODULE_FUNCTION( loadLights, ModelEditor )

//------------------------------------------------

// File Menu Open
void CModelEditorApp::OnFileOpen()
{
	static char BASED_CODE szFilter[] =	"Model (*.model)|*.model||";
	CFileDialog fileDlg (TRUE, "", "", OFN_FILEMUSTEXIST| OFN_HIDEREADONLY, szFilter);

	std::string modelsDir;
	MRU::instance().getDir( "models" ,modelsDir );
	fileDlg.m_ofn.lpstrInitialDir = modelsDir.c_str();

	if ( fileDlg.DoModal() == IDOK )
	{
		modelToLoad_ = BWResource::dissolveFilename( std::string( fileDlg.GetPathName() ));
		if (!BWResource::validPath( modelToLoad_ ))
		{
			::MessageBox( AfxGetApp()->m_pMainWnd->GetSafeHwnd(),
				"You can only load a model located in one of the resource folders.",
				"Unable to resolve model path",
				MB_OK | MB_ICONWARNING );
			modelToLoad_ = "";
		}
	}
}
static PyObject * py_openFile( PyObject * args )
{
	if (MeApp::instance().canExit( false ))
		CModelEditorApp::instance().OnFileOpen();

	Py_Return;
}
PY_MODULE_FUNCTION( openFile, ModelEditor )

//------------------------------------------------

// File Menu Add
void CModelEditorApp::OnFileAdd()
{
	static char BASED_CODE szFilter[] =	"Model (*.model)|*.model||";
	CFileDialog fileDlg (TRUE, "", "", OFN_FILEMUSTEXIST| OFN_HIDEREADONLY, szFilter);

	std::string modelsDir;
	MRU::instance().getDir( "models" ,modelsDir );
	fileDlg.m_ofn.lpstrInitialDir = modelsDir.c_str();

	if ( fileDlg.DoModal() == IDOK )
	{
		modelToAdd_ = BWResource::dissolveFilename( std::string( fileDlg.GetPathName() ));
		if (!BWResource::validPath( modelToAdd_ ))
		{
			::MessageBox( AfxGetApp()->m_pMainWnd->GetSafeHwnd(),
				"You can only add a model located in one of the resource folders.",
				"Unable to resolve model path",
				MB_OK | MB_ICONWARNING );
			modelToAdd_ = "";
		}
	}
}
static PyObject * py_addFile( PyObject * args )
{
	CModelEditorApp::instance().OnFileAdd();
	Py_Return;
}
PY_MODULE_FUNCTION( addFile, ModelEditor )

//------------------------------------------------

void CModelEditorApp::OnFileReloadTextures()
{
	CWaitCursor wait;
	
	Moo::ManagedTexture::accErrs( true );
	
	Moo::TextureManager::instance()->reloadAllTextures();

	std::string errStr = Moo::ManagedTexture::accErrStr();
	if (errStr != "")
	{
		ERROR_MSG( "Moo:ManagedTexture::load, unable to load the following textures:\n"
				"%s\n\nPlease ensure these textures exist.\n", errStr.c_str() );
	}

	Moo::ManagedTexture::accErrs( false );

}
static PyObject * py_reloadTextures( PyObject * args )
{
	CModelEditorApp::instance().OnFileReloadTextures();
	Py_Return;
}
PY_MODULE_FUNCTION( reloadTextures, ModelEditor )

//------------------------------------------------

void CModelEditorApp::OnFileRegenBoundingBox()
{
	CWaitCursor wait;

	int animCount = MeApp::instance().mutant()->animCount();
	CLoadingDialog* load = NULL;
	if (animCount > 4)
	{
		load = new CLoadingDialog( "Regenerating Visibility Box..." );
		load->setRange( animCount );
	}
		
	MeApp::instance().mutant()->recreateModelVisibilityBox( new AnimLoadFunctor< CLoadingDialog >(load, &CLoadingDialog::step), true );

	ME_INFO_MSG( "Regenerated visibility box\n" );

	MeApp::instance().camera()->boundingBox(
		MeApp::instance().mutant()->zoomBoundingBox() );

	if (load) delete load;
}
static PyObject * py_regenBoundingBox( PyObject * args )
{
	CModelEditorApp::instance().OnFileRegenBoundingBox();
	Py_Return;
}
PY_MODULE_FUNCTION( regenBoundingBox, ModelEditor )

//------------------------------------------------

//The preferences dialog
void CModelEditorApp::OnAppPrefs()
{
	CPrefsDlg prefsDlg;
	prefsDlg.DoModal();
}
static PyObject * py_appPrefs( PyObject * args )
{
	CModelEditorApp::instance().OnAppPrefs();
	Py_Return;
}
PY_MODULE_FUNCTION( appPrefs, ModelEditor )
