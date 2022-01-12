/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "server/bwconfig.hpp"

#include "cstdmf/debug.hpp"
#include "resmgr/bwresource.hpp"

#include <sys/types.h>
#include <pwd.h>

DECLARE_DEBUG_COMPONENT( 0 )

namespace BWConfig
{

int shouldDebug = 0;
std::string badFilename;

Container g_sections;

bool g_inited = false;

bool isBad()
{
	if (!badFilename.empty())
	{
		BWResource::openSection( badFilename );
		ERROR_MSG( "Could not open configuration file %s\n",
				badFilename.c_str() );
		return true;
	}

	return false;
}

void init()
{
	const char * BASE_FILENAME = "server/bw.xml";

	g_inited = true;
	g_sections.clear();
	std::string filename = BASE_FILENAME;

	// Attempt to find a personolised configuration file first. This is of the
	// form "bw_<username>.xml".
	bool isUserFilename = false;
	struct passwd * pPasswordEntry = ::getpwuid( ::getUserId() );

	if (pPasswordEntry && (pPasswordEntry->pw_name != NULL))
	{
		filename = std::string( "server/bw_" ) +
			pPasswordEntry->pw_name + ".xml";
		isUserFilename = true;
	}

	while (!filename.empty())
	{
		// This also handles reload.
		BWResource::instance().purge( filename );
		DataSectionPtr pDS = BWResource::openSection( filename );

		if (pDS)
		{
			INFO_MSG( "BWConfig::init: Adding %s\n", filename.c_str() );
			g_sections.push_back( std::make_pair( pDS, filename ) );
			filename = pDS->readString( "parentFile" );
		}
		else if (isUserFilename)
		{
			filename = "server/bw.xml";
		}
		else
		{
			ERROR_MSG( "Could not find %s\n", filename.c_str() );
			badFilename = filename;
			filename = "";
		}

		isUserFilename = false;
	}

	update( "debugConfigOptions", shouldDebug );

	DebugFilter::instance().filterThreshold(
		BWConfig::get( "outputFilterThreshold",
			DebugFilter::instance().filterThreshold() ) );

	DebugFilter::instance().hasDevelopmentAssertions(
		BWConfig::get( "hasDevelopmentAssertions",
			DebugFilter::instance().hasDevelopmentAssertions() ) );


}

bool processCommandLine( int argc, char * argv[] )
{
	if (!g_sections.empty())
	{
		DataSectionPtr pDS = g_sections.front().first;

		for (int i = 0; i < argc - 1; ++i)
		{
			if (argv[i][0] == '+')
			{
				pDS->writeString( argv[i]+1, argv[ i+1 ] );
				INFO_MSG( "BWConfig::init: "
						"Setting '%s' to '%s' from command line.\n",
					argv[i]+1, argv[ i+1 ] );

				++i;
			}
		}
	}
	else
	{
		ERROR_MSG( "BWConfig::init: No configuration section.\n" );
		return false;
	}

	return true;
}


bool init( int argc, char * argv[] )
{
	if (!g_inited)
	{
		init();
	}
	else
	{
		WARNING_MSG( "BWConfig::init: "
				"Already initialised before parse args.\n" );
	}

	return processCommandLine( argc, argv );
}



bool hijack( Container & container, int argc, char * argv[] )
{
	g_inited = true;
	badFilename.clear();
	g_sections.swap( container );
	processCommandLine( argc, argv );
	return true;
}


DataSectionPtr getSection( const char * path, DataSectionPtr defaultValue )
{
	if (!g_inited)
	{
		init();
	}

	Container::iterator iter = g_sections.begin();

	while (iter != g_sections.end())
	{
		DataSectionPtr pResult = iter->first->openSection( path );

		if (pResult)
		{
			if (shouldDebug > 1)
			{
				DEBUG_MSG( "BWConfig::getSection: %s = %s (from %s)\n",
					path, pResult->asString().c_str(),
					iter->second.c_str() );
			}

			return pResult;
		}

		++iter;
	}

	if (shouldDebug > 1)
	{
		DEBUG_MSG( "BWConfig::getSection: %s was not set.\n", path );
	}

	return defaultValue;
}

void getChildrenNames( std::vector< std::string >& childrenNames,
		const char * parentPath )
{
	for ( Container::iterator iter = g_sections.begin();
			iter != g_sections.end(); ++iter )
	{
		DataSectionPtr pParent = iter->first->openSection( parentPath );

		if (pParent)
		{
			int numChildren = pParent->countChildren();
			for ( int i = 0; i < numChildren; ++i )
			{
				childrenNames.push_back( pParent->childSectionName( i ) );
			}
		}
	}
}

std::string get( const char * path, const char * defaultValue )
{
	std::string defaultValueStr( defaultValue );
	return get( path, defaultValueStr );
}


void prettyPrint( const char * path,
		const std::string & oldValue, const std::string & newValue,
		const char * typeStr, bool isHit )
{
#ifndef _WIN32
	const char * sectionName = basename( path );
#else
	const char * sectionName = path;
	const char * pCurr = path;
	while (*pCurr != '\0')
	{
		if (*pCurr == '/')
		{
			sectionName = pCurr + 1;
		}
		++pCurr;
	}
#endif

	std::string sorting = (sectionName == path) ? "aatoplevel/" : "";
	sorting += path;

	std::string changedValue = std::string( " " ) + newValue;

	if (oldValue == newValue)
	{
		changedValue = "";
	}

	DEBUG_MSG( "BWConfig: %-40s <%s> %s </%s> "
				"<!-- Type: %s%s%s -->\n",
			sorting.c_str(),
			sectionName,
			oldValue.c_str(),
			sectionName,
			typeStr,
			changedValue.c_str(),
			isHit ? "" : " NOT READ" );
}

} // namespace BWConfig

// bwconfig.cpp
