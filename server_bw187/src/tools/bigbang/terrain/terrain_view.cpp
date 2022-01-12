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
#include "terrain_view.hpp"
#include "terrain_functor.hpp"
#include "../terrain/editor_chunk_terrain.hpp"
#include "../terrain/editor_chunk_terrain_projector.hpp"
#include "appmgr/options.hpp"
#include "ashes/simple_gui_component.hpp"
#include "ashes/text_gui_component.hpp"
#include "ashes/simple_gui.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/auto_config.hpp"


//----------------------------------------------------
//	Section : TerrainTextureToolView
//----------------------------------------------------


#undef PY_ATTR_SCOPE
#define PY_ATTR_SCOPE TerrainTextureToolView::

PY_TYPEOBJECT( TerrainTextureToolView )

PY_BEGIN_METHODS( TerrainTextureToolView )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( TerrainTextureToolView )
	PY_ATTRIBUTE( rotation )
	PY_ATTRIBUTE( showHoles )
PY_END_ATTRIBUTES()

PY_FACTORY_NAMED( TerrainTextureToolView, "TerrainTextureToolView", View )

VIEW_FACTORY( TerrainTextureToolView )

/**
 *	Constructor.
 */
TerrainTextureToolView::TerrainTextureToolView(
		const std::string& resourceID,
		PyTypePlus * pType )
: TextureToolView( resourceID, pType ),
  rotation_( 0.f ),
  showHoles_( false )
{
}


/**
 *	This method renders the terrain texture tool, by projecting
 *	the texture onto the relevant terrain chunks as indicated
 *	in tool.relevantChunks()
 *
 *	@param	tool	The tool we are viewing.
 */
void TerrainTextureToolView::render( const Tool& tool )
{
	EditorChunkTerrainPtrs spChunks;

	ChunkPtrVector::const_iterator it = tool.relevantChunks().begin();
	ChunkPtrVector::const_iterator end = tool.relevantChunks().end();

	while ( it != end )
	{
		Chunk* pChunk = *it++;
		
		EditorChunkTerrain* pEct = static_cast<EditorChunkTerrain*>(
			ChunkTerrainCache::instance( *pChunk ).pTerrain());

		if (pEct != NULL)
		{
			spChunks.push_back( pEct );
		}
	}

	EditorChunkTerrainProjector::instance().projectTexture(
			pTexture_,
			tool.size(),
			rotation_,			
			tool.locator()->transform().applyToOrigin(),
			false,
			spChunks,
			showHoles_ );
}


/**
 *	Get an attribute for python
 */
PyObject * TerrainTextureToolView::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();

	return TextureToolView::pyGetAttribute( attr );
}


/**
 *	Set an attribute for python
 */
int TerrainTextureToolView::pySetAttribute( const char * attr, PyObject * value )
{
	PY_SETATTR_STD();

	return TextureToolView::pySetAttribute( attr, value );
}


/**
 *	Static python factory method
 */
PyObject * TerrainTextureToolView::pyNew( PyObject * args )
{
	char * textureName;
	if (!PyArg_ParseTuple( args, "|s", &textureName ))
	{
		PyErr_SetString( PyExc_TypeError, "View.TerrainTextureToolView: "
			"Argument parsing error: Expected an optional texture name" );
		return NULL;
	}

	if ( textureName != NULL )
		return new TerrainTextureToolView( textureName );
	else
		return new TerrainTextureToolView( "resources/maps/gizmo/disc.dds" );
}


//----------------------------------------------------
//	Section : TerrainChunkTextureToolView
//----------------------------------------------------

#undef PY_ATTR_SCOPE
#define PY_ATTR_SCOPE TerrainChunkTextureToolView::

PY_TYPEOBJECT( TerrainChunkTextureToolView )

PY_BEGIN_METHODS( TerrainChunkTextureToolView )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( TerrainChunkTextureToolView )
	PY_ATTRIBUTE( numPerChunk )
PY_END_ATTRIBUTES()

PY_FACTORY_NAMED( TerrainChunkTextureToolView, "TerrainChunkTextureToolView", View )

