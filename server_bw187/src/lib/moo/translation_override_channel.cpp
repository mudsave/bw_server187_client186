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
#include "translation_override_channel.hpp"
#include "animation_manager.hpp"
#include "cstdmf/binaryfile.hpp"


DECLARE_DEBUG_COMPONENT2( "Moo", 0 );


namespace Moo
{

// -----------------------------------------------------------------------------
// Section: TranslationOverrideChannel
// -----------------------------------------------------------------------------


TranslationOverrideChannel::TranslationOverrideChannel( const Vector3& translation, bool override, AnimationChannelPtr pBaseChannel, AnimationPtr pBaseAnim )
: translation_( translation ),
  override_( override ),
  pBaseChannel_( pBaseChannel ),
  pBaseAnim_( pBaseAnim )
{
	MF_ASSERT( pBaseChannel_.hasObject() );
    this->identifier( pBaseChannel_->identifier() );
}

/**
 *	Constructor.
 */
TranslationOverrideChannel::TranslationOverrideChannel()
:	translation_( 0, 0, 0 ),
	override_( false )
{
}


/**
 *	Destructor.
 */
TranslationOverrideChannel::~TranslationOverrideChannel()
{

}

Matrix TranslationOverrideChannel::result( float time ) const
{
	MF_ASSERT( pBaseChannel_.hasObject() );

	Matrix m;
	pBaseChannel_->result( time, m );
	if (override_)
		m.translation( translation_ );
	return m;
}

void TranslationOverrideChannel::result( float time, Matrix& out ) const
{
	MF_ASSERT( pBaseChannel_.hasObject() );

	pBaseChannel_->result( time, out );
	if (override_)
		out.translation( translation_ );
}

void TranslationOverrideChannel::result( float time, BlendTransform& out ) const
{
	MF_ASSERT( pBaseChannel_.hasObject() );

	pBaseChannel_->result( time, out );
	if (override_)
		out.translate = translation_;
}

bool TranslationOverrideChannel::load( const BinaryFile & bf )
{
	bool ret = false;
	this->AnimationChannel::load( bf );
	std::string animResource;
	bf >> animResource;

	this->pBaseAnim_ = AnimationManager::instance().find( animResource );
	if (this->pBaseAnim_.hasObject())
	{
		this->pBaseChannel_ = NULL;
		for (uint32 i = 0; i < this->pBaseAnim_->nChannelBinders(); i++)
		{
			const ChannelBinder& cb = pBaseAnim_->channelBinder( i );
			if (cb.channel()->identifier() == this->identifier())
			{
				this->pBaseChannel_ = cb.channel();
				i = this->pBaseAnim_->nChannelBinders();
				ret = true;
			}
		}
	}

	bf >> this->translation_;
	bf >> this->override_;

	return ret;
}

bool TranslationOverrideChannel::save( BinaryFile & bf ) const
{
	this->AnimationChannel::save( bf );
	std::string animResource = AnimationManager::instance().resourceID( this->pBaseAnim_.getObject() );
	bf << animResource;
	bf << this->translation_;
	bf << this->override_;

	return true;
}


TranslationOverrideChannel::TypeRegisterer
	TranslationOverrideChannel::s_rego_( 7, New );

};

// translation_override_channel.cpp
