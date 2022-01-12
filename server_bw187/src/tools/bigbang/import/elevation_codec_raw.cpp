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
#include "import/elevation_codec_raw.hpp"
#include "import/elevation_image.hpp"
#include "raw_import_dlg.hpp"


/**
 *  This function returns true if the extension of filename is "RAW" or "R16".
 *
 *  @param filename     The name of the file.
 *  @returns            True if the filename ends in "bmp", ignoring case.
 */
/*virtual*/ bool 
ElevationCodecRAW::canHandleFile
(
    std::string const   &filename
)
{
    return 
        strcmpi("raw", BWResource::getExtension(filename).c_str()) == 0
        ||
        strcmpi("r16", BWResource::getExtension(filename).c_str()) == 0;
}


/**
 *  This function loads the RAW file in filename into image.  
 *
 *  @param filename     The file to load.
 *  @param image        The image to read.
 *  @returns            The result of loading.
 */
/*virtual*/ ElevationCodec::LoadResult 
ElevationCodecRAW::load
(
    std::string         const &filename, 
    ElevationImage      &image,
    float               *left           /* = NULL*/,
    float               *top            /* = NULL*/,
    float               *right          /* = NULL*/,
    float               *bottom         /* = NULL*/,
    bool                loadConfigDlg   /* = false*/
)
{
    FILE *file = NULL;
    try
    {   
        int   width         = -1;
        int   height        = -1;
        float minv          =  0.0f;
        float maxv          = 50.0f;
        float leftv         = -1.0f;
        float topv          = -1.0f;
        float rightv        = -1.0f;
        float bottomv       = -1.0f;
        bool  littleEndian  = true;

        // Try reading the meta-contents file:
        std::string xmlFilename = 
            BWResource::instance().removeExtension(filename) + ".xml";
        DataSectionPtr xmlFile = BWResource::openSection(xmlFilename, false);
        if (xmlFile != NULL)
        {
            width           = xmlFile->readInt  ("export/imageWidth"  , -1   );
            height          = xmlFile->readInt  ("export/imageHeight" , -1   );
            minv            = xmlFile->readFloat("export/minHeight"   ,  0.0f);
            maxv            = xmlFile->readFloat("export/maxHeight"   , 50.0f);
            leftv           = xmlFile->readFloat("export/left"        , -1.0f);
            topv            = xmlFile->readFloat("export/top"         , -1.0f);
            rightv          = xmlFile->readFloat("export/right"       , -1.0f);
            bottomv         = xmlFile->readFloat("export/bottom"      , -1.0f);
            littleEndian    = xmlFile->readBool ("export/littleEndian", true );
        }
        else if (loadConfigDlg)
        {
            RawImportDlg rawImportDlg(filename.c_str());
            if (rawImportDlg.DoModal() == IDOK)
            {
                unsigned int iwidth, iheight;
                rawImportDlg.getResult(iwidth, iheight, littleEndian);
                width  = (int)iwidth;
                height = (int)iheight;
            }
            else
            {
                return LR_CANCELLED;
            }
        }

        if (width <= 0 || height <= 0)
            return LR_BAD_FORMAT;

        if (left != NULL && leftv != -1.0f)
            *left = leftv;
        if (top != NULL && topv != -1.0f)
            *top = topv;
        if (right != NULL && rightv != -1.0f)
            *right = rightv;
        if (bottom != NULL && bottomv != -1.0f)
            *bottom = bottomv;

        // Read the height data:
        file = fopen(filename.c_str(), "rb");
        if (file == NULL)
            return LR_BAD_FORMAT;
        MemImage<uint16> image16(width, height);
        image.resize(width, height);
        fread
        (
            image16.getRow(0), 
            image16.width()*image.height(), 
            sizeof(uint16), 
            file
        );

        // Twiddle if big endian format:
        if (!littleEndian)
        {
            uint16 *data = image16.getRow(0);
            size_t sz = width*height;
            for (size_t i = 0; i < sz; ++i, ++data)
            {
                *data = ((*data & 0xff) << 8) + (*data >> 8);
            }
        }

        // Convert to floating point format:
        for (unsigned int y = 0; y < image.height(); ++y)
        {
            float  *p       = image  .getRow(y);
            uint16 const *q = image16.getRow(y);
            
            for (unsigned int x = 0; x < image.width(); ++x, ++p, ++q)
            {
                *p = Math::lerp(*q, (uint16)0, (uint16)65535, minv, maxv);
            }
        }        

        // Cleanup:
        fclose(file); file = NULL;
    }
    catch (...)
    {
        if (file != NULL)
        {
            fclose(file);
            file = NULL;
        }
        throw;
    }

    return LR_OK;
}


