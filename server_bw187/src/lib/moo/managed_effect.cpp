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
#include "managed_effect.hpp"
#include "resmgr/multi_file_system.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/bin_section.hpp"
#include "resmgr/auto_config.hpp"
#include "resmgr/data_section_census.hpp"
#include "texture_manager.hpp"
#include <d3dx9.h>
#include "effect_state_manager.hpp"

DECLARE_DEBUG_COMPONENT2( "Moo", 0 );


#ifdef D3DXSHADER_USE_LEGACY_D3DX9_31_DLL
#define USE_LEGACY_D3DX9_DLL D3DXSHADER_USE_LEGACY_D3DX9_31_DLL
#else  // D3DXSHADER_USE_LEGACY_D3DX9_31_DLL
#define USE_LEGACY_D3DX9_DLL 0
#endif // D3DXSHADER_USE_LEGACY_D3DX9_31_DLL

namespace Moo
{

namespace { // anonymous

// Named constants
AutoConfigString s_shaderIncludePath( "system/shaderIncludePaths" );
const int c_MaxPixelShaderVersion = 3;

// Helper functions
EffectMacroSetting::MacroSettingVector & static_settings();

//TODO: combined D3D error message handling...
void printErrorMsg_( HRESULT hr, const std::string& format )
{	
	switch ( hr )
	{
		case D3DERR_DEVICELOST:
			ERROR_MSG( (format + "The device was lost and cannot be reset at this time.\n").c_str() );
			break;
		case D3DERR_INVALIDCALL:
			ERROR_MSG( (format + "Invalid call, one or more parameters are wrong.\n").c_str() );
			break;
		case D3DERR_NOTAVAILABLE:
			ERROR_MSG( (format + "This device does not support the queried technique.\n").c_str() );
			break;
		case D3DERR_OUTOFVIDEOMEMORY:
			ERROR_MSG( (format + "Out of video memory.\n").c_str() );
			break;
		case D3DERR_DRIVERINTERNALERROR:
			ERROR_MSG( (format + "Driver internal error.\n").c_str() );
			break;
		case E_OUTOFMEMORY:
			ERROR_MSG( (format + "Out of memory.\n").c_str() );
			break;
		case D3DXERR_INVALIDDATA:
			ERROR_MSG( (format + "D3DXERR_INVALIDDATA.\n").c_str() );
			break;
		default:
			ERROR_MSG( (format + "Unknown DirectX Error #%d\n").c_str(), hr );
	}
}

/**
 * This function dumps all techniques of an effect, listing their valid state
 * and all available passes. 
 */
void spamEffectInfo( ManagedEffectPtr effect )
{
	ID3DXEffect*		e = effect->pEffect();
	const char*			en= effect->resourceID().c_str();
	D3DXEFFECT_DESC		ed;

	TRACE_MSG("Effect:%s\n", en );

	if ( SUCCEEDED(e->GetDesc( &ed ) ) )
	{
		for ( uint i = 0; i < ed.Techniques; i++ )
		{
			TRACE_MSG( " Technique %d/%d:", i+1, ed.Techniques );

			D3DXHANDLE th = e->GetTechnique(i);
			if ( th )
			{
				D3DXTECHNIQUE_DESC td;

				if ( SUCCEEDED( e->GetTechniqueDesc( th, &td )) )
				{
					bool v = SUCCEEDED( e->ValidateTechnique( th ) );

					TRACE_MSG( "%s (%s)\n", td.Name,
						v ? "valid" : "invalid" );

					for ( uint j = 0; j < td.Passes; j++ )
					{
						TRACE_MSG("  Pass %d/%d:", j+1, td.Passes );

						D3DXHANDLE ph = e->GetPass( th, j );
						if ( ph )
						{
							D3DXPASS_DESC pd;

							if ( SUCCEEDED( e->GetPassDesc( ph, &pd )))
							{
								TRACE_MSG("%s\n", pd.Name );
							}
						}
						else
						{
							TRACE_MSG("Can't get pass handle.\n");
						}
					}
				}
				else
				{
					TRACE_MSG( "Can't get technique description.\n" );
				}
			}
			else
			{
				TRACE_MSG( "Can't get technique handle.\n" );
			}
		}
	}
	else
	{
		TRACE_MSG("Can't get effect description.\n" );
	}
}

} // namespace anonymous

class Vector4Property : public EffectProperty
{
public:
	bool apply( ID3DXEffect* pEffect, D3DXHANDLE hProperty )
	{
		return SUCCEEDED( pEffect->SetVector( hProperty, &value_ ) );
	}
	void be( const Vector4 & v ) { value_ = v; }

	bool getVector( Vector4 & v ) const { v = value_; return true; };

	void value( const Vector4& value ) { value_ = value; }
	const Vector4& value() const { return value_; }
protected:
	Vector4 value_;
};

class Vector4PropertyFunctor : public EffectPropertyFunctor
{
public:
	virtual EffectPropertyPtr create( DataSectionPtr pSection )
	{
		Vector4Property* prop = new Vector4Property;
		prop->value( pSection->asVector4() );
		return prop;
	}
	virtual EffectPropertyPtr create( D3DXHANDLE hProperty, ID3DXEffect* pEffect )
	{
		Vector4Property* prop = new Vector4Property;
		Vector4 v;
        pEffect->GetVector( hProperty, &v );
		prop->value( v );
		return prop;
	}
	virtual bool check( D3DXPARAMETER_DESC* propertyDesc )
	{
		return (propertyDesc->Class == D3DXPC_VECTOR &&
			propertyDesc->Type == D3DXPT_FLOAT);
	}
private:
};

class MatrixProperty : public EffectProperty, public Aligned
{
public:
	bool apply( ID3DXEffect* pEffect, D3DXHANDLE hProperty )
	{
		return SUCCEEDED( pEffect->SetMatrix( hProperty, &value_ ) );
	}
	void be( const Vector4 & v )
		{ value_.translation( Vector3( v.x, v.y, v.z ) ); /*hmmm*/ }

