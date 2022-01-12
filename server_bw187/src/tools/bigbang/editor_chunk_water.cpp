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
#include "editor_chunk_water.hpp"

#include "editor_chunk_substance.ipp"
#include "editor_chunk_vlo.hpp"

#include "big_bang.hpp"
#include "appmgr/options.hpp"
#include "item_editor.hpp"
#include "item_properties.hpp"
#include "romp/super_model.hpp"
#include "chunk/chunk_model.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_space.hpp"
#include "chunks/editor_chunk.hpp"

#include "snaps.hpp"

#define MAX_INDEX_COUNT 0xffffffff

std::vector<EditorChunkWater*> EditorChunkWater::instances_;

class ChunkWaterConfig
{
public:
	ChunkWaterConfig( Water::WaterState& config ) :
		config_( config ) {}

	bool validateTessellation( float &tess )
	{
		float width = config_.size_.x;
		float height = config_.size_.y;

		int countX = int( ceilf( width / tess ) ) + 1;
		int countZ = int( ceilf( height / tess ) ) + 1;
		int count = countX * countZ;

		//TODO: this limitation is kinda wrong...... it's just checking the 
		// max number of indices possible for a certain size/tessellation ..
		// if the water intersects terrain there will be a lot less indices, 
		// but be rejected by this test...

		if ((count < MAX_INDEX_COUNT) && width >= tess && height >= tess)	
			return true;
		else
			return false;
	}

	bool validateCellSize( float &size )
	{
		//TODO: the cell size needs to be limited based on tessellation and size
		if ( size < (2.f*config_.tessellation_) )	
			size = 2.f*config_.tessellation_;
		else if ( size< (config_.size_.x/10.f))
			size = (config_.size_.x/10.f);

		return true;
	}

	bool validateSize( float sizeX, float sizeY )
	{
		int countX = int( ceilf( sizeX / config_.tessellation_ ) ) + 1;
		int countZ = int( ceilf( sizeY / config_.tessellation_ ) ) + 1;
		int count = countX * countZ;

		if (config_.simCellSize_<=0)
			config_.simCellSize_ = 100.f;

		if ((count < MAX_INDEX_COUNT) && sizeX >= config_.tessellation_ && sizeY >= config_.tessellation_)	
			return true;	
		else
			return false;
	}

protected:
	Water::WaterState&	config_;
};


/**
 *	This is a MatrixProxy for chunk item pointers.
 */
class MyChunkItemMatrix : public ChunkWaterConfig, public ChunkItemMatrix
{
public:
	MyChunkItemMatrix( ChunkItemPtr pItem, Water::WaterState&	config)
		: ChunkItemMatrix( pItem ), ChunkWaterConfig( config )  {}

	bool EDCALL setMatrix( const Matrix & m );
};

bool MyChunkItemMatrix::setMatrix( const Matrix & m )
{
	Matrix newTransform = m;

	// Snap the transform of the matrix if it's asking for a different
	// translation
	Matrix currentTransform;
	getMatrix( currentTransform, false );
	if (!almostEqual( currentTransform.applyToOrigin(), newTransform.applyToOrigin() ))
	{
		Vector3 t = newTransform.applyToOrigin();

		Vector3 snaps = movementSnaps_;
		if (snaps == Vector3( 0.f, 0.f, 0.f ) && BigBang::instance().snapsEnabled() )
			snaps = BigBang::instance().movementSnaps();

		Snap::vector3( t, snaps );

		newTransform.translation( t );
	}

	BoundingBox bbox;
	pItem_->edBounds(bbox);
	bbox.transformBy( newTransform );
	Vector3 boxVolume = bbox.maxBounds() - bbox.minBounds();	

	float width = boxVolume.x;
	float height = boxVolume.z;
	bool result = false;

	float spaceSizeX = 0.f;
	float spaceSizeZ = 0.f;

	if (pItem_->chunk())
	{
		spaceSizeX = pItem_->chunk()->space()->maxCoordX() - pItem_->chunk()->space()->minCoordX();
		spaceSizeZ = pItem_->chunk()->space()->maxCoordZ() - pItem_->chunk()->space()->minCoordZ();		
	}

	if ( (width <= spaceSizeX && height <= spaceSizeZ) && validateSize(width, height) )
	{
		// always transient
		pItem_->edTransform( newTransform, true );
		result = true;
	}
	return result;
}

