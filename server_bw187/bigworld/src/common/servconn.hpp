/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef SERVER_CONNECTION_HPP
#define SERVER_CONNECTION_HPP

#include "cstdmf/stdmf.hpp"
#include "cstdmf/md5.hpp"
#include "math/sma.hpp"
#include "math/vector3.hpp"

#include "cstdmf/memory_stream.hpp"
#include "network/channel.hpp"
#include "network/endpoint.hpp"
#include "network/encryption_filter.hpp"
#include "network/public_key_cipher.hpp"
#include "common/client_interface.hpp"

#include "common/login_interface.hpp"

#include <set>


class ServerConnection;

/**
 *	This abstract class defines the interface that is called by
 *	ServerConnection::processInput.
 *
 *	@ingroup network
 */
class ServerMessageHandler
{
public:
	/// This method is called to create a new player as far as required to
	/// talk to the base entity. Only data shared between the base and the
	/// client is provided in this method - the cell data will be provided by
	/// onCellPlayerCreate later if the player is put on the cell also.
	virtual void onBasePlayerCreate( ObjectID id, EntityTypeID type,
		BinaryIStream & data, bool oldVersion ) = 0;

	/// This method is called to create a new player as far as required to
	/// talk to the cell entity. Only data shared between the cell and the
	/// client is provided in this method - the base data will have been
	/// previously provided by onBasePlayerCreate.
	virtual void onCellPlayerCreate( ObjectID id,
		SpaceID spaceID, ObjectID vehicleID, const Position3D & pos,
		float yaw, float pitch, float roll, BinaryIStream & data,
		bool oldVersion ) = 0;

	/// This method is called to indicate that the given entity is controlled
	/// by this client, at least as far as pose updates go. i.e. You may
	/// move the entity around with calls to addMove. Currently, control is
	/// implicit for the player entity and may not be withdrawn.
	virtual void onEntityControl( ObjectID id, bool control ) { }

	/// This method is called when an entity enters the client's AoI.
	/// If the entity has not been seen before then spaceID need not be
	/// recorded, since it will be provided again in onEntityCreate.
	virtual void onEntityEnter( ObjectID id, SpaceID spaceID,
		ObjectID vehicleID ) = 0;

	/// This method is called when an entity leaves the client's AoI.
	/// The CacheStamps are a record of the data received about the entity to
	/// this point, that should be reported to the server if the entity is
	/// seen again via the requestEntityUpdate message.
	virtual void onEntityLeave( ObjectID id, const CacheStamps & stamps ) = 0;

	/// This method is called in response to a requestEntityUpdate message to
	/// provide the bulk of the information about this entity. If the client has
	/// seen this entity before then data it already has is not resent
	/// (as determined by the CacheStamps sent in requestEntityUpdate).
	virtual void onEntityCreate( ObjectID id, EntityTypeID type,
		SpaceID spaceID, ObjectID vehicleID, const Position3D & pos,
		float yaw, float pitch, float roll, BinaryIStream & data,
		bool oldVersion ) = 0;

	/// This method is called by the server when it wants to provide multiple
	/// properties at once to the client for an entity in its AoI, such as when
	/// a detail level boundary is crossed.
	virtual void onEntityProperties( ObjectID id,
		BinaryIStream & data, bool oldVersion ) = 0;

	/// This method is called when the server sets a property on an entity.
	virtual void onEntityProperty( ObjectID objectID, int messageID,
		BinaryIStream & data, bool oldVersion ) = 0;

	/// This method is called when the server calls a method on an entity.
	virtual void onEntityMethod( ObjectID objectID, int messageID,
		BinaryIStream & data, bool oldVersion ) = 0;

	/// This method is called when the position of an entity changes.
	/// This will only be received for a controlled entity if the server
	/// overrides the position directly (physics correction, teleport, etc.)
	virtual void onEntityMove( ObjectID id, SpaceID spaceID, ObjectID vehicleID,
		const Position3D & pos, float yaw, float pitch, float roll,
		bool isVolatile ) = 0;

