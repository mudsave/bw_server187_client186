/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// mainframe.cpp : implementation of the MainFrame class
//

#include "pch.hpp"
#include "bigbang.h"

#include "mainframe.hpp"
#include "python_adapter.hpp"
#include "user_messages.hpp"
#include "page_scene.hpp"
#include "page_properties.hpp"
#include "property_list.hpp"

#include "big_bang.hpp"

#include "cstdmf/debug.hpp"
#include "pyscript/script.hpp"
#include "appmgr/options.hpp"

#include "splash_dialog.hpp"
#include "foldersetter.hpp"
#include "guimanager/gui_manager.hpp"

#include "panel_manager.hpp"

#include "controls/show_cursor_helper.hpp"


DECLARE_DEBUG_COMPONENT2( "BigBang2", 0 )


#ifdef _DEBUG
#define new DEBUG_NEW
#endif

static const int c_menuPaneWidth = 307;

// MainFrame

IMPLEMENT_DYNCREATE(MainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(MainFrame, CFrameWnd)
	ON_WM_CREATE()
	// Global help commands
	ON_COMMAND(ID_HELP_FINDER, CFrameWnd::OnHelpFinder)
//	ON_COMMAND(ID_HELP, CFrameWnd::OnHelp)
	ON_COMMAND(ID_CONTEXT_HELP, CFrameWnd::OnContextHelp)
	ON_COMMAND(ID_DEFAULT_HELP, CFrameWnd::OnHelpFinder)
	ON_WM_SIZE()
	ON_MESSAGE( WM_ENTERSIZEMOVE, OnEnterSizeMove)
	ON_MESSAGE( WM_EXITSIZEMOVE, OnExitSizeMove)
	ON_WM_CLOSE()
	ON_UPDATE_COMMAND_UI(ID_INDICATOR_TRIANGLES, OnUpdateIndicatorTriangles)
	ON_UPDATE_COMMAND_UI(ID_INDICATOR_SNAPS, OnUpdateIndicatorSnaps)
	ON_UPDATE_COMMAND_UI(ID_INDICATOR_POSITION, OnUpdateIndicatorPosition)
	ON_UPDATE_COMMAND_UI(ID_INDICATOR_MEMORYLOAD, OnUpdateIndicatorMemoryLoad)
	ON_UPDATE_COMMAND_UI(ID_INDICATOR_FRAMERATE, OnUpdateIndicatorFrameRate)
	ON_UPDATE_COMMAND_UI(ID_INDICATOR_CHUNKS, OnUpdateIndicatorChunks)
	ON_COMMAND(ID_SCENE_POPUP_INSERT, OnPopupInsert)
	ON_COMMAND(ID_SCENE_POPUP_DELETE, OnPopupDelete)
	ON_COMMAND(ID_SCENE_POPUP_RENAME, OnPopupRename)
	ON_COMMAND(ID_SCENE_POPUP_MOVESELECTIONHERE, OnPopupMoveSelectionHere)
	ON_COMMAND(ID_PROPERTIES_LIST_POPUP_ADDITEM, OnPopupPropertyListAddItem)
	ON_COMMAND(ID_PROPERTIES_LISTITEM_POPUP_REMOVEITEM, OnPopupPropertyListItemRemoveItem)
	ON_COMMAND_RANGE(GUI_COMMAND_START, GUI_COMMAND_END, OnGUIManagerCommand)
	ON_WM_MENUSELECT()
	ON_WM_EXITMENULOOP()
	ON_NOTIFY_RANGE( TBN_HOTITEMCHANGE, 0, 0xffffffff, OnToolbarHotItemChange  )
	ON_WM_SYSCOLORCHANGE()
END_MESSAGE_MAP()

static UINT indicators[] =
{
	ID_SEPARATOR,           // status line indicator
//	ID_INDICATOR_MESSAGE,
	ID_INDICATOR_MEMORYLOAD,
	ID_INDICATOR_POSITION,
	ID_INDICATOR_SNAPS,
	ID_INDICATOR_TRIANGLES,
	ID_INDICATOR_FRAMERATE,
	ID_INDICATOR_CHUNKS,
//	ID_INDICATOR_CAPS,
//	ID_INDICATOR_NUM,
//	ID_INDICATOR_SCRL,
};


// MainFrame construction/destruction

MainFrame::MainFrame()
	: pScriptObject_(NULL)
	, resizing_( false )
	, triangles_( "" )
	, initialised_( false )
	, GUI::ActionMaker<MainFrame>( "doSaveSelectionAsPrefab", &MainFrame::saveSelectionAsPrefab )
	, GUI::ActionMaker<MainFrame, 1>( "doShowToolbar", &MainFrame::showToolbar )
	, GUI::ActionMaker<MainFrame, 2>( "doHideToolbar", &MainFrame::hideToolbar )
	, GUI::UpdaterMaker<MainFrame>( "updateToolbar", &MainFrame::updateToolbar )
	, GUI::ActionMaker<MainFrame, 3>( "doShowStatusBar", &MainFrame::showStatusBar )
	, GUI::ActionMaker<MainFrame, 4>( "doHideStatusBar", &MainFrame::hideStatusBar )
	, GUI::UpdaterMaker<MainFrame, 1>( "updateStatusBar", &MainFrame::updateStatusBar )
	, GUI::ActionMaker<MainFrame, 5>( "doShowPlayerPreview", &MainFrame::showPlayerPreview )
	, GUI::ActionMaker<MainFrame, 6>( "doHidePlayerPreview", &MainFrame::hidePlayerPreview )
	, GUI::UpdaterMaker<MainFrame, 2>( "updatePlayerPreview", &MainFrame::updatePlayerPreview )
	, GUI::UpdaterMaker<MainFrame, 3>( "updateToolMode", &MainFrame::updateToolMode )
{
	m_bAutoMenuEnable = FALSE;
}

MainFrame::~MainFrame()
{
}

int MainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	//Added a splashscreen
	if( !IsDebuggerPresent() )
		CSplashDlg::ShowSplashScreen( this );

	EnableDocking( CBRS_ALIGN_ANY );

	if (!m_wndStatusBar.Create(this,
			WS_CHILD | WS_VISIBLE | CBRS_BOTTOM) ||
		!m_wndStatusBar.SetIndicators(indicators,
		  sizeof(indicators)/sizeof(UINT)))
	{
		TRACE0("Failed to create status bar\n");
		return -1;      // fail to create
	}

	// initialise the size of the indicators
//	int messageWidth = 250;
	int memoryLoadWidth = 150;
	int trianglesWidth = 80;
	int snapsWidth = 140;
	int positionWidth = 280;
	int chunksWidth = 190;
	int frameRateWidth = 60;

//	m_wndStatusBar.SetPaneInfo(m_wndStatusBar.CommandToIndex(ID_INDICATOR_MESSAGE),
//							ID_INDICATOR_MESSAGE, SBPS_NORMAL, messageWidth);
	m_wndStatusBar.SetPaneInfo(m_wndStatusBar.CommandToIndex(ID_INDICATOR_TRIANGLES),
							ID_INDICATOR_TRIANGLES, SBPS_NORMAL, trianglesWidth);
	m_wndStatusBar.SetPaneInfo(m_wndStatusBar.CommandToIndex(ID_INDICATOR_SNAPS),
							ID_INDICATOR_SNAPS, SBPS_NORMAL, snapsWidth);
	m_wndStatusBar.SetPaneInfo(m_wndStatusBar.CommandToIndex(ID_INDICATOR_MEMORYLOAD),
							ID_INDICATOR_MEMORYLOAD, SBPS_NORMAL, memoryLoadWidth);
	m_wndStatusBar.SetPaneInfo(m_wndStatusBar.CommandToIndex(ID_INDICATOR_POSITION),
							ID_INDICATOR_POSITION, SBPS_NORMAL, positionWidth);
	m_wndStatusBar.SetPaneInfo(m_wndStatusBar.CommandToIndex(ID_INDICATOR_CHUNKS),
							ID_INDICATOR_CHUNKS, SBPS_NORMAL, chunksWidth);
	m_wndStatusBar.SetPaneInfo(m_wndStatusBar.CommandToIndex(ID_INDICATOR_FRAMERATE),
							ID_INDICATOR_FRAMERATE, SBPS_NORMAL, frameRateWidth);

	return 0;
}

