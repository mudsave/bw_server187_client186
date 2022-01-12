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

#include "discrete_animation_channel.hpp"

#ifndef CODE_INLINE
#include "discrete_animation_channel.ipp"
#endif

#include "cstdmf/binaryfile.hpp"
#include "cstdmf/memory_counter.hpp"

memoryCounterDeclare( animation );

namespace Moo
{

DiscreteAnimationChannel::DiscreteAnimationChannel()
{
}

DiscreteAnimationChannel::~DiscreteAnimationChannel()
{
	memoryCounterSub( animation );
	memoryClaim( keyframes_ );
	for (DiscreteKeyframes::iterator it = keyframes_.begin();
		it != keyframes_.end();
		it++)
	{
		memoryClaim( keyframes_, it );
	}
}

Matrix DiscreteAnimationChannel::result( float time ) const
{
	Matrix m;
	DiscreteKeyframes::const_iterator it = keyframes_.upper_bound( time );
	if (it != keyframes_.begin())
		--it;
	if (it != keyframes_.end())
		m = (const Matrix&)it->second;
	else
		XPMatrixIdentity( &m );

	return m;
}

void DiscreteAnimationChannel::addKey( float time, const Matrix& key )
{
	keyframes_[ time ] = key;
}

void DiscreteAnimationChannel::result( float time, Matrix& out ) const
{
	DiscreteKeyframes::const_iterator it = keyframes_.upper_bound( time );
	if (it != keyframes_.begin())
		--it;
	if (it != keyframes_.end())
		out = (const Matrix&)it->second;
}

/**
 *	Call this method when you have finished adding keys.
 *	Currently it is only necessary for memory accounting balancing.
 */
void DiscreteAnimationChannel::finalise()
{
	memoryCounterAdd( animation );
	memoryClaim( keyframes_ );
	for (DiscreteKeyframes::iterator it = keyframes_.begin();
		it != keyframes_.end();
		it++)
	{
		memoryClaim( keyframes_, it );
	}
}

void DiscreteAnimationChannel::preCombine( const AnimationChannel & rOther )
{
	Matrix m;

	for (DiscreteKeyframes::iterator it = keyframes_.begin();
		it != keyframes_.end();
		it++)
	{
		rOther.result( it->first, m );
		//it->second.preMultiply( m );
		Matrix our( (Matrix&)it->second );	// copy since it->second unaligned
		our.preMultiply( m );
		it->second = our;
	}
}

void DiscreteAnimationChannel::postCombine( const AnimationChannel & rOther )
{
	Matrix m;

	for (DiscreteKeyframes::iterator it = keyframes_.begin();
		it != keyframes_.end();
		it++)
	{
		rOther.result( it->first, m );
		//it->second.postMultiply( m );
		//it->second.preMultiply( m );
		Matrix our( (Matrix&)it->second );	// copy since it->second unaligned
		our.postMultiply( m );
		it->second = our;
	}
}



bool DiscreteAnimationChannel::load( const BinaryFile & bf )
{
	this->AnimationChannel::load( bf );

	bf.readMap( keyframes_ );

	this->finalise();

	return true;
}


bool DiscreteAnimationChannel::save( BinaryFile & bf ) const
{
	this->AnimationChannel::save( bf );

	bf.writeMap( keyframes_ );

	return true;
}

DiscreteAnimationChannel::TypeRegisterer
	DiscreteAnimationChannel::s_rego_( 0, New );


std::ostream& operator<<(std::ostream& o, const DiscreteAnimationChannel& t)
{
	o << "DiscreteAnimationChannel\n";
	return o;
}

}

// discrete_animation_channel.cpp
