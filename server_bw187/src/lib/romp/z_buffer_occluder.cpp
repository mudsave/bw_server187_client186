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
#include "moo/material.hpp"
#include "moo/texturestage.hpp"
#include "moo/vertex_formats.hpp"
#include "moo/moo_dx.hpp"

#include "lens_effect.hpp"
#include "z_buffer_occluder.hpp"
#include "custom_mesh.hpp"

DECLARE_DEBUG_COMPONENT2( "Romp", 0 )

#ifndef CODE_INLINE
#include "z_buffer_occluder.ipp"
#endif

#define MAX_LENS_FLARES				256
#define SUN_WORLD_SIZE				75.f
#define NUM_SUN_FULL_FLARE_PIXELS	2500.f
#define NUM_SUN_SPLIT_FLARE_PIXELS	1000.f
#define SUN_LENIENCY				250


bool ZBufferOccluder::isAvailable()
{
	HRESULT hr = Moo::rc().device()->CreateQuery( D3DQUERYTYPE_OCCLUSION, NULL );
	return (hr==D3D_OK);
}


ZBufferOccluder::ZBufferOccluder():
	helper_( MAX_LENS_FLARES ),
	lastSunResult_( 0 ),
	sunThisFrame_( false )
{
	Moo::TextureStage ts[2];
	ts[0].colourOperation( Moo::TextureStage::SELECTARG1, Moo::TextureStage::TEXTURE_FACTOR, Moo::TextureStage::DIFFUSE );
	ts[0].alphaOperation( Moo::TextureStage::DISABLE );
	ts[1].colourOperation( Moo::TextureStage::DISABLE );
	ts[1].alphaOperation( Moo::TextureStage::DISABLE );
	pixelMat_.addTextureStage( ts[0] );
	pixelMat_.addTextureStage( ts[1] );
	pixelMat_.zBufferWrite( false );
	pixelMat_.zBufferRead( true );
	pixelMat_.doubleSided( true );
	pixelMat_.fogged( false );
	pixelMat_.textureFactor( 0xffff0000 );
}


ZBufferOccluder::~ZBufferOccluder()
{
}


void ZBufferOccluder::beginOcclusionTests()
{
	sunThisFrame_ = false;

	//reset used parameters.
	helper_.begin();	

	//set the material and rendering states.
	//- this assumes that while using the ZBuffer Occluder,
	//no other occluders use materials or device transforms.
	DX::Device* device = Moo::rc().device();
	static bool s_watchAdded = false;
	static bool s_viewFlareSources = false;
	static float s_pointSize = 3.f;
	if ( !s_watchAdded )
	{
		MF_WATCH ( "Render/View Flare Sources",
			s_viewFlareSources,
			Watcher::WT_READ_WRITE,
			"Visualise lens flare source geometry." );

		MF_WATCH ( "Render/Flare Source Size",
			s_pointSize,
			Watcher::WT_READ_WRITE,
			"Point size for lens flare source geometry." );

		s_watchAdded = true;
	}

	pixelMat_.set();
	Moo::rc().setRenderState( D3DRS_COLORWRITEENABLE, s_viewFlareSources ? D3DCOLORWRITEENABLE_RED: 0 );
	device->SetTransform( D3DTS_WORLD, &Matrix::identity );
	device->SetTransform( D3DTS_VIEW, &Moo::rc().view() );
	device->SetTransform( D3DTS_PROJECTION, &Moo::rc().projection() );
	Moo::rc().setVertexShader( NULL );
	Moo::rc().setFVF( Moo::VertexXYZ::fvf() );

	Moo::rc().setRenderState( D3DRS_POINTSCALEENABLE, FALSE );
	Moo::rc().setRenderState( D3DRS_POINTSIZE, *((DWORD*)&s_pointSize) );
}


/**
 *	This method checks the ray between the photon source
 *	and the eye, using the VisibilityTest function on the xbox.
 */
