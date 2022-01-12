/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef MSGTYPES_HPP
#define MSGTYPES_HPP

#include <float.h>

#include "basictypes.hpp"
#include "math/mathdef.hpp"
#include "cstdmf/binary_stream.hpp"

// -----------------------------------------------------------------------------
// Section: Uint
// -----------------------------------------------------------------------------

#if 0
/**
 * 	@internal
 * 	This class is used to pack an unsigned integer into a given
 * 	number of bytes. The template SIZE argument is the number
 * 	of bytes to use.
 */
template <int SIZE, class RETURN_TYPE = unsigned int>
class Uint
{
	public:
		/// Maximum value of this integer type.
		static float MAX_VALUE()	{ return (1 << (SIZE * 8)); }

		/// Number of bits - 1 in this float.
		static int SHIFT()			{ return (8 * (SIZE - 1)); }

	public:
		/// Constructor.
		Uint(RETURN_TYPE x) :
			highByte_(x >> SHIFT()),
			lowBytes_(x)
		{
		}

		/// Default constructor.
		Uint()
		{
		}

		/// This method returns the object as a standard int.
		operator RETURN_TYPE() const
		{
			return (highByte_ << SHIFT()) + lowBytes_;
		}

	private:
		unsigned char highByte_;
		Uint<SIZE - 1> lowBytes_;
};

/**
 * 	@internal
 * 	An integer template for integers of 1 byte. This is needed
 * 	because Uint is a recursive template.
 */
template <>
class Uint<1>
{
	public:
		/// This is the constructor.
		Uint(unsigned int x) : byte_(x) {};

		/// This is the destructor.
		Uint() {};

		/// This method returns the object as a standard int.
		operator unsigned int() const { return byte_; }

	private:
		unsigned char byte_;
};

// -----------------------------------------------------------------------------
// Section: Float
// -----------------------------------------------------------------------------

/**
 * 	@internal
 * 	This class is used to pack a float into a given
 * 	number of bytes. The template SIZE argument is the number
 * 	of bytes to use.
 */
template <int SIZE>
class Float
{
	public:
		/// The integer representation.
		typedef Uint<SIZE> Data;

		/// The minimum floating point value that can be expressed.
		static const float FLOAT_MIN()			{ return -13000.f; }

		/// The maximum floating point value that can be expressed.
		static const float FLOAT_MAX()			{ return  13000.f; }

		/// The range of floating point values that can be expressed.
		static const float Range()				{ return  FLOAT_MAX() - FLOAT_MIN(); }

	public:
		/// The constructor.
		Float(float x) :
			data_((int)((x - FLOAT_MIN()) * Data::MAX_VALUE()/Range() + 0.5f))
		{};

		/// The destructor.
		Float()
		{};

		/// Operator to return this object as a normal float.
		operator float() const
		{
			return Range() * float((unsigned int)data_) /
				(float)Data::MAX_VALUE() + FLOAT_MIN();
		}

	private:
		Data data_;
};
#endif


// -----------------------------------------------------------------------------
// Section: Coordinate
// -----------------------------------------------------------------------------

/**
 * 	This class is used to pack a 3d vector for network transmission.
 *
 * 	@ingroup network
 */
class Coord
{
//	typedef Float<3> Storage;
	typedef float Storage;

	public:
		/// Construct from x,y,z.
		Coord(float x, float y, float z) : x_(x), y_(y), z_(z)
		{}

		/// Construct from a Vector3.
		Coord( const Vector3 & v ) : x_(v.x), y_(v.y), z_(v.z)
		{}

		/// Default constructor.
		Coord()
		{}

		bool operator==( const Coord & oth ) const
		{ return x_ == oth.x_ && y_ == oth.y_ && z_ == oth.z_; }

	Storage x_; 	///< The x component of the coordinate.
	Storage y_;		///< The y component of the coordinate.
	Storage z_;		///< The z component of the coordinate.
};

inline BinaryIStream& operator>>( BinaryIStream &is, Coord &c )
{
	return is >> c.x_ >> c.y_ >> c.z_;
}

inline BinaryOStream& operator<<( BinaryOStream &os, const Coord &c )
{
	return os << c.x_ << c.y_ << c.z_;
}


/**
 *	This method is used to convert an angle in the range [-pi, pi) to an int8.
 *
 *	@see int8ToAngle
 */
inline int8 angleToInt8( float angle )
{
	return (int8)floorf( (angle * 128.f) / MATH_PI + 0.5f );
}


/**
 *	This method is used to convert an compressed angle to an angle in the range
 *	[-pi, pi).
 *
 *	@see angleToInt8
 */
inline float int8ToAngle( int8 angle )
{
	return float(angle) * (MATH_PI / 128.f);
}


