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
#include "romp/water.hpp"

#include "water_scene_renderer.hpp"

#include "moo/light_container.hpp"
#include "moo/render_context.hpp"
#include "moo/terrain_renderer.hpp"

#include "chunk/chunk_space.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_water.hpp"
#include "chunk/chunk_flare.hpp"
#include "chunk/chunk_tree.hpp"
#include "duplo/py_splodge.hpp"
#include "particle/particle_system.hpp"
#include "romp/rain.hpp"
#include "romp/enviro_minder.hpp"

#include "moo/visual_channels.hpp"
#include "moo/visual_compound.hpp"
#include "speedtree/speedtree_renderer.hpp"
#include "ashes/simple_gui_component.hpp"

#ifdef EDITOR_ENABLED
#include "gizmo/tool_manager.hpp"
#endif


DECLARE_DEBUG_COMPONENT2( "App", 0 )

// -----------------------------------------------------------------------------
// Section: Defines
// -----------------------------------------------------------------------------

//#define REFLECTION_FUDGE_FACTOR 0.8f
#define REFLECTION_FUDGE_FACTOR 0.5f
#define REFRACTION_FUDGE_FACTOR 0.5f
#define MAX_CAMERA_DIST 200.f

#pragma warning (disable:4355)	// this used in base member initialiser list

// -----------------------------------------------------------------------------
// Section: statics
// -----------------------------------------------------------------------------

static float reflectionFudge = 0.5f;
static float cullDistance = 300.f;

uint	WaterSceneRenderer::s_textureSize_ = 512;
uint	WaterSceneRenderer::s_maxReflections_ = 10;
uint32	WaterSceneRenderer::reflectionCount_=0;
bool	WaterSceneRenderer::s_drawTrees_ = true;
bool	WaterSceneRenderer::s_drawDynamics_ = true;
bool	WaterSceneRenderer::s_drawPlayer_ = true;
bool	WaterSceneRenderer::s_simpleRefraction_ = false;
bool	WaterSceneRenderer::s_modifyClip_ = false;
bool	WaterSceneRenderer::s_useClipPlane_ = true;
float	WaterSceneRenderer::s_maxReflectionDistance_ = 25.f;
float	WaterSceneRenderer::s_farPlane_ = 0.5f;

PyModel*			WaterSceneRenderer::playerModel_;
WaterScene*			WaterSceneRenderer::currentScene_ = 0;

// -----------------------------------------------------------------------------
// Section: WaterScene
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
WaterScene::WaterScene(float height) :
	drawingReflection_( false ),
	waterHeight_( height ),
	camPos_(0,0,0,0)
{
	refractionScene_ = new WaterSceneRenderer( this, false);		 
	reflectionScene_ = new WaterSceneRenderer( this, true);
}


/**
 *	Destruction
 */
WaterScene::~WaterScene()
{
	TextureRenderer::delStaggeredDynamic( this );

	//TODO: smart pointers...
	delete refractionScene_;
	refractionScene_=NULL;

	delete reflectionScene_;
	reflectionScene_=NULL;
}


/**
 *	Recreate the render targets for this scene.
 */
bool WaterScene::recreate( )
{
	if (refractionScene_->recreate() && reflectionScene_->recreate())
	{
		staggeredDynamic(true);
		return true;		
	}
	return false;
}


/**
 *	Determine if the scene should be drawn.
 */
bool WaterScene::checkOwners() const
{
	OwnerList::const_iterator it = owners_.begin();
	for (;it != owners_.end(); it++)
	{
		if ( (*it) && (*it)->shouldDraw() && 
			(*it)->drawMark() == (Waters::lastDrawMark()) )
		{
			Vector4 camPos = Moo::rc().invView().row(3);
			const Vector3& waterPos = (*it)->position();

			float camDist = (Vector3(camPos.x,camPos.y,camPos.z) - waterPos).length();
			float dist = camDist - (*it)->size().length()*0.5f;
			if (dist <= cullDistance)
				return true;
		}
	}
	return false;
}


/**
 * Retrieve the reflection texture.
 */
Moo::RenderTargetPtr WaterScene::reflectionTexture()
{ 
	return reflectionScene_->getTexture();
}


/**
 * Retrieve the refraction texture.
 */
Moo::RenderTargetPtr WaterScene::refractionTexture()
{
	return refractionScene_->getTexture();
}


/**
 * Test a point is close to the water.
 */
