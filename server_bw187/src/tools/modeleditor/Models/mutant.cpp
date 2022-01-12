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

#include "romp/super_model.hpp"

#include "resmgr/xml_section.hpp"
#include "resmgr/sanitise_helper.hpp"
#include "undo_redo.hpp"

#include "romp/geometrics.hpp"

#include "moo/vertices_manager.hpp"
#include "moo/managed_texture.hpp"
#include "moo/visual_channels.hpp"

#include "physics2/bsp.hpp"

#include "utilities.hpp"

#include "page_lod_consts.hpp"

DECLARE_DEBUG_COMPONENT( 0 )
#include "me_error_macros.hpp"

#include "mutant.hpp"

Mutant::Mutant( bool groundModel, bool centreModel ):
	modelName_ (""),
	visualName_ (""),
	primitivesName_ (""),
	superModel_ ( NULL ),
	playing_ (true),
	looping_ (true),
	virtualDist_ (-1.f),
	groundModel_(groundModel),
	centreModel_(centreModel),
	numNodes_(0),
	verts_(NULL),
	nVerts_(0),
	foundRootNode_( false ),
	hasRootNode_( false ),
	initialRootNode_( 0.f, 0.f, 0.f ),
	animMode_( true ),
	clearAnim_( false ),
	visibilityBoxDirty_( false ),
	texMemDirty_( true ),
	texMem_( 0 )
{
	extraModels_ = BWResource::openSection( "extraModels.xml", true );
}

Mutant::~Mutant()
{
	delete superModel_;
}

void Mutant::registerModelChangeCallback( SmartPointer <ModelChangeCallback> mcc )
{
	modelChangeCallbacks_.push_back( mcc );
}

void Mutant::unregisterModelChangeCallback( void* parent )
{
	ModelChangeCallbackVect::iterator it = modelChangeCallbacks_.begin();
	while (it != modelChangeCallbacks_.end())
	{
		if (parent == (*it)->parent())
		{
			it = modelChangeCallbacks_.erase( it );
		}
		else
		{
			++it;
		}
	}
}

bool Mutant::executeModelChangeCallbacks()
{
	bool ok = true;
	for (unsigned i=0; i<modelChangeCallbacks_.size(); i++)
	{
		ok = modelChangeCallbacks_[i]->execute() && ok;
	}
	return ok;
}

void Mutant::groundModel( bool lock )
{
	groundModel_ = lock;
}

void Mutant::centreModel( bool lock )
{
	centreModel_ = lock;
}

bool Mutant::hasVisibilityBox( std::string modelPath /* = "" */ )
{
	if (modelPath == "")
		modelPath = modelName_;

	if (modelPath == "") return 0;

	DataSectionPtr model = BWResource::openSection( modelPath, false );

	return ((model) && (model->openSection("visibilityBox/min")) &&
		(model->openSection("visibilityBox/max")));
}

int Mutant::animCount( std::string modelPath /* = "" */ )
{
	if (modelPath == "")
		modelPath = modelName_;

	if (modelPath == "") return 0;

	int animCount = 0;
	do
	{
		DataSectionPtr model = BWResource::openSection( modelPath, false );

		if (!model) break;
		
		std::vector< DataSectionPtr > pAnimations;
		model->openSections( "animation", pAnimations );
		animCount += pAnimations.size();
		modelPath = model->readString("parent","") + ".model";
	}
	while (modelPath != ".model");
	return animCount;
}

bool Mutant::nodefull( std::string modelPath /* = "" */ )
{
	if (modelPath == "")
		modelPath = modelName_;

	if (modelPath == "") return false;

	DataSectionPtr model = BWResource::openSection( modelPath, false );

	std::string visualPath = model->readString( "nodefullVisual", "" );

	return (visualPath != "");
}


/** 
 *  This method checks to see if the model can be batch rendered. 
 *  Models with tints or with alpha blending cannot be batched. 
 * 
 *  @return True if the model can be batched, false otherwise. 
 */ 
bool Mutant::canBatch() const
{ 
	// If the model is tinted then we cannot batch
	if (tints_.size() > 0)
	{
		return false;
	}

	// If the model is alpha blended then we cannot batch
	std::map< std::string, MaterialInfo >::const_iterator matIt = materials_.begin();
	std::map< std::string, MaterialInfo >::const_iterator matEnd = materials_.end();
	for (; matIt != matEnd; ++matIt)
	{
		EffectMaterialSet::const_iterator sectIt = matIt->second.effect.begin();
		EffectMaterialSet::const_iterator sectEnd = matIt->second.effect.end();
		for (; sectIt != sectEnd; ++sectIt)
		{
			Moo::EffectMaterial* effect = *sectIt;

			if ((effect != NULL) && (effect->channel() != NULL))
			{
				return false;
			}
		}
	}

	return true;
}


/** 
 *  This method sets whether the model can be batched. 
 * 
 *  @param val A value specifying whether to batch. 
 */ 
void Mutant::batched( bool val, bool undo /*=true*/ )
{
	if (!currModel_) 
	{
		return;
	}

	DataSectionPtr data = currModel_->openSection( "batched" );
	if (data == NULL) 
	{
		data = currModel_;
	}

	if (undo)
	{
		UndoRedo::instance().add( new UndoRedoOp( 0, data ));
		UndoRedo::instance().barrier( "Changing the \"Allow Model Batching\" flag", false );
	}

	currModel_->writeBool( "batched", val );
	dirty( currModel_ );
}


/** 
 *  This method returns the batch parameter for the model. 
 * 
 *  @return True if the model is batched, false otherwise. 
 */ 
bool Mutant::batched() const
{
	if (!currModel_) 
	{
		return false;
	}

	return currModel_->readBool( "batched", false );
}

void Mutant::dpvsOccluder( bool val )
{
	if (!currModel_) return;
	DataSectionPtr data = currModel_->openSection( "dpvsOccluder" );
	if (data == NULL) data = currModel_;
	UndoRedo::instance().add( new UndoRedoOp( 0, data ));
	UndoRedo::instance().barrier( "Changing the \"Use As Occluder\" flag", false );
	currModel_->writeBool( "dpvsOccluder", val );
	dirty( currModel_ );
}

bool Mutant::dpvsOccluder()
{
	if (!currModel_) return false;
	return currModel_->readBool( "dpvsOccluder", false );
}

/*
 * This reverts the model to it's on disk state.
 */
void Mutant::revertModel()
{
	loadModel( modelName_ );
}

/*
 * This reloads the entire model including the trees, animations and actions.
 */
void Mutant::reloadModel()
{
	//Clear the fashion to ensure the default stance is used
	fashions_.clear();
	drawModel( false, -1, false );

	//Reload the model
	loadModel( modelName_, true );
}

