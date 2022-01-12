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
#include "editor_chunk_sound.hpp"
#include "editor_chunk_substance.ipp"

#include "xactsnd/soundmgr.hpp"

#include "big_bang.hpp"
#include "appmgr/options.hpp"
#include "item_editor.hpp"
#include "item_properties.hpp"
#include "romp/super_model.hpp"
#include "chunk/chunk_model.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunks/editor_chunk.hpp"


// -----------------------------------------------------------------------------
// Section: EditorChunkSound
// -----------------------------------------------------------------------------

int EditorChunkSound_token = 0;

/**
 *	Constructor.
 */
EditorChunkSound::EditorChunkSound() :
	is3d_( false )
{
	transform_.setScale( 5.f, 5.f, 5.f );
}


/**
 *	Destructor.
 */
EditorChunkSound::~EditorChunkSound()
{
}



/**
 *	This method saves the data section pointer before calling its
 *	base class's load method
 */
bool EditorChunkSound::load( DataSectionPtr pSection, Chunk * pChunk )
{
	bool ok = this->EditorChunkSubstance<ChunkSound>::load( pSection, pChunk );
	if (ok)
	{
		is3d_ = pSection->readBool( "3d" );
		soundRes_ = "sounds/" + pSection->readString( "resource" );
		transform_.translation( pSection->readVector3( "position" ) );

		loop_ = pSection->readBool( "loop", true );
		loopDelay_ = pSection->readFloat( "loopDelay", 0.f );
		loopDelayVariance_ = pSection->readFloat( "loopDelayVariance", 0.f );

		this->notifySoundMgr();
	}
	return ok;
}



/**
 *	Save any property changes to this data section
 */
bool EditorChunkSound::edSave( DataSectionPtr pSection )
{
	if (!edCommonSave( pSection ))
		return false;

	pSection->writeBool( "3d", is3d_ );
	pSection->writeString( "resource", soundRes_.substr(7) );
	pSection->writeVector3( "position", transform_.applyToOrigin() );

	// TODO: if the current values for the properties below are the same
	// as the defaults, delete their subsections from the data section.

	pSection->writeFloat( "min", pSound_->minDistance() );
	pSection->writeFloat( "max", pSound_->maxDistance() );

	pSection->writeFloat( "attenuation", pSound_->defaultAttenuation() );
	pSection->writeFloat( "outsideAtten", pSound_->outsideAtten() );
	pSection->writeFloat( "insideAtten", pSound_->insideAtten() );

	pSection->writeFloat( "centreHour", pSound_->centreHour() );
	pSection->writeFloat( "minHour", pSound_->minHour() );
	pSection->writeFloat( "maxHour", pSound_->maxHour() );

	pSection->writeBool( "loop", loop_ );
	pSection->writeFloat( "loopDelay", loopDelay_ );
	pSection->writeFloat( "loopDelayVariance", loopDelayVariance_ );

	return true;
}



/**
 *	Change our transform, temporarily or permanently
 */
bool EditorChunkSound::edTransform( const Matrix & m, bool transient )
{
	// if this is only a temporary change, keep it in the same chunk
	if (transient)
	{
		transform_ = m;
		pSound_->position( pChunk_->transform().applyPoint(
			transform_.applyToOrigin() ), 0 );
		this->notifySoundMgr();
		return true;
	}

	// it's permanent, so find out where we belong now
	Chunk * pOldChunk = pChunk_;
	Chunk * pNewChunk = this->edDropChunk( m.applyToOrigin() );
	if (pNewChunk == NULL) return false;

	// make sure the chunks aren't readonly
	if (!EditorChunkCache::instance( *pOldChunk ).edIsWriteable() 
		|| !EditorChunkCache::instance( *pNewChunk ).edIsWriteable())
		return false;

	// ok, accept the transform change then
	transform_.multiply( m, pOldChunk->transform() );
	transform_.postMultiply( pNewChunk->transformInverse() );
	pSound_->position( pChunk_->transform().applyPoint(
		transform_.applyToOrigin() ), 0 );
	this->notifySoundMgr();

	// note that both affected chunks have seen changes
	BigBang::instance().changedChunk( pOldChunk );
	BigBang::instance().changedChunk( pNewChunk );

	// and move ourselves into the right chunk. we have to do this
	// even if it's the same chunk so the col scene gets recreated
	pOldChunk->delStaticItem( this );
	pNewChunk->addStaticItem( this );

	return true;
}


/**
 *	Add the properties of this flare to the given editor
 */
