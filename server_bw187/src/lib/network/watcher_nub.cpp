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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "network/endpoint.hpp"

#include "network/portmap.hpp"
#include "network/machine_guard.hpp"
#include "network/watcher_nub.hpp"
#include "network/mercury.hpp"
#include "network/misc.hpp"

#include "cstdmf/config.hpp"
#include "cstdmf/memory_counter.hpp"

DECLARE_DEBUG_COMPONENT2( "Network", 0 )
static const int WN_PACKET_SIZE = 0x10000;
static const char * WN_ERROR_IDENTIFIER_STR = "<Err>";
static const unsigned int WN_ERROR_IDENTIFIER_STRLEN =
		strlen( WN_ERROR_IDENTIFIER_STR ) + 1;	// includes null terminator
static const char * WN_ERROR_PACKETLIMIT_STR = "Exceeded maximum packet size";
static const unsigned int WN_ERROR_PACKETLIMIT_STRLEN =
		strlen( WN_ERROR_PACKETLIMIT_STR ) + 1;	// includes null terminator
// Always leave room at the end of the packet for error message... just in case
static const int WN_MAX_REPLY_SIZE = WN_PACKET_SIZE -
		WN_ERROR_IDENTIFIER_STRLEN - WN_ERROR_PACKETLIMIT_STRLEN;

memoryCounterDefine( watcher, Base );


/**
 * 	This is the constructor.
 */
WatcherNub::WatcherNub() :
	id_(-1),
	registered_(false),
	wrh_(NULL),
	insideReceiveRequest_(false),
	replyPacket_(new char[WN_PACKET_SIZE]),
	replyPointer_(NULL),
	reachedPacketLimit_( false ),
	requestPacket_(new char[WN_PACKET_SIZE]),
	isInitialised_( false ),
	socket_( /* useSyncHijack */ false )
{
	memoryCounterAdd( watcher );
	memoryClaim( replyPacket_ );
	memoryClaim( requestPacket_ );
}


/**
 *	This method initialises the watcher nub.
 */
bool WatcherNub::init( const char * listeningInterface, uint16 listeningPort )
{
	INFO_MSG( "WatcherNub::init: listeningInterface = '%s'\n",
			listeningInterface ? listeningInterface : "" );
	if (isInitialised_)
	{
		// WARNING_MSG( "WatcherNub::init: Already initialised.\n" );
		return true;
	}

	isInitialised_ = true;

	// open the socket
	socket_.socket(SOCK_DGRAM);
	if(!socket_.good())
	{
		ERROR_MSG( "WatcherNub::init: socket() failed\n" );
		return false;
	}

	if (socket_.setnonblocking(true))	// make it nonblocking
	{
		ERROR_MSG( "WatcherNub::init: fcntl(O_NONBLOCK) failed\n" );
		return false;
	}

	u_int32_t ifaddr = INADDR_ANY;
#ifndef _WIN32
	// If the interface resolves to an address, use that instead of
	// searching for a matching interface.
	if (inet_aton( listeningInterface, (struct in_addr *)&ifaddr ) == 0)
#endif
	{
		// ask endpoint to parse the interface specification into a name
		char ifname[IFNAMSIZ];
		bool listeningInterfaceEmpty =
			(listeningInterface == NULL || listeningInterface[0] == 0);
		if (socket_.findIndicatedInterface( listeningInterface, ifname ) == 0)
		{
			INFO_MSG( "WatcherNub::init: creating on interface '%s' (= %s)\n",
				listeningInterface, ifname );
			if (socket_.getInterfaceAddress( ifname, ifaddr ) != 0)
			{
				WARNING_MSG( "WatcherNub::init: couldn't get addr of interface %s "
					"so using all interfaces\n", ifname );
			}
		}
		else if (!listeningInterfaceEmpty)
		{
			WARNING_MSG( "WatcherNub::init: couldn't parse interface spec %s "
				"so using all interfaces\n", listeningInterface );
		}
	}

	if (socket_.bind( listeningPort, ifaddr ))
	{
		ERROR_MSG( "WatcherNub::init: bind() failed\n" );
		socket_.close();
		return false;
	}

	return true;
}


/**
 * 	This is the destructor.
 */
