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
#include "height/height_module.hpp"
#include "height/height_map.hpp"
#include "import/terrain_utils.hpp"
#include "page_terrain_import.hpp"
#include "big_bang.hpp"
#include "big_bang_camera.hpp"
#include "bigbang.h"
#include "mainframe.hpp"
#include "dlg_modeless_info.hpp"
#include "panel_manager.hpp"
#include "chunks/editor_chunk.hpp"
#include "import/elevation_blit.hpp"
#include "appmgr/app.hpp"
#include "appmgr/module_manager.hpp"
#include "appmgr/options.hpp"
#include "appmgr/closed_captions.hpp"
#include "math/colour.hpp"
#include "moo/camera.hpp"
#include "moo/effect_visual_context.hpp"
#include "moo/vertex_formats.hpp" 
#include "romp/custom_mesh.hpp"
#include "romp/time_of_day.hpp"
#include "resmgr/bwresource.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_space.hpp"
#include "common/base_camera.hpp"
#include "ashes/simple_gui.hpp"


DECLARE_DEBUG_COMPONENT2( "HeightModule", 2 );


typedef ModuleManager ModuleFactory;

namespace 
{
    enum ModifierKey
    {
        CTRL_KEY,
        SHIFT_KEY,
        ALT_KEY
    };

    // Modifier keys for editing.  If these are changed, the terrain import
    // panel's resource's help text needs to be changed too.
    ModifierKey     SnapToGridKey   = SHIFT_KEY;
    ModifierKey     SetMinKey       = CTRL_KEY;
    ModifierKey     SetMaxKey       = ALT_KEY;

    // Return true if the given key is down:
    bool isKeyDown(ModifierKey key)
    {
        switch (key)
        {
        case CTRL_KEY:
            return InputDevices::isCtrlDown();
        case SHIFT_KEY:
            return InputDevices::isShiftDown();
        case ALT_KEY:
            return InputDevices::isAltDown();
            break;
        default:
            return false;
        }
    }

    // Return true if snapping should occur:
    bool shouldGridSnap()
    {
        return isKeyDown(SnapToGridKey);
    }

    // Return true if getting the max. height should occur:
    bool shouldGetMaxHeight()
    {
        return isKeyDown(SetMaxKey);
    }

    // Return true if getting the min. height should occur:
    bool shouldGetMinHeight()
    {
        return isKeyDown(SetMinKey);
    }

    template<class V>
    void quad
    ( 
        CustomMesh<V>   &mesh,
        Vector3         const &bottomLeft, 
        float           xextent, 
        float           yextent,
        Vector2         const &bottomLeftUV, 
        float           xuvextent, 
        float           yuvextent,
        Vector2         const &bottomLeftUV2, 
        float           xuvextent2, 
        float           yuvextent2,
        bool            asLines     = false
    )
    {
        // Bottom left:
        V bl;
        bl.pos_ = bottomLeft;
        bl.uv_  = bottomLeftUV;
        bl.uv2_ = bottomLeftUV2;

        // Top left:
        V tl;
        tl.pos_ = bottomLeft    + Vector3(0.0f, yextent, 0.0f);
        tl.uv_  = bottomLeftUV  + Vector2(0.0f, yuvextent );
        tl.uv2_ = bottomLeftUV2 + Vector2(0.0f, yuvextent2);

        // Bottom right
        V br;
        br.pos_ = bottomLeft    + Vector3(xextent   , 0.0f, 0.0f);
        br.uv_  = bottomLeftUV  + Vector2(xuvextent , 0.0f);
        br.uv2_ = bottomLeftUV2 + Vector2(xuvextent2, 0.0f);

        // Top right
        V tr;
        tr.pos_ = bottomLeft    + Vector3(xextent, yextent, 0.0f);
        tr.uv_  = bottomLeftUV  + Vector2(xuvextent , yuvextent );
        tr.uv2_ = bottomLeftUV2 + Vector2(xuvextent2, yuvextent2);

        if (asLines)
        {
            mesh.push_back(tl);
            mesh.push_back(tr);
            mesh.push_back(br);
            mesh.push_back(bl);
            mesh.push_back(tl);
        }
        else
        {
            mesh.push_back(tl);
            mesh.push_back(tr);
            mesh.push_back(br);

            mesh.push_back(br);
            mesh.push_back(bl);
            mesh.push_back(tl);
        }
    }

    template<class V>
    void quad
    ( 
        CustomMesh<V>       &mesh, 
        Vector3             const &bottomLeft, 
        float               xextent, 
        float               yextent, 
        DWORD               colour,
        bool                asLines = false
    )
    {
        // Bottom left:
        V bl;
        bl.pos_    = bottomLeft;
        bl.colour_ = colour;

        // Top left:
        V tl;
        tl.pos_    = bottomLeft + Vector3( 0.0f, yextent, 0.0f );
        tl.colour_ = colour;

        // Bottom right:
        V br;
        br.pos_    = bottomLeft + Vector3( xextent, 0.0f, 0.0f );
        br.colour_ = colour;

        // Top right:
        V tr;
        tr.pos_    = bottomLeft + Vector3( xextent, yextent, 0.0f );
        tr.colour_ = colour;

        if (asLines)
        {
            mesh.push_back(tl);
            mesh.push_back(tr);
            mesh.push_back(br);
            mesh.push_back(bl);
            mesh.push_back(tl);
        }
        else
        {
            mesh.push_back(tl);
            mesh.push_back(tr);
            mesh.push_back(br);

            mesh.push_back(br);
            mesh.push_back(bl);
            mesh.push_back(tl);
        }
    }

    template<class V>
    void quad
    (
        CustomMesh<V>       &mesh, 
        Vector2             const &topLeft, 
        Vector2             const &bottomRight,
        DWORD               colour,
        bool                asLines = false
    )
    {
        // Bottom left:
        V bl;
        bl.pos_.x   = topLeft.x;
        bl.pos_.y   = bottomRight.y;
        bl.pos_.z   = 0.0f;
        bl.colour_  = colour;

        // Top left:
        V tl;
        tl.pos_.x   = topLeft.x;
        tl.pos_.y   = topLeft.y;
        tl.pos_.z   = 0.0f;
        tl.colour_  = colour;

        // Bottom right:
        V br;
        br.pos_.x   = bottomRight.x;
        br.pos_.y   = bottomRight.y;
        br.pos_.z   = 0.0f;
        br.colour_  = colour;

        // Top right:
        V tr;
        tr.pos_.x   = bottomRight.x;
        tr.pos_.y   = topLeft.y;
        tr.pos_.z   = 0.0f;
        tr.colour_  = colour;

        if (asLines)
        {
            mesh.push_back(tl);
            mesh.push_back(tr);
            mesh.push_back(br);
            mesh.push_back(bl);
            mesh.push_back(tl);
        }
        else
        {
            mesh.push_back(tl);
            mesh.push_back(tr);
            mesh.push_back(br);

            mesh.push_back(br);
            mesh.push_back(bl);
            mesh.push_back(tl);
        }
    }

