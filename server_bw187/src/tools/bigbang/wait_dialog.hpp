/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// Simple class to show a wait info message centered in the screen, or under
// the splash screen if it's visible

#pragma once

#include "resource.h"

// WaitDlg dialog

class WaitDlg : public CDialog
{
	DECLARE_DYNAMIC(WaitDlg)

public:
// Dialog Data
	enum { IDD = IDD_WAITDLG };

	WaitDlg(CWnd* pParent = NULL);
	virtual ~WaitDlg();

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnTimer(UINT nIDEvent);

	static WaitDlg* s_dlg_;

public:
	static void show( const std::string& info );
	static void hide( int delay = 0 );
};
