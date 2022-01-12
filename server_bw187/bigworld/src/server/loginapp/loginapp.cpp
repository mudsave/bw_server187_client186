/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "loginapp.hpp"

#include <sys/types.h>

#ifndef _WIN32
#include <sys/signal.h>
#include <pwd.h>
#else //ifndef _WIN32
#include <signal.h>
#endif // ndef _WIN32

#include "login_int_interface.hpp"

#include "dbmgr/db_interface.hpp"

#include "resmgr/bwresource.hpp"

#include "server/bwconfig.hpp"
#include "server/util.hpp"
#include "server/writedb.hpp"

#include "network/msgtypes.hpp"	// for angleToInt8
#include "network/watcher_glue.hpp"
#include "network/encryption_filter.hpp"

#include "./webserver/soapGameAccountBindingProxy.h"
#include "./webserver/GameAccountBinding.nsmap"
#include "./webserver/soapAutoActivateSoap12Proxy.h"
DECLARE_DEBUG_COMPONENT( 0 )


static uint gNumLogins = 0;
static uint gNumLoginFailures = 0;
static uint gNumLoginAttempts = 0;

static char g_buildID[81] = "Build ID";
static std::string g_wgsheartbeat = "WGSHeartbeat";
#define TIME_OUT_TIME 2 //connect超时时间2秒

uint32 g_latestVersion = uint32(-1);
uint32 g_impendingVersion = uint32(-1);

LoginApp * LoginApp::pInstance_ = NULL;

// -----------------------------------------------------------------------------
// Section: Misc
// -----------------------------------------------------------------------------

extern "C" void interruptHandler( int )
{
	if (LoginApp::pInstance())
	{
		LoginApp::pInstance()->intNub().breakProcessing();
	}
}

extern "C" void controlledShutDownHandler( int )
{
	if (LoginApp::pInstance())
	{
		LoginApp::pInstance()->controlledShutDown();
	}
}

typedef struct WGS_SESSION_KEY_EX
{
    char header_;
    char filedNumbero_;
    char flag_;
    std::string id_;
    std::string serialKey_;
    std::string ip_;

    static int protocolToString(const std::string &id, const std::string &serialKey, 
            const std::string &ip, std::string &data)
    {
        WGS_SESSION_KEY_EX ns;
        char buf[50] = {};

        ns.header_ = '&';
        ns.filedNumbero_ = '5';
        ns.flag_ = 'D';

        if(id.size() > 12) return 1;

        if(serialKey.size() != 12) return 2;

        if(ip.size() > 15) return 3;

        ns.id_ = id;
        ns.serialKey_ = serialKey;
        ns.ip_ = ip;

        sprintf(buf, "%c;%c;%c;%s;%s;%s;", ns.header_, 
                ns.filedNumbero_, ns.flag_, 
                ns.id_.c_str(), ns.serialKey_.c_str(), ns.ip_.c_str());

        data = buf;

        return 0;
    }

    static int protocolFromString(WGS_SESSION_KEY_EX &ns, const std::string &data)
    {
        std::vector< std::string > spdata;
        std::string str;
        std::stringstream ss(data);

        while(getline(ss, str, ';'))
            spdata.push_back(str);

        if(spdata.size() != 5) return 2;

        ns.header_ = spdata[0][0];
        ns.filedNumbero_ = spdata[1][0];
        ns.flag_ = spdata[2][0];
        ns.id_ = spdata[3];
        ns.serialKey_ = spdata[4];
        ns.ip_ = spdata[5];

        return 0;
    }
};
typedef struct WGS_SESSION_KEY_RETURN
{
    char header_;
    char filedNumbero_;
    char flag_;
    char retCode_;
    std::string id_;
    std::string serialKey_;

    static int protocolFromString(WGS_SESSION_KEY_RETURN &ns, const std::string &data)
    {
        std::vector< std::string > spdata;
        std::string str;
        std::stringstream ss(data);

        while(getline(ss, str, ';'))
            spdata.push_back(str);

        if(spdata.size() < 6) return -2;

        ns.header_ = spdata[0][0];
        ns.filedNumbero_ = spdata[1][0];
        ns.flag_ = spdata[2][0];
        ns.retCode_ = spdata[3][0];
        ns.id_ = spdata[4];
        ns.serialKey_ = spdata[5];

        return 10 + ns.id_.size() + ns.serialKey_.size();
    }
};

KeepWGSHeartbeat::KeepWGSHeartbeat()
{
    int timeout = 30;
    timeout = BWConfig::get( "loginApp/WGSServerHeartbeat", timeout);
    timeout = timeout * 1000000 ;
    timerID_ = LoginApp::pInstance()->intNub().registerTimer(timeout , this);
}

KeepWGSHeartbeat::~KeepWGSHeartbeat()
{
    LoginApp::pInstance()->intNub().cancelTimer(timerID_);
}

int KeepWGSHeartbeat::handleTimeout( Mercury::TimerID id, void * arg )
{
	LogOnParamsPtr pParams = new LogOnParams();
    pParams->username(g_wgsheartbeat);
    pParams->password("1234567890AB");

    Mercury::Address source(0, 0);

    int ret = LoginApp::pInstance()->wgsAuth()->sendAuthentication( source, 0, pParams );

    switch(ret)
    {
        case WGSServerAuthentication::WGS_SERVER_NOT_CONNECT:
                INFO_MSG("101 wgs server not connect\n");
            break;

        case WGSServerAuthentication::WGS_INPUT_FORMAT_ERROR:
                INFO_MSG("104 data format error\n");
            break;

        default:
	        DEBUG_MSG( "Ok send KeepWGSHeartbeat....\n");
    }
    return 1;
}

WGSAuth::WGSAuth(const Mercury::Address & source, 
        const Mercury::ReplyID replyID, LogOnParamsPtr pParams )
:source_(source),replyID_(replyID),pParams_(pParams)
{
    timerID_ = LoginApp::pInstance()->intNub().registerTimer(Mercury::DEFAULT_REQUEST_TIMEOUT + 2000000 , this);
}
WGSAuth::~WGSAuth()
{
    INFO_MSG( "WGSAuth::~WGSAuth..\n");
    LoginApp::pInstance()->intNub().cancelTimer(timerID_);
}

int WGSAuth::handleTimeout( Mercury::TimerID id, void * arg )
{
    LoginApp::pInstance()->wgsAuth()->eraseWGSLogin(pParams_->username());
    if(pParams_->username() == g_wgsheartbeat)
    {
        INFO_MSG( "WGS server replay timeout (%s)\n", pParams_->username().c_str());
        LoginApp::pInstance()->wgsAuth()->reconnect();
    }
    else
    {
        LoginApp::pInstance()->sendFailure(source_, replyID_, LogOnStatus::LOGIN_CUSTOM_DEFINED_ERROR, 
                "WGS server replay timeout", pParams_);
    }

    delete this;

    return 0;
}

WGSServerAuthentication::WGSServerAuthentication(const std::string &addr, int port)
    :sock_(0),serPort_(port),recvBuffer_(""),serAddr_(addr)
{
    timerID_ = Mercury::TIMER_ID_NONE;

    sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    wgsService_.sin_family = AF_INET;
    wgsService_.sin_addr.s_addr = inet_addr(addr.c_str());
    wgsService_.sin_port = htons(port);

    buf_ = new char[4001];
}

