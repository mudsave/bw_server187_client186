/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#if defined( DEFINE_INTERFACE_HERE ) || defined( DEFINE_SERVER_HERE )
#undef BASEAPP_EXT_INTERFACE_HPP
#endif

#ifndef BASEAPP_EXT_INTERFACE_HPP
#define BASEAPP_EXT_INTERFACE_HPP

// -----------------------------------------------------------------------------
// Section: Helper macros
// -----------------------------------------------------------------------------

#undef MF_BEGIN_BASE_APP_MSG
#define MF_BEGIN_BASE_APP_MSG( NAME )										\
	BEGIN_HANDLED_STRUCT_MESSAGE( NAME,										\
			BaseAppMessageHandler< BaseAppExtInterface::NAME##Args >,		\
			&BaseApp::NAME )												\

#undef MF_BEGIN_BASE_APP_MSG_WITH_HEADER
#define MF_BEGIN_BASE_APP_MSG_WITH_HEADER( NAME )							\
	BEGIN_HANDLED_STRUCT_MESSAGE( NAME,										\
		BaseAppMessageWithAddrAndHeaderHandler<BaseAppExtInterface::NAME##Args>,		\
		&BaseApp::NAME )													\

#undef MF_RAW_BASE_APP_MSG
#define MF_RAW_BASE_APP_MSG( NAME )											\
	MERCURY_HANDLED_VARIABLE_MESSAGE( NAME, 2, 								\
			BaseAppRawMessageHandler, &BaseApp::NAME )

#undef MF_BEGIN_BLOCKABLE_PROXY_MSG
#define MF_BEGIN_BLOCKABLE_PROXY_MSG( NAME )								\
	BEGIN_HANDLED_STRUCT_MESSAGE( NAME,										\
			ProxyMessageHandler< BaseAppExtInterface::NAME##Args >,			\
			&Proxy::NAME )													\

#undef MF_BEGIN_UNBLOCKABLE_PROXY_MSG
#define MF_BEGIN_UNBLOCKABLE_PROXY_MSG( NAME )								\
	BEGIN_HANDLED_STRUCT_MESSAGE( NAME,										\
			NoBlockProxyMessageHandler< BaseAppExtInterface::NAME##Args >,	\
			&Proxy::NAME )													\

#undef MF_VARLEN_BLOCKABLE_PROXY_MSG
#define MF_VARLEN_BLOCKABLE_PROXY_MSG( NAME )								\
	MERCURY_HANDLED_VARIABLE_MESSAGE( NAME, 2,								\
			ProxyVarLenMessageHandler< true >,								\
			&Proxy::NAME )

#undef MF_VARLEN_UNBLOCKABLE_PROXY_MSG
#define MF_VARLEN_UNBLOCKABLE_PROXY_MSG( NAME )								\
	MERCURY_HANDLED_VARIABLE_MESSAGE( NAME, 2,								\
			ProxyVarLenMessageHandler< false >,								\
			&Proxy::NAME )

// -----------------------------------------------------------------------------
// Section: Includes
// -----------------------------------------------------------------------------

#include "network/interface_minder.hpp"
#include "network/msgtypes.hpp"

// -----------------------------------------------------------------------------
// Section: BaseApp External Interface
// -----------------------------------------------------------------------------

BEGIN_MERCURY_INTERFACE( BaseAppExtInterface )

	// let the proxy know who we really are
	MF_RAW_BASE_APP_MSG( baseAppLogin )

	// let the proxy know who we really are
	MF_BEGIN_BASE_APP_MSG_WITH_HEADER( authenticate )
		SessionKey		key;
	END_STRUCT_MESSAGE()

	// send an update for our position. seqNum is used to refer to this position
	// later as the base for relative positions.
	MF_BEGIN_BLOCKABLE_PROXY_MSG( avatarUpdateImplicit )
		Coord			pos;
		YawPitchRoll	dir;
		RelPosRef		posRef;
	END_STRUCT_MESSAGE();

	MF_BEGIN_BLOCKABLE_PROXY_MSG( avatarUpdateExplicit )
		SpaceID			spaceID;
		ObjectID		vehicleID;
		Coord			pos;
		YawPitchRoll	dir;
		bool			onGround;
		RelPosRef		posRef;
	END_STRUCT_MESSAGE();

	MF_BEGIN_BLOCKABLE_PROXY_MSG( avatarUpdateWardImplicit )
		ObjectID		ward;
		Coord			pos;
		YawPitchRoll	dir;
	END_STRUCT_MESSAGE();

	MF_BEGIN_BLOCKABLE_PROXY_MSG( avatarUpdateWardExplicit )
		ObjectID		ward;
		SpaceID			spaceID;
		ObjectID		vehicleID;
		Coord			pos;
		YawPitchRoll	dir;
		bool			onGround;
	END_STRUCT_MESSAGE();

	// TODO: I'm pretty sure this is not used. We should remove it.
	// forward all further messages (over 128) in this bundle to the cell
	MERCURY_FIXED_MESSAGE( switchInterface, 0, NULL )

	// requestEntityUpdate:
	MF_VARLEN_BLOCKABLE_PROXY_MSG( requestEntityUpdate )
	//	ObjectID		id;
	//	EventNumber[]	lastEventNumbers;

	MF_BEGIN_BLOCKABLE_PROXY_MSG( enableEntities )
		uint32				presentVersion;
		uint32				futureVersion;
	END_STRUCT_MESSAGE();

	MF_BEGIN_BLOCKABLE_PROXY_MSG( setSpaceViewportAck )
		ObjectID			id;
		SpaceViewportID		spaceViewportID;
	END_STRUCT_MESSAGE();

	MF_BEGIN_BLOCKABLE_PROXY_MSG( setVehicleAck )
		ObjectID			id;
		ObjectID			vehicleID;
	END_STRUCT_MESSAGE();

	MF_BEGIN_UNBLOCKABLE_PROXY_MSG( restoreClientAck )
		int					id;
	END_STRUCT_MESSAGE();

	MF_VARLEN_UNBLOCKABLE_PROXY_MSG( identifyVersionPoint )
	//	uint16		rid;
	//	std::string	point;
	//	MD5::Digest	digest0;
	//	MD5::Digest	digest1;
	//	...

	MF_VARLEN_UNBLOCKABLE_PROXY_MSG( summariseVersionPoint )
	//	uint16		rid;
	//	uint32		version;
	//	std::string	point;

	MF_VARLEN_UNBLOCKABLE_PROXY_MSG( commenceResourceDownload )
	//	uint16		rid;
	//	uint32		version;
	//	std::string	point;
	//	std::string	resource;
	//	[uint32		offset];

	MF_BEGIN_UNBLOCKABLE_PROXY_MSG( disconnectClient )
		uint8 reason;
	END_STRUCT_MESSAGE()

	//MF_STRUCT_PROXY_MSG( continueResourceDownload )

	MF_BEGIN_BASE_APP_MSG( resourceVersionTag )
		uint8				tag;
	END_STRUCT_MESSAGE()

	// 128 to 254 are messages destined either for our entities
	// or for the cell's. They all look like this:
	MERCURY_VARIABLE_MESSAGE( entityMessage, 2, NULL )

END_MERCURY_INTERFACE()




#endif // BASEAPP_EXT_INTERFACE_HPP
