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
#include "height/height_map.hpp"
#include "project/space_helpers.hpp"
#include "import/elevation_modify.hpp"
#include "import/terrain_utils.hpp"
#include "moo/texture_compressor.hpp"
#include "cstdmf/debug.hpp"
#include <limits>


DECLARE_DEBUG_COMPONENT2( "WorldEditor", 0 )


namespace
{
    // Extents of the representable range of heights in the texture:
    const float         MIN_HEIGHT                  = -2048.0;
    const float         MAX_HEIGHT                  = +2048.0;

    // The number of chunks to convert to a height map per frame:
    const unsigned int  NUM_CHUNKS_PER_FRAME        = 10;

    // The number of chunks after which we show a progress bar when creating
    // the height map texture:
    const unsigned int  NUM_CHUNKS_FOR_TEX_PROGRESS = 200;

    // Convert from height to uint16 as used in the height map:
    inline uint16 heightToUint16(float height)
    {
        return 
            Math::lerp
            (
                Math::clamp(MIN_HEIGHT, height, MAX_HEIGHT),
                MIN_HEIGHT, 
                MAX_HEIGHT, 
                (uint16)0, 
                (uint16)65535
            );
    }

    // Covnert a uint16 as used in the texture to a height:
    float uint16ToHeight(uint16 theight)
    {
        return 
            Math::lerp
            (
                theight, 
                (uint16)0, 
                (uint16)65535, 
                MIN_HEIGHT, 
                MAX_HEIGHT
            );
    }

    // Convert a height delta to an int16 for use in the texture
    inline int16 heightDeltaToInt16(float heightDelta)
    {
        return (int16)(65535.0f*heightDelta/(MAX_HEIGHT - MIN_HEIGHT));
    }

    // Convert a height to a normalized float (in the range [0.0f, 1.0f]):
    float heightToFloat(float height)
    {
        float cheight = Math::clamp(MIN_HEIGHT, height, MAX_HEIGHT);
        return (cheight - MIN_HEIGHT)/(MAX_HEIGHT - MIN_HEIGHT);
    }

    // Test whether two rectangles intersect, and if they do return the
    // common area.  The code assumes that left < right and bottom < top.
    bool
    rectanglesIntersect
    (
        int  left1, int  top1, int  right1, int  bottom1,
        int  left2, int  top2, int  right2, int  bottom2,
        int &ileft, int &itop, int &iright, int &ibottom
    )
    {
        if 
        (
            right1  < left2
            ||                     
            left1   > right2
            ||                     
            top1    < bottom2
            ||                     
            bottom1 > top2
        )
        {
            return false;
        }
        ileft   = std::max(left1  , left2  );
        itop    = std::min(top1   , top2   ); // Note that bottom < top so
        iright  = std::min(right1 , right2 ); 
        ibottom = std::max(bottom1, bottom2);
        return true;
    }
}


/**
 *  This function gets the current instance of the HeightMap.
 *
 *  @return             The current instance of the HeightMap.
 */
/*static*/ HeightMap &HeightMap::instance()
{
    static HeightMap s_instance;
    return s_instance;
}


/**
 *  This function should be called by the client of the height map to give it
 *  information about the current world.
 *
 *  @param spaceName        The current space's folder.
 *  @param localToWorldX    The function that converts chunk numbers to 
 *                          unsigned chunk numbers in the x-direction.
 *  @param localToWorldY    The function that converts chunk numbers to 
 *                          unsigned chunk numbers in the y-direction.
 *  @param width            The width of the space in chunks.
 *  @param height           The height of the space in chunks.
 *  @param createAndValidateTexture Generate a valid texture for the height?
 */
void HeightMap::spaceInformation
(
    std::string         const &spaceName,
    int                 localToWorldX,
    int                 localToWorldY,
    uint16              width,
    uint16              height,
    bool                createAndValidateTexture    /*= true*/
)
{
    // Do we support the more advanced rendering method or only the fixed 
    // function pipeline?:
    isPS2_ = 
        Moo::rc().psVersion() >= 0x200 
        && 
        Moo::rc().supportsTextureFormat(D3DFMT_L16);

    gridWidth_      = width;
    gridHeight_     = height;
    spaceName_      = spaceName;
    localToWorldX_  = localToWorldX;
    localToWorldY_  = localToWorldY;

    // Work out the number of poles:
    DataSectionPtr dataSection 
        = BWResource::openSection(spaceName + "/00000000o.cdata/terrain");  
    if (dataSection == NULL)
        return;
    BinaryPtr binData = dataSection->asBinary();
    if (binData == NULL)
        return;
    Moo::BaseTerrainBlock::TerrainBlockHeader *header = 
        (Moo::BaseTerrainBlock::TerrainBlockHeader*)binData->data();
    float poleSpacing = header->spacing_;
    numPoles_    = header->heightMapWidth_;
    blockWidth_  = static_cast<unsigned int>((poleSpacing*(numPoles_ - 3.0f)));
    blockHeight_ = static_cast<unsigned int>((poleSpacing*(numPoles_ - 3.0f)));

    // Base the size of the texture on the number of poles and block sizes:
    unsigned int recommendWidth  = recommendHeightMapSize(numPoles_*gridWidth_);
    unsigned int recommendHeight = recommendHeightMapSize(numPoles_*gridHeight_);
    recommendSize_ = std::max(recommendWidth, recommendHeight);
    if (isPS2_)
    {
        heightMap_ = MemImage<uint16>(recommendSize_, recommendSize_);
    }

    if (createAndValidateTexture)
        createTexture();

    updateMinMax();
}


/**
 *  This is called to save the HeightMap to the cached file.  It also makes
 *  sure that the HeightMap is up to date.
 */
void HeightMap::save()
{
    if (hasTexture())
    {
        // Draw missing chunks:
        ElevationModify modify(true); // exclusive read access to chunks
        CWaitCursor waitCursor;
        while (drawSomeChunks()) 
            ;
        saveCachedHeightMap();
    }
}


/**
 *  This is called when the HeightMap is no longer needed.
 */
void HeightMap::onStop()
{
    deleteTexture();
}


/**
 *  This function sets the HeightMap's texture as the active texture in Moo's
 *  rendering context.
 *
 *  @param textureStage     The stage to place the texture.
 */
void HeightMap::setTexture(uint8 textureStage)
{
    ensureValidTexture();
    Moo::rc().setTexture(textureStage, texture_);
}


/**
 *  This is called to update the HeightMap.  We use the opportunity to make
 *  sure that the HeightMap is up to date.
 *
 *  @param dtime            The time since this function was last called.
 */
void HeightMap::update(float /*dtime*/)
{
    // Draw the chunks:
    while (drawSomeChunks()) 
        ;
}