// -----------------------------------------------------------------------------
// Section: EditorChunkWater
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
EditorChunkWater::EditorChunkWater( std::string uid )
{
	drawTransient_ = false;
	// Add us to the list of instances
	instances_.push_back( this );
	setUID(uid);
}


/**
 *	Destructor.
 */
EditorChunkWater::~EditorChunkWater()
{
	// Remove us from the list of instances
	std::vector<EditorChunkWater*>::iterator p = std::find(
		instances_.begin(), instances_.end(), this );

	MF_ASSERT( p != instances_.end() );

	instances_.erase( p );
}
//
//void EditorChunkSubstance<ChunkWater>::draw()
//{
//}

bool EditorChunkSubstance<ChunkWater>::load(DataSectionPtr pSect, Chunk * pChunk )
{
	pOwnSect_ = pSect;
//	edCommonLoad( pSect );
	return this->ChunkWater::load( pSect, pChunk );
}

//
//bool EditorChunkSubstance<ChunkWater>::addYBounds( BoundingBox& bb ) const
//{
//	// sorry for the conversion
////	bb.addBounds( ( ( EditorChunkSubstance<LT>* ) this)->edTransform().applyToOrigin().y );
//	return true;
//}

void EditorChunkSubstance<ChunkWater>::addAsObstacle()
{
}

void EditorChunkWater::edDelete( ChunkVLO* instigator )
{
	VeryLargeObject::edDelete( instigator );

	if (pWater_)
	{
		pWater_->deleteData();
	}
}

/**
 *	This method saves the data section pointer before calling its
 *	base class's load method
 */
bool EditorChunkWater::load( DataSectionPtr pSection, Chunk * pChunk )
{
	bool ok = this->EditorChunkSubstance<ChunkWater>::load( pSection, pChunk );
	if (ok)
	{
		localOri_ = config_.orientation_;
		worldPos_ = config_.position_;

		origin_.setIdentity();
		Matrix rtr; rtr.setRotateY( config_.orientation_ );
		origin_.postMultiply( rtr );
		origin_.translation( config_.position_ );

		bb_.setBounds( 
			config_.position_ + Vector3(	-config_.size_.x * 0.5f,
											0, 
											-config_.size_.y * 0.5f ),
			config_.position_ + Vector3(	config_.size_.x * 0.5f,
											0,
											config_.size_.y * 0.5f ) );
	}

	return ok;
}

void EditorChunkSubstance<ChunkWater>::toss(Chunk * pChunk  )
{
}

void EditorChunkWater::toss( )
{
	ChunkItemList::iterator it;
	for (it = itemList_.begin(); it != itemList_.end(); it++)
	{
		//only need to toss one for all the refs to be re-evaluated
		if ((*it))
		{
			(*it)->toss( (*it)->chunk() );
			break;
		}
	}
}


/**
 *	This is called after water has finished with the collision scene,
 *	i.e. we can now add stuff that it would otherwise collide with
 */
void EditorChunkWater::objectCreated()
{
	ChunkItemList::iterator it;
	for (it = itemList_.begin(); it != itemList_.end(); it++)
	{
		if ((*it) && (*it)->chunk())
			(*it)->objectCreated();			
	}
}


/**
 *
 */