bool Mutant::loadModel( const std::string& name, bool reload )
{
	if (name == "")
	{
		return false;
	}
	
	executeModelChangeCallbacks();

	//Backup the added models list in case the load fails
	static bool s_last_failed = false;
	static std::set< std::string > s_extra_models;
	std::vector< DataSectionPtr > extras;
	extraModels_->openSections( "model", extras );
	for (unsigned i=0; i<extras.size(); i++)
	{
		s_extra_models.insert( extras[i]->asString() );
	}
	
	if (!reload)
	{
		//Clear the list of missing files
		Mutant::clearFilesMissingList();
		
		//Check the model's validity before doing anything else...
		if (!ensureModelValid( name, "load" )) return false;
		
		//Purge all datasection caches to ensure the new model is loaded
		BWResource::instance().purgeAll();
		
		//Clear all reported Model::get messages
		Model::clearReportedErrors();
		
		//We need to delete the old supermodel to ensure that the
		//version on disk is loaded
		delete superModel_;
		superModel_ = NULL;
		
		extraModels_->delChildren();
		
		UndoRedo::instance().clear();
		UndoRedo::instance().setSavePoint();
	}
	
	if (reload)
	{
		//Reload all the models in the lod tree
		std::map < std::string , DataSectionPtr >::iterator it = models_.begin();
		std::map < std::string , DataSectionPtr >::iterator end = models_.end();
		
		for (;it!=end;++it)
		{
			ModelPtr original = Model::get( it->first );
			if ( original )
			{
				original->reload( it->second , false );
			}
		}
	}
	
	std::vector< std::string > models;
	
	models.push_back( name );
	
	//If the previous load failed re-add the old added models.
	if (s_last_failed)
	{
		std::set< std::string >::iterator it = s_extra_models.begin();
		std::set< std::string >::iterator end = s_extra_models.end();
		for(;it != end;++it)
		{
			addModel( *it, false );
		}
	}
	
	if (extraModels_->countChildren())
	{
		bool alreadyIn;
		extraModels_->openSections( "model", extras );
		for (unsigned i=0; i<extras.size(); i++)
		{
			alreadyIn = false;
			for (unsigned j=0; j<models.size(); j++)
			{
				if (extras[i]->asString() == models[j])
				{
					alreadyIn = true;
					break;
				}
			}
			if (!alreadyIn)
			{
				models.push_back( extras[i]->asString() );
			}
		}
	}
	
	//Moo::ManagedTexture::accErrs( true );
	
	SuperModel* newSuperModel = new SuperModel( models );
	if ((newSuperModel == NULL) || (!newSuperModel->isOK()))
	{
		//Moo::ManagedTexture::accErrs( false );
	
		ERROR_MSG( "An error occured while trying to load \"%s\".\n"
			"This model may need to be re-exported.\n", name.c_str() );
		delete newSuperModel;
		s_last_failed = true;
		loadModel( modelName_ );
		return false;
	}
	
	s_last_failed = false;
	
	//Clear the added models list now that we have loaded successfully
	s_extra_models.clear();
	
	delete superModel_; // OK, lets nuke the old supermodel now...
	superModel_ = newSuperModel;
	
	if (!reload)
	{
		//Now get the file datasections for the model
		ensureModelValid( name, "load", &currModel_, &currVisual_, &visualName_, &primitivesName_ );
		
		modelHist_.erase( modelHist_.begin(), modelHist_.end() ); // Clear the model history
		
		delete [] verts_;
		verts_ = NULL;
		nVerts_ = 0;
	
		animations_.clear();
		currDyes_.clear();
		currAnims_.clear();

		foundRootNode_ = false;
		hasRootNode_ = false;
		initialRootNode_ = Vector3( 0.f, 0.f, 0.f );

		modelBB_ =  BoundingBox::s_insideOut_;
		visibilityBB_ = BoundingBox::s_insideOut_;
		frameBB_ = BoundingBox::s_insideOut_;

		visibilityBoxDirty_ = false;
	}

	//Let's get the model's bounding box
	Vector3 minBB = currVisual_->readVector3( "boundingBox/min", Vector3( 0.f, 0.f, 0.f ) );
	Vector3 maxBB = currVisual_->readVector3( "boundingBox/max", Vector3( 0.f, 0.f, 0.f ) );
	modelBB_ = BoundingBox( minBB, maxBB );

	//Now let's get the visibility box
	if (currModel_->openSection("visibilityBox/min") &&
		currModel_->openSection("visibilityBox/max"))
	{
		minBB = currModel_->readVector3( "visibilityBox/min", Vector3( 0.f, 0.f, 0.f ) );
		maxBB = currModel_->readVector3( "visibilityBox/max", Vector3( 0.f, 0.f, 0.f ) );
		visibilityBB_ = BoundingBox( minBB, maxBB );
	}
	else
	{
		visibilityBB_ = modelBB_;
		visibilityBoxDirty_ = true; // The visibility box will require a recalculation
	}

	frameBB_ = modelBB_; // Make sure it is valid initialy.

	//We always want the model draw even if it is not visible
	superModel_->checkBB( false ); 

	modelName_ = name;

	regenAnimLists();
	regenMaterialsList();
	recalcTextureMemUsage();

	if (!reload)
	{
		reloadBSP();
		
		triggerUpdate("Anim");
		triggerUpdate("Act");
		triggerUpdate("LOD");
		triggerUpdate("Object");
		triggerUpdate("Materials");
	}

	this->postLoad();

	/*
	std::string errStr = Moo::ManagedTexture::accErrStr();
	if (errStr != "")
	{
		ERROR_MSG( "Moo:ManagedTexture::load, unable to load the following textures:\n"
				"%s\n\nPlease ensure these textures exist.\n", errStr.c_str() );
	}

	Moo::ManagedTexture::accErrs( false );
	*/

	return true;
}


/**
 *  This method is used to clean up the model after a load and also perform any
 *  post-load functions such as recreating the supermodel's functions.
 *
 *  TODO: Move more post load stuff out of loadModel.
 *  TODO: once supermodel no longer needs to reload after every change, change 
 *        this method to syncModel().
 */
void Mutant::postLoad()
{
	// Fix the batching flag for the model
	// i.e. do not batch if model has tints or effects with channels
	if (!this->canBatch() && this->batched())
	{
		WARNING_MSG( "Turning off batch rendering as the model has tints "
					 "and/or effect materials which contain channels.\n" );
		this->batched( false, false );
	}

	this->recreateFashions();
}


bool Mutant::addModel( const std::string& name, bool reload /* = true */ )
{
	if (!ensureModelValid( name, "add" )) return false;
	
	if (name == modelName_) return false;
	
	std::vector< DataSectionPtr > extras;
	extraModels_->openSections( "model", extras );
	
	for (unsigned i=0; i<extras.size(); i++)
	{
		if (name == extras[i]->asString()) return false;
	}

	UndoRedo::instance().add( new UndoRedoOp( 0, extraModels_ ));
	UndoRedo::instance().barrier( "Adding a model", false );

	extraModels_->newSection( "model" )->setString( name );

	if (reload)
	{
		reloadAllLists();
	}

	return true;
}

bool Mutant::hasAddedModels()
{
	return (extraModels_->countChildren() > 0);
}

void Mutant::removeAddedModels()
{
	UndoRedo::instance().add( new UndoRedoOp( 0, extraModels_ ));
	UndoRedo::instance().barrier( "Removing all added models", false );
	
	extraModels_->delChildren();
	reloadModel();
}

