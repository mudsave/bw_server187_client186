/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef ELEVATION_IMAGE_HPP
#define ELEVATION_IMAGE_HPP


#include "mem_image.hpp"
#include "import/elevation_codec.hpp"


/**
 *  This is a two-dimensional image of elevation data.
 */
class ElevationImage : public MemImage<float>
{
public:
    ElevationImage();

    ElevationImage(unsigned int w, unsigned int h);

    ElevationImage(ElevationImage const &other);

    ~ElevationImage();

    ElevationImage &operator=(ElevationImage const &other);

    ElevationCodec::LoadResult 
    load
    (
        std::string     const & filename,
        float           *left,
        float           *top,
        float           *right,
        float           *bottom,
        bool            configDialog
    );

    bool 
    save
    (
        std::string     const & filename,
        float           *left,
        float           *top,
        float           *right,
        float           *bottom
    );

    void flipHeight();

    void normalize();

    void range(float &minV, float &maxV) const;

protected:
    void copy(ElevationImage const &other);

    void destroy();

    static ElevationCodec *getCodec(std::string const &filename);
};

#endif // ELEVATION_IMAGE_HPP
