/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "anonymous_channel_client.hpp"

// -----------------------------------------------------------------------------
// Section: AnonymousChannelClient
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
AnonymousChannelClient::AnonymousChannelClient() :
	pChannelOwner_( NULL )
{
}


/**
 *	Destructor.
 */
AnonymousChannelClient::~AnonymousChannelClient()
{
	delete pChannelOwner_;
}


/**
 *	This method initialises this object.
 *
 *	@return true on success, otherwise false.
 */
bool AnonymousChannelClient::init( Mercury::Nub & nub,
		Mercury::InterfaceMinder & interfaceMinder,
		const Mercury::InterfaceElement & birthMessage,
		const char * componentName,
		int numRetries )
{
	interfaceName_ = componentName;

	bool result = true;

	interfaceMinder.handler( birthMessage.id(), this );

	if (nub.registerBirthListener( birthMessage, componentName ) !=
		Mercury::REASON_SUCCESS)
	{
		ERROR_MSG( "AnonymousChannelClient::init: "
			"Failed to register birth listener for %s\n",
			componentName );

		result = false;
	}

	Mercury::Address serverAddr( Mercury::Address::NONE );

	if (nub.findInterface( componentName, 0, serverAddr, numRetries ) !=
		Mercury::REASON_SUCCESS)
	{
		result = false;
	}

	// Everyone talking to another process via this mechanism is doing it
	// irregularly at the moment.  Could make this optional.
	pChannelOwner_ = new Mercury::ChannelOwner( nub );
	pChannelOwner_->channel().isIrregular( true );

	if (result)
	{
		this->addr( serverAddr );
	}

	return result;
}


void AnonymousChannelClient::addr( const Mercury::Address & addr )
{
	pChannelOwner_->channel().reset( addr );
}


/**
 *	This method handles a birth message telling us that a new server has
 *	started.
 */
void AnonymousChannelClient::handleMessage(
		const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data )
{
	Mercury::Address serverAddr;
	data >> serverAddr;

	MF_ASSERT( data.remainingLength() == 0 && !data.error() );

	this->addr( serverAddr );

	INFO_MSG( "AnonymousChannelClient::handleMessage: "
		"Got new %s at %s\n",
		interfaceName_.c_str(), pChannelOwner_->channel().c_str() );
}

// anonymous_channel_client.cpp
