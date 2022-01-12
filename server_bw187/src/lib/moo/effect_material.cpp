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
#include "effect_material.hpp"
#include "managed_effect.hpp"
#include "resmgr/bwresource.hpp"
#include "effect_state_manager.hpp"
#include "visual_channels.hpp"
#include "resmgr/xml_section.hpp"

DECLARE_DEBUG_COMPONENT2( "Moo", 0 );

namespace Moo
{

// -----------------------------------------------------------------------------
// Section: EffectMaterial
// -----------------------------------------------------------------------------

SimpleMutex EffectMaterial::effectInitQueueMutex;
std::list<EffectMaterial*> EffectMaterial::effectInitQueue;


/**
 *	Constructor.
 */
EffectMaterial::EffectMaterial() :
	channel_( NULL ),
	channelOverridden_(false),
	collisionFlags_(0),
	materialKind_(0),
	skinned_(false),
	bumpMapped_(false),
	initComplete_(false),
	techniqueExplicitlySet_(false)
#ifdef EDITOR_ENABLED
	,bspModified_( false )
#endif
{
	EffectManager::instance()->addListener(this);
}

/**
 *	Copy constructor.
 */
EffectMaterial::EffectMaterial( const EffectMaterial & other )
{
	*this = other;
	EffectManager::instance()->addListener(this);
}

/**
 *	Destructor.
 */
EffectMaterial::~EffectMaterial()
{
	SimpleMutexHolder mutex( EffectMaterial::effectInitQueueMutex );

	if (!effectInitQueue.empty())
	{
		std::list<EffectMaterial*>::iterator myInit = std::find( effectInitQueue.begin(), effectInitQueue.end(), this );
		if( myInit != effectInitQueue.end() )
		{
			effectInitQueue.erase( myInit );
		}
	}

	if (EffectManager::instance() != NULL)
	{
		EffectManager::instance()->delListener(this);
	}
			
	// unregister as feature listener from
	// all effects flagged as engine features
	std::vector<EffectIntPair>::iterator effIt  = this->pEffects_.begin();
	std::vector<EffectIntPair>::iterator effEnd = this->pEffects_.end();
	while (effIt != effEnd)
	{
		if (effIt->first->graphicsSettingEntry() != 0)
		{
			if (initComplete_)
			{
				MF_ASSERT(effIt->second != -1);
			}

			if (effIt->second != -1)
			{
				effIt->first->graphicsSettingEntry()->delListener(effIt->second);
			}
		}
		++effIt;
	}
}

/**
 *	This method completes the initialisation of the material.
 */
void EffectMaterial::finishInit() const
{
	MF_ASSERT_DEBUG( g_renderThread == true );
	initComplete_ = true;
	EffectMaterial* eMat = const_cast<EffectMaterial*>( this );
	for (uint32 i = 0; i < pEffects_.size(); i++)
	{
		if (Moo::rc().device()->TestCooperativeLevel() == D3D_OK)
		{
			D3DXHANDLE hT = NULL;
			EffectTechniqueSetting* pSetting = pEffects_[i].first->graphicsSettingEntry();
			if (pSetting && 
					pSetting->activeTechniqueHandle() != 0)
			{
				MF_ASSERT( pEffects_[i].second == -1 );
				eMat->pEffects_[i].second = pSetting->addListener( eMat );
				hT = pSetting->activeTechniqueHandle();
				eMat->hTechnique( hT, i );
			}
			else if (this->getFirstValidTechnique(pEffects_[i].first, hT))
			{
				eMat->hTechniqueInternal( hT, i );
			}
			else
			{
				// device is valid, but there is 
				// no valid technique for the card
				ERROR_MSG( 
					"Moo::EffectMaterial::finishInit - "
					"No valid technique found\n" );
				initComplete_ = false;
				break;
			}
		}
		else
		{
			// device is not valid,
			// try again later.
			initComplete_ = false;
			break;
		}
	}
}


bool EffectMaterial::getFirstValidTechnique(
	ManagedEffectPtr effect, D3DXHANDLE & hT) const
{
	D3DXHANDLE hTechnique = NULL;
	D3DXHANDLE lastValidTechniue = NULL;
	ID3DXEffect * d3dxeffect = effect->pEffect();
	while (d3dxeffect->FindNextValidTechnique(hTechnique, &hTechnique) == D3D_OK)
	{
		if (this->validateShaderVersion(d3dxeffect, hTechnique))
		{
			hT = hTechnique;
			break;
		}
		else if (hTechnique != NULL)
		{
			lastValidTechniue = hTechnique;
		}
	} 
	
	if (hT == NULL)
	{
		hT = lastValidTechniue;
		ERROR_MSG(
			"EffectMaterial::getFirstValidTechnique(): Could not find a "
			"technique matching shader version cap (material/effect: %s/%s)\n",
			this->identifier_.c_str(), 
			effect->resourceID().c_str() );
	}

	return hT != NULL;
}

bool EffectMaterial::validateShaderVersion(
	ID3DXEffect * d3dxeffect, 
	D3DXHANDLE    hTechnique) const
{
	int maxPSVersion = ManagedEffect::maxPSVersion(d3dxeffect, hTechnique);
	return maxPSVersion <= EffectManager::instance()->PSVersionCap();
}

/**
 *	This method sets up the initial constants and render states for rendering.
 *	@return true if successful.
 */
bool EffectMaterial::begin() const
{
	bool ret = false;

	if (!initComplete_)
	{
		finishInit();
		if (!initComplete_)
			return false;
	}
	
	for (uint32 i = 0; i < pEffects_.size(); i++)
	{
		ManagedEffectPtr pmEffect = pEffects_[i].first;
		D3DXHANDLE hTechnique = hTechniques_[i];
		if (!pmEffect.hasObject() || !hTechnique)
		{
			nPasses_[i] = 0;
			continue;
		}

		pmEffect->pEffect()->SetTechnique( hTechnique );
		pmEffect->setAutoConstants();
		ID3DXEffect* pEffect = pmEffect->pEffect();
		Properties::const_iterator it = properties_[i].begin();
		Properties::const_iterator end = properties_[i].end();
		while (it != end)
		{
			it->second->apply( pEffect, it->first );
			it++;
		}
		ret = SUCCEEDED(pEffect->Begin( &nPasses_[i], D3DXFX_DONOTSAVESTATE ));
	}
	return ret;
}

bool EffectMaterial::commitChanges() const
{
	bool ret = false;

	for (uint32 i = 0; i < pEffects_.size(); i++)
	{
		ret = SUCCEEDED(pEffects_[i].first->pEffect()->CommitChanges());
	}
	return ret;
}

/**
 *	This method cleans up after rendering with this material.
 *	@return true if successful
 */
bool EffectMaterial::end() const
{
	bool ret = false;
	for (uint32 i = 0; i < pEffects_.size(); i++)
	{
		if (nPasses_[i] != 0)
		{
			ret = SUCCEEDED(pEffects_[i].first->pEffect()->End());
		}
	}
	return ret;
}

/**
 *	This method sets up rendering for a specific pass
 *	@param pass the pass to render
 *	@return true if successful
 */
bool EffectMaterial::beginPass( uint32 pass ) const
{
	bool ret = false;

	for (uint32 i = 0; i < pEffects_.size(); i++)
	{
		if (nPasses_[i] != 0)
			ret = SUCCEEDED(pEffects_[i].first->pEffect()->BeginPass( 
				pass < nPasses_[i] ? pass : nPasses_[i] - 1  ));
	}
	return ret;
}

/**
 *	This method records all the states for the pass, note that the
 *	returned object is only valid until the end of the current 
 *	rendering loop.
 *	@param pass the pass to record
 *	@return the recorded states
 */
StateRecorder* EffectMaterial::recordPass( uint32 pass ) const
{
	StateRecorder* pStateRecorder = StateRecorder::get();
	pStateRecorder->init();
	for (uint32 i = 0; i < pEffects_.size(); i++)
	{
		ID3DXEffect* pEffect = pEffects_[i].first->pEffect();
		ID3DXEffectStateManager* pManager = NULL;
		pEffect->GetStateManager( &pManager );
		pEffect->SetStateManager( pStateRecorder );
		if (nPasses_[i] != 0)
		{
			pEffect->BeginPass( pass < nPasses_[i] ? pass : nPasses_[i] - 1 );
		}
		pEffect->EndPass();
		pEffect->SetStateManager( pManager );
		pManager->Release();
	}
	return pStateRecorder;

}

/**
 *	This method ends the current pass
 *	@return true if successful
 */
bool EffectMaterial::endPass() const
{
	for (uint32 i = 0; i < pEffects_.size(); i++)
	{
		if (nPasses_[i] != 0)
			pEffects_[i].first->pEffect()->EndPass();
	}
	return true;
}

/**
 *	This method loads an EffectMaterial from a datasection and sets up the tweakables.
 *	@param pSection the datasection to load the material from
 *	@return true if successful
 */
bool EffectMaterial::load( DataSectionPtr pSection, bool addDefault /*= true*/ )
{
	bool ret = false;
	MF_ASSERT( pSection.hasObject() );

	EffectPropertyMappings effectProperties;

	pEffects_.clear();
	nPasses_.clear();
	hTechniques_.clear();
	properties_.clear();
	
	//Do recursive loading of material
	this->loadInternal( pSection, effectProperties );

	if (pEffects_.size())
	{
		if(addDefault)
		{
			for (uint32 i = 0; i < pEffects_.size(); i++)
			{
				ManagedEffectPtr pEffect = pEffects_[i].first;

				// get the defaults for tweakables that have not already been overridden.
				const EffectPropertyMappings& defaultProperties = pEffect->defaultProperties();
				EffectPropertyMappings::const_iterator dit = defaultProperties.begin();
				EffectPropertyMappings::const_iterator dend = defaultProperties.end();
				while (dit != dend)
				{
					EffectPropertyMappings::iterator override = effectProperties.find( dit->first );
					if (override == effectProperties.end())
					{
						effectProperties.insert( *dit );
					}
					dit++;
				}
			}
		}
		ret = true;

		// Map the effect property names to the right d3dx handles.
		EffectPropertyMappings::iterator it = effectProperties.begin();
		EffectPropertyMappings::iterator end = effectProperties.end();
		while (it != end)
		{
			bool found = false;
			for (uint32 i = 0; i < pEffects_.size(); i++)
			{
				ID3DXEffect* pEffect = pEffects_[i].first->pEffect();
				Properties& properties = properties_[i];
				D3DXHANDLE h = pEffect->GetParameterByName( NULL, it->first.c_str() );
				if (h != NULL)
				{
					it->second->attach( h, pEffect );
//					properties.insert( std::make_pair( h, it->second ) );
					properties[ h ] = it->second;
					found = true;
				}
			}
			
			if (found == false)
			{
				if (pEffects_.size() == 1)
				{
					NOTICE_MSG( 
						"EffectMaterial::load - no parameter \"%s\" found in .fx file \"%s\"\n",
						it->first.c_str(), pEffects_[0].first->resourceID().c_str());
				}
				else
				{
					NOTICE_MSG( 
						"EffectMaterial::load - no parameter \"%s\" "
						"found in .fx files:\n", it->first.c_str());
						
					for (uint32 i = 0; i < pEffects_.size(); i++)
					{
						NOTICE_MSG("   %s\n", 
							pEffects_[i].first->resourceID().c_str());
					}
				}
			}
			it++;
		}


		// We still need to init the techniques, this can only be done in the main thread,
		// so we will wait until the effect gets used.
		initComplete_ = false;
		if (g_renderThread)
		{
			finishInit();
		}
		else
		{
			SimpleMutexHolder mutex( EffectMaterial::effectInitQueueMutex );
			
			effectInitQueue.push_back( this );
		}
	}
	return ret;
}

/*
 * This method does the recursive loading of the tweakables etc.
 */
bool EffectMaterial::loadInternal( DataSectionPtr pSection, EffectPropertyMappings& outProperties )
{
	bool ret = true;
	
	MF_ASSERT( pSection.hasObject() );

	// get the identifier if we don't have one already.
	if (!identifier_.length())
		identifier_ = pSection->readString( "identifier" );

	// open the effect itself

	std::vector< std::string > fxNames;
	pSection->readStrings( "fx", fxNames );

//	if (fxNames.size() > 1)
//	{
//		INFO_MSG( "EffectMaterial::loadInternal - found multiple .fx files in %s\n", pSection->sectionName().c_str() );
//	}

	if (!pEffects_.size() && fxNames.size())
	{
		pEffects_.clear();
		nPasses_.clear();
		hTechniques_.clear();
		properties_.clear();
		for (uint32 i = 0; i < fxNames.size();i++)
		{
			if (!this->loadEffect( fxNames[i] ))
			{
				ERROR_MSG( "EffectMaterial::loadInternal - unable to open .fx file %s\n", fxNames[i].c_str());
				ret = false;
			}
			nPasses_.push_back(0);
			hTechniques_.push_back(0);
			properties_.push_back( Properties() );
		}
	}
	
	// get the channel if it has been overridden in the material section
	if (!channelOverridden_)
	{
		std::string channel = pSection->readString("channel");
		if (channel.length())
		{
			channel_ = VisualChannel::get( channel );
			channelOverridden_ = true;
		}
	}
	
	// load another material file if we are inheriting
	DataSectionPtr pMFMSect = pSection->openSection( "mfm" );
	if(pMFMSect)
	{
		DataSectionPtr pBaseSection = BWResource::instance().openSection( pMFMSect->asString() );
		if (pBaseSection)
		{
			ret = loadInternal( pBaseSection, outProperties );
		}
	}
	
	// grab the mfm saved tweakable properties
	std::vector<DataSectionPtr> dataSections;
	pSection->openSections( "property", dataSections );
	while (dataSections.size())
	{
		DataSectionPtr pSect = dataSections.back();
		dataSections.pop_back();
		DataSectionPtr pChild = pSect->openChild( 0 );
		if (pChild)
		{
			PropertyFunctors::iterator it = g_effectPropertyProcessors.find( pChild->sectionName() );
			if (it != g_effectPropertyProcessors.end())
			{
				outProperties[pSect->asString()] = it->second->create( pChild );
			}
            else
            {
                DEBUG_MSG( "Could not find property processor for mfm-specified property %s\n", pChild->sectionName().c_str() );
            }
		}
	}

	// grab the editor only properties
	materialKind_ = pSection->readInt( "materialKind", materialKind_ );
	collisionFlags_ = pSection->readInt( "collisionFlags", collisionFlags_ );
	return ret;
}


/**
 * This method loads the actual effect.
 */
bool EffectMaterial::loadEffect( const std::string& effectResource )
{
	ManagedEffectPtr pEffect = EffectManager::get( effectResource );
	if (pEffect.hasObject())
	{
		// register as listener if effect 
		// is flagged as an engine feature
		int listenerId = -1;
		pEffects_.push_back(std::make_pair(pEffect, listenerId));
		
	}
	return pEffect.hasObject();
}


void EffectMaterial::onSelectTechnique(ManagedEffect * effect, D3DXHANDLE hTec)
{
	typedef std::vector<EffectIntPair>::const_iterator citerator;
	citerator effBegin = this->pEffects_.begin();
	citerator effEnd   = this->pEffects_.end();
	citerator effIt    = effBegin;
	while (effIt != effEnd)
	{
		if (effIt->first == ManagedEffectPtr(effect))
		{
			break;
		}
		++effIt;
	}
	
	MF_ASSERT(effIt != effEnd);
	this->hTechnique(hTec, std::distance(effBegin, effIt));
}


void EffectMaterial::onSelectPSVersionCap(int psVerCap)
{
	if (!this->techniqueExplicitlySet_)
	{			
		this->initComplete_ = false;
		this->finishInit();
	}
}

/**
 *	This method sets the current technique and selects the appropriate visual channel for the material.
 *	@param hTec handle to the technique to use.
 *	@param effectIndex index of the effect to set it on
 *	@return true if successful
 */
bool EffectMaterial::hTechnique( D3DXHANDLE hTec, uint32 effectIndex )
{
	this->techniqueExplicitlySet_ = true;
	return this->hTechniqueInternal(hTec, effectIndex);
}


bool EffectMaterial::hTechniqueInternal( D3DXHANDLE hTec, uint32 effectIndex )
{
	if (!initComplete_)
		finishInit();
		
	bool res = false;
	ID3DXEffect* pEffect = pEffects_[effectIndex].first->pEffect();
	D3DXTECHNIQUE_DESC techniqueDesc;

	// see if the technique exists
	if (SUCCEEDED(pEffect->GetTechniqueDesc( hTec, &techniqueDesc )))
	{
		hTechniques_[effectIndex] = hTec;
		res = true;
		if (!channelOverridden_)
			channel_ = NULL;
		bumpMapped_ = false;
		skinned_ = false;

		for (uint32 i = 0; i < pEffects_.size(); i++)
		{
			pEffect = pEffects_[i].first->pEffect();
			D3DXHANDLE hTechnique = hTechniques_[i];
			if (hTechnique == 0)
				continue;


			// if the visual channel has not been overridden from an mfm, get the channel from the technique
			if (!channelOverridden_)
			{
				D3DXHANDLE h = pEffect->GetAnnotationByName( hTechnique, "channel" );
				if (h)
				{
					LPCSTR s;
					if (SUCCEEDED(pEffect->GetString( h, &s )))
					{
						channel_ = VisualChannel::get( s );
					}
				}
				else
				{
					channel_ = NULL;
				}
			}

			// If any of the effects used by the visual material have the skinned annotation,
			// assume the material is skinned.
			if (!skinned_)
			{
				D3DXHANDLE h = pEffect->GetAnnotationByName( hTechnique, "skinned" );
				if (h)
				{
					BOOL b;
					if (SUCCEEDED(pEffect->GetBool( h, &b )))
					{
						skinned_ = b == TRUE;
					}
					else 
					{
						skinned_ = false;
					}
				}
				else
				{
					skinned_ = false;
				}
			}

			// If any of the effects used by the visual material have the bumpmapped annotation,
			// assume the material is bump mapped.
			if (!bumpMapped_)
			{
				D3DXHANDLE h = pEffect->GetAnnotationByName( hTechnique, "bumpMapped" );
				if (h)
				{
					BOOL b;
					if (SUCCEEDED(pEffect->GetBool( h, &b )))
					{
						bumpMapped_ = b == TRUE;
					}
					else 
					{
						bumpMapped_ = false;
					}
				}
				else
				{
					bumpMapped_ = false;
				}
			}
		}

	}
	else
	{
		ERROR_MSG( "EffectMaterial::hTechnique - Invalid technique handle passed\n" );
	}
	return res;
}

bool EffectMaterial::initFromEffect( const std::string& effect, const std::string& diffuseMap /* = "" */, int doubleSided /* = -1 */ )
{
	DataSectionPtr pSection = new XMLSection( "material" );
	if (pSection)
	{
		pSection->writeString( "fx", effect );

		if (diffuseMap != "")
		{
			DataSectionPtr pSect = pSection->newSection( "property" );
			pSect->setString( "diffuseMap" );
			pSect->writeString( "Texture", diffuseMap );
		}

		if (doubleSided != -1)
		{
			DataSectionPtr pSect = pSection->newSection( "property" );
			pSect->setString( "doubleSided" );
			pSect->writeBool( "Bool", !!doubleSided );			
		}

		return this->load( pSection );
	}

	return false;
}

/**
 *	Assigment operator.
 */
EffectMaterial & EffectMaterial::operator=( const EffectMaterial & other )
{
	SimpleMutexHolder mutex( EffectMaterial::effectInitQueueMutex );

	
	nPasses_                = other.nPasses_;
	hTechniques_            = other.hTechniques_;
	pEffects_               = other.pEffects_;
	properties_             = other.properties_;
	identifier_             = other.identifier_;
	channel_                = other.channel_;
	channelOverridden_      = other.channelOverridden_;
	skinned_                = other.skinned_;
	bumpMapped_             = other.bumpMapped_;
	initComplete_           = other.initComplete_;
	techniqueExplicitlySet_ = other.techniqueExplicitlySet_;
	materialKind_			= other.materialKind_;
	collisionFlags_			= other.collisionFlags_;

	std::vector<EffectIntPair>::iterator effIt  = this->pEffects_.begin();
	std::vector<EffectIntPair>::iterator effEnd = this->pEffects_.end();
	while (effIt != effEnd)
	{
		if (effIt->first->graphicsSettingEntry() != 0)
		{
			effIt->second = effIt->first->graphicsSettingEntry()->addListener(this);
		}
		++effIt;
	}
	
	if( !this->initComplete_ )
		effectInitQueue.push_back( this );

	return *this;
}

/**
 *	Retrieves value of the first property with the specified name.
 *	This is not the value in the DirectX effect but the  Moo::EffectProperty
 *	If the property is a integer the value is copied into result.
 *	@param result The int into which the result will be placed
 *	@param name The name of the property to read.
 *	@return Returns true if the property’s value was successfully retrieved.
 */
bool EffectMaterial::boolProperty( bool & result, const std::string & name ) const
{
	EffectPropertyPtr effectProperty = this->getProperty( name );
	if( effectProperty )
		return effectProperty->getBool( result );
	return false;
}

/**
 *	Retrieves value of the first property with the specified name.
 *	This is not the value in the DirectX effect but the  Moo::EffectProperty
 *	If the property is a integer the value is copied into result.
 *	@param result The int into which the result will be placed
 *	@param name The name of the property to read.
 *	@return Returns true if the property’s value was successfully retrieved.
 */
bool EffectMaterial::intProperty( int & result, const std::string & name ) const
{
	EffectPropertyPtr effectProperty = this->getProperty( name );
	if( effectProperty )
		return effectProperty->getInt( result );
	return false;
}

/**
 *	Retrieves value of the first property with the specified name.
 *	This is not the value in the DirectX effect but the  Moo::EffectProperty
 *	If the property is a float the value is copied into result.
 *	@param result The float into which the result will be placed
 *	@param name The name of the property to read.
 *	@return Returns true if the property’s value was successfully retrieved.
 */
bool EffectMaterial::floatProperty( float & result, const std::string & name ) const
{
	EffectPropertyPtr effectProperty = this->getProperty( name );
	if( effectProperty )
		return effectProperty->getFloat( result );
	return false;
}

/**
 *	Retrieves value of the first property with the specified name.
 *	This is not the value in the DirectX effect but the  Moo::EffectProperty
 *	If the property is a vector the value is copied into result.
 *	@param result The vector in which the result will be placed
 *	@param name The name of the property to read.
 *	@return Returns true if the property’s value was successfully retrieved.
 */
bool EffectMaterial::vectorProperty( Vector4 & result, const std::string & name ) const
{
	EffectPropertyPtr effectProperty = this->getProperty( name );
	if( effectProperty )
		return effectProperty->getVector( result );
	return false;
}

/**
 *	Retrieves the first Moo::EffectProperty with the specified name.
 *	@param name The name of the property to read.
 *	@return SmartPointer to the property. NULL if not found.
 */
EffectPropertyPtr EffectMaterial::getProperty( const std::string & name )
{
	for( uint iEffect = 0; iEffect < properties_.size(); iEffect++ )
	{
		ID3DXEffect* dxEffect = pEffects_[ iEffect].first->pEffect();
		if( !dxEffect )
			continue;

		for(Properties::iterator iProperty = properties_[iEffect].begin();
			iProperty != properties_[iEffect].end();
			++iProperty )
		{
			D3DXPARAMETER_DESC description;

			if( !SUCCEEDED( dxEffect->GetParameterDesc( iProperty->first, &description ) ) )
				continue;

			if( name == description.Name )
			{
				return iProperty->second;
			}
		}
	}

	return NULL;
}

/**
 *	Retrieves the first Moo::EffectProperty with the specified name.
 *	@param name The name of the property to read.
 *	@return ConstSmartPointer to the property. NULL if not found.
 */
ConstEffectPropertyPtr EffectMaterial::getProperty( const std::string & name ) const
{
	return const_cast<EffectMaterial*>( this )->getProperty( name );
}


/**
 *	This function replaces an existing effect property with the one given.
 *	@param name				The name of the property to replace.
 *	@param effectProperty	SmartPointer to the property to replace the existing.
 *	@return					Returns true if the replace succeeded.
 */
bool EffectMaterial::replaceProperty( const std::string & name, EffectPropertyPtr effectProperty )
{
	if (!effectProperty)
	{
		MF_ASSERT( !"Null pointer passed to EffectMaterial::replaceProperty()" );
		return false;
	}

	for( uint iEffect = 0; iEffect < properties_.size(); iEffect++ )
	{
		ID3DXEffect* dxEffect = pEffects_[ iEffect].first->pEffect();
		if( !dxEffect )
			continue;

		for(Properties::iterator iProperty = properties_[iEffect].begin();
			iProperty != properties_[iEffect].end();
			++iProperty )
		{
			D3DXPARAMETER_DESC description;

			if( !SUCCEEDED( dxEffect->GetParameterDesc( iProperty->first, &description ) ) )
				continue;

			if( name == description.Name )
			{
				iProperty->second = effectProperty;
				return true;
			}
		}
	}
	return false;
}


/**
 *	Runs finishInit on all un-initialised effect materials in a thread safe 
 *  manner. This function should only be called from the render thread.
 */
void EffectMaterial::finishEffectInits()
{
	MF_ASSERT( g_renderThread == true );

	SimpleMutexHolder mutex( EffectMaterial::effectInitQueueMutex );

	while( !effectInitQueue.empty() )
	{
		if ( !effectInitQueue.front()->initComplete_ )
		{
			effectInitQueue.front()->finishInit();
		}
		effectInitQueue.pop_front();
	}
}



#ifdef EDITOR_ENABLED
/**
 *  This method returns true if the given property is a default property.
 */
bool EffectMaterial::isDefault( EffectPropertyPtr pProperty, uint32 effectIndex )
{
	const EffectPropertyMappings& defaultProperties = pEffects_[effectIndex].first->defaultProperties();
	EffectPropertyMappings::const_iterator dit = defaultProperties.begin();
	EffectPropertyMappings::const_iterator dend = defaultProperties.end();
	while (dit != dend)
	{
		if ( dit->second == pProperty )
			return true;	
		dit++;
	}
	return false;
}

/**
 *  This method finds all shared default properties in the material, and
 *  replaces them with instanced properties.  This allows editing of
 *  properties without affecting other materials that use the same effect.
 */
void EffectMaterial::replaceDefaults()
{
	for (uint32 i = 0; i < pEffects_.size(); i++)
	{
		std::vector< std::pair<D3DXHANDLE,EffectPropertyPtr> > props;

		Properties::iterator it = properties_[i].begin();
		Properties::iterator end = properties_[i].end();
		while (it != end)
		{
			D3DXHANDLE propertyHandle = it->first;
			EffectPropertyPtr pProp = it->second;

			if ( this->isDefault(pProp, i) )
			{
                pProp->apply( pEffects_[i].first->pEffect(),propertyHandle );
				D3DXPARAMETER_DESC desc;
				pEffects_[i].first->pEffect()->GetParameterDesc( propertyHandle, &desc );

				//create a new property of the correct type
				PropertyFunctors::iterator fit = g_effectPropertyProcessors.begin();
				PropertyFunctors::iterator fend = g_effectPropertyProcessors.end();
				while ( fit != fend )
				{
					EffectPropertyFunctor* f = fit->second;
					if ( f->check( &desc ) )
					{
						pProp = f->create(propertyHandle, pEffects_[i].first->pEffect());
						break;
					}
					else
					{
						fit++;
					}
				}

				if ( !pProp )
				{
					WARNING_MSG( "Could not create a copy of an effect property\n" );
					return;
				}
			}

			props.push_back( std::make_pair(propertyHandle,pProp) );
			it++;
		}

		//reset the property list
        properties_[i].clear();
		for ( uint32 j=0; j<props.size(); j++ )
		{
			properties_[i].insert( props[j] );
		}
	}
}

#endif //EDITOR_ENABLED

}

// effect_material.cpp