	/// This method is called when data associated with a space is received.
	virtual void spaceData( SpaceID spaceID, SpaceEntryID entryID,
		uint16 key, const std::string & data ) = 0;

	/// This method is called when the given space is no longer visible
	/// to the client.
	virtual void spaceGone( SpaceID spaceID ) = 0;

	/// This method is called to deliver peer-to-peer data.
	virtual void onVoiceData( const Mercury::Address & srcAddr,
		BinaryIStream & data ) {};

	/// This method is called to deliver large proxy data when it is complete
	virtual void onProxyData( uint16 proxyDataID, BinaryIStream & data ) { }

	/// This method is called when the server tells us to reset all our
	/// entities. The player entity may optionally be saved (but still
	/// should not be considered to be in the world (i.e. no cell part yet))
	virtual void onEntitiesReset( bool keepPlayerOnBase ) {}

	/// This method is called to indicate that the client entity has been
	/// restored from a (recent) backup due to a failure on the server.
	virtual void onRestoreClient( ObjectID id,
		SpaceID spaceID, ObjectID vehicleID,
		const Position3D & pos, const Direction3D & dir,
		BinaryIStream & data, bool oldVersion ) {}

	/// This method is called when an enableEntities request was rejected.
	/// Usually it is because the server is in the middle of an update.
	/// The handler should check latest and impending versions and try later.
	/// (also the impending version may need to have been fully downloaded)
	virtual void onEnableEntitiesRejected() {}

	/// This enum defines how close new resources are to being needed
	enum ImpendingVersionProximity
	{
		DOWNLOAD,	///< download resources to disk (enableEntities when done)
		LOADIN,		///< load resources into memory (& resourceLoadinComplete)
		MIGRATE		///< migrate all to new version (just do it without delay)
	};

	/// This method is called to notify the availability of a new impending
	/// version of resources, and how close they are to being needed.
	/// If latestVersion differs from impendingVersion, handlers will receive
	/// a call to this method after a successful enableEntities request,
	/// if they did not indicate that they have already downloaded it.
	/// This method is not called after an unsuccessful enableEntities request.
	virtual void onImpendingVersion( ImpendingVersionProximity proximity ) { }
};


class LoginHandler;
typedef SmartPointer< LoginHandler > LoginHandlerPtr;

/**
 *  This class provides client-push reliability for off-channel Mercury
 *  messages.  Basically it keeps sending requests until one comes back.
 *  This object deletes itself.
 */
class RetryingRequest :
	public Mercury::ReplyMessageHandler,
	public Mercury::TimerExpiryHandler,
	public SafeReferenceCount
{
public:
	/// Default retry period for requests (1s).
	static const int DEFAULT_RETRY_PERIOD = 1000000;

	/// Default timeout period for requests (8s).
	static const int DEFAULT_TIMEOUT_PERIOD = 8000000;

	/// Default limit for attempts.
	static const int DEFAULT_MAX_ATTEMPTS = 10;

	RetryingRequest( LoginHandler & parent,
		const Mercury::Address & addr,
		const Mercury::InterfaceElement & ie,
		int retryPeriod = DEFAULT_RETRY_PERIOD,
		int timeoutPeriod = DEFAULT_TIMEOUT_PERIOD,
		int maxAttempts = DEFAULT_MAX_ATTEMPTS,
		bool useParentNub = true );

	virtual ~RetryingRequest();

	// Inherited interface

	virtual void handleMessage( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data,
		void * arg );

	virtual void handleException( const Mercury::NubException & exc,
		void * arg );

	virtual int handleTimeout( Mercury::TimerID id,
		void * arg );

	// Interface to be implemented by derived classes

	/**
	 *  This method should stream on the args for the request.  The
	 *  startRequest() call is taken care of beforehand.
	 */
	virtual void addRequestArgs( Mercury::Bundle & bundle ) = 0;

	/**
	 *  This method will be called on the first reply.  This object will be
	 *  deleted immediately afterwards.
	 */
	virtual void onSuccess( BinaryIStream & data ) = 0;

	/**
	 *  This method is called if the request should fail for any reason.  This
	 *  object will be deleted immediately afterwards.
	 */
	virtual void onFailure( Mercury::Reason reason ) {}

	/**
	 *  This method removes this request from the parent's childRequests_ list,
	 *  which means it will be destroyed as soon as all of its outstanding 
	 *  requests have either been replied to (and ignored) or timed out.
	 */
	virtual void cancel();

protected:
	void setNub( Mercury::Nub * pNub );
	void send();

protected:
	SmartPointer< LoginHandler > pParent_;
	Mercury::Nub * pNub_;
	const Mercury::Address addr_;
	const Mercury::InterfaceElement & ie_;
	Mercury::TimerID timerID_;
	bool done_;

private:
	int retryPeriod_;
	int timeoutPeriod_;

	/// The number of attempts that have been initiated from this object.
	int numAttempts_;

	/// The number of attempts that have been initiated but not terminated.
	int numOutstandingAttempts_;

	/// The maximum number of attempts that this object will make.
	int maxAttempts_;
};

