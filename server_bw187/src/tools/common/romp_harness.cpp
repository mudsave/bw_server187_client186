/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

//---------------------------------------------------------------------------
#include "pch.hpp"
#include "cstdmf/debug.hpp"
#include "resmgr/bwresource.hpp"
#include "romp_harness.hpp"
#include "romp/fog_controller.hpp"
#include "romp/lens_effect_manager.hpp"
#include "romp/engine_statistics.hpp"
#include "romp/console_manager.hpp"
#include "romp/rain.hpp"
#include "romp/sky.hpp"
#include "romp/time_of_day.hpp"
#include "romp/weather.hpp"
#include "romp/bloom_effect.hpp"
#include "romp/heat_shimmer.hpp"
#include "romp/full_screen_back_buffer.hpp"
#include "romp/geometrics.hpp"
#include "romp/enviro_minder.hpp"
#include "moo/render_context.hpp"
#include "moo/moo_math.hpp"
#include "moo/visual_channels.hpp"
#include "romp/water.hpp"
#include "romp/sky_gradient_dome.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk.hpp"
#include "gizmo/tool_manager.hpp"
#include "appmgr/options.hpp"
#include "romp/histogram_provider.hpp"

#ifndef STATIC_WATER
#include "editor_chunk_water.hpp"
#endif

PY_TYPEOBJECT( RompHarness )

PY_BEGIN_METHODS( RompHarness )
	PY_METHOD( setTime )
	PY_METHOD( setSecondsPerHour )
	PY_METHOD( setRainAmount )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( RompHarness )
	PY_ATTRIBUTE( fogEnable )
PY_END_ATTRIBUTES()

DECLARE_DEBUG_COMPONENT2( "WorldEditor", 2 )

RompHarness::RompHarness( PyTypePlus * pType ) :
	PyObjectPlus( pType ),
    dTime_( 0.033f ),
    inited_( false ),
	bloom_( NULL ),
	useBloom_( true ),	
	shimmer_( NULL ),
	useShimmer_( true )	
{
	waterMovement_[0] = Vector3(0,0,0);
	waterMovement_[1] = Vector3(0,0,0);

	if ( ChunkManager::instance().cameraSpace() )
	{
		MF_WATCH_REF( "Client Settings/Time of Day", *enviroMinder().timeOfDay(),
			&TimeOfDay::getTimeOfDayAsString, &TimeOfDay::setTimeOfDayAsString );
		MF_WATCH( "Client Settings/Secs Per Hour", *enviroMinder().timeOfDay(),
			MF_ACCESSORS( float, TimeOfDay, secondsPerGameHour ) );
	}
}

RompHarness::~RompHarness()
{
	if (bloom_)
	{
		delete bloom_;
		bloom_ = NULL;
	}

	if (shimmer_)
	{
		delete shimmer_;
		shimmer_=NULL;
	}

	Waters::instance().fini();
}


bool
RompHarness::init()
{
    inited_ = true;

	setSecondsPerHour( 0.f );
	DataSectionPtr pWatcherSection = Options::pRoot()->openSection( "romp/watcherValues" );
	if (pWatcherSection)
	{
		pWatcherSection->setWatcherValues();
	}

	if ( HeatShimmer::isSupported() )
	{
		shimmer_ = new HeatShimmer;
		if ( !shimmer_->init() )
		{			
			ERROR_MSG( "Shimmer failed to initialise\n" );
			delete shimmer_;
			shimmer_ = NULL;
		}
	}
	else
	{
		ERROR_MSG( "Heat Shimmer is not supported\n" );		
	}

	if ( Bloom::isSupported() )
	{
		bloom_ = new Bloom();
		if ( !bloom_->init() )
		{			
			ERROR_MSG( "Bloom failed to initialise\n" );
			delete bloom_;
			bloom_ = NULL;
		}
	}
	else
	{
		ERROR_MSG( "Blooming is not supported\n" );		
	}	

	Waters::instance().init();

	return true;
}

void RompHarness::changeSpace()
{
	setSecondsPerHour( 0.f );
}

void RompHarness::initWater( DataSectionPtr pProject )
{
}


void RompHarness::setTime( float t )
{
	enviroMinder().timeOfDay()->gameTime( t );
}


void RompHarness::setSecondsPerHour( float sph )
{
	enviroMinder().timeOfDay()->secondsPerGameHour( sph );
}


void RompHarness::setRainAmount( float r )
{
	enviroMinder().rain()->amount( r );
}