WGSServerAuthentication::~WGSServerAuthentication()
{
    if(timerID_ != Mercury::TIMER_ID_NONE)
    {
        LoginApp::pInstance()->intNub().cancelTimer(timerID_);
        timerID_ = Mercury::TIMER_ID_NONE;
    }
    else
    {
        LoginApp::pInstance()->intNub().deregisterFileDescriptor(socket_);

        close(sock_);
    }

    std::map<std::string, WGSAuth *>::iterator itor = WGSAuthCachedMap_.begin();
    for(; itor != WGSAuthCachedMap_.end(); ++itor)
    {
        delete itor->second;
    }

    WGSAuthCachedMap_.clear();

    delete [] buf_;
}

int WGSServerAuthentication::handleReplyMsg(std::string &msg)
{      

    INFO_MSG( "WGSServerAuthentication::handleReplyMsg(%s)\n", msg.c_str());
    std::vector< std::string > spdata;
    int nend = 0;   
    int nbegin = 0;  

    while(nend != -1)   
    {   
        nend = (size_t)msg.find_first_of('&', nbegin + 1);   

        if(nend == -1)   
        {
            spdata.push_back(msg.substr(nbegin, msg.length() - nbegin));   
        }
        else  
        {
            std::string value = msg.substr(nbegin, nend - nbegin) ;
            spdata.push_back(msg.substr(nbegin, nend - nbegin));   
        }

        nbegin = nend;   
    }   

    msg = "";

    for(size_t i = 0; i < spdata.size(); i++)
    {
        if (spdata[i][0] != '&')
        {
            INFO_MSG("Error(%s)\n", spdata[i].c_str());
            continue;
        }
        else
        {
            WGS_SESSION_KEY_RETURN np;
            int r = WGS_SESSION_KEY_RETURN::protocolFromString(np,spdata[i]);
            if (r == -2)
            {
                if(i == spdata.size() - 1)
                    msg = spdata[i];

                continue;
            }
            else if(r < 0)
            {
                continue;
            }

            std::map<std::string, WGSAuth *>::iterator itor = WGSAuthCachedMap_.find(np.id_);

            if(itor != WGSAuthCachedMap_.end())
            {
                WGSAuth *authOk = itor->second;

                if(authOk->pParams()->username() == g_wgsheartbeat)
                {
                }
                else if(np.flag_ == 'K' && np.retCode_ == '0')
                {
                    /* 設置密碼是空，這樣可以在游戲中創建一個不需要密碼的帳號 */
                    authOk->pParams()->password("");

                    INFO_MSG("Ok WGS Authorization :%s\n", authOk->pParams()->username().c_str());

                    LoginApp::pInstance()->WGSAuthContinue(authOk->source(), authOk->replyID(), authOk->pParams());

                    /* 這里存儲新的serial key 如果成功要把它返回給客戶端*/
                    authOk->pParams()->password(np.serialKey_);

                }
                else if(np.flag_ == 'E' && np.retCode_ == '0')
                {

                    LoginApp::pInstance()->sendFailure(authOk->source(), authOk->replyID(), LogOnStatus::LOGIN_CUSTOM_DEFINED_ERROR, 
                            np.serialKey_.c_str(), authOk->pParams());

                }

                delete itor->second;

                WGSAuthCachedMap_.erase(itor);
            }
            else
            {
                INFO_MSG("Error not find login info (%s) .. wgs ok return\n", spdata[i].c_str());
            }

        }
    }

    return 0;
}
int WGSServerAuthentication::handleInputNotification(int fd)
{
    int bytesRead;

    memset(buf_, 0x00, 4001);
    bytesRead = recv(socket_, buf_, 4001, 0);
    if(bytesRead == 0)
    {
        INFO_MSG( "shutdown server....\n");
        this->reconnect();
        return 0;
    }

    if(bytesRead == -1)
    {
        INFO_MSG( "Error occurred....\n");
        this->reconnect();
        return 0;
    }

    recvBuffer_ = recvBuffer_ + buf_;

    INFO_MSG( "handleInputNotification:%s(%d)\n", buf_, bytesRead);

    handleReplyMsg(recvBuffer_);

    return 1;
}

int WGSServerAuthentication::reconnect()
{
    INFO_MSG( "reconnect....\n");
    if(timerID_ != Mercury::TIMER_ID_NONE)
    {
        INFO_MSG( "wait connect....\n");
        return WGSServerAuthentication::WGS_SERVER_NOT_CONNECT;
    }

    LoginApp::pInstance()->intNub().deregisterFileDescriptor(socket_);
    socket_.close();
    sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    this->connect();
    return 0;
}

int WGSServerAuthentication::handleTimeout( Mercury::TimerID id, void * arg )
{
    this->connect();
    return 0;
}

void WGSServerAuthentication::stopWGSAuthentication()
{
}

int WGSServerAuthentication::sendAuthentication(const Mercury::Address &source, 
        const Mercury::ReplyID replyID, LogOnParamsPtr pParams)
{
    std::string sendbuf = "";

    if(timerID_ != Mercury::TIMER_ID_NONE)
    {
        return WGSServerAuthentication::WGS_SERVER_NOT_CONNECT;
    }

    int r = WGS_SESSION_KEY_EX::protocolToString(pParams->username(), pParams->password(), source.ipAsString(), sendbuf);
    if(r != 0)
    {
        INFO_MSG( "Error WGS_SESSION_KEY_EX::protocolToString:%s:%s:%s(%d)\n", 
                pParams->username().c_str(), pParams->password().c_str(), source.c_str(), r);
        return WGSServerAuthentication::WGS_INPUT_FORMAT_ERROR;
    }

    WGSAuth *nAuth = new WGSAuth(source, replyID, pParams);
    WGSAuthCachedMap_.insert(std::map<std::string, WGSAuth *>::value_type(pParams->username(), nAuth));

    int bytesRead;

    bytesRead = ::send(socket_, sendbuf.c_str(), sendbuf.size(), 0);

    INFO_MSG( "sendAuthentication:%s(%d)\n", sendbuf.c_str(), bytesRead);

    return 0;
}

int WGSServerAuthentication::connect()
{
    INFO_MSG( "Connect WGS server(%s:%d)\n", serAddr_.c_str(), serPort_);
    struct timeval tm;
    fd_set set;
    int error=-1, len;
    len = sizeof(int);

    unsigned long ul = 1;
    ioctl(sock_, FIONBIO, &ul); //设置为非阻塞模式
    bool ret = false;

    if(::connect(sock_, (const struct sockaddr *)&wgsService_, sizeof(wgsService_)) == -1)
    {
        tm.tv_sec  = TIME_OUT_TIME;
        tm.tv_usec = 0;
        FD_ZERO(&set);
        FD_SET(sock_, &set);
        if(select(sock_ + 1, NULL, &set, NULL, &tm) > 0)
        {
            getsockopt(sock_, SOL_SOCKET, SO_ERROR, &error, (socklen_t *)&len);
            if(error == 0) ret = true;
            else ret = false;
        } 
        else ret = false;
    }
    else ret = true;

    if(!ret) 
    {
        if(timerID_ == Mercury::TIMER_ID_NONE)
            timerID_ = LoginApp::pInstance()->intNub().registerTimer(50000000, this);

        INFO_MSG( "Error connect WGS server(%s:%d)......%d\n", serAddr_.c_str(), serPort_, errno);

        return 0;
    }
    else
    {
        INFO_MSG( "Ok connect WGS server......\n");
    }

    if(timerID_ != Mercury::TIMER_ID_NONE)
    {
        LoginApp::pInstance()->intNub().cancelTimer(timerID_);
        timerID_ = Mercury::TIMER_ID_NONE;
    }

    socket_.setFileDescriptor(sock_);
    socket_.setnonblocking(true);
    LoginApp::pInstance()->intNub().registerFileDescriptor(socket_, this);

    return 1;
}

