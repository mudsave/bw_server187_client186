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
#include "enviro_minder.hpp"

#ifndef CODE_INLINE
#include "enviro_minder.ipp"
#endif

#include "cstdmf/debug.hpp"
DECLARE_DEBUG_COMPONENT2( "Romp", 0 )

#include "resmgr/datasection.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/auto_config.hpp"

#include "chunk/chunk_manager.hpp"
#include "particle/particle_system.hpp"

#include "time_of_day.hpp"
#include "weather.hpp"
#include "sky_gradient_dome.hpp"
#include "sun_and_moon.hpp"
#include "clouds.hpp"
#include "sky.hpp"
#include "sea.hpp"
#include "rain.hpp"
#include "snow.hpp"
#include "flora.hpp"
#include "water.hpp"
#include "flora_renderer.hpp"
#include "sky_light_map.hpp"
#include "sky_dome_occluder.hpp"
#include "lens_effect_manager.hpp"

#include <limits>

//TODO : unsupported feature at the moment.  to be finished.
//#include "dapple.hpp"

#define SKY_LIGHT_MAP_ENABLE 1

#ifdef EDITOR_ENABLED
	extern bool g_disableSkyLightMap;
#endif

#include "math/colour.hpp"

static AutoConfigString s_floraXML( "environment/floraXML" );

namespace
{
    /**
    *	A little helper function that should go in cstdmf
    */
    template <class C> inline void del_safe( C * & object )
    {
	    if (object != NULL)
	    {
		    delete object;
		    object = NULL;
	    }
    }
	
	// For drawing background items, the viewport minimum Z value is pushed
	// out towards 1.
	float backgroundVPMinZ = 0.9f;

	class WatcherInitializer
	{
	public:
		WatcherInitializer()
		{
			MF_WATCH( "Render/backgroundVPMinZ", backgroundVPMinZ, 
				Watcher::WT_READ_WRITE,
				"MinZ value in viewport for background rendering" );
		}
	};

	WatcherInitializer s_watcherInitializer;
}

// -----------------------------------------------------------------------------
// Section: PlayerAttachments
// -----------------------------------------------------------------------------


/**
 *	This method adds a wannabe attachment to our list
 */
void PlayerAttachments::add( PyMetaParticleSystemPtr pSys, const std::string & node )
{
	PlayerAttachment pa;
	pa.pSystem = pSys;
	pa.onNode = node;
	this->push_back( pa );
}


// -----------------------------------------------------------------------------
// Section: EnviroMinder
// -----------------------------------------------------------------------------

EnviroMinder* EnviroMinder::s_activatedEM_ = NULL;

/**
 *	Constructor.
 */
EnviroMinder::EnviroMinder() :
	timeOfDay_( new TimeOfDay( 0.f ) ),
	weather_( new Weather() ),
	skyGradientDome_( new SkyGradientDome() ),
	sunAndMoon_( new SunAndMoon() ),
	clouds_( new Clouds() ),
	sky_( new Sky() ),
	seas_( new Seas() ),
	rain_( new Rain() ),
	snow_( new Snow() ),
	flora_( new Flora() ),
	//TODO : unsupported feature at the moment.  to be finished.
	//dapple_( new Dapple() ),
	thunder_( 0.f, 0.f, 0.f, 0.f ),
	playerDead_( false ),
	data_( NULL ),
	skyDomeOccluder_( NULL )
{
	// Create Sky Light Map
#ifdef SKY_LIGHT_MAP_ENABLE
	skyLightMap_ = new SkyLightMap;
#endif
}


/**
 *	Destructor.
 */
EnviroMinder::~EnviroMinder()
{
	del_safe( skyLightMap_ );
	del_safe( snow_ );
	del_safe( rain_ );
	del_safe( seas_ );
	del_safe( clouds_ );
	del_safe( sky_ );
	del_safe( sunAndMoon_ );
	del_safe( skyGradientDome_ );
	Py_XDECREF( weather_ );	weather_ = NULL;
	del_safe( timeOfDay_ );
	del_safe( flora_ );
	del_safe( skyDomeOccluder_ );
	//TODO : unsupported feature at the moment.  to be finished.
	//del_safe( dapple_ );
}