    template<class V>
    void vertQuad
    (
        CustomMesh<V>       &mesh, 
        float               x,
        float               hx,
        DWORD               colour,
        bool                asLines = false
    )
    {
        // Bottom left:
        V bl;
        bl.pos_.x   = x - hx;
        bl.pos_.y   = 65535.0f;
        bl.pos_.z   = 0.0f;
        bl.colour_  = colour;

        // Top left:
        V tl;
        tl.pos_.x   = x - hx;
        tl.pos_.y   = -65535.0f;
        tl.pos_.z   = 0.0f;
        tl.colour_  = colour;

        // Bottom right:
        V br;
        br.pos_.x   = x + hx;
        br.pos_.y   = 65535.0f;
        br.pos_.z   = 0.0f;
        br.colour_  = colour;

        // Top right:
        V tr;
        tr.pos_.x   = x + hx;
        tr.pos_.y   = -65535.0f;
        tr.pos_.z   = 0.0f;
        tr.colour_  = colour;

        if (asLines)
        {
            mesh.push_back(tl);
            mesh.push_back(tr);
            mesh.push_back(br);
            mesh.push_back(bl);
            mesh.push_back(tl);
        }
        else
        {
            mesh.push_back(tl);
            mesh.push_back(tr);
            mesh.push_back(br);

            mesh.push_back(br);
            mesh.push_back(bl);
            mesh.push_back(tl);
        }
    }

    template<class V>
    void horzQuad
    (
        CustomMesh<V>       &mesh, 
        float               y,
        float               hy,
        DWORD               colour,
        bool                asLines = false
    )
    {
        // Bottom left:
        V bl;
        bl.pos_.x   = 65535.0f;
        bl.pos_.y   = y - hy;        
        bl.pos_.z   = 0.0f;
        bl.colour_  = colour;

        // Top left:
        V tl;
        tl.pos_.x   = -65535.0f;
        tl.pos_.y   = y - hy;        
        tl.pos_.z   = 0.0f;
        tl.colour_  = colour;

        // Bottom right:
        V br;
        br.pos_.x   = 65535.0f;
        br.pos_.y   = y + hy;        
        br.pos_.z   = 0.0f;
        br.colour_  = colour;

        // Top right:
        V tr;
        tr.pos_.x   = -65535.0f;
        tr.pos_.y   = y + hy;        
        tr.pos_.z   = 0.0f;
        tr.colour_  = colour;

        if (asLines)
        {
            mesh.push_back(tl);
            mesh.push_back(tr);
            mesh.push_back(br);
            mesh.push_back(bl);
            mesh.push_back(tl);
        }
        else
        {
            mesh.push_back(tl);
            mesh.push_back(tr);
            mesh.push_back(br);

            mesh.push_back(br);
            mesh.push_back(bl);
            mesh.push_back(tl);
        }
    }

    template<class V>
    void quad
    ( 
        CustomMesh<V>       &mesh,
        Vector3             const &bottomLeft, 
        float               xextent, 
        float               yextent,
	    Vector2             const &bottomLeftUV, 
        float               xuvextent, 
        float               yuvextent 
    )
    {
	    // bottom left
	    V bl;
	    bl.pos_ = bottomLeft;
	    bl.uv_ = bottomLeftUV;

	    // top left
	    V tl;
	    tl.pos_ = bottomLeft + Vector3( 0.f, yextent, 0.f );
	    tl.uv_ = bottomLeftUV + Vector2( 0.f, yuvextent );

	    // bottom right
	    V br;
	    br.pos_ = bottomLeft + Vector3( xextent, 0.f, 0.f );
	    br.uv_ = bottomLeftUV + Vector2( xuvextent, 0.f );

	    // top right
	    V tr;
	    tr.pos_ = bottomLeft + Vector3( xextent, yextent, 0.f );
	    tr.uv_ = bottomLeftUV + Vector2( xuvextent, yuvextent );


	    mesh.push_back( tl );
	    mesh.push_back( tr );
	    mesh.push_back( br );

	    mesh.push_back( br );
	    mesh.push_back( bl );
	    mesh.push_back( tl );
    }
}


IMPLEMENT_CREATOR(HeightModule, Module);


HeightModule* HeightModule::currentInstance_ = 0;


HeightModule::HeightModule() : 
    gridWidth_(0), 
    gridHeight_(0),
    minX_(0),
    minY_(0),
    spaceAlpha_(0.5f),
    localToWorld_(GridCoord::zero()),
    mouseDownOn3DWindow_(false),
    mode_(EXPORT_TERRAIN),
    terrainShader_(NULL),
    selectionShader_(NULL),
    impMode_(ElevationBlit::REPLACE),
    impMinV_(0.0f),
    impMaxV_(0.0f),
    impStrength_(1.0f),
    gettingHeight_(false),
    lastCursorPos_(-1, -1)
{
    currentInstance_ = this;
}


HeightModule::~HeightModule()
{
    currentInstance_ = NULL;
}


bool HeightModule::init(DataSectionPtr pSection)
{
    return true;
}


