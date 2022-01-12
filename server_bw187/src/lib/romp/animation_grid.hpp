/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef ANIMATIONGRID_HPP
#define ANIMATIONGRID_HPP

#include "math/perlin_noise.hpp"

/**
 *	This interface sets vertex shader constants
 *	that correspond to a grid of animation values.
 */
class AnimationGrid
{
public:
	virtual ~AnimationGrid()	{};
	//set vertex shader constants.
	virtual void set( int constantBase ) = 0;
	virtual void update( float dTime ) = 0;
};


/**
 *	This class creates a grid of animation values
 *	for the flora vertex shader.
 *
 *	It blends perlin noise in with a wind amount,
 *	that is settable using the watchers ( or python
 *	watchers interface ).
 *
 *	The watchers controlling wind are in
 *	"Client Settings/Flora/Wind X"
 *	"Client Settings/Flora/Wind Z"
 */
class FloraAnimationGrid : public AnimationGrid
{
public:
	FloraAnimationGrid( int width, int height );
	~FloraAnimationGrid();

	virtual void set( int constantBase );
	virtual void update( float dTime );
private:
	Vector4*	constants_;
	float*		noise_;
	Vector4*	direction_;
	int			nConstants_;
	int			width_;
	int			height_;
	double		tTime_;
	PerlinNoise perlin_;
};

#endif