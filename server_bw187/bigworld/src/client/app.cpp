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

// Define this to one to draw our watermark (BigWorld Technology logo)
#define DRAW_WATERMARK 1

//#define WANT_DIARIES
#define TIMESTAMP_UNRELIABLE	// for broken laptops

#include "app.hpp"
#include "resource.h"

#include <python.h>
#include <algorithm>

#include "cstdmf/config.hpp"
#include "cstdmf/watcher.hpp"
#include "cstdmf/timestamp.hpp"
#include "cstdmf/profile.hpp"
#include "cstdmf/profiler.hpp"
#include "cstdmf/dogwatch.hpp"
#include "cstdmf/debug.hpp"
#include "cstdmf/main_loop_task.hpp"
#include "cstdmf/bgtask_manager.hpp"
#include "cstdmf/memory_trace.hpp"
#include "cstdmf/base64.h"

#include "resmgr/bwresource.hpp"
#include "resmgr/access_monitor.hpp"
#include "resmgr/auto_config.hpp"
#include "resmgr/win_file_system.hpp"


#include "math/colour.hpp"
#include "math/blend_transform.hpp"
#include "moo/visual_manager.hpp"
#include "moo/texture_manager.hpp"
#include "moo/interpolated_animation_channel.hpp"
#include "moo/animation_manager.hpp"
#include "moo/streamed_animation_channel.hpp"
#include "moo/animating_texture.hpp"
#include "moo/terrain_renderer.hpp"
#include "moo/visual_channels.hpp"
#include "moo/effect_material.hpp"
#include "moo/effect_visual_context.hpp"
#include "moo/visual_compound.hpp"

#include "romp/clouds.hpp"
#include "romp/sky.hpp"
#include "romp/sky_light_map.hpp"
#include "romp/time_of_day.hpp"
#include "romp/sky_gradient_dome.hpp"
#include "romp/sun_and_moon.hpp"
#include "romp/star_dome.hpp"
#include "romp/water.hpp"
#include "romp/fog_controller.hpp"
#include "romp/rain.hpp"
#include "romp/snow.hpp"
#include "romp/weather.hpp"
#include "romp/lens_effect_manager.hpp"
#include "romp/terrain_occluder.hpp"
#include "romp/sea.hpp"
#include "romp/console_manager.hpp"
#include "romp/console.hpp"
#include "romp/engine_statistics.hpp"
#include "romp/geometrics.hpp"
#include "romp/super_model.hpp"
#include "romp/flora.hpp"
#include "romp/gui_progress.hpp"
#include "romp/texture_renderer.hpp"
#include "romp/enviro_minder.hpp"
#include "romp/diary_display.hpp"
#include "romp/full_screen_back_buffer.hpp"
extern void setupTextureFeedPropertyProcessors();

#include "romp/full_screen_back_buffer.hpp"
#include "romp/back_buffer_fx.hpp"
#include "romp/heat_shimmer.hpp"
#include "romp/bloom_effect.hpp"
#include "romp/flash_bang_effect.hpp"
#include "romp/z_buffer_occluder.hpp"
#include "romp/frame_logger.hpp"

#include "particle/py_particle_system.hpp"
#include "particle/py_meta_particle_system.hpp"
#include "particle/actions/particle_system_action.hpp"

#include "shadow_manager.hpp"

class BackBufferEffect;
class HeatShimmer;
class Bloom;
#include "moo/vertex_declaration.hpp"


#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk_obstacle.hpp"
#include "chunk/chunk.hpp"
#include "network/watcher_glue.hpp"

#if UMBRA_ENABLE
#include "chunk/chunk_umbra.hpp"
#endif

#include "cstdmf/memory_stream.hpp"
#include "network/remote_stepper.hpp"
#include "common/servconn.hpp"
#include "pyscript/py_data_section.hpp"
#include "pyscript/py_output_writer.hpp"
#include "input/event_converters.hpp"
#include "input/input_cursor.hpp"
#include "zip/zlib.h"

#include "camera/direction_cursor.hpp"
#include "camera/camera_control.hpp"
#include "camera/free_camera.hpp"
#include "camera/projection_access.hpp"
#include "camera/annal.hpp"

#include "duplo/pymodel.hpp"
#include "duplo/py_splodge.hpp"
#include "duplo/foot_trigger.hpp"

#include "world.hpp"
#include "app_config.hpp"

#include "entity_flare_collider.hpp"
#include "adaptive_lod_controller.hpp"

#include "script_bigworld.hpp"
#include "py_server.hpp"
#include "python_server.hpp"
#include "personality.hpp"
#include "entity.hpp"
#include "entity_type.hpp"
#include "entity_manager.hpp"
#include "player.hpp"
#include "filter.hpp"
#include "entity_picker.hpp"
#include "py_chunk_light.hpp"
#include "py_chunk_spot_light.hpp"
#include "action_matcher.hpp"	// for the 'debug_' static
#include "connection_control.hpp"
#include "client_camera.hpp"
#include "alert_manager.hpp"

#include "voip_app.hpp"
#include "update_handler.hpp"

#include "ashes/simple_gui.hpp"
#include "ashes/text_gui_component.hpp"
#include "ashes/frame_gui_component.hpp"
#include "ashes/clip_gui_shader.hpp"
#include "ashes/alpha_gui_shader.hpp"

#include "compile_time.hpp"
#include "romp/histogram_provider.hpp"

#ifndef TIMESTAMP_UNRELIABLE
#define frameTimerSetup() stampsPerSecond()
#define frameTimerValue() timestamp()
#define frameTimerFreq() stampsPerSecondD()
#define FRAME_TIMER_TYPE uint64
#else
#include <mmsystem.h>
#define frameTimerSetup() timeBeginPeriod(1)
#define frameTimerValue() timeGetTime()
#define frameTimerFreq() 1000.0
#define FRAME_TIMER_TYPE DWORD
#endif


#ifndef CODE_INLINE
#include "app.ipp"
#endif

#include "version_info.hpp"
#include "frame_writer.hpp"

DECLARE_DEBUG_COMPONENT2( "App", 0 );

PROFILER_DECLARE( Sys_Sleep, "Sys Sleep" );

PROFILER_DECLARE( App_Tick, "App Tick" );
PROFILER_DECLARE( App_Draw, "App Draw" );

PROFILER_DECLARE( FinaleApp_present, "FinaleApp Present" );

// -----------------------------------------------------------------------------
// Section: Config string
// -----------------------------------------------------------------------------

#if   defined( _DEBUG )
#define CONFIG_STRING "DEBUG"
#elif defined( _INSTRUMENTED )
#define CONFIG_STRING "INSTRUMENTED"
#elif defined( _HYBRID )
#if defined( _EVALUATION )
#define CONFIG_STRING "EVALUATION"
#else
#define CONFIG_STRING "HYBRID"
#endif
#elif defined(_RELEASE)
#define CONFIG_STRING "RELEASE"
#else
#define CONFIG_STRING "UNKNOWN"
#endif

const char * configString		= CONFIG_STRING;

#define SAFE_DELETE(pointer)\
	if (pointer != NULL)    \
	{                       \
		delete pointer;     \
		pointer = NULL;     \
	}

// -----------------------------------------------------------------------------
// Section: Statics and globals
// -----------------------------------------------------------------------------
App * App::pInstance_ = NULL;

static const float FAR_DISTANCE  = 10000.0f;
//there are 4 stages to progress;
//app startup, shader compilation, preloads + personality script init.
//in C++ the total progress goes to 100%, but the gui progress bar script
//rescales this value for display if it needs some leeway at the end of the
//progress bar for personality script initialisation.
static const float PROGRESS_TOTAL = 3.f;
static const float APP_PROGRESS_STEP = 1.f / 10.f;	//app startup has 10 steps

static DogWatch g_splodgeWatch( "Splodge" );
static DogWatch g_sortedWatch( "DrawSorted" );
static DogWatch g_floraWatch( "Flora" );
static DogWatch g_lensEffectsWatch( "LensEffects" );

bool g_drawWireframe = false;

/// Adjusting this allows run-time adjustment of the discrete level of detail.
float CLODPower = 10.f;

// reference the ChunkInhabitants that we want to be able to load.
extern int ChunkAttachment_token;
extern int ChunkModel_token;
extern int ChunkLight_token;
extern int ChunkTerrain_token;
extern int ChunkFlare_token;
extern int ChunkWater_token;
extern int ChunkSound_token;
extern int ChunkEntity_token;
extern int ChunkPortal_token;
extern int ChunkParticles_token;
extern int ChunkTree_token;
extern int ChunkStationNode_token;
static int s_chunkTokenSet = ChunkModel_token | ChunkLight_token |
	ChunkTerrain_token | ChunkFlare_token | ChunkWater_token |
	ChunkSound_token | ChunkEntity_token | ChunkPortal_token |
	ChunkParticles_token | ChunkTree_token | ChunkStationNode_token |
	ChunkAttachment_token;

extern int PyParticleSystem_token;
extern int PyMetaParticleSystem_token;
static int PS_tokenSet = PyParticleSystem_token | PyMetaParticleSystem_token;

extern int Tracker_token;
extern int PyMorphControl_token;
static int fashionTokenSet = Tracker_token | PyMorphControl_token;

extern int FootTrigger_token;
extern int PySplodge_token;
static int attachmentTokenSet = FootTrigger_token | PySplodge_token;

extern int PyModelObstacle_token;
static int embodimentTokenSet = PyModelObstacle_token;

extern int Decal_token;
extern int PyModelRenderer_token;
extern int PySceneRenderer_token;
extern int TextureFeeds_token;
extern int Oscillator_token;
extern int Servo_token;
extern int PyEntities_token;
extern int Homer_token;
extern int Bouncer_token;
extern int Propellor_token;
extern int LinearHomer_token;
extern int Orbitor_token;
extern int BoxAttachment_token;
extern int SkeletonCollider_token;
extern int PyCommon_token;
extern int PyPatrolPath_token;
#if BIGWORLD_VOIP
extern int PyVOIP_token;
#else
static int PyVOIP_token = 1;
#endif
extern int ServerDiscovery_token;
extern int Pot_token;

static int miscTokenSet = PyModelRenderer_token | PySceneRenderer_token |
	PyEntities_token | Oscillator_token | Homer_token | Bouncer_token |
	Propellor_token | ServerDiscovery_token | Pot_token | TextureFeeds_token |
	Servo_token | LinearHomer_token | Orbitor_token | BoxAttachment_token |
	SkeletonCollider_token | Decal_token | PyCommon_token |
	PyPatrolPath_token | PyVOIP_token;

extern int LatencyGUIComponent_token;
static int guiTokenSet = LatencyGUIComponent_token;

extern int EntityDirProvider_token;
extern int DiffDirProvider_token;
extern int ScanDirProvider_token;
static int dirProvTokenSet = EntityDirProvider_token | DiffDirProvider_token |
	ScanDirProvider_token;

extern int ProfilerApp_token;
static int profilerApp = ProfilerApp_token;

#if BIGWORLD_VOIP
extern int VOIPApp_token;
#else
static int VOIPApp_token = 1;
#endif
static int mainLoopTaskTokenSet =	VOIPApp_token;


static const std::string s_resourcesXML = "resources.xml";

bool gWorldDrawEnabled = true;
const char * const gWorldDrawLoopTasks[] = {	"Canvas",
												"World",
												"Flora",
												"Facade",
												"Lens"		};

#ifdef _DEBUG

extern uint32 heapSize();

static uint32 _totalMemUsedBytes;
static uint32 _totalMemUsed;


#define MEM_SIGNATURE	'mfmm'
/*
void* operator new(size_t amount)
{
	//TRACE_MSG("operator new:\n");

	_totalMemUsedBytes += amount;
	_totalMemUsed = _totalMemUsedBytes / 1024;
	uint32 *mem = (uint32*) malloc(amount + (sizeof(uint32)*2));
	*mem++ = MEM_SIGNATURE;
	*mem++ = amount;
	return mem;
}



void operator delete(void *memory)
{
	//TRACE_MSG("operator delete:\n");

	if (!memory)
		return;

	uint32 *mem = (uint32 *)memory;
	uint32 sz = *--mem;
	uint32 sig = *--mem;

	if (sig != MEM_SIGNATURE) {
		OutputDebugString("delete: bad sig on free **********\n");
		ENTER_DEBUGGER();
	} else {
		_totalMemUsedBytes -= sz;
		_totalMemUsed = _totalMemUsedBytes / 1024;
		free(mem);
	}
}


void* operator new[](size_t amount)
{
	//TRACE_MSG("operator new[]:\n");
	return (operator new)(amount);
}


void operator delete[](void *memory)
{
	//TRACE_MSG("operator delete[]:\n");
	(operator delete)(memory);
}
*/

#endif // _DEBUG

static std::string getBuildRevision();
static AutoConfigString s_engineConfigXML("system/engineConfigXML");
static AutoConfigString s_scriptsConfigXML("system/scriptsConfigXML");
static AutoConfigString loadingScreenName("system/loadingScreen");
static AutoConfigString loadingScreenGUI("system/loadingScreenGUI");
static AutoConfigString s_graphicsSettingsXML("system/graphicsSettingsXML");
static AutoConfigString s_floraXML("environment/floraXML");
static AutoConfigString s_shadowsXML("system/shadowsXML");
static AutoConfigString s_blackTexture("system/blackBmp");
static int  s_framesCounter              = -1;
static bool s_usingDeprecatedBigWorldXML = false;
static bool displayLoadingScreen();
static void freeLoadingScreen();
static void loadingText( const std::string & s );
static DataSectionPtr s_scriptsPreferences = NULL;
static std::string s_configFileName = "";
static std::string s_preferencesFilename = "";
double getGameTotalTime();



static void criticalInitError( const char * format, ... );
static void criticalMessageHandler( const char * msg );
static bool messageTimePrefix( int componentPriority,
	int messagePriority, const char * format, va_list argPtr );



// -----------------------------------------------------------------------------
// Section: Initialisation and Termination
// -----------------------------------------------------------------------------

/**
 *	The constructor for the App class. App::init needs to be called to fully
 *	initialise the application.
 *
 *	@see App::init
 */
App::App( const std::string & configFilename ) :
	hWnd_( NULL ),

	dTime_(0.0f),
	lastTime_(0),
	lastFrameEndTime_(0),
	minFrameTime_(0),
	totalTime_( 0.f ),
	minimumFrameRate_( 8.f ),
#if !ENABLE_DEBUG_KEY_HANDLER
	debugKeyEnable_( false ),
#else
	debugKeyEnable_( true ),
#endif
	activeCursor_( NULL )
{
	frameTimerSetup();
	lastTime_ = frameTimerValue();

	// Make sure that this is the only instance of the app.
	MF_ASSERT( pInstance_ == NULL );
	pInstance_ = this;

	// Clear key routing tables
	ZeroMemory( &keyRouting_, sizeof( keyRouting_ ) );

	// Run things that configure themselves from a config file
	if (!AutoConfig::configureAllFrom( s_resourcesXML ))
	{
		criticalInitError(
			"Could not find resources.xml, which should "
			"contain the location of system resources!" );

		throw InitError( "Could not load resources XML" );
	}

	// Load engine_config.xml
	DataSectionPtr configRoot =
		BWResource::instance().openSection(s_engineConfigXML.value());

	if (AppConfig::instance().init(configRoot))
	{
		s_usingDeprecatedBigWorldXML = false;
		s_configFileName = s_engineConfigXML.value();
	}
	else
	{
		DataResource dataRes;
		if (dataRes.load(configFilename) == DataHandle::DHE_NoError &&
			AppConfig::instance().init(dataRes.getRootSection()))
		{
			s_usingDeprecatedBigWorldXML = true;
			s_configFileName = configFilename;

			WARNING_MSG(
					"Client using deprecated bigworld.xml. Should use"
					"engine_config.xml and scripts_config.xml instead.\n");\
		}
		else
		{
			criticalInitError(
					"Could not load config file: %s!",
					configFilename.c_str() );

			throw InitError( "Could not load config file" );
		}
	}

	DataSectionPtr configSection = AppConfig::instance().pRoot();

	BW_INIT_WATCHER_DOC( "client" );

	lastFrameEndTime_ = timestamp();
	int frameRate = configSection->readInt( "renderer/maxFrameRate", 0 );
	minFrameTime_ = frameRate != 0 ? uint64(frameTimerFreq() / frameRate) : 0;

	s_framesCounter = configSection->readInt( "debug/framesCount", 0 );

	// Initialise Access Monitoring.
	AccessMonitor::instance().active( configSection->readBool(
		"accessMonitor", false ) );
}


/**
 *	Destructor for the App class.
 */
App::~App()
{
	BWResource::watchAccessFromCallingThread(false);
	if (MainLoopTasks::root().initted())
	{
		MainLoopTasks::root().fini();
	}

	pInstance_ = NULL;
}


/*~	function BigWorld.setCursor
 *
 *	Sets the active cursor. The active cursor will get all mouse,
 *	keyboard and axis events forwarded to it. Only one cursor can
 *	be active at a time.
 *
 *	@param	cursor	the new active cursor, or None to simply
 *					deactivate the current one.
 */
PyObject * App::py_setCursor( PyObject * args )
{
	PyObject* pyCursor = NULL;
	if (!PyArg_ParseTuple( args, "O", &pyCursor ))
	{
		PyErr_SetString( PyExc_TypeError, "py_setCursor: Argument parsing error." );
		return NULL;
	}

	// set to null
	InputCursor * cursor = NULL;
	if (pyCursor != Py_None)
	{
		// type check
		if (!InputCursor::Check( pyCursor ))
		{
			PyErr_SetString( PyExc_TypeError, "py_setCursor: Expected a Cursor." );
			return NULL;
		}

		cursor = static_cast< InputCursor * >( pyCursor );
	}

	App::instance().activeCursor( cursor );
	Py_Return;
}
PY_MODULE_STATIC_METHOD( App, setCursor, BigWorld )

extern bool processOutstandingMessages();	// in WinMain
extern bool g_bAppQuit; //winmain
extern void initNetwork();

#ifdef TRACK_MEMORY_BLOCKS

namespace MemoryTrackingSocket
{
	void send( const void * data, uint32 size );

	enum { CALL_STACK_DEPTH = 32 };

	static THREADLOCAL( bool ) handling = false;

	static void sendAllocationMessage( void * addr )
	{
		if (handling) return;
		handling = true;

		uint32 msg[3+CALL_STACK_DEPTH];
		msg[0] = (uint32)addr;
		msg[1] = 0;

		uint32 blocks = allocatedSizeBase( addr ) >> 4;
		if (blocks < 65536)
			msg[2] = blocks<<16;
		else
			msg[2] = (blocks|128)<<8;/* = ((blocks>>8)<<16) | (1<<15);*/


		DmCaptureStackBackTrace( CALL_STACK_DEPTH, (void**)(msg+3) );
		uint32 frames = 0;
		while (frames < CALL_STACK_DEPTH && (msg+3)[frames++] != 0) ; //scan
		msg[2] |= frames;

		MemoryTrackingSocket::send( msg, (3 + frames)*sizeof(uint32) );
		handling = false;
	}

	static void sendDeallocationMessage( void * addr )
	{
		if (handling) return;
		handling = true;

		uint32 msg[3];
		msg[0] = (uint32)addr;
		msg[1] = uint32(-1);
		msg[2] = 0;
		MemoryTrackingSocket::send( msg, sizeof(msg) );

		handling = false;
	}

	typedef uint32 (WINAPI *TellDebuggerFn)(
		unsigned long ulCode, void * pvData );
	static TellDebuggerFn s_TellDebuggerDefault = NULL;

	struct DebuggerIF
	{
		uint32			pSomething0_;
		uint32			pSomething1_;
		uint32			filler_;
		uint32			pSomething3_;
		uint32			pSomething4_;
		TellDebuggerFn	pTellDebugger_;
	};

	enum DebugMsgCodes
	{
		DMSG_ALLOC = 0x0A,
		DMSG_FREE = 0x0B
	};

	/**
	*	This function intercepts a call to the debugger from the runtime
	*	environment. We are interested in memory alloction messages.
	*/
	static uint32 WINAPI tellDebuggerInterloper(
		unsigned long ulCode, void * pvData )
	{
		if (ulCode == DMSG_ALLOC)
		{
			uint32 addr = ((uint32*)pvData)[0];
			uint32 size = ((uint32*)pvData)[1];
			//dprintf( "Allocated %d at 0x%08X\n", size, addr );

			sendAllocationMessage( (void*)addr );
		}
		else if (ulCode == DMSG_FREE)
		{
			uint32 addr = ((uint32*)pvData)[0];
			//dprintf( "Freed 0x%08X\n", addr );

			sendDeallocationMessage( (void*)addr );
		}
		else
		{
			//dprintf( "Telling debugger %d 0x%08X\n", ulCode, pvData );
		}
		return (*s_TellDebuggerDefault)( ulCode, pvData );
	}


	static DebuggerIF * get_g_dmi()
	{
		__asm
		{
			mov eax, DWORD PTR fs:0x20
			mov eax, DWORD PTR 0x250[eax]
		}
	}


	extern "C"
	{
		extern __declspec(dllimport) HRESULT __stdcall
			MmAllocateContiguousMemory( DWORD NumberOfBytes );
		extern __declspec(dllimport) HRESULT __stdcall
			MmAllocateContiguousMemoryEx(
				DWORD NumberOfBytes, DWORD LowestAcceptableAddress,
				DWORD HighestAcceptableAddress, DWORD Alignment, DWORD Protect );
		extern __declspec(dllimport) HRESULT __stdcall
			MmFreeContiguousMemory( PVOID BaseAddress );
	}

	const uint8 & irqLevel = *(uint8*)0x8004C5D0;

	static HRESULT (__stdcall *s_AllocateContiguousMemoryDefault)( DWORD NumberOfBytes );
	static HRESULT __stdcall AllocateContiguousMemoryInterloper( DWORD NumberOfBytes )
	{
		HRESULT res = (*s_AllocateContiguousMemoryDefault)( NumberOfBytes );
		if (irqLevel == 0)
			sendAllocationMessage( (void*)res );
		return res;
	}

	static HRESULT (__stdcall *s_AllocateContiguousMemoryExDefault)(
		DWORD, DWORD, DWORD, DWORD, DWORD );
	static HRESULT __stdcall AllocateContiguousMemoryExInterloper(
		DWORD NumberOfBytes, DWORD LowestAcceptableAddress,
		DWORD HighestAcceptableAddress, DWORD Alignment, DWORD Protect )
	{
		HRESULT res = (*s_AllocateContiguousMemoryExDefault)(
			NumberOfBytes, LowestAcceptableAddress,
			HighestAcceptableAddress, Alignment, Protect );
		if (irqLevel == 0)
			sendAllocationMessage( (void*)res );
		return res;
	}

	static HRESULT (__stdcall *s_FreeContiguousMemoryDefault)( PVOID BaseAddress );
	static HRESULT __stdcall FreeContiguousMemoryInterloper( PVOID BaseAddress )
	{
		if (irqLevel == 0)
			sendDeallocationMessage( BaseAddress );
		HRESULT res = (*s_FreeContiguousMemoryDefault)( BaseAddress );
		return res;
	}

#define GET_SYMBOL_ADDRESS( ADDRVAR, SYMBOL )	\
		__asm { lea eax, SYMBOL };				\
		void ** ADDRVAR = emptyFunc()			\

