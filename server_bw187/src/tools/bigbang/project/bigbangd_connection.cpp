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

#include "bigbangd_connection.hpp"
#include "resmgr/bwresource.hpp"
#include <sstream>

#ifndef _NAVGEN
//For commentating
#include "appmgr/commentary.hpp"
#endif // _NAVGEN

DECLARE_DEBUG_COMPONENT2( "BigBangdConnection", 2 );

namespace
{

struct GetCstr
{
	std::string operator()( const unsigned char* data, int& offset ) const
	{
		int size = *(int*)data;
		std::string result;
		while( offset < size && data[ offset ] )
		{
			result += (char)data[ offset ];
			++offset;
		}
		return result;
	}
}
getCstr;

struct GetString
{
	std::string operator()( const unsigned char* data, int& offset ) const
	{
		std::string result;
		result.assign( (char*)data + offset + sizeof( int ), *(int*)( data + offset ) );
		offset += sizeof( int ) + *(int*)( data + offset );
		return result;
	}
}
getString;

struct getNum
{
	const unsigned char* data_;
	int& offset_;
	getNum( const unsigned char* data, int& offset ) : data_( data ), offset_( offset )
	{}
#define CONVERT_OPERATOR( T )					\
	operator T()							\
	{										\
		T t = *(T*)( data_ + offset_ );		\
		offset_ += sizeof( T );				\
		return t;							\
	}
	CONVERT_OPERATOR( short )
	CONVERT_OPERATOR( int )
	CONVERT_OPERATOR( float )
};

/** Read pDS as binary, and return it as a string with a \0 on the end */
std::string readBinaryAsString( DataSectionPtr pDS  )
{
	uint len = pDS->asBinary()->len();
	char* c = new char[len + 1];

	char* p = (char*) pDS->asBinary()->data();

	for (uint i = 0; i < len; i++)
		c[i] = *p++;

	c[len] = '\0';

	std::string s = c;

	delete [] c;

	return s;
}

/** Get the branch for the current space */
std::string getCurrentTag( const std::string& space )
{
	DataSectionPtr pDS = BWResource::openSection( space + "/CVS/Tag" );	
	if (pDS)
	{
		std::string s = readBinaryAsString( pDS );

		// ignore the 1st character, it's a marker we don't need
		char* start = &s[1];

		// ignore all CRs and LFs at the end
		char* end = &s[s.size() - 1];
		while (*end == '\r' || *end == '\n')
			end--;
		*(++end) = '\0';

		return start;
	}
	else
	{
		return "MAIN";
	}
}
}

