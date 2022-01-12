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
#undef LOGIN_INTERFACE_HPP
#endif

#ifndef LOGIN_INTERFACE_HPP
#define LOGIN_INTERFACE_HPP

#include "cstdmf/md5.hpp"
#include "cstdmf/binary_stream.hpp"
#include "cstdmf/smartpointer.hpp"
#include <cstdlib>

// -----------------------------------------------------------------------------
// Section: Includes
// -----------------------------------------------------------------------------

// Everything in this section is only parsed once.

#ifndef LOGIN_INTERFACE_HPP_ONCE_ONLY
#define LOGIN_INTERFACE_HPP_ONCE_ONLY

#include "network/mercury.hpp"

// Version 11: pitch and roll are sent down in createEntity.
// Version 12: Added voiceData to the client interface.
// Version 13: Entity to update sent up in avatarUpdate.
// Version 14: EntityTypeID changed to a uint16.
// Version 15: Implemented spaces, including space viewports and space data.
// Version 16: setGameTime has only (server) game time. Renamed from setTime.
// Version 17: Implemented vehicles and split enterAoI into 3.
// Version 18: Upstream avatarUpdate does vehicles. Removed requestBandwidth.
// Version 19: Added cell fault tolerance
// Version 20: Resource versioning and basic update messages.
// Version 21: Added changeProxy to client interface.
// Version 22: Add base app fault tolerance
// Version 23: Messages for co-ordinated (live) resource updates
// Version 24: Client (and server) authentication with a session key
// Version 25: Player entity data from createPlayer instead of login reply
// Version 26: Separate createBasePlayer and createCellPlayer messages
// Version 27: Explicit pose corrections and control toggle. Removed cell ids.
// Version 28: Replaced LogOnReplyStatus with LogOnStatus.
// Version 29: Client type indices collapsed. Signed/unsigned data MD5s differ.
// Version 30: Changes to how Mercury handles once-off reliable data.
// Version 31: Changed packed float y-value format.
// Version 32: Add baseAppLogin message to fix NAT issues.
// Version 33: Added configuration option for ordering client-server channel
// Version 34: Changed login to use once-off reliability to loginapp
// Version 35: Added setGameTime message to BaseApp to fix restore from DB.
// Version 36: Reverted to 1472 for MTU. Added disconnectClient and loggedOff.
// Version 37: Implemented piggybacking for ordered channels
// Version 38: Added proxyFilterData for encrypted connection support.
// Version 39: Logging in no longer uses once-off reliability
// Version 40: LOGIN_VERSION is now 4 bytes
// Version 41: piggyback length changed to ones complement
// Version 42: Added encryption support
// Version 43: Added new login status code for rejection due to rate limiter
#define LOGIN_VERSION_RATE_LIMIT_STATUS uint32( 43 )
#define OLDEST_SUPPORTED_LOGIN_VERSION 	uint32( 42 )
#define LOGIN_VERSION 					LOGIN_VERSION_RATE_LIMIT_STATUS

/**
 *  Once-off reliable resend period from client, in microseconds.
 *  Used for the once-off reliable login message.
 */
const int CLIENT_ONCEOFF_RESEND_PERIOD = 1 * 1000 * 1000; // 1 second

/**
 *	Once-off reliable max resends. Used for the once-off reliable login
 *	message.
 */
const int CLIENT_ONCEOFF_MAX_RESENDS = 50;

namespace Mercury
{
class PublicKeyCipher;
};

/**
 *  This class wraps the parameters sent by the client during login.  These need
 *  to be passed from the loginapp -> dbmgr -> baseapp.
 */
class LogOnParams : public SafeReferenceCount
{
public:
	/// An enumeration of flags for fields that are optionally streamed.
	typedef uint8 Flags;
	static const Flags HAS_DIGEST = 0x1;
	static const Flags HAS_ALL = 0x1;
	static const Flags PASS_THRU = 0xFF;

	LogOnParams() :
		flags_( HAS_ALL )
	{
		nonce_ = std::rand();
		digest_.clear();
	}

	LogOnParams( const std::string & username, const std::string & password,
		const std::string & encryptionKey ) :
		flags_( HAS_ALL ),
		username_( username ),
		password_( password ),
		encryptionKey_( encryptionKey )
	{
		nonce_ = std::rand();
		digest_.clear();
	}

	bool addToStream( BinaryOStream & data, Flags flags = PASS_THRU,
		Mercury::PublicKeyCipher * pKey = NULL );

	bool readFromStream( BinaryIStream & data,
		Mercury::PublicKeyCipher * pKey = NULL );

	Flags flags() const { return flags_; }

	const std::string & username() const { return username_; }
	void username( const std::string & username ) { username_ = username; }

	const std::string & password() const { return password_; }
	void password( const std::string & password ) { password_ = password; }

	const std::string & encryptionKey() const { return encryptionKey_; }
	void encryptionKey( const std::string & s ) { encryptionKey_ = s; }

	const MD5::Digest & digest() const { return digest_; }
	void digest( const MD5::Digest & digest ){ digest_ = digest; }

	/**
	 *	Check if security information is the same between two login requests.
	 */
	bool operator==( const LogOnParams & other )
	{
		return username_ == other.username_ &&
			password_ == other.password_ &&
			encryptionKey_ == other.encryptionKey_ &&
			nonce_ == other.nonce_;
	}

private:
	Flags flags_;
	std::string username_;
	std::string password_;
	std::string encryptionKey_;
	uint32 nonce_;
	MD5::Digest digest_;
};

