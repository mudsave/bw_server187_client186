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
#include "bloom_effect.hpp"
#include "transfer_mesh.hpp"
#include "back_buffer_copy.hpp"
#include "resmgr/auto_config.hpp"

#include "resmgr/bwresource.hpp"
#include "texture_feeds.hpp"
#include "cstdmf/debug.hpp"
#include "full_screen_back_buffer.hpp"

DECLARE_DEBUG_COMPONENT2( "Romp", 0 );

AutoConfigString s_downSampleEffect( "system/bloom/downSample" );
AutoConfigString s_colourScaleEffect( "system/bloom/colourScale" );
AutoConfigString s_gaussianBlurEffect( "system/bloom/gaussianBlur" );
AutoConfigString s_transferEffect( "system/bloom/transfer" );

static FilterSample Filter4[] = {        // 12021 4-tap filter
	{ 1.f/6.f, -2.5f },
	{ 2.f/6.f, -0.5f },
	{ 2.f/6.f,  0.5f },
	{ 1.f/6.f,  2.5f },
};

static FilterSample Filter24[] = {
	{ 0.3327f,-10.6f },
	{ 0.3557f, -9.6f },
	{ 0.3790f, -8.6f },
	{ 0.4048f, -7.6f },
	{ 0.4398f, -6.6f },
	{ 0.4967f, -5.6f },
	{ 0.5937f, -4.6f },
	{ 0.7448f, -3.6f },
	{ 0.9418f, -2.6f },
	{ 1.1414f, -1.6f },
	{ 1.2757f, -0.6f },
	{ 1.2891f,  0.4f },
	{ 1.1757f,  1.4f },
	{ 0.9835f,  2.4f },
	{ 0.7814f,  3.4f },
	{ 0.6194f,  4.4f },
	{ 0.5123f,  5.4f },
	{ 0.4489f,  6.4f },
	{ 0.4108f,  7.4f },
	{ 0.3838f,  8.4f },
	{ 0.3603f,  9.4f },
	{ 0.3373f, 10.4f },
	{ 0.0000f,  0.0f },
	{ 0.0000f,  0.0f },
};

#define SETTINGS_ENABLED (this->bloomSettings_->activeOption() == 0)

Bloom::Bloom():
    renderTargetWidth_(0),
	renderTargetHeight_(0),
	transferMesh_(NULL),
	inited_(false),
	watcherEnabled_(true),
	bbc_(NULL),
	rt0_(NULL),
	rt1_(NULL),
	wasteOfMemory_(NULL),
	filterMode_(1),
	colourAtten_ (0.9f),
	bbWidth_(0),
	bbHeight_(0),
	bloomBlur_( true ),
	nPasses_(2),
	downSample_( NULL ),
	gaussianBlur_( NULL ),
	colourScale_( NULL ),
	transfer_( NULL )
#ifdef EDITOR_ENABLED
	,editorEnabled_(true)
#endif
{		
	// bloom filter settings
	this->bloomSettings_ = 
		Moo::makeCallbackGraphicsSetting(
			"BLOOM_FILTER", *this, 
			&Bloom::setBloomOption, 
			Bloom::isSupported() ? 0 : 1,
			false, false);
				
	this->bloomSettings_->addOption("ON", Bloom::isSupported());
	this->bloomSettings_->addOption("OFF", true);
	Moo::GraphicsSetting::add(this->bloomSettings_);

	if (Bloom::isSupported())
	{
		MF_WATCH( "Client Settings/fx/Bloom/enable",
			watcherEnabled_,
			Watcher::WT_READ_WRITE,
			"Enable the full-screen blooming effect," );
		MF_WATCH( "Client Settings/fx/Bloom/filter mode", filterMode_,
			Watcher::WT_READ_WRITE,
			"Gaussian blur filter kernel mode, either 0 (4x4 kernel, "
			"faster) or 1 (24x24 kernel, slower)." );
		MF_WATCH( "Client Settings/fx/Bloom/colour attenuation",
			colourAtten_,
			Watcher::WT_READ_WRITE,
			"Colour attenuation per-pass.  Should be set much lower if using "
			"the 24x24 filter kernel." );
		MF_WATCH( "Client Settings/fx/Bloom/bloom and blur",
			bloomBlur_,
			Watcher::WT_READ_WRITE,
			"If set to true, then blooming AND blurring occur.  If set to false"
			", only the blur takes place (and is not overlaid on the screen.)" );
		MF_WATCH( "Client Settings/fx/Bloom/num passes",
			nPasses_,
			Watcher::WT_READ_WRITE,
			"Set the number of blurring passes applied to the bloom texture." );		
	}

	FullScreenBackBuffer::addUser( this );
}


