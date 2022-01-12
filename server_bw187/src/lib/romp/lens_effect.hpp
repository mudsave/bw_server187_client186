/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef LENS_EFFECT_HPP
#define LENS_EFFECT_HPP

#include <iostream>
#include "moo/moo_math.hpp"
#include "resmgr/datasection.hpp"

///This #define determines how quickly lens effects fade in/out
#define OLDEST_LENS_EFFECT 0.15f


/**
 *	This class holds the properties of a lens effect, and
 *	performs tick/draw logic.
 */
class LensEffect
{
public:
	LensEffect();

	virtual bool load( DataSectionPtr pSection );
	virtual bool save( DataSectionPtr pSection );

	float	age( void ) const;
	void	age( float a );

	uint32	colour( void ) const;
	void	colour( uint32 c );

	uint32	id( void ) const;
	void	id( uint32 _id );

	const Vector3 & position( void ) const;
	void	position( const Vector3 & pos );

	float	size( void ) const;
	void	size( float s );

	float	width( void ) const;
	void	width( float w );

	float	height( void ) const;
	void	height( float h );

	const std::string & material( void ) const;
	void	material( const std::string &  m );

	float	maxDistance() const;

	float	clipDepth( void ) const;
	void	clipDepth( float d );

	uint32	added() const;
	void	added( uint32 when );

	virtual void tick( float dTime );
	virtual void draw( void );

	//special case sun flare properties
	bool	isSunFlare( void ) const;
	bool	isSunSplit( void ) const;

	int operator == ( const LensEffect & other );

protected:
	void	drawFlare( const Vector4 & clipPos, float age, float scale );

	float			age_;
	uint32			id_;
	Vector3			position_;

	uint32			colour_;
	std::string		material_;
	float			clipDepth_;
	float			width_;
	float			height_;
	float			maxDistance_;

	uint32			added_;

	bool			isSunFlare_;
	bool			isSunSplit_;

	std::vector<class LensEffect>	secondaries_;

	friend std::ostream& operator<<(std::ostream&, const LensEffect&);
};

#ifdef CODE_INLINE
#include "lens_effect.ipp"
#endif




#endif // LENS_EFFECT_HPP
