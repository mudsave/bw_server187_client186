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
#include "controls/image_button.hpp"


using namespace controls;


BEGIN_MESSAGE_MAP(ImageButton, CButton)
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()


/**
 *  Constructor.
 */
ImageButton::ImageButton():
	CButton(),
	m_defID(0),
	m_disabledID(0),
	m_clrMask(RGB(255, 255, 255))
{
}


/**
 *  Destructor.
 */
ImageButton::~ImageButton()
{
}


/**
 *  Set the id of the bitmap to draw with.
 * 
 *  @param defID            The id of the default bitmap to draw.
 *  @param disabledID       The id of the disabled bitmap to draw.
 *  @param clrMask          The transparent colour.
 */
void 
ImageButton::setBitmapID
(
    UINT        defID, 
    UINT        disabledID, 
    COLORREF    clrMask     /* = RGB(255, 255, 255)*/
)
{
    m_defID         = defID;
    m_disabledID    = disabledID;
    m_clrMask       = clrMask;
}


/**
 *  Draw the button.
 * 
 *  @drawItemStruct         Drawing information.
 */
/*virtual*/ void ImageButton::DrawItem(LPDRAWITEMSTRUCT drawItemStruct)
{
    CDC   *dc  = CDC::FromHandle(drawItemStruct->hDC);
    CRect rect = drawItemStruct->rcItem;

    bool sel     = ((drawItemStruct->itemState & ODS_SELECTED) == ODS_SELECTED);
    bool disable = ((drawItemStruct->itemState & ODS_DISABLED) == ODS_DISABLED);

    // Draw the frame:
    UINT state = DFCS_BUTTONPUSH;
    if (sel)
        state |= DFCS_PUSHED;
    if (disable)
        state |= DFCS_INACTIVE;
    dc->DrawFrameControl(&drawItemStruct->rcItem, DFC_BUTTON, state);
    
    // Draw the bitmap:
    UINT id = disable ? m_disabledID : m_defID;
    HBITMAP hbitmap = ::LoadBitmap(AfxGetInstanceHandle(), MAKEINTRESOURCE(id));
    if (hbitmap != NULL)
    {
        // Get the dimensions:
        BITMAP bitmap;
        ::GetObject(hbitmap, sizeof(BITMAP), &bitmap);

        int delta = /*sel ? 2 :*/ 0;

        transparentBlit
        (
            dc, 
            (rect.Width () - bitmap.bmWidth )/2 + delta,
            (rect.Height() - bitmap.bmHeight)/2 + delta,
            bitmap.bmWidth,
            bitmap.bmHeight,
            hbitmap,
            m_clrMask
        );

        // Cleanup:
        ::DeleteObject(hbitmap);
    }
}


/**
 *  Add the owner draw style to the button.
 */
/*afx_msg*/ void ImageButton::PreSubclassWindow()
{
    ModifyStyle(0, BS_OWNERDRAW);
    CButton::PreSubclassWindow();
}


/**
 *  Draw the background of the button.
 * 
 *  @param dc           The drawing context.
 *  @returns            TRUE.
 */
/*afx_msg*/ BOOL ImageButton::OnEraseBkgnd(CDC *dc)
{
    CRect rect;
    GetClientRect(&rect);
    dc->FillSolidRect(rect, ::GetSysColor(COLOR_BTNFACE));
    return TRUE;
}


/**
 *  Transparent blit.
 * 
 *  @param dc           The drawing context.
 *  @param left         The left coordinate.
 *  @param top          The top coordinate.
 *  @param width        The width of the bitmap.
 *  @param height       The height of the bitmap.
 *  @param bitmap       The bitmap to draw.
 *  @param clrMask      The transparent colour.
 */
void
ImageButton::transparentBlit
(
    CDC                 *dc,
    int                 left,
    int                 top,
    int                 width,
    int                 height,
    HBITMAP             bitmap,
    COLORREF            clrMask
) const
{
    CDC srcDC;
    srcDC.CreateCompatibleDC(dc);
    HGDIOBJ oldBmp = srcDC.SelectObject(bitmap);
    dc->TransparentBlt
    (
        left, 
        top, 
        width, 
        height, 
        &srcDC, 
        0, 
        0, 
        width, 
        height, 
        clrMask
    );
	srcDC.SelectObject(oldBmp);
    srcDC.DeleteDC();
}