bool WaterScene::closeToWater( const Vector3& pos )
{
	bool closeToWater=false;
	OwnerList::iterator it = owners_.begin();
	for (;it != owners_.end() && closeToWater==false; it++)
	{
		const Vector3& p = (*it)->position();
		float dist = (p - pos).length() - (*it)->size().length()*0.5f;
		
		closeToWater = (dist<WaterSceneRenderer::s_maxReflectionDistance_);
	}
	return closeToWater;
}


/**
 * Check if the given model should be drawn in the water scene
 */
bool WaterScene::shouldReflect( const Vector3& pos, PyModel* model )
{
	if (drawingReflection_)
		return reflectionScene_->shouldReflect(pos, model) && closeToWater(pos);
	else
		return refractionScene_->shouldReflect(pos, model) && closeToWater(pos);
}


/**
 * Check the water surfaces are drawing
 */
bool WaterScene::shouldDraw() const
{
	return checkOwners();
}

/**
 * Render the water scene textures, refraction first then reflection.
 */
void WaterScene::render( float dTime )
{
	drawingReflection_=false;
	if (Waters::drawRefraction())
		refractionScene_->render(dTime);

	drawingReflection_=true;
	if (Waters::drawReflection())
		reflectionScene_->render(dTime);
}


#ifdef EDITOR_ENABLED
/**
 * See if any of the water surfaces are using the read only red mode.
 */
bool WaterScene::drawRed()
{
	OwnerList::const_iterator it = owners_.begin();
	for (;it != owners_.end(); it++)
	{
		if ((*it)->drawRed())
			return true;
	}
	return false;
}
#endif //EDITOR_ENABLED


/**
 * Tell the texture renderer management to treat this as a full dynamic
 */
void WaterScene::dynamic( bool state )
{
	dynamic_ = state;
	if (state)
		TextureRenderer::addDynamic( this );
	else
		TextureRenderer::delDynamic( this );
}


/**
 * Tell the texture renderer management to treat this as a staggered dynamic
 */
void WaterScene::staggeredDynamic( bool state )
{
	dynamic_ = state;
	if (state)
		TextureRenderer::addStaggeredDynamic( this );
	else
		TextureRenderer::delStaggeredDynamic( this );
}


// -----------------------------------------------------------------------------
// Section: WaterSceneRenderer
// -----------------------------------------------------------------------------


/**
 *	Constructor.
 */
WaterSceneRenderer::WaterSceneRenderer( WaterScene* scene, bool reflect  ) :	
	TextureRenderer( s_textureSize_, s_textureSize_ ),
	scene_( scene ),
	reflection_( reflect )
{
	//we don't want the texture renderer to clear our render target.
	clearTargetEachFrame_ = false;		

	static bool firstTime = true;

	s_useClipPlane_ = true;

	if (firstTime)
	{
		MF_WATCH( "Client Settings/Water/Draw Trees",				s_drawTrees_,
		 		Watcher::WT_READ_WRITE,
		 		"Draw trees in water scenes?" );
		MF_WATCH( "Client Settings/Water/Draw Dynamics",			s_drawDynamics_, 
				Watcher::WT_READ_WRITE,
				"Draw dynamic objects in water scenes?" );
		MF_WATCH( "Client Settings/Water/Draw Player",				s_drawPlayer_, 
				Watcher::WT_READ_WRITE, 
				"Draw the players reflection/refraction?" );
		MF_WATCH( "Client Settings/Water/Simple refraction",		s_simpleRefraction_, 
				Watcher::WT_READ_WRITE,
				"Use the simplified water refraction scene?" );		
		MF_WATCH( "Client Settings/Water/Max Reflections",			s_maxReflections_,
				Watcher::WT_READ_WRITE,
				"Maximum number of dynamic objects to reflect/refract" );
		MF_WATCH( "Client Settings/Water/Max Reflection Distance",	s_maxReflectionDistance_,
				Watcher::WT_READ_WRITE,
				"Maximum distance a dynamic object will reflect/refract" );
		MF_WATCH( "Client Settings/Water/Scene/Scene Clip Modification",	s_modifyClip_,
				Watcher::WT_READ_WRITE,
				"Use the clip plane modifications on the water scenes?" );
		MF_WATCH( "Client Settings/Water/Scene/Use Clip Plane",				s_useClipPlane_,
				Watcher::WT_READ_WRITE,
				"Clip the water reflection/refraction along the water plane?" );
		MF_WATCH( "Client Settings/Water/Scene/Scene Far Plane Ratio",			s_farPlane_,
				Watcher::WT_READ_WRITE,
				"Defines the ratio for far plane to use in the water scenes" );
		MF_WATCH( "Client Settings/Water/Scene/Reflection Fudge",			reflectionFudge,
				Watcher::WT_READ_WRITE,
				"Fudge factor for the clip planes of reflection" );	
		MF_WATCH( "Client Settings/Water/Scene/Cull Distance",				cullDistance,
				Watcher::WT_READ_WRITE,
				"Distance where a water scene will stop being updated" );

		firstTime=false;
	}

	pRT_ = new Moo::RenderTarget( "WaterSceneTarget" );	
}

