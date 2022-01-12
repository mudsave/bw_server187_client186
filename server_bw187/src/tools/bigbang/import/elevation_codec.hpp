/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef ELEVATION_CODEC_HPP
#define ELEVATION_CODEC_HPP

class   ElevationImage;

/**
 *  This is the base class for elevation codecs that can read and write
 *  elevation data from/to non-native formats.
 */
class ElevationCodec
{
public:
    virtual ~ElevationCodec();

    virtual bool canHandleFile(std::string const &filename) = 0;

    enum LoadResult
    {
        LR_OK,
        LR_BAD_FORMAT,
        LR_CANCELLED,
        LR_UNKNOWN_CODEC
    };

    virtual LoadResult load
    (
        std::string         const &filename, 
        ElevationImage      &image,
        float               *left           = NULL,
        float               *top            = NULL,
        float               *right          = NULL,
        float               *bottom         = NULL,
        bool                loadConfigDlg   = false
    ) = 0;

    virtual bool save
    (
        std::string         const &filename, 
        ElevationImage      const &image,
        float               *left           = NULL,
        float               *top            = NULL,
        float               *right          = NULL,
        float               *bottom         = NULL
    ) = 0;

    virtual bool canLoad() const;

    virtual bool canSave() const;
};

#endif // ELEVATION_CODEC_HPP
