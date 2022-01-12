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
#include "full_screen_back_buffer.hpp"
#include "geometrics.hpp"
#include "texture_feeds.hpp"
#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT2( "Romp", 0 );


FullScreenBackBuffer FullScreenBackBuffer::s_instance;

PyTextureProviderPtr feed;

FullScreenBackBuffer::FullScreenBackBuffer():
	pRT_( NULL ),
	inited_( false ),
	failed_( false )
{
}


FullScreenBackBuffer::~FullScreenBackBuffer()
{
	if (pRT_)
	{
		pRT_=NULL;
	}

	if (feed)
	{
		feed=NULL;
	}
}


const uint32 largerPow2( uint32 number )
{
	const float LOG2_10 = 3.3219280948873626f;
	float n = log10f( float(number) ) * LOG2_10;
	uint32 shift = uint32(ceil(n));
	if (n-floor(n) < 0.01f)
		shift = uint32(floor(n));
	return 1 << shift;
}


void FullScreenBackBuffer::deleteUnmanagedObjects()
{
	if (pRT_)
	{
		pRT_->release();
		pRT_ = NULL;
	
		feed = NULL;
		TextureFeeds::delTextureFeed( "backBuffer" );
	}

	inited_ = false;
	failed_ = false;
}


bool FullScreenBackBuffer::initInternal()
{
	pRT_ = new Moo::RenderTarget( "FullScreenBackBufferCopy" );

	uint32 w = (uint32)Moo::rc().screenWidth();
	uint32 h = (uint32)Moo::rc().screenHeight();
	uint32 bufferW = w;
	uint32 bufferH = h;
	uint32 caps = Moo::rc().deviceInfo( Moo::rc().deviceIndex() ).caps_.TextureCaps;
	reuseZBuffer_ = true;
	if (caps & D3DPTEXTURECAPS_POW2 && !(caps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL))
	{
		bufferW = largerPow2( w );
		bufferH = largerPow2( h );
		if (caps & D3DPTEXTURECAPS_SQUAREONLY)
			bufferW = bufferH = max( bufferW, bufferW );
		reuseZBuffer_ = false;
	}
	vp_.X = 0;
	vp_.Y = 0;
	vp_.MinZ = 0;
	vp_.MaxZ = 1;
	vp_.Width = DWORD(w);
	vp_.Height = DWORD(h);

	if (pRT_->create( bufferW, bufferH, reuseZBuffer_ ))
	{
		feed = PyTextureProviderPtr( new PyTextureProvider( NULL, pRT_ ), true);
		TextureFeeds::addTextureFeed( "backBuffer",  feed );
		inited_ = true;
	}
	else
	{
		inited_ = false;
		failed_ = true;
		pRT_ = NULL;
		return false;
	}

	return (pRT_->pTexture() != NULL);
}


bool FullScreenBackBuffer::beginSceneInternal()
{
	//Check if anyone actually wants a full screen buffer
	if ( !hasEnabledUsers() )
	{
		return false;
	}
	
	if ( !inited_ && !failed_ )
	{
		this->initInternal();			
	}

	if ( !inited_ )
		return false;

	
	//Note pRT_->push sets the width and height of the render context, but not the viewport.
	pRT_->push();
	Moo::rc().device()->SetViewport( &vp_ );
	Moo::rc().device()->Clear( 0, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_TARGET, 0x00000080, 1, 0 );

	//Now set the viewport to be screen sized.
	Moo::rc().screenWidth( vp_.Width );
	Moo::rc().screenHeight( vp_.Height );
	Moo::rc().device()->SetViewport( &vp_ );

	Users::iterator it = users_.begin();
	while (it != users_.end())
	{
		User* user = *it++;
		if (user->isEnabled())
		{
			user->beginScene();
		}
	}

	return true;
}


void FullScreenBackBuffer::endSceneInternal()
{
	//If nobody wanted us, we don't need to end scene.
	if ( !hasEnabledUsers() || !inited_ )
	{
		return;
	}

	Users::iterator it = users_.begin();
	while (it != users_.end())
	{
		User* user = *it++;
		if (user->isEnabled())
		{
			user->endScene();
		}
	}	
	
	pRT_->pop();

	//Now do the transfer
	bool transferred = false;
	it = users_.begin();
	while (it != users_.end())
	{
		User* user = *it++;
		if (user->isEnabled())
		{
			transferred |= user->doTransfer( transferred );
		}
	}

	//If nobody has transferred the offscreen back buffer over
	//to the main back buffer, then do it now.
	if (!transferred)
	{		
		Moo::rc().device()->SetPixelShader(NULL);

		//this is just a plain-vanilla copy.
		Moo::rc().setTexture( 0, FullScreenBackBuffer::renderTarget().pTexture() );

		Geometrics::texturedRect( Vector2(0.f,0.f),
			Vector2(Moo::rc().screenWidth(),Moo::rc().screenHeight()),
			Vector2(0,0),
			Vector2(FullScreenBackBuffer::uSize(),FullScreenBackBuffer::vSize()),
			Moo::Colour( 1.f, 1.f, 1.f, 1.f ), true );
	}

	//Now do post-transfer filters	
	it = users_.begin();
	while (it != users_.end())
	{
		User* user = *it++;
		if (user->isEnabled())
		{
			user->doPostTransferFilter();
		}
	}
}


void FullScreenBackBuffer::addUserInternal( User* u )
{
	users_.push_back(u);	
}


void FullScreenBackBuffer::delUserInternal( User* u )
{
	Users::iterator it = users_.begin();
	while (it != users_.end())
	{		
		if (*it == u)
		{
			users_.erase(it);
			return;
		}
		it++;
	}

	WARNING_MSG( "Tried to remove a FSBB user not in the list\n" );
}


bool FullScreenBackBuffer::hasEnabledUsers()
{
	Users::iterator it = users_.begin();
	while (it != users_.end())
	{
		User* user = *it++;
		if (user->isEnabled())
			return true;
	}

	return false;
}