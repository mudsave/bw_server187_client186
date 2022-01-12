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
#include "editor_chunk_light.hpp"
#include "editor_chunk_substance.ipp"

#include "big_bang.hpp"
#include "appmgr/options.hpp"
#include "item_editor.hpp"
#include "item_properties.hpp"
#include "romp/super_model.hpp"
#include "chunk/chunk_model.hpp"
#include "chunks/editor_chunk.hpp"
#include "chunk/chunk_manager.hpp"


static const uint32	GIZMO_INNER_COLOUR = 0xbf10ff10;
static const float	GIZMO_INNER_RADIUS = 2.0f;
static const uint32	GIZMO_OUTER_COLOUR = 0xbf1010ff;
static const float	GIZMO_OUTER_RADIUS = 4.0f;


// -----------------------------------------------------------------------------
// Section: LightColourWrapper
// -----------------------------------------------------------------------------


/**
 *	This helper class gets and sets a light colour.
 *	Since all the lights have a 'colour' accessor, but there's no
 *	base class that collects them (for setting anyway), we use a template.
 */
template <class LT> class LightColourWrapper :
	public UndoableDataProxy<ColourProxy>
{
public:
	explicit LightColourWrapper( SmartPointer<LT> pItem ) :
		pItem_( pItem )
	{
	}

	virtual Moo::Colour EDCALL get() const
	{
		return pItem_->pLight()->colour();
	}

	virtual void EDCALL setTransient( Moo::Colour v )
	{
		pItem_->pLight()->colour( v );
	}

	virtual bool EDCALL setPermanent( Moo::Colour v )
	{
		// make it valid
		if (v.r < 0.f) v.r = 0.f;
		if (v.r > 1.f) v.r = 1.f;
		if (v.g < 0.f) v.g = 0.f;
		if (v.g > 1.f) v.g = 1.f;
		if (v.b < 0.f) v.b = 0.f;
		if (v.b > 1.f) v.b = 1.f;
		v.a = 1.f;

		// set it
		this->setTransient( v );

		// flag the chunk as having changed
		BigBang::instance().changedChunk( pItem_->chunk() );

		pItem_->markInfluencedChunks();

		// update its data section
		pItem_->edSave( pItem_->pOwnSect() );

		return true;
	}

	virtual std::string EDCALL opName()
	{
		return "Set " + pItem_->edDescription() + " colour";
	}

private:
	SmartPointer<LT>	pItem_;
};


// -----------------------------------------------------------------------------
// Section: EditorChunkDirectionalLight
// -----------------------------------------------------------------------------

IMPLEMENT_CHUNK_ITEM( EditorChunkDirectionalLight, directionalLight, 1 )


/**
 *	Save our data to the given data section
 */
bool EditorChunkDirectionalLight::edSave( DataSectionPtr pSection )
{
	if (!edCommonSave( pSection ))
		return false;

	Moo::Colour vcol = pLight_->colour();
	pSection->writeVector3( "colour",
		Vector3( vcol.r, vcol.g, vcol.b ) * 255.f );

	pSection->writeVector3( "direction", pLight_->direction() );

	pSection->writeBool( "dynamic", dynamicLight() );
	pSection->writeBool( "static", staticLight() );	
	pSection->writeBool( "specular", specularLight() );	

	pSection->writeFloat( "multiplier", pLight_->multiplier() );

	return true;
}


/**
 *	Add our properties to the given editor
 */
bool EditorChunkDirectionalLight::edEdit( class ChunkItemEditor & editor )
{
	editor.addProperty( new ColourProperty( "colour",
		new LightColourWrapper<EditorChunkDirectionalLight>( this ) ) );

	MatrixProxy * pMP = new ChunkItemMatrix( this );
	editor.addProperty( new GenRotationProperty( "direction", pMP ) );

	editor.addProperty( new GenBoolProperty( "static", 
		new AccessorDataProxy< EditorChunkDirectionalLight, BoolProxy >(
			this, "static", 
			&EditorChunkDirectionalLight::staticLightGet, 
			&EditorChunkDirectionalLight::staticLightSet ) ) );

	editor.addProperty( new GenBoolProperty( "dynamic", 
		new AccessorDataProxy< EditorChunkDirectionalLight, BoolProxy >(
			this, "dynamic", 
			&EditorChunkDirectionalLight::dynamicLightGet, 
			&EditorChunkDirectionalLight::dynamicLightSet ) ) );

	editor.addProperty( new GenBoolProperty( "specular", 
		new AccessorDataProxy< EditorChunkDirectionalLight, BoolProxy >(
			this, "specular", 
			&EditorChunkDirectionalLight::specularLightGet, 
			&EditorChunkDirectionalLight::specularLightSet ) ) );

	editor.addProperty( new GenFloatProperty( "multiplier",
		new AccessorDataProxy<EditorChunkDirectionalLight,FloatProxy>(
			this, "multiplier", 
			&EditorChunkDirectionalLight::getMultiplier, 
			&EditorChunkDirectionalLight::setMultiplier ) ) );

	return true;
}