/**
 *	This method is used to convert an angle in the range [-pi/2, pi/2) to an
 *	int8.
 *
 *	@see int8ToHalfAngle
 */
inline int8 halfAngleToInt8( float angle )
{
	return (int8)floorf( (angle * 256.f) / MATH_PI + 0.5f );
}


/**
 *	This method is used to convert an compressed angle to an angle in the range
 *	[-pi/2, pi/2).
 *
 *	@see halfAngleToInt8
 */
inline float int8ToHalfAngle( int8 angle )
{
	return float(angle) * (MATH_PI / 256.f);
}


/**
 *	This class is used to pack a yaw and pitch value for network transmission.
 *
 *	@ingroup network
 */
class YawPitch
{
public:
	YawPitch( float yaw, float pitch )
	{
		this->set( yaw, pitch );
	}

	YawPitch() {};

	void set( float yaw, float pitch )
	{
		yaw_   = angleToInt8( yaw );
		pitch_ = halfAngleToInt8( pitch );
	}

	void get( float & yaw, float & pitch ) const
	{
		yaw   = int8ToAngle( yaw_ );
		pitch = int8ToHalfAngle( pitch_ );
	}

	friend BinaryIStream& operator>>( BinaryIStream& is, YawPitch &yp );
	friend BinaryOStream& operator<<( BinaryOStream& os, const YawPitch &yp );

private:
	uint8	yaw_;
	uint8	pitch_;
};

// Streaming operators
inline BinaryIStream& operator>>( BinaryIStream& is, YawPitch &yp )
{
	return is >> yp.yaw_ >> yp.pitch_;
}


inline BinaryOStream& operator<<( BinaryOStream& os, const YawPitch &yp )
{
	return os << yp.yaw_ << yp.pitch_;
}



/**
 *	This class is used to pack a yaw, pitch and roll value for network
 *	transmission.
 *
 *	@ingroup network
 */
class YawPitchRoll
{
public:
	YawPitchRoll( float yaw, float pitch, float roll )
	{
		this->set( yaw, pitch, roll );
	}

	YawPitchRoll() {};

	bool operator==( const YawPitchRoll & oth ) const
	{
		return yaw_ == oth.yaw_ && pitch_ == oth.pitch_ && roll_ == oth.roll_;
	}

	void set( float yaw, float pitch, float roll )
	{
		yaw_   = angleToInt8( yaw );
		pitch_ = halfAngleToInt8( pitch );
		roll_ = halfAngleToInt8( roll );
	}

	void get( float & yaw, float & pitch, float & roll ) const
	{
		yaw   = int8ToAngle( yaw_ );
		pitch = int8ToHalfAngle( pitch_ );
		roll = int8ToHalfAngle( roll_ );
	}

	bool nearTo( const YawPitchRoll & oth ) const
	{
		return uint( (yaw_-oth.yaw_+1) | (pitch_-oth.pitch_+1) |
			(roll_-oth.roll_+1) ) <= 3;
	}

	friend BinaryIStream& operator>>( BinaryIStream& is, YawPitchRoll &ypr );
	friend BinaryOStream& operator<<( BinaryOStream& os,
		const YawPitchRoll &ypr );

private:
	uint8	yaw_;
	uint8	pitch_;
	uint8	roll_;
};

inline BinaryIStream& operator>>( BinaryIStream &is, YawPitchRoll &ypr )
{
	return is >> ypr.yaw_ >> ypr.pitch_ >> ypr.roll_;
}

inline BinaryOStream& operator<<( BinaryOStream &os, const YawPitchRoll &ypr )
{
	return os << ypr.yaw_ << ypr.pitch_ << ypr.roll_;
}

/**
 *	This class is used to store a packed x and z coordinate.
 */
class PackedXZ
{
protected:
	/** @internal */
	union MultiType
	{
		float	asFloat;
		uint	asUint;
		int		asInt;
	};

public:
	PackedXZ() {};
	PackedXZ( float x, float z );

	void packXZ( float x, float z );
	void unpackXZ( float & x, float & z ) const;

	friend BinaryIStream& operator>>( BinaryIStream& is, PackedXZ &xz );
	friend BinaryOStream& operator<<( BinaryOStream& os, const PackedXZ &xz );

private:
	unsigned char data_[3];
};


/**
 *	Constructor.
 */
inline PackedXZ::PackedXZ( float x, float z )
{
	this->packXZ( x, z );
}


/**
 *	This function compresses the two input floats into the low 3 bytes of the
 *	return value. It does this as follows:
 *
 *	Bits 12-23 can be thought of as a 12-bit float storing the x-value.
 *	Bits 0-11 can be thought of as a 12-bit float storing the z-value.
 *	Bits 0-7 are the mantissa, bits 8-10 are the unsigned exponent and bit 11 is
 *	the sign bit.
 *
 *	The input values must be in the range (-510, 510).
 */
