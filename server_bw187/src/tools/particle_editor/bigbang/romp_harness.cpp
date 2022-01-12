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
#include "romp/sky_gradient_dome.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_space.hpp"
#include "gizmo/tool_manager.hpp"
#include "appmgr/options.hpp"

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
    enviroMinder_( NULL ),
    dTime_( 0.033f ),
    inited_( false ),
	bloom_( NULL ),
	useBloom_( true ),
	shimmer_( NULL ),
	useShimmer_( true )
{

	if ( ChunkManager::instance().cameraSpace() )
	{
		enviroMinder_ = &ChunkManager::instance().cameraSpace()->enviro();

		MF_WATCH_REF( "Client Settings/Time of Day", *enviroMinder_->timeOfDay(),
			&TimeOfDay::getTimeOfDayAsString, &TimeOfDay::setTimeOfDayAsString );
		MF_WATCH( "Client Settings/Secs Per Hour", *enviroMinder_->timeOfDay(),
			MF_ACCESSORS( float, TimeOfDay, secondsPerGameHour ) );
	}
}


bool
RompHarness::init()
{
    inited_ = true;

	DataSectionPtr pWatcherSection = Options::pRoot()->openSection( "romp/watcherValues" );
	if (pWatcherSection)
	{
		pWatcherSection->setWatcherValues();
	}

	if ( HeatShimmer::isSupported() )
	{
		shimmer_ = new HeatShimmer;
		if (!shimmer_->init())
		{
			ERROR_MSG( "Shimmer failed to initialise\n" );
			Options::pRoot()->writeInt("render/misc/drawShimmer", 0 );
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
			Options::pRoot()->writeInt("render/misc/drawBloom", 0 );
			delete bloom_;
			bloom_ = NULL;
		}
	}
	else
	{
		ERROR_MSG( "Blooming is not supported\n" );		
	}

	enviroMinder_->activate();

	return true;
}


void RompHarness::setTime( float t )
{
	enviroMinder_->timeOfDay()->gameTime( t );
}


void RompHarness::setSecondsPerHour( float sph )
{
	enviroMinder_->timeOfDay()->secondsPerGameHour( sph );
}


void RompHarness::setRainAmount( float r )
{
	enviroMinder_->rain()->amount( r );
}


void RompHarness::update( float dTime, bool globalWeather )
{
	dTime_ = dTime;

	if ( inited_ )
    {
		this->fogEnable(
			Options::pRoot()->readInt( "render/misc/drawFog", 1 ) != 0 );
		enviroMinder_->tick( dTime_, true );
        FogController::instance().tick();        
    }
}


void RompHarness::drawPreSceneStuff()
{
	useShimmer_ = !!Options::pRoot()->readInt("render/misc/drawShimmer", 1 );
	useBloom_ = !!Options::pRoot()->readInt("render/misc/drawBloom", 1 );

	if (bloom_)
		bloom_->setEditorEnabled( useBloom_ );

	if (shimmer_)
		shimmer_->setEditorEnabled( useShimmer_ );

	FullScreenBackBuffer::beginScene();	

	EnviroMinder::DrawSelection flags;
	flags.value = 0;
	flags.value |= ( Options::pRoot()->readInt(
		"render/misc/drawSunAndMoon", 0 ) ? EnviroMinder::DrawSelection::sunAndMoon : 0 );
	flags.value |= ( Options::pRoot()->readInt(
		"render/misc/drawSky", 0 ) ? EnviroMinder::DrawSelection::skyGradient : 0 );
	flags.value |= ( Options::pRoot()->readInt(
		"render/misc/drawClouds", 0 ) ? EnviroMinder::DrawSelection::clouds : 0 );
	flags.value |= ( Options::pRoot()->readInt(
		"render/misc/drawSunAndMoon", 0 ) ? EnviroMinder::DrawSelection::sunFlare : 0 );

	enviroMinder_->drawHind( dTime_, flags );
}


void RompHarness::drawDelayedSceneStuff()
{
	EnviroMinder::DrawSelection flags;
	flags.value = 0;
	
	flags.value |= ( Options::pRoot()->readInt(
		"render/misc/drawSunAndMoon", 0 ) ? EnviroMinder::DrawSelection::sunAndMoon : 0 );
	flags.value |= ( Options::pRoot()->readInt(
		"render/misc/drawSky", 0 ) ? EnviroMinder::DrawSelection::skyGradient : 0 );
	flags.value |= ( Options::pRoot()->readInt(
		"render/misc/drawClouds", 0 ) ? EnviroMinder::DrawSelection::clouds : 0 );
	flags.value |= ( Options::pRoot()->readInt(
		"render/misc/drawSunAndMoon", 0 ) ? EnviroMinder::DrawSelection::sunFlare : 0 );

	enviroMinder_->drawHindDelayed( dTime_, flags );
}


void RompHarness::drawPostSceneStuff()
{
	enviroMinder_->drawFore( dTime_ );

	Moo::SortedChannel().draw();

	LensEffectManager::instance().tick( dTime_ );

	FullScreenBackBuffer::endScene();

	LensEffectManager::instance().draw();
}


void RompHarness::propensity( const std::string& weatherSystemName, float amount )
{
	if ( enviroMinder_->weather() )
    {
    	WeatherSystem * ws = enviroMinder_->weather()->system( weatherSystemName );
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
	return enviroMinder_->timeOfDay();
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

	enviroMinder_->timeOfDay()->gameTime( t );

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

	enviroMinder_->timeOfDay()->secondsPerGameHour( t );

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

	enviroMinder_->rain()->amount( a );

	Py_Return;
}


/**
 *	This method enables or disables global fogging
 */
void RompHarness::fogEnable( bool state )
{
	FogController::instance().enable( state );

	Options::pRoot()->writeInt( "render/misc/drawFog", state ? 1 : 0 );
}


/**
 *	This method returns the global fogging state.
 */
bool RompHarness::fogEnable() const
{
	return FogController::instance().enable();
}

//---------------------------------------------------------------------------
