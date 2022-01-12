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
#include "sheet_terrain.hpp"
#include "page_terrain_filter.hpp"
#include "page_terrain_height.hpp"
#include "page_terrain_mesh.hpp"
#include "page_terrain_texture.hpp"
#include "user_messages.hpp"


IMPLEMENT_DYNAMIC(SheetTerrain, CPropertySheet)

BEGIN_MESSAGE_MAP(SheetTerrain, CPropertySheet)
	ON_MESSAGE(WM_RESIZEPAGE, OnResizePage)
	ON_MESSAGE(WM_UPDATE_CONTROLS, OnUpdateControls)
END_MESSAGE_MAP()

SheetTerrain::SheetTerrain(CWnd* pWndParent)
	: CPropertySheet(AFX_IDS_APP_TITLE, pWndParent)
{
	PageTerrainTexture* texturePage = new PageTerrainTexture();
	AddPage(texturePage);

	PageTerrainHeight* heightPage = new PageTerrainHeight();
	AddPage(heightPage);

	PageTerrainFilter* filterPage = new PageTerrainFilter();
	AddPage(filterPage);

	PageTerrainMesh* meshPage = new PageTerrainMesh();
	AddPage(meshPage);
}

SheetTerrain::~SheetTerrain()
{
	while ( GetPageCount() != 0)
	{
		CPropertyPage* tempPage = GetPage(0);
		RemovePage( tempPage );
		delete tempPage;
	}
}

afx_msg LRESULT SheetTerrain::OnResizePage(WPARAM wParam, LPARAM lParam)
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

	// refresh pages
	SetActivePage(GetActiveIndex());

	return 0;
}

afx_msg LRESULT SheetTerrain::OnUpdateControls(WPARAM wParam, LPARAM lParam)
{
	CPropertyPage* page = GetActivePage();
	if (!page)
		return 0;

	page->SendMessage(WM_UPDATE_CONTROLS, 0, 0);
	return 0;
}