/**
 *	Load method. Any errors are handled internally, even if loading fails.
 *	This method must be called before the environment classes can be used.
 */
bool EnviroMinder::load( DataSectionPtr pDS, bool loadFromExternal /*= true*/ )
{
	data_ = pDS;
	float farPlane = 500.f;

	// initialise ourselves from the space.settings or project file
	if (pDS)
	{
        loadTimeOfDay(pDS, loadFromExternal);

        loadSkyGradientDome(pDS, loadFromExternal, farPlane);

		// Load Seas
		seas_->clear();
		DataSectionPtr spSeas = pDS->openSection( "seas" );
		if (spSeas)
		{
			for (DataSectionIterator it = spSeas->begin();
				it != spSeas->end();
				it++)
			{
				Sea * ns = new Sea();
				ns->load( *it );
				seas_->push_back( ns );
			}
		}

		// Load Sky Domes
		skyDomes_.clear();
		std::vector<DataSectionPtr>	pSections;
		pDS->openSections( "skyDome", pSections );
		if (pSections.size())
		{
			for (std::vector<DataSectionPtr>::iterator it = pSections.begin();
				it != pSections.end();
				it++)
			{
				Moo::VisualPtr spSkyDome = Moo::VisualManager::instance()->get( (*it)->asString() );
				if (spSkyDome.hasObject())
					skyDomes_.push_back( spSkyDome );
			}
		}

		//Load flora
		std::string floraXML = pDS->readString( "flora", s_floraXML );
		DataSectionPtr spFlora = BWResource::instance().openSection( floraXML );
		flora_->init( spFlora );
	}

	sunAndMoon_->create();
	sunAndMoon_->timeOfDay( timeOfDay_ );	

	rain_->addAttachments( playerAttachments_ );
	snow_->addAttachments( playerAttachments_ );

	//note:farPlane may have been set in the Sky xml file.  ( more general )
	//that setting can be overridden in the space file.		( less general )
	farPlane = pDS->readFloat( "farPlane", farPlane );
    setFarPlane(farPlane);
	
	return true;
}


#ifdef EDITOR_ENABLED
/**
 *	Save method. Any errors are handled internally, even if saving fails.
 *  This can only be called after the load routine is called.
 */
bool EnviroMinder::save( DataSectionPtr pDS, bool saveToExternal /*= true*/ ) const
{
	// initialise ourselves from the space.settings or project file
	if (pDS)
	{
		// Save TimeOfDay
		if ( !todFile_.empty() )
	        pDS->writeString( "timeOfDay", todFile_ );
        DataSectionPtr pTODSect = NULL;
        if (saveToExternal)
		{
			if ( !todFile_.empty() )
		        pTODSect = BWResource::openSection( todFile_ );
			else
				ERROR_MSG( "EnviroMinder::save: Could not save Time Of Day because its file path is empty.\n" );
		}
        else
        {
            pTODSect = pDS;
        }
		if ( pTODSect )
		{
    		timeOfDay_->save( pTODSect );
			if (saveToExternal)
				pTODSect->save();
		}

		// Save SkyGradientDome
		if ( !sgdFile_.empty() )
		    pDS->writeString( "skyGradientDome", sgdFile_ );
        DataSectionPtr pSkyDomeSect = NULL;
        if (saveToExternal)
		{
			if ( !sgdFile_.empty() )
			    pSkyDomeSect = BWResource::openSection( sgdFile_ );
			else
				ERROR_MSG( "EnviroMinder::save: Could not save Sky Dome because its file path is empty.\n" );
		}
        else
        {
            pSkyDomeSect = pDS;
        }
		if ( pSkyDomeSect )
		{
			skyGradientDome_->save( pSkyDomeSect );
			pSkyDomeSect->writeFloat
			( 
				"farPlane", 
				EnviroMinderSettings::instance().farPlane()
			);
			if (saveToExternal)
				pSkyDomeSect->save();
		}

		// Save Seas
        if (seas_ != NULL && !seas_->empty())
        {
            pDS->deleteSections("seas");
		    DataSectionPtr spSeas = pDS->openSection( "seas", true );
            int idx = 0;
            for 
            (
                Seas::const_iterator it = seas_->begin();
                it != seas_->end();
                ++it, ++idx
            )
			{
                char buffer[64];
                sprintf(buffer, "sea_%d", idx);
                DataSectionPtr pSeaSect = BWResource::openSection( buffer );
				(*it)->save( pSeaSect );
            }
        }
        
		// Save Sky Domes:
        pDS->deleteSections("skyDome");
        for
        (
            std::vector<Moo::VisualPtr>::const_iterator it = skyDomes_.begin();
            it != skyDomes_.end();
            ++it
        )
        {
            DataSectionPtr skyDomeSection = pDS->newSection( "skyDome" );
            skyDomeSection->setString( (*it)->resourceID() );
		}
	}

	//note:farPlane may have been set in the Sky xml file.  ( more general )
	//that setting can be overridden in the space file.		( less general )
    pDS->writeFloat
    ( 
        "farPlane", 
        EnviroMinderSettings::instance().farPlane() 
    );
	
	return true;
}
#endif