/**
 *	Get the current transform
 */
const Matrix & EditorChunkDirectionalLight::edTransform()
{
	/*Vector3 dir = pLight_->direction();
	Vector3 up( 0.f, 1.f, 0.f );
	if (fabs(up.dotProduct( dir )) > 0.9f) up = Vector3( 0.f, 0.f, 1.f );
	Vector3 across = dir.crossProduct( up );
	across.normalise();
	transform_[0] = across;
	transform_[1] = across.crossProduct(dir);
	transform_[2] = dir;*/
	if (pChunk_ != NULL)
	{
		const BoundingBox & bb = pChunk_->boundingBox();
		transform_.translation(
			pChunk_->transformInverse().applyPoint(
				(bb.maxBounds() + bb.minBounds()) / 2.f ) );
	}
	else
	{
		transform_.translation( Vector3::zero() );
	}

	return transform_;
}


/**
 *	Change our transform, temporarily or permanently
 */
bool EditorChunkDirectionalLight::edTransform(
	const Matrix & m, bool transient )
{
	// if this is only a temporary change, keep it in the same chunk
	if (transient)
	{
		transform_ = m;
		pLight_->direction( transform_.applyToUnitAxisVector(2) );
		pLight_->worldTransform( pChunk_->transform() );
		return true;
	}

	// it's permanent, but we have no position so we're not changing chunks
	transform_ = m;
	pLight_->direction( transform_.applyToUnitAxisVector(2) );
	pLight_->worldTransform( pChunk_->transform() );

	BigBang::instance().changedChunk( pChunk_ );

	// and move ourselves into the right chunk. we have to do this
	// even if it's the same chunk so the col scene gets recreated
	// (and other things methinks)
	Chunk * pChunk = pChunk_;
	pChunk->delStaticItem( this );
	pChunk->addStaticItem( this );

	// Recalculate static lighting for our chunk
	StaticLighting::markChunk( pChunk_ );

	return true;
}

void EditorChunkDirectionalLight::loadModel()
{
	model_ = Model::get( "resources/models/directional_light.model" );
}

bool EditorChunkDirectionalLight::load( DataSectionPtr pSection )
{
	if (!EditorChunkMooLight<ChunkDirectionalLight>::load( pSection ))
		return false;

	// Set the transform
	Vector3 dir = pLight_->direction();
	dir.normalise();

	Vector3 up( 0.f, 1.f, 0.f );
	if (fabsf( up.dotProduct( dir ) ) > 0.9f)
		up = Vector3( 0.f, 0.f, 1.f );

	Vector3 xaxis = up.crossProduct( dir );
	xaxis.normalise();

	transform_[1] = xaxis;
	transform_[0] = xaxis.crossProduct( dir );
	transform_[0].normalise();
	transform_[2] = dir;
	transform_.translation( Vector3(0,0,0) );

	pLight_->multiplier( pSection->readFloat( "multiplier", 1.f ) );

	return true;
}




// -----------------------------------------------------------------------------
// Section: EditorChunkOmniLight
// -----------------------------------------------------------------------------

IMPLEMENT_CHUNK_ITEM( EditorChunkOmniLight, omniLight, 1 )

bool EditorChunkOmniLight::load( DataSectionPtr pSection )
{
	if (!EditorChunkPhysicalMooLight<ChunkOmniLight>::load( pSection ))
		return false;

	pLight_->multiplier( pSection->readFloat( "multiplier", 1.f ) );

	return true;
}

/**
 *	Save our data to the given data section
 */
bool EditorChunkOmniLight::edSave( DataSectionPtr pSection )
{
	if (!edCommonSave( pSection ))
		return false;

	Moo::Colour vcol = pLight_->colour();
	pSection->writeVector3( "colour",
		Vector3( vcol.r, vcol.g, vcol.b ) * 255.f );

	pSection->writeVector3( "position", pLight_->position() );
	pSection->writeFloat( "innerRadius", pLight_->innerRadius() );
	pSection->writeFloat( "outerRadius", pLight_->outerRadius() );

	pSection->writeBool( "dynamic", dynamicLight() );
	pSection->writeBool( "static", staticLight() );
	pSection->writeBool( "specular", specularLight() );	

	pSection->writeFloat( "multiplier", pLight_->multiplier() );

	return true;
}


/**
 *	This helper class gets and sets a radius. It works on any chunk item
 *	that has a pLight member which returns an object with innerRadius
 *	and outerRadius.
 */
