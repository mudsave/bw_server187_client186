/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// cbutton_color.cpp : implementation file
//

#include "stdafx.h"
#include "cbutton_color.hpp"
#include "main_frame.h"
#include "resource.h"

#include "color_picker.hpp"

#define WM_COLOR_BUTTON_DELETE WM_USER + 1

// CButtonColor

void AFXAPI DDX_CButtonColor(CDataExchange *pDX, int nIDC, Vector4 & crColour)
{
    HWND hWndCtrl = pDX->PrepareCtrl(nIDC);
    ASSERT (hWndCtrl != NULL);                
    
    CButtonColor* pColourButton = (CButtonColor *) CWnd::FromHandle(hWndCtrl);
    if (pDX->m_bSaveAndValidate)
    {
		crColour = pColourButton->getColor();
    }
    else 
    {
		// initializing
		pColourButton->setColor(crColour);
    }
}


IMPLEMENT_DYNAMIC(CButtonColor, CButton)
CButtonColor::CButtonColor()
	: CButton()
	, color_()
	, clicked_(false)
	, toDelete_(false)
{
}

CButtonColor::~CButtonColor()
{
}

BEGIN_MESSAGE_MAP(CButtonColor, CButton)
	ON_CONTROL_REFLECT(BN_CLICKED, OnBnClicked)
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONUP()
	ON_COMMAND(ID_COLORBUTTON_ALPHA, OnColorbuttonAlpha)
	ON_COMMAND(ID_COLORBUTTON_COLOR, OnColorbuttonColor)
	ON_COMMAND(ID_COLORBUTTON_DELETE, OnColorbuttonDelete)
END_MESSAGE_MAP()


// CButtonColor message handlers

enum Direction
{
	Direction_Down = 0,
	Direction_Up = 1,
	Direction_Left = 2,
	Direction_Right = 3
};

static void DrawArrow(CDC* pDC, RECT* pRect, Direction iDirection, COLORREF clrArrow /*= RGB(0,0,0)*/)
{
	POINT ptsArrow[3];

	switch (iDirection)
	{
		case Direction_Down :
		{
			ptsArrow[0].x = pRect->left;
			ptsArrow[0].y = pRect->top;
			ptsArrow[1].x = pRect->right;
			ptsArrow[1].y = pRect->top;
			ptsArrow[2].x = (pRect->left + pRect->right)/2;
			ptsArrow[2].y = pRect->bottom;
			break;
		}

		case Direction_Up :
		{
			ptsArrow[0].x = pRect->left;
			ptsArrow[0].y = pRect->bottom;
			ptsArrow[1].x = pRect->right;
			ptsArrow[1].y = pRect->bottom;
			ptsArrow[2].x = (pRect->left + pRect->right)/2;
			ptsArrow[2].y = pRect->top;
			break;
		}

		case Direction_Left :
		{
			ptsArrow[0].x = pRect->right;
			ptsArrow[0].y = pRect->top;
			ptsArrow[1].x = pRect->right;
			ptsArrow[1].y = pRect->bottom;
			ptsArrow[2].x = pRect->left;
			ptsArrow[2].y = (pRect->top + pRect->bottom)/2;
			break;
		}

		case Direction_Right :
		{
			ptsArrow[0].x = pRect->left;
			ptsArrow[0].y = pRect->top;
			ptsArrow[1].x = pRect->left;
			ptsArrow[1].y = pRect->bottom;
			ptsArrow[2].x = pRect->right;
			ptsArrow[2].y = (pRect->top + pRect->bottom)/2;
			break;
		}
	}
	
	CBrush brsArrow(clrArrow);
	CPen penArrow(PS_SOLID, 1 , clrArrow);

	CBrush* pOldBrush = pDC->SelectObject(&brsArrow);
	CPen*   pOldPen   = pDC->SelectObject(&penArrow);
	
	pDC->SetPolyFillMode(WINDING);
	pDC->Polygon(ptsArrow, 3);

	pDC->SelectObject(pOldBrush);
	pDC->SelectObject(pOldPen);
}


