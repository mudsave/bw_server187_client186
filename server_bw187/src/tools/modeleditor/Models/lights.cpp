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

#include "gizmo/general_properties.hpp"
#include "resmgr/auto_config.hpp"

#include "mru.hpp"

#include "me_light_proxies.hpp"

DECLARE_DEBUG_COMPONENT( 0 )
#include "me_error_macros.hpp"

#include "lights.hpp"

static AutoConfigString s_default_lights( "system/defaultLightsPath" );

/*static*/ Lights* Lights::s_instance_ = NULL;

void GeneralLight::elect()
{
	editor_->elect();
}

void GeneralLight::expel()
{
	editor_->expel();
}

void GeneralLight::enabled( bool v )
{
	enabled_ = v;
	Lights::instance().dirty( true );
	Lights::instance().regenerateLightContainer();
}

bool GeneralLight::enabled()
{
	return enabled_;
}

AmbientLight::AmbientLight()
{
	enabled_ = false;
	light_ = Moo::Colour( 1.f, 1.f, 1.f, 1.f );
	editor_ = new GeneralEditor();

	//Colour
	ColourProxyPtr colourProxy = new MeLightColourWrapper<AmbientLight>( this );
	ColourProperty* pColProp = new ColourProperty( "Colour", colourProxy );
	pColProp->UIDesc( "The colour of the ambient light" );
	editor_->addProperty( pColProp );
}
Moo::Colour AmbientLight::colour()
{
	return light_;
}
void AmbientLight::colour( Moo::Colour v )
{
	light_ = v;
	Lights::instance().regenerateLightContainer();
}
Moo::Colour AmbientLight::light()
{
	return light_;
}

OmniLight::OmniLight()
{
	GeneralProperty* pProp = NULL;
	
	//Colour
	ColourProxyPtr colourProxy = new MeLightColourWrapper<Moo::OmniLight>( light_ );
	pProp = new ColourProperty( "Colour", colourProxy );
	pProp->UIDesc( "The colour of the omni light" );
	editor_->addProperty( pProp );

	//Position
	matrixProxy_ = new MeLightPosMatrixProxy<Moo::OmniLight>(
		light_,
		&matrix_ );
	pProp = new GenPositionProperty("Position", matrixProxy_);
	pProp->UIDesc( "The position of the omni light" );
	editor_->addProperty( pProp );

	//Inner Radius
	FloatProxyPtr minRadiusProxy = new MeLightFloatProxy<Moo::OmniLight>(
		light_,
		&(Moo::OmniLight::innerRadius),
		&(Moo::OmniLight::innerRadius),
		"full strength radius",
		10.f);
	pProp = new GenRadiusProperty("Full Strength Radius", minRadiusProxy, matrixProxy_, 0xbf10ff10, 2.f );
	pProp->UIDesc( "The full strength radius of the omni light" );
	editor_->addProperty( pProp );

	//Outer Radius
	FloatProxyPtr maxRadiusProxy = new MeLightFloatProxy<Moo::OmniLight>(
		light_,
		&(Moo::OmniLight::outerRadius),
		&(Moo::OmniLight::outerRadius),
		"fall-off radius",
		20.f);
	pProp = new GenRadiusProperty("Fall-off Radius", maxRadiusProxy, matrixProxy_, 0xbf1010ff, 4.f);
	pProp->UIDesc( "The fall-off radius of the omni light" );
	editor_->addProperty( pProp );

	//Multiplier
	FloatProxyPtr multiplierProxy = new MeLightFloatProxy<Moo::OmniLight>(
		light_,
		&(Moo::OmniLight::multiplier),
		&(Moo::OmniLight::multiplier),
		"multiplier",
		1.f);
	pProp = new GenFloatProperty( "Multiplier", multiplierProxy );
	pProp->UIDesc( "The intensity multiplier factor of the omni light" );
	editor_->addProperty( pProp );

	light_->worldTransform( Matrix::identity );
}