Bloom::~Bloom()
{
	FullScreenBackBuffer::removeUser( this );

	this->finz();

	if ( rt0_ ) delete rt0_;
	if ( rt1_ ) delete rt1_;
	if ( wasteOfMemory_ ) delete wasteOfMemory_;
	if ( downSample_ ) delete downSample_;
	if ( gaussianBlur_ ) delete gaussianBlur_;
	if ( colourScale_ ) delete colourScale_;
	if ( transfer_ ) delete transfer_;
}


bool Bloom::isSupported()
{
	if (Moo::rc().vsVersion() < 0x101)
	{
		INFO_MSG( "Blooming is not supported because the vertex shader version is not sufficient\n" );
		return false;
	}
	if (Moo::rc().psVersion() < 0x101)
	{
		INFO_MSG( "Blooming is not supported because the pixel shader version is not sufficient\n" );
		return false;
	}
	if (!BWResource::openSection( s_downSampleEffect ))
	{
		INFO_MSG( "Blooming is not supported because the down sample effect could not be found\n" );
		return false;
	}
	if (!BWResource::openSection( s_gaussianBlurEffect ))
	{
		INFO_MSG( "Blooming is not support because the gaussian blur effect could not be found\n" );
		return false;
	}
	if (!BWResource::openSection( s_colourScaleEffect ))
	{
		INFO_MSG( "Blooming is not supported because the gaussian blur effect could not be found\n" );
		return false;
	}	
	if (!BWResource::openSection( s_transferEffect ) )
	{
		INFO_MSG( "Blooming is not supported because the transfer effect could not be found\n" );
		return false;
	}

	const Moo::DeviceInfo& di = Moo::rc().deviceInfo( Moo::rc().deviceIndex() );

	//TODO : relax this constraint and support blooming using next-power-of-2-up textures.
	if (di.caps_.TextureCaps & D3DPTEXTURECAPS_POW2 && !(di.caps_.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL))
	{
		INFO_MSG( "Blooming is not supported because non-power of 2 textures are not supported\n" );
		return false;
	}

	return true;
}


bool Bloom::isEnabled()
{
	bool enabled = (inited_ && SETTINGS_ENABLED && watcherEnabled_);
#ifdef EDITOR_ENABLED
	enabled &= editorEnabled_;
#endif
	return enabled;
}


/**
 *	This method allocates pointer rt if it is null, and then calls
 *	render target create.  For non-editors it registers the texture
 *	map as a texture feed as well.
 *
 *	@return true if the render target texture was created succesfully.
 */
bool Bloom::safeCreateRenderTarget(
	Moo::RenderTarget*& rt,
	int width,
	int height,
	bool reuseZ,
	const std::string& name )
{
	if ( !rt ) rt = new Moo::RenderTarget( name );
	if (rt->create( width, height, reuseZ ))
	{
		TextureFeeds::addTextureFeed(name,new PyTextureProvider(NULL,rt));
	}
	return (rt->pTexture() != NULL);
}


/**
 *	This method allocates pointer mat if it is null, and initialises
 *	the material from the given effect name.
 *	If anything fails, the material pointer is freed.
 *
 *	@return true if the effect material was successfully initialised.
 */
bool Bloom::safeCreateEffect( Moo::EffectMaterial*& mat, const std::string& effectName )
{
	DataSectionPtr pSection = BWResource::openSection(effectName);
	if ( pSection )
	{
		if (!mat) mat = new Moo::EffectMaterial;
		if (mat->initFromEffect(effectName))
		{
			return true;
		}
		delete mat;
		mat = NULL;
	}
	return false;
}