	void value( Matrix& value ) { value_ = value; };
	const Matrix& value() const { return value_; }
protected:
	Matrix value_;
};

class MatrixPropertyFunctor : public EffectPropertyFunctor
{
public:
	virtual EffectPropertyPtr create( DataSectionPtr pSection )
	{
		MatrixProperty* prop = new MatrixProperty;
		Matrix m;
		m.row(0) = pSection->readVector4( "row0" );
		m.row(1) = pSection->readVector4( "row1" );
		m.row(2) = pSection->readVector4( "row2" );
		m.row(3) = pSection->readVector4( "row3" );
		prop->value( m );
		return prop;
	}
	virtual EffectPropertyPtr create( D3DXHANDLE hProperty, ID3DXEffect* pEffect )
	{
		MatrixProperty* prop = new MatrixProperty;
		Matrix m;
        pEffect->GetMatrix( hProperty, &m );
		prop->value( m );
		return prop;
	}
	virtual bool check( D3DXPARAMETER_DESC* propertyDesc )
	{
		return ((propertyDesc->Class == D3DXPC_MATRIX_ROWS ||
            propertyDesc->Class == D3DXPC_MATRIX_COLUMNS) &&
			propertyDesc->Type == D3DXPT_FLOAT);
	}
private:
};

class TextureProperty : public EffectProperty
{
public:
	bool apply( ID3DXEffect* pEffect, D3DXHANDLE hProperty )
	{
		if (!value_.hasObject())
			return SUCCEEDED( pEffect->SetTexture( hProperty, NULL ) );
		return SUCCEEDED( pEffect->SetTexture( hProperty, value_->pTexture() ) );
	}
	void be( const Vector4 & v ) { }
	void be( const BaseTexturePtr pTex ) { value_ = pTex; }

	void value( BaseTexturePtr value ) { value_ = value; };
	const BaseTexturePtr value() const { return value_; }

	bool getResourceID( std::string & s ) const { s = value_ ? value_->resourceID() : ""; return true; };
protected:
	BaseTexturePtr value_;
};

class TexturePropertyFunctor : public EffectPropertyFunctor
{
public:
	virtual EffectPropertyPtr create( DataSectionPtr pSection )
	{
		TextureProperty* prop = new TextureProperty;
		BaseTexturePtr pTexture = TextureManager::instance()->get( pSection->asString() );
		prop->value( pTexture );
		return prop;
	}
	virtual EffectPropertyPtr create( D3DXHANDLE hProperty, ID3DXEffect* pEffect )
	{
		TextureProperty* prop = new TextureProperty;
		return prop;
	}
	virtual bool check( D3DXPARAMETER_DESC* propertyDesc )
	{
		return (propertyDesc->Class == D3DXPC_OBJECT &&
			(propertyDesc->Type == D3DXPT_TEXTURE ||
			propertyDesc->Type == D3DXPT_TEXTURE1D ||
			propertyDesc->Type == D3DXPT_TEXTURE2D ||
			propertyDesc->Type == D3DXPT_TEXTURE3D ||
			propertyDesc->Type == D3DXPT_TEXTURECUBE ));
	}
private:
};

class FloatProperty : public EffectProperty
{
public:
	bool apply( ID3DXEffect* pEffect, D3DXHANDLE hProperty )
	{
		return SUCCEEDED( pEffect->SetFloat( hProperty, value_ ) );
	}
	void be( const Vector4 & v ) { value_ = v.x; }

	bool getFloat( float & f ) const { f = value_; return true; };

	void value( float value ) { value_ = value; };
	float value() const { return value_; }
protected:
	float value_;
};

class FloatPropertyFunctor : public EffectPropertyFunctor
{
public:
	virtual EffectPropertyPtr create( DataSectionPtr pSection )
	{
		FloatProperty* prop = new FloatProperty;
		prop->value( pSection->asFloat() );
		return prop;
	}
	virtual EffectPropertyPtr create( D3DXHANDLE hProperty, ID3DXEffect* pEffect )
	{
		FloatProperty* prop = new FloatProperty;
		float v;
        pEffect->GetFloat( hProperty, &v );
		prop->value( v );
		return prop;
	}
	virtual bool check( D3DXPARAMETER_DESC* propertyDesc )
	{
		return (propertyDesc->Class == D3DXPC_SCALAR &&
			propertyDesc->Type == D3DXPT_FLOAT);
	}
private:
};

class BoolProperty : public EffectProperty
{
public:
	bool apply( ID3DXEffect* pEffect, D3DXHANDLE hProperty )
	{
		return SUCCEEDED( pEffect->SetBool( hProperty, value_ ) );
	}
	void be( const Vector4 & v ) { value_ = (v.x != 0); }

	bool getBool( bool & b ) const { b = ( value_ != 0 ); return true; };

	void value( BOOL value ) { value_ = value; };
	BOOL value() const { return value_; }
protected:
	BOOL value_;
};

class BoolPropertyFunctor : public EffectPropertyFunctor
{
public:
	virtual EffectPropertyPtr create( DataSectionPtr pSection )
	{
		BoolProperty* prop = new BoolProperty;
		prop->value( pSection->asBool() );
		return prop;
	}
	virtual EffectPropertyPtr create( D3DXHANDLE hProperty, ID3DXEffect* pEffect )
	{
		BoolProperty* prop = new BoolProperty;
		BOOL v;
        pEffect->GetBool( hProperty, &v );
		prop->value( v );
		return prop;
	}
	virtual bool check( D3DXPARAMETER_DESC* propertyDesc )
	{
		return (propertyDesc->Class == D3DXPC_SCALAR &&
			propertyDesc->Type == D3DXPT_BOOL);
	}
private:
};

class IntProperty : public EffectProperty
{
public:
	bool apply( ID3DXEffect* pEffect, D3DXHANDLE hProperty )
	{
		return SUCCEEDED( pEffect->SetInt( hProperty, value_ ) );
	}
	void be( const Vector4 & v ) { value_ = int(v.x); }

	bool getInt( int & i ) const { i = value_; return true; };

