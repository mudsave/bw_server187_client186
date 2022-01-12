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
#include "import/terrain_utils.hpp"
#include "import/elevation_modify.hpp"
#include "project/space_helpers.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_obstacle.hpp"
#include "project/bigbangd_connection.hpp"


/**
 *  This returns information about the format of the terrain in the give 
 *  space.
 *
 *  @param space            The space to look in.  If this is empty then the
 *                          current space is used.
 *  @param poleSpacing      The spacing between poles.
 *  @param blockWidth       The width and height of a terrain block in meters.
 *  @param numPoles         The width and height of a terrain block in terms
 *                          of poles.
 */
bool TerrainUtils::terrainFormat
(
    std::string             const &space_,
    float                   &poleSpacing,
    float                   &blockWidth,
    int                     &numPoles
)
{
    std::string space = space_;
    if (space.empty())
        space = BigBang::instance().getCurrentSpace();
        
	DataSectionPtr dataSection 
        = BWResource::openSection( space + "/00000000o.cdata/terrain" );	
	if (!dataSection)
	{
		return false;
	}
	BinaryPtr binData = dataSection->asBinary();
	if (!binData)
	{
		return false;
	}
	Moo::BaseTerrainBlock::TerrainBlockHeader *header = 
        (Moo::BaseTerrainBlock::TerrainBlockHeader*)binData->data();

	poleSpacing = header->spacing_;
	numPoles    = header->heightMapWidth_;
	blockWidth  = (poleSpacing*(numPoles - 3.0f));

	return true;
}


/**
 *  This function returns the size of the terrain in grid units.
 *
 *  @param space            The space to get.  if this is empty then the 
 *                          current space is used.
 *  @param gridMinX         The minimum grid x unit.
 *  @param gridMinY         The minimum grid y unit.
 *  @param gridMaxX         The maximum grid x unit.
 *  @param gridMaxY         The maximum grid y unit.
 */
void TerrainUtils::terrainSize
(
    std::string             const &space_,
    int                     &gridMinX,
    int                     &gridMinY,
    int                     &gridMaxX,
    int                     &gridMaxY
)
{
    std::string space = space_;
    if (space.empty())
        space = BigBang::instance().getCurrentSpace();

    // work out grid size
    DataSectionPtr pDS = BWResource::openSection(space + "/space.settings"); 
    if (pDS)
    {
        gridMinX = pDS->readInt("bounds/minX",  1);
        gridMinY = pDS->readInt("bounds/minY",  1);
        gridMaxX = pDS->readInt("bounds/maxX", -1);
        gridMaxY = pDS->readInt("bounds/maxY", -1);
    }
}


TerrainUtils::TerrainGetInfo::TerrainGetInfo()
:
chunk(NULL),
chunkTerrain(NULL)
{
}


TerrainUtils::TerrainGetInfo::~TerrainGetInfo()
{
    chunk           = NULL;
    chunkTerrain    = NULL;
}


/**
 *  This gets the raw terrain height information for the given block.
 *
 *  @param gx               The grid x position.
 *  @param gy               The grid y position.
 *  @param terrainImage     The terrain image data.
 *  @param forceToMemory    Force loading the chunk to memory.
 *  @param getInfo          If you later set the terrain data then you need to
 *                          pass an empty one of these here.
 *  @return                 If not in memory then return the data section
 *                          for the chunk's terrain.
 */
