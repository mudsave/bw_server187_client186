#pragma once

#include "resource.h"
#include "afxwin.h"
#include "controls/auto_tooltip.hpp"

// NewSpace dialog

class NewSpaceDlg : public CDialog
{
	DECLARE_DYNAMIC(NewSpaceDlg)

	DECLARE_AUTO_TOOLTIP( NewSpaceDlg, CDialog )

public:
	NewSpaceDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~NewSpaceDlg();

// Dialog Data
	enum { IDD = IDD_NEWSPACE };

private:
	CString createdSpace_;

	void validateFile( CEdit& ctrl, bool isPath );
	bool validateDim( CEdit& edit );
	void validateDimChars( CEdit& edit );
	void formatChunkToKms( CString& str );

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CEdit space_;
	CEdit width_;
	CEdit height_;
	CProgressCtrl progress_;
	CString defaultSpace_;
	std::string defaultSpacePath_;
	int mChangeSpace;
	CStatic widthKms_;
	CStatic heightKms_;

	CString createdSpace() { return createdSpace_; }

	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedOk();
	afx_msg void OnEnChangeSpace();
	afx_msg void OnEnChangeSpacePath();
	afx_msg void OnBnClickedNewspaceBrowse();
	afx_msg void OnEnChangeWidth();
	afx_msg void OnEnChangeHeight();
	CEdit spacePath_;
	CButton buttonCancel_;
	CButton buttonCreate_;
};
