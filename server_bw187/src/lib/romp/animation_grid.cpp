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
#include "animation_grid.hpp"
#include "math/perlin_noise.hpp"
#include "flora_vertex_type.hpp"
#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT2( "romp", 0 )

static float s_noiseFactor = 0.25f;
static float s_windX = 1.f;
static float s_windZ = 1.f;

/**
 *	This class exposes a 64x4 matrix to the effect file engine, which
 *	provides perlin noise animation vectors
 */
class AnimationGridSetter : public Moo::EffectConstantValue, public Aligned
{
public:
	AnimationGridSetter( Vector4* grid, int size ):
		grid_( grid ),
		size_( size )
	{
	}

	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{		
		pEffect->SetVectorArray(constantHandle, grid_, size_);
		return true;
	}

private:
	Vector4* grid_;
	uint32 size_;
};


FloraAnimationGrid::FloraAnimationGrid( int width, int height ):
	width_( width ),
	height_( height ),
	tTime_( 0.0 )
{
	nConstants_ = width_*height_;
	constants_ = new Vector4[nConstants_];
	noise_ = new float[nConstants_];
	direction_ = new Vector4[nConstants_];	
	*Moo::EffectConstantValue::get( "FloraAnimationGrid" ) =
		new AnimationGridSetter(constants_,nConstants_);

	//Create the watchers once only
	static bool s_createdWatchers = false;
	if (!s_createdWatchers)
	{
		MF_WATCH( "Client Settings/Flora/Noise Factor", s_noiseFactor, Watcher::WT_READ_WRITE, "Multiplier for flora noise animation grid.  Affects overall flora movement." );
		MF_WATCH( "Client Settings/Flora/Wind X", s_windX, Watcher::WT_READ_WRITE, "Multiplier for world X direction of flora animation." );
		MF_WATCH( "Client Settings/Flora/Wind Z", s_windZ, Watcher::WT_READ_WRITE, "Multiplier for world Z direction of flora animation." );
		s_createdWatchers = true;
	}
}


FloraAnimationGrid::~FloraAnimationGrid()
{
	delete[] constants_;
	delete[] noise_;
	delete[] direction_;
}


void FloraAnimationGrid::set( int constantBase )
{
	Moo::rc().device()->SetVertexShaderConstantF( constantBase, Vector4( 0.f, 0.f, 255.f, 0.f ), 1 );
	Moo::rc().device()->SetVertexShaderConstantF( constantBase+1, (float*)constants_, nConstants_ );
}


void FloraAnimationGrid::update( float dTime )
{
	tTime_ += (double)dTime;
	float animationTime = (float)( fmod( tTime_, 1000.0 ) );

	//TODO : this function just animates the x components for now.
	for ( int z=0; z<height_; z++ )
	{
		//remember - 4 values per x value ( a constant is a vector4 )
		for ( int x=0; x<width_; x++ )
		{
			int idx = x+(z*width_);

			Vector4* pVec = &constants_[x+(z*width_)];
			noise_[idx] = perlin_.noise3( Vector3((float)x,(float)z,animationTime) );
			noise_[idx] *= (-1.f * s_noiseFactor);
			pVec->x = noise_[idx] * s_windX;
			pVec->y = 0.f;
			pVec->z = noise_[idx] * s_windZ;
			pVec->w = 0.f;
		}
	}
}