	void value( int value ) { value_ = value; };
	int value() const { return value_; }
protected:
	int value_;
};

class IntPropertyFunctor : public EffectPropertyFunctor
{
public:
	virtual EffectPropertyPtr create( DataSectionPtr pSection )
	{
		IntProperty* prop = new IntProperty;
		prop->value( pSection->asInt() );
		return prop;
	}
	virtual EffectPropertyPtr create( D3DXHANDLE hProperty, ID3DXEffect* pEffect )
	{
		IntProperty* prop = new IntProperty;
		int v;
        pEffect->GetInt( hProperty, &v );
		prop->value( v );
		return prop;
	}
	virtual bool check( D3DXPARAMETER_DESC* propertyDesc )
	{
		return (propertyDesc->Class == D3DXPC_SCALAR &&
			propertyDesc->Type == D3DXPT_INT );
	}
private:
};

PropertyFunctors g_effectPropertyProcessors;

bool setupTheseThings()
{
#define EP(x) g_effectPropertyProcessors[#x] = new x##PropertyFunctor
	EP(Vector4);
	EP(Matrix);
	EP(Float);
	EP(Bool);
	EP(Texture);
	EP(Int);

	return true;
}

static bool blah = setupTheseThings();


/*
 *	Helper function to see if a parameter is artist editable
 */
static bool artistEditable( ID3DXEffect* pEffect, D3DXHANDLE hProperty )
{
	BOOL artistEditable = FALSE;
	D3DXHANDLE hAnnot = pEffect->GetAnnotationByName( hProperty, "artistEditable" );
	if (!hAnnot)
		hAnnot = pEffect->GetAnnotationByName( hProperty, "worldBuilderEditable" );
	if (hAnnot)
		pEffect->GetBool( hAnnot, &artistEditable );	

	hAnnot = pEffect->GetAnnotationByName( hProperty, "UIName" );
	return (artistEditable == TRUE) || (hAnnot != 0);
}


// -----------------------------------------------------------------------------
// Section: ManagedEffect
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
ManagedEffect::ManagedEffect():
	resourceID_( "Not loaded" ),
	techniques_(),
	settingName_(),
	settingEntry_(0),
	settingsAdded_(false)
{
}


/**
 *	Destructor.
 */
ManagedEffect::~ManagedEffect()
{
	EffectManager::del( this );
}

/**	
 *	This method sets the automatic constants 
 */
void ManagedEffect::setAutoConstants()
{
	MappedConstants::iterator it = mappedConstants_.begin();
	MappedConstants::iterator end = mappedConstants_.end();
	while (it != end)
	{
		EffectConstantValue* pValue = (*it->second).getObject();
		if (pValue)
			(*pValue)( pEffect_.pComObject(), it->first );
		it++;
	}
}

void ManagedEffect::recordAutoConstants( RecordedEffectConstants& recordedList )
{
	MappedConstants::iterator it = mappedConstants_.begin();
	MappedConstants::iterator end = mappedConstants_.end();
	while (it != end)
	{
		EffectConstantValue* pValue = (*it->second).getObject();
		if (pValue)
		{
			RecordedEffectConstant* pRecorded = 
				pValue->record( pEffect_.pComObject(), it->first );
			if (pRecorded)
				recordedList.push_back( pRecorded );
		}
		it++;
	}
}

/**
 *	Returns the graphics setting entry for this effect, if it has 
 *	been registered as such. Othorwise, returns an empty pointer.
 */
EffectTechniqueSetting * ManagedEffect::graphicsSettingEntry()
{ 
	if (!settingsAdded_)
			this->registerGraphicsSettings( this->resourceID_ );
	return this->settingEntry_.getObject(); 
}

/**
 *	Retrieves the maximun pixel shader version used by 
 *	all passes in the provided technique.
 *
 *	@param	hTechnique	technique to be queried.
 *	@return				major shader version number.
 */
int ManagedEffect::maxPSVersion(D3DXHANDLE hTechnique)
{
	return ManagedEffect::maxPSVersion(pEffect_.pComObject(), hTechnique);
}
	
/**
 *	Retrieves the maximum pixel shader version used by all passes 
 *	in the provided technique in the given effect object. (static)
 *
 *	@param	d3dxeffect	ID3DEffect object which contains the technique.
 *	@param	hTechnique	technique to be queried.
 *	@return				major shader version number.
 */
int ManagedEffect::maxPSVersion(
	ID3DXEffect * d3dxeffect, 
	D3DXHANDLE    hTechnique)
{
	int maxPSVersion = 10;	
	D3DXTECHNIQUE_DESC techniqueDesc;
	if (d3dxeffect->GetTechniqueDesc(hTechnique, &techniqueDesc) == D3D_OK)
	{
		maxPSVersion = 0;
		for (uint i=0; i<techniqueDesc.Passes; ++i)
		{
			D3DXPASS_DESC passDesc;
			D3DXHANDLE hPass = d3dxeffect->GetPass(hTechnique, i);
			if (d3dxeffect->GetPassDesc(hPass, &passDesc) == D3D_OK)
			{
				const DWORD * psFunction = passDesc.pPixelShaderFunction;
				int psVersion = (D3DXGetShaderVersion(psFunction) & 0xff00) >> 8;
				maxPSVersion  = std::max(maxPSVersion, psVersion);
			}
		}
	}
	return maxPSVersion;
}

/**
 *	This class implements the ID3DXInclude interface used for loading effect files
 *	and their includes.
 */
class EffectIncludes : public ID3DXInclude
{
public:
	EffectIncludes()
	{

	}
	~EffectIncludes()
	{

	}
	static EffectIncludes& instance()
	{
		static EffectIncludes instance;
		return instance;
	}