typedef SmartPointer< RetryingRequest > RetryingRequestPtr;


/**
 *  This class handles logging into the LoginApp.
 */
class LoginRequest : public RetryingRequest
{
public:
	LoginRequest( LoginHandler & parent );

	virtual void addRequestArgs( Mercury::Bundle & bundle );
	virtual void onSuccess( BinaryIStream & data );
	virtual void onFailure( Mercury::Reason reason );
};


/**
 *  This class handles the baseapp login stage of the login process.  It is a
 *  bit different to the above classes in that it doesn't retry on itself, it
 *  has a max attempts of 1 but its timeout code creates a new instance of
 *  itself.
 */
class BaseAppLoginRequest : public RetryingRequest
{
public:
	BaseAppLoginRequest( LoginHandler & parent );
	virtual ~BaseAppLoginRequest();

	virtual int handleTimeout( Mercury::TimerID id, void * arg );
	virtual void addRequestArgs( Mercury::Bundle & bundle );
	virtual void onSuccess( BinaryIStream & data );

	Mercury::Nub & nub() { return *pNub_; }
	Mercury::Channel & channel() { return *pChannel_; }
	virtual void cancel();

private:
	/// Each instance has its own Channel.  The winning instance transfers its
	/// Channel to the ServerConnection.
	Mercury::Channel * pChannel_;

	/// The attempt number.
	int attempt_;
};

typedef SmartPointer< BaseAppLoginRequest > BaseAppLoginRequestPtr;


/**
 *	This class is used to manage the various stages of logging into the server.
 *	This covers logging into both the loginapp and the baseapp.
 */
class LoginHandler : public SafeReferenceCount
{
public:
	LoginHandler( ServerConnection * pServerConnection, 
				  LogOnStatus loginNotSent = LogOnStatus::NOT_SET );
	~LoginHandler();

	void start( const Mercury::Address & loginAppAddr, LogOnParamsPtr pParams );
	void finish();

	void sendLoginAppLogin();
	void onLoginReply( BinaryIStream & data );

	void sendBaseAppLogin();
	void onBaseAppReply( BaseAppLoginRequestPtr pHandler,
		SessionKey sessionKey );

	void onFailure( Mercury::Reason reason );

	const LoginReplyRecord & replyRecord() const { return replyRecord_; }

	bool done() const						{ return done_; }
	int status() const						{ return status_; }
	LogOnParamsPtr pParams()				{ return pParams_; }

	const std::string & errorMsg() const	{ return errorMsg_; }

	void setError( int status, const std::string & errorMsg )
	{
		status_ = status;
		errorMsg_ = errorMsg;
	}

	ServerConnection * pServerConnection() const { return pServerConnection_; }
	ServerConnection & servConn() const { return *pServerConnection_; }

	const Mercury::Address & loginAddr() const { return loginAppAddr_; }
	const Mercury::Address & baseAppAddr() const { return baseAppAddr_; }

