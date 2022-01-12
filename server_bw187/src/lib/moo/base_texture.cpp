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

#include "cstdmf/memory_counter.hpp"

#include "base_texture.hpp"
#include "texture_manager.hpp"

#ifndef CODE_INLINE
#include "base_texture.ipp"
#endif

memoryCounterDefine( texture, Texture );

namespace Moo
{

BaseTexture::BaseTexture()
{
	memoryCounterAdd( texture );
	memoryClaim( this );
}

BaseTexture::~BaseTexture()
{
	memoryCounterSub( texture );
	memoryClaim( this );
}

const std::string& BaseTexture::resourceID( ) const
{
	static std::string s;
	return s;
}


uint32 BaseTexture::textureMemoryUsed( DX::Texture* pTex )
{
	uint32 textureMemoryUsed = sizeof(DX::Texture);

	DX::Surface* surface;
	if( pTex && SUCCEEDED( pTex->GetSurfaceLevel( 0, &surface ) ) )
	{
		D3DSURFACE_DESC desc;
		surface->GetDesc( &desc );
		textureMemoryUsed = sizeof(DX::Surface) + DX::surfaceSize( desc );
		surface->Release();
		surface = NULL;

		for( uint32 i = 1; i < pTex->GetLevelCount(); i++ )
		{
			if( SUCCEEDED( pTex->GetSurfaceLevel( i, &surface ) ) )
			{
				surface->GetDesc( &desc );
				textureMemoryUsed += sizeof(DX::Surface) + DX::surfaceSize( desc );
				surface->Release();
			}
		}
	}

	return textureMemoryUsed;
}


/**
 *	This method deletes this texture from the texture manager
 */
void BaseTexture::delFromManager()
{
	TextureManager::del( this );
	// could put this in BaseTexture's destructor but then the
	// virtual resourceID fn wouldn't work for the debug message :)
}



}

// base_texture.cpp