bool Bloom::init()
{
	if ( inited_ )
		return true;

	bbWidth_ = (int)Moo::rc().screenWidth();
	bbHeight_ = (int)Moo::rc().screenHeight();
    renderTargetWidth_ = std::max(1, bbWidth_ >> 2);
	renderTargetHeight_ = std::max(1, bbHeight_ >> 2);

	if ( bbWidth_ == 0 || bbHeight_ == 0 )
		return false;

	transferMesh_ = new SimpleTransfer;
	bbc_ = new RectBackBufferCopy;
	bbc_->init();

	//TODO : find out another way to better use memory
	if (!safeCreateRenderTarget( wasteOfMemory_, bbWidth_, bbHeight_, true, "wasteOfMemory" ))
	{
		ERROR_MSG( "Could not create texture pointer for bloom render target W.O.M\n" );
		return false;
	}
	
	//Render target 0 is a quarter size target.
	if (!safeCreateRenderTarget( rt0_, renderTargetWidth_, renderTargetHeight_, false, "bloom" ))	
	{
		ERROR_MSG( "Could not create texture pointer for bloom render target 0\n" );
		return false;
	}

	//Render target 1 is also a quarter size target.
	if (!safeCreateRenderTarget( rt1_, renderTargetWidth_, renderTargetHeight_, false, "bloom2" ))
	{
		ERROR_MSG( "Could not create texture pointer for bloom render target 1\n" );
		return false;
	}

	// Create the shaders
	if (!safeCreateEffect( downSample_, s_downSampleEffect ) )
	{
		ERROR_MSG( "Could not load effect material for the downsample effect\n" );		
		return false;
	}

	if (!safeCreateEffect( gaussianBlur_, s_gaussianBlurEffect ) )
	{
		ERROR_MSG( "Could not load effect material for the gaussian blur effect\n" );		
		return false;
	}

	if (!safeCreateEffect( colourScale_, s_colourScaleEffect ) )
	{
		ERROR_MSG( "Could not load effect material for the colour scale effect\n" );		
		return false;
	}

	if (!safeCreateEffect( transfer_, s_transferEffect ) )
	{
		ERROR_MSG( "Could not load effect material for the transfer effect\n" );		
		return false;
	}

	downSampleParameters_.effect( downSample_->pEffect()->pEffect() );
	colourScaleParameters_.effect( colourScale_->pEffect()->pEffect() );
	gaussianParameters_.effect( gaussianBlur_->pEffect()->pEffect() );
	transferParameters_.effect( transfer_->pEffect()->pEffect() );	

	inited_ = true;
	return true;
}


void Bloom::finz()
{
	if ( !inited_ )
		return;

	if (Moo::rc().device())
	{
		if ( colourScale_ )
		{
			delete colourScale_;
			colourScale_ = NULL;
		}

		if ( gaussianBlur_ )
		{
			delete gaussianBlur_;
			gaussianBlur_ = NULL;
		}

		if ( downSample_ )
		{
			delete downSample_;
			downSample_ = NULL;
		}
	}

	if(bbc_)
	{
		bbc_->finz();
		delete bbc_;
		bbc_ = NULL;
	}

	if (rt0_)
	{
		rt0_->release();
	}

	if (rt1_)
	{
		rt1_->release();
	}

	if (wasteOfMemory_)
	{
		wasteOfMemory_->release();
	}

	if (transferMesh_)
	{
		delete transferMesh_;
		transferMesh_ = NULL;
	}

	TextureFeeds::delTextureFeed("wasteOfMemory");
	TextureFeeds::delTextureFeed("bloom");
	TextureFeeds::delTextureFeed("bloom2");

	inited_ = false;
}


void Bloom::deleteUnmanagedObjects()
{
	downSampleParameters_.effect( NULL );	
	colourScaleParameters_.effect( NULL );	
	gaussianParameters_.effect( NULL );	
	transferParameters_.effect( NULL );	
}



void Bloom::applyPreset( bool blurOnly, int filterMode, float colourAtten, int nPasses )
{
	bloomBlur_ = !blurOnly;
	filterMode_ = filterMode;
	colourAtten_ = colourAtten;
	nPasses_ = nPasses;
}


