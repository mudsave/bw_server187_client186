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

#include "mru.hpp"

#include "appmgr/options.hpp"

/*static*/ MRU& MRU::instance()
{
	static MRU s_instance_;
	return s_instance_;
}

MRU::MRU():
	maxMRUs_(10)
{}

void MRU::update( const std::string& mruName, const std::string& file, bool add /* = true */ )
{
	std::vector<std::string> files;
	if (add)
	{
		files.push_back( file );
	}

	DataSectionPtr mruList = Options::pRoot()->openSection( mruName, true );

	char entry[8];
	std::string tempStr;
			
	for (unsigned i=0; i<maxMRUs_; i++)
	{
		sprintf( entry, "file%d", i );
		std::string tempStr = mruList->readString( entry, file );
		if ( tempStr != file )
		{
			files.push_back( tempStr );
		}
	}

	mruList->delChildren();

	for (unsigned i=0; i<min( maxMRUs_, files.size() ); i++)
	{
		sprintf( entry, "file%d", i );
		mruList->writeString( entry, files[i] );
	}

	Options::save();
}

void MRU::read( const std::string& mruName, std::vector<std::string>& files )
{
	DataSectionPtr mruList = Options::pRoot()->openSection( mruName, true );

	char entry[8];
	std::string tempStr;
			
	for (unsigned i=0; i<maxMRUs_; i++)
	{
		sprintf( entry, "file%d", i );
		std::string tempStr = mruList->readString( entry, "" );
		if ( tempStr != "" )
		{
			files.push_back( tempStr );
		}
	}
}

void MRU::getDir( const std::string& mruName, std::string& dir, const std::string& defaultDir /* = "" */ )
{
	dir = defaultDir;
	
	DataSectionPtr mruList = Options::pRoot()->openSection( mruName, false );

	if (mruList)
	{
		dir = mruList->readString( "file0", defaultDir );

		dir = BWResource::resolveFilename( dir );

		std::replace( dir.begin(), dir.end(), '/', '\\' );
	}
}