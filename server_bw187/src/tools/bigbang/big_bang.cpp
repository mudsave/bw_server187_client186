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
#include "big_bang.hpp"
#include <psapi.h>
#pragma comment( lib, "psapi.lib" )
#include "appmgr/options.hpp"
#include "appmgr/commentary.hpp"
#include "chunks/editor_chunk.hpp"
#include "pyscript/py_data_section.hpp"
#include "pyscript/py_output_writer.hpp"
#include "moo/terrain_renderer.hpp"
#include "moo/visual_channels.hpp"
#include "math/sma.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk_obstacle.hpp"
#include "chunk/chunk_loader.hpp"
#include "chunk/chunk_item_tree_node.hpp"
#include "chunk/chunk_vlo.hpp"
#include "resmgr/auto_config.hpp"
#include "romp/engine_statistics.hpp"
#include "romp/console_manager.hpp"
#include "romp/flora.hpp"
#include "romp/time_of_day.hpp"
#include "romp/fog_controller.hpp"
#include "romp/lens_effect_manager.hpp"
#include "romp/z_buffer_occluder.hpp"
#include "romp/debug_geometry.hpp"
#include "romp/flora.hpp"
#include "gizmo/gizmo_manager.hpp"
#include "gizmo/tool_manager.hpp"
#include "chunk_overlapper.hpp"
#include "common/mouse_look_camera.hpp"
#include "common/utilities.hpp"
#include "snaps.hpp"
#include "static_lighting.hpp"
#include "selection_filter.hpp"
#include "cvswrapper.hpp"
#include <fstream>
#include <sstream>
#include <time.h>
#include "big_bang_camera.hpp"
#include "page_scene.hpp"
#include "aux_space_files.hpp"
#include "editor_chunk_entity.hpp"
#include "editor_chunk_tree.hpp"
#include "bigbang.h"
#include "mainframe.hpp"
#include "page_terrain_import.hpp"
#include "project/space_map.hpp"
#include "project/space_helpers.hpp"
#include "project/project_module.hpp"
#include "height/height_map.hpp"
#include "height/height_module.hpp"
#include "common/material_properties.hpp"
#include "appmgr/application_input.hpp"
#include "foldersetter.hpp"
#include "guimanager/gui_manager.hpp"
#include "guimanager/gui_input_handler.hpp"
#include "guimanager/gui_functor_option.hpp"
#include "speedtree/speedtree_renderer.hpp"
#include "new_space_dlg.h"
#include "bigbangdoc.hpp"
#include "controls/message_box.hpp"
#include "splash_dialog.hpp"
#include "wait_dialog.hpp"
#include "panel_manager.hpp"
#include "common/page_messages.hpp"
#include "item_frustum_locator.hpp"
#include "romp/water.hpp"
#include "geh/geh.hpp"
#include "compile_time.hpp"
#include "low_memory_dlg.hpp"
#include <algorithm>

#include "romp/texture_renderer.hpp"
#include "romp/water_scene_renderer.hpp"

static DogWatch s_chunkTick( "chunk_tick" );
static DogWatch s_chunkDraw( "chunk_draw" );
static DogWatch s_terrainDraw( "terrain_draw" );
static DogWatch s_rompDraw( "romp_draw" );
static DogWatch s_drawSorted( "draw_sorted" );
static DogWatch s_render( "render" );
static DogWatch s_update( "update" );
static DogWatch s_detailTick( "detail_tick" );
static DogWatch s_detailDraw( "detail_draw" );

//for ChunkManager
std::string g_specialConsoleString;

static AutoConfigString s_terrainSelectionFx( "selectionfx/terrain" );
//--------------------------------
//CHUNKIN EXTERNS
// reference the ChunkInhabitants that we want to be able to load.
extern int ChunkModel_token;
extern int ChunkLight_token;
extern int ChunkTerrain_token;
extern int ChunkFlare_token;
extern int ChunkWater_token;
extern int ChunkOverlapper_token;
extern int ChunkParticles_token;
extern int ChunkTree_token;
extern int PyPatrolPath_token;
static int s_chunkTokenSet = ChunkModel_token | ChunkLight_token |
	ChunkTerrain_token | ChunkFlare_token | ChunkWater_token |
	ChunkOverlapper_token | ChunkParticles_token |
	ChunkTree_token | PyPatrolPath_token;

extern int ScriptedModule_token;
static int s_moduleTokenSet = ScriptedModule_token;

extern int genprop_gizmoviews_token;
static int giz = genprop_gizmoviews_token;


static Moo::EffectMaterial * selectionMaterial = NULL;


DECLARE_DEBUG_COMPONENT2( "BigBang", 2 )

SmartPointer<BigBang> BigBang::s_instance = NULL;
BigBangDebugMessageCallback BigBang::debugMessageCallback_;


BigBang::BigBang()
	: inited_( false )
	, workingChunk_( NULL )
	, updating_( false )
	, chunkManagerInited_( false )
    , dTime_( 0.1f )
	, romp_( NULL )
    , globalWeather_( false )
    , totalTime_( 0.f )
    , hwndInput_( NULL )
    , hwndGraphics_( NULL )
    , changedEnvironment_( false )
    , secsPerHour_( 0.f )
	, switchTime_( 0 )
	, currentMonitoredChunk_( 0 )
	, currentPrimGroupCount_( 0 )
	, angleSnaps_( 0.f )
	, movementSnaps_( 0.f, 0.f, 0.f )
	, canSeeTerrain_( false )
	, mapping_( NULL )
	, spaceManager_( NULL )
	, drawSelection_( false )
	, renderDisabled_( false )
	, conn_( NULL )
	, GUI::ActionMaker<BigBang>( "changeSpace", &BigBang::changeSpace )
	, GUI::ActionMaker<BigBang, 1>( "newSpace", &BigBang::newSpace )
	, GUI::ActionMaker<BigBang, 2>( "recentSpace", &BigBang::recentSpace )
	, GUI::ActionMaker<BigBang, 3>( "touchAllChunk", &BigBang::touchAllChunk )
	, GUI::ActionMaker<BigBang, 4>( "clearUndoRedoHistory", &BigBang::clearUndoRedoHistory )
	, GUI::ActionMaker<BigBang, 5>( "doExternalEditor", &BigBang::doExternalEditor )
	, GUI::ActionMaker<BigBang, 6>( "doReloadAllTextures", &BigBang::doReloadAllTextures )
	, GUI::ActionMaker<BigBang, 7>( "doReloadAllChunks", &BigBang::doReloadAllChunks )
	, GUI::ActionMaker<BigBang, 8>( "doExit", &BigBang::doExit )
	, GUI::UpdaterMaker<BigBang>( "updateUndo", &BigBang::updateUndo )
	, GUI::UpdaterMaker<BigBang, 1>( "updateRedo", &BigBang::updateRedo )
	, GUI::UpdaterMaker<BigBang, 2>( "updateExternalEditor", &BigBang::updateExternalEditor )
	, killingUpdatingFiber_( false )
	, updatingFiber_( 0 )
	, lastModifyTime_( 0 )
	, cursor_( NULL )
    , waitCursor_( true )
	, warningOnLowMemory_( true )
	, settingSelection_( false )
{
	runtimeInitMaterialProperties();
	setPlayerPreviewMode( false );
	resetCursor();
}


BigBang::~BigBang()
{
	if (inited_) this->fini();

	std::vector<PhotonOccluder*>::iterator it = occluders_.begin();
	std::vector<PhotonOccluder*>::iterator end = occluders_.end();

	while ( it != end )
	{
		delete *it++;
	}
	delete spaceManager_;
}


void BigBang::fini()
{
	if ( inited_ )
	{
		DEBUG_MSG( "Calling BigBang Destructor\n" );
		mapping_ = NULL;
		ChunkManager::instance().fini();
		inited_ = false;
	}

	if ( romp_ )
	{
		PyObject * pMod = PyImport_AddModule( "BigBang" );
		PyObject_DelAttrString( pMod, "romp" );
		PyObject_DelAttrString( pMod, "opts" );

		Py_DECREF( romp_ );
		romp_ = NULL;
	}

	while( ToolManager::instance().tool() )
	{
		WARNING_MSG( "BigBang::fini : There is still a tool on the stack that should have been cleaned up\n" );
		ToolManager::instance().popTool();
	}

	EditorEntityType::shutdown();
	ChunkItemTreeNode::nodeCache().fini();
	EditorChunkTree::fini();
	if( conn_ )
	{
		conn_->disconnect();
		delete conn_;
		conn_ = 0;
	}

	if (selectionMaterial)
	{
		delete selectionMaterial;
		selectionMaterial = NULL;
	}

	Moo::TerrainRenderer::fini();

	s_instance = NULL;
}


BigBang& BigBang::instance()
{
	if ( !s_instance )
	{
		s_instance = new BigBang;
	}
	return *s_instance;
}

#include "moo/effect_material.hpp"
#include "common/material_editor.hpp"

namespace
{

struct isChunkFileExists
{
	bool operator()( const std::string& filename, ChunkDirMapping* dirMapping )
	{
		return BWResource::fileExists( dirMapping->path() + filename + ".chunk" );
	}
}
isChunkFileExists;

};

void BigBang::update( float dTime )
{
	if ( !inited_ )
		return;

	if( updating_ )
		return;
	updating_ = true;

	if( !ChunkManager::instance().busy() && dirtyLightingChunks_.empty() && dirtyTerrainShadowChunks_.empty() )
	{
		std::string chunkToLoad;
		if( !nonloadedDirtyLightingChunks_.empty() )
		{
			if( isChunkFileExists( *nonloadedDirtyLightingChunks_.begin(), chunkDirMapping() ) )
				chunkToLoad = *nonloadedDirtyLightingChunks_.begin();
			else
				nonloadedDirtyLightingChunks_.erase( nonloadedDirtyLightingChunks_.begin() );
		}
		else if( !nonloadedDirtyTerrainShadowChunks_.empty() )
		{
			if( isChunkFileExists( *nonloadedDirtyTerrainShadowChunks_.begin(), chunkDirMapping() ) )
				chunkToLoad = *nonloadedDirtyTerrainShadowChunks_.begin();
			else
				nonloadedDirtyTerrainShadowChunks_.erase( nonloadedDirtyTerrainShadowChunks_.begin() );
		}
		if( !chunkToLoad.empty() )
			ChunkManager::instance().loadChunkExplicitly( chunkToLoad, chunkDirMapping() );
	}

	static bool s_testMaterialEdit = false;
	if (s_testMaterialEdit)
	{
		Moo::EffectMaterial * m = new Moo::EffectMaterial();
		DataSectionPtr pSection = BWResource::openSection( "sets/testing/glove.mfm" );
		m->load( pSection );
		SmartPointer<MaterialEditor> pME( new MaterialEditor(m), true );
		pME = NULL;
		s_testMaterialEdit = false;
	}

	s_update.start();

	dTime_ = dTime;
    totalTime_ += dTime;

	g_specialConsoleString = "";

	postPendingErrorMessages();

	// set input focus as appropriate
	bool acceptInput = cursorOverGraphicsWnd();
	InputDevices::setFocus( acceptInput );

	//GIZMOS
	if ( InputDevices::isShiftDown() ||
		InputDevices::isCtrlDown() ||
		InputDevices::isAltDown() )
	{
		// if pressing modifier keys, remove the forced gizmo set to enable
		// normal gizmo behaviour with the modifier keys.
		GizmoManager::instance().forceGizmoSet( NULL );
	}

	//TOOLS
	// calculate the current world ray from the mouse position
	// (don't do this if moving the camera around (for more response)
	bool castRay = !InputDevices::isKeyDown(KeyEvent::KEY_RIGHTMOUSE);
	if ( acceptInput && castRay )
	{
		worldRay_ = getWorldRay( currentCursorPosition() );

		ToolPtr spTool = ToolManager::instance().tool();
		if ( spTool )
		{
			spTool->calculatePosition( worldRay_ );
			spTool->update( dTime );
		}
	}

	//CHUNKS
	if ( chunkManagerInited_ )
    {
        s_chunkTick.start();
		markChunks();
		ChunkManager::instance().tick( dTime_ );
		s_chunkTick.stop();
    }

	// recalc lighting
	if (Options::getOptionInt( "enableDynamicUpdating", 0 ) == 1 && GetTickCount() - lastModifyTime_ > 1000 )
		doBackgroundUpdating();

	//ENTITY MODELS
	EditorChunkEntity::calculateDirtyModels();

    if ( romp_ )
    	romp_->update( dTime_, globalWeather_ );

    // update the flora redraw state
	bool drawFlora = !!Options::getOptionInt( "render/environment/drawDetailObjects", 1 );
	drawFlora &= !!Options::getOptionInt( "render/environment", 0 );
	drawFlora &= !Options::getOptionInt( "render/hideOutsideObjects", 0 );
	Flora::enabled(drawFlora);

	static bool firstTime = true;
	static bool canUndo, canRedo, canEE;
	static bool playerPreviewMode;
	static std::string cameraMode;
	static int terrainWireFrame;
	if( firstTime || canUndo != UndoRedo::instance().canUndo() ||
		canRedo != UndoRedo::instance().canRedo() ||
		canEE != !!updateExternalEditor( NULL ) ||
		playerPreviewMode != isInPlayerPreviewMode() ||
		cameraMode != Options::getOptionString( "camera/speed" ) ||
		terrainWireFrame != Options::getOptionInt( "render/terrain/wireFrame", 0 ) )
	{
		firstTime = false;
		canUndo = UndoRedo::instance().canUndo();
		canRedo = UndoRedo::instance().canRedo();
		canEE = !!updateExternalEditor( NULL );
		cameraMode = Options::getOptionString( "camera/speed" );
		playerPreviewMode = isInPlayerPreviewMode();
		terrainWireFrame = Options::getOptionInt( "render/terrain/wireFrame", 0 );
		GUI::Manager::instance()->update();
	}

    s_update.stop();

	if( warningOnLowMemory_ )
	{
		if( (int)getMemoryLoad() > Options::getOptionInt( "warningMemoryLoadLevel", 90 ) )
		{
			if( LowMemoryDlg().DoModal() == IDC_SAVE )
			{
				UndoRedo::instance().clear();
				quickSave();

				unloadChunks();
			}
			else
				warningOnLowMemory_ = false;
		}
	}
	updating_ = false;
}

/**
 *	This method writes out some status panel sections that are done every frame.
 *	i.e. FPS and cursor location.
 */
