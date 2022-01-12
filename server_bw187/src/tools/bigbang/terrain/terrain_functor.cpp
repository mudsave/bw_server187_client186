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
#include "terrain_functor.hpp"
#include "input/input.hpp"
#include "gizmo/tool.hpp"
#include "../big_bang.hpp"
#include "appmgr/options.hpp"
#include "appmgr/commentary.hpp"
#include "editor_chunk_terrain.hpp"
#include "romp/flora.hpp"
#include "cstdmf/debug.hpp"
#include "../big_bang_progress_bar.hpp"
#include "gizmo/undoredo.hpp"
#include "resmgr/bwresource.hpp"
#include "chunks/editor_chunk.hpp"
#include "resmgr/auto_config.hpp"

static AutoConfigString s_notFoundTexture( "system/notFoundBmp" );

DECLARE_DEBUG_COMPONENT2( "WorldEditor", 2 )


//----------------------------------------------------
//	Section : MouseDragHandler
//----------------------------------------------------


/** 
 *	MouseDragHandler Constructor, inits all member variables.
 */
MouseDragHandler::MouseDragHandler() :
	draggingLeftMouse_(false),
	draggingMiddleMouse_(false),
	draggingRightMouse_(false)
{
}


/** 
 *	This private method is used to update the internal mouse button state
 *	variables to track the mouse dragging properly.
 *	@param currentState	contains the current mouse button state
 *	@param stateVar		a ref. to the corresponding state var. to be updated
 *	@return stateVar	returns the resulting state after the update
 *	@see isDragging()
 */
bool MouseDragHandler::updateDragging( const bool currentState, bool& stateVar )
{
	stateVar = ( !currentState ? false : stateVar );

	return stateVar;
}

/** 
 *	This method is called by a client class with "true" when it receives a 
 *	Mouse Key Down event, and is called with "false" when it receives the
 *	Mouse Key Up event top end dragging. NOTE: The "isDragging" method also
 *	updates state variables in case the mouse button was released outside the
 *	client area of the window.
 *	@param event	Contains the mouse button pressed or released
 *	@param tool		Tool to use. Not used in this method
 *	@return false	It only looks at mouse key events to update drag.
 *	@see isDragging()
 */
void MouseDragHandler::setDragging( const draggingEnum key, const bool val )
{
	switch ( key )
	{
	case KEY_LEFTMOUSE:
		draggingLeftMouse_ = val;
		break;

	case KEY_MIDDLEMOUSE:
		draggingMiddleMouse_ = val;
		break;

	case KEY_RIGHTMOUSE:
		draggingRightMouse_ = val;
		break;
	}
}

/** 
 *	This method tells if the user is doing a valid drag in the window. A valid
 *	drag is started when the user presses down a mouse button in the window and
 *	moves the mouse inside the window without releasing the button. To start
 *	a drag, a client class calls "setDragging" with "true" when it receives a 
 *	Mouse Key Down event, and calls it with "false" when it receives the
 *	Mouse Key Up event top end dragging.
 *	NOTE: this function also updates state variables in case the mouse button
 *	was released outside the client area of the window.
 *	@param event	KeyEvent containing the mouse button pressed or released
 *	@param tool		Tool to use. Not used in this method
 *	@return false	It only looks at mouse key events to update drag.
 *	@see setDragging()
 */
bool MouseDragHandler::isDragging( const draggingEnum key )
{
	bool mouseKey = false;

	switch ( key )
	{
	case KEY_LEFTMOUSE:
		mouseKey = updateDragging(
					InputDevices::isKeyDown( KeyEvent::KEY_LEFTMOUSE ),
					draggingLeftMouse_);		
		break;

	case KEY_MIDDLEMOUSE:
		mouseKey = updateDragging(
					InputDevices::isKeyDown( KeyEvent::KEY_MIDDLEMOUSE ),
					draggingMiddleMouse_);		
		break;

	case KEY_RIGHTMOUSE:
		mouseKey = updateDragging(
					InputDevices::isKeyDown( KeyEvent::KEY_RIGHTMOUSE ),
					draggingRightMouse_);		
		break;
	}

	return mouseKey;
}



//----------------------------------------------------
//	Section : HeightPoleFunctor ( Base class )
//----------------------------------------------------
HeightPoleFunctor::HeightPoleFunctor( PyTypePlus * pType ):
	ToolFunctor( pType )
{
}


/** 
 *	This method handles mouse events and saves the state of the mouse buttons
 *  for dragging/painting to work with the tools, using the dragHandler member
 *	@param event	KeyEvent containing the mouse button pressed or released
 *	@param tool		Tool to use. Not used in this method
 *	@return false	It only looks at mouse key events to update drag.
 */
bool HeightPoleFunctor::handleKeyEvent( const KeyEvent & event, Tool& tool ) 
{
	if( InputDevices::isAltDown() )
		return false;
	switch ( event.key() )
	{
	case KeyEvent::KEY_LEFTMOUSE:
		dragHandler.setDragging(
						MouseDragHandler::KEY_LEFTMOUSE,
						event.isKeyDown() );
		if( event.isKeyDown() )
		{
			ChunkPtrVector& chunks = tool.relevantChunks();

			ChunkPtrVector::iterator it = chunks.begin();
			ChunkPtrVector::iterator end = chunks.end();

			// bail if any of the chunks aren't locked
			while ( it != end )
			{
				Chunk* pChunk = *it++;

				if (!EditorChunkCache::instance( *pChunk ).edIsWriteable() )
				{
					BigBang::instance().addCommentaryMsg(
						"Could not edit terrain of non-locked chunk. Please go to Project panel to lock it before editing." );
					break;
				}
			}
		}
		break;
	case KeyEvent::KEY_MIDDLEMOUSE:
		dragHandler.setDragging(
						MouseDragHandler::KEY_MIDDLEMOUSE,
						event.isKeyDown() );
		break;
	case KeyEvent::KEY_RIGHTMOUSE:
		dragHandler.setDragging(
						MouseDragHandler::KEY_RIGHTMOUSE,
						event.isKeyDown() );
		break;
	}
	return false;
}


/**
 *	This method gets all the relevant height poles, and calls the
 *	virtual HeightPoleFunction() for each.
 *
 *	@param tool		the tool to use.
 *	@param allChunkVertices	true to iterate over all vertices in
 *					the relevant chunks, false to iterator over
 *					all vertices covered by the square region underneath
 *					the tool's location.
 */