int WGSServerAuthentication::eraseWGSLogin(const std::string &id)
{
    std::map<std::string, WGSAuth *>::iterator itor = WGSAuthCachedMap_.find(id);
    if(itor != WGSAuthCachedMap_.end())
    {
        WGSAuthCachedMap_.erase(itor);
    }
    return 0;
}

// -----------------------------------------------------------------------------
// Section: DatabaseReplyHandler
// -----------------------------------------------------------------------------

/**
 *	An instance of this class is used to receive the reply from a call to
 *	the database.
 */
class DatabaseReplyHandler : public Mercury::ReplyMessageHandler
{
public:
	DatabaseReplyHandler(
		const Mercury::Address & clientAddr,
		const Mercury::ReplyID replyID,
		LogOnParamsPtr pParams );

	virtual ~DatabaseReplyHandler() {}

	virtual void handleMessage( const Mercury::Address & source,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data,
		void * arg );

	virtual void handleException( const Mercury::NubException & ne,
		void * arg );

	LogOnParamsPtr pParams() { return pParams_; }
	Mercury::Address	clientAddr() { return clientAddr_; };
	Mercury::ReplyID	replyID() { return replyID_; };
private:
	Mercury::Address	clientAddr_;
	Mercury::ReplyID	replyID_;
	LogOnParamsPtr		pParams_;
};


// -----------------------------------------------------------------------------
// Section: LoginApp
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
LoginApp::LoginApp( uint16 loginPort ) :
	privateKey_( /* hasPrivate: */ true ),
	intNub_( 0, BW_INTERNAL_INTERFACE( loginApp ) ),
	extNub_( htons( loginPort ),
			BWConfig::get( "loginApp/externalInterface",
				BWConfig::get( "externalInterface" ) ).c_str() ),
	netMask_(),
	externalIP_( 0 ),
	isControlledShutDown_( false ),
	systemOverloaded_( 0 ),
	allowLogin_( true ),
	allowProbe_( true ),
	logProbes_( false ),
	autoRegister_( false ),
	registerTimeout_( 10 ),
    workerThreadMgr_( intNub_ ),
    threadPool_( workerThreadMgr_, BWConfig::get( "loginApp/registerThreadNumber", 2)),
    validateServer_(""),
    registerServer_(""),
	lastRateLimitCheckTime_( 0 ),
	numAllowedLoginsLeft_( 0 ),
	loginRateLimit_( 0 ),
    rateLimitDuration_( 0 ),
    useWGS_ ( false )
{
	pInstance_ = this;

	extNub_.isVerbose( BWConfig::get( "loginApp/verboseExternalNub", false ) );

	double maxLoginDelay = BWConfig::get( "loginApp/maxLoginDelay", 10.f );
	maxLoginDelay_ = uint64( maxLoginDelay * ::stampsPerSecondD() );

	// these must match those of the client
	extNub_.onceOffResendPeriod( CLIENT_ONCEOFF_RESEND_PERIOD );
	extNub_.onceOffMaxResends( CLIENT_ONCEOFF_MAX_RESENDS );

	// Mark extNub as 'external' so that once-off-reliability is disabled.
	extNub_.isExternal( true );
}


/**
 *	This method initialises this object.
 */
