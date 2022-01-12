/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef RENDER_CONTEXT_HPP
#define RENDER_CONTEXT_HPP

#include "moo_math.hpp"
#include "moo_dx.hpp"

#include "camera.hpp"
#include "com_object_wrap.hpp"
#include "light_container.hpp"
#include "cstdmf/vectornodest.hpp"
#include "cstdmf/aligned.hpp"
#include "cstdmf/concurrency.hpp"
#include "math/matrix.hpp"
#include <stack>

#include <iostream>

#ifndef D3DRS_MAX
// Must be larger than the maximum valid value of the enum D3DRENDERSTATETYPE
#define D3DRS_MAX       210
#endif D3DRS_MAX

#ifndef D3DTSS_MAX
// Must be larger than the maximum valid value of the enum D3DTEXTURESTAGESTATETYPE
#define D3DTSS_MAX      33
#endif D3DTSS_MAX

#ifndef D3DSAMP_MAX
// Must be larger than the maximum valid value of the enum D3DSAMPLERSTATETYPE
#define D3DSAMP_MAX		14
#endif D3DSAMP_MAX

namespace Moo
{
	//forward decl.
	class Material;
	class OcclusionQuery;

	// flags for various codepaths for different video cards
	enum CompatibilityFlag
	{
		COMPATIBILITYFLAG_NOOVERWRITE = 1 << 0
	};

	struct DeviceInfo
	{
		D3DADAPTER_IDENTIFIER9			identifier_;
		D3DCAPS9						caps_;
		uint32							adapterID_;
		bool							windowed_;
		D3DDISPLAYMODE					windowedDisplayMode_;
		std::vector< D3DDISPLAYMODE >	displayModes_;
		uint32							compatibilityFlags_;
	};

extern THREADLOCAL( bool ) g_renderThread;

	struct RSCacheEntry
	{
		D3DRENDERSTATETYPE				state;
		uint32							currentValue;
	};

	struct TSSCacheEntry
	{
		uint16							stage;
		uint16							type;
		uint32							currentValue;
	};
	struct SampCacheEntry
	{
		uint16							stage;
		uint16							type;
		uint32							currentValue;
	};

	struct TextureCacheEntry
	{
		uint16							stage;
		DX::BaseTexture*				pCurrentTexture;
	};

	/**
	 *	This class creates and manages the d3d device, backbuffer etc, it's
	 *	the current state of rendering in that it contains the current lights,
	 *	matrices, lod state, zoom factor and so forth. Anything that is of global
	 *	importance to rendering should be contained in the render context.
	 */
	class RenderContext : public Aligned
	{
	public:
		~RenderContext();

		static RenderContext&		instance();

		uint32						nDevices( ) const;
		const DeviceInfo&			deviceInfo( uint32 i ) const;

		bool						checkDevice(bool *reset = NULL);
		bool						checkDeviceYield(bool *reset = NULL);

		bool						createDevice( HWND hWnd,
													uint32 deviceIndex = 0,
													uint32 modeIndex = 0,
													bool windowed = true,
													bool wantStencil = false );
		void						releaseDevice( void );
		void						resetDevice( const D3DPRESENT_PARAMETERS &d3dpp );
		bool						changeMode( 
										uint32 modeIndex, 
										bool   windowed, 
										bool   testCooperative = false,
										uint32 backBufferWidthOverride = 0 );

		DX::Device*					device() const;

		void						restoreCursor( bool state );

		bool						windowed( ) const;
		uint32						deviceIndex( ) const;
		uint32						modeIndex( ) const;
		bool						stencilAvailable( ) const;
		DWORD						maxVertexIndex( ) const;

		uint						getAvailableTextureMem( ) const;

		bool						mixedVertexProcessing() const { return mixedVertexProcessing_; }

		D3DFORMAT					getMatchingZBufferFormat( D3DFORMAT colourFormat );

		bool						pushRenderTarget();
		bool						popRenderTarget();

		const Camera&				camera( ) const;
		Camera&						camera( );
		void						camera( const Camera& cam );

		const Matrix&				projection( ) const;
		Matrix&						projection( );
		void						projection( Matrix& m );
		void						scaleOffsetProjection( const float scale, const float x, const float y );

		const Matrix&				view( ) const;
		Matrix&						view( );
		void						view( const Matrix& m );

		const Matrix&				world() const;
		Matrix&						world();
		void						world( const Matrix& m );

		const Matrix&				viewProjection( ) const;
		const Matrix&				invView( ) const;

		void						preMultiply( const Matrix& m );
		void						postMultiply( const Matrix& m );

		void						push( );
		void						pop( );
		void						reset( );
		void						reset( const Matrix& m );