/**
 *	This method ticks all the environment stuff.
 */
void EnviroMinder::tick( float dTime, bool outside,
	const WeatherSettings * pWeatherOverride )
{
	ParticleSystem::windVelocity( Vector3(
		weather_->settings().windX,
		0,
		weather_->settings().windY ) );

	weather_->tick( dTime );

	const WeatherSettings & forecast = (pWeatherOverride == NULL) ?
		weather_->settings() : *pWeatherOverride;

	// tell the clouds, rain and snow about the weather
	Vector3 sunDir =
		timeOfDay_->lighting().sunTransform.applyToUnitAxisVector(2);
	sunDir.x = -sunDir.x;
	sunDir.z = -sunDir.z;
	sunDir.normalise();
	uint32 sunCol = Colour::getUint32( timeOfDay_->lighting().sunColour );

	float sunAngle = 2.f * MATH_PI - DEG_TO_RAD( ( timeOfDay_->gameTime() / 24.f ) * 360 );		
	sky_->update( forecast, dTime, sunDir, sunCol, sunAngle );
	clouds_->update( forecast, dTime, sunDir, sunCol, sunAngle );
	rain_->update( forecast, outside );
	snow_->update( forecast, playerDead_ );

	// update the sun and moon positions and light/ambient/fog/etc colours
	timeOfDay_->tick( dTime );
	skyGradientDome_->update( timeOfDay_->gameTime() );

	this->decideLightingAndFog();

	// get the sky to decide what if any lightning it wants
	thunder_  = sky_->decideLightning( dTime );
	//TODO : ask sky and clouds for thunder activity
	//thunder_ = clouds_->decideLightning( dTime );

	// and tick the weather
	static DogWatch weatherTick("Weather");
	weatherTick.start();
	rain_->tick( dTime );
	snow_->tick( dTime );
	weatherTick.stop();

	//Update the flora
	flora_->update( dTime, *this );
}


/**
 *	This method is called when the environment is about to be used.
 *	This occurs when the camera enters a new space, or the engine
 *	is about to draw this space through some kind of portal.
 */
void EnviroMinder::activate()
{
	//Can only activate one enviro minder at a time.  If this assert
	//is hit, there is a coding error.
	MF_ASSERT( !s_activatedEM_ );
	s_activatedEM_ = this;

	if (flora_->pRenderer() == NULL)
	{
		ERROR_MSG("EnviroMinder::activate: Could not activate flora, the renderer is not initialised.");
	}
	else
	{	
		flora_->floraReset();
		flora_->pRenderer()->lightMap().activate();
	}

	//Set the far plane for this space
	if (data_)
	{
		float farPlane = data_->readFloat( "farPlane", 500.f );
		this->farPlane(farPlane);
		clouds_->activate(*this, data_);
		sky_->activate(*this, data_,skyLightMap_);

		//Set the sky light map and dapple map for this space
		if (skyLightMap_)
		{
			skyLightMap_->activate(*this,data_);
		}

		//TODO : unsupported feature at the moment.  to be finished.
		//dapple_->activate(*this,data_,skyLightMap_);

		//Hitting this assert means activate has been called twice in a row.bad
		MF_ASSERT( !skyDomeOccluder_ )
		if (SkyDomeOccluder::isAvailable())
		{
			skyDomeOccluder_ = new SkyDomeOccluder( *this );
			LensEffectManager::instance().addPhotonOccluder( skyDomeOccluder_ );
		}
		else
		{
			INFO_MSG( "Sky domes will not provide lens flare occlusion, "
				"because scissor rects are unsuppported on this card\n" );
		}
	}
}


