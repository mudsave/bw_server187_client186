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

bool ExportSettings::readSettings( const std::string &fileName )
{
	DataResource file( fileName, RESOURCE_TYPE_XML );

	DataSectionPtr pRoot = file.getRootSection( );

	if( pRoot )
	{
		unitScale_ = pRoot->readFloat( "unitScale", unitScale_ );
		exportMode_ = (ExportMode)pRoot->readInt( "exportMode", exportMode_ );
		allowScale_ = pRoot->readBool( "allowScale", allowScale_ );
		bumpMapped_ = pRoot->readBool( "bumpMapped", bumpMapped_ );
		keepExistingMaterials_ = pRoot->readBool( "keepExistingMaterials",
			keepExistingMaterials_ );
		boneCount_ = pRoot->readInt( "boneCount", boneCount_ );
		return true;
	}
	return false;
}

bool ExportSettings::writeSettings( const std::string &fileName )
{
	DataResource file( fileName, RESOURCE_TYPE_XML );

	DataSectionPtr pRoot = file.getRootSection( );


	if( pRoot )
	{
		pRoot->writeInt( "exportMode", exportMode_ );
		pRoot->writeBool( "allowScale", allowScale_ );
		pRoot->writeBool( "bumpMapped", bumpMapped_ );
		pRoot->writeBool( "keepExistingMaterials", keepExistingMaterials_ );
		pRoot->writeFloat( "unitScale", unitScale_ );
		pRoot->writeInt( "boneCount", boneCount_ );
		file.save();
		return true;
	}
	return false;
}

// expsetsio.cpp