/**
 *	Destructor.
 */
WaterSceneRenderer::~WaterSceneRenderer()
{
	pRT_=NULL;
	//TextureRenderer::delStaggeredDynamic( this );
}


/**
 *	Recreate the render targets for this scene.
 */
bool WaterSceneRenderer::recreate( )
{
	bool success = false;
	pRT_->clearOnRecreate( true, Moo::Colour(1,1,1,1) );
	//success = pRT_->create( size, size, false , D3DFMT_A8R8G8B8, NULL, D3DFMT_D16 );
	success = pRT_->create( s_textureSize_, s_textureSize_, false , D3DFMT_R5G6B5, NULL, D3DFMT_D16 );	

	//if (success)
	//{
	//	//dynamic(true);
	//	staggeredDynamic(true);
	//	return true;		
	//}
	return success;
}

/**
 *	Determine if a certain model should be rendered.
 */
//TODO: only render objects close to the water AND close to the camera??
//TODO: optmize more...
bool WaterSceneRenderer::shouldReflect( const Vector3& pos, PyModel* model )
{ 
	bool shouldReflect;
	if (playerModel() == model) //TODO: this will still draw all instances of the player model..fix!!!
		shouldReflect = s_drawPlayer_;
	else
		shouldReflect = (reflectionCount_ < s_maxReflections_) && s_drawDynamics_;	

	Vector4 camPos = Moo::rc().invView().row(3);
	float camDist = (Vector3(camPos.x,camPos.y,camPos.z) - pos).length();
	shouldReflect = shouldReflect && (camDist < MAX_CAMERA_DIST);

	return (shouldReflect);
}


/**
 *	Determine if the water scene should be drawn.
 */
bool WaterSceneRenderer::shouldDraw() const
{
	return (reflection_ || Waters::drawRefraction());
}


/**
 *	TODO:
 */
//void WaterSceneRenderer::resetDrawState()
//{
//
//}


/**
 *	Draw ourselves
 */
