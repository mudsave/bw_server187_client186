/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef ELEVATION_CODEC_BMP_HPP
#define ELEVATION_CODEC_BMP_HPP

#include "import/elevation_codec.hpp"

/**
 *  This codec reads/writes BMP data.
 */
class ElevationCodecBMP : public ElevationCodec
{
public:
    /*virtual*/ bool canHandleFile(std::string const &filename);

    /*virtual*/ LoadResult load
    (
        std::string         const &filename, 
        ElevationImage      &image,
        float               *left           = NULL,
        float               *top            = NULL,
        float               *right          = NULL,
        float               *bottom         = NULL,
        bool                loadConfigDlg   = false
    );

    /*virtual*/ bool save
    (
        std::string         const &filename, 
        ElevationImage      const &image,
        float               *left           = NULL,
        float               *top            = NULL,
        float               *right          = NULL,
        float               *bottom         = NULL
    );

    /*virtual*/ bool canLoad() const;

    /*virtual*/ bool canSave() const;

    static ElevationCodecBMP &instance();
};

#endif // ELEVATION_CODEC_BMP_HPP