	void addChildRequest( RetryingRequestPtr pRequest );
	void removeChildRequest( RetryingRequestPtr pRequest );
	void addCondemnedNub( Mercury::Nub * pNub );

	int numBaseAppLoginAttempts() const { return numBaseAppLoginAttempts_; }

private:
	Mercury::Address	loginAppAddr_;
	Mercury::Address	baseAppAddr_;

	LogOnParamsPtr		pParams_;

	ServerConnection* 	pServerConnection_;
	LoginReplyRecord	replyRecord_;
	bool				done_;
	uint8				status_;

	std::string			errorMsg_;

	/// Number of BaseAppLoginRequests that have been created
	int numBaseAppLoginAttempts_;

	/// Currently active RetryingRequests
	typedef std::set< RetryingRequestPtr > ChildRequests;
	ChildRequests childRequests_;

	/// Nubs that were formerly owned by BaseAppLoginRequests that we have to
	/// clean up when we are destroyed.
	typedef std::vector< Mercury::Nub* > CondemnedNubs;
	CondemnedNubs condemnedNubs_;
};


struct ResUpdateRequest;

/**
 *	This class is used to represent a connection to the server.
 *
 *	@ingroup network
 */
class ServerConnection : public Mercury::BundlePrimer
{
public:
	ServerConnection();
	~ServerConnection();

	bool processInput();
	void registerInterfaces( Mercury::Nub & nub );

	void setInactivityTimeout( float seconds );

	LogOnStatus logOn( ServerMessageHandler * pHandler,
		const char * serverName,
		const char * username,
		const char * password,
		const char * publicKeyPath = NULL, // reverse order here
		uint16 port = 0 );

	LoginHandlerPtr logOnBegin(
		const char * serverName,
		const char * username,
		const char * password,
		const char * publicKeyPath = NULL,
		uint16 port = 0 );

	LogOnStatus logOnComplete(
		LoginHandlerPtr pLRH,
		ServerMessageHandler * pHandler );

	void enableReconfigurePorts() { tryToReconfigurePorts_ = true; }
	void enableEntities( bool impendingVersionDownloaded );

	bool online() const;
	bool offline() const				{ return !this->online(); }
	void disconnect( bool informServer = true );

	void channel( Mercury::Channel & channel ) { pChannel_ = &channel; }

	// Stuff that would normally be provided by ChannelOwner, however we can't
	// derive from it because of destruction order.
	Mercury::Channel & channel();
	Mercury::Bundle & bundle() { return this->channel().bundle(); }
	const Mercury::Address & addr() const;

	Mercury::EncryptionFilterPtr pFilter() { return pFilter_; }

	void addMove( ObjectID id, SpaceID spaceID, ObjectID vehicleID,
		const Vector3 & pos, float yaw, float pitch, float roll,
		bool onGround, const Vector3 & globalPos );
	//void rcvMoves( ObjectID id );

	BinaryOStream & startProxyMessage( int messageId );
	BinaryOStream & startAvatarMessage( int messageId );
	BinaryOStream & startEntityMessage( int messageId, ObjectID entityId );

	void requestEntityUpdate( ObjectID id,
		const CacheStamps & stamps = CacheStamps() );

	const std::string & errorMsg() const	{ return errorMsg_; }

	ObjectID connectedID() const			{ return id_; }
	void sessionKey( SessionKey key ) { sessionKey_ = key; }

	// ---- Statistics ----

	float latency() const		{ return pChannel_ ? float( pChannel_->roundTripTimeInSeconds() ) : 0.f; }
	float minLatency() const	{ return latencyTab_->min(); }
	float maxLatency() const	{ return latencyTab_->max(); }
	float avgLatency() const	{ return latencyTab_->average(); }

	uint32 packetsIn() const;
	uint32 packetsOut() const;

	uint32 bitsIn() const;
	uint32 bitsOut() const;

	uint32 messagesIn() const;
	uint32 messagesOut() const;

	double bpsIn() const;
	double bpsOut() const;

	double packetsPerSecondIn() const;
	double packetsPerSecondOut() const;

