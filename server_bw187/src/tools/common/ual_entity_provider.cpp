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
 *	This file implements all classes needed to implement the asset locator's
 *	entity providers.
 */


#include "pch.hpp"
#include "resmgr/bwresource.hpp"
#include "ual_entity_provider.hpp"
#include "entitydef/entity_description_map.hpp"


DECLARE_DEBUG_COMPONENT( 0 )

int UalEntityProv_token;


///////////////////////////////////////////////////////////////////////////////
// Section: UalEntityMap
///////////////////////////////////////////////////////////////////////////////
/**
 *	Helper class to keep one single entity map for the entity providers
 */
class UalEntityMap
{
public:
	static UalEntityMap* instance()
	{
		static UalEntityMap instance_;
		return &instance_;
	}

	static int size()
	{
		return (int)instance()->entities_.size();
	}

	/**
	 *	Fills an AssetInfo structure with the entity at 'index'
	 */
	static AssetInfo assetInfo( int index )
	{
		if ( index < 0 || index >= (int)size() )
			return AssetInfo();

		// Set the long name to be the .def file in the 'entities/defs' folder.
		// Handling an entity by its def file in WE makes things easier.
		std::string name = instance()->entities_[ index ];
		std::string entityFile = BWResource::resolveFilename(
			"entities/defs/" + name + ".def" );
		
		// UAL uses back-slashes, as windows
		std::replace( entityFile.begin(), entityFile.end(), '/', '\\' );

		return AssetInfo( "ENTITY", name, entityFile );
	}

	static const int ENTITY_NOT_FOUND = -1;

	static int find( const std::string& entity )
	{
		for ( int i = 0; i < (int)instance()->entities_.size(); ++i )
		{
			if ( _stricmp( instance()->entities_[ i ].c_str(), entity.c_str() ) == 0 )
				return i;
		}
		return ENTITY_NOT_FOUND;
	}

private:
	EntityDescriptionMap map_;
	std::vector<std::string> entities_;

	static bool s_comparator( const std::string& a, const std::string& b )
	{
		return _stricmp( a.c_str(), b.c_str() ) <= 0;
	}

	UalEntityMap()
	{
		// Init the map when the instance is created.
		map_.parse( BWResource::openSection( "entities/entities.xml" ) );

		if ( map_.size() > 0 )
		{
			entities_.reserve( map_.size() );
			for ( int i = 0; i < map_.size(); i++ )
			{
				const EntityDescription& desc =
					map_.entityDescription( i );

				if ( !desc.name().empty() )
				{
					// TODO: This is necessary because EntityDescriptionMap::parse
					// creates empty entries when it fails to load an entity's
					// def file. This should be fixed in EntityDescriptionMap.
					entities_.push_back( desc.name() );
				}
			}

			std::sort< std::vector<std::string>::iterator >(
				entities_.begin(), entities_.end(), s_comparator );
		}
	}
};


///////////////////////////////////////////////////////////////////////////////
// Section: EntityThumbProv
///////////////////////////////////////////////////////////////////////////////
/**
 *	Entity thumbnail provider. Needed for entities in the history or favourites
 *	lists.
 */
class EntityThumbProv : public ThumbnailProvider
{
public:
	bool isValid( const std::string& file )
	{
		if ( BWResource::getExtension( file ) != "def" )
			return false;

		std::string entityName =
			BWResource::removeExtension( BWResource::getFilename( file ) );
		return UalEntityMap::find( entityName ) != UalEntityMap::ENTITY_NOT_FOUND;
	}

	bool needsCreate( const std::string& file, std::string& thumb, int& size )
	{
		thumb = imageFile_;
		return false;
	}

	bool prepare( const std::string& file )
	{
		// should never get called
		MF_ASSERT( false );
		return false;
	}

	bool render( const std::string& file, Moo::RenderTarget* rt  )
	{
		// should never get called
		MF_ASSERT( false );
		return false;
	}

	static void imageFile( const std::string& file ) { imageFile_ = file; }

private:
	static std::string imageFile_;

	DECLARE_THUMBNAIL_PROVIDER()

};
IMPLEMENT_THUMBNAIL_PROVIDER( EntityThumbProv )
std::string EntityThumbProv::imageFile_;



///////////////////////////////////////////////////////////////////////////////
// Section: EntityVFolderProvider
///////////////////////////////////////////////////////////////////////////////

EntityVFolderProvider::EntityVFolderProvider( const std::string& thumb ) :
	index_( 0 ),
	thumb_( thumb )
{
	// Make sure the entity map gets initialised in the main thread. If we
	// don't do this, the first call to UalEntityMap::instance() could come
	// directly from the thumbnail thread, and UalEntityMap uses python.
	UalEntityMap::instance();
}

EntityVFolderProvider::~EntityVFolderProvider()
{
}

bool EntityVFolderProvider::startEnumChildren( const VFolderItemDataPtr parent )
{
	index_ = 0;
	return UalEntityMap::size() > 0;
}


VFolderItemDataPtr EntityVFolderProvider::getNextChild( CImage& img )
{
	if ( index_ >= (int)UalEntityMap::size() )
		return NULL;

	AssetInfo info = UalEntityMap::assetInfo( index_ );

	if ( info.text().empty() || info.longText().empty() )
		return NULL;

	VFolderItemDataPtr newItem = new VFolderItemData(
		this, info, VFolderProvider::GROUP_ITEM, false );

	getThumbnail( newItem, img );

	index_++;

	return newItem;
}


