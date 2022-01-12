/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// page_project.cpp : implementation file
//

#include "pch.hpp"
#include "bigbang.h"
#include "big_bang.hpp"
#include "appmgr/options.hpp"
#include "guimanager/gui_manager.hpp"
#include "page_project.hpp"
#include "user_messages.hpp"
#include "python_adapter.hpp"
#include "project/project_module.hpp"



// PageProject


// GUITABS content ID ( declared by the IMPLEMENT_BASIC_CONTENT macro )
const std::string PageProject::contentID = "PageProject";


PageProject::PageProject()
	: CFormView(PageProject::IDD),
	pageReady_( false )
{
}

PageProject::~PageProject()
{
}

void PageProject::DoDataExchange(CDataExchange* pDX)
{
	CFormView::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PROJECT_MAP_ALPHA_SLIDER, blendSlider_);
	DDX_Control(pDX, IDC_PROJECT_SELECTION_LOCK, selectionLock_);
	DDX_Control(pDX, IDC_PROJECT_COMMIT_MESSAGE, commitMessage_);
	DDX_Control(pDX, IDC_PROJECT_COMMIT_KEEPLOCKS, commitKeepLocks_);
	DDX_Control(pDX, IDC_PROJECT_COMMIT_ALL, commitAll_);
	DDX_Control(pDX, IDC_PROJECT_DISCARD_KEEPLOCKS, discardKeepLocks_);
	DDX_Control(pDX, IDC_PROJECT_DISCARD_ALL, discardAll_);
	DDX_Control(pDX, IDC_CALCULATEDMAP, mCalculatedMap);
	DDX_Control(pDX, IDC_PROJECT_UPDATE, update_);
}

void PageProject::InitPage()
{
	INIT_AUTO_TOOLTIP();
	blendSlider_.SetRangeMin(1);
	blendSlider_.SetRangeMax(100);
	blendSlider_.SetPageSize(0);

	commitMessage_.SetLimitText(1000);
	commitKeepLocks_.SetCheck(BST_CHECKED);
	discardKeepLocks_.SetCheck(BST_UNCHECKED);

	OnEnChangeProjectCommitMessage();

	commitMessage_.SetLimitText( 512 );

	pageReady_ = true;
}


BEGIN_MESSAGE_MAP(PageProject, CFormView)
	ON_WM_SETFOCUS()
	ON_MESSAGE (WM_ACTIVATE_TOOL, OnActivateTool)
	ON_MESSAGE(WM_UPDATE_CONTROLS, OnUpdateControls)
	ON_WM_HSCROLL()
	ON_BN_CLICKED(IDC_PROJECT_SELECTION_LOCK, OnBnClickedProjectSelectionLock)
	ON_BN_CLICKED(IDC_PROJECT_COMMIT_ALL, OnBnClickedProjectCommitAll)
	ON_BN_CLICKED(IDC_PROJECT_DISCARD_ALL, OnBnClickedProjectDiscardAll)
	ON_EN_CHANGE(IDC_PROJECT_COMMIT_MESSAGE, OnEnChangeProjectCommitMessage)
	ON_BN_CLICKED(IDC_PROJECT_UPDATE, &PageProject::OnBnClickedProjectUpdate)
END_MESSAGE_MAP()


// PageProject message handlers

void PageProject::OnSetFocus( CWnd* pOldWnd )
{
	//if ( BigBang2App::instance().panelsReady() )
	//{
	//	BigBang::instance().updateUIToolMode( PageProject::contentID );
	//	OnActivateTool( 0, 0 );
 //       updateMode();
	//}
}

LRESULT PageProject::OnActivateTool(WPARAM wParam, LPARAM lParam)
{
	if (PythonAdapter::instance())
		PythonAdapter::instance()->onPageControlTabSelect("pgc", "Project");

	return 0;
}

