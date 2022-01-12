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
#include "connection_control.hpp"
#include "script_bigworld.hpp"
#include "entity_manager.hpp"
#include "personality.hpp"
#include "app_config.hpp"
#include "update_handler.hpp"
#include "network/watcher_glue.hpp"

DECLARE_DEBUG_COMPONENT2( "Server", 0 )


static void scriptBandwidthFromServerMutator( int bandwidth );
extern void setBandwidthFromServerMutator( void (*mutatorFn)( int bandwidth ) );

// -----------------------------------------------------------------------------
// Section: ConnectionControl
// -----------------------------------------------------------------------------


/**
 *	Constructor.
 */
ConnectionControl::ConnectionControl() :
	pServerConnection_( new ServerConnection() ),
	pLoginInProgress_( NULL ),
	connected_( false ),
	initialPacketsIn_( -1 )
		,
		loggerMessageEndpoint_( false ),
		pLoggerMessageForwarder_( NULL )
{
	// enable an extra validation packet to be sent to work around
	// client firewalls reassigning ports
	pServerConnection_->enableReconfigurePorts();
	setBandwidthFromServerMutator( scriptBandwidthFromServerMutator );
	loggerMessageEndpoint_.socket( SOCK_DGRAM );
	loggerMessageEndpoint_.bind();
	// (start forwarding logging messages)
	pLoggerMessageForwarder_ = new LoggerMessageForwarder( "Client",
		loggerMessageEndpoint_, pServerConnection_->nub(),
		AppConfig::instance().pRoot()->read( "loggerID", 0 ) );

}


/**
 *	Destructor.
 */
ConnectionControl::~ConnectionControl()
{
	// pLoggerMessageForwarder cancels timers on server connection's nub on 
	// destruction
	delete pLoggerMessageForwarder_;

	delete pServerConnection_;

}


/**
 *	Remove all existing entities, then start to connect to the given server.
 *	If the server name is empty then just do the removing part.
 *
 *	Note that progressFn may be called before this method returns.
 */
