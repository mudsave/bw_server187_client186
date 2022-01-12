/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "expsets.hpp"
#include "resmgr/dataresource.hpp"

bool ExportSettings::readSettings( const char* filename )
{
	DataResource file( filename, RESOURCE_TYPE_XML );

	DataSectionPtr pRoot = file.getRootSection( );

	if( pRoot )
	{		
		referenceNodesFile_ = pRoot->readString( "referenceNodesFile", referenceNodesFile_ );
		allowScale_ = pRoot->readBool( "allowScale", allowScale_ );
		exportMorphAnimation_ = pRoot->readBool( "exportMorphAnimation", exportMorphAnimation_ );
		exportNodeAnimation_ = pRoot->readBool( "exportNodeAnimation", exportNodeAnimation_ );
		return true;
	}
	return false;
}

bool ExportSettings::writeSettings( std::string fileName )
{
	DataResource file( fileName, RESOURCE_TYPE_XML );

	DataSectionPtr pRoot = file.getRootSection( );


	if( pRoot )
	{		
		pRoot->writeString( "referenceNodesFile", referenceNodesFile_ );
		pRoot->writeBool( "allowScale", allowScale_ );
		pRoot->writeBool( "exportMorphAnimation", exportMorphAnimation_ );
		pRoot->writeBool( "exportNodeAnimation", exportNodeAnimation_ );

		file.save();
		return true;
	}
	return false;
}

// expsetsio.cpp
