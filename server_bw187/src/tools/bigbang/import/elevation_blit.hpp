/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef ELEVATION_BLIT_HPP
#define ELEVATION_BLIT_HPP


#include "import/mem_image.hpp"


class   ElevationImage;


/**
 *  This functions hide the details of blitting new terrain data to a space.
 */
namespace ElevationBlit
{
    enum Mode
    {
        ADDITIVE   ,
        SUBTRACTIVE,
        OVERLAY    ,
        MIN        ,
        MAX        ,
        REPLACE
    };

    size_t importUndoSize
    (
        float               left,
        float               top,
        float               right,
        float               bottom
    );

    bool saveUndoForImport
    (
        float               left,
        float               top,
        float               right,
        float               bottom,
        std::string         const &description,
        bool                showProgress
    );

    bool import
    (
        ElevationImage      const &elevationImage,
        float               left,
        float               top,
        float               right,
        float               bottom,
        Mode                mode,
        float               minv,
        float               maxv,
        float               alpha,
        bool                showProgress,
        bool                forceToMemory
    );

    bool exportTo
    (
        ElevationImage      &image,
        float               left,
        float               top,
        float               right,
        float               bottom,
        bool                showProgress
    );
}


#endif // ELEVATION_BLIT_HPP