	static inline void ** emptyFunc() { __asm mov eax, eax };

	static void setValBypass( void *& loc, void * val )
	{
		MEMORY_BASIC_INFORMATION curProt;
		VirtualQuery( &loc, &curProt, sizeof(curProt) );
		if (!(curProt.Protect & PAGE_READWRITE))
		{
			DWORD newProt = PAGE_READWRITE;
			DWORD oldProt = 0;
			VirtualProtect( &loc, sizeof(void*), newProt, &oldProt );
		}

		loc = val;	// all that work for this!

		if (!(curProt.Protect & PAGE_READWRITE))
		{
			DWORD newProt = curProt.Protect;
			DWORD oldProt = 0;
			VirtualProtect( &loc, sizeof(void*), newProt, &oldProt );
		}
	}

	static SimpleMutex * oneAnything = NULL;
	static Endpoint * s_socket = NULL;

	void init()
	{
		MF_ASSERT( s_socket == NULL );

		if (oneAnything == NULL)	// allocate before catching allocs!
			oneAnything = new SimpleMutex();

		Endpoint listener;
		listener.socket( SOCK_STREAM );
		listener.bind( htons(37645) );
		listener.listen( 1 );

		INFO_MSG( "MemoryTrackingSocket::init(): "
			"Waiting for memory tracking connection on port %d\n", 37645 );

		s_socket = listener.accept();

		INFO_MSG( "MemoryTrackingSocket::init(): "
			"Accepted memory tracking connection\n" );

		// reroute debugger messages to our function
		DebuggerIF * g_dmi = get_g_dmi();
		s_TellDebuggerDefault = g_dmi->pTellDebugger_;
		g_dmi->pTellDebugger_ = tellDebuggerInterloper;

		// change the address of contiguous memory allocations and
		// deallocations to go through our functions too
		s_AllocateContiguousMemoryDefault = MmAllocateContiguousMemory;
		GET_SYMBOL_ADDRESS( pMmACM, MmAllocateContiguousMemory );
		setValBypass( *pMmACM, (void*)AllocateContiguousMemoryInterloper );
		s_AllocateContiguousMemoryExDefault = MmAllocateContiguousMemoryEx;
		GET_SYMBOL_ADDRESS( pMmACMEx, MmAllocateContiguousMemoryEx );
		setValBypass( *pMmACMEx, (void*)AllocateContiguousMemoryExInterloper );
		s_FreeContiguousMemoryDefault = MmFreeContiguousMemory;
		GET_SYMBOL_ADDRESS( pMmFCM, MmFreeContiguousMemory );
		setValBypass( *pMmFCM, (void*)FreeContiguousMemoryInterloper );
	}

	struct AtomicInt
	{
		inline int __fastcall inc()
		{
			__asm mov eax, 1
			__asm lock xadd [ecx], eax
		}

		inline int __fastcall add( int amt )
		{
			__asm mov eax, edx
			__asm lock xadd [ecx], eax
		}

		int val_;
	};

	void send( const void * data, uint32 size )
	{
		// static initialisers can register allocations with their static
		// memory counter before it has been constructed
		if (s_socket == NULL) return;

		oneAnything->grab();

		static AtomicInt nsent = { 0 };
		static bool waitingAck = false;
		static volatile int nkilos = 0;

		// send this message
		s_socket->send( data, size );

		// see if we've got an ack back
		if (waitingAck)
		{
			// TODO: fix race condition here if we remove oneAnything mutex
			bool shouldBlock = nsent.val_ > 512;
			if (!shouldBlock)
				s_socket->setnonblocking( true );

			int ack = -1;
			int myks = nkilos;
			if (s_socket->recv( &ack, 4 ) == 4 || shouldBlock)
			{
				if (ack != myks)
				{
					ERROR_MSG( "MTS: Got wrong ack: %d instead of %d\n",
						ack, myks );
					// but there's not much we can do about it...
					delete s_socket;
					s_socket = NULL;
				}
				else
				{
					nkilos++;
					waitingAck = false;
				}
			}

			if (!shouldBlock && s_socket != NULL)
				s_socket->setnonblocking( false );
		}

		// see if we expect another ack
		if (nsent.inc() == 1024-1)
		{
			nsent.add( -1024 );
			waitingAck = true;

			//static SimpleMutex oneReceiver;
/*
			int myks = nkilos++;
			int ack = -1;

			//INFO_MSG( "MTS: Waiting for ack for %d kilos\n", myks );
			//oneReceiver.grab();
			if (s_socket->recv( &ack, 4 ) != 4 || ack != myks)
			{
				ERROR_MSG( "MTS: Got wrong ack: %d instead of %d\n",
					ack, myks );
				// but there's not much we can do about it...
				delete s_socket;
				s_socket = NULL;
			}
			else
			{
				//INFO_MSG( "MTS: Got ack\n" );
			}
*/
			//oneReceiver.give();
		}
		oneAnything->give();
	}
};
#endif


uint32 memTotal();
uint32 memUsed();
uint32 memLeft();

uint32 memoryAccountedFor();
int32 memoryUnclaimed();




static DogWatch	g_watchTick("Tick");
static DogWatch	g_watchInput("Input");
static DogWatch	g_watchUpdate("Update");
static DogWatch	g_watchOutput("Output");

typedef SmartPointer<PyModel> PyModelPtr;

/**
 *	Device task
 */
class DeviceApp : public MainLoopTask
{
public:
	DeviceApp() :
		dTime_( 0.f ),
		soundEnabled_( true )
	{ MainLoopTasks::root().add( this, "Device/App", NULL ); }

	DeviceApp::~DeviceApp()
	{
		/*MainLoopTasks::root().del( this, "Device/App" );*/
	}

	virtual bool init();
	virtual void fini()
	{
		ShadowManager::instance().fini();
		Moo::VertexDeclaration::fini();

		SAFE_DELETE(DeviceApp::s_pStartupProgTask_);
		SAFE_DELETE(DeviceApp::s_pProgress_);

		// release the render context

		// has to be done here and not in device, as this may free up
		// some pythonised stuff
		Moo::rc().releaseDevice();
	}

	void deleteGUI()
	{
		SAFE_DELETE(DeviceApp::s_pProgress_);
	}


	bool savePreferences();
	virtual void tick( float dTime );
	virtual void inactiveTick( float dTime );
	virtual void draw();

private:
	float	dTime_;
	Vector3	bgColour_;
	bool	soundEnabled_;

public:
	static DeviceApp	instance;
	static HINSTANCE	s_hInstance_;
	static HWND			s_hWnd_;
	static ProgressDisplay	* s_pProgress_;
	static GUIProgressDisplay * s_pGUIProgress_;
	static ProgressTask		* s_pStartupProgTask_;
	static std::vector< PyModelPtr > updateModels_;
};

DeviceApp DeviceApp::instance;

HINSTANCE DeviceApp::s_hInstance_ = NULL;
HWND DeviceApp::s_hWnd_ = NULL;
ProgressDisplay * DeviceApp::s_pProgress_ = NULL;
GUIProgressDisplay * DeviceApp::s_pGUIProgress_ = NULL;
ProgressTask * DeviceApp::s_pStartupProgTask_ = NULL;

std::vector< PyModelPtr > DeviceApp::updateModels_;


/**
 *	This functor is called when any message is printed
 */

struct MessageTimePrefix : public DebugMessageCallback
{
	bool handleMessage( int componentPriority,
		int messagePriority, const char * format, va_list /*argPtr*/ )
	{
		static THREADLOCAL( bool ) newLine = true;

		if (DebugFilter::shouldAccept( componentPriority, messagePriority ))
		{
			if (newLine && (&App::instance()) != NULL)
			{
				char buf[ 256 ];
				sprintf( buf, "%.3f ", float(::getGameTotalTime()) );
				OutputDebugString( buf );
			}

			int len = strlen(format);
			newLine = (len && format[len - 1] == '\n');
		}

		return false;
	}

};


/**
 *	Debug task
 */
class DebugApp : public MainLoopTask
{
public:
	DebugApp() :
		pVersionInfo_( NULL ),
		pPyServer_( NULL ),
		dTime_( 0.f ),
		fps_( 0.f ),
		maxFps_( 0.f ),
		minFps_( 0.f ),
		fpsIndex_( 0 ),
		timeSinceFPSUpdate_( 1.f ),
		sleepTime_( 1.f ),
		slowTime_( 1.f ),
		drawSpecialConsole_( false ),
		shouldBreakOnCritical_( true ),
		shouldAddTimePrefix_( false )
	{
		MainLoopTasks::root().add( this, "Debug/App", NULL );
		ZeroMemory( fpsCache_, sizeof( fpsCache_ ) );
	}

	DebugApp::~DebugApp()
	{ /*MainLoopTasks::root().del( this, "Debug/App" );*/ }

	virtual bool init();
	virtual void fini();
	virtual void tick( float dTime );
	virtual void draw();

	static DebugApp instance;

	VersionInfo			* pVersionInfo_;

private:

#if ENABLE_PYTHON_TELNET_SERVICE && !defined(BIGWORLD_CLIENT_ONLY)
	PythonServer		* pPyServer_;
#else
	PyObject			* pPyServer_;
#endif

	std::string			driverName_;
	std::string			driverDesc_;
	std::string			deviceName_;
	std::string			hostName_;

	float				dTime_;
	float				fps_;
	float				maxFps_;
	float				minFps_;
	float				fpsCache_[50];
    int					fpsIndex_;
	float				timeSinceFPSUpdate_;
	float				sleepTime_;

public:
	float				slowTime_;
	bool				drawSpecialConsole_;
	bool				shouldBreakOnCritical_;
	bool				shouldAddTimePrefix_;
};

DebugApp DebugApp::instance;

/**
 *	This functor is called when a critical message is printed.
 */

struct CriticalHandler : public CriticalMessageCallback
{

	void handleCritical( const char * msg )
	{
		if (!DebugApp::instance.shouldBreakOnCritical_) return;

		bool runningFullScreen = !Moo::rc().windowed();
		if (Moo::rc().device() && runningFullScreen)
		{
			Moo::rc().changeMode( Moo::rc().modeIndex(), true );
		}
		ShowCursor( TRUE );

		char buffer[2*BUFSIZ];

		sprintf( buffer,
			"%s\nEnter debugger?\nCancel will exit the program.\n", msg );

		if (DeviceApp::s_pProgress_)
		{
			std::string errorMsg = "Critical Error: ";
			DeviceApp::s_pProgress_->add(errorMsg + buffer);
		}

		switch (::MessageBox( NULL, buffer, "Critical Error", MB_YESNOCANCEL ))
		{
			case IDYES:		ENTER_DEBUGGER();		break;
			case IDNO:								break;
			case IDCANCEL:	exit( EXIT_FAILURE );	break;
		}

		ShowCursor( FALSE );
		if (Moo::rc().device() && runningFullScreen)
		{
			Moo::rc().changeMode( Moo::rc().modeIndex(), false );
		}
	}

};


bool DeviceApp::init()
{
	DEBUG_MSG( "DeviceApp::init: Initially using %d(~%d)KB\n",
		memUsed(), memoryAccountedFor() );

	MEM_TRACE_BEGIN( "DeviceApp::init" )

	DataSectionPtr configSection = AppConfig::instance().pRoot();

	// 1. Input

	MEM_TRACE_BEGIN( "Input" )
	// init input devices
	char deviceInitFlags = 0;
	if (configSection->readBool( "inputDevices/exclusive", true ))
	{
		deviceInitFlags |= InputDevices::EXCLUSIVE_MODE;
	}
	if (!InputDevices::init( s_hInstance_, s_hWnd_, deviceInitFlags ))
	{
		ERROR_MSG( "App::init: Init inputDevices FAILED\n" );
		MEM_TRACE_END()
		MEM_TRACE_END()
		return false;
	}

	InputDevices::joystick().useKeyboard();

	MEM_TRACE_END()


	// 2. Network
	MEM_TRACE_BEGIN( "Network" )
	initNetwork();

	ResUpdateHandler::instance().init();	// add 'dyn' res path

	ConnectionControl::serverConnection()->pTime( &App::instance().totalTime_ );
	ConnectionControl::serverConnection()->initDebugInfo();
	Watcher::rootWatcher().addChild( "Nub",
		Mercury::Nub::pWatcher(), &ConnectionControl::serverConnection()->nub());
	MEM_TRACE_END()

	// 3. Graphics
	MEM_TRACE_BEGIN( "Graphics" )

	// Search suitable video mode
	const Moo::DeviceInfo& di = Moo::rc().deviceInfo( 0 );

	// search for a suitable video mode.
	// This will be used as a fallback.
	const int numTraits = 6;
	const int32 modeTraits[numTraits][4] = {
		{ 1024, 768, D3DFMT_X8R8G8B8, D3DFMT_A8B8G8R8 },
		{ 800,  600, D3DFMT_X8R8G8B8, D3DFMT_A8B8G8R8 },
		{ 640,  480, D3DFMT_X8R8G8B8, D3DFMT_A8B8G8R8 },
		{ 1024, 768, D3DFMT_R5G6B5,   D3DFMT_X1R5G5B5 },
		{ 800,  600, D3DFMT_R5G6B5,   D3DFMT_X1R5G5B5 },
		{ 640,  480, D3DFMT_R5G6B5,   D3DFMT_X1R5G5B5 },
	};

	int modeIndex   = 0;
	int searchIndex = 0;
	const int numDisplay = di.displayModes_.size();
	while(searchIndex < numDisplay * numTraits)
	{
		const int mIndex = searchIndex % numDisplay;
		const int tIndex = (searchIndex / numDisplay) % numTraits;
		if( di.displayModes_[mIndex].Width  == modeTraits[tIndex][0]  &&
			di.displayModes_[mIndex].Height == modeTraits[tIndex][1] && (
			di.displayModes_[mIndex].Format == modeTraits[tIndex][2] ||
			di.displayModes_[mIndex].Format == modeTraits[tIndex][3]))
		{
			modeIndex = mIndex;
			break;
		}
		++searchIndex;
	}

	bool windowed     = true;
	bool waitVSync    = false;
	float aspectRatio = 4.0f/3.0f;
	int windowWidth   = 800;
	int windowHeight  = 600;

	// load graphics preferences
	s_preferencesFilename = configSection->readString( "preferences", "" );
	if ( !s_preferencesFilename.empty() )
	{
		s_preferencesFilename = BWResource::appDirectory() + s_preferencesFilename;
	}

	DataResource dataRes;
	if (dataRes.load(s_preferencesFilename) == DataHandle::DHE_NoError)
	{
		DataSectionPtr graphicsPreferences =
			dataRes.getRootSection()->openSection("graphicsPreferences");

		if (graphicsPreferences.exists())
		{
			Moo::GraphicsSetting::read(graphicsPreferences);
		}

		DataSectionPtr devicePreferences =
			dataRes.getRootSection()->openSection("devicePreferences");

		if (devicePreferences.exists())
		{
			modeIndex    = devicePreferences->readInt("modeIndex", modeIndex);
			windowed     = devicePreferences->readBool("windowed", windowed);
			waitVSync    = devicePreferences->readBool("waitVSync", waitVSync);
			aspectRatio  = devicePreferences->readFloat("aspectRatio", aspectRatio);
			windowWidth  = devicePreferences->readInt("windowWidth", windowWidth);
			windowHeight = devicePreferences->readInt("windowHeight", windowHeight);

			// limit width and height
			windowWidth  = std::min(std::max(windowWidth, 512), 1600);
			windowHeight = std::min(std::max(windowHeight, 384), 1200);
		}

		s_scriptsPreferences =
			dataRes.getRootSection()->openSection("scriptsPreferences", true);
	}

	bgColour_ = Vector3( 160, 180, 250 ) * 0.9f;
	uint32 deviceIndex = 0;


	// Uncomment this block to
	// use NVIDIA NVPerfHUD's device
	/*for (uint32 i = 0; i < Moo::rc().nDevices(); i++)
	{
		if (std::string(Moo::rc().deviceInfo(i).identifier_.Description) ==
			std::string("NVIDIA NVPerfHUD"))
		{
			deviceIndex = i;
		}
	}*/


	App::instance().resizeWindow(windowWidth, windowHeight);
	Moo::rc().fullScreenAspectRatio(aspectRatio);
	Moo::rc().waitForVBL(waitVSync);

	int maxModeIndex = Moo::rc().deviceInfo(deviceIndex).displayModes_.size() - 1;
	modeIndex = std::min(modeIndex, maxModeIndex);
	Moo::rc().createDevice( s_hWnd_, deviceIndex, modeIndex, windowed, true );

	DebugFilter::instance().addCriticalCallback( new CriticalHandler );
	DebugFilter::instance().addMessageCallback( new MessageTimePrefix );

	Moo::VisualChannel::initChannels();

	bool ret = true;

	DataSectionPtr ptr = BWResource::instance().openSection("shaders/formats");
	if (ptr)
	{
		DataSectionIterator it = ptr->begin();
		DataSectionIterator end = ptr->end();
		while (it != end)
		{
			std::string format = (*it)->sectionName();
			uint32 off = format.find_first_of( "." );
			format = format.substr( 0, off );
			Moo::VertexDeclaration::get( format );
			it++;
		}
	}

	// wait for windows to
	// send us a paint message
	uint64	tnow = timestamp();
	while ( (timestamp() - tnow < stampsPerSecond()/2) && ret)
	{
		ret = processOutstandingMessages();
	}

	MEM_TRACE_END()

	// init the texture feed instance, this
	// registers material section processors.
	setupTextureFeedPropertyProcessors();

	// 4. Sound
	MEM_TRACE_BEGIN( "Sound" )
	soundEnabled_ = configSection->readBool( "soundMgr/enabled", soundEnabled_ );
	if (soundEnabled_)
	{
		DataSectionPtr dsp = configSection->openSection( "soundMgr" );

		if (dsp)
		{
			if (!SoundManager::instance().initialise( dsp ))
			{
				ERROR_MSG( "DeviceApp::init: Failed to initialise sound\n" );
			}
		}
		else
		{
			ERROR_MSG( "DeviceApp::init: "
				"No <soundMgr> config section found, sound support is "
				"disabled\n" );
		}

#if 0
		soundMgr().init( s_pProgress_ );
		DataSectionPtr dsp = configSection->openSection("soundMgr");
		if (dsp)
		{
			soundMgr().init(hWnd_,
				dsp->readInt("performanceChannels", 0),
				dsp->readFloat("rolloffFactor", 0),
				dsp->readFloat("dopplerFactor", 0),
				dsp->readInt("maxFxPaths", 0),
				dsp->readInt("maxAmbientPaths", 0),
				dsp->readInt("maxStatic3DPaths", 0),
				dsp->readInt("maxSimplePaths", 0)
			);
		}
		else
		{
			::soundMgr().init(hWnd_);
		}
#endif	// 0
	}
	else
	{
		SoundManager::instance().errorLevel( SoundManager::SILENT );
	}
	MEM_TRACE_END()

	DataSectionPtr spSection = BWResource::instance().openSection( s_shadowsXML );
	ShadowManager::instance().init( spSection );

	MEM_TRACE_END()

	FontManager::instance().preCreateAllFonts();  

	return ret;
}


bool DeviceApp::savePreferences()
{
	bool result = false;
	if (!s_preferencesFilename.empty())
	{
		DataResource dataRes(s_preferencesFilename, RESOURCE_TYPE_XML);
		DataSectionPtr root = dataRes.getRootSection();

		// graphics preferences
		DataSectionPtr grPref = root->openSection("graphicsPreferences", true);
		Moo::GraphicsSetting::write(grPref);

		// device preferences
		DataSectionPtr devPref = root->openSection("devicePreferences", true);
		devPref->delChildren();
		devPref->writeBool( "windowed",    Moo::rc().windowed());
		devPref->writeBool( "waitVSync",   Moo::rc().waitForVBL());
		devPref->writeInt(  "modeIndex",   Moo::rc().modeIndex());
		devPref->writeFloat("aspectRatio", Moo::rc().fullScreenAspectRatio());

		POINT windowSize = App::instance().windowSize();
		devPref->writeInt("windowWidth",  windowSize.x);
		devPref->writeInt("windowHeight", windowSize.y);

		// script preferences
		DataSectionPtr scrptPref = root->openSection("scriptsPreferences", true);
		scrptPref->delChildren();
		scrptPref->copy(s_scriptsPreferences);

		// save it
		if (dataRes.save() == DataHandle::DHE_NoError)
		{
			result = true;
		}
		else
		{
			ERROR_MSG("Could not save preferences file.");
		}
	}
	return result;
}

void DeviceApp::tick( float dTime )
{
	if (s_pProgress_ != NULL)
	{
		delete s_pStartupProgTask_;
		delete s_pProgress_;
		s_pStartupProgTask_ = NULL;
		s_pProgress_ = NULL;
	}

	dTime_ = dTime;

	g_watchInput.start();

	InputDevices::processEvents( App::instance() );
	if (AnnalVault::isPlaying()) App::instance().handleInputPlayback();

	//  make sure we are still connected.
	ConnectionControl::instance().tick();

	EntityManager::instance().gatherInput();
	WatcherGlue::instance().receiveRequest();

	g_watchInput.stop();

	// get the direction cursor to process its input immediately here too
	DirectionCursor::instance().tick( dTime );

	Moo::EffectMaterial::finishEffectInits();

	Moo::rc().beginScene();
}

void DeviceApp::inactiveTick( float dTime )
{
	dTime_ = dTime;

	//  make sure we are still connected.
	ConnectionControl::instance().tick();

	EntityManager::instance().gatherInput();
}

PROFILER_DECLARE( DeviceApp_draw, "DeviceApp Draw" );

