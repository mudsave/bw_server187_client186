/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#pragma once

class PageObject;
class PageTerrain;
class PageProperties;
class PageScene;
class PageOptions;
class PageProject;
class PageErrors;

#include "user_messages.hpp"


class SheetMenu : public CPropertySheet
{
	DECLARE_DYNAMIC(SheetMenu)

public:
	SheetMenu(CWnd* pParentWnd = NULL);
	SheetMenu(UINT nIDCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
	SheetMenu(LPCTSTR pszCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
	virtual ~SheetMenu();

protected:
	DECLARE_MESSAGE_MAP()

	PageObject* pageObject_;
	PageTerrain* pageTerrain_;
	PageProperties* pageProperties_;
	PageScene* pageScene_;
	PageOptions* pageOptions_;
	PageProject* pageProject_;
	PageErrors* pageErrors_;

public:
	virtual BOOL OnInitDialog();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg LRESULT OnResizePage(WPARAM wParam, LPARAM lParam);
	afx_msg void OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized);
	afx_msg LRESULT OnUpdateControls(WPARAM wParam, LPARAM lParam);
};
