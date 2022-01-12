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

#include "animation_channel.hpp"

#ifndef CODE_INLINE
#include "animation_channel.ipp"
#endif

#include "cstdmf/binaryfile.hpp"
#include "cstdmf/memory_counter.hpp"

DECLARE_DEBUG_COMPONENT2( "Moo", 0 )

memoryCounterDeclare( animation );

namespace Moo
{

/**
 *	Constructor
 */
AnimationChannel::AnimationChannel()
{
	memoryCounterAdd( animation );
	memoryClaim( this );
}

/**
 *	Copy constructor
 */
AnimationChannel::AnimationChannel( const AnimationChannel & other )
{
	memoryCounterAdd( animation );
	memoryClaim( this );

	identifier_ = other.identifier_;
	memoryClaim( identifier_ );
}

/**
 *	Destructor
 */
AnimationChannel::~AnimationChannel()
{
	memoryCounterSub( animation );
	memoryClaim( identifier_ );
	memoryClaim( this );
}


bool AnimationChannel::load( const BinaryFile & bf )
{
	bf >> identifier_;
	memoryCounterAdd( animation );
	memoryClaim( identifier_ );
	return true;
}

bool AnimationChannel::save( BinaryFile & bf ) const
{
	bf << identifier_;
	return true;
}


/**
 *	This static method constructs a new channel of the given type.
 */
AnimationChannel * AnimationChannel::construct( int type )
{
	ChannelTypes::iterator found = channelTypes_->find( type );
	if (found != channelTypes_->end())
		return (*found->second)();

	ERROR_MSG( "AnimationChannel::construct: "
		"Channel type %d is unknown.\n", type );
	return NULL;
	/*
	switch (type )
	{
	case 0:
		return new DiscreteAnimationChannel();
	case 1:
		return new InterpolatedAnimationChannel();
	case 2:
		return new MorphAnimationChannel();
	case 3:
		return new InterpolatedAnimationChannel( false );
	case 4:
		return new InterpolatedAnimationChannel( true, true );
	case 5:
		return new StreamedAnimationChannel();
	}

	return NULL;
	*/
}

/**
 *	Static method to register a channel type
 */
void AnimationChannel::registerChannelType( int type, Constructor cons )
{
	if (channelTypes_ == NULL) channelTypes_ = new ChannelTypes();
	(*channelTypes_)[ type ] = cons;
	INFO_MSG( "AnimationChannel: registered type %d\n", type );
}


/// static initialiser
AnimationChannel::ChannelTypes * AnimationChannel::channelTypes_ = NULL;

extern int MorphAnimationChannel_token;
extern int CueChannel_token;
static int s_channelTokens = MorphAnimationChannel_token | CueChannel_token;

} // namespace Moo

// animation_channel.cpp
