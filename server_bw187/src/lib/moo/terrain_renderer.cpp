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

#include "terrain_renderer.hpp"
#include "terrain_block.hpp"
#include "terrain_texture_setter.hpp"
#include "effect_visual_context.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/auto_config.hpp"


#ifndef CODE_INLINE
#include "terrain_renderer.ipp"
#endif

DECLARE_DEBUG_COMPONENT2( "Moo", 0 )

bool g_drawTerrain = true;

static AutoConfigString s_fogGradientBmpName( "environment/fogGradientBmpName");
static BasicAutoConfig< float > s_terrainTextureSpacing( "environment/terrainTextureSpacing", 10.0f );
static AutoConfigString s_effectName( "system/terrainEffect" );

namespace Moo
{

// explicit casting method for float to dword bitwise conversion (ish).
inline DWORD toDWORD( float f )
{
	return *(DWORD*)(&f);
}

static float s_penumbraSize = 0.1f;
static float s_terrainTilingFactor_ = -0.1f;
static TerrainRenderer * s_instance = NULL;
bool TerrainRenderer::s_enableSpecular = true;

TerrainRenderer& TerrainRenderer::instance()
{
	if (s_instance == NULL)
	{
		s_instance = new TerrainRenderer;
	}
	return *s_instance;
}


void TerrainRenderer::fini()
{
	if (s_instance != NULL)
	{
		delete s_instance;
		s_instance = NULL;
	}
}

// -----------------------------------------------------------------------------
// Section: Construction/Destruction
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
TerrainRenderer::TerrainRenderer()
: renderer_( NULL ),
  isVisible_( false )
{
	if ( s_terrainTilingFactor_ < 0.f )
	{
		float scale = s_terrainTextureSpacing.value();
		scale = Math::clamp( 0.01f, scale, 100.f );
		s_terrainTilingFactor_ = 1.f / scale;
		MF_WATCH( "Render/Terrain/texture tile frequency",
				s_terrainTilingFactor_, Watcher::WT_READ_WRITE,
				"Frequency of terrain textures, or, how many textures per metre. "
				"(e.g. 0.1 means 1 texture repeat every 10 metres)." );
	}

	// moved initialization to constructor 
	// to avoid loading files while the game 
	// simulation is already running.
	renderer_ = new EffectFileRenderer;
}


/**
 *	Destructor.
 */
TerrainRenderer::~TerrainRenderer()
{
	delete renderer_;
	renderer_ = NULL;
}

// -----------------------------------------------------------------------------
// Section: Rendering
// -----------------------------------------------------------------------------

/**
 * This method adds a block to the current frames list of blocks to draw.
 * @param transform the world transform of the terrain block
 * @param pBlock the block to be rendered
 */
void TerrainRenderer::addBlock( const Matrix& transform, TerrainBlock * pBlock )
{
	MF_ASSERT( pBlock != NULL );

	blocks_.push_back( BlockInPlace( transform, pBlock ) );
	isVisible_ = true;
}

/**
 * This methods draws the terrain blocks that have been added to the list since
 * the last call to this method.
 */
void TerrainRenderer::draw(Moo::EffectMaterial* alternateEffect)
{
	Moo::EffectVisualContext::instance().isOutside( true );

	if (blocks_.size() && rc().device() && g_drawTerrain)
	{
		if (!renderer_)
		{
			createUnmanagedObjects();
		}
		if (renderer_)
			renderer_->draw( blocks_, alternateEffect);
		else
			blocks_.clear();

	}
	else
	{
		blocks_.clear();
	}
}

// -----------------------------------------------------------------------------
// Section: Setup
// -----------------------------------------------------------------------------

/**
 *	This method is an implementation of Moo::DeviceCallback::createUnmanagedObjects.
 *	The terrain renderer for the available hardware should be created here.
 */
void TerrainRenderer::createUnmanagedObjects()
{}

/**
 *	This method is an implementation of Moo::DeviceCallback::createUnmanagedObjects.
 *	The device specific portion of the terrainrenderer should be deleted here.
 */
void TerrainRenderer::deleteUnmanagedObjects()
{}

// -----------------------------------------------------------------------------
// Section: Renderers
// -----------------------------------------------------------------------------

/**
 * Virtual destructor for the devicespecific renderer object.
 */
TerrainRenderer::Renderer::~Renderer()
{
}

class SunAngleConstantSetter : public EffectConstantValue
{
	bool SunAngleConstantSetter::operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		Vector4 value(0.5,0.5,0.5,0.5);