void HeightModule::onStart()
{
    // Needed, otherwise the mouse cursor is hidden when we start?!
    ::ShowCursor(1);

    CWaitCursor waitCursor; // this may take a while

    // Read some options in:
    spaceAlpha_ = 
        Options::getOptionFloat("render/project/spaceMapAlpha", 0.5f);
    spaceAlpha_ = Math::clamp(0.0f, spaceAlpha_, 1.0f);

    std::string space = BigBang::instance().getCurrentSpace();

    handDrawnMap_ = 
        Moo::TextureManager::instance()->get
        (
            space + "/space.map.bmp",
            false,                      // allowAnimation
            false                       // mustExist
        );

    if (!handDrawnMap_)
    {
        handDrawnMap_ = 
            Moo::TextureManager::instance()->get
            (
                "resources/maps/default.map.bmp",
                false,                      // allowAnimation
                false                       // mustExist
            );
    }

    // Work out grid size:
    int minX, minY, maxX, maxY;
    TerrainUtils::terrainSize(space, minX, minY, maxX, maxY);
	minX_           = minX;
	minY_           = minY;
    gridWidth_      = maxX - minX + 1;
    gridHeight_     = maxY - minY + 1;
    localToWorld_   = GridCoord( minX, minY );

    // Work out the selection area:
    static std::string lastSpace;
    if (lastSpace != space)
    {
        lastSpace = space;
        terrainTopLeft_     = Vector2(0.0f, static_cast<float>(gridHeight_));
        terrainBottomRight_ = Vector2(static_cast<float>(gridWidth_), 0.0f);
    }
    else
    {
        terrainTopLeft_ = 
            Options::getOptionVector2
            (
                "render/height/selTopLeft", 
                Vector2(0.0f, static_cast<float>(gridHeight_))
            );
        terrainBottomRight_ = 
            Options::getOptionVector2
            (
                "render/height/selBottomRight", 
                Vector2(static_cast<float>(gridWidth_), 0.0f)
            );
    }

    viewPosition_ = Vector3(gridWidth_/2.0f, gridHeight_/2.0f, -1.0f);

    // Set the zoom to the extents of the grid
    float angle = Moo::rc().camera().fov()/2.0f;
    float yopp  = gridHeight_/2.0f;
    float xopp  = gridWidth_ /2.0f;

    // Get the distance away we have to be to see the x points and the y points
    float yheight = yopp/tanf(angle);
    float xheight = xopp/tanf(angle*Moo::rc().camera().aspectRatio());
    
    // Go back the furthest amount between the two of them
    viewPosition_.z = min(-1.05f*xheight, -1.05f*yheight);
    minViewZ_ = viewPosition_.z*1.1f;
    savedFarPlane_ = Moo::rc().camera().farPlane();
    Moo::Camera camera = Moo::rc().camera();
    camera.farPlane(minViewZ_*-1.1f);
    Moo::rc().camera(camera);

    // Create an automatic space map, to display the current progress
    HeightMap::instance().spaceInformation
    (
        space, 
        localToWorld_.x, 
        localToWorld_.y,
        gridWidth_, 
        gridHeight_
    );

    cc_ = new ClosedCaptions();
    Commentary::instance().addView(&*cc_);
    cc_->visible( true );

    // Load the shaders:
    std::string terrainShaderFile =
        BWResource::resolveFilename("resources/shaders/heightmap.fx");
    if (HeightMap::instance().isPS2())
    {
        terrainShaderFile =
           BWResource::resolveFilename("resources/shaders/heightmap2.fx");
    }
    terrainShader_ = new Moo::EffectMaterial();
    terrainShader_->initFromEffect(terrainShaderFile);

    std::string selShaderFile = 
        BWResource::resolveFilename("resources/shaders/heightmap_sel.fx");
    selectionShader_ = new Moo::EffectMaterial();
    selectionShader_->initFromEffect(selShaderFile);
}


int HeightModule::onStop()
{
    ::ShowCursor(0);

    HeightMap::instance().save();
    HeightMap::instance().onStop();

    Options::setOptionFloat
    (
        "render/project/spaceMapAlpha", 
        spaceAlpha_
    );
    Options::setOptionVector2
    (
        "render/height/selTopLeft", 
        terrainTopLeft_
    );
    Options::setOptionVector2
    (
        "render/height/selBottomRight", 
        terrainBottomRight_
    );

    cc_->visible(false);
    Commentary::instance().delView(&*cc_);
    cc_ = NULL;

    Moo::Camera camera = Moo::rc().camera();
    camera.farPlane(savedFarPlane_);
    Moo::rc().camera(camera);

    delete terrainShader_  ; terrainShader_   = NULL;
    delete selectionShader_; selectionShader_ = NULL;

    return 0;
}


void HeightModule::onPause()
{
    cc_->visible(false);
    Commentary::instance().delView(&*cc_);
}


void HeightModule::onResume( int exitCode )
{
    Commentary::instance().addView(&*cc_);
    cc_->visible( true );
}


bool HeightModule::updateState(float dTime)
{
    cc_->update( dTime );
    HeightMap::instance().update(dTime);

    // Set input focus as appropriate:
    bool acceptInput = BigBang::instance().cursorOverGraphicsWnd();
    InputDevices::setFocus(acceptInput);

    // Handle show/hide cursor for panning:
    if (InputDevices::isKeyDown(KeyEvent::KEY_RIGHTMOUSE))
    {
        if (lastCursorPos_.x == -1 && lastCursorPos_.y == -1)
        {
            ::ShowCursor(FALSE);
            ::GetCursorPos(&lastCursorPos_);
        }
        ::SetCursorPos(lastCursorPos_.x, lastCursorPos_.y);
    }
    else
    {
        if (lastCursorPos_.x != -1 && lastCursorPos_.y != -1)
        {
            ::ShowCursor(TRUE);
            lastCursorPos_ = CPoint(-1, -1);
        }
    }

    SimpleGUI::instance().update(dTime);

    return true;
}


