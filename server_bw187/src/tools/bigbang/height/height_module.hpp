/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef HEIGHT_MODULE_HPP
#define HEIGHT_MODULE_HPP

#include "appmgr/module.hpp"
#include "project/grid_coord.hpp"
#include "import/elevation_image.hpp"
#include "import/elevation_blit.hpp"

class ClosedCaptions;
class Moo::EffectMaterial;


/**
 *  This module draws a height map of the terrain in the space.
 *
 *  The user can select a rectangle to import/export etc.
 */
class HeightModule : public FrameworkModule
{
public:
    enum Mode
    {
        IMPORT_TERRAIN,         // user is importing terrain
        EXPORT_TERRAIN          // user is exporting terrain
    };

    HeightModule();
    ~HeightModule();

    virtual bool init( DataSectionPtr pSection );

    virtual void onStart();
    virtual int  onStop();

    virtual void onPause();
    virtual void onResume( int exitCode );

    virtual bool updateState( float dTime );
    virtual void render( float dTime );

    virtual bool handleKeyEvent( const KeyEvent & keyEvent );  

    void gotoWorldPos    ( const KeyEvent &keyEvent );
    void startTerrainEdit( const KeyEvent &keyEvent );
    void startGetHeight  ( const KeyEvent &keyEvent );

    virtual bool handleMouseEvent( const MouseEvent & mouseEvent );

    void handleZoom       ( const MouseEvent & mouseEvent );
    void handlePan        ( const MouseEvent & mouseEvent );
    void handleTerrainEdit( const MouseEvent & mouseEvent );
    void handleGetHeight  ( const MouseEvent & mouseEvent );

    /** Set blend value for space map **/
    void projectMapAlpha( float a );
    float projectMapAlpha();

    /** Get/set the current mode **/
    Mode getMode() const;

    /** Set the import elevation image, resets the mode to export if null **/
    void setElevationData
    (
        SmartPointer<ElevationImage>    image,
        float                           *left   = NULL,
        float                           *top    = NULL,
        float                           *right  = NULL,
        float                           *bottom = NULL
    );

    /** Rotate the imported elevation data **/
    void rotateElevationData(bool clockwise);

    enum FlipDir
    {
        FLIP_X,
        FLIP_Y,
        FLIP_HEIGHT
    };

    /** Flip the elevation data about the given axis */
    void flipElevationData(FlipDir dir);

    /** Set the import options */
    void 
    setImportOptions
    (
        ElevationBlit::Mode mode,
        float               minV, 
        float               maxV,
        float               strength
    );

    /** The size of the undo if import was done. */
    size_t undoSize() const;

    /** Do an import of the data */
    bool doImport(bool doUndo, bool showProgress, bool forceToMem);

    /** Do an export of the data */
    bool doExport(char const *filename, bool showProgress);

    /** Update the min/max range. */
    void updateMinMax();

    /** Ensure that the cached height map is calculated. */
    static void ensureHeightMapCalculated();

    /** Flag that the height map is completely invalid and should not be saved. */
    static void doNotSaveHeightMap();

    /** Get the current instance on the module stack, if any */
    static HeightModule* currentInstance();

private:
    HeightModule( const HeightModule& );
    HeightModule& operator=( const HeightModule& );

protected:
    void onQuickStart();

    void writeStatus();

    /** Convert from screen to world coordinates */
    Vector2 gridPos(POINT pt) const;

    /** Where in the grid the mouse is currently pointing */
    Vector2 currentGridPos() const;

    /** Get a world position from a grid position */
    Vector3 gridPosToWorldPos(Vector2 const &gridPos) const;

    /** Get a grid position from a world position */
    Vector2 worldPosToGridPos(Vector3 const &pos) const;