void EditorChunkWater::saveFile( Chunk* pChunk )
{
	if (deleted())
	{
		std::string fileName( uid_ + ".vlo" );
		if (pChunk)
		{
			DataSectionPtr ds = pChunk->mapping()->pDirSection();
			ds->deleteSection( fileName );
		}
			
		if (BWResource::fileExists( fileName ))
			BigBang::instance().eraseAndRemoveFile( fileName );

		fileName = chunkPath_ + fileName;

		if (BWResource::fileExists( fileName ))
			BigBang::instance().eraseAndRemoveFile( fileName );

		dataSection_=NULL;
	}
	else if (changed_ && dataSection_)
	{
		if (pWater_)
			pWater_->saveTransparencyTable();
		dataSection_->save();
		changed_ = false;
	}
}

/**
 *
 */
void EditorChunkWater::save()
{	
	if (!dataSection_)
		return;

	DataSectionPtr waterSection = dataSection_->openSection( "water" );

	if (!waterSection)
		return;

	waterSection->writeVector3( "position", worldPos_ );
	waterSection->writeFloat( "orientation", localOri_ );
	waterSection->writeVector3( "size", Vector3( config_.size_.x, 0.f, config_.size_.y ) );

	waterSection->writeFloat( "fresnelConstant", config_.fresnelConstant_ );
	waterSection->writeFloat( "fresnelExponent", config_.fresnelExponent_ );

	waterSection->writeVector4( "reflectionTint", config_.reflectionTint_ );
	waterSection->writeFloat( "reflectionStrength", config_.reflectionScale_ );

	waterSection->writeVector4( "refractionTint", config_.refractionTint_ );
	waterSection->writeFloat( "refractionStrength", config_.refractionScale_ );

	waterSection->writeFloat( "tessellation", config_.tessellation_ );
	waterSection->writeFloat( "consistency", config_.consistency_ );

	waterSection->writeFloat( "textureTessellation", config_.textureTessellation_ );

	waterSection->writeVector2( "scrollSpeed1", config_.scrollSpeed1_ );	
	waterSection->writeVector2( "scrollSpeed2", config_.scrollSpeed2_ );
	waterSection->writeVector2( "waveScale", config_.waveScale_ );
	

	waterSection->writeFloat( "windVelocity", config_.windVelocity_ );

	waterSection->writeFloat( "sunPower", config_.sunPower_ );

	waterSection->writeString( "waveTexture", config_.waveTexture_ );

	waterSection->writeFloat( "cellsize", config_.simCellSize_ );	
	waterSection->writeFloat( "smoothness", config_.smoothness_ );

	waterSection->writeBool( "useEdgeAlpha", config_.useEdgeAlpha_ );	
	waterSection->writeBool( "useSimulation", config_.useSimulation_ );	

}


/**
 *	Save any property changes to this data section
 */
bool EditorChunkWater::edSave( DataSectionPtr pSection )
{
	return true;
}


/**
 *
 */
void EditorChunkWater::drawRed(bool val)
{
    if (pWater_)
		pWater_->drawRed(val);
}


/**
 *
 */
const Matrix & EditorChunkWater::origin()
{
	origin_.setIdentity();
	origin_.translation( worldPos_ );
	return origin_;
}


/**
 *	Get the current transform
 */
const Matrix & EditorChunkWater::edTransform()
{
	transform_.setScale( config_.size_.x, 1.f, config_.size_.y );
	Matrix rtr; rtr.setRotateY( localOri_ );
	transform_.postMultiply( rtr );
	transform_.translation( worldPos_ );
	return transform_;
}


/**
 *
 */
const Matrix & EditorChunkWater::localTransform( )
{
	transform_.setScale( config_.size_.x, 1.f, config_.size_.y );
	Matrix rtr; rtr.setRotateY( localOri_ );
	transform_.postMultiply( rtr );
	transform_.translation( worldPos_ );

	return transform_;
}


/**
 *
 */
const Matrix & EditorChunkWater::localTransform( Chunk * pChunk )
{
	edTransform();
	if (pChunk)
		transform_.postMultiply( pChunk->transformInverse() );

	return transform_;
}