DirLight::DirLight()
{
	GeneralProperty* pProp = NULL;
	
	//Colour
	ColourProxyPtr colourProxy = new MeLightColourWrapper<Moo::DirectionalLight>( light_ );
	pProp = new ColourProperty( "Colour", colourProxy );
	pProp->UIDesc( "The colour of the directional light" );
	editor_->addProperty( pProp );

	//Rotation
	matrixProxy_ = new MeLightDirMatrixProxy<Moo::DirectionalLight>(
		light_,
		&matrix_ );
	pProp = new GenRotationProperty( "Direction", matrixProxy_);
	pProp->UIDesc( "The direction of the directional light" );
	editor_->addProperty( pProp ); 

	//Multiplier
	FloatProxyPtr multiplierProxy = new MeLightFloatProxy<Moo::DirectionalLight>(
		light_,
		&(Moo::DirectionalLight::multiplier),
		&(Moo::DirectionalLight::multiplier),
		"multiplier",
		1.f);
	pProp = new GenFloatProperty( "Multiplier", multiplierProxy );
	pProp->UIDesc( "The intensity multiplier factor of the directional light" );
	editor_->addProperty( pProp );

	light_->worldTransform( Matrix::identity );
}

SpotLight::SpotLight()
{
	GeneralProperty* pProp = NULL;
	
	//Colour
	ColourProxyPtr colourProxy = new MeLightColourWrapper<Moo::SpotLight>( light_ );
	pProp = new ColourProperty( "Colour", colourProxy );
	pProp->UIDesc( "The colour of the spot light" );
	editor_->addProperty( pProp );
		
	//Position + Rotation
	matrixProxy_ = new MeSpotLightPosDirMatrixProxy<Moo::SpotLight>(
		light_,
		&matrix_ );
	pProp = new GenPositionProperty( "Position", matrixProxy_);
	pProp->UIDesc( "The position of the spot light" );
	editor_->addProperty( pProp );
	pProp = new GenRotationProperty( "Direction", matrixProxy_);
	pProp->UIDesc( "The direction of the spot light" );
	editor_->addProperty( pProp );

	//Set the initial position and direction
	position( Vector3( 0.f, 2.f, 0.f ));
	direction( Vector3( 0.f, -1.f, 0.f ));

	//Inner Radius
	FloatProxyPtr minRadiusProxy = new MeLightFloatProxy<Moo::SpotLight>(
		light_,
		&(Moo::SpotLight::innerRadius),
		&(Moo::SpotLight::innerRadius),
		"full strength radius",
		10.f);
	pProp = new GenRadiusProperty("Full Strength Radius", minRadiusProxy, matrixProxy_, 0xbf10ff10, 2.f );
	pProp->UIDesc( "The full strength radius of the spot light" );
	editor_->addProperty( pProp );

	//Outer Radius
	FloatProxyPtr maxRadiusProxy = new MeLightFloatProxy<Moo::SpotLight>(
		light_,
		&(Moo::SpotLight::outerRadius),
		&(Moo::SpotLight::outerRadius),
		"fall-off radius",
		20.f);
	pProp = new GenRadiusProperty("Fall-off Radius", maxRadiusProxy, matrixProxy_, 0xbf1010ff, 4.f);
	pProp->UIDesc( "The fall-off radius of the spot light" );
	editor_->addProperty( pProp );

	//Cone Angle
	FloatProxyPtr coneAngleProxy = new MeLightConeAngleProxy( light_, 30.f );
	pProp = new AngleProperty( "Cone Angle", coneAngleProxy, matrixProxy_ );
	pProp->UIDesc( "The cone angle of the spot light" );
	editor_->addProperty( pProp );

	//Multiplier
	FloatProxyPtr multiplierProxy = new MeLightFloatProxy<Moo::SpotLight>(
		light_,
		&(Moo::SpotLight::multiplier),
		&(Moo::SpotLight::multiplier),
		"multiplier",
		1.f);
	pProp = new GenFloatProperty( "Multiplier", multiplierProxy );
	pProp->UIDesc( "The intensity multiplier factor of the directional light" );
	editor_->addProperty( pProp );

	light_->worldTransform( Matrix::identity );
}

Lights::Lights()
{
	s_instance_ = this;
	
	//TODO: These values need to come from Options.xml
	for (int i=0; i<4; i++)
		omni_.push_back( new OmniLight );

	for (int i=0; i<2; i++)
		dir_.push_back( new DirLight );

	for (int i=0; i<2; i++)
		spot_.push_back( new SpotLight );

	regenerateLightContainer();

	currFile_ = NULL;

	dirty_ = false;
}

Lights::~Lights()
{
	for (int i=0; i<numOmni(); i++)
		delete omni_[i];

	for (int i=0; i<numDir(); i++)
		delete dir_[i];

	for (int i=0; i<numSpot(); i++)
		delete spot_[i];
}