void BigBang::writeStatus()
{
	char buf[1024];

	//Panel 0 - memory load
	sprintf( buf, "Memory Load: %u%%", getMemoryLoad() );
	BigBang::instance().setStatusMessage( 0, buf );

	//Panel 1 - num polys
	sprintf( buf, "%d tris.", Moo::rc().primitiveCount() );
	BigBang::instance().setStatusMessage( 1, buf );

	//Panel 2 - snaps
	if ( this->snapsEnabled() )
	{
		Vector3 snaps = this->movementSnaps();
		DistanceFormatter &fmt = DistanceFormatter::s_def;

		std::string strs[3];
		strs[0] = fmt.format( snaps.x ).c_str();
		strs[1] = this->terrainSnapsEnabled() ? "T" : fmt.format( snaps.y ).c_str();
		strs[2] = fmt.format( snaps.z ).c_str();

		sprintf( buf, "%s, %s, %s", strs[0].c_str(), strs[1].c_str(), strs[2].c_str() );
		BigBang::instance().setStatusMessage( 2, buf );
	}
	else
	{
		if ( this->terrainSnapsEnabled() )
		{
			BigBang::instance().setStatusMessage( 2, "0cm., T, 0cm." );
		}
		else
		{
			BigBang::instance().setStatusMessage( 2, "snaps free." );
		}
	}

	//Panel 3 - locator position
	if ( ToolManager::instance().tool() && ToolManager::instance().tool()->locator() )
	{
		Vector3 pos = ToolManager::instance().tool()->locator()->transform().applyToOrigin();
		Chunk* chunk = ChunkManager::instance().cameraSpace()->findChunkFromPoint( pos );

		if (chunk)
		{
			std::vector<DataSectionPtr>	modelSects;
			EditorChunkCache::instance( *chunk ).pChunkSection()->openSections( "model", modelSects );

			sprintf( buf, "%0.2f, %0.2f, %0.2f %s m: %d pg: %d",
				pos.x, pos.y, pos.z,
				chunk->identifier().c_str(),
				(int) modelSects.size(),
				currentPrimGroupCount_
				);
		}
		else
		{
			sprintf( buf, "%0.2f, %0.2f, %0.2f", pos.x, pos.y, pos.z );
		}

		BigBang::instance().setStatusMessage( 3, buf );
	}
	else
	{
		BigBang::instance().setStatusMessage( 3, "" );
	}

	//Panel5 - fps

	//7 period simple moving average of the frames per second
	static SMA<float>	averageFPS(7);
	static float		countDown = 1.f;

	float fps = dTime_ == 0.f ? 0.f : 1.f / dTime_;
	averageFPS.append( fps );

	if ( countDown < 0.f )
	{
		sprintf( buf, "%0.1f fps.", averageFPS.average() );
		BigBang::instance().setStatusMessage( 4, buf );
		countDown = 1.f;
	}
	else
	{
		countDown -= dTime_;
	}

	// Panel 6 - number of chunks loaded
	EditorChunkCache::lock();

	int dirtyTotal = dirtyChunks();
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

	BigBang::instance().setStatusMessage( 5, buf );

}

/**
 *	This method renders the scene in a standard way.
 *	Call this method, or call each other method individually,
 *	interspersed with your own custom routines.
 */
void BigBang::render( float dTime )
{
	if( renderDisabled_ )
		return;
	if ( !inited_ )
	    return;

	if( farPlane() != Options::getOptionInt( "graphics/farclip", (int)farPlane() ) )
		farPlane( (float)Options::getOptionInt( "graphics/farclip" ) );

	EditorChunkItem::hideAllOutside( !!Options::getOptionInt( "render/hideOutsideObjects", 0 ) );

	// Setup the data for counting the amount of primitive groups in the chunk
	// the locator is in, used for the status bar
	currentMonitoredChunk_ = 0;
	currentPrimGroupCount_ = 0;
	if ( ToolManager::instance().tool() && ToolManager::instance().tool()->locator() )
	{
		Vector3 pos = ToolManager::instance().tool()->locator()->transform().applyToOrigin();
		currentMonitoredChunk_ = ChunkManager::instance().cameraSpace()->findChunkFromPoint( pos );
	}

	// update any dynamic textures
	TextureRenderer::updateDynamics( dTime_ );
	//or just the water??

	this->beginRender();
	this->renderRompPreScene();

	if ( chunkManagerInited_ )
	{
		this->renderChunks();

		Moo::LightContainerPtr lc = new Moo::LightContainer;

		lc->addDirectional(
			ChunkManager::instance().cameraSpace()->sunLight() );
		lc->ambientColour(
			ChunkManager::instance().cameraSpace()->ambientLight() );

		Moo::rc().lightContainer( lc );
	}

	this->renderTerrain( dTime );
	this->renderRompDelayedScene();
	this->renderRompPostScene();
	Moo::rc().setRenderState( D3DRS_CLIPPING, TRUE  );
	this->renderEditorGizmos();
	this->renderDebugGizmos();
	GeometryDebugMarker::instance().draw();
	GizmoManager::instance().draw();

	if (Options::getOptionBool( "drawSpecialConsole", false ) &&
		!g_specialConsoleString.empty())
	{
		static XConsole * pSpecCon = new XConsole();
		pSpecCon->clear();
		pSpecCon->setCursor( 0, 0 );
		pSpecCon->print( g_specialConsoleString );
		pSpecCon->draw( 0.1f );
	}

	Chunks_drawCullingHUD();
	this->endRender();

	// write status sections.
	// we write them here, because it is only here
	// that we can retrieve the poly count.
	writeStatus();

    // if no chunks are loaded then show the arrow + 
    showBusyCursor();
}


/**
 *	Note : this method assumes Moo::rc().view() has been set accordingly.
 *		   it is up to the caller to set up this matrix.
 */
void BigBang::beginRender()
{
    s_render.start();

	bool useShadows = Moo::rc().stencilAvailable();

	if ( useShadows )
	{
		Moo::rc().device()->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER |
			D3DCLEAR_STENCIL, 0x000020, 1, 0 );
	}
	else
	{
		Moo::rc().device()->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
			0x000020, 1, 0 );
	}

	Moo::rc().reset();
	Moo::rc().updateViewTransforms();
	Moo::rc().updateProjectionMatrix();
}


void BigBang::renderRompPreScene()
{

    // draw romp pre scene
    s_rompDraw.start();
    romp_->drawPreSceneStuff();
    s_rompDraw.stop();

	FogController::instance().commitFogToDevice();
}

void BigBang::renderChunks()
{
	// draw chunks
	if (chunkManagerInited_)
    {
    	s_chunkDraw.start();

		Moo::rc().setRenderState( D3DRS_FILLMODE,
			Options::getOptionInt( "render/scenery/wireFrame", 0 ) ?
				D3DFILL_WIREFRAME : D3DFILL_SOLID );

		ChunkManager::instance().camera( Moo::rc().invView(), ChunkManager::instance().cameraSpace() );
		Chunk::hideIndoorChunks_ = (Options::getOptionInt("render/shells")==0);
	    ChunkManager::instance().draw();

		// render overlapping chunks
		speedtree::SpeedTreeRenderer::beginFrame( &romp_->enviroMinder() );
		std::vector<Chunk*>::iterator ci = ChunkOverlapper::drawList.begin();
		Chunk * cc = ChunkManager::instance().cameraChunk();
		if (cc)
		{
			for (; ci != ChunkOverlapper::drawList.end(); ++ci)
			{
				Chunk* c = *ci;
				if ( !c->online() )
				{
					//TODO : this shoudln't happen, chunks should get out
					// of the drawList when they are offline.
					DEBUG_MSG( "BigBang::renderChunks: Trying to draw chunk %s while it's offline!\n", c->resourceID().c_str() );
					continue;
				}
				if (c->drawMark() != cc->drawMark())
					c->drawSelf();
			}
			bool save = ChunkManager::s_enableChunkCulling;
			ChunkManager::s_enableChunkCulling = false;
			for( std::vector<ChunkItemPtr>::iterator iter = selectedItems_.begin();
				iter != selectedItems_.end(); ++iter )
			{
				ChunkItemPtr ci = *iter;
				if( ci->chunk()->drawMark() != cc->drawMark() )
					ci->chunk()->drawSelf();
			}
			ChunkManager::s_enableChunkCulling = save;
		}
		ChunkOverlapper::drawList.clear();

		if( cc )
		{
			for( std::vector<ChunkItemPtr>::iterator iter = selectedItems_.begin();
				iter != selectedItems_.end(); ++iter )
			{
				Chunk* c = (*iter)->chunk();
				if (c && c->drawMark() != cc->drawMark())
						c->drawSelf();
			}
		}

		// inside chunks will not render if they are not reacheable through portals. 
		// If game visibility is off, the overlappers are used to render not-connected 
		// chunks. But, with the visibility bounding box culling, the overlapper may 
		// not be rendered, causing the stray shell to be invisible, even if it is 
		// itself inside the camera frustum. To fix this situation, when game visibility 
		// is turned off, after rendering the chunks, it goes through all loaded chunks, 
		// trying to render those that are inside and haven't been rendered for this frame. 
		// Visibility bounding box culling still applies.
		if (cc != NULL && Options::getOptionInt("render/shells/gameVisibility", 1) == 0)
		{			
			ChunkSpacePtr space = ChunkManager::instance().cameraSpace();
			if (space.exists())
			{
				for (ChunkMap::iterator it = space->chunks().begin();
					it != space->chunks().end();
					it++)
				{
					for (uint i = 0; i < it->second.size(); i++)
					{
						if (it->second[i] != NULL                       &&
							!it->second[i]->isOutsideChunk()            &&
							it->second[i]->drawMark() != cc->drawMark() &&
							it->second[i]->online())
						{
							it->second[i]->drawSelf();
						}
					}
				}
			}
		}

		speedtree::SpeedTreeRenderer::endFrame();
		Moo::rc().setRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );

        s_chunkDraw.stop();
    }
}


void BigBang::renderTerrain( float dTime )
{	
	if (selectionMaterial == NULL)
	{
		selectionMaterial = new Moo::EffectMaterial();
		selectionMaterial->initFromEffect( s_terrainSelectionFx );
	}
	if (Options::getOptionInt( "render/terrain", 1 ))
	{
		// draw terrain
		s_terrainDraw.start();

		canSeeTerrain_ = Moo::TerrainRenderer::instance().canSeeTerrain();

		Moo::rc().setRenderState( D3DRS_FILLMODE,
			Options::getOptionInt( "render/terrain/wireFrame", 0 ) ?
				D3DFILL_WIREFRAME : D3DFILL_SOLID );

		if( drawSelection() )
			Moo::TerrainRenderer::instance().draw( selectionMaterial );
		else
			Moo::TerrainRenderer::instance().draw();


		if (!readOnlyTerrainBlocks_.empty())
		{
			AVectorNoDestructor< BlockInPlace >::iterator i = readOnlyTerrainBlocks_.begin();
			for (; i != readOnlyTerrainBlocks_.end(); ++i)
				Moo::TerrainRenderer::instance().addBlock( i->first, &*(i->second) );

			setReadOnlyFog();

			if( drawSelection() )
				Moo::TerrainRenderer::instance().draw( selectionMaterial );
			else
				Moo::TerrainRenderer::instance().draw();

			readOnlyTerrainBlocks_.clear();

			FogController::instance().commitFogToDevice();
		}


		s_terrainDraw.stop();

		Moo::rc().setRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
	}
	else
	{
		canSeeTerrain_ = false;
		Moo::TerrainRenderer::instance().clear();
	}
}


void BigBang::renderEditorGizmos()
{
	// draw tools
	ToolPtr spTool = ToolManager::instance().tool();
	if ( spTool )
	{
		spTool->render();
	}
}


void BigBang::renderDebugGizmos()
{
}


void BigBang::renderRompDelayedScene()
{
	// draw romp delayed scene
    s_rompDraw.start();
    romp_->drawDelayedSceneStuff();
    s_rompDraw.stop();
}


void BigBang::renderRompPostScene()
{
    // draw romp post scene
    s_rompDraw.start();
    romp_->drawPostSceneStuff();
    s_rompDraw.stop();
}


void BigBang::endRender()
{
    s_render.stop();
}

bool BigBang::init( HINSTANCE hInst, HWND hwndInput, HWND hwndGraphics )
{
	if ( !inited_ )
    {
		GUI::Win32InputDevice::install();
		class BigBangCriticalErrorHandler : public CriticalErrorHandler
		{
			virtual void recordInfo( bool willExit )
			{
				writeDebugFiles( NULL, willExit );
			}
		};

		enableFeedBack( Options::getOptionInt( "sendCrashDump", 1 ) != 0 );
        hwndInput_ = hwndInput;
        hwndGraphics_ = hwndGraphics;

	    ::ShowCursor( true );

		//init python data sections
		PyObject * pMod = PyImport_AddModule( "BigBang" );	// borrowed
		PyObject_SetAttrString( pMod, "opts", SmartPointer<PyObject>(
			new PyDataSection( Options::pRoot() ), true ).getObject() );

		// create the editor entities descriptions
		// this cannot be called from within the load thread
		// as python and load load thread hate each other
		EditorEntityType::startup();

		// Init GUI Manager
		// Backwards compatibility for options.xml without this option.
		// Otherwise all buttons light up
		Options::setOptionInt( "render/chunk/vizMode", 
			Options::getOptionInt( "render/chunk/vizMode", 0 ));
		GUI::OptionFunctor::instance()->setOption( this );
		DataSectionPtr guiRoot = BWResource::openSection( "resources/data/gui.xml" );
		if( guiRoot )
			for( int i = 0; i < guiRoot->countChildren(); ++i )
				GUI::Manager::instance()->add( new GUI::Item( guiRoot->openChild( i ) ) );

		// Init chunk manager
		ChunkManager::instance().init();

		class BigBangMRUProvider : public MRUProvider
		{
			virtual void set( const std::string& name, const std::string& value )
			{
				Options::setOptionString( name, value );
			}
			virtual const std::string get( const std::string& name ) const
			{
				return Options::getOptionString( name );
			}
		};
		static BigBangMRUProvider BigBangMRUProvider;
		spaceManager_ = new SpaceManager( BigBangMRUProvider );

		while( spaceManager_->num() )
			if( changeSpace( spaceManager_->entry( 0 ), false ) )
				break;

		if( ! spaceManager_->num() )
		{
			CSplashDlg::HideSplashScreen();
			for(;;)
			{
				MainFrame* mainFrame = (MainFrame *)BigBang2App::instance().mainWnd();
				MsgBox mb( "Open space",
					"BigBang cannot find a valid default space to open, do you want to open "
					"one, create one or exit bigbang?",
					"&Open", "&Create", "E&xit" );
				int result = mb.doModal( mainFrame->m_hWnd );
				if( result == 0 )
				{
					if( changeSpace( GUI::ItemPtr() ) )
						break;
				}
				else if( result == 1 )
				{
					if( newSpace( GUI::ItemPtr() ) )
						break;
				}
				else
					ExitProcess( 0 );//sorry
			}
		}

/*		ChunkSpacePtr space = new ChunkSpace(1);
		std::string spacePath = Options::getOptionString( "space/mru0" );
		mapping_ = space->addMapping( SpaceEntryID(), Matrix::identity, spacePath );
		if (!mapping_)
		{
			CRITICAL_MSG( "Couldn't load %s as a space.\n\n"
						"Please run SpaceChooser to select a valid space.\n",
						spacePath.c_str() );
			return false;
		}
		ChunkManager::instance().camera( Matrix::identity, space );
		// Call tick to give it a chance to load the outdoor seed chunk, before
		// we ask it to load the dirty chunks
		ChunkManager::instance().tick( 0.f );

		// We don't want our space going anywhere
		space->incRef();*/

		chunkManagerInited_ = true;

		if (ZBufferOccluder::isAvailable())
			occluders_.push_back( new ZBufferOccluder );
		else
			occluders_.push_back( new ChunkObstacleOccluder );
		LensEffectManager::instance().addPhotonOccluder( occluders_.back() );

		if ( !initRomp() )
		{
			return false;
		}

		// start up the camera
		// Inform the camera of the window handle
		bigBangCamera_ = new BigBangCamera;

		inited_ = true;

		//watchers
		MF_WATCH( "Client Settings/Far Plane",
		*this,
		MF_ACCESSORS( float, BigBang, farPlane) );

		MF_WATCH( "Render/Draw Portals", ChunkBoundary::Portal::drawPortals_ );

		// set the saved far plane
		float fp = Options::getOptionFloat( "graphics/farclip", 500 );
		farPlane( fp );

		// Use us to provide the snap settings for moving objects etc
		SnapProvider::instance( this );
		CoordModeProvider::ins() = this;

		ApplicationInput::disableModeSwitch();
	}

	return true;
}
BWLock::BigBangdConnection*& BigBang::connection()
{
	if( conn_ && !conn_->connected() )
	{
		CWaitCursor wait;

		// show wait message
		std::string msg = "Connecting to BigWorld lock server " + conn_->host() + "...";

		if( conn_ )
		{
			// Try to connect. This call will block if it can't reach the server
			conn_->connect();

			if( conn_->connected() )
			{
				WaitDlg::show( msg + "done" );
			}
			else
			{
				WaitDlg::show( msg + "failed" );
				INFO_MSG( "Unable to connect to bwlockd\n" );
				BigBang::instance().addCommentaryMsg( "Unable to connect to bwlockd", Commentary::WARNING );
				delete conn_;
				conn_ = NULL;
			}
		}
		// hide delayed
		WaitDlg::hide( 1000 );
	}
	return conn_;
}

