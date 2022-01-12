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
#include "import/elevation_blit.hpp"
#include "import/elevation_image.hpp"
#include "import/elevation_modify.hpp"
#include "import/elevation_undo.hpp"
#include "import/terrain_utils.hpp"
#include "project/grid_coord.hpp"
#include "project/chunk_walker.hpp"
#include "project/space_helpers.hpp"
#include "big_bang.hpp"
#include "math/mathdef.hpp"
#include <limits>


using namespace Math;


namespace
{
    //
    // Test whether two rectangles intersect, and if they do return the
    // common area.  The code assumes that left < right and bottom < top.
    //
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

    //
    // Get space settings for the current space.
    //
    bool spaceSettings
    (
        std::string const &space,
        uint16      &gridWidth,
        uint16      &gridHeight,
        GridCoord   &localToWorld
    )
    {	    
	    DataSectionPtr pDS   = BWResource::openSection(space + "/space.settings");
	    if (pDS)
	    {
		    int minX = pDS->readInt("bounds/minX",  1);
		    int minY = pDS->readInt("bounds/minY",  1);
		    int maxX = pDS->readInt("bounds/maxX", -1);
		    int maxY = pDS->readInt("bounds/maxY", -1);

		    gridWidth  = maxX - minX + 1;
		    gridHeight = maxY - minY + 1;
		    localToWorld = GridCoord(minX, minY);

            return true;
	    }
	    else
	    {
		    return false;
	    }
    }

    //
    // The size of the terrain as if it were one giant bitmap.
    //
    void
    heightMapExtents
    (
        unsigned int    gridWidth,
        unsigned int    gridHeight,
        unsigned int    numPoles,
        unsigned int    &width,
        unsigned int    &height
    )
    {
        // -3*(gridWidth - 1) is overlap of adjacent tiles, 
        // -2 for edges not used.
        width  = gridWidth *numPoles - 3*(gridWidth  - 1) - 2;
        height = gridHeight*numPoles - 3*(gridHeight - 1) - 2;
    }

    //
    // The position of the gridX'th, gridY'th terrain tile as if the terrain
    // were one giant bitmap.  Note that edge tiles can have coordinates that
    // outside the giant bitmap become some poles are set to zero.
    //
    void 
    tilePosition
    (
        unsigned int    numPoles,
        unsigned int    gridX,
        unsigned int    gridY,
        int             &left,
        int             &top,
        int             &right,
        int             &bottom
    )
    {
        left   = gridX*(numPoles - 3);
        bottom = gridY*(numPoles - 3);
        right  = left   + numPoles - 1;
        top    = bottom + numPoles - 1;
    }

    //
    // Like Math::lerp but handles degerate cases.
    //
    template<typename SRC, typename DST>
    inline DST 
    safeLerp
    (
        SRC                 const &x, 
        SRC                 const &src_a, 
        SRC                 const &src_b, 
        DST                 const &dst_a,
        DST                 const &dst_b    
    )
    {
        if (src_a == src_b)
            return dst_a;
        else
            return (DST)((dst_b - dst_a)*(x - src_a)/(src_b - src_a)) + dst_a;
    }
}


/**
 *  This function computes the memory (in bytes) required for a terrain import
 *  over the given grid coordinates.  The coordinates do not need be clipped
 *  or normalized (left < right etc).
 *
 *  @param leftf        The left coordinate.
 *  @param topf         The top coordinate.
 *  @param rightf       The right coordinate.
 *  @param bottomf      The bottom coordinate.
 *  @returns            The size of the undo buffer if an import is done.
 */
