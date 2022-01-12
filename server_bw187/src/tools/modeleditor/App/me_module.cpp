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

#include "model_editor.h"
#include "me_shell.hpp"
#include "me_app.hpp"
#include "romp_harness.hpp"
#include "main_frm.h"
#include "tools_camera.hpp"

#include "appmgr/module_manager.hpp"
#include "appmgr/options.hpp"
#include "appmgr/closed_captions.hpp"
#include "ashes/simple_gui.hpp"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk_manager.hpp"
#include "duplo/material_draw_override.hpp"
#include "gizmo/gizmo_manager.hpp"
#include "gizmo/tool_manager.hpp"
#include "math/planeeq.hpp"
#include "moo/terrain_renderer.hpp"
#include "moo/texture_manager.hpp"
#include "moo/visual_channels.hpp"
#include "resmgr/auto_config.hpp"
#include "resmgr/bwresource.hpp"
#include "romp/custom_mesh.hpp"
#include "romp/flora.hpp"
#include "romp/geometrics.hpp"
#include "romp/time_of_day.hpp"
#include "romp/texture_renderer.hpp"

#include "speedtree/speedtree_renderer.hpp"

#include "ual/ual_manager.hpp"

#include "page_object.hpp"
#include "page_materials.hpp"

#include "me_module.hpp"

static DogWatch s_detailTick( "detail_tick" );
static DogWatch s_detailDraw( "detail_draw" );

DECLARE_DEBUG_COMPONENT2( "Shell", 0 )

// Here are all our tokens to get the various components to link
extern int ChunkModel_token;
extern int ChunkLight_token;
extern int ChunkTerrain_token;
extern int ChunkFlare_token;
extern int ChunkWater_token;
extern int ChunkTree_token;
extern int ChunkParticles_token;

static int s_chunkTokenSet = ChunkModel_token | ChunkLight_token |
	ChunkTerrain_token | ChunkFlare_token | ChunkWater_token |
	ChunkTree_token | ChunkParticles_token;

extern int genprop_gizmoviews_token;
static int giz = genprop_gizmoviews_token;

typedef ModuleManager ModuleFactory;

IMPLEMENT_CREATOR(MeModule, Module);

static AutoConfigString s_light_only( "system/lightOnlyEffect" ); 

static bool s_enableScripting = false;
static const float NUM_FRAMES_AVERAGE = 16.f;

MeModule* MeModule::s_instance_ = NULL;

MeModule::MeModule()
	: selectionStart_( Vector2::zero() )
	, currentSelection_( GridRect::zero() )
	, localToWorld_( GridCoord::zero() )
	, averageFps_(0.0f)
	, scriptDict_(NULL)
	, renderingThumbnail_(false)
	, materialPreviewMode_(false)
	, rt_( NULL )
{
	ASSERT(s_instance_ == NULL);
	s_instance_ = this;

	lastCursorPosition_.x = 0;
	lastCursorPosition_.y = 0;
}

MeModule::~MeModule()
{
	ASSERT(s_instance_);
	delete wireframeRenderer_;
	s_instance_ = NULL;
}

bool MeModule::init( DataSectionPtr pSection )
{
	//Let's initialise the shadow caster
	pShadowCommon_ = new ShadowCommon;
	pShadowCommon_->init( Options::pRoot()->openSection("renderer/shadows") );

	//Initialise the shadow caster
	pCaster_ = new ShadowCaster;
	pCaster_->init( pShadowCommon_, false);

	//Register the shadow caster for the flora
	EnviroMinderSettings::instance().shadowCaster( &*pCaster_ );

	//Make a MaterialDraw override for wireframe rendering
	std::string light_only = s_light_only;
	light_only = light_only.substr( 0, light_only.rfind(".") );
	wireframeRenderer_ = new MaterialDrawOverride( light_only, true, true );
	
	return true;
}

void MeModule::onStart()
{
	if (s_enableScripting)
		initPyScript();

	cc_ = new ClosedCaptions();
	Commentary::instance().addView( &*cc_ );
	cc_->visible( true );
}


