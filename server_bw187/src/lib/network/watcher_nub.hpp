/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef WATCHER_NUB_HPP
#define WATCHER_NUB_HPP

#include "endpoint.hpp"

/**
 * 	This structure is the network message used to register a watcher.
 *
 * 	@ingroup watcher
 */
struct WatcherRegistrationMsg
{
	int		version;	// currently 0
	int		uid;		// your uid
	int		message;	// WATCHER_...
	int		id;			// e.g. 14
	char	abrv[32];	// e.g. "cell14"
	char	name[64];	// e.g. "Cell 14"
};

#ifdef _WIN32
#pragma warning(disable: 4200)
#endif

/**
 * 	This structure is used to send watcher data across the network.
 *
 * 	@ingroup watcher
 */
struct WatcherDataMsg
{
	int		message;
	int		count;
	char	string[0];
};


/**
 * 	This enumeration contains the possible watcher message IDs.
 *
 * 	@ingroup watcher
 */
enum WatcherMsg
{
	WATCHER_MSG_REGISTER = 0,
	WATCHER_MSG_DEREGISTER = 1,
	WATCHER_MSG_FLUSHCOMPONENTS = 2,

	WATCHER_MSG_GET = 16,
	WATCHER_MSG_SET = 17,
	WATCHER_MSG_TELL = 18,
	WATCHER_MSG_GET_WITH_DESC = 20,

	WATCHER_MSG_EXTENSION_START = 107
};

#define WATCHER_DESC_MASK	0x4

/* Packets:
	Reg and Dereg are just a WatcherRegistrationMsg Get and Set are a
	WatcherDataMsg followed by 'count' strings (for get) or string pairs (for
	set and tell). Every get/set packet is replied to with a tell packet.
*/

/**
 *	This class is used to process requests that the WatcherNub has
 *	received. You need one of these.
 *
 * 	@ingroup watcher
 */
class WatcherRequestHandler
{
public:
	virtual ~WatcherRequestHandler() {};

	/**
	 *	This function is called by the WatcherNub when it receives a
	 *	WATCHER_MSG_GET request.
	 *
	 *	@param	path	The object to get. It has no leading slash
	 * 					and will only have a trailing slash for list-style 'GET'
	 * 					requests. I think you can probably ignore it in any
	 * 					case.
	 * 	@param withDesc Boolean flag that indicates whether watcher description
	 *					should be returned.
	 *	@see	processWatcherSetRequest
	 */
	virtual void processWatcherGetRequest( const char * path,
		   	bool withDesc = false ) = 0;

	/**
	 *	This function is called by the WatcherNub when it receives a
	 *	WATCHER_MSG_SET request.
	 *
	 *	@param	path	The object to set. It will have no trailing slash.
	 *	@param	valueString			The value to set the object to.
	 *
	 *	@see	processWatcherGetRequest
	 */
	virtual void processWatcherSetRequest( const char * path,
		const char * valueString ) = 0;

	virtual void processExtensionMessage( int messageID,
				char * data, int dataLen, const Mercury::Address & addr );
};


/**
 *	This class is the core of the Watcher. It is responsible for receiving
 *	watcher requests and sending replies.
 *
 * 	@ingroup watcher
 */
class WatcherNub
{
public:
	WatcherNub();
	~WatcherNub();

	bool init( const char * listeningInterface, uint16 listeningPort );

	int registerWatcher( int id, char * abrv, char * longName,
			const char * listeningInterface = NULL,
			uint16 listeningPort = 0 );
	int deregisterWatcher();
	int resetServer();

	void setRequestHandler( WatcherRequestHandler * pWatcherRequestHandler );

	// get the file descriptor for the socket
	int getSocketDescriptor();
	// read (or try to read) a request
	bool receiveRequest();

	Endpoint & socket()		{ return socket_; }

	// add to the reply packet - only callable from within the callback.
	bool addReply( const char * identifier, const char * desc,
			const char * value );

	bool addReply( const char * identifier,	const char * value );

private:
	void notifyMachineGuard();
	int watcherControlMessage( int message, bool withid );

	int		id_;
	bool	registered_;

	WatcherRequestHandler *wrh_;

	bool	insideReceiveRequest_;
	char	*replyPacket_;
	char	*replyPointer_;
	bool	reachedPacketLimit_;

	char	*requestPacket_;

	bool	isInitialised_;

	Endpoint	socket_;

	char	abrv_[32];
	char	name_[64];
};


/**
 *	This class handles standard watcher requests.
 *
 * 	@ingroup watcher
 */
class StandardWatcherRequestHandler : public WatcherRequestHandler
{
public:
	StandardWatcherRequestHandler( WatcherNub & nub );

	virtual void processWatcherGetRequest( const char * path,
		bool withDesc = false );
	virtual void processWatcherSetRequest( const char * path,
		const char * valueString );

private:
	WatcherNub & nub_;
};

#endif // WATCHER_NUB_HPP
