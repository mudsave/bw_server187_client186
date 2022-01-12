/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef TERRAIN_UTILS_HPP
#define TERRAIN_UTILS_HPP


#include "import/mem_image.hpp"
#include "moo/base_terrain_block.hpp"


class   Chunk;
class   ChunkTerrain;


namespace TerrainUtils
{
    bool terrainFormat
    (
        std::string             const &space,
        float                   &poleSpacing,
        float                   &blockWidth,
        int                     &numPoles
    );

    void terrainSize
    (
        std::string             const &space,
        int                     &gridMinX,
        int                     &gridMinY,
        int                     &gridMaxX,
        int                     &gridMaxY
    );

    struct TerrainGetInfo
    {
        DataSectionPtr          dataSection;
        Chunk                   *chunk;
        Moo::TerrainBlockPtr    block;
        std::string             chunkName;
        ChunkTerrain            *chunkTerrain;

        TerrainGetInfo();
        ~TerrainGetInfo();
    };

    BinaryPtr getTerrain
    (
        int                     gx,
        int                     gy,
        MemImage<float>         &terrainImage,
        bool                    forceToMemory,
        TerrainGetInfo          *getInfo        = NULL
    );

    void setTerrain
    (
        int                     gx,
        int                     gy,
        MemImage<float>         const &terrainImage,
        TerrainGetInfo          const &getInfo,
        bool                    forceToDisk
    );

    bool isLocked
    (
        int                     gx,
        int                     gy
    );

    float heightAtPos
    (
        float                   gx,
        float                   gy,
        bool                    forceLoad   = false
    );
}

#endif // TERRAIN_UTILS_HPP
