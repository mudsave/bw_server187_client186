/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#if defined(DEFINE_INTERFACE_HERE) || defined(DEFINE_SERVER_HERE)
#undef CLIENT_INTERFACE_HPP
#endif


#ifndef CLIENT_INTERFACE_HPP
#define CLIENT_INTERFACE_HPP

#include "network/interface_minder.hpp"
#include "network/msgtypes.hpp"
#include "cstdmf/md5.hpp"

// Temporary defines
#define MF_BEGIN_INTERFACE					BEGIN_MERCURY_INTERFACE
#define MF_END_INTERFACE					END_MERCURY_INTERFACE
#define MF_END_MSG							END_STRUCT_MESSAGE

#define MF_BEGIN_MSG						BEGIN_STRUCT_MESSAGE

#define MF_BEGIN_HANDLED_PREFIXED_MSG		BEGIN_HANDLED_PREFIXED_MESSAGE
#define MF_BEGIN_HANDLED_MSG				BEGIN_HANDLED_STRUCT_MESSAGE


// -----------------------------------------------------------------------------
// Section: Helper macros
// -----------------------------------------------------------------------------


#define MF_BEGIN_CLIENT_MSG( NAME )											\
	MF_BEGIN_HANDLED_MSG( NAME,												\
			ClientMessageHandler< ClientInterface::NAME##Args >,			\
			&ServerConnection::NAME )										\

#define MF_VARLEN_CLIENT_MSG( NAME )										\
	MERCURY_HANDLED_VARIABLE_MESSAGE( NAME, 2,								\
			ClientVarLenMessageHandler,	&ServerConnection::NAME )

#define MF_VARLEN_WITH_ADDR_CLIENT_MSG( NAME )								\
	MERCURY_HANDLED_VARIABLE_MESSAGE( NAME, 2,								\
			ClientVarLenWithAddrMessageHandler,	&ServerConnection::NAME )



// -----------------------------------------------------------------------------
// Section: Client interface
// -----------------------------------------------------------------------------

#pragma pack(push, 1)
MF_BEGIN_INTERFACE( ClientInterface )

	MF_BEGIN_CLIENT_MSG( authenticate )
		uint32				key;
	END_STRUCT_MESSAGE()

	MF_BEGIN_CLIENT_MSG( bandwidthNotification )
		uint				bps;
	END_STRUCT_MESSAGE()

	MF_BEGIN_CLIENT_MSG( updateFrequencyNotification )
		uint8				hertz;
	END_STRUCT_MESSAGE()

	MF_BEGIN_CLIENT_MSG( setGameTime )
		TimeStamp			gameTime;
	END_STRUCT_MESSAGE()

	MF_BEGIN_CLIENT_MSG( resetEntities )
		bool			keepPlayerOnBase;
	END_STRUCT_MESSAGE()

	MF_VARLEN_CLIENT_MSG( createBasePlayer )
	MF_VARLEN_CLIENT_MSG( createCellPlayer )

	MF_VARLEN_CLIENT_MSG( spaceData )
	//	ObjectID		spaceID
	//	SpaceEntryID	entryID
	//	uint16			key;
	//	char[]			value;		// rest of message

	MF_BEGIN_CLIENT_MSG( spaceViewportInfo )
		ObjectID			gatewaySrcID;
		ObjectID			gatewayDstID;
		SpaceID				spaceID;
		SpaceViewportID		spaceViewportID;
	END_STRUCT_MESSAGE()

	MF_VARLEN_CLIENT_MSG( createEntity )
	MF_VARLEN_CLIENT_MSG( updateEntity )

	MF_BEGIN_CLIENT_MSG( enterAoI )
		ObjectID			id;
		IDAlias				idAlias;
	END_STRUCT_MESSAGE()

	MF_BEGIN_CLIENT_MSG( enterAoIThruViewport )
		ObjectID			id;
		IDAlias				idAlias;
		SpaceViewportID		spaceViewportID;
	END_STRUCT_MESSAGE()

	MF_BEGIN_CLIENT_MSG( enterAoIOnVehicle )
		ObjectID			id;
		ObjectID			vehicleID;
		IDAlias				idAlias;
	END_STRUCT_MESSAGE()

	MF_VARLEN_CLIENT_MSG( leaveAoI )
	//	ObjectID		id;
	//	EventNumber[]	lastEventNumbers;	// rest


	// The interface that is shared with the base app.
#define MF_BEGIN_COMMON_UNRELIABLE_MSG MF_BEGIN_CLIENT_MSG
#define MF_BEGIN_COMMON_PASSENGER_MSG MF_BEGIN_CLIENT_MSG
#define MF_BEGIN_COMMON_RELIABLE_MSG MF_BEGIN_CLIENT_MSG
#include "common_client_interface.hpp"

	// This message is used to send an accurate position of an entity down to
	// the client. It is usually sent when the volatile information of an entity
	// becomes less volatile.
	MF_BEGIN_CLIENT_MSG( detailedPosition )
		ObjectID		id;
		Vector3			position;
		Direction3D		direction;
	END_STRUCT_MESSAGE()

	// This is used to send a physics correction or other server-set position
	// to the client for an entity that it is controlling
	MF_BEGIN_CLIENT_MSG( forcedPosition )
		ObjectID		id;
		SpaceID			spaceID;
		ObjectID		vehicleID;
		Position3D		position;
		Direction3D		direction;
	END_STRUCT_MESSAGE()

	MF_BEGIN_CLIENT_MSG( controlEntity )
		ObjectID		id;
		bool			on;
	END_STRUCT_MESSAGE()


	MF_VARLEN_WITH_ADDR_CLIENT_MSG( voiceData )

	MF_VARLEN_CLIENT_MSG( restoreClient )
	MF_VARLEN_CLIENT_MSG( restoreBaseApp )


	MF_BEGIN_CLIENT_MSG( versionPointIdentity )
		uint16			rid;
		uint8			index;
		uint8			reserved;
		uint32			version;
	END_STRUCT_MESSAGE()

	MF_BEGIN_CLIENT_MSG( versionPointSummary )
		uint16			rid;
		MD5::Digest		change;
		MD5::Digest		census;
	END_STRUCT_MESSAGE()

	MF_VARLEN_CLIENT_MSG( resourceFragment )
	// ResourceFragmentArgs
#ifndef CLIENT_INTERFACE_HPP_ONCE
	struct ResourceFragmentArgs {
		uint16			rid;
		uint8			seq;
		uint8			flags; };	// 1 = first (method in seq), 2 = final
	//	uint8			data[];		// rest
#endif

	MF_BEGIN_CLIENT_MSG( resourceVersionStatus )	// (reply to enableEntities)
		uint32			latestVersion;		// -1 => imminentVersion impending,
		uint32			imminentVersion;	// but not required to be loaded
	END_STRUCT_MESSAGE()

	MF_BEGIN_CLIENT_MSG( resourceVersionTag )
		uint8				tag;
	END_STRUCT_MESSAGE()

	MF_BEGIN_CLIENT_MSG( loggedOff )
		uint8	reason;
	END_STRUCT_MESSAGE()

	// 128 to 254 are messages destined for our entities.
	// They all look like this:
	MERCURY_VARIABLE_MESSAGE( entityMessage, 2, NULL )

MF_END_INTERFACE()
#pragma pack( pop )

#define CLIENT_INTERFACE_HPP_ONCE

#endif // CLIENT_INTERFACE_HPP
