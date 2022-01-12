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

#include "chunk_model.hpp"
#include "chunk_model_obstacle.hpp"
#include "chunk_manager.hpp"

#include "math/blend_transform.hpp"
#include "moo/render_context.hpp"
#include "moo/visual_compound.hpp"

#include "romp/static_light_fashion.hpp"
#include "romp/model_compound.hpp"

#include "resmgr/bwresource.hpp"

#include "physics2/bsp.hpp"

#if UMBRA_ENABLE
#include <umbraModel.hpp>
#include <umbraObject.hpp>
#include "chunk_umbra.hpp"
#endif

int ChunkModel_token;

DECLARE_DEBUG_COMPONENT2( "Chunk", 0 )

// ----------------------------------------------------------------------------
// Section: ChunkMaterial
// ----------------------------------------------------------------------------
ChunkMaterial::~ChunkMaterial()
{
	delete material_;
}

void ChunkMaterial::dress( SuperModel & superModel )
{
	if( superModel.curModel( 0 ) )
		override_ = superModel.curModel( 0 )->overrideMaterial( material_->identifier(), material_ );
	else
		override_.savedMaterials_.clear();
}

void ChunkMaterial::undress( SuperModel & superModel )
{
	override_.reverse();
}

// ----------------------------------------------------------------------------
// Section: ChunkModel
// ----------------------------------------------------------------------------

#undef IMPLEMENT_CHUNK_ITEM_ARGS
#define IMPLEMENT_CHUNK_ITEM_ARGS (pSection, pChunk)
IMPLEMENT_CHUNK_ITEM( ChunkModel, model, 0 )
IMPLEMENT_CHUNK_ITEM_ALIAS( ChunkModel, shell, 0 )

ChunkModel::ChunkModel() :
	ChunkItem( 3 ),
	pSuperModel_( NULL ),
	pAnimation_( NULL ),
	animRateMultiplier_( 1.f ),
	transform_( Matrix::identity ),
	reflectionVisible_( false )
#if UMBRA_ENABLE	
	,umbraOccluder_( false )
#endif
{
}


ChunkModel::~ChunkModel()
{
	fv_.clear();
	pAnimation_ = NULL;

	if (pSuperModel_ != NULL)
	{
		delete pSuperModel_;
	}
}

#include "cstdmf/diary.hpp"

// in chunk.cpp
extern void readMooMatrix( DataSectionPtr pSection, const std::string & tag,
	Matrix &result );

/**
 *	Load yourself from this section
 */