WatcherNub::~WatcherNub()
{
	memoryCounterSub( watcher );

	if (registered_)
	{
		this->deregisterWatcher();
	}

	if (socket_.good())
	{
		socket_.close();
	}

	if (requestPacket_ != NULL)
	{
		memoryClaim( requestPacket_ );
		delete [] requestPacket_;
		requestPacket_ = NULL;
	}

	if (replyPacket_ != NULL)
	{
		memoryClaim( replyPacket_ );
		delete [] replyPacket_;
		replyPacket_ = NULL;
	}
}


/**
 * 	This method broadcasts a watcher register message for this watcher.
 *
 * 	@param id			Numeric id for this watcher.
 * 	@param abrv			Short name for this watcher.
 * 	@param longName		Long name for this watcher.
 * 	@param listeningInterface	The name of the network interface to listen on.
 * 	@param listeningPort		The port to listen on.
 */
int WatcherNub::registerWatcher( int id, char *abrv, char *longName,
	   const char * listeningInterface, uint16 listeningPort )
{
	if (!this->init( listeningInterface, listeningPort ))
	{
		ERROR_MSG( "WatcherNub::registerWatcher: init failed.\n" );
		return -1;
	}

	// make sure we're not already registered...
	if (registered_)
		this->deregisterWatcher();

	// set up a few things
	id_ = id;

	strncpy( abrv_, abrv, sizeof(abrv_) );
	abrv_[sizeof(abrv_)-1]=0;

	strncpy( name_, longName, sizeof(name_) );
	name_[sizeof(name_)-1]=0;

	// and go for it
	int ret = this->watcherControlMessage(WATCHER_MSG_REGISTER,true);

	if (ret == 0)
	{
		registered_ = true;
		this->notifyMachineGuard();
	}
	return ret;
}


/**
 * 	This method broadcasts a watcher deregister message for this watcher.
 */
int WatcherNub::deregisterWatcher()
{
	if (!registered_)
		return 0;
	else
	{
		int ret = this->watcherControlMessage( WATCHER_MSG_DEREGISTER, true );
		if (ret == 0)
		{
			registered_ = false;
			this->notifyMachineGuard();
		}

		return ret;
	}
}


/**
 * 	This method sends a message to the machined process.
 */
void WatcherNub::notifyMachineGuard()
{
	u_int16_t port = 0;
	socket_.getlocaladdress( &port, NULL );

	ProcessMessage pm;
	pm.param_ = pm.PARAM_IS_MSGTYPE |
		(registered_ ? pm.REGISTER : pm.DEREGISTER);
	pm.category_ = pm.WATCHER_NUB;
	pm.uid_ = getUserId();
	pm.pid_ = mf_getpid();
	pm.port_ = port;
	pm.id_ = id_;
	pm.name_ = abrv_;

	uint32 destip = htonl( 0x7F000001U );
	int reason;
	if ((reason = pm.sendAndRecv( 0, destip )) != Mercury::REASON_SUCCESS)
	{
		ERROR_MSG( "Couldn't register watcher nub with machined: %s\n",
			Mercury::reasonToString( (Mercury::Reason)reason ) );
	}
}


/**
 * 	This method broadcasts a watcher flush components message for
 * 	this watcher.
 */
int WatcherNub::resetServer()
{
	if(registered_)
		return -1;
	else
		return this->watcherControlMessage( WATCHER_MSG_FLUSHCOMPONENTS, true );
}


/**
 * 	This method broadcasts a message for this watcher.
 *
 * 	@param message		The message to broadcast.
 * 	@param withid		If true, the id and names are also sent.
 */