size_t ElevationBlit::importUndoSize
(
    float               leftf,
    float               topf,
    float               rightf,
    float               bottomf
)
{
    size_t numChunks = 0;
    int    nPoles    = 0;

    { // force scope of modifyTerrain
        ElevationModify modifyTerrain(true);

        if (!modifyTerrain)
            return 0;

        std::string space = BigBang::instance().getCurrentSpace();

	    uint16 gridWidth, gridHeight;
	    GridCoord localToWorld(0, 0);
        if (!spaceSettings(space, gridWidth, gridHeight, localToWorld))
        {
            return 0;
        }

	    // Retrieve destination terrain format, by opening an existing
	    // terrain file and reading its header:
	    float poleSpacing;
	    float blockWidth;
        if 
        (
            !TerrainUtils::terrainFormat
            (
                space, 
                poleSpacing, 
                blockWidth, 
                nPoles
            )
        )
	    {
		    return 0;
	    }

	    LinearChunkWalker lcw;
	    lcw.spaceInformation(space, localToWorld, gridWidth, gridHeight);

	    std::string chunkName;
	    int16 gridX, gridZ;

        // Pretend the terrain is one giant bitmap of size 
        // widthPixels x heightPixels:
        unsigned int widthPixels, heightPixels;
        heightMapExtents
        (
            gridWidth, 
            gridHeight, 
            nPoles, 
            widthPixels, 
            heightPixels
        );

        // Convert the selection into the giant bitmap coordinates:
        int left   = (int)(leftf  *widthPixels /gridWidth );
        int top    = (int)(topf   *heightPixels/gridHeight);
        int right  = (int)(rightf *widthPixels /gridWidth );
        int bottom = (int)(bottomf*heightPixels/gridHeight);

        // Handle some degenerate cases:
        if (left == right || top == bottom)
            return false;

        // Normalize the coordinates:
        if (left > right ) std::swap(left, right );
        if (top  < bottom) std::swap(top , bottom);
               
	    while (lcw.nextTile(chunkName, gridX, gridZ))
	    {
            if (!TerrainUtils::isLocked(gridX, gridZ))
            {
                // The coordinates of this chunk in the giant bitmap system:
                int tileLeft, tileRight, tileBottom, tileTop;
                tilePosition
                (
                    nPoles, 
                    gridX - localToWorld.x, 
                    gridZ - localToWorld.y, 
                    tileLeft, 
                    tileTop, 
                    tileRight, 
                    tileBottom
                ); 

                // Only process chunks that intersect the selection:
                int ileft, itop, iright, ibottom;
                if 
                (
                    rectanglesIntersect
                    (
                        tileLeft, tileTop, tileRight, tileBottom,
                        left    , top    , right    , bottom    ,
                        ileft   , itop   , iright   , ibottom   
                    )
                )
                {
                    ++numChunks;
                }
            }
        }
    } 

    return numChunks*sizeof(float)*nPoles*nPoles;
}


/**
 *  This function sets up an undo/redo operation for terrain import.  The 
 *  coordinates do not need be clipped or normalized (left < right etc).
 *
 *  @param leftf        The left coordinate.
 *  @param topf         The top coordinate.
 *  @param rightf       The right coordinate.
 *  @param bottomf      The bottom coordinate.
 *  @param description  The description of the undo/redo operation.
 *  @param showProgress Show a progress bar while generating undo/redo data.
 */