bool LoginApp::init( int argc, char * argv[], uint16 loginPort )
{

	// Check if specified port on which the LoginApp should listen  
	// is already used.  If it is and if the LoginApp configuration option 
	// shouldShutDownIfPortUsed is set, then stop this LoginApp.

	DataSectionPtr pLoginAppConfig = BWConfig::getSection( "loginApp" );
	std::vector<int> externalPorts;
	if (pLoginAppConfig)
		pLoginAppConfig->readInts( "externalPort", externalPorts );
	std::string extInterface = BWConfig::get( "loginApp/externalInterface", BWConfig::get( "externalInterface" ) );
	for (uint i = 0; i < externalPorts.size() && extNub_.socket() == -1; i++) 
	{ 
		uint16 port = externalPorts[i]; 
		INFO_MSG( "LoginApp::LoginApp: Attempting to re-bind external interface to %s:%d\n", extInterface.c_str(), port ); 
		if (extNub_.recreateListeningSocket( htons( port ), extInterface.c_str() )) 
			break; 
	} 

	if (extNub_.socket() == -1)
	{
		ERROR_MSG( "Loginapp::init: "
			"Unable to bind to external interface on specified port %d.\n",
			loginPort );
		return false;
	}

	std::string privateKeyPath = BWConfig::get( "loginApp/privateKey",
		"server/loginapp.privkey" );

	if (!privateKeyPath.empty())
	{
		if (!LoginApp::setKeyFromResource( privateKeyPath ))
		{
			return false;
		}
	}
	else
	{
		ERROR_MSG( "LoginApp::init: "
			"You must specify a private key to use with the "
			"<loginApp/privateKey> option in bw.xml\n" );

		return false;
	}

	if (intNub_.socket() == -1)
	{
		ERROR_MSG( "Failed to create Nub on internal interface.\n" );
		return false;
	}

	if ((extNub_.address().ip == 0) ||
			(intNub_.address().ip == 0))
	{
		ERROR_MSG( "LoginApp::init: Failed to open UDP ports. "
				"Maybe another process already owns it\n" );
		return false;
	}

	BW_INIT_WATCHER_DOC( "loginapp" );

	BWConfig::update( "loginApp/shutDownSystemOnExit", isControlledShutDown_ );
	MF_WATCH( "shutDownSystemOnExit", isControlledShutDown_ );

	std::string netMask = BWConfig::get( "loginApp/localNetMask" );
	netMask_.parse( netMask.c_str() );

	// Should use inet_aton but this does not work under Windows.
	std::string extAddr = BWConfig::get( "loginApp/externalAddress" );
	externalIP_ = inet_addr( extAddr.c_str() );

	if (netMask_.containsAddress( intNub_.address().ip ))
	{
		INFO_MSG( "Local subnet: %s\n", netMask.c_str() );
		INFO_MSG( "External addr: %s\n", extAddr.c_str() );
	}
	else
	{
		WARNING_MSG( "LoginApp::LoginApp: "
					"localNetMask %s does not match local ip %s\n",
				netMask.c_str(), intNub_.address().c_str() );
		INFO_MSG( "Not using localNetMask\n" );

		netMask_.clear();
	}

	MF_WATCH( "Num logins", gNumLogins );
	MF_WATCH( "Num failed logins", gNumLoginFailures );
	MF_WATCH( "Num login attempts", gNumLoginAttempts );

	// ---- What used to be in loginsvr.cpp

	ReviverSubject::instance().init( &intNub_, "loginApp" );

	// make sure the nub came up ok
	if (extNub_.address().ip == 0)
	{
		CRITICAL_MSG( "login::init: Failed to start Nub.\n"
			"\tLog in port is probably in use.\n"
			"\tIs there another login server running on this machine?\n" );
		return false;
	}

	INFO_MSG( "External address = %s\n", extNub_.address().c_str() );
	INFO_MSG( "Internal address = %s\n", intNub_.address().c_str() );

	if (BW_INIT_ANONYMOUS_CHANNEL_CLIENT( dbMgr_, intNub_,
			LoginIntInterface, DBInterface, 0 ))
	{
		INFO_MSG( "LoginApp::init: DB addr: %s\n",
			this->dbMgr().channel().c_str() );
	}
	else
	{
		INFO_MSG( "LoginApp::init: Database not ready yet.\n" );
	}

	LoginInterface::registerWithNub( extNub_ );
	LoginIntInterface::registerWithNub( intNub_ );

	// Decide whether or not we're allowing logins and/or probes
	allowLogin_ = BWConfig::get( "loginApp/allowLogin", allowLogin_ );
	allowProbe_ = BWConfig::get( "loginApp/allowProbe", allowProbe_ );
	logProbes_ = BWConfig::get( "loginApp/logProbes", logProbes_ );
	autoRegister_ = BWConfig::get( "loginApp/autoRegister", autoRegister_ );
	registerTimeout_ = BWConfig::get( "loginApp/registerTimeout", registerTimeout_ );
    validateServer_ = BWConfig::get( "loginApp/validateServer", "http://transfer.gyyx.cn:81" );
    registerServer_ = BWConfig::get( "loginApp/registerServer", "http://172.16.1.13:8080" );

	MF_WATCH( "allowLogin", allowLogin_ );
	MF_WATCH( "allowProbe", allowProbe_ );
	MF_WATCH( "logProbes", logProbes_ );
	MF_WATCH( "autoRegister", autoRegister_);
	MF_WATCH( "registerTimeout", registerTimeout_);

    if(autoRegister_)
    {
		NOTICE_MSG( "LoginApp::init: bw.xml/loginApp/autoRegister is enabled.\n" );
    }

	if (allowProbe_)
	{
		NOTICE_MSG( "LoginApp::init: bw.xml/loginApp/allowProbe is enabled. "
			"This is a development-time feature only and should be disabled "
			"during production.\n" );
	}

	// Set up the rate limiting parameters
	uint32 rateLimitSeconds = BWConfig::get( "loginApp/rateLimitDuration",
		uint32( 0 ) );
	rateLimitDuration_ = rateLimitSeconds * stampsPerSecond();
	BWConfig::update( "loginApp/loginRateLimit", loginRateLimit_ );

	if (rateLimitSeconds)
	{
		INFO_MSG( "LoginApp::init: "
				"Login rate limiting enabled: period = %lu, limit = %d\n",
			rateLimitSeconds, loginRateLimit_ );
	}

	numAllowedLoginsLeft_ = loginRateLimit_;
	lastRateLimitCheckTime_ = timestamp();
	MF_WATCH( "rateLimit/duration", *this,
			&LoginApp::rateLimitSeconds, &LoginApp::rateLimitSeconds );
	MF_WATCH( "rateLimit/loginLimit", loginRateLimit_ );

	Mercury::Reason reason =
		LoginIntInterface::registerWithMachined( intNub_, 0 );

	if (reason != Mercury::REASON_SUCCESS)
	{
		ERROR_MSG( "LoginApp::init: Unable to register with nub. "
			"Is machined running?\n");
		return false;
	}

	LoginInterface::registerWithMachined( extNub_, 0 );

	// Handle Ctrl+C
	signal( SIGINT, interruptHandler );
#ifndef _WIN32
	// TODO: Controlled shutdown cannot be started under Windows.
	signal( SIGUSR1, controlledShutDownHandler );
#endif

	// Make the external nub a slave to the internal one.
	intNub_.registerChildNub( &extNub_ );
    useWGS_ = BWConfig::get( "loginApp/useWGSServer", useWGS_);
    if(useWGS_)
    {
        wgsAuth_ = new WGSServerAuthentication(BWConfig::get( "loginApp/WGSServerAddress", "127.0.0.1"),
                BWConfig::get( "loginApp/WGSServerPort", 0));
        wgsAuth_->connect();
        wgsHearbeat_ = new KeepWGSHeartbeat();
    }
	// Start up watchers
	BW_REGISTER_WATCHER( 0, "loginapp", "LoginApp", "loginApp", intNub_ );

	Watcher & root = Watcher::rootWatcher();
	root.addChild( "nub", Mercury::Nub::pWatcher(), &intNub_ );
	root.addChild( "nubExternal", Mercury::Nub::pWatcher(), &extNub_ );

	// root.addChild( "dbMgr", Mercury::Channel::pWatcher(),
	//		&this->dbMgr().channel() );

	return true;
}


/**
 *	This method performs the main loop of this application.
 */
void LoginApp::run()
{
	intNub_.processUntilBreak();

	INFO_MSG( "LoginApp::run: Terminating normally.\n" );

	INFO_MSG( "isDBReady = %d.  isControlledShutDown_ = %d\n",
			this->isDBReady(), isControlledShutDown_ );

	if (this->isDBReady() && isControlledShutDown_)
	{
		Mercury::Bundle	& dbBundle = this->dbMgr().bundle();
		DBInterface::controlledShutDownArgs args;
		args.stage = SHUTDOWN_REQUEST;
		dbBundle << args;
		this->dbMgr().send();

		intNub_.processUntilChannelsEmpty();
	}

    if(useWGS_ && wgsAuth_ != NULL)
    {
        delete wgsAuth_;
        delete wgsHearbeat_;
    }
}


/**
 *	This method sends a failure message back to the client.
 */
void LoginApp::sendFailure( const Mercury::Address & addr,
	Mercury::ReplyID replyID, int status, const char * pDescription,
	LogOnParamsPtr pParams )
{
	if (pDescription == NULL)
		pDescription = "";

	INFO_MSG( "LoginApp::sendFailure: "
		"LogOn for %s failed, LogOnStatus %d, description '%s'.\n",
			addr.c_str(), status, pDescription );

	++gNumLoginFailures;

	Mercury::Bundle bundle;

	// Replies to failed login attempts are not reliable as that would be a
	// DOS'ing vulnerability
	bundle.startReply( replyID, Mercury::RELIABLE_NO );
	bundle << (int8)status;
	bundle << pDescription;

	LoginApp & app = LoginApp::instance();
	app.extNub_.send( addr, bundle );

	if (*pDescription == 0)
	{
		WARNING_MSG( "LoginApp::sendFailure: "
			"Sent LogOnStatus %d without a description (bad form)", status );
	}

	// Erase the cache mapping for this attempt if appropriate
	if (pParams)
	{
		app.cachedLoginMap_.erase( addr );
	}
}



/*
 * 请求 web server 注册模块
 */
class RegisterTask : public WorkerThread::ITask
{
public:
    RegisterTask(
		const Mercury::Address & clientAddr,
		const Mercury::ReplyID replyID,
		LogOnParamsPtr pParams ):
	clientAddr_( clientAddr ),
	replyID_( replyID ),
	pParams_( pParams )
    {
    }

    virtual ~RegisterTask()
    {
    }

