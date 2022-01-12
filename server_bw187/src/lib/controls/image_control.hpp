/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef IMAGE_CONTROL_HPP
#define IMAGE_CONTROL_HPP


#include "controls/dib_section32.hpp"


namespace controls
{
    /**
    *  This control displays an image inside itself.
    */
    class ImageControl : public CWnd
    {
    public:
        ImageControl();

        BOOL Create(DWORD style, RECT const &extents, CWnd *parent, unsigned int id = 0);

        BOOL subclass(unsigned int resourceID, CWnd *parent);

        void setImage(DibSection32 const &dibSection);

        DibSection32 const &getImage() const;

        DibSection32 &getImage();

        enum BorderStyle
        {
            BS_NONE,
            BS_BLACKRECT,
            BS_SUNKEN,
            BS_RAISED
        };

        BorderStyle getBorder() const;

        void setBorder(BorderStyle border);

        bool getPreserveAspectRatio() const;

        void setPreserveAspectRatio(bool preserve);

        COLORREF getBackgroundColour() const;

        void setBackgroundColour(COLORREF colour);

        unsigned int getBorderPadding() const;

        void setBorderPadding(unsigned int padding);

        void beginSet();

        void endSet();

    protected:
        afx_msg void OnPaint();

        afx_msg BOOL OnEraseBkgnd(CDC *dc);

        DECLARE_MESSAGE_MAP()

    private:
        DibSection32        image_;
        BorderStyle         border_;
        bool                aspect_;   
        COLORREF            background_;
        unsigned int        padding_;
        size_t              setCount_;
    };
}


#endif // IMAGE_CONTROL_HPP
