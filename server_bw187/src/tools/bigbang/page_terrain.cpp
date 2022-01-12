/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// page_terrain.cpp : implementation file
//

#include "pch.hpp"
#include "bigbang.h"
#include "page_terrain.hpp"
#include "python_adapter.hpp"
#include "user_messages.hpp"

// PageTerrain

IMPLEMENT_DYNAMIC(PageTerrain, CPropertyPage)
PageTerrain::PageTerrain()
	: CPropertyPage(PageTerrain::IDD)
{
}

PageTerrain::~PageTerrain()
{
}

void PageTerrain::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
}


BOOL PageTerrain::OnSetActive()
{
	CString name;
	GetWindowText(name);
	if (PythonAdapter::instance())
		PythonAdapter::instance()->onPageControlTabSelect("pgc", name.GetBuffer());

	OnResizePage(0, 0);

	return CPropertyPage::OnSetActive();
}


BEGIN_MESSAGE_MAP(PageTerrain, CPropertyPage)
	ON_MESSAGE(WM_RESIZEPAGE, OnResizePage)
	ON_MESSAGE(WM_UPDATE_CONTROLS, OnUpdateControls)
END_MESSAGE_MAP()


// PageTerrain message handlers

BOOL PageTerrain::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	// create the property sheet within the placeholder window control
	CWnd* propSheetWnd = GetDlgItem(IDC_PROPSHEET_TERRAIN_PLACEHOLDER);
	sheetTerrain_ = new SheetTerrain(propSheetWnd);
	if (!sheetTerrain_->Create(propSheetWnd, WS_CHILD | WS_VISIBLE, 0))
	{
		delete sheetTerrain_;
		sheetTerrain_ = NULL;
		return FALSE;
	}

	// fit the property sheet into the placeholder window control
	CRect rectPropSheet;
	propSheetWnd->GetWindowRect(rectPropSheet);
	sheetTerrain_->SetWindowPos(NULL, 0, 0, rectPropSheet.Width(), 
		rectPropSheet.Height(), SWP_NOZORDER | SWP_NOACTIVATE);

	SetRedraw();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

afx_msg LRESULT PageTerrain::OnResizePage(WPARAM wParam, LPARAM lParam)
{
	// resize the property pane to correspond with the size of the wnd
	CWnd* propSheetWnd = GetDlgItem(IDC_PROPSHEET_TERRAIN_PLACEHOLDER);
	if (!propSheetWnd)
		return 0;

	CRect rectPage;
	GetWindowRect(rectPage);

	propSheetWnd->SetWindowPos(NULL, 
		0, 0,
		rectPage.Width(), rectPage.Height(),
		SWP_NOZORDER | SWP_NOACTIVATE);

	if (!sheetTerrain_)
		return 0;

	sheetTerrain_->PostMessage(WM_RESIZEPAGE, 0, 0);

	return 0;
}

afx_msg LRESULT PageTerrain::OnUpdateControls(WPARAM wParam, LPARAM lParam)
{
	sheetTerrain_->SendMessage(WM_UPDATE_CONTROLS, 0, 0);
	return 0;
}