BinaryPtr TerrainUtils::getTerrain
(
    int                     gx,
    int                     gy,
    MemImage<float>         &terrainImage,
    bool                    forceToMemory,
    TerrainGetInfo          *getInfo        /*= NULL*/
)
{
    BinaryPtr result;

    // Get the name of the chunk:
    std::string chunkName;
    int16 wgx = (int16)gx;
    int16 wgy = (int16)gy;
    ::chunkID(chunkName, wgx, wgy);
    if (chunkName.empty())
        return NULL;
    if (getInfo != NULL)
        getInfo->chunkName = chunkName;

    // Force the chunk into memory if requested:
    if (forceToMemory)
    {
        ChunkManager::instance().loadChunkNow
        (
            chunkName,
            BigBang::instance().chunkDirMapping()
        );
    }
    
    // Is the chunk in memory?
    Moo::TerrainBlockPtr terrainBlock = NULL;
    Chunk *chunk = 
        ChunkManager::instance().findChunkByName
        (
            chunkName,
            BigBang::instance().chunkDirMapping()
        );
    ChunkTerrain *chunkTerrain = NULL;
    if (chunk != NULL)
    {
        ChunkTerrain *chunkTerrain = 
            ChunkTerrainCache::instance(*chunk).pTerrain();
        if (chunkTerrain != NULL)
        {
            terrainBlock = chunkTerrain->block();
            if (getInfo != NULL)
            {
                getInfo->chunk = chunk;
                getInfo->chunkTerrain = chunkTerrain;
            }
        }
    }

    // If the chunk is in memory, get the terrain data:
    if (terrainBlock != NULL)
    {
        if (getInfo != NULL)
            getInfo->block = terrainBlock;

        // We are only reading height data, hence the const cast:   
        float *terrainData = const_cast<float *>(&terrainBlock->heightMap()[0]);
        terrainImage.resize
        (
            terrainBlock->polesWidth(), 
            terrainBlock->polesHeight(), 
            terrainData,
            false
        );
    }
    // It's not in memory, load it from disk:
    else
    {
        std::string    space       = BigBang::instance().getCurrentSpace();
        std::string    cdataName   = space + "/" + chunkName + ".cdata";
        DataSectionPtr dataSection = BWResource::openSection(cdataName);
        if (getInfo != NULL)
            getInfo->dataSection = dataSection;
        if (dataSection != NULL)
        {
            DataSectionPtr terrainSection = dataSection->openSection("terrain");
            if (terrainSection != NULL)
            {
                result = terrainSection->asBinary();

                // Get the terrain data for this tile:
                int toffset = sizeof(Moo::BaseTerrainBlock::TerrainBlockHeader);
                Moo::BaseTerrainBlock::TerrainBlockHeader* header = 
                    (Moo::BaseTerrainBlock::TerrainBlockHeader*)result->data();
                int textures = header->nTextures_*header->textureNameSize_;
                uint8 *tdata = (uint8 *)result->data();
                float *terrainData = (float*)(tdata + toffset + textures);
                int polesWidth = header->heightMapWidth_;
                int polesHigh  = header->heightMapHeight_;
                size_t terrainSize = polesWidth*polesHigh*sizeof(float);
                terrainImage.resize(polesWidth, polesHigh, terrainData, false);
            }
        }
    }

    return result;
}


/**
 *  This function sets raw terrain data.
 *
 *  @param gx               The grid x position.
 *  @param gy               The grid y position.
 *  @param terrainImage     The terrain image data.  This needs to be the
 *                          data passed back via getTerrain.
 *  @param getInfo          If you later set the terrain data then you need to
 *                          pass an empty one of these here.  This needs to be 
 *                          the data passed back via getTerrain.
 *  @param forceToDisk      Force saving the terrain to disk.
 */