void HeightPoleFunctor::apply( Tool& tool, bool allChunkVertices )
{
	ChunkPtrVector& chunks = tool.relevantChunks();

	ChunkPtrVector::iterator it = chunks.begin();
	ChunkPtrVector::iterator end = chunks.end();

	// bail if any of the chunks aren't locked
	while ( it != end )
	{
		Chunk* pChunk = *it++;

		if (!EditorChunkCache::instance( *pChunk ).edIsWriteable() )
			return;
	}



	it = chunks.begin();
	while ( it != end )
	{
		Chunk* pChunk = *it++;

		EditorChunkTerrain* pEct = static_cast<EditorChunkTerrain*>(
			ChunkTerrainCache::instance( *pChunk ).pTerrain());

		if (!pEct)
			continue;

		perChunkFunction( *pEct );

		Vector3 offset = tool.locator()->transform().applyToOrigin() -
							pChunk->transform().applyToOrigin();
		

		int32 polesWidth = pEct->block().polesWidth();
		int32 blocksWidth = pEct->block().blocksWidth();
		float spacing = pEct->block().spacing();

		int32 xi;
		int32 zi;
		int32 numX;

		if ( allChunkVertices )
		{
			xi = -1 + polesWidth/2;
			zi = -1 + polesWidth/2;
			numX = polesWidth;
		}
		else
		{
			xi = (int) floorf( (offset.x / spacing) + 0.5f );
			zi = (int) floorf( (offset.z / spacing) + 0.5f );
			numX = (int) floorf( tool.size() / spacing );
		}

		for (int32 x = max(int32(-1),xi-numX/int32(2));
			x <= min(blocksWidth+1,xi+numX/2);
			x++)
		{
			for (int32 z = max(int32(-1),zi-numX/int32(2));
				z <= min(blocksWidth+1,zi+numX/2);
				z++)
			{
				//Get the affected height pole
				Moo::TerrainBlock::Pole pole(pEct->block(), x, z );

				//Get the pole's relative position to the brush.
				//Currently, the y-value is zeroed out.
				Vector3 relPos( ((float)x) * spacing, 0.f, ((float)z) * spacing );
				relPos -= offset;
				relPos[1] = 0.f;

				perHeightPoleFunction( pole, relPos, x, z );
			}
		}
	}

	this->onFinish( tool );

	it = chunks.begin();
	while ( it != chunks.end() )
	{
		Chunk* pChunk = *it++;

		EditorChunkTerrain* pEct = static_cast<EditorChunkTerrain*>(
			ChunkTerrainCache::instance( *pChunk ).pTerrain());

		if (!pEct)
			continue;

		BigBang::instance().changedTerrainBlock( pChunk );

		pEct->block().deleteManagedObjects();
		pEct->block().createManagedObjects();
	}
}


//----------------------------------------------------
//	Section : PoleUndo
//----------------------------------------------------
class HeightBarrier : public UndoRedo::Operation
{
public:
	typedef HeightBarrier This;

	HeightBarrier() : UndoRedo::Operation( int(typeid(This).name()) )
	{}

	virtual void undo()
	{
		// first add the current state of this block to the undo/redo list
		UndoRedo::instance().add( new This() );

		for( std::vector<ChunkPtr>::iterator iter = chunks_.begin();
			iter != chunks_.end(); ++iter )
		{
			EditorChunkTerrain* pEct = static_cast<EditorChunkTerrain*>(
				ChunkTerrainCache::instance( **iter ).pTerrain());
			if( pEct )
			{
				pEct->onAlterDimensions();
				( (EditorChunkItem*) pEct )->toss( *iter );
			}
		}
		chunks_.clear();
	}
	virtual bool iseq( const UndoRedo::Operation & oth ) const
	{
		return false;
	}
	static void add( ChunkPtr chunk )
	{
		chunks_.push_back( chunk );
	}
private:
	static std::vector<ChunkPtr> chunks_;
};

std::vector<ChunkPtr> HeightBarrier::chunks_;

/**
 *	This template class undoes a change to some feature of a terrain block
 */
template <class HOLDER> class PoleUndo : public UndoRedo::Operation
{
public:
	typedef PoleUndo<HOLDER> This;

	PoleUndo( EditorTerrainBlockPtr pBlock, ChunkPtr pChunk ) :
		UndoRedo::Operation( int(typeid(This).name()) ),
		pChunk_( pChunk ),
		pBlock_( pBlock ),
		data_( pBlock )
	{
		addChunk( pChunk_ );
	}

	virtual void undo()
	{
		// first add the current state of this block to the undo/redo list
		UndoRedo::instance().add( new PoleUndo<HOLDER>( pBlock_, pChunk_ ) );

		// now apply our stored change
		data_.restore( pBlock_ );

		// and let everyone know it's changed
		BigBang::instance().changedTerrainBlock( pChunk_ );
		pBlock_->deleteManagedObjects();
		pBlock_->createManagedObjects();

		HeightBarrier::add( pChunk_ );
		// really need to save the undo/redo barrier number when
		// each block was first changed and mark it as unchanged
		// if we undo over that number. that will be fun!
	}

	virtual bool iseq( const UndoRedo::Operation & oth ) const
	{
		const PoleUndo<HOLDER> & roth =
			static_cast<const PoleUndo<HOLDER>&>( oth );
		return pBlock_ == roth.pBlock_;
	}

private:
	EditorTerrainBlockPtr	pBlock_;
	ChunkPtr				pChunk_;
	HOLDER					data_;
};

//----------------------------------------------------
//	Section : HeightPoleAlphaFunctor
//----------------------------------------------------


/**
 *	This method initialises the PoleAlpha helper class,
 *  given a packed 32 bit alpha value.
 */
PoleAlpha::PoleAlpha( uint32 a )
{
	alpha_ = Vector4( (float)((a & 0xff000000)>>24) / 255.f,
					  ((float)((a & 0x00ff0000)>>16) - 128.f) / 127.f,
					  ((float)((a & 0x0000ff00)>>8) - 128.f) / 127.f,
					  ((float)(a & 0x000000ff) - 128.f) / 127.f );
}


/**
 *	This method alters one of the alpha levels in a PoleAlpha.
 *
 *	@param amount	The amount to alter the alpha level by.
 *	@param level	The alpha level to adjust.
 */
void PoleAlpha::addToAlphaLevel( float amount, int level )
{
	alpha_[3-level] += (amount/255.f);
}


/**
 *	This method returns the re-normalized packed 32 bit alpha.
 *
 *	@return	The normalized alpha, usable by Moo::TerrainBlock
 */