// MainFrame diagnostics

#ifdef _DEBUG
void MainFrame::AssertValid() const
{
	CFrameWnd::AssertValid();
}

void MainFrame::Dump(CDumpContext& dc) const
{
	CFrameWnd::Dump(dc);
}

#endif //_DEBUG

void MainFrame::frameUpdate( bool forceRedraw /*= false*/ )
{
	// update controls on child windows
	PanelManager::instance()->updateControls();

    BOOL redrawPanels = forceRedraw ? TRUE : FALSE;

	// update the status bar
	m_wndStatusBar.SetPaneText(m_wndStatusBar.CommandToIndex(ID_INDICATOR_MEMORYLOAD),
		BigBang::instance().getStatusMessage( 0 ).c_str(), redrawPanels);
	m_wndStatusBar.SetPaneText(m_wndStatusBar.CommandToIndex(ID_INDICATOR_TRIANGLES),
		BigBang::instance().getStatusMessage( 1 ).c_str(), redrawPanels);
	m_wndStatusBar.SetPaneText(m_wndStatusBar.CommandToIndex(ID_INDICATOR_SNAPS),
		BigBang::instance().getStatusMessage( 2 ).c_str(), redrawPanels);
	m_wndStatusBar.SetPaneText(m_wndStatusBar.CommandToIndex(ID_INDICATOR_POSITION),
		BigBang::instance().getStatusMessage( 3 ).c_str(), redrawPanels);
	m_wndStatusBar.SetPaneText(m_wndStatusBar.CommandToIndex(ID_INDICATOR_FRAMERATE),
		BigBang::instance().getStatusMessage( 4 ).c_str(), TRUE); // Always redraw
	m_wndStatusBar.SetPaneText(m_wndStatusBar.CommandToIndex(ID_INDICATOR_CHUNKS),
		BigBang::instance().getStatusMessage( 5 ).c_str(), redrawPanels);

	// update the scene tab
	//TODO : put back in when page scene is working correctly
	//PageScene::instance().update();

	// remove the focus if over the 3d pane and selection filter not being used
	// (as selection filter drops over the 3d view)
	if (BigBang::instance().cursorOverGraphicsWnd() &&
		GetCapture() == NULL //&&
//		!m_wndToolBar2.selectionFilter_.GetDroppedState() &&
//		!m_wndToolBar2.coordFilter_.GetDroppedState()
	)
	{
		// TODO: this is a bit of a hack on selected item, 
		// they should be able to look after themselves
		if (PropertyItem::selectedItem())
			PropertyItem::selectedItem()->loseFocus();

		SetFocus();
	}
}