bool BigBang::postLoadThreadInit()
{
	if (1)
	{
		// create the fibers
		mainFiber_ = ::ConvertThreadToFiber( NULL );
		MF_ASSERT( mainFiber_ );
		updatingFiber_ = ::CreateFiber( 1024*1024, BigBang::backgroundUpdateLoop, this );
		MF_ASSERT( updatingFiber_ );

		::SetThreadPriority(
			ChunkManager::instance().chunkLoader()->thread()->handle(),
			THREAD_PRIORITY_ABOVE_NORMAL );

		// if we have any chunks wait for the chunk loader to get started first,
		// so that we don't reorient its bootstrapping routine
		if (!nonloadedDirtyLightingChunks_.empty() ||
			!nonloadedDirtyTerrainShadowChunks_.empty() ||
			!nonloadedDirtyThumbnailChunks_.empty() )
		{
			ChunkManager & cm = ChunkManager::instance();
			while (!cm.cameraChunk())
			{
				BgTaskManager::instance()->tick();
				cm.camera( Moo::rc().invView(), cm.cameraSpace() );
				markChunks();
				cm.tick( 0 );
				Sleep( 50 );
			}
		}
    }

    return true;
}

ChunkDirMapping* BigBang::chunkDirMapping()
{
	return mapping_;
}

bool BigBang::initRomp()
{
	if ( !romp_ )
	{
		romp_ = new RompHarness;

		// set it into the BigBang module
		PyObject * pMod = PyImport_AddModule( "BigBang" );	// borrowed
		PyObject_SetAttrString( pMod, "romp", romp_ );

		if ( !romp_->init() )
		{
			return false;
		}
		
		romp_->enviroMinder().activate();
		BigBang::instance().timeOfDay()->gameTime(
			float(
				Options::getOptionInt(
					"graphics/timeofday",
					12 * TIME_OF_DAY_MULTIPLIER /*noon*/ )
			) / float( TIME_OF_DAY_MULTIPLIER )
		);
	}
	return true;
}


void BigBang::focus( bool state )
{
    InputDevices::setFocus( state );
}


void BigBang::timeOfDay( float t )
{
	if ( romp_ )
    	romp_->setTime( t );
}

void BigBang::rainAmount( float a )
{
	if ( romp_ )
    	romp_->setRainAmount( a );
}


void BigBang::propensity( const std::string& weatherSystemName, float amount )
{
	if ( romp_ )
    {
    	romp_->propensity( weatherSystemName, amount );
    }
}

bool BigBang::cursorOverGraphicsWnd() const
{
	HWND fore = ::GetForegroundWindow();
	if ( fore != hwndInput_ && ::GetParent( fore ) != hwndInput_ )
		return false; // foreground window is not the main window nor a floating panel.

	RECT rt;
	::GetWindowRect( hwndGraphics_, &rt );
	POINT pt;
	::GetCursorPos( &pt );

	if ( pt.x < rt.left ||
		pt.x > rt.right ||
		pt.y < rt.top ||
		pt.y > rt.bottom )
	{
		return false;
	}

	HWND hwnd = ::WindowFromPoint( pt );
	if ( hwnd != hwndGraphics_ )
		return false; // it's a floating panel, return.
	HWND parent = hwnd;
	while( ::GetParent( parent ) != NULL )
		parent = ::GetParent( parent );
	::SendMessage( hwnd, WM_MOUSEACTIVATE, (WPARAM)parent, WM_LBUTTONDOWN * 65536 + HTCLIENT );
	return true;
}

POINT BigBang::currentCursorPosition() const
{
	POINT pt;
	::GetCursorPos( &pt );
	::ScreenToClient( hwndGraphics_, &pt );

	return pt;
}

Vector3 BigBang::getWorldRay(POINT& pt) const
{
	return getWorldRay( pt.x, pt.y );
}

Vector3 BigBang::getWorldRay(int x, int y) const
{
	Vector3 v = Moo::rc().invView().applyVector(
		Moo::rc().camera().nearPlanePoint(
			(float(x) / Moo::rc().screenWidth()) * 2.f - 1.f,
			1.f - (float(y) / Moo::rc().screenHeight()) * 2.f ) );
	v.normalise();

	return v;
}

/*
void BigBang::pushTool( ToolPtr tool )
{
	tools_.push( tool );
}


void BigBang::popTool()
{
	if ( !tools_.empty() )
		tools_.pop();
}


ToolPtr BigBang::tool()
{
	if ( !tools_.empty() )
		return tools_.top();

	return NULL;
}
*/


void BigBang::addCommentaryMsg( const std::string& msg, int id )
{
	Commentary::instance().addMsg( msg, id );
}


void BigBang::addError(Chunk* chunk, ChunkItem* item, const char * format, ...)
{
	va_list argPtr;
	va_start( argPtr, format );

	char buffer[1000];
	vsprintf(buffer, format, argPtr);

	// add to the gui errors pane
	if( &MsgHandler::instance() )
		MsgHandler::instance().addAssetErrorMessage( buffer, chunk, item );

	// add to the view comments
	addCommentaryMsg( buffer, Commentary::CRITICAL );
}

/**
 *	This is for things that want to mark chunks as changed but don't want to
 *	include big_bang.hpp, ie, EditorChunkItem
 */
void changedChunk( Chunk * pChunk )
{
	BigBang::instance().changedChunk( pChunk );
}

bool chunkWritable( Chunk * pChunk )
{
	return EditorChunkCache::instance( *pChunk ).edIsWriteable();
}

void BigBang::changedChunk( Chunk * pChunk, bool rebuildNavmesh /*= true*/  )
{
	MF_ASSERT( pChunk );
	MF_ASSERT( pChunk->loading() || pChunk->loaded() );

	if (!EditorChunkCache::instance( *pChunk ).edIsLocked())
		return;

	changedChunks_.insert( pChunk );
	//Any chunk that is changed for whatever reason now has a dirty thumbnail.
	//Thus nobody needs to call "dirtyThumbnail" explicitly : since changedChunk
	//is called for any such chunk.
	dirtyThumbnail( pChunk );
	
	if ( rebuildNavmesh )
	{
		// something changed, so mark it's navigation mesh dirty.
		EditorChunkCache::instance( *pChunk ).navmeshDirty( true );
	}
}

void BigBang::changedChunkLighting( Chunk* pChunk )
{
	MF_ASSERT( pChunk );

	if( workingChunk_ == pChunk )
		workingChunk_ = NULL;

	// Don't calc for outside chunks
	if (pChunk->isOutsideChunk())
		return;

	if (!EditorChunkCache::instance( *pChunk ).edIsLocked())
		return;

	// Ensure that it's only in the list once, and it's always at the end
	std::vector<Chunk*>::iterator pos = std::find( dirtyLightingChunks_.begin(),
		dirtyLightingChunks_.end(),
		pChunk );

	if (pos != dirtyLightingChunks_.end())
		dirtyLightingChunks_.erase( pos );

	if( EditorChunkCache::instance( *pChunk ).lightingUpdated() )
	{
		EditorChunkCache::instance( *pChunk ).lightingUpdated( false );
		changedChunks_.insert( pChunk );
	}
	dirtyLightingChunks_.push_back( pChunk );
	lastModifyTime_ = GetTickCount();
}


void BigBang::changedTerrainShadows( Chunk * pChunk )
{
	MF_ASSERT( pChunk->isOutsideChunk() );

	if( workingChunk_ == pChunk )
		workingChunk_ = NULL;

	if (!EditorChunkCache::instance( *pChunk ).edIsLocked())
		return;

	std::vector<Chunk*>::iterator pos = std::find( dirtyTerrainShadowChunks_.begin(),
		dirtyTerrainShadowChunks_.end(),
		pChunk );

	if ( pos == dirtyTerrainShadowChunks_.end() )
		dirtyTerrainShadowChunks_.push_back( pChunk );
	if( EditorChunkCache::instance( *pChunk ).shadowUpdated() )
	{
		EditorChunkCache::instance( *pChunk ).shadowUpdated( false );
		changedChunks_.insert( pChunk );
	}
}

/*
 *	This function add all unloaded chunk that refer the vlo to out dirtyVLO list
 */
void BigBang::dirtyVLO( const std::string& type, const std::string& id, const BoundingBox& bb )
{
	for( float x = bb.minBounds().x; x <= bb.maxBounds().x; x += GRID_RESOLUTION )
	{
		for( float z = bb.minBounds().z; z <= bb.maxBounds().z; z += GRID_RESOLUTION )
		{
			Chunk* chunk = ChunkManager::instance().cameraSpace()->findChunkFromPoint(
				Vector3( x, 0.f, z ) );
			if( chunk )
			{
				if( !chunk->loaded() )
				{
					dirtyVloReferences_[ chunk->identifier() ].insert(
						std::make_pair( type, id ) );
				}
			}
			else
			{
				std::string identifier = mapping_->outsideChunkIdentifier( Vector3( x, 0.f, z ) );
				if( !identifier.empty() )
					dirtyVloReferences_[ identifier ].insert( std::make_pair( type, id ) );
			}
		}
	}
}

/*
 *	This method checks if a vlo in a chunk is dirty
 *	@param	chunk	the Chunk contains the VLO
 *	@param	id	uid of the VLO
 *	@return	true if it is dirty, otherwise false
 */
bool BigBang::isDirtyVlo( Chunk* chunk, const std::string& id )
{
	DirtyVLOReferences::iterator iter = dirtyVloReferences_.find( chunk->identifier() );
	if( iter != dirtyVloReferences_.end() )
	{
		const std::set< std::pair<std::string, std::string> >& vlos =
			iter->second;
		for( std::set< std::pair<std::string, std::string> >::const_iterator iter = vlos.begin();
			iter != vlos.end(); ++iter )
		{
			if( iter->second == id )
				return true;
		}
	}
	return false;
}

void BigBang::markTerrainShadowsDirty( Chunk * pChunk )
{
	MF_ASSERT( pChunk );

	if (!pChunk->isOutsideChunk())
		return;

	if (!EditorChunkCache::instance( *pChunk ).edIsWriteable())
		return;

	//DEBUG_MSG( "marking terrain dirty in chunk %s\n", pChunk->identifier().c_str() );

	// ok, now add every chunk within MAX_TERRAIN_SHADOW_RANGE metres along
	// the x axis of pChunk

	changedTerrainShadows( pChunk );

	// shadows were directly changed in this chunk, which means that an item's
	// bounding box is overlapping this chunk, so mark it's navmesh dirty
	EditorChunkCache::instance( *pChunk ).navmeshDirty( true );

	// do 100, -100, 200, -200, etc, so the chunks closest to what just got
	// changed get recalced 1st
	for (float xpos = GRID_RESOLUTION;
		xpos < (MAX_TERRAIN_SHADOW_RANGE + 1.f);
		xpos = (xpos < 0.f ? GRID_RESOLUTION : 0.f) + (xpos * -1.f))
	{
		Vector3 pos = pChunk->centre() + Vector3( xpos, 0.f, 0.f );
		ChunkSpace::Column* col = ChunkManager::instance().cameraSpace()->column( pos, false );
		if (!col)
			continue;

		Chunk* c = col->pOutsideChunk();
		if (!c)
			continue;

		changedTerrainShadows( c );
	}

	lastModifyTime_ = GetTickCount();
}

void BigBang::markTerrainShadowsDirty( const BoundingBox& bb )
{
	// Find all the chunks bb is in; we know it will be < 100m in the x & z
	// directions, however

	Vector3 a(bb.minBounds().x, 0.f, bb.minBounds().z);
	Vector3 b(bb.minBounds().x, 0.f, bb.maxBounds().z);
	Vector3 c(bb.maxBounds().x, 0.f, bb.maxBounds().z);
	Vector3 d(bb.maxBounds().x, 0.f, bb.minBounds().z);

	// Add chunks from the four corners
	// This is a bit dodgy, should be asking the column for the outside chunk
	std::set<Chunk*> chunks;
	chunks.insert( ChunkManager::instance().cameraSpace()->findChunkFromPoint( a ) );
	chunks.insert( ChunkManager::instance().cameraSpace()->findChunkFromPoint( b ) );
	chunks.insert( ChunkManager::instance().cameraSpace()->findChunkFromPoint( c ) );
	chunks.insert( ChunkManager::instance().cameraSpace()->findChunkFromPoint( d ) );

	// Remove the null chunk, if that got added
	chunks.erase((Chunk*) 0);

	for (std::set<Chunk*>::iterator i = chunks.begin(); i != chunks.end(); ++i)
		markTerrainShadowsDirty( *i );
	lastModifyTime_ = GetTickCount();
}

void BigBang::dirtyThumbnail( Chunk * pChunk )
{
	if( EditorChunkCache::instance( *pChunk ).thumbnailUpdated() )
	{
		EditorChunkCache::instance( *pChunk ).thumbnailUpdated( false );
		changedChunks_.insert( pChunk );
	}
	if( std::find( dirtyThumbnailChunks_.begin(), dirtyThumbnailChunks_.end(),
		pChunk ) == dirtyThumbnailChunks_.end() )
		dirtyThumbnailChunks_.push_back( pChunk );
	SpaceMap::instance().dirtyThumbnail( pChunk );
    HeightMap::instance().dirtyThumbnail( pChunk );
	lastModifyTime_ = GetTickCount();
}

void BigBang::resetChangedLists()
{
	dirtyLightingChunks_.clear();
	changedChunks_.clear();
	changedTerrainBlocks_.clear();
	changedThumbnailChunks_.clear();
	dirtyTerrainShadowChunks_.clear();
	dirtyThumbnailChunks_.clear();
}

bool BigBang::isDirtyLightChunk( Chunk * pChunk )
{
	return std::find( dirtyLightingChunks_.begin(), dirtyLightingChunks_.end(), pChunk )
		!= dirtyLightingChunks_.end();
}

void BigBang::doBackgroundUpdating()
{
	// Go to the update fiber, and do some processing
	switchTime_ = timestamp();
	SwitchToFiber( updatingFiber_ );
}

/*
	If we've spent > 30ms in the lighting fiber, switch back to the main one

	I came up with the 30ms value after a lil bit of playing, it's a decent
	comprimise between efficency & interactivity
*/
bool BigBang::fiberPause()
{
	// If we're not in the lighting fibre, don't bother checking
	if (switchTime_ == 0)
		return false;


	uint64 now = timestamp();

	double dTime = ((int64)(now - switchTime_)) / stampsPerSecondD();

	if (dTime > 0.03)
	{
		switchTime_ = 0;
		SwitchToFiber( mainFiber_ );
		return true;
	}
	else
	{
		return false;
	}
}

