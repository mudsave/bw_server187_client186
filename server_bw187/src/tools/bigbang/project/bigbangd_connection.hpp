/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef BIGBANGD_CONNECTION_HPP
#define BIGBANGD_CONNECTION_HPP

#include "grid_coord.hpp"
#include "network/endpoint.hpp"
#include <string>
#include <set>

namespace BWLock
{

const unsigned char	BWLOCKCOMMAND_INVALID = 0;
const unsigned char	BWLOCKCOMMAND_CONNECT = 'C';
const unsigned char	BWLOCKCOMMAND_SETUSER = 'A';
const unsigned char	BWLOCKCOMMAND_SETSPACE = 'S';
const unsigned char	BWLOCKCOMMAND_LOCK = 'L';
const unsigned char	BWLOCKCOMMAND_UNLOCK = 'U';
const unsigned char	BWLOCKCOMMAND_GETSTATUS = 'G';

const unsigned char	BWLOCKFLAG_SUCCESS = 0;

const unsigned int BWLOCK_MAX_USERNAME_LENGTH = 32;
const unsigned int BWLOCK_MAX_SPACE_LENGTH = 1024;
const unsigned int BWLOCK_MAX_DESCRIPTION_LENGTH = 10240;

#pragma pack( push, 1 )

struct Command
{
	unsigned int size_;
	unsigned char id_;
	unsigned char flag_;
	Command( unsigned char id ) : id_( id ), flag_( 0 ), size_( 0xffffffff )
	{}
};

struct SetUserCommand : public Command
{
	char username_[ BWLOCK_MAX_USERNAME_LENGTH + 1 ];
	SetUserCommand( const std::string& username )
		: Command( BWLOCKCOMMAND_SETUSER )
	{
		strncpy( username_, username.c_str(), BWLOCK_MAX_USERNAME_LENGTH );
		username_[ BWLOCK_MAX_USERNAME_LENGTH ] = 0;
		size_ = sizeof( Command ) + strlen( username_ );
	}
};

struct SetSpaceCommand : public Command
{
	char spacename_[ BWLOCK_MAX_SPACE_LENGTH + 1 ];
	SetSpaceCommand( const std::string& spacename )
		: Command( BWLOCKCOMMAND_SETSPACE )
	{
		strncpy( spacename_, spacename.c_str(), BWLOCK_MAX_SPACE_LENGTH );
		spacename_[ BWLOCK_MAX_SPACE_LENGTH ] = 0;
		size_ = sizeof( Command ) + strlen( spacename_ );
	}
};

struct LockCommand : public Command
{
	short left_;
	short top_;
	short right_;
	short bottom_;
	char desc_[ BWLOCK_MAX_DESCRIPTION_LENGTH + 1 ];
	LockCommand( short left, short top, short right, short bottom, const std::string& desc )
		: Command( BWLOCKCOMMAND_LOCK ), left_( left ), top_( top ), right_( right ), bottom_( bottom )
	{
		strncpy( desc_, desc.c_str(), BWLOCK_MAX_DESCRIPTION_LENGTH );
		desc_[ BWLOCK_MAX_DESCRIPTION_LENGTH ] = 0;
		size_ = sizeof( LockCommand ) - BWLOCK_MAX_DESCRIPTION_LENGTH - 1 + strlen( desc_ );
	}
};

struct UnlockCommand : public Command
{
	short left_;
	short top_;
	short right_;
	short bottom_;
	char desc_[ BWLOCK_MAX_DESCRIPTION_LENGTH + 1 ];
	UnlockCommand( short left, short top, short right, short bottom, const std::string& desc )
		: Command( BWLOCKCOMMAND_UNLOCK ), left_( left ), top_( top ), right_( right ), bottom_( bottom )
	{
		strncpy( desc_, desc.c_str(), BWLOCK_MAX_DESCRIPTION_LENGTH );
		desc_[ BWLOCK_MAX_DESCRIPTION_LENGTH ] = 0;
		size_ = sizeof( UnlockCommand ) - BWLOCK_MAX_DESCRIPTION_LENGTH - 1 + strlen( desc_ );
	}
};

struct GetStatusCommand : public Command
{
	GetStatusCommand() : Command( BWLOCKCOMMAND_GETSTATUS )
	{
		size_ = sizeof( UnlockCommand );
	}
};

#pragma pack( pop )

struct Rect
{
	short left_;
	short top_;
	short right_;
	short bottom_;
	bool in( int x, int y ) const
	{
		return x >= left_ && x <= right_ && y >= top_ && y <= bottom_;
	}
	bool intersect( const Rect& that ) const
	{
		if( in( that.left_, that.top_ ) || in( that.right_, that.top_ )
			|| in( that.left_, that.bottom_ ) || in( that.right_, that.bottom_ ) )
			return true;
		return that.in( left_, top_ ) || that.in( right_, top_ )
			|| that.in( left_, bottom_ ) || that.in( right_, bottom_ );
	}
	bool operator<( const Rect& that ) const
	{
		if( left_ < that.left_ )
			return true;
		else if( left_ == that.left_ )
		{
			if( top_ < that.top_ )
				return true;
			else if( top_ == that.top_ )
			{
				if( right_ < that.right_ )
					return true;
				else if( right_ == that.right_ )
				{
					if( bottom_ < that.bottom_ )
						return true;
				}
			}
		}
		return false;
	}
};

inline bool operator ==( const Rect& r1, const Rect& r2 )
{
	return r1.left_ == r2.left_ && r1.top_ == r2.top_ && r1.right_ == r2.right_
		&& r1.bottom_ == r2.bottom_;
}

struct Lock
{
	Rect rect_;
	std::string username_;
	std::string desc_;
	float time_;
};

struct Computer
{
	std::string name_;
	std::vector<Lock> locks_;
};

struct GridInfo
{
	std::string username_;
	std::string computer_;
	std::string time_;
	std::string desc_;
};

struct BWLockdCache;

class BigBangdConnection
{
public:
	BigBangdConnection( const std::string& hoststr, const std::string& space,
		const std::string& username, int xExtent, int zExtent );
	~BigBangdConnection();

