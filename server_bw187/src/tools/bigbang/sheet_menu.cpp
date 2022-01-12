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
#include "bigbang.h"
#include "sheet_menu.hpp"

#include "page_object.hpp"
#include "page_terrain.hpp"
#include "page_properties.hpp"
#include "page_scene.hpp"
#include "page_options.hpp"
#include "page_project.hpp"
#include "page_errors.hpp"


// SheetMenu

IMPLEMENT_DYNAMIC(SheetMenu, CPropertySheet)
SheetMenu::SheetMenu(CWnd* pParentWnd)
	:CPropertySheet(IDS_SHEET_MENU, pParentWnd)
{

//	pageObject_ = new PageObject;
//	pageTerrain_ = new PageTerrain;
	//pageScene_ = new PageScene;
//	pageOptions_ = new PageOptions;
//	pageProject_ = new PageProject;
//	pageErrors_ = new PageErrors;
//	pageProperties_ = new PageProperties;

//	AddPage(pageObject_);
//	AddPage(pageTerrain_);
//	AddPage(pageProperties_);
	//AddPage(pageScene_);
//	AddPage(pageOptions_);
//	AddPage(pageProject_);
//	AddPage(pageErrors_);

//	pageErrors_->addError("Hello world.  What nice weather.  What nice music.  Hello little butterfly! Hello polar bear!  Hello there nice lady", NULL, NULL);
}

SheetMenu::SheetMenu(UINT nIDCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(nIDCaption, pParentWnd, iSelectPage)
{
}

SheetMenu::SheetMenu(LPCTSTR pszCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(pszCaption, pParentWnd, iSelectPage)
{
}

SheetMenu::~SheetMenu()
{
	delete pageErrors_;
	delete pageProject_;
	delete pageOptions_;
	//delete pageScene_;
	delete pageTerrain_;
	delete pageObject_;
}


BEGIN_MESSAGE_MAP(SheetMenu, CPropertySheet)
	ON_WM_SIZE()
	ON_MESSAGE(WM_RESIZEPAGE, OnResizePage)
	ON_WM_ACTIVATE()
	ON_MESSAGE(WM_UPDATE_CONTROLS, OnUpdateControls)
END_MESSAGE_MAP()


// SheetMenu message handlers

BOOL SheetMenu::OnInitDialog()
{
	BOOL bResult = CPropertySheet::OnInitDialog();

	return bResult;
}

void SheetMenu::OnSize(UINT nType, int cx, int cy)
{
	CPropertySheet::OnSize(nType, cx, cy);

	// TODO: Add your message handler code here
}

afx_msg LRESULT SheetMenu::OnResizePage(WPARAM wParam, LPARAM lParam)
{
 	// size the property sheet to fit into the parent window
	CRect rect;
	GetParent()->GetClientRect(&rect);
	MoveWindow(rect);

	// size the tab control
	CTabCtrl* tab = GetTabControl();
	if (!tab)
		return 0;
	tab->AdjustRect(FALSE, &rect);
	rect.top = 14;
	tab->MoveWindow(rect, TRUE);

	// resize the page
	CPropertyPage* page = GetActivePage();
	if (!page)
		return 0;

	page->PostMessage(WM_RESIZEPAGE, 0, 0);

	// refresh
	SetActivePage(GetActiveIndex());

	return 0;
}


void SheetMenu::OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized)
{
	CPropertySheet::OnActivate(nState, pWndOther, bMinimized);

	// TODO: Add your message handler code here
}

afx_msg LRESULT SheetMenu::OnUpdateControls(WPARAM wParam, LPARAM lParam)
{
	CPropertyPage* page = GetActivePage();
	if (!page)
		return 0;

	page->SendMessage(WM_UPDATE_CONTROLS, 0, 0);
	return 0;
}