/**
 *  This function gets the HeightMap's texture.
 *
 *  @return                 The HeightMap's texture.
 */
DX::Texture *HeightMap::texture() const 
{ 
    ensureValidTexture();
    return texture_; 
}


/**
 *  This function gets the minimum height of the terrain, including any
 *  not yet placed imported terrain.
 *
 *  @return                 The mininum height of the terrain, taking into
 *                          account terrain which is being imported.
 */
float HeightMap::minHeight() const
{
    return minHeight_;
}


/**
 *  This function gets the maximum height of the terrain, including any
 *  not yet placed imported terrain.
 *
 *  @return                 The maximum height of the terrain, taking into
 *                          account terrain which is being imported.
 */
float HeightMap::maxHeight() const
{
    return maxHeight_;
}


/**
 *  This function makes sure that the min/max heights are up to date.
 */
void HeightMap::updateMinMax()
{
    minHeight_ = std::min(minHeightTerrain_, minHeightImpTerrain_);
    maxHeight_ = std::max(maxHeightTerrain_, maxHeightImpTerrain_);
    invalidTexture_ = true;
}


/**
 *  This function converts from a height to a normalized height (0..1) where
 *  0 represents -2048 and 1 represents 2048.  Values outside +/-2048 are 
 *  clipped.
 *
 *  @param height           The height in meters.
 *
 *  @return                 The normalized height.                
 */
/*static*/ float HeightMap::normalizeHeight(float height)
{
    return heightToFloat(height);
}


/**
 *  This function 'draws' imported terrain.
 *
 *  @param left         The left coordinate in grid units.
 *  @param top          The top coordinate in grid units.
 *  @param right        The right coordinate in grid units.
 *  @param bottom       The bottom coordinate in grid units.
 *  @param mode         How is the terrain data placed - replace existing 
 *                      terrain etc.
 *  @param minv         The height to scale the minimum of the imported
 *                      terrain.
 *  @param maxv         The height to scale the maximum of the imported 
 *                      terrain.
 *  @param alpha        The strength of the import (0...1).
 */
void HeightMap::drawImportedTerrain
(
    float               left,
    float               top,
    float               right,
    float               bottom,
    ElevationBlit::Mode mode,
    float               minv,
    float               maxv,
    float               alpha,
    ElevationImage      const &image
)
{
    restoreSubimage
    (
        impTerrLeft_,
        impTerrTop_,
        impTerrRight_,
        impTerrBottom_,
        impTerrSubImg_
    );

    impTerrLeft_    = left;
    impTerrTop_     = top;
    impTerrRight_   = right;
    impTerrBottom_  = bottom;

    extractSubimage
    (
        impTerrLeft_,
        impTerrTop_,
        impTerrRight_,
        impTerrBottom_,
        impTerrSubImg_
    );

    addTerrain
    (
        impTerrLeft_,
        impTerrTop_,
        impTerrRight_,
        impTerrBottom_,
        mode,
        minv,
        maxv,
        alpha,
        image
    );
}


/**
 *  This function restores the HeightMap after drawing some terrain.
 */
void HeightMap::undrawImportedTerrain()
{
    restoreSubimage
    (
        impTerrLeft_,
        impTerrTop_,
        impTerrRight_,
        impTerrBottom_,
        impTerrSubImg_
    );
}


/**
 *  This function is called when importing terrain is cancelled.
 */
void HeightMap::clearImpTerrain()
{
    restoreSubimage
    (
        impTerrLeft_,
        impTerrTop_,
        impTerrRight_,
        impTerrBottom_,
        impTerrSubImg_
    );

    impTerrLeft_   = impTerrTop_ = impTerrRight_ = impTerrBottom_ = 0.0f;
    impTerrSubImg_ = MemImage<uint16>();

    minHeightImpTerrain_ = +std::numeric_limits<float>::max();
    maxHeightImpTerrain_ = -std::numeric_limits<float>::max();
}


/**
 *  This function invalidates the area that the imported terrain would go
 *  into.
 */
void HeightMap::invalidateImportedTerrain()
{
    // Invalidate the imported terrain:
    int left   = (int)impTerrLeft_   - 1;
    int top    = (int)impTerrTop_    + 1;
    int right  = (int)impTerrRight_  + 1;    
    int bottom = (int)impTerrBottom_ - 1;
    if (right < left  ) std::swap(left, right );
    if (top   < bottom) std::swap(top , bottom);
    for (int y = bottom; y <= top; ++y)
    {
        for (int x = left; x <= right; ++x)
        {
            dirtyThumbnail(x + localToWorldX_, y + localToWorldY_);
        }
    }

    // Draw the chunks:
    while (drawSomeChunks()) 
        ;

    // The saved backbuffer is no longer needed:
    impTerrSubImg_ = MemImage<uint16>();
}


/**
 *  This function is called when a chunk is dirty and needs updating in the
 *  HeightMap.  
 *
 *  @param chunk            The dirty chunk.
 */
void HeightMap::dirtyThumbnail(Chunk *chunk)
{
    int16 gridX, gridY;
	::gridFromChunk(chunk, gridX, gridY);
    dirtyThumbnail(gridX, gridY);
}


/**
 *  This function is called to add a dirty chunk by coordinates.  We check
 *  that the chunk is not already dirty.  Note that the coordinates are in the
 *  range (localToWorldX_, localToWorldY_) to 
 *  (localToWorldX_ + gridWidth, localToWorldY_ + gridHeight).
 *
 *  @param gridX            The x coordinate of the dirty chunk.
 *  @param gridY            The y coordinate of the dirty chunk.
 */
void HeightMap::dirtyThumbnail(int gridX, int gridY)
{
    gridX -= localToWorldX_; // convert to (0..gridWidth - 1).
    gridY -= localToWorldY_; // convert to (0..gridHeight - 1).

    // See if undrawnChunks_ already has the chunk at x, y
    bool alreadyContains = false;
    for
    (
        std::list<GridPos>::iterator it = undrawnChunks_.begin();
        it != undrawnChunks_.end();
        ++it
    )
    {
        if (it->first == (int)gridX && it->second == (int)gridY)
        {
            alreadyContains = true;
            break;
        }
    }
    if (!alreadyContains)
        undrawnChunks_.push_back(GridPos(gridX, gridY));
}


/**
 *  This function gets an approximate height at the given position.  This 
 *  height includes currently imported terrain.
 *
 *  @param pos              The position to query, in grid units.
 *
 *  @return                 The height a the given position.
 */
