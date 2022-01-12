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
#include "terrain_texturing_functors.hpp"
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
//	Section : HeightPoleAlphaFunctor
//----------------------------------------------------
PY_TYPEOBJECT( HeightPoleAlphaFunctor )

PY_BEGIN_METHODS( HeightPoleAlphaFunctor )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( HeightPoleAlphaFunctor )
	PY_ATTRIBUTE( texture )
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
	HeightPoleFunctor::handleKeyEvent(event,tool);

	if ( event.isKeyDown() )
	{
		switch ( event.key() )
		{
		case KeyEvent::KEY_1:
		case KeyEvent::KEY_2:
		case KeyEvent::KEY_3:
		case KeyEvent::KEY_4:
			if( ( InputDevices::modifiers() & ( MODIFIER_SHIFT | MODIFIER_CTRL | MODIFIER_ALT ) ) == 0 )
			{
				int originalBlendLevel = this->blendLevel();
				this->blendLevel( event.key() - KeyEvent::KEY_1 );
				std::string tName = sampleTextureName( tool ); 

				DataSectionPtr pSect = Options::pRoot()->openSection( "terrain" );
				if ( originalBlendLevel == this->blendLevel() && pSect )
				{
					int existingCh = pSect->readInt( this->textureName(), -1 );
					DEBUG_MSG( "%s, curr channel %d, curr texture's channel is %d\n", this->textureName().c_str(), existingCh, this->blendLevel() );
					if ( existingCh == -1 || existingCh == this->blendLevel() )
					{
						this->nextBlendLevelTexture( tool );
					}
					else
					{
						this->textureName( tName );
					}
				}
				else
				{
					this->textureName( tName );
				}
			}
			return true;
		case KeyEvent::KEY_MIDDLEMOUSE:
			this->textureName( sampleTextureName( tool ) );
		}
	}

	return false;
}


/**
 * This method chooses the next texture for the current texture layer.
 */
void HeightPoleAlphaFunctor::nextBlendLevelTexture( Tool& tool )
{
	//find the current texture name in all texture names
	//that use this blend level
	char chName[256];
	sprintf( chName, "terrain/channel%d\0", this->blendLevel() );
	std::vector<std::string> textureNames;
	DataSectionPtr pSect = Options::pRoot()->openSection( chName );
	if ( pSect )
	{
		pSect->readStrings( "texture", textureNames );

		std::string stripped = BWResource::removeExtension( this->textureName() );
		std::string strippedCurrent = BWResource::removeExtension( sampleTextureName( tool ) );

		bool foundDefault = stricmp( stripped.c_str(), strippedCurrent.c_str() ) == 0;
		//choose the next one
		uint i = 0;
		while ( i < textureNames.size() )
		{
			std::string strippedCmp = BWResource::removeExtension( textureNames[i].c_str() );
			if( stricmp( strippedCmp.c_str(), strippedCurrent.c_str() ) == 0 )
				foundDefault = true;
			if ( !stricmp( strippedCmp.c_str(), stripped.c_str() ) )
			{
				i = ( i + 1 ) % textureNames.size();
				if( i == 0 && !foundDefault )
					break;
				this->textureName( textureNames[i] );
				return;
			}
			i++;
		}

		if( !foundDefault )
			this->textureName( sampleTextureName( tool ) );

		//none found. choose the first one.
		else if ( textureNames.size() > 0 )
			this->textureName( textureNames[0] );
	}
}


/**
 *	This method returns the texture under the mouse cursor, at the tool's locator.
 *
 *	@param	The tool from which to retrieve the locator.
 *
 *	@return	The texture name.
 */