uint32 PoleAlpha::alpha()
{
	float total = alpha_[0] + alpha_[1] + alpha_[2] + alpha_[3];
	alpha_ = alpha_ / total;
	
	int a = int(alpha_[0] * 255.f);
	int r = int(alpha_[1] * 127.f + 128.f);
	int g = int(alpha_[2] * 127.f + 128.f);
	int b = int(alpha_[3] * 127.f + 128.f);

	return ( (a<<24) | (r<<16) | (g<<8) | b );
}


PY_TYPEOBJECT( HeightPoleAlphaFunctor )

PY_BEGIN_METHODS( HeightPoleAlphaFunctor )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( HeightPoleAlphaFunctor )
	PY_ATTRIBUTE( texture )
	PY_ATTRIBUTE( channelTexture0 )
	PY_ATTRIBUTE( channelTexture1 )
	PY_ATTRIBUTE( channelTexture2 )
	PY_ATTRIBUTE( channelTexture3 )
	PY_ATTRIBUTE( channel )
PY_END_ATTRIBUTES()

PY_FACTORY_NAMED( HeightPoleAlphaFunctor, "HeightPoleAlphaFunctor", Functor )

FUNCTOR_FACTORY( HeightPoleAlphaFunctor )


/**
 *	Constructor.
 */
HeightPoleAlphaFunctor::HeightPoleAlphaFunctor( PyTypePlus * pType ):
	HeightPoleFunctor( pType ),
	blendLevel_( 0 ),
	applying_( false )
{
	textureName_ = s_notFoundTexture;

	this->blendLevel( 0 );
}


/**
 *	This method handles key events for the HeightPoleAlphaFunctor.
 *	Currently, it changes the selected alpha blend level.
 *
 *When pressing 1..4,
 * a) If the currently selected texture does not exist for the channel then
 * the correct texture for the channel is sampled from the mouse cursor position.
 * This is the same effect as pressing the middle mouse button.
 *
 * b) If a new channel is selected, then a) occurs, because you are trying to 
 * move the current channel's texture over to a new channel.  To change a texture's
 * channels, you must use the gui, or edit options.xml.
 * 
 * c) If the same channel is selected, and the current texture exists for that
 * channel, then the next available texture for that channel is selected.
 *
 *	@param event	the key event to process.
 *	@param tool		the tool to use.
 *
 *	@return true	the key event was handled.
 */
bool HeightPoleAlphaFunctor::handleKeyEvent( const KeyEvent & event, Tool& tool )
{
	return HeightPoleFunctor::handleKeyEvent(event,tool);
}

/**
 *	This method returns the texture under the mouse cursor, at the tool's locator.
 *
 *	@param	The tool from which to retrieve the locator.
 *
 *	@return	The texture name.
 */
static std::string s_mapName("");
const std::string& HeightPoleAlphaFunctor::sampleTextureName( Tool& tool, int level )
{
	if ( s_mapName.empty() )
		s_mapName = s_notFoundTexture;

	Chunk* pChunk = tool.currentChunk();
	if ( pChunk )
	{
		EditorChunkTerrain* pEct = static_cast<EditorChunkTerrain*>(
				ChunkTerrainCache::instance( *pChunk ).pTerrain());

		if ( pEct )
		{
			s_mapName = BWResource::removeExtension(
				pEct->block().textureName( level ) );
			if( BWResource::fileExists( s_mapName + ".bmp" ) )
				s_mapName += ".bmp";
			else if( BWResource::fileExists( s_mapName + ".tga" ) )
				s_mapName += ".tga";
			else if( BWResource::fileExists( s_mapName + ".jpg" ) )
				s_mapName += ".jpg";
			else if( BWResource::fileExists( s_mapName + ".dds" ) )
				s_mapName += ".dds";
			return s_mapName;
		}
	}

	return s_mapName;
}


/**
 *	This method chooses a new blend level.
 *
 *	@param newBlendLevel	the new alpha blend level to draw with.
 */
void HeightPoleAlphaFunctor::blendLevel( int newBlendLevel )
{
	if ( blendLevel_ != newBlendLevel )
	{
		char buf[64];
		blendLevel_ = newBlendLevel;
		sprintf( buf, "texture channel %d\0", blendLevel_+1 );
		BigBang::instance().addCommentaryMsg( buf, 0 );
	}
}


/**
 *	This method is called once per frame, and if the left mouse button
 *	is down, applies the alpha blending function.
 *
 *	@param	dTime	the change in time since the last frame.
 *	@param	tool	the tool we are using.
 */
void HeightPoleAlphaFunctor::update( float dTime, Tool& tool )
{
	if ( textureName_ == s_notFoundTexture.value() )
	{
		this->textureName( sampleTextureName( tool, blendLevel() ) );
	}

	channelTexture0_ = sampleTextureName( tool, 0 );
	channelTexture1_ = sampleTextureName( tool, 1 );
	channelTexture2_ = sampleTextureName( tool, 2 );
	channelTexture3_ = sampleTextureName( tool, 3 );

	if ( dragHandler.isDragging(MouseDragHandler::KEY_LEFTMOUSE) )
	{
		applying_ = true;

		//per update calculations
		falloff_ = ( 2.f / tool.size() );
		strength_ = tool.strength() * 10 * dTime;

		HeightPoleFunctor::apply( tool );
	}
	else
	{
		if (applying_)
		{
			applying_ = false;

			UndoRedo::instance().barrier( "Terrain Alpha Change", true );
			Flora::floraReset();
		}
	}
}


/**
 *	This little struct holds the bit we need of a terrain change for an undo
 */
struct PoleAlphaHolder
{
	PoleAlphaHolder( EditorTerrainBlockPtr pBlock ) :
		textures_( pBlock->textures() ),
		blendValues_( pBlock->blendValues() ) { }
	void restore( EditorTerrainBlockPtr pBlock )
		{ pBlock->blendValues() = blendValues_;
		  pBlock->textures() = textures_; }
	std::vector<Moo::BaseTexturePtr>	textures_;
	std::vector<uint32>					blendValues_;
};


/**
 * This method validates the texture in the chunk.
 * It is called once per chunk that is under the brush.
 *
 * @param ect	The editorChunkTerrain that is being valid.
 */
void HeightPoleAlphaFunctor::perChunkFunction( EditorChunkTerrain& ect )
{
	UndoRedo::instance().add( new PoleUndo<PoleAlphaHolder>( &ect.block(), ect.chunk() ) );

	const std::string& blockTexture = ect.block().textureName( blendLevel() );

	if ( blockTexture != textureName_ )
	{
		ect.block().textureName( blendLevel(), textureName_ );
	}
}


/**
 *	This method paints alpha into a height pole.
 *	It is called once per height pole per chunk.
 *
 *	@param	pole	The height pole that is being considered.
 *	@param	relPos	The relative position of the brush to this pole.
 *	@param	xIdx	The x index into the terrain block data array.
 *	@param	zIdx	The z index into the terrain block data array.
 */