namespace BWLock
{

BigBangdConnection::BigBangdConnection(const std::string& hoststrArg, const std::string& space,
									   const std::string& username, int xExtent, int zExtent )
	: port_( 8168 )
	, connected_( false )
	, username_( username )
	, xExtent_( xExtent )
	, zExtent_( zExtent )
{
	char localhost[ 10240 ];
	gethostname( localhost, 10240 );

	if( hostent* ent = gethostbyname( localhost ) )
	{
		char** addr_list = ent->h_addr_list;
		while( *addr_list )
		{
			unsigned long ip = *(DWORD*)( *addr_list );
			if( HOSTENT* host = gethostbyaddr( (char*)&ip, sizeof( ip ), AF_INET ) )
			{
				self_ = host->h_name;
				self_ = self_.substr( 0, self_.find( '.' ) );

				std::string defaultHost = "bigbangd.syd.microforte.com.au";
				const std::string & hoststr = (!hoststrArg.empty()) ? hoststrArg : defaultHost;

				int pos = hoststr.find_first_of( ':' );
				if (pos == std::string::npos)
					host_ = hoststr;
				else
				{
					host_ = hoststr.substr( 0, pos );
					std::string portstr = hoststr.substr( pos + 1 );
					port_ = atoi( portstr.c_str() );
				}

				lockspace_ = space + '/' + getCurrentTag(space);
				break;
			}
			++addr_list;
		}
	}
}

BigBangdConnection::~BigBangdConnection()
{
	invalidateCaches();
}

bool BigBangdConnection::connect()
{
	invalidateCaches();

	MF_ASSERT( !connected_ );
	char buf[256];

	uint32 addr;

	if( Endpoint::convertAddress( host_.c_str(), addr ) != 0 )
	{
		sprintf( buf, "BigBangdConnection::Connect(): Couldn't resolve address %s\n", host_.c_str() );
		INFO_MSG( buf );
#ifndef _NAVGEN
		Commentary::instance().addMsg( buf, Commentary::CRITICAL );
#endif // _NAVGEN
		return false;
	}

	ep_.socket( SOCK_STREAM );
	if (ep_.connect( htons( port_ ), addr ) == SOCKET_ERROR)
	{
		sprintf( buf, "BigBangdConnection::Connect(): Couldn't connect, last error is %d\n", WSAGetLastError() );
		INFO_MSG( buf );
#ifndef _NAVGEN
		Commentary::instance().addMsg( buf, Commentary::CRITICAL );
#endif // _NAVGEN
		return false;
	}

	connected_ = true;
	INFO_MSG( "Connected to bwlockd\n" );
#ifndef _NAVGEN
	Commentary::instance().addMsg( "Connected to bwlockd" );
#endif // _NAVGEN

	processReply( BWLOCKCOMMAND_CONNECT );

	sendCommand( &SetSpaceCommand( lockspace_ ) );
	processReply( BWLOCKCOMMAND_SETSPACE );

	sendCommand( &SetUserCommand( username_ ) );
	processReply( BWLOCKCOMMAND_SETUSER );

	sendCommand( &GetStatusCommand() );
	std::vector<unsigned char> reply = getReply( BWLOCKCOMMAND_GETSTATUS );

	if( connected_ )
	{
		int offset = sizeof( Command );
		unsigned char* command = &reply[0];
		int total = *(int*)command;

		while( offset < total )
		{
			int recordSize = getNum( command, offset );
			Computer computer;
			computer.name_ = getString( command, offset );
			computer.name_ = computer.name_.substr( 0, computer.name_.find( '.' ) );

			int lockNum = getNum( command, offset );
			while( lockNum )
			{
				Lock lock;
				lock.rect_.left_ = getNum( command, offset );
				lock.rect_.top_ = getNum( command, offset );
				lock.rect_.right_ = getNum( command, offset );
				lock.rect_.bottom_ = getNum( command, offset );
				lock.username_ = getString( command, offset );
				lock.desc_ = getString( command, offset );
				lock.time_ = getNum( command, offset );
				computer.locks_.push_back( lock );
				--lockNum;
			}
			computers_.push_back( computer );
		}
	}
	return connected_;
}

bool BigBangdConnection::changeSpace( std::string newSpace )
{
	std::string saveSpace = lockspace_;
	lockspace_ = newSpace + '/' + getCurrentTag( newSpace );
	if( connected() )
		disconnect();
	connect();
	if( connected() )
		return true;
	disconnect();
	lockspace_ = saveSpace;
	connect();
	return false;
}

void BigBangdConnection::disconnect()
{
	invalidateCaches();
	computers_.clear();
	ep_.close();
	connected_ = false;
}

bool BigBangdConnection::connected() const
{
	return connected_;
}

void BigBangdConnection::linkPoint( int16 oldLeft, int16 oldTop, int16 newLeft, int16 newTop )
{
	std::set<Rect> oldRects = getLockRects( oldLeft, oldTop );
	std::set<Rect> newRects = getLockRects( newLeft, newTop );
	std::set<Rect>::size_type oldSize = oldRects.size();
	oldRects.insert( newRects.begin(), newRects.end() );
	if( oldRects.size() != oldSize )
		linkPoints_.push_back( LinkPoint( Point( oldLeft, oldTop ), Point( newLeft, newTop ) ) );
}

bool BigBangdConnection::lock( GridRect rect, const std::string description )
{
	if( !connected_ )
	{
		INFO_MSG( "not connected, not aquiring lock\n");
		return false;
	}

	INFO_MSG( "starting lock\n" );

	int left = min( rect.bottomLeft.x, rect.topRight.x ) - xExtent_;
	int right = max( rect.bottomLeft.x, rect.topRight.x ) + xExtent_ - 1;
	int top = min( rect.bottomLeft.y, rect.topRight.y ) - zExtent_;
	int bottom = max( rect.bottomLeft.y, rect.topRight.y ) + zExtent_ - 1;

	sendCommand( &LockCommand( left, top, right, bottom, description ) );
	std::vector<unsigned char> result = getReply( BWLOCKCOMMAND_LOCK, true );
	Command* command = (Command*)&result[0];

#ifndef _NAVGEN
	int offset = sizeof( Command );
	std::string comment = getCstr( (unsigned char*)command, offset );
	Commentary::instance().addMsg( comment,
		command->flag_ ? Commentary::CRITICAL : Commentary::COMMENT );
#endif // _NAVGEN
	return command->flag_ == BWLOCKFLAG_SUCCESS;
}

void BigBangdConnection::unlock( Rect rect, const std::string description )
{
	if( !connected_ )
		return;

	INFO_MSG( "starting unlock\n" );

	sendCommand( &UnlockCommand( rect.left_, rect.top_, rect.right_, rect.bottom_, description ) );
	std::vector<unsigned char> result = getReply( BWLOCKCOMMAND_UNLOCK, true );

#ifndef _NAVGEN
	Command* command = (Command*)&result[0];

	int offset = sizeof( Command );
	std::string comment = getCstr( (unsigned char*)command, offset );
	Commentary::instance().addMsg( comment,
		command->flag_ ? Commentary::CRITICAL : Commentary::COMMENT );
#endif // _NAVGEN
}

bool BigBangdConnection::isWritableByMe( int16 x, int16 z ) const
{
	for (int i = -xExtent_; i < xExtent_ + 1; ++i)
	{
		for (int j = -zExtent_; j < zExtent_ + 1; ++j)
		{
			int curX = x + i;
			int curY = z + j;

			if (!isLockedByMe(curX,curY))
				return false;
		}
	}
	return true;
}

bool BigBangdConnection::isLockedByMe( int16 x, int16 z ) const
{
	for( std::vector<Computer>::const_iterator iter = computers_.begin(); iter != computers_.end(); ++iter )
	{
		if( stricmp( iter->name_.c_str(), self_.c_str() ) == 0 )
		{
			for( std::vector<Lock>::const_iterator it = iter->locks_.begin();
				it != iter->locks_.end(); ++it )
			{
				if( it->rect_.in( x, z ) )
					return true;
			}
		}
	}
	return false;
}

bool BigBangdConnection::isLockedByOthers( int16 x, int16 z ) const
{
	for( std::vector<Computer>::const_iterator iter = computers_.begin(); iter != computers_.end(); ++iter )
	{
		if( stricmp( iter->name_.c_str(), self_.c_str() ) != 0 )
		{
			for( std::vector<Lock>::const_iterator it = iter->locks_.begin();
				it != iter->locks_.end(); ++it )
			{
				if( it->rect_.in( x, z ) )
					return true;
			}
		}
	}
	return false;
}

bool BigBangdConnection::isSameLock( int16 x1, int16 z1, int16 x2, int16 z2 ) const
{
	if( !isLockedByMe( x1, z1 ) || !isLockedByMe( x2, z2 ) )
		return false;
	std::set<Rect> rects = getLockRects( x1, z1 );
	for( std::set<Rect>::iterator iter = rects.begin(); iter != rects.end(); ++iter )
		if( iter->in( x2, z2 ) )
			return true;
	return false;
}

GridInfo BigBangdConnection::getGridInformation( int16 x, int16 z ) const
{
	GridInfo result;
	for( std::vector<Computer>::const_iterator iter = computers_.begin(); iter != computers_.end(); ++iter )
	{
		for( std::vector<Lock>::const_iterator it = iter->locks_.begin();
			it != iter->locks_.end(); ++it )
		{
			if( it->rect_.in( x, z ) )
			{
				char tmpbuf[128];
				time_t time = (time_t)it->time_;
#if _MSC_VER >= 1400
				ctime_s( tmpbuf, 26, &time );
#else
				strncpy(tmpbuf, ctime(&time), sizeof(tmpbuf)/sizeof(char));
#endif
				result.username_ = it->username_;
				result.computer_ = iter->name_;
				result.time_ = tmpbuf;
				result.desc_ = it->desc_;;
				return result;
			}
		}
	}
	return result;
}

std::set<Rect> BigBangdConnection::getLockRectsNoLink( int16 x, int16 z ) const
{
	for( std::vector<Computer>::const_iterator iter = computers_.begin(); iter != computers_.end(); ++iter )
	{
		if( stricmp( iter->name_.c_str(), self_.c_str() ) == 0 )
		{
			for( std::vector<Lock>::const_iterator it = iter->locks_.begin();
				it != iter->locks_.end(); ++it )
			{
				if( it->rect_.in( x, z ) )
				{
					std::set<Rect> result;
					result.insert( it->rect_ );
					bool changed = true;
					while( changed )
					{
						changed = false;
						for( std::vector<Lock>::const_iterator it = iter->locks_.begin();
							it != iter->locks_.end(); ++it )
						{
							if( result.find( it->rect_ ) != result.end() )
								continue;
							for( std::set<Rect>::iterator sit = result.begin(); sit != result.end(); ++sit )
							{
								if( sit->intersect( it->rect_ ) )
								{
									result.insert( it->rect_ );
									changed = true;
									break;
								}
							}
						}
					}
					return result;
				}
			}
		}
	}
	return std::set<Rect>();
}

std::set<Rect> BigBangdConnection::getLockRects( int16 x, int16 z ) const
{
	std::set<Rect> result = getLockRectsNoLink( x, z );
	for( std::vector<LinkPoint>::const_iterator iter = linkPoints_.begin(); iter != linkPoints_.end(); ++iter )
	{
		std::pair<int16,int16> p1 = iter->first;
		std::pair<int16,int16> p2 = iter->second;
		for( std::set<Rect>::const_iterator siter = result.begin(); siter != result.end(); ++siter )
			if( siter->in( p1.first, p1.second ) )
			{
				std::set<Rect> sub = getLockRectsNoLink( p2.first, p2.second );
				result.insert( sub.begin(), sub.end() );
				break;
			}
			else if( siter->in( p2.first, p2.second ) )
			{
				std::set<Rect> sub = getLockRectsNoLink( p1.first, p1.second );
				result.insert( sub.begin(), sub.end() );
				break;
			}
	}
	return result;
}

unsigned int BigBangdConnection::getLockRectNum() const
{
	for( std::vector<Computer>::const_iterator iter = computers_.begin(); iter != computers_.end(); ++iter )
		if( stricmp( iter->name_.c_str(), self_.c_str() ) == 0 )
			return iter->locks_.size();
	return 0;
}

Rect BigBangdConnection::getLockRect( int index ) const
{
	for( std::vector<Computer>::const_iterator iter = computers_.begin(); iter != computers_.end(); ++iter )
		if( stricmp( iter->name_.c_str(), self_.c_str() ) == 0 )
			return iter->locks_.at( index ).rect_;
	throw "should check getLockRectNum before call getLockRect";// shouldn't reach here
}

void BigBangdConnection::sendCommand( const Command* command )
{
	if( connected_ )
	{
		unsigned int offset = 0;
		while( offset != command->size_ )
		{
			int ret = ep_.send( (const unsigned char*)command + offset, command->size_ - offset );
			if( ret == SOCKET_ERROR || ret == 0 )
			{
				INFO_MSG( "sendCommand socket error or socket broken, last error is %d\n", WSAGetLastError() );
				disconnect();
				break;
			}

			offset += ret;
		}
	}
}

std::vector<unsigned char> BigBangdConnection::recvCommand()
{
	std::vector<unsigned char> result;
	if( connected_ && available() )
	{
		unsigned int offset = 0;
		result.resize( sizeof( unsigned int ) );
		while( offset != result.size() )
		{
			int ret = ep_.recv( &result[0] + offset, int( result.size() - offset ) );

			if( ret == SOCKET_ERROR || ret == 0 )
			{
				INFO_MSG( "recvCommand socket error or socket broken, last error is %d\n", WSAGetLastError() );
				result.clear();
				disconnect();
				return result;
			}

			offset += ret;
		}

		result.resize( *(unsigned int*)&result[0] );
		while( offset != result.size() )
		{
			int ret = ep_.recv( &result[0] + offset, int( result.size() - offset ) );

			if( ret == SOCKET_ERROR || ret == 0 )
			{
				INFO_MSG( "recvAll socket error or socket broken, last error is %d\n", WSAGetLastError() );
				result.clear();
				disconnect();
				return result;
			}

			offset += ret;
		}
	}
	return result;
}

std::vector<unsigned char> BigBangdConnection::getReply( unsigned char command /*= BWLOCKCOMMAND_INVALID*/,
														bool processInternalCommand /*= false*/ )
{
	while( connected_ )
	{
		std::vector<unsigned char> reply = recvCommand();
		if( reply.empty() )
		{
			Sleep( 10 );
			continue;
		}
		Command* c = (Command*)&reply[0];
		if( c->id_ >= 'a' && c->id_ <= 'z' )
		{
			if( processInternalCommand )
			{
				this->processInternalCommand( reply );
				continue;
			}
			else
				return reply;
		}
		else if( c->id_ != command && command != BWLOCKCOMMAND_INVALID )
			continue;
		return reply;
	}
	return std::vector<unsigned char>();
}

void BigBangdConnection::processInternalCommand( const std::vector<unsigned char>& comm )
{
	if( connected() )
	{
		Command* c = (Command*)&comm[0];
		int offset = sizeof( Command );
		const unsigned char* command = &comm[0];
		int total = *(int*)command;

		Lock lock;
		std::string computerName_;
		lock.rect_.left_ = getNum( command, offset );
		lock.rect_.top_ = getNum( command, offset );
		lock.rect_.right_ = getNum( command, offset );
		lock.rect_.bottom_ = getNum( command, offset );
		computerName_ = getString( command, offset );
		computerName_ = computerName_.substr( 0, computerName_.find( '.' ) );
		lock.username_ = getString( command, offset );
		lock.desc_ = getString( command, offset );
		lock.time_ = getNum( command, offset );

		if( c->id_ == 'l' )
		{
			bool found = false;
			for( std::vector<Computer>::iterator iter = computers_.begin();
				iter != computers_.end(); ++iter )
			{
				if( iter->name_ == computerName_ )
				{
					iter->locks_.push_back( lock );
					found = true;
					break;
				}
			}
			if( !found )
			{
				Computer computer;
				computer.name_ = computerName_;
				computer.locks_.push_back( lock );
				computers_.push_back( computer );
			}
		}
		else if( c->id_ == 'u' )
		{
			bool found = false;
			for( std::vector<Computer>::iterator iter = computers_.begin();
				iter != computers_.end(); ++iter )
			{
				if( iter->name_ == computerName_ )
				{
					for( std::vector<Lock>::iterator it = iter->locks_.begin();
						it != iter->locks_.end(); ++it )
					{
						if( it->rect_ == lock.rect_ )
						{
							iter->locks_.erase( it );
							if( iter->locks_.empty() )
								computers_.erase( iter );
							found = true;
							break;
						}
					}
				}
				if( found )
					break;
			}
		}
	}
	invalidateCaches();
}

void BigBangdConnection::processReply( unsigned char command )
{
	std::vector<unsigned char> c = getReply( command );
	if( !c.empty() )
	{
		Command* command = (Command*)&c[0];

#ifndef _NAVGEN
		int offset = sizeof( Command );
		std::string comment = getCstr( (unsigned char*)command, offset );
		Commentary::instance().addMsg( comment,
			command->flag_ ? Commentary::CRITICAL : Commentary::COMMENT );
#endif // _NAVGEN

		if( command->flag_ )
			disconnect();
		return;
	}
}

bool BigBangdConnection::available()
{
	if( connected() )
	{
		fd_set read;
		FD_ZERO( &read );
		FD_SET( ep_, &read );
		timeval timeval = { 0 };
		int result = select( 0, &read, 0, 0, &timeval );
		if( result == SOCKET_ERROR )
		{
			disconnect();
			return false;
		}
		return result != 0;
	}
	return false;
}

const std::vector<Computer>& BigBangdConnection::computers() const
{
	return computers_;
}

std::vector<unsigned char> BigBangdConnection::getLockData( int minX, int minY, unsigned int gridWidth, unsigned int gridHeight )
{
	std::vector<unsigned char> result;
	for( int y = 0; y < (int)gridHeight; ++y )
		for( int x = 0; x < (int)gridWidth; ++x )
			if( isWritableByMe( x + minX, y + minY ) )
				result.push_back( 3 );
			else if( isLockedByMe( x + minX, y + minY ) )
				result.push_back( 1 );
			else if( isLockedByOthers( x + minX, y + minY ) )
				result.push_back( 2 );
			else
				result.push_back( 0 );
	return result;
}

bool BigBangdConnection::tick()
{
	if( available() )
	{
		std::vector<unsigned char> command = getReply();
		processInternalCommand( command );
		return true;
	}
	return false;
}

std::string BigBangdConnection::host() const
{
	return host_;
}

std::set<BWLockdCache*> BigBangdConnection::caches_;

void BigBangdConnection::registerCache( BWLockdCache* cache )
{
	caches_.insert( cache );
}

void BigBangdConnection::unregisterCache( BWLockdCache* cache )
{
	if( caches_.find( cache ) != caches_.end() )
		caches_.erase( caches_.find( cache ) );
}

void BigBangdConnection::invalidateCaches()
{
	for( std::set<BWLockdCache*>::iterator iter = caches_.begin(); iter != caches_.end(); ++iter )
		(*iter)->invalidate();
}

};
