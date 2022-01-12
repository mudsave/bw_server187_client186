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

#include "resource.h"

// CModelessInfoDialog

class CModelessInfoDialog : public CDialog
{
public:
	CModelessInfoDialog( const std::string& title, const std::string& msg );
	~CModelessInfoDialog();

// Dialog Data
	enum { IDD = IDD_MODELESS_INFO };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL CModelessInfoDialog::OnInitDialog();

	 CStatic infoText_;
	 std::string msg_;
	 std::string title_;

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};