int WatcherNub::watcherControlMessage(int message, bool withid)
{
    int ret = 0;

	// turn on the broadcast flag
	if (socket_.setbroadcast(true))
	{
		perror( "WatcherNub::watcherControlMessage: "
                "setsockopt(SO_BROADCAST) failed\n" );
        ret = -1;
    }
    else
    {
        // build the packet
        WatcherRegistrationMsg  wrm;
        wrm.version = 0;
        wrm.uid = getUserId();
        wrm.message = message;

        if(withid)
        {
            wrm.id = id_;
            strcpy(wrm.abrv,abrv_);
            strcpy(wrm.name,name_);
        }
        else
        {
            wrm.id = -1;
            wrm.abrv[0] = 0;
            wrm.name[0] = 0;
        }

        // send the message
        if (socket_.sendto( &wrm,sizeof(wrm),
					htons( PORT_WATCHER ) ) != sizeof(wrm))
        {
            perror( "WatcherNub::watcherControlMessage: sendto failed\n" );
            ret = -1;
        }

        // turn off the broadcast flag
        if (socket_.setbroadcast( false ))
        {
            perror( "WatcherNub::watcherControlMessage: "
                    "setsockopt(-SO_BROADCAST) failed\n" );
            ret = -1;
        }

		/*
        if (ret == 0)
        {
            // HACK: Also send it to 202.44.187.29 (syd.microforte.com.au)
            socket_.sendto( &wrm, sizeof(wrm), htons( PORT_WATCHER ),
					htonl( 0xCA2CBB1D ) );
            // (old ISDN ip: 203.42.41.254 = 0xCB2A29FE)
        }
		*/
    }

    return ret;
}


/**
 * 	This method sents the handler to receive events for this watcher.
 *
 * 	@param wrh	The WatcherRequestHandler object to receive events.
 */
void WatcherNub::setRequestHandler( WatcherRequestHandler *wrh )
{
	if (insideReceiveRequest_)
	{
		ERROR_MSG( "WatcherNub::setRequestHandler: "
			"Can't call me while inside receiveRequest!\n" );
		return;
	}

	wrh_ = wrh;
}


/**
 * 	This method returns the UDP socket used by this watcher.
 */
int WatcherNub::getSocketDescriptor()
{
	return socket_;
}


/**
 * 	This method should be called to handle requests on the socket.
 */
bool WatcherNub::receiveRequest()
{
	if (!isInitialised_)
	{
		// TODO: Allow calls to this when not initialised before the client
		// currently does this. Should really fix the client so that this is
		// only called once initialised.
		return false;
	}

	sockaddr_in		senderAddr;
	int				len;

	if (wrh_ == NULL)
	{
		ERROR_MSG( "WatcherNub::receiveRequest: Can't call me before\n"
			"\tcalling setRequestHandler(WatcherRequestHandler*)\n" );
		return false;
	}

	if (insideReceiveRequest_)
	{
		ERROR_MSG( "WatcherNub::receiveRequest: BAD THING NOTICED:\n"
			"\tAttempted re-entrant call to receiveRequest\n" );
		return false;
	}

	insideReceiveRequest_ = true;

	// try to recv
	len = socket_.recvfrom( requestPacket_, WN_PACKET_SIZE, senderAddr );

	if (len == -1)
	{
		// EAGAIN = no packets waiting, ECONNREFUSED = rejected outgoing packet

#ifdef _WIN32
		int err = WSAGetLastError();
		if (err != WSAEWOULDBLOCK && err != WSAECONNREFUSED && err != WSAECONNRESET)
#else
		int err = errno;
		if (err != EAGAIN && err != ECONNREFUSED)
#endif
		{
			ERROR_MSG( "WatcherNub::receiveRequest: recvfrom failed\n" );
		}

		insideReceiveRequest_ = false;
		return false;
	}

	// make sure we haven't picked up a weird packet (from when broadcast
	// was on, say ... hey, it could happen!)
	WatcherDataMsg * wdm = (WatcherDataMsg*)requestPacket_;

	if (len < (int)sizeof(WatcherDataMsg))
	{
		ERROR_MSG( "WatcherNub::receiveRequest: Packet is too short\n" );
		insideReceiveRequest_ = false;
		return false;
	}

	if (! (wdm->message == WATCHER_MSG_GET ||
		   wdm->message == WATCHER_MSG_GET_WITH_DESC ||
		   wdm->message == WATCHER_MSG_SET
		) )
	{
		wrh_->processExtensionMessage( wdm->message,
								requestPacket_ + sizeof( wdm->message ),
								len - sizeof( wdm->message ),
								Mercury::Address( senderAddr.sin_addr.s_addr,
									senderAddr.sin_port ) );
		insideReceiveRequest_ = false;
		return true;
	}

	// ok, it's for us then. let's set up a reply
	((WatcherDataMsg*)replyPacket_)->message = WATCHER_MSG_TELL;
	((WatcherDataMsg*)replyPacket_)->count = 0;
	replyPointer_ = ((WatcherDataMsg*)replyPacket_)->string;
	reachedPacketLimit_ = false;

	// and call back to the program
	switch (wdm->message)
	{
		case WATCHER_MSG_GET:
		case WATCHER_MSG_GET_WITH_DESC:
		{
			char	*astr = wdm->string;
			for(int i=0;i<wdm->count;i++)
			{
				wrh_->processWatcherGetRequest( astr,
					   	(wdm->message & WATCHER_DESC_MASK) > 0 );
				astr += strlen(astr)+1;
			}
		}
		break;

		case WATCHER_MSG_SET:
		{
			char	*astr = wdm->string;
			for(int i=0;i<wdm->count;i++)
			{
				char	*bstr = astr + strlen(astr)+1;
				wrh_->processWatcherSetRequest(astr,bstr);
				astr = bstr + strlen(bstr)+1;
			}
		}
		break;

		default:
			// BW1.9.0 tools generate new messages that 1.8.2 does not
			// understand
			// WARNING_MSG( "WatcherNub::receiveRequest: Unknown message %d\n",
			//		wdm->message );
		break;
	}

	// send the reply (even if it's empty)
	int replyLen = replyPointer_ - replyPacket_;
	socket_.sendto( replyPacket_, replyLen, senderAddr );

	// and we're done!
	insideReceiveRequest_ = false;

	return true;
}