float HeightMap::heightAtGridPos(Vector2 const &pos) const
{
    if 
    (
        pos.x < 0 || pos.x >= (float)gridWidth_ 
        ||
        pos.y < 0 || pos.y >= (float)gridHeight_
        ||
        !hasTexture()
    )
    {
        return std::numeric_limits<float>::max();
    }
    else
    {
        int twidth  = textureWidth();
        int theight = textureHeight();
        int tx = (int)(pos.x/gridWidth_ *twidth );
        int ty = (int)(pos.y/gridHeight_*theight);
        uint16 *bits;
        unsigned int pitch;
        beginTexture(&bits, &pitch);
        uint16 iheight = *(bits + ty*pitch + tx);
        endTexture();
        return uint16ToHeight(iheight);
    }
}


/**
 *  This function returns the minimum and maximum heights in the given range.
 *  We do not assume anything about the grid coordinates, though if the 
 *  rectangle formed by them does not intersect the terrain then 
 *  +/-std::numeric_limits<float>::max() (in minv/maxv resp) will be returned.
 *  The height returned includes currently imported terrain.
 *
 *  @param pos1         Coordinate 1 in grid units.
 *  @param pos2         Coordinate 2 in grid units.
 *  @param minv         The minimum height.
 *  @param maxv         The maximum height.
 */
void 
HeightMap::heightRange
(
    Vector2             const &pos1, 
    Vector2             const &pos2,
    float               &minv,
    float               &maxv
) const
{
    minv = +std::numeric_limits<float>::max();
    maxv = -std::numeric_limits<float>::max();

    int twidth  = textureWidth ();
    int theight = textureHeight();

    // Convert the coordinates to texture coordinates:
    int tx1 = (int)(pos1.x/gridWidth_ *twidth );
    int ty1 = (int)(pos1.y/gridHeight_*theight);
    int tx2 = (int)(pos2.x/gridWidth_ *twidth );
    int ty2 = (int)(pos2.y/gridHeight_*theight);

    // Normalize:
    if (tx1 > tx2) std::swap(tx1, tx2);
    if (ty1 > ty2) std::swap(ty1, ty2);

    // Increase by one (if clicked at a single point):
    ++tx2; ++ty2;

    // Clip:
    if
    (
        !rectanglesIntersect
        (
            tx1, ty2, tx2, ty1,
            0, (int)twidth-1, (int)theight-1, 0,
            tx1, ty2, tx2, ty1
        )
    )
    {
        return;
    }

    // Handle degenerate cases:
    if (tx1 == tx2 || ty1 == ty2)
        return;

    // The result as an uint16:
    uint16 imin = 65535;
    uint16 imax =     0;

    uint16 *bits;
    unsigned int pitch;
    beginTexture(&bits, &pitch);
    for (int y = ty1; y <= ty2; ++y)
    {
        uint16 const *p = bits + y*pitch + tx1;
        for (int x = tx1; x <= tx2; ++x, ++p)
        {
            imin = std::min(imin, *p);
            imax = std::max(imax, *p);
        }
    }
    endTexture();

    // Convert the result to floating point:
    minv = uint16ToHeight(imin);
    maxv = uint16ToHeight(imax);
}


/**
 *  This makes sure that the HeightMap is valid by drawing all dirty chunks.
 *  Optionally a progress bar is shown.
 *
 *  @param showProgress     If true then a progress screen is shown if there
 *                          are more than NUM_CHUNKS_FOR_TEX_PROGRESS dirty
 *                          chunks to process.
 */
void HeightMap::makeValid(bool showProgress /*= true*/)
{
    ElevationModify modify(true); // exclusive read access to chunks

    BigBangProgressBar *progress  = NULL;
	ProgressTask       *genHMTask = NULL;
    
    try
    { 
        if 
        (
            showProgress 
            && 
            undrawnChunks_.size() >= NUM_CHUNKS_FOR_TEX_PROGRESS
        )
        {
            progress = new BigBangProgressBar();
            genHMTask =
                new ProgressTask
                (
                    progress, 
                    "Generating height map texture", 
                    (float)(undrawnChunks_.size()/NUM_CHUNKS_PER_FRAME)
                );
        }

        while (drawSomeChunks()) 
        {
            if (genHMTask != NULL)
                genHMTask->step();
        }

        delete progress ; progress  = NULL;
        delete genHMTask; genHMTask = NULL;
    }
    catch (...)
    {
        delete progress ; progress  = NULL;
        delete genHMTask; genHMTask = NULL;
        throw;
    }
}


/**
 *  This is called to delete the saved height map and prevent it from being 
 *  saved.  This is because the user is closing this space and not saving it,
 *  and so the HeightMap is completely invalid.
 */
void HeightMap::doNotSaveHeightMap()
{
    // Delete the texture:
    if (texture_ != NULL)
    {
        texture_->Release();
        texture_ = NULL;
    }

    // Delete the cached file:
    std::string filename = 
         BWResource::resolveFilename(cachedHeightMapName());
    ::unlink(filename.c_str());

    // Clear the height map:
    if (!isPS2_)
        heightMap_ = MemImage<uint16>();
}


/**
 *  This function returns true if we are using the more advanced rendering 
 *  path.
 */
bool HeightMap::isPS2() const
{
    return isPS2_;
}


/**
 *  This extracts a sub-image of the terrain into the given image for later
 *  retrieval.  The image is resized as necessary.  The coordinates are clipped
 *  if necessary.
 *
 *  @param leftf        The left coordinate in grid units.
 *  @param topf         The top coordinate in grid units.
 *  @param rightf       The right coordinate in grid units.
 *  @param bottomf      The bottom coordinate in grid units.
 *  @param subImage     The image to save to.
 */
void HeightMap::extractSubimage
(
    float               leftf,
    float               topf,
    float               rightf,
    float               bottomf,
    MemImage<uint16>    &subImage
) const
{
    subImage = MemImage<uint16>();

    int left, top, right, bottom;
    bool ok = 
        toTexCoords(leftf, topf, rightf, bottomf, left, top, right, bottom);
    if (!ok)
        return;

    // Blit into the subimage:
    subImage.resize(right - left + 1, top - bottom + 1);
    uint16 *bits;
    unsigned int pitch;
    beginTexture(&bits, &pitch);
    for (int y = bottom; y <= top; ++y)
    {
        uint16 *p       = subImage.getRow(y - bottom);
        uint16 const *q = bits + y*pitch + left;
        for (int x = left; x <= right; ++x, ++p, ++q)
        {
            *p = *q;
        }
    }
    endTexture();

    invalidTexture_ = true;
}


/**
 *  This restores a sub-image of the terrain from the given image.  The image 
 *  is resized as necessary.  The coordinates are clipped if necessary.
 *
 *  @param leftf        The left coordinate in grid units.
 *  @param topf         The top coordinate in grid units.
 *  @param rightf       The right coordinate in grid units.
 *  @param bottomf      The bottom coordinate in grid units.
 *  @param subImage     The image to restore from.
 */