void DeviceApp::draw()
{
	PROFILER_SCOPED( DeviceApp_draw );

	// begin rendering
	if (Moo::rc().mixedVertexProcessing())
		Moo::rc().device()->SetSoftwareVertexProcessing( TRUE );

	Moo::rc().device()->Clear( 0, NULL,
		D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
		Colour::getUint32( bgColour_ ), 1, 0 );

	Moo::rc().nextFrame();

	//HACK_MSG( "Heap size %d\n", heapSize() );
	// update any dynamic textures
	Moo::rc().setRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALPHA |
		D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE );
	TextureRenderer::updateDynamics( dTime_ );
	Moo::rc().setRenderState( D3DRS_COLORWRITEENABLE,
		D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE );

    // ### FMOD
    Vector3 cameraPosition = ClientCamera::instance().camera()->invView().applyToOrigin();
    Vector3 cameraDirection = ClientCamera::instance().camera()->invView().applyToUnitAxisVector( 2 );
    Vector3 cameraUp = ClientCamera::instance().camera()->invView().applyToUnitAxisVector( 1 );
    SoundManager::instance().setListenerPosition( cameraPosition, cameraDirection, cameraUp, dTime_ );

	// 'draw' sounds, i.e. commit our position
	ChunkSpacePtr pSpace = ChunkManager::instance().cameraSpace();
	if (pSpace)
	{
		EnviroMinder & enviro = pSpace->enviro();

//		g_watchSound.start();
		if( Player::entity() )
		{
			PyModel* pModel = Player::entity()->pPrimaryModel();

			Vector3 bbDiag;
			float minDim;
			bool inShell = false;//pModel != NULL && !pModel->isOutside();

/*			if (inShell) {
				if (pModel->chunk() != NULL)
				{
					bbDiag = pModel->chunk()->boundingBox().maxBounds() -
							pModel->chunk()->boundingBox().minBounds();
				}
				minDim = min(fabsf(bbDiag.x), fabsf(bbDiag.z));
			} else*/
				minDim = 0;
#if 0
			soundMgr().setListenerPos(
				Moo::rc().invView(),
				Vector3(Player::entity()->position()),
				inShell,
				minDim,
				ChunkManager::instance().cameraSpace()->enviro().
					timeOfDay()->gameTime(),
				float()//this->totalTime_)
			);
			soundMgr().doWork();
#endif
			// TODO: Re-enable this sound with the new FMOD API
//			BWSound::playRain( enviro.rain()->amount(), dTime_ );
		}

//		g_watchSound.stop();
	} // end of 'if (pSpace)'

	ShadowManager::instance().captureShadows();

	// This is some slightly dodgy code to get around problems caused by PyModelNode
	// only being updated when an object is rendered.
	std::vector< PyModelPtr >::iterator it = DeviceApp::updateModels_.begin();
	while (it != DeviceApp::updateModels_.end())
	{
		PyModelPtr pModel = *it;
		if (pModel)
		{
			BoundingBox bb = pModel->boundingBox();
			Matrix cullTransform = Moo::rc().viewProjection();
			cullTransform.preMultiply( pModel->worldTransform() );
			bb.calculateOutcode( cullTransform );
			if (bb.combinedOutcode())
			{
				pModel->draw( pModel->worldTransform(), 0 );
			}
		}
		++it;
	}

}

/**
 *	Script task
 */
class ScriptApp : public MainLoopTask
{
	ScriptApp()
	{ MainLoopTasks::root().add( this, "Script/App", NULL ); }

	ScriptApp::~ScriptApp()
	{ /*MainLoopTasks::root().del( this, "Script/App" );*/ }

	virtual bool init();
	virtual void fini();
	virtual void tick( float dTime );
	virtual void inactiveTick( float dTime );
	virtual void draw() { }

private:
	static ScriptApp instance;
};
ScriptApp ScriptApp::instance;

bool ScriptApp::init()
{
	DEBUG_MSG( "ScriptApp::init: Initially using %d(~%d)KB\n",
		memUsed(), memoryAccountedFor() );
	MEM_TRACE_BEGIN( "ScriptApp::init" )

	if (!BigWorldClientScript::init())
	{
		criticalInitError( "App::init: BigWorldClientScript::init() failed!" );
		MEM_TRACE_END()

		return false;
	}


#if ENABLE_WATCHERS

	MF_WATCH( "Active Entities",	EntityManager::instance().entities(),
		static_cast<Entities::size_type (Entities::*)() const>( &Entities::size ),
		"Shows the number of enitties in the players's AoI" );

	MapWatcher<Entities> * pESeq = new MapWatcher<Entities>(
			EntityManager::instance().entities() );
	pESeq->addChild( "*", new BaseDereferenceWatcher(
		&Entity::watcher() ) );
	Watcher::rootWatcher().addChild( "Entities", pESeq );

	MF_WATCH( "Client Settings/Entity Collisions", ActionMatcher::globalEntityCollision_,
		Watcher::WT_READ_WRITE, "Enables entity - entity collision testing" );

	MF_WATCH( "Client Settings/Debug Action Matcher", ActionMatcher::debug_, Watcher::WT_READ_WRITE,
				"if this is set to true, action matcher will run in debug mode "
				"that means, every model that is using action matcher will has an string displayed on "
				"top of it shows the current parameters");

#endif // ENABLE_WATCHERS

	DataSectionPtr configSection = AppConfig::instance().pRoot();
	SimpleGUI::instance().init(configSection);

	//Now the scripts are setup, we can load a progress bar
	DeviceApp::s_pGUIProgress_ = new GUIProgressDisplay( loadingScreenGUI, processOutstandingMessages );
	if (DeviceApp::s_pGUIProgress_->gui())
	{
		SimpleGUI::instance().addSimpleComponent( *DeviceApp::s_pGUIProgress_->gui() );
		DeviceApp::s_pProgress_    = DeviceApp::s_pGUIProgress_;
	}
	else
	{
		delete DeviceApp::s_pGUIProgress_;
		DeviceApp::s_pGUIProgress_ = NULL;
		// Legacy Progress bar
		//NOTE : only reading the loading screen from ui/loadingScreen
		//for legacy support.  loading screen is not AutoConfig'd from
		//resources.xml in the system/loadingScreen entry.
		if ( loadingScreenName.value() == "" )
			configSection->readString( "ui/loadingScreen", "" );
		Vector3 colour = configSection->readVector3( "ui/loadingText", Vector3(255,255,255) );
		DeviceApp::s_pProgress_ = new ProgressDisplay( NULL, displayLoadingScreen, Colour::getUint32( colour, 255 ) );
	}

	char buf[ 256 ];
	sprintf( buf, "Build: %s %s. BuildId: %s",
		configString, compileTimeString, getBuildRevision().c_str() );
	DeviceApp::s_pProgress_->add( buf );

	if (DeviceApp::s_pGUIProgress_)
	{
		DeviceApp::s_pStartupProgTask_ = new ProgressTask( DeviceApp::s_pProgress_, "App Startup", PROGRESS_TOTAL );
	}
	else
	{
		//subtract one from progress total because there is no personality script part to it.
		DeviceApp::s_pStartupProgTask_ = new ProgressTask( DeviceApp::s_pProgress_, "App Startup", PROGRESS_TOTAL - 1 );
	}
	bool ret = !g_bAppQuit && DeviceApp::s_pStartupProgTask_->step(APP_PROGRESS_STEP);
	// Resource Stocktake
	ResUpdateHandler::instance().stocktake( DeviceApp::s_pProgress_ );

	MEM_TRACE_END()

	return ret;
}

void ScriptApp::fini()
{
	Watcher::fini();
	DeviceApp::instance.deleteGUI();
	BigWorldClientScript::fini();
}


// TODO: It would be good to put these functions in a better place. Perhaps put
// them in a namespace.
/**
 *	This function returns the total game time elapsed
 */
double getGameTotalTime()
{
	return App::instance().getTime();
}


/**
 *	This function returns the estimated server time.
 */
double getServerTime()
{
	ServerConnection * pSC = EntityManager::instance().pServer();

	if (pSC)
	{
		return pSC->serverTime( App::instance().getTime() );
	}

	return -1.0;
}


void ScriptApp::tick( float dTime )
{
	double totalTime = getGameTotalTime();

	static DogWatch	dwScript("Script");
	dwScript.start();

	// very first call any timers that have gone off
	// (including any completion procedures stored by action queues
	//  on their last tick)
	BigWorldClientScript::tick( totalTime );

	static DogWatch	dwPhysics("Physics");
	dwPhysics.start();

	// TODO: There is a weird bug related to the following line of code. It
	//	shows up when total time is very large. Try it by setting
	//	App::totalTime_ to something larger than 600,000. It looks as though
	//	the arguments to the following call are rounded to floats. This causes
	//	rounding problems.

	// call all the wards ticks for controlled entities
	// very important to call this (i.e. filter input) before
	// the entitymanager's (i.e. filter output)
	// also note: physics may call callbacks of its own
	Physics::tickAll( totalTime, totalTime - dTime );

	dwPhysics.stop();

	static DogWatch dwEntity("Entity");
	dwEntity.start();

	// call all the entitymanager's ticks
	EntityManager::instance().tick( totalTime, totalTime - dTime );

	// tell the server anything we've been waiting to
	ServerConnection * pSC = ConnectionControl::serverConnection();
	if (pSC != NULL && pSC->online())
	{
		if (totalTime - pSC->lastSendTime() >= pSC->minSendInterval())
			pSC->send();
	}

	dwEntity.stop();

	// see if the player's environemnt has changed
	static int oldPlayerEnvironment = -1;
	int newPlayerEnvironment = !isPlayerOutside();

	if (oldPlayerEnvironment != newPlayerEnvironment)
	{
		Script::call(
			PyObject_GetAttrString( Personality::instance(),
				"onChangeEnvironments" ),
			Py_BuildValue( "(i)", newPlayerEnvironment ),
			"Personality onChangeEnvironments: " );

		oldPlayerEnvironment = newPlayerEnvironment;
	}

	dwScript.stop();
}

void ScriptApp::inactiveTick( float dTime )
{
	this->tick(dTime);
}


/**
 *	Camera task
 */
class CameraApp : public MainLoopTask
{
	CameraApp()
	{ MainLoopTasks::root().add( this, "Camera/App", NULL ); }

	CameraApp::~CameraApp()
	{ /*MainLoopTasks::root().del( this, "Camera/App" );*/ }

	virtual bool init();
	virtual void fini();
	virtual void tick( float dTime );
	virtual void inactiveTick( float dTime );
	virtual void draw() { }

private:
	static CameraApp instance;
};
CameraApp CameraApp::instance;

bool CameraApp::init()
{
	DEBUG_MSG( "CameraApp::init: Initially using %d(~%d)KB\n",
		memUsed(), memoryAccountedFor() );

	DataSectionPtr configSection = AppConfig::instance().pRoot();

	ClientSpeedProvider::initInstance();
	ClientCamera::initInstance( configSection );

	MF_WATCH( "Render/Near Plane",
		*ClientCamera::instance().projAccess(),
		MF_ACCESSORS( float, ProjectionAccess, nearPlane ), "Camera near plane." );

	MF_WATCH( "Render/Far Plane",
		*ClientCamera::instance().projAccess(),
		MF_ACCESSORS( float, ProjectionAccess, farPlane ),"Camera far plane." );

	MF_WATCH( "Render/Fov",
		*ClientCamera::instance().projAccess(),
		MF_ACCESSORS( float, ProjectionAccess, fov ), "Field of view." );


	// Set up the camera
	float farPlaneDistance =
		configSection->readFloat( "camera/farPlane", 250.f );
	Moo::Camera cam( 0.25f, farPlaneDistance, DEG_TO_RAD(60.f), 2.f );
	cam.aspectRatio(
		float(Moo::rc().screenWidth()) / float(Moo::rc().screenHeight()) );
	Moo::rc().camera( cam );

	// read in some ui settings
	CameraControl::strafeRate( configSection->readFloat(
		"ui/straferate", 100.0 ) );

	CameraControl::initDebugInfo();

	return DeviceApp::s_pStartupProgTask_->step(APP_PROGRESS_STEP);
}

void CameraApp::fini()
{}


/**
 *	Helper class for tweening matrices
 */
struct TweenMatrix
{
	static void tween( const Matrix & a, const Matrix & b,
		float t, Matrix & out )
	{
		Matrix	ainv;	ainv.invert( a );
		Matrix	binv;	binv.invert( b );

		BlendTransform	ba( ainv ), bb( binv );
		ba.blend( t, bb );
		ba.output( out );

		out.invert();
	}
};

static AnnalSynchronised<Matrix,TweenMatrix,std::avector<Matrix> >	s_cameraAnnal;

void CameraApp::tick( float dTime )
{
	static DogWatch	dwCameras("Cameras");
	dwCameras.start();

	ClientCamera::instance().update( dTime );
	
	const BaseCameraPtr pCamera = ClientCamera::instance().camera();
	Matrix cameraMatrix;
	extern Matrix* g_profilerAppMatrix;
	if (g_profilerAppMatrix)
	{
		cameraMatrix = *g_profilerAppMatrix;
	}
	else
	{	
		cameraMatrix = pCamera->view();
	}

	// record it	
	if (AnnalVault::isRecording())
	{
		s_cameraAnnal.push( cameraMatrix );
	}
	else if (AnnalVault::isPlaying())
	{
		s_cameraAnnal.pop( cameraMatrix );
	}

	// and apply the change
	Moo::rc().reset();
	Moo::rc().view( cameraMatrix );
	Moo::rc().updateProjectionMatrix();
	Moo::rc().updateViewTransforms();

	// tell the chunk manager about it too
	ChunkSpacePtr pSpace = ChunkManager::instance().cameraSpace();

	if (pCamera && pCamera->spaceID() != 0)
		pSpace = ChunkManager::instance().space( pCamera->spaceID(), false );
	else if (Player::entity() && Player::entity()->pSpace())
		pSpace = Player::entity()->pSpace();

	if (pSpace)
	{
		ChunkManager::instance().camera( Moo::rc().invView(), pSpace );
	}
	else
	{
		ChunkManager::instance().camera( Matrix::identity, NULL );
	}

	static SpaceID lsid = -1;
	SpaceID tsid = pSpace ? pSpace->id() : -1;
	if (lsid != tsid)
	{
		lsid = tsid;

		INFO_MSG("CameraApp::tick: Camera space id is %d\n", lsid );
	}

	dwCameras.stop();
}

void CameraApp::inactiveTick( float dTime )
{
	this->tick( dTime );
}

/**
 *	Canvas task
 */
class CanvasApp : public MainLoopTask
{
public:
	CanvasApp() :
		gammaCorrectionOutside_( 1.f ),
		gammaCorrectionInside_( 1.f ),
		gammaCorrectionSpeed_( 0.2f ),
		dTime_( 0.f ),
		bloomFilter_( NULL ),
		heatFilter_( NULL ),
		playerFader_( NULL ),
		flashBang_( NULL )
	{ MainLoopTasks::root().add( this, "Canvas/App", NULL ); }

	CanvasApp::~CanvasApp()
	{ /*MainLoopTasks::root().del( this, "Canvas/App" );*/ }

	virtual bool init();
	virtual void fini()
	{
		Moo::EffectVisualContext::instance().fini();
		SAFE_DELETE(bloomFilter_);
		SAFE_DELETE(heatFilter_);
	}
	virtual void tick( float dTime );
	virtual void draw();

	static CanvasApp instance;

	std::vector<Vector4ProviderPtr>		flashBangAnimations_;
	Bloom*	bloomFilter()				{ return bloomFilter_; }
	HeatShimmer* heatFilter()			{ return heatFilter_; }

private:

	AdaptiveLODController	lodController_;
	float gammaCorrectionOutside_;
	float gammaCorrectionInside_;
	float gammaCorrectionSpeed_;

	PlayerFader *			playerFader_;
	Bloom *					bloomFilter_;
	HeatShimmer *			heatFilter_;
	FlashBangEffect *		flashBang_;

	float	dTime_;
public:
	EnviroMinder::DrawSelection	drawSkyCtrl_;

	void finishFilters();
};
CanvasApp CanvasApp::instance;

/*~ function BigWorld.flashBangAnimation
 *	@components{ client }
 *
 *	This function adds a vector4 provider for the flash bang animation.
 *	The output of the flashbanganimation is modulated with the previous
 *	frame of the game to create a saturated frame buffer. There can be
 *	any number of flash bang animations running at the same time.
 *
 *	@param p the Vector4Provider to add
 */
void flashBangAnimation( Vector4ProviderPtr p ) { CanvasApp::instance.flashBangAnimations_.push_back( p ); };
PY_AUTO_MODULE_FUNCTION( RETVOID, flashBangAnimation, ARG( Vector4ProviderPtr, END ), BigWorld )

/*~ function BigWorld.removeFlashBangAnimation
 *	@components{ client }
 *
 *	This function removes a Vector4Provider from the list of flash bang
 *	animations
 *
 *	@param p the Vector4Provider to remove
 */
void removeFlashBangAnimation( Vector4ProviderPtr p )
{	std::vector< Vector4ProviderPtr >& fba = CanvasApp::instance.flashBangAnimations_;
	std::vector< Vector4ProviderPtr >::iterator it = std::find( fba.begin(), fba.end(), p );
	if (it != fba.end())
		fba.erase( it );
};
PY_AUTO_MODULE_FUNCTION( RETVOID, removeFlashBangAnimation, ARG( Vector4ProviderPtr, END ), BigWorld )

/*~ function BigWorld.addAlwaysUpdateModel
 *	@components{ client }
 *
 *	This function adds a PyModel to a list of models that have their nodes updated
 *	even when they are not visible.
 *	@param pModel the model to always update
 */
void addAlwaysUpdateModel( PyModelPtr pModel )
{
	DeviceApp::updateModels_.push_back( pModel );
}
PY_AUTO_MODULE_FUNCTION( RETVOID, addAlwaysUpdateModel, ARG( PyModelPtr, END ), BigWorld )

/*~ function BigWorld.delAlwaysUpdateModel
 *	@components{ client }
 *
 *	This function removes a model from the update list.
 *	@param pModel the model to remove from the update list
 */
void delAlwaysUpdateModel( PyModelPtr pModel )
{
	std::vector< PyModelPtr >::iterator it = std::find(
		DeviceApp::updateModels_.begin(), DeviceApp::updateModels_.end(), pModel );
	if (it != DeviceApp::updateModels_.end())
	{
		DeviceApp::updateModels_.erase( it );
	}
}
PY_AUTO_MODULE_FUNCTION( RETVOID, delAlwaysUpdateModel, ARG( PyModelPtr, END ), BigWorld )

bool CanvasApp::init()
{
	DEBUG_MSG( "CanvasApp::init: Initially using %d(~%d)KB\n",
		memUsed(), memoryAccountedFor() );

	MEM_TRACE_BEGIN( "CanvasApp::init" )

	DataSectionPtr configSection = AppConfig::instance().pRoot();

	// Initialise the consoles
	ConsoleManager & mgr = ConsoleManager::instance();

	TextConsole * pTextConsole = new TextConsole();
	XConsole * pPythonConsole = new PythonConsole();
	XConsole * pStatusConsole = new XConsole();

	mgr.add( pTextConsole,			"Help",			KeyEvent::KEY_F1 );
	mgr.add( pPythonConsole,		"Python" );
	mgr.add( pStatusConsole,		"Status" );

	pTextConsole->init( BWResource::instance().openSection( "text/help.xml" ) );

	BigWorldClientScript::setPythonConsoles( pPythonConsole, pPythonConsole );

	Vector3 colour = configSection->readVector3( "ui/loadingText", Vector3(255,255,255) );
	pStatusConsole->setConsoleColour( Colour::getUint32( colour, 255 ) );
	pStatusConsole->setScrolling( true );
	pStatusConsole->setCursor( 0, pStatusConsole->visibleHeight()-2 );

	// print some status information
	loadingText( std::string( "Resource path:   " ) +
		BWResource::getDefaultPath() );

	loadingText(std::string( "App config file: " ) + s_configFileName);

	// Initialise the adaptive lod controller
	lodController_.minimumFPS( 10.f );
	lodController_.addController( "clod", &CLODPower, 10.f, 15.f, 50.f );

	MF_WATCH( "Client Settings/LOD/FPS",
		lodController_,
		&AdaptiveLODController::effectiveFPS,
		"Effective fps as seen by the adaptive Level-of-detail controller." );
	MF_WATCH( "Client Settings/LOD/Minimum fps",
		lodController_,
		MF_ACCESSORS( float, AdaptiveLODController, minimumFPS ),
		"Minimum fps setting for the adaptive level-of-detail controller.  FPS "
		"below this setting will cause adaptive lodding to take place." );

	for ( int i = 0; i < lodController_.numControllers(); i++ )
	{
		AdaptiveLODController::LODController & controller = lodController_.controller( i );
		std::string watchPath = "Client Settings/LOD/" + controller.name_;
		std::string watchName = watchPath + "/current";
		MF_WATCH( watchName.c_str(), controller.current_, Watcher::WT_READ_ONLY );
		watchName = "Client Settings/LOD/";
		watchName = watchName + controller.name_;
		watchName = watchName + " curr";
		MF_WATCH( watchName.c_str(), controller.current_, Watcher::WT_READ_ONLY );
		watchName = watchPath + "/default";
		MF_WATCH( watchName.c_str(), controller, MF_ACCESSORS( float, AdaptiveLODController::LODController, defaultValue ));
		watchName = watchPath + "/worst";
		MF_WATCH( watchName.c_str(), controller, MF_ACCESSORS( float, AdaptiveLODController::LODController, worst ));
		watchName = watchPath + "/speed";
		MF_WATCH( watchName.c_str(), controller, MF_ACCESSORS( float, AdaptiveLODController::LODController, speed ));
		watchName = watchPath + "/importance";
		MF_WATCH( watchName.c_str(), controller.relativeImportance_ );
	}

	// and some fog stuff
	Moo::rc().fogNear( 0 );
	Moo::rc().fogFar( 500 );
	Moo::rc().setRenderState( D3DRS_FOGCOLOR, 0x102030 );
	Moo::Material::standardFogColour( 0x102030 );

	// Renderer settings
	Moo::rc().tripleBuffering( configSection->readBool( "renderer/tripleBuffering", true ) );

	MF_WATCH( "waitForVBL", Moo::rc(),
		MF_ACCESSORS( bool, Moo::RenderContext, waitForVBL ),
		"Enable locking of frame presentation to the vertical blank signal" );
	MF_WATCH( "tripleBuffering", Moo::rc(),
		MF_ACCESSORS( bool, Moo::RenderContext, tripleBuffering ),
		"Enable triple-buffering, including the front-buffer and 2 back buffers" );

	gammaCorrectionOutside_ = configSection->readFloat(
		"renderer/gammaCorrectionOutside", configSection->readFloat(
		"renderer/gammaCorrection", gammaCorrectionOutside_ ) );
	gammaCorrectionInside_ = configSection->readFloat(
		"renderer/gammaCorrectionInside", configSection->readFloat(
		"renderer/gammaCorrection", gammaCorrectionInside_ ) );
	gammaCorrectionSpeed_ = configSection->readFloat(
		"renderer/gammaCorrectionSpeed", gammaCorrectionSpeed_ );

	MF_WATCH( "Render/Gamma Correction Outside",
		gammaCorrectionOutside_,
		Watcher::WT_READ_WRITE,
		"Gama correction factor when the camera is in outside chunks" );
	MF_WATCH( "Render/Gamma Correction Inside",
		gammaCorrectionInside_,
		Watcher::WT_READ_WRITE,
		"Gamma correction factor when the camera is in indoor chunks" );
	MF_WATCH( "Render/Gamma Correction Now", Moo::rc(),
		MF_ACCESSORS( float, Moo::RenderContext, gammaCorrection),
		"Current gama correction factor" );

	Moo::rc().gammaCorrection( gammaCorrectionOutside_ );

	MF_WATCH( "Render/Enviro draw",
		(int&)drawSkyCtrl_,
		Watcher::WT_READ_WRITE,
		"Enable / Disable various environment features such as sky, "
		"sun, moon and clouds." );

	// misc stuff

	ActionMatcher::globalEntityCollision_ =
		configSection->readBool( "entities/entityCollision", false );

	ParticleSystem::active( configSection->readBool(
		"entities/particlesActive", ParticleSystem::active() ) );

	Physics::setMovementThreshold( configSection->readFloat(
		"entities/movementThreshold", 0.25 ) );

	bool ret = DeviceApp::s_pStartupProgTask_->step(APP_PROGRESS_STEP);	

	if (heatFilter_ == NULL)
	{
		if (HeatShimmer::isSupported())
		{
			heatFilter_ = new HeatShimmer;
			if (!heatFilter_->init())
			{
				ERROR_MSG( "Heat Shimmer failed to initialise\n" );
				delete heatFilter_;
				heatFilter_ = NULL;
			}
		}
		else
		{
			INFO_MSG( "Heat Shimmer is not supported on this hardware\n" );
		}
	}

	if (playerFader_ == NULL)
	{
		playerFader_ = new PlayerFader();
	}

	if (bloomFilter_ == NULL)
	{
		if (Bloom::isSupported())
		{
			bloomFilter_ = new Bloom();
			if (!bloomFilter_->init())
			{
				ERROR_MSG( "Blooming failed to initialise\n" );
				delete bloomFilter_;
				bloomFilter_ = NULL;
			}
		}
		else
		{
			INFO_MSG( "Blooming is not supported on this hardware\n" );
		}
	}

	if (flashBang_ == NULL)
	{
		flashBang_ = new FlashBangEffect;
	}

	MEM_TRACE_END()

	return ret;
}


