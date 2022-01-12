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
#include "afxwin.h"
#include "controls/show_cursor_helper.hpp"


// LowMemoryDlg dialog

class LowMemoryDlg : public CDialog
{
	DECLARE_DYNAMIC(LowMemoryDlg)

public:
	LowMemoryDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~LowMemoryDlg();

// Dialog Data
	enum { IDD = IDD_OUT_OF_MEMORY };

protected:
	ShowCursorHelper scopedShowCursor_;

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CButton mOK;
public:
	afx_msg void OnBnClickedContinue();
public:
	afx_msg void OnBnClickedSave();
public:
	afx_msg void OnBnClickedOk();
public:
	virtual void OnCancel();
};