		LightContainerPtr pLC = rc().lightContainer();
		if (pLC)
		{
			float f = 0.5f;
			f = -atan2f( pLC->directionals()[0]->worldDirection().y,
				pLC->directionals()[0]->worldDirection().x );
			f = float( ( f + ( MATH_PI * 1.5 ) ) / ( MATH_PI * 2 ) );
			f = fmodf( f, 1 );
			f = min( 1.f, max( (f * 2.f ) - 0.5f, 0.f ) );
			value.set(f,f,f,f);
		}
		
		pEffect->SetVector(constantHandle, &value);
		return true;
	}
};

class PenumbraSizeSetter : public EffectConstantValue
{
	bool PenumbraSizeSetter::operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{		
		float psReciprocal = 1.f / s_penumbraSize;
		Vector4 value( psReciprocal, psReciprocal, psReciprocal, psReciprocal );
		pEffect->SetVector(constantHandle, &value);
		return true;
	}
};

class TextureTransformSetter : public EffectConstantValue
{
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{		
		Vector4 transform[2];
		transform[0].set( s_terrainTilingFactor_, 0.f, 0.f, 0.f );
		transform[1].set( 0.f, 0.f, s_terrainTilingFactor_, 0.f );
		pEffect->SetVectorArray(constantHandle, transform, 2);		
		return true;
	}
};

class FogTransformSetter : public EffectConstantValue
{
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{	
		// Calculate the fog transform matrix, we only want to worry about the z
		// component of the camera space position and map that onto our texture
		// according to fog begin and end.
		float fogBeg = rc().fogNear();
		float fogEnd = rc().fogFar();

		Matrix transform;
		transform.setIdentity();
		transform[0][0] = 0.f;
		transform[2][0] = 1.f / ( fogEnd - fogBeg );
		transform[3][0] = -fogBeg * transform[2][0];		

		pEffect->SetMatrix(constantHandle, &transform);
		return true;
	}
};

class FogTextureSetter : public EffectConstantValue
{
public:
	FogTextureSetter()
	{
		pFogGradientBmp_ = TextureManager::instance()->get(s_fogGradientBmpName);
	}

	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{				
		if (pFogGradientBmp_ && pFogGradientBmp_->pTexture())
			pEffect->SetTexture(constantHandle,pFogGradientBmp_->pTexture());
		else
			WARNING_MSG( "Could not set fog gradient bitmap" );
		return true;
	}

	BaseTexturePtr pFogGradientBmp_;
};


static float s_specularDiffuseAmount = 0.2f;
static float s_specularPower = 16.f;
static float s_specularMultiplier = 1.f;
static float s_specularFresnelExp = 5.f;
static float s_specularFresnelConstant = 0.f;

/**
 * Constructor for the effect file renderer.
 */