bool ElevationBlit::saveUndoForImport
(
    float           leftf,
    float           topf,
    float           rightf,
    float           bottomf,
    std::string     const &description,
    bool            showProgress
)
{
    std::list<ElevationUndoPos> modifiedChunksPos; // list of intersecting chunks

    std::string space = BigBang::instance().getCurrentSpace();

	uint16 gridWidth, gridHeight;
	GridCoord localToWorld(0, 0);
    if (!spaceSettings(space, gridWidth, gridHeight, localToWorld))
    {
        return false;
    }

	// Retrieve destination terrain format, by opening an existing
	// terrain file and reading its header:
	float poleSpacing;
	float blockWidth;
	int   nPoles;
    if 
    (
        !TerrainUtils::terrainFormat
        (
            std::string(), 
            poleSpacing, 
            blockWidth, 
            nPoles
        )
    )
	{
		return false;
	}

	BigBangProgressBar  *progress   = NULL;
	ProgressTask        *importTask = NULL;
    int                 progressCnt = 0;
    if (showProgress)
    {
        progress    = new BigBangProgressBar;
        importTask  = 
            new ProgressTask
            ( 
                progress, 
                "Saving Terrain Data for undo/redo", 
                (float)(gridWidth*gridHeight/100)
            );
    }

	LinearChunkWalker lcw;
	lcw.spaceInformation(space, localToWorld, gridWidth, gridHeight);

	std::string chunkName;
	int16 gridX, gridZ;

    // Pretend the terrain is one giant bitmap of size 
    // widthPixels x heightPixels:
    unsigned int widthPixels, heightPixels;
    heightMapExtents
    (
        gridWidth, 
        gridHeight, 
        nPoles, 
        widthPixels, 
        heightPixels
    );

    // Convert the selection into the giant bitmap coordinates:
    int left   = (int)(leftf  *(widthPixels  - 1)/gridWidth );
    int top    = (int)(topf   *(heightPixels - 1)/gridHeight);
    int right  = (int)(rightf *(widthPixels  - 1)/gridWidth );
    int bottom = (int)(bottomf*(heightPixels - 1)/gridHeight);

    // Handle some degenerate cases:
    if (left == right || top == bottom)
        return false;

    // Normalize the coordinates:
    if (left > right ) std::swap(left, right );
    if (top  < bottom) std::swap(top , bottom);
    
	while (lcw.nextTile(chunkName, gridX, gridZ))
	{
		if (importTask != NULL && progressCnt++%100 == 0)
			importTask->step();

        // The coordinates of this chunk in the giant bitmap system:
        int tileLeft, tileRight, tileBottom, tileTop;
        tilePosition
        (
            nPoles, 
            gridX - localToWorld.x, 
            gridZ - localToWorld.y, 
            tileLeft, 
            tileTop, 
            tileRight, 
            tileBottom
        );  

        // Only process chunks that intersect the selection:
        int ileft, itop, iright, ibottom;
        if 
        (
            rectanglesIntersect
            (
                tileLeft, tileTop, tileRight, tileBottom,
                left    , top    , right    , bottom    ,
                ileft   , itop   , iright   , ibottom   
            )
        )
        {
            modifiedChunksPos.push_back(ElevationUndoPos(gridX, gridZ));
        }
    }

    if (modifiedChunksPos.empty())
        return false;

    UndoRedo::instance().add(new ElevationUndo(modifiedChunksPos));
    UndoRedo::instance().barrier(description, false);

    delete progress   ; progress  = NULL;
    delete importTask; importTask = NULL;

    return true;
}


/**
 *  This function does a terrain import.  The coordinates do not need be 
 *  clipped or normalized (left < right etc).
 *
 *  @param image        The image to import.
 *  @param leftf        The left coordinate of the import area.
 *  @param topf         The top coordinate of the import area.
 *  @param rightf       The right coordinate of the import area.
 *  @param bottomf      The bottom coordinate of the import area. 
 *  @param mode         The import mode (replace, additive etc).
 *  @param min          The minimum height of the import area.
 *  @param max          The maximum height of the import area.
 *  @param alpha        The strength (0..1) of the import.  0.0 leaves the
 *                      area intact, 1.0 is a full import.
 *  @param showProgress If true then show progress while importing.
 */