void RompHarness::update( float dTime, bool globalWeather )
{
	dTime_ = dTime;

	if ( inited_ )
    {
		this->fogEnable( !!Options::getOptionInt( "render/environment/drawFog", 0 ));

		Chunk * pCC = ChunkManager::instance().cameraChunk();
		bool outside = (pCC == NULL || pCC->isOutsideChunk());
		enviroMinder().tick( dTime_, outside );
        FogController::instance().tick();        
    }

    //Intersect tool with water
	this->disturbWater();
}

void RompHarness::disturbWater()
{
#ifndef STATIC_WATER
	if ( ToolManager::instance().tool() && ToolManager::instance().tool()->locator() )
	{
		waterMovement_[0] = waterMovement_[1];
		waterMovement_[1] = ToolManager::instance().tool()->locator()->
			transform().applyToOrigin();

		const std::vector<EditorChunkWater*>& waterItems = EditorChunkWater::instances();
		std::vector<EditorChunkWater*>::const_iterator i = waterItems.begin();
		for (; i != waterItems.end(); ++i)
		{
			(*i)->sway( waterMovement_[0], waterMovement_[1], 1.f );
		}

		//s_phloem.addMovement( waterMovement_[0], waterMovement_[1] );
	}
#endif
}



void RompHarness::drawPreSceneStuff( bool sparkleCheck /* = false */, bool renderEnvironment /* = true */ )
{
	useShimmer_ = !!Options::getOptionInt("render/environment/drawShimmer", 1 );
	useBloom_ = !!Options::getOptionInt("render/environment/drawBloom", 1 );

	//set the actual values
	Options::setOptionInt("render/environment/drawShimmer", useShimmer_ ? 1 : 0 );
	Options::setOptionInt("render/environment/drawBloom",  useBloom_ ? 1 : 0 );

	useShimmer_ = !sparkleCheck && useShimmer_;
	useBloom_ = !sparkleCheck && useBloom_;

	//and modify useBloom temporarily.
	useShimmer_ &= !!Options::getOptionInt("render/environment", 0 );
	useBloom_ &= !!Options::getOptionInt("render/environment", 0 );

	if (bloom_)
		bloom_->setEditorEnabled( useBloom_ );

	if (shimmer_)
		shimmer_->setEditorEnabled( useShimmer_ );

	FullScreenBackBuffer::beginScene();	

	EnviroMinder::DrawSelection flags;
	flags.value = 0;
	flags.value |= ( Options::getOptionInt(
		"render/environment/drawSunAndMoon", 0 ) ? EnviroMinder::DrawSelection::sunAndMoon : 0 );
	flags.value |= ( Options::getOptionInt(
		"render/environment/drawSky", 0 ) ? EnviroMinder::DrawSelection::skyGradient : 0 );
	flags.value |= ( Options::getOptionInt(
		"render/environment/drawClouds", 0 ) ? EnviroMinder::DrawSelection::clouds : 0 );
	flags.value |= ( Options::getOptionInt(
		"render/environment/drawSunAndMoon", 0 ) ? EnviroMinder::DrawSelection::sunFlare : 0 );

	flags = renderEnvironment && !!Options::getOptionInt( "render/environment", 1 ) ? flags : 0;

	enviroMinder().drawHind( dTime_, flags, renderEnvironment );
}


void RompHarness::drawDelayedSceneStuff( bool renderEnvironment /* = true */ )
{
	EnviroMinder::DrawSelection flags;
	flags.value = 0;
	flags.value |= ( Options::getOptionInt(
		"render/environment/drawSunAndMoon", 0 ) ? EnviroMinder::DrawSelection::sunAndMoon : 0 );
	flags.value |= ( Options::getOptionInt(
		"render/environment/drawSky", 0 ) ? EnviroMinder::DrawSelection::skyGradient : 0 );
	flags.value |= ( Options::getOptionInt(
		"render/environment/drawClouds", 0 ) ? EnviroMinder::DrawSelection::clouds : 0 );
	flags.value |= ( Options::getOptionInt(
		"render/environment/drawSunAndMoon", 0 ) ? EnviroMinder::DrawSelection::sunFlare : 0 );
	flags = renderEnvironment && !!Options::getOptionInt( "render/environment", 1 ) ? flags : 0;

	enviroMinder().drawHindDelayed( dTime_, flags );
}