void BigBang::stopBackgroundCalculation()
{
	if( updatingFiber_ )
	{
		killingUpdatingFiber_ = true;
		while( killingUpdatingFiber_ )
			doBackgroundUpdating();
	}
}

/*
	Never ending function required to run the background updating fiber.

	Just recalculates lighting, terrain shadows, etc as needed forever
*/
void BigBang::backgroundUpdateLoop( PVOID param )
{
	for(;;)
	{
		if(!((BigBang*)param)->killingUpdatingFiber_)
		{
			// Calculate static lighting
			std::vector<Chunk*>& lightingChunks = BigBang::instance().dirtyLightingChunks_;

			if( !lightingChunks.empty() && !((BigBang*)param)->killingUpdatingFiber_ )
			{
				Vector3 cameraPosition = Moo::rc().invView().applyToOrigin();
				Chunk* nearestChunk = NULL;
				float distance = 99999999.f;
				for( std::vector<Chunk*>::iterator iter = lightingChunks.begin();
					iter != lightingChunks.end(); ++iter )
				{
					if( ( (*iter)->centre() - cameraPosition ).lengthSquared() < distance &&
						(*iter)->online() )
					{
						distance = ( (*iter)->centre() - cameraPosition ).lengthSquared();
						nearestChunk = (*iter);
					}
				}
				if( nearestChunk &&
					((BigBang*)param)->EnsureNeighbourChunkLoaded( nearestChunk, StaticLighting::STATIC_LIGHT_PORTAL_DEPTH ) &&
					!EditorChunkCache::instance( *nearestChunk ).edIsDeleted() &&
					( BigBang::instance().workingChunk( nearestChunk ), EditorChunkCache::instance( *nearestChunk ).edRecalculateLighting() ) )
				{
					if( BigBang::instance().isWorkingChunk( nearestChunk ) )
						for( std::vector<Chunk*>::iterator iter = lightingChunks.begin();
							iter != lightingChunks.end(); ++iter )
						{
							if( (*iter) == nearestChunk )
							{
								lightingChunks.erase( iter );
								break;
							}
						}
					BigBang::instance().workingChunk( NULL );
					BigBang::instance().fiberPause();
				}
			}

			((BigBang*)param)->killingUpdatingFiber_ = false;
			((BigBang*)param)->switchTime_ = 0;
			SwitchToFiber( ((BigBang*)param)->mainFiber_ );

			// Calculate terrain shadows
			std::vector<Chunk*>& terrainChunks = BigBang::instance().dirtyTerrainShadowChunks_;

			if( !terrainChunks.empty() && !((BigBang*)param)->killingUpdatingFiber_ )
			{
				Vector3 cameraPosition = Moo::rc().invView().applyToOrigin();
				Chunk* nearestChunk = NULL;
				float distance = 99999999.f;
				std::vector<Chunk*>::iterator maxIter;
				for( std::vector<Chunk*>::iterator iter = terrainChunks.begin();
					iter != terrainChunks.end(); ++iter )
				{
					if( ( (*iter)->centre() - cameraPosition ).lengthSquared() < distance &&
						(*iter)->online() )
					{
						distance = ( (*iter)->centre() - cameraPosition ).lengthSquared();
						nearestChunk = (*iter);
						maxIter = iter;
					}
				}
				BigBang::instance().workingChunk( NULL );
				if( nearestChunk &&
					((BigBang*)param)->EnsureNeighbourChunkLoadedForShadow( nearestChunk ) &&
					!EditorChunkCache::instance( *nearestChunk ).edIsDeleted() )
				{
					if( static_cast<EditorChunkTerrain*>(ChunkTerrainCache::instance( *nearestChunk ).pTerrain() ) )
					{
						EditorChunkTerrain* pEct = static_cast<EditorChunkTerrain*>(
							ChunkTerrainCache::instance( *nearestChunk ).pTerrain());
						MF_ASSERT( pEct );
						BigBang::instance().workingChunk( nearestChunk );
						pEct->calculateShadows();
					}
					if( BigBang::instance().isWorkingChunk( nearestChunk ) )
						for( std::vector<Chunk*>::iterator iter = terrainChunks.begin();
							iter != terrainChunks.end(); ++iter )
						{
							if( (*iter) == nearestChunk )
							{
								terrainChunks.erase( iter );
								break;
							}
						}
					BigBang::instance().workingChunk( NULL );
					BigBang::instance().fiberPause();
				}
			}
		}
		((BigBang*)param)->killingUpdatingFiber_ = false;
		((BigBang*)param)->switchTime_ = 0;
		SwitchToFiber( ((BigBang*)param)->mainFiber_ );
	}
}

void BigBang::loadChunkForThumbnail( const std::string& chunkName )
{
	if( isChunkFileExists( chunkName, chunkDirMapping() ) )
	{
		ChunkPtr chunk = ChunkManager::instance().findChunkByName( chunkName, chunkDirMapping() );
		if( chunk && !chunk->online() )
			ChunkManager::instance().loadChunkExplicitly( chunkName, chunkDirMapping() );
		// the following implementation is much slower
		// in case there is any error in the upper one, we can revert to it
/*		if( chunk->online() )
		{
			EditorChunkCache::instance( *chunk ).calculateThumbnail();
			SpaceMap::instance().swapInTextures( chunk );
			std::vector<Chunk*>::iterator iter =
				std::find( dirtyThumbnailChunks_.begin(), dirtyThumbnailChunks_.end(), chunk );
			if( iter != dirtyThumbnailChunks_.end() )
				dirtyThumbnailChunks_.erase( iter );
		}
		else
		{
			killingUpdatingFiber_ = true;
			while( killingUpdatingFiber_ )
				doBackgroundUpdating();
			ChunkOverlapper::drawList.clear();
			ChunkManager::instance().switchToSyncMode( true );
			ChunkManager::instance().loadChunkNow( chunk );
			ChunkManager::instance().checkLoadingChunks();
			ChunkManager::instance().switchToSyncMode( false );
			ChunkOverlapper::drawList.clear();
		}*/
	}
}

/** Write a file to disk and (optionally) add it to cvs */
bool BigBang::saveAndAddChunkBase( const std::string & resourceID,
	const SaveableObjectBase & saver, bool add, bool addAsBinary )
{
	// add it before saving for if it has been cvs removed but not committed
	if (add)
	{
		CVSWrapper( currentSpace_ ).addFile( resourceID + ".cdata", addAsBinary );
		CVSWrapper( currentSpace_ ).addFile( resourceID + ".chunk", addAsBinary );
	}

	// save it out
	bool result = saver.save( resourceID );

	// add it again for if it has been ordinarily (re-)created
	if (add)
	{
		CVSWrapper( currentSpace_ ).addFile( resourceID + ".cdata", addAsBinary );
		CVSWrapper( currentSpace_ ).addFile( resourceID + ".chunk", addAsBinary );
	}

	return result;
}

/** Delete a file from disk and (eventually) remove it from cvs */
void BigBang::eraseAndRemoveFile( const std::string & resourceID )
{
	std::string fileName = BWResource::resolveFilename( resourceID );
	std::string backupFileName;
	if( fileName.size() > strlen( "i.chunk" ) &&
		strcmp( fileName.c_str() + fileName.size() - strlen( "i.chunk" ), "i.chunk" ) == 0 )
		backupFileName = fileName.substr( 0, fileName.size() - strlen( "i.chunk" ) ) + "i.~chunk~";
	if( !backupFileName.empty() && !BWResource::fileExists( backupFileName ) )
		::MoveFile( fileName.c_str(), backupFileName.c_str() );
	else
		::DeleteFile( fileName.c_str() );

	CVSWrapper( currentSpace_ ).removeFile( fileName );
}


void BigBang::changedTerrainBlock( Chunk* pChunk, bool rebuildNavmesh /*= true*/ )
{
	EditorChunkTerrain* pEct = static_cast<EditorChunkTerrain*>(
		ChunkTerrainCache::instance( *pChunk ).pTerrain());

	if (pEct)
	{
		changedTerrainBlocks_.insert( &pEct->block() );
		dirtyThumbnail( pChunk );
	}

	changedChunk( pChunk, rebuildNavmesh );
}

void BigBang::chunkShadowUpdated( Chunk * pChunk )
{
	EditorChunkCache::instance( *pChunk ).shadowUpdated( true );
	changedChunks_.insert( pChunk );
	dirtyThumbnail( pChunk );
}

void BigBang::changedThumbnail( Chunk* pChunk )
{
	MF_ASSERT( pChunk );

	if (!EditorChunkCache::instance( *pChunk ).edIsLocked())
		return;

	EditorChunkCache::instance( *pChunk ).thumbnailUpdated( true );
	changedChunks_.insert( pChunk );
	changedThumbnailChunks_.insert( pChunk );
	SpaceMap::instance().swapInTextures( pChunk );

	// once a chunk thumbnail has been changed it's chunk will never be
	// unloaded for this session of the editor, even when it is saved
	// (this is so that the thumbnail list does not include deleted chunks )
}


void BigBang::changedTerrainBlockOffline( const std::string& chunkName )
{
	nonloadedDirtyTerrainShadowChunks_.insert( chunkName );
	nonloadedDirtyThumbnailChunks_.insert( chunkName );
}

/**
 * Save changed terrain and chunk files, without recalculating anything
 */
bool BigBang::saveChangedFiles( SuperModelProgressDisplay& progress )
{
	bool errors = false;

    PanelManager::instance()->onBeginSave();

	// Save auxilliary files
	AuxSpaceFiles::syncIfConstructed();

	// Save terrain chunks
	ProgressTask terrainTask( &progress, "Saving terrain data", float(changedTerrainBlocks_.size()) );

	std::set<EditorTerrainBlockPtr>::iterator it = changedTerrainBlocks_.begin();
	std::set<EditorTerrainBlockPtr>::iterator end = changedTerrainBlocks_.end();

	while ( it != end )
	{
		terrainTask.step();

		EditorTerrainBlockPtr spBlock = *it++;

		Commentary::instance().addMsg( spBlock->resourceID_, 0 );

		//before we save, what if the resource exists in a binary section?
		//e.g. blahblah.cdata/terrain
		//strip off anything after the last dot.
		const std::string& resID = spBlock->resourceID_;
		int posDot = resID.find_last_of( '.' );
		int posSls = resID.substr( posDot, resID.size() ).find_first_of( "/" );
		if ( (posDot>=0) && (posSls>0) )
		{
			bool add = !BWResource::fileExists( spBlock->resourceID_.substr(0,posDot+6) );
			if ( !BigBang::instance().saveAndAddChunk(
					spBlock->resourceID_.substr(0,posDot+6), spBlock, add, true ) )
				errors = true;
		}
		else
		{
			bool add = !BWResource::fileExists( spBlock->resourceID_ );
			//legacy .terrain file suport
			if ( !BigBang::instance().saveAndAddChunk(
					spBlock->resourceID_, spBlock, add, true ) )
				errors = true;
		}
	}

	// Save object chunks.
	ProgressTask chunkTask( &progress, "Saving scenery data", float(changedChunks_.size()) );

	std::set<Chunk*>::iterator cit = changedChunks_.begin();
	std::set<Chunk*>::iterator cend = changedChunks_.end();

	while ( cit != cend )
	{
		chunkTask.step();

		Chunk * pChunk = *cit++;

		Commentary::instance().addMsg( "Saving " + pChunk->identifier(), 0 );

		if ( !EditorChunkCache::instance( *pChunk ).edSave() )
			errors = true;
	}
	
	VeryLargeObject::cleanup();

	if ( 1 )
	{
		std::string space = BigBang::instance().getCurrentSpace();
        std::string spaceSettingsFile = space + "/space.settings";
		DataSectionPtr	pDS = BWResource::openSection( spaceSettingsFile  );	
		if (pDS)
		{
            if (romp_ != NULL)
                romp_->enviroMinder().save(pDS);
            pDS->save( spaceSettingsFile );
            changedEnvironment_ = false;
		}

		ProgressTask thumbnailTask( &progress, "Saving thumbnail data", float(dirtyThumbnailChunks_.size()) );
		//Before clearing the changed object chunks, we go through the dirty thumbnaillist.
		//if the thumbnail is dirty but the chunk was not, then we just save the .cdata.
		//
		//if the thumbnail and chunk is dirty, then the chunk would have already saved
		//the .cdata and thus the thumbnail.
		std::set<Chunk*>::iterator tit = changedThumbnailChunks_.begin();
		std::set<Chunk*>::iterator tend = changedThumbnailChunks_.end();

		while ( tit != tend )
		{
			thumbnailTask.step();

			Chunk * pChunk = *tit++;

			if ( std::find( changedChunks_.begin(), changedChunks_.end(), pChunk ) == changedChunks_.end() )
			{
				//only need to save the .cdata, since the chunk itself has not changed
				//(according to the std::find we just did)
				if ( !EditorChunkCache::instance( *pChunk ).edSaveCData() )
				{
					errors = true;
					Commentary::instance().addMsg(
						"Error saving " + pChunk->identifier() + ".cData", Commentary::CRITICAL );
				}
				else
					Commentary::instance().addMsg( "Saving " + pChunk->identifier() + ".cData", 0 );
			}

			SpaceMap::instance().swapInTextures( pChunk );
		}
	}

	StationGraph::saveAll();
	SpaceMap::instance().save();

	if ( !errors )
	{
		// clear dirty lists only if no errors were generated
		changedTerrainBlocks_.clear();
		changedThumbnailChunks_.clear();
		changedChunks_.clear();
	}

    PanelManager::instance()->onEndSave();

	return !errors;
}

void BigBang::checkUpToDate( Chunk * pChunk )
{
	MF_ASSERT( pChunk );

	if( dirtyVloReferences_.find( pChunk->identifier() ) != dirtyVloReferences_.end() )
	{
		const std::set< std::pair<std::string, std::string> >& vlos =
			dirtyVloReferences_[ pChunk->identifier() ];
		for( std::set< std::pair<std::string, std::string> >::const_iterator iter = vlos.begin();
			iter != vlos.end(); ++iter )
		{
			std::vector<DataSectionPtr> vloSecs;
			EditorChunkCache::instance( *pChunk ).pChunkSection()->openSections( "vlo", vloSecs );
			bool found = false;
			for( std::vector<DataSectionPtr>::iterator vsiter = vloSecs.begin();
				vsiter != vloSecs.end(); ++vsiter )
			{
				if( (*vsiter)->readString( "uid" ) == iter->second &&
					(*vsiter)->readString( "type" ) == iter->first )
				{
					found = true;
					break;
				}
			}
			if( !found )
			{
				DataSectionPtr vlo = EditorChunkCache::instance( *pChunk ).pChunkSection()->newSection( "vlo" );
				vlo->writeString( "uid", iter->second );
				vlo->writeString( "type", iter->first );
				pChunk->loadItem( vlo );
				changedChunk( pChunk );
			}
		}
		dirtyVloReferences_.erase( pChunk->identifier() );
	}

	std::string name = pChunk->identifier();

	//Check if the lighting is out of date
	std::set<std::string>::iterator i = nonloadedDirtyLightingChunks_.find( name );
	if (i != nonloadedDirtyLightingChunks_.end())
	{
		changedChunkLighting( pChunk );
		nonloadedDirtyLightingChunks_.erase( i );
	}
	else if( !EditorChunkCache::instance( *pChunk ).lightingUpdated() && !pChunk->isOutsideChunk() )
		changedChunkLighting( pChunk );

	//Check if the shadow data is out of date
	std::set<std::string>::iterator ti = nonloadedDirtyTerrainShadowChunks_.find( name );
	if (ti != nonloadedDirtyTerrainShadowChunks_.end())
	{
		changedTerrainShadows( pChunk );
		nonloadedDirtyTerrainShadowChunks_.erase( ti );
	}
	else if( !EditorChunkCache::instance( *pChunk ).shadowUpdated() && pChunk->isOutsideChunk() )
		changedTerrainShadows( pChunk );

	//Check if the thumbnail is out of date
	std::set<std::string>::iterator thi = nonloadedDirtyThumbnailChunks_.find( name );
	if (thi != nonloadedDirtyThumbnailChunks_.end())
	{
		dirtyThumbnail( pChunk );
		nonloadedDirtyThumbnailChunks_.erase( thi );
	}
	else if( !EditorChunkCache::instance( *pChunk ).thumbnailUpdated() ||
		!EditorChunkCache::instance( *pChunk ).pCDataSection()->openSection( "thumbnail.dds" ) )
		dirtyThumbnail( pChunk );
}

