/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

/**
 *	StringUtils: String utility methods.
 */

#include "pch.hpp"
#include <windows.h>
#include "Shlwapi.h"
#include <algorithm>
#include <string>
#include <vector>
#include "string_utils.hpp"
#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT( 0 );


std::string StringUtils::vectorToString( const std::vector<std::string>& vec, char separator /* = ';' */ )
{
	std::string ret;
	for( std::vector<std::string>::const_iterator i = vec.begin(); i != vec.end(); ++i )
	{
		if ( ret.length() > 0 )
			ret += separator;
		ret += (*i);
	}

	return ret;
}

void StringUtils::vectorFromString( const std::string str, std::vector<std::string>& vec, char* separators /* = ",;" */ )
{
	char* resToken;
	int curPos = 0;

	resToken = strtok( (char*)str.c_str(), separators );
	while ( resToken != 0 )
	{
		std::string subStr = resToken;
		vec.push_back( subStr );

		resToken = strtok( 0, separators );
	}
}

bool StringUtils::matchExtension( const std::string& fname, const std::vector<std::string>& extensions )
{
	if ( extensions.empty() )
		return true;

	int dot = (int)fname.find_last_of( '.' ) + 1;
	if ( dot <= 0 )
		return false;

	std::string ext = fname.substr( dot );
	toLowerCase( ext );

	for( std::vector<std::string>::const_iterator i = extensions.begin(); i != extensions.end(); ++i )
		if ( (*i) == ext )
			return true;

	return false;
}

bool StringUtils::matchSpec( const std::string& fname, const std::vector<std::string>& specs )
{
	if ( specs.empty() )
		return true;

	const char* pfname = fname.c_str();

	for( std::vector<std::string>::const_iterator i = specs.begin(); i != specs.end(); ++i )
		if ( PathMatchSpec( pfname, (*i).c_str() ) )
			return true;

	return false;
}

bool StringUtils::findInVector( const std::string& str, const std::vector<std::string>& vec )
{
	if ( vec.empty() )
		return true;

	const char* pstr = str.c_str();

	for( std::vector<std::string>::const_iterator i = vec.begin(); i != vec.end(); ++i )
		if ( _stricmp( (*i).c_str(), pstr ) == 0 )
			return true;

	return false;
}

void StringUtils::filterSpecVector( std::vector<std::string>& vec, const std::vector<std::string>& exclude )
{
	if ( vec.empty() || exclude.empty() )
		return;

	for( std::vector<std::string>::iterator i = vec.begin(); i != vec.end(); )
	{
		if ( matchSpec( (*i), exclude ) )
			i = vec.erase( i );
		else
			++i;
	}
}

void StringUtils::toLowerCase( std::string& str )
{
	std::transform( str.begin(), str.end(), str.begin(), tolower );
}

void StringUtils::toUpperCase( std::string& str )
{
	std::transform( str.begin(), str.end(), str.begin(), toupper );
}

void StringUtils::toMixedCase( std::string& str )
{
	bool lastSpace = true;
	
	std::string::iterator it = str.begin();
	std::string::iterator end = str.end();

	for(; it != end; ++it)
	{
		if (lastSpace)
			*it = toupper( *it );
		else
			*it = tolower( *it );
		lastSpace = ( *it == ' ' );
	}
}

const std::string StringUtils::lowerCase( const std::string& str )
{
	std::string temp = str;
	toLowerCase( temp );
	return temp;
}

const std::string StringUtils::upperCase( const std::string& str )
{
	std::string temp = str;
	toUpperCase( temp );
	return temp;
}

void StringUtils::replace( std::string& str, char ch, char rep )
{
    std::replace(str.begin(), str.end(), ch, rep );
}

void StringUtils::replace( std::string& str, const std::string& from, const std::string& to )
{
	if( !from.empty() )
	{
		std::string newStr;
		while( const char* p = strstr( str.c_str(), from.c_str() ) )
		{
			newStr.insert( newStr.end(), str.c_str(), p );
			newStr += to;
			str.erase( str.begin(), str.begin() + ( p - str.c_str() ) + from.size() );
		}
		str = newStr + str;
	}
}

bool StringUtils::copyToClipboard( const std::string & str )
{
    bool ok = false;
    if (::OpenClipboard(NULL))
    {
        HGLOBAL data = 
            ::GlobalAlloc
            (
                GMEM_MOVEABLE, 
                (str.length() + 1)*sizeof(char)
            );
        if (data != NULL && ::EmptyClipboard() != FALSE)
        {
            LPTSTR cstr = (LPTSTR)::GlobalLock(data);
            memcpy(cstr, str.c_str(), str.length()*sizeof(char));
            cstr[str.length()] = '\0';
            ::GlobalUnlock(data);
            ::SetClipboardData(CF_TEXT, data);
            ok = true;
        }
        ::CloseClipboard();
    }
    return ok;
}

