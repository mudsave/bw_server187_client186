/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef PROXY_HPP
#define PROXY_HPP

#include "base.hpp"
#include "baseapp_int_interface.hpp"
#include "rate_limit_message_filter.hpp"

#include "cstdmf/memory_stream.hpp"
#include "cstdmf/profile.hpp"
#include "cstdmf/concurrency.hpp"

#include "common/baseapp_ext_interface.hpp"
#include "common/client_interface.hpp"

#include "network/channel.hpp"

#include "pyscript/stl_to_py.hpp"

#include "mailbox.hpp"


#ifdef INSTRUMENTATION
	//#define LOG_INSTRUMENTATION
		// don't write out stats to "proxy.log" from destructor
#endif

class ClientEntityMailBox;
class BaseEntityMailBox;
class MemoryOStream;



/*~ class BigWorld.Proxy
 *	@components{ base }
 *
 *	The Proxy is a special type of Base that has an associated Client. As such,
 *	it handles all the server updates for that Client. There is no direct script
 *	call to create a Proxy specifically.
 *
 */

/**
 *	This class is used to represent a proxy. A proxy is a special type of base.
 *	It has an associated client.
 */
class Proxy: public Base
{
	// Py_InstanceHeader( Proxy )
	Py_Header( Proxy, Base )

public:
	static const int MAX_INCOMING_PACKET_SIZE = 1024;
	static const int MAX_OUTGOING_PACKET_SIZE = 1024;

	Proxy( ObjectID id, DatabaseID dbID, EntityType * pType );
	~Proxy();

 	void onClientDead( Mercury::Reason reason = Mercury::REASON_SUCCESS,
		bool shouldExpectClient = true );
	void onClientNoSuchPort();

	void onDestroy();

	void restoreClient( SpaceID spaceID );

	void backup( BinaryOStream & stream );
	void restore( BinaryIStream & stream, bool isNewStyle );

	void proxyRestoreTo();
	void onEnableWitnessAck();

	/// @name Accessors
	//@{
	bool hasClient() const			{ return pClientChannel_ != NULL; }
	bool isClientChannel( Mercury::Channel * pClientChannel ) const	
	{ return (pClientChannel_ != NULL) && (pClientChannel == pClientChannel_); }

	bool entitiesEnabled() const	{ return entitiesEnabled_; }

	const Mercury::Address & clientAddr() const;
	SessionKey sessionKey() const				{ return sessionKey_; }

	ClientEntityMailBox * pClientEntityMailBox() const
									{ return pClientEntityMailBox_; }

	uint32 presentVersion()	{ return presentVersion_; }
	uint32 futureVersion()	{ return futureVersion_; }
	void noticeImpendingVersion();
	bool loadedImminentVersion( uint32 v ) { return loadedVersion_ == v; }
	//@}

	// Script related methods
	PyObject * pyGetAttribute( const char * attr );
	int pySetAttribute( const char * attr, PyObject * value );

	PY_AUTO_METHOD_DECLARE( RETOK, giveClientTo, ARG( PyObjectPtr, END ) )
	bool giveClientTo( PyObjectPtr pDestProxy );

	bool attachToClient( const Mercury::Address & clientAddr,
		Mercury::ReplyID loginReplyID = Mercury::REPLY_ID_NONE );

	PyObject * pyGet_client();
	PY_RO_ATTRIBUTE_SET( client )
	PY_RO_ATTRIBUTE_DECLARE( hasClient(), hasClient )
	PY_RO_ATTRIBUTE_DECLARE( clientAddr(), clientAddr )

	PY_RO_ATTRIBUTE_DECLARE( entitiesEnabled_, entitiesEnabled )

	PY_RO_ATTRIBUTE_DECLARE( wardsHolder_, wards )

	PY_WO_ATTRIBUTE_GET( bandwidthPerSecond ) // read-only for now
	int pySet_bandwidthPerSecond( PyObject * value );

	PY_RO_ATTRIBUTE_DECLARE( roundTripTime(), roundTripTime )
	PY_RO_ATTRIBUTE_DECLARE( timeSinceHeardFromClient(),
			timeSinceHeardFromClient )
	PY_RW_ATTRIBUTE_DECLARE( latencyTriggersHolder_, latencyTriggers )
	PY_RO_ATTRIBUTE_DECLARE( latencyAtLastCheck_, latencyLast )

	PY_AUTO_METHOD_DECLARE( RETOWN, addProxyData,
		ARG( uint16, ARG( PyObjectPtr, END ) ) )
	PyObject * addProxyData( uint16 pdid, PyObjectPtr pPyString );

	/* Internal Interface */

	void cellEntityCreated();
	void cellEntityDestroyed( const Mercury::Address * pSrcAddr );

	// Start of messages forwarded from cell ...

	void sendToClient( const BaseAppIntInterface::sendToClientArgs & args );
	void createCellPlayer( BinaryIStream & data );
	void spaceData( BinaryIStream & data );
	void spaceViewportInfo(
		const BaseAppIntInterface::spaceViewportInfoArgs & args );
	void enterAoI( const BaseAppIntInterface::enterAoIArgs & args );
	void enterAoIThruViewport( const BaseAppIntInterface::enterAoIThruViewportArgs & args );
	void enterAoIOnVehicle( const BaseAppIntInterface::enterAoIOnVehicleArgs & args );
	void leaveAoI( BinaryIStream & data );
	void createEntity( BinaryIStream & data );
	void updateEntity( BinaryIStream & data );