typedef SmartPointer< LogOnParams > LogOnParamsPtr;


/**
 *  Streaming operator for LogOnParams that ignores encryption.
 */
inline BinaryOStream & operator<<( BinaryOStream & out, LogOnParams & params )
{
	params.addToStream( out );
	return out;
}


/**
 *  Streaming operator for LogOnParams that ignores encryption.
 */
inline BinaryIStream & operator>>( BinaryIStream & in, LogOnParams & params )
{
	params.readFromStream( in );
	return in;
}


/**
 *	This class is used to encapsulate the status returned by
 *	ServerConnection::logOn.
 *
 *	@see ServerConnection::logOn
 *
 *	@ingroup network
 */
class LogOnStatus
{
public:

	/// This enumeration contains the possible results of a logon attempt.
	enum Status
	{
		// client status values
		NOT_SET,
		LOGGED_ON,
		CONNECTION_FAILED,
		DNS_LOOKUP_FAILED,
		UNKNOWN_ERROR,
		CANCELLED,
		ALREADY_ONLINE_LOCALLY,
		PUBLIC_KEY_LOOKUP_FAILED,
		LAST_CLIENT_SIDE_VALUE = 63,

		// server status values
		LOGIN_MALFORMED_REQUEST,
		LOGIN_BAD_PROTOCOL_VERSION,

		LOGIN_REJECTED_NO_SUCH_USER,
		LOGIN_REJECTED_INVALID_PASSWORD,
		LOGIN_REJECTED_ALREADY_LOGGED_IN,
		LOGIN_REJECTED_BAD_DIGEST,
		LOGIN_REJECTED_DB_GENERAL_FAILURE,
		LOGIN_REJECTED_DB_NOT_READY,
		LOGIN_REJECTED_ILLEGAL_CHARACTERS,
		LOGIN_REJECTED_BASEAPPMGR_NOT_READY,
		LOGIN_REJECTED_UPDATER_NOT_READY,
		LOGIN_REJECTED_NO_BASEAPPS,
		LOGIN_REJECTED_BASEAPP_OVERLOAD,
		LOGIN_REJECTED_CELLAPP_OVERLOAD,
		LOGIN_REJECTED_BASEAPP_TIMEOUT,
		LOGIN_REJECTED_BASEAPPMGR_TIMEOUT,
		LOGIN_REJECTED_DBMGR_OVERLOAD,
		LOGIN_REJECTED_LOGINS_NOT_ALLOWED,
		LOGIN_REJECTED_RATE_LIMITED,

        LOGIN_ACCOUNT_BLOCKED = 249,
        LOGIN_CONTROL_BY_GM = 250,
		LOGIN_WEBSERVER_BUSY = 251,
	    LOGIN_RELOGIN = 252,
	    LOGIN_NEED_WEBSERVER_REGISTER = 253,
		LOGIN_CUSTOM_DEFINED_ERROR = 254,
		LAST_SERVER_SIDE_VALUE = 255
	};

	/// This is the constructor.
	LogOnStatus(Status status = NOT_SET) : status_(status)
	{
	}

	/// This method returns true if the logon succeeded.
	bool succeeded() const {return status_ == LOGGED_ON;}

	/// This method returns true if the logon failed.
	bool fatal() const
	{
		return
			status_ == CONNECTION_FAILED ||
			status_ == CANCELLED ||
			status_ == UNKNOWN_ERROR;
	}

	/// This method returns true if the logon was successful, or is still
	/// pending.
	bool okay() const
	{
		return
			status_ == NOT_SET ||
			status_ == LOGGED_ON;
	}

	/// Assignment operator.
	void operator = (int status) { status_ = status; }

	/// This operator returns the status as an integer.
	operator int() const { return status_; }

private:
	int status_;
};

/**
 * 	This structure contains the reply from a successful login.
 */
struct LoginReplyRecord
{
	Mercury::Address	serverAddr;			// send to here
	uint32				sessionKey;			// use this session key
	uint32				latestVersion;		// version running now
	uint32				impendingVersion;	// version that must be ready
};

// Probe reply is a list of pairs of strings
// Some strings can be interpreted as integers
#define PROBE_KEY_HOST_NAME			"hostName"
#define PROBE_KEY_OWNER_NAME		"ownerName"
#define PROBE_KEY_USERS_COUNT		"usersCount"
#define PROBE_KEY_UNIVERSE_NAME		"universeName"
#define PROBE_KEY_SPACE_NAME		"spaceName"
#define PROBE_KEY_BINARY_ID			"binaryID"
#define PROBE_KEY_LATEST_VERSION	"latestVersion"
#define PROBE_KEY_IMPENDING_VERSION "impendingVersion"

#endif // LOGIN_INTERFACE_HPP_ONCE_ONLY

#include "network/interface_minder.hpp"

// -----------------------------------------------------------------------------
// Section: Login Interface
// -----------------------------------------------------------------------------

#pragma pack(push,1)
BEGIN_MERCURY_INTERFACE( LoginInterface )

	// uint8 version
	// bool encrypted
	// LogOnParams
	MERCURY_VARIABLE_MESSAGE( login, 2, &gLoginHandler )

	MERCURY_FIXED_MESSAGE( probe, 0, &gProbeHandler )

	MERCURY_VARIABLE_MESSAGE( webServicesRegister, 2, &gwebServicesRegisterHandler )

END_MERCURY_INTERFACE()

#pragma pack(pop)

#endif // LOGIN_INTERFACE_HPP
