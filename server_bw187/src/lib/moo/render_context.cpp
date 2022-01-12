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

#include <algorithm>
#include <d3dx9.h>

#include "cstdmf/dogwatch.hpp"
#include "cstdmf/watcher.hpp"

#include "device_callback.hpp"

#include "render_context.hpp"
#include "vertex_formats.hpp"
#include "dynamic_vertex_buffer.hpp"
#include "dynamic_index_buffer.hpp"
#include "effect_state_manager.hpp"

#include "processor_affinity.hpp"


#ifndef CODE_INLINE
#include "render_context.ipp"
#endif

Moo::RSCacheEntry	Moo::RenderContext::rsCache_[D3DRS_MAX];
Moo::TSSCacheEntry	Moo::RenderContext::tssCache_[64][D3DTSS_MAX];
Moo::SampCacheEntry	Moo::RenderContext::sampCache_[64][D3DSAMP_MAX];
Moo::TextureCacheEntry	Moo::RenderContext::textureCache_[64];

DX::VertexDeclaration* Moo::RenderContext::vertexDeclaration_;
uint32 Moo::RenderContext::fvf_;

namespace // anonymous
{

static bool s_lost=false;

}

DECLARE_DEBUG_COMPONENT2( "Moo", 0 )

// comparison operators for d3ddisplaymodes
inline bool operator == ( const D3DDISPLAYMODE& dm1, const D3DDISPLAYMODE& dm2 )
{
	return ( ( dm1.Width == dm2.Width ) &&
		( dm1.Height == dm2.Height ) &&
		( dm1.Format == dm2.Format ) &&
		( dm1.RefreshRate == dm2.RefreshRate ) );
}

inline bool operator != ( const D3DDISPLAYMODE& dm1, const D3DDISPLAYMODE& dm2 )
{
	return !( dm1 == dm2 );
}

inline bool operator < ( const D3DDISPLAYMODE& dm1, const D3DDISPLAYMODE& dm2 )
{
	if( dm1.Width < dm2.Width )
		return true;
	if( dm1.Width > dm2.Width )
		return false;
	if( dm1.Height < dm2.Height )
		return true;
	if( dm1.Height > dm2.Height )
		return false;
	if( dm1.Format < dm2.Format )
		return true;
	if( dm1.Format > dm2.Format )
		return false;
	if( dm1.RefreshRate < dm2.RefreshRate )
		return true;
	if( dm1.RefreshRate > dm2.RefreshRate )
		return false;
	return false;
}

