// bigbang2.h : main header file for the bigbang2 application
//
#pragma once

#ifndef __AFXWIN_H__
#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols

class PythonAdapter;


// BigBang2App:
// See bigbang2.cpp for the implementation of this class
//

class BigBang2App : public CWinApp
{
public:
	BigBang2App();
	~BigBang2App();

	static BigBang2App & instance() { return *s_instance_; }
	CWnd* mainWnd() { return m_pMainWnd; }
	App* mfApp() { return s_mfApp; }
	bool panelsReady();

private:
	static BigBang2App * s_instance_;
	App* s_mfApp;
	PythonAdapter* pythonAdapter_;
	HANDLE updateMailSlot_;

	BOOL parseCommandLineMF();


// Overrides
public:
	virtual BOOL InitInstance();

// Implementation
	DECLARE_MESSAGE_MAP()
	virtual BOOL OnIdle(LONG lCount);
	virtual int ExitInstance();
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
};

extern BigBang2App theApp;