bool MeModule::initPyScript()
{
	// make a python dictionary here
	PyObject * pScript = PyImport_ImportModule("me_module");

	scriptDict_ = PyModule_GetDict(pScript);

	PyObject * pInit = PyDict_GetItemString( scriptDict_, "init" );
	if (pInit != NULL)
	{
		PyObject * pResult = PyObject_CallFunction( pInit, "" );

		if (pResult != NULL)
		{
			Py_DECREF( pResult );
		}
		else
		{
			PyErr_Print();
			return false;
		}
	}
	else
	{
		PyErr_Print();
		return false;
	}

	return true;
}



int MeModule::onStop()
{
	::ShowCursor( 0 );

	//TODO: Work out why I can't call this here...
	//Save all options when exiting
	//Options::save();

	return 0;
}


bool MeModule::updateState( float dTime )
{
	static float s_lastDTime = 1.f/60.f;
	if (dTime != 0.f)
		dTime = dTime/NUM_FRAMES_AVERAGE + (NUM_FRAMES_AVERAGE-1.f)*s_lastDTime/NUM_FRAMES_AVERAGE;
	s_lastDTime = dTime;

	//Update the commentary
	cc_->update( dTime );
	
	SimpleGUI::instance().update( dTime );

	// update the camera.  this interprets the view direction from the mouse input
	MeApp::instance().camera()->update( dTime, true );

	// tick time and update the other components, such as romp
	MeShell::instance().romp().update( dTime, false );

	// gizmo manager
	POINT cursorPos = CMainFrame::instance().currentCursorPosition();
	Vector3 worldRay = CMainFrame::instance().getWorldRay(cursorPos.x, cursorPos.y);
	ToolPtr spTool = ToolManager::instance().tool();
	if (spTool)
	{
		spTool->calculatePosition( worldRay );
		spTool->update( dTime );
	}
	else if (GizmoManager::instance().update(worldRay))
		GizmoManager::instance().rollOver();

	bool checkForSparkles = !!Options::getOptionInt( "settings/checkForSparkles", 0 );
	bool useTerrain = !checkForSparkles && !!Options::getOptionInt( "settings/useTerrain", 1 );

	// set input focus as appropriate
	bool acceptInput = CMainFrame::instance().cursorOverGraphicsWnd();
	InputDevices::setFocus( acceptInput );
		
	ChunkManager::instance().tick( dTime );

	speedtree::SpeedTreeRenderer::tick( dTime );

	return true;
}

void MeModule::beginRender()
{
	Moo::Camera cam = Moo::rc().camera();
	cam.nearPlane( Options::getOptionFloat( "render/misc/nearPlane", 0.01f ));
	cam.farPlane( Options::getOptionFloat( "render/misc/farPlane", 300.f ));
	Moo::rc().camera( cam );
	
	if (Moo::rc().mixedVertexProcessing())
		Moo::rc().device()->SetSoftwareVertexProcessing( TRUE );

	Moo::rc().nextFrame();
	Moo::rc().reset();
	Moo::rc().updateViewTransforms();
	Moo::rc().updateProjectionMatrix();
}

bool MeModule::renderThumbnail( const std::string& fileName )
{
	std::string modelName = BWResource::resolveFilename( fileName );
	std::replace( modelName.begin(), modelName.end(), '/', '\\' );
	std::string screenShotName = BWResource::removeExtension( modelName );
	screenShotName += ".thumbnail.jpg";
		
	if (!rt_)
	{
		rt_ = new Moo::RenderTarget( "Model Thumbnail" );
		if ((!rt_) || (!rt_->create( 128,128 )))
		{
			WARNING_MSG( "Could not create render target for model thumbnail.\n" );
			delete rt_;
			return false;
		}
	}
	
	if ( rt_->push() )
	{
		//Use the same aspect ratio for the camera as the render target (1:1)
		Moo::Camera cam = Moo::rc().camera();
		cam.aspectRatio( 1.f );
		Moo::rc().camera( cam );

		updateFrame( 0.f );
		
		Moo::rc().beginScene();
		renderingThumbnail_ = true;

		//Make sure the colour channels are enabled for alpha blended models
		Moo::rc().setRenderState( D3DRS_COLORWRITEENABLE, 
			D3DCOLORWRITEENABLE_RED |
			D3DCOLORWRITEENABLE_GREEN |
			D3DCOLORWRITEENABLE_BLUE );

		Moo::rc().device()->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
			RGB( 255, 255, 255), 1, 0 );

		render( 0.f );

		renderingThumbnail_ = false;
		Moo::rc().endScene();

		HRESULT hr = D3DXSaveTextureToFile( screenShotName.c_str(),
			D3DXIFF_JPG, rt_->pTexture(), NULL );

		rt_->pop();

		if ( FAILED( hr ) )
		{
			WARNING_MSG( "Could not create model thumbnail \"%s\" (D3D error 0x%x).\n", screenShotName.c_str(), hr);
			return false;
		}

		PageObject::currPage()->OnUpdateThumbnail();
		
		UalManager::instance()->updateItem( modelName );

		return true;
	}
	
	return false;
}

