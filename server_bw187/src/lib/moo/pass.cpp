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
#include "pass.hpp"
#include "vertex_operation.hpp"
#include "pixel_operation.hpp"
#include "managed_vertex_shader.hpp"
#include "managed_pixel_shader.hpp"
#include "sampler.hpp"

DECLARE_DEBUG_COMPONENT2( "Moo", 0 );

namespace Moo
{

inline DWORD toDWORD( float f )
{
	return *(DWORD*)(&f);
}

static inline DWORD toDWORD( const Vector4& f )
{
	return D3DCOLOR_COLORVALUE( f.x, f.y, f.z, f.w );
}

// -----------------------------------------------------------------------------
// Section: Pass
// -----------------------------------------------------------------------------

/*
 * Base load functor
 */ 
class PassFunctor
{
public:
	virtual ~PassFunctor(){};
	virtual bool operator ()( Pass& pass, DataSectionPtr pData, const Samplers& samplers ) = 0;
};

/*
 * Macro for creating bool loader class
 */
#define PASS_BOOL_READER( x ) \
class Pass##x##Reader : public PassFunctor \
{ \
public: \
	~Pass##x##Reader() {}; \
	bool operator () ( Pass& pass, DataSectionPtr pData, const Samplers& samplers ) \
	{ \
		pass.##x##( pData->asBool() ); \
		return true; \
	} \
};

// bool reader class definitions
PASS_BOOL_READER( zBufferRead );
PASS_BOOL_READER( zBufferWrite );
PASS_BOOL_READER( doubleSided );
PASS_BOOL_READER( fogged );

/*
 * Fog colour reader
 */
class PassFogColourReader : public PassFunctor
{
public:
	~PassFogColourReader() {};
	bool operator () ( Pass& pass, DataSectionPtr pData, const Samplers& samplers )
	{
		pass.fogColour( toDWORD( pData->asVector4() ) );
		pass.fogOverride( true );
		return true;
	}
};
/*
 * bool renderstate reader functor
 */
class PassRSboolReader : public PassFunctor
{
public:
	PassRSboolReader( D3DRENDERSTATETYPE state ) :
	  state_( state )
	{
	}
	bool operator() ( Pass& pass, DataSectionPtr pData, const Samplers& samplers )
	{
		pass.addRenderState( state_, pData->asBool() );
		return true;
	}
private:
	D3DRENDERSTATETYPE state_;
};

/*
 * int renderstate reader functor
 */
class PassRSintReader : public PassFunctor
{
public:
	PassRSintReader( D3DRENDERSTATETYPE state ) :
	  state_( state )
	{
	}
	bool operator() ( Pass& pass, DataSectionPtr pData, const Samplers& samplers )
	{
		pass.addRenderState( state_, pData->asInt() );
		return true;
	}
private:
	D3DRENDERSTATETYPE state_;
};

/*
 * vector4 renderstate reader functor
 */
class PassRSVector4Reader : public PassFunctor
{
public:
	PassRSVector4Reader( D3DRENDERSTATETYPE state ) :
	  state_( state )
	{
	}
	bool operator() ( Pass& pass, DataSectionPtr pData, const Samplers& samplers )
	{
		pass.addRenderState( state_, toDWORD( pData->asVector4() ) );
		return true;
	}
private:
	D3DRENDERSTATETYPE state_;
};

/*
 * D3DBLEND renderstate reader functor
 */
class PassRSD3DBLENDReader : public PassFunctor
{
public:
	PassRSD3DBLENDReader( D3DRENDERSTATETYPE state ) :
	  state_( state )
	{
	}
	typedef std::map< std::string, D3DBLEND > ArgumentMap;
	bool operator() ( Pass& pass, DataSectionPtr pData, const Samplers& samplers )
	{
		if (argumentMap_.empty())
		{
			#define BLEND_ENTRY( x ) \
				argumentMap_.insert( std::make_pair( #x, D3DBLEND_##x ) )
			BLEND_ENTRY( ZERO );
			BLEND_ENTRY( ONE );
			BLEND_ENTRY( SRCCOLOR );
			BLEND_ENTRY( INVSRCCOLOR );
			BLEND_ENTRY( SRCALPHA );
			BLEND_ENTRY( INVSRCALPHA );
			BLEND_ENTRY( DESTALPHA );
			BLEND_ENTRY( INVDESTALPHA );
			BLEND_ENTRY( DESTCOLOR );
			BLEND_ENTRY( INVDESTCOLOR );
			BLEND_ENTRY( SRCALPHASAT );
			BLEND_ENTRY( BOTHSRCALPHA );
			BLEND_ENTRY( BOTHINVSRCALPHA );
			BLEND_ENTRY( BLENDFACTOR );
			BLEND_ENTRY( INVBLENDFACTOR );
		}
		ArgumentMap::iterator it = argumentMap_.find( pData->asString() );
		if (it != argumentMap_.end())
		{
			pass.addRenderState( state_, it->second );
		}
		else
		{
			ERROR_MSG( "PassRSD3DBLENDReader::() - illegal D3DBLEND %s\n", pData->asString().c_str() );
		}
		return true;
	}
private:
	D3DRENDERSTATETYPE state_;
	static ArgumentMap argumentMap_;
};

PassRSD3DBLENDReader::ArgumentMap PassRSD3DBLENDReader::argumentMap_;

/*
 * D3DCMP renderstate reader functor
 */
class PassRSD3DCMPFUNCReader : public PassFunctor
{
public:
	PassRSD3DCMPFUNCReader( D3DRENDERSTATETYPE state ) :
	  state_( state )
	{
	}
	typedef std::map< std::string, D3DCMPFUNC > ArgumentMap;
	bool operator() ( Pass& pass, DataSectionPtr pData, const Samplers& samplers )
	{
		if (argumentMap_.empty())
		{
			#define CMPFUNC_ENTRY( x ) \
				argumentMap_.insert( std::make_pair( #x, D3DCMP_##x ) )
			CMPFUNC_ENTRY( NEVER );
			CMPFUNC_ENTRY( LESS );
			CMPFUNC_ENTRY( EQUAL );
			CMPFUNC_ENTRY( LESSEQUAL );
			CMPFUNC_ENTRY( GREATER );
			CMPFUNC_ENTRY( NOTEQUAL );
			CMPFUNC_ENTRY( GREATEREQUAL );
			CMPFUNC_ENTRY( ALWAYS );
		}
		ArgumentMap::iterator it = argumentMap_.find( pData->asString() );
		if (it != argumentMap_.end())
		{
			pass.addRenderState( state_, it->second );
		}
		else
		{
			ERROR_MSG( "PassRSD3DCMPFUNCReader::() - illegal D3DCMPFUNC %s\n", pData->asString().c_str() );
		}
		return true;
	}
private:
	D3DRENDERSTATETYPE state_;
	static ArgumentMap argumentMap_;
};

PassRSD3DCMPFUNCReader::ArgumentMap PassRSD3DCMPFUNCReader::argumentMap_;

/*
 * texturestage reader functor
 */
class PassTextureStageReader : public PassFunctor
{
public:
	~PassTextureStageReader(){};
	bool operator() ( Pass& pass, DataSectionPtr pSection, const Samplers& samplers )
	{
		Pass::TextureStage* ts = new Pass::TextureStage();
		if (ts->load( pSection, samplers ))
		{
			pass.addTextureStage( ts );
		}
		return true;
	}

private:
};

/*
 * Vertex operation reader functor
 */
class PassVertexShaderReader : public PassFunctor
{
public:
	virtual bool operator ()( Pass& pass, DataSectionPtr pData, const Samplers& samplers )
	{
		SingleVertexShader* pVS = new SingleVertexShader;
		if (pVS->load( pData ))
		{
			pass.vertexOperation( pVS );
		}
		else
		{
			delete pVS;
			return false;
		}
		return true;
	}

private:
};

/*
 * Vertex operation reader functor
 */
class PassPixelShaderReader : public PassFunctor
{
public:
	virtual bool operator ()( Pass& pass, DataSectionPtr pData, const Samplers& samplers )
	{
		SinglePixelShader* pPS = new SinglePixelShader;
		if (pPS->load( pData, samplers ))
		{
			pass.pixelOperation( pPS );
		}
		else
		{
			delete pPS;
			return false;
		}
		return true;
	}

private:
};

/*
 * Vertex operation reader functor
 */
class PassVertexShaderSetReader : public PassFunctor
{
public:
	virtual bool operator ()( Pass& pass, DataSectionPtr pData, const Samplers& samplers )
	{
		VertexShaderSetOperation* pVSS = new VertexShaderSetOperation;
		if (pVSS->load( pData ))
		{
			pass.vertexOperation( pVSS );
		}
		else
		{
			delete pVSS;
			return false;
		}
		return true;
	}

private:
};

/*
 * Pass loader class
 */
class PassReader
{
public:
	typedef std::map<std::string, PassFunctor*> PassProcessors;

	static PassReader& instance()
	{
		static PassReader inst;
		return inst;
	}

	bool load( Pass& pass, DataSectionPtr pSection, const Samplers& samplers )
	{
		DataSectionIterator it = pSection->begin();
		DataSectionIterator end = pSection->end();
		while (it != end)
		{
			PassProcessors::iterator processor = passProcessors_.find( it->sectionName() );
			if (processor != passProcessors_.end())
			{
				(*processor->second)( pass, *it, samplers );
			}
			else
			{
				ERROR_MSG( "Passreader::load - don't know how to load %s\n", it->sectionName().c_str() );
			}
			it++;
		}
		return true;
	}

private:
	PassReader()
	{
		#define ADD_PASS_BOOL_READER( x ) \
			passProcessors_.insert( std::make_pair( #x, new Pass##x##Reader ) )

		ADD_PASS_BOOL_READER( zBufferRead );
		ADD_PASS_BOOL_READER( zBufferWrite );
		ADD_PASS_BOOL_READER( doubleSided );
		ADD_PASS_BOOL_READER( fogged );

		#define ADD_PASS_RENDERSTATE_READER( x, y ) \
			passProcessors_.insert( \
			std::make_pair( #x, new PassRS##y##Reader( D3DRS_##x ) ) )

		ADD_PASS_RENDERSTATE_READER( ALPHABLENDENABLE, bool );
		ADD_PASS_RENDERSTATE_READER( ALPHATESTENABLE, bool );
		ADD_PASS_RENDERSTATE_READER( DESTBLEND, D3DBLEND );
		ADD_PASS_RENDERSTATE_READER( SRCBLEND, D3DBLEND );
		ADD_PASS_RENDERSTATE_READER( ALPHAREF, int );
		ADD_PASS_RENDERSTATE_READER( ALPHAFUNC, D3DCMPFUNC );
		ADD_PASS_RENDERSTATE_READER( TEXTUREFACTOR, Vector4 );

		passProcessors_.insert( std::make_pair( "textureStage", new PassTextureStageReader ) );
		passProcessors_.insert( std::make_pair( "fogColour", new PassFogColourReader ) );
		passProcessors_.insert( std::make_pair( "vertexShader", new PassVertexShaderReader ) );
		passProcessors_.insert( std::make_pair( "vertexShaderSet", new PassVertexShaderSetReader ) );
		passProcessors_.insert( std::make_pair( "pixelShader", new PassPixelShaderReader ) );
/*		passProcessors_.insert( std::make_pair( "fixedVertex", new PassFixedVertexReader ) );*/
	}
	PassProcessors passProcessors_;
};

/**
 * set method
 * @param pData render data that is going to be used to render the pass
 */
void Pass::set( RenderData* pData )
{
	RenderStates::iterator it = renderStates_.begin();
	RenderStates::iterator end = renderStates_.end();

	// Set up generic states
	while (it != end)
	{
		rc().setRenderState( it->first, it->second );
		it++;
	}

	// set up implied states
	rc().setRenderState( D3DRS_ZWRITEENABLE, zBufferWrite_ );
	rc().setRenderState( D3DRS_ZENABLE, zBufferWrite_ || zBufferRead_ );
	rc().setRenderState( D3DRS_ZFUNC, zBufferRead_ ? D3DCMP_LESSEQUAL : D3DCMP_ALWAYS );
	rc().setRenderState( D3DRS_CULLMODE, doubleSided_ ? D3DCULL_NONE : D3DCULL_CCW );


	if (fogged_)
	{
		rc().setRenderState( D3DRS_FOGENABLE, TRUE );
		if (fogOverride_)
		{
			rc().setRenderState( D3DRS_FOGCOLOR, fogColour_ );
		}
		else
		{
			rc().setRenderState( D3DRS_FOGCOLOR, Material::standardFogColour() );
		}
	}
	else
	{
		rc().setRenderState( D3DRS_FOGENABLE, FALSE );
	}

	for (uint32 i = 0; i < textureStages_.size(); i++)
	{
		textureStages_[i]->set( i );
	}

	if (pVertexOperation_)
		pVertexOperation_->set( pData );

	if (pPixelOperation_)
		pPixelOperation_->set( pData );

}

Pass::Pass() :
zBufferRead_( true ),
zBufferWrite_( true ),
doubleSided_( false ),
fogged_( true ),
fogOverride_( false ),
fogColour_( 0 ),
pPixelOperation_( NULL ),
pVertexOperation_( NULL )
{

}

Pass::~Pass()
{
	TextureStages::iterator it = textureStages_.begin();
	TextureStages::iterator end = textureStages_.end();
	while (it != end)
	{
		delete *it;
		it++;
	}
	if (pPixelOperation_)
		delete pPixelOperation_;
	if (pVertexOperation_)
		delete pVertexOperation_;
}

bool Pass::load( DataSectionPtr pSection, const Samplers& samplers  )
{
	PassReader::instance().load( *this, pSection, samplers );
	return true;
}

// -----------------------------------------------------------------------------
// Section: TextureStage
// -----------------------------------------------------------------------------

/*
 * Base load functor
 */
class TSFunctor
{
public:
	virtual ~TSFunctor(){};
	virtual bool operator()( Pass::TextureStage& stage, DataSectionPtr pData, const Samplers& samplers  ) const = 0;
private:
};

/*
 * Texture argument reader functor
 */
class TArgReader : public TSFunctor
{
public:
	TArgReader( D3DTEXTURESTAGESTATETYPE state ):
	  state_( state )
	{
	}
	~TArgReader(){};
	bool operator()( Pass::TextureStage& stage, DataSectionPtr pData, const Samplers& samplers  ) const
	{
		if (argumentMap_.empty())
		{
			#define TARG_ENTRY( x ) \
				argumentMap_.insert( std::make_pair( #x, D3DTA_##x ) );
			TARG_ENTRY( DIFFUSE );
			TARG_ENTRY( CURRENT );
			TARG_ENTRY( TEXTURE );
			TARG_ENTRY( TFACTOR );
			TARG_ENTRY( SPECULAR );
			TARG_ENTRY( TEMP );
			TARG_ENTRY( CONSTANT );
		}
		ArgumentMap::iterator it = argumentMap_.find( pData->asString() );
		if (it != argumentMap_.end())
		{
			stage.states_.push_back( std::make_pair( state_, it->second ) );
			return false;
		}
		else
		{
			ERROR_MSG( "TArgReader::() - Unknown textureArgument %s\n",
				pData->asString().c_str() );
		}
		return true;
	}
private:
	typedef std::map<std::string, DWORD> ArgumentMap;
	static ArgumentMap	argumentMap_;

	D3DTEXTURESTAGESTATETYPE	state_;
};

TArgReader::ArgumentMap TArgReader::argumentMap_;

/*
 * Texture operation reader functor
 */
class TOPReader : public TSFunctor
{
public:
	TOPReader( D3DTEXTURESTAGESTATETYPE state ):
	  state_( state )
	{
	}
	~TOPReader(){};
	bool operator()( Pass::TextureStage& stage, DataSectionPtr pData, const Samplers& samplers  ) const
	{
		if (argumentMap_.empty())
		{
			#define TOP_ENTRY( x ) \
				argumentMap_.insert( std::make_pair( #x, D3DTOP_##x ) );
			TOP_ENTRY( DISABLE );
			TOP_ENTRY( SELECTARG1 );
			TOP_ENTRY( SELECTARG2 );
			TOP_ENTRY( MODULATE );
			TOP_ENTRY( MODULATE2X );
			TOP_ENTRY( MODULATE4X );
			TOP_ENTRY( ADD );
			TOP_ENTRY( ADDSIGNED );
			TOP_ENTRY( ADDSIGNED2X );
			TOP_ENTRY( SUBTRACT );
			TOP_ENTRY( ADDSMOOTH );
			TOP_ENTRY( BLENDDIFFUSEALPHA );
			TOP_ENTRY( BLENDTEXTUREALPHA );
			TOP_ENTRY( BLENDFACTORALPHA );
			TOP_ENTRY( BLENDTEXTUREALPHAPM );
			TOP_ENTRY( BLENDCURRENTALPHA );
			TOP_ENTRY( PREMODULATE );
			TOP_ENTRY( MODULATEALPHA_ADDCOLOR );
			TOP_ENTRY( MODULATECOLOR_ADDALPHA );
			TOP_ENTRY( MODULATEINVALPHA_ADDCOLOR );
			TOP_ENTRY( MODULATEINVCOLOR_ADDALPHA );
			TOP_ENTRY( BUMPENVMAP );
			TOP_ENTRY( BUMPENVMAPLUMINANCE );
			TOP_ENTRY( DOTPRODUCT3 );
			TOP_ENTRY( MULTIPLYADD );
			TOP_ENTRY( LERP );
		}
		ArgumentMap::iterator it = argumentMap_.find( pData->asString() );
		if (it != argumentMap_.end())
		{
			stage.states_.push_back( std::make_pair( state_, it->second ) );
		}
		else
		{
			ERROR_MSG( "TArgReader::() - Unknown textureArgument %s\n",
				pData->asString().c_str() );
			return false;
		}
		return true;
	}
private:
	typedef std::map<std::string, D3DTEXTUREOP> ArgumentMap;
	static ArgumentMap	argumentMap_;

	D3DTEXTURESTAGESTATETYPE	state_;
};

TOPReader::ArgumentMap TOPReader::argumentMap_;


/*
 * Float reader functor
 */
class TSFloatReader : public TSFunctor
{
public:
	TSFloatReader( D3DTEXTURESTAGESTATETYPE state ):
	  state_( state )
	{
	}
	~TSFloatReader(){};
	bool operator()( Pass::TextureStage& stage, DataSectionPtr pData, const Samplers& samplers  ) const
	{
		stage.states_.push_back( std::make_pair( state_, toDWORD( pData->asFloat() ) ) );
		return true;
	}
private:
	D3DTEXTURESTAGESTATETYPE	state_;
};

/*
 * Vector4 reader functor
 */
class TSVector4Reader : public TSFunctor
{
public:
	TSVector4Reader( D3DTEXTURESTAGESTATETYPE state ):
	  state_( state )
	{
	}
	~TSVector4Reader(){};
	bool operator()( Pass::TextureStage& stage, DataSectionPtr pData, const Samplers& samplers  ) const
	{
		stage.states_.push_back( std::make_pair( state_, toDWORD( pData->asVector4() ) ) );
		return true;
	}
private:
	D3DTEXTURESTAGESTATETYPE	state_;
};

/*
 * Texture coordinate index reader functor
 */
class TSTexCoordIndexReader : public TSFunctor
{
public:
	~TSTexCoordIndexReader(){};
	bool operator()( Pass::TextureStage& stage, DataSectionPtr pData, const Samplers& samplers  ) const
	{
		DWORD tci = pData->asInt();
		DataSectionPtr pGenerateSection = pData->openSection( "generation" );
		if (pGenerateSection)
		{
			typedef std::map<std::string, DWORD> TCIMap;
			static TCIMap tciMap;
			if (tciMap.empty())
			{
#define TCI_ENTRY( x ) tciMap.insert( std::make_pair(#x, D3DTSS_TCI_##x) )
				TCI_ENTRY( PASSTHRU );
				TCI_ENTRY( CAMERASPACENORMAL );
				TCI_ENTRY( CAMERASPACEPOSITION );
				TCI_ENTRY( CAMERASPACEREFLECTIONVECTOR );
				TCI_ENTRY( SPHEREMAP );
			}
			TCIMap::iterator it = tciMap.find( pGenerateSection->asString() );
			if (it != tciMap.end())
			{
				tci |= it->second;
			}
			else
			{
				ERROR_MSG( "TSTexCoordIndexReader::() - unknown generation %s", pGenerateSection->asString().c_str() );
			}
		}

		stage.states_.push_back( std::make_pair( D3DTSS_TEXCOORDINDEX, tci ) );
		return true;
	}
private:
};

/*
 * Texture transform reader functor
 */
class TSTextureTransformReader : public TSFunctor
{
public:
	~TSTextureTransformReader(){};

	bool operator()( Pass::TextureStage& stage, DataSectionPtr pData, const Samplers& samplers  ) const
	{
		DWORD ttf;
		ttf = pData->asInt();
		if (pData->readBool( "projected", false ))
		{
			ttf |= D3DTTFF_PROJECTED;
		}

		stage.states_.push_back( std::make_pair( D3DTSS_TEXTURETRANSFORMFLAGS, ttf ) );
		return true;
	}
};

/*
 * Sampler reader functor
 */
class TSSamplerReader : public TSFunctor
{
public:
	~TSSamplerReader() {}
	bool operator () ( Pass::TextureStage& stage, DataSectionPtr pData, const Samplers& samplers  ) const
	{
		std::string sampName = pData->asString();
		Samplers::const_iterator it = samplers.begin();
		Samplers::const_iterator end = samplers.end();

		bool res = false;
		while ((it != end) && (res == false))
		{
			if ((*it)->identifier() == sampName)
			{
				stage.sampler_ = *it;
				res = true;
			}
			it++;
		}
        if (res == false)
		{
			ERROR_MSG( "TSSamplerReader::() - No sampler %s defined in material\n", sampName.c_str() );
		}
		return res;
	}
};

/*
 * Texture stage loader class
 */
class TSReader
{
public:
	~TSReader(){};

	static TSReader& instance()
	{
		static TSReader inst;
		return inst;
	}

	bool load( Pass::TextureStage& stage, DataSectionPtr pSection, const Samplers& samplers )
	{
		DataSectionIterator it = pSection->begin();
		DataSectionIterator end = pSection->end();
		while (it != end)
		{
			TSProcessors::iterator processor = tsProcessors_.find( it->sectionName() );
			if (processor != tsProcessors_.end())
			{
				(*processor->second)( stage, *it, samplers );
			}
			else
			{
				ERROR_MSG( "TSReader::load - don't know how to load %s\n", it->sectionName().c_str() );
			}
			it++;
		}

		return true;
	}

private:
	TSReader()
	{
		// 
		#define TSOP_ENTRY( x, y ) \
			tsProcessors_.insert( std::make_pair( #x, new y( D3DTSS_##x ) ) );

		TSOP_ENTRY( ALPHAOP, TOPReader );
		TSOP_ENTRY( ALPHAARG0, TArgReader );
		TSOP_ENTRY( ALPHAARG1, TArgReader );
		TSOP_ENTRY( ALPHAARG2, TArgReader );
		TSOP_ENTRY( COLOROP, TOPReader );
		TSOP_ENTRY( COLORARG0, TArgReader );
		TSOP_ENTRY( COLORARG1, TArgReader );
		TSOP_ENTRY( COLORARG2, TArgReader );
		TSOP_ENTRY( CONSTANT, TSVector4Reader );
		TSOP_ENTRY( BUMPENVMAT00, TSFloatReader );
		TSOP_ENTRY( BUMPENVMAT01, TSFloatReader );
		TSOP_ENTRY( BUMPENVMAT10, TSFloatReader );
		TSOP_ENTRY( BUMPENVMAT11, TSFloatReader );
		TSOP_ENTRY( BUMPENVLSCALE, TSFloatReader );
		TSOP_ENTRY( BUMPENVLOFFSET, TSFloatReader );
		TSOP_ENTRY( RESULTARG, TArgReader );
		tsProcessors_["TEXCOORDINDEX"] = new TSTexCoordIndexReader;
		tsProcessors_["TEXTURETRANSFORMFLAGS"] = new TSTextureTransformReader;
		tsProcessors_["sampler"] = new TSSamplerReader;
	}
	typedef std::map<std::string, TSFunctor* > TSProcessors;
	TSProcessors	tsProcessors_;
};

/**
 *	This method loads the texturestage for the pass
 *	@param pSection the datasection to load the texture stage from.
 *	@return true if successful, false if there is an error
 */
bool Pass::TextureStage::load( DataSectionPtr pSection, const Samplers& samplers  )
{
	TSReader::instance().load( *this, pSection, samplers );
	return true;
}

void Pass::TextureStage::set( uint32 stage )
{
	if (sampler_)
		sampler_->set( stage );
	else
		Moo::rc().setTexture( stage, NULL );
	States::iterator it = states_.begin();
	States::iterator end = states_.end();
	while (it != end)
	{
		Moo::rc().setTextureStageState( stage, it->first, it->second );
		it++;
	}
}

}

// pass.cpp
