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
#include "controls/dib_section32.hpp"


using namespace controls;


namespace
{
    /** 
     *  Header for BMP files.
     */
    struct BitmapFileHeader
    {
        uint16      bfType;
        uint32      bfSize;
        uint16      bfReserved1;
        uint16      bfReserved2;
        uint32      bfOffbits;
    };
}


/**
 *  This is the default ctor for DibSection32.  It creates an empty image.
 */
DibSection32::DibSection32()
:
hbitmap_(NULL),
lines_(NULL)
{
    ::ZeroMemory(&bmi_, sizeof(bmi_));
}


/**
 *  This is the ctor for DibSection32 that creates an image of size 
 *  width*height.
 *
 *  @param width        The width of the image.
 *  @param height       The height of the image.
 */
DibSection32::DibSection32(unsigned int width, unsigned int height)
:
hbitmap_(NULL),
lines_(NULL)
{
    ::ZeroMemory(&bmi_, sizeof(bmi_));
    create(width, height);
}


/**
 *  This is the copy ctor for DibSection32s.
 *
 *  @param other        The DibSection32 to copy.
 */
DibSection32::DibSection32(DibSection32 const &other)
:
hbitmap_(NULL),
lines_(NULL)
{
    ::ZeroMemory(&bmi_, sizeof(bmi_));
    copy(other);
}


/**
 *  DibSection32 dtor.
 */
DibSection32::~DibSection32()
{
    destroy();
}


/**
 *  DibSection32 assignment.
 *
 *  @param other        The DibSection32 to copy.     
 *  @returns            *this.
 */
DibSection32 &DibSection32::operator=(DibSection32 const &other)
{
    if (this != &other)
        copy(other);
    return *this;
}


/**
 *  This returns the width of the image.
 *
 *  @returns            The width of the image.
 */
unsigned int DibSection32::width() const
{
    return (unsigned int)bmi_.bmiHeader.biWidth;
}


/**
 *  This returns the height of the image.
 *
 *  @returns            The height of the image.
 */
unsigned int DibSection32::height() const
{
    return (unsigned int)bmi_.bmiHeader.biHeight;
}


/**
 *  This returns whether this is an actual image.
 *
 *  @returns            True if the image has no pixels, true if it does.
 */
bool DibSection32::isEmpty() const
{
    return hbitmap_ == NULL;
}


/**
 *  This returns the address of the first pixel in the given row.
 *
 *  @param row          The row to get.
 *  @returns            The address of the first pixel in the image.
 */
COLORREF *DibSection32::getRow(unsigned int row)
{
    return lines_[row];
}


/**
 *  This returns the address of the first pixel in the given row.
 *
 *  @param row          The row to get.
 *  @returns            The address of the first pixel in the image.
 */
COLORREF const *DibSection32::getRow(unsigned int row) const
{
    return lines_[row];
}


/**
 *  This resizes the image, but does not preserve existing data.
 *
 *  @param w            The new width.
 *  @param h            The new height.
 */
void DibSection32::resize(unsigned int w, unsigned int h)
{
    if (w != width() && h != height())
    {
        destroy();
        create(w, h);
    }
}


/**
 *  This returns the size of the image's buffer.
 *
 *  @returns            The size of the image buffer in bytes.
 */
size_t DibSection32::bufferSizeBytes() const
{
    return height()*rowSizeBytes();
}


/**
 *  This returns the size of a row of pixels in the image.
 *
 *  @returns            The size of a pixel row in bytes.
 */
size_t DibSection32::rowSizeBytes() const
{
    return width()*sizeof(COLORREF);
}


/**
 *  This returns the BITMAPINFO associated with the image.
 *
 *  @returns            The BITMAPINFO associated with the image.
 */
BITMAPINFO const &DibSection32::getBITMAPINFO() const
{
    return bmi_;
}


