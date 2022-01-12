/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// NewPlacementDlg.cpp : implementation file
//
#include "pch.hpp"
#include "new_placement_dlg.hpp"

// NewPlacementDlg dialog

IMPLEMENT_DYNAMIC(NewPlacementDlg, CDialog)

NewPlacementDlg::NewPlacementDlg(CWnd* pParent)
	: CDialog(NewPlacementDlg::IDD, pParent)
	, newName_(_T(""))
{
}

NewPlacementDlg::~NewPlacementDlg()
{}

void NewPlacementDlg::existingNames(std::vector<std::string> const &names)
{
	existingNames_ = names;
}

BOOL NewPlacementDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	return TRUE;
}

void NewPlacementDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_NEWPLACEMENTTEXT, newName_);
	DDV_MaxChars(pDX, newName_, 40);
	DDX_Control(pDX, IDC_NEWPLACEMENTTEXT, newNameCtrl_);
}

// Messages

BEGIN_MESSAGE_MAP(NewPlacementDlg, CDialog)
END_MESSAGE_MAP()

void NewPlacementDlg::OnOK()
{
	CString str;
	newNameCtrl_.GetWindowText( str );
	if (!isValidName(str))
	{
		MessageBox( "The entered text is not a valid preset name.\n"
			"It should not match any existing preset names and\n"
			"should not be empty or contain any of the characters \"<>/\\:!@#$%^&*()\".",
			"Placement Preset Name Error", MB_ICONERROR );
		newNameCtrl_.SetFocus();
		return;
	}

	CDialog::OnOK();
}


bool NewPlacementDlg::isValidName(CString const &str) const
{
	CString s = str;
	s.Trim();
	// Empty names are not allowed:
	if (s.IsEmpty())
		return false;
	// A whole bunch of special characters are not allowed:
	if (s.FindOneOf("<>/\\:!@#$%^&*()") != -1)
		return false;
	// Names cannot match existing names, unless name is the same as newName_
	// (which occurs when the user does a rename but changes nothing):
	CString newName = newName_;
	for (size_t i = 0; i < existingNames_.size(); ++i)
	{
		if 
		(
			existingNames_[i] == s.GetBuffer() 
			&& 
			existingNames_[i] != newName.GetBuffer()
		)
		{
			return false;
		}
	}
	// Must be ok
	return true;
}