    virtual void run()
    {
        AutoActivateSoap12Proxy RegisterProxy;
        std::string registerResult;
        struct _ns1__RegisterResponse result;
        struct _ns1__Register ns1__Register;

        LoginApp & app = LoginApp::instance();

	    uint8 status = LogOnStatus::LOGIN_REJECTED_NO_SUCH_USER;

        Mercury::Bundle bundle;

        RegisterProxy.connect_timeout = app.registerTimeout(); 
        
        ns1__Register.serverIP = (char *)app.extNub().address().ipAsString();
        ns1__Register.userName = (char *)pParams_->username().c_str(); 
        ns1__Register.password = (char *)pParams_->password().c_str();
        ns1__Register.clientIp = (char *)clientAddr_.ipAsString();

        RegisterProxy.serverAdd_ = app.validateServer();

        if (RegisterProxy.Register(&ns1__Register, &result) != SOAP_OK)
        {
            RegisterProxy.soap_stream_fault(std::cerr);
            ERROR_MSG( "Exec validate error:(%s)\n", app.validateServer().c_str() );
        }
        else
        {
            switch(result.RegisterResult->ErrorCode)
            {
                case ns1__AutoActResultCode__UnknownError:
                    registerResult = "ns1__AutoActResultCode__UnknownError";
                    break;
                case ns1__AutoActResultCode__Succeed:
                    registerResult = "ns1__AutoActResultCode__Succeed";
                    status = LogOnStatus::LOGIN_RELOGIN;
                    break;
                case ns1__AutoActResultCode__ParameterIsNull:
                    registerResult = "ns1__AutoActResultCode__ParameterIsNull";
                    break;
                case ns1__AutoActResultCode__UsernameOrPasswordNoSuited:
                    registerResult = "ns1__AutoActResultCode__UsernameOrPasswordNoSuited";
                    break;
                case ns1__AutoActResultCode__GameInterfaceError:
                    registerResult = "ns1__AutoActResultCode__GameInterfaceError";
                    break;
                case ns1__AutoActResultCode__GameInterfaceException:
                    registerResult = "ns1__AutoActResultCode__GameInterfaceException";
                    break;
                case ns1__AutoActResultCode__ServerNoActivate:
                    registerResult = "ns1__AutoActResultCode__ServerNoActivate";
                    break;
                default:
                    registerResult = "UnknownError";
                    break;
            }
        }

        if( status == LogOnStatus::LOGIN_RELOGIN )
        {
            GameAccountBindingProxy RegisterAccountProxy;
            struct ns2__RegisterResponse resultAccount;

            std::string info = "account=" + pParams_->username() ;
            info = info + " password=" ;
            info = info + pParams_->password() ;
            info = info + " IP=" ;
            info = info + clientAddr_.ipAsString();
            if( result.RegisterResult != NULL && result.RegisterResult->IsAdult)
                info = info + " adult=1" ;
            else
                info = info + " adult=0" ;

            if( result.RegisterResult != NULL && result.RegisterResult->IsPresentee)
                info = info + " presentee=1" ;
            else
                info = info + " presentee=0" ;

            INFO_MSG( "Register string:(%s)\n", info.c_str() );

            RegisterAccountProxy.serverAdd_ = app.registerServer();
            RegisterAccountProxy.connect_timeout = app.registerTimeout(); 

            if( RegisterAccountProxy.Register( (char *)info.c_str(), resultAccount ) == SOAP_OK )
            {
                /* ErrorCode:
                 * 0:成功
                 * 1:失败
                 * 101:帐号不存在
                 * 102:帐号已经存在
                 * 103:资料不齐全(帐号和密码不能为空)
                 * 201:验证码错误
                 * 200:订单已经成功执行
                 */
                if( resultAccount._return_->ErrorCode == 0 || resultAccount._return_->ErrorCode == 102 )
                {
                    INFO_MSG( "Exec Register ok (%s)\n",(char *)pParams_->username().c_str() );
                }
                else
                {
                    INFO_MSG( "Exec Register error:(%d)\n",resultAccount._return_->ErrorCode );
                    status = LogOnStatus::LOGIN_REJECTED_NO_SUCH_USER;
                }
            }
            else
            {
                INFO_MSG( "Exec Register except:(%s)\n", app.registerServer().c_str() );
                status = LogOnStatus::LOGIN_REJECTED_NO_SUCH_USER;
            }
        }


        INFO_MSG( "LoginApp  Register %s "
                   "(serverIP:%s userName:%s clientIp:%s )\n",
                   status == LogOnStatus::LOGIN_RELOGIN ? "OK" : "ERROR",
                   app.extNub().address().ipAsString(),
                   pParams_->username().c_str(),
                   clientAddr_.ipAsString());

        bundle.startReply( replyID_, Mercury::RELIABLE_NO );
        bundle << (int8)status;
        bundle << registerResult.c_str();

        app.extNub().send( clientAddr_, bundle );
    }

    virtual void onRunComplete()
    {
	    LoginApp & app = LoginApp::instance();
	    app.removeCachedLoginMap(clientAddr_);
        delete this;
    }

private:
	Mercury::Address	clientAddr_;
	Mercury::ReplyID	replyID_;
	LogOnParamsPtr		pParams_;
};

/*
 * 激活帐号接口
 */
void LoginApp::webServicesRegister( const Mercury::Address & source,
		                            Mercury::UnpackedMessageHeader & header,
		                            BinaryIStream & data )
{

    mapRegistLock_.grab();
    bool isReattempt = (cachedRegistMap_.count( source ) != 0);
    mapRegistLock_.give();

    if( isReattempt )
    {
		data.finish();
		return;
    }

	uint32 version = 0;
	data >> version;

	LogOnParamsPtr pParams = new LogOnParams();

	if (!pParams->readFromStream( data, &privateKey_ ))
	{
		this->sendFailure( source,
                           header.replyID,
			               LogOnStatus::LOGIN_MALFORMED_REQUEST,
			               "Could not destream register parameters" );

		data.finish();
		return;
	}


    RegisterTask* pTask = 
		new RegisterTask( source, header.replyID, pParams );
    bool isok = threadPool_.doTask( *pTask );

    if(isok)
    {
	    INFO_MSG( "LoginApp::webServicesRegister:{%s} (%s)\n",
		        pParams->username().c_str(),
		        source.c_str() );

        mapRegistLock_.grab();
	    cachedRegistMap_[ source ] = timestamp();
        mapRegistLock_.give();
    }
    else
    {
	    INFO_MSG( "LoginApp::webServicesRegister is busy: {%s} (%s)\n",
		        pParams->username().c_str(),
		        source.c_str() );

		this->sendFailure( source,
                           header.replyID,
			               LogOnStatus::LOGIN_WEBSERVER_BUSY,
			               "register server is busy" );
    }
    
}

/**
 *	This method is the one that actually receives the login requests.
 */