void CanvasApp::tick( float dTime )
{
	dTime_ = dTime;

	// Update the animating textures
	Moo::AnimatingTexture::tick( dTime );
	Moo::Material::tick( dTime );
	Moo::EffectVisualContext::instance().tick( dTime );

	//Adaptive degradation section
	lodController_.fpsTick( 1.f / dTime );

	// set the values
	int controllerIdx = 0;

	/*float recommendedDistance = lodController_.controller( controllerIdx++ ).current_;
	if ( this->getFarPlane() > recommendedDistance )
	{
		this->setFarPlane( recommendedDistance );
	}*/
	Moo::rc().lodPower( lodController_.controller( controllerIdx++ ).current_ );

	if (flashBang_)
	{
		if (flashBangAnimations_.size())
		{
			Vector4 v( 0, 0, 0, 0 );
			for (uint i = 0; i <  flashBangAnimations_.size(); i++)
			{
				Vector4 vt;
				flashBangAnimations_[i]->tick( dTime );
				flashBangAnimations_[i]->output( vt );
				v.x = max( vt.x, v.x );
				v.y = max( vt.y, v.y );
				v.z = max( vt.z, v.z );
				v.w = max( vt.w, v.w );
			}

			flashBang_->fadeValues( v );
		}
		else
		{
			flashBang_->fadeValues( Vector4( 0, 0, 0, 0 ) );
		}
	}
}


PROFILER_DECLARE( CanvasApp_draw, "CanvasApp Draw" );

void CanvasApp::draw()
{
	PROFILER_SCOPED( CanvasApp_draw );

	if(!::gWorldDrawEnabled)
		return;

	// set the gamma level
	float desiredGamma = isCameraOutside() ?
		gammaCorrectionOutside_ : gammaCorrectionInside_;
	float currentGamma = Moo::rc().gammaCorrection();
	if (currentGamma != desiredGamma)
	{
		currentGamma += Math::clamp( gammaCorrectionSpeed_ * dTime_,
			desiredGamma - currentGamma );
		Moo::rc().gammaCorrection( currentGamma );
	}

	// note we need to call pf->update before asking the FSBB if it is enabled.
	playerFader_->update();

	FullScreenBackBuffer::beginScene();

	// render the backdrop
	ChunkSpacePtr pSpace = ChunkManager::instance().cameraSpace();
	if (pSpace)
	{
		pSpace->enviro().drawHind( dTime_, drawSkyCtrl_ );
	}
}


void CanvasApp::finishFilters()
{
	if(!::gWorldDrawEnabled)
		return;

	FullScreenBackBuffer::endScene();

	HistogramProvider::instance().update();

	if (flashBang_)
	{
		flashBang_->draw();
	}
}


/*~ function BigWorld.selectBloomPreset
 *
 *	This method selects one of two preset values for the blooming filter.
 *	0 - a good default for standard blooming effect. Uses the quick 4x4 filter.
 *	1 - a good default for blurring the screen. Uses the slower 24x24 filter.
 *
 *	You can configure blooming by using the watcher values instead of using
 *	this selectBloomPreset funciton.  See the client programming guide for
 *	more details.
 *
 *	@param	Number of the bloom preset to choose, either 0 or 1.
 */
static void selectBloomPreset( uint32 i )
{
	Bloom* b = CanvasApp::instance.bloomFilter();

	if ( !b )
		return;

	i = i % 2;

	switch ( i )
	{
	case 0:
		//default blooming
		b->applyPreset( false, GAUSS_4X4 , 0.9f, 2 );
		break;
	case 1:
		//good focus blur setting
		b->applyPreset( true, GAUSS_24X24 , 0.08f, 1 );
		break;
	}
}
PY_AUTO_MODULE_FUNCTION(
	RETVOID, selectBloomPreset, ARG( uint32, END ), BigWorld )



/**
 *	World task
 */
class WorldApp : public MainLoopTask
{
public:
	WorldApp() :
		canSeeTerrain_( false ),
		wireFrameStatus_( 0 ),
		debugSortedTriangles_( 0 )
	{ MainLoopTasks::root().add( this, "World/App", NULL ); }

	WorldApp::~WorldApp()
	{ /*MainLoopTasks::root().del( this, "World/App" );*/ }

	virtual bool init();
	virtual void fini();
	virtual void tick( float dTime );
	virtual void inactiveTick( float dTime );
	virtual void draw();

	static WorldApp instance;

	bool canSeeTerrain() const					{ return canSeeTerrain_; }

private:
	bool canSeeTerrain_;
	float dTime_;

public:
	uint32				wireFrameStatus_;
	uint32				debugSortedTriangles_;
};
WorldApp WorldApp::instance;




/**
 *	This class sets the time of day on the server when it changes
 *	on the client.
 */
class ServerTODUpdater : public TimeOfDay::UpdateNotifier
{
public:
	ServerTODUpdater( ServerConnection * pSC ) : pSC_( pSC ) { }

	virtual void updated( const TimeOfDay & tod )
	{
		PyObject * pPers = Personality::instance();
		if (pPers == NULL) return;

		Script::call(
			PyObject_GetAttrString( pPers, "onTimeOfDayLocalChange" ),
			Py_BuildValue( "(ff)", tod.gameTime(), tod.secondsPerGameHour() ),
			"ServerTODUpdater notification: ",
			true );
	}

private:
	ServerConnection	* pSC_;
};

int menuChoice( const std::string & prompt, std::vector<std::string> & items )
{
	// TODO: Bring up a dialog box and ask!
	return 0;
}

extern bool g_drawTerrain;

bool WorldApp::init()
{
	DEBUG_MSG( "WorldApp::init: Initially using %d(~%d)KB\n",
		memUsed(), memoryAccountedFor() );

	MEM_TRACE_BEGIN( "WorldApp::init" )

	DataSectionPtr configSection = AppConfig::instance().pRoot();

	DEBUG_MSG( "Task: Initialising BgTaskManager...\n" );
	BgTaskManager::instance()->init( BgTaskManager::USE_LOADING_THREAD, true );

	DEBUG_MSG( "Chunk: Initialising ChunkManager...\n" );
	ChunkManager::instance().init();

	MF_WATCH( "Render/Wireframe Mode",
		WorldApp::instance.wireFrameStatus_,
		Watcher::WT_READ_WRITE,
		"Toggle wire frame mode between none, scenery, terrain and all." );

	MF_WATCH( "Render/Debug Sorted Tris",
		WorldApp::instance.debugSortedTriangles_,
		Watcher::WT_READ_WRITE,
		"Toggle sorted triangle debug mode.  Values of 1 or 2 will display "
		"sorted triangles so you can see how many are being drawn and where." );

//	MF_WATCH( "Draw Wireframe", g_drawWireframe );

	MF_WATCH( "Render/Draw Portals",
		ChunkBoundary::Portal::drawPortals_,
		Watcher::WT_READ_WRITE,
		"Draw portals on-screen as red polygons." );

	MF_WATCH( "Render/Draw Skeletons",
		PyModel::drawSkeletons_,
		Watcher::WT_READ_WRITE,
		"Draw the skeletons of PyModels." );

	MF_WATCH( "Render/Draw Node Labels",
		PyModel::drawNodeLabels_,
		Watcher::WT_READ_WRITE,
		"Draw model's node labels?" );

	MF_WATCH( "Render/Plot Foottriggers",
		FootTrigger::plotEnabled_,
		Watcher::WT_READ_WRITE,
		"Plot foot triggers height?" );

	MF_WATCH( "Render/Terrain/draw",
		g_drawTerrain,
		Watcher::WT_READ_WRITE,
		"Enable drawing of the terrain." );

	bool ret = DeviceApp::s_pStartupProgTask_->step(APP_PROGRESS_STEP) &&
					DeviceApp::s_pProgress_->draw( true );	// for if new loading screen

	MEM_TRACE_END()

	return ret;
}


void WorldApp::fini()
{
	BgTaskManager::instance()->fini();
    EntityManager::instance().fini();
	ChunkManager::instance().fini();
	Moo::TerrainRenderer::fini();
}

void WorldApp::tick( float dTime )
{
	dTime_ = dTime;
	PyChunkLight::revAll();
	PyChunkSpotLight::revAll();

	static DogWatch sceneTick("Scene");
	sceneTick.start();

	// do the background task manager tick
	BgTaskManager::instance()->tick();

	// do the chunk tick now
	ChunkManager::instance().tick( dTime );

	// tick the shadows
	ShadowManager::instance().tick( dTime );

	sceneTick.stop();
}

void WorldApp::inactiveTick( float dTime )
{
	this->tick( dTime );
}


PROFILER_DECLARE( WorldApp_draw, "WorldApp Draw" );
PROFILER_DECLARE( WorldApp_terrain, "WorldApp Terrain" );
PROFILER_DECLARE( WorldApp_visualCompound, "WorldApp VisualCompound" );

void WorldApp::draw()
{
	PROFILER_SCOPED( WorldApp_draw );
	static DogWatch sceneWatch( "Scene" );

	// draw the design-time scene
	sceneWatch.start();
	Moo::rc().setRenderState( D3DRS_FILLMODE,
		(wireFrameStatus_ & 2) ? D3DFILL_WIREFRAME : D3DFILL_SOLID );

	EnviroMinder * envMinder = 0;
	if (ChunkManager::instance().cameraSpace().getObject() != 0)
	{
		envMinder = &ChunkManager::instance().cameraSpace()->enviro();
	}

#if UMBRA_ENABLE
	if (UmbraHelper::instance().umbraEnabled())
	{
		Moo::rc().setRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
	ChunkManager::instance().umbraDraw();

		if (wireFrameStatus_ & (1 | 2))
		{
			Moo::rc().device()->EndScene();

			Vector3 bgColour = Vector3( 0, 0, 0 );
			Moo::rc().device()->Clear( 0, NULL,
					D3DCLEAR_ZBUFFER,
					Colour::getUint32( bgColour ), 1, 0 );

			Moo::rc().device()->BeginScene();

			Moo::rc().setRenderState( D3DRS_FILLMODE,
				(wireFrameStatus_ & 2) ? D3DFILL_WIREFRAME : D3DFILL_SOLID );

			//Moo::rc().setRenderState( D3DRS_FILLMODE, D3DFILL_WIREFRAME );
			ChunkManager::instance().umbraRepeat();
			//Moo::rc().setRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );

	//		Moo::rc().device()->SetViewport( &oldViewport );
		}
	}
	else
	{
		ChunkManager::instance().draw();
	}
#else
	ChunkManager::instance().draw();
#endif

	sceneWatch.stop();

	static DogWatch terrainWatch( "Terrain" );
	terrainWatch.start();
	Moo::rc().setRenderState( D3DRS_FILLMODE,
		(wireFrameStatus_ & 1) ? D3DFILL_WIREFRAME : D3DFILL_SOLID );

	ChunkSpacePtr pSpace = ChunkManager::instance().cameraSpace();
	if (pSpace) Moo::rc().lightContainer( pSpace->lights() );

	canSeeTerrain_ = Moo::TerrainRenderer::instance().canSeeTerrain();

	PROFILER_BEGIN( WorldApp_terrain );
	Moo::TerrainRenderer::instance().draw();
	PROFILER_END();

	Moo::rc().setRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
	terrainWatch.stop();

	sceneWatch.start();

	Moo::rc().setRenderState( D3DRS_FILLMODE,
		(wireFrameStatus_ & 2) ? D3DFILL_WIREFRAME : D3DFILL_SOLID );

	if (ChunkManager::instance().cameraSpace())
	{
		Moo::LightContainerPtr pRCLC = Moo::rc().lightContainer();
		Moo::LightContainerPtr pRCSLC = Moo::rc().specularLightContainer();
		static Moo::LightContainerPtr pLC = new Moo::LightContainer;
		static Moo::LightContainerPtr pSLC = new Moo::LightContainer;

		pLC->directionals().clear();
		pLC->ambientColour( ChunkManager::instance().cameraSpace()->ambientLight() );
		if (ChunkManager::instance().cameraSpace()->sunLight())
			pLC->addDirectional( ChunkManager::instance().cameraSpace()->sunLight() );
		pSLC->directionals().clear();
		pSLC->ambientColour( ChunkManager::instance().cameraSpace()->ambientLight() );
		if (ChunkManager::instance().cameraSpace()->sunLight())
			pSLC->addDirectional( ChunkManager::instance().cameraSpace()->sunLight() );
		Moo::rc().lightContainer( pLC );
		Moo::rc().specularLightContainer( pSLC );

		static DogWatch s_dwCompound( "Visual Compound" );
		s_dwCompound.start();
		PROFILER_BEGIN( WorldApp_visualCompound );
		Moo::VisualCompound::drawAll();
		PROFILER_END();
		s_dwCompound.stop();

		static DogWatch s_dwVisualBatches( "Visual Batches" );
		s_dwVisualBatches.start();
		Moo::Visual::drawBatches();
		s_dwVisualBatches.stop();

		Moo::rc().lightContainer( pRCLC );
		Moo::rc().specularLightContainer( pRCSLC );
	}

	Moo::rc().setRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
	sceneWatch.stop();

	ShadowManager::instance().renderShadows( canSeeTerrain_ );

	// Draw the delayed background of our environment.
	// We do this here in order to save fillrate, as we can
	// test the delayed background against the opaque scene that
	// is in the z-buffer.
	if (pSpace)
	{
		EnviroMinder & enviro = pSpace->enviro();
		enviro.drawHindDelayed( dTime_, CanvasApp::instance.drawSkyCtrl_ );
	}


//	Moo::rc().setRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALPHA |
//		D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE );
	TextureRenderer::updateCachableDynamics( dTime_ );
//	Moo::rc().setRenderState( D3DRS_COLORWRITEENABLE,
//		D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE );


}


/*~	function BigWorld.worldDrawEnabled
 *	Sets and gets the value of a flag used to control if the world is drawn.
 *	Note: the value of this flag will also be used to turn the watching of
 *	files being loaded in the main thread on or off. That is, if enabled,
 *	warnings will be issued into the debug output whenever a file is
 *	accessed from the main thread.
 *	@param	newValue	(optional) True enables the drawing of the world, False disables.
 *	@return Bool		If the no parameters are passed the current value of the flag is returned.
 */
static PyObject * py_worldDrawEnabled( PyObject * args )
{
	if ( PyTuple_Size( args ) == 1 )
	{
		int newDrawEnabled;

		PyArg_ParseTuple( args, "i", &newDrawEnabled );
		::gWorldDrawEnabled = (newDrawEnabled != 0);

		for( uint i=0; i<sizeof(gWorldDrawLoopTasks)/sizeof(const char*); i++ )
		{
			MainLoopTask * task = MainLoopTasks::root().getMainLoopTask( gWorldDrawLoopTasks[i] );
			if( task )
				task->enableDraw = ::gWorldDrawEnabled;
		}

		// when turning world draw enabled off, turn fs
		// access watching off straight away to prevent
		// warning of files being accessed in this frame.
		if (!gWorldDrawEnabled)
		{
			BWResource::watchAccessFromCallingThread(false);
		}

		Py_Return;
	}
	else if ( PyTuple_Size( args ) == 0 )
	{
		PyObject * pyId = PyBool_FromLong(static_cast<long>(::gWorldDrawEnabled));
		return pyId;
	}
	else
	{
		PyErr_SetString(
			PyExc_TypeError,
			"py_worldDrawEnabled expects one boolean or no arguments." );
		return NULL;
	}
}
PY_MODULE_FUNCTION( worldDrawEnabled, BigWorld )



/**
 *	Facade task
 */
class FacadeApp : public MainLoopTask
{
public:
	FacadeApp() : dTime_( 0.f )
	{ MainLoopTasks::root().add( this, "Facade/App", NULL ); }

	FacadeApp::~FacadeApp()
	{ /*MainLoopTasks::root().del( this, "Facade/App" );*/ }

	virtual bool init();
	virtual void fini();
	virtual void tick( float dTime );
	virtual void draw();

private:
	static FacadeApp instance;

	float			dTime_;
	ChunkSpacePtr	lastCameraSpace_;
	SmartPointer<TimeOfDay::UpdateNotifier>	todUpdateNotifier_;
};
FacadeApp FacadeApp::instance;

bool FacadeApp::init()
{
	EntityPicker::instance().selectionFoV( DEG_TO_RAD( 10.f ) );
	EntityPicker::instance().selectionDistance( 80.f );
	EntityPicker::instance().deselectionFoV( DEG_TO_RAD( 80.f ) );

	todUpdateNotifier_ =
		new ServerTODUpdater( ConnectionControl::serverConnection() );

	Waters::instance().init();

	return DeviceApp::s_pStartupProgTask_->step(APP_PROGRESS_STEP);
}

void FacadeApp::fini()
{
    Waters::instance().fini();

	// throw away this reference
	lastCameraSpace_ = NULL;
}


namespace
{
/*
 Wraps up the "Client Settings/Time of Day" and "Client Settings/Secs Per
 Hour" watcher values in a Singleton class. This way, they can reflect the
 current camera space.
 */
class TimeGlobals
{

private:
	TimeGlobals()
	{}

public:

	static TimeGlobals & instance()
	{
		static TimeGlobals tg;
		return tg;
	}

	void setupWatchersFirstTimeOnly()
	{
		static bool beenHere = false;

		if ( !beenHere )
		{
			MF_WATCH( "Client Settings/Time of Day",
				TimeGlobals::instance(),
				MF_ACCESSORS( std::string, TimeGlobals, timeOfDayAsString ), "Current time-of-day" );
			MF_WATCH( "Client Settings/Secs Per Hour",
				TimeGlobals::instance(),
				MF_ACCESSORS( float, TimeGlobals, secondsPerGameHour ), "Rate of actual seconds per game hour" );
		}

		beenHere = true;
	}

	TimeOfDay * getTimeOfDay() const
	{
		ChunkSpacePtr pSpace = ChunkManager::instance().cameraSpace();
		if (!pSpace)
		{
			ERROR_MSG( "app.cpp::getTimeOfDay: pSpace null!" );
			return NULL;
		}

		EnviroMinder & enviro = pSpace->enviro();

		TimeOfDay * tod = enviro.timeOfDay();

		return tod;
	}

	std::string timeOfDayAsString() const
	{
		return getTimeOfDay()->getTimeOfDayAsString();
	}

	void timeOfDayAsString( std::string newTime )
	{
		getTimeOfDay()->setTimeOfDayAsString( newTime );
	}

	float secondsPerGameHour() const
	{
		return getTimeOfDay()->secondsPerGameHour();
	}

	void secondsPerGameHour( float t )
	{
		getTimeOfDay()->secondsPerGameHour( t );
	}

};

}


void FacadeApp::tick( float dTime )
{
	dTime_ = dTime;

	// update the entity picker
	//  (must be here, after the camera position has been set)
	EntityPicker::instance().update( dTime );

	// update the weather
	ChunkSpacePtr pSpace = ChunkManager::instance().cameraSpace();
	if (!pSpace) return;

	EnviroMinder & enviro = pSpace->enviro();

	// if this space is new to us than transfer our Facade apparatus to it
	if (lastCameraSpace_ != pSpace)
	{
		// removing it from the old one first if necessary
		if (lastCameraSpace_)
		{
			EnviroMinder & oldEnviro = lastCameraSpace_->enviro();
			oldEnviro.deactivate();
			oldEnviro.timeOfDay()->delUpdateNotifier( todUpdateNotifier_ );
		}

		lastCameraSpace_ = pSpace;
		enviro.activate();
		enviro.timeOfDay()->addUpdateNotifier( todUpdateNotifier_ );

		// Probably not the ideal spot for this now.
		TimeGlobals::instance().setupWatchersFirstTimeOnly();
	}

	enviro.tick( dTime, isCameraOutside() );

	// play thunder if there is any
	const Vector4 & thunder = enviro.thunder();
	if (thunder.w < 1.f)
	{
		// TODO: Re-enable this sound with the new FMOD API
		// BWSound::playThunder( Vector3( (const float*)thunder ), thunder.w );
	}
}


void FacadeApp::draw()
{
	// Draw the main batch of sorted triangles
	Moo::rc().setRenderState( D3DRS_FILLMODE,
		(WorldApp::instance.wireFrameStatus_ & 2) ? D3DFILL_WIREFRAME : D3DFILL_SOLID );

	if (WorldApp::instance.debugSortedTriangles_ % 4)
	{
		switch ( WorldApp::instance.debugSortedTriangles_ % 4 )
		{
		case 1:
			Moo::rc().device()->Clear( 0, NULL, D3DCLEAR_ZBUFFER, 0x00, 1, 0 );

			Moo::Material::setVertexColour();
			Moo::rc().setRenderState( D3DRS_ZENABLE, FALSE );
			Moo::rc().setRenderState( D3DRS_ZFUNC, D3DCMP_ALWAYS );
			Moo::rc().setTexture(0, Moo::TextureManager::instance()->get(s_blackTexture)->pTexture());
			Geometrics::texturedRect( Vector2(0.f,0.f),
					Vector2(Moo::rc().screenWidth(),Moo::rc().screenHeight()),
					Moo::Colour( 1.f,1.f,1.f, 0.75f ), true );
			break;
		case 2:
			Moo::rc().device()->Clear( 0, NULL, D3DCLEAR_ZBUFFER, 0x00, 1, 0 );

			Moo::Material::setVertexColour();
			Moo::rc().setRenderState( D3DRS_BLENDOP, D3DBLENDOP_SUBTRACT );
			Moo::rc().setRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
			Moo::rc().setRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
			Moo::rc().setRenderState( D3DRS_ZENABLE, FALSE );
			Moo::rc().setRenderState( D3DRS_ZFUNC, D3DCMP_ALWAYS );
			Moo::rc().setTexture(0, Moo::TextureManager::instance()->get(s_blackTexture)->pTexture());
			Geometrics::texturedRect( Vector2(0,0),
					Vector2(Moo::rc().screenWidth(),Moo::rc().screenHeight()),
					Moo::Colour( 0.f, 1.f, 1.f, 1.f ), true );
			Moo::rc().setRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
			break;
		case 3:
			Moo::rc().device()->Clear( 0, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_TARGET, 0x00000000, 1, 0 );

			Moo::Material::setVertexColour();
			Moo::rc().setRenderState( D3DRS_ZENABLE, FALSE );
			Moo::rc().setRenderState( D3DRS_ZFUNC, D3DCMP_ALWAYS );
			Moo::rc().setTexture(0, Moo::TextureManager::instance()->get(s_blackTexture)->pTexture());
			Geometrics::texturedRect( Vector2(0.f,0.f),
					Vector2(Moo::rc().screenWidth(),Moo::rc().screenHeight()),
					Moo::Colour( 1.f,1.f,1.f, 1.f ), true );
			break;
		}
	}
	Moo::rc().setRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );

	ChunkSpacePtr pSpace = ChunkManager::instance().cameraSpace();
	if (pSpace)
	{
		// Draw waters
		Waters& phloem = Waters::instance();
		phloem.rainAmount( pSpace->enviro().rain()->amount() );

		if (Player::instance().entity())
		{
			phloem.playerPos( Player::instance().entity()->position() );
			WaterSceneRenderer::setPlayerModel( Player::instance().entity()->pPrimaryModel() );
		}

		//TODO: move waters into the enviro minder.
		phloem.drawDrawList( dTime_ );

		// Draw the forward part of our environment including flora
		pSpace->enviro().drawFore( dTime_, true, WorldApp::instance.canSeeTerrain(), false );

		// Render the shadowing onto the flora
		ShadowManager::instance().renderFloraShadows( pSpace->enviro().flora() );
	}

	// Draw the sorted triangles
	g_sortedWatch.start();
	Moo::SortedChannel::draw();
	g_sortedWatch.stop();
}