//////////////


// MainFrame message handlers

#include "moo/render_context.hpp"

BOOL MainFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext)
{
	BOOL result = CFrameWnd::OnCreateClient(lpcs, pContext);

	initialised_ = ( result != 0 );

	return result;
}

void MainFrame::OnSize(UINT nType, int cx, int cy)
{
	if (!initialised_)
		return;

	CFrameWnd::OnSize(nType, cx, cy);	// this calls RecalcLayout()
}

LRESULT MainFrame::OnEnterSizeMove (WPARAM, LPARAM)
{
	// Set the resizing_ flag to true, so the view knows that we are resizing
	// and that it shouldn't change the Moo mode.
	resizing_ = true;
	return 0;
}

LRESULT MainFrame::OnExitSizeMove (WPARAM, LPARAM)
{
	// Set the resizing_ flag back to false, so the view knows that it has to
	// change the Moo mode on the next repaint.
	resizing_ = false;
	// And send the repaint message to the view.
	::InvalidateRect( BigBang::instance().hwndGraphics(), NULL, TRUE );
	::UpdateWindow( BigBang::instance().hwndGraphics() );
	return 0;
}

BOOL MainFrame::OnWndMsg(UINT message, WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
	// capture the alt key and stop the menu bar from gaining focus
	if (wParam == SC_KEYMENU)
		return 1;

	return CFrameWnd::OnWndMsg(message, wParam, lParam, pResult);
}