void LoginApp::login( const Mercury::Address & source,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data )
{

	if (rateLimitDuration_ &&
			(timestamp() > lastRateLimitCheckTime_ + rateLimitDuration_) )
	{
		// reset number of allowed logins per time block if we're rate limiting
		numAllowedLoginsLeft_ = loginRateLimit_;
		lastRateLimitCheckTime_ = timestamp();
	}

	if (!allowLogin_)
	{
		WARNING_MSG( "LoginApp::login: "
			"Dropping login attempt from %s as logins aren't allowed yet\n",
			source.c_str() );

		this->sendFailure( source, header.replyID,
			LogOnStatus::LOGIN_REJECTED_LOGINS_NOT_ALLOWED,
			"Logins currently not permitted" );
		data.finish();
		return;
	}

	bool isReattempt = (cachedLoginMap_.count( source ) != 0);
 	INFO_MSG( "LoginApp::login: %s from %s\n",
		isReattempt ? "Re-attempt" : "Attempt", source.c_str() );

	++gNumLoginAttempts;

	uint32 version = 0;
	data >> version;

	if (data.error())
	{
		ERROR_MSG( "LoginApp::login: "
			"Not enough data on stream (%d bytes total)\n",
			header.length );

		this->sendFailure( source, header.replyID,
			LogOnStatus::LOGIN_MALFORMED_REQUEST,
			"Undersized login message");

		return;
	}

	if ((version < OLDEST_SUPPORTED_LOGIN_VERSION) || (version > LOGIN_VERSION))
	{
		ERROR_MSG( "LoginApp::login: "
			"User at %s tried to log on with version %d. Expected between %d "
			"and %d\n",
			source.c_str(), (int)version, OLDEST_SUPPORTED_LOGIN_VERSION, 
			LOGIN_VERSION );

		char msg[BUFSIZ];

		sprintf( msg, "Incorrect protocol version. "
				"Client version is %d, server supports version %d to %d. "
				"Your %s is out of date.", (int)version, 
				(int)OLDEST_SUPPORTED_LOGIN_VERSION, (int)LOGIN_VERSION,
			  	version < LOGIN_VERSION ? "client" : "server" );

		this->sendFailure( source, header.replyID,
			LogOnStatus::LOGIN_BAD_PROTOCOL_VERSION, msg );

		data.finish();

		return;
	}

	bool isRateLimited = (rateLimitDuration_ && numAllowedLoginsLeft_ == 0);
	if (isRateLimited)
	{
		NOTICE_MSG( "LoginApp::login: "
				"Login from %s not allowed due to rate limiting\n",
				source.c_str() );

		// We used to return LOGIN_REJECTED_LOGINS_NOT_ALLOWED before
		// LOGIN_VERSION_RATE_LIMIT_STATUS.
		LogOnStatus status = (version < LOGIN_VERSION_RATE_LIMIT_STATUS) ?
				LogOnStatus::LOGIN_REJECTED_LOGINS_NOT_ALLOWED :
				LogOnStatus::LOGIN_REJECTED_RATE_LIMITED;
		this->sendFailure( source, header.replyID, status,
				"Logins temporarily disallowed due to rate limiting" );
		data.finish();
		return;
	}

	if (!this->isDBReady())
	{
		INFO_MSG( "LoginApp::login: "
			"Attempted login when database not yet ready.\n" );

		this->sendFailure( source, header.replyID,
			LogOnStatus::LOGIN_REJECTED_DB_NOT_READY, "DB not ready" );

		return;
	}

	if (systemOverloaded_ != 0)
	{
		if (systemOverloadedTime_ + stampsPerSecond() < timestamp())
		{
			systemOverloaded_ = 0;
		}
		else
		{
			INFO_MSG( "LoginApp::login: "
				"Attempted login when system overloaded or not yet ready.\n" );
			this->sendFailure( source, header.replyID,
				systemOverloaded_, "System overloaded wait state." );
			return;
		}
	}

	// First check whether this is a repeat attempt from a recent pending
	// login before attempting to decrypt and log in.
	if (this->handleResentPendingAttempt( source, header.replyID ))
	{
		// ignore this one, it's in progress
		data.finish();
		return;
	}

	// Read off login parameters
	LogOnParamsPtr pParams = new LogOnParams();

	if (!pParams->readFromStream( data, &privateKey_ ))
	{
		this->sendFailure( source, header.replyID,
			LogOnStatus::LOGIN_MALFORMED_REQUEST,
			"Could not destream login parameters" );

		return;
	}

	// First check whether this is a repeat attempt from a recent
	// resolved login before attempting to log in.
	if (this->handleResentCachedAttempt( source, pParams, header.replyID ))
	{
		// ignore this one, we've seen it recently
		return;
	}

	if (rateLimitDuration_)
	{
		// We've done the hard work of decrypting the logon parameters now, so
		// we count this as a login with regards to rate-limiting.
		--numAllowedLoginsLeft_;
	}


	INFO_MSG( "Logging in %s{%s} (%s)\n",
		pParams->username().c_str(),
		pParams->password().c_str(),
		source.c_str() );

	// Remember that this attempt is now in progress and discard further
	// attempts from that address for some time after it completes.
	cachedLoginMap_[ source ].reset();
	cachedLoginMap_[ source ].pParams( pParams );

    if(useWGS_)
    {
        int ret = wgsAuth_->sendAuthentication( source, header.replyID, pParams );
        switch(ret)
        {
            case WGSServerAuthentication::WGS_SERVER_NOT_CONNECT:
                this->sendFailure(source, header.replyID, LogOnStatus::LOGIN_CUSTOM_DEFINED_ERROR, 
                    "101 wgs server not connect", pParams);
                break;

            case WGSServerAuthentication::WGS_INPUT_FORMAT_ERROR:
                this->sendFailure(source, header.replyID, LogOnStatus::LOGIN_CUSTOM_DEFINED_ERROR, 
                    "104 data format error", pParams);
                break;

            default:
	            DEBUG_MSG( "Ok sendAuthentication....\n");
        }
        return ;
    }

	DatabaseReplyHandler * pDBHandler =
		new DatabaseReplyHandler( source, header.replyID, pParams );

	Mercury::Bundle	& dbBundle = this->dbMgr().bundle();
	dbBundle.startRequest( DBInterface::logOn, pDBHandler, NULL,
		Mercury::DEFAULT_REQUEST_TIMEOUT + 2000000 ); // 2 seconds extra

	dbBundle << source << *pParams;

	this->dbMgr().send();
}


// -----------------------------------------------------------------------------
// Section: DatabaseReplyHandler
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
DatabaseReplyHandler::DatabaseReplyHandler(
		const Mercury::Address & clientAddr,
		Mercury::ReplyID replyID,
		LogOnParamsPtr pParams ) :
	clientAddr_( clientAddr ),
	replyID_( replyID ),
	pParams_( pParams )
{
	DEBUG_MSG( "DBReplyHandler created at %f\n",
		float(double(timestamp())/stampsPerSecondD()) );
}


/**
 *	This method is called by the nub when a message comes back from the system.
 *	It deletes itself at the end.
 */