void HeightModule::render(float dTime)
{
    // Update the scales.
    CPoint pt(0, 0);
    Vector2 worldPos = gridPos(pt);
    ++pt.x;
    Vector2 xWorldPos = gridPos(pt);
    --pt.x; ++pt.y;
    Vector2 yWorldPos = gridPos(pt);
    scaleX_ = fabsf(xWorldPos.x - worldPos.x);
    scaleY_ = fabsf(yWorldPos.y - worldPos.y);

    if (!Moo::rc().device())
        return;

    if (!handDrawnMap_)
        return;

    DX::Device* device_ = Moo::rc().device();

    device_->Clear
    ( 
        0, 
        NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
        0xff004e98, // blue
        1, 
        0 
    );
    Moo::rc().reset();
    Moo::rc().updateViewTransforms();
    Moo::rc().updateProjectionMatrix();

    Moo::EffectVisualContext::instance().initConstants();
    ComObjectWrap<ID3DXEffect> pEffect = terrainShader_->pEffect()->pEffect();

    Matrix view;
    view.setTranslate(viewPosition_);
    view.invert();
    Matrix WVP( Moo::rc().world() );
    WVP.postMultiply( view );
    WVP.postMultiply( Moo::rc().projection() );
    pEffect->SetMatrix("worldViewProjection", &WVP);

    HeightMap::instance().update(dTime);

    float minV = HeightMap::normalizeHeight(HeightMap::instance().minHeight());
    float maxV = HeightMap::normalizeHeight(HeightMap::instance().maxHeight());
    float invScaleHeight = 1.0f/(maxV - minV);
    pEffect->SetFloat("minHeight", minV);
    pEffect->SetFloat("invScaleHeight", invScaleHeight);
    pEffect->SetFloat("alpha", spaceAlpha_);
    pEffect->SetTexture("handDrawnMap", handDrawnMap_->pTexture());
    pEffect->SetTexture("heightMap", HeightMap::instance().texture());

    bool flipTerrain = !HeightMap::instance().isPS2();

    terrainShader_->begin();
    terrainShader_->beginPass(0);
    {
		CustomMesh<Moo::VertexXYZUV2> mesh;
        quad
        ( 
            mesh, 
            Vector3(0.0f, 0.0f, 0.0f),
            static_cast<float>(gridWidth_ ),
            static_cast<float>(gridHeight_),
            flipTerrain ? Vector2(0.0f, 0.0f) : Vector2(0.0f, 1.0f), 
            1.0f,   
            flipTerrain ? +1.0f : -1.0f,
            Vector2( 0.0f, 0.0f ), 
            1.0f, 
            1.0f
        );
        mesh.drawEffect();
    }
    terrainShader_->endPass();
    terrainShader_->end();

    // Draw the current grid, selection:
    pEffect = selectionShader_->pEffect()->pEffect();
    pEffect->SetMatrix("worldViewProjection", &WVP);

    selectionShader_->begin();
    selectionShader_->beginPass(0);
    {
        // If getting height, draw the selected rectangle:
        if (gettingHeight_)
        {
            bool showMaxV = shouldGetMaxHeight();
            bool showMinV = shouldGetMinHeight();
            DWORD colour = 0x40000000;
            if (showMaxV)
                colour |= 0x000000ff;
            if (showMinV)
                colour |= 0x0000ff00;
            drawRect(heightDownPt_, heightCurrentPt_, colour);
        }

        // Draw the grid if Alt is down:
        if (shouldGridSnap())
        {
            for (float x = -4.0f; x <= gridWidth_ + 4.0f; x += 1.0f)
            {
                drawVLine(x, 0x40007f00, 0.5);
            }
            for (float y = -4.0f; y <= gridHeight_ + 4.0f; y += 1.0f)
            {
                drawHLine(y, 0x40007f00, 0.5);
            }
        }

        drawRect(terrainTopLeft_, terrainBottomRight_, 0x7f0000ff, 1);

        drawHandle
        (
            Vector2(terrainTopLeft_.x, terrainTopLeft_.y),
            0xff0000ff, 3, 3
        );
        drawHandle
        (
            Vector2(terrainTopLeft_.x, 0.5f*(terrainBottomRight_.y + terrainTopLeft_.y)),
            0xff0000ff, 3, 3
        );
        drawHandle
        (
            Vector2(terrainTopLeft_.x, terrainBottomRight_.y),
            0xff0000ff, 3, 3
        );
        drawHandle
        (
            Vector2(terrainBottomRight_.x, terrainTopLeft_.y),
            0xff0000ff, 3, 3
        );
        drawHandle
        (
            Vector2(terrainBottomRight_.x, 0.5f*(terrainBottomRight_.y + terrainTopLeft_.y)),
            0xff0000ff, 3, 3
        );
        drawHandle
        (
            Vector2(terrainBottomRight_.x, terrainBottomRight_.y),
            0xff0000ff, 3, 3
        );
        drawHandle
        (
            Vector2(0.5f*(terrainTopLeft_.x + terrainBottomRight_.x), terrainTopLeft_.y),
            0xff0000ff, 3, 3
        );
        drawHandle
        (
            Vector2(0.5f*(terrainTopLeft_.x + terrainBottomRight_.x), terrainBottomRight_.y),
            0xff0000ff, 3, 3
        );

        // Draw the camera position:
        // This is commented out because it should be added in 1.8.1
        //Matrix view = BigBangCamera::instance().currentCamera().view();
        //view.invert();
        //Vector3 pos = view.applyToOrigin();
        //Vector2 gridPos = worldPosToGridPos(pos);
        //drawHandle
        //(
        //    gridPos,
        //    0xffff0000, 5, 1
        //);   
        //drawHandle
        //(
        //    gridPos,
        //    0xffff0000, 1, 5
        //); 
    }
    selectionShader_->endPass();
    selectionShader_->end();

    SimpleGUI::instance().draw();
    writeStatus();
}


bool HeightModule::handleKeyEvent(const KeyEvent & keyEvent)
{
    updateCursor();
        
    if (keyEvent.key() == KeyEvent::KEY_LEFTMOUSE)
    {
        if ( keyEvent.isKeyDown() )
        {
            mouseDownOn3DWindow_ = true;
        }
        else
        {
            mouseDownOn3DWindow_ = false;
        }
    }

    // Go to the world location when the middle mouse button is held down
    if (keyEvent.key() == KeyEvent::KEY_MIDDLEMOUSE && !keyEvent.isKeyDown())
    {
        gotoWorldPos(keyEvent);
        return true;
    }

    // Start getting height:
    if
    (
        keyEvent.key() == KeyEvent::KEY_LEFTMOUSE 
        &&
        keyEvent.isKeyDown()
        &&
        (
            shouldGetMaxHeight()
            ||
            shouldGetMinHeight()
        )
    )
    {
        startGetHeight(keyEvent);
    }

    // Stop getting height:
    if
    (
        keyEvent.key() == KeyEvent::KEY_LEFTMOUSE 
        &&
        !keyEvent.isKeyDown()
        &&
        gettingHeight_
    )
    {
        gettingHeight_ = false;
    }

    // Editing imported terrain case:
    if
    (
        keyEvent.key() == KeyEvent::KEY_LEFTMOUSE 
        && 
        keyEvent.isKeyDown()
        && 
        !gettingHeight_
    )
    {
        startTerrainEdit(keyEvent);
    }

    return false;
}

void HeightModule::gotoWorldPos(const KeyEvent & /*keyEvent*/)
{
	// Get where we click in grid coords
	Vector2 gridPos = currentGridPos();
	Vector3 world = gridPosToWorldPos(gridPos);
    
    // Find the height at this point
    float height = TerrainUtils::heightAtPos(world.x, world.z, true);
    world.y = height + Options::getOptionInt( "graphics/farclip", 500 )/10.0f;

	// Set the view matrix to the new world coords
	Matrix view = BigBangCamera::instance().currentCamera().view();
	view.setTranslate(world);
	view.preRotateX(DEG_TO_RAD(30.f));
	view.invert();
	BigBangCamera::instance().currentCamera().view(view);

    if (Options::getOptionInt("camera/ortho") == BigBangCamera::CT_Orthographic)
    {
        BigBangCamera::instance().changeToCamera(BigBangCamera::CT_MouseLook   );
        BigBangCamera::instance().changeToCamera(BigBangCamera::CT_Orthographic);
    }

    // Now, change back to object mode
    PyObject* pModule = PyImport_ImportModule("BigBangDirector");
    if (pModule != NULL)
    {
        PyObject* pScriptObject = PyObject_GetAttr(pModule, Py_BuildValue("s", "bd"));

        if (pScriptObject != NULL)
        {
            Script::call
            (
                PyObject_GetAttrString(pScriptObject, "changeToMode"),
                Py_BuildValue( "(s)", "Object" ),
                "HeightModule"
            );                
            Py_DECREF(pScriptObject);
        }
        PanelManager::instance()->setDefaultToolMode();
        Py_DECREF(pModule);
    }
}