template <class LT> class LightRadiusWrapper :
	public UndoableDataProxy<FloatProxy>
{
public:
	LightRadiusWrapper( SmartPointer<LT> pItem, bool isOuter ) :
		pItem_( pItem ),
		isOuter_( isOuter )
	{
	}

	virtual float EDCALL get() const
	{
		if (isOuter_)
		{
			return pItem_->pLight()->outerRadius();
		}
		else
		{
			return pItem_->pLight()->innerRadius();
		}
	}

	virtual void EDCALL setTransient( float f )
	{
		if (isOuter_)
		{
			pItem_->pLight()->outerRadius( f );
		}
		else
		{
			pItem_->pLight()->innerRadius( f );
		}

		// update world inner and outer
		Matrix world = pItem_->chunk()->transform();
		pItem_->pLight()->worldTransform( world );
	}

	virtual bool EDCALL setPermanent( float f )
	{
		// make sure it's valid
		if (f < 0.f) return false;

		// mark old chucks for recalc
		StaticLighting::markChunks( pItem_->chunk(), pItem_->pLight() );

		// toss the chunk to refresh the light states
		Chunk* chunk = pItem_->chunk();
		chunk->delStaticItem( pItem_ );
		chunk->addStaticItem( pItem_ );

		// set it
		this->setTransient( f );

		// flag the chunk as having changed
		BigBang::instance().changedChunk( pItem_->chunk() );

		// mark new chucks for recalc
		StaticLighting::markChunks( pItem_->chunk(), pItem_->pLight() );

		// update its data section
		pItem_->edSave( pItem_->pOwnSect() );

		return true;
	}

	virtual std::string EDCALL opName()
	{
		return "Set " + pItem_->edDescription() +
			(isOuter_ ? " out" : " inn") + "er radius";
	}

private:
	SmartPointer<LT>	pItem_;
	bool				isOuter_;
};



/**
 *	Add our properties to the given editor
 */
bool EditorChunkOmniLight::edEdit( class ChunkItemEditor & editor )
{
	editor.addProperty( new ColourProperty( "colour",
		new LightColourWrapper<EditorChunkOmniLight>( this ) ) );

	MatrixProxy * pMP = new ChunkItemMatrix( this );
	editor.addProperty( new GenPositionProperty( "position", pMP ) );

	editor.addProperty( new GenRadiusProperty( "inner radius",
		new LightRadiusWrapper<EditorChunkOmniLight>( this, false ), pMP,
		GIZMO_INNER_COLOUR, GIZMO_INNER_RADIUS ) );
	editor.addProperty( new GenRadiusProperty( "outer radius",
		new LightRadiusWrapper<EditorChunkOmniLight>( this, true ), pMP,
		GIZMO_OUTER_COLOUR, GIZMO_OUTER_RADIUS ) );

	editor.addProperty( new GenBoolProperty( "static", 
		new AccessorDataProxy< EditorChunkOmniLight, BoolProxy >(
			this, "static", 
			&EditorChunkOmniLight::staticLightGet, 
			&EditorChunkOmniLight::staticLightSet ) ) );

	editor.addProperty( new GenBoolProperty( "dynamic", 
		new AccessorDataProxy< EditorChunkOmniLight, BoolProxy >(
			this, "dynamic", 
			&EditorChunkOmniLight::dynamicLightGet, 
			&EditorChunkOmniLight::dynamicLightSet ) ) );

	editor.addProperty( new GenBoolProperty( "specular", 
		new AccessorDataProxy< EditorChunkOmniLight, BoolProxy >(
			this, "specular", 
			&EditorChunkOmniLight::specularLightGet, 
			&EditorChunkOmniLight::specularLightSet ) ) );

	editor.addProperty( new GenFloatProperty( "multiplier",
		new AccessorDataProxy<EditorChunkOmniLight,FloatProxy>(
			this, "multiplier", 
			&EditorChunkOmniLight::getMultiplier, 
			&EditorChunkOmniLight::setMultiplier ) ) );



	return true;
}


/**
 *	Get the current transform
 */
const Matrix & EditorChunkOmniLight::edTransform()
{
	transform_.setIdentity();
	transform_.translation( pLight_->position() );

	return transform_;
}

/*
template<class LightType>
bool influencesReadOnlyChunk( Chunk* srcChunk, Vector3 atPosition, LightType light,
	std::set<Chunk*>& searchedChunks = std::set<Chunk*>(),
	int currentDepth = 0 )
{
	searchedChunks.insert( srcChunk );

	if (!EditorChunkCache::instance( *srcChunk ).edIsLocked())
		return true;

	// Stop if we've reached the maximum portal traversal depth
	if (currentDepth == StaticLighting::STATIC_LIGHT_PORTAL_DEPTH)
		return false;

	for (Chunk::piterator pit = srcChunk->pbegin(); pit != srcChunk->pend(); pit++)
	{
		if (!pit->hasChunk() || !pit->pChunk->online())
			continue;

		// Don't mark outside chunks
		//if (pit->pChunk->isOutsideChunk())
		//	continue;

		// ^^^ TEMP! commented out for testing only

		// We've already marked it, skip
		if ( searchedChunks.find( pit->pChunk ) != searchedChunks.end() )
			continue;

		if ( !pit->pChunk->boundingBox().intersects( atPosition, light->worldOuterRadius() ) )
			continue;

		if ( influencesReadOnlyChunk( pit->pChunk, atPosition, light, searchedChunks, currentDepth + 1 ) )
			return true;
	}

	return false;
}
*/



/**
 *	Change our transform, temporarily or permanently
 */