/**
 *	Lens task
 */
class LensApp : public MainLoopTask
{
public:
	LensApp() :
		dTime_( 0.f ),
		lastCameraSpace_( NULL )
	{ MainLoopTasks::root().add( this, "Lens/App", NULL ); }

	LensApp::~LensApp()
	{ /*MainLoopTasks::root().del( this, "Lens/App" );*/ }

	virtual bool init();
	virtual void fini();
	virtual void tick( float dTime );
	virtual void draw();

private:
	static LensApp instance;

	float dTime_;

	// LensEffect occluders
	std::vector<PhotonOccluder *>		ownOccluders_;
	ChunkSpacePtr		lastCameraSpace_;
};
LensApp LensApp::instance;


bool LensApp::init()
{
	if (ZBufferOccluder::isAvailable())
	{
		ownOccluders_.push_back( new ZBufferOccluder() );
		LensEffectManager::instance().addPhotonOccluder( ownOccluders_.back() );
	}

	return DeviceApp::s_pStartupProgTask_->step(APP_PROGRESS_STEP);
}

void LensApp::fini()
{
	while (ownOccluders_.size() > 0)
	{
		delete ownOccluders_.back();
		ownOccluders_.erase( ownOccluders_.end() - 1 );
	}

	LensEffectManager::instance().finz();
	lastCameraSpace_ = NULL;
}

void LensApp::tick( float dTime )
{
	dTime_ = dTime;

	// rearrange photon occluders if the camera just changed spaces
	ChunkSpacePtr pSpace = ChunkManager::instance().cameraSpace();
	if (lastCameraSpace_ != pSpace)
	{
		if (lastCameraSpace_)
		{
			if (!ZBufferOccluder::isAvailable())
			{
				LensEffectManager::instance().delPhotonOccluder( ownOccluders_.back() );
				ownOccluders_.pop_back();
			}
		}

		lastCameraSpace_ = pSpace;

		if (!ZBufferOccluder::isAvailable())
		{
			ownOccluders_.push_back( new ChunkObstacleOccluder() );
			LensEffectManager::instance().addPhotonOccluder( ownOccluders_.back() );
		}
	}
}

void LensApp::draw()
{
	if(!::gWorldDrawEnabled)
		return;

	g_lensEffectsWatch.start();
	LensEffectManager::instance().tick( dTime_ );
	g_lensEffectsWatch.stop();

	// Finish off the back buffer filters now
	CanvasApp::instance.finishFilters();
	// Note: I have moved this after drawFore, because I reckon the things
	// in there (seas, rain) prolly want to be affected by the filters too.

	// draw the lens effects
	g_lensEffectsWatch.start();
	LensEffectManager::instance().draw();
	g_lensEffectsWatch.stop();
}



/**
 *	GUI task
 */
class GUIApp : public MainLoopTask
{
public:
	GUIApp()
	{ MainLoopTasks::root().add( this, "GUI/App", NULL ); }

	GUIApp::~GUIApp()
	{ /*MainLoopTasks::root().del( this, "GUI/App" );*/ }

	virtual bool init();
	virtual void fini();
	virtual void tick( float dTime );
	virtual void draw();

public:
	static GUIApp instance;

private:
};
GUIApp GUIApp::instance;

bool GUIApp::init()
{
	// access simple gui
	SimpleGUI::instance().hInstance( DeviceApp::s_hInstance_ );
	SimpleGUI::instance().hwnd( DeviceApp::s_hWnd_ );

	DataSectionPtr configSection = AppConfig::instance().pRoot();

	// access the alert manager instance (so it creates its GUI stuff)
	AlertManager::instance();

	return DeviceApp::s_pStartupProgTask_->step(APP_PROGRESS_STEP);
}

void GUIApp::fini()
{
	//put here to avoid problems when client is shut down at weird spots
	// in the startup loop.
	Watcher::fini();
	DeviceApp::instance.deleteGUI();


	SimpleGUI::instance().fini();
}

void GUIApp::tick( float dTime )
{
	static DogWatch	dwGUI("GUI");
	dwGUI.start();

	// update the GUI components
	SimpleGUI::instance().update( dTime );

	dwGUI.stop();
}

void GUIApp::draw()
{
	// draw UI
	SimpleGUI::instance().draw();
	ChunkSpacePtr pSpace = ChunkManager::instance().cameraSpace();
	if (pSpace != NULL && pSpace->enviro().flora() != NULL)
	{
		pSpace->enviro().flora()->drawDebug();
	}
	Moo::SortedChannel::draw();
}


class PythonServer;


std::string g_specialConsoleString;

/**
 *	Helper class to save out diaries
 */
class MyDiarySaver : public Diary::Saver
{
public:
	MyDiarySaver( int i )
	{
		char fname[42];
		sprintf( fname, "c:\\thread%d.diary", i );
		pFILE_ = fopen( fname, "wb" );
	}

	~MyDiarySaver()
	{
		fclose( pFILE_ );
	}

	virtual void save( DiaryEntries::const_iterator beg,
		DiaryEntries::const_iterator end )
	{
		char * bigbuf = new char[65536];
		char * bbcur = bigbuf;
		while (beg != end)
		{
			DiaryEntry & de = **beg;

			*((uint64*&)bbcur)++ = de.start_;
			*((uint64*&)bbcur)++ = de.stop_;
			strcpy( bbcur, de.desc_.c_str() );
			bbcur += de.desc_.length()+1;
			*bbcur++ = (de.level_ << 4) | (de.colour_ & 7);

			++beg;
		}
		fwrite( bigbuf, 1, bbcur-bigbuf, pFILE_ );
		delete [] bigbuf;
	}

	static void openAll()
	{
		DiaryEntry & deOA = Diary::instance().add( "oa" );
		std::vector<Diary*> diaries;
		Diary::look( diaries );
		for (uint i = 0; i < diaries.size(); i++)
		{
			diaries[i]->saver( new MyDiarySaver( i ) );
		}
		deOA.stop();
	}

	static void closeAll()
	{
		std::vector<Diary*> diaries;
		Diary::look( diaries );
		for (uint i = 0; i < diaries.size(); i++)
		{
			Diary::Saver * pSV = diaries[i]->saver();
			if (pSV != NULL)
			{
				diaries[i]->saver( NULL );
				delete pSV;
			}
		}
	}

private:
	FILE * pFILE_;
};



/**
 *	This helper class displays a graph of the framerate.
 *	It is drawn even when the engine statistics console is not up.
 */
class FrameRateGraph : public Moo::DeviceCallback
{
public:
	void graph( float t )
	{
		if (display_)
		{

			float sHeight = Moo::rc().screenHeight();
			// The dHeight makes 60hz be halfway up the screen;
			float dHeight = sHeight / 120.f;
			float fps = sHeight - (dHeight / t);
			values_[valueIndex_++] = fps;
			valueIndex_ = valueIndex_ % 100;
			uint vi = valueIndex_;
			for (int i = 0; i < 100; i++)
			{
				verts_[ i ].pos_.y = values_[ vi % 100 ];
				vi++;
			}
			DX::Device* pDev = Moo::rc().device();
			mat_.set();
			Moo::rc().setFVF( Moo::VertexTL::fvf() );
			pDev->DrawPrimitiveUP( D3DPT_LINELIST, 3, measuringLines_, sizeof( Moo::VertexTL ) );
			pDev->DrawPrimitiveUP( D3DPT_LINESTRIP, 99, verts_, sizeof( Moo::VertexTL ) );
		}
	}
	static FrameRateGraph& instance()
	{
		static FrameRateGraph graph;
		return graph;
	}
private:
	FrameRateGraph()
	: valueIndex_(0),
	  display_( false )
	{
		mat_.fogged( false );
		mat_.zBufferRead( false );
		mat_.zBufferWrite( false );
		Moo::TextureStage ts;
		ts.colourOperation( Moo::TextureStage::SELECTARG2 );
		mat_.addTextureStage( ts );
		mat_.addTextureStage( Moo::TextureStage() );

		createUnmanagedObjects();

		MF_WATCH( "DisplayFramerateGraph",
			display_,
			Watcher::WT_READ_WRITE,
			"Enable frame rate graph, including 30 and 60 fps indicators." );
	}

	void createUnmanagedObjects()
	{
		float sWidth = Moo::rc().screenWidth();
		float sHeight = Moo::rc().screenHeight();
		// The dHeight makes 60hz be halfway up the screen;
		float dHeight = sHeight / 120.f;

        for (int i = 0; i < 100; i++)
		{
			values_[i] = sHeight;
			verts_[i].pos_.set( (sWidth / 100.f) * float(i), 0, 0, 1 );
			verts_[i].colour_ = 0x00ff0000;

		}

		measuringLines_[0].pos_.set( 0, sHeight - float(30 * dHeight), 0, 1 );
		measuringLines_[1].pos_.set( sWidth, sHeight - float(30 * dHeight), 0, 1 );
		measuringLines_[2].pos_.set( 0, sHeight - float(60 * dHeight), 0, 1 );
		measuringLines_[3].pos_.set( sWidth, sHeight - float(60 * dHeight), 0, 1 );
		measuringLines_[4].pos_.set( 0, sHeight - float(55 * dHeight), 0, 1 );
		measuringLines_[5].pos_.set( sWidth, sHeight - float(55 * dHeight), 0, 1 );
		measuringLines_[0].colour_ = 0x000000ff;
		measuringLines_[1].colour_ = 0x000000ff;
		measuringLines_[2].colour_ = 0x000000ff;
		measuringLines_[3].colour_ = 0x000000ff;
		measuringLines_[4].colour_ = 0x0000ff00;
		measuringLines_[5].colour_ = 0x0000ff00;

	}

    uint valueIndex_;
	float values_[100];
	Moo::VertexTL verts_[100];
	Moo::Material mat_;
	Moo::VertexTL measuringLines_[6];
	bool display_;
};

class SumWV : public WatcherVisitor
{
public:

	virtual bool visit( Watcher::Type type,
		const std::string & label,
		const std::string & desc,
		const std::string & valueStr )
	{
		if (label.substr( label.size()-4 ) == "Size")
		{
			//dprintf( "\t%s is %s\n", label.c_str(), valueStr.c_str() );
			sum_ += atoi( valueStr.c_str() );
		}
		return true;
	}
	uint32 sum_;
};
static SumWV s_sumWV;
static bool s_memoryAccountedFor_running = false;
uint32 memoryAccountedFor()
{
	if (s_memoryAccountedFor_running) return 0;
	s_memoryAccountedFor_running = true;
	s_sumWV.sum_ = 0;
#if ENABLE_WATCHERS
	Watcher::rootWatcher().visitChildren( NULL, "Memory/", s_sumWV );
#endif
	s_memoryAccountedFor_running = false;
	return s_sumWV.sum_ / 1024;
}
int32 memoryUnclaimed()
{
	// we are zero if memoryAccountedFor is running, even if
	// we didn't run it (since we are not accounted for!)
	if (s_memoryAccountedFor_running) return 0;
	memoryAccountedFor();
	return (memUsed() * 1024) - s_sumWV.sum_;
}

extern void registerAccountBudget( const std::string & account, uint32 budget );
extern void registerAccountContributor( const std::string & account,
	const std::string & name, uint32 & c );
extern void registerAccountContributor( const std::string & account,
	const std::string & name, uint32 (*c)() );


/**
 *	Shows a critical initialization error message box.
 *	Except in release builds, explanations about what can
 *	be wrong and where to look for more information on running
 *	the client will be displayed in the message box.
 */
void criticalInitError( const char * format, ... )
{
	// build basic error message
	char buffer1[ BUFSIZ * 2 ];

	va_list argPtr;
	va_start( argPtr, format );
	vsprintf( buffer1, format, argPtr );
	va_end( argPtr );

	// add additional explanations
	const char pathMsg[] =
#ifndef RELEASE
		"%s\n\nThe most probable causes for this error are running "
		"the game executable from the wrong working directory or "
		"having a wrong BW_RES_PATH environment variable. For more "
		"information on how to correctly setup and run BigWorld "
		"client, please refer to the Client Installation Guide, "
		"in mf/bigworld/doc directory.\n";
#else
		"%s"
#endif

	char buffer2[ BUFSIZ * 2 ];
	sprintf( buffer2, pathMsg, buffer1 );

	CRITICAL_MSG( buffer2 );
}




bool DebugApp::init()
{
	MEM_TRACE_BEGIN( "DebugApp::init" )

	DataSectionPtr configSection = AppConfig::instance().pRoot();

	// 1. Version Info

	// query the version
	pVersionInfo_ = new VersionInfo();
	pVersionInfo_->queryAll();

	//retrieve host name
	char buf[256];
	if ( !gethostname( buf, 256 ) )
		hostName_ = buf;
	else
		hostName_ = "LocalHost";

	//retrieve device and driver info
	/*D3DEnum_DeviceInfo* device = NULL;
	D3DEnum_DriverInfo* driver = NULL;

	Moo::rc().getDeviceParams( &device, &driver );
	if ( driver != NULL )
	{
		driverDesc_ = driver->strDesc;
		driverName_ = driver->strName;
	}
	else
	{*/
		driverDesc_ = "unknown";
		driverName_ = "unknown";
	//}

	/*if ( device != NULL )
	{
		deviceName_ = device->strName;
	}
	else
	{*/
		deviceName_ = "unknown";
	//}

	MF_WATCH( "System/Network/Host", hostName_, Watcher::WT_READ_WRITE, "Server hostname" );

	MF_WATCH( "System/Operating System/Version", *pVersionInfo_, &VersionInfo::OSName, "Name of Operating System" );
	MF_WATCH( "System/Operating System/Major", *pVersionInfo_, &VersionInfo::OSMajor, "Major version number of Operating System" );
	MF_WATCH( "System/Operating System/Minor", *pVersionInfo_, &VersionInfo::OSMinor, "Minor version number of Operating System" );
	MF_WATCH( "System/Operating System/Pack", *pVersionInfo_, &VersionInfo::OSServicePack, "Installed service pack" );

	MF_WATCH( "System/DirectX/Version", *pVersionInfo_, &VersionInfo::DXName, "Version of installed DirectX" );
	MF_WATCH( "System/DirectX/Major", *pVersionInfo_, &VersionInfo::DXMajor, "Major version number of installed DirectX" );
	MF_WATCH( "System/DirectX/Minor", *pVersionInfo_, &VersionInfo::DXMinor, "Minor version number of installed DirectX" );
	MF_WATCH( "System/DirectX/Device", deviceName_, Watcher::WT_READ_WRITE, "DirectX device name" );
	MF_WATCH( "System/DirectX/Driver Name", driverName_, Watcher::WT_READ_WRITE, "DirectX driver name" );
	MF_WATCH( "System/DirectX/Driver Desc", driverDesc_, Watcher::WT_READ_WRITE, "DirectX driver description" );
	MF_WATCH( "System/Video Adapter/Driver", *pVersionInfo_, &VersionInfo::adapterDriver, "Name of video adapter driver" );
	MF_WATCH( "System/Video Adapter/Desc", *pVersionInfo_, &VersionInfo::adapterDesc, "Description of video adapter driver" );
	MF_WATCH( "System/Video Adapter/Major Version", *pVersionInfo_, &VersionInfo::adapterDriverMajorVer, "Major version number of video adapter" );
	MF_WATCH( "System/Video Adapter/Minor Version", *pVersionInfo_, &VersionInfo::adapterDriverMinorVer, "Minor version number of video adapter" );

	MF_WATCH( "System/Global Memory/Total Physical(K)", *pVersionInfo_, &VersionInfo::totalPhysicalMemory, "Total physical memory" );
	MF_WATCH( "System/Global Memory/Avail Physical(K)", *pVersionInfo_, &VersionInfo::availablePhysicalMemory, "Available physical memory" );
	MF_WATCH( "System/Global Memory/Total Virtual(K)", *pVersionInfo_, &VersionInfo::totalVirtualMemory, "Total virtual memory" );
	MF_WATCH( "System/Global Memory/Avail Virtual(K)", *pVersionInfo_, &VersionInfo::availableVirtualMemory, "Available virtual memory" );
	MF_WATCH( "System/Global Memory/Total Page File(K)", *pVersionInfo_, &VersionInfo::totalPagingFile, "Total page file memory" );
	MF_WATCH( "System/Global Memory/Avail Page File(K)", *pVersionInfo_, &VersionInfo::availablePagingFile, "Available page file memory" );

	MF_WATCH( "System/Process Memory/Page Faults", *pVersionInfo_, &VersionInfo::pageFaults, "Memory page faults" );
	MF_WATCH( "System/Process Memory/Peak Working Set", *pVersionInfo_, &VersionInfo::peakWorkingSet, "Peak working set	size" );
	MF_WATCH( "System/Process Memory/Working Set", *pVersionInfo_, &VersionInfo::workingSet, "Current working set size" );
	MF_WATCH( "System/Process Memory/Quota Peaked Page Pool Usage", *pVersionInfo_, &VersionInfo::quotaPeakedPagePoolUsage, "Quota peak amount of paged pool usage" );
	MF_WATCH( "System/Process Memory/Quota Page Pool Usage", *pVersionInfo_, &VersionInfo::quotaPagePoolUsage, "Current quota amount of paged pool usage" );
	MF_WATCH( "System/Process Memory/Quota Peaked Nonpage Pool Usage", *pVersionInfo_, &VersionInfo::quotaPeakedNonPagePoolUsage, "Quota peak amount of non-paged pool usage" );
	MF_WATCH( "System/Process Memory/Quota Nonpage Pool Usage", *pVersionInfo_, &VersionInfo::quotaNonPagePoolUsage, "Current quota amount of non-paged pool usage" );
	MF_WATCH( "System/Process Memory/Peak Page File Usage", *pVersionInfo_, &VersionInfo::peakPageFileUsage, "Peak amount of page file space used" );
	MF_WATCH( "System/Process Memory/Page File Usage", *pVersionInfo_, &VersionInfo::pageFileUsage, "Current amount of page file space used" );

	static float cpuSpeed_ = float( stampsPerSecondD() / 1000000.0 );
	//
	MF_WATCH( "System/CPU/Speed", cpuSpeed_, Watcher::WT_READ_ONLY, "CPU Speed" );

	//
	MF_WATCH<int32>( "Memory/UnclaimedSize", &memoryUnclaimed, NULL, "Amount of unclaimed memory" );
	registerAccountContributor( "Base", "UnclaimedSize", (uint32(*)())memoryUnclaimed );

//	MF_WATCH( "Memory/Tale KB", &memoryAccountedFor );
	MF_WATCH<uint32>( "Memory/Used KB", &memUsed, NULL, "Amount of memory in use" );
	MF_WATCH<uint32>( "Memory/Full KB", &memTotal, NULL, "Total system memory" );
	MF_WATCH<uint32>( "Memory/Free KB", &memLeft, NULL, "Free memory" );

	MF_WATCH( "Memory/TextureManagerReckoning", *Moo::TextureManager::instance(),
		&Moo::TextureManager::textureMemoryUsed, "Amount of texture being used by textures" );

	const uint32 MB = 1024*1024;
	registerAccountBudget( "Texture", 16*MB );
	// Texture memory and associated data
	registerAccountBudget( "Geometry", 8*MB );		// +4 from BSP:3
	// Object-based memory related to geoemtry
	registerAccountBudget( "Animation", 4*MB );
	// Animation
	registerAccountBudget( "Sound", 4*MB );
	// Sound

	registerAccountBudget( "Video", 6*MB );
	// Video memory for push buffers and render targets
	registerAccountBudget( "Environment", 4*MB );	// +0 from Terrain:3
	// Mostly constant sized memory for environmental effects

	registerAccountBudget( "Scene", 1*MB );			// +1
	// Render scene definition and management structures (instance-based)
	registerAccountBudget( "Substance", 1*MB );		// +1
	// Collision scene definition and management structurs

	registerAccountBudget( "Base", 8*MB );
	// Base code and data and miscellaneous data structures
	registerAccountBudget( "Entity", 8*MB );		// +2 from GUI:1
	// Entity code and data (network and script-created or related objects)

	// Total: 60MB (But don't tell anyone!)


	// 2. Misc watchers

	// Top Level Info.
	MF_WATCH( "F/FPS",
		fps_,
		Watcher::WT_READ_ONLY,
		"Frames-per-second of the previous frame." );
	MF_WATCH( "F/maxFPS",
		maxFps_,
		Watcher::WT_READ_ONLY,
		"Recorded maxmimum FPS" );
	MF_WATCH( "F/minFPS",
		minFps_,
		Watcher::WT_READ_ONLY,
		"Recorded minimum FPS" );

	MF_WATCH( "FPS",
		fps_,
		Watcher::WT_READ_ONLY,
		"Frames-per-second of the previous frame." );

#ifdef _DEBUG
	MF_WATCH( "Mem allocs (bytes)",
		_totalMemUsedBytes,
		Watcher::WT_READ_ONLY,
		"Current number of bytes used." );

	MF_WATCH( "Memory allocs (k)",
		_totalMemUsed,
		Watcher::WT_READ_ONLY,
		"Current number of kilobytes used." );
#endif

	MF_WATCH( "debug/filterThreshold",
				DebugFilter::instance(),
				MF_ACCESSORS( int, DebugFilter, filterThreshold ) );

	MF_WATCH( "Build/Rev", &getBuildRevision  );

	MF_WATCH( "Build/Date", const_cast<char*&>(compileTimeString),
		Watcher::WT_READ_ONLY,
		"Build date of this executable." );

	MF_WATCH( "Build/Config", const_cast<char*&>(configString),
		Watcher::WT_READ_ONLY,
		"Build configuration of this executable." );

	MF_WATCH( "Build/Old string", const_cast<char*&>(buildIdString),
		Watcher::WT_READ_ONLY );

	MF_WATCH( "Client Settings/Slow time",
		slowTime_,
		Watcher::WT_READ_WRITE,
		"Time divisor.  Increase this value to slow down time." );

	MF_WATCH( "Client Settings/Sleep time (ms)",
		sleepTime_,
		Watcher::WT_READ_WRITE,
		"Number of milliseconds to pause (yield) in-between frames." );

	MF_WATCH( "Client Settings/Consoles/Special",
		drawSpecialConsole_,
		Watcher::WT_READ_WRITE,
		"Toggle drawing of the special (debug) console." );


	// 3. Diary Savers
#ifdef WANT_DIARIES
	Diary::s_enabled_ = true;
	Sleep( 50 );
	MyDiarySaver::openAll();
	atexit( MyDiarySaver::closeAll );
#endif


	// 4. Python server
#if ENABLE_PYTHON_TELNET_SERVICE
		pPyServer_ = new PythonServer();
		pPyServer_->startup( ConnectionControl::serverConnection()->nub(), 50001 );
#endif


	// 5. Critical message callback (move much earlier?)
	shouldBreakOnCritical_ = configSection->readBool(
		"breakOnCritical", shouldBreakOnCritical_ );
	shouldAddTimePrefix_ = configSection->readBool(
		"addTimePrefix", shouldAddTimePrefix_ );

	MF_WATCH( "Debug/Break on critical",
		shouldBreakOnCritical_,
		Watcher::WT_READ_WRITE,
		"If enabled, will display an error dialog on critical errors. "
		"Otherwise the application will attempt to continue running." );

	MF_WATCH( "Debug/Add time prefix",
		shouldAddTimePrefix_,
		Watcher::WT_READ_WRITE,
		"" );

	MF_WATCH( "Debug/Log slow frames",
		EngineStatistics::logSlowFrames_,
		Watcher::WT_READ_WRITE,
		"If enabled, slow frames are logged to a file so you can cross-check "
		"them with other debug output." );


	// 6. Consoles
	ConsoleManager & mgr = ConsoleManager::instance();
	XConsole * pStatisticsConsole =
		new StatisticsConsole( &EngineStatistics::instance() );
	mgr.add( pStatisticsConsole,	"Statistics" );
	mgr.add( new XConsole(),		"Special" );
	mgr.add( new DebugConsole(),	"Watcher" );
	mgr.add( new HistogramConsole(),	"Histogram" );

	FrameRateGraph::instance();

#if ENABLE_DOG_WATCHERS
		FrameLogger::init();
#endif // ENABLE_DOG_WATCHERS

	// 7. Watcher value specific settings
	DataSectionPtr pWatcherSection = configSection->openSection( "watcherValues" );
	if (pWatcherSection)
	{
		pWatcherSection->setWatcherValues();
	}

	bool ret = DeviceApp::s_pStartupProgTask_->step(APP_PROGRESS_STEP);

	MEM_TRACE_END()

	return ret;
}

