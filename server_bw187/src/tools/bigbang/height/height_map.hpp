/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef HEIGHT_MAP_HPP
#define HEIGHT_MAP_HPP

#include "import/elevation_image.hpp"
#include "import/elevation_blit.hpp"
#include "moo/device_callback.hpp"
#include "moo/moo_dx.hpp"


class   Chunk;


/**
 *  This class manages a terrain height map texture.
 */
class HeightMap : public Moo::DeviceCallback
{
public:
    static HeightMap &instance();

    void spaceInformation
    (
        std::string         const &spaceName,
        int                 localToWorldX,
        int                 localToWorldY,
        uint16              width,
        uint16              height,
        bool                createAndValidateTexture    = true
    );

    void save();

    void onStop();

    void setTexture(uint8 textureStage);

    void update(float dtime);

    DX::Texture *texture() const;

    float minHeight() const;

    float maxHeight() const;

    void updateMinMax();

    static float normalizeHeight(float height);

    void drawImportedTerrain
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
    );

    void undrawImportedTerrain();

    void clearImpTerrain();

    void invalidateImportedTerrain();
    
    void dirtyThumbnail(Chunk *chunk);

    void dirtyThumbnail(int gridX, int gridY);

    float heightAtGridPos(Vector2 const &pos) const;

    void 
    heightRange
    (
        Vector2             const &pos1, 
        Vector2             const &pos2,
        float               &minv,
        float               &maxv
    ) const;

    void makeValid(bool progress = true);

    void doNotSaveHeightMap();

    bool isPS2() const;

protected:

    void extractSubimage
    (
        float               left,
        float               top,
        float               right,
        float               bottom,
        MemImage<uint16>    &subImage
    ) const;

    void restoreSubimage
    (
        float               left,
        float               top,
        float               right,
        float               bottom,
        MemImage<uint16>    const &subImage
    );

    void addTerrain
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
    );

    HeightMap();

    ~HeightMap();

	void createTexture();

	void deleteTexture();

    void 
    chunkToHeightMap
    (
        int                 gx, 
        int                 gy
    );

    bool drawSomeChunks();

    unsigned int recommendHeightMapSize(unsigned int x) const;

    bool toTexCoords
    (
        float           left,
        float           top,
        float           right,
        float           bottom,
        int             &texLeft,
        int             &texTop,
        int             &texRight,
        int             &texBottom,
        int             *ileft      = NULL,
        int             *itop       = NULL,
        int             *iright     = NULL,
        int             *ibottom    = NULL
    ) const;

    void blitAdditive
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
    );

    void blitSubtractive
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
    );

    void blitMin
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
    );

    void blitMax
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
    );

    void blitReplace
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
    );

    std::string cachedHeightMapName() const;

    bool loadCachedHeightMap();

    void saveCachedHeightMap();

    void redrawAllChunks();

    void ensureValidTexture() const;

    void beginTexture(uint16 **base, unsigned int *pitch) const;

    void endTexture() const;

    unsigned int textureWidth() const;

    unsigned int textureHeight() const;

    bool hasTexture() const;

private:
    typedef std::pair<int, int> GridPos;

    mutable DX::Texture     *texture_;
    unsigned int            gridWidth_;
    unsigned int            gridHeight_;
    std::string             spaceName_;
    float                   minHeightTerrain_;
    float                   maxHeightTerrain_;
    float                   minHeightImpTerrain_;
    float                   maxHeightImpTerrain_;
    float                   minHeight_;
    float                   maxHeight_;
    unsigned int            numPoles_;
    unsigned int            blockWidth_;
    unsigned int            blockHeight_;
    int                     localToWorldX_;
    int                     localToWorldY_;
    std::list<GridPos>      undrawnChunks_;
    MemImage<uint16>        heightMap_;
    MemImage<uint16>        impTerrSubImg_;
    float                   impTerrLeft_;
    float                   impTerrTop_;
    float                   impTerrRight_;
    float                   impTerrBottom_;
    bool                    initialized_;
    mutable bool            invalidTexture_;
    mutable bool            isEightBitTex_;
    bool                    isPS2_;
    unsigned int            recommendSize_;
};

#endif // HEIGHT_MAP_HPP