bool EditorChunkOmniLight::edTransform( const Matrix & m, bool transient )
{
	// it's permanent, so find out where we belong now
	Chunk * pOldChunk = pChunk_;
	Chunk * pNewChunk = this->edDropChunk( m.applyToOrigin() );
	if (pNewChunk == NULL) return false;

	// if this is only a temporary change, keep it in the same chunk
	if (transient)
	{
		transform_ = m;
		pLight_->position( transform_.applyToOrigin() );
		pLight_->worldTransform( pChunk_->transform() );
		return true;
	}

	// make sure the chunks aren't readonly
	if (!EditorChunkCache::instance( *pOldChunk ).edIsWriteable() 
		|| !EditorChunkCache::instance( *pNewChunk ).edIsWriteable())
		return false;

	// Ok, harder question: if we influence any non locked chunks in our old or new positions,
	// we can't move (+ do the same thing for edEdit, and use readonly props in the same case)

	/*
	if (influencesReadOnlyChunk( pOldChunk, pLight_->worldPosition(), pLight_ ))
		return false;
	if (influencesReadOnlyChunk( pNewChunk, pNewChunk->transform().applyPoint( pLight_->position() ), pLight_ ))
		return false;
	*/

	// ^^^ Ignored the problem by making static lights only traverse a single
	// portal, thus our 1 grid square barrier will take care of everything


	// Update static lighting in the old location
	markInfluencedChunks();


	// ok, accept the transform change then
	transform_.multiply( m, pOldChunk->transform() );
	transform_.postMultiply( pNewChunk->transformInverse() );
	pLight_->position( transform_.applyToOrigin() );
	pLight_->worldTransform( pNewChunk->transform() );

	// note that both affected chunks have seen changes
	BigBang::instance().changedChunk( pOldChunk );
	BigBang::instance().changedChunk( pNewChunk );

	// and move ourselves into the right chunk. we have to do this
	// even if it's the same chunk so the col scene gets recreated
	pOldChunk->delStaticItem( this );
	pNewChunk->addStaticItem( this );

	// Update static lighting in the new location
	markInfluencedChunks();


	return true;
}


void EditorChunkOmniLight::loadModel()
{
	ModelPtr model;

	// We need to keep the static and dynamic models in member variables to
	// prevent a crash. If we don't keep both models, switching models here
	// deletes the old model, but since "delObstacles" marks the column as
	// stale but doesn't delete the actual obstacle, and because
	// ChunkBSPObstacle has a reference to the BSP of this deleted model, a
	// collide test results in a crash.
	if (staticLight())
	{
		if (!modelStatic_)
		{
			modelStatic_ = Model::get( "resources/models/static.model" );
		}
		model = modelStatic_;
	}
	else
	{
		if (!modelDynamic_)
		{
			modelDynamic_ = Model::get( "resources/models/dynamic.model" );
		}
		model = modelDynamic_;
	}
	if( model_ != model )
	{
		if( pChunk_ )
			ChunkModelObstacle::instance( *pChunk_ ).delObstacles( this );
		model_ = model;
		if( pChunk_ )
			this->addAsObstacle();
	}
}





// -----------------------------------------------------------------------------
// Section: EditorChunkSpotLight
// -----------------------------------------------------------------------------

IMPLEMENT_CHUNK_ITEM( EditorChunkSpotLight, spotLight, 1 )


bool EditorChunkSpotLight::load( DataSectionPtr pSection )
{
	if (!EditorChunkPhysicalMooLight<ChunkSpotLight>::load( pSection ))
		return false;

	// Set the transform
	Vector3 dir = - pLight_->direction();
	dir.normalise();

	Vector3 up( 0.f, 0.f, 1.f );
	if (fabsf( up.dotProduct( dir ) ) > 0.9f)
		up = Vector3( 1.f, 0.f, 0.f );

	Vector3 xaxis = up.crossProduct( dir );
	xaxis.normalise();

	transform_[0] = xaxis;
	transform_[2] = xaxis.crossProduct( dir ) * -1.f;
	transform_[2].normalise();
	transform_[1] = dir;
	transform_.translation( pLight_->position() );

	pLight_->multiplier( pSection->readFloat( "multiplier", 1.f ) );

	return true;
}



/**
 *	Save our data to the given data section
 */
bool EditorChunkSpotLight::edSave( DataSectionPtr pSection )
{
	if (!edCommonSave( pSection ))
		return false;

	Moo::Colour vcol = pLight_->colour();
	pSection->writeVector3( "colour",
		Vector3( vcol.r, vcol.g, vcol.b ) * 255.f );

	pSection->writeVector3( "position", pLight_->position() );
	pSection->writeVector3( "direction", pLight_->direction() );
	pSection->writeFloat( "innerRadius", pLight_->innerRadius() );
	pSection->writeFloat( "outerRadius", pLight_->outerRadius() );
	pSection->writeFloat( "cosConeAngle", pLight_->cosConeAngle() );

	pSection->writeBool( "dynamic", dynamicLight() );
	pSection->writeBool( "static", staticLight() );
	pSection->writeBool( "specular", specularLight() );	

	pSection->writeFloat( "multiplier", pLight_->multiplier() );

	return true;
}