void Lights::regenerateLightContainer()
{
	lc_ = new Moo::LightContainer;
	
	if (ambient()->enabled())
		lc_->ambientColour( ambient()->light() );

	for (int i=0; i<numOmni(); i++)
		if (omni(i)->enabled())
			lc_->addOmni( omni(i)->light() );

	for (int i=0; i<numDir(); i++)
		if (dir(i)->enabled())
			lc_->addDirectional( dir(i)->light() );

	for (int i=0; i<numSpot(); i++)
		if (spot(i)->enabled())
			lc_->addSpot( spot(i)->light() );
}

void Lights::disableAllLights()
{
	ambient()->enabled( false );
	for (int i=0; i<numOmni(); i++)
	{
		omni(i)->enabled( false );
	}
	for (int i=0; i<numDir(); i++)
	{
		dir(i)->enabled( false );
	}
	for (int i=0; i<numSpot(); i++)
	{
		spot(i)->enabled( false );
	}
}

bool Lights::newSetup()
{
	if (dirty_)
	{
		int result = ::MessageBox( AfxGetApp()->m_pMainWnd->GetSafeHwnd(),
			"Changes were made to the current lighting setup.\nDo you want to save changes before closing?",
			"Lights Changed", MB_YESNOCANCEL | MB_ICONWARNING );
		if( result == IDCANCEL )
		{
			return false;
		}
		if( result == IDYES )
		{
			if (!save())
			{
				return false;
			}
		}
		if( result == IDNO )
		{
			ME_WARNING_MSG( "Closing light setup without saving changes.\n" );
		}
	}

	disableAllLights();
	currName_ = "";
	currFile_ = NULL;

	dirty_ = false;

	regenerateLightContainer();

	return true;
}

bool Lights::open( const std::string& name, DataSectionPtr file /* = NULL */ )
{
	if (file == NULL)
	{
		file = BWResource::openSection( name );
		if (file == NULL) return false;
	}
		
	int omniCount = 0;
	int dirCount = 0;
	int spotCount = 0;

	if (!newSetup()) return false;

	std::vector< DataSectionPtr > pLights;
	file->openSections( "Lights/Light", pLights );
	std::vector< DataSectionPtr >::iterator lightsIt = pLights.begin();
	std::vector< DataSectionPtr >::iterator lightsEnd = pLights.end();
	while (lightsIt != lightsEnd)
	{
		DataSectionPtr pLight = *lightsIt++;
		std::string lightType = pLight->readString( "Type", "" );

		bool enabled = pLight->readBool("Enabled", false);
		Vector3 empty ( 0.f, 0.f, 0.f );
		Vector3 colourVector = pLight->readVector3( "Color", empty ) / 255.f;
		Moo::Colour colour(
			colourVector[2],
			colourVector[1],
			colourVector[0],
			1.f);
		Vector3 position = pLight->readVector3( "Location", empty );
		Vector3 direction = pLight->readVector3( "Orientation", empty );
		float innerRadius = pLight->readFloat( "Full_Radius", 0.f );
		float outerRadius = pLight->readFloat( "Falloff_Radius", 0.f );
		float cosConeAngle = (float)(cos( pLight->readFloat( "Cone_Size", 30.f ) * MATH_PI / 180.f ));
			
		if (lightType == "Ambient")
		{
			ambient()->enabled( enabled );
			ambient()->colour( colour );
		}
		else if (lightType == "Omni")
		{
			if (omniCount >= numOmni()) continue;
			omni(omniCount)->enabled( enabled );
			omni(omniCount)->light()->colour( colour );
			omni(omniCount)->position( position );
			omni(omniCount)->light()->innerRadius( innerRadius );
			omni(omniCount)->light()->outerRadius( outerRadius );
			omni(omniCount)->light()->worldTransform( Matrix::identity );
			omniCount++;
		}
		else if (lightType == "Directional")
		{
			if (dirCount >= numDir()) continue;
			dir(dirCount)->enabled( enabled );
			dir(dirCount)->light()->colour( colour );
			dir(dirCount)->direction( direction );
			dir(dirCount)->light()->worldTransform( Matrix::identity );
			dirCount++;
		}
		else if (lightType == "Spot")
		{
			if (spotCount >= numSpot()) continue;
			spot(spotCount)->enabled( enabled );
			spot(spotCount)->light()->colour( colour );
			spot(spotCount)->position( position );
			spot(spotCount)->direction( direction );
			spot(spotCount)->light()->innerRadius( innerRadius );
			spot(spotCount)->light()->outerRadius( outerRadius );
			spot(spotCount)->light()->cosConeAngle( cosConeAngle );
			spot(spotCount)->light()->worldTransform( Matrix::identity );
			spotCount++;
		}
	}

	currName_ = name;
	currFile_ = file;

	dirty_ = false;

	regenerateLightContainer();

	return true;
}