/**
 * 	This method adds reply fields to the current reply message.
 */
bool WatcherNub::addReply(const char * identifier, const char * value)
{
	if(!insideReceiveRequest_)
	{
		ERROR_MSG( "WatcherNub::addReply: Can't call me from\n"
			"\toutside receiveRequest!\n" );
		return false;
	}

	if (reachedPacketLimit_)
		return false;

	size_t idLen = strlen( identifier ) + 1;	// includes null terminator
	size_t valueLen = strlen( value ) + 1;		// includes null terminator
	char* pValueStartPos = replyPointer_ + idLen;
	char* pEndPos = pValueStartPos + valueLen;

	if ( (pEndPos - replyPacket_) > WN_MAX_REPLY_SIZE )
	{
		ERROR_MSG( "WatcherNub::addReply: Can't add reply due to packet size "
					"limit: %s - %s\n", identifier, value );
		reachedPacketLimit_ = true;
		memcpy( replyPointer_, WN_ERROR_IDENTIFIER_STR,
					WN_ERROR_IDENTIFIER_STRLEN );
		replyPointer_ += WN_ERROR_IDENTIFIER_STRLEN;
		memcpy( replyPointer_, WN_ERROR_PACKETLIMIT_STR,
					WN_ERROR_PACKETLIMIT_STRLEN );
		replyPointer_ += WN_ERROR_PACKETLIMIT_STRLEN;
		return false;
	}

	memcpy( replyPointer_, identifier, idLen );
	memcpy( pValueStartPos, value, valueLen );
	replyPointer_ = pEndPos;

	((WatcherDataMsg*)replyPacket_)->count++;

	return true;
}

bool WatcherNub::addReply(const char * identifier, const char * desc,
		const char * value)
{
	if(!insideReceiveRequest_)
	{
		ERROR_MSG( "WatcherNub::addReply: Can't call me from\n"
			"\toutside receiveRequest!\n" );
		return false;
	}

	if (reachedPacketLimit_)
		return false;

	size_t idLen = strlen( identifier ) + 1;	// includes null terminator
	size_t descLen = strlen( desc ) + 1;	// includes null terminator
	size_t valueLen = strlen( value ) + 1;		// includes null terminator
	char* pValueStartPos = replyPointer_ + idLen;
	char* pDescPos = pValueStartPos + valueLen;
	char* pEndPos = pDescPos + descLen;

	if ( (pEndPos - replyPacket_) > WN_MAX_REPLY_SIZE )
	{
		ERROR_MSG( "WatcherNub::addReply: Can't add reply due to packet size "
					"limit: %s - %s\n", identifier, value );
		reachedPacketLimit_ = true;
		memcpy( replyPointer_, WN_ERROR_IDENTIFIER_STR,
					WN_ERROR_IDENTIFIER_STRLEN );
		replyPointer_ += WN_ERROR_IDENTIFIER_STRLEN;
		memcpy( replyPointer_, WN_ERROR_PACKETLIMIT_STR,
					WN_ERROR_PACKETLIMIT_STRLEN );
		replyPointer_ += WN_ERROR_PACKETLIMIT_STRLEN;
		return false;
	}

	memcpy( replyPointer_, identifier, idLen );
	memcpy( pValueStartPos, value, valueLen );
	memcpy( pDescPos, desc, descLen );
	replyPointer_ = pEndPos;

	((WatcherDataMsg*)replyPacket_)->count++;

	return true;
}