/**
 *	This class is the data under a spot light's angle property.
 */
class SLAngleWrapper : public UndoableDataProxy<FloatProxy>
{
public:
	explicit SLAngleWrapper( SmartPointer<EditorChunkSpotLight> pItem ) :
		pItem_( pItem )
	{
	}

	virtual float EDCALL get() const
	{
		return RAD_TO_DEG( acosf( pItem_->pLight()->cosConeAngle() ));
	}

	virtual void EDCALL setTransient( float f )
	{
		pItem_->pLight()->cosConeAngle( cosf( DEG_TO_RAD(f) ) );
	}

	virtual bool EDCALL setPermanent( float f )
	{
		// complain if it's invalid
		if (f < 0.01f || f > 180.f - 0.01f) return false;

		// set it
		this->setTransient( f );

		// flag the chunk as having changed
		BigBang::instance().changedChunk( pItem_->chunk() );

		pItem_->markInfluencedChunks();

		// update its data section
		pItem_->edSave( pItem_->pOwnSect() );

		return true;
	}

	virtual std::string EDCALL opName()
	{
		return "Set " + pItem_->edDescription() + " cone angle";
	}

private:
	SmartPointer<EditorChunkSpotLight>	pItem_;
};



/**
 *	Add our properties to the given editor
 */
bool EditorChunkSpotLight::edEdit( class ChunkItemEditor & editor )
{
	editor.addProperty( new ColourProperty( "colour",
		new LightColourWrapper<EditorChunkSpotLight>( this ) ) );

	MatrixProxy * pMP = new ChunkItemMatrix( this );
	editor.addProperty( new GenPositionProperty( "position", pMP ) );
	editor.addProperty( new GenRotationProperty( "direction", pMP ) );

	editor.addProperty( new GenRadiusProperty( "inner radius",
		new LightRadiusWrapper<EditorChunkSpotLight>( this, false ), pMP,
		GIZMO_INNER_COLOUR, GIZMO_INNER_RADIUS ) );
	editor.addProperty( new GenRadiusProperty( "outer radius",
		new LightRadiusWrapper<EditorChunkSpotLight>( this, true ), pMP,
		GIZMO_OUTER_COLOUR, GIZMO_OUTER_RADIUS ) );

	editor.addProperty( new AngleProperty( "cone angle",
		new SLAngleWrapper( this ), pMP ) );

	editor.addProperty( new GenBoolProperty( "static", 
		new AccessorDataProxy< EditorChunkSpotLight, BoolProxy >(
			this, "static", 
			&EditorChunkSpotLight::staticLightGet, 
			&EditorChunkSpotLight::staticLightSet ) ) );

	editor.addProperty( new GenBoolProperty( "dynamic", 
		new AccessorDataProxy< EditorChunkSpotLight, BoolProxy >(
			this, "dynamic", 
			&EditorChunkSpotLight::dynamicLightGet, 
			&EditorChunkSpotLight::dynamicLightSet ) ) );

	editor.addProperty( new GenBoolProperty( "specular", 
		new AccessorDataProxy< EditorChunkSpotLight, BoolProxy >(
			this, "specular", 
			&EditorChunkSpotLight::specularLightGet, 
			&EditorChunkSpotLight::specularLightSet ) ) );

	editor.addProperty( new GenFloatProperty( "multiplier",
		new AccessorDataProxy<EditorChunkSpotLight,FloatProxy>(
			this, "multiplier", 
			&EditorChunkSpotLight::getMultiplier, 
			&EditorChunkSpotLight::setMultiplier ) ) );


	return true;
}


/**
 *	Get the current transform
 */
const Matrix & EditorChunkSpotLight::edTransform()
{
	return transform_;
}


/**
 *	Change our transform, temporarily or permanently
 */
bool EditorChunkSpotLight::edTransform( const Matrix & m, bool transient )
{
	Vector3 posn;

	// it's permanent, so find out where we belong now
	Chunk * pOldChunk = pChunk_;
	Chunk * pNewChunk = this->edDropChunk( m.applyToOrigin() );
	if (pNewChunk == NULL) return false;

	// if this is only a temporary change, keep it in the same chunk
	if (transient)
	{
		transform_ = m;
		pLight_->position( transform_.applyToOrigin() );
		pLight_->direction( transform_.applyToUnitAxisVector(1) * -1.f);
		pLight_->worldTransform( pChunk_->transform() );

		posn = pLight_->direction();
		//dprintf( "transient dirn (%f,%f,%f)\n", posn[0], posn[1], posn[2] );

		return true;
	}

	posn = m.applyToUnitAxisVector(1) * -1.f;
	//dprintf( "permanent dirn (%f,%f,%f)\n", posn[0], posn[1], posn[2] );

	// make sure the chunks aren't readonly
	if (!EditorChunkCache::instance( *pOldChunk ).edIsWriteable() 
		|| !EditorChunkCache::instance( *pNewChunk ).edIsWriteable())
		return false;

	// Update static lighting in the old location
	markInfluencedChunks();

	// ok, accept the transform change then
	transform_.multiply( m, pOldChunk->transform() );
	transform_.postMultiply( pNewChunk->transformInverse() );
	pLight_->position( transform_.applyToOrigin() );
	pLight_->direction( transform_.applyToUnitAxisVector(1) * -1.f );
	pLight_->worldTransform( pNewChunk->transform() );

	// note that both affected chunks have seen changes
	BigBang::instance().changedChunk( pOldChunk );
	BigBang::instance().changedChunk( pNewChunk );

	// and move ourselves into the right chunk. we have to do this
	// even if it's the same chunk so the col scene gets recreated
	pOldChunk->delStaticItem( this );
	pNewChunk->addStaticItem( this );

	// Update static lighting in the new location
	markInfluencedChunks();

	return true;
}