void MeModule::render( float dTime )
{
	static float s_lastDTime = 1.f/60.f;
	if (dTime != 0.f)
		dTime = dTime/NUM_FRAMES_AVERAGE + (NUM_FRAMES_AVERAGE-1.f)*s_lastDTime/NUM_FRAMES_AVERAGE;
	s_lastDTime = dTime;

	bool groundModel = !!Options::getOptionInt( "settings/groundModel", 0 );
	bool centreModel = !!Options::getOptionInt( "settings/centreModel", 0 );
	bool showLightModels = !!Options::getOptionInt( "settings/showLightModels", 1 );
	bool useCustomLighting = !!Options::getOptionInt( "settings/useCustomLighting", 0 );

	bool checkForSparkles = !!Options::getOptionInt( "settings/checkForSparkles", 0 );

	bool showAxes = !checkForSparkles && !!Options::getOptionInt( "settings/showAxes", 0 );
	bool showBoundingBox = !checkForSparkles && !!Options::getOptionInt( "settings/showBoundingBox", 0 );
	bool useTerrain = !checkForSparkles && !!Options::getOptionInt( "settings/useTerrain", 1 );
	bool useFloor = !checkForSparkles && !useTerrain && !!Options::getOptionInt( "settings/useFloor", 1 );
	
	bool showBsp = !checkForSparkles && !!Options::getOptionInt( "settings/showBsp", 0 );
	bool special = checkForSparkles || showBsp;
	
	bool showModel = special || !!Options::getOptionInt( "settings/showModel", 1 );
	
	bool showWireframe = !special && !!Options::getOptionInt( "settings/showWireframe", 0 );
	bool showSkeleton = !checkForSparkles && !!Options::getOptionInt( "settings/showSkeleton", 0 );
	bool showPortals = !special && !!Options::getOptionInt( "settings/showPortals", 0 );
	bool showShadowing = showModel && !showWireframe && !showBsp && !!Options::getOptionInt( "settings/showShadowing", 1 );
	bool showOriginalAnim = !special && !!Options::getOptionInt( "settings/showOriginalAnim", 0 );

	Vector3 bkgColour = Options::getOptionVector3( "settings/bkgColour", Vector3( 255.f, 255.f, 255.f )) / 255.f;

	float modelDist = 0.f;

	int numTris = 0;

	bool posChanged = true;
	
	if ( MeApp::instance().mutant()->modelName() != "" )
	{
		MeApp::instance().mutant()->groundModel( groundModel );
		MeApp::instance().mutant()->centreModel( centreModel );
		MeApp::instance().mutant()->updateActions( dTime );
		MeApp::instance().mutant()->updateFrameBoundingBox();

		if (!materialPreviewMode_)
		{
			static bool s_last_groundModel = !groundModel;
			static bool s_last_centreModel = !centreModel;
			if ((s_last_groundModel != groundModel) || (s_last_centreModel != centreModel))
			{
				//Do this to ensure the model nodes are up to date
				MeApp::instance().mutant()->drawModel();
				MeApp::instance().camera()->boundingBox(
					MeApp::instance().mutant()->zoomBoundingBox() );
				s_last_groundModel = groundModel;
				s_last_centreModel = centreModel;
				MeApp::instance().camera()->update();
				bool posChanged = true;
			}
		}
	}

	MeApp::instance().camera()->render( dTime );
	
	TextureRenderer::updateDynamics( dTime );
	
	beginRender();

	MeShell::instance().romp().drawPreSceneStuff( checkForSparkles, useTerrain );

	//Do this so that the skeleton is defined if the model is not drawn
	if (((!showModel) || (showBsp)) && showSkeleton)
	{
		modelDist = MeApp::instance().mutant()->drawModel();
	}

	Moo::RenderContext& rc = Moo::rc();
	DX::Device* pDev = rc.device();
	
	rc.device()->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
			Moo::Colour( 
			checkForSparkles ? 1.f : bkgColour[0], 
			checkForSparkles ? 1.f : bkgColour[1], 
			checkForSparkles ? 1.f : bkgColour[2],
			0.f), 1, 0 );

	rc.setVertexShader( NULL );
	rc.setPixelShader( NULL );
	rc.device()->SetPixelShader( NULL );

	rc.setRenderState( D3DRS_CLIPPING, TRUE );
	rc.setRenderState( D3DRS_LIGHTING, FALSE );
	rc.setRenderState( D3DRS_FOGTABLEMODE, D3DFOG_LINEAR );
	rc.setRenderState( D3DRS_FOGCOLOR, 0x00000000 );

	float fogVal = rc.fogNear();
	rc.setRenderState( D3DRS_FOGSTART, *(DWORD*)(&fogVal) );
	fogVal = rc.fogFar();
	rc.setRenderState( D3DRS_FOGEND, *(DWORD*)(&fogVal) );

	static Vector3 N(0.0, 0.0, 0.0);
	static Vector3 X(1.0, 0.0, 0.0); 
	static Vector3 Y(0.0, 1.0, 0.0); 
	static Vector3 Z(0.0, 0.0, 1.0); 
	static Moo::Colour colourRed( 0.5f, 0.f, 0.f, 1.f );
	static Moo::Colour colourGreen( 0.f, 0.5f, 0.f, 1.f );
	static Moo::Colour colourBlue( 0.f, 0.f, 0.5f, 1.f );

	Moo::LightContainerPtr lc = new Moo::LightContainer;
	
	lc->addDirectional( ChunkManager::instance().cameraSpace()->sunLight() );
	lc->ambientColour( ChunkManager::instance().cameraSpace()->ambientLight() );

	rc.lightContainer( lc );
	rc.specularLightContainer( lc );

	// Use the sun direction as the shadowing direction.
	EnviroMinder & enviro = ChunkManager::instance().cameraSpace()->enviro();
	Vector3 lightTrans = enviro.timeOfDay()->lighting().sunTransform[2];

	speedtree::SpeedTreeRenderer::update();

	if ((useCustomLighting) && (MeApp::instance().lights()->lightContainer()->directionals().size()))
	{
		lightTrans = Vector3(0,0,0) - MeApp::instance().lights()->lightContainer()->directional(0)->direction();
	}

	if (useTerrain)
	{
		speedtree::SpeedTreeRenderer::beginFrame( &enviro );
		
		this->renderChunks();

		speedtree::SpeedTreeRenderer::endFrame();

		// chunks don't keep the same light container set
		rc.lightContainer( lc );
		rc.specularLightContainer( lc );

		this->renderTerrain( dTime, showShadowing );

		// draw sky etc.
		MeShell::instance().romp().drawDelayedSceneStuff();
	}

	//Make sure we restore valid texture stages before continuing
	rc.setTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG2 );
	rc.setTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
	rc.setTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
	rc.setTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
	rc.setTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );

	// Draw the axes
	if (showAxes)
	{
		Geometrics::drawLine(N, X, colourGreen);
		Geometrics::drawLine(N, Y, colourBlue);
		Geometrics::drawLine(N, Z, colourRed);
	}

	if (checkForSparkles)
	{
		rc.lightContainer( MeApp::instance().blackLight() );
		rc.specularLightContainer( MeApp::instance().blackLight() );
	}
	else if (useCustomLighting)
	{
		rc.lightContainer( MeApp::instance().lights()->lightContainer() );
		rc.specularLightContainer( MeApp::instance().lights()->lightContainer() );
	}
	else // Game Lighting
	{
		rc.lightContainer( lc );
		rc.specularLightContainer( lc );
	}

	if (useFloor)
	{
		MeApp::instance().floor()->render();
	}

	if (showBsp)
	{
		rc.lightContainer( MeApp::instance().whiteLight() );
		rc.specularLightContainer( MeApp::instance().blackLight() );
	}

	if ((showWireframe) && (!materialPreviewMode_))
	{
		rc.setRenderState( D3DRS_FILLMODE, D3DFILL_WIREFRAME );
		rc.lightContainer( MeApp::instance().blackLight() );
		rc.specularLightContainer( MeApp::instance().blackLight() );
		
		// Use a lightonly renderer in case the material will render nothing (e.g. some alpha shaders)
		Moo::Visual::s_pDrawOverride = wireframeRenderer_; 
	}
	else
	{
		rc.setRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
	}

	if (materialPreviewMode_)
	{
		if (PageMaterials::currPage()->previewObject())
		{
			PageMaterials::currPage()->previewObject()->draw();
			if (showBoundingBox)
			{
				Geometrics::wireBox( PageMaterials::currPage()->previewObject()->boundingBox(), 0x000000ff );
			}
		}
	}
	else if (MeApp::instance().mutant())
	{
		rc.resetPrimitiveCount();
		
		// Render the model
		modelDist = MeApp::instance().mutant()->render( dTime, showModel, showBoundingBox, showSkeleton, showPortals, showBsp );

		numTris = rc.primitiveCount( );
		
		if (showShadowing)
		{
			//TODO: Move the following line so that the watcher can work correctly.
			//Set the shadow quailty
			pShadowCommon_->shadowQuality( Options::getOptionInt( "renderer/shadows/quality", 2 )  );

			// Render to the shadow caster
			pCaster_->begin( MeApp::instance().mutant()->visibililtyBox(), lightTrans );
			MeApp::instance().mutant()->drawModel( false, modelDist );
			pCaster_->end();
		}
	}

	//Make sure we restore these after we are done
	rc.setRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
	Moo::Visual::s_pDrawOverride = NULL;

	//Return to game lighting
	rc.lightContainer( lc );
	rc.specularLightContainer( lc );
		
	// render the other components, such as romp and flora.
	MeShell::instance().romp().drawPostSceneStuff( useTerrain, useTerrain, showShadowing );

	if ((!materialPreviewMode_) && (showShadowing))
	{
		speedtree::SpeedTreeRenderer::beginFrame( &enviro );
		
		// Start the shadow rendering and render the shadow onto the terrain (if required)
		pCaster_->beginReceive( useTerrain );

		speedtree::SpeedTreeRenderer::endFrame();

		//Do self shadowing
		if (MeApp::instance().mutant())
		{
			MeApp::instance().mutant()->render( 0, showModel, false, false, false, false );
		}

		//Shadow onto the floor
		if (useFloor)
		{
			MeApp::instance().floor()->render( &(pShadowCommon_->pReceiverOverride()->pRigidEffect_) );
		}

		pCaster_->endReceive();
	}

	Moo::SortedChannel::draw();

	if (!renderingThumbnail_)
	{	
		if ((showOriginalAnim) && (!materialPreviewMode_) && (MeApp::instance().mutant()))
		{
			Moo::LightContainerPtr lc = rc.lightContainer();

			rc.setRenderState( D3DRS_FILLMODE, D3DFILL_WIREFRAME );
			Moo::Visual::s_pDrawOverride = wireframeRenderer_; 

			rc.lightContainer( MeApp::instance().blackLight() );
			rc.specularLightContainer( MeApp::instance().blackLight() );

			MeApp::instance().mutant()->drawOriginalModel();

			rc.setRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
			Moo::Visual::s_pDrawOverride = NULL;

			rc.lightContainer( lc );
			rc.specularLightContainer( lc );
		}
		
		if (showLightModels)
		{
			// gizmo me
			ToolPtr spTool = ToolManager::instance().tool();
			if (spTool)
			{
				spTool->render();
			}

			GizmoManager::instance().draw();
		}
		
		SimpleGUI::instance().draw();

		char buf[256];
		
		static float s_fpsCount = -1.f;
		s_fpsCount -= dTime;
		if (s_fpsCount < 0.f)
		{
			if (dTime != 0.f)
			{
				sprintf( buf, "%.1f fps", 1.f/dTime );
				s_fpsCount = 1.f;
			}
			else
			{
				strcpy( buf, "0 fps" );
			}
			CMainFrame::instance().setStatusText( ID_INDICATOR_FRAMERATE, buf );
		}

		static int s_lastNumTris = -1;
		if (numTris != s_lastNumTris)
		{
			sprintf( buf, "%d triangles", numTris );
			CMainFrame::instance().setStatusText( ID_INDICATOR_TRIANGLES, buf );
			s_lastNumTris = numTris;
		}
	}

	Chunks_drawCullingHUD();

	// render the camera with the new view direction
	endRender();
}

