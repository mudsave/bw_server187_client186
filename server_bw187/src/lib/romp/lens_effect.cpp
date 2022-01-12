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

#include "lens_effect.hpp"
#include "lens_effect_manager.hpp"
#include "cstdmf/debug.hpp"
#include "moo/render_context.hpp"
#include "math/colour.hpp"
#include "resmgr/auto_config.hpp"

DECLARE_DEBUG_COMPONENT2( "Romp", 2 )

#ifndef CODE_INLINE
#include "lens_effect.ipp"
#endif

static Moo::VertexTLUV tv_[4];
Vector2 g_lensFlareClipSpaceOffset(0.f,0.f);

/**
 *	This global method specifies the resources required by this file
 */
static AutoConfigString s_sunFlareName( "environment/sunFlareMaterial" );
static AutoConfigString s_sunSplitName( "environment/sunSplitMaterial" );

///constructor
LensEffect::LensEffect() :
	age_( OLDEST_LENS_EFFECT ),
	id_( 0 ),
	position_( 0, 0, 0 ),
	colour_( 0 ),
	material_( "" ),
	clipDepth_( 1.f ),
	width_( 0.1f ),
	height_( 0.1f ),
	maxDistance_( 150.f ),
	added_( LensEffectManager::s_drawCounter_ - 16 ),
	isSunSplit_( false ),
	isSunFlare_( false )
{
}


/**
 *	This method loads the lens effect's properties
 *	from a data section pointer
 */
bool LensEffect::load( DataSectionPtr pSection )
{
	if ( !pSection )
		return false;

	secondaries_.clear();

	//load our stuff
	this->material( pSection->readString( "type", "" ) );
	this->size( pSection->readFloat( "size", 1.f ) );
	this->width( pSection->readFloat( "width", this->width() ) );
	this->height( pSection->readFloat( "height", this->height() ) );
	this->clipDepth( pSection->readFloat( "depth", 1.f ) );
	this->colour( Colour::getUint32( pSection->readVector4( "rgba", Vector4( 255.f, 255.f, 255.f, 255.f ) ) ) );
	this->maxDistance_ = pSection->readFloat( "maxDistance", 150.f );

	//preload our texture
	LensEffectManager::instance().preload( *this );

	//load any secondary flares
	DataSectionPtr secondariesSection = pSection->openSection( "secondaries" );
	if ( secondariesSection )
	{
		std::vector<DataSectionPtr> secondaries;
		secondariesSection->openSections( "Flare", secondaries );

		std::vector<DataSectionPtr>::iterator it = secondaries.begin();
		std::vector<DataSectionPtr>::iterator end = secondaries.end();

		while ( it != end )
		{
			DataSectionPtr & flareSection = *it++;

			LensEffect secondary;
			if ( secondary.load( flareSection ) )
			{
				secondaries_.push_back( secondary );
			}
		}
	}

	return true;
}


/**
 *	This method saves the lens effect's properties
 *	to a data section pointer.
 */
bool LensEffect::save( DataSectionPtr pSection )
{
	//TODO : implement
	return false;
}


/**
 * @see type
 */
void LensEffect::material( const std::string & m )
{
	material_ = m;

	isSunFlare_ = ( m == s_sunFlareName.value() );
	isSunSplit_ = ( m == s_sunSplitName.value() );
}


/**
 *	This method updates the lens effect's age by the passed in
 *	delta time.
 *
 *	@param dTime	the change in time from the last frame
 */
void LensEffect::tick( float dTime )
{
	if ( isSunFlare_ || isSunSplit_ )
		this->age_ += dTime;
	else
	{
		if ( dTime > 0 )
			this->age_ += OLDEST_LENS_EFFECT/2.f;
		else
			this->age_ -= OLDEST_LENS_EFFECT/10.f;
	}

	if ( this->age_ < 0.f )
		age_ = 0.f;
}


/**
 *	This method draws the lens effect.
 */
