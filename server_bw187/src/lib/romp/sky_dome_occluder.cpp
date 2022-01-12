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
#include "cstdmf/debug.hpp"
#include "moo/moo_dx.hpp"
#include "moo/scissors_setter.hpp"

#include "lens_effect.hpp"
#include "sky_dome_occluder.hpp"
#include "enviro_minder.hpp"
#include "clouds.hpp"
#include "time_of_day.hpp"

DECLARE_DEBUG_COMPONENT2( "Romp", 0 )

//This occluder counts the number of occluding pixels over the sun.
static float s_minDapplePercent = 0.2f;
static float s_minSolidPercent = 0.7f;
static bool s_showOcclusionTests = false;

const int SUN_WORLD_SIZE = 75;

// -----------------------------------------------------------------------------
// Section: Occlusion Testing Effect Constant
// -----------------------------------------------------------------------------
/**
 *	This class tells sky domes whether or not they should draw using their
 *	standard shaders, or their occlusion test shader
 */
class OcclusionTestEnable : public Moo::EffectConstantValue
{
public:
	OcclusionTestEnable():
	  value_( false )
	{		
	}
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		pEffect->SetInt(constantHandle, value_ ? 1 : 0);
		return true;
	}
	void value( bool value )
	{
		value_ = value;
	}
private:
	bool value_;
};

static OcclusionTestEnable s_occlusionTestConstant;


// -----------------------------------------------------------------------------
// Section: Occlusion Testing Alpha Reference Constant
// -----------------------------------------------------------------------------
/**
 *	This class sets the alpha reference value for sky domes to use when doing
 *	their occlusion test pass.
 */
class OcclusionTestAlphaRef : public Moo::EffectConstantValue
{
public:
	OcclusionTestAlphaRef():
	  value_( 0x00000000 )
	{		
	}
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		pEffect->SetInt(constantHandle, value_);
		return true;
	}
	void value( uint32 value )
	{
		value_ = value;
	}
private:
	uint32	value_;
};

static OcclusionTestAlphaRef s_occlusionTestAlphaRef;


// -----------------------------------------------------------------------------
// Section: Sky Dome Occluder
// -----------------------------------------------------------------------------
bool SkyDomeOccluder::isAvailable()
{
	HRESULT hr = Moo::rc().device()->CreateQuery( D3DQUERYTYPE_OCCLUSION, NULL );	
	return ((hr==D3D_OK) && (Moo::ScissorsSetter::isAvailable()));
}


/**
 *	Constructor.
 */
SkyDomeOccluder::SkyDomeOccluder(EnviroMinder& enviroMinder):
	helper_( 1, 200000 ),
	lastSunResult_( 0 ),
	enviroMinder_( enviroMinder ),
	sunThisFrame_( false )
{
	*Moo::EffectConstantValue::get("EnvironmentOcclusionTest") = 
		&s_occlusionTestConstant;

	*Moo::EffectConstantValue::get("EnvironmentOcclusionAlphaRef") =
		&s_occlusionTestAlphaRef;
}


SkyDomeOccluder::~SkyDomeOccluder()
{
}


void SkyDomeOccluder::beginOcclusionTests()
{
	sunThisFrame_ = false;

	if (!s_showOcclusionTests)
	{
		Moo::rc().setRenderState( D3DRS_COLORWRITEENABLE, 0 );
	}
	else
	{
		Moo::rc().setRenderState( D3DRS_COLORWRITEENABLE,
			D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE );
	}

	//reset used parameters.
	helper_.begin();
}


/**
 *	This method checks the ray between the photon source
 *	and the eye, using the VisibilityTest function on the xbox.
 */
PhotonOccluder::Result SkyDomeOccluder::collides( 
	const Vector3 & photonSourcePosition,
	const Vector3 & cameraPosition,
	const LensEffect& le )
{
	bool isTheSun = ( le.isSunFlare() || le.isSunSplit() );
	if (!isTheSun || sunThisFrame_)
	{
		//We are only interested in testing the sun flare.
		return PhotonOccluder::NONE;
	}

	DX::Device* device = Moo::rc().device();
	PhotonOccluder::Result result = PhotonOccluder::NONE;

	OcclusionQueryHelper::Handle queryHandle;
	queryHandle = helper_.handleFromId( 0 );
	sunThisFrame_ = true;

	uint32 possiblePixels = 0;
	
	if (helper_.beginQuery(queryHandle))
	{		
		possiblePixels = this->drawOccluders();		
		helper_.endQuery(queryHandle);		
	}
	else
	{
		ERROR_MSG( "Could not begin sky dome visibility test for the sun "
					"lens flare\n" );
	}
/*
	//relative size represents how much larger the frame buffer is
	//than that of the xbox.  the pixel constants were tweaked to
	//look good on the xbox, later transported to PC
	//and square this value, because it represents an area / num pixels
	float relativeSize = (Moo::rc().screenWidth() / 640.f);	
	//i thought you'd just square the relativeSize but alas, tweaking
	//shows the following magic maths to give a better result.  go figure.
	float SUN_CONSTANT_FACTOR = relativeSize * relativeSize * relativeSize * relativeSize;		

	//we have to slow down changes in the sun flare from
	//one type to another - using an exact value for
	//number of pixels can cause flickering of flares.
	uint fullFlare = (uint)(NUM_SUN_FULL_FLARE_PIXELS*SUN_CONSTANT_FACTOR);
	uint splitFlare = (uint)(NUM_SUN_SPLIT_FLARE_PIXELS*SUN_CONSTANT_FACTOR);

	if ( lastSunResult_ > fullFlare )
	{
		//make it easier to stay as full flare
		fullFlare -= SUN_LENIENCY;
	}
	else if ( lastSunResult_ > splitFlare )
	{
		//make it easier to stay as split flare
		splitFlare -= SUN_LENIENCY;
		fullFlare += SUN_LENIENCY;
	}
	else
	{
		//make it easier to stay as no flare
		splitFlare += SUN_LENIENCY;
	}*/

	lastSunResult_ = helper_.avgResult(queryHandle);
	float coverage = (float)lastSunResult_ / (float)possiblePixels;

	if ( coverage < s_minDapplePercent )
	{
		return PhotonOccluder::NONE;
	}
	else if ( coverage < s_minSolidPercent )
	{
		return PhotonOccluder::DAPPLED;
	}
	else
	{		
		return PhotonOccluder::SOLID;
	}	
}