void HeightModule::startTerrainEdit(const KeyEvent & /*keyevent*/)
{
    normalizeTerrainRect();
    POINT cursorPt = BigBang::instance().currentCursorPosition();
    terrainDownPt_ = gridPos(cursorPt);
    terrainDir_    = pointToDir(cursorPt);
    terrainTLDown_ = terrainTopLeft_;
    terrainBRDown_ = terrainBottomRight_;
}


void HeightModule::startGetHeight(const KeyEvent & /*keyevent*/)
{
    gettingHeight_   = true;
    POINT cursorPt   = BigBang::instance().currentCursorPosition();
    heightDownPt_    = gridPos(cursorPt);
    heightCurrentPt_ = heightDownPt_;
    heightQuery();
}


bool HeightModule::handleMouseEvent(const MouseEvent &mouseEvent)
{
    bool handled = false;

    updateCursor();

    // Adjust zoom:
    if (mouseEvent.dz() != 0)
    {
        handleZoom(mouseEvent);
        handled = true;
    }

    // Pan around:
    if 
    (
        (mouseEvent.dx() != 0 || mouseEvent.dy() != 0) 
        && 
        InputDevices::isKeyDown(KeyEvent::KEY_RIGHTMOUSE)
    )
    {
        handlePan(mouseEvent);
        handled = true;
    }

    // Editing the terrain's extents:
    if 
    (
        (mode_ == IMPORT_TERRAIN || mode_ == EXPORT_TERRAIN)
        &&
        (mouseEvent.dx() != 0 || mouseEvent.dy() != 0) 
        && 
        InputDevices::isKeyDown( KeyEvent::KEY_LEFTMOUSE ) 
        && 
        mouseDownOn3DWindow_
        && 
        !gettingHeight_
    )
    {
        handleTerrainEdit(mouseEvent);
        handled = true;
    }

    // Getting height:
    if
    (
        (mouseEvent.dx() != 0 || mouseEvent.dy() != 0)
        && 
        mouseDownOn3DWindow_
        && 
        gettingHeight_
    )
    {
        handleGetHeight(mouseEvent);
    }

    return handled;
}


void HeightModule::handleZoom(const MouseEvent &mouseEvent)
{
    viewPosition_.z += (mouseEvent.dz() > 0) ? -minViewZ_/25.0f : minViewZ_/25.0f;

    if (viewPosition_.z > -1.0f)
        viewPosition_.z = -1.0f;

    if (viewPosition_.z < minViewZ_)
        viewPosition_.z = minViewZ_;
}


void HeightModule::handlePan(const MouseEvent &mouseEvent)
{
    viewPosition_.x -= scaleX_*mouseEvent.dx();
    viewPosition_.y += scaleY_*mouseEvent.dy();
}


void HeightModule::handleTerrainEdit(const MouseEvent &/*mouseEvent*/)
{
    Vector2 curPos = currentGridPos();
    bool redraw = true;
    switch (terrainDir_)
    {
    case TERRAIN_MISS:
        redraw = false;
        break;
    case TERRAIN_MIDDLE:
        {
        Vector2 delta       = curPos - terrainDownPt_;
        terrainTopLeft_     = terrainTLDown_ + delta;
        terrainBottomRight_ = terrainBRDown_ + delta;
        Vector2 snapTL(terrainTopLeft_);
        Vector2 snapBR(terrainBottomRight_);
        snapTL.x = snapTerrainCoord(snapTL.x);
        snapTL.y = snapTerrainCoord(snapTL.y);
        snapBR.x = snapTerrainCoord(snapBR.x);
        snapBR.y = snapTerrainCoord(snapBR.y);
        if 
        (
            fabs(snapTL.x - terrainTopLeft_.x) 
            <= 
            fabs(snapBR.x - terrainBottomRight_.x)
        )
        {
            float diff = terrainBottomRight_.x - terrainTopLeft_.x;
            terrainTopLeft_    .x = snapTL.x;
            terrainBottomRight_.x = terrainTopLeft_.x + diff;
        }
        else
        {
            float diff = terrainBottomRight_.x - terrainTopLeft_.x;            
            terrainBottomRight_.x = snapBR.x;
            terrainTopLeft_    .x = terrainBottomRight_.x - diff;
        }
        if 
        (
            fabs(snapTL.y - terrainTopLeft_.y) 
            <= 
            fabs(snapBR.y - terrainBottomRight_.y)
        )
        {
            float diff = terrainBottomRight_.y - terrainTopLeft_.y;
            terrainTopLeft_    .y = snapTL.y;
            terrainBottomRight_.y = terrainTopLeft_.y + diff;
        }
        else
        {
            float diff = terrainBottomRight_.y - terrainTopLeft_.y;            
            terrainBottomRight_.y = snapBR.y;
            terrainTopLeft_    .y = terrainBottomRight_.y - diff;
        }
        }
        break;
    case TERRAIN_NORTH:
        terrainTopLeft_    .y = snapTerrainCoord(curPos.y);
        break;
    case TERRAIN_NORTH_EAST:
        terrainBottomRight_.x = snapTerrainCoord(curPos.x);
        terrainTopLeft_    .y = snapTerrainCoord(curPos.y);
        break;
    case TERRAIN_EAST:
        terrainBottomRight_.x = snapTerrainCoord(curPos.x);
        break;
    case TERRAIN_SOUTH_EAST:
        terrainBottomRight_.x = snapTerrainCoord(curPos.x);
        terrainBottomRight_.y = snapTerrainCoord(curPos.y);
        break;
    case TERRAIN_SOUTH:
        terrainBottomRight_.y = snapTerrainCoord(curPos.y);
        break;
    case TERRAIN_SOUTH_WEST:
        terrainTopLeft_    .x = snapTerrainCoord(curPos.x);
        terrainBottomRight_.y = snapTerrainCoord(curPos.y);
        break;
    case TERRAIN_WEST:
        terrainTopLeft_    .x = snapTerrainCoord(curPos.x);
        break;
    case TERRAIN_NORTH_WEST:
        terrainTopLeft_    .x = snapTerrainCoord(curPos.x);
        terrainTopLeft_    .y = snapTerrainCoord(curPos.y);
        break;
    }
    if (redraw)
        redrawImportedTerrain();
}


