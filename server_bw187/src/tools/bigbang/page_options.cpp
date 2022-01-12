/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// page_object.cpp : implementation file
//

#include "pch.hpp"
#include "bigbang.h"
#include "page_options.hpp"
#include "sheet_options.hpp"
#include "python_adapter.hpp"

IMPLEMENT_DYNAMIC(PageOptions, CPropertyPage)
PageOptions::PageOptions()
	: CPropertyPage(PageOptions::IDD)
	, sheetOptions_(NULL)
{
}

PageOptions::~PageOptions()
{
}

void PageOptions::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
}


BOOL PageOptions::OnSetActive()
{
	CString name;
	GetWindowText(name);
	if (PythonAdapter::instance())
		PythonAdapter::instance()->onPageControlTabSelect("pgc", name.GetBuffer());

	OnResizePage(0, 0);

	return CPropertyPage::OnSetActive();
}


BEGIN_MESSAGE_MAP(PageOptions, CPropertyPage)
	ON_WM_SIZE()
	ON_MESSAGE (WM_RESIZEPAGE, OnResizePage)
	ON_MESSAGE(WM_UPDATE_CONTROLS, OnUpdateControls)
END_MESSAGE_MAP()


// PageOptions message handlers

BOOL PageOptions::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	// create the property sheet within the placeholder window control
	CWnd* propSheetWnd = GetDlgItem(IDC_PROPSHEET_OPTION_PLACEHOLDER);
	sheetOptions_ = new SheetOptions(propSheetWnd);
	if (!sheetOptions_->Create(propSheetWnd, WS_CHILD | WS_VISIBLE, 0))
	{
		delete sheetOptions_;
		sheetOptions_ = NULL;
		return FALSE;
	}

	// fit the property sheet into the placeholder window control
	CRect rectPropSheet;
	propSheetWnd->GetWindowRect(rectPropSheet);
	sheetOptions_->SetWindowPos(NULL, 0, 0, rectPropSheet.Width(), 
		rectPropSheet.Height(), SWP_NOZORDER | SWP_NOACTIVATE);

	SetRedraw();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void PageOptions::OnSize(UINT nType, int cx, int cy)
{
	CPropertyPage::OnSize(nType, cx, cy);
}

afx_msg LRESULT PageOptions::OnResizePage(WPARAM wParam, LPARAM lParam)
{
	// resize the property pane to correspond with the size of the wnd
	CWnd* propSheetWnd = GetDlgItem(IDC_PROPSHEET_OPTION_PLACEHOLDER);
	if (!propSheetWnd)
		return 0;

	CRect rectPage;
	GetWindowRect(rectPage);

	propSheetWnd->SetWindowPos(NULL, 
		0, 0,
		rectPage.Width(), rectPage.Height(),
		SWP_NOZORDER | SWP_NOACTIVATE);

	if (!sheetOptions_)
		return 0;

	sheetOptions_->PostMessage(WM_RESIZEPAGE, 0, 0);

	return 0;
}

afx_msg LRESULT PageOptions::OnUpdateControls(WPARAM wParam, LPARAM lParam)
{
	CPropertyPage* page = sheetOptions_->GetActivePage();

	if( page )
		page->SendMessage(WM_UPDATE_CONTROLS, 0, 0);
	
	return 0;
}