void HeightPoleAlphaFunctor::perHeightPoleFunction(
		Moo::TerrainBlock::Pole& pole, const Vector3& relPos,
		int xIdx, int zIdx )
{
	uint32& alpha = pole.blend();

	float adjustedStrength = (1.f - (relPos.length() * falloff_)) * strength_;
	if (adjustedStrength>0.f)
	{
		PoleAlpha pa( alpha );
		pa.addToAlphaLevel( adjustedStrength, blendLevel_ );
		alpha = pa.alpha();
	}
}


/**
 *	Get an attribute for python
 */
PyObject * HeightPoleAlphaFunctor::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();

	return HeightPoleFunctor::pyGetAttribute( attr );
}


/**
 *	Set an attribute for python
 */
int HeightPoleAlphaFunctor::pySetAttribute( const char * attr, PyObject * value )
{
	PY_SETATTR_STD();

	return HeightPoleFunctor::pySetAttribute( attr, value );
}


/**
 *	Static python factory method
 */
PyObject * HeightPoleAlphaFunctor::pyNew( PyObject * args )
{
	return new HeightPoleAlphaFunctor;
}


//----------------------------------------------------
//	Section : HeightPoleHeightFilterFunctor
//----------------------------------------------------

PY_TYPEOBJECT( HeightPoleHeightFilterFunctor )

PY_BEGIN_METHODS( HeightPoleHeightFilterFunctor )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( HeightPoleHeightFilterFunctor )
	PY_ATTRIBUTE( applyRate )
	PY_ATTRIBUTE( falloff )
	PY_ATTRIBUTE( strengthMod )
	PY_ATTRIBUTE( framerateMod )
	PY_ATTRIBUTE( index )
	PY_ATTRIBUTE( constant )
	PY_ATTRIBUTE( name )
PY_END_ATTRIBUTES()

PY_FACTORY( HeightPoleHeightFilterFunctor, Functor )

FUNCTOR_FACTORY( HeightPoleHeightFilterFunctor )

std::vector<HeightPoleHeightFilterFunctor::FilterDef> HeightPoleHeightFilterFunctor::s_filters_;

/**
 *	Constructor.
 */
HeightPoleHeightFilterFunctor::HeightPoleHeightFilterFunctor( PyTypePlus * pType ):
	HeightPoleFunctor( pType ),
	falloff_( 0.f ),
	strength_( 0.f ),
	applying_( false ),
	appliedFor_( 0.f ),
	applyRate_( 0.f ),
	useFalloff_( 0 ),
	useStrength_( false ),
	useDTime_( false ),
	filterIndex_( 0 ),
	pHeightPoleCache_( NULL )
{
	if( s_filters_.empty() )
	{
		DataResource filters;
		if( filters.load( "resources/data/filters.xml" ) != DataHandle::DHE_NoError )
		{
    		ERROR_MSG( "HeightPoleHeightFilterFunctor::HeightPoleHeightFilterFunctor()"
				" - Could not open resources/data/filters.xml\n" );
			return;
		}

		DataSectionPtr pSection = filters.getRootSection();
		if( !pSection )
		{
    		ERROR_MSG( "HeightPoleHeightFilterFunctor::HeightPoleHeightFilterFunctor()"
				" - Could not open resources/data/filters.xml\n" );
			return;
		}

		std::vector<DataSectionPtr>	pSections;
		pSection->openSections( "filter", pSections );

		std::vector<DataSectionPtr>::iterator it = pSections.begin();
		std::vector<DataSectionPtr>::iterator end = pSections.end();

		bool included = true;
		while( it != end )
		{
			DataSectionPtr pFilter = *it++;

			FilterDef filter;
			filter.constant_ = pFilter->readFloat( "constant", 0.f );

			std::vector<DataSectionPtr>	kernels;
			pFilter->openSections( "kernel", kernels );

			if( kernels.size() != 3 )
			{
				filter.kernel_[ 0 ] = 0.f;
				filter.kernel_[ 1 ] = 0.f;
				filter.kernel_[ 2 ] = 0.f;
				filter.kernel_[ 3 ] = 0.f;
				filter.kernel_[ 4 ] = 1.f;
				filter.kernel_[ 5 ] = 0.f;
				filter.kernel_[ 6 ] = 0.f;
				filter.kernel_[ 7 ] = 0.f;
				filter.kernel_[ 8 ] = 0.f;
				filter.kernel_[ 9 ] = 0.f;
			}
			else
			{
				for( int i = 0; i < 3; ++i )
				{
					Vector3 kernel = kernels[i]->asVector3();
					filter.kernel_[ i * 3 + 0 ] = kernel.x;
					filter.kernel_[ i * 3 + 1 ] = kernel.y;
					filter.kernel_[ i * 3 + 2 ] = kernel.z;
				}
				filter.kernel_[ 9 ] = pFilter->readFloat( "strengthRatio" );
			}

			filter.kernelSum_ = pFilter->readFloat( "kernelSum" );
			filter.included_ = pFilter->readBool( "included", true );
			filter.name_ = pFilter->readString( "name", "unknown filter" );

			s_filters_.push_back( filter );
		}
	}
	if( !s_filters_.empty() )
	{
		filter_ = s_filters_[ filterIndex_ ];
	}
	else
	{
		filter_.constant_ = 0;
		memset( filter_.kernel_, 0, sizeof(filter_.kernel_) );
		filter_.kernelSum_ = 0;
		filter_.included_ = false;
	}
}

/**
 *	Destructor.
 */
HeightPoleHeightFilterFunctor::~HeightPoleHeightFilterFunctor()
{
	if (pHeightPoleCache_ != NULL)
	{
		delete [] pHeightPoleCache_;
		pHeightPoleCache_ = NULL;
	}
}

/** 
 *	This method updates the height pole height functor.
 *	if the left mouse button is down, the filter will be applied
 *
 *	@param dTime	The change in time since the last frame.
 *	@param tool		The tool we are using.
 */