bool StringUtils::canCopyFromClipboard()
{
    bool ok = false;
    if (::OpenClipboard(NULL))
    {
        ok = ::IsClipboardFormatAvailable(CF_TEXT) != FALSE;
        ::CloseClipboard();
    }
    return ok;
}

bool StringUtils::copyFromClipboard( std::string & str )
{
    bool ok = false;
    str.clear();
    if (::OpenClipboard(NULL))
    {
        HGLOBAL data = ::GetClipboardData(CF_TEXT);
        if (data != NULL)
        {
            LPTSTR cstr = (LPTSTR)::GlobalLock(data);
            str = cstr;
            ::GlobalUnlock(data);
            ok = true;
        }
        ::CloseClipboard();
    }
    return ok;
}

void StringUtils::increment( std::string& str, IncrementStyle incrementStyle )
{
    //
    // For IS_EXPLORER the incrementation goes like:
    //
    //      original string
    //      Copy of original string
    //      Copy (2) of original string
    //      Copy (3) of original string
    //      ...
    //
    if (incrementStyle == IS_EXPLORER)
    {
        // Handle the degenerate case:
        if (str.empty())
            return;

        // See if the string starts with "Copy of ".  If it does then the result
        // will be "Copy (2) of" remainder.
        if (str.substr(0, 8) == "Copy of ")
        {
            std::string remainder = str.substr(8, std::string::npos);
            str = "Copy (2) of " + remainder;
            return;
        }

        // See if the string starts with "Copy (n) of " where n is a number.  If it
        // does then the result will be "Copy (n + 1) of " remainder.
        if (str.substr(0, 6) == "Copy (")
        {
            size_t       pos = 6;
            unsigned int num = 0;
            while (pos < str.length() && ::isdigit(str[pos]))
            {
                num *= 10;
                num += str[pos] - '0';
                ++pos;
            }
            ++num;
            std::stringstream numstr;
            numstr << num;
            if (pos < str.length())
            {
                if (str.substr(pos, 5) == ") of ")
                {
                    std::string remainder = str.substr(pos + 5, std::string::npos);
                    str  = "Copy (";
                    str += numstr.str();
                    str += ") of ";
                    str += remainder;
                    return;
                }
            }
        }

        // The result must be "Copy of " str.
        str = "Copy of " + str;
        return;
    }
    //
    // For IS_END the incrementation goes like:
    //
    //      original string
    //      original string 2
    //      original string 3
    //      original string 4
    //      ...
    //
    // or, if the orignal string is "original string(0)":
    //
    //      original string(0)
    //      original string(1)
    //      original string(2)
    //      ...
    //
    else if (incrementStyle == IS_END)
    {
        if (str.empty())
            return;

        char lastChar    = str[str.length() - 1];
        bool hasLastChar = ::isdigit(lastChar) == 0;

        // Find the position of the ending number and where it begins:
        int pos = (int)str.length() - 1;
        if (hasLastChar)
            --pos;
        unsigned int count = 0;
        unsigned int power = 1;
        bool hasDigits = false;
        for (;pos >= 0 && ::isdigit(str[pos]) != 0; --pos)
        {
            count += power*(str[pos] - '0'); 
            power *= 10;
            hasDigits = true;
        }

        // Case where there was no number:
        if (!hasDigits)
        {
            count = 1;
            ++pos;
            hasLastChar = false;
        }

        // Increment the count:
        ++count;    

        // Construct the result:
        std::stringstream stream;
        std::string prefix = str.substr(0, pos + 1);
        stream << prefix.c_str();
        if (!hasDigits)
            stream << ' ';
        stream << count;
        if (hasLastChar)
            stream << lastChar;
        str = stream.str();
        return;
    }
    else
    {
        return;
    }
}

bool 
StringUtils::makeValidFilename
(
    std::string     &str, 
    char            replaceChar /*= '_'*/,
    bool            allowSpaces /*= true*/
)
{
    static char const *NOT_ALLOWED         = "/<>?\\|*:";
    static char const *NOT_ALLOWED_NOSPACE = "/<>?\\|*: ";

    bool changed = false; // Were any changes made?

    // Remove leading whitespace:
    while (!str.empty() && ::isspace(str[0]))
    {
        changed = true;
        str.erase(str.begin() + 0);
    }

    // Remove trailing whitespace:
    while (!str.empty() && ::isspace(str[str.length() - 1]))
    {
        changed = true;
        str.erase(str.begin() + str.length() - 1);
    }

    // Handle the degenerate case:
    if (str.empty())
    {
        str = replaceChar;
        return false;
    }
    else
    {
        // Look for and replace characters that are not allowed:        
        size_t pos = std::string::npos;
        do
        {
            if (allowSpaces)
                pos = str.find_first_of(NOT_ALLOWED);
            else
                pos = str.find_first_of(NOT_ALLOWED_NOSPACE);
            if (pos != std::string::npos)
            {
                str[pos] = replaceChar;
                changed = true;
            }
        }
        while (pos != std::string::npos);
        return !changed;
    }
}