bool EditorChunkSound::edEdit( class ChunkItemEditor & editor )
{
	editor.addProperty( new GenBoolProperty( "3d",
		new SlowPropReloadingProxy<EditorChunkSound,BoolProxy>(
			this, "ambient or 3d selector", 
			&EditorChunkSound::is3dGet, 
			&EditorChunkSound::is3dSet ) ) );

	editor.addProperty( new ResourceProperty(
		"resource",
		new SlowPropReloadingProxy<EditorChunkSound,StringProxy>(
			this, "sound resource", 
			&EditorChunkSound::resGet, 
			&EditorChunkSound::resSet ),
		".wav" /* for now */ ) );

	MatrixProxy * pMP = new ChunkItemMatrix( this );
	editor.addProperty( new GenPositionProperty( "position", pMP ) );

	editor.addProperty( new GenRadiusProperty( "inner radius",
		new AccessorDataProxy<EditorChunkSound,FloatProxy>(
			this, "inner radius", 
			&EditorChunkSound::innerRadiusGet, 
			&EditorChunkSound::innerRadiusSet ), pMP ) );
	editor.addProperty( new GenRadiusProperty( "outer radius",
		new AccessorDataProxy<EditorChunkSound,FloatProxy>(
			this, "outer radius", 
			&EditorChunkSound::outerRadiusGet, 
			&EditorChunkSound::outerRadiusSet ), pMP ) );

	editor.addProperty( new GenFloatProperty( "attenuation",
		new AccessorDataProxy<EditorChunkSound,FloatProxy>(
			this, "attenuation", 
			&EditorChunkSound::attenuationGet, 
			&EditorChunkSound::attenuationSet ) ) );
	editor.addProperty( new GenFloatProperty( "outside extra attenuation",
		new AccessorDataProxy<EditorChunkSound,FloatProxy>(
			this, "outside extra attenutation",
			&EditorChunkSound::outsideAttenGet, 
			&EditorChunkSound::outsideAttenSet ) ) );
	editor.addProperty( new GenFloatProperty( "inside extra attenuation",
		new AccessorDataProxy<EditorChunkSound,FloatProxy>(
			this, "inside extra attenutation",
			&EditorChunkSound::insideAttenGet, 
			&EditorChunkSound::insideAttenSet ) ) );

	editor.addProperty( new GenFloatProperty( "centre hour",
		new AccessorDataProxy<EditorChunkSound,FloatProxy>(
			this, "centre hour", 
			&EditorChunkSound::centreHourGet, 
			&EditorChunkSound::centreHourSet ) ) );
	editor.addProperty( new GenFloatProperty( "inner hours",
		new AccessorDataProxy<EditorChunkSound,FloatProxy>(
			this, "inner hours", 
			&EditorChunkSound::innerHoursGet, 
			&EditorChunkSound::innerHoursSet ) ) );
	editor.addProperty( new GenFloatProperty( "outer hours",
		new AccessorDataProxy<EditorChunkSound,FloatProxy>(
			this, "outer hours", 
			&EditorChunkSound::outerHoursGet, 
			&EditorChunkSound::outerHoursSet ) ) );

	// There are no interfaces to these in Base3DSound so we have to
	//  set them by reloading the sound. Not too terrible for now.
	editor.addProperty( new GenBoolProperty( "loop",
		new SlowPropReloadingProxy<EditorChunkSound,BoolProxy>(
			this, "loop enable flag", 
			&EditorChunkSound::loopGet, 
			&EditorChunkSound::loopSet ) ) );
	editor.addProperty( new GenFloatProperty( "loop delay",
		new SlowPropReloadingProxy<EditorChunkSound,FloatProxy>(
			this, "loop delay", 
			&EditorChunkSound::loopDelayGet, 
			&EditorChunkSound::loopDelaySet ) ) );
	editor.addProperty( new GenFloatProperty( "loop delay variance",
		new SlowPropReloadingProxy<EditorChunkSound,FloatProxy>(
			this, "loop delay variance", 
			&EditorChunkSound::loopDVGet, 
			&EditorChunkSound::loopDVSet ) ) );

	return true;
}


/**
 *	Reload method. Note that this method hides the reload method in the
 *	template that we derive from, and causes it not to be ever generated.
 *	This is most fortunate, because that (hidden) method would not compile
 *	because it calls 'load' with the wrong arguments. More template fun!
 */
bool EditorChunkSound::reload()
{
	bool ok = this->load( pOwnSect_, pChunk_ );
	return ok;
}

#define NOEXEC rf
#define ECS_FLOAT_PROP( NAME, METH, EXEC, CHECK )		\
float EditorChunkSound::NAME##Get() const				\
{														\
	return pSound_->METH();								\
}														\
														\
bool EditorChunkSound::NAME##Set( const float & rf )	\
{														\
	float f = EXEC;										\
	if (!(CHECK)) return false;							\
	pSound_->METH( f );									\
	this->notifySoundMgr();								\
	return true;										\
}														\

// These restrictions on min and max are the sound manager's.
// It asserts (!) if a sound is loaded with min >= max.
ECS_FLOAT_PROP( innerRadius, minDistance,
	NOEXEC, f >= 0.f && f <= pSound_->maxDistance() )
ECS_FLOAT_PROP( outerRadius, maxDistance,
	NOEXEC, f >= 0.f && f >= pSound_->minDistance() )

// Only the default attenuation needs to be <= 0,
// the others can be either boost or a drop the volume.
ECS_FLOAT_PROP( attenuation, defaultAttenuation, NOEXEC, f <= 0.f )
ECS_FLOAT_PROP( outsideAtten, outsideAtten, NOEXEC, 1 )
ECS_FLOAT_PROP( insideAtten, insideAtten, NOEXEC, 1 )

// We check with this check condition and normalise the hour,
// but we still check to make sure it's not negative
// (not sure if fmod will always do this for us).
// SoundMgr does not do asserts on min and max hour.
ECS_FLOAT_PROP( centreHour, centreHour, fmodf( rf, 24.f ), f >= 0.f )
ECS_FLOAT_PROP( innerHours, minHour, fmodf( rf, 24.f ), f >= 0.f )
ECS_FLOAT_PROP( outerHours, maxHour, fmodf( rf, 24.f ), f >= 0.f )



/**
 *	Return a modelptr that is the representation of this chunk item
 */
ModelPtr EditorChunkSound::reprModel() const
{
	static ModelPtr model =
		Model::get( "resources/models/sound.model" );
	return model;
}




/**
 *	This method lets the sound manager know that its sound has changed.
 *	
 *	Currently, we can only tell it to update all sounds ... but that's ok.
 */
void EditorChunkSound::notifySoundMgr()
{
	soundMgr().updateASAP();
}


/// Write the factory statics stuff
#undef IMPLEMENT_CHUNK_ITEM_ARGS
#define IMPLEMENT_CHUNK_ITEM_ARGS (pSection, pChunk)
IMPLEMENT_CHUNK_ITEM( EditorChunkSound, sound, 1 )

// editor_chunk_sound.cpp