void HeightPoleHeightFilterFunctor::update( float dTime, Tool& tool )
{
	bool lBtnDown = InputDevices::isKeyDown( KeyEvent::KEY_LEFTMOUSE );
	bool mBtnDown = InputDevices::isKeyDown( KeyEvent::KEY_MIDDLEMOUSE );

	if ( dragHandler.isDragging(MouseDragHandler::KEY_LEFTMOUSE) || dragHandler.isDragging(MouseDragHandler::KEY_MIDDLEMOUSE) )
	{
		if( !applying_ )
			UndoRedo::instance().add( new HeightBarrier() );
		applying_ = true;

		// limit the applications to a fixed rate if desired
		if (applyRate_ > 0.f)
		{
			appliedFor_ += dTime;

			if (appliedFor_ < 1.f / applyRate_) return;
			
			// but don't apply more than once per frame...
			appliedFor_ = fmodf( appliedFor_, 1.f / applyRate_ );
		}

		//per update calculations
		falloff_ = ( 2.f / tool.size() );
		strength_ = lBtnDown ? filter_.constant_ : -filter_.constant_;
		if (useStrength_) strength_ *= tool.strength() * 0.001f;
		if (useDTime_) strength_ *= dTime * 30.f;

		HeightPoleFunctor::apply( tool );
	}
	else
	{
		if (applying_)
		{
			// turn off functor
			appliedFor_ = applyRate_ > 0 ? 1.f / applyRate_ : 0.f;
			applying_ = false;

			// regenerate collision scene for affected chunks
			for (ChunkItemSet::iterator it = affectedItems_.begin();
				it != affectedItems_.end();
				it++)
			{
				(*it)->toss( (*it)->chunk() );
			}
			affectedItems_.clear();

			// and put in an undo barrier
			UndoRedo::instance().add( new HeightBarrier() );
			UndoRedo::instance().barrier( "Terrain Height Change", true );

			Flora::floraReset();
		}
	}
}


/**
 *	This method responds to the comma and period keys,
 *	which changes the filter that is to be applied.
 *
 *	@param	event	The key event to process.
 *	@param	t		The tool we are using.
 */
bool HeightPoleHeightFilterFunctor::handleKeyEvent( const KeyEvent & event, Tool& t )
{
	HeightPoleFunctor::handleKeyEvent(event,t);

	if( !filter_.included_ )
		return false;
	if ( event.isKeyDown() )
	{
		int dir = 1;
		switch ( event.key() )
		{
		case KeyEvent::KEY_COMMA:
			dir = -1;
			// fallthru intentional
		case KeyEvent::KEY_PERIOD:
			this->filterIndex( filterIndex_ + dir );

			while( !filter_.included_ )
			{
				this->filterIndex( filterIndex_ + dir );
			}

			Options::setOptionInt( "terrain/filter/index", filterIndex_ );

			char sbuf[256];
			sprintf( sbuf, "Selected filter %d: %s",
				filterIndex_, filter_.name_.c_str() );

			BigBang::instance().addCommentaryMsg( sbuf, 0 );

			return true;
			break;
		}
	}

	return false;
}


/**
 *	This little struct holds the bit we need of a terrain change for an undo
 */
struct PoleHeightHolder
{
	PoleHeightHolder( EditorTerrainBlockPtr pBlock ) :
		heightMap_( pBlock->heightMap() )
	{}
	void restore( EditorTerrainBlockPtr pBlock )
	{
		const_cast<std::vector<float>&>(pBlock->heightMap()) = heightMap_;
	}
	std::vector<float>		heightMap_;
};


/**
 *	This method is called before any of perheightpole class are made.
 *	It saves the current map of hiehgt poles so that the filter
 *	application doesn't pollute itself.
 */
void HeightPoleHeightFilterFunctor::perChunkFunction(
	class EditorChunkTerrain & rEct )
{
	if (pHeightPoleCache_ != NULL)
	{
		delete [] pHeightPoleCache_;
		pHeightPoleCache_ = NULL;
	}

	int polesWidth = rEct.block().polesWidth();
	int poleCount = polesWidth * polesWidth;
	pHeightPoleCache_ = new float[ poleCount ];
	memcpy( pHeightPoleCache_, &rEct.block().heightMap().front(),
		poleCount * sizeof(float) );

	UndoRedo::instance().add(
		new PoleUndo<PoleHeightHolder>( &rEct.block(), rEct.chunk() ) );

	BigBang::instance().markTerrainShadowsDirty( rEct.chunk() );
}



/**
 *	This method makes a height pole get its height filtered
 *
 *	@param	pole	The height pole that is being filtered
 *	@param	relPos	The relative position of the brush to this pole.
 *	@param	xIdx	The x index into the terrain block data array.
 */
void HeightPoleHeightFilterFunctor::perHeightPoleFunction(
		class Moo::TerrainBlock::Pole& pole,
		const Vector3& relPos,
		int xIdx, int zIdx )
{
	int w = pole.block().blocksWidth();
	if (xIdx < 0 || xIdx >= w || zIdx < 0 || zIdx >= w) return;

	int polesWidth = pole.block().polesWidth();

	float falloffFactor = (1.f - (relPos.length() * falloff_));
	if (falloffFactor > 0.f)
	{
		// fade out the filter effect as desired
		falloffFactor = useFalloff_ ?
			1.f - powf( 1.f-falloffFactor, (float) useFalloff_ ) : 1.f;

		float acc = strength_ * filter_.kernel_[ 9 ];
		// strength_ is derived from filter_.constant_

		int i = 0;
		for (int z = -1; z <= 1; z++) for (int x = -1; x <= 1; x++)
		{
			float * polePeek = pHeightPoleCache_ +
				(xIdx+x+1) + (zIdx+z+1)*polesWidth;
			acc += (*polePeek) * filter_.kernel_[ i++ ];

			//acc += pole.height(x+z*polesWidth) * filter_.kernel_[ i++ ];
		}

		acc /= filter_.kernelSum_;

		float& height = pole.height();
		height = height * (1.f - falloffFactor) + acc * falloffFactor;
	}
}


/**
 *	This method is called when all height poles have been adjusted.
 *	we recalculate the bounding box of the terrain chunks that have
 *	been changed.
 *
 *	@param	t	The tool we are using.
 */
void HeightPoleHeightFilterFunctor::onFinish( Tool& t )
{
	if (pHeightPoleCache_ != NULL)
	{
		delete [] pHeightPoleCache_;
		pHeightPoleCache_ = NULL;
	}

	//Temporarily increase the tools area of influence so that
	//height poles at the edges of chunks are updated correctly.
	t.findRelevantChunks( 4.f );

	ChunkPtrVector::iterator it = t.relevantChunks().begin();
	ChunkPtrVector::iterator end = t.relevantChunks().end();

	while ( it != end )
	{
		Chunk* pChunk = *it++;

		EditorChunkTerrain* pEct = static_cast<EditorChunkTerrain*>(
			ChunkTerrainCache::instance( *pChunk ).pTerrain());

		if (pEct)
		{
			pEct->onAlterDimensions();

			affectedItems_.insert( pEct );
		}
	}

	t.findRelevantChunks();
}