void HeightMap::restoreSubimage
(
    float               leftf,
    float               topf,
    float               rightf,
    float               bottomf,
    MemImage<uint16>    const &subImage
)
{
    if (subImage.isEmpty())
        return;

    int left, top, right, bottom;
    bool ok = 
        toTexCoords(leftf, topf, rightf, bottomf, left, top, right, bottom);
    if (!ok)
        return;

    // Blit into the subimage:
    uint16 *bits;
    unsigned int pitch;
    beginTexture(&bits, &pitch);
    for (int y = bottom; y <= top; ++y)
    {
        uint16 *p       = bits + y*pitch + left;;
        uint16 const *q = subImage.getRow(y - bottom);            
        for (int x = left; x <= right; ++x, ++p, ++q)
        {
            *p = *q;
        }
    }
    endTexture();

    invalidTexture_ = true;
}


/**
 *  This function is used to 'blit' imported terrain into the HeightMap 
 *  using the appropriate mode.
 *
 *  @param leftf        The left coordinate in grid units.
 *  @param topf         The top coordinate in grid units.
 *  @param rightf       The right coordinate in grid units.
 *  @param bottomf      The bottom coordinate in grid units.
 *  @param mode         How is the terrain data placed - replace existing 
 *                      terrain etc.
 *  @param minv         The height to scale the minimum of the imported
 *                      terrain.
 *  @param maxv         The height to scale the maximum of the imported 
 *                      terrain.
 *  @param alpha        The strength of the import (0...1).
 *  @param image        The imported terrain.
 */
void HeightMap::addTerrain
(
    float               leftf,
    float               topf,
    float               rightf,
    float               bottomf,
    ElevationBlit::Mode mode,
    float               minv,
    float               maxv,
    float               alpha, 
    ElevationImage      const &image
)
{
    if (minv > maxv) 
        std::swap(minv, maxv);

    float offset = 0.0f;
    float scale  = 0.0f;
    switch (mode)
    {
    case ElevationBlit::ADDITIVE:       // fall through deliberate
    case ElevationBlit::SUBTRACTIVE:   
        offset = 0.0f;
        scale  = alpha*(maxv - minv);
        break;
    case ElevationBlit::OVERLAY:
        offset = -0.5f*alpha*(maxv - minv);
        scale  = alpha*(maxv - minv);
        break;
    case ElevationBlit::MAX:            // fall through deliberate
    case ElevationBlit::MIN:            // fall through deliberate
    case ElevationBlit::REPLACE:
        offset = minv;
        scale  = alpha*(maxv - minv);
        break;
    }

    int ileft   = 0;
    int itop    = image.height() - 1;
    int iright  = image.width () - 1;
    int ibottom = 0;

    int left, top, right, bottom;
    bool ok = 
        toTexCoords
        (
            leftf , topf , rightf , bottomf, 
            left  , top  , right  , bottom ,
            &ileft, &itop, &iright, &ibottom
        );
    if (!ok)
        return;

    switch (mode)
    {
    case ElevationBlit::OVERLAY:       // fall through
    case ElevationBlit::ADDITIVE:
        blitAdditive
        (
            image, 
            left , top , right , bottom , 
            ileft, itop, iright, ibottom,
            scale, offset
        );
        break;
    case ElevationBlit::SUBTRACTIVE:
        blitSubtractive
        (
            image, 
            left , top , right , bottom , 
            ileft, itop, iright, ibottom,
            scale, offset
        );
        break;
    case ElevationBlit::MIN:
        blitMin
        (
            image, 
            left , top , right , bottom , 
            ileft, itop, iright, ibottom,
            scale, offset
        );
        break;
    case ElevationBlit::MAX:
        blitMax
        (
            image, 
            left , top , right , bottom , 
            ileft, itop, iright, ibottom,
            scale, offset
        );
        break;
    case ElevationBlit::REPLACE:
        blitReplace
        (
            image, 
            left , top , right , bottom , 
            ileft, itop, iright, ibottom,
            scale, offset
        );
        break;
    }
}


/**
 *  This is the HeightMap constructor.
 */
HeightMap::HeightMap() :
    texture_(NULL),
    gridWidth_(0),
    gridHeight_(0),
    minHeight_(+std::numeric_limits<float>::max()),
    maxHeight_(-std::numeric_limits<float>::max()),
    minHeightImpTerrain_(+std::numeric_limits<float>::max()),
    maxHeightImpTerrain_(-std::numeric_limits<float>::max()),
    minHeightTerrain_(+std::numeric_limits<float>::max()),
    maxHeightTerrain_(-std::numeric_limits<float>::max()),
    numPoles_(0),
    blockWidth_(0),
    blockHeight_(0),
    localToWorldX_(0),
    localToWorldY_(0),
    impTerrLeft_(0.0f),
    impTerrTop_(0.0f),
    impTerrRight_(0.0f),
    impTerrBottom_(0.0f),
    initialized_(false),
    invalidTexture_(true),
    isEightBitTex_(true),
    isPS2_(false),
    recommendSize_(0)
{
}


/**
 *  This is the HeightMap destructor.
 */
HeightMap::~HeightMap()
{
}


/**
 *  This function creates the texture used to draw the height map.  We try and
 *  load the height map from disk, and if that fails we recreate it.
 */
void HeightMap::createTexture()
{
    if (texture_ != NULL)
    {
        texture_->Release();
        texture_ = NULL;
    }

    // Try loading off disk
    if (loadCachedHeightMap())
    {
        // Draw missing chunks:        
        CWaitCursor waitCursor;
        makeValid();
    }
    // Recreate from scratch:
    else
    {
        ElevationModify modify(true); // exclusive read access to chunks
        CWaitCursor waitCursor;
        redrawAllChunks();
    }
}


/**
 *  This function deletes the texture used to draw the height map.  We save
 *  it to disk for later retrieval.
 */
void HeightMap::deleteTexture()
{
    if (texture_ != NULL)
    {
        texture_->Release();
        texture_ = NULL;
    }
    invalidTexture_ = true;
}


/**
 *  This function takes the chunk at gx, gy and adds it to our height map.
 *
 *  @param gx           The x coordinate of the chunk in grid coordiantes.
 *  @param gy           The y coordinate of the chunk in grid coordiantes.
 */