void DatabaseReplyHandler::handleMessage(
	const Mercury::Address & /*source*/,
	Mercury::UnpackedMessageHeader & header,
	BinaryIStream & data,
	void * /*arg*/ )
{
	uint8 status;
	data >> status;

	if (status != LogOnStatus::LOGGED_ON)
	{
		LoginApp & app = LoginApp::instance();

		if (status == LogOnStatus::LOGIN_REJECTED_NO_SUCH_USER && app.autoRegister())
		{
			DEBUG_MSG( "DatabaseReplyHandler::handleMessage(%s): "
					"start regist thread (status=%x)\n",
				clientAddr_.c_str(), status );

			LoginApp::sendFailure( clientAddr_, 
                                   replyID_, 
                                   LogOnStatus::LOGIN_NEED_WEBSERVER_REGISTER, 
                                   "LOGIN_NEED_WEBSERVER_REGISTER",
				                   pParams_ );

		}
        else if (data.remainingLength() > 0)
		{
			std::string msg;
			data >> msg;
			LoginApp::sendFailure( clientAddr_, replyID_, status, msg.c_str(),
				pParams_ );
		}
		else
		{
			LoginApp::sendFailure( clientAddr_, replyID_, status,
				"Database returned an unelaborated error. Check DBMgr log.",
				pParams_ );
		}

		if (app.systemOverloaded() == 0 &&
				status == LogOnStatus::LOGIN_REJECTED_BASEAPP_OVERLOAD ||
				status == LogOnStatus::LOGIN_REJECTED_CELLAPP_OVERLOAD ||
				status == LogOnStatus::LOGIN_REJECTED_DBMGR_OVERLOAD)
		{
			DEBUG_MSG( "DatabaseReplyHandler::handleMessage(%s): "
					"failure due to overload (status=%x)\n",
				clientAddr_.c_str(), status );
			app.systemOverloaded( status );
		}
		delete this;
		return;
	}

	if (data.remainingLength() < int(sizeof( LoginReplyRecord )))
	{
		ERROR_MSG( "DatabaseReplyHandler::handleMessage: "
						"Login failed. Expected %d bytes got %d\n",
				sizeof( LoginReplyRecord ), data.remainingLength() );

		if (data.remainingLength() == sizeof(LoginReplyRecord) - sizeof(int))
		{
			ERROR_MSG( "DatabaseReplyHandler::handleMessage: "
					"This can occur if a login is attempted to an entity type "
					"that is not a Proxy.\n" );

			LoginApp::sendFailure( clientAddr_, replyID_,
				LogOnStatus::LOGIN_CUSTOM_DEFINED_ERROR,
				"Database returned a non-proxy entity type.",
				pParams_ );
		}
		else
		{
			LoginApp::sendFailure( clientAddr_, replyID_,
				LogOnStatus::LOGIN_REJECTED_DB_GENERAL_FAILURE,
				"Database returned an unknown error.",
				pParams_ );
		}

		delete this;
		return;
	}

	LoginReplyRecord lrr;
	data >> lrr;

	// If the client has an external address, send them the firewall
	// address instead!

	if (!LoginApp::instance().netMask().containsAddress( clientAddr_.ip ))
	{
		INFO_MSG( "DatabaseReplyHandler::handleMessage: "
				"Redirecting external client %s to firewall.\n",
			clientAddr_.c_str() );
		lrr.serverAddr.ip = LoginApp::instance().externalIP();
	}

	LoginApp::instance().sendAndCacheSuccess( clientAddr_,
			replyID_, lrr, pParams_ );

	gNumLogins++;

	delete this;
}


/**
 *	This method is called by the nub when no message comes back from the system,
 *	or some other error occurs. It deletes itself at the end.
 */
void DatabaseReplyHandler::handleException(
	const Mercury::NubException & ne,
	void * /*arg*/ )
{
	LoginApp::sendFailure( clientAddr_, replyID_,
		LogOnStatus::LOGIN_REJECTED_DBMGR_OVERLOAD, "No reply from DBMgr.",
		pParams_ );

	WARNING_MSG( "DatabaseReplyHandler: got an exception (%s)\n",
			Mercury::reasonToString( ne.reason() ) );
	DEBUG_MSG( "DBReplyHandler timed out at %f\n",
			float(double(timestamp())/stampsPerSecondD()) );

	delete this;
}


// -----------------------------------------------------------------------------
// Section: ProbeHandler
// -----------------------------------------------------------------------------

/**
 *	This method handles the probe message.
 */
void LoginApp::probe( const Mercury::Address & source,
	Mercury::UnpackedMessageHeader & header,
	BinaryIStream & /*data*/ )
{
	if (logProbes_)
	{
		INFO_MSG( "LoginApp::probe: Got probe from %s\n", source.c_str() );
	}

	if (!allowProbe_ || header.length != 0) return;

	Mercury::Bundle bundle;
	bundle.startReply( header.replyID );

	char buf[256];
	gethostname( buf, sizeof(buf) ); buf[sizeof(buf)-1]=0;
	bundle << PROBE_KEY_HOST_NAME << buf;

#ifndef _WIN32
	struct passwd *pwent = getpwuid( getUserId() );
	const char * username = pwent ? (pwent->pw_name ? pwent->pw_name : "") : "";
	bundle << PROBE_KEY_OWNER_NAME << username;

	if (!pwent)
	{
		ERROR_MSG( "LoginApp::probe: "
			"Process uid %d doesn't exist on this system!\n", getUserId() );
	}

#else
	DWORD bsz = sizeof(buf);
	if (!GetUserName( buf, &bsz )) buf[0]=0;
	bundle << PROBE_KEY_OWNER_NAME << buf;
#endif

	sprintf( buf, "%d", gNumLogins );
	bundle << PROBE_KEY_USERS_COUNT << buf;

	bundle << PROBE_KEY_UNIVERSE_NAME << BWConfig::get( "universe" );
	bundle << PROBE_KEY_SPACE_NAME << BWConfig::get( "space" );

	bundle << PROBE_KEY_BINARY_ID << g_buildID;

	sprintf( buf, "%lu", g_latestVersion );
	bundle << PROBE_KEY_LATEST_VERSION << buf;
	sprintf( buf, "%lu", g_impendingVersion );
	bundle << PROBE_KEY_IMPENDING_VERSION << buf;

	extNub_.send( source, bundle );
}


/**
 *	This class is used to handle messages from this process Mercury interfaces.
 */
class LoginAppRawMessageHandler : public Mercury::InputMessageHandler
{
	public:
		typedef void (LoginApp::*Handler)(
			const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & stream );

		// Constructors
		LoginAppRawMessageHandler( Handler handler ) :
			handler_( handler )
		{}

	private:
		virtual void handleMessage( const Mercury::Address & srcAddr,
				Mercury::UnpackedMessageHeader & header,
				BinaryIStream & data )
		{
			(LoginApp::instance().*handler_)( srcAddr, header, data );
		}

		Handler handler_;
};

LoginAppRawMessageHandler gLoginHandler( &LoginApp::login );
LoginAppRawMessageHandler gwebServicesRegisterHandler( &LoginApp::webServicesRegister );
LoginAppRawMessageHandler gProbeHandler( &LoginApp::probe );
LoginAppRawMessageHandler gShutDownHandler( &LoginApp::controlledShutDown );


/**
 *	This method sends a reply to a client indicating that logging in has been
 *	successful. It also caches this information so that it can be resent if
 *	necessary.
 */
void LoginApp::sendAndCacheSuccess( const Mercury::Address & addr,
		Mercury::ReplyID replyID, const LoginReplyRecord & replyRecord,
		LogOnParamsPtr pParams )
{
    if(!useWGS_)
    {
	    this->sendSuccess( addr, replyID, replyRecord,
		    pParams->encryptionKey() );
    }
    else
    {
	    this->sendSuccess( addr, replyID, replyRecord,
		    pParams);
    }

	cachedLoginMap_[ addr ].replyRecord( replyRecord );

	// Do not let the map get too big. Just check every so often to get rid of
	// old caches.

	if (cachedLoginMap_.size() > 100)
	{
		CachedLoginMap::iterator iter = cachedLoginMap_.begin();

		while (iter != cachedLoginMap_.end())
		{
			CachedLoginMap::iterator prevIter = iter;
			++iter;

			if (prevIter->second.isTooOld())
			{
				cachedLoginMap_.erase( prevIter );
			}
		}
	}
}