void SkyDomeOccluder::endOcclusionTests()
{
	helper_.end();

	Moo::rc().setRenderState( D3DRS_COLORWRITEENABLE,
		D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE );
}


/**
 *	This method draws all the occluder objects, such as the
 *	clouds and the sky boxes.  It uses the automatic effect
 *	variable "EnvironmentOcclusionTest" to signify to these
 *	visuals that they should draw themselves using their
 *	occlusion shaders instead of their ordinary ones.
 *
 *	@return	int	the number of pixels in the region we are drawing to.
 */
uint32 SkyDomeOccluder::drawOccluders()
{
	static uint32 s_alphaRef = 0xf824785f;
	if (s_alphaRef == 0xf824785f)
	{
		s_alphaRef = 0x00000080;
		MF_WATCH( "Render/Sky Dome Occlusion Alpha Reference",
			s_alphaRef,
			Watcher::WT_READ_WRITE,
			"Alpha Reference used by sky domes when they are drawing their"
			"occlusion test pass.  Lower values indicate the sun lens flare"
			"is more likely to shine through" );
		MF_WATCH( "Render/Sky Dome Occlusion/ Last Result",
			lastSunResult_,
			Watcher::WT_READ_ONLY,
			"Latest result from the sky dome occlusion test" );
		MF_WATCH( "Render/Sky Dome Occlusion/ min dapple",
			s_minDapplePercent,
			Watcher::WT_READ_WRITE,
			"Occluder coverage of sun greater than this value may dapple the sun" );
		MF_WATCH( "Render/Sky Dome Occlusion/ min solid",
			s_minSolidPercent,
			Watcher::WT_READ_WRITE,
			"Occluder coverage of sun greater than this value will obstruct the sun" );
		MF_WATCH( "Render/Sky Dome Occlusion/ show tests",
			s_showOcclusionTests,
			Watcher::WT_READ_WRITE,
			"turn this on to show the occlusion tests on-screen" );
	}

	s_occlusionTestConstant.value(true);
	s_occlusionTestAlphaRef.value(s_alphaRef);

	Matrix oldView( Moo::rc().view() );		
	//following fudgy matrix code is copied from sun_and_moon.
	//my question is, what does "sunTransform" mean when the sun
	//then draws itself differently?
	Matrix newView( enviroMinder_.timeOfDay()->lighting().sunTransform );
	newView.postRotateX( DEG_TO_RAD( 2.f * enviroMinder_.timeOfDay()->sunAngle() ) );
	newView.postRotateY( MATH_PI );
	newView.invert();
	Moo::rc().view( newView );

	//temporarily set a centered viewport of the desired size.
	//then we rotate the view matrix to look at the sun, thus
	//drawing sky domes etc. in the centre of the screen (and
	//thus centered in the viewport).  The viewport clips the
	//sky domes to the size of the occlusion test, and the
	//occlusion query stuff records how many pixels were drawn.
	uint32 w = (uint32)Moo::rc().halfScreenWidth();
	uint32 h = (uint32)Moo::rc().halfScreenHeight();
	uint32 nPixels = 2 * (SUN_WORLD_SIZE * w) / 640;
	Moo::ScissorsSetter v( w-nPixels, h-nPixels, nPixels, nPixels );	

	Clouds * clouds = enviroMinder_.clouds();
	if (clouds)
	{
		clouds->draw();
	}

	std::vector<Moo::VisualPtr>& skyDomes = enviroMinder_.skyDomes();
	std::vector<Moo::VisualPtr>::iterator it = skyDomes.begin();
	std::vector<Moo::VisualPtr>::iterator end= skyDomes.end();

	while (it != end)
	{
		Moo::VisualPtr& pVisual = *it++;
		pVisual->draw(true);
	}

	s_occlusionTestConstant.value(false);

	Moo::rc().view( oldView );

	return (nPixels * nPixels);
}


/**
 *	This method releases all queries and resets the viz test status.
 *	It is called when the device is lost.
 *
 *	The entire class is reset
 */
void SkyDomeOccluder::deleteUnmanagedObjects()
{	
	lastSunResult_ = 0;
	sunThisFrame_ = false;
}


std::ostream& operator<<(std::ostream& o, const SkyDomeOccluder& t)
{
	o << "SkyDomeOccluder\n";
	return o;
}

// sky_dome_occluder.cpp