void EditorChunkSpotLight::loadModel()
{
	model_ = Model::get( "resources/models/spot_light.model" );
}

// -----------------------------------------------------------------------------
// Section: PulseColourWrapper
// -----------------------------------------------------------------------------

/**
 *	This helper class gets and sets a light colour on a pulse light.
 */
class PulseColourWrapper :
	public UndoableDataProxy<ColourProxy>
{
public:
	explicit PulseColourWrapper( SmartPointer<EditorChunkPulseLight> pItem ) :
		pItem_( pItem )
	{
	}

	virtual Moo::Colour EDCALL get() const
	{
		return pItem_->colour();
	}

	virtual void EDCALL setTransient( Moo::Colour v )
	{
		pItem_->colour( v );
	}

	virtual bool EDCALL setPermanent( Moo::Colour v )
	{
		// make it valid
		if (v.r < 0.f) v.r = 0.f;
		if (v.r > 1.f) v.r = 1.f;
		if (v.g < 0.f) v.g = 0.f;
		if (v.g > 1.f) v.g = 1.f;
		if (v.b < 0.f) v.b = 0.f;
		if (v.b > 1.f) v.b = 1.f;
		v.a = 1.f;

		// set it
		this->setTransient( v );

		// flag the chunk as having changed
		BigBang::instance().changedChunk( pItem_->chunk() );

		pItem_->markInfluencedChunks();

		// update its data section
		pItem_->edSave( pItem_->pOwnSect() );

		return true;
	}

	virtual std::string EDCALL opName()
	{
		return "Set " + pItem_->edDescription() + " colour";
	}

private:
	SmartPointer<EditorChunkPulseLight>	pItem_;
};

// -----------------------------------------------------------------------------
// Section: EditorChunkPulseLight
// -----------------------------------------------------------------------------

IMPLEMENT_CHUNK_ITEM( EditorChunkPulseLight, pulseLight, 1 )

bool EditorChunkPulseLight::load( DataSectionPtr pSection )
{
	if (!EditorChunkPhysicalMooLight<ChunkPulseLight>::load( pSection ))
		return false;

	this->staticLight( false );

	pLight_->multiplier( pSection->readFloat( "multiplier", 1.f ) );

	return true;
}

/**
 *	Save our data to the given data section
 */
bool EditorChunkPulseLight::edSave( DataSectionPtr pSection )
{
	if (!edCommonSave( pSection ))
		return false;

	Moo::Colour vcol = colour();
	pSection->writeVector3( "colour",
		Vector3( vcol.r, vcol.g, vcol.b ) * 255.f );

	pSection->writeVector3( "position", position_ );
	pSection->writeFloat( "innerRadius", pLight_->innerRadius() );
	pSection->writeFloat( "outerRadius", pLight_->outerRadius() );

	pSection->writeFloat( "multiplier", pLight_->multiplier() );

	return true;
}


/**
 *	Add our properties to the given editor
 */
bool EditorChunkPulseLight::edEdit( class ChunkItemEditor & editor )
{
	editor.addProperty( new ColourProperty( "colour",
		new PulseColourWrapper( this ) ) );

	MatrixProxy * pMP = new ChunkItemMatrix( this );
	editor.addProperty( new GenPositionProperty( "position", pMP ) );

	editor.addProperty( new GenRadiusProperty( "inner radius",
		new LightRadiusWrapper<EditorChunkPulseLight>( this, false ), pMP,
		GIZMO_INNER_COLOUR, GIZMO_INNER_RADIUS ) );
	editor.addProperty( new GenRadiusProperty( "outer radius",
		new LightRadiusWrapper<EditorChunkPulseLight>( this, true ), pMP,
		GIZMO_OUTER_COLOUR, GIZMO_OUTER_RADIUS ) );

	editor.addProperty( new GenFloatProperty( "multiplier",
		new AccessorDataProxy<EditorChunkPulseLight,FloatProxy>(
			this, "multiplier", 
			&EditorChunkPulseLight::getMultiplier, 
			&EditorChunkPulseLight::setMultiplier ) ) );



	return true;
}