/**
 *  This function saves the image into the given file.
 *
 *  @param filename     The name of the file to save.
 *  @param image        The image to save.
 *  @returns            True if the image could be saved, false otherwise.
 */
/*virtual*/ bool 
ElevationCodecRAW::save
(
    std::string         const &filename, 
    ElevationImage      const &image,
    float               *left           /* = NULL*/,
    float               *top            /* = NULL*/,
    float               *right          /* = NULL*/,
    float               *bottom         /* = NULL*/
)
{
    if (image.isEmpty())
        return false;

    FILE  *file = NULL;
    uint8 *row  = NULL;
    try
    {
        // Get the range of data:
        float minv, maxv;
        image.range(minv, maxv);

        // Convert to a 16 bit format:
        MemImage<uint16> image16(image.width(), image.height());
        for (unsigned int y = 0; y < image.height(); ++y)
        {
            uint16 *p       = image16.getRow(y);
            float  const *q = image  .getRow(y);
            for (unsigned int x = 0; x < image.width(); ++x, ++p, ++q)
            {
                *p = Math::lerp(*q, minv, maxv, (uint16)0, (uint16)65535);
            }
        }

        // Flip the orientation vertically:
        image16.flip(false);

        // Write the raw data file:
        file = fopen(filename.c_str(), "wb");
        if (file == NULL)
            return false;
        fwrite
        (
            image16.getRow(0), 
            image16.width()*image16.height(), 
            sizeof(uint16), 
            file
        );
        fclose(file); file = NULL;

        // Write out the meta-contents file with the size of the terrain:
        std::string xmlFilename = 
            BWResource::instance().removeExtension(filename) + ".xml";
        DataSectionPtr xmlFile = BWResource::openSection(xmlFilename, true);
        if (xmlFile == NULL)
            return false;
	    xmlFile->writeFloat("export/minHeight"   , minv);
	    xmlFile->writeFloat("export/maxHeight"   , maxv);
	    xmlFile->writeInt  ("export/imageWidth"  , image.width ());
	    xmlFile->writeInt  ("export/imageHeight" , image.height());
        xmlFile->writeBool ("export/littleEndian", true);
        if (left != NULL)
            xmlFile->writeFloat("export/left", *left);
        if (top != NULL)
            xmlFile->writeFloat("export/top", *top);
        if (right != NULL)
            xmlFile->writeFloat("export/right", *right);
        if (bottom != NULL)
            xmlFile->writeFloat("export/bottom", *bottom);
        xmlFile->save(xmlFilename);
    }
    catch (...)
    {
        if (file != NULL)
            fclose(file);
        delete[] row; row = NULL;
        throw;
    }

    return true; // not yet implemented
}


/**
 *  This function indicates that we can read BMP files.
 *
 *  @returns            True.
 */
/*virtual*/ bool ElevationCodecRAW::canLoad() const
{
    return true;
}


/**
 *  This function indicates that we can write BMP files.
 *
 *  @returns            True.
 */
/*virtual*/ bool ElevationCodecRAW::canSave() const
{
    return true;
}


/**
 *  This function returns the singleton instance of the BMP elevation codec.
 *
 *  @returns            The BMP elevation codec.
 */
/*static*/ ElevationCodecRAW &ElevationCodecRAW::instance()
{
    static ElevationCodecRAW s_instance;
    return s_instance;
}