/**
 *	Get an attribute for python
 */
PyObject * HeightPoleHeightFilterFunctor::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();

	return HeightPoleFunctor::pyGetAttribute( attr );
}


/**
 *	Set an attribute for python
 */
int HeightPoleHeightFilterFunctor::pySetAttribute( const char * attr, PyObject * value )
{
	PY_SETATTR_STD();

	return HeightPoleFunctor::pySetAttribute( attr, value );
}

/**
 *	Static python factory method
 */
PyObject * HeightPoleHeightFilterFunctor::pyNew( PyObject * args )
{
	return new HeightPoleHeightFilterFunctor;
}


/**
 *	Set the filter to the given preset index
 */
void HeightPoleHeightFilterFunctor::filterIndex( int i )
{
	int numFils = s_filters_.size();
	if ( !numFils )
		return;
	filterIndex_ = ( i + numFils ) % numFils;
	filter_ = s_filters_[ filterIndex_ ];
}

//----------------------------------------------------
//	Section : HeightPoleSetHeightFunctor
//----------------------------------------------------

PY_TYPEOBJECT( HeightPoleSetHeightFunctor )

PY_BEGIN_METHODS( HeightPoleSetHeightFunctor )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( HeightPoleSetHeightFunctor )
	PY_ATTRIBUTE( height )
	PY_ATTRIBUTE( relative )
PY_END_ATTRIBUTES()

PY_FACTORY_NAMED( HeightPoleSetHeightFunctor, "HeightPoleSetHeightFunctor", Functor )

FUNCTOR_FACTORY( HeightPoleSetHeightFunctor )


HeightPoleSetHeightFunctor::HeightPoleSetHeightFunctor( PyTypePlus * pType ):
	HeightPoleFunctor( pType ),
	height_( 0.f ),
	applying_( false )
{
}

HeightPoleSetHeightFunctor::~HeightPoleSetHeightFunctor()
{
}

/** 
 *	This method updates the height pole height functor.
 *	if the left mouse button is down, the filter will be applied
 *
 *	@param dTime	The change in time since the last frame.
 *	@param tool		The tool we are using.
 */
void HeightPoleSetHeightFunctor::update( float dTime, Tool& tool )
{
	if ( dragHandler.isDragging(MouseDragHandler::KEY_LEFTMOUSE) )
	{
		if( !applying_ )
			UndoRedo::instance().add( new HeightBarrier() );
		applying_ = true;

		HeightPoleFunctor::apply( tool );
	}
	else
	{
		if (applying_)
		{
			// turn off functor
			applying_ = false;

			// regenerate collision scene for affected chunks
			for (ChunkItemSet::iterator it = affectedItems_.begin();
				it != affectedItems_.end();
				it++)
			{
				(*it)->toss( (*it)->chunk() );
			}
			affectedItems_.clear();

			// and put in an undo barrier
			UndoRedo::instance().add( new HeightBarrier() );
			UndoRedo::instance().barrier( "Terrain Height Change", true );

			poles_.clear();

			Flora::floraReset();
		}
	}
}


/**
 *	This method is called before any of perheightpole class are made.
 *	It saves the current map of hiehgt poles so that the filter
 *	application doesn't pollute itself.
 */
void HeightPoleSetHeightFunctor::perChunkFunction(
	class EditorChunkTerrain & rEct )
{
	UndoRedo::instance().add( new PoleUndo<PoleHeightHolder>( &rEct.block(), rEct.chunk() ) );

	BigBang::instance().markTerrainShadowsDirty( rEct.chunk() );
}



/**
 *	This method makes a height pole get its height filtered
 *
 *	@param	pole	The height pole that is being filtered
 *	@param	relPos	The relative position of the brush to this pole.
 *	@param	xIdx	The x index into the terrain block data array.
 */
void HeightPoleSetHeightFunctor::perHeightPoleFunction(
		class Moo::TerrainBlock::Pole& pole,
		const Vector3& relPos,
		int xIdx, int zIdx )
{
	int w = pole.block().blocksWidth();
	if (xIdx < 0 || xIdx >= w || zIdx < 0 || zIdx >= w) return;

	if( relative_ )
	{
		std::pair< void*, std::pair<int, int> > pp =
			std::make_pair( &pole.block(), std::make_pair( xIdx, zIdx ) );
		if( poles_.find( pp ) == poles_.end() )
		{
			pole.height() += height_;
			poles_.insert( pp );
		}
	}
	else
		pole.height() = height_;
}


/**
 *	This method is called when all height poles have been adjusted.
 *	we recalculate the bounding box of the terrain chunks that have
 *	been changed.
 *
 *	@param	t	The tool we are using.
 */
void HeightPoleSetHeightFunctor::onFinish( Tool& t )
{
	//Temporarily increase the tools area of influence so that
	//height poles at the edges of chunks are updated correctly.
	t.findRelevantChunks( 4.f );
	
	ChunkPtrVector::iterator it = t.relevantChunks().begin();
	ChunkPtrVector::iterator end = t.relevantChunks().end();

	while ( it != end )
	{
		Chunk* pChunk = *it++;

		EditorChunkTerrain* pEct = static_cast<EditorChunkTerrain*>(
			ChunkTerrainCache::instance( *pChunk ).pTerrain());

		if (pEct)
		{
			pEct->onAlterDimensions();

			affectedItems_.insert( pEct );
		}
	}
	
	t.findRelevantChunks();
}

/**
 *	Get an attribute for python
 */
PyObject * HeightPoleSetHeightFunctor::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();

	return HeightPoleFunctor::pyGetAttribute( attr );
}


/**
 *	Set an attribute for python
 */
int HeightPoleSetHeightFunctor::pySetAttribute( const char * attr, PyObject * value )
{
	PY_SETATTR_STD();

	return HeightPoleFunctor::pySetAttribute( attr, value );
}

/**
 *	Static python factory method
 */
PyObject * HeightPoleSetHeightFunctor::pyNew( PyObject * args )
{
	return new HeightPoleSetHeightFunctor;
}

//----------------------------------------------------
//	Section : HeightPoleHoleFunctor
//----------------------------------------------------

PY_TYPEOBJECT( HeightPoleHoleFunctor )

PY_BEGIN_METHODS( HeightPoleHoleFunctor )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( HeightPoleHoleFunctor )
	PY_ATTRIBUTE( fillNotCut )
PY_END_ATTRIBUTES()

PY_FACTORY_NAMED( HeightPoleHoleFunctor, "HeightPoleHoleFunctor", Functor )

FUNCTOR_FACTORY( HeightPoleHoleFunctor )

/**
 *	Constructor.
 */