namespace
{
	void writeNames(DataSectionPtr ds,
						const std::string& tag,
						const std::vector<std::string>& names)
	{
		std::vector<std::string>::const_iterator i = names.begin();
		for (; i != names.end(); ++i)
			ds->newSection( tag )->setString( (*i) );
	}

	void writeNames(DataSectionPtr ds,
						const std::string& tag,
						const std::set<std::string>& names)
	{
		std::set<std::string>::const_iterator i = names.begin();
		for (; i != names.end(); ++i)
			ds->newSection( tag )->setString( (*i) );
	}

	void writeNames(DataSectionPtr ds,
						const std::string& tag,
						const std::set<Chunk*>& names)
	{
		std::set<Chunk*>::const_iterator i = names.begin();
		for (; i != names.end(); ++i)
//			if (!EditorChunkCache::instance( **i ).edIsDeleted())
				ds->newSection( tag )->setString( (*i)->identifier() );
	}

	void writeNames(DataSectionPtr ds,
						const std::string& tag,
						const std::vector<Chunk*>& names)
	{
		std::vector<Chunk*>::const_iterator i = names.begin();
		for (; i != names.end(); ++i)
//			if (!EditorChunkCache::instance( **i ).edIsDeleted())
				ds->newSection( tag )->setString( (*i)->identifier() );
	}
	
	std::string getPythonStackTrace()
	{
		std::string stack = "";
		
		PyObject *ptype, *pvalue, *ptraceback;
		PyErr_Fetch( &ptype, &pvalue, &ptraceback);

		if (ptraceback != NULL)
		{
			// use traceback.format_exception 
			// to get stacktrace as a string
			PyObject* pModule = PyImport_ImportModule( "traceback" );
			if (pModule != NULL)
			{
				PyObject * formatFunction = PyObject_GetAttr( 
						pModule, Py_BuildValue( "s", "format_exception" ) );

				if (formatFunction != NULL)
				{
					PyObject * list = Script::ask( 
							formatFunction, Py_BuildValue( "(OOO)", ptype, pvalue, ptraceback ), 
							"BigBang", false, false );
							
					if (list != NULL)
					{
						for (int i=0; i < PyList_Size( list ); ++i)
						{
							stack += PyString_AS_STRING( PyList_GetItem( list, i ) );
						}
						Py_DECREF( list );
					}
				}
				Py_DECREF( pModule );
			}
		}
		
		// restore error so that PyErr_Print still sends
		// traceback to console (PyErr_Fetch clears it)
		PyErr_Restore( ptype, pvalue, ptraceback);

		return stack;
	}



}

/**
 * Write the current set (loaded and non loaded) of dirty chunks out.
 */
bool BigBang::writeDirtyList()
{
	std::string resname = spaceManager_->entry( 0 ) + '/' +
		"space.localsettings";

	DataSectionPtr pDS = BWResource::openSection( resname, true );

	if (!pDS)
	{
		addError( NULL, NULL,
			"Can't open %s to save dirty lists\n", resname.c_str() );
		return false;
	}

	std::string reentryFolder = pDS->readString( "reentryFolder" );
	std::string startPosition = pDS->readString( "startPosition" );
	std::string startDirection = pDS->readString( "startDirection" );

	pDS->delChildren();

	if( !reentryFolder.empty() )
		pDS->writeString( "reentryFolder", reentryFolder );

	if( !startPosition.empty() )
		pDS->writeString( "startPosition", startPosition );

	if( !startDirection.empty() )
		pDS->writeString( "startDirection", startDirection );

	writeNames( pDS, "dirtylighting", nonloadedDirtyLightingChunks_ );
	writeNames( pDS, "dirtylighting", dirtyLightingChunks_ );

	writeNames( pDS, "dirtyterrain", nonloadedDirtyTerrainShadowChunks_);
	writeNames( pDS, "dirtyterrain", dirtyTerrainShadowChunks_ );

	writeNames( pDS, "dirtythumbnail", nonloadedDirtyThumbnailChunks_);
	writeNames( pDS, "dirtythumbnail", dirtyThumbnailChunks_);

	for( DirtyVLOReferences::iterator iter = dirtyVloReferences_.begin();
		iter != dirtyVloReferences_.end(); ++iter )
	{
		for( std::set< std::pair<std::string, std::string> >::iterator siter = iter->second.begin();
			siter != iter->second.end(); ++siter )
		{
			DataSectionPtr vlo = pDS->newSection( "dirtyVLO" );
			vlo->setString( iter->first );
			vlo->writeString( "type", siter->first );
			vlo->writeString( "uid", siter->second );
		}
	}

	pDS->save();
	return true;
}

bool BigBang::checkForReadOnly() const
{
	bool readOnly = Options::getOptionInt("objects/readOnlyMode", 0) != 0;
	if( readOnly )
	{
		MessageBox( *BigBang2App::instance().mainWnd(),
			"Bigbang is running in Read Only mode. You cannot save any modifications",
			"Read Only", MB_OK );
	}
	return readOnly;
}
/**
 * Only save changed chunk and terrain data, don't recalculate anything.
 *
 * Dirty lists are persisted to disk.
 */
void BigBang::quickSave()
{
	saveFailed_ = false;

	if( checkForReadOnly() )
		return;

	bool errors = false;

	BigBangProgressBar progress;

	Commentary::instance().addMsg( "Quick Saving...", 1 );

	if ( !saveChangedFiles( progress ) )
		errors = true;

	if ( !writeDirtyList() )
		errors = true;

	if ( errors )
	{
		const char* errorMsg = "Quick Save completed with errors. See Errors tab for more information.";
		MessageBox( *BigBang2App::instance().mainWnd(),
			errorMsg,
			"Errors while performing a Quick Save",
			MB_ICONERROR );
		addError( NULL, NULL, errorMsg );
	}
	else
		Commentary::instance().addMsg( "Quick Save complete", 1 );

	MainFrame* mainFrame = (MainFrame *)BigBang2App::instance().mainWnd();
	mainFrame->InvalidateRect( NULL );
	mainFrame->UpdateWindow();

	saveFailed_ = errors;
}

bool BigBang::EnsureNeighbourChunkLoaded( Chunk* chunk, int level )
{
	if( !chunk->online() )
		return false;

	if( level == 0 )
		return true;

	for( ChunkBoundaries::iterator bit = chunk->joints().begin();
		bit != chunk->joints().end(); ++bit )
	{
		for( ChunkBoundary::Portals::iterator ppit = (*bit)->unboundPortals_.begin();
			ppit != (*bit)->unboundPortals_.end(); ++ppit )
		{
			ChunkBoundary::Portal* pit = *ppit;
			if( !pit->hasChunk() )
				continue;
			return false;
		}
	}

	for( ChunkBoundaries::iterator bit = chunk->joints().begin();
		bit != chunk->joints().end(); ++bit )
	{
		for( ChunkBoundary::Portals::iterator ppit = (*bit)->boundPortals_.begin();
			ppit != (*bit)->boundPortals_.end(); ++ppit )
		{
			ChunkBoundary::Portal* pit = *ppit;
			if( !pit->hasChunk() ) continue;

			if( !EnsureNeighbourChunkLoaded( pit->pChunk, level - 1 ) )
				return false;
		}
	}
	return true;
}

bool BigBang::EnsureNeighbourChunkLoadedForShadow( Chunk* chunk )
{
	int16 gridX, gridZ;
	if( !gridFromChunk( chunk, gridX, gridZ ) )
		return true;// assume
	int16 dist = (int16)( ( MAX_TERRAIN_SHADOW_RANGE + 1.f ) / GRID_RESOLUTION );
	for( int16 z = gridZ - 1; z <= gridZ + 1; ++z )
		for( int16 x = gridX - dist; x <= gridX + dist; ++x )
		{
			std::string chunkName;
			chunkID( chunkName, x, z );

			if( chunkName.empty() )
				continue;

			Chunk* c = ChunkManager::instance().findChunkByName( chunkName, chunkDirMapping() );

			if( !c )
				continue;
			if( !c->loaded() )
				return false;
		}
	return true;
}

void BigBang::loadNeighbourChunk( Chunk* chunk, int level )
{
	if( level == 0 )
		return;

	for( ChunkBoundaries::iterator bit = chunk->joints().begin();
		bit != chunk->joints().end(); ++bit )
	{
		for( ChunkBoundary::Portals::iterator ppit = (*bit)->unboundPortals_.begin();
			ppit != (*bit)->unboundPortals_.end(); ++ppit )
		{
			ChunkBoundary::Portal* pit = *ppit;
			if( !pit->hasChunk() )
				continue;

			ChunkManager::instance().loadChunkNow( pit->pChunk->identifier(), chunkDirMapping() );
		}
		ChunkManager::instance().checkLoadingChunks();
	}

	for( ChunkBoundaries::iterator bit = chunk->joints().begin();
		bit != chunk->joints().end(); ++bit )
	{
		for( ChunkBoundary::Portals::iterator ppit = (*bit)->boundPortals_.begin();
			ppit != (*bit)->boundPortals_.end(); ++ppit )
		{
			ChunkBoundary::Portal* pit = *ppit;
			if( !pit->hasChunk() ) continue;

			loadNeighbourChunk( pit->pChunk, level - 1 );
		}
	}
}

void BigBang::loadChunkForShadow( Chunk* chunk )
{
	int16 gridX, gridZ;
	if( !gridFromChunk( chunk, gridX, gridZ ) )
		return;
	int16 dist = (int16)( ( MAX_TERRAIN_SHADOW_RANGE + 1.f ) / GRID_RESOLUTION );
	for( int16 z = gridZ - 1; z <= gridZ + 1; ++z )
		for( int16 x = gridX - dist; x <= gridX + dist; ++x )
		{
			std::string chunkName;
			chunkID( chunkName, x, z );

			if( chunkName.empty() )
				continue;

			Chunk* c = ChunkManager::instance().findChunkByName( chunkName, chunkDirMapping() );

			if( c && c->loaded() )
				continue;

			ChunkManager::instance().loadChunkNow( chunkName, chunkDirMapping() );
		}
	ChunkManager::instance().checkLoadingChunks();
}

void BigBang::saveChunk( const std::string& chunkName, ProgressTask& task )
{
	this->saveChunk( ChunkManager::instance().findChunkByName( chunkName, chunkDirMapping() ), task );
}

void BigBang::saveChunk( Chunk* chunk, ProgressTask& task )
{
	if( !chunk->loaded() )
	{
		ChunkManager::instance().loadChunkNow( chunk );
		ChunkManager::instance().checkLoadingChunks();
	}
	ChunkManager::instance().cameraSpace()->focus( chunk->centre() );
	loadNeighbourChunk( chunk, StaticLighting::STATIC_LIGHT_PORTAL_DEPTH );
	ChunkManager::instance().cameraSpace()->focus( chunk->centre() );
	if( chunk->isOutsideChunk() )
		loadChunkForShadow( chunk );
	ChunkManager::instance().cameraSpace()->focus( chunk->centre() );

	EditorChunkCache& chunkCache = EditorChunkCache::instance( *chunk );

	if( !chunkCache.edIsLocked() )
	{
		ERROR_MSG( "chunk %s is marked as dirty, but isn't locked!\n", chunk->identifier().c_str() );
		return;
	}

	if( !chunk->online() )
	{
		ERROR_MSG( "chunk %s is marked as dirty, but isn't online!\n", chunk->identifier().c_str() );
		return;
	}

	if( chunkCache.edIsDeleted() )
		return;

	std::vector<Chunk*>::iterator current = std::find( dirtyLightingChunks_.begin(),
		dirtyLightingChunks_.end(), chunk );

	BigBang::instance().workingChunk( chunk );
	if( current != dirtyLightingChunks_.end() )
	{
		task.step();

		MF_ASSERT( !chunk->isOutsideChunk() );

		dirtyLightingChunks_.erase( current );

		Commentary::instance().addMsg( "Calculating lighting for " + chunk->identifier(), 0 );
		chunkCache.edRecalculateLighting();
	}

	current = std::find( dirtyTerrainShadowChunks_.begin(), dirtyTerrainShadowChunks_.end(), chunk );
	if( current != dirtyTerrainShadowChunks_.end() )
	{
		task.step();

		dirtyTerrainShadowChunks_.erase( current );

		EditorChunkTerrain* pEct = static_cast<EditorChunkTerrain*>(
			ChunkTerrainCache::instance( *chunk ).pTerrain());

		if( pEct )
		{
			Commentary::instance().addMsg( "Calculating shadows for " + chunk->identifier(), 0 );
			pEct->calculateShadows(false);
		}
	}

	current = std::find( dirtyThumbnailChunks_.begin(), dirtyThumbnailChunks_.end(), chunk );
	if( current != dirtyThumbnailChunks_.end() )
	{
		task.step();

		dirtyThumbnailChunks_.erase( current );

		Commentary::instance().addMsg( "Calculating thumbnails for " + chunk->identifier(), 0 );
		EditorChunkCache::instance( *chunk ).calculateThumbnail();
	}
	BigBang::instance().workingChunk( NULL );
}

static unsigned int currentTime = 0;
static const unsigned int saveInterval = 1000 * 300;// 5 minutes

template<typename T>
bool BigBang::save( T t, ProgressTask& task, BigBangProgressBar& progress )
{
	if( currentTime == 0 )
		currentTime = GetTickCount();
	bool errors = false;
	while( !t.empty() )
	{
		saveChunk( *t.begin(), task );
		t.erase( t.begin() );
		if( GetTickCount() - currentTime > saveInterval )
		{
			currentTime = GetTickCount();
			if ( !writeDirtyList() )
				errors = true;
			if ( !saveChangedFiles( progress ) )
				errors = true;

			unloadChunks();
		}
	}
	return !errors;
}