void ConnectionControl::connect( const std::string & server,
	PyObject * loginParams,
	SmartPointer<PyObject> progressFn )
{
	// if already connected, tell progress function step 0 failed.
	if (connected_ || pServerConnection_ == NULL)
	{
		PyObject * pfn = &*progressFn;
		Py_XINCREF( pfn );
		Script::call( pfn, Py_BuildValue( "(ii)", 0, 0 ),
			"ConnectionControl::connect: ", true );
		return;
	}

	// ok, starting to log on then
	progressFn_ = progressFn;

	// clear the entity manager's state
	EntityManager::instance().disconnected();

	// if we're not really connecting anywhere, get out now
	if (server.empty())
	{
		connected_ = true;
		server_ = std::string();

		return;
	}

	// extract login parameters
	server_ = server;
	std::string username;
	std::string password;
	std::string publicKeyPath;
	int inactivityTimeout = 60;
	if (loginParams != NULL)
	{
		PyObject * attr;

#define GET_KEYWORD_ARG( NAME )												\
		attr = PyObject_GetAttrString( loginParams, #NAME );				\
																			\
		if (attr != NULL)													\
		{																	\
			Script::setData( attr, NAME );									\
		}																	\
		else																\
		{																	\
			PyErr_Clear();													\
		}																	\
																			\
		Py_XDECREF( attr );													\

		GET_KEYWORD_ARG( username );
		GET_KEYWORD_ARG( password );
		GET_KEYWORD_ARG( publicKeyPath );
		GET_KEYWORD_ARG( inactivityTimeout );
	}

	// start trying to connect then
	pServerConnection_->setInactivityTimeout( (float)inactivityTimeout );

	pLoginInProgress_ = pServerConnection_->logOnBegin(
		server_.c_str(), username.c_str(), password.c_str(),
		publicKeyPath.c_str() );

	// and wait for the reply
	connected_ = true;
	initialPacketsIn_ = -1;
}

/**
 *	Disconnect if connected.
 */
void ConnectionControl::disconnect()
{
	if (!connected_ || pLoginInProgress_ != NULL) return;

	if (!server_.empty() && pServerConnection_->online())
	{
		pServerConnection_->send();
		pServerConnection_->disconnect();
	}
	this->disconnected();
}


/**
 *	Return the current server, or NULL if not connected
 */
const char * ConnectionControl::server() const
{
	return connected_ ? server_.c_str() : NULL;
}

/**
 *	Start to determine the status of the given server.
 *
 *	Note that progressFn may be called before this method returns
 */
void ConnectionControl::probe( const std::string & server,
	SmartPointer<PyObject> progressFn )
{
	PyObject * pfn = &*progressFn;
	Py_XINCREF( pfn );
	Script::call( pfn, Py_BuildValue( "(O)", Py_None ),
		"ConnectionControl::probe: " );
}



/**
 *	Tick method
 */
void ConnectionControl::tick()
{
	// process events so the nub works even when we are offline
	if (!connected_)
	{
		// process pending events because entity manager won't
		pServerConnection_->processInput();
	}

	// are we connected, and connected to a server?
	if (connected_ && !server_.empty())
	{
		// do we have a login in progress that isn't finished?
		if (pLoginInProgress_ != NULL && !pLoginInProgress_->done())
		{
			// process pending events because entity manager won't
			pServerConnection_->processInput();
		}
		// do we have a login in progress that just finished?
		if (pLoginInProgress_ != NULL && pLoginInProgress_->done())
		{
			// save vars locally in case player creation fiddles with us
			PyObject * pfn = &*progressFn_;
			Py_XINCREF( pfn );

			LoginHandlerPtr pHandler = pLoginInProgress_;
			pLoginInProgress_ = NULL;

			// ok, see if it was successfull and create the player!
			LogOnStatus status = pServerConnection_->logOnComplete(
				pHandler, &EntityManager::instance() );

			// tell the script about our success (or otherwise)
			if (!status.succeeded())
			{
				connected_ = false;
				progressFn_ = NULL;
				const char* pStrErrorMsg = pServerConnection_->errorMsg().c_str();
				Script::call( pfn,
					Py_BuildValue( "(iis)", 1, -int(status), pStrErrorMsg ),
					"ConnectionControl::tick: ", true );
			}
			else
			{
				// ok, we've logged in
				initialPacketsIn_ = pServerConnection_->packetsIn();
				if (initialPacketsIn_+1 == 0) initialPacketsIn_ = 0;	// hmmm
				// let the update manager do its thing
				ResUpdateHandler::instance().serverConnection(
					pServerConnection_ );

				// make sure we're not offline already or dependent on an update
				if (pServerConnection_->online() &&
					!ResUpdateHandler::instance().rootDownloadInProgress())
				{
					// tell the server we want to know about its entities
					pServerConnection_->enableEntities(
						/*impendingVersionDownloaded:*/false );

					// tell the entity manager that we have reconnected
					EntityManager::instance().connected( *pServerConnection_ );
				}
				// else we'll detect going offline below
				// and let the script in on the good news
				//  (the player will be there by now)
				Script::call( pfn, Py_BuildValue( "(ii)", 1, 1 ),
					"ConnectionControl::tick: ", true );
			}
		}

		// did we get some data?
		if (initialPacketsIn_+1 != 0 &&
			initialPacketsIn_ != pServerConnection_->packetsIn())
		{
			initialPacketsIn_ = -1;

			if (progressFn_)
			{
				PyObject * pfn = &*progressFn_;
				Py_INCREF( pfn );
				BigWorldClientScript::callNextFrame(
					pfn, Py_BuildValue( "(ii)", 2, 1 ),
					"ConnectionControl::tick: " );
			}
		}

		// tick the res update handler
		ResUpdateHandler::instance().tick();

		// process input ourself if we are waiting for entities to be enabled
		if (pServerConnection_->online() &&
			EntityManager::instance().pServer() == NULL)
		{
			pServerConnection_->processInput();
		}

		// did we go offline?
		if (pLoginInProgress_ == NULL && !pServerConnection_->online())
		{
			this->disconnected();
		}
	}
}

/**
 *	This private method is called when we have been disconnected
 */
void ConnectionControl::disconnected()
{
	if (progressFn_)
	{
		PyObject * pfn = &*progressFn_;
		Py_INCREF( pfn );

		// can't call immediately 'coz could be in the middle of a tick
		BigWorldClientScript::callNextFrame(
			pfn, Py_BuildValue( "(ii)", 6, 1 ),
			"ConnectionControl::disconnected: " );
	}

	connected_ = false;
	progressFn_ = NULL;
	// let the update handler know to give up
	ResUpdateHandler::instance().serverConnection( NULL );
}


/**
 *	Static instance method
 */
ConnectionControl & ConnectionControl::instance()
{
	static ConnectionControl cc;
	return cc;
}


// -----------------------------------------------------------------------------
// Section: Python stuff
// -----------------------------------------------------------------------------

/*~ function BigWorld connect
 *  This function initiates a connection between the client and a specified
 *  server. If the client is not already connected to a server and is able to
 *  reach the network then all entities which reside on the client are
 *  destroyed in preparation for entering the server's game. This function can
 *  be used to tell the client to run offline, or to pass username, password,
 *  and other login data to the server. The client can later be disconnected
 *  from the server by calling BigWorld.disconnect.
 *  @param server This is a string representation of the name of the server
 *  which the client is to attempt to connect. This can be in the form of an IP
 *  address (eg: "11.12.123.42"), a domain address
 *  (eg: "server.coolgame.company.com"), or an empty string (ie: "" ). If an
 *  empty string is passed in then the client will run in offline mode. If a
 *  connection is successfully established then subsequent calls to
 *  BigWorld.server will return this value.
 *  @param loginParams This is a Python object that may contain members which
 *  provide login data. If a member is not present in this object then a
 *  default value is used instead. These members are:
 *
 *  username - The username for the player who is logging in. This defaults to
 *  "".
 *
 *  password - The password for the player who is logging in. This defaults to
 *  "".
 *
 *  inactivityTimeout - The amount of time the client should wait for a
 *  response from the server before assuming that the connection has failed
 *  in seconds. This defaults to 60.0. This is the only member that is read
 *  from the object but not sent to the server.
 *
 *  publicKeyPath - The location of a keyfile that contains the loginapp's
 *  public key, used to encrypt the login handshake.  This defaults to
 *  "loginapp.pubkey".
 *
 *  Code Example:
 *  @{
 *  # This example defines a class of object, an instance of which could be
 *  # passed as the loginParams argument to BigWorld.connect.
 *
 *  # define LoginInfo
 *  class LoginInfo:
 *      # init
 *      def __init__( self, username, password, inactivityTimeout ):
 *          self.username          = username
 *          self.password          = password
 *          self.inactivityTimeout = inactivityTimeout
 *  @}
 *  This data is only sent by clients which are supplied to licensees of the
 *  BigWorld server technology.
 *  @param progressFn This is a Python function which is called by BigWorld to
 *  report on the status of the connection. This function must take three
 *  arguments, where the first is interpreted as an integer indicating where
 *  in the connection process the report indicated, the second is interpreted as
 *	an integer indicating the status at that point and the third is a string
 *	message that the server may have sent.
 *
 *	It should be noted that this function may be called before BigWorld.connect
 *	returns.
 *
 *  Possible (progress, status) pairs that may be passed to this function are
 *  listed below.  Please see bigworld/src/common/login_interface.hpp for the
 *  definitive reference for these status codes.
 *
 *  0, 0 - The connection could not be initiated as either the client was
 *  already connected to a server, the network could not be reached, or the
 *  client was running in offline mode. If the client is running offline then
 *  it must be disconnected before it can connect to a server.
 *
 *  1, 1 - The client has successfully logged in to the server. Once the client
 *  begins to receive data (as notified by a call to this function with
 *  arguments 2, 1) it can consider itself to have completed the connection.
 *  Note that this is not called when the client connects in offline mode.
 *
 *  The following are client side errors:
 *
 *  1, -2 - The client failed to make a connection to the network.
 *
 *  1, -3 - The client failed to locate the server IP address via DNS.
 *
 *  1, -4 - An unknown client-side error has occurred.
 *
 *  1, -5 - The login was cancelled by the client.
 *
 *  1, -6 - The client is already online locally (i.e. exploring a space
 *  offline).
 *
 *  The following are server side errors:
 *
 *  1, -64 - The login packet sent to the server was malformed.
 *
 *  1, -65 - The login protocol the client used does not match the one on the
 *  server.
 *
 *  1, -66 - The server database did not contain an entry for the specified
 *  username and was running in a mode that did not allow for unknown users
 *  to connect, or could not create a new entry for the user. The database
 *  would most likely be unable to create a new entry for a user if an
 *  inappropriate entity type being listed in bw.xml as database/entityType.
 *
 *  1, -67 - A global password was specified in bw.xml, and it did not match the
 *  password with which the login attempt was made.
 *
 *  1, -68 - A client with this username is already logged into the server.
 *
 *  1, -69 - The defs and/or entities.xml are not identical on the client and
 *  the server.
 *
 *  1, -70 - A general database error has occurred, for example the database may
 *  have been corrupted.
 *
 *  1, -71 - The database is not ready yet.
 *
 *  1, -72 - There are illegal characters in either the username or password.
 *
 *  1, -73 - The baseappmgr is not ready yet.
 *
 *  1, -74 - The updater is not ready yet.
 *
 *  1, -75 - There are no baseapps registered at present.
 *
 *  1, -76 - Baseapps are overloaded and logins are being temporarily
 *  disallowed.
 *
 *  1, -77 - Cellapps are overloaded and logins are being temporarily
 *  disallowed.
 *
 *  1, -78 - The baseapp that was to act as the proxy for the client timed out
 *  on its reply to the dbmgr.
 *
 *  1, -79 - The baseappmgr is not responding.
 *
 *  1, -80 - The dbmgr is overloaded.
 *
 *  1, -81 - Logins are not allowed by the server yet.
 *
 *  1, -82 - Logins are temporarily not allowed by the server due to login
 * 	rate limit.
 * 
 *  2, 1 - The client has begun to receive data from the server. This indicates
 *  that the conection process is complete.
 *
 *  6, 1 - The client has been disconnected from the server.
 *  @return None
 */
static void connect( const std::string & server,
	SmartPointer<PyObject> loginParams,
	SmartPointer<PyObject> progressFn )
{
	ConnectionControl::instance().connect( server, &*loginParams, progressFn );
}
PY_AUTO_MODULE_FUNCTION( RETVOID, connect, ARG( std::string, NZARG(
	SmartPointer<PyObject>, ARG( SmartPointer<PyObject>, END ) ) ), BigWorld )

/*~ function BigWorld disconnect
 *  If the client is connected to a server then this will terminate the
 *  connection. Otherwise it does nothing. This function does not delete any
 *  entities, however it does stop the flow of data regarding entity status
 *  to and from the server.
 *
 *  The client can be connected to a server via the BigWorld.connect function.
 *  @return None
 */
static void disconnect()
{
	ConnectionControl::instance().disconnect();
}
PY_AUTO_MODULE_FUNCTION( RETVOID, disconnect, END, BigWorld )

/*~ function BigWorld server
 *  This function returns the name of the server to which the client is
 *  currently connected. The name is identical to the string which would have
 *  been passed in as the first argument to the BigWorld.connect function to
 *  establish the connection. Usually this is a string representation of an IP
 *  address. If the client is running in offline mode, then this is an empty
 *  string. Otherwise, if the client is neither connected to a server nor
 *  running in offline mode, then this returns None.
 *  @return The name of the server to which the client is connected, an empty
 *  string if the client is running in offline mode, or None if the client is
 *  not connected.
 */
static PyObject * server()
{
	const char * sname = ConnectionControl::instance().server();
	if (sname == NULL)
	{
		Py_Return;
	}
	else
	{
		return Script::getData( sname );
	}
}
PY_AUTO_MODULE_FUNCTION( RETOWN, server, END, BigWorld )

static void probe( const std::string & server,
	SmartPointer<PyObject> progressFn )
{
	ConnectionControl::instance().probe( server, progressFn );
}

/*~ function BigWorld.probe
 *	The function probe is used to determine the status of a particular server.
 *	This has not yet been implemented.
 *	@param server String. Specifies the name or address of server.
 *	@param progressFn Callback Function (python function object, or an instance of
 *	a python class that implements the __call__ interface). The function to be
 *	called when information on the status for a particular server is available.
 */
PY_AUTO_MODULE_FUNCTION( RETVOID, probe, ARG( std::string, ARG(
	SmartPointer<PyObject>, END ) ), BigWorld )



#include "pyscript/script_math.hpp"

/*~ class BigWorld.LatencyInfo
 *	The LatencyInfo class is a sub class of Vector4Provider that has an
 *	overloaded value attribute. This attribute
 *	is overloaded to return a 4 float sequence consisting of
 *	( 1, minimum latency, maximum latency, average latency ) if the client is
 *	connected to a server. If the client is not connected to the server then
 *	the value attribute returns ( 0, 0, 0, 0 ).
 *
 *	Call the BigWorld.LatencyInfo function to create an instance of a
 *	LatencyInfo object.
 */

/**
 *	This class provides latency information to Python
 */
class LatencyInfo : public Vector4Provider
{
	Py_Header( LatencyInfo, Vector4Provider )

public:
	virtual void output( Vector4 & val );

	PY_DEFAULT_CONSTRUCTOR_FACTORY_DECLARE()
};

PY_TYPEOBJECT( LatencyInfo )

PY_BEGIN_METHODS( LatencyInfo )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( LatencyInfo )
PY_END_ATTRIBUTES()

/*~ function BigWorld.LatencyInfo
 *	The function LatencyInfo is a factory function to create and return
 *	a LatencyInfo object. A LatencyInfo object is a Vector4Provider with
 *	an overloaded value attribute. This attribute (for a LatencyInfo object)
 *	is overloaded to return a 4 float sequence consisting of
 *	( 1, minimum latency, maximum latency, average latency ) if the client is
 *	connected to a server. If the client is not connected to the server then
 *	the value attribute returns ( 0, 0, 0, 0 ).
 *	@return A new LatencyInfo object (see also Vector4Provider).
 */
PY_FACTORY_NAMED( LatencyInfo, "LatencyInfo", BigWorld )

/**
 *	Output method ... get the actual data.
 */
void LatencyInfo::output( Vector4 & val )
{
	ServerConnection& server =
		*ConnectionControl::serverConnection();
	if (server.online())
	{
		val.x = 1;
		val.y = server.minLatency();
		val.z = server.maxLatency();
		val.w = server.avgLatency();
	}
	else
	{
		val.set( 0.f, 0.f, 0.f, 0.f );
	}
}


/**
 *	This function calls the personality script to send a message to the server
 *	to set the current bandwidth. It is invoked when the server connection's
 *	watcher is set to a different value.
 */
static void scriptBandwidthFromServerMutator( int bandwidth )
{
	Script::call(
		PyObject_GetAttrString( Personality::instance(),
			"onInternalBandwidthSetRequest" ),
		Py_BuildValue( "(i)", bandwidth ),
		"scriptBandwidthFromServerMutator: " );
}

// connection_control.cpp