inline void PackedXZ::packXZ( float xValue, float zValue )
{
	const float addValues[] = { 2.f, -2.f };
	const uint32 xCeilingValues[] = { 0, 0x7ff000 };
	const uint32 zCeilingValues[] = { 0, 0x0007ff };

	PackedXZ::MultiType x; x.asFloat = xValue;
	PackedXZ::MultiType z; z.asFloat = zValue;

	// We want the value to be in the range (-512, -2], [2, 512). Take 2 from
	// negative numbers and add 2 to positive numbers. This is to make the
	// exponent be in the range [2, 9] which for the exponent of the float in
	// IEEE format is between 10000000 and 10000111.

	x.asFloat += addValues[ x.asInt < 0 ];
	z.asFloat += addValues[ z.asInt < 0 ];

	uint result = 0;

	// Here we check if the input values are out of range. The exponent is meant
	// to be between 10000000 and 10000111. We check that the top 5 bits are
	// 10000.
	// We also need to check for the case that the rounding causes an overflow.
	// This occurs when the bits of the exponent and mantissa we are interested
	// in are all 1 and the next significant bit in the mantissa is 1.
	// If an overflow occurs, we set the result to the maximum result.
	result |= xCeilingValues[((x.asUint & 0x7c000000) != 0x40000000) ||
										((x.asUint & 0x3ffc000) == 0x3ffc000)];
	result |= zCeilingValues[((z.asUint & 0x7c000000) != 0x40000000) ||
										((z.asUint & 0x3ffc000) == 0x3ffc000)];


	// Copy the low 3 bits of the exponent and the high 8 bits of the mantissa.
	// These are the bits 15 - 25. We then add one to this value if the high bit
	// of the remainder of the mantissa is set. It magically works that if the
	// mantissa wraps around, the exponent is incremented by one as required.
	result |= ((x.asUint >>  3) & 0x7ff000) + ((x.asUint & 0x4000) >> 2);
	result |= ((z.asUint >> 15) & 0x0007ff) + ((z.asUint & 0x4000) >> 14);

	// We only need this for values in the range [509.5, 510.0). For these
	// values, the above addition overflows to the sign bit.
	result &= 0x7ff7ff;

	// Copy the sign bit (the high bit) from the values to the result.
	result |=  (x.asUint >>  8) & 0x800000;
	result |=  (z.asUint >> 20) & 0x000800;

	BW_PACK3( (char*)data_, result );
}



/**
 *	This function uncompresses the values that were created by pack.
 */
inline void PackedXZ::unpackXZ( float & xarg, float & zarg ) const
{
	uint data = BW_UNPACK3( (const char*)data_ );

	MultiType & xu = (MultiType&)xarg;
	MultiType & zu = (MultiType&)zarg;

	// The high 5 bits of the exponent are 10000. The low 17 bits of the
	// mantissa are all clear.
	xu.asUint = 0x40000000;
	zu.asUint = 0x40000000;

	// Copy the low 3 bits of the exponent and the high 8 bits of the mantissa
	// into the results.
	xu.asUint |= (data & 0x7ff000) << 3;
	zu.asUint |= (data & 0x0007ff) << 15;

	// The produced floats are in the range (-512, -2], [2, 512). Change this
	// back to the range (-510, 510). [Note: the sign bit is not on yet.]
	xu.asFloat -= 2.0f;
	zu.asFloat -= 2.0f;

	// Copy the sign bit across.
	xu.asUint |= (data & 0x800000) << 8;
	zu.asUint |= (data & 0x000800) << 20;
}

// Streaming operators
inline BinaryIStream& operator>>( BinaryIStream& is, PackedXZ &xz )
{
	MF_ASSERT( sizeof( xz.data_ ) == 3 );
	void *src = is.retrieve( sizeof( xz.data_ ) );
	BW_NTOH3_ASSIGN( (char*)xz.data_, (const char*)src );
	return is;
}


inline BinaryOStream& operator<<( BinaryOStream& os, const PackedXZ &xz )
{
	MF_ASSERT( sizeof( xz.data_ ) == 3 );
	void *dest = os.reserve( sizeof( xz.data_ ) );
	BW_HTON3_ASSIGN( (char*)dest, (const char*)xz.data_ );
	return os;
}


// -----------------------------------------------------------------------------
// Section: class PackedXHZ
// -----------------------------------------------------------------------------

// TODO: This is just a placeholder at the moment. The idea is that we send down
// a message that identifies the chunk that the entity is in and the approx.
// height within that chunk so that the client can find the correct level.
/**
 * TODO: to be documented.
 */
class PackedXHZ : public PackedXZ
{
public:
	PackedXHZ() : height_( 0 ) {};
private:
	int8 height_;
};