// -----------------------------------------------------------------------------
// Section: WatcherRequestHandler
// -----------------------------------------------------------------------------

/**
 *	This virtual method handles any messages that the watcher nub does not know
 *	how to handle.
 */
void WatcherRequestHandler::processExtensionMessage( int messageID,
				char * data, int dataLen, const Mercury::Address & addr )
{
	// 1.9 tools generate messages that 1.8 components don't understand.
	if (messageID < 26 || messageID > 29)
	{
		ERROR_MSG( "WatcherRequestHandler::processExtensionMessage: "
			"Unknown message %d from %s. Message len = %d\n",
			messageID, addr.c_str(), dataLen );
	}
}


// -----------------------------------------------------------------------------
// Section: StandardWatcherRequestHandler
// -----------------------------------------------------------------------------

StandardWatcherRequestHandler::StandardWatcherRequestHandler(
		WatcherNub & nub ) : nub_( nub )
{
}


/**
 * TODO: to be documented.
 */
class SWRHVisitor : public WatcherVisitor
{
public:
	SWRHVisitor( const std::string & path,
			WatcherNub & nub, bool withDesc ) :
		path_( path ),
		nub_( nub ),
		withDesc_( withDesc )
	{
		if (path_ != "" && path_[ path_.size() - 1 ] != '/')
		{
			path_ += '/';
		}
	}

protected:
	virtual bool visit( Watcher::Type /*type*/,
		const std::string & label,
		const std::string & desc,
		const std::string & valueStr )
	{
		if (withDesc_)
		{
			return nub_.addReply( ( path_ + label ).c_str(),
					desc.c_str(), valueStr.c_str() );
		}
		else
		{
			return nub_.addReply( ( path_ + label ).c_str(),
					valueStr.c_str() );
		}
	}

private:
	std::string path_;
	WatcherNub & nub_;
	bool withDesc_;
};


/**
 * 	This method handles watcher get requests.
 *
 * 	@param path		The path of the watcher request.
 *	@param withDesc	Indicates whether the description should also be returned.
 */
void StandardWatcherRequestHandler::processWatcherGetRequest(
	const char * path, bool withDesc )
{
#if ENABLE_WATCHERS
	std::string result;
	std::string desc;
	Watcher::Type type = Watcher::WT_INVALID;

	if (Watcher::rootWatcher().getAsString( NULL, path, result, desc, type ))
	{
		if (type == Watcher::WT_DIRECTORY)
		{
			SWRHVisitor visitor( path, nub_, withDesc );

			Watcher::rootWatcher().visitChildren( NULL, path, visitor );
		}
		else if (type != Watcher::WT_INVALID)
		{
			// ok, it's a single value: add away then
			if (withDesc)
				nub_.addReply( path, desc.c_str(), result.c_str() );
			else
				nub_.addReply( path, result.c_str() );
		}
		else
		{
			ERROR_MSG(
					"StandardWatcherRequestHandler::processWatcherGetRequest: "
						"%s has invalid type %d\n", path, type );
		}
	}
#endif
}


/**
 * 	This method handles watcher set requests.
 *
 * 	@param path			The path of the watcher object.
 * 	@param valueString	The value to which it should be set.
 */
void StandardWatcherRequestHandler::processWatcherSetRequest( const char * path,
	const char * valueString )
{
#if ENABLE_WATCHERS
	if (!Watcher::rootWatcher().setFromString( NULL, path, valueString ))
	{
		WARNING_MSG( "StandardWatcherRequestHandler::processWatcherSetRequest: "
				"Setting %s failed\n", path );
	}

	std::string result;
	std::string desc;
	Watcher::Type type = Watcher::WT_INVALID;

	if (Watcher::rootWatcher().getAsString( NULL, path, result, desc, type ))
	{
		// and reply with what it got set to.
		nub_.addReply( path, result.c_str() );
	}
#endif
}

// watcher_nub.cpp