/**
 *	This method sends a reply to a client indicating that logging in has been
 *	successful.
 */
void LoginApp::sendSuccess( const Mercury::Address & addr,
	Mercury::ReplyID replyID, const LoginReplyRecord & replyRecord,
	const std::string & encryptionKey )
{
	Mercury::Bundle b;
	b.startReply( replyID );
	b << (int8)LogOnStatus::LOGGED_ON;

	// We have to encrypt the reply record because it contains the session key
	Mercury::EncryptionFilter filter( encryptionKey );
	MemoryOStream clearText;
	clearText << replyRecord;
	filter.encryptStream( clearText, b );

	this->extNub().send( addr, b );
}

void LoginApp::sendSuccess( const Mercury::Address & addr,
	Mercury::ReplyID replyID, const LoginReplyRecord & replyRecord,
	LogOnParamsPtr pParams  )
{
	Mercury::Bundle b;
	b.startReply( replyID );
	b << (int8)LogOnStatus::LOGGED_ON;

	// We have to encrypt the reply record because it contains the session key
	Mercury::EncryptionFilter filter( pParams->encryptionKey() );
	MemoryOStream clearText;
    clearText << replyRecord << pParams->password();
    filter.encryptStream( clearText, b );

	this->extNub().send( addr, b );
}

/**
 *	This method checks whether there is a login in progress from this
 *	address.
 */
bool LoginApp::handleResentPendingAttempt( const Mercury::Address & addr,
		Mercury::ReplyID replyID )
{
	CachedLoginMap::iterator iter = cachedLoginMap_.find( addr );

	if (iter != cachedLoginMap_.end())
	{
		CachedLogin & cache = iter->second;

		if (cache.isPending())
		{
			DEBUG_MSG( "Ignoring repeat attempt from %s "
					"while another attempt is in progress (for %s)\n",
				addr.c_str(),
				cache.pParams()->username().c_str() );

			return true;
		}
	}
	return false;
}


/**
 *	This method checks whether there is a cached login attempt from this
 *	address. If there is, it is assumed that the previous reply was dropped and
 *	this one is resent.
 */
bool LoginApp::handleResentCachedAttempt( const Mercury::Address & addr,
		LogOnParamsPtr pParams, Mercury::ReplyID replyID )
{
	CachedLoginMap::iterator iter = cachedLoginMap_.find( addr );

	if (iter != cachedLoginMap_.end())
	{
		CachedLogin & cache = iter->second;
		if (!cache.isTooOld() && *cache.pParams() == *pParams)
		{
			this->sendSuccess( addr, replyID, cache.replyRecord(),
				cache.pParams()->encryptionKey() );

			return true;
		}
	}

	return false;
}


/**
 *  Handles incoming shutdown requests.  This is basically another way of
 *  triggering a controlled system shutdown instead of sending a SIGUSR1.
 */
void LoginApp::controlledShutDown( const Mercury::Address &source,
	Mercury::UnpackedMessageHeader &header,
	BinaryIStream &data )
{
	INFO_MSG( "LoginApp::controlledShutDown: "
		"Got shutdown command from %s\n", source.c_str() );

	this->controlledShutDown();
}


/**
 *	This method handles a message telling us the overload status of the system.
 *	It is received on the internal nub.
 */
/*
void LoginApp::systemOverloadStatus(
	const LoginIntInterface::systemOverloadStatusArgs & args )
{
	if (systemOverloaded_ && !args.overloaded)
	{
		DEBUG_MSG( "LoginApp::systemOverloadStatus: "
			"System no longer overloaded.\n" );
	}
	else if (!systemOverloaded_ && args.overloaded)
	{
		DEBUG_MSG( "LoginApp::systemOverloadStatus: "
			"System has become overloaded.\n" );
	}

	systemOverloaded_ = args.overloaded;
}
*/

/**
 *  This method sets the private key (used for initial client connection) of the 
 * 	LoginApp from a resource path.
 */
bool LoginApp::setKeyFromResource( const std::string & path )
{
	DataSectionPtr pSection = BWResource::openSection( path );

	if (pSection)
	{
		BinaryPtr pBinData = pSection->asBinary();
		std::string keyStr( pBinData->cdata(), pBinData->len() );

		return privateKey_.setKey( keyStr );
	}
	else
	{
		ERROR_MSG( "LoginApp::setKeyFromResource: "
			"Couldn't load private key from non-existent file %s\n",
			path.c_str() );

		return false;
	}
}


// -----------------------------------------------------------------------------
// Section: CachedLogin
// -----------------------------------------------------------------------------

/**
 *  This method returns true if this login is pending, i.e. we are waiting on
 *  the DBMgr to tell us whether or not the login is successful.
 */
bool LoginApp::CachedLogin::isPending() const
{
	return creationTime_ == 0;
}


/**
 *	This method returns whether or not this cache is too old to use.
 */
bool LoginApp::CachedLogin::isTooOld() const
{
	const uint64 MAX_LOGIN_DELAY = LoginApp::instance().maxLoginDelay();

	return !this->isPending() &&
		(::timestamp() - creationTime_ > MAX_LOGIN_DELAY);
}


/**
 *  This method sets the reply record into this cached object, and is called
 *  when the DBMgr replies.
 */
void LoginApp::CachedLogin::replyRecord( const LoginReplyRecord & record )
{
	replyRecord_ = record;
	creationTime_ = ::timestamp();
}


// -----------------------------------------------------------------------------
// Section: InputMessageHandlers
// -----------------------------------------------------------------------------

/**
 *	Class for struct-style Mercury message handler objects.
 */
template <class Args> class LoginAppStructMessageHandler :
	public Mercury::InputMessageHandler
{
public:
	typedef void (LoginApp::*Handler)( const Args & args );

	LoginAppStructMessageHandler( Handler handler ) :
		handler_( handler )
	{}

private:
	virtual void handleMessage( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header, BinaryIStream & data )
	{
		Args * pArgs = (Args*)data.retrieve( sizeof(Args) );
		(LoginApp::instance().*handler_)( *pArgs );
	}

	Handler handler_;
};


void LoginApp::removeCachedLoginMap( const Mercury::Address & addr )
{
    mapRegistLock_.grab();
	CachedRegistMap::iterator iter = cachedRegistMap_.find( addr );

	if (iter != cachedRegistMap_.end())
	{
        cachedRegistMap_.erase( iter );
	}
    mapRegistLock_.give();
}

int LoginApp::WGSAuthContinue( const Mercury::Address & addr,
        Mercury::ReplyID replyID, LogOnParamsPtr pParams )
{
    DatabaseReplyHandler * pDBHandler =
        new DatabaseReplyHandler(addr, replyID, pParams);

	Mercury::Bundle	& dbBundle = this->dbMgr().bundle();
	dbBundle.startRequest( DBInterface::logOn, pDBHandler, NULL,
		Mercury::DEFAULT_REQUEST_TIMEOUT + 2000000 ); // 2 seconds extra

	dbBundle << addr << *pParams;

	this->dbMgr().send();

    return 0;
}
#define DEFINE_SERVER_HERE
#include "login_int_interface.hpp"
// loginapp.cpp