TerrainRenderer::EffectFileRenderer::EffectFileRenderer()
: material_( NULL ),
  textureSetter_( NULL )  
{
	// Create the effect.
	if (rc().device())
	{
		material_ = new EffectMaterial;
		DataSectionPtr pSect = BWResource::openSection( s_effectName );
		if (pSect && material_->load(pSect))
		{
			ComObjectWrap<ID3DXEffect> pEffect = material_->pEffect()->pEffect();
			pEffect->GetFloat( "specularDiffuseAmount", &s_specularDiffuseAmount );
			pEffect->GetFloat( "specularPower", &s_specularPower );
			pEffect->GetFloat( "specularMultiplier", &s_specularMultiplier );
			pEffect->GetFloat( "fresnelExp", &s_specularFresnelExp );
			pEffect->GetFloat( "fresnelConstant", &s_specularFresnelConstant );
			static bool watcherAdded = false;
			if ( !watcherAdded )
			{
				watcherAdded = true;
				MF_WATCH( "Render/Terrain/specular diffuse amount", s_specularDiffuseAmount,
						Watcher::WT_READ_WRITE,
						"Amount of diffuse colour to add to the specular colour." );
				MF_WATCH( "Render/Terrain/specular power", s_specularPower,
						Watcher::WT_READ_WRITE,
						"Mathematical power of the specular reflectance function." );
				MF_WATCH( "Render/Terrain/specular multiplier", s_specularMultiplier,
						Watcher::WT_READ_WRITE,
						"Overall multiplier on the terrain specular effect." );
				MF_WATCH( "Render/Terrain/specular fresnel constant", s_specularFresnelConstant,
						Watcher::WT_READ_WRITE,
						"Fresnel constant for falloff calculations." );
				MF_WATCH( "Render/Terrain/specular fresnel falloff", s_specularFresnelExp,
						Watcher::WT_READ_WRITE,
						"Fresnel falloff rate." );
			}
			textureSetter_ = new EffectFileTextureSetter(pEffect);
		}
		else
		{
			CRITICAL_MSG( "Moo::TerrainRenderer::EffectFileRenderer::"
				"EffectFileRenderer - couldn't load effect material "
				"%s\n", s_effectName.value().c_str() );			
		}		
	}

	*EffectConstantValue::get( "SunAngle" ) = new SunAngleConstantSetter;
	*EffectConstantValue::get( "PenumbraSize" ) = new PenumbraSizeSetter;
	*EffectConstantValue::get( "TerrainTextureTransform" ) = new TextureTransformSetter;
	*EffectConstantValue::get( "FogTextureTransform" ) = new FogTransformSetter;
	*EffectConstantValue::get( "FogGradientTexture" ) = new FogTextureSetter;	

	pDecl_ = VertexDeclaration::get( "xyznds" );
}


/**
 * Destructor.
 */
TerrainRenderer::EffectFileRenderer::~EffectFileRenderer()
{
	if (material_)
		delete material_;

	if (textureSetter_)
		delete textureSetter_;	
}

/**
 * This method is the implementation of Renderer::draw. Its the main draw function
 * for the terrain.
 * @param blocks vector of blocks to be rendered.
 * @param alternateMaterial an optional material with which to draw
 */
void TerrainRenderer::EffectFileRenderer::draw( BlockVector& blocks,
	EffectMaterial* alternateMaterial )
{
	Moo::EffectMaterial* old = material_;
	if (alternateMaterial)
	{		
		material_ = alternateMaterial;
		textureSetter_->effect(material_->pEffect()->pEffect());
	}

	sortBlocks( blocks );	
	setInitialRenderStates();	
 	if (Moo::rc().mixedVertexProcessing())
 		Moo::rc().device()->SetSoftwareVertexProcessing(FALSE);
	renderBlocks( blocks );
 	if (Moo::rc().mixedVertexProcessing())
		Moo::rc().device()->SetSoftwareVertexProcessing(TRUE);

	
	if (alternateMaterial)
	{
		material_ = old;
		textureSetter_->effect(material_->pEffect()->pEffect());
	}
}


/**
 * This method will eventually sort the terrain blocks.
 */
void TerrainRenderer::EffectFileRenderer::sortBlocks( BlockVector& blocks )
{
	//TODO: something clever!
}

/**
 *	This method sets render states that only need be set once for the whole
 *	terrain.
 */
void TerrainRenderer::EffectFileRenderer::setInitialRenderStates()
{
	#define SAFE_SET( type, name, value ) \
	{\
		D3DXHANDLE h = pEffect->GetParameterByName( NULL, name );\
		if (h)\
		{\
			pEffect->Set##type( h, value );\
		}\
	}
	EffectVisualContext::instance().initConstants();
	rc().setVertexDeclaration( pDecl_->declaration() );
	ComObjectWrap<ID3DXEffect> pEffect = material_->pEffect()->pEffect();	
	SAFE_SET( Matrix, "view", &rc().view() );
	SAFE_SET( Matrix, "proj", &rc().projection() );
	SAFE_SET( Float, "specularDiffuseAmount", s_specularDiffuseAmount );
	SAFE_SET( Float, "specularPower", s_specularPower );
	SAFE_SET( Float, "specularMultiplier", TerrainRenderer::s_enableSpecular ? s_specularMultiplier : 0.f );
	SAFE_SET( Float, "fresnelExp", TerrainRenderer::s_enableSpecular ? s_specularFresnelExp : 0.f );
	SAFE_SET( Float, "fresnelConstant", TerrainRenderer::s_enableSpecular ? s_specularFresnelConstant : 0.f );
	if (rc().lightContainer())
		rc().lightContainer()->commitToFixedFunctionPipeline();
}

