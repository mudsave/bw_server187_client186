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

class SheetOptions;


class PageOptions : public CPropertyPage
{
	DECLARE_DYNAMIC(PageOptions)

public:
	PageOptions();
	virtual ~PageOptions();

// Dialog Data
	enum { IDD = IDD_PAGE_OPTIONS };

	SheetOptions* sheetOptions_;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg LRESULT OnResizePage(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnUpdateControls(WPARAM wParam, LPARAM lParam);
	virtual BOOL OnSetActive();
};