/** Save everything, and make sure all dirty data (static lighting, terrain shadows )is up to date */
void BigBang::save()
{
	saveFailed_ = false;

	if( checkForReadOnly() )
		return;

	bool errors = false;

	killingUpdatingFiber_ = true;
	while( killingUpdatingFiber_ )
		doBackgroundUpdating();

	ChunkOverlapper::drawList.clear();

	ChunkManager::instance().switchToSyncMode( true );
	BigBangProgressBar progress;

	Commentary::instance().addMsg( "Saving...", 1 );

	// Finish calculating static lighting
	ProgressTask savingTask( &progress, "Recalculating shadow, lighting and thumbnail",
		float( dirtyLightingChunks_.size() + dirtyTerrainShadowChunks_.size() +
		dirtyThumbnailChunks_.size() + nonloadedDirtyLightingChunks_.size()
		+ nonloadedDirtyTerrainShadowChunks_.size() + nonloadedDirtyThumbnailChunks_.size()
		+ dirtyVloReferences_.size() ) );

	//// non-loaded dirty chunks
	if ( !save( nonloadedDirtyLightingChunks_, savingTask, progress ) )
		errors = true;

	if ( !save( nonloadedDirtyTerrainShadowChunks_, savingTask, progress ) )
		errors = true;

	if ( !save( nonloadedDirtyThumbnailChunks_, savingTask, progress ) )
		errors = true;

	// loaded dirty chunks
	if ( !save( dirtyLightingChunks_, savingTask, progress ) )
		errors = true;

	if ( !save( dirtyTerrainShadowChunks_, savingTask, progress ) )
		errors = true;

	if ( !save( dirtyThumbnailChunks_, savingTask, progress ) )
		errors = true;

	std::set<std::string> chunks;
	for( DirtyVLOReferences::iterator iter = dirtyVloReferences_.begin(); iter != dirtyVloReferences_.end(); ++iter )
		chunks.insert( iter->first );

	if ( !save( dirtyThumbnailChunks_, savingTask, progress ) )
		errors = true;

	// Write out the current state of the non loaded dirty list
	if ( !writeDirtyList() )
		errors = true;

	if ( !saveChangedFiles( progress ) )
		errors = true;

    // Get the project module to update the dirty chunks.
    ProjectModule::regenerateAllDirty();

    // Get the terrain height import/export module to save its height map.
    HeightModule::ensureHeightMapCalculated();

	if ( errors )
	{
		const char* errorMsg = "Full Save completed with errors. See Errors tab for more information.";
		MessageBox( *BigBang2App::instance().mainWnd(),
			errorMsg,
			"Errors while performing a Full Save",
			MB_ICONERROR );
		addError( NULL, NULL, errorMsg );
	}
	else
    {
		Commentary::instance().addMsg( "Save complete", 1 );
    }

	// Check that we've actually been able to recalculate everything
	if (!nonloadedDirtyLightingChunks_.empty())
		Commentary::instance().addMsg( "WARNING! There are still non loaded chunks with dirty lighting", 1 );
	if (!nonloadedDirtyTerrainShadowChunks_.empty())
		Commentary::instance().addMsg( "WARNING! There are still non loaded chunks with dirty terrain shadows", 1 );
	if (!nonloadedDirtyThumbnailChunks_.empty())
		Commentary::instance().addMsg( "WARNING! There are still non loaded chunks with dirty thumbnails", 1 );
	if (!dirtyVloReferences_.empty())
		Commentary::instance().addMsg( "WARNING! There are still non loaded chunks with dirty vlo references", 1 );

	ChunkManager::instance().switchToSyncMode( false );

	ChunkOverlapper::drawList.clear();


	MainFrame* mainFrame = (MainFrame *)BigBang2App::instance().mainWnd();
	mainFrame->InvalidateRect( NULL );
	mainFrame->UpdateWindow();

	saveFailed_ = errors;
}


float BigBang::farPlane() const
{
	Moo::Camera camera = Moo::rc().camera();
	return camera.farPlane();
}


void BigBang::farPlane( float f )
{
	float farPlaneDistance = Math::clamp( 0.0f, f, 2500.f );

	Moo::Camera camera = Moo::rc().camera();
	camera.farPlane( farPlaneDistance );
	Moo::rc().camera( camera );

	// mark only things within the far plane as candidates for loading
	ChunkManager::instance().autoSetPathConstraints( farPlaneDistance );
}

bool BigBang::isItemSelected( ChunkItemPtr item ) const
{
	for (std::vector<ChunkItemPtr>::const_iterator it = selectedItems_.begin();
		it != selectedItems_.end();
		it++)
	{
		if ((*it) == item)
			return true;
	}

	return false;
}

bool BigBang::isChunkSelected( Chunk * pChunk ) const
{
	for (std::vector<ChunkItemPtr>::const_iterator it = selectedItems_.begin();
		it != selectedItems_.end();
		it++)
	{
		if ((*it)->chunk() == pChunk && (*it)->isShellModel())
			return true;
	}

	return false;
}

bool BigBang::isChunkSelected() const
{
	for (std::vector<ChunkItemPtr>::const_iterator it = selectedItems_.begin();
		it != selectedItems_.end();
		it++)
	{
		if ((*it)->isShellModel())
			return true;
	}

	return false;
}

bool BigBang::isItemInChunkSelected( Chunk * pChunk ) const
{
	for (std::vector<ChunkItemPtr>::const_iterator it = selectedItems_.begin();
		it != selectedItems_.end();
		it++)
	{
		if ((*it)->chunk() == pChunk && !(*it)->isShellModel())
			return true;
	}

	return false;
}

bool BigBang::isInPlayerPreviewMode() const
{
	return isInPlayerPreviewMode_;
}

void BigBang::setPlayerPreviewMode( bool enable )
{
	if( enable )
	{
		GUI::ItemPtr hideOrtho = ( *GUI::Manager::instance() )( "/MainToolbar/Edit/ViewOrtho/HideOrthoMode" );
		if( hideOrtho && hideOrtho->update() )
			hideOrtho->act();
	}
	isInPlayerPreviewMode_ = enable;
}

void BigBang::markChunks()
{
	if( !EditorChunkCache::chunks_.empty() )
		getSelection();
	else
		selectedItems_.clear();

	for( std::set<Chunk*>::iterator iter = EditorChunkCache::chunks_.begin();
		iter != EditorChunkCache::chunks_.end(); ++iter )
		(*iter)->removable( true );

	if( workingChunk_ )
		workingChunk_->removable( false );

	for( std::set<Chunk*>::iterator iter = changedChunks_.begin();
		iter != changedChunks_.end(); ++iter )
		(*iter)->removable( false );
	for( std::set<Chunk*>::iterator iter = changedThumbnailChunks_.begin();
		iter != changedThumbnailChunks_.end(); ++iter )
		(*iter)->removable( false );

	UndoRedo::instance().markChunk();
	for( std::vector<ChunkItemPtr>::iterator iter = selectedItems_.begin();
		iter != selectedItems_.end(); ++iter )
		if ( (*iter)->chunk() )
			(*iter)->chunk()->removable( false );
}

void BigBang::unloadChunks()
{
	killingUpdatingFiber_ = true;
	while( killingUpdatingFiber_ )
		doBackgroundUpdating();

	ChunkOverlapper::drawList.clear();

	ChunkManager::instance().switchToSyncMode( true );

	markChunks();

	std::set<Chunk*> chunks = EditorChunkCache::chunks_;
	for( std::set<Chunk*>::iterator iter = chunks.begin();
		iter != chunks.end(); ++iter )
	{
		Chunk* chunk = *iter;
		if( !chunk->removable() )
			continue;
		chunk->loose( false );
		chunk->eject();
	}

	ChunkManager::instance().switchToSyncMode( false );
}

void BigBang::setSelection( const std::vector<ChunkItemPtr>& items, bool updateSelection )
{
	PyObject* pModule = PyImport_ImportModule( "BigBangDirector" );
	if (pModule != NULL)
	{
		PyObject* pScriptObject = PyObject_GetAttr( pModule, Py_BuildValue( "s", "bd" ) );

		if (pScriptObject != NULL)
		{
			settingSelection_ = true;
			Script::call(
				PyObject_GetAttrString( pScriptObject, "setSelection" ),
				Py_BuildValue( "(Oi)",
					static_cast<PyObject*>( new ChunkItemGroup( items ) ),
					(int) updateSelection ),
				"BigBang");
			settingSelection_ = false;

			if (!updateSelection)
			{
				// Note that this doesn't update snaps etc etc - it is assumed
				// that revealSelection will be called some time in the near
				// future, and thus this will get updated properly. This only
				// happens here so that a call to selectedItems() following
				// this will return what's expected.
                std::vector<ChunkItemPtr> newSelection = items;
                selectedItems_.clear();
                for (size_t i = 0; i < newSelection.size(); ++i)
                    if (newSelection[i]->edCanAddSelection())
                        selectedItems_.push_back(newSelection[i]);
			}
			Py_DECREF( pScriptObject );
		}
		Py_DECREF( pModule );
	}
}

void BigBang::getSelection()
{
	PyObject* pModule = PyImport_ImportModule( "BigBangDirector" );
	if( pModule != NULL )
	{
		PyObject* pScriptObject = PyObject_GetAttr( pModule, Py_BuildValue( "s", "bd" ) );

		if( pScriptObject != NULL )
		{
			ChunkItemGroup* cig = new ChunkItemGroup();
			Script::call(
				PyObject_GetAttrString( pScriptObject, "getSelection" ),
				Py_BuildValue( "(O)", static_cast<PyObject*>( cig ) ),
				"BigBang");

            std::vector<ChunkItemPtr> newSelection = cig->get();
            selectedItems_.clear();
            for (size_t i = 0; i < newSelection.size(); ++i)
                if (newSelection[i]->edCanAddSelection())
                    selectedItems_.push_back(newSelection[i]);
			Py_DecRef( cig );
			Py_DECREF( pScriptObject );
		}
		Py_DECREF( pModule );
	}
}

bool BigBang::drawSelection() const
{
	return drawSelection_;
}

void BigBang::drawSelection( bool drawingSelection )
{
	drawSelection_ = drawingSelection;
}

bool BigBang::snapsEnabled() const
{
	return ( Options::getOptionInt( "snaps/xyzEnabled", 0 ) != 0 );
}


bool BigBang::terrainSnapsEnabled() const
{
	if (isChunkSelected())
		return false;

	return ( Options::getOptionInt( "snaps/itemSnapMode", 0 ) == 1 );
}

bool BigBang::obstacleSnapsEnabled() const
{
	if (isChunkSelected())
		return false;

	return ( Options::getOptionInt( "snaps/itemSnapMode", 0 ) == 2 );
}

BigBang::CoordMode BigBang::getCoordMode() const
{
	if( Options::getOptionString( "tools/coordFilter", "World" ) == "Local" )
		return COORDMODE_OBJECT;
	if( Options::getOptionString( "tools/coordFilter", "World" ) == "View" )
		return COORDMODE_VIEW;
	return COORDMODE_WORLD;
}

Vector3 BigBang::movementSnaps() const
{
    Vector3 movementSnap = 
        Options::getOptionVector3( "snaps/movement", Vector3( 0.f, 0.f, 0.f ) );
    // Don't snap in the y-direction if snaps and terrain locking are both 
    // enabled.
	if ( snapsEnabled() && terrainSnapsEnabled() )
    {
        movementSnap.y = 0.f;
    }

	return movementSnap;
}

float BigBang::angleSnaps() const
{
	if (snapsEnabled())
		return Snap::satisfy( angleSnaps_, Options::getOptionFloat( "snaps/angle", 0.f ) );
	else
		return angleSnaps_;
}

void BigBang::calculateSnaps()
{
	angleSnaps_ = 0.f;
	movementDeltaSnaps_ = Vector3( 0.f, 0.f, 0.f );

	for (std::vector<ChunkItemPtr>::iterator it = selectedItems_.begin();
		it != selectedItems_.end();
		it++)
	{
		angleSnaps_ = Snap::satisfy( angleSnaps_, (*it)->edAngleSnaps() );
		Vector3 m = (*it)->edMovementDeltaSnaps();

		movementDeltaSnaps_.x = Snap::satisfy( movementDeltaSnaps_.x, m.x );
		movementDeltaSnaps_.y = Snap::satisfy( movementDeltaSnaps_.y, m.y );
		movementDeltaSnaps_.z = Snap::satisfy( movementDeltaSnaps_.z, m.z );
	}
}


int BigBang::drawBSP() const
{
	static uint32 s_settingsMark_ = -16;
	static int drawBSP = 0;
	if (Moo::rc().frameTimestamp() != s_settingsMark_)
	{
		drawBSP = Options::getOptionInt( "drawBSP", 0 );
		s_settingsMark_ = Moo::rc().frameTimestamp();
	}
	return drawBSP;
}

void BigBang::snapAngles( Matrix& v )
{
	if ( snapsEnabled() )
		Snap::angles( v, angleSnaps() );
}

SnapProvider::SnapMode BigBang::snapMode( ) const
{
	return	terrainSnapsEnabled()	?	SNAPMODE_TERRAIN :
			obstacleSnapsEnabled()	?	SNAPMODE_OBSTACLE :
										SNAPMODE_XYZ ;
}

bool BigBang::snapPosition( Vector3& v )
{
    Vector3 origPosition = v;
	if ( snapsEnabled() )
    	v = Snap::vector3( v, movementSnaps() );
	if ( terrainSnapsEnabled() )
		v = Snap::toGround( v );
	else if ( obstacleSnapsEnabled() )
	{
		Vector3 startPosition = Moo::rc().invView().applyToOrigin();
		if (selectedItems_.size() > 1)
			startPosition = v - (Moo::rc().invView().applyToOrigin().length() * worldRay());
        bool hitObstacle = false;
        Vector3 newV = Snap::toObstacle( startPosition, worldRay(), false, 2500.f, &hitObstacle );
        if (!hitObstacle)
        {
            v = origPosition;
            return false;
        }
        else
        {
            v = newV;
        }
	}
    return true;
}

Vector3 BigBang::snapNormal( const Vector3& v )
{
	Vector3 result( 0, 1, 0 );// default for y axis, should never used
	if ( obstacleSnapsEnabled() )
	{
		Vector3 startPosition = Moo::rc().invView().applyToOrigin();
		if (selectedItems_.size() > 1)
			startPosition = v - (Moo::rc().invView().applyToOrigin().length() * worldRay());
		result = Snap::toObstacleNormal( startPosition, worldRay() );
	}
	return result;
}

void BigBang::snapPositionDelta( Vector3& v )
{
	v = Snap::vector3( v, movementDeltaSnaps_ );
}

float BigBang::angleSnapAmount()
{
	return angleSnaps();
}



void BigBang::addReadOnlyBlock( const Matrix& transform, Moo::TerrainBlockPtr pBlock )
{
	readOnlyTerrainBlocks_.push_back( BlockInPlace( transform, pBlock ) );
}

void BigBang::setReadOnlyFog()
{
	// Set the fog to a constant red colour
	float fnear = -10000.f;
	float ffar = 10000.f;
	DWORD colour = 0x00AA0000;

	Moo::rc().setRenderState( D3DRS_FOGCOLOR, colour );
	Moo::rc().setRenderState( D3DRS_FOGENABLE, true );

	Moo::rc().fogNear( fnear );
	Moo::rc().fogFar( ffar );

	Moo::Material::standardFogColour( colour );
}

bool BigBang::isPointInWriteableChunk( const Vector3& pt )
{
	return EditorChunk::outsideChunkWriteable( pt );
}

void BigBang::reloadAllChunks( bool askBeforeProceed )
{
	if( askBeforeProceed && !canClose( "&Reload" ) )
		return;

	std::string space = currentSpace_;
	currentSpace_ = "(empty)";
	MsgHandler::instance().removeAssetErrorMessages(); 
	changeSpace( space, true );
}

// -----------------------------------------------------------------------------
// Section: Error message handling
// -----------------------------------------------------------------------------