/**
 *	This method is called when the environment is to be replaced
 *	by another active environment.
 */
void EnviroMinder::deactivate()
{
	//Must deactivate the current enviro minder before activating
	//the new one.  If this assert is hit, fix the code.
	MF_ASSERT( s_activatedEM_ == this );
	s_activatedEM_ = NULL;

	if (skyLightMap_)
	{
		skyLightMap_->deactivate(*this);
	}

	if (flora_ && flora_->pRenderer() != NULL)
	{
		flora_->pRenderer()->lightMap().deactivate();
	}	
	
	//TODO : unsupported feature at the moment.  to be finished.
	//dapple_->deactivate(*this, skyLightMap_);
	sky_->deactivate(*this, skyLightMap_);
	clouds_->deactivate(*this);

	if (skyDomeOccluder_)
	{
		LensEffectManager::instance().delPhotonOccluder( skyDomeOccluder_ );
		del_safe( skyDomeOccluder_ );		
	}
}

/**
 *	Sets the current camera far plane. Will try using the FAR_PLANE
 *	settings if they've been initialised. Otherwise, will just set it directly.
 */
void EnviroMinder::farPlane(float farPlane)
{
	if (EnviroMinderSettings::instance().isInitialised())
	{
		EnviroMinderSettings::instance().farPlane(farPlane);
	}
	else
	{
		Moo::rc().camera().farPlane(farPlane);
	}
}


/**
 *	This method draws the selected hind parts of the environment stuff
 */
void EnviroMinder::drawHind( float dTime, DrawSelection drawWhat, bool showWeather /* = true */ )
{
	//add all known fog emitters
	skyGradientDome_->addFogEmitter();
	if (showWeather)
	{
		rain_->addFogEmitter();
	}

	//update and commit fog.
	FogController::instance().tick();
	FogController::instance().commitFogToDevice();

#ifdef EDITOR_ENABLED
	//link the clouds shadows to cloud drawing
	bool drawClouds = ((drawWhat & DrawSelection::clouds) != 0);
	g_disableSkyLightMap = !drawClouds;
#endif

	//Update light maps that will be used when we draw the rest of the scene.
	if (skyLightMap_)
	{
		//TODO : change how this works to make it generic.
		//skyLightMap_->update();

		//old school way
		sky_->updateLightMap(skyLightMap_);

		//todo in the old school way
		clouds_->updateLightMap(skyLightMap_);
	}

	//On old video cards we draw the clouds, sky etc. at the back
	//of the scene, and do not use the z-buffer.
	if (EnviroMinder::primitiveVideoCard())
		drawSkySunCloudsMoon( dTime, drawWhat );
}


/** 
  * This method draws the delayed background of our environment.
  * We do this here in order to save fillrate, as we can
  * test the delayed background against the opaque scene that
  * is in the z-buffer.*/
void EnviroMinder::drawHindDelayed( float dTime, DrawSelection drawWhat )
{
	//On newer video cards we draw the clouds, sky etc. after the
	//rest of the scene to save fillrate.	see EnviroMinder::drawHind
	if (!EnviroMinder::primitiveVideoCard())
		drawSkySunCloudsMoon( dTime, drawWhat );
}


/**
 *	This method draws the fore parts of the environment stuff
 */