bool ChunkModel::load( DataSectionPtr pSection, Chunk * pChunk )
{
	pAnimation_ = NULL;
	tintMap_.clear();
	materialOverride_.clear();
	fv_.clear();
	label_.clear();

	bool good = false;

	label_ = pSection->asString();

	std::vector<std::string> models;
	pSection->readStrings( "resource", models );

	//uint64 ltime = timestamp();
	std::string mname;
	if (!models.empty()) mname = models[0].substr( models[0].find_last_of( '/' )+1 );
	DiaryEntry & de = Diary::instance().add( mname );
	pSuperModel_ = new SuperModel( models );
	de.stop();
	//ltime = timestamp() - ltime;
	//dprintf( "\t%s took %f ms\n", models[0].c_str(),
	//	float(double(int64(ltime)) / stampsPerSecondD()) * 1000.f );

	if (pSuperModel_->nModels() > 0)
	{
		good = true;

		// load the specified animation
		DataSectionPtr pAnimSec = pSection->openSection( "animation" );
		if (pAnimSec)
		{
			pAnimation_ = pSuperModel_->getAnimation(
				pAnimSec->readString( "name" ) );
			pAnimation_->time = 0.f;
			pAnimation_->blendRatio = 1.0f;
			animRateMultiplier_ = pAnimSec->readFloat( "frameRateMultiplier", 1.f );

			if (pAnimation_->pSource( *pSuperModel_ ) == NULL)
			{
				ERROR_MSG( "SuperModel can't find its animation %s\n",
					pAnimSec->readString( "name" ).c_str() );
				pAnimation_ = NULL;
			}
			else
			{
				fv_.push_back( pAnimation_ );
			}
		}

		// load the specified dye/tint
		tintMap_.clear();

		std::vector<DataSectionPtr> dyeSecs;
		pSection->openSections( "dye", dyeSecs );

		for( std::vector<DataSectionPtr>::iterator iter = dyeSecs.begin(); iter != dyeSecs.end(); ++iter )
		{
			std::string dye = ( *iter )->readString( "name" );
			std::string tint = ( *iter )->readString( "tint" );
			SuperModelDyePtr dyePtr = pSuperModel_->getDye( dye, tint );

			if( dyePtr )
			{
				fv_.push_back( dyePtr );
				tintMap_[ dye ] = dyePtr;
			}
		}

		// load material overrides
		materialOverride_.clear();

		std::vector<DataSectionPtr> materialSecs;
		pSection->openSections( "material", materialSecs );

		for( std::vector<DataSectionPtr>::iterator iter = materialSecs.begin(); iter != materialSecs.end(); ++iter )
		{
			std::string identifier = (*iter)->readString( "identifier" );
			std::vector< Moo::EffectMaterial** > uses;
			if( pSuperModel_->topModel( 0 )->gatherMaterials( identifier, uses ) != 0 )
			{
				Moo::EffectMaterial* material = new Moo::EffectMaterial( **uses[0] );

				if( material->load( *iter, false ) )
				{
					materialOverride_.push_back( new ChunkMaterial( material ) );
				}
			}
		}

		// use any legacy dyes
		int i = 0;
		while (1)
		{
			SuperModelDyePtr pDye;
			char legName[128];
			sprintf( legName, "Legacy-%d", i );

			pDye = pSuperModel_->getDye( legName, "MFO" );
			if (!pDye) break;

			fv_.push_back( pDye );
			i++;
		}

		// use any specified dyes
		std::vector<DataSectionPtr>	dyes;
		pSection->openSections( "dye", dyes );
		for (uint i = 0; i < dyes.size(); i++)
		{
			DataSectionPtr pDyeSect = dyes[i];
			SuperModelDyePtr pDye = pSuperModel_->getDye(
				pDyeSect->readString( "matter" ),
				pDyeSect->readString( "tint" ) );
			if (!pDye) break;

			fv_.push_back( pDye );
		}

		// use static lighting if specified
		DataSectionPtr pSLSec = pSection->openSection( "lighting" );
		if (pSLSec)
		{
			DiaryEntry & de2 = Diary::instance().add( "lightn" );

			// check if using .cData or .lighting files
			std::string lightingTag = pSLSec->asString();
			if (lightingTag.substr(0, 8) == "lighting")
			{
				// .cdata
				if( pChunk )
					addStaticLighting( pChunk->binFileName() + "/" + pSLSec->asString() );
			}
			else
			{
				// .lighting file (only first model in the supermodel supported)
				addStaticLighting( pSLSec->asString() + ".lighting" );
			}
			de2.stop();
		}

		// load our transform
		readMooMatrix( pSection, "transform", transform_ );

		// This model might be suitable for the visual compound.
		if (models.size() == 1 &&
			!fv_.size())
		{
#ifndef EDITOR_ENABLED
		
			if (models[0].length()&&materialOverride_.size()==0&&pChunk)
			{
				// Create the model compound
				Matrix m = pChunk->transform();
				m.preMultiply( transform_ );
				pModelCompound_ = ModelCompound::get( models[0], m,
				uint32(pChunk) );
			}
#endif
		}

#if UMBRA_ENABLE
		// Only create occlusion models if we are running in software mode
		if (ChunkUmbra::softwareMode())
		{
			DataSectionPtr pModelSection = BWResource::openSection(models[0]);
			if (pModelSection)
			{
				umbraOccluder_ = pModelSection->readBool( "dpvsOccluder", false );
				umbraOccluder_ = pModelSection->readBool( "umbraOccluder", umbraOccluder_ );
				umbraModelName_ = models[0];
			}
		}
#endif

		if (isShellModel( pSection ))
			reflectionVisible_ = true;
		else
			reflectionVisible_ = pSection->readBool( "reflectionVisible", reflectionVisible_ );
		//reflectionVisible_ |= this->resourceIsOutsideOnly();
	}
	else
	{
		WARNING_MSG( "No models loaded into SuperModel\n" );

		delete pSuperModel_;
		pSuperModel_ = NULL;
	}

	return good;
}

void ChunkModel::addStaticLighting( const std::string& resName, DataSectionPtr modelLightingSection )
{
	if ( !modelLightingSection )
		modelLightingSection = BWResource::openSection( resName, false );

	if ( !modelLightingSection )
	{
		ERROR_MSG( "ChunkModel::addStaticLighting lighting file %s not found\n", resName.c_str() );
		return;
	}

	StaticLightFashionPtr pSLF = StaticLightFashion::get(
		*pSuperModel_, modelLightingSection );

	if (pSLF)
		fv_.push_back( pSLF );
}


