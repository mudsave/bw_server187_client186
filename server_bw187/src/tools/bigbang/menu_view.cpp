/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// menu_view.cpp : implementation file
//

#include "pch.hpp"
#include "bigbang.h"
#include "menu_view.hpp"
#include "sheet_menu.hpp"
#include "user_messages.hpp"

// MenuView

IMPLEMENT_DYNCREATE(MenuView, CFormView)

MenuView::MenuView()
	: CFormView(MenuView::IDD)
	, sheetMenu_(NULL)
	, myPropSheet_(NULL)
{
}

MenuView::~MenuView()
{
	if (sheetMenu_)
	{
		delete sheetMenu_;
	}
}

void MenuView::DoDataExchange(CDataExchange* pDX)
{
	CFormView::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(MenuView, CFormView)
	ON_WM_SIZE()
	ON_MESSAGE(WM_UPDATE_CONTROLS, OnUpdateControls)
END_MESSAGE_MAP()


// MenuView diagnostics

#ifdef _DEBUG
void MenuView::AssertValid() const
{
	CFormView::AssertValid();
}

void MenuView::Dump(CDumpContext& dc) const
{
	CFormView::Dump(dc);
}
#endif //_DEBUG


// MenuView message handlers

BOOL MenuView::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext)
{
	// TODO: Add your specialized code here and/or call the base class
	return CFormView::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext);
}

void MenuView::OnInitialUpdate()
{
	CFormView::OnInitialUpdate();

	// property testing
	CWnd* pwndPropSheetHolder = GetDlgItem(IDC_PROPSHEET_PLACEHOLDER);
	myPropSheet_ = new SheetMenu(pwndPropSheetHolder);
	if (!myPropSheet_->Create(pwndPropSheetHolder, 
		WS_CHILD | WS_VISIBLE, 0))
	{
		delete myPropSheet_;
		myPropSheet_ = NULL;
		ASSERT(0);
	}

	myPropSheet_->PostMessage(WM_RESIZEPAGE, 0, 0);
}

void MenuView::ShowPage( int idx )
{
	MF_ASSERT( myPropSheet_ );
	myPropSheet_->SetActivePage(idx);
}

void MenuView::OnSize(UINT nType, int cx, int cy)
{
	CFormView::OnSize(nType, cx, cy );

	// resize the property pane to correspond with the size of the wnd
	CWnd* propSheetWnd = GetDlgItem(IDC_PROPSHEET_PLACEHOLDER);
	if (!propSheetWnd)
		return;

	CRect rectPage;
	GetWindowRect(rectPage);
	ScreenToClient(rectPage);

	// TODO: work out why these negative numbers are needed
	propSheetWnd->SetWindowPos(NULL, 
		-2, -12,
		rectPage.Width() + 6, rectPage.Height() + 12,
		SWP_NOZORDER | SWP_NOACTIVATE);

	if (!myPropSheet_)
		return;

	myPropSheet_->PostMessage(WM_RESIZEPAGE, 0, 0);
}

afx_msg LRESULT MenuView::OnUpdateControls(WPARAM wParam, LPARAM lParam)
{
	myPropSheet_->SendMessage(WM_UPDATE_CONTROLS, 0, 0);
	return 0;
}
