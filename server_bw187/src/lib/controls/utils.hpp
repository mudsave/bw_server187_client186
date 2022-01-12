/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CONTROLS_UTILS_HPP
#define CONTROLS_UTILS_HPP


#include "controls/defs.hpp"
#include "controls/fwd.hpp"


namespace controls
{
    void CONTROLS_DLL
    replaceColour
    (
        HBITMAP             hbitmap,
        COLORREF            srcColour,
        COLORREF            dstColour
    );

    void CONTROLS_DLL
    replaceColour
    (
        HBITMAP             hbitmap,
        COLORREF            dstColour        
    );

    COLORREF
    getPixel
    (
        HBITMAP             hbitmap,
        unsigned int        x,
        unsigned int        y
    );
}


#endif // CONTROLS_UTILS_HPP