static std::string s_mapName("");
const std::string& HeightPoleAlphaFunctor::sampleTextureName( Tool& tool )
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
				pEct->block().textureName( this->blendLevel() ) );
			s_mapName += ".bmp";
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
		this->textureName( sampleTextureName( tool ) );
	}

	if ( dragHandler.isDragging(MouseDragHandler::KEY_LEFTMOUSE) )
	{
		applying_ = true;

		//per update calculations
		falloff_ = ( 2.f / tool.size() );
		strength_ = tool.strength() * dTime;

		HeightPoleFunctor::apply( tool );
	}
	else
	{
		if (applying_)
		{
			applying_ = false;

			UndoRedo::instance().barrier( "Terrain Alpha Change", true );
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
//	Section : ProceduralTexturingFunctor
//----------------------------------------------------
PY_TYPEOBJECT( ProceduralTexturingFunctor )

PY_BEGIN_METHODS( ProceduralTexturingFunctor )
	PY_METHOD( load )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( ProceduralTexturingFunctor )
	PY_ATTRIBUTE( noFalloff )
	PY_ATTRIBUTE( enabled )
PY_END_ATTRIBUTES()

PY_FACTORY_NAMED( ProceduralTexturingFunctor, "ProceduralTexturingFunctor", Functor )

FUNCTOR_FACTORY( ProceduralTexturingFunctor )


/**
 *	Constructor.
 */
ProceduralTexturingFunctor::ProceduralTexturingFunctor( PyTypePlus * pType ):
	HeightPoleFunctor( pType ),
	applying_( false ),
	noFalloff_( false ),
	enabled_( true )
{
	//Define rule types here
	ruleTypes_.push_back( "Slope" );
	ruleTypes_.push_back( "Height" );
}


bool ProceduralTexturingFunctor::load( PyDataSectionPtr pSect )
{
	DataSectionPtr pSection = pSect->pSection();
	std::vector<DataSectionPtr> rules;
	pSection->openSections( "rule", rules );
	for (uint32 i=0; i<rules.size(); i++)
	{
		for (uint32 j=0; j<ruleTypes_.size(); j++)
		{
			if (rules[i]->openSection( "min" + ruleTypes_[j] ))
			{
				Rule r(i);
				r.load( rules[i], ruleTypes_[j] );
				rules_.push_back( r );
			}
		}
	}
	return false;
}


/**
 *	This method handles key events for the ProceduralTexturingFunctor.
 *	Currently, it does nothing.
 *
 *	@param event	the key event to process.
 *	@param tool		the tool to use.
 *
 *	@return true	the key event was handled.
 */
bool ProceduralTexturingFunctor::handleKeyEvent( const KeyEvent & event, Tool& tool )
{
	if (!enabled_)
		return false;

	HeightPoleFunctor::handleKeyEvent(event,tool);

	if ( event.isKeyDown() )
	{
		switch ( event.key() )
		{
		default:
			break;
		}
	}

	return false;
}


/**
 *	This method is called once per frame, and if the left mouse button
 *	is down, applies the procedural texturing function.
 *
 *	@param	dTime	the change in time since the last frame.
 *	@param	tool	the tool we are using.
 */
void ProceduralTexturingFunctor::update( float dTime, Tool& tool )
{
	if ( !enabled_ && !applying_ )
		return;

	if ( dragHandler.isDragging(MouseDragHandler::KEY_LEFTMOUSE) || dragHandler.isDragging(MouseDragHandler::KEY_MIDDLEMOUSE) )
	{
		applying_ = true;

		//adjusting the size of the tool is a hack, that is used
		//at the moment to solve the problem of "slope"
		//i.e. what is "slope" at a point, the normal?  no because
		//for a single point that on each side falls off dramatically,
		//the normal points straight up indicating "flat"
		//result : fix the "slope" calculation and you can remove the size hack
		float size = tool.size();
		tool.size( size + 8.f );

		//per update calculations
		falloff_ = ( 2.f / tool.size() );
		strength_ = tool.strength() * dTime;

		HeightPoleFunctor::apply( tool );
		tool.size( size );
	}
	else
	{
		if (applying_)
		{
			applying_ = false;
			UndoRedo::instance().barrier( "Terrain Alpha Change", true );
		}
	}
}


/**
 * This method validates the texture in the chunk.
 * It is called once per chunk that is under the brush.
 *
 * @param ect	The editorChunkTerrain that is being valid.
 */
void ProceduralTexturingFunctor::perChunkFunction( EditorChunkTerrain& ect )
{
	ect_ = &ect;

	UndoRedo::instance().add( new PoleUndo<PoleAlphaHolder>( &ect.block(), ect.chunk() ) );

	for (uint32 i=0; i<rules_.size(); i++)
	{
		Rule& r = rules_[i];
		const std::string& blockTexture = ect.block().textureName( r.blendLevel() );

		if ( blockTexture != r.textureName() )
		{
			ect.block().textureName( r.blendLevel(), r.textureName() );
		}
	}
}


/**
 *	This method procedurally paints alpha into a height pole.
 *	It is called once per height pole per chunk.
 *
 *	@param	pole	The height pole that is being considered.
 *	@param	relPos	The relative position of the brush to this pole.
 *	@param	xIdx	The x index into the terrain block data array.
 *	@param	zIdx	The z index into the terrain block data array.
 */
void ProceduralTexturingFunctor::perHeightPoleFunction(
		Moo::TerrainBlock::Pole& pole, const Vector3& relPos,
		int xIdx, int zIdx )
{
	uint32& alpha = pole.blend();
	float slope = ect_->slope( xIdx, zIdx );

	float adjustedStrength = (1.f - (relPos.length() * falloff_)) * strength_;
	if (noFalloff_ || adjustedStrength>0.f)
	{
		float amts[4] = {1.f,1.f,1.f,1.f};
		bool touched[4] = {false,false,false,false};

		for (uint32 i = 0; i < rules_.size(); i++)
		{
			MF_ASSERT( ect_ );

			Rule& r = rules_[i];
			uint32 bl = r.blendLevel();

			touched[bl] = true;

			float amt;
			if (r.suffix() == "Slope")
				amt = r.match( slope );
			else if (r.suffix() == "Height")
				amt = r.match( pole.height() );

			amts[bl] *= amt;
		}

		PoleAlpha pa( 0x00808080 );
		//PoleAlpha pa(alpha);
		for (uint32 i=0; i<4; i++)
		{
			if (touched[i])
				pa.addToAlphaLevel(amts[i] * 255.f,i);
		}
		if (pa.alpha() != 0)
			alpha = pa.alpha();
	}
}


/**
 *	Get an attribute for python
 */
PyObject * ProceduralTexturingFunctor::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();

	return HeightPoleFunctor::pyGetAttribute( attr );
}


/**
 *	Set an attribute for python
 */
int ProceduralTexturingFunctor::pySetAttribute( const char * attr, PyObject * value )
{
	PY_SETATTR_STD();

	return HeightPoleFunctor::pySetAttribute( attr, value );
}


/**
 *	Static python factory method
 */
PyObject * ProceduralTexturingFunctor::pyNew( PyObject * args )
{
	return new ProceduralTexturingFunctor;
}


//---------------------------------------------------------
//	Section - ProceduralTexturingFunctor::Rule
//---------------------------------------------------------
ProceduralTexturingFunctor::Rule::Rule( int blendLevel ):
	textureName_( "" ),
	blendLevel_( blendLevel ),
	min_( -FLT_MAX ),
	max_( -FLT_MAX + 1.f )
{
}

bool ProceduralTexturingFunctor::Rule::load( DataSectionPtr pSection, const std::string& suffix )
{
	textureName_ = pSection->readString( "texture", textureName_ );
	min_ = pSection->readFloat(  "min" + suffix, min_ );
	max_ = pSection->readFloat(  "max" + suffix, max_ );
	rng_ = (max_ - min_) / 2.f;
	mid_ = min_ + rng_;
	suffix_ = suffix;

	return true;
}


float ProceduralTexturingFunctor::Rule::match( float amt )
{
	float dst = fabsf(amt-mid_) / rng_;
	//dst = powf(dst,8.f);
	float value = 1.f - dst;
	return Math::clamp( 0.f, value, 1.f );
}