void EnviroMinder::drawFore( float dTime, bool showWeather /* = true */, bool showFlora /* = true */, bool showFloraShadowing /* = false */ )
{
	// Draw the seas
	seas_->draw( dTime, timeOfDay_->gameTime() );

	// Draw the rain
	// Note: there is a brake to slow down the change in rain
	// amount so it can't suddenly start or stop
	//TODO : ask sky and clouds for rain
	float wantRain = min( sky_->precipitation()[0], 1.2f );
	//float wantRain = min( clouds_->precipitation()[0], 1.2f );
	float haveRain = rain_->amount();
	rain_->amount( haveRain + Math::clamp(dTime*0.03f, wantRain - haveRain) );
	//if (wantRain != haveRain) {
	//	DEBUG_MSG("EnviroMinder::drawFore: "
	//		"wantRain=%0.3f haveRain=%0.3f\n", wantRain, haveRain);
	//}

	// Draw the snow
	float precipitation = max( sky_->precipitation()[1], clouds_->precipitation()[1] );
	snow_->amount( min( precipitation, 1.f ) );
	//snow_->amount( min( clouds_->precipitation()[1], 1.2f ) );
	
	if (showWeather)
	{
		rain_->draw();
		snow_->draw();
	}

	if (showFlora)
	{
		//Render the flora
		flora_->draw( dTime, *this );
			
		//Render shadows onto the flora
		if (showFloraShadowing && (EnviroMinderSettings::instance().shadowCaster()))
		{
			flora_->drawShadows( EnviroMinderSettings::instance().shadowCaster() );
		}
	}

	//remove all known fog emitters
	skyGradientDome_->remFogEmitter();
	rain_->remFogEmitter();
}


#ifdef EDITOR_ENABLED

std::string EnviroMinder::timeOfDayFile() const
{
    return todFile_;
}

void EnviroMinder::timeOfDayFile(std::string const &filename)
{
    todFile_ = filename;
    if (data_ != NULL)
    {
        data_->writeString( "timeOfDay", filename );
        loadTimeOfDay(data_, true);
    }
}


std::string EnviroMinder::skyGradientDomeFile() const
{
    return sgdFile_;
}


void EnviroMinder::skyGradientDomeFile(std::string const &filename)
{
    sgdFile_ = filename;
    if (data_ != NULL)
    {
        data_->writeString( "skyGradientDome", filename );
        float fp = std::numeric_limits<float>::max();
        loadSkyGradientDome(data_, true, fp);
        if (fp != std::numeric_limits<float>::max())
            setFarPlane(fp);
    }
}


#endif // EDITOR_ENABLED


/**
 *	This method returns true if the video card supports a shader version.
 *	If no shaders are supported, we may draw the environment slightly
 *	differently, in particular the sky + clouds are drawn before the scene
 *	instead of afterwards.
 */
bool EnviroMinder::primitiveVideoCard()
{
	return (Moo::rc().psVersion() + Moo::rc().vsVersion() == 0);
}


// some relevant dog watches
DogWatch g_skyStuffWatch( "Sky Stuff" );
DogWatch g_dwClouds( "Clouds" );


void EnviroMinder::decideLightingAndFog()
{
	// tone down the scene lighting	
	// TODO : ask sky and clouds for light modulation
	float dimBy = max( 0.f, 1.f-sky_->avgDensity() ) * 0.7f + 0.3f;
	Vector4 lightDimmer( dimBy, dimBy, dimBy, 1.f );
	//Vector4 lightDimmer( sky_->avgColourDim(), 1.f );	//not implemented
	//Vector4 lightDimmer( clouds_->avgColourDim(), 1.f );
	OutsideLighting & outLight = timeOfDay_->lighting();
	outLight.sunColour = outLight.sunColour * lightDimmer;
	outLight.ambientColour = outLight.ambientColour * lightDimmer;		

	// calculate fog colours	
	float avd = min( sky_->avgDensity(), 1.f );	
	Vector3 modcol = (avd==-1) ? Vector3( 1.0f, 1.0f, 1.2f ) :
		Vector3( 1-avd/2.f, 1-avd/2.f, 1-avd/2.f );
	skyGradientDome_->fogModulation( modcol, clouds_->avgFogMultiplier() );		
}


/** 
 *	This method draws the sky, sun, clouds and moon.  It may be called either
 *	before the scene is rendered (non-shader cards), or after the scene is
 *	rendered (shader cards).
 *
 *	@param	dTime		time elapsed since last call.
 *	@param	drawWhat	bitset specifying what bits of the sky to draw.
 */
