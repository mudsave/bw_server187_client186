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
#include "appmgr/options.hpp"
#include "cstdmf/debug.hpp"
#include "chunk_photographer.hpp"
#include "chunk/chunk.hpp"
#include "chunk/chunk_water.hpp"
#include "chunk/chunk_flare.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_space.hpp"
#include "moo/render_context.hpp"
#include "moo/terrain_renderer.hpp"
#include "moo/texture_compressor.hpp"
#include "moo/visual_channels.hpp"
#include "duplo/py_splodge.hpp"
#include "romp/rain.hpp"
#include "chunks/editor_chunk.hpp"
#include "editor_chunk_link.hpp"

DECLARE_DEBUG_COMPONENT2( "WorldEditor", 0 )

ChunkPhotographer& ChunkPhotographer::instance()
{
	static ChunkPhotographer s_instance;
	return s_instance;
}


bool ChunkPhotographer::photograph( Chunk& chunk )
{
	return ChunkPhotographer::instance().takePhoto( chunk );
}


Moo::LightContainerPtr ChunkPhotographer::s_lighting_ = NULL;
OutsideLighting ChunkPhotographer::s_chunkLighting_;
extern bool g_disableSkyLightMap;


ChunkPhotographer::ChunkPhotographer():
width_( 128 ),
height_( 128 ),
pRT_( NULL ),
savedLighting_( NULL )
{
}


/**
 *	This method photographs a chunk.
 */
bool ChunkPhotographer::takePhoto( Chunk& chunk )
{
	// make the render target if necessary
	if (!pRT_)
	{
		pRT_ = new Moo::RenderTarget( "TextureRenderer" );
		pRT_->create( width_, height_ );
	}

	if (!pRT_)
	{
		WARNING_MSG( "ChunkPhotographer::takePhoto - failed because render target was not created\n" );
		return false;
	}

	this->beginScene();
	this->setStates(chunk);
	this->renderScene();
	this->resetStates();
	this->endScene();

	TextureCompressor tc( static_cast<DX::Texture*>(pRT_->pTexture()), D3DFMT_DXT1, 1 );
	if (Moo::rc().device()->TestCooperativeLevel() == D3D_OK)
	{
		tc.stow( EditorChunkCache::instance(chunk).pCDataSection(), "thumbnail.dds" );
		BigBang::instance().changedThumbnail( &chunk );
		return true;
	}
	else
	{
		return false;
	}
}


void ChunkPhotographer::beginScene()
{
	// set and clear the render target
	pRT_->push();
	Moo::rc().beginScene();
	if (Moo::rc().mixedVertexProcessing())
		Moo::rc().device()->SetSoftwareVertexProcessing( TRUE );
	Moo::rc().device()->Clear(
		0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff808080, 1.f, 0 );
}


