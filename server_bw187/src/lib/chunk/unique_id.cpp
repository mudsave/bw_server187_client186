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
#include "unique_id.hpp"
#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT2( "Chunk", 0 )

UniqueID UniqueID::s_zero_(0, 0, 0, 0);

UniqueID::UniqueID() :
	a(0), b(0), c(0), d(0)
{
}

UniqueID::UniqueID( const std::string& s ) :
	a(0), b(0), c(0), d(0)
{
	if (!s.empty())
	{
		unsigned int data[4];
		if (fromString(s, &data[0]))
        {
        	a = data[0]; b = data[1]; c = data[2]; d = data[3];            
        }
        else
        {
        	ERROR_MSG( "Error parsing UniqueID : %s", s.c_str() );
        }
	}
}

UniqueID::operator std::string() const
{
	char buf[256];
	sprintf( buf, "%08X.%08X.%08X.%08X", a, b, c, d );
	std::string retVal( buf );
	return retVal;
}

bool UniqueID::operator==( const UniqueID& rhs ) const
{
	return (a == rhs.a) && (b == rhs.b) && (c == rhs.c) && (d == rhs.d);
}

bool UniqueID::operator!=( const UniqueID& rhs ) const
{
	return !(*this == rhs);
}

bool UniqueID::operator<( const UniqueID& rhs ) const
{
	if (a < rhs.a)
		return true;
	else if (a > rhs.a)
		return false;

	if (b < rhs.b)
		return true;
	else if (b > rhs.b)
		return false;

	if (c < rhs.c)
		return true;
	else if (c > rhs.c)
		return false;

	if (d < rhs.d)
		return true;
	else if (d > rhs.d)
		return false;

	return false;
}

#ifdef _WIN32
UniqueID UniqueID::generate()
{
	UniqueID n;
#ifndef STATION_POSTPROCESSOR
	if (FAILED(CoCreateGuid( reinterpret_cast<GUID*>(&n) )))
		CRITICAL_MSG( "Couldn't create GUID" );
#endif
	return n;
}
#endif

bool UniqueID::isUniqueID( const std::string& s)
{
    unsigned int data[4];
    return fromString( s, &data[0] );
}

bool UniqueID::fromString( const std::string& s, unsigned int* data )
{
	if (s.empty())
		return false;
		
	std::string copyS(s);
    char* str = const_cast<char*>(copyS.c_str());
	for (int offset = 0; offset < 4; offset++)
	{
		char* initstr = str;
		data[offset] = strtoul(initstr, &str, 16);

		// strtoul will make these the same if it didn't read anything
		if (initstr == str)
		{			
			return false;
		}

		str++;
	}

    return true;
}

// unique_id.cpp
