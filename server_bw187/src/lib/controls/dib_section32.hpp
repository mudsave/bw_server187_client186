/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CONTROLS_DIBSECTION32_HPP
#define CONTROLS_DIBSECTION32_HPP


#include "controls/defs.hpp"
#include "controls/fwd.hpp"


namespace controls
{
    /**
     *  This class makes using win32 DibSections easier.  It only works on 
     *  32bpp dibsections.  It can be used anywhere a HBITMAP is used, but
     *  allows for access to the underlying image.
     */
    class CONTROLS_DLL DibSection32
    {
    public:
        DibSection32();

        DibSection32(unsigned int width, unsigned int height);

        DibSection32(DibSection32 const &other);

        ~DibSection32();

        DibSection32 &operator=(DibSection32 const &other);

        unsigned int width() const;

        unsigned int height() const;

        bool isEmpty() const;

        COLORREF *getRow(unsigned int row);

        COLORREF const *getRow(unsigned int row) const;

        void resize(unsigned int width, unsigned int height);

        size_t bufferSizeBytes() const;

        size_t rowSizeBytes() const;

        BITMAPINFO const &getBITMAPINFO() const;

        BITMAPINFOHEADER const &getBITMAPINFOHEADER() const;

        operator HBITMAP() const;

        HBITMAP detach();

        bool load(std::string const &filename);

        bool save(std::string const &filename) const;

        bool load(unsigned int resourceID);

    protected:
        void create(unsigned int width, unsigned int height);

        void copy(DibSection32 const &other);

        void destroy();

    private:
        BITMAPINFO              bmi_;
        HBITMAP                 hbitmap_;
        COLORREF                **lines_;
    };
}


#endif // CONTROLS_DIBSECTION32_HPP