/**
 *	This is a helper class for setting a float property on a chunk water item
 */
class ChunkWaterFloat : public UndoableDataProxy<FloatProxy>
{
public:
	ChunkWaterFloat( EditorChunkWaterPtr pItem, float & f,
		float min=0.f, float max=1.f, bool 
		limitMin=true, bool limitMax=true  ) :
		pItem_( pItem ),
		f_( f ),
		minValue_( min ),
		maxValue_( max ),
		limitMax_( limitMax ),
		limitMin_( limitMin )
	{ }


	virtual float EDCALL get() const			{ return f_; }
	virtual void EDCALL setTransient( float f )	{ f_ = f; }

	virtual bool EDCALL setPermanent( float f )
	{
		// complain if it's invalid
		//if (f < 0.f || f > 1.f) return false;
		//if (f < 0.f) return false;

		if ( (limitMin_ && (f < minValue_)) || (limitMax_ && (f > maxValue_)) ) return false;

		// set it
		this->setTransient( f );

		// flag the chunk as having changed
		// update its data section
		pItem_->edSave( pItem_->pOwnSect() );

		// and get it to regenerate stuff
		pItem_->toss( );
		pItem_->dirty();
		return true;
	}

	bool getRange( float& min, float& max, int& digits ) const
	{
		if ((limitMin_) && (limitMax_))
		{
			min = minValue_;
			max = maxValue_;
			digits = 1;
			return true;
		}
		return false;
	}

	virtual std::string EDCALL opName()
	{
		//return "Set " + pItem_->edDescription() + " float property";


		return "Set float property";
	}

private:
	EditorChunkWaterPtr	pItem_;
	float &				f_;

	float				minValue_,maxValue_;

	bool				limitMax_, limitMin_;
};

class ChunkWaterTessellation : public ChunkWaterConfig, public ChunkWaterFloat
{
public:
	ChunkWaterTessellation( EditorChunkWaterPtr pItem, Water::WaterState& config ) :
		ChunkWaterFloat( pItem, config.tessellation_, 1.f,1000.f,true,false),
		ChunkWaterConfig( config )
	{ }

	virtual bool EDCALL setPermanent( float f )
	{
		if ( validateTessellation( f ) )
		{
			return ChunkWaterFloat::setPermanent(f);
		}
		else
			return false;
	}
};

class ChunkWaterSimCellSize :  public ChunkWaterConfig, public ChunkWaterFloat
{
public:
	ChunkWaterSimCellSize( EditorChunkWaterPtr pItem, Water::WaterState& config ) :
		ChunkWaterFloat( pItem, config.simCellSize_, 0.f,1000.f,true,true),
		ChunkWaterConfig( config )
	{ }

	virtual bool EDCALL setPermanent( float f )
	{
		if ( validateCellSize( f ) )
			return ChunkWaterFloat::setPermanent(f);
		else
			return false;
	}
};


/**
 *	This helper class wraps up a water data property
 */
template <class DTYPE> class ChunkWaterData : public UndoableDataProxy<DTYPE>
{
public:
	explicit ChunkWaterData(EditorChunkWaterPtr pItem, typename DTYPE::Data& v ) :
		pItem_( pItem ),
		v_( v )
	{
	}

	virtual typename DTYPE::Data EDCALL get() const
	{
		return v_;
	}

	virtual void EDCALL setTransient( typename DTYPE::Data v )
	{
		v_ = v;
	}

	virtual bool EDCALL setPermanent( typename DTYPE::Data v )
	{
		setValue( v );
		return true;
	}

	virtual std::string EDCALL opName()
	{
		//return "Set " + pItem_->edDescription() + " colour";
		return "Set data";
	}

protected:	
	void setValue( typename DTYPE::Data v )
	{
		// set it
		this->setTransient( v );

		// update its data section
		pItem_->edSave( pItem_->pOwnSect() );

		// and get it to regenerate stuff
		pItem_->toss( );
		pItem_->dirty();
	}
private:
	EditorChunkWaterPtr	   pItem_;
	typename DTYPE::Data & v_;
};