VIEW_FACTORY( TerrainChunkTextureToolView )

/**
 *	constructor.
 */
TerrainChunkTextureToolView::TerrainChunkTextureToolView(
	const std::string& resourceID,
	PyTypePlus * pType ):
		TextureToolView( resourceID, pType ),
	numPerChunk_( 1.f )
{
}


/**
 *	This method renders the terrain texture chunk tool.  It does
 *	this by projecting a texture onto the terrain, making sure
 *	the entire chunk the tool is in is covered, even if the tool
 *	does not fill an entire chunk.
 *
 *	Similarly, if the tool overlaps multiple chunks, then those
 *	chunks will have the texture overlayed.
 *
 *	Some tool locators don't bother filling out the set of relevant
 *	chunks, for example the LineLocator.  If this is the case, then
 *	we just draw on the chunk the tool is in, using currentChunk()
 *
 *	@param	tool	The tool we are viewing.
 */
void TerrainChunkTextureToolView::render( const Tool& tool )
{
	EditorChunkTerrainPtrs spChunks;

	if ( tool.relevantChunks().size() )
	{

		ChunkPtrVector::const_iterator it = tool.relevantChunks().begin();
		ChunkPtrVector::const_iterator end = tool.relevantChunks().end();

		/*material_.begin();
		for ( uint32 i=0; i<material_.nPasses(); i++ )
		{
			material_.beginPass(i);
			Moo::rc().setSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
			Moo::rc().setSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );*/

			while ( it != end )
			{
				Chunk* pChunk = *it++;
				
				EditorChunkTerrain* pEct = static_cast<EditorChunkTerrain*>(
					ChunkTerrainCache::instance( *pChunk ).pTerrain());

				if (pEct != NULL)
				{
					spChunks.push_back( pEct );

					EditorChunkTerrainProjector::instance().projectTexture(
						pTexture_,
						100.f / numPerChunk_,
						0.f,						
						pChunk->transform().applyToOrigin() + Vector3( 50.f, 0.f, 50.f ),
						true,
						spChunks,
						false );

					spChunks.clear();
				}
			}
			/*material_.endPass();
		}
		material_.end();*/
	}
	else if ( tool.currentChunk() )
	{
		/*material_.begin();
		for ( uint32 i=0; i<material_.nPasses(); i++ )
		{
			material_.beginPass(i);*/
			Chunk* pChunk = tool.currentChunk();

			EditorChunkTerrain* pEct = static_cast<EditorChunkTerrain*>(
					ChunkTerrainCache::instance( *pChunk ).pTerrain());

			if (pEct)
			{
				spChunks.push_back( pEct );

				EditorChunkTerrainProjector::instance().projectTexture(
					pTexture_,
					100.f / numPerChunk_,
					0.f,					
					pChunk->transform().applyToOrigin() + Vector3( 50.f, 0.f, 50.f ),
					false,
					spChunks,
					false );
			}
			/*material_.endPass();
		}
		material_.end();*/
	}
}


/**
 *	Static python factory method
 */
PyObject * TerrainChunkTextureToolView::pyNew( PyObject * args )
{
	char * textureName;
	if (!PyArg_ParseTuple( args, "|s", &textureName ))
	{
		PyErr_SetString( PyExc_TypeError, "View.TerrainChunkTextureToolView: "
			"Argument parsing error: Expected an optional texture name" );
		return NULL;
	}

	if ( textureName != NULL )
		return new TerrainChunkTextureToolView( textureName );
	else
		return new TerrainChunkTextureToolView( "resources/maps/gizmo/square.dds" );
}


/**
 *	Get an attribute for python
 */
PyObject * TerrainChunkTextureToolView::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();

	return TextureToolView::pyGetAttribute( attr );
}


/**
 *	Set an attribute for python
 */
int TerrainChunkTextureToolView::pySetAttribute( const char * attr, PyObject * value )
{
	PY_SETATTR_STD();

	return TextureToolView::pySetAttribute( attr, value );
}