void HeightMap::chunkToHeightMap(int gx, int gy)
{
    if (gx < 0 || gx >= (int)gridWidth_ || gy < 0 || gy >= (int)gridHeight_)
        return;

    int twidth  = textureWidth ();
    int theight = textureHeight();

    // The coordinates of this tile within our texture:
    unsigned int dx1 = Math::lerp(gx    , 0, (int)gridWidth_ , 0U, twidth  - 1U);
    unsigned int dy1 = Math::lerp(gy    , 0, (int)gridHeight_, 0U, theight - 1U);
    unsigned int dx2 = Math::lerp(gx + 1, 0, (int)gridWidth_ , 0U, twidth  - 1U);
    unsigned int dy2 = Math::lerp(gy + 1, 0, (int)gridHeight_, 0U, theight - 1U);

    // Get the chunk's data:
    MemImage<float> terrainImage;
    gx += localToWorldX_;
    gy += localToWorldY_;
    BinaryPtr binary = TerrainUtils::getTerrain(gx, gy, terrainImage, false);

    if (!terrainImage.isEmpty())
    {
        // Iterate over the pixels for this chunk in the texture, bicubic 
        // sampling from the terrain data:
        uint16 *bits;
        unsigned int pitch;
        beginTexture(&bits, &pitch);
        for (unsigned dy = dy1; dy <= dy2; ++dy)
        {
            uint16 *p = bits + dy*pitch + dx1;
            float  sy = Math::lerp(dy, dy1, dy2, 0.0f, (float)numPoles_ - 3);
            for (unsigned int dx = dx1; dx <= dx2; ++dx, ++p)
            {
                float sx     = Math::lerp(dx, dx1, dx2, 0.0f, (float)numPoles_ - 3);
                float height = terrainImage.getValue(sx, sy);
                minHeightTerrain_ = std::min(minHeightTerrain_, height);
                maxHeightTerrain_ = std::max(maxHeightTerrain_, height);
                *p = heightToUint16(height);
            }
        }
        endTexture();
    }
    // In case the chunk could not be read then fill this part of the texture
    // with the height 0.0:
    else
    {
        uint16 fillValue = heightToUint16(0.0f);
        uint16 *bits;
        unsigned int pitch;
        beginTexture(&bits, &pitch);
        for (unsigned dy = dy1; dy <= dy2; ++dy)
        {
            uint16 *p = bits + dy*pitch + dx1;
            for (unsigned int dx = dx1; dx <= dx2; ++dx, ++p)
            {
                *p = fillValue;
            }
        }
        endTexture();
    }

    invalidTexture_ = true;
}


/**
 *  This function draws up to NUM_CHUNKS_PER_FRAME dirty chunks into the
 *  height map.
 *
 *  @returns            True if we drew NUM_CHUNKS_PER_FRAME dirty chunks.
 */
bool HeightMap::drawSomeChunks()
{
    if (undrawnChunks_.empty())
        return false;

    for 
    (
        unsigned int count = 0; 
        count < NUM_CHUNKS_PER_FRAME && !undrawnChunks_.empty();
        ++count
    )
    {
        GridPos gp = undrawnChunks_.front();
        chunkToHeightMap(gp.first, gp.second);
        undrawnChunks_.pop_front();
    }

    return true;
}


/**
 *  This function creates the recommended size of the height map in the.
 *
 *  @param x            The coordinate that we want the size of.
 *
 *  @return             The recommended size of this coordinate.
 */
unsigned int HeightMap::recommendHeightMapSize(unsigned int x) const
{
    // Find the next power of 2, or clamp to 2048.
    unsigned int result = 1;
    while (result < x && result < 2048)
        result *= 2;
    return result;
}


/**
 *  This function converts from grid coordinates to height map texture 
 *  coordinates.  It clips the coordinates and optionally returns the 
 *  clipped area.
 *
 *  @param leftf        The left coordinate in grid units.
 *  @param topf         The top coordinate in grid units.
 *  @param rightf       The right coordinate in grid units.
 *  @param bottomf      The bottom coordinate in grid units.
 *  @param texLeft      The left texture coordinate.
 *  @param texTop       The top texture coordinate.
 *  @param texRight     The right texture coordinate.
 *  @param texBottom    The bottom texture coordinate.
 *  @param ileft        The left clipped coordinate.
 *  @param itop         The top clipped coordinate.
 *  @param iright       The right clipped coordinate.
 *  @param ibottom      The bottom clipped coordinate.
 *
 *  @return             True if the rectangle intersects the texture rectangle,
 *                      false if the rectangle did not intersect.
 */
bool HeightMap::toTexCoords
(
    float               leftf,
    float               topf,
    float               rightf,
    float               bottomf,
    int                 &texLeft,
    int                 &texTop,
    int                 &texRight,
    int                 &texBottom,
    int                 *ileft      /*= NULL*/,
    int                 *itop       /*= NULL*/,
    int                 *iright     /*= NULL*/,
    int                 *ibottom    /*= NULL*/
) const
{
    // Normalize the rectangle:
    if (leftf > rightf)
        std::swap(leftf, rightf);
    if (bottomf > topf)
        std::swap(bottomf, topf);

    unsigned int twidth  = textureWidth();
    unsigned int theight = textureHeight();

    // Convert to texture coordinates:
    texLeft   = (int)(leftf  *twidth /gridWidth_ );
    texTop    = (int)(topf   *theight/gridHeight_);
    texRight  = (int)(rightf *twidth /gridWidth_ );
    texBottom = (int)(bottomf*theight/gridHeight_);

    // Save these coordinates:
    int otexLeft   = texLeft;
    int otexTop    = texTop;
    int otexRight  = texRight;
    int otexBottom = texBottom;
 
    if (texLeft == texRight || texTop == texBottom)
        return false;

    // Clip to the texture coordinates and return true if there is some
    // overlap:
    bool result =
        rectanglesIntersect
        (
            texLeft, texTop         , texRight        , texBottom,
            0      , (int)twidth - 1, (int)theight - 1, 0        ,
            texLeft, texTop         , texRight        , texBottom
        );
    if (result && ileft != NULL)
    {
        int width  = *iright - *ileft  ;
        int height = *itop   - *ibottom;
        *ileft   = Math::lerp(texLeft  , otexLeft  , otexRight , 0, width  - 1);
        *itop    = Math::lerp(texTop   , otexTop   , otexBottom, 0, height - 1);
        *iright  = Math::lerp(texRight , otexLeft  , otexRight , 0, width  - 1);
        *ibottom = Math::lerp(texBottom, otexTop   , otexBottom, 0, height - 1);
		result = (*ileft != *iright) && (*itop != *ibottom); // don't allow degenerate results
    }
    return result;
}


/**
 *  This function blits in imported terrain using the additive mode.
 *
 *  @param image        The terrain to import.
 *  @param left         The left coordinate in grid units.
 *  @param top          The top coordinate in grid units.
 *  @param right        The right coordinate in grid units.
 *  @param bottom       The bottom coordinate in grid units..
 *  @param ileft        The left clipped corodinate.
 *  @param itop         The top clipped corodinate.
 *  @param iright       The right clipped corodinate.
 *  @param ibottom      The bottom clipped corodinate.
 *  @param scale        The scale to apply to the imported terrain.
 *  @param offset       The offset to apply to the imported terrain.
 */