	// The following bit of unattractive macros is used to declare many of
	// the message handlers for messages sent from the cell that are to be
	// forwarded to the client. This includes the handlers for all of the
	// avatarUpdate messages.
#define MF_BEGIN_COMMON_RELIABLE_MSG( MESSAGE )								\
	void MESSAGE( const BaseAppIntInterface::MESSAGE##Args & args );

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

	void detailedPosition(
		const BaseAppIntInterface::detailedPositionArgs & args );

	void forcedPosition( const BaseAppIntInterface::forcedPositionArgs & args );

	void modWard( const BaseAppIntInterface::modWardArgs & args );

	// ... end of messages forwarded from cell.

	void pipeIntMessage( int type, BinaryIStream & data, uint length );

	bool sendToClient();

	/* External Interface */

	// receive an update from the client for our position
	void avatarUpdateImplicit(
		const BaseAppExtInterface::avatarUpdateImplicitArgs & args );
	void avatarUpdateExplicit(
		const BaseAppExtInterface::avatarUpdateExplicitArgs & args );
	void avatarUpdateWardImplicit(
		const BaseAppExtInterface::avatarUpdateWardImplicitArgs & args );
	void avatarUpdateWardExplicit(
		const BaseAppExtInterface::avatarUpdateWardExplicitArgs & args );

	void requestEntityUpdate( BinaryIStream & data );

	void enableEntities( const BaseAppExtInterface::enableEntitiesArgs & args );

	void setSpaceViewportAck(
		const BaseAppExtInterface::setSpaceViewportAckArgs & args );
	void setVehicleAck( const BaseAppExtInterface::setVehicleAckArgs & args );

	void restoreClientAck(
		const BaseAppExtInterface::restoreClientAckArgs & args );

	void identifyVersionPoint( BinaryIStream & data );
	void summariseVersionPoint( BinaryIStream & data );
	void commenceResourceDownload( BinaryIStream & data );

	void disconnectClient(
			const BaseAppExtInterface::disconnectClientArgs & args );

	void pipeExtMessage( int type, BinaryIStream & data, uint length );

	void handleExtMessage( int type, BinaryIStream & data, uint length );

	// selectors for which direction delay and loss calls affect
	enum
	{
		whichUDPIncoming = 1,
		whichUDPOutgoing = 2,
		whichUDPBoth = 3
	};

	// set a delay for this client
	void delay(uint msecMin, uint msecMax = 0, int whichUDP = whichUDPBoth);
	// set a loss for this client
	void loss(float percantageLoss, int whichUDP = whichUDPBoth);

	static Watcher & watcher();

	double roundTripTime() const;
	double timeSinceHeardFromClient() const;

	bool isRestoringClient() const	{ return isRestoringClient_; }

	typedef std::vector< ObjectID > Wards;
	typedef std::vector< float > LatencyTriggers;

	bool tickWorkerThread();

	void callClientMethod( BinaryIStream &data );

	static void sendsAuthToClient( bool v )	{ s_sendsAuthToClient = v; }
	static bool sendsAuthToClient()			{ return s_sendsAuthToClient; }

	const std::string & encryptionKey() const { return encryptionKey_; }
	void encryptionKey( const std::string & data ) { encryptionKey_ = data; }

	RateLimitMessageFilterPtr pRateLimiter() 		{ return pRateLimiter_; }

	void cellBackupHasWitness( bool v ) 	{ cellBackupHasWitness_ = v; }

	static float defaultAoIRadius() { return s_defaultAoIRadius; }
	static void defaultAoIRadius( float newRadius )
	{
		s_defaultAoIRadius = newRadius;
	}

	static float minClientInactivityResendDelay()
	{
		return s_minClientInactivityResendDelay;
	}
	static void minClientInactivityResendDelay( float newDelay )
	{
		s_minClientInactivityResendDelay = newDelay;
	}

private:
	// -------------------------------------------------------------------------
	// Section: Client channel stuff
	// -------------------------------------------------------------------------

	class ClientBundlePrimer : public Mercury::BundlePrimer
	{
	public:
		ClientBundlePrimer( Proxy & proxy ) : proxy_( proxy ) {}
		void primeBundle( Mercury::Bundle & bundle );
		int numUnreliableMessages() const;

	private:
		Proxy & proxy_;
	};

	friend class ClientBundlePrimer;

	bool hasOutstandingEnableWitness() const
		{ return numOutstandingEnableWitness_ != 0; }

public:
	Mercury::Channel & clientChannel() { return *pClientChannel_; }
private:
	Mercury::Bundle & clientBundle() { return pClientChannel_->bundle(); }

	void setClientChannel( Mercury::Channel * pChannel );
	void addOpportunisticData( Mercury::Bundle * b );
	void sendEnableDisableWitness( bool enable = true,
		bool isRestore = false );
	void logBadWardWarning( ObjectID ward );

	Mercury::Channel *	pClientChannel_;
	ClientBundlePrimer	clientBundlePrimer_;

	std::string			encryptionKey_;
	SessionKey			sessionKey_;

	ClientEntityMailBox * pClientEntityMailBox_;
	friend class ClientEntityMailBox;

	bool		entitiesEnabled_;
	bool		basePlayerCreatedOnClient_;

	Wards			wards_;
	PySTLSequenceHolder< Wards >			wardsHolder_;
	TimeStamp		lastModWardTime_;

	LatencyTriggers	latencyTriggers_;
	PySTLSequenceHolder< LatencyTriggers >	latencyTriggersHolder_;

	float	latencyAtLastCheck_;

	bool	isRestoringClient_;

	typedef std::vector<struct ResourceDownloadStatus *> ResourceDownloads;
	ResourceDownloads	resourceDownloads_;
	SimpleMutex			resourceDownloadLock_;
	class WorkerJob *	pWorkman_;

	class ProxyPusher *	selfMotivator_;

	TimeStamp			lastLatencyCheckTime_;

	uint32				presentVersion_;
	uint32				futureVersion_;
	uint32				loadedVersion_;

	uint16				nextProxyDataID_;

	ProfileGroup		pro_;


#define getProIn	(pro_[BaseApp::insIn])
#define getProOut	(pro_[BaseApp::insOut])

	static bool s_sendsAuthToClient;

	static float s_defaultAoIRadius;

	static float s_minClientInactivityResendDelay;

	// -------------------------------------------------------------------------
	// Section: Rate limiting declarations
	// -------------------------------------------------------------------------

	/**
	 *	Callback for the RateLimitMessageFilter.
	 */
	class RateLimitCallback :
			public RateLimitMessageFilter::Callback
	{
	public:
		/**
		 *	Constructor.
		 *
		 *	@param pProxy 	The associated proxy for this callback.
		 */
		RateLimitCallback( Proxy * pProxy ): pProxy_( pProxy )
		{}
		/**
		 *	Destructor.
		 */
		virtual ~RateLimitCallback()
		{}

		/**
		 *	Return the associated proxy.
		 */
		Proxy * pProxy() { return pProxy_; }


	public: // From RateLimitMessageFilter::Callback

		virtual BufferedMessage * createBufferedMessage(
			const Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data, Mercury::InputMessageHandler * pHandler );

		virtual void onFilterLimitsExceeded( const Mercury::Address & srcAddr,
			BufferedMessage * pMessage );

	private:
		Proxy * pProxy_;

	};


	/**
	 *	A buffered message for a proxy from a client.
	 */
	class ProxyBufferedMessage : public BufferedMessage
	{
	public:
		/**
		 * 	Constructor.
		 *
		 * 	@param header 		the message header
		 * 	@param data 		the message data
		 * 	@param pHandler 	the destination input message handler instance
		 * 	@parma callback 	the associated proxy's RateLimitCallback object
		 */
		ProxyBufferedMessage( const Mercury::UnpackedMessageHeader & header,
					BinaryIStream & data,
					Mercury::InputMessageHandler * pHandler ):
				BufferedMessage( header, data, pHandler )
		{}

		/**
		 *	Destructor.
		 */
		virtual ~ProxyBufferedMessage() {}

	public: // overridden from BufferedMessage

		virtual void dispatch( RateLimitMessageFilter::Callback * pCallback,
			const Mercury::Address & srcAddr );

	};


	/**
	 *	Pass ownership of the rate limiter and its associated callback object
	 *	to this proxy.
	 *
	 *	@param pRateLimiter 		the RateLimitMessageFilter object
	 */
	void pRateLimiter( RateLimitMessageFilterPtr pRateLimiter )
	{
		pRateLimiter_ = pRateLimiter;
		if (pRateLimiter_)
		{
			pRateLimiter_->setCallback( &rateLimitCallback_ );
		}
	}


	RateLimitMessageFilterPtr		pRateLimiter_;
	RateLimitCallback				rateLimitCallback_;

	bool			cellHasWitness_;
	bool			cellBackupHasWitness_;
	int				numOutstandingEnableWitness_;
};


/**
 *	This class is a mailbox that delivers to the client.
 */
class ClientEntityMailBox: public PyEntityMailBox
{
public:
	ClientEntityMailBox( Proxy & proxy ) :
		PyEntityMailBox(),
		proxy_( proxy ) {}

	virtual PyObject * pyGetAttribute( const char * attr );
	virtual BinaryOStream * getStream( const MethodDescription & methodDesc );
	virtual void sendStream();
	virtual const MethodDescription * findMethod( const char * attr ) const;

	BinaryOStream * getStreamForObjectID( int methodID, ObjectID objectID );

	const EntityDescription& getEntityDescription() const;

	EntityMailBoxRef ref() const;
	static EntityMailBoxRef static_ref( PyObject * pThis )
		{ return ((const ClientEntityMailBox*)pThis)->ref(); }


private:
	Proxy & proxy_;
};

#endif // PROXY_HPP
