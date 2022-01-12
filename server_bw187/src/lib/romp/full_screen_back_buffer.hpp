/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef FULL_SCREEN_BACK_BUFFER_HPP
#define FULL_SCREEN_BACK_BUFFER_HPP

#include "moo/render_target.hpp"


/**
 *	This class acts as a single sink for full-screen back buffer copies.  It does this
 *	by hijacking the render target before the main draw loop, and transferring the
 *	image to the back buffer later on.
 */
class FullScreenBackBuffer : public Moo::DeviceCallback
{
public:
	class User
	{
	public:
		virtual bool isEnabled() = 0;
		virtual void beginScene() = 0;
		virtual void endScene() = 0;
		virtual bool doTransfer( bool alreadyTransferred ) = 0;
		virtual void doPostTransferFilter() = 0;
	};

	static Moo::RenderTarget& renderTarget(){ return *s_instance.pRT_; }
	static bool init()						{ return s_instance.initInternal(); }
	static bool beginScene()				{ return s_instance.beginSceneInternal(); }
	static void endScene()					{ s_instance.endSceneInternal(); }
	static void addUser( User* u )			{ s_instance.addUserInternal( u ); }
	static void removeUser( User* u )		{ s_instance.delUserInternal( u ); }	
	static bool reuseZBuffer()				{ return s_instance.reuseZBuffer_; }
	static float uSize( ) 
	{ 
		return float(s_instance.vp_.Width) / 
			float(s_instance.pRT_->width() ); 
	}
	static float vSize( )					
	{ 
		return float(s_instance.vp_.Height) / 
			float(s_instance.pRT_->height() ); 
	}
	void deleteUnmanagedObjects();

	~FullScreenBackBuffer();

private:
	bool hasEnabledUsers();
	bool beginSceneInternal();
	void endSceneInternal();
	void addUserInternal( User* u );
	void delUserInternal( User* u );
	bool initInternal();
	FullScreenBackBuffer();
	static FullScreenBackBuffer	s_instance;
	DX::Viewport vp_;
	Moo::RenderTargetPtr pRT_;
	bool	inited_;
	bool	failed_;
	bool	reuseZBuffer_;
	typedef std::vector<User*> Users;
	Users users_;
};


#endif