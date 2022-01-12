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
#include "sheet_options.hpp"
#include "page_options_general.hpp"
#include "page_options_weather.hpp"
#include "page_options_histogram.hpp"
#include "appmgr/options.hpp"

IMPLEMENT_DYNAMIC(SheetOptions, CPropertySheet)

SheetOptions::SheetOptions(CWnd* pWndParent)
	: CPropertySheet(AFX_IDS_APP_TITLE, pWndParent)
{
//	PageOptionsGeneral* newPageOG = new PageOptionsGeneral();
//	AddPage(newPageOG);
//	PageOptionsWeather* newPageOW = new PageOptionsWeather();
//	AddPage(newPageOW);
//	PageOptionsHistogram* newPageHT = new PageOptionsHistogram();
//	AddPage(newPageHT);
}

SheetOptions::~SheetOptions()
{
	while ( GetPageCount() != 0)
	{
		CPropertyPage* tempPage = GetPage(0);
		RemovePage( tempPage );
		delete tempPage;
	}
}

BEGIN_MESSAGE_MAP(SheetOptions, CPropertySheet)
	ON_MESSAGE( WM_RESIZEPAGE, OnResizePage )
END_MESSAGE_MAP()

afx_msg LRESULT SheetOptions::OnResizePage(WPARAM wParam, LPARAM lParam)
{
 	// fit the property sheet into the place holder window, and show it
	CRect rect;
	
	// size the sheet
	GetParent()->GetClientRect(&rect);
	MoveWindow(rect);

	// size the tab control
	CTabCtrl* tab = GetTabControl();
	if (!tab)
		return 0;
	tab->AdjustRect(FALSE, &rect);
	rect.top = 4;
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