void MainFrame::OnUpdateIndicatorTriangles(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(); 
}

void MainFrame::OnUpdateIndicatorSnaps(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(); 
}

void MainFrame::OnUpdateIndicatorPosition(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(); 
}

void MainFrame::OnUpdateIndicatorMemoryLoad(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(); 
}

void MainFrame::OnUpdateIndicatorFrameRate(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(); 
}

void MainFrame::OnUpdateIndicatorChunks(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(); 
}

void MainFrame::GetMessageString(UINT nID, CString& rMessage) const
{
	if (nID == AFX_IDS_IDLEMESSAGE)
	{
		rMessage.Empty();
		return;
	}

	return CFrameWnd::GetMessageString(nID, rMessage);
}

BOOL MainFrame::PreTranslateMessage(MSG* pMsg)
{
	UINT msg = pMsg->message;

	if (msg == WM_SYSKEYDOWN) 
	{
		bool bAlt = (HIWORD(pMsg->lParam) & KF_ALTDOWN) == KF_ALTDOWN;
		
		TCHAR vkey = static_cast< TCHAR>(pMsg->wParam);
		bool bTab = GetKeyState(VK_TAB) > 0;
		bool bControl = GetKeyState(VK_CONTROL) > 0;

		if ( bAlt && !bTab && !bControl )
			return TRUE;
	}

	return CFrameWnd::PreTranslateMessage(pMsg);
}

// ----------------------------------------------------------------------------
LRESULT MainFrame::DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	return CFrameWnd::DefWindowProc(message, wParam, lParam);
}

void MainFrame::OnPopupInsert()
{
	PageScene::instance().OnPopupInsert();
}

void MainFrame::OnPopupDelete()
{
	PageScene::instance().OnPopupDelete();
}

void MainFrame::OnPopupRename()
{
	PageScene::instance().OnPopupRename();
}

void MainFrame::OnPopupMoveSelectionHere()
{
	PageScene::instance().OnPopupMoveSelectionHere();
}

void MainFrame::OnPopupPropertyListAddItem()
{
	PageProperties::instance().OnListAddItem();
}

void MainFrame::OnPopupPropertyListItemRemoveItem()
{
	PageProperties::instance().OnListItemRemoveItem();
}

void MainFrame::OnGUIManagerCommand(UINT nID)
{
	GUI::Manager::instance()->act( nID );
}

bool MainFrame::saveSelectionAsPrefab( GUI::ItemPtr item )
{
	if( !PythonAdapter::instance()->canSavePrefab() )
	{
		BigBang::instance().addCommentaryMsg( "You must select something to save as a prefab" );
		return false;
	}
	// open a save as dialog
	// szFilters is a text string that includes two file name filters:
	// "*.my" for "MyType Files" and "*.*' for "All Files."
	char szFilters[]=
		"Prefab Files (*.prefab)|*.prefab|All Files (*.*)|*.*||";

	ShowCursorHelper scopedShowCursor( true );

	// Create an Open dialog; the default file name extension is ".prefab".
	CFileDialog fileDlg(FALSE, "prefab", "*.prefab",
		OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, szFilters, this);

	CString path = "object/";
	CString prefabStr;
	prefabStr.LoadString(IDS_PAGE_PREFAB_CAPTION);
	path += prefabStr;
	path += '/';
	std::string initialDir = Options::getOptionString( (LPCTSTR)( path + "directory" ) ).c_str();
	if (initialDir.empty())
		initialDir = BWResource::getDefaultPath() + "/";
	std::replace(initialDir.begin(), initialDir.end(), '/', '\\');
	fileDlg.m_ofn.lpstrInitialDir = initialDir.c_str();


	// Display the file dialog. When user clicks OK, fileDlg.DoModal() 
	// returns IDOK.
	static FolderSetter FolderSetter;

	FolderGuard FolderGuard( FolderSetter );

	if( fileDlg.DoModal() != IDOK )
		return true;

	CString pathName = fileDlg.GetPathName();

	PythonAdapter::instance()->saveSelectionPrefab( pathName.GetBuffer() );
	return true;
}