std::ostream* createLogFile()
{
	char dateStr [9];
    char timeStr [9];
    _strdate( dateStr );
    _strtime( timeStr );

	static const int MAX_LOG_NAME = 8192;
	static char logName[ MAX_LOG_NAME + 1 ] = "";
	if( logName[0] == 0 )
	{
		DWORD len = GetModuleFileName( NULL, logName, MAX_LOG_NAME );
		while( len && logName[ len - 1 ] != '.' )
			--len;
		strcpy( logName + len, "log" );
	}

	std::ostream* logFile = NULL;
		
	if( logName[0] != 0 )
	{
		logFile = new std::ofstream( logName, std::ios::app );

		*logFile << "\n/-----------------------------------------------"
			"-------------------------------------------\\\n";

		*logFile << "BigWorld World Editor " << aboutVersionString <<
			" (compiled at " << aboutCompileTimeString << ")"
			"starting on " << dateStr << " " << timeStr << "\n\n";
		
		logFile->flush();
	}

	//Catch any commentary messages
	Commentary::instance().logFile( logFile );

	//Instanciate the Message handler to catch BigWorld messages
	MsgHandler::instance().logFile( logFile );

	return logFile;
}

SimpleMutex BigBang::pendingMessagesMutex_;
BigBang::StringVector BigBang::pendingMessages_;
std::ostream* BigBang::logFile_ = createLogFile();

/**
 * Post all messages we've recorded since the last time this was called
 */
void BigBang::postPendingErrorMessages()
{
	pendingMessagesMutex_.grab();
	StringVector::iterator i = pendingMessages_.begin();
	for (; i !=	pendingMessages_.end(); ++i)
	{
		Commentary::instance().addMsg( *i, Commentary::WARNING );
	}
	pendingMessages_.clear();
	pendingMessagesMutex_.give();
}

/**
 *	This static function implements the callback that will be called for each
 *	*_MSG.
 *
 *	This is thread safe. We only want to add the error as a commentary message
 *	from the main thread, thus the adding them to a vector. The actual posting
 *	is done in postPendingErrorMessages.
 */
bool BigBangDebugMessageCallback::handleMessage( int componentPriority,
		int messagePriority,
		const char * format, va_list argPtr )
{
	return BigBang::messageHandler( componentPriority, messagePriority, format, argPtr );
}


bool BigBang::messageHandler( int componentPriority,
		int messagePriority,
		const char * format, va_list argPtr )
{
	char buf[8192];
	_vsnprintf( buf, sizeof(buf), format, argPtr );
	buf[sizeof(buf)-1]=0;
	if (buf[ strlen(buf) - 1 ] == '\n')
	{
		buf[ strlen(buf) - 1 ] = '\0';
	}

	if ( DebugFilter::shouldAccept( componentPriority, messagePriority ) &&
			messagePriority == MESSAGE_PRIORITY_ERROR)
	{
		bool isNewError	   = false;
		bool isPythonError = PyErr_Occurred() != NULL;
		if (isPythonError)
		{
			std::string stacktrace = getPythonStackTrace();
			if( &MsgHandler::instance() )
				isNewError = MsgHandler::instance().addAssetErrorMessage( 
					buf, NULL, NULL, stacktrace.c_str());
			else
				isNewError = true;
					
			strcat( buf, "(see error tab for details)" );
		}

		if (isNewError) 
		{
			pendingMessagesMutex_.grab();
			pendingMessages_.push_back( buf );
			pendingMessagesMutex_.give();
		}
	}

	/*if (logFile_)
	{
		*logFile_ << buf << "\n";
		logFile_->flush();
	}*/

	return false;
}

void BigBang::addPrimGroupCount( Chunk* chunk, uint n )
{
	if (chunk == currentMonitoredChunk_)
		currentPrimGroupCount_ += n;
}

void BigBang::refreshWeather()
{
	if( romp_ )
		romp_->update( 1.0, globalWeather_ );
}

void BigBang::setStatusMessage( unsigned int index, const std::string& value )
{
	if( index >= statusMessages_.size() )
		statusMessages_.resize( index + 1 );
	statusMessages_[ index ] = value;
}

const std::string& BigBang::getStatusMessage( unsigned int index ) const
{
	static std::string empty;
	if( index >= statusMessages_.size() )
		return empty;
	return statusMessages_[ index ];
}

void BigBang::setCursor( HCURSOR cursor )
{
	if( cursor_ != cursor )
	{
		cursor_ = cursor;
		POINT mouse;
		GetCursorPos( &mouse );
		SetCursorPos( mouse.x, mouse.y + 1 );
		SetCursorPos( mouse.x, mouse.y );
	}
}

void BigBang::resetCursor()
{
	static HCURSOR cursor = ::LoadCursor( NULL, IDC_ARROW );
	setCursor( cursor );
}

unsigned int BigBang::dirtyChunks() const
{
	return dirtyLightingChunks_.size() + dirtyTerrainShadowChunks_.size() +
		nonloadedDirtyLightingChunks_.size() + nonloadedDirtyTerrainShadowChunks_.size();
}

bool BigBang::canClose( const std::string& action )
{
	bool changedTerrain = (changedTerrainBlocks_.size() != 0);
	bool changedScenery = (changedChunks_.size() != 0);
	bool changedThumbnail = (changedThumbnailChunks_.size() != 0);
	if ( changedTerrain || changedScenery || changedThumbnail || changedEnvironment_)
	{
		MainFrame* mainFrame = (MainFrame *)BigBang2App::instance().mainWnd();
		MsgBox mb( "Changed Files",
			"Changes were made to the scene. Do you want to save changes before quitting?\n\n"
			"Note: Process then Save will recalculate lighting and thumbnails",
			"&Save", "&Process then Save",
			action + " without Save", "&Cancel" );
		int result = mb.doModal( mainFrame->m_hWnd );
		if( result == 3 )
			return false;
		else if( result == 0 )
		{
			GUI::ItemPtr quickSave = ( *GUI::Manager::instance() )( "/MainToolbar/File/QuickSave" );
			quickSave->act();
			if ( saveFailed_ )
				return false;
		}
		else if( result == 1 )
		{
			GUI::ItemPtr save = ( *GUI::Manager::instance() )( "/MainToolbar/File/Save" );
			save->act();
			if ( saveFailed_ )
				return false;
		}
        else if (result == 2 )
        {
			HeightModule::doNotSaveHeightMap();
        }
		BigBang2App::instance().mfApp()->consumeInput();
	}
	UndoRedo::instance().clear();
	CSplashDlg::HideSplashScreen();
	return true;
}

void BigBang::updateUIToolMode( const std::string& pyID )
{
	PanelManager::instance()->updateUIToolMode( pyID );
}


//---------------------------------------------------------------------------

PY_MODULE_STATIC_METHOD( BigBang, worldRay, BigBang )
PY_MODULE_STATIC_METHOD( BigBang, farPlane, BigBang )
PY_MODULE_STATIC_METHOD( BigBang, save, BigBang )
PY_MODULE_STATIC_METHOD( BigBang, quickSave, BigBang )
PY_MODULE_STATIC_METHOD( BigBang, update, BigBang )
PY_MODULE_STATIC_METHOD( BigBang, render, BigBang )
PY_MODULE_STATIC_METHOD( BigBang, revealSelection, BigBang )
PY_MODULE_STATIC_METHOD( BigBang, isChunkSelected, BigBang )
PY_MODULE_STATIC_METHOD( BigBang, selectAll, BigBang )
PY_MODULE_STATIC_METHOD( BigBang, cursorOverGraphicsWnd, BigBang )
PY_MODULE_STATIC_METHOD( BigBang, importDataGUI, BigBang )
PY_MODULE_STATIC_METHOD( BigBang, exportDataGUI, BigBang )
PY_MODULE_STATIC_METHOD( BigBang, rightClick, BigBang )


/**
 *	The static python world ray method
 */
PyObject * BigBang::py_worldRay( PyObject * args )
{
	return Script::getData( s_instance->worldRay_ );
}


/**
 *	The static python far plane method
 */
PyObject * BigBang::py_farPlane( PyObject * args )
{
	float nfp = -1.f;
	if (!PyArg_ParseTuple( args, "|f", &nfp ))
	{
		//There was not a single float argument,
		//therefore return the far plane
		return PyFloat_FromDouble( s_instance->farPlane() );
	}

	if (nfp != -1) s_instance->farPlane( nfp );

	return Script::getData( s_instance->farPlane() );
}

/**
 *	The static python update method
 */
PyObject * BigBang::py_update( PyObject * args )
{
	float dTime = 0.033f;

	if (!PyArg_ParseTuple( args, "|f", &dTime ))
	{
		PyErr_SetString( PyExc_TypeError, "BigBang.update() "
			"takes only an optional float argument for dtime" );
		return NULL;
	}

	s_instance->update( dTime );

	Py_Return;
}


/**
 *	The static python render method
 */
PyObject * BigBang::py_render( PyObject * args )
{
	float dTime = 0.033f;

	if (!PyArg_ParseTuple( args, "|f", &dTime ))
	{
		PyErr_SetString( PyExc_TypeError, "BigBang.render() "
			"takes only an optional float argument for dtime" );
		return NULL;
	}

	s_instance->render( dTime );

	Py_Return;
}


/**
 *	The static python save method
 */
PyObject * BigBang::py_save( PyObject * args )
{
	s_instance->save();

	Py_Return;
}

/**
 *	The static python quick save method
 */
PyObject * BigBang::py_quickSave( PyObject * args )
{
	s_instance->quickSave();

	Py_Return;
}

SelectionOperation::SelectionOperation( const std::vector<ChunkItemPtr>& before, const std::vector<ChunkItemPtr>& after ) :
	UndoRedo::Operation( 0 ), before_( before ), after_( after )
{
	for( std::vector<ChunkItemPtr>::iterator iter = before_.begin(); iter != before_.end(); ++iter )
		addChunk( (*iter)->chunk() );
	for( std::vector<ChunkItemPtr>::iterator iter = after_.begin(); iter != after_.end(); ++iter )
		addChunk( (*iter)->chunk() );
}

void SelectionOperation::undo()
{
	BigBang::instance().setSelection( before_, false );
	UndoRedo::instance().add( new SelectionOperation( after_, before_ ) );
}

bool SelectionOperation::iseq( const UndoRedo::Operation & oth ) const
{
	// these operations never replace each other
	return false;
}

/**
 *	Notification of current selection
 *	call as selection changes
 */
PyObject * BigBang::py_revealSelection( PyObject * args )
{
	PyObject * p;
	if (PyArg_ParseTuple( args, "O", &p ))
	{
		if (ChunkItemRevealer::Check(p))
		{
			ChunkItemRevealer * revealer = static_cast<ChunkItemRevealer *>( p );
			
			std::vector<ChunkItemPtr> selectedItems = s_instance->selectedItems_;

            std::vector<ChunkItemPtr> newSelection;
			revealer->reveal( newSelection );
            s_instance->selectedItems_.clear();
            for (size_t i = 0; i < newSelection.size(); ++i)
                if (newSelection[i]->edCanAddSelection())
                    s_instance->selectedItems_.push_back(newSelection[i]);

			s_instance->calculateSnaps();

			bool different = selectedItems.size() != s_instance->selectedItems_.size();
			if( !different )
			{
				std::sort( selectedItems.begin(), selectedItems.end() );
				std::sort( s_instance->selectedItems_.begin(), s_instance->selectedItems_.end() );
				different = ( selectedItems != s_instance->selectedItems_ );
			}
			if( different )
			{
				UndoRedo::instance().add( new SelectionOperation( selectedItems, s_instance->selectedItems_ ) );

				if (!s_instance->settingSelection_)
					UndoRedo::instance().barrier( "Selection Change", false );
			}


			//TODO : put back in when page scene is working correctly
			//PageScene::instance().updateSelection( s_instance->selectedItems_ );
		}
	}

	Py_Return;
}

/**
 * If any chunks are in the selection
 */
PyObject * BigBang::py_isChunkSelected( PyObject * args )
{
	return Script::getData( BigBang::instance().isChunkSelected() );
}

/**
 * Select all editable items in all loaded chunks
 */
PyObject * BigBang::py_selectAll( PyObject * args )
{
	ChunkItemRevealer::ChunkItems allItems;
	VeryLargeObject::updateSelectionMark();

	ChunkMap::iterator i = ChunkManager::instance().cameraSpace()->chunks().begin();
	for (; i != ChunkManager::instance().cameraSpace()->chunks().end(); i++)
	{
		std::vector<Chunk*>& chunks = i->second;

		std::vector<Chunk*>::iterator j = chunks.begin();
		for (; j != chunks.end(); ++j)
		{
			Chunk* pChunk = *j;

			if (!pChunk->online() || !EditorChunkCache::instance( *pChunk ).edIsLocked() )
				continue;

			// Add all items in the chunk
			MatrixMutexHolder lock( pChunk );
			std::vector<ChunkItemPtr> chunkItems =
				EditorChunkCache::instance( *pChunk ).staticItems();

			std::vector<ChunkItemPtr>::const_iterator k;
			for (k = chunkItems.begin(); k != chunkItems.end(); ++k)
			{
				ChunkItemPtr item = *k;

				if (!SelectionFilter::canSelect( &*item, true, false ))
					continue;

				DataSectionPtr ds = item->pOwnSect();

				if (ds && ds->sectionName() == "overlapper")
					continue;

				if (ds && ds->sectionName() == "vlo")
				{
					if (!item->edCheckMark(VeryLargeObject::selectionMark()))
						continue;
				}

				allItems.push_back( item );

				// If we selected the shell model, don't select anything else
				if (!pChunk->isOutsideChunk() && k == chunkItems.begin())
					break;
			}
		}
	}

	return new ChunkItemGroup( allItems );
}


/*
 * should the input be listened too?
 */
PyObject * BigBang::py_cursorOverGraphicsWnd( PyObject * args )
{
	return PyInt_FromLong((long)s_instance->cursorOverGraphicsWnd());
}


/**
 *	Import a binary data file into the space.
 *	Currently this only opens .dt2 files and
 *	updates the terrain elevation.
 */
PyObject * BigBang::py_importDataGUI( PyObject * args )
{
    PanelManager::instance()->setToolMode( "TerrainImport" );

	Py_Return;
}


PyObject * BigBang::py_rightClick( PyObject * args )
{
	PyObject * p;
	if (PyArg_ParseTuple( args, "O", &p ))
	{
		if (ChunkItemRevealer::Check(p))
		{
			ChunkItemRevealer * revealer = static_cast<ChunkItemRevealer *>( p );
			std::vector<ChunkItemPtr> items;
			revealer->reveal( items );
			if( items.size() == 1 )
			{
				ChunkItemPtr item = items[0];
				std::vector<std::string> commands = item->edCommand( "" );
				if( commands.size() )
				{
					HMENU menu;

					menu = CreatePopupMenu();
					UINT pos = 0;
					for( std::vector<std::string>::iterator iter = commands.begin();
						iter != commands.end(); ++iter, ++pos )
					{
						MENUITEMINFO info = {
							sizeof( info ),
							MIIM_FTYPE | MIIM_ID | MIIM_STRING, MFT_STRING, MFS_DEFAULT,
							pos + 1, 0, 0, 0, 0, (LPSTR)(*iter).c_str(), 0, 0
						};
						InsertMenuItem( menu, pos, MF_BYPOSITION, &info );
					}
					POINT p;
					GetCursorPos( &p );
					ShowCursor( TRUE );
					int result = TrackPopupMenu( menu, TPM_RETURNCMD | TPM_LEFTBUTTON,
						p.x, p.y, 0, BigBang::instance().hwndGraphics(), NULL );
					if( result )
					{
						item->edExecuteCommand( "", result - 1 );
					}
					DestroyMenu( menu );
				}
			}
		}
	}

	Py_Return;
}


