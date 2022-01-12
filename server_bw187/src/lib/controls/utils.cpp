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
#include "controls/utils.hpp"


using namespace controls;


/**
 *  This function replaces srcColour in bitmap with dstColour.  It only works
 *  with 8bpp, 24bpp and 32bpp images.  It has only been tested with 8bpp
 *  images.
 *
 *  @param hbitmap      The bitmap to change.
 *  @param srcColour    The colour to replace.
 *  @param dstColour    The colour to replace with.
 */
void
controls::replaceColour
(
    HBITMAP             hbitmap,
    COLORREF            srcColour,
    COLORREF            dstColour
)
{
    if (hbitmap == NULL)
        return;

    // This can almost be certainly sped up, but at the moment I only use it
    // on small images.
    CBitmap bitmap;
    bitmap.Attach(hbitmap);
    CDC dc;
    dc.CreateCompatibleDC(NULL);
    CBitmap *oldBMP = dc.SelectObject(&bitmap);
    BITMAP bitmapInfo;
    bitmap.GetBitmap(&bitmapInfo);
    for (int y = 0; y < bitmapInfo.bmHeight; ++y)
    {
        for (int x = 0; x < bitmapInfo.bmWidth; ++x)
        {
            COLORREF thisPixel = dc.GetPixel(x, y);
            if (thisPixel == srcColour)
                dc.SetPixel(x, y, dstColour);
        }
    }
    bitmap.Detach();
    dc.SelectObject(oldBMP);
}


/**
 *  This function replaces the colour in the top-left pixel of the bitmap with 
 *  dstColour.  It only works with 8bpp, 24bpp and 32bpp images.  It has only 
 *  been tested with 8bpp images.
 *
 *  @param hbitmap      The bitmap to change.
 *  @param dstColour    The colour to replace with.
 */
void 
controls::replaceColour
(
    HBITMAP             hbitmap,
    COLORREF            dstColour        
)
{
    if (hbitmap == NULL)
        return;

    CBitmap bitmap;
    bitmap.Attach(hbitmap);
    CDC dc;
    dc.CreateCompatibleDC(NULL);
    CBitmap *oldBMP = dc.SelectObject(&bitmap);
    COLORREF srcColour = dc.GetPixel(0, 0);
    BITMAP bitmapInfo;
    bitmap.GetBitmap(&bitmapInfo);
    for (int y = 0; y < bitmapInfo.bmHeight; ++y)
    {
        for (int x = 0; x < bitmapInfo.bmWidth; ++x)
        {
            COLORREF thisPixel = dc.GetPixel(x, y);
            if (thisPixel == srcColour)
                dc.SetPixel(x, y, dstColour);
        }
    }
	dc.SelectObject(oldBMP);
    bitmap.Detach();
    replaceColour(hbitmap, srcColour, dstColour);
}


/**
 *  Return the colour of the (x, y)'th pixel in hbitmap.
 *
 *  @param hbitmap      The image to get the pixel from.
 *  @param x            The x coordinate.
 *  @param y            The y coordinate.
 *  @returns            The pixel at (x, y).
 */
COLORREF
controls::getPixel
(
    HBITMAP             hbitmap,
    unsigned int        x,
    unsigned int        y
)
{
    CDC dc;
    dc.CreateCompatibleDC(NULL);
    HGDIOBJ oldBMP = dc.SelectObject(hbitmap);
    COLORREF srcColour = dc.GetPixel(x, y);
    dc.SelectObject(oldBMP);
    return srcColour;
}
