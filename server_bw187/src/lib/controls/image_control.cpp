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
#include "controls/image_control.hpp"


using namespace controls;


BEGIN_MESSAGE_MAP(ImageControl, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()


/**
 *  This is the ImageControl's ctor.
 */
ImageControl::ImageControl()
:
border_(BS_SUNKEN),
aspect_(true),
background_(::GetSysColor(COLOR_3DFACE)),
padding_(0),
setCount_(0)
{
}


/**
 *  This creates an ImageControl
 *
 *  @param style    The window style to apply.  Typically this is 
 *                  WS_CHILD | WS_VISIBLE.
 *  @param extents  The extents of the window.
 *  @param parent   The parent window.
 *  @param id       The id of this window.
 *  @returns        TRUE if sucessfully created, FALSE if something went wrong.
 */
BOOL 
ImageControl::Create
(
    DWORD           style, 
    RECT            const &extents, 
    CWnd            *parent,
    unsigned int    id  /* = 0*/
)
{
    static bool     registered = false;
    static CString  classname;
    if (!registered)
    {
        classname = AfxRegisterWndClass(NULL, NULL, NULL, NULL);
        registered = true;
    }

    BOOL result = 
        CWnd::Create
        (
            classname,
            "ImageControl",
            style,
            extents,
            parent,
            id
        );

    return result;
}


/**
 *  This subclasses the control over an existing control.
 *
 *  @param resourceID       The id of the control to take over.
 *  @param parent           The parent window.
 *  @returns                TRUE upon success.
 */
BOOL ImageControl::subclass(unsigned int resourceID, CWnd *parent)
{
    if (parent == NULL)
        return FALSE;

    CWnd *wnd = parent->GetDlgItem(resourceID);
    if (wnd == NULL)
        return FALSE;

    CRect rect;
    wnd->GetWindowRect(&rect);
    parent->ScreenToClient(&rect);
    wnd->ShowWindow(SW_HIDE);

    BOOL result = Create(WS_CHILD | WS_VISIBLE, rect, parent, resourceID);
    if (result == FALSE)
        return FALSE;

    return TRUE;
}


/**
 *  This sets the image to draw with.
 *
 *  @param dibSection       The new image to display.
 */
void ImageControl::setImage(DibSection32 const &dibSection)
{
    image_ = dibSection;
}


/**
 *  This gets the image that is drawn.
 *
 *  @returns                The image drawn.
 */
DibSection32 const &ImageControl::getImage() const
{
    return image_;
}


/**
 *  This function returns the displayed image.
 *
 *  @returns                The displayed image.
 */
DibSection32 &ImageControl::getImage()
{
    return image_;
}


/**
 *  This function gets the border style.
 *
 *  @returns                The border style used to draw the control.
 */
ImageControl::BorderStyle ImageControl::getBorder() const
{
    return border_;
}


/**
 *  This function sets the border style.
 *
 *  @param display          The border style to draw the control with.
 */
void ImageControl::setBorder(BorderStyle display)
{
    border_ = display;
    if (setCount_ == 0)
    {
        Invalidate();
        RedrawWindow();
    }
}


/**
 *  This function returns true if the control preserves the image's aspect
 *  ratio.
 *
 *  @returns                True if the control preserves the image's aspect
 *                          ratio, false if the image is drawn in squashed.
 */
bool ImageControl::getPreserveAspectRatio() const
{
    return aspect_;
}


/**
 *  This function sets whether the control preserves the image's aspect ratio
 *  when drawing.
 *
 *  @param preserve         If true then the image's aspect ratio is preserved
 *                          and the control's background is drawn using
 *                          the background colour.  If false then the image is
 *                          fitted to be inside the border.
 */ 
void ImageControl::setPreserveAspectRatio(bool preserve)
{
    aspect_ = preserve;
    if (setCount_ == 0)
    {
        Invalidate();
        RedrawWindow();
    }
}


/**
 *  This function gets the colour to draw the part of the control that is not
 *  the image or the border.  By default it is COLOR_3DFACE.
 *
 *  @returns                The background colour to draw with.
 */
COLORREF ImageControl::getBackgroundColour() const
{
    return background_;
}


/**
 *  This function sets the colour to draw the part of the control that is not
 *  the image or the border.
 *
 *  @param colour           The new background colour.
 */
void ImageControl::setBackgroundColour(COLORREF colour)
{
    background_ = colour;
    if (setCount_ == 0)
    {
        Invalidate();
        RedrawWindow();
    }
}


/**
 *  This function gets the padding between the border and the image.
 *
 *  @returns                The distance between the border and the image.
 */
unsigned int ImageControl::getBorderPadding() const
{
    return padding_;
}

/**
 *  This function sets the padding between the border and the image.
 *
 *  @param padding          The new padding distance.
 */
void ImageControl::setBorderPadding(unsigned int padding)
{
    padding_ = padding;
    if (setCount_ == 0)
    {
        Invalidate();
        RedrawWindow();
    }
}


/**
 *  This function allows batch setting of properties without getting a redraw
 *  between each (i.e. helps prevent flicker).  It is recursive and only 
 *  redraws when a reference count drops to zero.
 */
void ImageControl::beginSet()
{
    ++setCount_;
}


/**
 *  This function allows batch setting of properties without getting a redraw
 *  between each (i.e. helps prevent flicker).  It is recursive and only 
 *  redraws when a reference count drops to zero.
 */
void ImageControl::endSet()
{
    if (--setCount_ == 0)
    {
        Invalidate();
        RedrawWindow();
    }
}


/**
 *  This function is called to paint the control.
 */
/*afx_msg*/ void ImageControl::OnPaint()
{
    CPaintDC dc(this);

    CRect rect;
    GetClientRect(&rect);

    // Draw the border:
    switch (border_)
    {
    case BS_NONE:
        break;

    case BS_BLACKRECT:
        dc.Draw3dRect(rect, RGB(0, 0, 0), RGB(0, 0, 0)); 
        rect.DeflateRect(1, 1);
        break;

    case BS_SUNKEN:
        dc.Draw3dRect
        (
            rect, 
            ::GetSysColor(COLOR_3DSHADOW), 
            ::GetSysColor(COLOR_3DHILIGHT)
        ); 
        rect.DeflateRect(1, 1);
        break;

    case BS_RAISED:
        dc.Draw3dRect
        (
            rect, 
            ::GetSysColor(COLOR_3DHILIGHT), 
            ::GetSysColor(COLOR_3DSHADOW)
        ); 
        rect.DeflateRect(1, 1);
        break;
    }

    // Draw the background:
    dc.FillSolidRect(rect, background_);
    rect.DeflateRect(padding_, padding_);

    // Degenerate case:
    if (image_.isEmpty())
    {
        ; // nothing to do
    }
    // Preserve the image's aspect ratio case:
    else if (aspect_)
    {
        float dispAspt = (float)rect.Width()/(float)rect.Height();
        float imgAspt  = (float)image_.width()/(float)image_.height();
        int dx = 0;
        int dy = 0;
        if (dispAspt > imgAspt)
        {
            dx = (int)(0.5f*(rect.Width() - rect.Height()*imgAspt));
        }
        else
        {
            dy = (int)(0.5f*(rect.Height() - rect.Width()/imgAspt));
        }
        CDC memDC;
        memDC.CreateCompatibleDC(NULL);
        HGDIOBJ oldBmp = ::SelectObject(memDC, (HBITMAP)image_);
        int oldMode = dc.SetStretchBltMode(HALFTONE);
        dc.StretchBlt
        (
            rect.left + dx,
            rect.top  + dy,
            rect.Width () - 2*dx,
            rect.Height() - 2*dy,
            &memDC,
            0,
            0,
            image_.width (),
            image_.height(),
            SRCCOPY
        );
        dc.SetStretchBltMode(oldMode);
        ::SelectObject(memDC, oldBmp);
    }
    // Fit into the available area case:
    else
    {
        CDC memDC;
        memDC.CreateCompatibleDC(NULL);
        HGDIOBJ oldBmp = ::SelectObject(memDC, (HBITMAP)image_);
        int oldMode = dc.SetStretchBltMode(HALFTONE);
        dc.StretchBlt
        (
            rect.left,
            rect.top,
            rect.Width (),
            rect.Height(),
            &memDC,
            0,
            0,
            image_.width (),
            image_.height(),
            SRCCOPY
        );
        dc.SetStretchBltMode(oldMode);
        ::SelectObject(memDC, oldBmp);
    }
}


/**
 *  This function is called to paint the control's background.  It does nothing
 *  but returns TRUE.  The actual painting is completely done in OnPaint.
 */
/*afx_msg*/ BOOL ImageControl::OnEraseBkgnd(CDC * /*dc*/)
{
    return TRUE; // all drawing is in OnPaint
}