void HeightModule::handleGetHeight(const MouseEvent & /*mouseEvent*/)
{
    heightQuery();
}


void HeightModule::projectMapAlpha(float a)
{
    spaceAlpha_ = a;
}


float HeightModule::projectMapAlpha()
{
    return spaceAlpha_;
}


HeightModule::Mode HeightModule::getMode() const
{
    return mode_;
}


void HeightModule::setElevationData
(
    SmartPointer<ElevationImage>    image,
    float                           *left,
    float                           *top,
    float                           *right,
    float                           *bottom
)
{
    if (left != NULL)
        terrainTopLeft_.x = *left; 
    if (top != NULL)
        terrainTopLeft_.y = *top;
    if (right != NULL)
        terrainBottomRight_.x = *right;
    if (bottom != NULL)
        terrainBottomRight_.y = *bottom;
    normalizeTerrainRect();

    // If not inside the space's area then do some hueristics:
    // (note assymetry between x and y below is because +y is up).
    if 
    (
        terrainTopLeft_.x < 0 || terrainBottomRight_.x > gridWidth_
        ||
        terrainTopLeft_.y > gridHeight_ || terrainBottomRight_.y < 0     
    )
    {
        // If the imported area still fits inside the space, then import to 
        // (0,gridHeight):
        float w = std::abs(terrainBottomRight_.x - terrainTopLeft_.x);
        float h = std::abs(terrainBottomRight_.y - terrainTopLeft_.y);
        if (w <= gridWidth_ && h <= gridHeight_)
        {
            WARNING_MSG( "Imported terrain outside space, it was moved to the top-left.\n" );
            terrainTopLeft_ = Vector2(0.0f, static_cast<float>(gridHeight_));
            terrainBottomRight_ = Vector2(w, static_cast<float>(gridHeight_) - h);
        }
        // The imported area doesn't fit, import into the entire space:
        else
        {
            WARNING_MSG( "Imported terrain does not fit into space, it was rescaled to fit.\n" );
            terrainTopLeft_     = Vector2(0.0f, static_cast<float>(gridHeight_));
            terrainBottomRight_ = Vector2(static_cast<float>(gridWidth_), 0.0f);
        }        
    }

    elevationData_ = image;
    if (elevationData_ != NULL)
    {
        mode_ = IMPORT_TERRAIN;
        redrawImportedTerrain();
        HeightMap::instance().updateMinMax();
    }
    else
    {
        mode_ = EXPORT_TERRAIN;
        HeightMap::instance().clearImpTerrain();
    }
}


void HeightModule::rotateElevationData(bool clockwise)
{
    if (elevationData_ != NULL)
    {
        elevationData_->rotate(clockwise);
        redrawImportedTerrain();
    }
}


void HeightModule::flipElevationData(FlipDir dir)
{
    if (elevationData_ != NULL)
    {
        switch (dir)
        {
        case FLIP_X:
            elevationData_->flip(true);  // true = flip x
            break;
        case FLIP_Y:
            elevationData_->flip(false); // false = flip y
            break;
        case FLIP_HEIGHT:
            elevationData_->flipHeight();
            break;
        }
        redrawImportedTerrain();
    }
}


void HeightModule::setImportOptions
(
    ElevationBlit::Mode mode,
    float               minV, 
    float               maxV,
    float               strength
)
{
    impMode_        = mode;
    impMinV_        = minV;
    impMaxV_        = maxV;
    impStrength_    = strength;
    if (elevationData_ != NULL)
    {
        redrawImportedTerrain();
    }
}


size_t HeightModule::undoSize() const
{
    return 
        ElevationBlit::importUndoSize
        (
            terrainTopLeft_    .x, terrainTopLeft_    .y,
            terrainBottomRight_.x, terrainBottomRight_.y
        );
}


bool HeightModule::doImport(bool doUndo, bool showProgress, bool forceToMem)
{
    if (elevationData_ != NULL)
    {
        bool result = true;
        if (doUndo)
        {
            result =
                ElevationBlit::saveUndoForImport
                (
                    terrainTopLeft_    .x, terrainTopLeft_    .y,
                    terrainBottomRight_.x, terrainBottomRight_.y,
                    "Import terrain",
                    showProgress
                );
        }
        if (result)
        {
            ElevationBlit::import
            (
                *elevationData_,
                terrainTopLeft_    .x, terrainTopLeft_    .y,
                terrainBottomRight_.x, terrainBottomRight_.y,
                impMode_,
                impMinV_,
                impMaxV_,
                impStrength_,
                showProgress,
                forceToMem
            );
            HeightMap::instance().invalidateImportedTerrain();
        }
        return result;
    }
    else
    {
        return false;
    }
}


bool HeightModule::doExport(char const *filename, bool showProgress)
{
    ElevationImage image;
    ElevationBlit::exportTo
    (
        image,
        terrainTopLeft_    .x, terrainTopLeft_    .y,
        terrainBottomRight_.x, terrainBottomRight_.y,
        showProgress
    );
    return 
        image.save
        (
            filename,
            &terrainTopLeft_    .x, &terrainTopLeft_    .y,
            &terrainBottomRight_.x, &terrainBottomRight_.y
        );
}


void HeightModule::updateMinMax()
{
    HeightMap::instance().updateMinMax();
}


/*static*/ void HeightModule::ensureHeightMapCalculated()
{
    HeightModule *heightModule = HeightModule::currentInstance();
    bool haveHeightModule = heightModule != NULL;
    if (!haveHeightModule)
    {
        heightModule = new HeightModule();
        heightModule->onStart();
    }
    HeightMap::instance().makeValid();
    if (!haveHeightModule)
    {
        heightModule->onStop();
        delete heightModule; heightModule = NULL;
    }    
}


/*static*/ void HeightModule::doNotSaveHeightMap()
{
    HeightModule *heightModule = HeightModule::currentInstance();
    bool haveHeightModule = heightModule != NULL;
    if (!haveHeightModule)
    {
        heightModule = new HeightModule();
        heightModule->onQuickStart();
    }
    HeightMap::instance().doNotSaveHeightMap();
    if (!haveHeightModule)
    {
        delete heightModule; heightModule = NULL;
    }    
}


