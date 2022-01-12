/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef BLOOM_EFFECT_HPP
#define BLOOM_EFFECT_HPP

#include "effect_parameter_cache.hpp"
#include "full_screen_back_buffer.hpp"
#include "moo/graphics_settings.hpp"

class BackBufferCopy;
class TransferMesh;

namespace Moo
{
	class RenderTarget;
	class EffectMaterial;
};

namespace DX
{
	typedef IDirect3DBaseTexture9	BaseTexture;
}

struct FilterSample {
	float fCoefficient;
	float fOffset;
};

enum filterModes
{
	GAUSS_4X4 = 1,
	GAUSS_24X24 = 3
};

/**
 *	This isolates the bright areas of the screen,
 *	and then smudges them back over themselves,
 *	creating a blur that encroaches on neighbouring
 *	pixels.
 */
class Bloom : public Moo::DeviceCallback, public FullScreenBackBuffer::User
{
public:
	Bloom();
	~Bloom();

	///can ever use
	static bool isSupported();
	
#ifdef EDITOR_ENABLED
	void setEditorEnabled( bool state )			{ editorEnabled_ = state; }
#endif

	bool isEnabled();
	void beginScene()	{};
	void endScene()		{};
	bool doTransfer( bool alreadyTransferred )	{ return false; }
	void doPostTransferFilter();

	bool init();
	void finz();

	void justBlur( bool state );
	void applyPreset( bool blurOnly, int filterMode , float colourAtten, int nPasses );
	
	void draw();
	
	void deleteUnmanagedObjects();

private:
	bool safeCreateRenderTarget( Moo::RenderTarget*& rt,
								int width,
								int height,
								bool reuseZ,
								const std::string& name );
	bool safeCreateEffect( Moo::EffectMaterial*& mat,
							const std::string& effectName );

	void	captureBackBuffer();
	void	downSample( DX::BaseTexture* pSrc,
						float colourAtten );
	void	filterCopy( DX::BaseTexture* pSrc,
						DWORD dwSamples,
						FilterSample rSample[],
						float colourAtten,
						bool filterX );
	
	Moo::EffectMaterial* downSample_;
	Moo::EffectMaterial* gaussianBlur_;
	Moo::EffectMaterial* colourScale_;
	Moo::EffectMaterial* transfer_;
	EffectParameterCache downSampleParameters_;
	EffectParameterCache gaussianParameters_;
	EffectParameterCache colourScaleParameters_;
	EffectParameterCache transferParameters_;
	bool inited_;
#ifdef EDITOR_ENABLED
	bool editorEnabled_;
#endif
	bool watcherEnabled_;	
	BackBufferCopy* bbc_;
	TransferMesh* transferMesh_;
	Moo::RenderTarget* rt0_;
	Moo::RenderTarget*	rt1_;
	Moo::RenderTarget*	wasteOfMemory_;
	uint32 renderTargetWidth_;
	uint32 renderTargetHeight_;
	
	Vector2	sourceDimensions_;
	int		filterMode_;
	int		bbWidth_;
	int		bbHeight_;
	int		srcWidth_;
	int		srcHeight_;
	float	colourAtten_;
	bool	bloomBlur_;
	int		nPasses_;

	typedef Moo::GraphicsSetting::GraphicsSettingPtr GraphicsSettingPtr;
	GraphicsSettingPtr bloomSettings_;
	void setBloomOption(int) {}
};

#endif