void Mutant::buildNodeList( DataSectionPtr data, std::set< std::string >& nodes )
{
	std::vector< DataSectionPtr > nodeData;
	data->openSections( "node", nodeData );
	for (unsigned i=0; i<nodeData.size(); i++)
	{
		nodes.insert( nodeData[i]->readString( "identifier", "" ));
		buildNodeList( nodeData[i], nodes );
	}
}

bool Mutant::canAddModel( std::string modelPath )
{
	std::set< std::string > newNodeNames;
	std::set< std::string > oldNodeNames;

	DataSectionPtr visual;
	ensureModelValid( modelPath, "read", NULL, &visual );
	buildNodeList( visual, newNodeNames );

	buildNodeList( currVisual_, oldNodeNames );

	std::set< std::string >::iterator nodeIt = newNodeNames.begin();
	std::set< std::string >::iterator nodeEnd = newNodeNames.end();

	for( ; nodeIt != nodeEnd; ++nodeIt )
	{
		if (oldNodeNames.find( *nodeIt ) != oldNodeNames.end())
		{
			if ((*nodeIt != "Scene Root") &&
				(nodeIt->find("HP_") == std::string::npos))
					return true;
		}
	}

	return false;
}

void Mutant::reloadAllLists()
{
	if (!superModel_) return;

	reloadModel();
	triggerUpdate("Anim");
	triggerUpdate("Act");
	triggerUpdate("LOD");
	triggerUpdate("Object");
	triggerUpdate("Materials");
}

void Mutant::regenAnimLists()
{
	if (!superModel_) return;

	static int s_animCounter = 0;

	// Make a backup of the  model datasections and animation data
	// and clear the current maps.
	
	std::map< std::string, DataSectionPtr> oldModels;
	std::swap( oldModels, models_ );
	
	std::map< StringPair, AnimationInfo > oldAnims;
	std::swap( oldAnims, animations_ );
	
	actions_.clear();
	dataFiles_.clear();
	animList_.clear();
	actList_.clear();
	lodList_.clear();
	
	std::string modelPath = modelName_;

	DataSectionPtr model = currModel_;

	dataFiles_[currVisual_] = visualName_;

	std::string modelName;
	
	// This code gets data sections of all the models, animations and actions
	// of the supermodel and stores them in maps for later reference.
	do
	{
		if (!model) break;

		std::string::size_type first = modelPath.rfind("/") + 1;
		std::string::size_type last = modelPath.rfind(".");
		modelName = modelPath.substr( first, last-first );
		
		models_[modelPath] = model;
		dataFiles_[model] = modelPath;

		//Animations
		std::vector< std::string > animNames;
		std::vector< DataSectionPtr > pAnimations;
		model->openSections( "animation", pAnimations );
		std::vector< DataSectionPtr >::iterator animIt = pAnimations.begin();
		std::vector< DataSectionPtr >::iterator animEnd = pAnimations.end();
		while (animIt != animEnd)
		{
			DataSectionPtr pData = *animIt++;
			std::string animName = pData->readString( "name", "" );

			if (animName != "")
			{
 				std::map< std::string, float > boneWeights;
				DataSectionPtr pAlpha = pData->openSection( "alpha");
				if (pAlpha)
				{
					DataSectionIterator alphaIt = pAlpha->begin();
					DataSectionIterator alphaEnd = pAlpha->end();
					while (alphaIt != alphaEnd)
					{
						DataSectionPtr bone = *alphaIt++;
						std::string boneName = SanitiseHelper::substringReplace
							(
								bone->sectionName(),
								SanitiseHelper::SANITISING_TOKEN,
								SanitiseHelper::SPACE_TOKEN
							);
						float boneVal = bone->asFloat(0.f);
						boneWeights[ boneName ] = boneVal;
					}
				}

				StringPair animID( animName, modelPath );
				
				DataSectionPtr frameRates = NULL;
				ChannelsInfoPtr channels = NULL;
				float animTime = -1.f;
				
				if (oldAnims.find( animID ) != oldAnims.end())
				{
					//Get the initial framerate
					frameRates = oldAnims[ animID ].frameRates;
					
					//Get the current channel settings
					channels = oldAnims[ animID ].channels;

					//Get the current animation time
					animTime = oldAnims[ animID ].animation->time;
				}
				else                                                            
                {                                                               
                    std::string animPath = pData->readString("nodes","");       
                    if ( animPath != "" )                                       
                    {                                                           
                        std::map< StringPair, AnimationInfo >::iterator oldAnimIt = oldAnims.begin();
                        std::map< StringPair, AnimationInfo >::iterator oldAnimEnd = oldAnims.end();
                        for ( ; oldAnimIt != oldAnimEnd ; oldAnimIt++ )         
                        {                                                       
                            std::string oldAnimPath = oldAnimIt->second.data->readString("nodes", "");
                            if ( animPath == oldAnimPath )                      
                                channels = oldAnimIt->second.channels;          
                        }                                                       
                    }                                                           
                }                                                               

				if (frameRates == NULL)
				{
					char buf[256];
					sprintf( buf, "frameRates%d.xml", s_animCounter++ );
					frameRates = BWResource::openSection( std::string( buf ), true );
					frameRates->writeFloat( "curr", pData->readFloat( "frameRate", 30.f ));
					frameRates->writeFloat( "init", pData->readFloat( "frameRate", 30.f ));
				}

				//Lets get the animation from the supermodel
				SuperModelAnimationPtr animation = superModel_->getAnimation( animName );
				if (animation)
				{
					animations_[ animID ] = AnimationInfo( pData, model, animation, boneWeights, frameRates, channels, animTime );
					animNames.push_back( animName );
				}
			}
		}
		
		animList_.push_back( TreeBranch( StringPair( modelName, modelPath ), animNames) );

		//Actions
		std::vector< std::string > actNames;
		std::vector< DataSectionPtr >	pActions;
		model->openSections( "action", pActions );
		std::vector< DataSectionPtr >::iterator actIt = pActions.begin();
		std::vector< DataSectionPtr >::iterator actEnd = pActions.end();
		while (actIt != actEnd)
		{
			DataSectionPtr data = *actIt++;
			std::string actName = data->readString("name","");
			if (actName != "")
			{
				//Lets get the action from the supermodel
				SuperModelActionPtr action = superModel_->getAction( actName );
				if (action)
				{
					actions_[ StringPair(actName,modelPath) ] = ActionInfo( data, model, action);
					actNames.push_back( actName );
				}
			}
		}
		actList_.push_back( TreeBranch( StringPair( modelName, modelPath ), actNames) );

		float dist = model->readFloat( "extent", LOD_HIDDEN );

		//The LOD list
		lodList_.push_back( LODEntry( StringPair( modelName, modelPath ), model->readFloat( "extent", LOD_HIDDEN )));

		//Move to the next LOD
		modelPath = model->readString("parent","") + ".model";
		if (modelPath != ".model")
		{
			//Make sure we use the same model DataSectionPtr as previously used if it exists.
			if (modelHist_.find( modelPath ) != modelHist_.end())
			{
				model = modelHist_[ modelPath ];
			}
			else // Otherwise load it up...
			{
				model = BWResource::openSection( modelPath );
			}
		}

	}
	while (modelPath != ".model");

	//The LOD list
	if (modelPath != ".model")
	{
		//This is a special case where the model cannot be loaded...
		modelName = BWResource::getFilename( modelPath );
		modelName = BWResource::removeExtension( modelName );
		
		lodList_.push_back( LODEntry( StringPair( modelName, modelPath ), LOD_MISSING ));
	}
}