void Bloom::doPostTransferFilter()
{
	// TODO : use StretchBlt to capture the backbuffer. 
	// The symptom of this at the moment is the blooming does not move
	// via heat shimmer, and also the player transparency creates a visual
	// discrepancy because the blooming ignores it.
	// if (alreadyTransferred)
	//		copy back buffer back to the full screen surface.
	//

	// Check to see if the temporary buffers need to be re-generated
	// due to a change in the frame buffer sizes..
	if(bbWidth_!= (int)Moo::rc().screenWidth() || bbHeight_ != Moo::rc().screenHeight())
	{
		this->finz();
		this->init();
	}

	if (!inited_)
		return;

	MF_ASSERT( isEnabled() )

	if (!downSampleParameters_.hasEffect())
	{
		downSampleParameters_.effect( downSample_->pEffect()->pEffect() );
		colourScaleParameters_.effect( colourScale_->pEffect()->pEffect() );
		gaussianParameters_.effect( gaussianBlur_->pEffect()->pEffect() );
		transferParameters_.effect( transfer_->pEffect()->pEffect() );
	}

	static DogWatch bloomTimer( "Bloom" );
	ScopedDogWatch btWatcher( bloomTimer );	
	
	Moo::rc().device()->SetTransform( D3DTS_WORLD, &Matrix::identity );
	Moo::rc().device()->SetTransform( D3DTS_VIEW, &Matrix::identity );
	Moo::rc().device()->SetTransform( D3DTS_PROJECTION, &Matrix::identity );
	Moo::rc().setPixelShader( 0 );

	DX::BaseTexture *pSource = NULL;
		
	if( bloomBlur_ )
	{
		// If we're blooming then setup and capture the back buffer
		// to highlight the "bright" colours into the stencil/Z buffer
		this->captureBackBuffer();		
		pSource = wasteOfMemory_->pTexture();
		sourceDimensions_.x = (float)wasteOfMemory_->width();
		sourceDimensions_.y = (float)wasteOfMemory_->height();		
	}
	else
	{
		// If we're only blurring, we can just use the texture in the
		// Fullscreen back buffer.		
		pSource = FullScreenBackBuffer::renderTarget().pTexture();
		sourceDimensions_.x = (float)bbWidth_;
		sourceDimensions_.y = (float)bbHeight_;
	}

	// Early out if there are missing textures.
	if(!pSource)
		return;
	
	// Downsample the current texture using a 16-tap single pass fetch
	srcWidth_ = bbWidth_;
	srcHeight_ = bbHeight_;	

	rt1_->push();
	this->downSample( pSource, 0.25f );
	rt1_->pop();
	
	// Get ready to do n passes on the blurs.
	srcWidth_ = renderTargetWidth_;
	srcHeight_ = renderTargetHeight_;

	uint sampleCount;
	FilterSample* pSamples;
	if( filterMode_ == GAUSS_24X24)
	{
		sampleCount = 24;
		pSamples = &Filter24[0];
	}
	else
	{
		sampleCount = 4;
		pSamples = &Filter4[0];
	}
	
	// Guassian blur
	for ( int p=0; p<nPasses_; p++ )
	{			
		// Apply subsequent filter passes using the selected filter
		// kernel - for different effects just add different kernels
		// NOTE: Number of entrries in the kernel must be a multiple
		// of 4.

		rt0_->push();	
		sourceDimensions_.x = (float)rt1_->width();
		sourceDimensions_.y = (float)rt1_->height();;	
		this->filterCopy( rt1_->pTexture(), sampleCount, pSamples, colourAtten_, true );
		rt0_->pop();
		
		rt1_->push();	
		sourceDimensions_.x = (float)rt0_->width();
		sourceDimensions_.y = (float)rt0_->height();;	
		this->filterCopy( rt0_->pTexture(), sampleCount, pSamples, colourAtten_, false );
		rt1_->pop();
	}
	
	// If we are just creating a blur texture, instead of blooming,
	// then we don't perform a full screen transfer. This will be
	// done at another time.
	if( bloomBlur_ )
	{
		transferParameters_.setTexture( "diffuseMap", rt1_->pTexture() );
		if (transfer_->begin())
		{
			for (uint32 i=0; i<transfer_->nPasses(); i++)
			{
				transfer_->beginPass(i);
				Vector2 tl( -0.5f, -0.5f );
				Vector2 dimensions((float)bbWidth_, (float)bbHeight_);
				transferMesh_->draw( tl, dimensions, Vector2(1.f,1.f), true );
				transfer_->endPass();
			}
			transfer_->end();
		}		
	}

	Moo::rc().setRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE );
	Moo::rc().setTexture(0, NULL);
	Moo::rc().setTexture(1, NULL);
	Moo::rc().setTexture(2, NULL);
	Moo::rc().setTexture(3, NULL);
	Moo::rc().setPixelShader( NULL );
}


