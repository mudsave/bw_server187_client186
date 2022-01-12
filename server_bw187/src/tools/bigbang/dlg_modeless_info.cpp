/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "pch.hpp"
#include "dlg_modeless_info.hpp"

CModelessInfoDialog::CModelessInfoDialog( const std::string& title, const std::string& msg ) :
CDialog(CModelessInfoDialog::IDD),
msg_( msg ),
title_( title )
{
	Create( IDD_MODELESS_INFO );
	//CenterWindow();
	//RedrawWindow();
}

CModelessInfoDialog::~CModelessInfoDialog()
{
	DestroyWindow();
}

void CModelessInfoDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_INFO, infoText_);
}

BOOL CModelessInfoDialog::OnInitDialog() 
{
   CDialog::OnInitDialog();
   
   SetWindowText(title_.c_str());
   infoText_.SetWindowText(msg_.c_str());

   return TRUE;
}

BEGIN_MESSAGE_MAP(CModelessInfoDialog, CDialog)
END_MESSAGE_MAP()