void MeModule::renderChunks()
{
	Moo::rc().setRenderState( D3DRS_FILLMODE,
		Options::getOptionInt( "render/scenery/wireFrame", 0 ) ?
			D3DFILL_WIREFRAME : D3DFILL_SOLID );

	ChunkManager::instance().camera( Moo::rc().invView(), *&(ChunkManager::instance().cameraSpace()) );	
	ChunkManager::instance().draw();

	Moo::rc().setRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
}

void MeModule::renderTerrain( float dTime /* = 0.f */, bool shadowing /* = false */ )
{
	if (Options::getOptionInt( "render/terrain", 1 ))
	{
		// draw terrain
		bool canSeeTerrain = Moo::TerrainRenderer::instance().canSeeTerrain();

		Moo::rc().setRenderState( D3DRS_FILLMODE,
			Options::getOptionInt( "render/terrain/wireFrame", 0 ) ?
				D3DFILL_WIREFRAME : D3DFILL_SOLID );

		Moo::TerrainRenderer::instance().draw();

		Moo::rc().setRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
	}
	else
	{
		Moo::TerrainRenderer::instance().clear();
	}
}


void MeModule::endRender()
{
	//Moo::rc().endScene();
}


bool MeModule::handleKeyEvent( const KeyEvent & event )
{
	// usually called through py script
	bool handled = MeApp::instance().camera()->handleKeyEvent(event);

	if (event.key() == KeyEvent::KEY_LEFTMOUSE)
	{
		if (event.isKeyDown())
		{
			handled = true;

			GizmoManager::instance().click();
		}
	}

	return handled;
}

