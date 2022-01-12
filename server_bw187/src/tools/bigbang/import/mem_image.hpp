/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef MEM_IMAGE_HPP
#define MEM_IMAGE_HPP


#include "cstdmf/smartpointer.hpp"
#include "math/mathdef.hpp"
#include <limits>
#include <fstream>


/**
 *  This template defines an in-memory image
 */
template<typename TYPE>
class MemImage : public ReferenceCount
{
public:
    MemImage();

    MemImage(unsigned int w, unsigned int h);

    MemImage(unsigned int w, unsigned int h, TYPE const &value);

    MemImage(unsigned int w, unsigned int h, TYPE *pixels, bool owns = true);

    MemImage(MemImage const &other);

    ~MemImage();

    MemImage &operator=(MemImage const &other);

    bool isEmpty() const;

    unsigned int width() const;

    unsigned int height() const;

    TYPE *const getRow(unsigned int y) const;

    TYPE *getRow(unsigned int y);

    TYPE getValue(int x, int y) const;

    TYPE getValue(unsigned int x, unsigned int y) const;

    TYPE getValue(float x, float y) const;

    double getValue(double x, double y) const;

    void setValue(int x, int y, TYPE value);

    void resize(unsigned int w, unsigned int h);

    void resize(unsigned int w, unsigned int h, TYPE const &value);

    void resize(unsigned int w, unsigned int h, TYPE *pixels, bool owns);

    void flip(bool flipX);

    void rotate(bool clockwise);

    void toCSV(std::string const &filename) const;

protected:
    void copy(MemImage const &other);

    void destroy();

    float positive(float x) const;

    float bicubicKernel(float x) const;

    double positive(double x) const;

    double bicubicKernel(double x) const;

private:
    TYPE            *data_;
    unsigned int    width_;
    unsigned int    height_;
    bool            owns_;
};


/** 
 *  This is the default constructor for a MemImage.
 */
template<typename TYPE>
inline MemImage<TYPE>::MemImage()
:
data_(NULL),
width_(0),
height_(0),
owns_(true)
{
}


/**
 *  This constructs a MemImage of the given size.
 *
 *  @param w        The width of the image.
 *  @param h        The height of the image.
 */
template<typename TYPE>
inline MemImage<TYPE>::MemImage(unsigned int w, unsigned int h)
:
data_(new TYPE[w*h]),
width_(w),
height_(h),
owns_(true)
{
}


/**
 *  This constructs a MemImage of the given size.
 *
 *  @param w        The width of the image.
 *  @param h        The height of the image.
 *  @param value    The value to initialize the image with.
 */
template<typename TYPE>
inline MemImage<TYPE>::MemImage(unsigned int w, unsigned int h, TYPE const &value)
:
data_(new TYPE[w*h]),
width_(w),
height_(w),
owns_(true)
{
    TYPE *p = data_;
    size_t sz = w*h;
    for (size_t i = 0; i < sz; ++i, ++p)
        *p = value;
}


/**
 *  This constructor allows for pretending an existing bit of memory is an 
 *  image.
 *
 *  @param w        The width of the image.
 *  @param h        The height of the image.
 *  @param pixels   The image's pixels from left to right, top to bottom.
 *  @param owns     If true then the pixels are deleted in the destructor,
 *                  if false then the pixels are left alone.
 */
template<typename TYPE>
inline MemImage<TYPE>::MemImage(unsigned int w, unsigned int h, TYPE *pixels, bool owns /*= true*/)
:
data_(pixels),
width_(w),
height_(h),
owns_(owns)
{
}


/**
 *  This is the copy constructor for MemImages.
 *
 *  @param other    The image to copy from.
 */
template<typename TYPE>
inline MemImage<TYPE>::MemImage(MemImage const &other)
:
data_(NULL),
w(0),
h(0),
owns_(true)
{
    copy(other);
}


/**
 *  This is the destructor for MemImages.
 *
 *  @param other    The image to copy from.
 */
