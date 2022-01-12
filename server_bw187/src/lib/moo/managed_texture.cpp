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

#include "managed_texture.hpp"
#include "resmgr/datasection.hpp"
#include "resmgr/bwresource.hpp"
#include "cstdmf/timestamp.hpp"
#include "cstdmf/debug.hpp"
#include "resmgr/auto_config.hpp"

#include "render_context.hpp"

#ifndef CODE_INLINE
#include "managed_texture.ipp"
#endif

#include "cstdmf/memory_counter.hpp"

#include "dds.h"

DECLARE_DEBUG_COMPONENT2( "Moo", 0 )

extern SIZE_T textureMemSum;
memoryCounterDeclare( texture );

static AutoConfigString s_notFoundBmp( "system/notFoundBmp" );

namespace Moo
{

uint32 ManagedTexture::frameTimestamp_ = -1;
uint32 ManagedTexture::totalFrameTexmem_ = 0;

/*static*/ bool					ManagedTexture::s_accErrs = false;
/*static*/ std::string			ManagedTexture::s_accErrStr = "";
/*static*/ void					ManagedTexture::accErrs( bool acc ) { s_accErrs = acc; s_accErrStr = ""; }
/*static*/ const std::string&	ManagedTexture::accErrStr() { return s_accErrStr; } 

ManagedTexture::ManagedTexture()
: valid_( false ),
  textureMemoryUsed_( 0 )
{
}

ManagedTexture::ManagedTexture( const std::string& resourceID, bool mustExist, int mipSkip )
: resourceID_( resourceID ),
  valid_( false ),
  width_( 0 ),
  height_( 0 ),
  format_( D3DFMT_UNKNOWN ),
  mipSkip_( mipSkip ),
  textureMemoryUsed_( 0 ),
  localFrameTimestamp_( 0 ),
  failedToLoad_( false )
#if MANAGED_CUBEMAPS
  ,cubemap_( false )
#endif
{
	load( mustExist );

	memoryCounterAdd( texture );
	memoryClaim( resourceID_ );
}

ManagedTexture::~ManagedTexture()
{

	memoryCounterSub( texture );
	memoryClaim( resourceID_ );

	// let the texture manager know we're gone
	this->delFromManager();
}

HRESULT ManagedTexture::reload( )
{
	release();
	failedToLoad_ = false;
	return load( );
}

HRESULT ManagedTexture::reload(const std::string & resourceID)
{
	release();
	failedToLoad_ = false;
	resourceID_   = resourceID;
	return load();
}

HRESULT ManagedTexture::load( bool mustExist )
{
	if (failedToLoad_)
		return E_FAIL;

	// Has the texture already been initialised?
	if( valid_ )
		return S_OK;
	
	// Have we created the device to load the texture into?
	if( Moo::rc().device() == (void*)NULL )
		return E_FAIL;

	// Debug: time the texture load.
	uint64 totalTime = timestamp();

	HRESULT hr = E_FAIL;

	// Open the texture file resource.
	BinaryPtr texture;
	if (resourceID_.substr( 0, 4 ) != "////")
		texture = BWResource::instance().rootSection()->readBinary( resourceID_ );
	else
		texture = new BinaryBlock( resourceID_.data()+4, resourceID_.length()-4 );
	if( !texture.hasObject() )
	{
		if (mustExist)
		{
			if (!ManagedTexture::s_accErrs)
			{
				WARNING_MSG( "Moo::ManagedTexture::load: "
					"Failed to load texture resource: %s - Invalid format?\n", resourceID_.c_str() );
			}
			else
			{
				if (s_accErrStr.find( resourceID_ ) == std::string::npos)
				{
					s_accErrStr = s_accErrStr + "\n * " + resourceID_ + " (invalid format?)";
				}
			}
		}
		resourceID_ = s_notFoundBmp;
		if (resourceID_.substr( 0, 4 ) != "////")
			texture = BWResource::instance().rootSection()->readBinary( resourceID_ );
		else
			texture = new BinaryBlock( resourceID_.data()+4, resourceID_.length()-4 );
	}
	if( texture.hasObject() )
	{
		//DX::Texture* tex = NULL;
		
/*		MEMORYSTATUS preStat;
		GlobalMemoryStatus( &preStat );*/

		static PALETTEENTRY palette[256];

#if MANAGED_CUBEMAPS
		DWORD* head = (DWORD*)texture->data();		
		if ( head[0] == DDS_MAGIC_VALUE && head[1] == 124 )
		{			
			// we have a DDS file....
			if ( ((DDS_HEADER*)&head[1])->dwCubemapFlags & DDS_CUBEMAP )
			{
				// and we have a cube map...
				cubemap_ = true;
			}
		}
#endif // MANAGED_CUBEMAPS

		DWORD mipFilter = BWResource::getExtension(resourceID_) == "dds"
			? D3DX_SKIP_DDS_MIP_LEVELS(this->mipSkip(), D3DX_FILTER_BOX|D3DX_FILTER_MIRROR)
			: D3DX_FILTER_BOX|D3DX_FILTER_MIRROR;

#if MANAGED_CUBEMAPS
		//LPDIRECT3DCUBETEXTURE9
		if ( cubemap_ )
		{
			DX::CubeTexture* tex = NULL;

			hr = D3DXCreateCubeTextureFromFileInMemoryEx( 
				Moo::rc().device(), texture->data(), texture->len(),
				D3DX_DEFAULT, D3DX_DEFAULT, 0, D3DFMT_UNKNOWN, 
				D3DPOOL_MANAGED, D3DX_FILTER_BOX|D3DX_FILTER_MIRROR, 
				mipFilter, 0, NULL, palette, &tex );

			if( SUCCEEDED( hr ) )
			{
				DX::Surface* surface;
				if( SUCCEEDED( tex->GetCubeMapSurface(D3DCUBEMAP_FACE_POSITIVE_X,  0, &surface ) ) )
				{
					D3DSURFACE_DESC desc;
					surface->GetDesc( &desc );

					width_ = desc.Width;
					height_ = desc.Height;

					surface->Release();
					surface = NULL;
				}

				// Assign the texture to the smartpointer.
				texture_.pComObject( static_cast< DX::BaseTexture* >( tex ) );
				// Decrement the referencecount of the texture. 
				// ( as we want the smartpointer to have the only reference to the texture )
				tex->Release( );

				valid_ = true;

				// Note : this must be called after the pTexture() will return
				// a pointer ( valid_ and texture_ must be set )
				//textureMemoryUsed_ = BaseTexture::textureMemoryUsed();
				textureMemoryUsed_ = texture->len() >> (this->mipSkip() * 3);
				originalTextureMemoryUsed_ = textureMemoryUsed_;

				// Debug: output texture load time.
				totalTime = timestamp() - totalTime;
				totalTime = (totalTime * 1000) / stampsPerSecond();
	/*			DEBUG_MSG( "Moo : ManagedTexture : Loaded texture %s, Width: %d, Height: %d, Mem: %d, it took %d milliseconds\n", 
					resourceID_.c_str(), width_, height_, textureMemoryUsed_, (uint32)totalTime );*/		
			}
			else
			{
				failedToLoad_ = true;
				if (mustExist)
				{
					if (!ManagedTexture::s_accErrs)
					{
						WARNING_MSG( "Moo::ManagedTexture::load: "
							"Failed to load texture resource: %s - Invalid format?\n", resourceID_.c_str() );
					}
					else
					{
						if (s_accErrStr.find( resourceID_ ) == std::string::npos)
						{
							s_accErrStr = s_accErrStr + "\n * " + resourceID_ + " (invalid format?)";
						}
					}
				}
			}
		}
		else
#endif //MANAGED_CUBEMAPS
		{
		DX::Texture* tex = NULL;
		// Create the texture and the mipmaps.
		hr = D3DXCreateTextureFromFileInMemoryEx( 
			Moo::rc().device(), texture->data(), texture->len(),
			D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0, D3DFMT_UNKNOWN, 
			D3DPOOL_MANAGED, D3DX_FILTER_BOX|D3DX_FILTER_MIRROR, 
			mipFilter, 0, NULL, palette, &tex );

/*		MEMORYSTATUS postStat;
		GlobalMemoryStatus( &postStat );

		textureMemSum += preStat.dwAvailPhys - postStat.dwAvailPhys;

		TRACE_MSG( "ManagedTexture: Loaded texture '%s'. textures used memory: %d\n",
			resourceID_.c_str(), textureMemSum / 1024 );*/

		if( SUCCEEDED( hr ) )
		{
			DX::Surface* surface;
			if( SUCCEEDED( tex->GetSurfaceLevel( 0, &surface ) ) )
			{
				D3DSURFACE_DESC desc;
				surface->GetDesc( &desc );

				width_ = desc.Width;
				height_ = desc.Height;
				format_ = desc.Format;

				surface->Release();
				surface = NULL;
			}

			// Assign the texture to the smartpointer.
			texture_.pComObject( static_cast< DX::BaseTexture* >( tex ) );
			// Decrement the referencecount of the texture. 
			// ( as we want the smartpointer to have the only reference to the texture )
			tex->Release( );

			valid_ = true;

			// Note : this must be called after the pTexture() will return
			// a pointer ( valid_ and texture_ must be set )
			//textureMemoryUsed_ = BaseTexture::textureMemoryUsed();
			textureMemoryUsed_ = texture->len() >> (this->mipSkip() * 2);
			originalTextureMemoryUsed_ = textureMemoryUsed_;

			// Debug: output texture load time.
			totalTime = timestamp() - totalTime;
			totalTime = (totalTime * 1000) / stampsPerSecond();
/*			DEBUG_MSG( "Moo : ManagedTexture : Loaded texture %s, Width: %d, Height: %d, Mem: %d, it took %d milliseconds\n", 
				resourceID_.c_str(), width_, height_, textureMemoryUsed_, (uint32)totalTime );*/		
		}
		else
		{
			failedToLoad_ = true;
			if (mustExist)
			{
				if (!ManagedTexture::s_accErrs)
				{
					WARNING_MSG( "Moo::ManagedTexture::load: "
						"Failed to load texture resource: %s - Invalid format?\n", resourceID_.c_str() );
				}
				else
				{
					if (s_accErrStr.find( resourceID_ ) == std::string::npos)
					{
						s_accErrStr = s_accErrStr + "\n * " + resourceID_ + " (invalid format?)";
					}
				}
			}
		}
		}

	}
	else
	{
		failedToLoad_ = true;
		if (mustExist)
		{
			if (!ManagedTexture::s_accErrs)
			{
				WARNING_MSG( "Moo::ManagedTexture::load: "
					"Failed to read binary resource: %s\n", resourceID_.c_str() );
			}
			else
			{
				if (s_accErrStr.find( resourceID_ ) == std::string::npos)
				{
					s_accErrStr = s_accErrStr + "\n * " + resourceID_ + " (failed to read binary resource)";
				}
			}
		}
	}
	return hr;
}

HRESULT ManagedTexture::release( )
{
	// release our reference to the texture
	texture_.pComObject( NULL );
	valid_ = false;
	failedToLoad_ = false;

	textureMemoryUsed_ = 0;
	width_ = 0;
	height_ = 0;

	return S_OK;
}

DX::BaseTexture* ManagedTexture::pTexture()
{
	if( !valid_ )
		load( true );

	if (!rc().frameDrawn(frameTimestamp_))
	{
		totalFrameTexmem_ = 0;
	}

	if (frameTimestamp_ != localFrameTimestamp_)
	{
		localFrameTimestamp_ = frameTimestamp_;
		totalFrameTexmem_ += textureMemoryUsed_;
	}
	return texture_.pComObject();
}

uint32 ManagedTexture::width( ) const
{
	return width_;
}

uint32 ManagedTexture::height( ) const
{
	return height_;
}

D3DFORMAT ManagedTexture::format( ) const
{
	return format_;
}

uint32 BaseTexture::textureMemoryUsed( )
{
	return BaseTexture::textureMemoryUsed(
			static_cast<DX::Texture*>(pTexture()) );
}

const std::string& ManagedTexture::resourceID( ) const
{
	return resourceID_;
}

std::ostream& operator<<(std::ostream& o, const ManagedTexture& t)
{
	o << "ManagedTexture\n";
	return o;
}

}
