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

#include "sheet_terrain.hpp"


// PageTerrain

class PageTerrain : public CPropertyPage
{
	DECLARE_DYNAMIC(PageTerrain)

public:
	PageTerrain();
	virtual ~PageTerrain();

	SheetTerrain* sheetTerrain_;

// Dialog Data
	enum { IDD = IDD_PAGE_TERRAIN };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnSetActive();
	virtual BOOL OnInitDialog();
	afx_msg LRESULT OnResizePage(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnUpdateControls(WPARAM wParam, LPARAM lParam);
};