template<typename TYPE>
inline MemImage<TYPE>::~MemImage()
{
    destroy();
}


/**
 *  This is the assignment operator for MemImages.
 *  
 *  @param other    The image to copy from.
 */
template<typename TYPE>
inline MemImage<TYPE> &MemImage<TYPE>::operator=(MemImage const &other)
{
    if (this != &other)
        copy(other);
    return *this;
}

/**
 *  This function returns true if the image does not yet represent anything.
 *
 *  @returns        True if the image is empty.
 */
template<typename TYPE>
inline bool MemImage<TYPE>::isEmpty() const
{
    return data_ == NULL;
}


/**
 *  This function returns the width of the image.
 *
 *  @returns        The width of the image, 0 if the image is empty.
 */
template<typename TYPE>
inline unsigned int MemImage<TYPE>::width() const
{
    return width_;
}


/**
 *  This function returns the height of the image.
 *
 *  @returns        The height of the image, 0 if the image is empty.
 */
template<typename TYPE>
inline unsigned int MemImage<TYPE>::height() const
{
    return height_;
}


/**
 *  This function returns the address of the first elevation value in the given 
 *  row.  Elevation values in the row are guaranteed to be linearly accessible
 *  from this pixel.
 *
 *  @param y        The y coordinate of the row to get.
 *  @returns        The address of the first pixel in row y.  
 */
template<typename TYPE>
inline TYPE *const MemImage<TYPE>::getRow(unsigned int y) const
{
    return data_ + y*width_;
}


/**
 *  This function returns the address of the first elevation value in the given 
 *  row.  Elevation values in the row are guaranteed to be linearly accessible
 *  from this pixel.
 *
 *  @param y        The y coordinate of the row to get.
 *  @returns        The address of the first pixel in row y.  
 */
template<typename TYPE>
inline TYPE *MemImage<TYPE>::getRow(unsigned int y)
{
    return data_ + y*width_;
}


/**
 *  This safely gets the value of a pixel at (x, y).  Safe means boundary
 *  clamping wrapping conditions.
 *
 *  @param x        The x coordinate.
 *  @param y        The y coordinate.
 *  @returns        The value of the image at (x, y).
 */
template<typename TYPE>
inline TYPE MemImage<TYPE>::getValue(int x, int y) const
{
    x = Math::clamp(0, x, (int)width_  - 1);
    y = Math::clamp(0, y, (int)height_ - 1);
    return *(data_ + y*width_ + x);
}


/**
 *  This safely gets the value of a pixel at (x, y).  Safe means boundary
 *  clamping wrapping conditions.
 *
 *  @param x        The x coordinate.
 *  @param y        The y coordinate.
 *  @returns        The value of the image at (x, y).
 */
template<typename TYPE>
inline TYPE MemImage<TYPE>::getValue(unsigned int x, unsigned int y) const
{
    if (x >= width_ - 1)
        x = width_ - 1;
    if (y >= height_ - 1)
        y = height_ - 1;
    return *(data_ + y*width_ + x);
}


/**
 *  This safely gets the value of a pixel at (x, y) using bicubic interpolation.  
 *  Safe means boundary clamping wrapping conditions.  It assumes that you can
 *  multiply a pixel by a scalar and add pixels to interpolate them.
 *
 *  @param x        The x coordinate.
 *  @param y        The y coordinate.
 *  @returns        The value of the image at (x, y).
 */