	/*
	 *	The overridden open method
	 */
	HRESULT __stdcall Open( D3DXINCLUDE_TYPE IncludeType, LPCSTR pFileName,
		LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes )
	{
		// Make sure our return values are initialised
		*ppData = NULL;
		*pBytes = 0;
		BinaryPtr pData;

//  Commented the 'dissolveFilename' line below as it seems it's not needed
//		std::string relativeFilename = BWResource::dissolveFilename( pFileName );
        DataSectionPtr pFile = BWResource::openSection( pFileName );
		if (pFile)
		{
//			DEBUG_MSG( "EffectIncludes::Open - opening %s\n", pFileName.c_str() );
			pData = pFile->asBinary();
		}
		else
		{
			std::string name = BWResource::getFilename( pFileName );
			std::string pathname = currentPath_ + name;
            pFile = BWResource::openSection( pathname );
			if (pFile)
			{
				pData = pFile->asBinary();
			}
			else
			{

				const EffectManager::IncludePaths& paths = EffectManager::instance()->includePaths();
				EffectManager::IncludePaths::const_iterator it = paths.begin();
				EffectManager::IncludePaths::const_iterator end = paths.end();
				while (it != end)
				{
					pathname = *(it++) + name;
                    pFile = BWResource::openSection( pathname );
					if (pFile)
					{
						pData = pFile->asBinary();
						it = end;
					}
				}
			}
		}

		if (pData)
		{
			*ppData = pData->data();
			*pBytes = pData->len();
			includes_.push_back( std::make_pair( *ppData, pData ) );

			includesNames_.push_back( pFileName );
		}
		else
		{
			ERROR_MSG( "EffectIncludes::Open - Failed to load %s\n", pFileName );
			return E_FAIL;
		}

		return S_OK;
	}
	HRESULT __stdcall Close( LPCVOID pData )
	{
		std::vector< IncludeFile >::iterator it = includes_.begin();
		std::vector< IncludeFile >::iterator end = includes_.end();
		while (it != end)
		{
			if ((*it).first == pData)
			{
				includes_.erase( it );
				break;
			}
			it++;
		}
		return S_OK;
	}
	void resetDependencies() { includesNames_.clear(); }
	std::list< std::string >& dependencies() { return includesNames_; }
	void currentPath( const std::string& currentPath ) { currentPath_ = currentPath; }
	const std::string& currentPath( ) const { return currentPath_; }
private:
	typedef std::pair<LPCVOID, BinaryPtr> IncludeFile;
	std::vector< IncludeFile > includes_;
	std::list< std::string > includesNames_;
	std::string currentPath_;
};


/**
 *	This method checks if the specified resource file is modified.
 *	@param objectName object file built from the resource file.
 *	@param resName resource file.
 *	@return true if the resource file or its includes has been updated and needs a recompile.
 */
bool EffectManager::checkModified( const std::string& objectName, const std::string& resName )
{
	//Check the .fxo for recompilation
	//1) check if the .fxo exists
	//2) check if it's newer than the .fx
	//3) check if it's newer than it's included files.

	uint64 objTime = BWResource::modifiedTime( objectName );
	bool recompile = (objTime == 0);
	if (!recompile)
	{
		uint64 resTime = BWResource::modifiedTime( resName );
		uint64 incTime = 0;

		DataSectionPtr pSection = BWResource::instance().rootSection()->openSection( objectName );
		if (pSection)
		{
			DataSectionPtr effect = pSection->openSection( "effect" );
			if (effect) //check if it's a new file...
			{
				std::vector< std::string >::iterator itDep;
				DataSectionIterator it;
				std::vector< std::string > dependencies;

				// File the dependency list
				for(it = pSection->begin(); it != pSection->end(); it++)
				{
					DataSectionPtr pDS = *it;
					if(pDS->sectionName() == "depends")
					{
						std::string value;
						BinaryPtr pStringData = pDS->asBinary();
						const char* pValues = reinterpret_cast<const char*>( pStringData->data() );
						value.assign( pValues, pValues + pStringData->len() );
						dependencies.push_back( value );
					}
				}

				if (dependencies.size() == 0)
					incTime = objTime;

				for (itDep =dependencies.begin(); itDep != dependencies.end(); itDep++)
				{
					std::string name = (*itDep);
					std::string pathname = EffectIncludes::instance().currentPath() + name;
					uint64 depTime = BWResource::modifiedTime( pathname );

					// Not found in the current path, go through the rest.
					if ( depTime == 0 )
					{
						const EffectManager::IncludePaths& paths = EffectManager::instance()->includePaths();
						EffectManager::IncludePaths::const_iterator it = paths.begin();
						EffectManager::IncludePaths::const_iterator end = paths.end();
						while (it != end)
						{
							pathname = *(it++) + name;
							depTime = BWResource::modifiedTime( pathname );
							if (depTime != 0)
								it = end;
						}
					}				
					incTime = std::max( incTime, depTime );
				}
			}
			else
				incTime = objTime+1; // force recompile for old file
		}
		recompile = (objTime < resTime) || (objTime < incTime);
	}

	return recompile;
}


/**
 *	Generate the array of preprocessor definitions.
 */
D3DXMACRO* ManagedEffect::generatePreprocessors()
{
	D3DXMACRO * preProcess = NULL;

	// sets up macro definitions
	const EffectManager::StringStringMap & macros = EffectManager::instance()->macros();
	preProcess = new D3DXMACRO[macros.size()+2];

	int idx = 0;	
	EffectManager::StringStringMap::const_iterator macroIt  = macros.begin();
	EffectManager::StringStringMap::const_iterator macroEnd = macros.end();
	while (macroIt != macroEnd)
	{	
		preProcess[idx].Name       = macroIt->first.c_str();
		preProcess[idx].Definition = macroIt->second.c_str();
		++macroIt;
		++idx;
	}
	preProcess[idx].Name         = "IN_GAME";
	preProcess[idx].Definition   = "true";
	preProcess[idx+1].Name       = NULL;
	preProcess[idx+1].Definition = NULL;

	return preProcess;
}


/**
 *	This method loads the managed effect into D3D from the passed binary section
 *	@param bin the binary data for the effect.
  *	@param preProcessDefinition preprocessor definitions.
 *	@return true if successful
 */
bool ManagedEffect::cache( BinaryPtr bin, D3DXMACRO * preProcessDefinition )
{
	#define SAFE_RELEASE(pointer)\
		if (pointer != NULL)     \
		{                        \
			pointer->Release();  \
			pointer = NULL;      \
		}

	ID3DXEffect* pEffect = NULL;
	ID3DXBuffer* pBuffer = NULL;
	D3DXMACRO * preProcess = NULL;

	if (preProcessDefinition == NULL)
		preProcess = preProcessDefinition = generatePreprocessors();

	// create the effect
	HRESULT hr = E_FAIL;
	if ( bin )
	{
		hr = D3DXCreateEffect( rc().device(),
			bin->cdata(), bin->len(), preProcessDefinition, 
			&EffectIncludes::instance(), USE_LEGACY_D3DX9_DLL, 
			NULL, &pEffect, &pBuffer );	
	}

	if (preProcess)
	{
		delete [] preProcess;
		preProcess = NULL;
	}

	SAFE_RELEASE(pBuffer);
	
	// Set up the effect and our handled constants.
	if ( bin && SUCCEEDED(hr) )
	{
		// Associate our state manager with the effect
		pEffect->SetStateManager( StateManager::instance() );
		D3DXEFFECT_DESC effectDesc;
        pEffect->GetDesc( &effectDesc );
		// Iterate over the effect parameters.
		defaultProperties_.clear();
		mappedConstants_.clear();
		for (uint32 i = 0; i < effectDesc.Parameters; i++)
		{
			D3DXHANDLE param = pEffect->GetParameter( NULL, i );
			D3DXPARAMETER_DESC paramDesc;
			pEffect->GetParameterDesc( param, &paramDesc );
			if (artistEditable( pEffect, param ))
			{
				bool found = false;
				// if the parameter is artist editable find the right effectproperty 
				// processor and create the default property for it
				PropertyFunctors::iterator it = g_effectPropertyProcessors.begin();
				PropertyFunctors::iterator end = g_effectPropertyProcessors.end();
				while (it != end)
				{
					if (it->second->check( &paramDesc ))
					{
						defaultProperties_[ paramDesc.Name ] = it->second->create( param, pEffect );
						found = true;
						it = end;
					}
					else
					{
						it++;
					}
				}

				if ( !found )
				{
					ERROR_MSG( "Could not find property processor for %s\n", paramDesc.Name );
				}
			}
			else if (paramDesc.Semantic )
			{
				// If the parameter has a Semantic, set it up as an automatic constant.
				mappedConstants_.push_back( std::make_pair( param, EffectConstantValue::get( paramDesc.Semantic ) ) );
			}
		}

		pEffect_ = pEffect;
		pEffect->Release();
		pEffect = NULL;

		return true;
	}
	else
	{
		if (bin)
			printErrorMsg_(hr, "ManagedEffect::load - Error creating the effect " + resourceID_ );	
		else
			ERROR_MSG("ManagedEffect::load - Error creating the effect %s\n", resourceID_.c_str());
	}
	return false;

#undef SAFE_RELEASE
}


BinaryPtr ManagedEffect::compile( const std::string& resName, D3DXMACRO * preProcessDefinition )
{
	#define SAFE_RELEASE(pointer)\
		if (pointer != NULL)     \
		{                        \
			pointer->Release();  \
			pointer = NULL;      \
		}

	ID3DXBuffer* pBuffer = NULL;
	D3DXMACRO * preProcess = NULL;

	if (preProcessDefinition == NULL)
		preProcess = preProcessDefinition = generatePreprocessors();

	// use the current fxoInfix to compile the set of 
	// fx using the currently active macro definitions
	std::string effectPath = BWResource::getFilePath( resName );
	EffectIncludes::instance().currentPath( effectPath );

	std::string fxoInfix   = EffectManager::instance()->fxoInfix();
	std::string fileName   = BWResource::removeExtension(resName);
	std::string objectName = !fxoInfix.empty() 
		? fileName + "." + fxoInfix + ".fxo"
		: fileName + ".fxo";

	EffectIncludes::instance().resetDependencies();

	// Check if the file or any of its dependents have been modified.
	bool recompile = EffectManager::instance()->checkModified( objectName, resName );

	BinaryPtr bin = 0;
	if (recompile)
	{
		DEBUG_MSG( 
			"ManagedEffect::compiling %s (%s)\n", 
			resName.c_str(), objectName.c_str() );
			
		ID3DXEffectCompiler* pCompiler = NULL;
		bin = BWResource::instance().fileSystem()->readFile( resName );
		HRESULT hr;
		if ( bin )
		{
			hr = D3DXCreateEffectCompiler( bin->cdata(), bin->len(),
				preProcessDefinition, &EffectIncludes::instance(), 
				USE_LEGACY_D3DX9_DLL, &pCompiler,
				&pBuffer );
		}
		if ( !bin || FAILED(hr))
		{
			if (preProcess)
			{
				delete [] preProcess;
				preProcess = NULL;
			}
		
			ERROR_MSG("ManagedEffect::load - Unable to create effect compiler for %s: %s\n",
									resName.c_str(),
									pBuffer?pBuffer->GetBufferPointer():"" );

			SAFE_RELEASE(pCompiler);
			SAFE_RELEASE(pBuffer);
			return NULL;
		}

		SAFE_RELEASE(pBuffer);

		MF_ASSERT( pCompiler != NULL );

		ID3DXBuffer* pEffectBuffer = NULL;
		hr = pCompiler->CompileEffect( 0, &pEffectBuffer, &pBuffer );
		SAFE_RELEASE(pCompiler);
		if (FAILED(hr))
		{
			if (preProcess)
			{
				delete [] preProcess;
				preProcess = NULL;
			}

			ERROR_MSG("ManagedEffect::load - Unable to compile effect %s\n%s",
									resName.c_str(),
									pBuffer?pBuffer->GetBufferPointer():"" );

			SAFE_RELEASE(pBuffer);
			return NULL;
		}
		SAFE_RELEASE(pBuffer);

		bin = new BinaryBlock( pEffectBuffer->GetBufferPointer(),
			pEffectBuffer->GetBufferSize() );

		BWResource::instance().fileSystem()->eraseFileOrDirectory(
			objectName );

		// create the new section
		size_t lastSep = objectName.find_last_of('/');
		std::string parentName = objectName.substr(0, lastSep);
		DataSectionPtr parentSection = BWResource::openSection( parentName );
		MF_ASSERT(parentSection);

		std::string tagName = objectName.substr(lastSep + 1);

		// make it
		DataSectionPtr pSection = new BinSection( tagName, new BinaryBlock( NULL, 0 ) );
		pSection->setParent( parentSection );

		MF_ASSERT( pSection );
		pSection->delChildren();
		DataSectionPtr effectSection = pSection->openSection( "effect", true );
		effectSection->setParent(pSection);
		bool warn = true;
		if ( pSection->writeBinary( "effect", bin ) )
		{
			// Write the dependency list			
			DataSectionPtr pNewSec = NULL;
			std::list<std::string>::iterator it;
			std::list<std::string> &src = EffectIncludes::instance().dependencies();
			src.unique(); // remove possible duplicates..

			for(it = src.begin(); it != src.end(); it++)
			{
				pNewSec = pSection->newSection("depends");
				std::string str((*it));

				BinaryPtr binaryBlockString = new BinaryBlock( str.data(), str.size() );
				pNewSec->setBinary( binaryBlockString );
			}

			// Now actually save...
			if ( !pSection->save() )
			{
				// It failed, but it might be because it's running from Zip(s)
				// file(s). If so, there should be a writable, cache directory
				// in the search paths, so try to build the folder structure in
				// it and try to save again.
				BWResource::ensurePathExists( effectPath );	
				warn = !pSection->save();
			}
			else
			{
				warn = false;
			}
		}

		if (warn)
		{
			WARNING_MSG( "Could not save recompiled %s.\n",
				objectName.c_str() );
		}

		effectSection->setParent(NULL);
		effectSection = NULL;
		pSection = NULL;
		parentSection = NULL;
		SAFE_RELEASE(pEffectBuffer);
	}
	else
	{
		DataSectionPtr pSection = BWResource::instance().rootSection()->openSection( objectName );
		if (pSection)
		{
			bin = pSection->readBinary( "effect" );
			if (!bin) // left in to load old files.
				bin = BWResource::instance().fileSystem()->readFile( objectName );
		}
		else
			bin = BWResource::instance().fileSystem()->readFile( objectName );
	}

	if (preProcess)
	{
		delete [] preProcess;
		preProcess = NULL;
	}

	return bin;

#undef SAFE_RELEASE
}

bool ManagedEffect::load( const std::string& effectResource )
{
	#define SAFE_RELEASE(pointer)\
		if (pointer != NULL)     \
		{                        \
			pointer->Release();  \
			pointer = NULL;      \
		}

	resourceID_ = effectResource;

	D3DXMACRO * preProcessDefinition = generatePreprocessors();

	std::string resName = effectResource;

	// use the current fxoInfix to compile the set of 
	// fx using the currently active macro definitions
	std::string effectPath = BWResource::getFilePath( effectResource );
	EffectIncludes::instance().currentPath( effectPath );
	std::string fxoInfix   = EffectManager::instance()->fxoInfix();
	std::string fileName   = BWResource::removeExtension(resName);
	std::string objectName = !fxoInfix.empty() 
		? fileName + "." + fxoInfix + ".fxo"
		: fileName + ".fxo";

	BinaryPtr bin = compile(resName, preProcessDefinition);

	bool ret = cache(bin, preProcessDefinition);
	delete [] preProcessDefinition;
	return ret;

#undef SAFE_RELEASE
}

/**
 *	Runs through all loaded effect files, registering 
 *	their graphics settings when relevant. 
 *
 *	@return	False if application was quit during processing.
 */
bool EffectManager::registerGraphicsSettings()
{
	bool result = true;
	Effects::iterator it = effects_.begin();
	Effects::iterator end = effects_.end();
	while (it != end)
	{
		if (it->second)
		{
			if (!it->second->registerGraphicsSettings(it->first))
			{
				result = false;
				break;
			}
		}
		it++;
	}
	return result;
}

/**
 *	Retrieve "graphicsSetting" label and "label" annotation from 
 *	the effect file, if any. Effects labelled as graphics settings will 
 *	be registered into moo's graphics settings registry. Each technique 
 *	tagged by a "label" annotation will be added as an option to the
 *	effect's graphic setting object. Materials and other subsystems can 
 *	then register into the setting entry and select the appropriate 
 *	technique when the selected options/technique changes.
 *
 *	Like any graphics setting, more than one effect can be registered 
 *	under the same setting entry. For this to happen, they must share 
 *	the same label, have the same number of tagged techniques, each
 *	with the same label and appearing in the same order. If two or 
 *	more effects shared the same label, but the above rules are 
 *	not respected, an assertion will fail. 
 *
 *	@param	effectResource	name of the effect file.
 *	@return	False if application was quit during processing.
 *	@see	GraphicsSetting::add
 */
bool ManagedEffect::registerGraphicsSettings(
	const std::string & effectResource)
{
	if (!Moo::rc().checkDeviceYield())
	{
		ERROR_MSG("ManagedEffect: device lost when registering settings\n");
		return false;
	}
	
	settingsAdded_ = true;
	bool result = true;
	this->techniques_.clear();
	D3DXHANDLE feature = pEffect_->GetParameterByName(0, "graphicsSetting");
	if (feature != 0)
	{
		D3DXHANDLE nameHandle = 
			pEffect_->GetAnnotationByName(feature, "label");
			
		LPCSTR settingName;
		if (nameHandle != 0 && 
				SUCCEEDED(pEffect_->GetString(nameHandle, &settingName)))
		{
			int firstValidTech = -1;
			D3DXEFFECT_DESC effectDesc;
			pEffect_->GetDesc(&effectDesc);
			this->settingName_ = settingName;

			if (Moo::rc().mixedVertexProcessing())
				Moo::rc().device()->SetSoftwareVertexProcessing( TRUE );

			for (uint i=0; i<effectDesc.Techniques; ++i)
			{
				TechniqueInfo technique;
				technique.handle = pEffect_->GetTechnique(i);
				if (technique.handle != NULL)
				{
					technique.psVersion = this->maxPSVersion(technique.handle);
					technique.supported = SUCCEEDED(
						pEffect_->ValidateTechnique(technique.handle));
					
					LPCSTR techDesc;
					D3DXHANDLE techDescHandle = 
						pEffect_->GetAnnotationByName(
							technique.handle, "label");						
						
					if (techDescHandle != 0 &&
							SUCCEEDED(pEffect_->GetString(
								techDescHandle, &techDesc)))
					{
						technique.name = techDesc;
						this->techniques_.push_back(technique);
					}
					else
					{
						D3DXTECHNIQUE_DESC desc;
						pEffect_->GetTechniqueDesc(technique.handle, &desc);
						WARNING_MSG(
							"Effect labeled as a feature but no "
							"\"label\" annotation provided for "
							"technique \"%s\" (won't be listed)\n", desc.Name);
					}
				}
				else
				{
					INFO_MSG( "ManagedEffect::registerGraphicsSettings - invalid handle : %s\n", effectResource.c_str());
				}
			}
			if (Moo::rc().mixedVertexProcessing())
				Moo::rc().device()->SetSoftwareVertexProcessing( FALSE );

		}
		else 
		{
			ERROR_MSG(
				"Effect \"%s\" labeled as feature but no "
				"\"label\" annotation provided\n", 
				effectResource.c_str());
		}
	}

	// register engine feature if 
	// effect is labeled as such
	if (!this->settingName_.empty() && !this->techniques_.empty())
	{
		this->settingEntry_ = new EffectTechniqueSetting(this, this->settingName_);
		typedef TechniqueInfoVector::const_iterator citerator;
		citerator techIt  = this->techniques_.begin();
		citerator techEnd = this->techniques_.end();
		while (techIt != techEnd)
		{
			this->settingEntry_->addTechnique(*techIt);
			++techIt;
		}
		
		// register it
		GraphicsSetting::add(this->settingEntry_);
	}
	
	return result;
}

// -----------------------------------------------------------------------------
// Section: EffectManager
// -----------------------------------------------------------------------------

// Constructor
EffectManager::EffectManager():
	effects_(),
	includePaths_(),
	effectsLock_(),
	macros_(),
	fxoInfix_(""),
	listeners_()
{
	addIncludePaths( s_shaderIncludePath );
}

// Destructor
EffectManager::~EffectManager()
{
	if (effects_.size())
	{
		WARNING_MSG( "EffectManager::~EffectManager - not empty on destruct, this could cause problems.\n" ); 
		Effects::iterator it = effects_.begin();
		while (it != effects_.end())
		{
			INFO_MSG( "-- Effect not deleted: %s\n", it->first.c_str() );
			it++;
		}
	}
}

/**
 *	This method adds the includepaths from pathString to the include search path.
 *	@param pathString the paths to add, paths can be separated by ;
 */
void EffectManager::addIncludePaths( const std::string& pathString )
{
	std::string rest = pathString;
	while (rest.length())
	{
		std::string::size_type index = rest.find_first_not_of( ";" );
		if (index != std::string::npos)
		{
			rest = rest.substr( index );
		}

		index = rest.find_first_of( ";" );
		std::string token;
		if (index != std::string::npos)
		{
			token = rest.substr( 0, index );
			rest = rest.substr( index + 1 );
		}
		else
		{
			token = rest;
			rest = "";
		}
		index = token.find_first_not_of(" ");
		std::string::size_type end = token.find_last_not_of(" ");
		if (index != std::string::npos && end != std::string::npos)
		{
			token = BWResource::formatPath( token.substr( index, end - index + 1 ) );
			DEBUG_MSG( "EffectManager::addIncludePaths - adding include path %s\n", token.c_str() );
			includePaths_.push_back( token );
		}
	}

}

/**
 *	Get the instance of the EffectManager singleton
 */
EffectManager* EffectManager::instance()
{
	return pInstance_;
}

void EffectManager::del( ManagedEffect* pEffect )
{
	if (pInstance_)
		pInstance_->delInternal( pEffect );
}

/*
 *	Remove the effect from the effect manager, called automatically in the
 *	managed effect destructor.
 */
void EffectManager::delInternal( ManagedEffect* pEffect )
{
	SimpleMutexHolder smh( effectsLock_ );

	Effects::iterator it = effects_.begin();
	Effects::iterator end = effects_.end();
	while (it != end)
	{
		if (it->second == pEffect)
		{
			effects_.erase( it );
			return;
		}
		else
			it++;
	}
}

/**
 *	Sets the current pixel shader version cap. This will force all 
 *	MaterialEffects that use GetNextValidTechnique to limit the valid 
 *	techniques to those within the current pixel shader version cap. 
 *	Effect files registered as graphics settings will also be capped,
 *	but they can be individually reset later. Implicitly called when
 *	the user changes the PS Vestion Cap's active option.
 *
 *	@param activeOption		the new active option index.
 */
// TODO: this should really be in EffectMaterial.
void EffectManager::setPSVerCapOption(int activeOption)
{
	Effects::const_iterator effIt = this->effects_.begin();
	Effects::const_iterator effEnd = this->effects_.end();
	while (effIt != effEnd)
	{
		EffectTechniqueSetting * setting = effIt->second->graphicsSettingEntry();
		if (setting != NULL)
		{			
			setting->setPSCapOption(this->PSVersionCap());
		}
		effIt++;
	}

	// update all listeners (usually EffectMaterial instances)
	this->listenersLock_.grab();
	ListenerVector::const_iterator listIt  = this->listeners_.begin();
	ListenerVector::const_iterator listEnd = this->listeners_.end();
	while (listIt != listEnd)
	{
		(*listIt)->onSelectPSVersionCap(c_MaxPixelShaderVersion-activeOption);
		++listIt;
	}
	this->listenersLock_.give();
}

/**
 *	Get the effect from the manager, create it if it's not there.
 *
 *	@param resourceID the name of the effect to load
 *	@return pointer to the loaded effect
 */
ManagedEffect* EffectManager::get( const std::string& resourceID )
{
	MF_ASSERT( pInstance_ );
	// See if we have a copy of the effect already
	ManagedEffect* pEffect = pInstance_->find( resourceID );
	if (!pEffect)
	{
		// Create the effect if it could not be found.
		pEffect = new ManagedEffect;
		if (pEffect->load( resourceID ))
		{
			pInstance_->add( pEffect, resourceID );
		}
		else
		{
			ERROR_MSG( "EffectManager::get - unable to load %s\n", resourceID.c_str() );
			delete pEffect;
			pEffect = NULL;
		}
	}
	return pEffect;
}

/**
 *	As the name suggests, this only compiles a shader and discards
 *	the results.
 *
 *	@param resourceID the name of the effect to compile.
 */
void EffectManager::compileOnly( const std::string& resourceID )
{
	MF_ASSERT( pInstance_ );
	// See if we have a copy of the effect already
	ManagedEffect* pEffect = pInstance_->find( resourceID );
	if (!pEffect)
	{
		// Create the effect if it could not be found.
		pEffect = new ManagedEffect;
		pEffect->compile( resourceID );

		delete pEffect;
		pEffect = NULL;		
	}	
}


/**
 *	Find an effect in the effect manager
 *	@param resourceID the name of the effect
 *	@return pointer to the effect
 */
ManagedEffect* EffectManager::find( const std::string& resourceID )
{
	SimpleMutexHolder smh( effectsLock_ );
	ManagedEffect* pEffect = NULL;
	Effects::iterator it = effects_.find( resourceID );
	if (it != effects_.end())
		pEffect = it->second;

	return pEffect;
}

/**
 *	Add an effect to the effect manager
 *	@param pEffect the effect
 *	@param resourceID the name of the effect
 */
void EffectManager::add( ManagedEffect* pEffect, const std::string& resourceID )
{
	SimpleMutexHolder smh( effectsLock_ );
	effects_.insert( std::make_pair( resourceID, pEffect ) );
}

void EffectManager::createUnmanagedObjects()
{
	SimpleMutexHolder smh( effectsLock_ );

	Effects::iterator it = effects_.begin();
	Effects::iterator end = effects_.end();
	while (it != end)
	{
		if (it->second)
		{
			it->second->pEffect()->OnResetDevice();
		}
		it++;
	}
}

void EffectManager::deleteUnmanagedObjects()
{
	SimpleMutexHolder smh( effectsLock_ );

	Effects::iterator it = effects_.begin();
	Effects::iterator end = effects_.end();
	while (it != end)
	{
		if (it->second)
		{
			it->second->pEffect()->OnLostDevice();
		}
		it++;
	}
}

/**
 *	Sets effect file compile time macro definition.
 *
 *	@param	macro	macro name.
 *	@param	value	macro value.
 */
void EffectManager::setMacroDefinition(
	const std::string & macro, 
	const std::string & value)
{
	this->macros_[macro] = value;
}

/**
 *	Returns effect file compile time macro definitions.
 */
const EffectManager::StringStringMap & EffectManager::macros()
{
	return this->macros_;
}

/**
 *	Returns fxo file infix.
 */
const std::string & EffectManager::fxoInfix() const
{
	return this->fxoInfix_;
}

/**
 *	Sets fxo file infix. This infix is used when compiling the fx files to 
 *	differentiate different compilation settings (most commonly different
 *	macro definition). It's currently used by the EffectMacroSetting.
 */
void EffectManager::fxoInfix(const std::string & infix)
{
	this->fxoInfix_ = infix;
}

/**
 *	Retrieves the current pixel shader version cap (major version number).
 */
int EffectManager::PSVersionCap() const
{
	return (c_MaxPixelShaderVersion - this->psVerCapSettings_->activeOption());
}

// static objects
EffectManager* EffectManager::pInstance_ = NULL;

/**
 * This method inits the effect manager instance
 */
void EffectManager::init()
{
	pInstance_ = new EffectManager;
	
	// Initialise the macro settings for the effect files 
	// These settings are used to select different graphics 
	// options when compiling effects 
	EffectMacroSetting::setupSettings();
	EffectMacroSetting::updateManagerInfix();
	
 	// Set up the max pixelshader version graphics option 
 	int maxPSVerSupported = (Moo::rc().psVersion() & 0xff00) >> 8; 
 	pInstance_->psVerCapSettings_ =  
		Moo::makeCallbackGraphicsSetting(
			"SHADER_VERSION_CAP", *pInstance_, 
			&EffectManager::setPSVerCapOption	, 
			c_MaxPixelShaderVersion-maxPSVerSupported, 
			false, false);

	for (int i=0; i<=c_MaxPixelShaderVersion; ++i)
	{
		const int shaderVersion = (c_MaxPixelShaderVersion-i);
		std::stringstream verStream;
		verStream << "SHADER_MODEL_" << shaderVersion;
		bool supported = shaderVersion <= maxPSVerSupported;
		pInstance_->psVerCapSettings_->addOption(verStream.str(), supported);
	}
	Moo::GraphicsSetting::add(pInstance_->psVerCapSettings_);

}

void EffectManager::fini()
{
	delete pInstance_;
	pInstance_ = NULL;
}

/**
 *	Registers an EffectManager listener instance.
 */
void EffectManager::addListener(IListener * listener)
{
	this->listenersLock_.grab();
	MF_ASSERT(std::find(
		this->listeners_.begin(), 
		this->listeners_.end(), 
		listener) == this->listeners_.end());
		
	this->listeners_.push_back(listener);
	this->listenersLock_.give();
}

/**
 *	Unregisters an EffectManager listener instance.
 */
void EffectManager::delListener(IListener * listener)
{
	this->listenersLock_.grab();
	ListenerVector::iterator listIt = std::find(
		this->listeners_.begin(), this->listeners_.end(), listener);

	MF_ASSERT(listIt != this->listeners_.end());
	this->listeners_.erase(listIt);
	this->listenersLock_.give();
}

// -----------------------------------------------------------------------------
// Section: EffectTechniqueSetting
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 *
 *	@param	owner	owning ManagedEffect object.
 *	@param	label	label for setting.
 */
EffectTechniqueSetting::EffectTechniqueSetting(
		ManagedEffect * owner, const std::string & label):
	GraphicsSetting(label, -1, false, false),
	owner_(owner),
	techniques_(),
	listeners_()
{}
	
/**
 *	Adds new technique to effect. This will add a new 
 *	option by the same label into the base GraphicsSetting.
 *
 *	@param	info	technique information.
 */
void EffectTechniqueSetting::addTechnique(const ManagedEffect::TechniqueInfo &info)
{
	this->GraphicsSetting::addOption(info.name, info.supported);
	this->techniques_.push_back(std::make_pair(info.handle, info.psVersion));
}
	
/**
 *	Virtual functions called by the base class whenever the current 
 *	option changes. Will notify all registered listeners and slaves.
 *
 *	@param	optionIndex	index of new option selected.
 */
void EffectTechniqueSetting::onOptionSelected(int optionIndex)
{
	this->listenersLock_.grab();
	ListenersMap::iterator listIt  = this->listeners_.begin();
	ListenersMap::iterator listEnd = this->listeners_.end();
	while (listIt != listEnd)
	{
		listIt->second->onSelectTechnique(
			this->owner_, this->techniques_[optionIndex].first);
		++listIt;
	}
	this->listenersLock_.give();
}		

/**
 *	Sets the pixel shader version cap. Updates effect to use a supported 
 *	technique that is still within the new cap. If none is found, use the
 *	last one available (should be the less capable).
 *
*	@param	psVersionCap	maximum major version number.
 */
void EffectTechniqueSetting::setPSCapOption(int psVersionCap)
{
	typedef D3DXHandlesPSVerVector::const_iterator citerator;
	citerator techBegin = this->techniques_.begin();
	citerator techEnd   = this->techniques_.end();
	citerator techIt    = techBegin;
	while (techIt != techEnd)
	{
		if (techIt->second <= psVersionCap)
		{
			int optionIndex = std::distance(techBegin, techIt);
			this->GraphicsSetting::selectOption(optionIndex);
			break;
		}
		++techIt;
	}
	if (techIt == techEnd)
	{
		this->GraphicsSetting::selectOption(this->techniques_.size()-1);
	}
}

/**
 *	Returns handle to currently active technique.
 *
 *	@return	handle to active technique.
 */
D3DXHANDLE EffectTechniqueSetting::activeTechniqueHandle() const
{
	int activeOption = this->GraphicsSetting::activeOption();
	if (activeOption == -1) return 0;
	if (activeOption >= (int)(this->techniques_.size())) return 0;
	return this->techniques_[activeOption].first;
}

/**
 *	Registers an EffectTechniqueSetting listener instance.
 *
 *	@param	listener	listener to register.
 *	@return				id of listener (use this to unregister).
 */
int EffectTechniqueSetting::addListener(IListener * listener)
{
	++EffectTechniqueSetting::listenersId;
	
	this->listenersLock_.grab();
	this->listeners_.insert(std::make_pair(EffectTechniqueSetting::listenersId, listener));
	this->listenersLock_.give();
	
	return EffectTechniqueSetting::listenersId;
}

/**
 *	Unregisters EffectTechniqueSetting listener identified by given id.
 *
 *	@param	listenerId	id of listener to be unregistered.
 */
void EffectTechniqueSetting::delListener(int listenerId)
{
	this->listenersLock_.grab();
	ListenersMap::iterator listIt = this->listeners_.find(listenerId);
	MF_ASSERT(listIt !=  this->listeners_.end());
	this->listeners_.erase(listIt);
	this->listenersLock_.give();
}

int EffectTechniqueSetting::listenersId = 0;

// -----------------------------------------------------------------------------
// Section: EffectTechniqueSetting
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 *
 *	@param	label		label for setting.
 *	@param	macro		macro to be defined.
 *	@param	setupFunc	callback that will setup this EffectMacroSetting.
 */
EffectMacroSetting::EffectMacroSetting(
		const std::string & label, 
		const std::string & macro, 
		SetupFunc setupFunc) :
	GraphicsSetting(label, -1, false, true),
	macro_(macro),
	setupFunc_(setupFunc),
	values_()
{
	EffectMacroSetting::add(this);
}

/**
 *	Adds option to this EffectMacroSetting.
 *
 *	@param	label		label for option.
 *	@param	isSupported	true if option is supported in this system.
 *	@param	value		value the macro should be defined to when is 
 *						option is active.
 */
void EffectMacroSetting::addOption(
	const std::string & label, 
	bool                isSupported, 
	const std::string & value)
{
	this->GraphicsSetting::addOption(label, isSupported);
	this->values_.push_back(value);
}

/**
 *	Defines the macro according to the option selected. Implicitly 
 *	called whenever the user changes the current active option.
 */		
void EffectMacroSetting::onOptionSelected(int optionIndex)
{
	EffectManager::instance()->setMacroDefinition(
		this->macro_, 
		this->values_[optionIndex]);
}

/**
 *	REgisters setting to static list of EffectMacroSetting instances.
 */
void EffectMacroSetting::add(EffectMacroSettingPtr setting)
{
	static_settings().push_back(setting);
}

/**
 *	Update the EffectManager's fxo infix according to the currently 
 *	selected macro effect options.
 */
void EffectMacroSetting::updateManagerInfix()
{
	std::stringstream infix;
	MacroSettingVector::const_iterator setIt  = static_settings().begin();
	MacroSettingVector::const_iterator setEnd = static_settings().end();
	while (setIt != setEnd)
	{
		infix << (*setIt)->activeOption();
		++setIt;
	}
	EffectManager::instance()->fxoInfix(infix.str());
}

/**
 *	Sets up registered macro effect instances.
 */	
void EffectMacroSetting::setupSettings()
{
	MacroSettingVector::iterator setIt  = static_settings().begin();
	MacroSettingVector::iterator setEnd = static_settings().end();
	while (setIt != setEnd)
	{
		(*setIt)->setupFunc_(**setIt);
		GraphicsSetting::add(*setIt);
		++setIt;
	}
}

namespace { // anonymous

/**
 *	Helper function to work around static-initialization-order issues.
 */
EffectMacroSetting::MacroSettingVector & static_settings()
{
	static EffectMacroSetting::MacroSettingVector settings;
	return settings;
}

} // namespace anonymous

} // namespace moo

// managed_effect.cpp