void EnviroMinder::drawSkySunCloudsMoon( float dTime, DrawSelection drawWhat )
{
	if (!drawWhat) return;

	g_skyStuffWatch.start();	

	// Update viewport for background - from background minZ to 1.0f
	D3DVIEWPORT9 oldvp, vp;
	Moo::rc().device()->GetViewport( &oldvp );
	
	// Copy old viewport, just change minZ - this fixes precision isses with NVIDIA 8800.
	// Ideally we would only render background objects beyond this distance.
	memcpy( &vp, &oldvp, sizeof( vp ) );
	vp.MinZ	= backgroundVPMinZ; 

	Moo::rc().device()->SetViewport( &vp );

	// sky dome is the explicit, static hand-painted dome.
	// we also draw a dynamic gradient dome, and sun, moon and clouds
	if (drawWhat & DrawSelection::skyGradient)
		skyGradientDome_->draw( timeOfDay_ );

	//draw the sun and moon for real
	if (drawWhat & DrawSelection::sunAndMoon)
		sunAndMoon_->draw();

	//we must set these, because sky boxes + clouds are not meant to set their
	//own COLORWRITEENABLE flags, so if somebody does, then we must restore the
	//state in code.
	Moo::rc().setRenderState( D3DRS_COLORWRITEENABLE,
		D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE );

	//draw clouds
	g_dwClouds.start();
	if (drawWhat & DrawSelection::clouds)
	{				
		clouds_->draw();
		sky_->draw();
	}

	g_dwClouds.stop();	
	
	std::vector<Moo::VisualPtr>::iterator it = skyDomes_.begin();
	std::vector<Moo::VisualPtr>::iterator end = skyDomes_.end();
	while ( it != end )
	{
		//we must set these, because sky boxes + clouds are not meant to set their
		//own COLORWRITEENABLE flags, so if somebody does, then we must restore the
		//state in code.
		Moo::rc().setRenderState( D3DRS_COLORWRITEENABLE,
			D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE );
		//For sky dome .fx files, use ENVIRONMENT_TRANSFORM, SUN_POSITION,
		//SUN_COLOUR, TIME_OF_DAY
		(*it++)->draw(true);		
	}

	// Restore old viewport
	Moo::rc().device()->SetViewport( &oldvp );

	g_skyStuffWatch.stop();
}


void EnviroMinder::loadTimeOfDay(DataSectionPtr data, bool loadFromExternal)
{
    // Load TimeOfDay
    std::string todFile = data->readString( "timeOfDay" );
#ifdef EDITOR_ENABLED
    if (!todFile.empty())
        todFile_ = todFile;
#endif
	if (!todFile.empty() && loadFromExternal)
	{
		DataSectionPtr pSubSect = BWResource::openSection( todFile );
		if (pSubSect)
		{
			timeOfDay_->load( pSubSect );
		}
		else
		{
			ERROR_MSG( "EnviroMinder::load: "
				"Cannot open timeOfDay resource '%s'\n",
				todFile.c_str() );
		}
	}
	else
	{
		timeOfDay_->load( data, !loadFromExternal );
	}
}


void EnviroMinder::loadSkyGradientDome
(
    DataSectionPtr  data, 
    bool            loadFromExternal,
    float           &farPlane
)
{
	// Load SkyGradientDome
	std::string sgdFile = data->readString( "skyGradientDome" );
#ifdef EDITOR_ENABLED
    if (!sgdFile.empty())
        sgdFile_ = sgdFile;
#endif
	if (!sgdFile.empty() && loadFromExternal)
	{
		DataSectionPtr pSubSect = BWResource::openSection( sgdFile );
		if (pSubSect)
		{
			skyGradientDome_->load( pSubSect );
			farPlane = pSubSect->readFloat( "farPlane", farPlane );
		}
		else
		{
			ERROR_MSG( "EnviroMinder::load: "
				"Cannot open skyGradientDome resource '%s'\n",
				sgdFile.c_str() );
		}
	}
	else
	{
		skyGradientDome_->load( data );
	}
}