bool EDCALL ChunkWaterData<Vector4Proxy>::setPermanent( Vector4 v )
{
	// make it valid
	if (v.x < 0.f) v.x = 0.f;
	if (v.x > 1.f) v.x = 1.f;
	if (v.y < 0.f) v.y = 0.f;
	if (v.y > 1.f) v.y = 1.f;
	if (v.z < 0.f) v.z = 0.f;
	if (v.z > 1.f) v.z = 1.f;
	v.w = 1.f;

	setValue( v );
	return true;
}


//need a new matrix proxy / float proxy etc etc..... and use them to update the right variables...
//perhaps i can get the scaling to work from the reference transformations ALA the translation? 

/**
 *	Add the properties to the given editor
 */
bool EditorChunkWater::edEdit( class ChunkItemEditor & editor, const ChunkItemPtr pItem )
{
	MatrixProxy * pMP = new MyChunkItemMatrix( pItem, config_ );
	editor.addProperty( new GenPositionProperty( "position", pMP ) );
	editor.addProperty( new GenRotationProperty( "orientation", pMP ) );
	editor.addProperty( new GenScaleProperty( "size", pMP ) );

	editor.addProperty( new GenFloatProperty( "fresnel constant",
		new ChunkWaterFloat( this, config_.fresnelConstant_ ) ) );
	editor.addProperty( new GenFloatProperty( "fresnel exponent",
		new ChunkWaterFloat( this, config_.fresnelExponent_, 0.f, 7.f ) ) );

	editor.addProperty( new ColourProperty( "reflection tint",
		new ChunkWaterData< Vector4Proxy >( this, config_.reflectionTint_ ) ) );

	editor.addProperty( new GenFloatProperty( "reflection strength",
		new ChunkWaterFloat( this, config_.reflectionScale_ ) ) );

	editor.addProperty( new ColourProperty( "refraction tint",		
		new ChunkWaterData< Vector4Proxy >( this, config_.refractionTint_ ) ) );

	editor.addProperty( new GenFloatProperty( "refraction Strength",
		new ChunkWaterFloat( this, config_.refractionScale_ ) ) );

	editor.addProperty( new GenFloatProperty( "mesh Size",
		new ChunkWaterTessellation( this, config_ ) ) );

	editor.addProperty( new GenFloatProperty( "consistency",
		new ChunkWaterFloat( this, config_.consistency_, 0.f, 0.98f, true, true ) ) );

	editor.addProperty( new GenFloatProperty( "texture Size",
		new ChunkWaterFloat( this, config_.textureTessellation_,0.f,1000.f,true,false ) ) );

	editor.addProperty( new Vector2Property( "scroll speed 1",
		new ChunkWaterData< Vector2Proxy >( this, config_.scrollSpeed1_ ) ) );

	editor.addProperty( new Vector2Property( "scroll speed 2",
		new ChunkWaterData< Vector2Proxy >( this, config_.scrollSpeed2_ ) ) );

	editor.addProperty( new Vector2Property( "wave scale",
		new ChunkWaterData< Vector2Proxy >( this, config_.waveScale_ ) ) );

	editor.addProperty( new GenFloatProperty( "wind speed",
		new ChunkWaterData<FloatProxy>( this, config_.windVelocity_ ) ) );

	editor.addProperty( new GenFloatProperty( "surface smoothness",
		new ChunkWaterFloat( this, config_.smoothness_ ) ) );

	editor.addProperty( new GenFloatProperty( "sun Power",
		new ChunkWaterFloat( this, config_.sunPower_, 1.f, 128.f ) ) );

	TextProperty* pProp = new TextProperty( "wave texture",
							new ChunkWaterData< StringProxy >(this, config_.waveTexture_ ) );
	pProp->fileFilter( "Texture files(*.jpg;*.tga;*.bmp;*.dds)|*.jpg;*.tga;*.bmp;*.dds||" );
	editor.addProperty(pProp);

	editor.addProperty( new GenFloatProperty( "sim cell size",
		new ChunkWaterSimCellSize( this, config_ ) ) );

	editor.addProperty( new GenBoolProperty( "use edge alpha",
		new ChunkWaterData< BoolProxy >( this, config_.useEdgeAlpha_ ) ) );

	editor.addProperty( new GenBoolProperty( "use simulation",
		new ChunkWaterData< BoolProxy >( this, config_.useSimulation_ ) ) );

	return true;
}