		void						updateProjectionMatrix( );
		void						updateViewTransforms( );

		LightContainerPtr			lightContainer( ) const;
		void						lightContainer( LightContainerPtr pLC );

		LightContainerPtr			specularLightContainer( ) const;
		void						specularLightContainer( LightContainerPtr pLC );

		const D3DSURFACE_DESC&		backBufferDesc( ) const;
		const D3DPRESENT_PARAMETERS& presentParameters( ) const;

		void						addToPrimitiveCount( uint32 n );
		uint32						primitiveCount( ) const;
		uint32						resetPrimitiveCount( );

		void						addToPrimitiveGroupCount( uint32 n = 1 );
		uint32						primitiveGroupCount( ) const;
		uint32						resetPrimitiveGroupCount( );

		uint32						frameTimestamp( ) const;
		bool						frameDrawn( uint32& frame ) const;
		void						nextFrame( );

		void						fogNear( float fogNear );
		float						fogNear( ) const;
		void						fogFar( float fogFar );
		float						fogFar( ) const;

		float						screenWidth() const;
		float						screenHeight() const;
		float						halfScreenWidth() const;
		float						halfScreenHeight() const;
		void						screenWidth( int width );
		void						screenHeight( int height );
		void						backBufferWidthOverride( uint32 backBufferWidthOverride );
		uint32						backBufferWidthOverride() const;

		void						fullScreenAspectRatio( float ratio );
		float						fullScreenAspectRatio() const;

		void						objectAlphaOverride( float alpha );
		uint32						objectAlphaOverride( void ) const;

		void						screenShot( const std::string& fileName = std::string() );
		DX::Surface*				getScreenCopy();

		float						lodValue() const;
		void						lodValue( float value );

		float						lodFar() const;
		void						lodFar( float value );

		float						lodPower() const;
		void						lodPower( float value );

		float						zoomFactor() const;

		float						gammaCorrection() const;
		void						gammaCorrection( float gammaCorrection );

		bool						waitForVBL() const;
		void						waitForVBL( bool wait );
		bool						tripleBuffering() const { return tripleBuffering_; }
		void						tripleBuffering( bool enabled ) { tripleBuffering_ = enabled; }

		bool						supportsTextureFormat( D3DFORMAT fmt ) const;

		uint16						psVersion() const;
		uint16						vsVersion() const;
		uint16						maxSimTextures() const;

		/*
		 *	pause/resume methods to free/realloc resources. Useful in windowed
		 *	apps that need to be friendly with other DX apps.
		 */
		void						pause();
		void						resume();
		bool						paused() const;

		/*
		 * State caching
		 */

		uint32						setRenderState(D3DRENDERSTATETYPE state, uint32 value);
		uint32						getRenderState(D3DRENDERSTATETYPE state );
		uint32						setTextureStageState( uint32 stage, D3DTEXTURESTAGESTATETYPE type, uint32 value );
		uint32						setSamplerState( uint32 stage, D3DSAMPLERSTATETYPE type, uint32 value);
		uint32						setTexture(DWORD stage, DX::BaseTexture* pTexture);

		uint32						setVertexShader( DX::VertexShader* pVS );
		uint32						setPixelShader( DX::PixelShader* pPS );
		HRESULT						setIndices( ComObjectWrap<DX::IndexBuffer> pIB );
		ComObjectWrap<DX::IndexBuffer>	getIndices() const;
		uint32						setVertexDeclaration( DX::VertexDeclaration* pVD );
		uint32						setFVF( uint32 fvf );

		uint32						drawIndexedPrimitiveUP( D3DPRIMITIVETYPE PrimitiveType,
															UINT minIndex,
															UINT numVertices,
															UINT primitiveCount,
															CONST void* pIndexData,
															D3DFORMAT indexDataFormat,
															CONST void* pVertexStreamZeroData,
															UINT vertexStreamZeroStride );
		uint32						drawIndexedPrimitive( D3DPRIMITIVETYPE type,
														  INT baseVertexIndex,
														  UINT minIndex,
														  UINT numVertices,
														  UINT startIndex,
														  UINT primitiveCount );
		uint32						drawPrimitive( D3DPRIMITIVETYPE primitiveType,
												   UINT startVertex,
												   UINT primitiveCount );
		uint32						drawPrimitiveUP( D3DPRIMITIVETYPE primitiveType,
													 UINT primitiveCount,
													 CONST void* pVertexStreamZeroData,
													 UINT vertexStreamZeroStride );

		void						getNearPlaneRect( Vector3 & corner,
										Vector3 & xAxis, Vector3 & yAxis ) const;