void Mutant::regenMaterialsList()
{
	if (!superModel_) return;

	materials_.erase(materials_.begin(),materials_.end());
	dyes_.erase(dyes_.begin(),dyes_.end());
	tints_.erase(tints_.begin(),tints_.end());

	tintedMaterials_.erase( tintedMaterials_.begin(), tintedMaterials_.end() );

	materialNameDataToRemove_.clear();
	
	materialList_.clear();
		
	std::vector< DataSectionPtr >	pRenderSets;
	currVisual_->openSections( "renderSet", pRenderSets );
	std::vector< DataSectionPtr >::iterator renderSetsIt = pRenderSets.begin();
	std::vector< DataSectionPtr >::iterator renderSetsEnd = pRenderSets.end();
	while (renderSetsIt != renderSetsEnd)
	{
		DataSectionPtr renderSet = *renderSetsIt++;

		std::vector< DataSectionPtr >	pGeometries;
		renderSet->openSections( "geometry", pGeometries );
		std::vector< DataSectionPtr >::iterator geometriesIt = pGeometries.begin();
		std::vector< DataSectionPtr >::iterator geometriesEnd = pGeometries.end();
		while (geometriesIt != geometriesEnd)
		{
			DataSectionPtr geometries = *geometriesIt++;

			std::string vertGroup = geometries->readString( "vertices", "" );
			if (vertGroup.find("/") == -1)
				vertGroup = primitivesName_ + "/" + vertGroup;

			std::vector< DataSectionPtr >	pPrimitiveGroups;
			geometries->openSections( "primitiveGroup", pPrimitiveGroups );
			std::vector< DataSectionPtr >::iterator primitiveGroupsIt = pPrimitiveGroups.begin();
			std::vector< DataSectionPtr >::iterator primitiveGroupsEnd = pPrimitiveGroups.end();
			while (primitiveGroupsIt != primitiveGroupsEnd)
			{
				DataSectionPtr primitiveGroups = *primitiveGroupsIt++;

				std::vector< DataSectionPtr >	pMaterials;
				primitiveGroups->openSections( "material", pMaterials );
				std::vector< DataSectionPtr >::iterator materialsIt = pMaterials.begin();
				std::vector< DataSectionPtr >::iterator materialsEnd = pMaterials.end();
				while (materialsIt != materialsEnd)
				{
					DataSectionPtr materials = *materialsIt++;

					std::string identifier = materials->readString("identifier","");

					materials_[identifier].name = identifier;
					
					materials_[identifier].data.push_back( materials );

					materials_[identifier].format = Moo::VerticesManager::instance()->get( vertGroup )->format();
						
					std::vector< Moo::EffectMaterial** > uses;
					
					int numMats = superModel_->topModel(0)->gatherMaterials( identifier, uses );
					for (int i=0; i<numMats; i++)
					{
						materials_[identifier].effect.insert( *uses[i] );
					}

					int found = -1;
					for (unsigned i=0; i<materialList_.size(); i++)
					{
						if (materialList_[i].first.first == identifier)
						{
							found = i;
							break;
						}
					}

					if ( found == -1 )
					{
						std::vector< std::string > empty;
						materialList_.push_back( TreeBranch( StringPair( identifier, "" ), empty ));
					}
				}
			}
		}
	}
	
	// Read in the material name aliases
	DataSectionPtr data = currModel_->openSection( "materialNames", true );
	std::vector< DataSectionPtr >	pNames;
	data->openSections( "material", pNames );
	std::vector< DataSectionPtr >::iterator namesIt = pNames.begin();
	std::vector< DataSectionPtr >::iterator namesEnd = pNames.end();
	while (namesIt != namesEnd)
	{
		DataSectionPtr materialNameData = *namesIt++;

		std::string original = materialNameData->readString( "original", "" );
		std::string displayName = materialNameData->readString( "display", original );

		if (materials_.find(original) != materials_.end())
		{
			materials_[original].name = displayName;
			materials_[original].nameData = materialNameData;
		}
		else
		{
			materialNameDataToRemove_.push_back( materialNameData );
		}
	}

	std::vector< DataSectionPtr >	pDyes;
	currModel_->openSections( "dye", pDyes );
	std::vector< DataSectionPtr >::iterator dyesIt = pDyes.begin();
	std::vector< DataSectionPtr >::iterator dyesEnd = pDyes.end();
	while (dyesIt != dyesEnd)
	{
		DataSectionPtr dye = *dyesIt++;
		std::string replaces = dye->readString("replaces","");
		std::string matter = dye->readString("matter","");

		dyes_[matter] = dye;

		int found = -1;
		for (unsigned i=0; i<materialList_.size(); i++)
		{
			if (materialList_[i].first.first == replaces)
			{
				materialList_[i].first.second = matter;
				found = i;
				break;
			}
		}

		// If we have not found a matching material, try the next
		if ( found == -1 ) continue;

		std::vector< DataSectionPtr >	pTints;
		dye->openSections( "tint", pTints );
		std::vector< DataSectionPtr >::iterator tintIt = pTints.begin();
		std::vector< DataSectionPtr >::iterator tintEnd = pTints.end();
		while (tintIt != tintEnd)
		{
			DataSectionPtr tint = *tintIt++;
			std::string name = tint->readString("name","");
			materialList_[found].second.push_back( name );

			tints_[matter][name].data = tint;

			tints_[matter][name].format = materials_[replaces].format;

			Model::Tint* pTint = NULL;

			superModel_->topModel(0)->getDye( matter, name, &pTint);

			tints_[matter][name].effect = pTint->newMaterial_;

			tints_[matter][name].effect->load( tints_[matter][name].data->openSection( "material" ));

			SuperModelDyePtr spDye = superModel_->getDye( matter , name );
			tints_[matter][name].dye = spDye;
		}

		Model::Tint* pTint = NULL;
		superModel_->topModel(0)->getDye( matter, "Default", &pTint);
		tints_[matter]["Default"].effect = pTint->newMaterial_;

		SuperModelDyePtr spDye = superModel_->getDye( matter , "Default" );
		tints_[matter]["Default"].dye = spDye;

		//Load the default material in case the visual has changed
		if ((materials_.find(replaces) != materials_.end()) && (materials_[replaces].data[0]))
		{
			tints_[matter]["Default"].effect->load( materials_[replaces].data[0] );
		}

		tintedMaterials_[replaces] = matter;
	}

	// Now reload any materials that are NOT tints...
	std::map< std::string, MaterialInfo >::iterator matIt = materials_.begin();
	std::map< std::string, MaterialInfo >::iterator matEnd = materials_.end();

	for ( ;matIt != matEnd; ++matIt )
	{
		DataSectionPtr data = matIt->second.data[0];

		if (data == NULL) continue;

		std::string id = data->readString( "identifier", "" );

		if (tintedMaterials_.find( id ) != tintedMaterials_.end()) continue;
		
		std::set< Moo::EffectMaterial* >::iterator sectIt = matIt->second.effect.begin();
		std::set< Moo::EffectMaterial* >::iterator sectEnd = matIt->second.effect.end();

		for (;sectIt != sectEnd; ++sectIt)
		{
			Moo::EffectMaterial* effect = *sectIt;

			if (effect == NULL) continue;

			effect->load( data );
		}
	}
}

