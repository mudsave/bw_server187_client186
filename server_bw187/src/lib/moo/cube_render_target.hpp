/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CUBE_RENDER_TARGET_HPP
#define CUBE_RENDER_TARGET_HPP

#include <iostream>
#include "base_texture.hpp"

namespace Moo
{
	/**
	 *	This class implements functionality to handle a cubic 
	 *	environment render target.
	 */
	class CubeRenderTarget : public BaseTexture
	{
	public:
		CubeRenderTarget( const std::string& identifier );
		~CubeRenderTarget();

		bool					create( uint32 cubeDimensions );
		HRESULT					release();

		bool					setRenderSurface( D3DCUBEMAP_FACES face );
		void					setCubeViewMatrix( D3DCUBEMAP_FACES face, 
													const Vector3& centre );

		// Virtual methods from BaseTexture
		virtual DX::BaseTexture*	pTexture( );
		virtual uint32			width( ) const;
		virtual uint32			height( ) const;

	private:
		std::string				identifier_;
		uint32					cubeDimensions_;

		ComObjectWrap< DX::CubeTexture > texture_;
		ComObjectWrap< DX::Surface > depthStencilSurface_;

		CubeRenderTarget(const CubeRenderTarget&);
		CubeRenderTarget& operator=(const CubeRenderTarget&);

		friend std::ostream& operator<<(std::ostream&, const CubeRenderTarget&);
	};
}

#ifdef CODE_INLINE
#include "cube_render_target.ipp"
#endif




#endif
/*cube_render_target.hpp*/
