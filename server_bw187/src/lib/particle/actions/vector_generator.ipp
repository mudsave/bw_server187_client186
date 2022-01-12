/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifdef CODE_INLINE
#define INLINE inline
#else
#define INLINE
#endif


// -----------------------------------------------------------------------------
// Section: VectorGenerator
// -----------------------------------------------------------------------------

/**
 *	This is the destructor for Vector Generator.
 */
INLINE VectorGenerator::~VectorGenerator()
{
}


// -----------------------------------------------------------------------------
// Section: PointVectorGenerator
// -----------------------------------------------------------------------------

/**
 *	This is the constructor for PointVectorGenerator.
 *
 *	@param	point	Point from where the vectors are generated.
 */
INLINE PointVectorGenerator::PointVectorGenerator( const Vector3 &point )
{
	position(point);
}

/**
 *	This method generates the vectors for PointVectorGenerator.
 *
 *	@param	vector	Vector to be over-written with the new vector.
 */
INLINE void PointVectorGenerator::generate( Vector3 &vector ) const
{
	vector = position_;
}


// -----------------------------------------------------------------------------
// Section: LineVectorGenerator
// -----------------------------------------------------------------------------

/**
 *	This is the constructor for LineVectorGenerator.
 *
 *	@param start	The start point of the line interval.
 *	@param end		The end point of the line interval.
 */
INLINE LineVectorGenerator::LineVectorGenerator( const Vector3 &Start,
		const Vector3 &End )
{
	start(Start);
	end(End);
}

/**
 *	This method generates the vectors for LineVectorGenerator.
 *
 *	@param	vector	Vector to be over-written with the new vector.
 */
INLINE void LineVectorGenerator::generate( Vector3 &vector ) const
{
	vector = origin_ + unitRand() * direction_;
}


// -----------------------------------------------------------------------------
// Section: BoxVectorGenerator
// -----------------------------------------------------------------------------

/**
 *	This is the constructor for BoxVectorGenerator.
 *
 *	@param corner			One corner of the box.
 *	@param oppositeCorner	The opposite corner of the box.
 */
INLINE BoxVectorGenerator::BoxVectorGenerator( const Vector3 &Corner,
		const Vector3 &OppositeCorner )
{
	corner(Corner);
	oppositeCorner(OppositeCorner);
}


/* vector_generator.ipp */