bool ElevationBlit::import
(
    ElevationImage      const &image,
    float               leftf,
    float               topf,
    float               rightf,
    float               bottomf,
    Mode                mode,
    float               minv,
    float               maxv,
    float               alpha,
    bool                showProgress,
    bool                forceToMemory
)
{
    // Note that the code below uses double precision arithmetic.  This is
    // because floating point sometimes gives results that are innacurate
    // enough to be visible as very tiny cracks between chunks.

    if (minv > maxv) 
        std::swap(minv, maxv);

    double offset = 0.0;
    double scale  = 0.0;
    switch (mode)
    {
    case ElevationBlit::ADDITIVE:      // fall through deliberate
    case ElevationBlit::SUBTRACTIVE:   
        offset = 0.0;
        scale  = alpha*(maxv - minv);
        break;
    case ElevationBlit::OVERLAY:
        offset = -0.5*alpha*(maxv - minv);
        scale  = alpha*(maxv - minv);
        break;
    case ElevationBlit::MAX:           // fall through deliberate
    case ElevationBlit::MIN:           // fall through deliberate
    case ElevationBlit::REPLACE:
        offset = minv;
        scale  = alpha*(maxv - minv);
        break;
    }

    ElevationModify modifyTerrain;
    if (!modifyTerrain)
        return false;

    std::string space = BigBang::instance().getCurrentSpace();
	uint16 gridWidth, gridHeight;
	GridCoord localToWorld(0, 0);
    if (!spaceSettings(space, gridWidth, gridHeight, localToWorld))
        return false;

	// Retrieve destination terrain format, by opening an existing
	// terrain file and reading its header:
	float poleSpacing;
	float blockWidth;
	int   nPoles;
    if 
    (
        !TerrainUtils::terrainFormat
        (
            space, 
            poleSpacing, 
            blockWidth, 
            nPoles
        )
    )
	{
		return false;
	}

	LinearChunkWalker lcw;
	lcw.spaceInformation(space, localToWorld, gridWidth, gridHeight);

	std::string chunkName;
	int16 gridX, gridZ;

    // Pretend the terrain is one giant bitmap of size 
    // widthPixels x heightPixels:
    unsigned int widthPixels, heightPixels;
    heightMapExtents
    (
        gridWidth, 
        gridHeight, 
        nPoles, 
        widthPixels, 
        heightPixels
    );

    // Convert the selection into the giant bitmap coordinates:
    int left   = (int)(leftf  *(widthPixels  - 1)/gridWidth );
    int top    = (int)(topf   *(heightPixels - 1)/gridHeight);
    int right  = (int)(rightf *(widthPixels  - 1)/gridWidth );
    int bottom = (int)(bottomf*(heightPixels - 1)/gridHeight);

    // Handle some degenerate cases:
    if (left == right || top == bottom)
        return false;

    // Normalize the coordinates:
    if (left > right ) std::swap(left, right );
    if (top  < bottom) std::swap(top , bottom);
    
	BigBangProgressBar  *progress   = NULL;
	ProgressTask        *importTask = NULL;
    int                 progressCnt = 0;
    if (showProgress)
    {
        progress    = new BigBangProgressBar;
        importTask  = 
            new ProgressTask
            ( 
                progress, 
                "Importing Terrain Data", 
                float(gridWidth*gridHeight/100) 
            );
    }

	while (lcw.nextTile(chunkName, gridX, gridZ))
	{
		if (importTask != NULL && progressCnt++%100 == 0)
			importTask->step();

        // The coordinates of this chunk in the giant bitmap system:
        int tileLeft, tileRight, tileBottom, tileTop;
        tilePosition
        (
            nPoles, 
            gridX - localToWorld.x, 
            gridZ - localToWorld.y, 
            tileLeft, 
            tileTop, 
            tileRight, 
            tileBottom
        );   

        // Only process chunks that intersect the selection:
        int ileft, itop, iright, ibottom;
        if 
        (
            rectanglesIntersect
            (
                tileLeft, tileTop, tileRight, tileBottom,
                left    , top    , right    , bottom    ,
                ileft   , itop   , iright   , ibottom   
            )
        )
        {
            if (!TerrainUtils::isLocked(gridX, gridZ))
            {
                MemImage<float> terrainImage;
                TerrainUtils::TerrainGetInfo getInfo;
                BinaryPtr binary =
                    TerrainUtils::getTerrain
                    (
                        gridX, gridZ,
                        terrainImage,
                        forceToMemory,
                        &getInfo
                    );

			    if (!terrainImage.isEmpty())
                {
                    // Destination coordinates within this chunk:
                    int dleft   = lerp(ileft  , tileLeft  , tileRight, 0, nPoles - 1);
                    int dtop    = lerp(itop   , tileBottom, tileTop  , 0, nPoles - 1);
                    int dright  = lerp(iright , tileLeft  , tileRight, 0, nPoles - 1);
                    int dbottom = lerp(ibottom, tileBottom, tileTop  , 0, nPoles - 1);                 

                    // Source coordinates within the imported image:
                    double sleft   = lerp(ileft  , left, right , 0.0, (float)image.width () - 1.0);
                    double stop    = lerp(itop   , top , bottom, 0.0, (float)image.height() - 1.0);
                    double sright  = lerp(iright , left, right , 0.0, (float)image.width () - 1.0);
                    double sbottom = lerp(ibottom, top , bottom, 0.0, (float)image.height() - 1.0);

                    // Blit the terrain onto this chunk using bicubic 
                    // interpolation.  dx, dy iterate over the chunk,
                    // sx, sy are interpolated coordinates within the imported
                    // image.
                    for (int dy = dbottom; dy <= dtop; ++dy)
                    {
                        float *dst = terrainImage.getRow(dy) + dleft;
                        double sy   = safeLerp((double)dy, (double)dbottom, (double)dtop, (double)sbottom, (double)stop);
                        for (int dx = dleft; dx <= dright; ++dx, ++dst)
                        {
                            double sx    = safeLerp((double)dx, (double)dleft, (double)dright, (double)sleft, (double)sright);
                            double value = image.getValue(sx, sy)*scale + offset;
                            switch (mode)
                            {
                            case ElevationBlit::OVERLAY: // fall thru deliberate
                            case ElevationBlit::ADDITIVE:
                                *dst += (float)value;
                                break;
                            case ElevationBlit::SUBTRACTIVE:
                                *dst -= (float)value;
                                break;                            
                            case ElevationBlit::MIN:
                                *dst = std::min(*dst, (float)value);
                                break;
                            case ElevationBlit::MAX:
                                *dst = std::max(*dst, (float)value);
                                break;
                            case ElevationBlit::REPLACE:
                                *dst = (float)value;
                                break;
                            }                                
                        }
                    }
                    TerrainUtils::setTerrain
                    (
                        gridX, 
                        gridZ, 
                        terrainImage, 
                        getInfo,
                        !forceToMemory
                    );
                }
            }
        }
	}

    delete progress   ; progress  = NULL;
    delete importTask; importTask = NULL;

    return true;
}


