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

class SheetMenu;
class SheetMenu;

// MenuView form view

class MenuView : public CFormView
{
	DECLARE_DYNCREATE(MenuView)

protected:
	MenuView();           // protected constructor used by dynamic creation
	virtual ~MenuView();

public:
	enum { IDD = IDD_MENUVIEW };
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
	void	ShowPage( int idx );

	SheetMenu* sheetMenu_;
	SheetMenu* myPropSheet_;

	virtual void OnInitialUpdate();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg LRESULT OnUpdateControls(WPARAM wParam, LPARAM lParam);
};