/**
 *	Export binary data from the space.
 */
PyObject * BigBang::py_exportDataGUI( PyObject *args )
{
    PanelManager::instance()->setToolMode( "TerrainImport" );

	Py_Return;
}


//---------------------------------------------------------------------------
/*
static PyObject * py_recalcTerrainShadows( PyObject * args )
{
	ChunkMap& chunks = ChunkManager::instance().cameraSpace()->chunks();

	std::vector<Chunk*> outsideChunks;
	for (ChunkMap::iterator it = chunks.begin(); it != chunks.end(); it++)
	{
		Chunk* pChunk = it->second;

		if (!pChunk->online())
			continue;

		if (!pChunk->isOutsideChunk())
			continue;

		if (!EditorChunkCache::instance( *pChunk ).edIsLocked())
			continue;

		outsideChunks.push_back( pChunk );
	}

	BigBangProgressBar progress;
	ProgressTask terrainShadowsTask( &progress, "Calculating terrain shadows", float(outsideChunks.size()) );

	std::vector<Chunk*>::iterator it = outsideChunks.begin();
	for (; it != outsideChunks.end(); ++it)
	{
		terrainShadowsTask.step();

		EditorChunkTerrain* pEct = static_cast<EditorChunkTerrain*>(
			ChunkTerrainCache::instance( **it ).pTerrain());

		pEct->calculateShadows();
	}

	Py_Return;
}
PY_MODULE_FUNCTION( recalcTerrainShadows, BigBang )
*/

//---------------------------------------------------------------------------

void findRelevantChunks( ToolPtr tool, float buffer = 0.f )
{
	if ( tool->locator() )
	{
		float halfSize = buffer + tool->size() / 2.f;
		Vector3 start( tool->locator()->transform().applyToOrigin() -
						Vector3( halfSize, 0.f, halfSize ) );
		Vector3 end( tool->locator()->transform().applyToOrigin() +
						Vector3( halfSize, 0.f, halfSize ) );

		EditorChunk::findOutsideChunks(
			BoundingBox( start, end ), tool->relevantChunks() );

		tool->currentChunk() =
			EditorChunk::findOutsideChunk( tool->locator()->transform().applyToOrigin() );
	}
}
//---------------------------------------------------------------------------
bool BigBang::changeSpace( const std::string& space, bool reload )
{
	static int id = 1;

	if( currentSpace_ == space )
		return true;

	renderDisabled_ = true;
	if( updatingFiber_ )
	{
		killingUpdatingFiber_ = true;
		while( killingUpdatingFiber_ )
			doBackgroundUpdating();
	}

	if ( romp_ )
		romp_->enviroMinder().deactivate();

	if( !currentSpace_.empty() )
	{
		setSelection( std::vector<ChunkItemPtr>(), false );
		setSelection( std::vector<ChunkItemPtr>(), true );
	}

	ChunkManager::instance().switchToSyncMode( true );

	if( !reload )
	{
		//Clear the message list before loading...
		MsgHandler::instance().clear();
		
		if( conn_ )
		{
			delete conn_;
			conn_ = NULL;
		}
		if ( Options::getOptionBool( "bwlockd/use", true ))
		{
			std::string host = Options::getOptionString( "bwlockd/host" );
			std::string username = Options::getOptionString( "bwlockd/username" );
			if( !host.empty() && username.empty() )
			{
				char name[1024];
				DWORD size = 1024;
				GetUserName( name, &size );
				username = name;
			}
			if( !host.empty() && !username.empty() )
			{
				std::string hostname = host.substr( 0, host.find( ':' ) );
				std::string msg = "Connecting to BigWorld lock server " + hostname + "...";
				WaitDlg::show( msg );
				static const int xExtent = (int)( ( MAX_TERRAIN_SHADOW_RANGE + 1.f ) / GRID_RESOLUTION );
				conn_ = new BWLock::BigBangdConnection( host, space, username, xExtent, 1 );
			}
			connection();
		}
	}

	ChunkOverlapper::drawList.clear();

	BWResource::instance().purgeAll();
	workingChunk( NULL );
	ChunkManager::instance().clearAllSpaces();
	ChunkManager::instance().camera( Matrix::identity, NULL );

	ChunkSpacePtr chunkSpace = ChunkManager::instance().space( id );
	++id;
	mapping_ = chunkSpace->addMapping( SpaceEntryID(), Matrix::identity, space );
	if( !mapping_ )
	{
		spaceManager_->removeSpaceFromRecent( space );
		ChunkManager::instance().switchToSyncMode( false );
		renderDisabled_ = false;
		return false;
	}

	currentSpace_ = space;

	if( !reload )
		PanelManager::instance()->setToolMode( "Objects" );

	resetChangedLists();

	ChunkManager::instance().switchToSyncMode( false );
	ChunkManager::instance().camera( Matrix::identity, chunkSpace );
	ChunkManager::instance().tick( 0.f );

	ToolManager::instance().changeSpace( getWorldRay( currentCursorPosition() ) );

	DataSectionPtr spaceSection = BWResource::openSection( space + "/space.localsettings" );
	if( !reload && spaceSection )
	{
		Vector3 dir, pos;
		pos = spaceSection->readVector3( "startPosition", Vector3( 0.f, 2.f, 0.f ) );
		dir = spaceSection->readVector3( "startDirection" );
		Matrix m;
		m.setIdentity();
		m.setRotate( dir[2], dir[1], dir[0] );
		m.translation( pos );
		m.invert();
		Moo::rc().view( m );
	}

	// set the window title to the current space name
	BigBang2Doc::instance().SetTitle( space.c_str() );

	spaceManager_->addSpaceIntoRecent( space );

	if( &BigBangCamera::instance() )
		BigBangCamera::instance().respace( Moo::rc().view() );

	if ( romp_ )
		romp_->enviroMinder().activate();
	Flora::floraReset();
	UndoRedo::instance().clear();

	updateRecentFile();

	if( !reload )
		PanelManager::instance()->setDefaultToolMode();

    secsPerHour_ = romp_->timeOfDay()->secondsPerGameHour();

	romp_->changeSpace();

	update( 0.f );

    PanelManager::instance()->onNewSpace();

	DataSectionPtr pDS = BWResource::openSection( currentSpace_ + "/space.settings" );	
	if (pDS)
	{
		int minX = pDS->readInt( "bounds/minX", 1 );
		int minY = pDS->readInt( "bounds/minY", 1 );
		int maxX = pDS->readInt( "bounds/maxX", -1 );
		int maxY = pDS->readInt( "bounds/maxY", -1 );

		SpaceMap::instance().spaceInformation( space, GridCoord( minX, minY ),
			maxX - minX + 1, maxY - minY + 1 );
	}

	pDS = BWResource::openSection( Options::getOptionString( "space/mru0" ) + '/' +
		"space.localsettings" );

	nonloadedDirtyLightingChunks_.clear();
	nonloadedDirtyTerrainShadowChunks_.clear();
	nonloadedDirtyThumbnailChunks_.clear();
	dirtyVloReferences_.clear();
	if (pDS)
	{
		std::vector<DataSectionPtr> chunks;
		pDS->openSections( "dirtylighting", chunks );
		for (std::vector<DataSectionPtr>::iterator i = chunks.begin(); i != chunks.end(); ++i)
		{
			nonloadedDirtyLightingChunks_.insert( (*i)->asString() );
		}

		chunks.clear();
		pDS->openSections( "dirtyterrain", chunks );
		for (std::vector<DataSectionPtr>::iterator i = chunks.begin(); i != chunks.end(); ++i)
		{
			nonloadedDirtyTerrainShadowChunks_.insert( (*i)->asString() );
		}

		chunks.clear();
		pDS->openSections( "dirtythumbnail", chunks );
		for (std::vector<DataSectionPtr>::iterator i = chunks.begin(); i != chunks.end(); ++i)
		{
			if( *(*i)->asString().rbegin() == 'o' )
				nonloadedDirtyThumbnailChunks_.insert( (*i)->asString() );
		}

		chunks.clear();
		pDS->openSections( "dirtyVLO", chunks );
		for (std::vector<DataSectionPtr>::iterator i = chunks.begin(); i != chunks.end(); ++i)
		{
			dirtyVloReferences_[ (*i)->asString() ].insert(
				std::make_pair( (*i)->readString( "type" ), (*i)->readString( "id" ) ) );
		}
	}

	renderDisabled_ = false;
	return true;
}

bool BigBang::changeSpace( GUI::ItemPtr item )
{
	if( !canClose( "C&hange Space" ) )
		return false;
	std::string space = spaceManager_->browseForSpaces( hwndInput_ );
	space = BWResource::dissolveFilename( space );
	if( !space.empty() )
		return changeSpace( space, false );
	return false;
}

bool BigBang::newSpace( GUI::ItemPtr item )
{
	if( !canClose( "C&hange Space" ) )
		return false;
	NewSpaceDlg dlg;
	bool result = ( dlg.DoModal() == IDOK );
	if( result && dlg.mChangeSpace )
		result = changeSpace( (LPCTSTR)dlg.createdSpace(), false );
	return result;
}

bool BigBang::recentSpace( GUI::ItemPtr item )
{
	if( !canClose( "C&hange Space" ) )
		return false;
	std::string spaceName = (*item)[ "spaceName" ];
	if( *spaceName.rbegin() != '/' )
		spaceName += '/';
	spaceName += "space.settings";
	if( !BWResource::fileExists( spaceName ) )
	{
		spaceManager_->removeSpaceFromRecent( (*item)[ "spaceName" ] );
		updateRecentFile();
		return false;
	}
	return changeSpace( (*item)[ "spaceName" ], false );
}

bool BigBang::doReloadAllTextures( GUI::ItemPtr item )
{
	AfxGetApp()->DoWaitCursor( 1 );
	Moo::TextureManager::instance()->reloadAllTextures();
	AfxGetApp()->DoWaitCursor( 0 );
	return true;
}

bool BigBang::doReloadAllChunks( GUI::ItemPtr item )
{
	AfxGetApp()->DoWaitCursor( 1 );
	reloadAllChunks( true );
	AfxGetApp()->DoWaitCursor( 0 );
	return true;
}

bool BigBang::doExit( GUI::ItemPtr item )
{
	AfxGetApp()->GetMainWnd()->PostMessage( WM_COMMAND, ID_APP_EXIT );
	return true;
}

void BigBang::updateRecentFile()
{
	GUI::ItemPtr recentFiles = ( *GUI::Manager::instance() )( "/MainMenu/File/RecentFiles" );
	if( recentFiles )
	{
		while( recentFiles->num() )
			recentFiles->remove( 0 );
		for( unsigned int i = 0; i < spaceManager_->num(); ++i )
		{
			std::stringstream name, displayName;
			name << "mru" << i;
			displayName << '&' << i << "  " << spaceManager_->entry( i );
			GUI::ItemPtr item = new GUI::Item( "ACTION", name.str(), displayName.str(),
				"",	"", "", "recentSpace", "", "" );
			item->set( "spaceName", spaceManager_->entry( i ) );
			recentFiles->add( item );
		}
	}
}

bool BigBang::touchAllChunk( GUI::ItemPtr item )
{
	std::string spacePath = BWResource::resolveFilename( chunkDirMapping()->path() );
	if( *spacePath.rbegin() != '\\' )
		spacePath += '\\';

	std::set<std::string> loaded;
	EditorChunkCache::lock();
	for( std::set<Chunk*>::iterator iter = EditorChunkCache::chunks_.begin();
		iter != EditorChunkCache::chunks_.end(); ++iter )
	{
		loaded.insert( (*iter)->identifier() );
		if( (*iter)->isOutsideChunk() )
			changedTerrainShadows( *iter );
		else
			changedChunkLighting( *iter );
		EditorChunkCache::instance( *(*iter) ).navmeshDirty( true );
		dirtyThumbnail( *iter );
	}
	EditorChunkCache::unlock();

    std::set<std::string> chunks = Utilities::gatherFilesInDirectory(
			BWResource::resolveFilename( chunkDirMapping()->path() ),
			"*.chunk" );

	nonloadedDirtyTerrainShadowChunks_.clear();
	nonloadedDirtyLightingChunks_.clear();
	nonloadedDirtyThumbnailChunks_.clear();

	for( std::set<std::string>::iterator iter = chunks.begin();
		iter != chunks.end(); ++iter )
	{
		if( loaded.find( *iter ) != loaded.end() )
			continue;
		if( *iter->rbegin() == 'o' || *iter->rbegin() == 'O' )
		{
			nonloadedDirtyTerrainShadowChunks_.insert( *iter );
			nonloadedDirtyThumbnailChunks_.insert( *iter );
		}
		else
			nonloadedDirtyLightingChunks_.insert( *iter );
	}

	return true;
}

bool BigBang::clearUndoRedoHistory( GUI::ItemPtr item )
{
	UndoRedo::instance().clear();
	return true;
}

unsigned int BigBang::updateUndo( GUI::ItemPtr item )
{
	return UndoRedo::instance().canUndo();
}

unsigned int BigBang::updateRedo( GUI::ItemPtr item )
{
	return UndoRedo::instance().canRedo();
}

bool BigBang::doExternalEditor( GUI::ItemPtr item )
{
	if( selectedItems_.size() == 1 )
		selectedItems_[ 0 ]->edExecuteCommand( "", 0 );
	return true;
}

unsigned int BigBang::updateExternalEditor( GUI::ItemPtr item )
{
	return selectedItems_.size() == 1 && !selectedItems_[ 0 ]->edCommand( "" ).empty();
}

std::string BigBang::get( const std::string& key ) const
{
	return Options::getOptionString( key );
}

bool BigBang::exist( const std::string& key ) const
{
	return Options::optionExists( key );
}

void BigBang::set( const std::string& key, const std::string& value )
{
	Options::setOptionString( key, value );
}

std::string BigBang::getCurrentSpace() const
{
	return currentSpace_;
}

void BigBang::showBusyCursor()
{
    // Set the cursor to the arrow + hourglass if there are not yet any loaded
    // chunks, or reset it to the arrow cursor if we were displaying the wait
    // cursor.
    EditorChunkCache::lock();
	bool loadedChunk = EditorChunkCache::chunks_.size() > 0;
	EditorChunkCache::unlock();
    if (waitCursor_ || !loadedChunk)
    {
        BigBang::instance().setCursor
        (
            loadedChunk 
                ? ::LoadCursor(NULL, IDC_ARROW)
                : ::LoadCursor(NULL, IDC_APPSTARTING)
        );
        waitCursor_ = !loadedChunk;
    }
}

unsigned int BigBang::getMemoryLoad()
{
	MEMORYSTATUSEX memoryStatus = { sizeof( memoryStatus ) };
	GlobalMemoryStatusEx( &memoryStatus );
	DWORDLONG cap = memoryStatus.ullTotalVirtual - 300 * 1024 * 1024; //  300M room gives some sense of safety
	if( cap > memoryStatus.ullTotalPhys * 2 )
		cap = memoryStatus.ullTotalPhys * 2;
	PROCESS_MEMORY_COUNTERS pmc = { sizeof( pmc ) };
	GetProcessMemoryInfo( GetCurrentProcess(), &pmc, sizeof( pmc ) );

	DWORDLONG used = pmc.PagefileUsage;
	if( used > cap )
		used = cap;
	return unsigned int( used * 100 / cap );
}

extern int Math_token;
extern int GUI_token;
extern int ResMgr_token;
static int s_moduleTokens =
	Math_token |
	GUI_token |
	ResMgr_token;