void ChunkPhotographer::setStates( Chunk& chunk )
{
	//first, setup some rendering options.
	//we need to draw as cheaply as possible, and
	//not clobber the main pipeline.

	Waters::simulationEnabled(false);
	Waters::drawRefraction(false);
	Waters::drawReflection(false);

	ChunkFlare::ignore( true );
    EditorChunkLink::enableDraw( false );
    EditorChunkStationNode::enableDraw( false );
	PySplodge::s_ignoreSplodge_ = true;
	Rain::disable( true );
	g_disableSkyLightMap = true;    

	//save some states we are about to change
	oldFOV_ = Moo::rc().camera().fov();
	savedLighting_ = Moo::rc().lightContainer();
	savedChunkLighting_ = &ChunkManager::instance().cameraSpace()->enviro().timeOfDay()->lighting();
	oldInvView_ = Moo::rc().invView();

	//create some lighting
	if ( !s_lighting_ )
	{
		Vector4 ambientColour( 0.08f,0.02f,0.1f,1.f );

		//outside lighting for chunks
		s_chunkLighting_.sunTransform.setRotateX( DEG_TO_RAD(90.f) );
		s_chunkLighting_.sunTransform.postRotateZ( DEG_TO_RAD(20.f) );
		s_chunkLighting_.sunColour.set( 1.f, 1.f, 1.f, 1.f );		
		s_chunkLighting_.ambientColour = ambientColour;

		//light container for terrain
		Moo::DirectionalLightPtr spDirectional = new Moo::DirectionalLight(
			Moo::Colour(1,1,1,1), Vector3(0,0,-1.f) );
		spDirectional->worldTransform( s_chunkLighting_.sunTransform );

		s_lighting_ = new Moo::LightContainer;
		s_lighting_->ambientColour( Moo::Colour( ambientColour ) );
		s_lighting_->addDirectional( spDirectional );
	}

	Moo::rc().lightContainer( s_lighting_ );

	//setup the correct transform for the given chunk.
	//adds of .25 is for the near clipping plane.
	std::vector<ChunkItemPtr> items;
	EditorChunkCache::instance(chunk).allItems( items );
	BoundingBox bb( Vector3::zero(), Vector3::zero() );
	for( std::vector<ChunkItemPtr>::iterator iter = items.begin(); iter != items.end(); ++iter )
		(*iter)->addYBounds( bb );
	Matrix view;
	Vector3 lookFrom( chunk.transform().applyToOrigin() );	
	lookFrom.x += GRID_RESOLUTION / 2.f;
	lookFrom.z += GRID_RESOLUTION / 2.f;
	lookFrom.y = bb.maxBounds().y + 0.25f + 300.f;

	float chunkHeight = bb.maxBounds().y - bb.minBounds().y + 320.f;
	view.lookAt( lookFrom, Vector3(0,-1,0), Vector3(0,0,1) );
	Moo::rc().push();
	Moo::rc().world( Matrix::identity );
	Matrix proj;
	proj.orthogonalProjection( GRID_RESOLUTION, GRID_RESOLUTION, 0.25f, chunkHeight + 0.25f );
	Moo::rc().view( view );
	Moo::rc().projection( proj );
	Moo::rc().updateViewTransforms();
	Moo::TerrainRenderer::instance().enableSpecular( false );

	//make sure there are no states set into the main part of bigbang
	//that could upset the rendering
	Moo::rc().setRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
	Moo::rc().setRenderState( D3DRS_FOGENABLE, FALSE );
	FogController::instance().enable( false );	
}


void ChunkPhotographer::renderScene()
{
	Moo::rc().lightContainer( s_lighting_ );
	ChunkManager::instance().camera( Moo::rc().invView(),
		ChunkManager::instance().cameraSpace() );
	ChunkManager::instance().cameraSpace()->heavenlyLightSource( &s_chunkLighting_ );
	ChunkManager::instance().cameraSpace()->updateHeavenlyLighting();	
	ChunkManager::instance().draw();

	//turn off alpha writes, because we are saving in DXT1 format ( one-bit alpha )
	//and the terrain normally uses alpha channel encoding, which upsets the bitmap.
	//not sure why the terrain has to output its alpha, but hell this is a workaround.
	Moo::rc().setRenderState(D3DRS_COLORWRITEENABLE,D3DCOLORWRITEENABLE_BLUE|D3DCOLORWRITEENABLE_RED|D3DCOLORWRITEENABLE_GREEN);
	Moo::rc().lightContainer( s_lighting_ );
	Moo::TerrainRenderer::instance().draw();
	Moo::rc().setRenderState(D3DRS_COLORWRITEENABLE,D3DCOLORWRITEENABLE_BLUE|D3DCOLORWRITEENABLE_RED|D3DCOLORWRITEENABLE_GREEN|D3DCOLORWRITEENABLE_ALPHA);

	// Draw water
	Waters::instance().projectView(true);	
	Waters::instance().drawDrawList( 0.f );
	Waters::instance().projectView(false);

	Moo::SortedChannel::draw();
}


void ChunkPhotographer::resetStates()
{
	//set the stuff back
	Moo::rc().lightContainer( savedLighting_ );
	ChunkManager::instance().camera( oldInvView_,
		ChunkManager::instance().cameraSpace() );
	ChunkManager::instance().cameraSpace()->heavenlyLightSource( savedChunkLighting_ );
	Moo::rc().camera().fov( oldFOV_ );
	Moo::rc().updateProjectionMatrix();
	Moo::rc().pop();
	Moo::TerrainRenderer::instance().enableSpecular( true );

	//restore drawing states!
	ChunkFlare::ignore( false );
    EditorChunkLink::enableDraw( true );
    EditorChunkStationNode::enableDraw( true );
	PySplodge::s_ignoreSplodge_ = false;
	Rain::disable( false );
	g_disableSkyLightMap = false;
}


void ChunkPhotographer::endScene()
{
	// and reset everything
	Moo::rc().endScene();
	pRT_->pop();
}