/**
 *  This returns the BITMAPINFOHEADER associated with the image.
 *
 *  @returns            The BITMAPINFOHEADER associated with the image.
 */
BITMAPINFOHEADER const &DibSection32::getBITMAPINFOHEADER() const
{
    return bmi_.bmiHeader;
}


/**
 *  This returns the HBITMAP of the DibSection.  This is null if the image is
 *  empty.
 *
 *  @returns            The handle to the underlying DibSection.
 */
DibSection32::operator HBITMAP() const
{
    return hbitmap_;
}


/**
 *  This detaches the HBITMAP from the object and returns it.  The callee is
 *  then responsible for calling ::DeleteObject on it.
 */
HBITMAP DibSection32::detach()
{
    HBITMAP result = hbitmap_;
    hbitmap_ = NULL;
    destroy();
    return result;
}


/**
 *  This loads the DibSection.  Currenly we only support 32bpp images.
 *
 *  @param filename     The file to load.
 *  @returns            True if successfully loaded, false otherwise.
 */ 
bool DibSection32::load(std::string const &filename)
{
    resize(0, 0);

    FILE *file = NULL;
    try
    {
        file = fopen(filename.c_str(), "rb");
        if (file == NULL)
            return false;

        size_t numRead = 0;

        // Read the file header:
        BitmapFileHeader fheader;
        numRead = fread(&fheader, 1, sizeof(fheader), file);
        if (numRead != sizeof(fheader))
        {
            fclose(file);
            return false;
        }
        if (fheader.bfType != (uint16)('M' << 8 | 'B'))
        {
            fclose(file);
            return false;
        }

        // Read the info header:
        BITMAPINFOHEADER iheader;
        numRead = fread(&iheader, 1, sizeof(iheader), file);
        if (numRead != sizeof(iheader))
        {
            fclose(file);
            return false;
        }
        // At the moment we only support 32bpp images:
        if (iheader.biBitCount != 32)
        {
            fclose(file);
            return false;
        }
        resize(iheader.biWidth, iheader.biHeight);

        // Read the image data:
        for (unsigned int y = 0; y < (unsigned int)iheader.biHeight; ++y)
        {
            COLORREF *row = getRow(iheader.biHeight - y - 1);
            numRead = fread(row, 1, sizeof(COLORREF)*iheader.biWidth, file);
            if (numRead != sizeof(COLORREF)*iheader.biWidth)
            {
                fclose(file);
                return false;
            }
        }

        fclose(file);
    }
    catch (...)
    {
        if (file != NULL)
            fclose(file);
        file = NULL;
        throw;
    }

    return true;
}


/**
 *  This saves the DibSection. 
 *
 *  @param filename     The file to save.
 *  @returns            True if successfully saved, false otherwise.
 */ 
bool DibSection32::save(std::string const &filename) const
{
    if (isEmpty())
        return false;

    FILE *file = NULL;
    try
    {
        file = fopen(filename.c_str(), "wb");
        if (file == NULL)
            return false;

        // Write the file header:
        BitmapFileHeader fheader;
        ::memset(&fheader, 0, sizeof(fheader));
        fheader.bfType      = (uint16)('M' << 8 | 'B');
        fheader.bfSize      = (uint32)
                                (
                                    sizeof(BitmapFileHeader) 
                                    + sizeof(BITMAPINFOHEADER)
                                    + width()*height()*sizeof(COLORREF)
                                );
        fheader.bfOffbits   = sizeof(BitmapFileHeader) 
                                + sizeof(BITMAPINFOHEADER);
        fwrite(&fheader, sizeof(fheader), 1, file);

        // Write the info header:
        BITMAPINFOHEADER iheader;
        ::memset(&iheader, 0, sizeof(iheader));
        iheader.biSize      = sizeof(iheader);
        iheader.biWidth     = (int32)width();
        iheader.biHeight    = (int32)height();
        iheader.biPlanes    =  1;
        iheader.biBitCount  = 32;
        iheader.biSizeImage = sizeof(COLORREF)*iheader.biWidth*iheader.biHeight;
        fwrite(&iheader, sizeof(iheader), 1, file);

        // Write the image data:
        for (unsigned int y = 0; y < height(); ++y)
        {
            COLORREF const *row = getRow(height() - y - 1);
            fwrite(row, sizeof(COLORREF), width(), file);
        }

        // Cleanup
        fclose(file);
    }
    catch (...)
    {
        if (file != NULL)
            fclose(file);
        file = NULL;
        throw;
    }

    return true;
}


