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
#include "import/elevation_image.hpp"
#include "import/elevation_codec_bmp.hpp"
#include "import/elevation_codec_dted.hpp"
#include "import/elevation_codec_raw.hpp"
#include "import/elevation_codec_terragen.hpp"
#include <limits>

/**
 *  This creates an empty ElevationImage.
 */
ElevationImage::ElevationImage()
:
MemImage<float>()
{
}


/**
 *  This creates an ElevationImage of size w, h.
 */
ElevationImage::ElevationImage(unsigned int w, unsigned int h)
:
MemImage<float>(w, h)
{
}


/**
 *  This is the copy constructor for ElevationImage.
 *
 *  @param  other           The ElevationImage to copy from.
 */
ElevationImage::ElevationImage(ElevationImage const &other)
:
MemImage<float>()
{
    copy(other);
}


/**
 *  This is the ElevationImage destructor.
 */
ElevationImage::~ElevationImage()
{
    destroy();
}


/**
 *  This is the ElevationImage assignment operator.
 *
 *  @param  other           The ElevationImage to copy from.
 *  @returns                *this.
 */
ElevationImage &ElevationImage::operator=(ElevationImage const &other)
{
    if (this != &other)
        copy(other);
    return *this;
}


/**
 *  Load the elevation data from a file.
 *
 *  @param filename     The file to load from.
 *  @returns            The result of loading.
 */
ElevationCodec::LoadResult ElevationImage::load
(
    std::string     const & filename,
    float           *left,
    float           *top,
    float           *right,
    float           *bottom,
    bool            configDialog
)
{
    ElevationCodec *codec = getCodec(filename);
    if (codec == NULL)
    {
        return ElevationCodec::LR_UNKNOWN_CODEC;
    }
    else
    {
        return 
            codec->load
            (
                filename, 
                *this, 
                left , top   , 
                right, bottom,
                configDialog
            );
    }
}


/**
 *  Save the elevation data to a file.
 *
 *  @param filename     The file to save to.
 *  @returns            True if the file could be saved, false otherwise.
 */
bool ElevationImage::save
(
    std::string     const & filename,
    float           *left,
    float           *top,
    float           *right,
    float           *bottom
)
{
    ElevationCodec *codec = getCodec(filename);
    if (codec == NULL)
        return false;
    else
        return codec->save(filename, *this, left, top, right, bottom);
}


/**
 *  This function flips the height data so that the points of minimum value
 *  become points of maximum value and vice-versa.
 */
void ElevationImage::flipHeight()
{
    if (isEmpty())
        return;

    float minV, maxV;
    range(minV, maxV);

    size_t sz  = width()*height();
    float  *p  = getRow(0);    
    float  sum = minV + maxV;
    for (size_t i = 0; i < sz; ++i, ++p)
        *p = sum - *p;
}


/**
 *  Normalize the range of the image into the range [0, 1].
 */
void ElevationImage::normalize()
{
    if (isEmpty())
        return;

    float minv, maxv;
    range(minv, maxv);
    if (minv == maxv)
    {
        ::memset(getRow(0), 0, width()*height()*sizeof(float));
    }
    else
    {
        float scale = 1.0f/(maxv - minv);
        size_t sz = width()*height();
        float *p = getRow(0);
        for (size_t i = 0; i < sz; ++i, ++p)
            *p = (*p - minv)*scale;
    }
}


/**
 *  Return the range of values within the elevation data.
 *
 *  @param minV         The minimum height in the image.
 *  @param maxV         The maximum height in the image.
 */
void ElevationImage::range(float &minV, float &maxV) const
{
    minV = +std::numeric_limits<float>::max();
    maxV = -std::numeric_limits<float>::max();
    size_t sz = width()*height();
    float const *p = getRow(0);
    for (size_t i = 0; i < sz; ++i, ++p)
    {
        minV = std::min(minV, *p);
        maxV = std::max(maxV, *p);
    }
}


/**
 *  This function does the actual copying from another ElevationData.
 *
 *  @param other        The ElevationData to copy from.
 */
void ElevationImage::copy(ElevationImage const &other)
{
    destroy();
    MemImage<float>::copy(other);
}


/**
 *  This function cleans up resources owned by an ElevationData.
 */
void ElevationImage::destroy()
{
    MemImage<float>::destroy();
}


/**
 *  This function returns an appropriate codec to load/save elevation data from
 *  or to.
 *
 *  @param filename     The name of the file to load/save.
 *  @returns            The codec to load/save given the file extension of 
 *                      filename.  If no appropriate codec can be found then
 *                      NULL is returned.
 */
/*static*/ ElevationCodec *
ElevationImage::getCodec
(
    std::string         const &filename
)
{
    if (ElevationCodecBMP::instance().canHandleFile(filename))
        return &ElevationCodecBMP::instance();
    else if (ElevationCodecDTED::instance().canHandleFile(filename))
        return &ElevationCodecDTED::instance();
    else if (ElevationCodecRAW::instance().canHandleFile(filename))
        return &ElevationCodecRAW::instance();
    else if (ElevationCodecTerragen::instance().canHandleFile(filename))
        return &ElevationCodecTerragen::instance();
    else
        return NULL;
}