void WaterSceneRenderer::renderSelf( float dTime )
{
	if ((!ChunkManager::instance().cameraSpace()) )
		return;

#if EDITOR_ENABLED
	if (scene()->drawRed())
	{
		Moo::rc().device()->Clear(
			0, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_TARGET, 0x0, 1.f, 0 );
		return;
	}
#endif

	static DogWatch w3( "WaterScene" );
	w3.start();

	bool saveDyn,saveTree;
	if (s_simpleRefraction_ && !reflection_)
	{
		saveTree =		s_drawTrees_;
		saveDyn =		s_drawDynamics_;
		s_drawTrees_ = false;
		s_drawDynamics_ = false;
	}

	//first, setup some rendering options.
	//we need to draw as cheaply as possible, and
	//not clobber the main pipeline.
	ChunkWater::simpleDraw( true );
	ChunkFlare::ignore( true );
	PySplodge::s_ignoreSplodge_ = true;
	Rain::disable( true );
	//Moo::Visual::disableBatching( true );
	//Moo::VisualCompound::disable( true );

	Moo::Visual::disableBatching( false );
	Moo::VisualCompound::disable( false );

	//Moo::VisualChannel::enabled( false );

	//ParticleSystem::active( false );

	//TODO: turn on low LOD	
	//TODO: turn off attachments? or base them on their owner?
	//TODO: turn off specular under water.

	scene()->setCameraPos(Moo::rc().invView().row(3));
	bool flipScene = reflection_;
	bool underWater = (scene()->cameraPos().y < waterHeight());
	//TODO: when under water, nothing else really needs to be rendered...........

	Moo::rc().reflectionScene(true);
	currentScene(this->scene());

	Moo::SortedChannel::push();
	//save some stuff before we stuff around ( haha )
	Matrix invView = Moo::rc().invView();
	Moo::LightContainerPtr pLC = Moo::rc().lightContainer();
	
	D3DXPLANE clipPlaneOffset, clipPlaneActual;
	if (flipScene)
	{
		// flip the scene
		// 1   0   0   0
		// 0   1   0   0
		// 0   0  -1  2h
		// 0   0   0   1
		if (underWater)
		{
			clipPlaneOffset = D3DXPLANE(0.f, -1.f, 0.f, (waterHeight() + reflectionFudge)); //including error fudgery factor.			

			clipPlaneActual = D3DXPLANE(0.f, -1.f, 0.f, (waterHeight() + 0.1f)); //including error fudgery factor.
		}
		else
		{
			clipPlaneOffset = D3DXPLANE(0.f, 1.f, 0.f, -(waterHeight() - reflectionFudge)); //including error fudgery factor.
			clipPlaneActual = D3DXPLANE(0.f, 1.f, 0.f, -(waterHeight()-0.1f)); //including error fudgery factor.
		}

		Matrix refMatrix;
		refMatrix.setTranslate(0.f, waterHeight()*2.f, 0.f); //water height*2		
		refMatrix._22 = -1.0f; // flip the scene

		Matrix view = Moo::rc().view();
		view.preMultiply(refMatrix);
		
		Moo::rc().view( view );
		Moo::rc().mirroredTransform(true);

	}
	else
	{
		if (underWater)
		{
			clipPlaneOffset = D3DXPLANE(0.f, 1.f, 0.f, -waterHeight() + REFRACTION_FUDGE_FACTOR); //including error fudgery factor.
			clipPlaneActual = D3DXPLANE(0.f, 1.f, 0.f, -waterHeight() + 0.1f); //including error fudgery factor.
		}
		else
		{
			clipPlaneOffset = D3DXPLANE(0.f, -1.f, 0.f, waterHeight() + REFRACTION_FUDGE_FACTOR); //including error fudgery factor.
			clipPlaneActual = D3DXPLANE(0.f, -1.f, 0.f, waterHeight() + 0.1f); //including error fudgery factor.
		}
	}
	Matrix world = Moo::rc().world();
	Moo::rc().world( Matrix::identity );

	DX::Viewport oldViewport;
	Moo::rc().device()->GetViewport( &oldViewport );

	float oldFOV = Moo::rc().camera().fov();
	//float oldNear = Moo::rc().camera().nearPlane();
	float oldFar = Moo::rc().camera().farPlane();
	float oldFogFar = Moo::rc().fogFar();

	if (s_modifyClip_)
	{
		//Careful with changing these values: can produce some nasty clip artifacts in the water surface
		Moo::rc().camera().farPlane( min(s_farPlane_ * oldFar, oldFar) );

		//change the fog too
		Moo::rc().fogFar( min(s_farPlane_ * oldFar, oldFogFar) );
	}

	float fov = oldFOV;
	DX::Viewport newViewport = oldViewport;
	if (reflection_)
	{

		newViewport.Width = width_;

		Moo::rc().device()->SetViewport( &newViewport );

		Moo::rc().screenWidth( width_ );
		Moo::rc().screenHeight( height_ - 2 );
	}
	else
	{
		newViewport.Width = width_;

		Moo::rc().device()->SetViewport( &newViewport );

		Moo::rc().screenWidth( width_ );
		Moo::rc().screenHeight( height_ );
	}
	Moo::rc().updateProjectionMatrix();
	Moo::rc().updateViewTransforms();

	D3DXPLANE transformedClipPlaneOffset, transformedClipPlaneActual;

	// clip plane
	if (s_useClipPlane_)
	{
		//TODO: optimise
		Matrix worldViewProjectionMatrix = Moo::rc().viewProjection();
		worldViewProjectionMatrix.preMultiply( world );

		Matrix worldViewProjectionInverseTransposeMatrix;
		D3DXMatrixInverse((D3DXMATRIX*)&worldViewProjectionInverseTransposeMatrix, NULL, (D3DXMATRIX*)&worldViewProjectionMatrix);
		D3DXMatrixTranspose((D3DXMATRIX*)&worldViewProjectionInverseTransposeMatrix,(D3DXMATRIX*)&worldViewProjectionInverseTransposeMatrix);		
		
		D3DXPlaneTransform(&transformedClipPlaneOffset, &clipPlaneOffset, (D3DXMATRIX*)&worldViewProjectionInverseTransposeMatrix);
		D3DXPlaneTransform(&transformedClipPlaneActual, &clipPlaneActual, (D3DXMATRIX*)&worldViewProjectionInverseTransposeMatrix);

		Moo::rc().device()->SetClipPlane(0, transformedClipPlaneActual);
		Moo::rc().setRenderState(D3DRS_CLIPPLANEENABLE, 1);
	}

	//draw the scene
#ifdef EDITOR_ENABLED
	Moo::rc().device()->Clear(
		0, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_TARGET, 0x0, 1.f, 0 );
#else
	Moo::rc().device()->Clear(
		0, NULL, D3DCLEAR_ZBUFFER, 0x0, 1.f, 0 );
#endif

	ChunkManager::instance().camera( Moo::rc().invView(),
		ChunkManager::instance().cameraSpace() );

	//if (underWater)
	//{
		// and some fog stuff
	//	Moo::rc().fogNear( 0 );
	//	Moo::rc().fogFar( 500 );
	//	Moo::rc().setRenderState( D3DRS_FOGCOLOR, 0x102030 );
	//	Moo::Material::standardFogColour( 0x102030 );
		//rc.setRenderState( D3DRS_FOGENABLE, FALSE );
		//pEffect->SetFloat( constantHandle, fogStart_ ? rc().fogNear() : rc().fogFar() );
		//FogController
	//}

	
	bool drawTrees = speedtree::SpeedTreeRenderer::drawTrees(s_drawTrees_);
	//float oldMode = speedtree::SpeedTreeRenderer::lodMode(0.5f);
	
	clearReflectionCount();
	ChunkManager::instance().draw();

	Moo::VisualCompound::drawAll();
	Moo::Visual::drawBatches();
	
	Moo::rc().lightContainer( ChunkManager::instance().cameraSpace()->lights() );

	if (s_useClipPlane_)
		Moo::rc().device()->SetClipPlane(0, transformedClipPlaneOffset);

	Moo::TerrainRenderer::instance().draw();

//	Moo::VisualCompound::drawAll();
//	Moo::Visual::drawBatches();

	ChunkSpacePtr pCameraSpace = ChunkManager::instance().cameraSpace();
	if (pCameraSpace)
	{
		pCameraSpace->enviro().drawHindDelayed( dTime );		
	}

#if EDITOR_ENABLED
	// draw tools
	ToolPtr spTool = ToolManager::instance().tool();
	if ( spTool )
	{
		spTool->render();
	}
#endif

	
	// Now drawing the particles systems and alpha blended objects:		
	Moo::SortedChannel::draw();

	Moo::SortedChannel::pop();


	//set the stuff back
	Moo::rc().lightContainer( pLC );
	Moo::rc().device()->SetViewport( &oldViewport );
	Moo::rc().screenWidth( oldViewport.Width );
	Moo::rc().screenHeight( oldViewport.Height );
	
	//restore drawing states!
	ChunkWater::simpleDraw( false );
	ChunkFlare::ignore( false );
	PySplodge::s_ignoreSplodge_ = false;
	Rain::disable( false );
//	Moo::VisualChannel::enabled(true);
	Moo::VisualCompound::disable( false );
	Moo::Visual::disableBatching( false );

	//always needs to be changed?
	if (s_modifyClip_)
	{
		Moo::rc().camera().farPlane( oldFar );
		Moo::rc().fogFar( oldFogFar );
	}

	Moo::rc().camera().fov( oldFOV );
	//Moo::rc().camera().nearPlane( oldNear );
	//Moo::rc().camera().farPlane( oldFar );
	Moo::rc().updateProjectionMatrix();

	ChunkManager::instance().camera( invView,
		ChunkManager::instance().cameraSpace() );

	//speedtree::SpeedTreeRenderer::lodMode(oldMode);
	speedtree::SpeedTreeRenderer::drawTrees(drawTrees);

	Moo::rc().updateProjectionMatrix();

	if (s_simpleRefraction_ && !reflection_)
	{
		s_drawTrees_ =		saveTree;
		s_drawDynamics_ =	saveDyn;
		//s_drawPlayer_ =		savePlayer;
	}

	if (s_useClipPlane_)
		Moo::rc().setRenderState(D3DRS_CLIPPLANEENABLE, 0);
	
	Moo::rc().mirroredTransform(false);
	Moo::rc().reflectionScene(false);
//	ParticleSystem::active(true);

	w3.stop();
}