HeightPoleHoleFunctor::HeightPoleHoleFunctor( PyTypePlus * pType ):
	HeightPoleFunctor( pType ),
	falloff_( 0.f ),
	fillNotCut_( false ),
	applying_( false )
{
}


/** 
 *	This method updates the height pole hole functor.
 *	if the left mouse button is down, the functor is applied
 *
 *	@param dTime	The change in time since the last frame.
 *	@param tool		The tool we are using.
 */
void HeightPoleHoleFunctor::update( float dTime, Tool& tool )
{
	if ( dragHandler.isDragging(MouseDragHandler::KEY_LEFTMOUSE) )
	{
		applying_ = true;

		//per update calculations
		falloff_ = ( 2.f / tool.size() );

		//Doing all this offset shennanigans is necessary
		//because although the user "points" at quads, the
		//underlying mesh is still vertex based : the quad
		//actually touched hangs off the corner of the vertex.		
		Matrix savedTransform = tool.locator()->transform();
		Matrix newTransform( savedTransform );		
		newTransform._41 -= 2.f;
		newTransform._43 -= 2.f;

		tool.locator()->transform(newTransform);
		HeightPoleFunctor::apply( tool );
		tool.locator()->transform(savedTransform);
	}
	else
	{
		if (applying_)
		{
			applying_ = false;

			UndoRedo::instance().barrier( "Terrain Hole Change", true );
		}
	}
}


/**
 *	This little struct holds the bit we need of a terrain change for an undo
 */
struct PoleHoleHolder
{
	PoleHoleHolder( EditorTerrainBlockPtr pBlock ) :
		holes_( pBlock->holes() ) { }
	void restore( EditorTerrainBlockPtr pBlock )
		{ pBlock->holes() = holes_; }
	std::vector<bool>		holes_;
};

/**
 *	This method is called before any of perheightpole class are made.
 *	It saves the current hole map in the undo/redo list
 */
void HeightPoleHoleFunctor::perChunkFunction(
	class EditorChunkTerrain & rEct )
{
	UndoRedo::instance().add( new PoleUndo<PoleHoleHolder>( &rEct.block(), rEct.chunk() ) );
}


/**
 *	This method cuts out or fills in a hole
 *
 *	@param	pole	The height pole that is going up or down.
 *	@param	relPos	The relative position of the brush to this pole.
 *	@param	xIdx	The x index into the terrain block data array.
 */
void HeightPoleHoleFunctor::perHeightPoleFunction(
		class Moo::TerrainBlock::Pole& pole,
		const Vector3& relPos,
		int xIdx, int zIdx )
{
	int w = pole.block().blocksWidth();
	if (xIdx < 0 || xIdx >= w || zIdx < 0 || zIdx >= w) return;
	pole.hole( !fillNotCut_ );	
}


/**
 *	Get an attribute for python
 */
PyObject * HeightPoleHoleFunctor::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();

	return HeightPoleFunctor::pyGetAttribute( attr );
}


/**
 *	Set an attribute for python
 */
int HeightPoleHoleFunctor::pySetAttribute( const char * attr, PyObject * value )
{
	PY_SETATTR_STD();

	return HeightPoleFunctor::pySetAttribute( attr, value );
}


/**
 *	Static python factory method
 */
PyObject * HeightPoleHoleFunctor::pyNew( PyObject * args )
{
	return new HeightPoleHoleFunctor;
}


//----------------------------------------------------
//	Section : HeightPoleEcotypeFunctor
//----------------------------------------------------

PY_TYPEOBJECT( HeightPoleEcotypeFunctor )

PY_BEGIN_METHODS( HeightPoleEcotypeFunctor )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( HeightPoleEcotypeFunctor )
PY_END_ATTRIBUTES()

PY_FACTORY_NAMED( HeightPoleEcotypeFunctor, "HeightPoleEcotypeFunctor", Functor )

FUNCTOR_FACTORY( HeightPoleEcotypeFunctor )

/**
 *	constructor.
 */
HeightPoleEcotypeFunctor::HeightPoleEcotypeFunctor( PyTypePlus * pType ):
	HeightPoleFunctor( pType ),
	pScriptObject_( NULL ),
	pProgress_( NULL ),
	pChunkProgress_( NULL )
{
	PyObject* pModule = PyImport_ImportModule( "Ecotypes" );
	if (pModule == NULL)
	{
		WARNING_MSG( "Could not get module %s\n", "Ecotypes" );
		PyErr_Print();
	}
	else
	{
		this->retrieveScriptObject( pModule );
		Py_DECREF( pModule );
	}
}


/**
 *	destructor.  releases the script object.
 */
HeightPoleEcotypeFunctor::~HeightPoleEcotypeFunctor()
{
	Py_XDECREF( pScriptObject_ );
	pScriptObject_ = NULL;
}


/**
 *	This method sets the private member pScriptObject_ from the given module,
 *	by finding the Ecotype class.
 *
 *	@param pModule	The python module containing the Ecotype class.
 */
void HeightPoleEcotypeFunctor::retrieveScriptObject( PyObject * pModule )
{
	Py_XDECREF( pScriptObject_ );
	pScriptObject_ = NULL;

	MF_ASSERT( pModule );

	char * className =
		const_cast<char *>( "Ecotypes" );

	pScriptObject_ = PyObject_CallMethod( pModule, className, "" );

	if (!pScriptObject_)
	{
		ERROR_MSG( "HeightPoleEcotypeFunctor::HeightPoleEcotypeFunctor : No script object %s\n", className );
		PyErr_Print();
	}
}


/**
 *	This method reloads the script object for the ecotype functor.
 *	All errors are handled internally.
 */
void HeightPoleEcotypeFunctor::reloadScript()
{
	// get rid of script object
	Py_XDECREF( pScriptObject_ );
	pScriptObject_ = NULL;

	//Check if python has ever loaded the module
	PyObject *modules = PyImport_GetModuleDict();
	PyObject *m;

	// if so, reload it
	if ((m = PyDict_GetItemString(modules, "Ecotypes")) != NULL &&
	    PyModule_Check(m))
	{
		m = PyImport_ReloadModule( m );
	}
	else
	{
		m = PyImport_ImportModule( "Ecotypes" );

	}

	// if module was loaded, get the script object
	if (m)
	{
		this->retrieveScriptObject( m );
	}
	else
	{
		PyErr_Print();
		PyErr_Clear();
	}


	if ( pScriptObject_ )
	{
		BigBang::instance().addCommentaryMsg( "Ecotypes.py reloaded", 0 );
	}
	else
	{
		BigBang::instance().addCommentaryMsg( "Could not reload Ecotypes.py",
								Commentary::CRITICAL );
	}
}