bool MeModule::handleMouseEvent( const MouseEvent & event )
{
	return MeApp::instance().camera()->handleMouseEvent(event);
}

Vector2 MeModule::currentGridPos()
{
	POINT pt = MeShell::instance().currentCursorPosition();
	Vector3 cursorPos = Moo::rc().camera().nearPlanePoint(
			(float(pt.x) / Moo::rc().screenWidth()) * 2.f - 1.f,
			1.f - (float(pt.y) / Moo::rc().screenHeight()) * 2.f );

	Matrix view;
	view.setTranslate( viewPosition_ );

	Vector3 worldRay = view.applyVector( cursorPos );
	worldRay.normalise();

	PlaneEq gridPlane( Vector3(0.f, 0.f, 1.f), .0001f );

	Vector3 gridPos = gridPlane.intersectRay( viewPosition_, worldRay );

	return Vector2( gridPos.x, gridPos.y );
}


Vector3 MeModule::gridPosToWorldPos( Vector2 gridPos )
{
	Vector2 w = (gridPos + Vector2( float(localToWorld_.x), float(localToWorld_.y) )) *
		GRID_RESOLUTION;

	return Vector3( w.x, 0, w.y);
}



// ----------------
// Python Interface
// ----------------

/**
 *	The static python render method
 */
PyObject * MeModule::py_render( PyObject * args )
{
	float dTime = 0.033f;

	if (!PyArg_ParseTuple( args, "|f", &dTime ))
	{
		PyErr_SetString( PyExc_TypeError, "ModelEditor.render() "
			"takes only an optional float argument for dtime" );
		return NULL;
	}

	s_instance_->render( dTime );

	Py_Return;
}