/**
 *	recreate the fashions of this model using the current settings
 */
void Mutant::recreateFashions()
{
	if (!superModel_) return;

	fashions_.clear();
	
	bool actSet = false;
	
	if (!animMode_)
	{
		if ((currAct_.first != "") && (currAct_.second != "") && (actions_.find(currAct_) != actions_.end()))
		{
			SuperModelActionPtr action = actions_[currAct_].action; 
			if (action)
			{
				action->passed = 0.f;
        		action->played = 0.f;

				if ((action->pSource) && (!action->pSource->filler_))
				{
					ModelPtr model = superModel_->topModel(0);

					if (model)
					{
						int animIdx = model->getAnimation( action->pSource->animation_ );

						Model::Animation* anim = model->lookupLocalAnimation( animIdx );

						if (anim)
						{
							action->finish = anim->duration_;
							fashions_.push_back( action );
							actSet = true;
						}
					}
				}
				else
				{
					action->finish = -1.f;
					fashions_.push_back( action );
					actSet = true;
				}
			}
		}
	}

	if (!clearAnim_ && ((animMode_) || (!actSet)))
	{
		AnimIt animIt = currAnims_.begin();
		AnimIt animEnd = currAnims_.end();
		
		while (animIt != animEnd)
		{
			if ((animIt->second.first != "") && (animIt->second.second != "") && (animations_.find(animIt->second) != animations_.end()))
			{
				SuperModelAnimationPtr animation = animations_[animIt->second].animation; 
				if (animation)
				{
					animation->blendRatio = 1.f;
					fashions_.push_back( animation );
				}
			}
			animIt++;
		}
	}

	Dyes::iterator dyeIt = currDyes_.begin();
	Dyes::iterator dyeEnd = currDyes_.end();

	while ( dyeIt != dyeEnd )
	{
		if (tints_.find( dyeIt->first ) != tints_.end())
		{
			if (tints_[dyeIt->first].find( dyeIt->second ) != tints_[dyeIt->first].end())
			{
				fashions_.push_back( tints_[dyeIt->first][dyeIt->second].dye );
			}
		}
		dyeIt++;
	}
}

void Mutant::recreateModelVisibilityBox( AnimLoadCallbackPtr pAnimLoadCallback, bool undoable )
{
	if (!superModel_) return;

	//Reset the bounding box first
	visibilityBB_ = BoundingBox::s_insideOut_;
	
	//Save the current fashion
	FashionVector oldFashion = fashions_;

	//Save the current positional settings
	bool grounded = groundModel_;
	groundModel_ = false;
	bool centred = centreModel_;
	centreModel_ = false;

	//Do an initial update in case this is a static model
	updateFrameBoundingBox();
	updateVisibilityBox();
	
	std::map < StringPair , AnimationInfo >::iterator it =  animations_.begin();
	std::map < StringPair , AnimationInfo >::iterator end =  animations_.end();

	for(; it != end; ++it)
	{
		SuperModelAnimationPtr animation = it->second.animation; 
		if ((animation) && (animation->pSource(*superModel_)))
		{
			fashions_.clear();
			animation->time = 0.f;
			animation->blendRatio = 1.f;
			fashions_.push_back( animation );
			while ( animation->time <= animation->pSource(*superModel_)->duration_ )
			{
				updateFrameBoundingBox();
				updateVisibilityBox();
				animation->time += 1.f / 60.f;
			}

			//Do a final
			animation->time = animation->pSource(*superModel_)->duration_;
			updateFrameBoundingBox();
			updateVisibilityBox();

			if (pAnimLoadCallback)
				pAnimLoadCallback->execute();
		}
	}

	DataSectionPtr data = currModel_->openSection( "visibilityBox" );
	if (data == NULL) data = currModel_->newSection( "visibilityBox" );
		
	if (undoable)
	{
		UndoRedo::instance().add( new UndoRedoOp( 0, data ));
	}

	//Save the new bounding box
	data->writeVector3( "min", visibilityBB_.minBounds() ); 
	data->writeVector3( "max", visibilityBB_.maxBounds() ); 
	dirty( currModel_ );

	if (undoable)
	{
		UndoRedo::instance().barrier( "Updating the model's visibility box.", false );
	}

	visibilityBoxDirty_ = false; // OK, visibility box all updated

	//Restore the fashion
	fashions_ = oldFashion;

	//Restore the positional settings
	groundModel_ = grounded;
	centreModel_ = centred;
}

void Mutant::updateAnimation( const StringPair& animID, float dTime )
{
	if (!superModel_) return;

	if ((animID.first != "") && (animID.second != "") && (animations_.find(animID) != animations_.end()))
	{
		SuperModelAnimationPtr animation = animations_[animID].animation; 

		if ((animation) && (animation->pSource(*superModel_)))
		{
			float initFrameRate = animations_[animID].frameRates->readFloat( "init", 30.f );
			float currFrameRate = animations_[animID].frameRates->readFloat( "curr", 30.f );
			
			if (playing_)
			{
				animation->time += dTime * (currFrameRate / initFrameRate);
			}

			while ( animation->time > animation->pSource(*superModel_)->duration_ )
			{
				if (looping_)
				{
					if (animation->pSource(*superModel_)->duration_ != 0.f)
					{
						animation->time -= animation->pSource(*superModel_)->duration_;
					}
					else
					{
						animation->time = 0.f;
					}
				}
				else
				{
					animation->time = 0.f;
					playing_ = false;
				}
			}
		}
	}
}

void Mutant::updateAction( const StringPair& actID, float dTime )
{
	if (!superModel_) return;

	if ((actID.first != "") && (actID.second != "") && (actions_.find(actID) != actions_.end()))
	{
		SuperModelActionPtr action = actions_[actID].action; 

		if (action)
		{
			action->passed += dTime;
        	action->played += dTime;
			if ((action->finish > 0.f) && (action->played > action->finish) && !clearAnim_)
			{
				animMode_ = true;
				recreateFashions();
			}
		}
	}
}

/**
 *	draws the skeleton of this model
 */