void DebugApp::fini()
{
	if (pVersionInfo_)
	{
		delete pVersionInfo_;
		pVersionInfo_ = NULL;
	}

#if ENABLE_PYTHON_TELNET_SERVICE && !defined(BIGWORLD_CLIENT_ONLY)
		if (pPyServer_)
		{
			pPyServer_->shutdown();
			Py_DECREF( pPyServer_ );
			pPyServer_ = NULL;
		}
#endif
}


void DebugApp::tick( float dTime )
{
	dTime_ = dTime;
	if (sleepTime_ > 0)
	{
		PROFILER_SCOPED( Sys_Sleep );
		static DogWatch dwSleep( "Sleep" );
		dwSleep.start();
		Sleep( int(sleepTime_) );
		dwSleep.stop();
	}


	if (fpsIndex_ >= 50)
		fpsIndex_ = 0;

	fpsCache_[ fpsIndex_ ] = 1.f / dTime;

	minFps_ = fpsCache_[0];
	maxFps_ = fpsCache_[0];

	for (uint i = 1; i < 50; i++)
	{
		minFps_ = min( minFps_, fpsCache_[i] );
		maxFps_ = max( maxFps_, fpsCache_[i] );
	}


	fpsIndex_++;

	timeSinceFPSUpdate_ += dTime;
	if (timeSinceFPSUpdate_ >= 0.5f)
	{
		fps_ = 1.f / dTime;
		timeSinceFPSUpdate_ = 0;
	}

#if ENABLE_PYTHON_TELNET_SERVICE && !defined(BIGWORLD_CLIENT_ONLY)
	pPyServer_->pollInput();
#endif

}


PROFILER_DECLARE( DebugApp_draw, "DebugApp Draw" );

void DebugApp::draw()
{
	PROFILER_SCOPED( DebugApp_draw );

	EntityPicker::instance().drawDebugStuff();

	FrameRateGraph::instance().graph( dTime_ );

	PyModel::debugStuff( dTime_ );

	DiaryDisplay::displayAll();

#if 0
	// Draw sound debugging info
	soundMgr().drawDebugStuff();
#endif

	ClientSpeedProvider::instance().drawDebugStuff();


#ifdef CHUNKINTEST
	static DogWatch dwHullCollide( "HullCollide" );
	// test the collision scene (testing only!)
	Vector3 camPos = Moo::rc().invView().applyToOrigin();
	Vector3 camDir = Moo::rc().invView().applyToUnitAxisVector( 2 );
	camPos += camDir * Moo::rc().camera().nearPlane();
	DisplayCC dcc;

	dwHullCollide.start();
	Vector3 camDirProp(camDir.z,0,-camDir.x);
	WorldTriangle camTri(
		camPos - Moo::rc().invView().applyToUnitAxisVector( 0 ),
		camPos + Moo::rc().invView().applyToUnitAxisVector( 0 ),
		camPos + Moo::rc().invView().applyToUnitAxisVector( 1 ) );

	float res = ChunkManager::instance().space( 0 )->collide(
		camTri, camPos + camDir * 15, dcc );
	dwHullCollide.stop();

	char resBuf[256];
	sprintf( resBuf, "Collision result: %f\n\n", res );
	g_specialConsoleString += resBuf;
#endif

	// update our frame statistics
	EngineStatistics::instance().tick( dTime_ );

	// draw consoles
	static DogWatch consoleWatch( "Console" );
	consoleWatch.start();

	ConsoleManager::instance().draw( dTime_ );

	// draw our special console too
	if (drawSpecialConsole_)
	{
		XConsole * specialConsole =
			ConsoleManager::instance().find( "Special" );
		specialConsole->clear();
		if (g_specialConsoleString.empty())
		{
			specialConsole->setCursor( 0, 0 );
			specialConsole->print( "Special Console (empty)" );
		}
		else
		{
			specialConsole->setCursor( 0, 0 );
			specialConsole->print( g_specialConsoleString );
		}
		specialConsole->draw( dTime_ );
	}
	g_specialConsoleString = "";

	consoleWatch.stop();
}


// memory total in KB
uint32 memTotal()
{
	return 262144;
}

// memory currently used in KB
uint32 memUsed()
{
	VersionInfo	* pVI = DebugApp::instance.pVersionInfo_;
	if (pVI == NULL) return 0;
	return pVI->workingSetRefetched();
}

// memory remaining in KB
uint32 memLeft()
{
	return 65536;
}

/**
 *	Finale task
 */
class FinaleApp : public MainLoopTask, InputHandler
{
public:
	FinaleApp() :
		pFrameWriter_( NULL ),
		dTime_( 0.f ),
		cancelPreloads_( false ),
		disablePreloads_( false ),
		donePreloads_( false )
	{ MainLoopTasks::root().add( this, "Finale/App", NULL ); }

	FinaleApp::~FinaleApp()
	{ /*MainLoopTasks::root().del( this, "Finale/App" );*/ }

	bool handleKeyEvent(const KeyEvent & event);

	virtual bool init();
	virtual void fini();
	virtual void tick( float dTime );
	virtual void draw();

public:
	static FinaleApp instance;

	void toggleFrameWriter();

private:
	void runPreloads( ProgressTask & preloadTask );
	void findEffects( const std::string& folderName,
						std::vector<std::string>& ret );
	bool cacheEffects( const std::vector<std::string>& effects );

	float				dTime_;
	FrameWriter			* pFrameWriter_;
	bool				cancelPreloads_;
	bool				disablePreloads_;
	bool				donePreloads_;

	std::vector< PyObject* > preloadedPyObjects_;
};
FinaleApp FinaleApp::instance;


bool FinaleApp::init()
{
	DataSectionPtr configSection = AppConfig::instance().pRoot();

	// Run any preloads

	bool ret = true;

	//Note: the shader cache has been changed to only compile the standard
	// shader set and not create D3D effect object. This will be created
	// when the shader it touched in the preloads...
	if (configSection->readBool( "renderer/cacheEffects", true ))
	{
		// First cache all of the effects used by the system.
		std::vector<std::string> effectNames;
		this->findEffects( "shaders/std_effects/", effectNames );
		this->findEffects( "shaders/std_effects/shadows/", effectNames );
		this->findEffects( "shaders/std_effects/shimmer/", effectNames );
		this->findEffects( "shaders/std_effects/stipple/", effectNames );
		this->findEffects( "shaders/bloom/", effectNames );
		this->findEffects( "shaders/decal/", effectNames );
		this->findEffects( "shaders/environment/", effectNames );
		this->findEffects( "shaders/flora/", effectNames );
		this->findEffects( "shaders/terrain/", effectNames );
		this->findEffects( "shaders/water/", effectNames );
		this->findEffects( "shaders/speedtree/", effectNames );
		ret = this->cacheEffects( effectNames );
	}
	else
	{
		// Tick the progress bar because shader compilation / caching uses "1"
		if( DeviceApp::s_pProgress_ )
		{
			ret = DeviceApp::s_pStartupProgTask_->step(1.f) &&
					DeviceApp::s_pProgress_->draw( true );
		}
	}

	// Register the graphics settings for the effect files. 
 	// This is done by loading up effect files that define the settings. 
	DeviceApp::s_pProgress_->add( "Registering graphics settings" );

	std::vector<std::string> settingsEffects_; 
 	configSection->readStrings( "renderer/settings/effect", settingsEffects_ ); 
 	for (uint32 i = 0; i < settingsEffects_.size(); i++) 
 	{ 
 		Moo::ManagedEffectPtr pEffect = Moo::EffectManager::get( settingsEffects_[i] ); 
 		if (pEffect.hasObject()) 
 		{ 
 			pEffect->registerGraphicsSettings( settingsEffects_[i] ); 
 		} 
	}

	DataSectionPtr pSection = BWResource::instance().openSection( s_graphicsSettingsXML );

	// setup far plane options
	DataSectionPtr farPlaneOptionsSec = pSection->openSection("farPlane");
	EnviroMinderSettings::instance().init(farPlaneOptionsSec);

	// setup far flora options
	DataSectionPtr floraOptionsSec = pSection->openSection("flora");
	FloraSettings::instance().init(floraOptionsSec);

	Moo::StreamedAnimation::s_cacheMaxSize_ =
		( configSection->readInt( "animation/streamCacheSizeKB", 1024 )<<10 );

	// TODO: Move this whole functionality into the personality script!
	// It just needs to be able to access a sequence of entity modules.
	if (ret && !(disablePreloads_ = configSection->readBool( "entities/disablePreloads", false )))
	{
		DEBUG_MSG( "App::init: Before preloading using %d(~%d)KB\n",
			memUsed(), memoryAccountedFor() );

		if (DeviceApp::s_pGUIProgress_)
		{
			DeviceApp::s_pProgress_->add( "Preloading Resources" );
			this->runPreloads( *DeviceApp::s_pStartupProgTask_ );
		}
		else
		{
			ProgressTask    preloadTask =
				ProgressTask( DeviceApp::s_pProgress_, "Preloading Resources", 1.f );

			this->runPreloads( preloadTask );
		}

		DEBUG_MSG( "App::init: After preloading using %d(~%d)KB\n",
			memUsed(), memoryAccountedFor() );
	}

	// Import the personality
	std::string persName = configSection->readString( "personality" );
	PyObject * pPersonality = PyImport_ImportModule( (char*)persName.c_str() );
	if (pPersonality == NULL)
	{
		if (PyErr_Occurred())
		{
			PyErr_PrintEx(0);
			PyErr_Clear();
		}
		criticalInitError(
				"App::initPersonality: Could not load personality module <%s>!",
				persName.c_str() );

		return false;
	}
	Personality::instance().set( pPersonality );

	ret = ret && DeviceApp::s_pStartupProgTask_->step(APP_PROGRESS_STEP);

	DataSectionPtr scriptsConfig;
	if (!s_usingDeprecatedBigWorldXML)
	{
		scriptsConfig = BWResource::instance().openSection(s_scriptsConfigXML);
		if (!scriptsConfig.exists())
		{
			criticalInitError(
					"App::initPersonality: Could not load scripts config file <%s>!",
					s_scriptsConfigXML.value().c_str() );

			return false;
		}
	}

	//  Call personality.init function
	int extraParam = s_usingDeprecatedBigWorldXML ? 0 : 2;
	PyObject * pTuple = NULL;
	if (DeviceApp::s_pGUIProgress_)
	{
		pTuple = PyTuple_New( 2 + extraParam );
		PyObject * gui = DeviceApp::s_pGUIProgress_->gui().getObject();
		Py_INCREF(gui);
		PyTuple_SetItem( pTuple, 1 + extraParam, &*gui );
	}
	else
	{
		pTuple = PyTuple_New( 1 + extraParam );
	}

	DataSectionPtr engineConfig = AppConfig::instance().pRoot();
	if (!s_usingDeprecatedBigWorldXML)
	{
		PyTuple_SetItem( pTuple, 0, new PyDataSection( scriptsConfig ) );
		PyTuple_SetItem( pTuple, 1, new PyDataSection( engineConfig ) );
		PyTuple_SetItem( pTuple, 2, new PyDataSection( s_scriptsPreferences ) );
	}
	else
	{
		PyTuple_SetItem( pTuple, 0, new PyDataSection( engineConfig ) );
	}
	Script::call( PyObject_GetAttrString( pPersonality, "init" ),
		pTuple, "App::initPersonality: " );

	//  Load it into the __main__ modules list too
	persName = "import " + persName;
	PyRun_SimpleString( const_cast<char*>( persName.c_str() ) );

	// And start the game!
	Script::call( PyObject_GetAttrString( pPersonality, "start" ),
		PyTuple_New( 0 ), "App::init (start personality): " );


	return ret;
}

void FinaleApp::fini()
{
	BigWorldClientScript::clearSpaces();
	BigWorldClientScript::clearTimers();

	// stop recording
	if (pFrameWriter_ != NULL)
	{
		if (pFrameWriter_->started()) pFrameWriter_->stop();
		delete pFrameWriter_;
		pFrameWriter_ = NULL;
	}

	// Tell the personality script about it
	if (Personality::instance() != NULL)
	{
		Script::call( PyObject_GetAttrString( Personality::instance(), "fini" ),
			PyTuple_New( 0 ), "App::~App: " );
	}

	while (preloadedPyObjects_.size())
	{
		Py_DECREF(preloadedPyObjects_.back());
		preloadedPyObjects_.pop_back();

	}

	// Delete the player and entities.
	// (when we have a wrapper around personality,
	// it can do these when it's fini'd)
	ConnectionControl::instance().disconnect();
	EntityManager::instance().disconnected();
}


void FinaleApp::findEffects( const std::string& folderName,
							   std::vector<std::string>& ret )
{
	DataSectionPtr pFolder = BWResource::openSection( folderName );
	if (pFolder)
	{
		DataSectionIterator it = pFolder->begin();
		DataSectionIterator end = pFolder->end();
		while (it != end)
		{
			DataSectionPtr pDS = *it++;
			if (BWResource::getExtension( pDS->sectionName() ) == "fx")
			{
				ret.push_back(std::string(folderName + pDS->sectionName()));
			}
		}
	}
}

bool FinaleApp::cacheEffects( const std::vector<std::string>& effects )
{
	//TODO: if this system has alot of ram... preload the shaders as well as compiling?
	float step = 1.f / (float)(effects.size()+1);

	if( DeviceApp::s_pProgress_ )
	{
		DeviceApp::s_pProgress_->add( "Compiling Shaders for your system. This may take a while." );
		DeviceApp::s_pProgress_->draw( true );
	}

	for (uint i=0; i<effects.size(); i++)
	{
		Moo::EffectManager::compileOnly(effects[i]);
		if( DeviceApp::s_pProgress_ )
		{
			DeviceApp::s_pStartupProgTask_->step(step);
			DeviceApp::s_pProgress_->draw( true );
		}
	}
	BWResource::instance().purgeAll();

	INFO_MSG( "FinaleApp::cacheEffects - CACHING COMPLETE\n" );

	if( DeviceApp::s_pProgress_ )
	{
		DeviceApp::s_pStartupProgTask_->step(step);
		DeviceApp::s_pProgress_->draw( true );
	}

	return true;
}

bool FinaleApp::handleKeyEvent(const KeyEvent & event)
{
	bool handled = false;

	// check if this is a key up event
	if (event.isKeyDown())
	{
		if ( InputDevices::isKeyDown( KeyEvent::KEY_JOYBACK ) )
			this->cancelPreloads_ = true;
		if ( InputDevices::isKeyDown( KeyEvent::KEY_JOYSTART ) )
			this->cancelPreloads_ = true;
		if ( InputDevices::isKeyDown( KeyEvent::KEY_ESCAPE ) )
			this->cancelPreloads_ = true;
		if ( InputDevices::isKeyDown( KeyEvent::KEY_CAPSLOCK ) )
			this->cancelPreloads_ = true;
	}

	return 1;
}


void FinaleApp::tick( float dTime )
{
	dTime_ = dTime;
}

void FinaleApp::draw()
{
	// end rendering
	static DogWatch endSceneWatch("EndScene");
	endSceneWatch.start();

	Moo::rc().endScene();
	PROFILER_BEGIN( FinaleApp_present );
	Moo::rc().device()->Present( NULL, NULL, NULL, NULL );
	PROFILER_END();
	endSceneWatch.stop();

	Moo::rc().resetPrimitiveCount();

	// save out if recording
	if (pFrameWriter_ != NULL && pFrameWriter_->started())
	{
		static DogWatch movieWatch("Movie");
		movieWatch.start();

		ComObjectWrap<DX::Surface> copySurface = Moo::rc().getScreenCopy();

		if (copySurface.pComObject())
		{
			D3DSURFACE_DESC desc;

			copySurface->GetDesc( &desc );

			RECT	r;
			r.top = 0;
			r.left = 0;
			r.right = desc.Width;
			r.bottom = desc.Height;

			D3DLOCKED_RECT lr;

			if (SUCCEEDED( copySurface->LockRect( &lr, &r, D3DLOCK_READONLY ) ))
			{
				pFrameWriter_->save( &lr, dTime_ );
				copySurface->UnlockRect();
			}
			else
			{
				DEBUG_MSG( "Couldn't lock screen for movie recording\n" );
			}

		}
		movieWatch.stop();
	}

	// Only care on warning about loading of files
	// in the main thread if we are drawing the world
	BWResource::watchAccessFromCallingThread(gWorldDrawEnabled);
}


/**
 *	This method starts or stops recording as appropriate
 */
void FinaleApp::toggleFrameWriter()
{
	if (pFrameWriter_ != NULL && pFrameWriter_->started())
	{
		pFrameWriter_->stop();
		App::instance().setWindowTitleNote( 1, "" );
	}
	else
	{
		static int movieNumber = 0;
		char movieName[100];
		sprintf( movieName, "movie%03d", movieNumber );
		movieNumber++;

		static int nBuffers = 0;
		if (!nBuffers)
		{
			nBuffers = 16;
			MF_WATCH( "Client Settings/Movie/Buffers",
				nBuffers,
				Watcher::WT_READ_WRITE,
				"Number of buffers for the movie frame writer." );
		}

		if (!pFrameWriter_) pFrameWriter_ = new FrameWriter();
		RECT r;
		r.top = 0;
		r.left = 0;
		r.bottom = int(Moo::rc().screenHeight());
		r.right = int(Moo::rc().screenWidth());

		pFrameWriter_->start( movieName, &r, nBuffers );
		App::instance().setWindowTitleNote( 1,
			std::string("Writing ") + movieName + ".avi" );
	}
}



/**
 *	This method runs any preload activities
 */
void FinaleApp::runPreloads( ProgressTask & preloadTask )
{
	MEM_TRACE_BEGIN( "FinaleApp::runPreloads" )

	PyObject * pPreloadList = EntityType::getPreloads();

	int	listlen = PyList_Size( pPreloadList );

	float dStep = 1.f / (listlen + 2.f);
	preloadTask.step(dStep);

	for (int i=0; i < listlen; i++, preloadTask.step(dStep) )
	{
		InputDevices::processEvents( *this );
		if (this->cancelPreloads_)
			break;

		// make sure it's reasonable
		PyObject * pString = PyList_GetItem( pPreloadList, i );
		if (!PyString_Check( pString ) && !PyTuple_Check( pString ))
		{
			WARNING_MSG( "App::init: Couldn't preload resource named "
				"in element %d of list because it's not a string "
				"or a tuple\n", i );
			continue;
		}

		// see if it's a texture
		if (PyString_Check( pString ))
		{
			char * str = PyString_AsString( pString );
			char * pend = strrchr( str, '.' );

			if (pend != NULL && (!strcmp( pend, ".bmp" ) ||
				!strcmp( pend, ".tga" ) || !strcmp( pend, ".dds" )))
			{
				Moo::BaseTexturePtr pTex =
					Moo::TextureManager::instance()->get( str );
				if (!pTex)
				{
					WARNING_MSG( "App::init: "
						"Could not preload texture '%s'\n", str );
				}
				else
				{
					pTex->incRef();	// keep this around forever
					//TRACE_MSG( "App::init: "
					//	"After loading texture %s using %dKB\n",
					//	str, memUsed() );
				}
				continue;
			}
			else if ((pend != NULL && !(strcmp( pend, ".font" ) )))
			{
				FontManager::instance().get( str );
				//TRACE_MSG( "App::init: "
				//		"After loading font %s using %dKB\n",
				//		str, memUsed() );
				continue;
			}
			else if (!(pend != NULL && !(strcmp( pend, ".model" ) )))
			{
				DataSectionPtr pSect = BWResource::instance().openSection( str );
				if (pSect)
				{
					pSect->incRef();
					//TRACE_MSG( "App::init: "
					//	"After loading resource %s using %dKB\n",
					//	str, memUsed() );
				}
				continue;
			}

		}

		// ok, assume it's a model then
		SmartPointer<PyObject> pModelStr( PyObject_Str( pString ), true );
		if (!pModelStr)
		{
			WARNING_MSG( "App::init: "
				"Couldn't do str() on elt %d of preload list!\n", i );
			PyErr_PrintEx(0);
			PyErr_Clear();

			continue;
		}

		// find out what we should call it
		char * str = pModelStr ?
			PyString_AsString( pModelStr.getObject() ) : NULL;

		// figure out what arguments to pass to the factory method
		bool isString = PyString_Check( pString );
		PyObject * pArgs = pString;
		if (isString)
		{
			pArgs = PyTuple_New( 1 );
			PyTuple_SetItem( pArgs, 0, pString );
			Py_INCREF( pString );
		}

		// make the model!
		PyObject * pModel = PyModel::pyNew( pArgs );

		// decref the tuple if we made one
		if (isString) Py_DECREF( pArgs );

		// complain if we couldn't load it
		if (!pModel)
		{
			WARNING_MSG( "App::init: "
				"Couldn't preload a model\n", str );
			PyErr_PrintEx(0);
			PyErr_Clear();
			continue;
		}

		preloadedPyObjects_.push_back( pModel );

		// leak this reference so the model stays loaded
		//Py_DECREF( pModel );

		// and that's all there is to it
		//TRACE_MSG( "App::init: "
		//	"After loading model %s using %dKB\n", str, memUsed() );
	}

	Py_DECREF( pPreloadList );
	preloadTask.step(dStep);
	donePreloads_ = true;
	MEM_TRACE_END()
}