void 
HeightMap::blitAdditive
(
    ElevationImage      const &image,
    int                 left,
    int                 top,
    int                 right,
    int                 bottom,
    int                 ileft,
    int                 itop,
    int                 iright,
    int                 ibottom,
    float               scale,
    float               offset
)
{
    // Blit into the subimage:
    uint16 mint   = 65535;
    uint16 maxt   =     0;
    uint16 result =     0;

    minHeightImpTerrain_ = +std::numeric_limits<float>::max();
    maxHeightImpTerrain_ = +std::numeric_limits<float>::max();

    uint16 *bits;
    unsigned int pitch;
    beginTexture(&bits, &pitch);
    for (int y = bottom; y <= top; ++y)
    {
        uint16 *p   = bits + y*pitch + left;
        int    sy   = Math::lerp(y, top, bottom, itop, ibottom);  
        float  *row = image.getRow(sy);
        for (int x = left; x <= right; ++x, ++p)
        {
            int sx = Math::lerp(x, left, right, ileft, iright);
            float height = *(row + sx)*scale + offset;
            result = *p + heightDeltaToInt16(height);
            mint = std::min(mint, result);
            maxt = std::max(maxt, result);
            *p = result;
        }
    }
    endTexture();

    minHeightImpTerrain_ = uint16ToHeight(mint);
    maxHeightImpTerrain_ = uint16ToHeight(maxt);
    invalidTexture_      = true;
}


/**
 *  This function blits in imported terrain using the subtractive mode.
 *
 *  @param image        The terrain to import.
 *  @param left         The left coordinate in grid units.
 *  @param top          The top coordinate in grid units.
 *  @param right        The right coordinate in grid units.
 *  @param bottom       The bottom coordinate in grid units..
 *  @param ileft        The left clipped corodinate.
 *  @param itop         The top clipped corodinate.
 *  @param iright       The right clipped corodinate.
 *  @param ibottom      The bottom clipped corodinate.
 *  @param scale        The scale to apply to the imported terrain.
 *  @param offset       The offset to apply to the imported terrain.
 */
void 
HeightMap::blitSubtractive
(
    ElevationImage  const &image,
    int             left,
    int             top,
    int             right,
    int             bottom,
    int             ileft,
    int             itop,
    int             iright,
    int             ibottom,
    float           scale,
    float           offset
)
{
    // Blit into the subimage:
    uint16 mint   = 65535;
    uint16 maxt   =     0;
    uint16 result =     0;

    minHeightImpTerrain_ = +std::numeric_limits<float>::max();
    maxHeightImpTerrain_ = +std::numeric_limits<float>::max();

    uint16 *bits;
    unsigned int pitch;
    beginTexture(&bits, &pitch);
    for (int y = bottom; y <= top; ++y)
    {
        uint16 *p   = bits + y*pitch + left;
        int    sy   = Math::lerp(y, top, bottom, itop, ibottom);  
        float  *row = image.getRow(sy);
        for (int x = left; x <= right; ++x, ++p)
        {
            int sx = Math::lerp(x, left, right, ileft, iright);
            float height = *(row + sx)*scale + offset;
            result = *p - heightDeltaToInt16(height);
            mint = std::min(mint, result);
            maxt = std::max(maxt, result);
            *p = result;
        }
    }
    endTexture();

    minHeightImpTerrain_ = uint16ToHeight(mint);
    maxHeightImpTerrain_ = uint16ToHeight(maxt);
    invalidTexture_      = true;
}


/**
 *  This function blits in imported terrain using the minimum mode.
 *
 *  @param image        The terrain to import.
 *  @param left         The left coordinate in grid units.
 *  @param top          The top coordinate in grid units.
 *  @param right        The right coordinate in grid units.
 *  @param bottom       The bottom coordinate in grid units..
 *  @param ileft        The left clipped corodinate.
 *  @param itop         The top clipped corodinate.
 *  @param iright       The right clipped corodinate.
 *  @param ibottom      The bottom clipped corodinate.
 *  @param scale        The scale to apply to the imported terrain.
 *  @param offset       The offset to apply to the imported terrain.
 */
void 
HeightMap::blitMin
(
    ElevationImage  const &image,
    int             left,
    int             top,
    int             right,
    int             bottom,
    int             ileft,
    int             itop,
    int             iright,
    int             ibottom,
    float           scale,
    float           offset
)
{
    // Blit into the subimage:
    uint16 mint   = 65535;
    uint16 maxt   =     0;
    uint16 result =     0;

    minHeightImpTerrain_ = +std::numeric_limits<float>::max();
    maxHeightImpTerrain_ = +std::numeric_limits<float>::max();

    uint16 *bits;
    unsigned int pitch;
    beginTexture(&bits, &pitch);
    for (int y = bottom; y <= top; ++y)
    {
        uint16 *p   = bits + y*pitch + left;
        int    sy   = Math::lerp(y, top, bottom, itop, ibottom);  
        float  *row = image.getRow(sy);
        for (int x = left; x <= right; ++x, ++p)
        {
            int sx = Math::lerp(x, left, right, ileft, iright);
            float height = *(row + sx)*scale + offset;
            result = std::min(*p, heightToUint16(height));
            mint = std::min(mint, result);
            maxt = std::max(maxt, result);
            *p = result;
        }
    }
    endTexture();

    minHeightImpTerrain_ = uint16ToHeight(mint);
    maxHeightImpTerrain_ = uint16ToHeight(maxt);
    invalidTexture_      = true;
}


/**
 *  This function blits in imported terrain using the maximum mode.
 *
 *  @param image        The terrain to import.
 *  @param left         The left coordinate in grid units.
 *  @param top          The top coordinate in grid units.
 *  @param right        The right coordinate in grid units.
 *  @param bottom       The bottom coordinate in grid units..
 *  @param ileft        The left clipped corodinate.
 *  @param itop         The top clipped corodinate.
 *  @param iright       The right clipped corodinate.
 *  @param ibottom      The bottom clipped corodinate.
 *  @param scale        The scale to apply to the imported terrain.
 *  @param offset       The offset to apply to the imported terrain.
 */
