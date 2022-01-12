/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// bigbang2view.cpp : implementation of the BigBang2View class
//

#include "pch.hpp"
#include "bigbang.h"
#include "mainframe.hpp"
#include "big_bang.hpp"

#include "bigbangdoc.hpp"
#include "bigbangview.hpp"
#include "appmgr/module_manager.hpp"

#include "input/input.hpp"

#include "common/cooperative_moo.hpp"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// BigBang2View

IMPLEMENT_DYNCREATE(BigBang2View, CView)

BEGIN_MESSAGE_MAP(BigBang2View, CView)
	ON_WM_SIZE()
	ON_WM_PAINT()
	ON_WM_SETCURSOR()
END_MESSAGE_MAP()

// BigBang2View construction/destruction

BigBang2View::BigBang2View() :
	lastRect_( 0, 0, 0, 0 )
{
	// TODO: add construction code here

}

BigBang2View::~BigBang2View()
{
}

BOOL BigBang2View::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	// changing style to no-background to avoid flicker in the 3D view.
	cs.lpszClass = AfxRegisterWndClass(
		CS_OWNDC | CS_HREDRAW | CS_VREDRAW, ::LoadCursor(NULL, IDC_ARROW), 0 );
	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	cs.style &= ~WS_BORDER;
	return CView::PreCreateWindow(cs);
}

// BigBang2View drawing

void BigBang2View::OnDraw(CDC* /*pDC*/)
{
	BigBang2Doc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	// TODO: add draw code for native data here
}


// BigBang2View diagnostics

#ifdef _DEBUG
void BigBang2View::AssertValid() const
{
	CView::AssertValid();
}

void BigBang2View::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

BigBang2Doc* BigBang2View::GetDocument() const // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(BigBang2Doc)));
	return (BigBang2Doc*)m_pDocument;
}
#endif //_DEBUG


// BigBang2View message handlers

void BigBang2View::OnSize( UINT nType, int cx, int cy )
{
	CView::OnSize( nType, cx, cy );
//	No longer changing Moo mode here, since it's too slow
	//if ((cx > 0 && cy > 0) && Moo::rc().device() && Moo::rc().windowed())
	//{
	//	Moo::rc().changeMode(Moo::rc().modeIndex(), Moo::rc().windowed(), true);
	//}
}

void BigBang2View::OnPaint()
{
	CView::OnPaint();

	CRect rect;
	GetClientRect( &rect );

	if ( BigBang2App::instance().mfApp() &&
		ModuleManager::instance().currentModule() )
	{
		if ( CooperativeMoo::beginOnPaint() )
		{		
			// changing mode when a paint message is received and the size of the
			// window is different than last stored size.
			if ( lastRect_ != rect &&
				Moo::rc().device() && Moo::rc().windowed() &&
				rect.Width() && rect.Height() &&
				!((MainFrame*)BigBang2App::instance().mainWnd())->resizing() )
			{
				CWaitCursor wait;
				Moo::rc().changeMode(Moo::rc().modeIndex(), Moo::rc().windowed(), true);
				lastRect_ = rect;
			}

			BigBang2App::instance().mfApp()->updateFrame( false );

			CooperativeMoo::endOnPaint();
		}
	}
	else
	{
		CWindowDC dc( this );

		dc.FillSolidRect( rect, ::GetSysColor( COLOR_BTNFACE ) );
	}
}

void BigBang2View::OnActivateView(BOOL bActivate, CView* pActivateView, CView* pDeactiveView)
{
	CView::OnActivateView(bActivate, pActivateView, pDeactiveView);
}

BOOL BigBang2View::OnSetCursor( CWnd* wnd, UINT, UINT )
{
	::SetCursor( BigBang::instance().cursor() );
	return TRUE;
}