/**
 *	WatcherVisitor that prints memory counters
 */
static class MemoryWV : public WatcherVisitor
{
	virtual bool visit( Watcher::Type type,
		const std::string & label,
		const std::string & desc,
		const std::string & valueStr )
	{
		dprintf( "%s\t%s\n", label.c_str(), valueStr.c_str() );
		return true;
	}
} s_mwv;

/*~ function BigWorld.dumpMemCounters
 *	@components{ client }
 *
 *	This debugging function prints out the current value of memory watchers
 *	found in "Memory/" watcher directory.
 */
void dumpMemCounters()
{
#if ENABLE_WATCHERS
	Watcher::rootWatcher().visitChildren( NULL, "Memory/", s_mwv );
#endif
}
PY_AUTO_MODULE_FUNCTION( RETVOID, dumpMemCounters, END, BigWorld );


#ifdef USE_MEMORY_TRACER
/**
 *	This class traverses the memory trace tree, and writes to the output window
 */
class MemoryTraceDebugStringWalker : public ResourceMemoryTrace::TreeWalker
{
public:
	MemoryTraceDebugStringWalker( uint32 maxDepth, uint32 minKB ):
	  maxDepth_( maxDepth ),
	  minKB_( minKB )
	{
	}

	bool atLeaf( const std::string& id, uint32 memUsed, uint32 depth )
	{
		if ( minKB_ > memUsed )
			return false;

		for ( uint32 i=0; i<depth; i++ )
		{
			buf_[i] = '.';
		}
		buf_[depth]=(char)0;
		DEBUG_MSG( "%s%s %d\n", buf_, id.c_str(), memUsed );
		return ( maxDepth_ > depth );
	}

	char buf_[256];
	uint32 maxDepth_;
	uint32 minKB_;
};

void outputMemoryTrace( uint32 maxDepth = 1, uint32 minKB = 50 )
{
	MemoryTraceDebugStringWalker w( maxDepth, minKB );
	ResourceMemoryTrace::instance().traverse( w );
}
PY_AUTO_MODULE_FUNCTION(
	RETVOID, outputMemoryTrace, OPTARG( uint32, 1, OPTARG( uint32, 50, END ) ), BigWorld )

#endif


/**
 *	This method initialises the application.
 *
 *	@param hInstance	The HINSTANCE associated with this application.
 *	@param hWnd			The main window associated with this application.
 *
 *	@return		True if the initialisation succeeded, false otherwise.
 */
bool App::init( HINSTANCE hInstance, HWND hWnd )
{
	hWnd_ = hWnd;

	// Pass some parameters
	DeviceApp::s_hInstance_ = hInstance;
	DeviceApp::s_hWnd_ = hWnd;

	// Compress animations on load so we'll only save the compressed versions
    Moo::InterpolatedAnimationChannel::inhibitCompression( false );

	// Set up the MainLoopTask groups and dependencies
	MainLoopTasks::root().add( NULL, "Device", NULL );
#if BIGWORLD_VOIP
	MainLoopTasks::root().add( NULL, "VOIP",   ">Device", NULL );
	MainLoopTasks::root().add( NULL, "Script", ">VOIP",   NULL );
#else
	MainLoopTasks::root().add( NULL, "Script", ">Device", NULL );
#endif
	MainLoopTasks::root().add( NULL, "Camera", ">Script", NULL );
	MainLoopTasks::root().add( NULL, "Canvas", ">Camera", NULL );
	MainLoopTasks::root().add( NULL, "World",  ">Canvas", NULL );
	MainLoopTasks::root().add( NULL, "Flora",  ">World",  NULL );
	MainLoopTasks::root().add( NULL, "Facade", ">Flora",  NULL );
	MainLoopTasks::root().add( NULL, "Lens",   ">Facade", NULL );
	MainLoopTasks::root().add( NULL, "GUI",    ">Lens",   NULL );
	MainLoopTasks::root().add( NULL, "Debug",  ">GUI",    NULL );
	MainLoopTasks::root().add( NULL, "Finale", ">Debug",  NULL );

	// And initialise them all!
	bool ok = MainLoopTasks::root().init();

	MF_WATCH( "Debug/debugKeyEnable",
		debugKeyEnable_,
		Watcher::WT_READ_WRITE,
		"Toggle use of the debug key (CAPS)" );

	// Only set a default cursor if the personality script didn't set one
	if ( this->activeCursor_ == NULL )
		this->activeCursor( &DirectionCursor::instance() );

	// Unload the loading screen
	if ( ok )
	{
		freeLoadingScreen();
	}


#ifdef USE_MEMORY_TRACER
	outputMemoryTrace();
#endif

	// resetting lastTime so that the
	// first frame's dTime doesn't account
	// for the initialization time
	lastTime_ = frameTimerValue();

	return ok;
}

/**
 *	This function returns the current build revision
 */
static std::string getBuildRevision()
{
	return BWResource::instance().rootSection()->readString( "build.xml/rev" );
}


/**
 *	This class stores loading screen information.
 *	This class is only used by the method App::loadingScreen()
 *
 *	@see App::loadingScreen
 */
class LoadingScreenInfo
{
public:
	LoadingScreenInfo( std::string & name, bool fullScreen ) :
		name_( name ),
		fullScreen_( fullScreen )
	{
	}

	typedef std::vector< LoadingScreenInfo >	Vector;

	std::string & name( void )	{ return name_; }
	bool fullScreen( void )		{ return fullScreen_; }
private:
	std::string name_;
	bool		fullScreen_;
};



/**
 *	This method displays the loading screen.
 *	Assumes beginscene has already been called.
 */
static std::string lastUniverse = "/";
static Moo::Material loadingMat;

static bool displayLoadingScreen()
{
	if (!processOutstandingMessages())
		return false;

	if (DeviceApp::s_pGUIProgress_)
	{
		// and give watcher a go
		WatcherGlue::instance().receiveRequest();

		return true;
	}

	static bool fullScreen = true;
	static CustomMesh<Moo::VertexTLUV> mesh( D3DPT_TRIANGLESTRIP );
	static bool s_inited = false;

	//HACK_MSG( "displayLoadingScreen: heap size is %d\n", heapSize() );

	if (s_inited)
	{
		if ( loadingScreenName.value() != "" )
		{
			loadingMat.set();
			mesh.draw();
		}

		// and we draw the status console here too...
		XConsole * pStatCon = ConsoleManager::instance().find( "Status" );
		if (pStatCon != NULL) pStatCon->draw( 1.f );

		return true;
	}

	Moo::BaseTexturePtr	loadingBack = Moo::TextureManager::instance()->get( loadingScreenName );

	loadingMat = Moo::Material();
	Moo::TextureStage ts;
	ts.pTexture( loadingBack );
	ts.useMipMapping( false );
	ts.colourOperation( Moo::TextureStage::SELECTARG1 );
	loadingMat.addTextureStage( ts );
	Moo::TextureStage ts2;
	ts2.colourOperation( Moo::TextureStage::DISABLE );
	ts2.alphaOperation( Moo::TextureStage::DISABLE );
	loadingMat.addTextureStage( ts2 );
	loadingMat.fogged( false );

	Moo::VertexTLUV vert;
	mesh.clear();

	vert.colour_ = 0xffffffff;
	vert.pos_.z = 0;
	vert.pos_.w = 1;

	vert.pos_.x = 0;
	vert.pos_.y = 0;
	vert.uv_.set( 0,0 );
	mesh.push_back( vert );

	vert.pos_.x = Moo::rc().screenWidth();
	vert.pos_.y = 0;
	vert.uv_.set( 1,0 );
	mesh.push_back( vert );

	vert.pos_.x = 0;
	vert.pos_.y = Moo::rc().screenHeight();
	vert.uv_.set( 0,1 );
	mesh.push_back( vert );

	vert.pos_.x = Moo::rc().screenWidth();
	vert.pos_.y = Moo::rc().screenHeight();
	vert.uv_.set( 1,1 );
	mesh.push_back( vert );

	//fix texel alignment
	for ( int i = 0; i < 4; i++ )
	{
		mesh[i].pos_.x -= 0.5f;
		mesh[i].pos_.y -= 0.5f;
	}

	s_inited = true;

	// call ourselves to draw now that we're set up
	return displayLoadingScreen();
}


/**
 *	This method ensures the resources we used just for loading are freed up.
 */
static void freeLoadingScreen()
{
	lastUniverse = "/";
	loadingMat = Moo::Material();
}

/**
 *	Draw this loading text message. They appear beneath the progress bars
 */
static void loadingText( const std::string& s )
{
	ConsoleManager::instance().find( "Status" )->print( s + "\n" );

	if (DeviceApp::s_pProgress_ != NULL)
	{
		DeviceApp::s_pProgress_->draw( true );
	}

	INFO_MSG( "%s\n", s.c_str() );
}

// -----------------------------------------------------------------------------
// Section: Update and Draw functions
// -----------------------------------------------------------------------------



extern DogWatch	g_watchSound;	// declared in soundmgr.cpp


/**
 *	This method is called once per frame to do all the application processing
 *	for that frame.
 *	@param active if this value is true, do tick and draw, if not do inactive tick
 */
bool App::updateFrame(bool active)
{
	Profiler::instance().tick();

	DiaryEntry & deAll = Diary::instance().add( "A" );

	// Update the window title if the logging state has changed
	static int vaultNote = 0;
	int newVaultNote = int(AnnalVault::isRecording()) | (int(AnnalVault::isPlaying())<<1);
	if (newVaultNote != vaultNote)
	{
		vaultNote = newVaultNote;
		char * vaultNoteTexts[] = { "", "Recording Log", "Replaying Log", "" };
		this->setWindowTitleNote( 0, vaultNoteTexts[ newVaultNote ] );
	}

	// Timing
	this->calculateFrameTime();

	// Only do work if some time has passed as a lot of tick functionality relies on
	// dTime being non-zero
	if (dTime_ > 0.f)
	{
		// If we are active tick and render if not do inactive tick
		if (active)
		{
			// Now tick (and input)
			{
				PROFILER_SCOPED( App_Tick );
				g_watchTick.start();
				DiaryEntry & deTick = Diary::instance().add( "T" );
				MainLoopTasks::root().tick( dTime_ );
				deTick.stop();
				g_watchTick.stop();
			}

			// And draw
			if (Moo::rc().checkDevice())
			{
				PROFILER_SCOPED( App_Draw );
				g_watchOutput.start();
				DiaryEntry & deDraw = Diary::instance().add( "D" );
				MainLoopTasks::root().draw();
				deDraw.stop();
				g_watchOutput.stop();
			}
		}
		else
		{
			// Do the inacitve tick
			PROFILER_SCOPED( App_Tick );
			g_watchTick.start();
			DiaryEntry & deTick = Diary::instance().add( "T" );
			MainLoopTasks::root().inactiveTick( dTime_ );
			g_watchTick.stop();
		}

		uint64 frameEndTime = timestamp();
		if (minFrameTime_ > 0 && frameEndTime < lastFrameEndTime_ + minFrameTime_)
		{
			PROFILER_SCOPED( Sys_Sleep );
			uint32 sleepTime = uint32((lastFrameEndTime_ + minFrameTime_ - frameEndTime) * 1000 / stampsPerSecond());
			//dprintf("Sleeping for %d ms\n", sleepTime);
			::Sleep( sleepTime );
		}
		lastFrameEndTime_ = timestamp();

		deAll.stop();

		if (s_framesCounter <= 0)
		{
			return true;
		}
		else
		{
			--s_framesCounter;
			if (s_framesCounter % 100 == 0)
			{
				DEBUG_MSG("s_framesCounter: %d\n", s_framesCounter);
			}
			return  s_framesCounter > 0;
		}
	}

	return true;
}





/**
 *	This method is called once a frame by updateFrame to update the scene.
 *
 *	@param dTime	The amount of time that has elapsed since the last update.
 */
void App::updateScene( float dTime )
{
}








/**
 *	This method updates the pose of any cameras in the scene.
 */
void App::updateCameras( float dTime )
{
}


class DisplayCC : public CollisionCallback
{
private:
	virtual int operator()( const ChunkObstacle & obstacle,
		const WorldTriangle & triangle, float dist )
	{
		Moo::rc().push();
		Moo::rc().world( obstacle.transform_ );

		Moo::Colour yellow( 0x00ffff00 );
		Geometrics::drawLine( triangle.v0(), triangle.v1(), yellow );
		Geometrics::drawLine( triangle.v1(), triangle.v2(), yellow );
		Geometrics::drawLine( triangle.v2(), triangle.v0(), yellow );

		Moo::rc().pop();
		return 1|2;
	}
};



/**
 *	This method renders a frame using the current camera and current scene.
 */
void App::renderFrame()
{
}

/**
 *	This method draws the 3D world, i.e. anything that uses the Z buffer
 */
void App::drawWorld()
{
}


/**
 *	This method draws what is considered to be the scene, i.e. everything
 *	that is placed at a definite transform.
 */
void App::drawScene()
{
}



/**
 *	This struct records the keyboard state at a point in time.
 *	We need to record the whole state because the script calls
 *	back into the client to determine which bindings to activate.
 */
struct KeyboardState
{
	KeyboardState() {}

	KeyboardState( const KeyEvent & ev, bool * table ) :
		event( ev )
	{
		memcpy( keyDownTable, table, sizeof ( keyDownTable ) );
	}

	KeyEvent	event;
	bool		keyDownTable[ KeyEvent::NUM_KEYS ];
};

static AnnalIrregular<KeyboardState> s_scriptKeys;


/**
 *	This method plays back key events that were recorded earlier
 *	(May do more in the future)
 */
void App::handleInputPlayback()
{
	bool keyDownTable[ KeyEvent::NUM_KEYS ];
	memcpy( keyDownTable, InputDevices::keyDownTable(), sizeof(keyDownTable) );

	KeyboardState	ks;

	while (s_scriptKeys.pop( ks ))
	{
		KeyEvent & event = ks.event;

		memcpy( InputDevices::keyDownTable(), ks.keyDownTable,
			sizeof(keyDownTable) );

		if (Player::entity() != NULL)
		{
			Script::call(
				PyObject_GetAttrString( Player::entity(), "handleKeyEvent" ),
				Script::getData( event ),
				"App::handleInputPlayback: " );
		}
	}

	memcpy( InputDevices::keyDownTable(), keyDownTable, sizeof(keyDownTable) );
}


/**
 *	This method is called to start quitting the application.
 *	You may optionally restart the application.
 */