		HRESULT						beginScene();
		HRESULT						endScene();

		bool						mirroredTransform() const { return mirroredTransform_; }
		void						mirroredTransform(bool mirrored ) { mirroredTransform_ = mirrored; }

		bool						reflectionScene() const { return reflectionScene_; }
		void						reflectionScene(bool reflections ) { reflectionScene_ = reflections; }

		OcclusionQuery*				createOcclusionQuery();
		void						destroyOcclusionQuery(OcclusionQuery*);

		void						beginQuery(OcclusionQuery*);
		void						endQuery(OcclusionQuery*);
		bool						getQueryResult(int& visiblePixels, OcclusionQuery*, bool wait);

	private:
		/**
		 *	This class is a helper class for the RenderContext to push and pop the
		 *	current rendertarget.
		 */
		struct RenderTargetStack
		{
		public:
			bool push( class RenderContext* rc );
			bool pop( class RenderContext* rc );
			int nStackItems();
		private:
			class StackItem : public Aligned
			{
			public:
				StackItem() :
					cam_( 0.5f, 200.f, MATH_PI * 0.5f, 1.f )
				{
				}
				~StackItem()
				{
				}
				ComObjectWrap< DX::Surface >	renderSurface_;
				ComObjectWrap< DX::Surface >	zbufferSurface_;
				DX::Viewport					viewport_;
				Matrix							view_;
				Matrix							projection_;
				Camera							cam_;
			};

			typedef std::avector< StackItem > StackItems;
			StackItems stackItems_;
		};

		D3DSURFACE_DESC				backBufferDesc_;

		D3DPRESENT_PARAMETERS		presentParameters_;
		RECT						windowedRect_;
		HWND						windowHandle_;
		bool						windowed_;
		LONG						windowedStyle_;
		bool						stencilWanted_;
		bool						stencilAvailable_;
		uint32						deviceIndex_;
		uint32						modeIndex_;
		uint32						backBufferWidthOverride_;
		float						fullScreenAspectRatio_;
		std::vector< DeviceInfo >	devices_;

		bool						mixedVertexProcessing_;

		ComObjectWrap< DX::Interface >	d3d_;
		ComObjectWrap< DX::Device >		device_;

		ComObjectWrap< DX::Surface >	screenCopySurface_;

		RenderTargetStack			renderTargetStack_;

		Camera						camera_;
		Matrix						projection_;
		Matrix						view_;
		Matrix						viewProjection_;
		Matrix						invView_;

		float						projScale_;
		float						projXOffset_;
		float						projYOffset_;

		AVectorNoDestructor< Matrix > world_;

		float						fogNear_;
		float						fogFar_;

		LightContainerPtr			lightContainer_;
		LightContainerPtr			specularLightContainer_;

		uint32						primitiveGroupCount_;
		uint32						primitiveCount_;
		uint32						currentFrame_;

/*	    void						addPrimCount( uint32 primCount )
		{
			primCount = primCount >> primitiveHistogramResolution_;
			primCount = min( primCount, uint32(255) );
			primitiveHistogram_[primCount]++;
		}*/

		float						halfScreenWidth_;
		float						halfScreenHeight_;

		void						fillPresentationParameters( );
		void						updateDeviceInfo( );

		void						initRenderStates();

		uint32						alphaOverride_;
		float						lodValue_;
		float						lodPower_;
		float						lodFar_;
		float						zoomFactor_;

		bool						waitForVBL_;
		bool						tripleBuffering_;

		uint16						psVersion_;
		uint16						vsVersion_;
		uint16                      maxSimTextures_;
		float						gammaCorrection_;
		void						setGammaCorrection();

		static RSCacheEntry			rsCache_[D3DRS_MAX];
		static TSSCacheEntry		tssCache_[64][D3DTSS_MAX];
		static SampCacheEntry		sampCache_[64][D3DSAMP_MAX];
		static TextureCacheEntry	textureCache_[64];

		static DX::VertexDeclaration* vertexDeclaration_;
		static uint32				fvf_;

		int							beginSceneCount_;

		bool mirroredTransform_, reflectionScene_;

		bool						paused_;

		std::vector<OcclusionQuery*> queryList_;

		RenderContext();
		RenderContext(const RenderContext&);
		RenderContext& operator=(const RenderContext&);

		void releaseQueryObjects(void);
		void createQueryObjects(void);

		friend std::ostream& operator<<(std::ostream&, const RenderContext&);
	};

	inline RenderContext& rc( )
	{
		return RenderContext::instance();
	}
};

#ifdef CODE_INLINE
#include "render_context.ipp"
#endif




#endif // RENDER_CONTEXT_HPP