void LensEffect::draw( void )
{
	if ( age_ >= 0.f && age_ < OLDEST_LENS_EFFECT )
	{
		float age = 1.f - ( age_ / OLDEST_LENS_EFFECT );

		//calculate distance and projected postiion of flare
		Vector4 projPos( position().x, position().y, position().z, 1.f );
		Moo::rc().viewProjection().applyPoint( projPos, projPos );

		if ( projPos.w > 0.f )
		{
			float oow = 1.f / projPos.w;
			projPos.x *= oow;
			projPos.y *= oow;
			projPos.z *= oow;
			projPos.w = oow;

			//calculate distance of flare
			float dist = Vector3( position()- Moo::rc().invView().applyToOrigin() ).length();

			float scale = 1.f;
			if ( !isSunFlare_ && !isSunSplit_ )
			{
				float t = dist/this->maxDistance();
				if ( t > 1.f )
					t = 1.f;
				if ( t < 0.f )
					t = 0.f;
				scale = 1.f - ( t*t );
			}		

			if ( scale > 0.f )
			{
				this->drawFlare( projPos, age, scale );

				//draw secondaries
				std::vector<LensEffect>::iterator it = secondaries_.begin();
				std::vector<LensEffect>::iterator end = secondaries_.end();

				while ( it != end )
				{
					LensEffect & le = *it++;

					le.drawFlare( projPos, age, scale );
				}
			}
		}
	}
}


/**
 *	This method draws a square mesh, using a given material.
 *	Assumes rendering states have been setup correctly.
 *
 *	@param clipPos	the position in clip space of the flare
 *	@param age		the age of the flare, used for fading in/out
 *	@param scale	the scale to apply to the flare's opacity (alpha)
 */
void LensEffect::drawFlare( const Vector4 & clipPos, float age, float scale )
{
	Moo::EffectMaterial* material = LensEffectManager::instance().getMaterial( *this );
	if ( !material )
		return;

	Vector4 projPos = clipPos;
	projPos.z = 0;
	projPos.w = 1;

	//the clipSpaceOffset is used to correct problems in the big screenshot code.
	//it says how far displaced the virtual clip space origin is from the current origin.
	projPos.x = (projPos.x - g_lensFlareClipSpaceOffset.x) * clipDepth() + g_lensFlareClipSpaceOffset.x;
	projPos.y = (projPos.y - g_lensFlareClipSpaceOffset.y) * clipDepth() + g_lensFlareClipSpaceOffset.y;

	float halfScreenWidth = Moo::rc().screenWidth() / 2.f;
	float halfScreenHeight = Moo::rc().screenHeight() / 2.f;

	//Create a spin-to-face poly
	float xExtent = Moo::rc().projection().m[0][0];
	float yExtent = Moo::rc().projection().m[1][1];

	int alpha = ( colour() & 0xff000000 ) >> 24;
	alpha = (int)(age * (float)alpha * scale);
	int colour = (alpha << 24) | ( this->colour() & 0x00ffffff);

	tv_[0].colour_ = colour;
	tv_[1].colour_ = colour;
	tv_[2].colour_ = colour;
	tv_[3].colour_ = colour;

	float effectXExtent = xExtent * width();
	float effectYExtent = yExtent * height();

	tv_[0].pos_.x = projPos.x - effectXExtent;
	tv_[0].pos_.y = projPos.y - effectYExtent;
	tv_[0].pos_.z = projPos.z;
	tv_[0].pos_.w = projPos.w;
	tv_[0].uv_.set( 0.f, 1.f );

	tv_[1].pos_.x = projPos.x - effectXExtent;
	tv_[1].pos_.y = projPos.y + effectYExtent;
	tv_[1].pos_.z = projPos.z;
	tv_[1].pos_.w = projPos.w;
	tv_[1].uv_.set( 0.f, 0.f );

	tv_[2].pos_.x = projPos.x + effectXExtent;
	tv_[2].pos_.y = projPos.y - effectYExtent;
	tv_[2].pos_.z = projPos.z;
	tv_[2].pos_.w = projPos.w;
	tv_[2].uv_.set( 1.f, 1.f );

	tv_[3].pos_.x = projPos.x + effectXExtent;
	tv_[3].pos_.y = projPos.y + effectYExtent;
	tv_[3].pos_.z = projPos.z;
	tv_[3].pos_.w = projPos.w;
	tv_[3].uv_.set( 1.f, 0.f );

	//mult by half screen width / height
	for ( int i = 0; i < 4; i++ )
	{
		tv_[i].pos_.x *= halfScreenWidth;		tv_[i].pos_.y *= -halfScreenHeight;
		tv_[i].pos_.x += halfScreenWidth;		tv_[i].pos_.y += halfScreenHeight;
	}

	Moo::rc().setFVF( Moo::VertexTLUV::fvf() );
	material->begin();
	for ( uint32 i=0; i<material->nPasses(); i++ )
	{
		material->beginPass(i);
		Moo::rc().drawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, tv_, sizeof(tv_[0]) );
		material->endPass();
	}
	material->end();
}


std::ostream& operator<<(std::ostream& o, const LensEffect& t)
{
	o << "LensEffect\n";
	return o;
}

// lens_effect.cpp