void App::quit( bool restart )
{
	if (restart)
	{
		STARTUPINFO si;
		PROCESS_INFORMATION pi;

		ZeroMemory( &si, sizeof(si) );
		si.cb = sizeof(si);
		ZeroMemory( &pi, sizeof(pi) );

		::CreateProcess( NULL, ::GetCommandLine(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi );
	}

	PostMessage( hWnd_, WM_CLOSE, 0, 0 );
}


/**
 *	This method overrides the InputHandler method to handle key events. These
 *	events include when mouse and joystick buttons are pressed, as well as when
 *	the keyboard keys are pressed or released.
 *
 *	@param event	The current mouse event.
 *
 *	@return			True if the event was handled, false otherwise.
 */
bool App::handleKeyEvent(const KeyEvent & event)
{
	// Create debug key event if the debug key is enabled.
	if (event.key() == KeyEvent::KEY_JOYBACK
		|| event.key() == KeyEvent::KEY_CAPSLOCK)
	{
		KeyEvent thisEvent( event.isKeyDown() ? KeyEvent::KEY_DOWN : KeyEvent::KEY_UP,
			KeyEvent::KEY_DEBUG, InputDevices::modifiers() );
		if (debugKeyEnable_ ||
			(InputDevices::keyDownTable()[KeyEvent::KEY_DEBUG] == true &&
			 event.isKeyUp()))
		{
			InputDevices::keyDownTable()[KeyEvent::KEY_DEBUG] = event.isKeyDown();
			this->handleKeyEvent( thisEvent );
		}
	}

	bool handled = false;

	// check if this is a key up event
	eEventDestination keySunk = EVENT_SINK_NONE;
	if (event.isKeyUp())
	{
		// disallow it when there has been no recorded key down.
		// this can happen when events are lost
		if (keyRouting_[event.key()] == EVENT_SINK_NONE) return true;

		// otherwise cache where it when to
		keySunk = keyRouting_[event.key()];

		// and clear it in advance
		keyRouting_[event.key()] = EVENT_SINK_NONE;
	}

	// only consider debug keys if caps lock is down
	if (!handled && event.isKeyDown() &&
		/*(InputDevices::isKeyDown( KeyEvent::KEY_CAPSLOCK ) ||
		InputDevices::isKeyDown( KeyEvent::KEY_JOYBACK ))*/
		InputDevices::isKeyDown( KeyEvent::KEY_DEBUG ))
	{
		handled = this->handleDebugKeyDown( event );
		if (handled)
		{
			keyRouting_[event.key()] = EVENT_SINK_DEBUG;
		}
	}

	// consume keyups corresponding to EVENT_SINK_DEBUG keydowns
	if (keySunk == EVENT_SINK_DEBUG) handled = true;


	// give the active console and console manager a go
	if (!handled)
	{
		handled = ConsoleManager::instance().handleKeyEvent( event );
		if (handled && event.isKeyDown())
		{
			keyRouting_[event.key()] = EVENT_SINK_CONSOLE;
		}
	}

	// consume keyups corresponding to EVENT_SINK_CONSOLE keydowns
	if (keySunk == EVENT_SINK_CONSOLE) handled = true;

	// now give the personality script its go
	if (!handled)
	{
		PyObject * ret = Script::ask(
			PyObject_GetAttrString( Personality::instance(), "handleKeyEvent" ),
			Script::getData( event ),
			"Personality handleKeyEvent: " );
		Script::setAnswer( ret, handled, "Personality handleKeyEvent retval" );

		if (handled && event.isKeyDown())
		{
			keyRouting_[event.key()] = EVENT_SINK_PERSONALITY;
		}
	}

	// consume keyups corresponding to EVENT_SINK_PERSONALITY keydowns
	if (keySunk == EVENT_SINK_PERSONALITY) handled = true;


	// give the camera a look at it
	if (!handled)
	{
		handled = ClientCamera::instance().camera()->handleKeyEvent( event );
		if (handled && event.isKeyDown())
		{
			keyRouting_[event.key()] = EVENT_SINK_CAMERA;
		}
	}

	// consume keyups corresponding to EVENT_SINK_CAMERA keydowns
	if (keySunk == EVENT_SINK_CAMERA) handled = true;

	// give the app its chance (it only wants keydowns)
	if (!handled && event.isKeyDown())
	{
		handled = this->handleKeyDown( event );
		if (handled && event.isKeyDown())
		{
			keyRouting_[event.key()] = EVENT_SINK_APP;
		}
	}

	// consume keyups corresponding to EVENT_SINK_APP keydowns
	if (keySunk == EVENT_SINK_APP) handled = true;


	// finally let the script have the crumbs
	//  (has to be last since we don't extract 'handled' from it)
	if (!handled)
	{
		if (!AnnalVault::isPlaying() && Player::entity() != NULL)
		{
			Script::call(
				PyObject_GetAttrString( Player::entity(), "handleKeyEvent" ),
				Script::getData( event ),
				"Player handleKeyEvent: " );
		}

		// TODO: Look at return value and set 'handled'

		// For now, sink all key presses in scripts
		if (event.isKeyDown())
		{
			keyRouting_[event.key()] = EVENT_SINK_SCRIPT;
			handled = true;
		}
		else if (event.isKeyUp())
		{
			keyRouting_[event.key()] = EVENT_SINK_NONE;
			handled = true;
		}

		// record that we sent this event if we're recording
		if (AnnalVault::isRecording())
		{
			s_scriptKeys.push( KeyboardState( event, InputDevices::keyDownTable() ) );
		}
	}


	// and for sanity make sure the key routing entry is cleared
	//  if we got a key up (it should already have been)
	if (keySunk != EVENT_SINK_NONE && !handled)
	{
		WARNING_MSG( "KeyUp for 0x%02x routed to %d was unclaimed!\n",
			int(event.key()), int(keySunk) );
	}

	return handled;
}


/**
 *	This function handles key and button down events.
 *	It is called by App::handleKeyEvent above.
 */
bool App::handleKeyDown( const KeyEvent & event )
{
	bool handled = true;

	switch (event.key())
	{

	case KeyEvent::KEY_F4:
		if (event.isAltDown())
		{
			this->quit();
		}
		break;

	case KeyEvent::KEY_SYSRQ:
		Moo::rc().screenShot();
		break;

	case KeyEvent::KEY_RETURN:
		if (InputDevices::isAltDown( ))
		{
			if( Moo::rc().device() )
				Moo::rc().changeMode( Moo::rc().modeIndex(), !Moo::rc().windowed() );
		}
		else
		{
			handled = false;
		}
		break;

	case KeyEvent::KEY_TAB:
		if (!event.isAltDown())
		{
			handled = false;
		}
		else if( Moo::rc().device() && !Moo::rc().windowed() )
		{
			Moo::rc().changeMode( Moo::rc().modeIndex(), true );
		}
		break;

	default:
		handled = false;
	}

	return handled;
}


/**
 *	Handle a debugging key. This only gets called if caps lock is down.
 */
bool App::handleDebugKeyDown( const KeyEvent & event )
{
#if !ENABLE_DEBUG_KEY_HANDLER

	return false;

#else // ENABLE_DEBUG_KEY_HANDLER


	bool handled = true;

	switch (event.key())
	{

	case KeyEvent::KEY_F2:
	{
		RECT clientRect;
		GetClientRect( hWnd_, &clientRect );

		const int numSizes = 4;
		POINT sizes[numSizes] = {
			{ 512, 384 }, { 640, 480 },
			{ 800, 600 }, { 1024, 768 } };

		int i = 0;
		int width = this->windowSize().x;
		for (; i < numSizes; ++i)
		{
			if (sizes[i].x == width)
			{
				break;
			}
		}
		i = (i + (event.isShiftDown() ? numSizes-1 : 1)) % numSizes;
		this->resizeWindow(sizes[i].x, sizes[i].y);
		break;
	}


	case KeyEvent::KEY_F3:
		if (event.isCtrlDown())
		{
			if (!AnnalVault::isStopped())
			{
				AnnalVault::instance().stop();
			}
			else if (!event.isShiftDown())
			{
				AnnalVault::instance().record();
			}
			else
			{
				AnnalVault::instance().playback();
			}
		}
		else if (event.isShiftDown())
		{
			FinaleApp::instance.toggleFrameWriter();
		}
		break;

	case KeyEvent::KEY_F4:
		if( !( event.isCtrlDown() || event.isAltDown() ) )
		{
			ConsoleManager::instance().toggle( "Histogram" );
		}
		break;



	case KeyEvent::KEY_F6:
	{
		int modsum =
				( event.isCtrlDown()  ? EnviroMinder::DrawSelection::skyGradient : 0 ) |
				( event.isAltDown()   ? EnviroMinder::DrawSelection::clouds      : 0 ) |
				( event.isShiftDown() ?
					EnviroMinder::DrawSelection::sunAndMoon +
					EnviroMinder::DrawSelection::sunFlare : 0 );

		if (!modsum)
		{
			// toggle all sky drawing options
			CanvasApp::instance.drawSkyCtrl_ =
				CanvasApp::instance.drawSkyCtrl_ ? 0 : EnviroMinder::DrawSelection::all;
		}
		else
		{
			// toggle the option formed by adding the modifier's binary values
			CanvasApp::instance.drawSkyCtrl_ = CanvasApp::instance.drawSkyCtrl_ ^ modsum;

			// as in other areas (such as watcher menus),
			// shift is worth one, ctrl is worth 2, and alt is worth 4.
		}
		break;
	}


	case KeyEvent::KEY_F8:
		WorldApp::instance.wireFrameStatus_++;
		break;


	case KeyEvent::KEY_F9:
	{
		bool darkBG = ConsoleManager::instance().darkenBackground();
		ConsoleManager::instance().setDarkenBackground(!darkBG);
	}

	case KeyEvent::KEY_F10:
		{
			Moo::Camera cam = Moo::rc().camera();
			cam.ortho( !cam.ortho() );
			Moo::rc().camera( cam );
		}
		break;

	case KeyEvent::KEY_F11:
	{
		DEBUG_MSG( "App::handleKeyDown: Reloading entity script classes...\n" );
		XConsole * pConsole = ConsoleManager::instance().find( "Python" );

		if (EntityType::reload())
		{
			DEBUG_MSG( "App::handleKeyDown: reload successful!\n" );
#if 0
			::soundMgr().playFx( "loadGun" );
#endif
			pConsole->print( "App: Script reload succeeded.\n" );
		}
		else
		{
			DEBUG_MSG( "App::handleKeyDown: reload failed.\n" );
#if 0
			::soundMgr().playFx( "players/hurt" );
#endif
			pConsole->print( "App: Script reload failed.\n" );

			if (PyErr_Occurred())
			{
				PyErr_PrintEx(0);
				PyErr_Clear();
			}
		}
		break;
	}
#if UMBRA_ENABLE
	case KeyEvent::KEY_O:
	{
		// Toggle umbra occlusion culling on/off using the watchers
		std::string result;
		std::string desc;
		Watcher::Type type;
		if (Watcher::rootWatcher().getAsString( NULL, "Render/Umbra/occlusionCulling", result, desc, type ))
		{
			if (result == std::string( "true" ))
			{
				Watcher::rootWatcher().setFromString( NULL, "Render/Umbra/occlusionCulling", "false" );
			}
			else
			{
				Watcher::rootWatcher().setFromString( NULL, "Render/Umbra/occlusionCulling", "true" );
			}
		}
		break;
	}
	case KeyEvent::KEY_U:
	{
		// Toggle umbra code path on/off 

		UmbraHelper::instance().umbraEnabled( !UmbraHelper::instance().umbraEnabled() );

		break;
	}
#endif

	case KeyEvent::KEY_5:
	case KeyEvent::KEY_H:
		Filter::isActive( !Filter::isActive() );
		ConsoleManager::instance().find( "Python" )->print( Filter::isActive() ?
			"App: Filter is on\n" :
			"App: Filter is off\n" );
		break;

	case KeyEvent::KEY_I:
		// Invert mouse verticals for DirectionCursor.
		CameraControl::isMouseInverted(
			!CameraControl::isMouseInverted() );

		ConsoleManager::instance().find( "Python" )->print(
				CameraControl::isMouseInverted() ?
			"App: Mouse Vertical Movement Inverted\n" :
			"App: Mouse Vertical Movement Normal\n" );
		break;

	case KeyEvent::KEY_J:
		InputDevices::joystick().useJoystick();
		DirectionCursor::instance().lookSpring( true );
		ConsoleManager::instance().find( "Python" )->print( "App: Using joystick\n" );
		break;


	case KeyEvent::KEY_K:
		InputDevices::joystick().useKeyboard();
		DirectionCursor::instance().lookSpring( false );
		ConsoleManager::instance().find( "Python" )->print( "App: Using keyboard\n" );
		break;


	case KeyEvent::KEY_N:
		EntityManager::instance().displayIDs(
			!EntityManager::instance().displayIDs() );
		break;

	case KeyEvent::KEY_P:
		if (!event.isCtrlDown())
		{
			// Toggle python console
			ConsoleManager::instance().toggle( "Python" );
		}
		break;

	case KeyEvent::KEY_LBRACKET:
	{
		if (ChunkManager::instance().cameraSpace().getObject() != NULL)
		{
			EnviroMinder & enviro =
				ChunkManager::instance().cameraSpace()->enviro();

			if (event.isShiftDown() )
			{
				// Move back an hour.
				enviro.timeOfDay()->gameTime(
					enviro.timeOfDay()->gameTime() - 1.0f );
			}
			else
			{
				// Move back by 10 minutes.
				enviro.timeOfDay()->gameTime(
					enviro.timeOfDay()->gameTime() - 10.0f/60.0f );
			}
		}
		break;
	}


	case KeyEvent::KEY_RBRACKET:
	{
		if (ChunkManager::instance().cameraSpace().getObject() != NULL)
		{
			EnviroMinder & enviro =
				ChunkManager::instance().cameraSpace()->enviro();

			if (event.isShiftDown() )
			{
				// Move forward an hour.
				enviro.timeOfDay()->gameTime(
					enviro.timeOfDay()->gameTime() + 1.0f );
			}
			else
			{
				// Move forward 10 minutes.
				enviro.timeOfDay()->gameTime(
					enviro.timeOfDay()->gameTime() + 10.0f/60.0f );
			}
		}
		break;
	}

	case KeyEvent::KEY_F5:
		if (!event.isCtrlDown())
		{
			ConsoleManager::instance().toggle( "Statistics" );
		}
		else
		{
			DebugApp::instance.drawSpecialConsole_ ^= true;
		}
		break;
	case KeyEvent::KEY_F7:
		if (!event.isCtrlDown())
		{
			ConsoleManager::instance().toggle( "Watcher" );
		}
		else
		{
			ParticleSystem::active( !ParticleSystem::active() );
		}
		break;


	case KeyEvent::KEY_APOSTROPHE:
		/*
		if (event.isShiftDown()) g_cloudRadius-=20; else g_cloudRadius+=20;
		if (g_cloudRadius < 1) g_cloudRadius = 1;

		srand( 12345 );
		sky_->newCloud( g_cloudRadius );
		*/

		if (event.isCtrlDown())
		{
			char fileName[100];

			if (!event.isShiftDown())
			{
				static int saveVaultNumber = 0;
				static int initMFWatch = false;
				if (!initMFWatch) {
					MF_WATCH( "Client Settings/Movie/saveVaultNum", saveVaultNumber );
					initMFWatch = false;
				}
				sprintf( fileName, "log%03d.vault", saveVaultNumber );
				saveVaultNumber++;

				char buf[64];
				sprintf(buf, "Saving log %d", saveVaultNumber - 1);
				//this->addChatMsg(-1, buf);
				AnnalVault::instance().save( fileName );
				//this->addChatMsg(-1, "Save finished");
			}
			else
			{
				static int loadVaultNumber = 0;
				static int initMFWatch = false;
				if (!initMFWatch) {
					MF_WATCH( "Client Settings/Movie/loadVaultNum", loadVaultNumber );
					initMFWatch = false;
				}

				sprintf( fileName, "log%03d.vault", loadVaultNumber );
				loadVaultNumber++;

				char buf[64];
				sprintf(buf, "Loading log %d", loadVaultNumber - 1);
				//this->addChatMsg(-1, buf);
				AnnalVault::instance().load( fileName );
				//this->addChatMsg(-1, "Load finished");
			}
		}
		break;

	case KeyEvent::KEY_JOYB:
		ConsoleManager::instance().toggle( "Statistics" );
		break;
	case KeyEvent::KEY_JOYX:
		ConsoleManager::instance().toggle( "Watcher" );
		break;
	case KeyEvent::KEY_JOYY:
		PyOutputWriter::flush();
		ConsoleManager::instance().toggle( "Python" );
		break;
	case KeyEvent::KEY_JOYLTRIGGER:
		if (InputDevices::isKeyDown( KeyEvent::KEY_JOYRTRIGGER ))
		{
			DEBUG_MSG( "Reloading entity script classes...\n" );
			XConsole * pConsole = ConsoleManager::instance().find( "Python" );

			if (EntityType::reload())
			{
				DEBUG_MSG( "Script reload successful!\n" );
				pConsole->print( "App: Script reload succeeded.\n" );
			}
			else
			{
				DEBUG_MSG( "Script reload failed.\n" );
				pConsole->print( "App: Script reload failed.\n" );

				if (PyErr_Occurred())
				{
					PyErr_PrintEx(0);
					PyErr_Clear();
				}
			}
		}
		else
		{
			handled = false;
		}
		break;
	case KeyEvent::KEY_JOYDUP:
	{
		EnviroMinder & enviro =
			ChunkManager::instance().cameraSpace()->enviro();
		enviro.timeOfDay()->gameTime(
			enviro.timeOfDay()->gameTime() + 0.5f );
		break;
	}
	case KeyEvent::KEY_JOYDDOWN:
	{
		EnviroMinder & enviro =
			ChunkManager::instance().cameraSpace()->enviro();
		enviro.timeOfDay()->gameTime(
			enviro.timeOfDay()->gameTime() - 0.5f );
		break;
	}
	case KeyEvent::KEY_JOYARPUSH:
	case KeyEvent::KEY_F:	// F for flush
	{
		// remember the camera's chunk
		Chunk * pCameraChunk = ChunkManager::instance().cameraChunk();
		if (pCameraChunk)
		{
			ChunkDirMapping * pCameraDirMapping = pCameraChunk->mapping();
			ChunkSpacePtr pSpace = pCameraChunk->space();

			// eject every chunk in sight
			ChunkMap & chunks = pSpace->chunks();
			for (ChunkMap::iterator it = chunks.begin(); it != chunks.end(); it++)
			{
				for (uint i = 0; i < it->second.size(); i++)
				{
					Chunk * pChunk = it->second[i];
					if (pChunk->online())
					{
						BWResource::instance().purge( pChunk->resourceID() );
						pChunk->loose( false );
						pChunk->eject();
					}
				}
			}

			// now reload the camera chunk
			ChunkManager::instance().loadChunkExplicitly(
				pCameraChunk->identifier(), pCameraDirMapping );

			// and repopulate the flora
			Flora::floraReset();
		}

		break;
	}

	default:
		handled = false;
	}

	return handled;

#endif // ENABLE_DEBUG_KEY_HANDLER
}



/**
 *	This method overrides the InputHandler method to handle mouse events.
 *
 *	@param event	The current mouse event.
 *
 *	@return			True if the event was handled, false otherwise.
 */
bool App::handleMouseEvent( const MouseEvent & event )
{
	bool handled = false;

	// Let the current camera have first shot at it
	if (!handled)
	{
		handled = ClientCamera::instance().camera()->handleMouseEvent( event );
	}

	// Now give the personality script a go
	if (!handled)
	{
		// We give the personality script any movements
		PyObject * ret = Script::ask(
			PyObject_GetAttrString( Personality::instance(), "handleMouseEvent" ),
			Script::getData( event ),
			"Personality handleMouseEvent: " );

		Script::setAnswer( ret, handled,
			"Personality Script handleMouseEvent retval" );
	}

	// And finally the active cursor gets its turn
	if (!handled && activeCursor_ != NULL)
	{
		handled = activeCursor_->handleMouseEvent( event );
	}

	return true;
}



/**
 *	This method overrides the InputHandler method to joystick axis events.
 *
 *	@return			True if the event was handled, false otherwise.
 */
bool App::handleAxisEvent( const AxisEvent & event )
{
	bool handled = false;

	if (InputDevices::joystick().isUsingKeyboard()) return false;

	// The debug consoles get in first
	if (!handled)
	{
		handled = ConsoleManager::instance().handleAxisEvent( event );
	}

	// Now give the personality script a go, if it ever needs this
	if (!handled)
	{
		PyObject * ret = Script::ask(
			PyObject_GetAttrString( Personality::instance(), "handleAxisEvent" ),
			Script::getData( event ),
			"Personality handleAxisEvent: " );
		Script::setAnswer( ret, handled, "Personality handleAxisEvent retval" );
	}

	// The current camera is next
	if (!handled)
	{
		handled = ClientCamera::instance().camera()->handleAxisEvent( event );
	}

	// And finally the active cursor gets its turn
	if (!handled && activeCursor_ != NULL)
	{
		handled = activeCursor_->handleAxisEvent( event );
	}

	// Physics gets anything that's left
	if (!handled)
	{
		handled = Physics::handleAxisEventAll( event );
	}

	return true;
}


/**
 *	Returns the current active cursor.
 */
InputCursor * App::activeCursor()
{
	return this->activeCursor_;
}


/**
 *	Sets the active cursor. The active cursor will get all mouse,
 *	keyboard and axis events forwarded to it. Only one cursor can
 *	be active at a time. The deactivate method will be called on
 *	the current active cursor (the one being deactivated), if any.
 *	The activate method will be called on the new active cursor.
 *	if any.
 *
 *	@param	cursor	the new active cursor, or NULL to simply
 *					deactivate the current one.
 *
 */
void App::activeCursor( InputCursor * cursor )
{
	if (this->activeCursor_)
	{
		this->activeCursor_->deactivate();
	}

	this->activeCursor_ = cursor;

	if (this->activeCursor_)
	{
		this->activeCursor_->activate();
		if ( InputDevices::instance().hasFocus() )
			activeCursor_->focus( true );
	}
}


bool App::savePreferences()
{
	return DeviceApp::instance.savePreferences();
}

// -----------------------------------------------------------------------------
// Section: Window Events
// -----------------------------------------------------------------------------


/**
 *	This method is called when the window has resized. It is called in response
 *	to the Windows event.
 */
void App::resizeWindow( void )
{
	if( Moo::rc().windowed() == true )
	{
		Moo::rc().changeMode( Moo::rc().modeIndex(), Moo::rc().windowed() );
	}
}


void App::resizeWindow(int width, int height)
{
	if (Moo::rc().device() == NULL || Moo::rc().windowed())
	{
		RECT	clientRect;
		GetClientRect( hWnd_, &clientRect );

		RECT	borderRect;
		GetWindowRect( hWnd_, &borderRect );

		POINT	border;
		border.x = (borderRect.right - borderRect.left) - clientRect.right;
		border.y = (borderRect.bottom - borderRect.top) - clientRect.bottom;

		MoveWindow(
			hWnd_, borderRect.left, borderRect.top,
			width + border.x, height + border.y, true );

		this->windowSize_.x = width;
		this->windowSize_.y = height;
	}
}

const POINT & App::windowSize() const
{
	return this->windowSize_;
}

/**
 *	This method is called when the window has moved. It is called in response to
 *	the Windows event and is important for DirectX.
 */
void App::moveWindow( int16 x, int16 y )
{
	//Moo::rc().device()->moveFrame( x, y );
}


/**
 *	This method is called when the application gets the focus.
 */
void App::handleSetFocus( bool state )
{
	DEBUG_MSG("App::handleSetFocus: %d\n", state);
	InputDevices::setFocus( state );
	if (pInstance_ != NULL && activeCursor_ != NULL)
	{
		activeCursor_->focus( state );
	}

	Moo::RenderContext::instance().restoreCursor(!state);

	if ( Moo::rc().device() && !Moo::rc().windowed())
		Moo::rc().changeMode( Moo::rc().modeIndex(), true );
}


extern const char * APP_TITLE;

/**
 *	This function sets (or clears, if the input string is empty)
 *	the title note identified by 'pos'
 */
void App::setWindowTitleNote( int pos, const std::string & note )
{
	if (note.empty())
	{
		TitleNotes::iterator it = titleNotes_.find( pos );
		if (it != titleNotes_.end()) titleNotes_.erase( it );
	}
	else
	{
		titleNotes_[ pos ] = note;
	}

	std::string newTitle = APP_TITLE;
	if (!titleNotes_.empty())
	{
		int n = 0;
		for (TitleNotes::iterator it = titleNotes_.begin();
			it != titleNotes_.end();
			it++, n++)
		{
			newTitle += n ? ", " : " [";
			newTitle += it->second;
		}
		newTitle += "]";
	}

	SetWindowText( hWnd_, newTitle.c_str() );
}


// -----------------------------------------------------------------------------
// Section: Statistics and Debugging
// -----------------------------------------------------------------------------

/**
 *	This method calculates the time between this frame and last frame.
 */
void App::calculateFrameTime()
{
	uint64 thisTime = frameTimerValue();

	// Need to be careful of the wraparound case here.
	FRAME_TIMER_TYPE delta = FRAME_TIMER_TYPE( thisTime - lastTime_ );

	dTime_ = float( delta / frameTimerFreq() );

	if (DebugApp::instance.slowTime_ > 0.000001f)
		dTime_ /= DebugApp::instance.slowTime_;

	totalTime_ += dTime_;
	lastTime_ = thisTime;
}


// -----------------------------------------------------------------------------
// Section: Miscellaneous App methods
// -----------------------------------------------------------------------------



/// Make sure the Python object ring hasn't been corrupted
void App::checkPython()
{
#ifdef Py_DEBUG
	PyObject* head = PyInt_FromLong(1000000);
	PyObject* p = head;

	INFO_MSG("App::checkPython: checking python...\n");

	while(p && p->_ob_next != head)
	{
		if((p->_ob_prev->_ob_next != p) || (p->_ob_next->_ob_prev != p))
		{
			CRITICAL_MSG("App::checkPython: Python object %0.8X is screwed\n", p);
		}
		p = p->_ob_next;
	}

	Py_DECREF(head);
	INFO_MSG("App::checkPython: done..\n");
#endif
}





/**
 *	Returns whether or not the camera is outside
 */
bool isCameraOutside()
{
	Chunk * pCC = ChunkManager::instance().cameraChunk();
	return pCC == NULL || pCC->isOutsideChunk();
}

/**
 *	Returns whether or not the player is outside
 */
bool isPlayerOutside()
{
	Entity * pPlayer;
	if ((pPlayer = Player::entity()) == NULL) return true;

	return pPlayer->pPrimaryEmbodiment() == NULL ||
		pPlayer->pPrimaryEmbodiment()->chunk() == NULL ||
		pPlayer->pPrimaryEmbodiment()->chunk()->isOutsideChunk();
}

/*~ function BigWorld quit
 *  Ask the application to quit.
 */
/**
 *	Ask the app to quit
 */
static void quit()
{
	App::instance().quit();
}
PY_AUTO_MODULE_FUNCTION( RETVOID, quit, END, BigWorld )

/*~ function BigWorld.playMovie
 *	@components{ client }
 *
 *	Placeholder for deprecated functionality.
 */
static void playMovie()
{
	//not on PC
}
PY_AUTO_MODULE_FUNCTION( RETVOID, playMovie, END, BigWorld )




/*~ function BigWorld timeOfDay
 *  Gets and sets the time of day in 24 hour time, as used by the environment
 *  system.
 *
 *  This can also be changed manually via the following key combinations:
 *
 *  CapsLock + "[":			Rewind time of day by 10 minutes
 *
 *  CapsLock + Shift + "[":	Rewind time of day by 1 hour
 *
 *  CapsLock + "]":			Advance time of day by 10 minutes
 *
 *  CapsLock + Shift + "]":	Advance time of day by 1 hour
 *
 *  @param time Optional string. If provided, the time of day is set to this.
 *	This can be in the format "hour:minute" (eg "01:00", "1:00", "1:0", "1:"
 *  ), or as a string representation of a float (eg "1", "1.0"). Note that an
 *  incorrectly formatted string will not throw an exception, nor will it
 *  change the time of day.
 *  @return A string representing the time of day at the end of the function
 *  call in the form "hh:mm" (eg "01:00" ).
 */
/**
 *	Function to let scripts set the time of day
 */
const std::string & timeOfDay( const std::string & tod )
{
	ChunkSpacePtr cameraSpace = ChunkManager::instance().cameraSpace();
	if (!cameraSpace.exists())
	{
		PyErr_Format(
			PyExc_EnvironmentError,
			"Could not access space to get EnviroMinder" );
		static const std::string empty;
		return empty;
	}

	EnviroMinder & enviro = cameraSpace->enviro();
	if (tod.size())
	{
		enviro.timeOfDay()->setTimeOfDayAsString(tod);
	}
	return enviro.timeOfDay()->getTimeOfDayAsString();
}
PY_AUTO_MODULE_FUNCTION(
	RETDATA, timeOfDay, OPTARG( std::string, "", END ), BigWorld )

/*~ function BigWorld spaceTimeOfDay
 *  Gets and sets the time of day in 24 hour time, as used by the environment
 *  system.
 *	@param spaceID The spaceID of the space to set/get the time.
 *  @param time Optional string. If provided, the time of day is set to this.
 *	This can be in the format "hour:minute" (eg "01:00", "1:00", "1:0", "1:"
 *  ), or as a string representation of a float (eg "1", "1.0"). Note that an
 *  incorrectly formatted string will not throw an exception, nor will it
 *  change the time of day.
 *  @return A string representing the time of day at the end of the function
 *  call in the form "hh:mm" (eg "01:00" ).
 */
/**
 *	Function to let scripts set the time of day for a given space.
 */
const std::string & spaceTimeOfDay( ChunkSpaceID spaceID, const std::string & tod )
{
	ChunkSpacePtr pSpace = ChunkManager::instance().space( spaceID );

	if ( pSpace )
	{
		EnviroMinder & enviro = pSpace->enviro();
		if (tod.size())
		{
			enviro.timeOfDay()->setTimeOfDayAsString(tod);
		}

		return enviro.timeOfDay()->getTimeOfDayAsString();
	}

	static std::string s_nullSpaceTime = "00:00";
	return s_nullSpaceTime;
}
PY_AUTO_MODULE_FUNCTION(
	RETDATA, spaceTimeOfDay, ARG( SpaceID, OPTARG( std::string, "", END )), BigWorld )



/*~	attribute BigWorld.platform
 *
 *	This is a string that represents which platform the client is running on,
 *	for example, "windows".
 */
/// Add the platform name attribute
PY_MODULE_ATTRIBUTE( BigWorld, platform, Script::getData( "windows" ) )