/**
 *	This method responds to the F9 key, which signals the functor to 
 *	calculate ecotypes for all current chunks.
 *
 *	@param	event	The key event to process.
 *	@param	t		The tool we are using.
 */
bool HeightPoleEcotypeFunctor::handleKeyEvent( const KeyEvent & event, Tool& t )
{
	HeightPoleFunctor::handleKeyEvent(event,t);

	if ( event.isKeyDown() )
	{
		switch ( event.key() )
		{
		case KeyEvent::KEY_RETURN:
			if ( !InputDevices::isAltDown() )
			{
				this->calculateEcotypes( t );
				return true;
			}
			case KeyEvent::KEY_F11:
				reloadScript();
			return true;
		}
	}

	return false;
}


void HeightPoleEcotypeFunctor::calculateEcotypes( Tool& t )
{
	//initialise the script
	if ( !pScriptObject_ )
	{
		BigBang::instance().addCommentaryMsg(
				"Could not find ecotypes.py", Commentary::CRITICAL );
	}
	else
	{
		BigBangProgressBar progress;
		pProgress_ = &progress;

		ProgressTask task( &progress, "Calculating Ecotypes", (float) t.relevantChunks().size() );
		pChunkProgress_ = &task;
		
		int dummy = 0;
		Script::call(
			PyObject_GetAttrString( pScriptObject_, "begin" ),
			Py_BuildValue( "(i)", dummy ),
			"HeightPoleEcotypeFunctor::handleKeyEvent" );

		//begin iterating through the chunks
		HeightPoleFunctor::apply( t, true );

		pProgress_ = NULL;
		pChunkProgress_ = NULL;

		// and put the barrier in the undo list
		UndoRedo::instance().barrier( "Terrain Ecotype Change", true );
	}
}


/**
 *	This function implements a script function. It applies the
 *	ecotype functor.
 */
static PyObject * py_applyEcoytpeFunctor( PyObject * args )
{
	PyObject* pTool;

	if (!PyArg_ParseTuple( args, "O", &pTool ) ||
		!Tool::Check( pTool ))
	{
		PyErr_SetString( PyExc_TypeError,
			"py_applyEcoytpeFunctor: Expected a Tool." );
		return NULL;
	}

	Tool* tool = static_cast<Tool*>( pTool );
	//this is a bit dodge.
	HeightPoleEcotypeFunctor& pFunctor =
		static_cast<HeightPoleEcotypeFunctor&>( *tool->functor() );
	pFunctor.calculateEcotypes( *tool );

	Py_Return;
}
PY_MODULE_FUNCTION( applyEcoytpeFunctor, Functor )


/**
 *	This little struct holds the bit we need of a terrain change for an undo
 */
struct PoleEcotypeHolder
{
	PoleEcotypeHolder( EditorTerrainBlockPtr pBlock ) :
		detailIDs_( pBlock->detailIDs() ) { }
	void restore( EditorTerrainBlockPtr pBlock )
		{ pBlock->detailIDs() = detailIDs_;
		  Flora::floraReset(); } // not so good calling so many times...
	std::vector<uint8>		detailIDs_;
};

/**
 *	This method is called once per chunk.  It precalculates the all
 *	per-chunk information before the per-heightpole methods are called.
 *
 *	Relevant information includes:
 *		relative heights.
 *
 *	@param	ect		The editor terrain chunk being considered.
 */
void HeightPoleEcotypeFunctor::perChunkFunction( class EditorChunkTerrain& ect )
{
	pChunkProgress_->step();

	UndoRedo::instance().add( new PoleUndo<PoleEcotypeHolder>( &ect.block(), ect.chunk() ) );

	pEct_ = &ect;

	//calculate relative elevations for the whole chunk
	ect.relativeElevations( relativeElevations_, w_, h_ );

	//resize the detail object section of the terrain block
	ect.block().resizeDetailIDs( w_, h_ );

	//tell the script object which textures we are working with.
	for ( int i = 0; i < 4; i++ )
	{
		Script::call(
			PyObject_GetAttrString( pScriptObject_, "textureLayer" ),
			Py_BuildValue( "(is)", i, ect.block().textureName(i).c_str() ),
			"HeightPoleEcotypeFunctor::perChunkFunction" );
	}
}


/**
 *	This method is called per height pole.  It calls into our script
 *	and works out which ecotype should exist at the height pole.
 *
 *	@param	pole	The height pole that is being considered.
 *	@param	relPos	The relative position of the brush to this pole.
 *	@param	xIdx	The x index into the terrain block data array.
 *	@param	zIdx	The z index into the terrain block data array.
 */
void HeightPoleEcotypeFunctor::perHeightPoleFunction(
		class Moo::TerrainBlock::Pole& pole,
		const Vector3& relativePosition,
		int xIdx, int zIdx )
{
	//because the height pole indices go from -1
	int x = xIdx + 1;
	int z = zIdx + 1;

	//call our script object to get the name of the winning ecotype for this pole.
	float elevation = pole.height();
	float slope = pEct_->slope( xIdx, zIdx );
	float relativeElevation = relativeElevations_[z * w_ + x];
	PoleAlpha pa( pole.blend() );
	Vector4 a( pa.normalisedAlpha() );

	PyObject* pRet = Script::ask(
		PyObject_GetAttrString( pScriptObject_, "calculateEcosystem" ),
		Py_BuildValue( "(fffffff)", elevation, relativeElevation, slope, a[3], a[2], a[1], a[0] ),
		"HeightPoleEcotypeFunctor::perHeightPoleFunction" );
		
	if (pRet == NULL)
	{
		return;
	}

	//store the ecotype
	int ecotypeID;
	if( !pRet || Script::setData( pRet, ecotypeID ))
	{
		PyErr_Print();
		PyErr_Clear();
	}
	else
	{
		MF_ASSERT( z >= 0 );
		MF_ASSERT( x >= 0 );
		MF_ASSERT( z < h_ );
		MF_ASSERT( x < w_ );
		
		pEct_->block().detailIDs()[z * w_ + x] = (uint8)ecotypeID;
	}
	Py_XDECREF( pRet );
}


/**
 *	This method is called when an apply() has finished.
 *  We repopulate the flora so the user can immediately see the results.
 *
 *	@param	t		unused.
 */
void HeightPoleEcotypeFunctor::onFinish( Tool& t )
{
	Flora::floraReset();
}


/**
 *	Static python factory method
 */
PyObject * HeightPoleEcotypeFunctor::pyNew( PyObject * args )
{
	return new HeightPoleEcotypeFunctor;
}