/**
 *  This function does a terrain export.  The coordinates do not need be 
 *  clipped or normalized (left < right etc).
 *
 *  @param image        The exported area.  The image is resized as 
 *                      appropriate.
 *  @param leftf        The left coordinate of the import area.
 *  @param topf         The top coordinate of the import area.
 *  @param rightf       The right coordinate of the import area.
 *  @param bottomf      The bottom coordinate of the import area. 
 *  @param showProgres  Show a progress indicator.
 */
bool ElevationBlit::exportTo
(
    ElevationImage      &image,
    float               leftf,
    float               topf,
    float               rightf,
    float               bottomf,
    bool                showProgress
)
{
    ElevationModify modifyTerrain(true); // read only access

	// Get size and format of terrain
    std::string space = BigBang::instance().getCurrentSpace();
	uint16 gridWidth, gridHeight;	
	GridCoord localToWorld(0, 0);
    if (!spaceSettings(space, gridWidth, gridHeight, localToWorld))
        return false;

	//retrieve terrain format, by opening an existing
	//terrain file and reading its header.
	float poleSpacing;
	float blockWidth;
	int   nPoles;
	if 
    (
        !TerrainUtils::terrainFormat
        (
            space, 
            poleSpacing, 
            blockWidth, 
            nPoles 
        )
    )
	{
		return false;
	}

    // Pretend the terrain is one giant bitmap of size 
    // widthPixels x heightPixels:
    unsigned int widthPixels, heightPixels;
    heightMapExtents
    (
        gridWidth, 
        gridHeight, 
        nPoles, 
        widthPixels, 
        heightPixels
    );

    // Convert the selection into the giant bitmap coordinates:
    int left   = (int)(leftf  *(widthPixels  - 1)/gridWidth );
    int top    = (int)(topf   *(heightPixels - 1)/gridHeight);
    int right  = (int)(rightf *(widthPixels  - 1)/gridWidth );
    int bottom = (int)(bottomf*(heightPixels - 1)/gridHeight);

    // Normalize the coordinates:
    if (left > right ) std::swap(left, right );
    if (top  < bottom) std::swap(top , bottom);

    // Clip the selected area against the giant bitmap coords:
    if 
    (
        !rectanglesIntersect
        (
            0   , heightPixels, widthPixels, 0,
            left, top         , right      , bottom,
            left, top         , right      , bottom
        )
    )
    {
        return false; // outside of range!
    }

    // Handle some degenerate cases:
    if (left == right || top == bottom)
        return false;

	// create storage for it
    int dwidth  = right - left   + 1;
    int dheight = top   - bottom + 1;
    image.resize(dwidth, dheight, 0.0f);

	LinearChunkWalker lcw;
	lcw.spaceInformation(space, localToWorld, gridWidth, gridHeight);

	std::string chunkName;
	int16 gridX, gridZ;

	BigBangProgressBar  *progress   = NULL;
	ProgressTask        *importTask = NULL;
    int                 progressCnt = 0;
    if (showProgress)
    {
        progress    = new BigBangProgressBar;
        importTask  = 
            new ProgressTask
            ( 
                progress, 
                "Exporting Terrain Data", 
                float(gridWidth*gridHeight/100) 
            );
    }

	while (lcw.nextTile(chunkName, gridX, gridZ))
	{
		if (importTask != NULL && progressCnt++%100 == 0)
			importTask->step();

        // The coordinates of this chunk in the giant bitmap system:
        int tileLeft, tileRight, tileBottom, tileTop;
        tilePosition
        (
            nPoles, 
            gridX - localToWorld.x, 
            gridZ - localToWorld.y, 
            tileLeft, 
            tileTop, 
            tileRight, 
            tileBottom
        );     

        // Only process chunks that intersect the selection:
        int ileft, itop, iright, ibottom;
        if 
        (
            rectanglesIntersect
            (
                tileLeft, tileTop, tileRight, tileBottom,
                left    , top    , right    , bottom    ,
                ileft   , itop   , iright   , ibottom   
            )
        )
        {
            if (!TerrainUtils::isLocked(gridX, gridZ))
            {
                MemImage<float> terrainImage;
                BinaryPtr binary =
                    TerrainUtils::getTerrain
                    (
                        gridX, gridZ,
                        terrainImage,
                        false
                    );                

			    if (!terrainImage.isEmpty())
                {
                    // Source coordinates within this chunk:
                    int sleft   = lerp(ileft  , tileLeft  , tileRight, 0, nPoles - 1);
                    int stop    = lerp(itop   , tileBottom, tileTop  , 0, nPoles - 1);
                    int sright  = lerp(iright , tileLeft  , tileRight, 0, nPoles - 1);
                    int sbottom = lerp(ibottom, tileBottom, tileTop  , 0, nPoles - 1);                 

                    // Destination coordinates within the exported image:
                    int dleft   = lerp(ileft  , left  , right , 0, dwidth  - 1);
                    int dtop    = lerp(itop   , bottom, top   , 0, dheight - 1);
                    int dright  = lerp(iright , left  , right , 0, dwidth  - 1);
                    int dbottom = lerp(ibottom, bottom, top   , 0, dheight - 1);

                    int dy = dbottom;
                    int sy = sbottom;
                    for (; sy <= stop; ++sy, ++dy)
                    {
                        float const *src = terrainImage.getRow(sy) + sleft;
                        float *dst       = image.getRow(dy) + dleft;
                        int   sx         = sleft;
                        int   dx         = dleft;
                        for (; sx <= sright; ++sx, ++dx, ++src, ++dst)
                        {
                            float srcv = *src;
                            *dst = srcv;
                        }
                    }
		        }
            }
	    }
    }

    delete progress   ; progress  = NULL;
    delete importTask; importTask = NULL;

    return true;
}
