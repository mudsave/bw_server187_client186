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

#include "cstdmf/debug.hpp"
#include "render_context.hpp"

#include "cube_render_target.hpp"

#ifndef CODE_INLINE
#include "cube_render_target.ipp"
#endif

DECLARE_DEBUG_COMPONENT2( "Moo", 0 )

// Convert a hresult error to a std::string
inline std::string errorAsString( HRESULT hr )
{
//	char s[1024];
//	D3DXGetErrorString( hr, s, 1000 );
//	return std::string( s );
	return std::string();
}

namespace Moo
{

	CubeRenderTarget::CubeRenderTarget( const std::string& identifier ) :
		identifier_( identifier ),
		cubeDimensions_( 0 )
	{
	}

	CubeRenderTarget::~CubeRenderTarget()
	{
	}


	/**
	 *	This method returns a pointer to the d3d texture of this cubemap.
	 */
	DX::BaseTexture* CubeRenderTarget::pTexture( )
	{
		return reinterpret_cast< DX::BaseTexture* >( &*texture_ );
	}

	/**
	 *	This method returns the width of one cubemap surface.
	 */
	uint32 CubeRenderTarget::width( ) const
	{
		return cubeDimensions_;
	}

	/**
	 *	This method returns the height of one cubemap surface
	 */
	uint32 CubeRenderTarget::height( ) const
	{
		return cubeDimensions_;
	}

	/**
	 *	This method creates a cubemap and a z/stencil buffer, of the supplied dimensions.
	 *	@param cubeDimensions the size of each side of the cubemap.
	 *	@return true if the method succeeds.
	 */
	bool CubeRenderTarget::create( uint32 cubeDimensions )
	{
		MF_ASSERT( cubeDimensions > 0 );
		bool ret = false;

		DX::Device* device = rc().device();

		release();

		if (device)
		{
			DX::CubeTexture* pCT = NULL;

			HRESULT hr = device->CreateCubeTexture( cubeDimensions,
													1, D3DUSAGE_RENDERTARGET,
													D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
													&pCT, NULL );

			if (SUCCEEDED( hr ))
			{
				texture_ = pCT;
				pCT->Release();
				cubeDimensions_ = cubeDimensions;
				DX::Surface* pDS = NULL;

				hr = device->CreateDepthStencilSurface(	cubeDimensions, cubeDimensions, 
														rc().getMatchingZBufferFormat( D3DFMT_A8R8G8B8 ),
														D3DMULTISAMPLE_NONE, 0, TRUE,
														&pDS, NULL );
				if (SUCCEEDED( hr ))
				{
					depthStencilSurface_ = pDS;
					pDS->Release();
					ret = true;
				}
				else
				{
					CRITICAL_MSG( "CubeRenderTarget::create: error creating the zbuffer: %s\n",
						errorAsString( hr ).c_str() );
					release();
				}
			}
			else
			{
				CRITICAL_MSG( "CubeRenderTarget::create: error creating the cubetexture: %s\n",
					errorAsString( hr ).c_str() );
			}
		}
		return ret;
	}

	/**
	 *	This method sets a render surface, it does NOT store the previous render target etc,
	 *	so it is important to call RenderContext::pushRendertarget if you want to restore
	 *	the previous render target.
	 *	@param face the identifier of the cubemap face to set as a render target.
	 *	@return true if the operation was successful
	 */
	bool CubeRenderTarget::setRenderSurface( D3DCUBEMAP_FACES face )
	{
		MF_ASSERT( &*texture_ );

		DX::Surface* pCubeSurface = NULL;
		HRESULT hr = texture_->GetCubeMapSurface( face, 0, &pCubeSurface );

		bool ret = false;

		if (SUCCEEDED( hr ))
		{
			DX::Device* device = rc().device();
			hr = device->SetRenderTarget( 0, pCubeSurface );
			if (SUCCEEDED( hr ))
			{
				hr = device->SetDepthStencilSurface( &*depthStencilSurface_ );
				if (SUCCEEDED( hr ))
					ret = true;
			}
			else
			{
				CRITICAL_MSG( "CubeRenderTarget::setRenderSurface: unable to set rendertarget: %s\n",
								errorAsString(hr).c_str() );
			}
			pCubeSurface->Release();
		}
		else
		{
			CRITICAL_MSG( "CubeRenderTarget::setRenderSurface: couldn't get cube map surface: %s\n",
							errorAsString(hr).c_str() );
		}

		return ret;
	}

	/**
	 *	This method sets up the view matrix for rendering to a specified cubemap face.
	 *	@param face the cubemap face we want to render to.
	 *	@param centre the camera position we want to render from.
	 */
	void CubeRenderTarget::setCubeViewMatrix( D3DCUBEMAP_FACES face, const Vector3& centre )
	{
		Matrix m = Matrix::identity;

		switch( face )
		{
		case D3DCUBEMAP_FACE_POSITIVE_X:
			m.setRotateY( DEG_TO_RAD( 90 ) );
			break;
		case D3DCUBEMAP_FACE_NEGATIVE_X:
			m.setRotateY( DEG_TO_RAD( -90 ) );
			break;
		case D3DCUBEMAP_FACE_POSITIVE_Y:
			m.setRotateX( DEG_TO_RAD( -90 ) );
			break;
		case D3DCUBEMAP_FACE_NEGATIVE_Y:
			m.setRotateX( DEG_TO_RAD( 90 ) );
			break;
		case D3DCUBEMAP_FACE_NEGATIVE_Z:
			m.setRotateY( DEG_TO_RAD( 180 ) );
			break;
		}

		m.translation( centre );
		m.invert();
		rc().view( m );
		rc().updateViewTransforms();
	}

	/**
	 * This method releases the cubemap and the zbuffer surface.
	 */
	HRESULT CubeRenderTarget::release()
	{
		cubeDimensions_ = 0;
		texture_ = NULL;
		depthStencilSurface_ = NULL;

		return 0;
	}

	std::ostream& operator<<(std::ostream& o, const CubeRenderTarget& t)
	{
		o << "CubeRenderTarget\n";
		return o;
	}
}

/*cube_render_target.cpp*/
