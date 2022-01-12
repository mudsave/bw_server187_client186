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

#include "pch.hpp"

#include "splash_dialog.hpp"
#include "wait_dialog.hpp"


const int WAITDLG_TIMER_ID = 1971; // Arbitary choice


// WaitDlg dialog

WaitDlg* WaitDlg::s_dlg_ = NULL;


IMPLEMENT_DYNAMIC(WaitDlg, CDialog)
WaitDlg::WaitDlg(CWnd* pParent /*=NULL*/)
	: CDialog(WaitDlg::IDD, pParent)
{
}

WaitDlg::~WaitDlg()
{
}

BEGIN_MESSAGE_MAP(WaitDlg, CDialog)
	ON_WM_TIMER()
END_MESSAGE_MAP()

void WaitDlg::show( const std::string& info )
{
	AfxGetMainWnd()->UpdateWindow();
	if ( !s_dlg_ )
	{
		s_dlg_ = new WaitDlg;
		if ( !s_dlg_->Create( WaitDlg::IDD, AfxGetMainWnd() ) )
		{
			delete s_dlg_;
			return;
		}

		CRect rect;
		if ( CSplashDlg::visibleInfo( &rect ) )
		{
			s_dlg_->SetWindowPos(
				&CWnd::wndTopMost,
				rect.left, rect.bottom,
				rect.Width(), 25,
				0 );
		}
		else
			s_dlg_->CenterWindow();
	}
	else
	{
		s_dlg_->KillTimer( WAITDLG_TIMER_ID );
	}

	s_dlg_->GetDlgItem( IDC_WAITDLG_INFO )->SetWindowText( info.c_str() );
	s_dlg_->ShowWindow( SW_SHOW );
	s_dlg_->UpdateWindow();
}

void WaitDlg::hide( int delay )
{
	if( !s_dlg_ )
		return;

	if ( delay )
		s_dlg_->SetTimer( WAITDLG_TIMER_ID, delay, NULL );
	else
	{
		s_dlg_->KillTimer( WAITDLG_TIMER_ID );
		s_dlg_->DestroyWindow();
		AfxGetMainWnd()->UpdateWindow();
		delete s_dlg_;
		s_dlg_ = NULL;
	}
}

void WaitDlg::OnTimer(UINT nIDEvent)
{
	hide( 0 );
}