/**
 * This method calculates the texture transform for the fixed function pipeline.
 */
void TerrainRenderer::EffectFileRenderer::textureTransform( const Matrix& world, Matrix& ret ) const
{
	ret.multiply( world, rc().view() );	
	ret.invert();
	Matrix rotatem;
	rotatem.setRotateX( DEG_TO_RAD( 90 ) );
	Matrix scalem;
	scalem.setScale( s_terrainTilingFactor_, s_terrainTilingFactor_, s_terrainTilingFactor_ );
	ret.postMultiply( rotatem );
	ret.postMultiply( scalem );
}

/**
 * This method iterates through the terrain blocks and renders them.
 * @param blocks vector of blocks to render.
 */
void TerrainRenderer::EffectFileRenderer::renderBlocks( BlockVector& blocks )
{
	ComObjectWrap<ID3DXEffect> pEffect = material_->pEffect()->pEffect();
	D3DXHANDLE wvp = pEffect->GetParameterByName(NULL,"worldViewProj");
	D3DXHANDLE vp = pEffect->GetParameterByName(NULL,"viewProj");
	D3DXHANDLE world = pEffect->GetParameterByName(NULL,"world");	
	D3DXHANDLE tex = pEffect->GetParameterByName(NULL,"textransform");

	if (material_->begin())
	{
		BlockVector::iterator it = blocks.begin();
		BlockVector::iterator end = blocks.end();
		
		while (it != end)
		{
			Matrix& m = it->first;

			/* for shader technique */
			Matrix full(m);
			full.postMultiply(rc().viewProjection());
			if (wvp)
				pEffect->SetMatrix( wvp, &full );

			/* for texture stage renderer*/
			if (world)
				pEffect->SetMatrix( world, &m );
			Matrix tt;
			this->textureTransform(m,tt);
			if (tex)
				pEffect->SetMatrix( tex, &tt );

			if (vp)
				pEffect->SetMatrix( vp, &rc().viewProjection() );

			pEffect->CommitChanges();
			
			for (uint32 i=0; i<material_->nPasses(); i++)
			{
				if (material_->beginPass(i))
				{
					it->second->draw(textureSetter_);
					material_->endPass();
				}
			}

			it++;			
		}				

		material_->end();		
	}
	blocks.clear();

	//have to do this because the fixed function effect file sets the transform
	//flags to 2, and must be disabled for every other object.
	rc().setTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	rc().setTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);

	//for shader 1.1 hardware, and the flora light map which also uses the alpha
	//channel and this renderer.
	rc().setRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_GREEN|D3DCOLORWRITEENABLE_BLUE|D3DCOLORWRITEENABLE_RED);
}


/**
 *	Adjusts the effect settings related to sky lightmapping. Tests 
 *	hardware capabilities and forces disable lightmapping is not supported. 
 *	Also, makes sure the setting is present, otherwise fx compilation will fail.
 */
void TerrainRenderer::configureKeywordSetting(Moo::EffectMacroSetting & setting)
{
	bool supported = Moo::rc().psVersion() >= 0x0100;
	setting.addOption("ON", supported, "1");
	setting.addOption("OFF", true, "0");
}


Moo::EffectMacroSetting * TerrainRenderer::s_terrainSpeculerSetting = 
	new Moo::EffectMacroSetting(
		"TERRAIN_SPECULAR", "TERRAIN_SPECULAR_ENABLE",
		&TerrainRenderer::configureKeywordSetting);




} //namespace Moo

// terrain_renderer.cpp
