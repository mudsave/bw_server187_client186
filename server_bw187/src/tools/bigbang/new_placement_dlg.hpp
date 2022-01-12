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
#include "afxwin.h"


// NewPlacementDlg dialog

class NewPlacementDlg : public CDialog
{
	DECLARE_DYNAMIC(NewPlacementDlg)

public:
// Dialog Data
	enum { IDD = IDD_RANDNEWPRESET };

	NewPlacementDlg(CWnd* pParent = NULL);
	~NewPlacementDlg();

	void existingNames(std::vector<std::string> const &names);

	CString newName_;

protected:
	CEdit newNameCtrl_;
	std::vector<std::string> existingNames_;

	void DoDataExchange(CDataExchange* pDX);

	BOOL OnInitDialog();
	void OnOK();

	DECLARE_MESSAGE_MAP()

	bool isValidName(CString const &str) const;
};