void TerrainUtils::setTerrain
(
    int                     /*gx*/,
    int                     /*gy*/,
    MemImage<float>         const &terrainImage,
    TerrainGetInfo          const &getInfo,
    bool                    forceToDisk
)
{
    if (getInfo.dataSection != NULL)
    {
		getInfo.dataSection->save();
        BigBang::instance().changedTerrainBlockOffline(getInfo.chunkName);
    }
    if (getInfo.chunk != NULL)
    {
        BigBang::instance().changedTerrainBlock(getInfo.chunk);
        BigBang::instance().markTerrainShadowsDirty(getInfo.chunk);
        if (forceToDisk)
        {
            std::string    space       = BigBang::instance().getCurrentSpace();
            std::string    cdataName   = space + "/" + getInfo.chunkName + ".cdata";
            DataSectionPtr dataSection = BWResource::openSection(cdataName);
            if (dataSection != NULL)
            {
                DataSectionPtr terrainSection = dataSection->openSection("terrain");
                if (terrainSection != NULL)
                {
                    BinaryPtr result = terrainSection->asBinary();

                    // Get the terrain data for this tile:
                    int toffset = sizeof(Moo::BaseTerrainBlock::TerrainBlockHeader);
                    Moo::BaseTerrainBlock::TerrainBlockHeader* header = 
                        (Moo::BaseTerrainBlock::TerrainBlockHeader*)result->data();
                    int textures = header->nTextures_*header->textureNameSize_;
                    uint8 *tdata = (uint8 *)result->data();
                    float *terrainData = (float*)(tdata + toffset + textures);
                    int polesWidth = header->heightMapWidth_;
                    int polesHigh  = header->heightMapHeight_;
                    size_t terrainSize = polesWidth*polesHigh*sizeof(float);

                    // Copy the terrain and save:
                    ::memcpy(terrainData, terrainImage.getRow(0), terrainSize);
                    dataSection->save();
                }
            }
        }
        if (getInfo.chunkTerrain != NULL)
            getInfo.chunkTerrain->calculateBB();
    }
    if (getInfo.block != NULL)
    {
        getInfo.block->deleteManagedObjects();
		getInfo.block->createManagedObjects();
    }
}


/**
 *  This function returns true if the terrain chunk at (gx, gy) is locked.
 *
 *  @param gx               The x grid coordinate.
 *  @param gy               The y grid coordinate.
 *  @returns                True if the chunk at (gx, gy) is locked, false
 *                          if it isn't locked.
 */
bool TerrainUtils::isLocked
(
    int                     gx,
    int                     gy
)
{
    // See if we are in a multi-user environment and connected:    
	BWLock::BigBangdConnection* connection = BigBang::instance().connection();
	if( connection )
	{
		if( !connection->isLockedByMe( gx, gy ) )
			return true;
	}

    return false;
}


/**
 *  This function returns the height at the given position.  It includes items
 *  on the ground.
 *
 *  @param gx               The x grid coordinate.
 *  @param gy               The y grid coordinate.
 *  @param forceLoad        Force loading the chunk and getting focus at this
 *                          point to do collisions.  The camera should be moved
 *                          near here.
 *  @returns                The height at the given position.
 */
float TerrainUtils::heightAtPos
(
    float                   gx,
    float                   gy,
    bool                    forceLoad   /*= false*/
)
{
    ChunkManager &chunkManager = ChunkManager::instance();

    if (forceLoad)
    {
        ElevationModify elevationModify(true);

        Vector3 chunkPos = Vector3(gx, 0.0f, gy);
        ChunkManager::instance().cameraSpace()->focus(chunkPos);
        std::string chunkName;
        int16 wgx = (int16)floor(gx/GRID_RESOLUTION);
        int16 wgy = (int16)floor(gy/GRID_RESOLUTION);
        ::chunkID(chunkName, wgx, wgy);
        if (!chunkName.empty())
        {
            chunkManager.loadChunkNow
            (
                chunkName,
                BigBang::instance().chunkDirMapping()
            );
            chunkManager.checkLoadingChunks();
        }
        ChunkManager::instance().cameraSpace()->focus(chunkPos);
    }

    const float MAX_SEARCH_HEIGHT = 1000000.0f;
    Vector3 srcPos(gx, +MAX_SEARCH_HEIGHT, gy);
    Vector3 dstPos(gx, -MAX_SEARCH_HEIGHT, gy);
    
    float dist = 
        chunkManager.cameraSpace()->collide
        (
            srcPos, 
            dstPos, 
            ClosestObstacle::s_default
        );
    float result = 0.0;
    if (dist > 0.0)
        result = MAX_SEARCH_HEIGHT - dist;

    return result;
}