// -----------------------------------------------------------------------------
// Section: class PackedXYZ
// -----------------------------------------------------------------------------

/**
 *	This class is used to store a packed x, y, z coordinate. At the moment, it
 *	stores the y value as a 16 bit float with a 4 bit exponent and 11 bit
 *	mantissa. This is a bit of overkill. It can handle values in the range
 *	(-131070.0, 131070.0)
 */
class PackedXYZ : public PackedXZ
{
public:
	PackedXYZ();
	PackedXYZ( float x, float y, float z );

	void packXYZ( float x, float y, float z );
	void unpackXYZ( float & x, float & y, float & z ) const;

	void y( float value );
	float y() const;

	friend BinaryIStream& operator>>( BinaryIStream& is, PackedXYZ &xyz );
	friend BinaryOStream& operator<<( BinaryOStream& is, const PackedXYZ &xyz );

private:
	unsigned char yData_[2];
};


/**
 *	Constructor.
 */
inline PackedXYZ::PackedXYZ()
{
}


/**
 *	Constructor.
 */
inline PackedXYZ::PackedXYZ( float x, float y, float z )
{
	this->packXZ( x, z );
	this->y( y );
}


/**
 *	This method packs the three input floats into this object.
 */
inline void PackedXYZ::packXYZ( float x, float y, float z )
{
	this->y( y );
	this->packXZ( x, z );
}


/**
 *	This method unpacks this object into the three input floats.
 */
inline void PackedXYZ::unpackXYZ( float & x, float & y, float & z ) const
{
	y = this->y();
	this->unpackXZ( x, z );
}


/**
 *	This method sets the packed y value from a float.
 */
inline void PackedXYZ::y( float yValue )
{
	// TODO: no range checking is done on the input value or rounding of the
	// result.

	// Add bias to the value to force the floating point exponent to be in the
	// range [2, 15], which translates to an IEEE754 exponent in the range
	// 10000000 to 1000FFFF (which contains a +127 bias).
	// Thus only the least significant 4 bits of the exponent need to be
	// stored.

	const float addValues[] = { 2.f, -2.f };
	MultiType y; y.asFloat = yValue;
	y.asFloat += addValues[ y.asInt < 0 ];

	uint16& yDataInt = *(uint16*)yData_;

	// Extract the lower 4 bits of the exponent, and the 11 most significant
	// bits of the mantissa (15 bits all up).
	yDataInt = (y.asUint >> 12) & 0x7fff;

	// Transfer the sign bit.
 	yDataInt |= ((y.asUint >> 16) & 0x8000);

}


/**
 *	This method returns the packed y value as a float.
 */
inline float PackedXYZ::y() const
{
	// Preload output value with 0(sign) 10000000(exponent) 00..(mantissa).
	MultiType y;
	y.asUint = 0x40000000;

	uint16& yDataInt = *(uint16*)yData_;

	// Copy the 4-bit lower 4 bits of the exponent and the
	// 11 most significant bits of the mantissa.
	y.asUint |= (yDataInt & 0x7fff) << 12;

	// Remove value bias.
	y.asFloat -= 2.f;

	// Copy the sign bit.
	y.asUint |= (yDataInt & 0x8000) << 16;

	return y.asFloat;
}


// Streaming operators
inline BinaryIStream& operator>>( BinaryIStream& is, PackedXYZ &xyz )
{
	MF_ASSERT( sizeof( xyz.yData_ ) == sizeof( uint16 ) );
	return is >> (PackedXZ&)xyz >> *(uint16*)xyz.yData_;
}


inline BinaryOStream& operator<<( BinaryOStream& os, const PackedXYZ &xyz )
{
	MF_ASSERT( sizeof( xyz.yData_ ) == sizeof( uint16 ) );
	return os << (PackedXZ&)xyz << *(uint16*)xyz.yData_;
}


typedef uint8 IDAlias;

typedef uint8 SpaceViewportID;
const SpaceViewportID NO_SPACE_VIEWPORT_ID = SpaceViewportID( -1 );


// -----------------------------------------------------------------------------
// Section: class RelPosRef
// -----------------------------------------------------------------------------

/**
 *	This class defines the x, y and z position in metres that coordinates sent
 *	down to clients are relative to. The x and z values in this structure are
 *	themselves relative to the global position of the client's entity.
 */
class RelPosRef
{
public:
	uint8	x_;
	uint8	y_;
	uint8	z_;
	uint8	seqNum_;
};

inline BinaryIStream& operator>>( BinaryIStream &is, RelPosRef &r )
{
	return is >> r.x_ >> r.y_ >> r.z_ >> r.seqNum_;
}

inline BinaryOStream& operator<<( BinaryOStream &os, const RelPosRef &r )
{
	return os << r.x_ << r.y_ << r.z_ << r.seqNum_;
}

#endif // MSGTYPES_HPP
