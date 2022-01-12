/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

/**
 *	ThumbnailManager: Thumbnail generator class
 */



#include "pch.hpp"
#include <string>
#include <vector>
#include "string_utils.hpp"
#include "thumbnail_utils.hpp"
#include <moo/render_context.hpp>


// Default providers tokens to ensure that they get compiled
extern int ImageThumbProv_token;
extern int ModelThumbProv_token;
static int s_chunkTokenSet = 0
	| ImageThumbProv_token
	| ModelThumbProv_token
	;


// ThumbnailManager

ThumbnailManager* ThumbnailManager::instance_ = 0;

ThumbnailManager* ThumbnailManager::instance()
{
	if ( !instance_ )
		instance_ = new ThumbnailManager();
	return instance_;
}

ThumbnailManager::ThumbnailManager() :
	postfix_( ".thumbnail.jpg" ),
	folder_( ".bwthumbs" ),
	size_( 64 ),
	backColour_( RGB( 255, 255, 255 ) ),
	rt_( "ThumbnailManager" ),
	rtOk_( false )
{
}

void ThumbnailManager::registerProvider( ThumbnailProviderPtr provider )
{
	if ( !provider )
		return;
	providers_.push_back( provider );
}

void ThumbnailManager::recalcSizeKeepAspect( int w, int h, int& origW, int& origH )
{
	float k = 1;
	if ( origW > origH && origW > 0 )
		k = (float)w / origW;
	else if ( origH > 0 )
		k = (float)h / origH;
	origW = (int)( origW * k );
	origH = (int)( origH * k );
}

bool ThumbnailManager::renderImage( const std::string& file, Moo::RenderTarget* rt )
{
	if ( file.empty() || !rt || !PathFileExists( file.c_str() ) )
		return false;

	DX::Texture* pTexture;

	//load an image into a texture
	D3DXIMAGE_INFO imgInfo;
	if ( !SUCCEEDED( D3DXGetImageInfoFromFile( file.c_str(), &imgInfo ) ) )
		return false;

	int w = rt->width();
	int h = rt->height();

	int origW = imgInfo.Width;
	int origH = imgInfo.Height;
	recalcSizeKeepAspect( w, h, origW, origH );
	origW = ( origW >> 2 ) << 2; // align to 4-pixel boundary
	origH = ( origH >> 2 ) << 2; // align to 4-pixel boundary

	DX::BaseTexture* pBaseTex = rt->pTexture();
	DX::Texture* pDstTexture;
	if( pBaseTex && pBaseTex->GetType() == D3DRTYPE_TEXTURE )
	{
		pDstTexture = (DX::Texture*)pBaseTex;
		pDstTexture->AddRef();
	}
	else
		return false;
	
	if ( !SUCCEEDED( D3DXCreateTextureFromFileEx(
    		Moo::rc().device(),
			file.c_str(),
			origW, origH,
			1, 0, D3DFMT_A8R8G8B8,
			D3DPOOL_SYSTEMMEM,
			D3DX_FILTER_TRIANGLE,
			D3DX_DEFAULT,
			0, NULL, NULL,
			&pTexture ) ) )
	{
		pDstTexture->Release();
		return false;
	}

	DX::Surface* pSrcSurface;
	if ( !SUCCEEDED( pTexture->GetSurfaceLevel( 0, &pSrcSurface ) ) )
	{
		pDstTexture->Release();
		return false;
	}

	DX::Surface* pDstSurface;
	if ( !SUCCEEDED( pDstTexture->GetSurfaceLevel( 0, &pDstSurface ) ) )
	{
		pSrcSurface->Release();
		pDstTexture->Release();
		return false;
	}

	// divide dest point by two and align to 4-pixel boundary
	POINT pt = { ( ( w - origW ) >> 3 ) << 2, ( ( h - origH ) >> 3 ) << 2 };

	Moo::rc().device()->UpdateSurface(
		pSrcSurface, NULL,
		pDstSurface, &pt );

	pDstSurface->Release();
	pSrcSurface->Release();

	pDstTexture->Release();
	return true;
}

void ThumbnailManager::stretchImage( CImage& img, int w, int h, bool highQuality )
{
	if ( img.IsNull() )
		return;
	int origW = img.GetWidth();
	int origH = img.GetHeight();
	
	recalcSizeKeepAspect( w, h, origW, origH );

	CImage image;
	image.Create( w, h, 32 );

	CDC* pDC = CDC::FromHandle( image.GetDC() );
	pDC->FillSolidRect( 0, 0, w, h, backColour_ );
	if ( highQuality )
		pDC->SetStretchBltMode( HALFTONE );
	img.StretchBlt( pDC->m_hDC, ( w - origW ) / 2, ( h - origH ) / 2, origW, origH );
	image.ReleaseDC();

	img.Destroy();
	img.Create( w, h, 32 );

	pDC = CDC::FromHandle( img.GetDC() );
	image.BitBlt( pDC->m_hDC, 0, 0 );
	img.ReleaseDC();
}

void ThumbnailManager::create( const std::string& file, const std::string& postfix, CImage& img, int w, int h )
{
	std::string fname = file;
	std::replace( fname.begin(), fname.end(), '/', '\\' );
	std::string path;
	int slash = (int)fname.find_last_of( '\\' );
	std::string hiddenThumb;
	if ( slash > 0 )
	{
		path = fname.substr( 0, slash ) + "\\" + folder_;
		hiddenThumb = path + fname.substr( slash );
	}
	else
	{
		path = folder_;
		hiddenThumb = path + '\\' + fname;
	}
	hiddenThumb += postfix;

	bool canLoad = true;
	for( std::vector<ThumbnailProviderPtr>::iterator i = providers_.begin();
		i != providers_.end(); ++i )
	{
		if ( !(*i)->isValid( file ) )
			continue;

		if ( (*i)->needsCreate( file, hiddenThumb ) )
		{
			CWaitCursor wait;

			if ( !rtOk_ )
				rtOk_ = rt_.create( size_, size_ );

			if ( rtOk_ && rt_.push() )
			{
				Moo::rc().device()->BeginScene();
				Moo::rc().device()->Clear( 0, NULL,
					D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, backColour_, 1, 0 );
				Moo::rc().device()->SetVertexShader( NULL );
				Moo::rc().device()->SetPixelShader( NULL );
				Moo::rc().setRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALPHA
					| D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE );

				canLoad = (*i)->create( file, &rt_ );
				Moo::rc().device()->EndScene();

				if ( canLoad )
				{
					if ( !PathIsDirectory( path.c_str() ) )
					{
						// create folder if it doesn't exist
						CreateDirectory( path.c_str(), 0 );
						DWORD att = GetFileAttributes( path.c_str() );
						SetFileAttributes( path.c_str(), att | FILE_ATTRIBUTE_HIDDEN );
					}
					D3DXSaveTextureToFile(
						hiddenThumb.c_str(), D3DXIFF_JPG, rt_.pTexture(), NULL );
				}

				rt_.pop();
			}
		}
		break;
	}
	if ( canLoad )
		img.Load( hiddenThumb.c_str() );

	if ( !img.IsNull() )
	{
		// Is rescale needed? this one should only get called if the caller
		// requests a size different than "size_"
		if ( img.GetWidth() != w || img.GetHeight() != h )
			stretchImage( img, w, h, true );
	}
}