/**
 *	overridden draw method
 */
void ChunkModel::draw()
{
	static DogWatch drawWatch( "ChunkModel" );
	ScopedDogWatch watcher( drawWatch );

	static bool firstTime = true;
	static bool useCompound = true;
	if (firstTime)
	{
		MF_WATCH( "Chunks/Use Compound", useCompound, Watcher::WT_READ_WRITE,
			"When enabled, ChunkModel will use the VisualCompound "
			"to render suitable models" );
		firstTime = false;
	}

	if (pSuperModel_ != NULL && 
		(!pModelCompound_.hasObject() || !useCompound || Moo::VisualCompound::disable() || Moo::rc().reflectionScene()) )
	{
		//TODO: optimize
		//TODO: sort out the overriding of reflection (should be able to set the reflection
		// per model but be able to override the setting in the editor.
		//TODO: distance check from water position?
		if (Moo::rc().reflectionScene() && !reflectionVisible_ /*&& !Waters::shouldReflect(this)*/ )
			return;

		Moo::rc().push();
		Moo::rc().preMultiply( transform_ );

		std::vector<ChunkMaterialPtr>::size_type size = materialOverride_.size();

		fv_.insert( fv_.end(), materialOverride_.begin(), materialOverride_.end() );

		pSuperModel_->draw( &fv_, materialOverride_.size() );

		fv_.resize( fv_.size() - size );

		Moo::rc().pop();
	}
	else if (pModelCompound_.hasObject() && useCompound)
	{
		if (!pModelCompound_->draw())
		{
			pModelCompound_ = NULL;
		}
	}
}

/**
 *	overridden tick method
 */
void ChunkModel::tick( float dTime )
{
	if (pAnimation_)
	{
		pAnimation_->time += dTime * animRateMultiplier_;
		float duration = pAnimation_->pSource( *pSuperModel_ )->duration_;
		pAnimation_->time -= __int64( pAnimation_->time / duration ) * duration;
	}
}

void ChunkModel::syncInit()
{
#if UMBRA_ENABLE
	// Grab the visibility bounding box
	BoundingBox bb = BoundingBox::s_insideOut_;
	pSuperModel_->visibilityBox(bb);

	pUmbraModel_ = NULL;

	// If this object is a umbra occluder, create a umbra object from its geometry
	Moo::VisualPtr pVisual = pSuperModel_->topModel(0)->getVisual();
	if (pVisual	&& umbraOccluder_)
	{
		// If we have occluder already, create a copy of the object, but retain the models,
		// so that we don't waste memory on multiple instances of the umbra geometry
		pUmbraObject_ = UmbraObjectProxy::getCopy( umbraModelName_ );
		if (!pUmbraObject_.exists())
		{
			// We assume that any occluder object is a static model with only one renderset.
			Moo::Visual::RenderSetVector& renderSets = pVisual->renderSets();
			if (renderSets.size() && renderSets[0].geometry_.size())
			{
				Moo::Visual::Geometry& geometry = renderSets[0].geometry_[0];
				MF_ASSERT( geometry.vertices_ && geometry.primitives_ );

				// Iterate over the primitive groups and collect the triangles
				std::vector<uint32> indices;
				for (uint32 i = 0; i < geometry.primitiveGroups_.size(); i++)
				{
					Moo::Visual::PrimitiveGroup& pg = geometry.primitiveGroups_[i];
					
					// If the material fails or it has a channel (i.e. sorted) do
					// not create occlusion geometry.
					if (pg.material_->channel() == NULL && pg.material_->begin())
					{
						pg.material_->end();

						BOOL alphaTest = FALSE;
						if (FAILED(pg.material_->pEffect()->pEffect()->GetBool( "alphaTestEnable", &alphaTest )))
							alphaTest = FALSE;

						// Only create occlusion geometry for solid objects
						if (!alphaTest)
						{
							// Copy triangle indices
							const Moo::Primitive::PrimGroup& primGroup = geometry.primitives_->primitiveGroup(pg.groupIndex_);
							if (geometry.primitives_->indices().format() == D3DFMT_INDEX16)
							{
								const uint16* pInd = (const uint16*)geometry.primitives_->indices().indices();
								indices.insert( indices.end(), pInd + primGroup.startIndex_, 
									pInd + primGroup.startIndex_ + primGroup.nPrimitives_ * 3);
							}
							else
							{
								const uint32* pInd = (const uint32*)geometry.primitives_->indices().indices();
								indices.insert( indices.end(), pInd + primGroup.startIndex_, 
									pInd + primGroup.startIndex_ + primGroup.nPrimitives_ * 3);
							}
						}
					}
				}
				
				if (indices.size())
				{
					// Create occlusion model using our indices
					pUmbraModel_ = UmbraModelProxy::getMeshModel( &geometry.vertices_->vertexPositions().front(),
						&indices.front(), geometry.vertices_->vertexPositions().size(), indices.size() / 3 );

					// If the occlusion geometry and the model geometry is the same use the occlusion model
					// as both test and write model, otherwise use the bounding box as test model.
					if (indices.size() == geometry.primitives_->indices().size())
					{
						pUmbraObject_ = UmbraObjectProxy::get( pUmbraModel_, pUmbraModel_, umbraModelName_ );
					}
					else
					{
						UmbraModelProxyPtr pBBModel = UmbraModelProxy::getObbModel( &geometry.vertices_->vertexPositions().front(),
							geometry.vertices_->vertexPositions().size() );
						pUmbraObject_ = UmbraObjectProxy::get( pBBModel, pUmbraModel_, umbraModelName_ );
					}
				}
			}
		}
	}
	
	// If the umbra object has not been created create a umbra model based on the bounding box of the
	// model, and do not use as occluder.
	if (!pUmbraObject_.hasObject())
	{
		pUmbraModel_ = UmbraModelProxy::getObbModel( bb.minBounds(), bb.maxBounds() );
		pUmbraObject_ = UmbraObjectProxy::get( pUmbraModel_ );
	}
	
	// Set the user pointer up to point at this chunk item
	pUmbraObject_->object()->setUserPointer( (void*)this );

	// Set up object transforms
	Matrix m = pChunk_->transform();
	m.preMultiply( transform_ );
	pUmbraObject_->object()->setObjectToCellMatrix( (Umbra::Matrix4x4&)m );
	pUmbraObject_->object()->setCell( pChunk_->getUmbraCell() );
#endif
}