/**
 *	This method updates our local vars from the transform
 */
void EditorChunkWater::updateLocalVars( const Matrix & m )
{
	const Vector3 & woriVec = m.applyToUnitAxisVector(2);
	localOri_ = atan2f( woriVec.x, woriVec.z );
	Matrix unrot; unrot.setRotateY( -localOri_ );
	unrot.preMultiply( m );

	config_.size_.x = unrot.applyToUnitAxisVector(0).length();
	config_.size_.y = unrot.applyToUnitAxisVector(2).length();

	transform_ = m;
	worldPos_ = transform_.applyToOrigin();
}


/**
 *
 */
void EditorChunkWater::dirty()
{
	ChunkWater::dirty();
	changed_ = true;
	updateWorldVars(Matrix::identity);
}


/**
 *	This method updates our caches of world space variables
 */
void EditorChunkWater::updateWorldVars( const Matrix & m )
{
	config_.position_ = ( worldPos_ );	
	bb_.setBounds( 
		worldPos_ + Vector3( -config_.size_.x*0.5f, 0, -config_.size_.y*0.5f),
		worldPos_ + Vector3(  config_.size_.x*0.5f, 0,  config_.size_.y*0.5f));
	config_.orientation_ = localOri_;
}


/**
 *
 */
void EditorChunkWater::draw( ChunkSpace* pSpace )
{
	if( !BigBang::instance().drawSelection() )
		ChunkWater::draw( pSpace );
}


/**
 *	Return a modelptr that is the representation of this chunk item
 */
ModelPtr EditorChunkWater::reprModel() const
{
	static ModelPtr waterModel =
		Model::get( "resources/models/water.model" );
	return waterModel;
}

VLOFactory EditorChunkWater::factory_( "water", 1, EditorChunkWater::create );


/**
 *
 */
bool EditorChunkWater::create( Chunk * pChunk, DataSectionPtr pSection, std::string uid )
{
	//TODO: check it isnt already created?
	EditorChunkWater * pItem = new EditorChunkWater( uid );	

	if (pItem->load( pSection, pChunk ))
		return true;

	//the VLO destructor should clear out the static pointer map
	delete pItem;
	return false;
}


ChunkItemFactory::Result EditorChunkWater::oldCreate( Chunk * pChunk, DataSectionPtr pSection )
{
	bool converted = pSection->readBool("deprecated",false);
	if (converted)
		return ChunkItemFactory::Result( NULL, "Failed to create legacy water" );

	pSection->writeBool("deprecated", true);
	BigBang::instance().changedChunk( pChunk );

	//TODO: generalise the want flags...
	EditorChunkVLO * pVLO = new EditorChunkVLO( );	
	if (!pVLO->legacyLoad( pSection, pChunk, std::string("water") ))	
	{
		delete pVLO;
		return ChunkItemFactory::Result( NULL, "Failed to create legacy water" );
	}
	else
	{
		pChunk->addStaticItem( pVLO );
		pVLO->edTransform( pVLO->edTransform(), false );
		return ChunkItemFactory::Result( pVLO );
	}
}

/// Static factory initialiser
ChunkItemFactory EditorChunkWater::oldWaterFactory_( "water", 0, EditorChunkWater::oldCreate );

// editor_chunk_water.cpp