/**
 *	Get the current transform
 */
const Matrix & EditorChunkPulseLight::edTransform()
{
	transform_.setIdentity();
	transform_.translation( position_ );

	return transform_;
}

/**
 *	Change our transform, temporarily or permanently
 */
bool EditorChunkPulseLight::edTransform( const Matrix & m, bool transient )
{
	// it's permanent, so find out where we belong now
	Chunk * pOldChunk = pChunk_;
	Chunk * pNewChunk = this->edDropChunk( m.applyToOrigin() );
	if (pNewChunk == NULL) return false;

	// if this is only a temporary change, keep it in the same chunk
	if (transient)
	{
		transform_ = m;
		position_ = transform_.applyToOrigin();
		pLight_->position( position_ + animPosition_ );
		pLight_->worldTransform( pChunk_->transform() );
		return true;
	}

	// make sure the chunks aren't readonly
	if (!EditorChunkCache::instance( *pOldChunk ).edIsWriteable() 
		|| !EditorChunkCache::instance( *pNewChunk ).edIsWriteable())
		return false;

	// Ok, harder question: if we influence any non locked chunks in our old or new positions,
	// we can't move (+ do the same thing for edEdit, and use readonly props in the same case)

	/*
	if (influencesReadOnlyChunk( pOldChunk, pLight_->worldPosition(), pLight_ ))
		return false;
	if (influencesReadOnlyChunk( pNewChunk, pNewChunk->transform().applyPoint( pLight_->position() ), pLight_ ))
		return false;
	*/

	// ^^^ Ignored the problem by making static lights only traverse a single
	// portal, thus our 1 grid square barrier will take care of everything


	// Update static lighting in the old location
	markInfluencedChunks();


	// ok, accept the transform change then
	transform_.multiply( m, pOldChunk->transform() );
	transform_.postMultiply( pNewChunk->transformInverse() );
	position_ =  transform_.applyToOrigin();
	pLight_->position( position_ + animPosition_ );
	pLight_->worldTransform( pNewChunk->transform() );

	// note that both affected chunks have seen changes
	BigBang::instance().changedChunk( pOldChunk );
	BigBang::instance().changedChunk( pNewChunk );

	// and move ourselves into the right chunk. we have to do this
	// even if it's the same chunk so the col scene gets recreated
	pOldChunk->delStaticItem( this );
	pNewChunk->addStaticItem( this );

	// Update static lighting in the new location
	markInfluencedChunks();


	return true;
}


void EditorChunkPulseLight::loadModel()
{
	model_ = Model::get( "resources/models/dynamic.model" );
}




// -----------------------------------------------------------------------------
// Section: EditorChunkAmbientLight
// -----------------------------------------------------------------------------

ChunkItemFactory EditorChunkAmbientLight::factory_( "ambientLight", 1, EditorChunkAmbientLight::create );

ChunkItemFactory::Result EditorChunkAmbientLight::create( Chunk * pChunk, DataSectionPtr pSection )
{
	{
		MatrixMutexHolder lock( pChunk );
		std::vector<ChunkItemPtr> items = EditorChunkCache::instance( *pChunk ).staticItems();
		for( std::vector<ChunkItemPtr>::iterator iter = items.begin(); iter != items.end(); ++iter )
		{
			if( strcmp( (*iter) -> edClassName(), "ChunkAmbientLight" ) == 0 )
			{
				// Check to see for loading errors (Two ambient lights in one chunk file) 
 				if ( !pChunk->loaded() ) 
 				{ 
					WARNING_MSG( "Chunk %s contains two or more ambient lights.\n", pChunk->identifier().c_str() );
 					return ChunkItemFactory::SucceededWithoutItem(); 
 				} 
				(*iter)->edPreDelete(); 
 				pChunk->delStaticItem( *iter ); 

 				return ChunkItemFactory::Result( (*iter), "The old ambient light was removed", true ); 
			}
		}
	}

	EditorChunkAmbientLight* pItem = new EditorChunkAmbientLight();

	if( pItem->load( pSection ) )
	{
		pChunk->addStaticItem( pItem );
		return ChunkItemFactory::Result( pItem );
	}

	delete pItem;
	return ChunkItemFactory::Result( NULL, "Failed to create ambient light" );
}

/**
 *	Save our data to the given data section
 */
bool EditorChunkAmbientLight::edSave( DataSectionPtr pSection )
{
	MF_ASSERT( pSection );

	if (!edCommonSave( pSection ))
		return false;

	Moo::Colour vcol = colour_;
	pSection->writeVector3( "colour",
		Vector3( vcol.r, vcol.g, vcol.b ) * 255.f );

	pSection->writeFloat( "multiplier", multiplier() );
	return true;
}


/**
 *	Add our properties to the given editor
 */
