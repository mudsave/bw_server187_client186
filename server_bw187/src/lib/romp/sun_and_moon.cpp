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

#include "sun_and_moon.hpp"
#include "enviro_minder.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/datasection.hpp"
#include "resmgr/auto_config.hpp"
#include "moo/render_context.hpp"
#include "moo/texture_manager.hpp"
#include "lens_effect_manager.hpp"
#include "fog_controller.hpp"

#include "time_of_day.hpp"

DECLARE_DEBUG_COMPONENT2( "Romp", 0 );

#ifndef CODE_INLINE
#include "sun_and_moon.ipp"
#endif


static AutoConfigString s_sunBmpName( "environment/sunBmpName" );
static AutoConfigString s_moonBmpName( "environment/moonBmpName" );
static AutoConfigString s_moonMaskBmpName( "environment/moonMaskBmpName" );
static AutoConfigString s_sunFlareXML( "environment/sunFlareXML" );
static AutoConfigString s_moonFlareXML( "environment/moonFlareXML" );


///Constructor
SunAndMoon::SunAndMoon()
:moonMask_( NULL ),
 moon_	( NULL ),
 sun_( NULL ),
 timeOfDay_( NULL ),
 sunMat_( NULL ),
 moonMat_( NULL ),
 moonMaskMat_( NULL ),
 sunMoonSet_( NULL )
{
	if (!EnviroMinder::primitiveVideoCard())
		sunMoonSet_ = ShaderManager::instance().shaderSet( "xyznduv", "sunandmoon" );
}


///Destructor
SunAndMoon::~SunAndMoon()
{
	this->destroy();

	sunMoonSet_ = NULL;
}


/**
 *	This method draws a square mesh, using a given material.
 *	This method is used for the moon, moon mask, sun and flare.
 *
 *	@param cam		the mesh to camera matrix
 *	@param pMesh	a pointer to the square mesh to draw
 *	@param pMat		a pointer to the material to draw with
 */
void SunAndMoon::drawSquareMesh( Matrix & cam, CustomMesh<Moo::VertexXYZNDUV> * pMesh, Moo::Material * pMat )
{
//	Moo::rc().setRenderState( D3DRS_SOFTWAREVERTEXPROCESSING, TRUE );
//	Moo::rc().setRenderState( D3DRS_CLIPPING, TRUE );
	Moo::rc().setPixelShader( NULL );
	if (sunMoonSet_)  
		Moo::rc().setVertexShader( sunMoonSet_->shader( 0, 0, 0, true ) );
	else
		Moo::rc().setVertexShader( NULL );		
	Moo::rc().setFVF( Moo::VertexXYZNDUV::fvf() );

	//1,2,3,4 - viewProjection
	Matrix worldViewProj( cam );	
	worldViewProj.postMultiply( Moo::rc().projection() );
	XPMatrixTranspose( &worldViewProj, &worldViewProj );
	Moo::rc().device()->SetVertexShaderConstantF( 1, (const float*)&worldViewProj, 4 );

	Moo::rc().device()->SetTransform( D3DTS_WORLD, &Matrix::identity );
	Moo::rc().device()->SetTransform( D3DTS_VIEW, &cam );
	Moo::rc().device()->SetTransform( D3DTS_PROJECTION, &Moo::rc().projection() );

	Moo::rc().setRenderState( D3DRS_LIGHTING, FALSE );

	pMat->set();
	pMesh->drawEffect();

	Moo::rc().setVertexShader( NULL );
}


/**
 *	This method creates the sun and moon meshes / materials
 */
void SunAndMoon::create( void )
{
	this->destroy();

	createMoon( );
	createSun( );


	//Sun flare
	DataSectionPtr pLensEffectsSection = BWResource::openSection( s_sunFlareXML );
	if ( pLensEffectsSection )
	{
		std::vector<DataSectionPtr> pFlareSections;
		pLensEffectsSection->openSections( "Flare", pFlareSections );

		std::vector<DataSectionPtr>::iterator it = pFlareSections.begin();
		std::vector<DataSectionPtr>::iterator end = pFlareSections.end();

		while ( it != end )
		{
			DataSectionPtr & section = *it++;

			LensEffect l;
			l.load( section );
			sunLensEffects_.push_back( l );
		}
	}
	else
	{
		WARNING_MSG( "Could not find lens effects definitions for the Sun\n" );
	}


	//Moon flare
	DataSectionPtr pMoonLensEffectsSection = BWResource::openSection( s_moonFlareXML );
	if ( pMoonLensEffectsSection )
	{
		DataSectionPtr pFlareSection = pMoonLensEffectsSection->openSection( "Flare" );
		
		if ( pFlareSection )
		{
			LensEffect l;
			l.load( pFlareSection );
			moonLensEffects_.push_back( l );
		}
	}
	else
	{
		WARNING_MSG( "Could not find lens effects definitions for the Moon\n" );
	}
}


/**
 *	This method cleans up memory allocated for the sun and moon
 */