template<typename TYPE>
inline TYPE MemImage<TYPE>::getValue(float x, float y) const
{
    int ix = (int)Math::clamp(0.0f, x, width_  - 1.0f);
    int iy = (int)Math::clamp(0.0f, y, height_ - 1.0f);

    TYPE p00 = getValue(ix - 1, iy - 1);
    TYPE p01 = getValue(ix    , iy - 1);
    TYPE p02 = getValue(ix + 1, iy - 1);
    TYPE p03 = getValue(ix + 2, iy - 1);

    TYPE p10 = getValue(ix - 1, iy    );
    TYPE p11 = getValue(ix    , iy    );
    TYPE p12 = getValue(ix + 1, iy    );
    TYPE p13 = getValue(ix + 2, iy    );

    TYPE p20 = getValue(ix - 1, iy + 1);
    TYPE p21 = getValue(ix    , iy + 1);
    TYPE p22 = getValue(ix + 1, iy + 1);
    TYPE p23 = getValue(ix + 2, iy + 1);

    TYPE p30 = getValue(ix - 1, iy + 2);
    TYPE p31 = getValue(ix    , iy + 2);
    TYPE p32 = getValue(ix + 1, iy + 2);
    TYPE p33 = getValue(ix + 2, iy + 2);

    TYPE dx = x - floorf(x);
    TYPE dy = y - floorf(y);

    float kx[4], ky[4];
    for (int i = 0; i <= 3; ++i)
    {
        kx[i] = bicubicKernel(i - 1 - dx);
        ky[i] = bicubicKernel(i - 1 - dy);
    }

    TYPE sum = 0;

    sum += p00*kx[0]*ky[0];
    sum += p01*kx[1]*ky[0];
    sum += p02*kx[2]*ky[0];
    sum += p03*kx[3]*ky[0];
                        
    sum += p10*kx[0]*ky[1];
    sum += p11*kx[1]*ky[1];
    sum += p12*kx[2]*ky[1];
    sum += p13*kx[3]*ky[1];
                        
    sum += p20*kx[0]*ky[2];
    sum += p21*kx[1]*ky[2];
    sum += p22*kx[2]*ky[2];
    sum += p23*kx[3]*ky[2];
                        
    sum += p30*kx[0]*ky[3];
    sum += p31*kx[1]*ky[3];
    sum += p32*kx[2]*ky[3];
    sum += p33*kx[3]*ky[3];

    return sum;
}


/**
 *  This safely gets the value of a pixel at (x, y) using bicubic interpolation.  
 *  Safe means boundary clamping wrapping conditions.  It assumes that you can
 *  multiply a pixel by a scalar and add pixels to interpolate them.
 *
 *  @param x        The x coordinate.
 *  @param y        The y coordinate.
 *  @returns        The value of the image at (x, y).
 */
template<typename TYPE>
inline double MemImage<TYPE>::getValue(double x, double y) const
{
    int ix = (int)Math::clamp(0.0, x, width_  - 1.0);
    int iy = (int)Math::clamp(0.0, y, height_ - 1.0);

    TYPE p00 = getValue(ix - 1, iy - 1);
    TYPE p01 = getValue(ix    , iy - 1);
    TYPE p02 = getValue(ix + 1, iy - 1);
    TYPE p03 = getValue(ix + 2, iy - 1);

    TYPE p10 = getValue(ix - 1, iy    );
    TYPE p11 = getValue(ix    , iy    );
    TYPE p12 = getValue(ix + 1, iy    );
    TYPE p13 = getValue(ix + 2, iy    );

    TYPE p20 = getValue(ix - 1, iy + 1);
    TYPE p21 = getValue(ix    , iy + 1);
    TYPE p22 = getValue(ix + 1, iy + 1);
    TYPE p23 = getValue(ix + 2, iy + 1);

    TYPE p30 = getValue(ix - 1, iy + 2);
    TYPE p31 = getValue(ix    , iy + 2);
    TYPE p32 = getValue(ix + 1, iy + 2);
    TYPE p33 = getValue(ix + 2, iy + 2);

    double dx = x - floor(x);
    double dy = y - floor(y);

    double kx[4], ky[4];
    for (int i = 0; i <= 3; ++i)
    {
        kx[i] = bicubicKernel(i - 1 - dx);
        ky[i] = bicubicKernel(i - 1 - dy);
    }

    double sum = 0;

    sum += p00*kx[0]*ky[0];
    sum += p01*kx[1]*ky[0];
    sum += p02*kx[2]*ky[0];
    sum += p03*kx[3]*ky[0];
                        
    sum += p10*kx[0]*ky[1];
    sum += p11*kx[1]*ky[1];
    sum += p12*kx[2]*ky[1];
    sum += p13*kx[3]*ky[1];
                        
    sum += p20*kx[0]*ky[2];
    sum += p21*kx[1]*ky[2];
    sum += p22*kx[2]*ky[2];
    sum += p23*kx[3]*ky[2];
                        
    sum += p30*kx[0]*ky[3];
    sum += p31*kx[1]*ky[3];
    sum += p32*kx[2]*ky[3];
    sum += p33*kx[3]*ky[3];

    return sum;
}