void EnviroMinder::setFarPlane(float farPlane)
{
	this->farPlane( farPlane );
	//set the far plane for the chunk loader
	ChunkManager::instance().autoSetPathConstraints(farPlane);
}

// -----------------------------------------------------------------------------
// Section: EnviroMinderSettings
// -----------------------------------------------------------------------------

/**
 *	Register the graphics settings controlled by 
 *	the envirominder. Namely, the FAR_PLANE setting.
 */
void EnviroMinderSettings::init(DataSectionPtr resXML)
{
	// far plane settings
	this->farPlaneSettings_ = 
		Moo::makeCallbackGraphicsSetting(
			"FAR_PLANE", *this, 
			&EnviroMinderSettings::setFarPlaneOption,
			-1, false, false);
				
	if (resXML.exists())
	{
		DataSectionIterator sectIt  = resXML->begin();
		DataSectionIterator sectEnd = resXML->end();
		while (sectIt != sectEnd)
		{
			static const float undefined = -1.0f;
			float farPlane = (*sectIt)->readFloat("value", undefined);
			std::string label = (*sectIt)->readString("label");
			if (!label.empty() && farPlane != undefined)
			{
				this->farPlaneSettings_->addOption(label, true);
				this->farPlaneOptions_.push_back(farPlane);
			}
			++sectIt;
		}
	}
	else
	{
		this->farPlaneSettings_->addOption("HIGHT", true);
		this->farPlaneOptions_.push_back(1.0f);
	}
	Moo::GraphicsSetting::add(this->farPlaneSettings_);
}


/**
 *	Sets the far plane. This value will be adjusted by the current
 *	FAR_PLANE setting before it gets set into the camera.
 */
void EnviroMinderSettings::farPlane(float farPlane)
{
	// EnviroMinderSettings needs 
	// to be initialized first
	MF_ASSERT(this->farPlaneSettings_.exists());
	
	this->curFarPlane_ = farPlane;
	this->setFarPlaneOption(this->farPlaneSettings_->activeOption());
}


/**
 *  Gets the current camera far plane.
 */
float EnviroMinderSettings::farPlane() const
{
    return this->curFarPlane_;
}


/**
 *	Returns true if settings have been initialized.
 */
bool EnviroMinderSettings::isInitialised() const
{
	return this->farPlaneSettings_.exists();
}


/**
 * Registers the shadow caster to use when casting shadows for the flora.
 */
void EnviroMinderSettings::shadowCaster( ShadowCaster* shadowCaster )
{
	shadowCaster_ = shadowCaster;
}


/**
 * Returns the registered the shadow caster to use when casting shadows for the flora.
 */
ShadowCaster* EnviroMinderSettings::shadowCaster() const
{
	return shadowCaster_;
}


/**
 *	Returns singleton EnviroMinderSettings instance.
 */
EnviroMinderSettings & EnviroMinderSettings::instance()
{
	static EnviroMinderSettings instance;
	return instance;
}


/**
 *	Sets the viewing distance. Implicitly called 
 *	whenever the user changes the FAR_PLANE setting.
 *
 *	The far plane setting's options can be set in the resource.xml file.
 *	Bellow is an example of how to setup the options:
 *
 *	<resources.xml>
 *		<system>	
 *			<farPlaneOptions>
 *				<option>
 *					<farPlane>1.0</farPlane>
 *					<label>FAR</label>
 *				</option>
 *				<option>
 *					<farPlane>0.5</farPlane>
 *					<label>NEAR</label>
 *				</option>
 *			</farPlaneOptions>
 *		</system>
 *	</resources.xml>
 *
 *	For each <option> entry in <farPlaneOptions>, <label> will be used
 *	to identify the option and farPlane will be multiplied to the absolute
 *	far plane value defined in space.settings or sky.xml files. See 
 *	EnviroMinderSettings::farPlane().
 *	
 */
void EnviroMinderSettings::setFarPlaneOption(int optionIndex)
{
	float ratio = this->farPlaneOptions_[optionIndex];
	Moo::rc().camera().farPlane(this->curFarPlane_ * ratio);
}
