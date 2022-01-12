/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// lens_effect.ipp

#ifdef CODE_INLINE
#define INLINE inline
#else
#define INLINE
#endif


/**
 *	This method returns the age of the lens effect
 */
INLINE float LensEffect::age( void ) const
{
	return age_;
}


/**
 * @see age
 */
INLINE void LensEffect::age( float a )
{
	age_ = a;
}


/**
 *	This method returnss the colour of the lens effect
 */
INLINE uint32 LensEffect::colour( void ) const
{
	return colour_;
}


/**
 * @see colour
 */
INLINE void	LensEffect::colour( uint32 c )
{
	colour_ = c;
}


/**
 * This method returns the id of the lens effect
 */
INLINE uint32 LensEffect::id( void ) const
{
	return id_;
}


/**
 * @see id
 */
INLINE void	LensEffect::id( uint32 _id )
{
	id_ = _id;
}


/**
 *	This method returns the world position of the lens effect
 */
INLINE const Vector3 & LensEffect::position( void ) const
{
	return position_;
}


/**
 * @see position
 */
INLINE void	LensEffect::position( const Vector3 & pos )
{
	position_ = pos;
}


/**
 * This method returns the size, in clip space, of the lens effect
 * Note that a lens effect doesn't really have a size,
 * just a width and height.  Size() returns the larger of the two.	
 */
INLINE float LensEffect::size( void ) const
{
	return ( width() > height() ) ? width() : height();
}


/**
 * @see size
 */
INLINE void	LensEffect::size( float s )
{
	this->width( s );
	this->height( s );
}


/**
 * This method returns the material of the lens effect
 */
INLINE const std::string & LensEffect::material( void ) const
{
	return material_;
}


/**
 *	This method sets the clip depth of the flare.
 *	The clip depth is the amount the clip position
 *	of the flare is scaled by; useful for multi-flare
 *	effects, like sun through a telephoto lens
 */
INLINE float LensEffect::clipDepth( void ) const
{
	return clipDepth_;
}


/**
 *	@see clipDepth
 */
INLINE void	LensEffect::clipDepth( float d )
{
	clipDepth_ = d;
}


/**
 *	This method gets the width of the flare.
 *	@see size
 */
INLINE float LensEffect::width( void ) const
{
	return width_;
}


/**
 *	@see width
 */
INLINE void LensEffect::width( float w )
{
	width_ = w;
}


/**
 *	This method gets the height of the flare.
 *	@see size
 */
INLINE float LensEffect::height( void ) const
{
	return height_;
}


/**
 *	@see height
 */
INLINE void LensEffect::height( float h )
{
	height_ = h;
}


/**
 *	This method gets the counter when this flare was last added.
 */
INLINE uint32 LensEffect::added( void ) const
{
	return added_;
}


/**
 *	@see added
 */
INLINE void LensEffect::added( uint32 when )
{
	added_ = when;
}




/**
 *	This method returns true if this flare has been
 *	identified as being the special case sun flare.
 *
 *	@todo remove special casing of sun flares.
 */
INLINE bool LensEffect::isSunFlare( void ) const
{
	return isSunFlare_;
}


/**
 *	This method returns true if this flare has been
 *	identified as being the special case sun split flare.
 *
 *	@todo remove special casing of sun flares.
 */
INLINE bool LensEffect::isSunSplit( void ) const
{
	return isSunSplit_;
}


/**
 *	This method returns the maximum distance this flare
 *	will be visible at.
 */
INLINE float LensEffect::maxDistance() const
{
	return maxDistance_;
}



// lens_effect.ipp