void RompHarness::drawPostSceneStuff( bool showWeather /* = true */, bool showFlora /* = true */, bool showFloraShadowing /* = false */ )
{
	// update any dynamic textures
	TextureRenderer::updateCachableDynamics( dTime_ );	

	bool drawWater = Options::getOptionInt( "render/environment", 1 ) &&
		Options::getOptionInt( "render/environment/drawWater", 1 );
	Waters::drawWaters( drawWater );

	bool drawWaterRefraction = drawWater &&
		Options::getOptionInt( "render/environment/drawWater/refraction", 1 );
	Waters::drawRefraction( drawWaterRefraction );

	bool drawWaterReflection = drawWater &&
		Options::getOptionInt( "render/environment/drawWater/reflection", 1 );
	Waters::drawReflection( drawWaterReflection );

	bool drawWaterSimulation = drawWater &&
		Options::getOptionInt( "render/environment/drawWater/simulation", 1 );
	Waters::simulationEnabled( drawWaterSimulation );

	bool drawWire = Options::getOptionInt( "render/scenery/wireFrame", 0 )==1;
	Waters::instance().drawWireframe(drawWire);

	if( drawWater )
	{
		Waters::instance().rainAmount( enviroMinder().rain()->amount() );
		Waters::instance().drawDrawList( dTime_ );
	}

	enviroMinder().drawFore( dTime_, showWeather, showFlora, showFloraShadowing );

	Moo::SortedChannel().draw();

	LensEffectManager::instance().tick( dTime_ );	

	FullScreenBackBuffer::endScene();	

	HistogramProvider::instance().update();	

	LensEffectManager::instance().draw();
}


void RompHarness::propensity( const std::string& weatherSystemName, float amount )
{
	if ( enviroMinder().weather() )
    {
    	WeatherSystem * ws = enviroMinder().weather()->system( weatherSystemName );
        if ( ws )
        {
        	float args[4];
            args[0] = 1.f;
            args[1] = 1.f;
            args[2] = 1.f;
            args[3] = 1.f;
        	ws->direct( amount, args, 0.f );
        }
    }
}


TimeOfDay* RompHarness::timeOfDay() const
{
	return enviroMinder().timeOfDay();
}

EnviroMinder& RompHarness::enviroMinder() const
{
	return ChunkManager::instance().cameraSpace()->enviro();
}

/**
 *	Get an attribute for python
 */
PyObject * RompHarness::pyGetAttribute( const char * attr )
{
	// try our normal attributes
	PY_GETATTR_STD();

	// ask our base class
	return PyObjectPlus::pyGetAttribute( attr );
}


/**
 *	Set an attribute for python
 */
int RompHarness::pySetAttribute( const char * attr, PyObject * value )
{
	// try our normal attributes
	PY_SETATTR_STD();

	// ask our base class
	return PyObjectPlus::pySetAttribute( attr, value );
}


/**
 *	The (non-static) python setTime method
 */
PyObject * RompHarness::py_setTime( PyObject * args )
{
	float t;

	if (!PyArg_ParseTuple( args, "f", &t ))
	{
		PyErr_SetString( PyExc_TypeError, "RompHarness.setTime() "
			"expects a float time" );
		return NULL;
	}

	enviroMinder().timeOfDay()->gameTime( t );

	Py_Return;
}


/**
 *	The (non-static) python setSecondsPerHour method
 */
PyObject * RompHarness::py_setSecondsPerHour( PyObject * args )
{
	float t;

	if (!PyArg_ParseTuple( args, "f", &t ))
	{
		PyErr_SetString( PyExc_TypeError, "RompHarness.setSecondsPerHour() "
			"expects a float time" );
		return NULL;
	}

	enviroMinder().timeOfDay()->secondsPerGameHour( t );

	Py_Return;
}


/**
 *	The (non-static) python setRainAmount method
 */
PyObject * RompHarness::py_setRainAmount( PyObject * args )
{
	float a;

	if (!PyArg_ParseTuple( args, "f", &a ))
	{
		PyErr_SetString( PyExc_TypeError, "RompHarness.setRainAmount() "
			"expects a float amount between 0 and 1" );
		return NULL;
	}

	enviroMinder().rain()->amount( a );

	Py_Return;
}


/**
 *	This method enables or disables global fogging
 */
void RompHarness::fogEnable( bool state )
{
	FogController::instance().enable( state );

	Options::setOptionInt( "render/environment/drawFog", state ? 1 : 0 );
}


/**
 *	This method returns the global fogging state.
 */
bool RompHarness::fogEnable() const
{
	return FogController::instance().enable();
}

//---------------------------------------------------------------------------