	double messagesPerSecondIn() const;
	double messagesPerSecondOut() const;

	int		bandwidthFromServer() const;
	void	bandwidthFromServer( int bandwidth );

	double movementBytesPercent() const;
	double nonMovementBytesPercent() const;
	double overheadBytesPercent() const;

	int movementBytesTotal() const;
	int nonMovementBytesTotal() const;
	int overheadBytesTotal() const;

	int movementMessageCount() const;

	void pTime( const double * pTime );

	/**
	 *	This method is used to return the pointer to current time.
	 *	It is used for server statistics and for syncronising between
	 *	client and server time.
	 */
	const double * pTime()					{ return pTime_; }

	double		serverTime( double gameTime ) const;
	double 		lastMessageTime() const;
	TimeStamp	lastGameTime() const;

	double		lastSendTime() const	{ return lastSendTime_; }
	double		minSendInterval() const	{ return minSendInterval_; }

	// ---- InterfaceMinder handlers ----
	void authenticate(
		const ClientInterface::authenticateArgs & args );
	void bandwidthNotification(
		const ClientInterface::bandwidthNotificationArgs & args );
	void updateFrequencyNotification(
		const ClientInterface::updateFrequencyNotificationArgs & args );

	void setGameTime( const ClientInterface::setGameTimeArgs & args );

	void resetEntities( const ClientInterface::resetEntitiesArgs & args );
	void createBasePlayer( BinaryIStream & stream, int length );
	void createCellPlayer( BinaryIStream & stream, int length );

	void spaceData( BinaryIStream & stream, int length );
	void spaceViewportInfo(
		const ClientInterface::spaceViewportInfoArgs & args );

	void enterAoI( const ClientInterface::enterAoIArgs & args );
	void enterAoIThruViewport(
		const ClientInterface::enterAoIThruViewportArgs & args );
	void enterAoIOnVehicle(
		const ClientInterface::enterAoIOnVehicleArgs & args );
	void leaveAoI( BinaryIStream & stream, int length );

	void createEntity( BinaryIStream & stream, int length );
	void updateEntity( BinaryIStream & stream, int length );