/**
 *  This loads the DibSection32 from a resource.  It's currently quit slow and
 *  only appropriate for icons and small images.
 *
 *  @param resourceID       The id of the resource to load the image from.
 *  @returns                True if successfully loaded.
 */
bool DibSection32::load(unsigned int resourceID)
{
    CBitmap bitmap;
    BOOL ok = bitmap.LoadBitmap(resourceID);
    if (ok == FALSE)
        return false;
    BITMAP bitmapInfo;
    bitmap.GetBitmap(&bitmapInfo);
    CDC memDC;
    memDC.CreateCompatibleDC(NULL);
    memDC.SelectObject(bitmap);
    resize(bitmapInfo.bmWidth, bitmapInfo.bmHeight);
    for (unsigned int y = 0; y < height(); ++y)
    {
        COLORREF *p = getRow(y);
        for (unsigned int x = 0; x < width(); ++x, ++p)
        {
            COLORREF colour = memDC.GetPixel(x, y);
            *p = colour;
        }
    }
    return true;
}


/**
 *  Create the DIBSECTION, the image buffer etc.
 *
 *  @param w            The width.
 *  @param h            The height.
 */
void DibSection32::create(unsigned int w, unsigned int h)
{
    destroy();

    // Setup the bitmap info header:
    ::ZeroMemory(&bmi_, sizeof(bmi_));
    bmi_.bmiHeader.biSize          = sizeof(bmi_.bmiHeader);
    bmi_.bmiHeader.biWidth         = (LONG)w;
    bmi_.bmiHeader.biHeight        = (LONG)h;
    bmi_.bmiHeader.biPlanes        =  1;
    bmi_.bmiHeader.biBitCount      = 32;
    bmi_.bmiHeader.biCompression   = BI_RGB;
    bmi_.bmiHeader.biSizeImage     = (DWORD)(4*w*h); // 4 bytes per pixel

    if (w != 0 && h != 0)
    {
        // Create the DIBSECTION:
        COLORREF *bits = NULL;
        hbitmap_ =
            ::CreateDIBSection
            (
                0,              // Use desktop DC
                &bmi_,
                DIB_RGB_COLORS,
                (void **)&bits,
                NULL,           // hsection should be null
                0               // offset should be zero
            );

        // Create the line-lookup table:
        lines_ = new COLORREF*[h];
        for (size_t i = 0; i < h; ++i)
        {
            lines_[i] = bits + (h - i - 1)*w; // note it's upside down
        }
    }
}


/**
 *  This copys from other to this DibSection32.
 *
 *  @param other            The other image to copy from.
 */
void DibSection32::copy(DibSection32 const &other)
{
    destroy();
    if (!other.isEmpty())
    {
        resize(other.width(), other.height());
        for (unsigned int y = 0; y < height(); ++y)
        {
            ::memcpy
            (
                getRow(y),
                other.getRow(y),
                rowSizeBytes()
            );
        }
    }
}


/**
 *  This cleans up the resources owned by the image.
 */
void DibSection32::destroy()
{
    if (hbitmap_ != NULL)
    {
        ::DeleteObject(hbitmap_);
        hbitmap_ = NULL;
        ::ZeroMemory(&bmi_, sizeof(bmi_));
    }
    if (lines_ != NULL)
    {
        delete[] lines_;
        lines_ = NULL;
    }
}