void Mutant::drawSkeleton()
{
	if (!superModel_) return;
	
	Moo::rc().push();
	Moo::rc().world( Matrix::identity );

	// draw skeleton for each model
	for (int i=0; i<superModel_->nModels(); ++i)
	{
		Model * model = superModel_->curModel( i );
		if (model != NULL)
		{
			Model::NodeTreeIterator it  = model->nodeTreeBegin();
            Model::NodeTreeIterator end = model->nodeTreeEnd();

            // Advance past the base node
            if (it != end) it++;

			while (it != end)
			{
				// skip leaf nodes
				std::string nodeName = it->pData->pNode->identifier();          
                if ( ( nodeName.substr(0, 3) != std::string("HP_") ) && ( nodeName.find("BlendBone", 0) == nodeName.npos) )
				{
					Vector3	fromPos = it->pParentTransform->applyToOrigin();

					const Matrix & nodeTrans = it->pData->pNode->worldTransform();
					Vector3 toPos = nodeTrans.applyToOrigin();

					// draw arrow body
					Geometrics::drawLine( fromPos, toPos, 0x0000ff00 );

					// draw arrow head
					Vector3 vDir  = (toPos - fromPos);
                    float length = 0.1f * vDir.length(); // Make the arrowhead proportional to the length of the bone.
					vDir.normalise();
					Vector3 ortog = vDir.crossProduct( nodeTrans[1] );
					ortog.normalise();
					Geometrics::drawLine( toPos, toPos - vDir * length + ortog * length, 0x00ff0000 );
					Geometrics::drawLine( toPos, toPos - vDir * length - ortog * length, 0x00ff0000 );
				}
				it++;
			}
		}
	}
	Moo::rc().pop();
}

Vector3 Mutant::getCurrentRootNode()
{	
	if (!superModel_) return Vector3( 0.f, 0.f, 0.f );
	
	Vector3 rootNode( Vector3::zero() );

	if (superModel_ != NULL)
	{
		Model * model = superModel_->curModel( 0 );
		if (model != NULL)
		{
			Model::NodeTreeIterator it  = model->nodeTreeBegin();
			Model::NodeTreeIterator end = model->nodeTreeEnd();

			if (it != end) it++;
			if (it != end) it++;

			if (it != end) 
			{
				Matrix nodeTrans = it->pData->pNode->worldTransform();
				rootNode = nodeTrans.applyToOrigin();
				hasRootNode_ = true;
			}
		}
	}

	return rootNode;
}

Matrix Mutant::transform( bool grounded, bool centred /* = true */ )
{
	if (!superModel_) return Matrix::identity;

	//TODO: Fix the centering behaviour better.
	
	Matrix transform = Matrix::identity;
	if (grounded)
	{
		transform[3][1] = - modelBB_.minBounds().y;
	}
	if (centred)
	{
		BoundingBox bb = modelBB_;

		/*
		if (hasRootNode_)
		{
			Matrix m = Matrix::identity;
			Vector3 t( getCurrentRootNode() - initialRootNode_ );
			t[1] = 0.f;
			m.setTranslate( t );
			bb.transformBy( m );
		}
		//*/

		Vector3 centre = bb.centre();
		transform[3][0] = -centre.x;
		transform[3][2] = -centre.z;
	}
	return transform;
}

BoundingBox Mutant::zoomBoundingBox()
{
	if (!superModel_) return BoundingBox( Vector3( 0.f, 0.f, 0.f ), Vector3( 0.f, 0.f, 0.f ));

	BoundingBox bb( modelBB_ );
	Matrix m;
	if (hasRootNode_)
	{
		m = Matrix::identity;
		Vector3 t( getCurrentRootNode() - initialRootNode_ );
		t[0] = 0.f;
		t[2] = 0.f;
		m.setTranslate( t );
	}
	else
	{
		m = transform( groundModel_, centreModel_ );
	}
	bb.transformBy( m );
	return bb;
}

BoundingBox Mutant::modelBoundingBox()
{
	if (!superModel_) return BoundingBox( Vector3( 0.f, 0.f, 0.f ), Vector3( 0.f, 0.f, 0.f ));

	BoundingBox bb( modelBB_ );
	Matrix m;
	if (hasRootNode_)
	{
		m = Matrix::identity;
		Vector3 t( getCurrentRootNode() - initialRootNode_ );
		m.setTranslate( t );
	}
	else
	{
		m = transform( groundModel_, centreModel_ );
	}
	bb.transformBy( m );
	return bb;
}

BoundingBox Mutant::visibililtyBox()
{
	if (!superModel_) return BoundingBox( Vector3( 0.f, 0.f, 0.f ), Vector3( 0.f, 0.f, 0.f ));

	BoundingBox bb( visibilityBB_ );
	Matrix m;
	if (hasRootNode_)
	{
		m = Matrix::identity;
		Vector3 t( getCurrentRootNode() - initialRootNode_ );
		m.setTranslate( t );
	}
	else
	{
		m = transform( groundModel_, centreModel_ );
	}
	bb.transformBy( m );
	return bb;
}

/*
 *	This class calculates the bounding box of an object from the
 *	transformed positions of its vertices.
 *
 */
class BoundingBoxDrawOverride : public Moo::Visual::DrawOverride
{
public:
	BoundingBoxDrawOverride() :
	  bb_( BoundingBox::s_insideOut_ )
	{
	}

	// This is the amount of vertices each model piece are allowed to use
	// for calculating their bounding boxes
	static const uint32 NUM_BB_VERTICES = 1000;

	HRESULT draw( Moo::Visual* pVisual, bool ignoreBoundingBox )
	{
		typedef Moo::Visual::RenderSetVector RSV;
		RSV& renderSets = pVisual->renderSets();
		RSV::iterator rsit = renderSets.begin();
		RSV::iterator rsend = renderSets.end();

		while (rsit != rsend)
		{
			Moo::Visual::RenderSet& rs = *(rsit++);
			Moo::Visual::GeometryVector::iterator git = rs.geometry_.begin();
			Moo::Visual::GeometryVector::iterator gend = rs.geometry_.end();
			while (git != gend)
			{
				Moo::Visual::Geometry& geom = *(git++);
				Moo::VerticesPtr pVerts = geom.vertices_;
				if (rs.treatAsWorldSpaceObject_)
				{
					// Use the software skinner
					Moo::BaseSoftwareSkinner* pSkinner = pVerts->softwareSkinner();

					if (pSkinner)
					{
						static VectorNoDestructor< Vector3 > positions;
						positions.resize( pVerts->nVertices() );

						uint32 skip = pVerts->nVertices() / NUM_BB_VERTICES;

						pSkinner->transformPositions( &positions.front(),
							0, pVerts->nVertices(), rs.transformNodes_, true,
							skip );

						uint32 nVertices = pVerts->nVertices() / (skip + 1);

						for (uint32 i = 0; i < nVertices; i++)
						{
							bb_.addBounds( positions[i] );
						}
					}
				}
				else
				{
					const Moo::Vertices::VertexPositions& vps = geom.vertices_->vertexPositions();
					const Matrix& wt = rs.transformNodes_[0]->worldTransform();

					uint32 skip = (vps.size() / NUM_BB_VERTICES) + 1;

					for (uint32 i = 0; i < vps.size(); i += skip)
					{
						bb_.addBounds( wt.applyPoint( vps[i] ) );
					}
				}
			}
		}

		return S_OK;
	}

	BoundingBox bb_;
};


void Mutant::updateFrameBoundingBox()
{
	if (!superModel_) return;

	static DogWatch s_bbTimer("BoundingBox");
	s_bbTimer.start();

	BoundingBoxDrawOverride bbdo;
	Moo::Visual::s_pDrawOverride = &bbdo;
	drawModel( true );
	Moo::Visual::s_pDrawOverride = NULL;
	frameBB_ = bbdo.bb_;

	if ( frameBB_ == BoundingBox::s_insideOut_ )
	{
		Vector3 root = getCurrentRootNode();
		frameBB_ = BoundingBox( root, root );
	}

	s_bbTimer.stop();
}