/**
 *	overridden lend method
 */
void ChunkModel::lend( Chunk * pLender )
{
	if (pSuperModel_ != NULL && pChunk_ != NULL)
	{
		Matrix world( pChunk_->transform() );
		world.preMultiply( this->transform_ );

		BoundingBox bb;
		pSuperModel_->visibilityBox( bb );
		bb.transformBy( world );

		this->lendByBoundingBox( pLender, bb );
	}
}


/**
 *	label accessor
 */
const char * ChunkModel::label() const
{
	return label_.c_str();
}


/**
 *	Add this model to (or remove it from) this chunk
 */
void ChunkModel::toss( Chunk * pChunk )
{
	// remove it from old chunk
	if (pChunk_ != NULL)
	{
		ChunkModelObstacle::instance( *pChunk_ ).delObstacles( this );
	}

	// call base class method
	this->ChunkItem::toss( pChunk );

	// add it to new chunk
	if (pChunk_ != NULL)
	{
		Matrix world( pChunk_->transform() );
		world.preMultiply( this->transform_ );

		for (int i = 0; i < this->pSuperModel_->nModels(); i++)
		{
			ChunkModelObstacle::instance( *pChunk_ ).addModel(
				this->pSuperModel_->topModel( i ), world, this );
		}
	}
}


bool ChunkModel::addYBounds( BoundingBox& bb ) const
{
	if (pSuperModel_)
	{
		BoundingBox me;
		pSuperModel_->visibilityBox( me );
		me.transformBy( transform_ );
		bb.addYBounds( me.minBounds().y );
		bb.addYBounds( me.maxBounds().y );
	}
	return true;
}


/**
 * Are we the interior mesh for the chunk?
 *
 * We check by seeing if the model is in the shells directory
 */
bool ChunkModel::isShellModel( const DataSectionPtr pSection ) const
{
	if (chunk() && chunk()->isOutsideChunk())
		return false;

	if( !pSuperModel_ || !pSuperModel_->topModel( 0 ) )
		return false;

	std::string itemRes = pSuperModel_->topModel( 0 )->resourceID();

	return itemRes.substr( 0, 7 ) == "shells/" ||
		itemRes.find( "/shells/" ) != std::string::npos;
}

// chunk_model.cpp