/**
 *  This safely sets the value of a pixel at (x, y).  Safe means if x or y
 *  are outside the valid range then nothing is done.
 *
 *  @param x        The x coordinate.
 *  @param y        The y coordinate.
 */
template<typename TYPE>
inline void MemImage<TYPE>::setValue(int x, int y, TYPE value)
{
    if (0 <= x && x < (int)width_ && 0 <= y && y < (int)height_)
        *(data_ + y*width_ + x) = value;
}


/**
 *  Resize the underlying image.  Existing pixels are not preserved.
 *
 *  @param w        The new width.
 *  @param h        The new height.
 */
template<typename TYPE>
inline void MemImage<TYPE>::resize(unsigned int w, unsigned int h)
{
    destroy();

    if (w != 0 && h != 0)
    {
        data_   = new TYPE[w*h];
        width_  = w;
        height_ = h;
        owns_   = true;
    }
}


/**
 *  Resize the underlying image.  Existing pixels are not preserved.
 *
 *  @param w        The new width.
 *  @param h        The new height.
 *  @param value    The new value to put into the image.
 */
template<typename TYPE>
inline void MemImage<TYPE>::resize(unsigned int w, unsigned int h, TYPE const &value)
{
    destroy();

    if (w != 0 && h != 0)
    {
        data_   = new TYPE[w*h];
        width_  = w;
        height_ = h;
        owns_   = true;

        size_t sz = w*h;
        TYPE   *p = data_;
        for (size_t i = 0; i < sz; ++i, ++p)
            *p = value;
    }
}


/**
 *  Resize the underlying image.  Existing pixels are not preserved.
 *
 *  @param w        The new width.
 *  @param h        The new height.
 *  @param pixels   The new pixel data.
 *  @paran owns     Does the image delete pixels in the dtor?
 */
template<typename TYPE>
inline void MemImage<TYPE>::resize(unsigned int w, unsigned int h, TYPE *pixels, bool owns)
{
    destroy();

    if (w != 0 && h != 0)
    {
        data_   = pixels;
        width_  = w;
        height_ = h;
        owns_   = owns;
    }
}


/**
 *  This function flips the image around the given axis.
 *
 *  @param flipX    If true then flip horizontally, if false then flip
 *                  vertically.
 */
template<typename TYPE>
inline void MemImage<TYPE>::flip(bool flipX)
{
    if (isEmpty())
        return;

    if (flipX)
    {
        for (unsigned int y = 0; y < height_; ++y)
        {
            TYPE *p = data_ + y*width_;
            TYPE *q = p + width_ - 1;
            for (unsigned int x = 0; x < width_/2; ++x, ++p, --q)
            {
                std::swap(*p, *q);
            }
        }
    }
    else
    {
        for (unsigned int x = 0; x < width_; ++x)
        {
            TYPE *p = data_ + x;
            TYPE *q = p + width_*(height_ - 1);
            for (unsigned int y = 0; y < height_/2; ++y)
            {
                std::swap(*p, *q);
                p += width_;
                q -= width_;
            }
        }
    } 
}


/**
 *  This function rotates the image.
 *
 *  @param clockwise    If true then rotate clockwise, if false then flip
 *                      anticlockwise.
 */