void SunAndMoon::destroy( void )
{
	if ( moonMask_ )
	{
		delete moonMask_;
		moonMask_ = NULL;
	}

	if ( moonMaskMat_ )
	{
		delete moonMaskMat_;
		moonMaskMat_ = NULL;
	}

	if ( moon_ )
	{
		delete moon_;
		moon_ = NULL;
	}

	if ( moonMat_ )
	{
		delete moonMat_;
		moonMat_ = NULL;
	}

	if ( sun_ )
	{
		delete sun_;
		sun_ = NULL;
	}

	if ( sunMat_ )
	{
		delete sunMat_;
		sunMat_ = NULL;
	}

	sunLensEffects_.clear();
	moonLensEffects_.clear();
}


/**
 *	This method creates the mesh and material for the sun + flares.
 */
void SunAndMoon::createSun( void )
{
	sun_ = new CustomMesh<Moo::VertexXYZNDUV>( D3DPT_TRIANGLESTRIP );

	Moo::VertexXYZNDUV vert;
	vert.colour_ = 0x80777777;
	vert.normal_.set( 0,0,-1 );

	vert.pos_.set(-1, 1, 0 );
	vert.uv_.set( 0, 0 );
	sun_->push_back( vert );

	vert.pos_.set( 1, 1, 0 );
	vert.uv_.set( 1, 0 );
	sun_->push_back( vert );

	vert.pos_.set( -1, -1, 0 );
	vert.uv_.set( 0, 1 );
	sun_->push_back( vert );

	vert.pos_.set( 1, -1, 0 );
	vert.uv_.set( 1,1 );
	sun_->push_back( vert );


	//Section : create sun material
	Moo::Material * mat = sunMat_ = new Moo::Material();
	Moo::TextureStage ts;
	Moo::TextureStage ts2;
	Moo::TextureStage ts3;


	ts.colourOperation( Moo::TextureStage::MODULATE, Moo::TextureStage::TEXTURE, Moo::TextureStage::TEXTURE_FACTOR );
	ts.alphaOperation( Moo::TextureStage::SELECTARG1 );
	ts.pTexture( Moo::TextureManager::instance()->get( s_sunBmpName ) );
	mat->addTextureStage( ts );
	mat->addTextureStage( ts2 );
	mat->alphaBlended( true );
	mat->zBufferRead( true );
	mat->zBufferWrite( false );
	mat->srcBlend( Moo::Material::SRC_ALPHA );
	mat->destBlend( Moo::Material::ONE );
	mat->fogged( false );
}


/**
 *	This method creates the mesh + materials for the moon and mask
 */
void SunAndMoon::createMoon( void )
{
	moonMask_ = new CustomMesh<Moo::VertexXYZNDUV>( D3DPT_TRIANGLESTRIP );
	moon_ = new CustomMesh<Moo::VertexXYZNDUV>( D3DPT_TRIANGLESTRIP );

	Moo::VertexXYZNDUV vert;
	vert.colour_ = 0x80777777;
	vert.normal_.set( 0,0,-1 );

	vert.pos_.set( -0.6f, 0.6f, 0 );
	vert.uv_.set( 0, 0 );
	moonMask_->push_back( vert );
	moon_->push_back( vert );

	vert.pos_.set( 0.6f, 0.6f, 0 );
	vert.uv_.set( 1, 0 );
	moonMask_->push_back( vert );
	moon_->push_back( vert );

	vert.pos_.set( -0.6f, -0.6f, 0 );
	vert.uv_.set( 0, 1 );
	moonMask_->push_back( vert );
	moon_->push_back( vert );

	vert.pos_.set( 0.6f, -0.6f, 0 );
	vert.uv_.set( 1,1 );
	moonMask_->push_back( vert );
	moon_->push_back( vert );

	//Create the moon mask material
	Moo::Material *mat = moonMaskMat_ = new Moo::Material();
	
	Moo::TextureStage ts;
	Moo::TextureStage ts2;


	ts.colourOperation( Moo::TextureStage::SELECTARG1, Moo::TextureStage::TEXTURE, Moo::TextureStage::TEXTURE_FACTOR );
	ts.alphaOperation( Moo::TextureStage::SELECTARG1, Moo::TextureStage::TEXTURE, Moo::TextureStage::TEXTURE_FACTOR );
	ts.pTexture( Moo::TextureManager::instance()->get( s_moonMaskBmpName ) );
	ts.textureCoordinateIndex( 0 );
	mat->addTextureStage( ts );
	mat->addTextureStage( ts2 );

	mat->alphaBlended( true );
	mat->sorted( false );
	mat->zBufferRead( false );
	mat->zBufferWrite( false );
	mat->srcBlend( Moo::Material::SRC_ALPHA );
	mat->destBlend( Moo::Material::INV_SRC_ALPHA );
	mat->fogged( false );


	//Create the moon material
	mat = moonMat_ = new Moo::Material();


	ts.colourOperation( Moo::TextureStage::MODULATE, Moo::TextureStage::TEXTURE, Moo::TextureStage::TEXTURE_FACTOR );
	ts.alphaOperation( Moo::TextureStage::SELECTARG1 );
	ts.pTexture( Moo::TextureManager::instance()->get( s_moonBmpName ) );
	mat->addTextureStage( ts );
	mat->addTextureStage( ts2 );
	mat->alphaBlended( true );
	mat->zBufferRead( true );
	mat->zBufferWrite( false );
	mat->srcBlend( Moo::Material::ONE );
	mat->destBlend( Moo::Material::ONE );
	mat->fogged( false );
}