void 
HeightMap::blitMax
(
    ElevationImage  const &image,
    int             left,
    int             top,
    int             right,
    int             bottom,
    int             ileft,
    int             itop,
    int             iright,
    int             ibottom,
    float           scale,
    float           offset
)
{
    // Blit into the subimage:
    uint16 mint   = 65535;
    uint16 maxt   =     0;
    uint16 result =     0;

    minHeightImpTerrain_ = +std::numeric_limits<float>::max();
    maxHeightImpTerrain_ = +std::numeric_limits<float>::max();

    uint16 *bits;
    unsigned int pitch;
    beginTexture(&bits, &pitch);
    for (int y = bottom; y <= top; ++y)
    {
        uint16 *p   = bits + y*pitch + left;
        int    sy   = Math::lerp(y, top, bottom, itop, ibottom);  
        float  *row = image.getRow(sy);
        for (int x = left; x <= right; ++x, ++p)
        {
            int sx = Math::lerp(x, left, right, ileft, iright);
            float height = *(row + sx)*scale + offset;
            result = std::max(*p, heightToUint16(height));
            mint = std::min(mint, result);
            maxt = std::max(maxt, result);
            *p = result;
        }
    }
    endTexture();

    minHeightImpTerrain_ = uint16ToHeight(mint);
    maxHeightImpTerrain_ = uint16ToHeight(maxt);
    invalidTexture_      = true;
}


/**
 *  This function blits in imported terrain using the replace mode.
 *
 *  @param image        The terrain to import.
 *  @param left         The left coordinate in grid units.
 *  @param top          The top coordinate in grid units.
 *  @param right        The right coordinate in grid units.
 *  @param bottom       The bottom coordinate in grid units..
 *  @param ileft        The left clipped corodinate.
 *  @param itop         The top clipped corodinate.
 *  @param iright       The right clipped corodinate.
 *  @param ibottom      The bottom clipped corodinate.
 *  @param scale        The scale to apply to the imported terrain.
 *  @param offset       The offset to apply to the imported terrain.
 */
void 
HeightMap::blitReplace
(
    ElevationImage  const &image,
    int             left,
    int             top,
    int             right,
    int             bottom,
    int             ileft,
    int             itop,
    int             iright,
    int             ibottom,
    float           scale,
    float           offset
)
{
    // Blit into the subimage:
    uint16 mint   = 65535;
    uint16 maxt   =     0;
    uint16 result =     0;

    minHeightImpTerrain_ = +std::numeric_limits<float>::max();
    maxHeightImpTerrain_ = +std::numeric_limits<float>::max();

    uint16 *bits;
    unsigned int pitch;
    beginTexture(&bits, &pitch);
    for (int y = bottom; y <= top; ++y)
    {
        uint16 *p   = bits + y*pitch + left;
        int    sy   = Math::lerp(y, top, bottom, itop, ibottom);  
        float  *row = image.getRow(sy);
        for (int x = left; x <= right; ++x, ++p)
        {
            int sx = Math::lerp(x, left, right, ileft, iright);
            float height = *(row + sx)*scale + offset;
            result = heightToUint16(height);
            mint = std::min(mint, result);
            maxt = std::max(maxt, result);
            *p = result;
        }
    }
    endTexture();

    minHeightImpTerrain_ = uint16ToHeight(mint);
    maxHeightImpTerrain_ = uint16ToHeight(maxt);
    invalidTexture_      = true;
}


/**
 *  This is the file name of the cached height map.
 */
std::string HeightMap::cachedHeightMapName() const
{
    return spaceName_ + "/space.height.raw";
}


/**
 *  This function tries to load the cached height map.
 *
 *  @return         True if the load was successful, false otherwise.
 */