template<typename TYPE>
inline void MemImage<TYPE>::rotate(bool clockwise)
{
    if (isEmpty())
        return;

    size_t sz  = width_*height_;
    TYPE *result = NULL;
    try
    {
        result = new float[sz];
        if (clockwise)
        {
            for (unsigned int y = 0; y < height_; ++y)
            {
                TYPE const *p = data_  + y*width_;
                TYPE *q       = result + (height_ - 1) - y;
                for (uint32 x = 0; x < width_; ++x)
                {
                    *q = *p;
                    ++p;
                    q += height_;
                }
            }
        }
        else
        {
            for (unsigned int y = 0; y < height_; ++y)
            {
                TYPE const *p = data_  + y*width_;
                TYPE *q       = result + height_*(width_ - 1) + y;
                for (uint32 x = 0; x < width_; ++x)
                {
                    *q = *p;
                    ++p;
                    q -= height_;
                }
            }
        }
        std::swap(width_, height_);
        owns_ = true;
        delete[] data_;
        data_ = result;
    }
    catch (...)
    {
        delete[] result;
        throw;
    }
}


/**
 *  This function writes the image as a CSV file to disk.  It's useful for
 *  debugging.
 */
template<typename TYPE>
inline void MemImage<TYPE>::toCSV(std::string const &filename) const
{
    std::ofstream out(filename.c_str());
    for (unsigned int y = 0; y < height(); ++y)
    {
        TYPE const *p = getRow((int)y);
        for (unsigned int x = 0; x < width(); ++x, ++p)
        {
            out << *p << ", ";
        }
        out << std::endl;
    }
    out.close();
}


/**
 *  This is the heart of image copying.
 *
 *  @returns other      The image to copy from.
 */
template<typename TYPE>
inline void MemImage<TYPE>::copy(MemImage const &other)
{
    destroy();
    if (!other.isEmpty())
    {
        data_   = new TYPE[other.width_*other.height_];
        ::memcpy(data_, other.data_, sizeof(TYPE)*other.width_*other.height_);
        width_  = other.width_;
        height_ = other.height_;
        owns_   = true;
    }
}


/**
 *  This cleans up data inside an image when it is no longer used.
 */
template<typename TYPE>
inline void MemImage<TYPE>::destroy()
{
    if (owns_)
        delete[] data_; 
    data_ = NULL;
    width_ = height_ = 0U;
}


/**
 *  This returns the positive part of x.
 *
 *  @param x        The number whose positive part to get.
 *  @returns        x if x >= 0.
 *                  0 if x <= 0.
 */
template<typename TYPE>
inline float MemImage<TYPE>::positive(float x) const
{
    return (x > 0.0f) ? x : 0.0f;
}


/**
 *  This returns the bicubic kernel for the fractional part x.
 *
 *  @param x        The fractional part to get.  This should be in [0, 1).
 */
template<typename TYPE>
inline float MemImage<TYPE>::bicubicKernel(float x) const
{
    float p0 = positive(x + 2);
    float p1 = positive(x + 1);
    float p2 = positive(x    );
    float p3 = positive(x - 1);
    return ((p0*p0*p0) - 4.0f*(p1*p1*p1) + 6.0f*(p2*p2*p2) - 4*(p3*p3*p3))/6.0f; 
}


/**
 *  This returns the positive part of x.
 *
 *  @param x        The number whose positive part to get.
 *  @returns        x if x >= 0.
 *                  0 if x <= 0.
 */
template<typename TYPE>
inline double MemImage<TYPE>::positive(double x) const
{
    return (x > 0.0) ? x : 0.0;
}


/**
 *  This returns the bicubic kernel for the fractional part x.
 *
 *  @param x        The fractional part to get.  This should be in [0, 1).
 */
template<typename TYPE>
inline double MemImage<TYPE>::bicubicKernel(double x) const
{
    double p0 = positive(x + 2);
    double p1 = positive(x + 1);
    double p2 = positive(x    );
    double p3 = positive(x - 1);
    return ((p0*p0*p0) - 4.0*(p1*p1*p1) + 6.0*(p2*p2*p2) - 4.0*(p3*p3*p3))/6.0; 
}


#endif // MEM_IMAGE_HPP
