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

#include "render_target.hpp"
#include "render_context.hpp"

#include "cstdmf/memory_counter.hpp"

#ifndef CODE_INLINE
#include "render_target.ipp"
#endif

DECLARE_DEBUG_COMPONENT2( "Moo", 0 )

memoryCounterDefineWithAlias( renderTarget, Video, "RenderTgtSize" );

namespace Moo
{

///Constructor
RenderTarget::RenderTarget( const std::string & identitifer ) :
	resourceID_( identitifer ),
	reuseZ_( false ),
	width_( 0 ),
	height_( 0 ),
	pixelFormat_( D3DFMT_A8R8G8B8 ),
	depthFormat_( D3DFMT_UNKNOWN ),
	autoClear_( false ),
	clearColour_( (DWORD)0x00000000 )
{
}


///Destructor
RenderTarget::~RenderTarget()
{
	this->release();
}

HRESULT RenderTarget::release()
{
	this->deleteUnmanagedObjects();
	width_ = 0;
	height_ = 0;

	return 0;
}


/**
 *	This method creates the render targets resources.
 *
 *	@param width	the desired width, in pixels, of the surface
 *	@param height	the desired height, in pixels, of the surface
 *	@param reuseMainZBuffer wether to try to use the main z 
 *			buffer as the zbuffer for this render target
 *	@param pixelFormat the desired pixel format of the render target
 *
 *	@return true if nothing went wrong.
 *	
 */
bool RenderTarget::create( int width, int height, bool reuseMainZBuffer, 
						  D3DFORMAT pixelFormat, RenderTarget* pDepthStencilParent,
						  D3DFORMAT depthFormatOverride )
{
	reuseZ_ = reuseMainZBuffer;
	width_ = width;
	height_ = height;
	pixelFormat_ = pixelFormat;
	depthFormat_ = depthFormatOverride;
	pDepthStencilParent_ = pDepthStencilParent;

	createUnmanagedObjects();

	return ( pRenderTarget_.hasComObject() );
}


/**
 *	This method pushes this render target as the current target
 *	for the device.
 *
 *	The current camera, projection matrix and viewport are saved.
 *
 *	returns true if the push was successful
 */
bool RenderTarget::push()
{
	if ( !pRenderTarget_.hasComObject() && width_ > 0 && height_ > 0 )
	{
		this->createUnmanagedObjects();
	}

	DX::Surface* pDepthTarget = NULL;
	if (pDepthStencilParent_.hasObject())
	{
		pDepthTarget = pDepthStencilParent_->depthBuffer();
	}
	else
	{
		pDepthTarget = pDepthStencilTarget_.pComObject();
	}


	MF_ASSERT( pRenderTarget_.hasComObject() );
	MF_ASSERT( pDepthTarget );

	if ( !pRenderTarget_.hasComObject() )
		return false;
	if ( !pDepthTarget )
		return false;

	if (!rc().pushRenderTarget())
		return false;

	//now, push the render target

	//get the top-level surface
	DX::Surface *pSurface;
	HRESULT hr = pRenderTarget_->GetSurfaceLevel( 0, &pSurface );
	if ( FAILED( hr ) )
	{
		rc().popRenderTarget();
		WARNING_MSG( "RenderTarget::push : Could not get surface level 0 for render target texture\n" );
		return false;
	}

	//set the render target
	hr = Moo::rc().device()->SetRenderTarget( 0, pSurface );
	if ( FAILED( hr ) )
	{
		WARNING_MSG( "RenderTarget::push : Unable to set render target on device\n" );
		pSurface->Release();
		rc().popRenderTarget();
		return false;
	}

	hr = Moo::rc().device()->SetDepthStencilSurface( pDepthTarget );
	if ( FAILED( hr ) )
	{
		WARNING_MSG( "RenderTarget::push : Unable to set depth target on device\n" );
		rc().popRenderTarget();
		return false;
	}

	//release the pSurface reference from the GetSurfaceLevel call
	pSurface->Release();

	Moo::rc().screenWidth( width_ );
	Moo::rc().screenHeight( height_ );

	return true;
}


/**
 *	This method pops the RenderTarget, and restores the
 *	camera, projection matrix and viewport
 */
void RenderTarget::pop()
{
	rc().popRenderTarget();
}


bool RenderTarget::valid()
{
	DX::Surface* pDepthTarget = NULL;
	if (pDepthStencilParent_.hasObject())
	{
		pDepthTarget = pDepthStencilParent_->depthBuffer();
	}
	else
	{
		pDepthTarget = pDepthStencilTarget_.pComObject();
	}

	return pRenderTarget_.hasComObject() && pDepthTarget;
}


void RenderTarget::deleteUnmanagedObjects( )
{
	memoryCounterSub( renderTarget );
	if (pRenderTarget_.hasComObject())
	{
		memoryClaim( pRenderTarget_ );

		pRenderTarget_ = NULL;
	}
	if (pDepthStencilTarget_.hasComObject())
	{
		if (!reuseZ_)
		{
			memoryClaim( pDepthStencilTarget_.hasComObject() );
		}

		pDepthStencilTarget_ = NULL;
	}
}


void RenderTarget::createUnmanagedObjects( )
{
	if ( width_ <= 0 || height_ <= 0 )
	{
		//no need to set up the render target yet
		return;
	}

	if ( pRenderTarget_.hasComObject() )
	{
		//probably this has been set up already during
		//the render target owner's createUnmanaged call.
		return;
	}

	memoryCounterAdd( renderTarget );

	DX::Texture* pTargetTexture = NULL;

	//The PC can only create standard 32bit colour render targets.
	HRESULT hr =  Moo::rc().device()->CreateTexture(
		width_, height_, 1, D3DUSAGE_RENDERTARGET, pixelFormat_, D3DPOOL_DEFAULT, &pTargetTexture, NULL );
	if (FAILED(hr))
	{
		WARNING_MSG( "RenderTarget : Could not create render target texture\n" );
		
		return;
	}

	pRenderTarget_ = pTargetTexture;
	pRenderTarget_->Release();

	memoryClaim( pRenderTarget_ );


	if (!pDepthStencilParent_.hasObject())
	{

		D3DFORMAT depthStencilFormat = Moo::rc().presentParameters().AutoDepthStencilFormat;

		//overriding depth format...
		if (depthFormat_ != D3DFMT_UNKNOWN) 
		{
			depthStencilFormat = depthFormat_;
		}

		DX::Surface* pTargetSurface = NULL;
		if ( reuseZ_ )
		{
			hr = Moo::rc().device()->GetDepthStencilSurface( &pTargetSurface );
			pDepthStencilTarget_ = pTargetSurface;
			pDepthStencilTarget_->Release();
		}
		else	
		{
			hr = Moo::rc().device()->CreateDepthStencilSurface(
					width_,
					height_,
					depthStencilFormat,
					D3DMULTISAMPLE_NONE, 0, TRUE,
					&pTargetSurface, NULL );

			if (!FAILED(hr))
			{
				pDepthStencilTarget_ = pTargetSurface;
				pDepthStencilTarget_->Release();
				memoryClaim( pDepthStencilTarget_ );
			}
		}

		if (FAILED(hr))
		{
			WARNING_MSG( "RenderTarget : Could not create depth stencil surface.\n" );
			this->deleteUnmanagedObjects();
			return;
		}
	}

	if (pRenderTarget_.hasComObject() != NULL && autoClear_)
	{
		this->push();
		rc().device()->Clear( 0, NULL, D3DCLEAR_TARGET, clearColour_, 1, 0 );
		this->pop();
	}
}


/**
 *	This method tells the render target whether or not to 
 *	clear its surface automatically upon a device recreation.
 *
 *	By default, render targets are left uninitialised after
 *	changing the screen size or switching to/from fullscreen.
 *
 *	@param	enable		True to enable auto-clear
 *	@param	col			Colour to clear the colour buffer to
 */
void RenderTarget::clearOnRecreate( bool enable, const Colour& col )
{
	autoClear_ = enable;
	clearColour_ = col;
}

}