bool EditorChunkAmbientLight::edEdit( class ChunkItemEditor & editor )
{
	MatrixProxy * pMP = new ChunkItemMatrix( this );
	editor.addProperty( new GenPositionProperty( "position", pMP ) );

	editor.addProperty( new ColourProperty( "colour",
		new LightColourWrapper<EditorChunkAmbientLight>( this ) ) );

	editor.addProperty( new GenFloatProperty( "multiplier",
		new AccessorDataProxy<EditorChunkAmbientLight,FloatProxy>(
			this, "multiplier", 
			&EditorChunkAmbientLight::getMultiplier, 
			&EditorChunkAmbientLight::setMultiplier ) ) );
	return true;
}

namespace
{
	/**
	 * Make m refer to the centre of chunk
	 * Used to ensure the ambient light always sits in the centre of a chunk
	 */
	void setToCentre( Matrix& m, Chunk* chunk )
	{
		m.setIdentity();
		if ( chunk != NULL )
		{
			if ( !chunk->isOutsideChunk() )
			{
				// it's a shell, so get the 'real' bounding box by getting the
				// bounds of the first shell model in it
				EditorChunkCache& cc = EditorChunkCache::instance( *chunk );
				MatrixMutexHolder lock( chunk );
				std::vector<ChunkItemPtr> items = cc.staticItems();
				for ( std::vector<ChunkItemPtr>::iterator i = items.begin();
					i != items.end(); ++i )
				{
					if ( (*i)->isShellModel() )
					{
						BoundingBox bb;
						(*i)->edBounds( bb );
						m.translation( (bb.maxBounds() + bb.minBounds()) / 2.f );
						break;
					}
				}
			}
			else
			{
				// outside chunk, so use the old formula, just in case
				const BoundingBox& bb = chunk->localBB();
				m.translation( (bb.maxBounds() + bb.minBounds()) / 2.f );
			}
		}
		else
		{
			m.translation( Vector3::zero() );
		}
	}
}

/**
 *	Get the current transform
 */
const Matrix & EditorChunkAmbientLight::edTransform()
{
	return transform_;
}


/**
 *	Change our transform, temporarily or permanently
 */
bool EditorChunkAmbientLight::edTransform(
	const Matrix & m, bool transient )
{
	// it's permanent, so find out where we belong now
	Chunk * pOldChunk = pChunk_;
	Chunk * pNewChunk = this->edDropChunk( m.applyToOrigin() );
	if (pNewChunk == NULL) return false;

	if (transient)
	{
		transform_ = m;
		return true;
	}

	// make sure the chunks aren't readonly
	if (!EditorChunkCache::instance( *pOldChunk ).edIsWriteable() 
		|| !EditorChunkCache::instance( *pNewChunk ).edIsWriteable())
		return false;

	// TODO: Don't accept the change if there's already an ambient light in
	// the new chunk

	// Update static lighting in the old location
	markInfluencedChunks();

	// ok, accept the transform change then
	setToCentre( transform_, pNewChunk );

	// note that both affected chunks have seen changes
	BigBang::instance().changedChunk( pOldChunk );
	BigBang::instance().changedChunk( pNewChunk );

	// and move ourselves into the right chunk. we have to do this
	// even if it's the same chunk so the col scene gets recreated
	pOldChunk->delStaticItem( this );
	pNewChunk->addStaticItem( this );

	// Update static lighting in the new location
	markInfluencedChunks();

	return true;
}


void EditorChunkAmbientLight::loadModel()
{
	model_ = Model::get( "resources/models/ambient_light.model" );
}


/**
 *	Set our colour
 */
void EditorChunkAmbientLight::colour( const Moo::Colour & c )
{
	// set our colour
	colour_ = c;

	// now fix up the light container's own ambient colour
	this->toss( pChunk_ );

	// and tell the light cache that it's stale
	ChunkLightCache::instance( *pChunk_ ).bind( false );
}

void EditorChunkAmbientLight::toss( Chunk * pChunk )
{
	if (pChunk)
		setToCentre( transform_, pChunk );

	if (pChunk_)
	{
		StaticLighting::StaticChunkLightCache & clc =
			StaticLighting::StaticChunkLightCache::instance( *pChunk_ );

		clc.lights()->ambient( D3DCOLOR( 0x00000000 ) );
	}

	this->EditorChunkSubstance<ChunkAmbientLight>::toss( pChunk );

	if (pChunk_)
	{
		StaticLighting::StaticChunkLightCache & clc =
			StaticLighting::StaticChunkLightCache::instance( *pChunk_ );

		clc.lights()->ambient( colour() * multiplier() );
	}
}

bool EditorChunkAmbientLight::load( DataSectionPtr pSection )
{
	if (!EditorChunkLight<ChunkAmbientLight>::load( pSection ))
		return false;

	multiplier( pSection->readFloat( "multiplier", 1.f ) );

	return true;
}

bool EditorChunkAmbientLight::setMultiplier( const float& m )
{
	multiplier(m);

	BigBang::instance().changedChunk( chunk() );

	markInfluencedChunks();

	// update its data section
	edSave( pOwnSect() );

	this->toss( pChunk_ );

	// and tell the light cache that it's stale
	ChunkLightCache::instance( *pChunk_ ).bind( false );

	return true;
}

// editor_chunk_light.cpp