namespace Moo
{

/**
 *
 */
class OcclusionQuery
{
public:
	int					index;
	IDirect3DQuery9*	queryObject;
private:
};

THREADLOCAL( bool ) g_renderThread = false;

namespace // anonyous
{

// Flie scope variables
HCURSOR s_OriginalMouseCursor = NULL;

// Helper functions
void printErrorMsg_( HRESULT hr, const std::string & msg );

} // namespace anonyous

/**
 *	This method sets up the gamma correction value on the device.
 */

void RenderContext::setGammaCorrection()
{
	gammaCorrection_ = max( 0.5f, min( 6.f, gammaCorrection_ ) );
	if (device_.hasComObject())
	{
		D3DGAMMARAMP ramp;

		for (uint32 i = 0; i < 256; i++)
		{
			float f = /*1.f -*/ (float(i) / 255.f);
			ramp.red[i] = ramp.green[i] = ramp.blue[i] = WORD( ( /*1.f -*/ powf( f, 1.f / gammaCorrection_ ) ) * 65535.f );
		}

		device_->SetGammaRamp( 0, D3DSGR_CALIBRATE, &ramp );
	}
}


/**
 *	The render contexts constructor, prepares to create a dx device, and
 *	sets default values.
 */
RenderContext::RenderContext()
: camera_( 0.25, 500, DEG_TO_RAD( 60 ), 4.f/3.f ),
  windowed_( true ),
  windowedStyle_( WS_OVERLAPPEDWINDOW ),
  currentFrame_( 0 ),
  primitiveGroupCount_( 0 ),
  fogNear_( 0 ),
  fogFar_( 500 ),
  halfScreenWidth_( 320.f ),
  halfScreenHeight_( 240.f ),
  fullScreenAspectRatio_( 4.f/3.f ),
  alphaOverride_( 0xff000000 ),
  lodValue_( 1.f ),
  lodFar_( 400 ),
  lodPower_( 10 ),
  zoomFactor_( 1 ),
  stencilWanted_( false ),
  stencilAvailable_( false ),
  gammaCorrection_( 1.f ),
  waitForVBL_( true ),
  tripleBuffering_( true ),
  vsVersion_( 0 ),
  psVersion_( 0 ),
  mixedVertexProcessing_( false ),
  mirroredTransform_(false),
  reflectionScene_(false),
  paused_( false ),
  beginSceneCount_(0)/*,
  primitiveHistogramResolution_( 5 )*/
{
	view_.setIdentity();
	projection_.setIdentity();
	viewProjection_.setIdentity();
	invView_.setIdentity();
	world_.push_back( projection_ );

	// Create the directx9 interface
	d3d_.pComObject( Direct3DCreate9( D3D_SDK_VERSION ) );
	if( d3d_.pComObject() != NULL )
	{
		// Reduce the reference count, because d3ds_.pComobject( ) increases it.
		d3d_->Release();

		// Iterate through the adapters.
		for (uint32 adapterIndex = 0; adapterIndex < d3d_->GetAdapterCount(); adapterIndex++)
		{
			DeviceInfo deviceInfo;
			deviceInfo.windowed_ = false;
			deviceInfo.adapterID_ = adapterIndex;
			if (D3D_OK == d3d_->GetAdapterIdentifier( adapterIndex, 0, &deviceInfo.identifier_ ))
			{
				if (D3D_OK == d3d_->GetDeviceCaps( adapterIndex, D3DDEVTYPE_HAL, &deviceInfo.caps_ ))
				{
					// set up compatibilty flags for special rendering codepaths
					deviceInfo.compatibilityFlags_ = 0;
					if (strstr(deviceInfo.identifier_.Description, "GeForce4 MX") != NULL)
					{
						// this card does not support D3DLOCK_NOOVERWRITE in the way flora uses it
						deviceInfo.compatibilityFlags_ |= COMPATIBILITYFLAG_NOOVERWRITE;
						WARNING_MSG("GeForce4 MX compatibility selected");
					}

					D3DFORMAT formats[] = { D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8, D3DFMT_A2B10G10R10, D3DFMT_X1R5G5B5, D3DFMT_A1R5G5B5, D3DFMT_R5G6B5 };
					uint32 nFormats = sizeof( formats ) / sizeof(D3DFORMAT);

					for (uint32 fi = 0; fi < nFormats; fi++)
					{
						D3DFORMAT format = formats[fi];
						// Iterate through display modes
						for (uint32 displayModeIndex = 0; displayModeIndex < d3d_->GetAdapterModeCount( adapterIndex, format ); displayModeIndex++)
						{
							D3DDISPLAYMODE mode;
							if (D3D_OK == d3d_->EnumAdapterModes( adapterIndex, format, displayModeIndex, &mode ))
							{
								mode.RefreshRate = 0;
								if( std::find( deviceInfo.displayModes_.begin(), deviceInfo.displayModes_.end(), mode ) == deviceInfo.displayModes_.end() )
								{
									if( mode.Width >= 640 && mode.Height >= 480 )
										deviceInfo.displayModes_.push_back( mode );
								}
							}
						}
					}
					deviceInfo.windowed_ = true; //( deviceInfo.caps_.Caps2 & D3DCAPS2_CANRENDERWINDOWED ) != 0;
				}

				if (deviceInfo.displayModes_.size() != 0 || deviceInfo.windowed_ == true)
				{

					if( deviceInfo.windowed_ )
					{
						d3d_->GetAdapterDisplayMode( deviceInfo.adapterID_, &deviceInfo.windowedDisplayMode_ );
					}

					std::sort( deviceInfo.displayModes_.begin(), deviceInfo.displayModes_.end() );
					devices_.push_back( deviceInfo );
				}
			}
		}

		if (!devices_.size())
		{
			CRITICAL_MSG( "Moo::RenderContext::RenderContext: No hardware rasterisation devices found.\n" );
		}
	}
	else
	{
		CRITICAL_MSG( "Moo::RenderContext::RenderContext: Unable to create Directx interface. Is Directx9c installed?\n" );
		return;
	}
}

/**
 *	This method changes the current display mode or windowed/fullscreen
 *	mode on the current device.
 *
 *	@param modeIndex is the index of the displaymode to choose.
 *	@param windowed tells the rendercontext whether to use windowed mode or not.
 *	@param testCooperative if true, tests the device's cooperative level before
 *		   changing the mode. The mode is not changed if the device is lost.
 *	@return true if the operation was successful.
 */
bool RenderContext::changeMode( uint32 modeIndex, bool windowed, bool testCooperative, uint32 backBufferWidthOverride )
{
	if (testCooperative && 
		Moo::rc().device()->TestCooperativeLevel() == D3DERR_DEVICELOST)
	{
		return false;
	}

	// Avoid changeMode to be called recursively from 
	// App::resizeWindow. resizeWindow will be triggered 
	// by the SetWindowPos call when going windowed.
	static bool changingMode = false;
	if (changingMode)
	{
		return false;
	}
	changingMode = true;

	bool oldWindowed_ = windowed_;

	screenCopySurface_ = NULL;
	StateRecorder::clear();
	DeviceCallback::deleteAllUnmanaged();
	releaseQueryObjects();
	MF_ASSERT( device_.pComObject() );
	modeIndex_ = modeIndex;
	windowed_ = windowed;
	backBufferWidthOverride_ = backBufferWidthOverride;
	fillPresentationParameters();

	// are we changing windowed status?
	if(windowed_ != oldWindowed_)
	{
		if(windowed_ == false)
		{
			// from windowed to fullscreen
			// save windows geometry info
			GetWindowRect( windowHandle_, &windowedRect_ );
			// save the windowed style to restore it later, and set it to
			// WS_POPUP so the fullscreen window doesn't have a caption.
			windowedStyle_ = GetWindowLong( windowHandle_, GWL_STYLE );
			SetWindowLong( windowHandle_, GWL_STYLE, WS_POPUP );
		}
		else
		{
			// from fullscreen to windowed
			// restore the window's windowed style
			SetWindowLong( windowHandle_, GWL_STYLE, windowedStyle_ );
			// restore windows geometry
			SetWindowPos( windowHandle_, HWND_NOTOPMOST,
						  windowedRect_.left, windowedRect_.top,
						  ( windowedRect_.right - windowedRect_.left ),
						  ( windowedRect_.bottom - windowedRect_.top ),
						  SWP_SHOWWINDOW );
		}
	}

    // Some drivers have difficulty in restoring a device for a minimised 
    // window unless the width and height of the window are set in the 
    // presentation parameters.
    WINDOWPLACEMENT placement;
    GetWindowPlacement(windowHandle_, &placement);
    if (backBufferWidthOverride_ == 0 && placement.showCmd == SW_SHOWMINIMIZED)
    {
        RECT &rect = placement.rcNormalPosition;
        presentParameters_.BackBufferWidth  = abs(rect.right  - rect.left);
        presentParameters_.BackBufferHeight = abs(rect.bottom - rect.top );
    }

	HRESULT hr = D3DERR_DEVICELOST;
	while( hr == D3DERR_DEVICELOST )
	{
		INFO_MSG( "RenderContext::changeMode - trying to reset \n" );
		hr = device_->Reset( &presentParameters_ );

		s_lost = true;
		if( FAILED( hr ) && hr != D3DERR_DEVICELOST )
		{
			std::string msg = "Moo::RenderContext::changeMode: Unable to reset device ";

			printErrorMsg_( hr, msg );
			changingMode = false;
			return false;
		}
		Sleep( 100 );
	}
	s_lost = false;

	beginSceneCount_ = 0;
	UINT availTexMem = device_->GetAvailableTextureMem();
	INFO_MSG( "RenderContext::changeMode - available texture memory after reset: %d\n", availTexMem );

	updateDeviceInfo();
	updateProjectionMatrix();
	initRenderStates();
	if (mixedVertexProcessing_)
		device_->SetSoftwareVertexProcessing( TRUE );

	DeviceCallback::createAllUnmanaged();
	createQueryObjects();

    // Set the affinity of the app.  There were some problems previously with
    // multi-core systems.
    updateProcessorAffinity();

	changingMode = false;
	return true;
}

/*
 *	this method updates all the info that can be queried from
 *	outside the rendercontext.
 */
void RenderContext::updateDeviceInfo( )
{
	DX::Surface*  pBackBuffer;
	device_->GetBackBuffer( 0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer );
	pBackBuffer->GetDesc( &backBufferDesc_ );
	pBackBuffer->Release();

	halfScreenWidth_ = float( backBufferDesc_.Width ) * 0.5f;
	halfScreenHeight_ = float( backBufferDesc_.Height ) * 0.5f;

	DX::Surface* surf = NULL;

	D3DDISPLAYMODE smode;
	d3d_->GetAdapterDisplayMode( devices_[ deviceIndex_ ].adapterID_, &smode );

	HRESULT hr;
	if ( SUCCEEDED( hr = device_->CreateOffscreenPlainSurface( backBufferDesc_.Width, backBufferDesc_.Height, 
		D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &surf, NULL ) ) )
	{
		screenCopySurface_.pComObject( surf );
		surf->Release();
	}
	else
	{
		screenCopySurface_.pComObject( NULL );
	}

	DEBUG_MSG( "Moo: Changed mode: %d %d\n", backBufferDesc_.Width, backBufferDesc_.Height );
}


/*
 *	This method sets up the presentation parameters, for a reset or a createdevice call.
 */
void RenderContext::fillPresentationParameters( void )
{
	ZeroMemory( &presentParameters_, sizeof( presentParameters_ ) );

	if( windowed_ )
	{
		presentParameters_.BackBufferWidth = backBufferWidthOverride_;
		presentParameters_.BackBufferHeight = static_cast<uint32>( backBufferWidthOverride_ * ( screenHeight() / screenWidth() ) );
		presentParameters_.BackBufferFormat = devices_[ deviceIndex_ ].windowedDisplayMode_.Format;
		presentParameters_.PresentationInterval = waitForVBL_ ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
	}
	else
	{
		D3DDISPLAYMODE mode = devices_[ deviceIndex_ ].displayModes_[ modeIndex_ ];
		presentParameters_.BackBufferWidth = mode.Width;
		presentParameters_.BackBufferHeight = mode.Height;
		presentParameters_.BackBufferFormat = mode.Format;
		presentParameters_.PresentationInterval = waitForVBL_ ? D3DPRESENT_INTERVAL_DEFAULT : D3DPRESENT_INTERVAL_IMMEDIATE;
	}

	presentParameters_.Windowed = windowed_;
	presentParameters_.BackBufferCount = tripleBuffering_ ? 2 : 1;
	presentParameters_.SwapEffect = D3DSWAPEFFECT_DISCARD;
	presentParameters_.MultiSampleType = D3DMULTISAMPLE_NONE;
	presentParameters_.EnableAutoDepthStencil = TRUE;
	if (!s_lost)
		presentParameters_.AutoDepthStencilFormat = getMatchingZBufferFormat( presentParameters_.BackBufferFormat );
	else
		presentParameters_.AutoDepthStencilFormat = D3DFMT_D16;
	presentParameters_.Flags = 0;
	presentParameters_.hDeviceWindow = windowHandle_;
}


/**
 *	This method releases the rendercontexts reference to a d3d device, and
 *	tries to clean up all references to d3d objects.
 */
void RenderContext::releaseDevice( void )
{
	ShowCursor( TRUE );
	SetCursor( s_OriginalMouseCursor );

	screenCopySurface_ = NULL;
	DeviceCallback::deleteAllUnmanaged();
	DeviceCallback::deleteAllManaged();
	releaseQueryObjects(); // TODO should assert here that queries have not leaked (i.e. other code should release the queries it uses)

	// Release all device objects
	if( device_.pComObject() != NULL )
	{

		device_->SetIndices( NULL );

		for( uint32 i = 0; ( i < devices_[ deviceIndex_ ].caps_.MaxStreams ) || ( i < 1 ); i++ )
		{
			device_->SetStreamSource( i, NULL, 0, 0 );
		}

		//release all textures
		for( uint32 i = 0; i < devices_[ deviceIndex_ ].caps_.MaxSimultaneousTextures; i++ )
		{
			device_->SetTexture( i, NULL );
		}

		//release shaders
		Moo::rc().setPixelShader( NULL );
		//reset window if it is not the correct size
		if( !windowed_ )
		{
			SetWindowLong( windowHandle_, GWL_STYLE, windowedStyle_ );
            SetWindowPos( windowHandle_, HWND_NOTOPMOST,
                          windowedRect_.left, windowedRect_.top,
                          ( windowedRect_.right - windowedRect_.left ),
                          ( windowedRect_.bottom - windowedRect_.top ),
                          SWP_SHOWWINDOW );
		}
	}

	// Release the render device
	device_.pComObject( NULL );
}

/*
 *	Gets amount of available texture memory.
 */
uint RenderContext::getAvailableTextureMem( ) const
{
	return device_->GetAvailableTextureMem();
}

/*
 *	Get a zbuffer format that is allowed with the current render format.
 */
D3DFORMAT RenderContext::getMatchingZBufferFormat( D3DFORMAT colourFormat )
{
	uint32 adapter = devices_[ deviceIndex_ ].adapterID_;

	if( colourFormat == D3DFMT_R8G8B8 || colourFormat == D3DFMT_X8R8G8B8 || colourFormat == D3DFMT_A8R8G8B8 )
	{
		if ( !stencilWanted_ )
		{
			if( SUCCEEDED( d3d_->CheckDeviceFormat( adapter, D3DDEVTYPE_HAL, colourFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D32 ) ) &&
				SUCCEEDED( d3d_->CheckDepthStencilMatch( adapter, D3DDEVTYPE_HAL, colourFormat, colourFormat, D3DFMT_D32 ) ) )
				return D3DFMT_D32;
			if( SUCCEEDED( d3d_->CheckDeviceFormat( adapter, D3DDEVTYPE_HAL, colourFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D24X8 ) ) &&
				SUCCEEDED( d3d_->CheckDepthStencilMatch( adapter, D3DDEVTYPE_HAL, colourFormat, colourFormat, D3DFMT_D24X8 ) ) )
				return D3DFMT_D24X8;
			if( SUCCEEDED( d3d_->CheckDeviceFormat( adapter, D3DDEVTYPE_HAL, colourFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D24S8 ) ) &&
				SUCCEEDED( d3d_->CheckDepthStencilMatch( adapter, D3DDEVTYPE_HAL, colourFormat, colourFormat, D3DFMT_D24S8 ) ) )
				return D3DFMT_D24S8;
			if( SUCCEEDED( d3d_->CheckDeviceFormat( adapter, D3DDEVTYPE_HAL, colourFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D24X4S4 ) ) &&
				SUCCEEDED( d3d_->CheckDepthStencilMatch( adapter, D3DDEVTYPE_HAL, colourFormat, colourFormat, D3DFMT_D24X4S4 ) ) )
				return D3DFMT_D24X4S4;
		}
		else
		{
			if( SUCCEEDED( d3d_->CheckDeviceFormat( adapter, D3DDEVTYPE_HAL, colourFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D24S8 ) ) &&
				SUCCEEDED( d3d_->CheckDepthStencilMatch( adapter, D3DDEVTYPE_HAL, colourFormat, colourFormat, D3DFMT_D24S8 ) ) )
			{
				stencilAvailable_ = true;
				return D3DFMT_D24S8;
			}
			else
			{
				stencilAvailable_ = false;
			}
		}
	}

	if ( !stencilWanted_ )
	{
		if( SUCCEEDED( d3d_->CheckDeviceFormat( adapter, D3DDEVTYPE_HAL, colourFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D16 ) ) &&
			SUCCEEDED( d3d_->CheckDepthStencilMatch( adapter, D3DDEVTYPE_HAL, colourFormat, colourFormat, D3DFMT_D16 ) ) )
			return D3DFMT_D16;
		if( SUCCEEDED( d3d_->CheckDeviceFormat( adapter, D3DDEVTYPE_HAL, colourFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D15S1 ) ) &&
			SUCCEEDED( d3d_->CheckDepthStencilMatch( adapter, D3DDEVTYPE_HAL, colourFormat, colourFormat, D3DFMT_D15S1 ) ) )
			return D3DFMT_D15S1;
		if( SUCCEEDED( d3d_->CheckDeviceFormat( adapter, D3DDEVTYPE_HAL, colourFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D24X8 ) ) &&
			SUCCEEDED( d3d_->CheckDepthStencilMatch( adapter, D3DDEVTYPE_HAL, colourFormat, colourFormat, D3DFMT_D24X8 ) ) )
			return D3DFMT_D24X8;
		if( SUCCEEDED( d3d_->CheckDeviceFormat( adapter, D3DDEVTYPE_HAL, colourFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D32 ) ) &&
			SUCCEEDED( d3d_->CheckDepthStencilMatch( adapter, D3DDEVTYPE_HAL, colourFormat, colourFormat, D3DFMT_D32 ) ) )
			return D3DFMT_D32;
		if( SUCCEEDED( d3d_->CheckDeviceFormat( adapter, D3DDEVTYPE_HAL, colourFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D24S8 ) ) &&
			SUCCEEDED( d3d_->CheckDepthStencilMatch( adapter, D3DDEVTYPE_HAL, colourFormat, colourFormat, D3DFMT_D24S8 ) ) )
			return D3DFMT_D24S8;
		if( SUCCEEDED( d3d_->CheckDeviceFormat( adapter, D3DDEVTYPE_HAL, colourFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D24X4S4 ) ) &&
			SUCCEEDED( d3d_->CheckDepthStencilMatch( adapter, D3DDEVTYPE_HAL, colourFormat, colourFormat, D3DFMT_D24X4S4 ) ) )
			return D3DFMT_D24X4S4;
	}
	else
	{
		if( SUCCEEDED( d3d_->CheckDeviceFormat( adapter, D3DDEVTYPE_HAL, colourFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D24S8 ) ) &&
			SUCCEEDED( d3d_->CheckDepthStencilMatch( adapter, D3DDEVTYPE_HAL, colourFormat, colourFormat, D3DFMT_D24S8 ) ) )
		{
			stencilAvailable_ = true;
			return D3DFMT_D24S8;
		}
		else
		{
			stencilAvailable_ = false;
		}
	}
	return D3DFMT_D16;
}

/**
 *	This method updates the projection matrix based on the current camera.
 */
void RenderContext::updateProjectionMatrix( )
{
	if ( !renderTargetStack_.nStackItems() )
	{
		if( !windowed_ )
		{
			camera_.aspectRatio( fullScreenAspectRatio_ );
		}
		else
		{
			camera_.aspectRatio( float( backBufferDesc_.Width ) / float( backBufferDesc_.Height ) );
		}
	}

	zoomFactor_ = camera_.fov() / DEG_TO_RAD( 60 );

	projection_.perspectiveProjection( camera_.fov(), camera_.aspectRatio(), camera_.nearPlane(), camera_.farPlane() );
}

/**
 *	This method checks the device's cooperative level. It returns true if
 *	the device is operational. If it has been lost, it tries to reset it,
 *	returning true if successful. It returns false if the device was lost 
 *	cannot be reset immediatly. CRITICAL_MSG will be triggered if the 
 *	device has entered an unrecoverable state.	
 *
 *  @param      reset set to true if the device was reset.
 *	@return		true if device is operational, false otherwise.
 */
bool RenderContext::checkDevice(bool *reset /*=NULL*/)
{
	bool result = false;
    if (reset != NULL)
        *reset = false;
	HRESULT hr = Moo::rc().device()->TestCooperativeLevel();
	switch (hr)
	{
		case D3D_OK:
			result = true;
			break;
		case D3DERR_DEVICELOST:
			result = false;
			break;
		case D3DERR_DEVICENOTRESET:
		{
			s_lost = true;
			int modeIndex = this->modeIndex();
			bool windowed = this->windowed();
			result = Moo::rc().changeMode(modeIndex, windowed);
            if (reset != NULL)
                *reset = result;
			break;
		}
		case D3DERR_DRIVERINTERNALERROR:
			CRITICAL_MSG("Internal Driver Error");
			break;
		case D3DERR_OUTOFVIDEOMEMORY:
			CRITICAL_MSG("D3DERR_OUTOFVIDEOMEMORY");
			break;
		default:
			INFO_MSG( "RenderContext::checkDevice - UNHANDLED D3D RESULT : %d\n", hr );
			break;
	}
	s_lost = !result;
	return result;
}

/**
 *	Tests the cooperative level of the device, seizing control if the 
 *	device has been lost. In this case, starts retesting the device 
 *	cooperative level, at a fixed time intervals, until it can be reset. 
 *	When that finally happens, resets the device and returns true. 
 *
 *	Messages requesting the application to close will make the function 
 *	return false. All Other messages are translated and dispatched to the 
 *	registered WinProc function. 
 *
 *	CRITICAL_MSG will be triggered if the device enters an unrecoverable 
 *	state.	
 *
 *  @param      reset set to true if the device was reset.
 *	@return		true if device is operational. False if a WM_QUIT 
 *				message or ALT+F4 key event has been intercepted.
 */ 
bool RenderContext::checkDeviceYield(bool *reset /*= NULL*/)
{
	bool result = true;
	while (!(result = this->checkDevice(reset)))
	{
		MSG msg;
		if (::PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			if ((msg.message == WM_QUIT) ||
				(msg.message == WM_KEYDOWN && 
					msg.wParam == VK_F4 && ::GetKeyState(VK_MENU)))
			{
				result = false;
				break;
			}
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}
		::Sleep(50);
	}
	return result;
}

/**
 *	Creates the d3d device and binds it to a window.
 *
 *	@param hWnd is the windowhandle to bind the d3d device to.
 *	@param deviceIndex is the index of the device to create.
 *	@param modeIndex is the index of the displaymode to initialise.
 *	@param windowed says whether to create a windowed or fullscreen device.
 *	@param requireStencil says whether a stencil buffer is required.
 *	@return true if the operation was successful.
 */
bool RenderContext::createDevice( HWND hWnd, uint32 deviceIndex,
								 uint32 modeIndex, bool windowed,
								 bool requireStencil )
{
	g_renderThread = true;
	if( deviceIndex > devices_.size() )
	{
		CRITICAL_MSG( "Moo::RenderContext::createDevice: Tried to select a device that does not exist\n" );
		return false;
	}

	// Have we created a device before?
	if( device_.pComObject() != NULL )
	{
		// If the device is the same as the current device, just change the display mode, else release the device.
		if( deviceIndex == deviceIndex_ && windowHandle_ == hWnd )
		{
			return changeMode( modeIndex, windowed );
		}
		else
		{
			releaseDevice();
		}
	}

	// Set up the current device parameters.
	windowHandle_ = hWnd;
	// Assumes that the style the window has when creating the device is the
	// style desired when in windowed mode, and will be restored when the
	// device is released.
	windowedStyle_ = GetWindowLong( windowHandle_, GWL_STYLE );
	deviceIndex_ = deviceIndex;
	modeIndex_ = modeIndex;
	windowed_ = windowed;
	stencilWanted_ = requireStencil;

	fillPresentationParameters();
	DX::Device* device;

	if( windowed_ == false )
	{
	    GetWindowRect( windowHandle_, &windowedRect_ );
	}

	uint32 vertexProcessing = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
	mixedVertexProcessing_ = false;

	if (devices_[deviceIndex].caps_.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)
	{
		// Treat hardware with no pixel or vertex shader support as fixed
		// function cards.
		if (uint16(devices_[deviceIndex].caps_.VertexShaderVersion) > 0x100 &&
			uint16(devices_[deviceIndex].caps_.PixelShaderVersion) > 0x100)
		{
			vertexProcessing = D3DCREATE_HARDWARE_VERTEXPROCESSING;
		}
		else
		{
			vertexProcessing = D3DCREATE_MIXED_VERTEXPROCESSING;
			mixedVertexProcessing_ = true;
		}
	}

	D3DDEVTYPE devType = D3DDEVTYPE_HAL;
	if (std::string(devices_[ deviceIndex_ ].identifier_.Description) == std::string("NVIDIA NVPerfHUD"))
		devType = D3DDEVTYPE_REF;

	int retries = 5;
	HRESULT hr = D3D_OK;
	while ( retries-- )
	{
		hr = d3d_->CreateDevice( devices_[ deviceIndex_ ].adapterID_, devType, windowHandle_,
			vertexProcessing | D3DCREATE_DISABLE_DRIVER_MANAGEMENT | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE, &presentParameters_, &device );

		if( SUCCEEDED( hr ) )
			break;

		WARNING_MSG( "Moo::RenderContext::createDevice: Unable to create device, retrying (%d retries left)\n", retries );
		// Wait a couple of seconds before reattempting, to give a chance to
		// other D3D apps to free there resources and/or quit properly
		Sleep( 2000 );
	}

	if ( !SUCCEEDED( hr ) )
	{
		printErrorMsg_( hr, "Moo::RenderContext::createDevice: Unable to create device" );
		return false;
	}

	// succeeded
	vsVersion_ = uint16( devices_[ deviceIndex_ ].caps_.VertexShaderVersion );
	psVersion_ = uint16( devices_[ deviceIndex_ ].caps_.PixelShaderVersion );
	maxSimTextures_ = uint16( devices_[ deviceIndex_ ].caps_.MaxSimultaneousTextures );
	beginSceneCount_ = 0;

	device_.pComObject( device );
	device->Release();
	setGammaCorrection();
	UINT availTexMem = device_->GetAvailableTextureMem();
	INFO_MSG( "RenderContext::createDevice - available texture memory after create: %d\n", availTexMem );

	updateDeviceInfo();
	updateProjectionMatrix();
	initRenderStates();
	if (mixedVertexProcessing_)
		device_->SetSoftwareVertexProcessing( TRUE );

	s_OriginalMouseCursor = SetCursor( NULL );
	ShowCursor( FALSE );

	// init managers if they have not been inited.
	if (!EffectManager::instance())
		EffectManager::init();
	if (!TextureManager::instance())
		TextureManager::init();
	if (!PrimitiveManager::instance())
		PrimitiveManager::init();
	if (!VerticesManager::instance())
		VerticesManager::init();
	if (!VisualManager::instance())
		VisualManager::init();

	DeviceCallback::createAllManaged();
	DeviceCallback::createAllUnmanaged();

    // Set the affinity of the app.  There were some problems previously with
    // multi-core systems.
	updateProcessorAffinity();

	MF_WATCH(	"Render/backBufferWidthOverride", 
				Moo::RenderContext::instance(), 
				MF_ACCESSORS( uint32, RenderContext, backBufferWidthOverride ),
				"Resize the back buffer. Only works in windowed mode. 0 matches window size. Aspect ratio is maintained." );

	return true;
}

/**
 *	This method initialises all the render states and TS states in the cache.
 */
void RenderContext::initRenderStates()
{
	int i,j;

	for( i = 0; i < 64; i++ )
	{
		for( j = 0; j < D3DTSS_MAX; j++ )
		{
			tssCache_[i][j].stage = i;
			tssCache_[i][j].type = j;
			tssCache_[i][j].currentValue = 0xff81ff81;
		}
		for( j = 0; j < D3DSAMP_MAX; j++ )
		{
			sampCache_[i][j].stage = i;
			sampCache_[i][j].type = j;
			sampCache_[i][j].currentValue = 0xff81ff81;
		}
		textureCache_[i].stage = i;
		textureCache_[i].pCurrentTexture = (DX::BaseTexture*)0xff81ff81;
	}

	for( i = 0; i < D3DRS_MAX; i++ )
	{
		rsCache_[i].state = (D3DRENDERSTATETYPE)i;
		rsCache_[i].currentValue = 0xff81ff81;
	}

    fvf_ = 0xff81ff81;
	vertexDeclaration_ = NULL;
}


/**
 *	This method updates the viewprojection and the inverse view matrix,
 *	should be called whenever the view transform or the projection transform
 *	gets changed.
 */
void RenderContext::updateViewTransforms()
{
	viewProjection_.multiply( view_, projection_ );
	invView_.invert( view_ );
}


void RenderContext::screenWidth( int width )
{
	halfScreenWidth_ = float(width) * 0.5f;
}

void RenderContext::screenHeight( int height )
{
	halfScreenHeight_ = float(height) * 0.5f;
}

/**
 * This function overrides the dimensions of the back buffer when in windowed mode 
 * using a given width.
 * 
 * Given values are first clamped to the maximum width supported by the device. 
 * The height is set so as to maintain the aspect ratio of the front buffer. 
 * A value of 0 means that this feature is disabled and the back buffer will 
 * have the same dimensions as the front.
 * This functionality can be used to produce screenshots at resolutions 
 * greater than that that can be displayed on a monitor.
 * 
 * @param backBufferWidthOverride is the width of the new back buffer.
 */
void RenderContext::backBufferWidthOverride( uint32 backBufferWidthOverride )
{
	uint32 maxWidth = this->deviceInfo(0).caps_.MaxTextureWidth;

	backBufferWidthOverride = std::min( maxWidth, backBufferWidthOverride );

	if( this->windowed() == false )
		backBufferWidthOverride = 0;

    if( backBufferWidthOverride != this->backBufferWidthOverride_ )
		this->changeMode(this->modeIndex(), this->windowed(), true, backBufferWidthOverride );
}

uint32 RenderContext::backBufferWidthOverride() const
{
	return backBufferWidthOverride_;
}


/**
 *	This method increases the current frame counter.
 */
void RenderContext::nextFrame( )
{
	DynamicVertexBufferBase::resetLockAll();
	DynamicIndexBuffer32::instance().resetLock();
	DynamicIndexBuffer16::instance().resetLock();
	DynamicSoftwareIndexBuffer32::instance().resetLock();
	DynamicSoftwareIndexBuffer16::instance().resetLock();
//	ZeroMemory( primitiveHistogram_, sizeof( primitiveHistogram_ ) );
	currentFrame_++;
}

/**
 * Takes a screenshot
 *
 * @param fileName - the name of the file to save out, if empty saves out sequential shotxxxx.bmp files.
 */
void RenderContext::screenShot( const std::string& fileName )
{
	static uint32 sequence = 0;

	std::string name = fileName;

	// if the name is 0 length we will create a sequential name for it.
	if( name.length() == 0 )
	{
		// go through filenames until we find one that has not been created yet.
		bool foundEmptyFile = false;
		while( !foundEmptyFile )
		{
			char findName[128];

			// Create the filename
			sprintf( findName, "shot%04d.bmp", sequence );
			FILE* f = fopen( findName, "rb" );

			// is there such a file?
			if( !f )
			{
				// nope, we have a winner.
				foundEmptyFile = true;
				name = findName;
			}
			else
			{
				// try the next file.
				sequence++;
				fclose( f );
			}
		}
	}

	ComObjectWrap<DX::Surface> backBuffer;

	if( SUCCEEDED( Moo::rc().device()->GetBackBuffer( 0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer ) ) )
	{
		if (SUCCEEDED( D3DXSaveSurfaceToFile( name.c_str(), D3DXIFF_BMP, backBuffer.pComObject(), NULL, NULL) ) )
		{
			INFO_MSG( "Moo::RenderContext::screenShot - saved image %s\n", name.c_str() );
		}
		else
		{
			D3DSURFACE_DESC backBufferDesc;
			IDirect3DSurface9* systemMemorySurface;

			backBuffer->GetDesc( &backBufferDesc );

			if( SUCCEEDED( Moo::rc().device()->CreateOffscreenPlainSurface(	backBufferDesc.Width,
																			backBufferDesc.Height,
																			backBufferDesc.Format,
																			D3DPOOL_SYSTEMMEM,
																			&systemMemorySurface,
																			NULL ) ) )
			{
				if( SUCCEEDED( Moo::rc().device()->GetRenderTargetData( backBuffer.pComObject(), systemMemorySurface ) ) )
				{
					if( SUCCEEDED( D3DXSaveSurfaceToFile( name.c_str(), D3DXIFF_BMP, systemMemorySurface, NULL, NULL) ) )
					{
						INFO_MSG( "Moo::RenderContext::screenShot - saved image %s\n", name.c_str() );
					}
				}

				systemMemorySurface->Release();
			}
		}
	}
	else
	{
		ERROR_MSG( "Moo::RenderContext::screenShot - unable to get backbuffer surface\n" );
	}
}

DX::Surface* RenderContext::getScreenCopy()
{
	DX::Surface* ret = NULL;
	if (screenCopySurface_.hasComObject())
	{
		ComObjectWrap<DX::Surface> backBuffer;
		if( SUCCEEDED( device_->GetBackBuffer( 0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer ) ) )
		{
			if (SUCCEEDED( D3DXLoadSurfaceFromSurface( screenCopySurface_.pComObject(), NULL, NULL,
				backBuffer.pComObject(), NULL, NULL, D3DX_FILTER_NONE, 0 ) ) )
			{
				ret = screenCopySurface_.pComObject();
			}
		}
		else
		{
			ERROR_MSG( "RenderContext::getScreenCopy - unable to get backbuffer\n" );
		}
	}
	else
	{
		ERROR_MSG( "RenderContext::getScreenCopy - no screencopy surface available\n" );
	}
	return ret;
}


RenderContext::~RenderContext()
{
	// Destroy managers if they have not been destroyed.
	StateRecorder::clear();
	if (VisualManager::instance())
		VisualManager::fini();
	if (EffectManager::instance())
		EffectManager::fini();
	if (TextureManager::instance())
		TextureManager::fini();
	if (PrimitiveManager::instance())
		PrimitiveManager::fini();
	if (VerticesManager::instance())
		VerticesManager::fini();
}


/*static*/ RenderContext & RenderContext::instance()
{
	static Moo::RenderContext s_instance;
	return s_instance;
}


bool RenderContext::supportsTextureFormat( D3DFORMAT fmt ) const
{
	if (device_.hasComObject())
	{
		if (SUCCEEDED( d3d_->CheckDeviceFormat( devices_[ deviceIndex_ ].adapterID_,
					D3DDEVTYPE_HAL,
					backBufferDesc_.Format,
					0,
					D3DRTYPE_TEXTURE,
					fmt) ) )
			return true;
	}

	return false;
}

std::ostream& operator<<(std::ostream& o, const RenderContext& t)
{
	o << "RenderContext\n";
	return o;
}

/**
 *	This method pushes a render target from the supplied RenderContext
 *	onto the stack.
 */
bool RenderContext::RenderTargetStack::push( RenderContext* rc )
{
	MF_ASSERT( rc );

	if (!rc)
		return false;

	DX::Device* device = rc->device();
	MF_ASSERT( device );

	if (!device)
		return false;

	StackItem si;
	si.cam_ = rc->camera();
	si.projection_ = rc->projection();
	si.view_ = rc->view();
	DX::Surface* ts;

	HRESULT hr = device->GetRenderTarget( 0, &ts );
	if( FAILED( hr ) )
	{
		CRITICAL_MSG( "Moo::RenderContext::RenderTargetStack::push: couldn't get current render target\n" );
		return false;
	}
	si.renderSurface_ = ts;
	ts->Release();

	hr = device->GetDepthStencilSurface( &ts );
	if( FAILED( hr ) )
	{
		CRITICAL_MSG( "Moo::RenderContext::RenderTargetStack::push: couldn't get current depth/stencil buffer\n" );
		return false;
	}
	si.zbufferSurface_ = ts;
	ts->Release();

	hr = device->GetViewport( &si.viewport_ );
	if( FAILED( hr ) )
	{
		CRITICAL_MSG( "Moo::RenderContext::RenderTargetStack::push: couldn't get current viewport\n" );
		return false;
	}

	stackItems_.push_back( si );
	return true;
}

/**
 *	This method pops a render target into the supplied render context.
 */
bool RenderContext::RenderTargetStack::pop( RenderContext* rc )
{
	MF_ASSERT( rc );

	MF_ASSERT( stackItems_.size() );
	if (!stackItems_.size())
	{
		CRITICAL_MSG( "Tried to pop the render target when no render target was there.\n" );
		return false;
	}

	DX::Device* device = rc->device();
	MF_ASSERT( device );
	if (!device)
		return false;

	StackItem si = stackItems_.back();
	stackItems_.pop_back();

	HRESULT hr = device->SetRenderTarget( 0, &*si.renderSurface_ );
	HRESULT hr2 = device->SetDepthStencilSurface( &*si.zbufferSurface_ );
	if( FAILED( hr ) || FAILED( hr2 ) )
	{
		CRITICAL_MSG( "Moo::RenderContext::RenderTargetStack::pop: couldn't set rendertarget/depth buffer\n" );
		return false;
	}
	hr = device->SetViewport( &si.viewport_ );
	if( FAILED( hr ) )
	{
		CRITICAL_MSG( "Moo::RenderContext::RenderTargetStack::pop: couldn't set viewport\n" );
		return false;
	}

	rc->camera( si.cam_ );
	rc->view( si.view_ );
	rc->projection( si.projection_ );
	rc->updateViewTransforms();

	rc->screenWidth( si.viewport_.Width );
	rc->screenHeight( si.viewport_.Height );

	return true;
}


/**
 *	This method returns the rectangle that is the intersection of the near-plane
 *	with the view frustum.
 *
 *	@param corner		The position of the bottom-left corner of the rectangle.
 *	@param xAxis		The length of the horizontal edges.
 *	@param yAxis		The length of the vertical edges.
 *
 *	@note	The invView matrix must be correct before this method is called. You
 *			may need to call updateViewTransforms.
 *
 *	@see invView
 *	@see updateViewTransforms
 */
void RenderContext::getNearPlaneRect( Vector3 & corner,
		Vector3 & xAxis, Vector3 & yAxis ) const
{
	const Matrix & matrix = this->invView();

	// zAxis is the vector from the camera position to the centre of the
	// near plane of the camera.
	Vector3 zAxis = matrix.applyToUnitAxisVector( Z_AXIS );
	zAxis.normalise();

	// xAxis is the vector from the centre of the near plane to its right edge.
	xAxis = matrix.applyToUnitAxisVector( X_AXIS );
	xAxis.normalise();

	// yAxis is the vector from the centre of the near plane to its top edge.
	yAxis = matrix.applyToUnitAxisVector( Y_AXIS );
	yAxis.normalise();

	const float fov = camera_.fov();
	const float nearPlane = camera_.nearPlane();
	const float aspectRatio = camera_.aspectRatio();

	const float yLength = nearPlane * tanf( fov / 2.0f );
	const float xLength = yLength * aspectRatio;

	xAxis *= xLength;
	yAxis *= yLength;
	zAxis *= nearPlane;

	Vector3 nearPlaneCentre( matrix.applyToOrigin() + zAxis );
	corner = nearPlaneCentre - xAxis - yAxis;
	xAxis *= 2.f;
	yAxis *= 2.f;
}


/**
 *	This function is used on the xbox only, and is stubbed here.
 */
void RenderContext::resetDevice( const D3DPRESENT_PARAMETERS &d3dpp )
{
	//stub.
}


/**
 *	Restores the windows cursor state, hidding or showing it depending on the 
 *	value passed.
 *
 *	@param	state	True will show the windows cursor. False will hide it.
 */
void RenderContext::restoreCursor( bool state )
{
	if (state)
	{
		while (!(::ShowCursor(true) >= 0));
		if (s_OriginalMouseCursor)
			SetCursor( s_OriginalMouseCursor );
	}
	else
	{
		while (!(::ShowCursor(false) < 0));
		if (device_.hasComObject())
			SetCursor( NULL );
	}
}



/**
 *	Frees DX resources temporarily.
 *	
 *	@see resume
 *	@see paused
 */
void RenderContext::pause()
{
	if ( Moo::rc().device()->TestCooperativeLevel() == D3D_OK )
	{
		if ( !paused_ )
			DeviceCallback::deleteAllUnmanaged();
		device_->EvictManagedResources();
	}
	paused_ = true;
}


/**
 *	Reallocates DX resources freed in pause.
 *	
 *	@see pause
 *	@see paused
 */
void RenderContext::resume()
{
	if ( !paused_ )
		return;

    bool reset = false;
	if ( checkDevice(&reset) )
	{
        if (!reset)
		    DeviceCallback::createAllUnmanaged();
		paused_ = false;
	}
}


/**
 *	Checks if the device is paused.
 *	
 *	@return true if the device is paused, false otherwise
 *	
 *	@see pause
 *	@see resume
 */
bool RenderContext::paused() const
{
	return paused_;
}

/**
 *	Wrapper for D3D BeginScene call, this reference counts the Begin/End
 *	Scene pairs, so that we can nest calls to begin and end scene.
 *	@return D3D_OK if successful d3d error code otherwise
 */
HRESULT RenderContext::beginScene()
{
	HRESULT hr = D3D_OK;
	if (beginSceneCount_ == 0)
	{
		hr = device_->BeginScene();
	}

	++beginSceneCount_;
	return hr;
}

/**
 *	Wrapper for D3D endScene call, this reference counts the Begin/End
 *	Scene pairs, so that we can nest calls to begin and end scene.
 *	@return D3D_OK if successful D3DERR_INVALIDCALL otherwise
 */
HRESULT	RenderContext::endScene()
{
	HRESULT hr = D3D_OK;
	--beginSceneCount_;
	if (beginSceneCount_ == 0)
	{
		hr = device_->EndScene();
	}
	else if (beginSceneCount_ < 0)
	{
		hr = D3DERR_INVALIDCALL;
		beginSceneCount_ = 0;
	}
	return hr;
}

void RenderContext::releaseQueryObjects(void)
{
	int nQueries = queryList_.size();
	for (int i = 0; i < nQueries; i++)
	{
		OcclusionQuery* query = queryList_[i];
		if (query && query->queryObject)
		{
			IDirect3DQuery9* queryObject = query->queryObject;			
			queryObject->Release();
			query->queryObject = NULL;
		}
	}
}

/*-------------------------------------------------------------------*//*!
 * \brief	
 *
 *//*-------------------------------------------------------------------*/

void RenderContext::createQueryObjects(void)
{
	int nQueries = queryList_.size();
	for (int i = 0; i < nQueries; i++)
	{
		OcclusionQuery* query = queryList_[i];

		IDirect3DQuery9* queryObject = NULL;
		device_->CreateQuery( D3DQUERYTYPE_OCCLUSION, &queryObject );
		query->queryObject = queryObject;
	}
}

/**
 * 
 */
OcclusionQuery* RenderContext::createOcclusionQuery()
{
	IDirect3DQuery9* pOcclusionQuery = NULL;
	HRESULT res = device_->CreateQuery( D3DQUERYTYPE_OCCLUSION, &pOcclusionQuery );

	if (res != S_OK) // creation failed
		return NULL;

	OcclusionQuery* q = new OcclusionQuery();
	q->queryObject = pOcclusionQuery;
	q->index = queryList_.size();
	queryList_.push_back( q );
	return q;
}

/**
 * 
 */
void RenderContext::destroyOcclusionQuery(OcclusionQuery* query)
{
	// swap query with the last one in the list and delete it
	OcclusionQuery* last = queryList_.back();
	if (query)
	{
		queryList_[query->index] = last;
		last->index = query->index;
		queryList_.pop_back();
		if (query->queryObject)
			query->queryObject->Release();
		delete query;
	}
}

/**
 * 
 */
void RenderContext::beginQuery(OcclusionQuery* query)
{
	query->queryObject->Issue( D3DISSUE_BEGIN );
}

/**
 * 
 */
void RenderContext::endQuery(OcclusionQuery* query)
{
	query->queryObject->Issue( D3DISSUE_END );
}

/**
 * @return true is query result was available, false otherwise
 */
bool RenderContext::getQueryResult(int& visiblePixels, OcclusionQuery* query, bool wait)
{
	IDirect3DQuery9* pOcclusionQuery = query->queryObject;

	if (wait)
	{
		HRESULT hres = S_FALSE;

		while( S_FALSE == (hres = pOcclusionQuery->GetData( &visiblePixels, sizeof(DWORD), D3DGETDATA_FLUSH )) )
		{
		}

		return S_OK == hres;
	}
	else
	{
		return S_OK == pOcclusionQuery->GetData( &visiblePixels, sizeof(DWORD), D3DGETDATA_FLUSH );
	}
}

namespace // anonyous
{

void printErrorMsg_( HRESULT hr, const std::string & msg )
{	
	switch ( hr )
	{
		case D3DERR_DEVICELOST:
			WARNING_MSG( (msg + "The device was lost and cannot be reset at this time.\n").c_str() );
			break;
		case D3DERR_INVALIDCALL:
			WARNING_MSG( (msg + "Invalid call, one or more parameters are wrong.\n").c_str() );
			break;
		case D3DERR_NOTAVAILABLE:
			WARNING_MSG( (msg + "This device does not support the queried technique.\n").c_str() );
			break;
		case D3DERR_OUTOFVIDEOMEMORY:
			WARNING_MSG( (msg + "Out of video memory.\n").c_str() );
			break;
		case D3DERR_DRIVERINTERNALERROR:
			CRITICAL_MSG( (msg + "Driver internal error.\n").c_str() );
			break;
		default:
			WARNING_MSG( (msg + "Unknown DirectX Error #%d\n").c_str(), hr );
	}
}

} // namespace anonyous

}