    enum TerrainDir
    {
        TERRAIN_MISS,
        TERRAIN_MIDDLE,
        TERRAIN_NORTH,
        TERRAIN_NORTH_EAST,
        TERRAIN_EAST,
        TERRAIN_SOUTH_EAST,
        TERRAIN_SOUTH,
        TERRAIN_SOUTH_WEST,
        TERRAIN_WEST,
        TERRAIN_NORTH_WEST
    };

    /** Get a handle direction from the screen point pt. */
    TerrainDir pointToDir(POINT pt) const;

    /** Make sure that left < right, bottom < top. */
    void normalizeTerrainRect();

    /** Snap based on whether the CTRL key is down. */
    float snapTerrainCoord(float v) const;

    /** Draw an unfilled rectangle at the given position in the given thickness. */
    void drawRect( const Vector2 &ul, const Vector2 &lr, uint32 colour, int ht);

    /** Draw an filled rectangle at the given position. */
    void drawRect( const Vector2 &ul, const Vector2 &lr, uint32 colour);

    /** Draw a handle at the world pos. v of size (2*hx + 1), (2*hy + 1) pixels. */
    void drawHandle( const Vector2 & v, uint32 colour, int hx, int hy ) const;

    /** Draw a vertical line. */
    void drawVLine( float x, uint32 colour, float hx ) const;

    /** Draw a horizontal line. */
    void drawHLine( float y, uint32 colour, float hy ) const;

    /** The imported terrain options/positions etc have changed, redraw it. */
    void redrawImportedTerrain();

    /** Perform a height query. */
    void heightQuery();

    /** Update the cursor. */
    void updateCursor();

private:
    float                               spaceAlpha_;
    Moo::BaseTexturePtr                 handDrawnMap_;
    Vector3                             viewPosition_;          /** Camera position, X & Y specify position, Z specifies zoom */
    float                               minViewZ_;
    float                               savedFarPlane_;         /** Saved far-plane, allows restoration when returning to main editing */   
	int									minX_;
	int									minY_;
    unsigned int                        gridWidth_;             /** Extent of the grid, in number of chunks. It starts at 0,0 */
    unsigned int                        gridHeight_;
    GridCoord                           localToWorld_;          /** Translation from local to world grid coordinates */
    SmartPointer<ClosedCaptions>        cc_;
    bool                                mouseDownOn3DWindow_;   /** True if the mouse button is down */
    Mode                                mode_;                  /** Current mode */
    SmartPointer<ElevationImage>        elevationData_;         /** Imported terrain */ 
    MemImage<uint16>                    subTexture_;            /** Sub texture of height map when importing and editing */
    Vector2                             terrainTopLeft_;        /** Top-left of the imported terrain */
    Vector2                             terrainBottomRight_;    /** Bottom-right of the imported terrain */
    TerrainDir                          terrainDir_;            /** Direction editing terrain */
    Vector2                             terrainDownPt_;         /** Point mouse went down */
    Vector2                             terrainTLDown_;
    Vector2                             terrainBRDown_;
    float                               scaleX_;                /** Horizontal size of a pixel in world units */
    float                               scaleY_;                /** Vertical size of a pixel in world units */
    Moo::EffectMaterial                 *terrainShader_;        /** Shader to draw terrain and blend with hand drawn map */
    Moo::EffectMaterial                 *selectionShader_;      /** Shader to draw selection rectangle */
    ElevationBlit::Mode                 impMode_;               /** How to display temp. bitmap. */
    float                               impMinV_;               /** Minimum height on import */
    float                               impMaxV_;               /** Maximum height on import */
    float                               impStrength_;           /** Import strength */
    Vector2                             heightDownPt_;          /** Getting height down point. */
    Vector2                             heightCurrentPt_;       /** Getting height current point. */
    bool                                gettingHeight_;         /** True if getting height. */
    CPoint                              lastCursorPos_;         /** Last cursor pos - for panning. */
    static HeightModule                 *currentInstance_;      /** The last created HeightModule */
};

#endif // HEIGHT_MODULE_HPP