void Mutant::updateVisibilityBox()
{
	if (!superModel_) return;

	BoundingBox oldBB = visibilityBB_;
	Matrix t( Matrix::identity );
	Vector3 trans( initialRootNode_ - getCurrentRootNode() );
	t.setTranslate( trans );
	BoundingBox bb( frameBB_ );
	bb.transformBy( t );
	visibilityBB_.addBounds( bb );

	if ( visibilityBB_ == BoundingBox::s_insideOut_ )
	{
		visibilityBB_ = oldBB;
	}
}

void Mutant::drawBoundingBoxes()
{
	if (!superModel_) return;

	/*Vector3 minB ( modelBB_.minBounds() - frameBB_.minBounds() );
	Vector3 maxB ( modelBB_.maxBounds() - frameBB_.maxBounds() );
	
	if (minB.length() + maxB.length() > 0.01f)
	{
		Moo::rc().push();
		Moo::rc().preMultiply( transform( groundModel_, centreModel_ ) );
		Geometrics::wireBox( frameBB_, 0x00ffff00 );
		Moo::rc().pop();
	}*/
	
	Moo::rc().push();
	Matrix m;
	if (hasRootNode_)
	{
		m = Matrix::identity;
		Vector3 t( getCurrentRootNode() - initialRootNode_ );
		m.setTranslate( t );
	}
	else
	{
		m = transform( groundModel_, centreModel_ );
	}
	Moo::rc().preMultiply( m );
	Geometrics::wireBox( visibilityBB_, 0x000000ff );
	Geometrics::wireBox( modelBB_, 0x00ffff00 );
	Moo::rc().pop();
}

float Mutant::drawModel( bool atOrigin, float atDist /* = -1.f */, bool drawIt /* = true */ )
{
	if (!superModel_) return atDist;

	Moo::rc().push();
	if (!atOrigin)
		Moo::rc().preMultiply( transform( groundModel_, centreModel_ ));
		
	if (virtualDist_ != -1)
	{
		atDist = virtualDist_;
	}
		
	atDist = superModel_->draw( &fashions_, 0, atDist, drawIt );

	if (!foundRootNode_)
	{
		initialRootNode_ = getCurrentRootNode();
		foundRootNode_ = true;
	}

	Moo::rc().pop();

	return atDist;
}

void Mutant::drawOriginalModel()
{
	if (!superModel_) return;

	if ( currAnims_.empty() ) return;
		
	Moo::rc().push();
    Moo::rc().preMultiply( transform( groundModel_, centreModel_ ) );

	std::map< size_t, std::vector<Moo::AnimationChannelPtr> > oldChannels;

	AnimIt animEnd = currAnims_.end();
	
	// OK let's now replace the animation channels with the uncompressed ones
	AnimIt animIt = currAnims_.begin();
	for (;animIt != animEnd; ++animIt)
	{
		if (animations_.find(animIt->second) == animations_.end()) 
			continue;

		SuperModelAnimationPtr superModelAnim = animations_[animIt->second].animation;

		if (superModelAnim == NULL) 
			continue;

		Model::Animation* modelAnim = superModelAnim->pSource(*superModel_);

		if (modelAnim == NULL) 
			continue;

		Moo::AnimationPtr anim = modelAnim->getMooAnim();

		if (anim == NULL) 
			continue;

		animations_[animIt->second].uncompressAnim( anim, oldChannels[animIt->first] );
	}
		
	// Now draw the model
	superModel_->draw( &fashions_, 0, virtualDist_ );

	// Finally put everything back as it was...
	animIt = currAnims_.begin();
	for (;animIt != animEnd; ++animIt)
	{
		if (animations_.find(animIt->second) == animations_.end()) 
			continue;

		SuperModelAnimationPtr superModelAnim = animations_[animIt->second].animation;

		if (superModelAnim == NULL) 
			continue;

		Model::Animation* modelAnim = superModelAnim->pSource(*superModel_);

		if (modelAnim == NULL) 
			continue;

		Moo::AnimationPtr anim = modelAnim->getMooAnim();

		if (anim == NULL) 
			continue;

		animations_[animIt->second].restoreAnim( anim, oldChannels[animIt->first] );
	}

	Moo::rc().pop();
}

