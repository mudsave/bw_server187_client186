/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef LOGINAPP_HPP
#define LOGINAPP_HPP

#include "cstdmf/memory_stream.hpp"


#include "network/mercury.hpp"
#include "network/netmask.hpp"
#include "network/public_key_cipher.hpp"

#include "resmgr/datasection.hpp"
#include "server/stream_helper.hpp"

#include "common/doc_watcher.hpp"

#include "login_int_interface.hpp"

#include "dbmgr/worker_thread.hpp"

typedef Mercury::ChannelOwner DBMgr;

class KeepWGSHeartbeat : public Mercury::TimerExpiryHandler
{
    public:
        KeepWGSHeartbeat();

        ~KeepWGSHeartbeat();

        virtual int handleTimeout( Mercury::TimerID id, void * arg );

    private:

        Mercury::TimerID timerID_;
};

class WGSAuth: public Mercury::TimerExpiryHandler
{
    public:
        WGSAuth(const Mercury::Address & source, 
                const Mercury::ReplyID replyID, LogOnParamsPtr pParams );

        ~WGSAuth();

        virtual int handleTimeout( Mercury::TimerID id, void * arg );

        const Mercury::Address &source() { return source_; };
        const Mercury::ReplyID replyID(){ return replyID_; };
        LogOnParamsPtr pParams() { return pParams_; };

    private:

        Mercury::Address source_;
        Mercury::ReplyID replyID_;
        LogOnParamsPtr pParams_;
        Mercury::TimerID timerID_;
};

class WGSServerAuthentication : public Mercury::InputNotificationHandler ,public Mercury::TimerExpiryHandler
{
    public:

        enum ERROR_WGS
        {
            WGS_SERVER_NOT_CONNECT = 101,
            WGS_NOT_GET_DATA = 102,
            WGS_IS_BUSY = 103,
            WGS_INPUT_FORMAT_ERROR = 104,
            WGS_TIMEOUT = 213
        };

        WGSServerAuthentication(const std::string &addr, int port);

        virtual ~WGSServerAuthentication();

        int handleInputNotification(int fd);

        virtual int handleTimeout( Mercury::TimerID id, void * arg );

        void stopWGSAuthentication();

        int sendAuthentication(const Mercury::Address &source, 
                const Mercury::ReplyID replyID, LogOnParamsPtr pParams);

        int connect();

        int reconnect();

        int eraseWGSLogin(const std::string &id);

        int handleReplyMsg(std::string &msg);

    private:

        int sock_;
        int serPort_;
        char *buf_;
        Endpoint socket_;
        std::string recvBuffer_;
        std::string serAddr_;
        sockaddr_in wgsService_; 
        Mercury::TimerID timerID_ ;
        std::map<std::string, WGSAuth *> WGSAuthCachedMap_;
};
/**
 *	This class implements the main singleton object in the login application.
 */
class LoginApp
{
public:
	LoginApp( uint16 loginPort = 0 );

	bool init( int argc, char * argv[], uint16 loginPort );
	void run();

	// external methods

	virtual void login( const Mercury::Address & source,
		Mercury::UnpackedMessageHeader & header, BinaryIStream & data );

	virtual void webServicesRegister( const Mercury::Address & source,
		Mercury::UnpackedMessageHeader & header, BinaryIStream & data );

	virtual void probe( const Mercury::Address & source,
		Mercury::UnpackedMessageHeader & header, BinaryIStream & data );

	// internal methods
	void controlledShutDown( const Mercury::Address & source,
		Mercury::UnpackedMessageHeader & header, BinaryIStream & data );

//	void systemOverloadStatus(
//		const LoginIntInterface::systemOverloadStatusArgs & args );


	const NetMask& 	netMask() const			{ return netMask_; }
	unsigned long	externalIP() const 		{ return externalIP_; }

public:
	static void sendFailure( const Mercury::Address & addr,
		Mercury::ReplyID replyID, int status, const char * msg = NULL,
		LogOnParamsPtr pParams = NULL );

	void sendAndCacheSuccess( const Mercury::Address & addr,
			Mercury::ReplyID replyID, const LoginReplyRecord & replyRecord,
			LogOnParamsPtr pParams );

	Mercury::Nub &	intNub()		{ return intNub_; }
	Mercury::Nub &	extNub()		{ return extNub_; }

	DBMgr & dbMgr()					{ return *dbMgr_.pChannelOwner(); }
	const DBMgr & dbMgr() const		{ return *dbMgr_.pChannelOwner(); }