bool MainFrame::showToolbar( GUI::ItemPtr item )
{
	return BaseMainFrame::showToolbar( item );
}

bool MainFrame::hideToolbar( GUI::ItemPtr item )
{
	return BaseMainFrame::hideToolbar( item );
}

unsigned int MainFrame::updateToolbar( GUI::ItemPtr item )
{
	return BaseMainFrame::updateToolbar( item );
}

bool MainFrame::showStatusBar( GUI::ItemPtr item )
{
	ShowControlBar( &m_wndStatusBar, TRUE, FALSE );
	return true;
}

bool MainFrame::hideStatusBar( GUI::ItemPtr item )
{
	ShowControlBar( &m_wndStatusBar, FALSE, FALSE );
	return true;
}

unsigned int MainFrame::updateStatusBar( GUI::ItemPtr item )
{
	return ~m_wndStatusBar.GetStyle() & WS_VISIBLE;
}

bool MainFrame::showPlayerPreview( GUI::ItemPtr item )
{
	BigBang::instance().setPlayerPreviewMode( true );
	return true;
}

bool MainFrame::hidePlayerPreview( GUI::ItemPtr item )
{
	BigBang::instance().setPlayerPreviewMode( false );
	return true;
}

unsigned int MainFrame::updatePlayerPreview( GUI::ItemPtr item )
{
	return !BigBang::instance().isInPlayerPreviewMode();
}

unsigned int MainFrame::updateToolMode( GUI::ItemPtr item )
{
	return (PanelManager::instance()->currentTool() == (*item)["toolMode"]) ? 1 : 0;
}

void MainFrame::OnClose()
{
	if (BigBang::instance().canClose("E&xit"))
	{
		// this can take a while, specially if the ThumbnailManager is waiting
		// for a big model to load, but it's the safest way
		PanelManager::instance()->onClose();

		CFrameWnd::OnClose();
	}
}

void MainFrame::OnMenuSelect( UINT nItemID, UINT nFlags, HMENU hSysMenu )
{
	std::string s;
	if( ! ( nFlags & ( MF_DISABLED | MF_GRAYED | MF_SEPARATOR ) ) )
	{
		GUI::Manager::instance()->update();
		GUI::ItemPtr item = GUI::Manager::instance()->findByCommandID( nItemID );
		if( item )
		{
			s = item->description();
			while( s.find( '&' ) != s.npos )
			{
				s.erase( s.begin() + s.find( '&' ) );
			}
		}
	}
	SetMessageText( s.c_str() );
}

void MainFrame::OnExitMenuLoop( BOOL bIsTrackPopupMenu )
{
	SetMessageText( "" );
}

void MainFrame::OnToolbarHotItemChange( UINT id, NMHDR* pNotifyStruct, LRESULT* result )
{
	*result = 0;
	std::string s;
	LPNMTBHOTITEM hotitem = (LPNMTBHOTITEM)pNotifyStruct;
	GUI::ItemPtr item = GUI::Manager::instance()->findByCommandID( hotitem->idNew );
	if( item )
	{
		s = item->description();
		while( s.find( '&' ) != s.npos )
		{
			s.erase( s.begin() + s.find( '&' ) );
		}
	}
	SetMessageText( s.c_str() );
}

void MainFrame::OnSysColorChange()
{
	CFrameWnd::OnSysColorChange();
	for( unsigned int i = 0; i < numToolbars_; ++i )
		toolbars_[ i ].SendMessage( WM_SYSCOLORCHANGE, 0, 0 );
}