/*static*/ HeightModule* HeightModule::currentInstance()
{
    return currentInstance_;
}


/**
 *  This function starts the HeightModule with enough information to
 *  access the HeightMap instance.
 */
void HeightModule::onQuickStart()
{
    std::string space = BigBang::instance().getCurrentSpace();
    int minX, minY, maxX, maxY;
    TerrainUtils::terrainSize(space, minX, minY, maxX, maxY);
	minX_ = minX;
	minY_ = minY;
    gridWidth_          = maxX - minX + 1;
    gridHeight_         = maxY - minY + 1;
    localToWorld_       = GridCoord( minX, minY );

    HeightMap::instance().spaceInformation
    (
        space, 
        localToWorld_.x, 
        localToWorld_.y,
        gridWidth_, 
        gridHeight_,
        false
    );
}


void HeightModule::writeStatus()
{
    BigBang::instance().setStatusMessage(1, "");
    BigBang::instance().setStatusMessage(2, "");
    BigBang::instance().setStatusMessage(4, "");

	char buf[1024];

	//Panel 0 - memory load
	sprintf( buf, "Memory Load: %u%%", BigBang::instance().getMemoryLoad() );
	BigBang::instance().setStatusMessage( 0, buf );

    // Panel 3 - locator position and height:
    Vector2 gridPos = currentGridPos();
    Vector3 world = gridPosToWorldPos(gridPos);
    ChunkDirMapping *dirMap = BigBang::instance().chunkDirMapping();
    std::stringstream output;
    output << dirMap->outsideChunkIdentifier(world);
    float height = HeightMap::instance().heightAtGridPos(gridPos);
    if (height != std::numeric_limits<float>::max())
        output << ' ' << height;
    output << '\0';
    std::string panelText = output.str();
    BigBang::instance().setStatusMessage(3, panelText);

    // Panel 5 - number of chunks loaded:
    EditorChunkCache::lock();

	int dirtyTotal = BigBang::instance().dirtyChunks();
	if ( dirtyTotal )
	{
		sprintf( buf, "%d chunks loaded (%d dirty)",
			EditorChunkCache::chunks_.size(),
			dirtyTotal );
	}
	else
	{
		sprintf( buf, "%d chunks loaded",
			EditorChunkCache::chunks_.size() );
	}
	EditorChunkCache::unlock();

    BigBang::instance().setStatusMessage(5, buf);

    MainFrame *mainFrame = reinterpret_cast<MainFrame *>(AfxGetMainWnd());
    mainFrame->frameUpdate(true);
}


Vector2 HeightModule::gridPos(POINT pt) const
{
    Vector3 cursorPos = Moo::rc().camera().nearPlanePoint(
            (float(pt.x) / Moo::rc().screenWidth()) * 2.0f - 1.0f,
            1.0f - (float(pt.y) / Moo::rc().screenHeight()) * 2.0f );

    Matrix view;
    view.setTranslate( viewPosition_ );

    Vector3 worldRay = view.applyVector( cursorPos );
    worldRay.normalise();

    PlaneEq gridPlane( Vector3(0.0f, 0.0f, 1.0f), .0001f );

    Vector3 gridPos = gridPlane.intersectRay( viewPosition_, worldRay );

    return Vector2( gridPos.x, gridPos.y );
}


Vector2 HeightModule::currentGridPos() const
{
    POINT pt = BigBang::instance().currentCursorPosition();
    return gridPos(pt);
}


Vector3 HeightModule::gridPosToWorldPos(Vector2 const &gridPos) const
{
    Vector2 w = 
        (gridPos + Vector2(float(localToWorld_.x), float(localToWorld_.y)))*GRID_RESOLUTION;

    return Vector3(w.x, 0, w.y);
}


Vector2 HeightModule::worldPosToGridPos(Vector3 const &pos) const
{
    return 
        Vector2
        (
            pos.x/GRID_RESOLUTION - float(localToWorld_.x),
            pos.z/GRID_RESOLUTION - float(localToWorld_.y)
        );
}


HeightModule::TerrainDir HeightModule::pointToDir(POINT pt) const
{
    // Get the position in the world:
    Vector2 worldPos = gridPos(pt);

    // Noramlized coordinates of the terrain selection:
    float left   = std::min(terrainTopLeft_.x, terrainBottomRight_.x);
    float right  = std::max(terrainTopLeft_.x, terrainBottomRight_.x);
    float top    = std::max(terrainTopLeft_.y, terrainBottomRight_.y);
    float bottom = std::min(terrainTopLeft_.y, terrainBottomRight_.y);

    // See if the point is close to any of the corners:
    if
    (
        fabsf(left - worldPos.x) < 3*scaleX_ &&
        fabsf(top  - worldPos.y) < 3*scaleY_
    )
    {
        return TERRAIN_NORTH_WEST;
    }
    if
    (
        fabsf(right - worldPos.x) < 3*scaleX_ &&
        fabsf(top   - worldPos.y) < 3*scaleY_
    )
    {
        return TERRAIN_NORTH_EAST;
    }
    if
    (
        fabsf(right  - worldPos.x) < 3*scaleX_ &&
        fabsf(bottom - worldPos.y) < 3*scaleY_
    )
    {
        return TERRAIN_SOUTH_EAST;
    }
    if
    (
        fabsf(left   - worldPos.x) < 3*scaleX_ &&
        fabsf(bottom - worldPos.y) < 3*scaleY_
    )
    {
        return TERRAIN_SOUTH_WEST;
    }

    // See if the point is close to any of the edges:
    if 
    (
        left <= worldPos.x && worldPos.x <= right 
        &&
        fabsf(top  - worldPos.y) < 3*scaleY_
    )
    {
        return TERRAIN_NORTH;
    }
    if 
    (
        bottom <= worldPos.y && worldPos.y <= top
        &&
        fabsf(right - worldPos.x) < 3*scaleX_
    )
    {
        return TERRAIN_EAST;
    }
    if 
    (
        left <= worldPos.x && worldPos.x <= right 
        &&
        fabsf(bottom  - worldPos.y) < 3*scaleY_
    )
    {
        return TERRAIN_SOUTH;
    }
    if 
    (
        bottom <= worldPos.y && worldPos.y <= top
        &&
        fabsf(left - worldPos.x) < 3*scaleX_
    )
    {
        return TERRAIN_WEST;
    }

    // See if the point is inside the selected terrain:
    if
    (
        left <= worldPos.x && worldPos.x <= right
        &&
        bottom <= worldPos.y && worldPos.y <= top
    )
    {
        return TERRAIN_MIDDLE;
    }

    // Must have missed:
    return TERRAIN_MISS;
}