void Bloom::captureBackBuffer()
{
	DX::Viewport vp;

	vp.X = vp.Y = 0;
	vp.MinZ = 0;
	vp.MaxZ = 1;
	vp.Width = bbWidth_;
	vp.Height = bbHeight_;

	if ( wasteOfMemory_->valid() && wasteOfMemory_->push() )
	{
		Moo::rc().device()->SetViewport( &vp );
		colourScaleParameters_.setTexture( "diffuseMap", FullScreenBackBuffer::renderTarget().pTexture() );	

		if (colourScale_->begin())
		{
			for ( uint32 i=0; i<colourScale_->nPasses(); i++ )
			{
				colourScale_->beginPass(i);
				// note bbc_ always applies pixel-texel alignment correction
				Vector2 tl(0,0);
				Vector2 dimensions((float)bbWidth_,(float)bbHeight_);
				bbc_->draw( tl, dimensions, tl, dimensions, true);
				colourScale_->endPass();
			}
			colourScale_->end();
		}
		
			
		wasteOfMemory_->pop();
	}
}


void Bloom::downSample(DX::BaseTexture* pSrc,
					   float colourAtten )
{
	Moo::rc().setFVF( D3DFVF_XYZRHW | D3DFVF_TEX4 );

	struct FILTER_VERTEX { float x, y, z, w; struct uv { float u, v; } tex[4]; };
	//size determines the subset of the dest. render target to draw into,
	//and is basically width/ height in clip coordinates.
	Vector2 size( (float)Moo::rc().screenWidth(),
					(float)(Moo::rc().screenHeight()));

	//fixup is the geometric offset required for exact pixel-texel alignment
	Vector2 fixup( -0.5f, -0.5f );

	FILTER_VERTEX v[4] =
	{ //   X					Y							Z			W
		{fixup.x,			fixup.y,			1.f ,		1.f},
		{size.x + fixup.x,	fixup.y,			1.f ,		1.f },
		{size.x + fixup.x,	size.y + fixup.y,	1.f ,		1.f },
		{fixup.x,			size.y + fixup.y,	1.f ,		1.f }
	};

	// Set uvs + pixel shader constant
	downSampleParameters_.setFloat( "colourAttenuation", colourAtten );	
	downSampleParameters_.setTexture( "diffuseMap", pSrc );

	if (downSample_->begin())
	{
		for (uint p=0; p<downSample_->nPasses(); p++)
		{
			downSample_->beginPass(p);
	
			float xOff[] = 
			{
				-1.f,1.f,1.f,-1.f
			};

			float yOff[] = 
			{	1.f,1.f,-1.f,-1.f
			};

			float srcWidth = (float)srcWidth_;
			float srcHeight = (float)srcHeight_;

			for ( int i=0; i<4; i++ )
			{				
				v[0].tex[i].u = xOff[i];
				v[0].tex[i].v = yOff[i] + srcHeight;
				v[1].tex[i].u = xOff[i] + srcWidth;
				v[1].tex[i].v = yOff[i] + srcHeight;
				v[2].tex[i].u = xOff[i] + srcWidth;
				v[2].tex[i].v = yOff[i];
				v[3].tex[i].u = xOff[i];
				v[3].tex[i].v = yOff[i];

				//convert linear texture coordinates to standard.
				for ( int j=0; j<4; j++ )
				{
					v[j].tex[i].u /= sourceDimensions_.x;
					v[j].tex[i].v /= -sourceDimensions_.y;
					v[j].tex[i].v += 1.f;		
				}
			}

			Moo::rc().drawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, &v, sizeof(FILTER_VERTEX));
			downSample_->endPass();
		}
		downSample_->end();
	}
}