bool Lights::save( const std::string& name /* = "" */ )
{
	DataSectionPtr file;
	
	if (name == "")
	{
		if (currFile_ == NULL)
		{
			static char BASED_CODE szFilter[] =	"Lighting Model (*.mvl)|*.mvl||";
			CFileDialog fileDlg (FALSE, "", "", OFN_OVERWRITEPROMPT, szFilter);

			std::string lightsDir;
			MRU::instance().getDir("lights", lightsDir, s_default_lights );
			fileDlg.m_ofn.lpstrInitialDir = lightsDir.c_str();

			if ( fileDlg.DoModal() == IDOK )
			{
				currName_ = BWResource::dissolveFilename( std::string( fileDlg.GetPathName() ));
				file = BWResource::openSection( currName_, true );
				MRU::instance().update( "lights", currName_, true );
			}
			else
			{
				return false;
			}
		}
		else
		{
			file = currFile_;
		}
	}
	else
	{
		currName_ = name;
		file = BWResource::openSection( name, true );
	}

	file->delChildren(); // Clear any old lights out

	DataSectionPtr pLights = file->newSection( "Lights" );

	{
		AmbientLight* light = ambient();
		Moo::Colour col = light->colour();
		
		DataSectionPtr pLight = pLights->newSection("Light");
		pLight->writeString( "Type", "Ambient" );
		pLight->writeBool("Enabled", light->enabled());
		pLight->writeVector3( "Color", Vector3( col.b, col.g, col.r ) * 255.f);
	}

	for (int i=0; i<numOmni(); i++)
	{
		OmniLight* light = omni(i);
		Moo::Colour col = light->light()->colour();

		DataSectionPtr pLight = pLights->newSection("Light");
		pLight->writeString( "Type", "Omni" );
		pLight->writeBool("Enabled", light->enabled());
		pLight->writeVector3( "Color", Vector3( col.b, col.g, col.r ) * 255.f);
		pLight->writeVector3( "Location", light->light()->position() );
		pLight->writeFloat( "Full_Radius", light->light()->innerRadius() );
		pLight->writeFloat( "Falloff_Radius", light->light()->outerRadius() );
	}

	for (int i=0; i<numDir(); i++)
	{
		DirLight* light = dir(i);
		Moo::Colour col = light->light()->colour();

		DataSectionPtr pLight = pLights->newSection("Light");
		pLight->writeString( "Type", "Directional" );
		pLight->writeBool("Enabled", light->enabled());
		pLight->writeVector3( "Color", Vector3( col.b, col.g, col.r ) * 255.f);
		pLight->writeVector3( "Orientation", light->light()->direction() );
	}

	for (int i=0; i<numSpot(); i++)
	{
		SpotLight* light = spot(i);
		Moo::Colour col = light->light()->colour();

		DataSectionPtr pLight = pLights->newSection("Light");
		pLight->writeString( "Type", "Spot" );
		pLight->writeBool("Enabled", light->enabled());
		pLight->writeVector3( "Color", Vector3( col.b, col.g, col.r ) * 255.f);
		pLight->writeVector3( "Location", light->light()->position() );
		pLight->writeVector3( "Orientation", -light->light()->direction() );
		pLight->writeFloat( "Full_Radius", light->light()->innerRadius() );
		pLight->writeFloat( "Falloff_Radius", light->light()->outerRadius() );
		pLight->writeFloat( "Cone_Size", (float)(180.f * acos(light->light()->cosConeAngle()) / MATH_PI) );
	}

	file->save();

	char buf[256];
	sprintf( buf, "Saving Lights: \"%s\"\n", currName_.c_str() );
	ME_INFO_MSG( buf );

	currFile_ = file;
	
	dirty_ = false;

	return true;
}