void HeightModule::normalizeTerrainRect()
{
    if (terrainTopLeft_.x > terrainBottomRight_.x)
        std::swap(terrainTopLeft_.x, terrainBottomRight_.x);
    if (terrainTopLeft_.y < terrainBottomRight_.y)
        std::swap(terrainTopLeft_.y, terrainBottomRight_.y);
}


float HeightModule::snapTerrainCoord(float v) const
{
    if (shouldGridSnap())
    {
        if (v >= 0.0f)
            return static_cast<float>(static_cast<int>((v + 0.5f)));
        else
            return static_cast<float>(static_cast<int>((v - 0.5f)));
    }
    else
    {
        return v;
    }
}


void 
HeightModule::drawRect
(
    Vector2     const &ul, 
    Vector2     const &br, 
    uint32      colour, 
    int         ht
)
{
    CustomMesh<Moo::VertexXYZL> leftEdge  ;
    CustomMesh<Moo::VertexXYZL> rightEdge ;
    CustomMesh<Moo::VertexXYZL> topEdge   ;
    CustomMesh<Moo::VertexXYZL> bottomEdge;

    quad
    (
        leftEdge,
        Vector2(ul.x - ht*scaleX_, ul.y - ht*scaleY_),
        Vector2(ul.x + ht*scaleX_, br.y - ht*scaleY_),
        colour
    );
    quad
    (
        rightEdge,
        Vector2(br.x - ht*scaleX_, ul.y - ht*scaleY_),
        Vector2(br.x + ht*scaleX_, br.y - ht*scaleY_),
        colour
    );
    quad
    (
        topEdge,
        Vector2(ul.x - ht*scaleX_, ul.y - ht*scaleY_),
        Vector2(br.x + ht*scaleX_, ul.y + ht*scaleY_),
        colour
    );
    quad
    (
        bottomEdge,
        Vector2(ul.x - ht*scaleX_, br.y - ht*scaleY_),
        Vector2(br.x + ht*scaleX_, br.y + ht*scaleY_),
        colour
    );

    leftEdge   .drawEffect();
    rightEdge  .drawEffect();
    topEdge    .drawEffect();
    bottomEdge .drawEffect();
}


void 
HeightModule::drawRect
(
    Vector2     const &ul, 
    Vector2     const &br, 
    uint32      colour
)
{
    CustomMesh<Moo::VertexXYZL> mesh;
    quad(mesh, ul, br, colour);
    mesh.drawEffect();
}


void 
HeightModule::drawHandle
( 
    Vector2     const &v, 
    uint32      colour, 
    int         hx, 
    int         hy 
) const
{
    CustomMesh<Moo::VertexXYZL> handleMesh;
    quad
    (
        handleMesh,
        Vector3(v.x - hx*scaleX_, v.y - hy*scaleY_, 0.0f),
        (2*hx + 1)*scaleX_,
        (2*hy + 1)*scaleX_,
        colour
    );
    handleMesh.drawEffect();
}


void HeightModule::drawVLine(float x, uint32 colour, float hx) const
{
    CustomMesh<Moo::VertexXYZL> lineMesh;
    vertQuad(lineMesh, x, hx*scaleX_, colour);
    lineMesh.drawEffect();
}


void HeightModule::drawHLine(float y, uint32 colour, float hy) const
{
    CustomMesh<Moo::VertexXYZL> lineMesh;
    horzQuad(lineMesh, y, hy*scaleY_, colour);
    lineMesh.drawEffect();
}


void HeightModule::redrawImportedTerrain()
{
    if (elevationData_ != NULL)
    {
        HeightMap::instance().drawImportedTerrain
        (
            terrainTopLeft_.x    , terrainTopLeft_.y    ,
            terrainBottomRight_.x, terrainBottomRight_.y,
            impMode_,
            impMinV_,
            impMaxV_,
            impStrength_,
            *elevationData_
        );
    }
}

void HeightModule::heightQuery()
{
    HeightMap::instance().undrawImportedTerrain();
    POINT cursorPt   = BigBang::instance().currentCursorPosition();
    heightCurrentPt_ = gridPos(cursorPt);
    float minv, maxv;
    HeightMap::instance().heightRange
    (
        heightDownPt_,
        heightCurrentPt_,
        minv,
        maxv
    );
    if (!shouldGetMaxHeight()) 
        maxv = -std::numeric_limits<float>::max();
    if (!shouldGetMinHeight()) 
        minv = +std::numeric_limits<float>::max();
    PageTerrainImport *pti = PageTerrainImport::instance();
    if (pti != NULL)
        pti->setQueryHeightRange(minv, maxv);
}


void HeightModule::updateCursor()
{
    // Update the cursor:
    if (mode_ == IMPORT_TERRAIN || mode_ == EXPORT_TERRAIN)
    {
        // Is the user sampling:
        if (shouldGetMaxHeight() || shouldGetMinHeight())
        {
            HCURSOR cursor = ::AfxGetApp()->LoadCursor(MAKEINTRESOURCE(IDC_HEIGHTPICKER));
            SetCursor(cursor);
        }
        // Is the mouse over the selection cursor?
        else
        {    
            TerrainDir dir = TERRAIN_MISS;
            if (InputDevices::isKeyDown(KeyEvent::KEY_LEFTMOUSE))
                dir = terrainDir_;
            else
                dir = pointToDir(BigBang::instance().currentCursorPosition());
            LPTSTR cursorID = NULL;
            switch (dir)
            {    
            case TERRAIN_MIDDLE:
                cursorID = IDC_SIZEALL;
                break;
            case TERRAIN_NORTH:         // fall through
            case TERRAIN_SOUTH:
                cursorID = IDC_SIZENS;
                break;
            case TERRAIN_EAST:          // fall through
            case TERRAIN_WEST:
                cursorID = IDC_SIZEWE;
                break;
            case TERRAIN_NORTH_EAST:    // fall through
            case TERRAIN_SOUTH_WEST:
                cursorID = IDC_SIZENESW;
                break;    
            case TERRAIN_SOUTH_EAST:    // fall through
            case TERRAIN_NORTH_WEST:
                cursorID = IDC_SIZENWSE;
                break;   
            case TERRAIN_MISS:          // fall through
            default:
                cursorID = IDC_ARROW;
                break;
            }
            SetCursor(::LoadCursor(NULL, cursorID));
        }
    }
    else
    {
        SetCursor(::LoadCursor(NULL, IDC_ARROW));
    }
}