bool HeightMap::loadCachedHeightMap()
{
    std::string textureName = 
        BWResource::resolveFilename(cachedHeightMapName());
    FILE *file = NULL;
    try
    {
        file = fopen(textureName.c_str(), "rb");
        if (file == NULL)
            return false;
        fseek(file, 0, SEEK_END);
        long sz = ftell(file);
        fseek(file, 0, SEEK_SET);
        long width = (long)std::sqrt((float)(sz/sizeof(uint16)));
        long height = width;
        if (isPS2_)
        {
            if (texture_ != NULL)
            {
                texture_->Release(); texture_ = NULL;
            }
            {
                HRESULT hr = 
                    D3DXCreateTexture
                    (
                        Moo::rc().device(),
                        width,
                        height,
                        1,                  // no mipmap levels
                        0,                  // usage
                        D3DFMT_L16,
                        D3DPOOL_MANAGED,
                        &texture_
                    );
            }
        }
        else
        {
            heightMap_ = MemImage<uint16>(width, height);
        }
        uint16 minv = 65535;
        uint16 maxv =     0;
        uint16 *bits;
        unsigned int pitch;
        beginTexture(&bits, &pitch);
        for (long y = 0; y < height; ++y)
        {
            uint16 *row = bits + y*pitch;
            fread(row, sizeof(uint16), width, file);
            uint16 *p = row;
            for (long x = 0; x < width; ++x)
            {
                minv = std::min(*p, minv);
                maxv = std::max(*p, maxv);
                ++p;
            }
        }
        endTexture();
        minHeightTerrain_ = uint16ToHeight(minv);
        maxHeightTerrain_ = uint16ToHeight(maxv);
        fclose(file);
        file = NULL;
        invalidTexture_ = true;
        return true;
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
}


/**
 *  This function saves the cached height map.
 */
void HeightMap::saveCachedHeightMap()
{
    if (!heightMap_.isEmpty())
    {
        clearImpTerrain();
        std::string textureName = 
            BWResource::resolveFilename(cachedHeightMapName());
        FILE *file = NULL;
        try
        {
            file = fopen(textureName.c_str(), "wb");
            if (file == NULL)
                return;
            unsigned int twidth  = textureWidth ();
            unsigned int theight = textureHeight();
            uint16 *bits;
            unsigned int pitch;
            beginTexture(&bits, &pitch);
            for (size_t y = 0; y < theight; ++y)
            {
                uint16 *row = bits + y*pitch;
                fwrite(row, sizeof(uint16), twidth, file);
            }
            endTexture();
            fclose(file);
            file = NULL;
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
    }
}


/**
 *  This function redraws all the chunks, whether they are dirty or not.
 */
void HeightMap::redrawAllChunks()
{
    if (isPS2_)
    {
        if (texture_ != NULL)
        {
            texture_->Release(); texture_ = NULL;
        }
        HRESULT hr =
            D3DXCreateTexture
            (
                Moo::rc().device(),
                recommendSize_,
                recommendSize_,
                1,                  // no mipmap levels
                0,                  // usage
                D3DFMT_L16,
                D3DPOOL_MANAGED,
                &texture_
            );
		MF_ASSERT(SUCCEEDED(hr));
    }
    else
    {
        heightMap_ = MemImage<uint16>(recommendSize_, recommendSize_);
    }

    minHeightTerrain_ = +std::numeric_limits<float>::max();
    maxHeightTerrain_ = -std::numeric_limits<float>::max();

    BigBangProgressBar *progress  = NULL;
	ProgressTask       *genHMTask = NULL;
    
    try
    { 
        if (gridHeight_*gridWidth_ >= NUM_CHUNKS_FOR_TEX_PROGRESS)
        {
            progress = new BigBangProgressBar();
            genHMTask =
                new ProgressTask
                (
                    progress, 
                    "Generating height map texture", 
                    (float)(gridHeight_)
                );
        }

        // Redraw all chunks:
        for (unsigned int y = 0; y < gridHeight_; ++y)
        {
            if (genHMTask != NULL)
                genHMTask->step();
            for (unsigned int x = 0; x < gridWidth_; ++x)
            {
                chunkToHeightMap(x, y);
            }
        }

        delete progress ; progress  = NULL;
        delete genHMTask; genHMTask = NULL;
    }
    catch (...)
    {
        delete progress ; progress  = NULL;
        delete genHMTask; genHMTask = NULL;
        throw;
    }

    // Everything is drawn:
    undrawnChunks_.clear();
}


/**
 *  This function makes sure that we have a texture that matches the height 
 *  map.
 */
void HeightMap::ensureValidTexture() const
{
    // We don't need to update this in PS2.0 systems:
    if (isPS2_)
        return;
    
    // Is the texture currently valid:
    if (!invalidTexture_ && texture_ != NULL)
        return;

    // Create the texture:
    if (texture_ == NULL)
    {
        isEightBitTex_ = Moo::rc().supportsTextureFormat(D3DFMT_L8);
        if (isEightBitTex_)
        {
            HRESULT hr =
                D3DXCreateTexture
                (
                    Moo::rc().device(),
                    heightMap_.width(),
                    heightMap_.height(),
                    1,                  // no mipmap levels
                    0,                  // usage
                    D3DFMT_L8,
                    D3DPOOL_MANAGED,
                    &texture_
                );
			MF_ASSERT(SUCCEEDED(hr));
        }
        if (texture_ == NULL)
        {
            HRESULT hr =
                D3DXCreateTexture
                (
                    Moo::rc().device(),
                    heightMap_.width(),
                    heightMap_.height(),
                    1,                  // no mipmap levels
                    0,                  // usage
                    D3DFMT_X8R8G8B8,
                    D3DPOOL_MANAGED,
                    &texture_
                );
			MF_ASSERT(SUCCEEDED(hr));
            isEightBitTex_ = false;
        }
    }

    D3DLOCKED_RECT rect;
    HRESULT hr = texture_->LockRect(0, &rect, NULL, 0);
    if (FAILED(hr))
    {
        ERROR_MSG("Unable to lock height map texture");
    }

    uint16 minh = heightToUint16(minHeight());
    uint16 maxh = heightToUint16(maxHeight());

    if (isEightBitTex_)
    {
        uint8 *textureBits = static_cast<uint8 *>(rect.pBits);
        if (minh == maxh)
        {
            for (unsigned int y = 0; y < heightMap_.height(); ++y)
            {
                uint8 *dest = textureBits + y*rect.Pitch;
                for (unsigned int x = 0; x < heightMap_.width(); ++x)
                {
                    *dest++ = 128;
                }
            }
        }
        else
        {
            for (unsigned int y = 0; y < heightMap_.height(); ++y)
            {
                uint8  *dest      = textureBits + y*rect.Pitch;
                uint16 const *src = heightMap_.getRow(y);
                for (unsigned int x = 0; x < heightMap_.width(); ++x)
                {
                    *dest = Math::clamp(0, Math::lerp(*src, minh, maxh, 0, 255), 255);
                    ++dest;
                    ++src;
                }
            }
        }
    }
    else
    {
        uint8 *textureBits = static_cast<uint8 *>(rect.pBits);
        if (minh == maxh)
        {
            for (unsigned int y = 0; y < heightMap_.height(); ++y)
            {
                uint8 *dest = textureBits + y*rect.Pitch;
                for (unsigned int x = 0; x < heightMap_.width(); ++x)
                {
                    uint8 val = 128;
                    *dest++ = val;
                    *dest++ = val;
                    *dest++ = val;
                    ++dest;
                }
            }
        }
        else
        {
            for (unsigned int y = 0; y < heightMap_.height(); ++y)
            {
                uint8  *dest      = textureBits + y*rect.Pitch;
                uint16 const *src = heightMap_.getRow(y);
                for (unsigned int x = 0; x < heightMap_.width(); ++x)
                {
                    uint8 val = Math::clamp(0, Math::lerp(*src, minh, maxh, 0, 255), 255);
                    *dest++ = val;
                    *dest++ = val;
                    *dest++ = val;
                    ++dest;
                    ++src;
                }
            }
        }
    }

    // Cleanup:
    texture_->UnlockRect(0);

    invalidTexture_ = false; // the texture is now valid
}


/**
 *  This function is used to begin modifying textures for the ps2.0 path. 
 *
 *  @param base         A pointer to the first pixel.
 *  @param pitch        The difference between scanlines in uint16s.
 */
void HeightMap::beginTexture(uint16 **base, unsigned int *pitch) const
{
    HRESULT hr;
    if (isPS2_)
    {
        D3DLOCKED_RECT  lockedRect;
        hr = texture_->LockRect(0, &lockedRect, NULL, 0);
        MF_ASSERT(SUCCEEDED(hr));
        *base  = reinterpret_cast<uint16 *>(lockedRect.pBits);
        *pitch = static_cast<unsigned int>(lockedRect.Pitch/sizeof(uint16));
    }
    else
    {
        *base  = heightMap_.getRow(0);
        *pitch = heightMap_.width();
    }
}


/**
 *  This function is used to end modify the texture for the ps2.0 path.
 */
void HeightMap::endTexture() const
{
    HRESULT hr;
    if (isPS2_)
    {
        hr = texture_->UnlockRect(0);
        MF_ASSERT(SUCCEEDED(hr));
    }
}


/**
 *  This returns the width of the texture.
 */
unsigned int HeightMap::textureWidth() const
{
    if (isPS2_)
    {
        D3DSURFACE_DESC desc;
        HRESULT hr = texture_->GetLevelDesc(0, &desc);
        MF_ASSERT(SUCCEEDED(hr));
        return desc.Width;
    }
    else
    {
        return heightMap_.width();
    }
}


/**
 *  This returns the height of the texture.
 */
unsigned int HeightMap::textureHeight() const
{
    if (isPS2_)
    {
        D3DSURFACE_DESC desc;
        HRESULT hr = texture_->GetLevelDesc(0, &desc);
        MF_ASSERT(SUCCEEDED(hr));
        return desc.Height;
    }
    else
    {
        return heightMap_.width();
    }
}


/**
 *  This returns true if we have a texture ready to be used.
 */ 
bool HeightMap::hasTexture() const
{
    return (isPS2_ && texture_ != NULL) || (!isPS2_ && !heightMap_.isEmpty());
}
