/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef SUN_AND_MOON_HPP
#define SUN_AND_MOON_HPP

#pragma warning ( disable: 4503 )

#include <iostream>
#include "custom_mesh.hpp"
#include "lens_effect.hpp"
#include "moo/material.hpp"

class TimeOfDay;
typedef SmartPointer< class ShaderSet > ShaderSetPtr;

class SunAndMoon
{
public:
	SunAndMoon();
	~SunAndMoon();

	void				timeOfDay( TimeOfDay* timeOfDay )		{ timeOfDay_ = timeOfDay; }

	void				create( void );
	void				destroy( void );

	void				draw();
	//call drawMoonMask in between drawStars() and drawGradient().
	void				drawMoonMask();

private:
	void				createMoon( void );
	void				createSun( void );

	void drawSquareMesh( Matrix & cam, CustomMesh<Moo::VertexXYZNDUV> * pMesh, Moo::Material * pMat );

	SunAndMoon(const SunAndMoon&);
	SunAndMoon& operator=(const SunAndMoon&);

	friend std::ostream& operator<<(std::ostream&, const SunAndMoon&);

	CustomMesh<Moo::VertexXYZNDUV>*	moonMask_;
	CustomMesh<Moo::VertexXYZNDUV>*	moon_;
	CustomMesh<Moo::VertexXYZNDUV>*	sun_;

	Moo::Material*		moonMaskMat_;
	Moo::Material*		moonMat_;
	Moo::Material*		sunMat_;
	ShaderSetPtr		sunMoonSet_;

	TimeOfDay*			timeOfDay_;

	typedef std::vector<LensEffect>	LensEffects;
	LensEffects			sunLensEffects_;
	LensEffects			moonLensEffects_;
};

#ifdef CODE_INLINE
#include "sun_and_moon.ipp"
#endif




#endif
/*sun_and_moon.hpp*/
