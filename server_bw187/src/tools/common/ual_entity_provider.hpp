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
 *	This file declares all classes needed to implement the asset locator's
 *	entity providers.
 */


#ifndef __UAL_ENTITY_PROVIDER_HPP__
#define __UAL_ENTITY_PROVIDER_HPP__

#include "ual/ual_dialog.hpp"
#include <map>


///////////////////////////////////////////////////////////////////////////////
//	Entity UAL VFolder provider classes
///////////////////////////////////////////////////////////////////////////////

/**
 *	This class implements the entity VFolder provider
 */
class EntityVFolderProvider : public VFolderProvider
{
public:
	explicit EntityVFolderProvider( const std::string& thumb );
	~EntityVFolderProvider();
	bool startEnumChildren( const VFolderItemDataPtr parent );
	VFolderItemDataPtr getNextChild( CImage& img );
	void getThumbnail( VFolderItemDataPtr data, CImage& img );
	const std::string getDescriptiveText( VFolderItemDataPtr data, int numItems, bool finished );
	bool getListProviderInfo(
		VFolderItemDataPtr data,
		std::string& retInitIdString,
		ListProvider*& retListProvider,
		bool& retItemClicked );

	// overriden member, to copy the listProvider pointer to a smart pointer
	// that can take care of deleting it on destruction. 'listProvider' must
	// be dynamically allocated because of this.
	virtual void setListProvider( ListProvider* listProvider );

private:
	int index_;
	ListProviderPtr listProviderHolder_;
	std::string thumb_;
	CImage img_;
};


///////////////////////////////////////////////////////////////////////////////
//	Entity UAL List provider classes
///////////////////////////////////////////////////////////////////////////////

/**
 *	This class implements the entity list provider
 */
class EntityListProvider : public ListProvider
{
public:
	explicit EntityListProvider( const std::string& thumb );

	void refresh();
	bool finished();
	int getNumItems();
	const AssetInfo getAssetInfo( int index );
	void getThumbnail( int index, CImage& img, int w, int h, ThumbnailUpdater* updater );
	void filterItems();

private:
	std::vector<AssetInfo> searchResults_;
	std::string thumb_;
	CImage img_;
};


///////////////////////////////////////////////////////////////////////////////
//	Entity UAL loader classes
///////////////////////////////////////////////////////////////////////////////

/**
 *	This class implements the entity loader class, that creates the VFolder and
 *	list providers from a given section of the UAL's config file.
 */
class UalEntityVFolderLoader : public UalVFolderLoader
{
public:
	bool test( const std::string& sectionName )
		{ return sectionName == "Entities"; }
	VFolderPtr load( UalDialog* dlg,
		DataSectionPtr section, VFolderPtr parent, DataSectionPtr customData,
		bool addToFolderTree );
};


#endif // __UAL_ENTITY_PROVIDER_HPP__