void EntityVFolderProvider::getThumbnail( VFolderItemDataPtr data, CImage& img )
{
	if ( !data )
		return;

	if ( img_.IsNull() )
	{
		// The image has not been cached yet, so load it into the img_ cache
		// member. Note the 'loadDirectly' param set to true, to load the image
		// directly. This will be done only once, the first time it's
		// requested.
		ThumbnailManager::instance()->create(
			BWResource::getFilePath( UalManager::instance()->getConfigFile() ) + thumb_,
			img_, 16, 16, NULL, true/*loadDirectly*/ );
	}
	// blit the cached image to the return image.
	img.Create( 16, 16, 32 );
	CDC* pDC = CDC::FromHandle( img.GetDC() );
	img_.BitBlt( pDC->m_hDC, 0, 0 );
	img.ReleaseDC();
}


const std::string EntityVFolderProvider::getDescriptiveText( VFolderItemDataPtr data, int numItems, bool finished )
{
	if ( !data )
		return "";

	if ( data->isVFolder() )
	{
		// it's the root entity VFolder, so build the appropriate text.
		char num[100];
		sprintf( num, "%d", UalEntityMap::size() );
		return std::string( "Entities (" ) + num + " items)";
	}
	else
	{
		// it's an item ( entity ), so return it's editor file.
		return data->assetInfo().longText();
	}
}


bool EntityVFolderProvider::getListProviderInfo(
	VFolderItemDataPtr data,
	std::string& retInitIdString,
	ListProvider*& retListProvider,
	bool& retItemClicked )
{
	if ( !data )
		return false;

	retItemClicked = !data->isVFolder();

	retInitIdString = "";

	if ( listProvider_ )
	{
		// filter the list provider to force a refresh.
		listProvider_->setFilterHolder( filterHolder_ ); 
		listProvider_->refresh();
	}

	retListProvider = listProvider_;

	return true;
}


/**
 *	Overriden member, to copy the listProvider pointer to a smart pointer
 *	that can take care of deleting it on destruction. 'listProvider' must
 *	be dynamically allocated because of this.
 */
void EntityVFolderProvider::setListProvider( ListProvider* listProvider )
{
	VFolderProvider::setListProvider( listProvider );
	// The listProviderHolder_ smartpointer will take care of destructing it.
	listProviderHolder_ = listProvider;
}


///////////////////////////////////////////////////////////////////////////////
// Section: EntityListProvider
///////////////////////////////////////////////////////////////////////////////
EntityListProvider::EntityListProvider( const std::string& thumb ) :
	thumb_( thumb )
{
}

void EntityListProvider::refresh()
{
	filterItems();
}


bool EntityListProvider::finished()
{
	return true; // it's not asyncronous
}


int EntityListProvider::getNumItems()
{
	return (int)searchResults_.size();
}


const AssetInfo EntityListProvider::getAssetInfo( int index )
{
	if ( index < 0 || getNumItems() <= index )
		return AssetInfo();

	return searchResults_[ index ];
}


void EntityListProvider::getThumbnail( int index, CImage& img, int w, int h, ThumbnailUpdater* updater )
{
	if ( index < 0 || getNumItems() <= index )
		return;

	if ( img_.IsNull() || img_.GetWidth() != w || img_.GetHeight() != h )
	{
		// The image has not been cached yet, so load it into the img_ cache
		// member. Note the 'loadDirectly' param set to true, to load the image
		// directly. This will be done only once, the first time it's
		// requested.
		ThumbnailManager::instance()->create(
			BWResource::getFilePath( UalManager::instance()->getConfigFile() ) + thumb_,
			img_, w, h, NULL, true/*loadDirectly*/ );
	}
	// blit the cached image to the return image
	if ( !img_.IsNull() )
	{
		img.Create( w, h, 32 );
		CDC* pDC = CDC::FromHandle( img.GetDC() );
		img_.BitBlt( pDC->m_hDC, 0, 0 );
		img.ReleaseDC();
	}
}


void EntityListProvider::filterItems()
{
	searchResults_.clear();

	if ( UalEntityMap::size() == 0 )
		return;

	searchResults_.reserve( UalEntityMap::size() );

	// fill the results vector with the filtered entities from the Entity map
	for( int i = 0; i < (int)UalEntityMap::size(); ++i )
	{
		AssetInfo info = UalEntityMap::assetInfo( i );
		if ( filterHolder_ && filterHolder_->filter( info.text(), info.longText() ) )
		{
			searchResults_.push_back( info );
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
// Section: UalEntityVFolderLoader
///////////////////////////////////////////////////////////////////////////////

/**
 *	Entity loader class 'load' method.
 */
VFolderPtr UalEntityVFolderLoader::load(
	UalDialog* dlg, DataSectionPtr section, VFolderPtr parent, DataSectionPtr customData,
	bool addToFolderTree )
{
	if ( !dlg || !section || !test( section->sectionName() ) )
		return NULL;

	beginLoad( dlg, section, customData, 2/*big icon*/ );

	bool showItems = section->readBool( "showItems", false );
	std::string thumb = section->readString( "thumbnail", "" );
	
	// Set the entity thumbnail provider's image file name
	EntityThumbProv::imageFile(
		BWResource::getFilePath(
			UalManager::instance()->getConfigFile() ) +
		thumb );

	// create VFolder and List providers, specifying the thumbnail to use
	EntityVFolderProvider* prov = new EntityVFolderProvider( thumb );
	prov->setListProvider( new EntityListProvider( thumb ) );

	VFolderPtr ret = endLoad( dlg,
		prov,
		parent, showItems, addToFolderTree );
	ret->setSortSubFolders( true );
	return ret;
}
static UalVFolderLoaderFactory entityFactory( new UalEntityVFolderLoader() );