/**
 *	This method draws the sun and moon.
 *	The sun and moon use the timeOfDay object to position themselves
 *	in the sky.
 */
void SunAndMoon::draw()
{
	// constants for lens flare collision distances
	float SUN_DISTANCE = Moo::rc().camera().farPlane() * 0.9999f;
	float MOON_DISTANCE = SUN_DISTANCE;

	if ( !timeOfDay_ )
		return;

	//calculate the orientation of the camera
	Matrix cameraOrientation;
	cameraOrientation.multiply( Moo::rc().world(), Moo::rc().view() );
	Vector3 camPos = Moo::rc().invView().applyToOrigin();
	cameraOrientation.translation( Vector3(0,0,0) );

	//draw sun
	Matrix sunToWorld = timeOfDay_->lighting().sunTransform;
	sunToWorld.postRotateX( DEG_TO_RAD( 2.f * timeOfDay_->sunAngle() ) );
	sunToWorld.postRotateY( MATH_PI );
	sunToWorld.translation( sunToWorld.applyToUnitAxisVector(2) * Vector3( 6, 3, 1 ).length() );

	//create the sun to camera matrix
	Matrix sunToCamera( cameraOrientation );
	sunToCamera.preMultiply( sunToWorld );

	//apply the texture factor
	uint32 additiveTextureFactor = FogController::instance().additiveFarObjectTFactor();
	uint32 textureFactor = FogController::instance().farObjectTFactor();

	sunMat_->textureFactor( additiveTextureFactor );
	moonMat_->textureFactor( textureFactor );

	drawSquareMesh( sunToCamera, sun_, sunMat_ );

	//post request for sun flare
	Vector3 supposedSunPosition( sunToWorld.applyToOrigin() );
	Vector3 cameraToSun( supposedSunPosition );
	cameraToSun.normalise();
	float dotp = cameraToSun.dotProduct( Moo::rc().invView().applyToUnitAxisVector(2) );
	//Vector3 actualSunPosition( camPos + cameraToSun * (SUN_DISTANCE / dotp) );
	Vector3 actualSunPosition( camPos + cameraToSun * SUN_DISTANCE );

	if ( dotp > 0.f )
	{
		LensEffects::iterator it = sunLensEffects_.begin();
		LensEffects::iterator end = sunLensEffects_.end();

		uint32 id = (uint32)sun_;

		while( it != end )
		{
			LensEffect & l = *it++;
			
			LensEffectManager::instance().add( (uint32)id++, actualSunPosition, l );
		}
	}
	
	//draw moon
	Matrix moonToWorld = timeOfDay_->lighting().moonTransform;
	moonToWorld.postRotateX( DEG_TO_RAD( 2.f * timeOfDay_->sunAngle() ) );
	moonToWorld.postRotateY( MATH_PI );
	moonToWorld.translation( moonToWorld.applyToUnitAxisVector(2) * Vector3( 6, 3, 1 ).length() );

	//create the moon to camera matrix
	Matrix moonToCamera( cameraOrientation );
	moonToCamera.preMultiply( moonToWorld );

	drawSquareMesh( moonToCamera, moon_, moonMat_ );
}


/**
 *	This method draws the moon mask.
 *	The moon mask is the black circular disk that sits behind the
 *	crescent moon, and obscures the stars.  We need to use this
 *	technique, becuase the 'blackened out' area of the crescent
 *	moon needs to be drawn in the sky gradient colour.
 */
void SunAndMoon::drawMoonMask()
{
	if ( !timeOfDay_ )
		return;

	//draw moon opacity
	Matrix m = timeOfDay_->lighting().moonTransform;
	m.postRotateY( MATH_PI );
	m.translation( m.applyToUnitAxisVector(2) * Vector3( 6, 3, 1 ).length() );

	Matrix cam;
	cam.multiply( Moo::rc().world(), Moo::rc().view() );
	cam.translation( Vector3(0,0,0) );
	cam.preMultiply( m );

	Matrix scale;
	scale.setScale( 1, 1, 1 );
	cam.preMultiply( scale );

	drawSquareMesh( cam, moonMask_, moonMaskMat_ );
}


std::ostream& operator<<(std::ostream& o, const SunAndMoon& t)
{
	o << "SunAndMoon\n";
	return o;
}

// sun_and_moon.cpp