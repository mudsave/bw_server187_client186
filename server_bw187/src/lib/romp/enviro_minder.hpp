/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef ENVIRO_MINDER_HPP
#define ENVIRO_MINDER_HPP

#include "cstdmf/smartpointer.hpp"
#include "moo/visual.hpp"
#include "particle/py_meta_particle_system.hpp"

class DataSection;
typedef SmartPointer<DataSection> DataSectionPtr;

class TimeOfDay;
class Weather;
class SkyGradientDome;
class SunAndMoon;
class Sky;
class Dapple;
class Seas;
class Rain;
class Snow;
class Clouds;
class SkyLightMap;
class Flora;
class ShadowCaster;
class SkyDomeOccluder;

struct WeatherSettings;


/**
 *	Something that wants to be attached to the main player model.
 */
struct PlayerAttachment
{
	PyMetaParticleSystemPtr pSystem;
	std::string		onNode;
};

/**
 *	A vector of PlayerAttachment objects
 */
class PlayerAttachments : public std::vector<PlayerAttachment>
{
public:
	void add( PyMetaParticleSystemPtr pSys, const std::string & node );
};


/**
 *	This class minds a whole lot of pointers to classes used to render
 *	and control the physical environment in the game. It deletes any
 *	pointers it holds in its destructor.
 */
class EnviroMinder
{
public:
	EnviroMinder();
	~EnviroMinder();

	struct DrawSelection
	{
		enum
		{
			// bit values for drawSkyCtrl are sorted in drawing order
			sunAndMoon	= 0x0001,
			skyGradient	= 0x0002,
			clouds		= 0x0004,
			sunFlare	= 0x0008,

			all			= 0xffff
		};
		uint32		value;

		DrawSelection() : value( all ) {}

		operator uint32()			{ return value; }
		void operator=( uint32 v )	{ value = v; }
	};

	bool load( DataSectionPtr pDS, bool loadFromExternal = true );
#ifdef EDITOR_ENABLED
    bool save( DataSectionPtr pDS, bool saveToExternal = true ) const;
#endif
	const DataSectionPtr pData()			{ return data_; }

	void tick( float dTime, bool outside,
		const WeatherSettings * pWeatherOverride = NULL );

	void activate();
	void deactivate();
	void farPlane(float farPlane);

	void drawHind( float dTime, DrawSelection drawWhat = DrawSelection(), bool showWeather = true );
	void drawHindDelayed( float dTime, DrawSelection drawWhat = DrawSelection() );
	void drawFore( float dTime, bool showWeather = true, bool showFlora = true, bool showFloraShadowing = false );

	TimeOfDay *         timeOfDay()         { return timeOfDay_; }
	Weather *           weather()           { return weather_; }
	SkyGradientDome *   skyGradientDome()   { return skyGradientDome_; }
	SunAndMoon *        sunAndMoon()        { return sunAndMoon_; }
	Sky *               sky()               { return sky_; }
	Clouds *            clouds()            { return clouds_; }
	Seas *              seas()              { return seas_; }
	Rain *              rain()              { return rain_; }
	Snow *              snow()              { return snow_; }
	SkyLightMap *       skyLightMap()       { return skyLightMap_; }
	Flora *				flora()				{ return flora_; }

	const Vector4 & thunder() const			{ return thunder_; }
    std::vector<Moo::VisualPtr> &skyDomes() { return skyDomes_; }

	PlayerAttachments & playerAttachments()	{ return playerAttachments_; }
	bool playerDead() const					{ return playerDead_; }
	void playerDead( bool isDead )			{ playerDead_ = isDead; }

#ifdef EDITOR_ENABLED
    std::string         timeOfDayFile() const;
    void                timeOfDayFile(std::string const &filename);

    std::string         skyGradientDomeFile() const;
    void                skyGradientDomeFile(std::string const &filename);
#endif

	static bool EnviroMinder::primitiveVideoCard();

private:
	EnviroMinder( const EnviroMinder& );
	EnviroMinder& operator=( const EnviroMinder& );

	void decideLightingAndFog();
	void drawSkySunCloudsMoon( float dTime, DrawSelection drawWhat );

    void loadTimeOfDay(DataSectionPtr data, bool loadFromExternal);
    void loadSkyGradientDome(DataSectionPtr data, bool loadFromExternal,
            float &farPlane);
    void setFarPlane(float farPlane);

	TimeOfDay		*           timeOfDay_;
	Weather			*           weather_;
	SkyGradientDome	*           skyGradientDome_;
	SunAndMoon		*           sunAndMoon_;
	Sky				*           sky_;
	Clouds			*           clouds_;
	Seas			*           seas_;
	Rain			*           rain_;
	Snow			*           snow_;
	SkyDomeOccluder *			skyDomeOccluder_;
	//TODO : unsupported feature at the moment.  to be finished.
	//Dapple			*			dapple_;
	SkyLightMap		*           skyLightMap_;
	Flora			*           flora_;
	std::vector<Moo::VisualPtr> skyDomes_;
	Vector4				        thunder_;
	PlayerAttachments	        playerAttachments_;
	bool				        playerDead_;
	DataSectionPtr	            data_;
	static EnviroMinder*		s_activatedEM_;

#ifdef EDITOR_ENABLED
    std::string                 todFile_;
    std::string                 sgdFile_;
#endif
};

/**
 *	Manages all graphics settings related to the EnviroMinder.
 */
class EnviroMinderSettings
{
public:
	void init(DataSectionPtr resXML);
	void farPlane(float farPlane);		
    float farPlane()  const;
    bool isInitialised() const;

	void shadowCaster( ShadowCaster* shadowCaster );
	ShadowCaster* shadowCaster() const;
	
	static EnviroMinderSettings & instance();
	
private:
	void setFarPlaneOption(int optionIndex);
	
	typedef std::vector<float> FloatVector;
	typedef Moo::GraphicsSetting::GraphicsSettingPtr GraphicsSettingPtr;
	
	GraphicsSettingPtr farPlaneSettings_;
	FloatVector farPlaneOptions_;
	float curFarPlane_;
	ShadowCaster* shadowCaster_;

private:
	EnviroMinderSettings() :
		farPlaneSettings_(NULL),
		farPlaneOptions_(),
		curFarPlane_(1000.0f),
		shadowCaster_(NULL)
	{}
};


#ifdef CODE_INLINE
#include "enviro_minder.ipp"
#endif

#endif // ENVIRO_MINDER_HPP