	// This unattractive bit of macros is used to declare all of the handlers
	// for (fixed length) messages sent from the cell to the client. It includes
	// methods such as all of the avatarUpdate handlers.
#define MF_BEGIN_COMMON_RELIABLE_MSG( MESSAGE )	\
	void MESSAGE( const ClientInterface::MESSAGE##Args & args );

#define MF_BEGIN_COMMON_PASSENGER_MSG MF_BEGIN_COMMON_RELIABLE_MSG
#define MF_BEGIN_COMMON_UNRELIABLE_MSG MF_BEGIN_COMMON_RELIABLE_MSG

#define MF_COMMON_ARGS( ARGS )
#define MF_END_COMMON_MSG()
#include "common/common_client_interface.hpp"
#undef MF_BEGIN_COMMON_RELIABLE_MSG
#undef MF_BEGIN_COMMON_PASSENGER_MSG
#undef MF_BEGIN_COMMON_UNRELIABLE_MSG
#undef MF_COMMON_ARGS
#undef MF_END_COMMON_MSG

	void detailedPosition( const ClientInterface::detailedPositionArgs & args );
	void forcedPosition( const ClientInterface::forcedPositionArgs & args );
	void controlEntity( const ClientInterface::controlEntityArgs & args );

	void voiceData( const Mercury::Address & srcAddr,
					BinaryIStream & stream, int length );

	void restoreClient( BinaryIStream & stream, int length );
	void restoreBaseApp( BinaryIStream & stream, int length );

	void versionPointIdentity(
		const ClientInterface::versionPointIdentityArgs & args );
	void versionPointSummary(
		const ClientInterface::versionPointSummaryArgs & args );
	void resourceFragment( BinaryIStream & stream, int length );

	void resourceVersionStatus(
		const ClientInterface::resourceVersionStatusArgs & args );
	void resourceVersionTag(
		const ClientInterface::resourceVersionTagArgs & args );

	void loggedOff( const ClientInterface::loggedOffArgs & args );

	void handleEntityMessage( int messageID, BinaryIStream & data, int length );

	void setMessageHandler( ServerMessageHandler * pHandler )
								{ if (pHandler_ != NULL) pHandler_ = pHandler; }

	/// This method returns the Nub for this connection.
	Mercury::Nub & nub() 						{ return nub_; }

	/**
	 *	The frequency of updates from the server.
	 */
	static const float & updateFrequency()		{ return s_updateFrequency_; }

	void initDebugInfo();

	void digest( const MD5::Digest & digest )	{ digest_ = digest; }
	const MD5::Digest digest() const			{ return digest_; }

	void send();


	uint32 latestVersion()		{ return latestVersion_; }
	uint32 impendingVersion()	{ return impendingVersion_; }

	bool identifyVersionPoint( const std::string & pt,
		const MD5::Digest & digest0, const MD5::Digest & digest1,
		uint32 & version, uint8 & index );
	bool summariseVersionPoint( uint32 version, const std::string & pt,
		MD5::Digest & changeMD5, MD5::Digest & censusMD5 );
	uint8 downloadResourceVersion( uint32 version, const std::string & pt,
		const std::string & resource, FILE * pFile, float * progressVal = NULL);

	void blockedCallback( void (*requestBlockedCB)() );

	void resourceLoadinComplete( uint32 version );

private:
	double appTime() const;
	void updateStats();

	// ---- Data members ----
	uint32				sessionKey_;
	std::string username_;

	// ---- Statistics ----
	class Statistic
	{
	public:
		Statistic() : value_( 0 ),
			oldValue_( 0 ),
			changePerSecond_( 0 )
		{}

		void update( double deltaTime )
		{
			changePerSecond_ = (value_ - oldValue_)/deltaTime;
			oldValue_ = value_;
		}

		operator uint32() const 				{ return value_; }
		void operator =( uint32 value )			{ value_ = value; }

		void operator +=( uint32 value )		{ value_ += value; }

		double changePerSecond() const			{ return changePerSecond_; }

	private:
		uint32 value_;
		uint32 oldValue_;
		double changePerSecond_;
	};

	Statistic packetsIn_;
	Statistic packetsOut_;

	Statistic bitsIn_;
	Statistic bitsOut_;

	Statistic messagesIn_;
	Statistic messagesOut_;

	Statistic totalBytes_;
	Statistic movementBytes_;
	Statistic nonMovementBytes_;
	Statistic overheadBytes_;

private:
	ServerConnection(const ServerConnection&);
	ServerConnection& operator=(const ServerConnection&);

	void initialiseConnectionState();
	bool setKeyFromResource( const std::string & path );

	virtual void primeBundle( Mercury::Bundle & bundle );
	virtual int numUnreliableMessages() const;

	ServerMessageHandler * pHandler_;

	ObjectID	id_;
	int			bandwidthFromServer_;

	const double * pTime_;
	double	lastTime_;
	double	lastSendTime_;
	double	minSendInterval_;

	Mercury::Nub 		nub_;
	Mercury::Channel*	pChannel_;
	Mercury::EncryptionFilterPtr pFilter_;

	bool		everReceivedPacket_;
	bool		tryToReconfigurePorts_;
	bool		entitiesEnabled_;

	float				inactivityTimeout_;
	MD5::Digest			digest_;

	/// This is a simple class to handle what time the client thinks is on the
	/// server.
	class ServerTimeHandler
	{
	public:
		ServerTimeHandler();

		void tickSync( uint8 newSeqNum, double currentTime );
		void gameTime( TimeStamp gameTime, double currentTime );

		double		serverTime( double gameTime ) const;
		double 		lastMessageTime() const;
		TimeStamp 	lastGameTime() const;


	private:
		static const double UNINITIALISED_TIME;

		uint8 tickByte_;
		double timeAtSequenceStart_;
		TimeStamp gameTimeAtSequenceStart_;
	} serverTimeHandler_;

	static float s_updateFrequency_;	// frequency of updates from the server.

	std::string errorMsg_;


	uint8	sendingSequenceNumber_;
	uint8	receivingSequenceNumber_;
	bool	sendingSequenceIncrement_;
	uint8	lastRequiredExplicitSeqNum_;
	bool	alwaysSendExplicitUpdates_;

	uint64	timeSent_[ 256 ];		// for calculating latency
	SMA<float>* latencyTab_;

	ObjectID	idAlias_[ 256 ];

	SpaceViewportID	nextEntitysViewportID_;
	ObjectID		nextEntitysVehicleID_;

	/**
	 *	This helper class stores the details of viewports into spaces
	 */
	class SpaceViewport
	{
	public:
		SpaceViewport();

		ObjectID	gatewaySrcID_;
		ObjectID	gatewayDstID_;
		SpaceID		spaceID_;
		bool		selfControlled_;
		Vector3		rcvdPosition_;	// TODO: use some history!
		Vector3		sentPositions_[ 256 ];

		/**
		 *	This reference position is rounded so that the rounding of points
		 *	relative to this are consistent. Got that? :)
		 */
		const Vector3 relativePosition( uint8 refSeqNum ) const
		{
			const Vector3 & pos = selfControlled_ ?
				sentPositions_[ refSeqNum ] : rcvdPosition_;
			return Vector3( floorf( pos.x ), floorf( pos.y ), floorf( pos.z ) );
		}
	};

	typedef std::map<SpaceViewportID,SpaceViewport> SpaceViewports;
	SpaceViewports				spaceViewports_;

	union EntityViewport
	{
		EntityViewport() : vehicleID_( -1 ) { svid_ = 0; }

		SpaceViewportID	svid_;
		ObjectID		vehicleID_;

		bool onVehicle() const				{ return int(vehicleID_) > 0; }
		void svid( SpaceViewportID svid )	{ vehicleID_ = -1; svid_ = svid; }
		void vehicleID( ObjectID vehicleID ){ vehicleID_ = vehicleID; }
	};
	typedef std::map<ObjectID,EntityViewport> EntityViewports;
	EntityViewports				defaultViewportIDs_;

	std::map<SpaceID,int>		spaceSeenCount_;

	SpaceViewportID				personalSpaceViewport_;

	std::map<ObjectID,bool>		sentOnGround_;

	SpaceViewportID svidFollow( ObjectID id, ObjectID & vehicleID ) const;
	SpaceViewport & getOrSetSpaceViewport( ObjectID id, ObjectID & vehicleID );
	void detailedPositionReceived( ObjectID id, SpaceViewport & svp,
		ObjectID vehicleID, const Vector3 & position );

	void sendSetViewportAck( ObjectID id, SpaceViewportID svid );
	void sendSetVehicleAck( ObjectID id, ObjectID vehicleID );

	const int FIRST_AVATAR_UPDATE_MESSAGE;
	const int LAST_AVATAR_UPDATE_MESSAGE;

	uint32	latestVersion_;
	uint32	impendingVersion_;
	uint32	imminentVersion_;
	uint32	previousVersion_;

	bool	versForCallIsOld_;
	bool	sendResourceVersionTag_;
	double	sendResourceVersionTagUntil_;

	void queueRURequest( ResUpdateRequest * rur );
	void completeRURequest( ResUpdateRequest * rur );
	ResUpdateRequest * findRURequest( uint16 rid, bool erase = true );
	void cancelRURequests();

	void sendRequestsAndBlock();
	void addOutstandingRequests();

	void (*requestBlockedCB_)();

	std::vector<struct ProxyDataTrack*>		proxyData_;

	MemoryOStream createCellPlayerMsg_;

public:
	Mercury::PublicKeyCipher & publicKey() { return publicKey_; }

private:
	Mercury::PublicKeyCipher publicKey_;
};


#ifdef CODE_INLINE
#include "servconn.ipp"
#endif



#endif // SERVER_CONNECTION_HPP