	bool isDBReady() const
	{
		return this->dbMgr().channel().isEstablished();
	}

	void controlledShutDown()
	{
		isControlledShutDown_ = true;
		intNub_.breakProcessing();
	}

	uint64 maxLoginDelay() const	{ return maxLoginDelay_; }

	static LoginApp & instance()	{ return *pInstance_; }
	static LoginApp * pInstance()	{ return pInstance_; }

	uint8 systemOverloaded() const
	{ return systemOverloaded_; }

	void systemOverloaded( uint8 status )
	{
		systemOverloaded_ = status;
		systemOverloadedTime_ = timestamp();
	}

	bool autoRegister(){ return autoRegister_;}; 
    int registerTimeout(){ return registerTimeout_;};
    void removeCachedLoginMap( const Mercury::Address & addr);

    std::string &validateServer(){ return validateServer_; };
    std::string &registerServer(){ return registerServer_; };
        int WGSAuthContinue( const Mercury::Address & addr,
                Mercury::ReplyID replyID, LogOnParamsPtr pParams );
        WGSServerAuthentication *wgsAuth() { return wgsAuth_; };
private:
	/**
	 *	This class is used to store a recent, successful login. It is used to
	 *	handle the case where the reply to the client is dropped.
	 */
	class CachedLogin
	{
	public:
		// We set creationTime_ to 0 to indicate that the login is pending.
		CachedLogin() : creationTime_( 0 ) {}

		bool isTooOld() const;
		bool isPending() const;

		void pParams( LogOnParamsPtr pParams ) { pParams_ = pParams; }
		LogOnParamsPtr pParams() { return pParams_; }

		void replyRecord( const LoginReplyRecord & record );
		const LoginReplyRecord & replyRecord() const { return replyRecord_; }

		/// This method re-initialises the cache object to indicate that it is
		/// pending.
		void reset() { creationTime_ = 0; }

	private:
		uint64 creationTime_;
		LogOnParamsPtr pParams_;
		LoginReplyRecord replyRecord_;
	};

	bool handleResentPendingAttempt( const Mercury::Address & addr,
		Mercury::ReplyID replyID );
	bool handleResentCachedAttempt( const Mercury::Address & addr,
		LogOnParamsPtr pParams, Mercury::ReplyID replyID );

	void sendSuccess( const Mercury::Address & addr,
		Mercury::ReplyID replyID, const LoginReplyRecord & replyRecord,
		const std::string & encryptionKey );
        void sendSuccess( const Mercury::Address & addr,
                Mercury::ReplyID replyID, const LoginReplyRecord & replyRecord,
                LogOnParamsPtr pParams );

	void rateLimitSeconds( uint newPeriod )
	{ rateLimitDuration_ = newPeriod * stampsPerSecond(); }
	uint rateLimitSeconds() const
	{ return rateLimitDuration_ / stampsPerSecond(); }

	bool setKeyFromResource( const std::string & path ); 

	Mercury::PublicKeyCipher privateKey_;
	Mercury::Nub		intNub_;
	Mercury::Nub		extNub_;

	NetMask 			netMask_;
	unsigned long 		externalIP_;

	bool				isControlledShutDown_;

	uint8				systemOverloaded_;
	uint64				systemOverloadedTime_;

	bool				allowLogin_;
	bool				allowProbe_;
	bool				logProbes_;
	bool				autoRegister_;
    int                 registerTimeout_;

	typedef std::map< Mercury::Address, CachedLogin > CachedLoginMap;
	CachedLoginMap 		cachedLoginMap_;


	typedef std::map< Mercury::Address, uint64 > CachedRegistMap;
	CachedRegistMap 		cachedRegistMap_;
    SimpleMutex				mapRegistLock_;

    WorkerThreadMgr		workerThreadMgr_;
    WorkerThreadPool	threadPool_;

	AnonymousChannelClient dbMgr_;

	uint64 				maxLoginDelay_;

    std::string         validateServer_;
    std::string         registerServer_;


	// Rate Limiting state

	// the time of the start of the last time block
	uint64 				lastRateLimitCheckTime_;
	// the number of logins left for this time block
	uint				numAllowedLoginsLeft_;
	// the number of logins allowed per time block
	int					loginRateLimit_;
	// the length of each time block for rate limiting
	uint64				rateLimitDuration_;

	static LoginApp * pInstance_;
        bool    useWGS_;
        WGSServerAuthentication *wgsAuth_;
        KeepWGSHeartbeat *wgsHearbeat_;
};

#endif // LOGINAPP_HPP
