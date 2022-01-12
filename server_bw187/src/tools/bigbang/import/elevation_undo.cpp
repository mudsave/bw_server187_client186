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
#include "import/elevation_undo.hpp"
#include "import/elevation_modify.hpp"
#include "import/elevation_blit.hpp"
#include "import/terrain_utils.hpp"
#include "project/space_helpers.hpp"


/**
 *  This function creates an empty ElevationUndoPos.
 *
 *  @param x        The x coordinate.
 *  @param y        The y coordinate.
 */
ElevationUndoPos::ElevationUndoPos
(
    int16   x, 
    int16   y
) 
:
x_(x), 
y_(y), 
data_(NULL) 
{
}


/**
 *  This is the copy ctor for ElevationUndoPos.  Note that it is shallow and
 *  only the position is copied, not the raw data.
 *
 *  @param other    The ElevationUndoPos to copy from.
 */
ElevationUndoPos::ElevationUndoPos(ElevationUndoPos const &other)
:
x_(other.x_), 
y_(other.y_), 
data_(NULL) 
{
}


/**
 *  This is the ElevationUndoPos dtor.
 */
ElevationUndoPos::~ElevationUndoPos() 
{ 
    delete[] data_; 
}


/**
 *  ElevationUndoPos assignment.
 *
 *  @param other    The ElevationUndoPos to copy from.
 *  @returns        *this.
 */
ElevationUndoPos &ElevationUndoPos::operator=(ElevationUndoPos const &other)
{
    if (this != &other)
    {
        delete[] data_; data_ = NULL;
        x_ = other.x_;
        y_ = other.y_;
    }
    return *this;
}


/**
 *  This is the elevation undo structure.
 *
 *  @param positions    The positions of the affected chunks.
 *                      The positions data should be NULL.
 */
/*explicit*/ 
ElevationUndo::ElevationUndo
(
    ElevationUndoPosList const &positions
)
:
UndoRedo::Operation((int)(typeid(ElevationUndo).name())),
positions_(positions)
{
    ElevationModify elevationModify(true);

	float poleSpacing;
	float blockWidth;
	int   nPoles;
    TerrainUtils::terrainFormat
    (
        std::string(), 
        poleSpacing, 
        blockWidth, 
        nPoles
    );

    for
    (
        ElevationUndoPosList::iterator it = positions_.begin();
        it != positions_.end();
        ++it
    )
    {
        ElevationUndoPos &pos = *it;
        if (!TerrainUtils::isLocked(pos.x_, pos.y_))
        {
            MemImage<float> terrainImage;
            BinaryPtr data = 
                TerrainUtils::getTerrain(pos.x_, pos.y_, terrainImage, false);

            // Copy into pos:
            if (!terrainImage.isEmpty())
            {
                size_t terrainSize = terrainImage.width()*terrainImage.height();
                pos.data_ = new float[terrainSize];
                ::memcpy(pos.data_, terrainImage.getRow(0), terrainSize*sizeof(float));
            }
        }
    }
}


/**
 *  This is the elevation import undo operation.  It restores the height
 *  data.
 */
/*virtual*/ void ElevationUndo::undo()
{
    // Save the current state to the undo/redo stack:
    UndoRedo::instance().add(new ElevationUndo(positions_));

    ElevationModify elevationModify(false);

    std::string space = BigBang::instance().getCurrentSpace();
	float       poleSpacing;
	float       blockWidth;
	int         nPoles;
    TerrainUtils::terrainFormat(space, poleSpacing, blockWidth, nPoles);

    for
    (
        ElevationUndoPosList::iterator it = positions_.begin();
        it != positions_.end();
        ++it
    )
    {
        ElevationUndoPos const &pos = *it;
        if (!TerrainUtils::isLocked(pos.x_, pos.y_))
        {
            MemImage<float> terrainImage;
            TerrainUtils::TerrainGetInfo getInfo;
            BinaryPtr data = 
                TerrainUtils::getTerrain
                (
                    pos.x_, pos.y_, 
                    terrainImage, 
                    false,
                    &getInfo
                );

            if (!terrainImage.isEmpty() && pos.data_ != NULL)
            {
                // Copy the data in pos into the terrain:
                size_t terrainSize = 
                    terrainImage.width()*terrainImage.height();
                ::memcpy(terrainImage.getRow(0), pos.data_, terrainSize*sizeof(float));

                // Save the data:
                TerrainUtils::setTerrain
                (
                    pos.x_, pos.y_, 
                    terrainImage,
                    getInfo,
                    false
                );
            }
        }
    }
}


/**
 *  This compares this elevation undo operation with another operation.
 *
 *  @returns        false.  They are never the same.
 */
/*virtual*/ bool ElevationUndo::iseq(UndoRedo::Operation const &other) const
{
    return false;
}