void PageProject::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	// this captures all the scroll events for the page
	if ( !pageReady_ )
		InitPage();

	if (PythonAdapter::instance())
	{
		PythonAdapter::instance()->onSliderAdjust("slrProjectMapBlend", 
												(float)blendSlider_.GetPos(), 
												(float)blendSlider_.GetRangeMin(), 
												(float)blendSlider_.GetRangeMax());		
	}

	CFormView::OnHScroll(nSBCode, nPos, pScrollBar);
}


afx_msg LRESULT PageProject::OnUpdateControls(WPARAM wParam, LPARAM lParam)
{
	if ( !pageReady_ )
		InitPage();

	if ( !IsWindowVisible() || !ProjectModule::currentInstance() )
		return 0;

	MF_ASSERT(PythonAdapter::instance());
	PythonAdapter::instance()->sliderUpdate(&blendSlider_, "slrProjectMapBlend");

	bool connected = BigBang::instance().connection() != NULL;
	if( !connected )
	{
		if( commitMessage_.IsWindowEnabled() )
		{
			commitMessage_.SetWindowText( "failed to connect to bwlockd, you cannot use the related functionality." );
			commitMessage_.EnableWindow( FALSE );
			selectionLock_.EnableWindow( FALSE );
			commitAll_.EnableWindow( FALSE );
			discardAll_.EnableWindow( FALSE );
		}
	}
	else
	{
		if( !commitMessage_.IsWindowEnabled() )
		{
			commitMessage_.SetWindowText( "" );
			commitMessage_.EnableWindow( TRUE );
		}
		selectionLock_.EnableWindow( ProjectModule::currentInstance()->isReadyToLock() );
		commitAll_.EnableWindow( ProjectModule::currentInstance()->isReadyToCommitOrDiscard());
		discardAll_.EnableWindow( ProjectModule::currentInstance()->isReadyToCommitOrDiscard());
	}
	return 0;
}

void PageProject::OnBnClickedProjectSelectionLock()
{
	CString commitMessage;
	commitMessage_.GetWindowText(commitMessage);
	if( commitMessage.GetLength() == 0 )
	{
		AfxMessageBox( "You must enter a message before lock", MB_OK );
		commitMessage_.SetFocus();
	}
	else
		PythonAdapter::instance()->projectLock( commitMessage.GetBuffer() );
}

void PageProject::OnBnClickedProjectCommitAll()
{
	const bool keepLocks = commitKeepLocks_.GetCheck() == BST_CHECKED;

	CString commitMessage;
	commitMessage_.GetWindowText(commitMessage);
	if( commitMessage.GetLength() == 0 )
	{
		AfxMessageBox( "You must enter a message before commit", MB_OK );
		commitMessage_.SetFocus();
	}
	else
	{
		MF_ASSERT(PythonAdapter::instance());
		PythonAdapter::instance()->commitChanges(commitMessage.GetBuffer(), keepLocks);
		commitMessage_.SetWindowText("");
	}
}

void PageProject::OnBnClickedProjectDiscardAll()
{
	const bool keepLocks = discardKeepLocks_.GetCheck() == BST_CHECKED;

	CString commitMessage;
	commitMessage_.GetWindowText(commitMessage);
	if( commitMessage.GetLength() == 0 )
		commitMessage = "(Discard)";
	PythonAdapter::instance()->discardChanges(commitMessage.GetBuffer(), keepLocks);
	commitMessage_.SetWindowText("");
}

void PageProject::OnEnChangeProjectCommitMessage()
{
	CString commitMessage;
	commitMessage_.GetWindowText( commitMessage );
	selectionLock_.EnableWindow( commitMessage.GetLength() && ProjectModule::currentInstance()->isReadyToLock() );
	commitAll_.EnableWindow( commitMessage.GetLength() && ProjectModule::currentInstance()->isReadyToCommitOrDiscard());
	discardAll_.EnableWindow( commitMessage.GetLength() && ProjectModule::currentInstance()->isReadyToCommitOrDiscard());
}

void PageProject::OnBnClickedProjectUpdate()
{
	MF_ASSERT(PythonAdapter::instance());
	PythonAdapter::instance()->updateSpace();
}