void Mutant::drawPortals()
{
	if (!superModel_) return;

	if (!currVisual_) return;

	//Load the portal material if we haven't already
    static Moo::EffectMaterial* pPortalMat = NULL;
	if (!pPortalMat)
	{
		pPortalMat = new Moo::EffectMaterial;
		pPortalMat->initFromEffect( "resources/effects/portal.fx" );
		if (!pPortalMat) return; 
	}

	Moo::rc().setVertexShader( NULL );
	Moo::rc().device()->SetPixelShader( NULL );
					
	Moo::rc().setRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
    Moo::rc().setFVF( Moo::VertexXYZ::fvf() );

    // First of all lets try and handle the old portal system

    std::vector<DataSectionPtr>	spOldPortalSets;
    currVisual_->openSections( "portal", spOldPortalSets );
    std::vector<DataSectionPtr>::iterator opit = spOldPortalSets.begin();
	std::vector<DataSectionPtr>::iterator opend = spOldPortalSets.end();
	while (opit != opend)
	{
		DataSectionPtr portalSet = *opit++;

        std::vector<DataSectionPtr>	spPointSets;
        portalSet->openSections( "point", spPointSets );
        std::vector<DataSectionPtr>::iterator ptit = spPointSets.begin();
        std::vector<DataSectionPtr>::iterator ptend = spPointSets.end();

        std::vector<Vector3> points;

        // Iterate through our points
        while (ptit != ptend)
        {
                points.push_back( (*ptit++)->asVector3() );
        }

        //Now we need to reorder the points to be in the correct order

        PlaneEq basePlane( points[0], points[1], points[2] );
        Vector3 baseNorm = basePlane.normal();
        baseNorm.normalise();

        bool swapped;
        do
        {
			swapped = false;
			for (unsigned i=3; i<points.size(); i++)
			{
				PlaneEq testPlane( points[0], points[i-1], points[i] );
				Vector3 testNorm = testPlane.normal();
				testNorm.normalise();
				if ((testNorm + baseNorm).length() < 1.0f)
				{
					swapped = true;
					Vector3 temp = points[i];
					points[i] = points[i-1];
					points[i-1] = temp;
				}
			}
        }
        while (swapped);

        Moo::rc().device()->SetTransform( D3DTS_WORLD, &transform( groundModel_, centreModel_ ) );
		Moo::rc().device()->SetTransform( D3DTS_VIEW, &Moo::rc().view() );
		Moo::rc().device()->SetTransform( D3DTS_PROJECTION, &Moo::rc().projection() );
        if (pPortalMat->begin())
        {
            for (uint32 i = 0; i < pPortalMat->nPasses(); i++)
            {
                pPortalMat->beginPass( i );
                Moo::rc().device()->DrawPrimitiveUP(
                    D3DPT_TRIANGLEFAN,
                    points.size() - 2,
                    &points[0],
                    sizeof( Moo::VertexXYZ ) );
                pPortalMat->endPass();
            }
            pPortalMat->end();
        }
    }

    // OK, now lets try the new setup

    std::vector<DataSectionPtr>	spBoundarySets;
    currVisual_->openSections( "boundary", spBoundarySets );
    std::vector<DataSectionPtr>::iterator bit = spBoundarySets.begin();
	std::vector<DataSectionPtr>::iterator bend = spBoundarySets.end();

	// Iterate through our boundaries.
	while (bit != bend)
	{
		DataSectionPtr boundarySet = *bit++;

        //Get the normal and distance
        Vector3 norm = boundarySet->readVector3( "normal", Vector3(0,0,0) );
        float d = boundarySet->readFloat( "d", 0.0 );

        std::vector<DataSectionPtr>	spPortalSets;
        boundarySet->openSections( "portal", spPortalSets );
        std::vector<DataSectionPtr>::iterator pit = spPortalSets.begin();
        std::vector<DataSectionPtr>::iterator pend = spPortalSets.end();

        // Iterate through our portals
        while (pit != pend)
        {
            DataSectionPtr portalSet = *pit++;

            Vector3 uAxis = portalSet->readVector3( "uAxis", Vector3(0,0,0) );
            Vector3 vAxis = norm.crossProduct( uAxis );
	        Vector3 origin = norm * d / norm.lengthSquared();

            std::vector<DataSectionPtr>	spPointSets;
            portalSet->openSections( "point", spPointSets );
            std::vector<DataSectionPtr>::iterator ptit = spPointSets.begin();
            std::vector<DataSectionPtr>::iterator ptend = spPointSets.end();

            std::vector<Vector3> points;

            // Iterate through our points
            while (ptit != ptend)
            {
				Vector3 pt = (*ptit++)->asVector3();

				points.push_back( Vector3( uAxis * pt[0] + vAxis * pt[1] + origin ) );
            }

            Moo::rc().device()->SetTransform( D3DTS_WORLD, &transform( groundModel_, centreModel_ ) );
			Moo::rc().device()->SetTransform( D3DTS_VIEW, &Moo::rc().view() );
			Moo::rc().device()->SetTransform( D3DTS_PROJECTION, &Moo::rc().projection() );
            if (pPortalMat->begin())
	        {
		        for (uint32 i = 0; i < pPortalMat->nPasses(); i++)
		        {
			        pPortalMat->beginPass( i );
			        Moo::rc().device()->DrawPrimitiveUP(
						D3DPT_TRIANGLEFAN,
						points.size() - 2,
						&points[0],
						sizeof( Moo::VertexXYZ ) );
			        pPortalMat->endPass();
		        }
		        pPortalMat->end();
	        }
        }
	}
}

void Mutant::reloadBSP()
{
	if (!superModel_) return;

	Moo::VisualPtr visual = superModel_->topModel(0)->getVisual();
	if (visual)	
	{
		visual->updateBSP();

		delete [] verts_;
		verts_ = NULL;
		nVerts_ = 0;

		const BSPTree *tree = superModel_->topModel(0)->decompose();

		if (tree)
		{
			tree->createVertexList(verts_, nVerts_, 0x000000FF);
		}
	}
}

void Mutant::drawBsp()
{
	if (!superModel_) return;

	Moo::rc().push();

	//set the transforms
	Moo::rc().device()->SetTransform( D3DTS_WORLD, &transform( groundModel_, centreModel_ ) );
	Moo::rc().device()->SetTransform( D3DTS_VIEW, &Moo::rc().view() );
	Moo::rc().device()->SetTransform( D3DTS_PROJECTION, &Moo::rc().projection() );

	Moo::rc().setVertexShader( NULL );
	Moo::rc().device()->SetPixelShader( NULL );
	Moo::rc().setFVF( Moo::VertexXYZL::fvf() );

	Moo::rc().setRenderState( D3DRS_ALPHATESTENABLE, FALSE );
	Moo::rc().setRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
	Moo::rc().setRenderState( D3DRS_LIGHTING, FALSE );
	Moo::rc().setRenderState( D3DRS_CULLMODE, D3DCULL_NONE);
	Moo::rc().setRenderState( D3DRS_ZWRITEENABLE, TRUE );
	Moo::rc().setRenderState( D3DRS_ZENABLE, D3DZB_TRUE );
	Moo::rc().setRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
	Moo::rc().setRenderState( D3DRS_FOGENABLE, FALSE );

	Moo::rc().setTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
	Moo::rc().setTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );
	Moo::rc().setTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
	Moo::rc().setTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
	Moo::rc().setTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );

	Moo::rc().drawPrimitiveUP( D3DPT_TRIANGLELIST, nVerts_ / 3, &verts_[0], sizeof( Moo::VertexXYZL ) );

	Moo::rc().pop();
}

void Mutant::updateActions( float dTime )
{
	if (!superModel_) return;

	updateAction( currAct_, dTime );
	
	AnimIt animIt = currAnims_.begin();
	AnimIt animEnd = currAnims_.end();
	while (animIt != animEnd)
	{
		updateAnimation( animIt->second, dTime );
		animIt++;
	}
}

float Mutant::render( float dTime, bool showModel, bool showBoundingBoxes, bool showSkeleton, bool showPortals, bool showBsp )
{
	if (!superModel_) return -1.f;

	float atDist = -1.f;

	if (showBsp)
	{
		drawBsp();
	}
	else if (showModel)
	{
		atDist = drawModel();
	}

	if (showSkeleton)
	{
		drawSkeleton();
	}

	if (showBoundingBoxes)
	{
		drawBoundingBoxes();
	}

	if (showPortals)
	{
		drawPortals();
	}

	return atDist;
}

void Mutant::dirty( DataSectionPtr data )
{
	dirty_.insert( data );
}

bool Mutant::dirty()
{
	return UndoRedo::instance().needsSave();
}

void Mutant::save()
{
	if (!dirty()) return;

	char buf[256];

	cleanMaterials();
	cleanTints();
	cleanMaterialNames();

	for ( std::set<DataSectionPtr>::iterator i = dirty_.begin(); i != dirty_.end(); i++ )
	{
		if (dataFiles_.find(*i) != dataFiles_.end())
		{
			sprintf( buf, "Saving file: %s\n", dataFiles_[*i].c_str() );
			ME_INFO_MSG( buf );
		}

		(*i)->save();
	}

	dirty_.clear();

	UndoRedo::instance().setSavePoint();

	//Let BigBang know that the model has changed
	if( modelName_ != "" )
	{
		HANDLE mailSlot = CreateFile( "\\\\.\\mailslot\\BigBangUpdate",
			GENERIC_WRITE, FILE_SHARE_READ, NULL,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0 );
		if( mailSlot != INVALID_HANDLE_VALUE )
		{
			DWORD bytesWritten;
			WriteFile( mailSlot, modelName_.c_str(), modelName_.size() + 1, &bytesWritten, 0 );
			CloseHandle( mailSlot );
		}
	}
}