	bool connect();
	bool changeSpace( std::string newSpace );
	void disconnect();
	bool connected() const;

	int xExtent() const	{	return	xExtent_;	}
	int zExtent() const	{	return	zExtent_;	}

	void linkPoint( int16 oldLeft, int16 oldTop, int16 newLeft, int16 newTop );

	bool lock( GridRect rect, const std::string description );
	void unlock( Rect rect, const std::string description );

	bool isWritableByMe( int16 x, int16 z ) const;
	bool isLockedByMe( int16 x, int16 z ) const;
	bool isLockedByOthers( int16 x, int16 z ) const;
	bool isSameLock( int16 x1, int16 z1, int16 x2, int16 z2 ) const;

	GridInfo getGridInformation( int16 x, int16 z ) const;

	std::set<Rect> getLockRects( int16 x, int16 z ) const;

	unsigned int getLockRectNum() const;
	Rect getLockRect( int index ) const;

	bool tick();// return true => the lock rects has been updated

	const std::vector<Computer>& computers() const;

	std::vector<unsigned char> getLockData( int minX, int minY, unsigned int gridWidth, unsigned int gridHeight );

	std::string host() const;

	static void registerCache( BWLockdCache* cache );
	static void unregisterCache( BWLockdCache* cache );
private:
	std::set<Rect> getLockRectsNoLink( int16 x, int16 z ) const;

	std::string self_;
	std::string host_;
	uint16 port_;
	std::string lockspace_;
	std::string username_;

	Endpoint ep_;
	void sendCommand( const Command* command );
	std::vector<unsigned char> recvCommand();
	std::vector<unsigned char> getReply( unsigned char command = BWLOCKCOMMAND_INVALID, bool processInternalCommand = false );
	void processReply( unsigned char command );
	void processInternalCommand( const std::vector<unsigned char>& command );
	bool available();
	bool connected_;
	std::vector<Computer> computers_;
	typedef std::pair<int16,int16> Point;
	typedef std::pair<Point,Point> LinkPoint;
	std::vector<LinkPoint> linkPoints_;
	int xExtent_;
	int zExtent_;

	static std::set<BWLockdCache*> caches_;
	static void invalidateCaches();
};

struct BWLockdCache
{
	BWLockdCache()
	{
		BigBangdConnection::registerCache( this );
	}
	virtual ~BWLockdCache()
	{
		BigBangdConnection::unregisterCache( this );
	}
	virtual void invalidate() = 0;
};

};

#endif // BIGBANGD_CONNECTION_HPP