void CButtonColor::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	ASSERT(lpDrawItemStruct);

	CDC*    pDC      = CDC::FromHandle(lpDrawItemStruct->hDC);
	UINT    state    = lpDrawItemStruct->itemState;
    CRect   rDraw    = lpDrawItemStruct->rcItem;

	//******************************************************
	//**                  Draw Outer Edge
	//******************************************************
	if (clicked_)
	{
		state |= ODS_SELECTED|ODS_FOCUS;
	}

	UINT uFrameState = DFCS_BUTTONPUSH|DFCS_ADJUSTRECT;

	if (state & ODS_SELECTED)
		uFrameState |= DFCS_PUSHED;

	if (state & ODS_DISABLED)
		uFrameState |= DFCS_INACTIVE;
	
	pDC->DrawFrameControl(&rDraw,
						  DFC_BUTTON,
						  uFrameState);

	if (state & ODS_SELECTED)
		rDraw.OffsetRect(1,1);

	//******************************************************
	//**                     Draw Focus
	//******************************************************
	if (state & ODS_FOCUS) 
    {
		RECT rFocus = {rDraw.left,
					   rDraw.top,
					   rDraw.right - 1,
					   rDraw.bottom};
  
        pDC->DrawFocusRect(&rFocus);
    }

	rDraw.DeflateRect(::GetSystemMetrics(SM_CXEDGE),
					  ::GetSystemMetrics(SM_CYEDGE));

	//******************************************************
	//**                     Draw Arrow
	//******************************************************
	const int arrowSizeX = 6;
	const int arrowSizeY = 10;
	CRect rArrow;
	rArrow.left		= rDraw.right - arrowSizeX - ::GetSystemMetrics(SM_CXEDGE) / 2;
	rArrow.right	= rArrow.left + arrowSizeX;
	rArrow.top		= (rDraw.bottom + rDraw.top) / 2 - arrowSizeY / 2;
	rArrow.bottom	= (rDraw.bottom + rDraw.top) / 2 + arrowSizeY / 2;

	Direction arrowDirection = Direction_Down;
	if (clicked_)
		arrowDirection = Direction_Right;

	DrawArrow(pDC, &rArrow, arrowDirection, (state & ODS_DISABLED) ? ::GetSysColor(COLOR_GRAYTEXT) : RGB(0,0,0));

	rDraw.right = rArrow.left - ::GetSystemMetrics(SM_CXEDGE)/2;

	//******************************************************
	//**                   Draw Separator
	//******************************************************
	pDC->DrawEdge(&rDraw,
				  EDGE_ETCHED,
				  BF_RIGHT);

	rDraw.right -= (::GetSystemMetrics(SM_CXEDGE) * 2) + 1 ;
				  
	//******************************************************
	//**                     Draw Color
	//******************************************************
/*	COLORREF m_Color(CLR_DEFAULT);
	COLORREF m_DefaultColor(::GetSysColor(COLOR_APPWORKSPACE));

	if ((state & ODS_DISABLED) == 0)
	{
		pDC->FillSolidRect(&rDraw,
						   (m_Color == CLR_DEFAULT)
						   ? m_DefaultColor
						   : m_Color);

		::FrameRect(pDC->m_hDC,
					&rDraw,
					(HBRUSH)::GetStockObject(BLACK_BRUSH));
	}
*/

}


void CButtonColor::PreSubclassWindow()
{
	// call our own draw function
	ModifyStyle(0, BS_OWNERDRAW);      

	CButton::PreSubclassWindow();
}


void CButtonColor::OnBnClicked()
{
	// TODO: Add your control notification handler code here
}


Vector4 CButtonColor::getColor(void) const
{
	return color_;
}

void CButtonColor::setColor(Vector4 color)
{
	color_ = color;
}


void CButtonColor::OnLButtonUp( UINT nFlags, CPoint point )
{
	OnColorbuttonColor();

	CButton::OnLButtonUp(nFlags, point);
}


void CButtonColor::OnRButtonUp(UINT nFlags, CPoint point)
{
	CButton::OnRButtonUp(nFlags, point);

	CMenu menu;
	VERIFY(menu.LoadMenu(IDR_COLORBUTTON_POPUP_MENU));

	const int colorButtonSubMenuID = 0;
	CMenu* pPopup = menu.GetSubMenu(colorButtonSubMenuID);
	ASSERT(pPopup != NULL);

	POINT pp;
	GetCursorPos(&pp);
	DWORD SelectionMade = pPopup->TrackPopupMenu(
		TPM_LEFTALIGN | TPM_LEFTBUTTON,
		pp.x,pp.y,this);

	pPopup->DestroyMenu();
}


void CButtonColor::OnColorbuttonAlpha()
{
#if 0
	// create an alpha drop window 
	AlphaSelectWnd * alphaWnd = new AlphaSelectWnd();
	alphaWnd->Create(_T("0"), 0, CRect(0,0,0,0), this, 3);
virtual BOOL Create(
   LPCTSTR lpszClassName,
   LPCTSTR lpszWindowName,
   DWORD dwStyle,
   const RECT& rect,
   CWnd* pParentWnd,
   UINT nID,
   CCreateContext* pContext = NULL
);
#endif
}

void CButtonColor::OnColorbuttonColor()
{
	// launch color picker
	clicked_ = TRUE;
	RedrawWindow();

	{
//		AfxBeginThread( RUNTIME_CLASS(DialogThread) );

/*		CColorDialogAlpha dlg(RGBtoCOLORREF(color_), CC_FULLOPEN | CC_ANYCOLOR, this);
		DialogThread * theThread = static_cast<DialogThread *>(AfxBeginThread( RUNTIME_CLASS(DialogThread) ));
		theThread->DoDialog(&dlg);

		if (dlg.DoModal() == IDOK)
		{
			COLORREF colorREF = dlg.GetColor();
			color_ = COLORREFtoRGB(colorREF);

			GetParent()->UpdateData(true);	// call the parent's DoDataExchange(..) fn
			GetParent()->RedrawWindow();
		}
*/
		CPoint spawnPoint(0, 0);
		ClientToScreen(&spawnPoint);
//		ColorPicker * picker = new ColorPicker(spawnPoint, this);
//		picker->SetWindowPos(&CWnd::wndTop, spawnPoint.x, spawnPoint.y, 100, 100, SWP_NOSIZE);
//		picker->setRGB(RGBtoCOLORREF(color_));
	}

	clicked_ = FALSE;
	RedrawWindow();
}

void CButtonColor::OnColorbuttonDelete()
{
	toDelete_ = true;
}