PhotonOccluder::Result ZBufferOccluder::collides( 
	const Vector3 & photonSourcePosition,
	const Vector3 & cameraPosition,
	const LensEffect& le )
{
	bool isTheSun = ( le.isSunFlare() || le.isSunSplit() );
	DX::Device* device = Moo::rc().device();

	PhotonOccluder::Result result = PhotonOccluder::NONE;

	//get result index for this lens flare
	OcclusionQueryHelper::Handle queryHandle;
	if ( isTheSun )
	{
		if ( !sunThisFrame_ )
		{
			queryHandle = helper_.handleFromId( 0 );
			sunThisFrame_ = true;
		}
		else
		{
			//don't allow multiple viz tests per frame for the sun.
			return PhotonOccluder::SOLID;
		}
	}
	else
		queryHandle = helper_.handleFromId( le.id() );


	//do the test for this frame
	if (helper_.beginQuery(queryHandle))
	{
		if (isTheSun)
		{
			//todo : don't special case the sun, just generalise area light sources			
			float nPixels = SUN_WORLD_SIZE * (Moo::rc().screenWidth() / 640.f);
			this->writeArea( photonSourcePosition,nPixels );
		}
		else
		{
			//write pixel
			this->writePixel( photonSourcePosition );
		}

		//end the test
		helper_.endQuery(queryHandle);		
	}
	else
	{
		ERROR_MSG( "Could not begin visibility test for lens flare\n" );
	}	

	if ( isTheSun )
	{
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
		}

		lastSunResult_ = helper_.avgResult(queryHandle);

		if ( lastSunResult_ > fullFlare )
		{
			//DEBUG_MSG( "Sun result - FULL  FLARE [idx %d]\n------------\n", queryHandle );
			return PhotonOccluder::NONE;
		}
		else if ( lastSunResult_ > splitFlare )
		{
			//DEBUG_MSG( "Sun result - SPLIT FLARE [idx %d]\n------------\n", queryHandle );
			return PhotonOccluder::DAPPLED;
		}
		else
		{
			//DEBUG_MSG( "Sun result - NO    FLARE [idx %d]\n------------\n", queryHandle );
			return PhotonOccluder::SOLID;
		}
	}
	else
	{
		return (helper_.avgResult(queryHandle) > 0) ? PhotonOccluder::NONE : PhotonOccluder::SOLID;
	}
}


void ZBufferOccluder::endOcclusionTests()
{
	//reset the colour writes ( see beginOcclusionTests )
	Moo::rc().setRenderState( D3DRS_COLORWRITEENABLE,
		D3DCOLORWRITEENABLE_RED | 
		D3DCOLORWRITEENABLE_GREEN | 
		D3DCOLORWRITEENABLE_BLUE | 
		D3DCOLORWRITEENABLE_ALPHA );

	helper_.end();
}


void ZBufferOccluder::writePixel( const Vector3& source )
{
	//write outs 4 pixels into the z-buffer, with no colour at all.
	Moo::VertexXYZ vert;
	vert.pos_ = source;

	Moo::rc().drawPrimitiveUP( D3DPT_POINTLIST, 1, &vert, sizeof( Moo::VertexXYZ ) );
}


void ZBufferOccluder::writeArea( const Vector3& source, float size )
{
	//write out a pixel into the z-buffer, with no colour at all.
	static CustomMesh<Moo::VertexXYZ> mesh( D3DPT_TRIANGLEFAN );
	mesh.clear();
	Moo::VertexXYZ vert;

	static float s_zFactor = 1.f;

	Vector3 src( source );
	src -= Moo::rc().invView().applyToUnitAxisVector(2) * s_zFactor;	//bring the sun viz. 1 metres inside the far plane.

	//this line accounts for the fact that due to the way we draw this mesh,
	//we have to scale the size compared to a farPlane default of 500.
	float fpFactor = Moo::rc().camera().farPlane() / 500.f;
	float h = (size / 2.f) * fpFactor;
	float w = h;
	Vector3 right = Moo::rc().invView().applyToUnitAxisVector(0) * w;
	Vector3 up = Moo::rc().invView().applyToUnitAxisVector(1) * h;
	vert.pos_ = src - right + up;
	mesh.push_back( vert );
	vert.pos_ = src + right + up;
	mesh.push_back( vert );
	vert.pos_ = src + right - up;
	mesh.push_back( vert );
	vert.pos_ = src - right - up;
	mesh.push_back( vert );

	mesh.draw();
}


/**
 *	This method releases all queries and resets the viz test status.
 *	It is called when the device is lost.
 *
 *	The entire class is reset
 */
void ZBufferOccluder::deleteUnmanagedObjects()
{	
	lastSunResult_ = 0;
	sunThisFrame_ = false;
}


std::ostream& operator<<(std::ostream& o, const ZBufferOccluder& t)
{
	o << "ZBufferOccluder\n";
	return o;
}

// z_buffer_occluder.cpp