void Bloom::filterCopy(DX::BaseTexture* pSrc,
					   DWORD dwSamples,
					   FilterSample rSample[],
					   float colourAtten,
					   bool filterX )
{
	Moo::rc().setFVF( D3DFVF_XYZRHW | D3DFVF_TEX4 );

	struct FILTER_VERTEX { float x, y, z, w; struct uv { float u, v; } tex[4]; };
	//size determines the subset of the dest. render target to draw into,
	//and is basically width/ height in clip coordinates.
	Vector2 size( (float)(Moo::rc().screenWidth()),
					(float)(Moo::rc().screenHeight()) );

	//fixup is the geometric offset required for exact pixel-texel alignment
	Vector2 fixup( -0.5f, -0.5f );

	FILTER_VERTEX v[4] =
	{ //   X					Y				Z		W
		{fixup.x,			fixup.y,			1.f, 1.f },
		{size.x + fixup.x,	fixup.y,			1.f, 1.f },
		{size.x + fixup.x,	size.y + fixup.y,	1.f, 1.f },
		{fixup.x,			size.y + fixup.y,	1.f, 1.f }
	};


	float srcWidth = (float)srcWidth_;
	float srcHeight = (float)srcHeight_;

	gaussianParameters_.setTexture( "diffuseMap", pSrc );

	if ( gaussianBlur_->begin() )
	{
		for ( uint p=0; p<gaussianBlur_->nPasses(); p++ )
		{
			gaussianBlur_->beginPass(p);			

			for( uint32 i = 0; i < dwSamples; i += 4 )
			{	
				Vector4 vWeights;
				for (uint32 iStage = 0; iStage < 4; iStage++)
				{
					// Set filter coefficients			
					float fcoeff = rSample[i+iStage].fCoefficient * colourAtten;
					vWeights[iStage] = fcoeff;

					if(filterX)
					{
						v[0].tex[iStage].u = rSample[i+iStage].fOffset;
						v[0].tex[iStage].v = 0.f;
						v[1].tex[iStage].u = srcWidth + rSample[i+iStage].fOffset;
						v[1].tex[iStage].v = 0.f;
						v[2].tex[iStage].u = srcWidth + rSample[i+iStage].fOffset;
						v[2].tex[iStage].v = srcHeight;
						v[3].tex[iStage].u = rSample[i+iStage].fOffset;
						v[3].tex[iStage].v = srcHeight;
					}
					else
					{
						v[0].tex[iStage].u = 0.f;
						v[0].tex[iStage].v = rSample[i+iStage].fOffset;
						v[1].tex[iStage].u = srcWidth;
						v[1].tex[iStage].v = rSample[i+iStage].fOffset;
						v[2].tex[iStage].u = srcWidth;
						v[2].tex[iStage].v = srcHeight + rSample[i+iStage].fOffset;
						v[3].tex[iStage].u = 0.f;
						v[3].tex[iStage].v = srcHeight + rSample[i+iStage].fOffset;
					}

					//convert linear texture coordinates to standard.
					for ( int i=0; i<4; i++ )
					{
						v[i].tex[iStage].u /= sourceDimensions_.x;
						v[i].tex[iStage].v /= -sourceDimensions_.y;
						v[i].tex[iStage].v += 1.f;
					}
				}
			
				//  only 1st pass is opaque
				gaussianParameters_.setBool( "AlphaBlendPass", i < 4 ? false : true );
				gaussianParameters_.setVector( "FilterCoefficents", &vWeights );
				gaussianParameters_.commitChanges();

				// Render 1 pass of the filter
				Moo::rc().drawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, &v, sizeof(FILTER_VERTEX));
			}
			gaussianBlur_->endPass();
		}
		gaussianBlur_->end();
	}
}
