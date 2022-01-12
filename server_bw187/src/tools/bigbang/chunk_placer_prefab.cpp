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

#include "chunk_placer.hpp"

#include "big_bang.hpp"
#include "item_view.hpp"
#include "snaps.hpp"
#include "chunk_item_placer.hpp"
#include "editor_chunk_vlo.hpp"

#include "chunks/editor_chunk.hpp"
#include "chunk/chunk_stationnode.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_space.hpp"

#include "gizmo/tool_locator.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/xml_section.hpp"
#include "resmgr/bin_section.hpp"


DECLARE_DEBUG_COMPONENT2( "Editor", 0 )


// -----------------------------------------------------------------------------
// Section: ChunkPlacer
// 
// This file implements the prefab related functionality of ChunkPlacer
//
// -----------------------------------------------------------------------------


PY_MODULE_STATIC_METHOD( ChunkPlacer, saveChunkPrefab, BigBang )
PY_MODULE_STATIC_METHOD( ChunkPlacer, loadChunkPrefab, BigBang )

/**
 *	Return the DataSectionPtr for the given filename, creating it's directory
 *	if needed.
 */
static DataSectionPtr openOrCreate( const std::string& fileName )
{
	std::string dirName = BWResource::getFilePath( fileName );

	DataSectionPtr dir = BWResource::openSection( dirName, true );
	if (!dir)
	{
		ERROR_MSG( "openOrCreate() - Couldn't open dir %s\n",
			dirName.c_str() );
		return NULL;
	}

	DataSectionPtr ds = dir->newSection( BWResource::getFilename( fileName ) );
	if (!ds)
	{
		ERROR_MSG( "openOrCreate() - Couldn't create file\n" );
		return NULL;
	}

	return ds;
}

/**
 *	Calculate world space average position of the given items
 */
static Vector3 calculateAverageOrigin( const ChunkItemRevealer::ChunkItems& items )
{
	Vector3 averagePosition = Vector3::zero();
	for (ChunkItemRevealer::ChunkItems::const_iterator i = items.begin();
			i != items.end();
			++i )
	{
		Chunk* chunk = (*i)->chunk();

		if (!chunk->online())
			continue;

		averagePosition += chunk->transform().applyPoint(
			(*i)->edTransform().applyToOrigin() );
	}
	averagePosition /= (float) items.size();

	return averagePosition;
}

/**
 * Calculate a bounding box encompasing all the items
 */
static BoundingBox calculateBoundingBox( const ChunkItemRevealer::ChunkItems& items )
{
	BoundingBox bb = BoundingBox::s_insideOut_;

	for (ChunkItemRevealer::ChunkItems::const_iterator i = items.begin();
			i != items.end();
			++i )
	{
		ChunkItemPtr item = *i;

		if (item->isShellModel())
		{
			bb.addBounds( item->chunk()->boundingBox() );
		}
		else
		{
			BoundingBox itembb;
			item->edBounds( itembb );

			itembb.transformBy( item->edTransform() );
			itembb.transformBy( item->chunk()->transform() );

			bb.addBounds( itembb );
		}
	}

	return bb;
}

static void calculateSnaps( const ChunkItemRevealer::ChunkItems& items, Vector3& positionSnaps, float& angleSnaps )
{
	positionSnaps = Vector3::zero();
	angleSnaps = 0.f;

	for (ChunkItemRevealer::ChunkItems::const_iterator i = items.begin();
			i != items.end();
			++i )
	{
		angleSnaps = Snap::satisfy( angleSnaps, (*i)->edAngleSnaps() );
		Vector3 m = (*i)->edMovementDeltaSnaps();

		positionSnaps.x = Snap::satisfy( positionSnaps.x, m.x );
		positionSnaps.y = Snap::satisfy( positionSnaps.y, m.y );
		positionSnaps.z = Snap::satisfy( positionSnaps.z, m.z );
	}
}

static bool savePrefab( const std::string& prefabName, ChunkItemRevealer::ChunkItems& items )
{
	// Create a new file in the models dir
	DataSectionPtr rootDS = openOrCreate( prefabName );
	if (!rootDS)
	{
		ERROR_MSG( "savePrefab() - Couldn't create file\n" );
		return false;
	}
	std::string binFileName = BWResource::changeExtension( prefabName,
		".pdata" );
	DataSectionPtr binDS = new BinSection(
		BWResource::getFilename( binFileName ),
		new BinaryBlock( NULL, 0 ) );

	binDS->setParent( BWResource::openSection(
		BWResource::getFilePath( binFileName ) ) );


	DataSectionPtr chunkDS = rootDS->newSection( "chunks" );
	DataSectionPtr itemDS = rootDS->newSection( "items" );
	if (!chunkDS || !itemDS)
	{
		ERROR_MSG( "savePrefab() - Couldn't create sub sections\n" );
		return false;
	}

	Vector3 positionSnaps;
	float angleSnaps;
	calculateSnaps( items, positionSnaps, angleSnaps );

	// Centre all the items to be around the origin
	Vector3 averageOrigin = calculateAverageOrigin( items );
	Snap::vector3( averageOrigin, positionSnaps );
	Vector3 offsetToOrigin = averageOrigin * -1.f;

	int chunkNumber = 0;
	int itemNumber = 0;
	for (ChunkItemRevealer::ChunkItems::iterator i = items.begin();
			i != items.end();
			++i )
	{
		ChunkItemPtr item = *i;
		Chunk* chunk = item->chunk();

		if (!chunk->online())
		{
			WARNING_MSG( "Chunk isn't online, can't save as prefab\n" );
			continue;
		}

		if (item->isShellModel())
		{
			// Write the entire chunk out
			char chunkNumberStr[10];
			sprintf( chunkNumberStr, "%d", chunkNumber );
			std::string chunkName( std::string( chunkNumberStr ) + ".chunk" );
			DataSectionPtr pDS = chunkDS->newSection( chunkName );
			if (!pDS)
			{
				ERROR_MSG( "savePrefab() - Couldn't create new section\n" );
				continue;
			}

			ChunkPlacer::utilCloneChunkDataSection( chunk, pDS, chunkName );

			pDS->deleteSections( "navmesh" );

			// boof the positions of the slected chunks be around the origin
			Matrix chunkTransform = pDS->readMatrix34( "transform" );
			chunkTransform.postTranslateBy( offsetToOrigin );
			pDS->writeMatrix34( "transform", chunkTransform );

			// boof the bounding box too!
			Vector3 bbMin = pDS->readVector3( "boundingBox/min" );
			bbMin += offsetToOrigin;
			pDS->writeVector3( "boundingBox/min", bbMin );

			Vector3 bbMax = pDS->readVector3( "boundingBox/max" );
			bbMax += offsetToOrigin;
			pDS->writeVector3( "boundingBox/max", bbMax );


			chunkNumber++;
			if (chunkNumber >= 1000000)
			{
				// too many
				ERROR_MSG( "savePrefab(): Too many chunks in prefab, max is 1000000\n" );
				return false;
			}

		}
		else
		{
			// Write the item's datasection out
			DataSectionPtr origDS = item->pOwnSect();
			DataSectionPtr pDS = itemDS->newSection( origDS->sectionName() );
			if (!pDS)
			{
				ERROR_MSG( "savePrefab() - Couldn't create new section\n" );
				continue;
			}

			pDS->copy( origDS );

			if ( origDS->sectionName() == "vlo" )
			{
				pDS->delChild("uid");
				EditorChunkVLO* vloChunkItem = reinterpret_cast<EditorChunkVLO*>(item.getObject());
				if (vloChunkItem)
				{
					pDS->copySections( vloChunkItem->object()->section() );
					std::string vloType = pDS->readString("type","");
					if (vloType != "")
					{
						DataSectionPtr vloDS = pDS->openSection(vloType);
						if (vloDS)
							vloDS->delChild("position");
					}
				}
			}

			// Add its location so we can place in back correctly
			Matrix m;
			m.multiply( item->edTransform(), chunk->transform() );
			m.postTranslateBy( offsetToOrigin );
			pDS->writeMatrix34( "prefabTransform", m );

			// Get the binary data for the item
			BinaryPtr bp = item->edExportBinaryData();
			if (bp)
			{
				char itemNumberStr[16];
				sprintf( itemNumberStr, "item%02d", itemNumber++ );

				binDS->writeBinary( itemNumberStr, bp );
				pDS->writeString( "prefabBin", itemNumberStr );
			}
		}

	}

	BoundingBox bb = calculateBoundingBox( items );
	
	rootDS->writeVector3( "boundingBox/min", bb.minBounds() + offsetToOrigin );
	rootDS->writeVector3( "boundingBox/max", bb.maxBounds() + offsetToOrigin );

	rootDS->writeVector3( "snaps/position", positionSnaps );
	rootDS->writeFloat( "snaps/angle", angleSnaps );

	BWResource::instance().purge( binFileName );

	rootDS->save();
	if ( binDS->countChildren() > 0 )
		binDS->save();
	else
		::DeleteFile( binFileName.c_str() );

	return true;
}

/**
 * Return s, ignoring all characters not in validCharacters
 *
 * All characters are lowercased, thus validCharacters must be all lowercase
 */
static std::string filterString( const std::string& s, const std::string& validCharacters )
{
	std::string r;
	for (int i = 0; i < (int) s.length(); i++)
	{
		char testChar = s[i];
		testChar = tolower(testChar);
		if (validCharacters.find(testChar) != -1)
		{
			r += testChar;
		}
	}

	return r;
}



PyObject* ChunkPlacer::py_saveChunkPrefab( PyObject * args )
{
	// get args
	PyObject * pPyRev;
	char * name;
	if (!PyArg_ParseTuple( args, "Os", &pPyRev, &name ) ||
		!ChunkItemRevealer::Check( pPyRev ))
	{
		PyErr_SetString( PyExc_ValueError, "BigBang.saveChunkPrefab() "
			"expects a ChunkItemRevealer and a filename" );
		return NULL;
	}
	
	ChunkItemRevealer* pRevealer = static_cast<ChunkItemRevealer*>( pPyRev );
	ChunkItemRevealer::ChunkItems items;
	pRevealer->reveal( items );

	// Using this rather than the below section as the user is being prompted
	// with a save dialog these days rather than a simple text entry
	std::string prefabName = BWResource::dissolveFilename( name );
	if (BWResource::getExtension( name ).empty())
		prefabName += ".prefab";

	// check for valid characters and add extension
	/*
	std::string prefabFilename = filterString( name, "abcdefghijklmnopqrstuvwxyz1234567890_-" );

	if ( (prefabFilename.length() <= 7) ||
		(prefabFilename.substr(prefabFilename.length() - 7) != ".prefab") )
		prefabFilename += ".prefab";

	// find a directory and name for the prefab
	std::string prefabName = "";
	{
		ChunkItemPtr pItem = *items.begin();
		Chunk* pChunk = pItem->chunk();
		DataSectionPtr pChunkSection = EditorChunkCache::instance( *pChunk ).pChunkSection();
		std::string modelName = pItem->pOwnSect()->readString( "resource" );
		MF_ASSERT( !modelName.empty() );

		std::string modelDir = BWResource::getFilePath( modelName );

		// strip back to the shell folder and change to prefab
		size_t shellIndex = modelName.find( "shells" );
		size_t postShellIndex = shellIndex + 7;
		size_t extensionIndex = modelName.find( ".model" );
		if (shellIndex == std::string::npos)
			return NULL;
		if (postShellIndex == std::string::npos)
			return NULL;
		if (extensionIndex == std::string::npos)
			return NULL;
		MF_ASSERT(shellIndex < postShellIndex);
		MF_ASSERT(postShellIndex < extensionIndex);

		prefabName = modelName.substr(0, shellIndex);
		prefabName += "prefabs/";

		if (items.size() == 1)
		{
			// change from shell to prefab dir
			prefabName += modelName.substr(postShellIndex, extensionIndex - postShellIndex);
			prefabName += "_";
		}
		else
		{
			prefabName += modelName.substr(postShellIndex, modelName.find( "/", postShellIndex ) + 1 - postShellIndex);
			prefabName += "groups/";
		}

		prefabName += prefabFilename;
	}
	*/

	if (!savePrefab( prefabName, items ))
		return NULL;


	INFO_MSG( "Saved chunk prefab %s\n", prefabName.c_str() );

	std::string returnMessage = "Saved chunk prefab: " + prefabName;
	return Py_BuildValue( "s", returnMessage.c_str() );
}

class IdMapping
{
public:
	UniqueID find(UniqueID oldID, bool realObject)
	{
		UniqueID newID = UniqueID::zero();

		IdMap::iterator it = idMap_.find( oldID );
		if (it != idMap_.end())
		{
			newID = it->second;
		}
		else
		{
			// else make a new id and push onto the map
			newID = UniqueID::generate();
			idMap_.insert( std::pair<UniqueID, UniqueID>( oldID, newID ) );
		}

		MF_ASSERT(newID != UniqueID::zero());

		// update info about the new object
		ObjectMap::iterator i = newObjectMap_.find( newID );
		if (i != newObjectMap_.end())
		{
			if (realObject)
			{
				// can't have two objects of the same id
				MF_ASSERT(i->second.realObject == false);
				i->second.realObject = true;
			}
			else
			{
				i->second.referenced++;
			}
		}
		else
		{
			ObjectInfo inf;
			if (realObject)
				inf.realObject = true;
			else
				inf.referenced = 1;
			
			newObjectMap_.insert(std::pair<UniqueID, ObjectInfo>( newID, inf ));
		}

		return newID;
	}

	bool realObject(UniqueID newID)
	{
		ObjectMap::iterator i = newObjectMap_.find( newID );
		if (i == newObjectMap_.end())
			return false;

		return i->second.realObject;
	}

	int referenced(UniqueID newID)
	{
		ObjectMap::iterator i = newObjectMap_.find( newID );
		if (i == newObjectMap_.end())
			return 0;

		return i->second.referenced;
	}

private:
	typedef std::map<UniqueID, UniqueID> IdMap;
	IdMap idMap_;

	class ObjectInfo
	{
	public:
		ObjectInfo() : realObject(false), referenced(0) {}
		bool realObject;	// is this an id for a real item
		int referenced;		// how many items reference this id
	};
	typedef std::map<UniqueID, ObjectInfo> ObjectMap;
	ObjectMap newObjectMap_;
};

/**
 *	Creates new GUIDs based off the old ones, so new items won't conflict with
 *	something already in the scene.
 */
static void rewriteLinks( DataSectionPtr item, IdMapping& clusterMarkerIDmap,
						IdMapping& stationIDmap, IdMapping& stationGraphIDmap )
{
	if ( (item->sectionName() == "marker") || (item->sectionName() == "marker_cluster") )
	{
		std::string myIdString = item->readString( "id", "" );
		MF_ASSERT(myIdString.length() > 0);
		item->writeString( "id", clusterMarkerIDmap.find( UniqueID(myIdString), true ) );

		if ( item->openSection( "parentID", false ) )
		{
			std::string parentIdString = item->readString( "parentID", "" );
			item->writeString( "parentID", clusterMarkerIDmap.find( UniqueID(parentIdString), false ) );
		}
	}

	if (item->sectionName() == "station")
	{
		std::string myIdString = item->readString( "id", "" );
		MF_ASSERT(myIdString.length() > 0);
		item->writeString( "id", stationIDmap.find( UniqueID(myIdString), true ) );

		std::string graphIdString = item->readString( "graph", "" );
		MF_ASSERT(graphIdString.length() > 0);
		item->writeString( "graph", stationGraphIDmap.find( UniqueID(graphIdString), false ) );

		DataSectionIterator iChildLinks;
		for(iChildLinks = item->begin(); iChildLinks != item->end(); iChildLinks++)
		{
			DataSectionPtr pLinkDS = *iChildLinks;
			if (pLinkDS->sectionName() != "link")
				continue;

			std::string toIdString = pLinkDS->readString( "to", "" );
			MF_ASSERT(toIdString.length() > 0);
			pLinkDS->writeString( "to", stationIDmap.find( UniqueID(toIdString), false ) );
		}
	}
}

/**
 *	Check that all the links are valid, in case the user didn't copy
 *	everything in the graph to the prefab etc
 */
static void verifyLinks( DataSectionPtr item,
						 std::vector<DataSectionPtr>& sectionsToErase,
						 IdMapping& clusterMarkerIDmap,
						 IdMapping& stationIDmap, IdMapping& stationGraphIDmap )
{
	if ( item->sectionName() == "marker_cluster" )
	{
		// check that the reference count is correct, remove if not referenced
		std::string myIdString = item->readString( "id", "" );
		MF_ASSERT(myIdString.length() > 0);
		int references = clusterMarkerIDmap.referenced( UniqueID(myIdString) );
		if (references == 0)
		{
			sectionsToErase.push_back(item);
			return;
		}
		else
		{
			item->writeInt( "number_children", references );
		}
	}

	if ( (item->sectionName() == "marker") || (item->sectionName() == "marker_cluster") )
	{
		// remove links to parents that don't exist
		if ( item->openSection( "parentID", false ) )
		{
			std::string parentIdString = item->readString( "parentID", "" );
			if ( !clusterMarkerIDmap.realObject( UniqueID(parentIdString) ) )
			{
				item->writeString( "parentID", UniqueID::zero() ); 
			}
		}
	}

	if ( item->sectionName() == "station" )
	{
		// remove links to stations that don't exist
		std::vector<DataSectionPtr> linksToErase;
		DataSectionIterator iChildLinks;
		for(iChildLinks = item->begin(); iChildLinks != item->end(); iChildLinks++)
		{
			DataSectionPtr pLinkDS = *iChildLinks;
			if (pLinkDS->sectionName() != "link")
				continue;

			std::string toIdString = pLinkDS->readString( "to", "" );
			MF_ASSERT(toIdString.length() > 0);
			if ( !stationIDmap.realObject( UniqueID(toIdString) ) )
			{
				linksToErase.push_back(pLinkDS);
				continue;
			}
		}

		for (std::vector<DataSectionPtr>::iterator li = linksToErase.begin();
			li != linksToErase.end(); li++)
		{
			item->delChild(*li);
		}
	}
}


/** Find an offset to move bb by to fit it in container */
static Vector3 findBoundingBoxOffset( const BoundingBox& bb, const BoundingBox& container )
{
	Vector3 offset = Vector3::zero();

	Vector3 minDiff = bb.minBounds() - container.minBounds();
	Vector3 maxDiff = bb.maxBounds() - container.maxBounds();

	if (minDiff.x < 0.f)
		offset.x = -minDiff.x;
	else if (maxDiff.x > 0.f)
		offset.x = -maxDiff.x;

	if (minDiff.y < 0.f)
		offset.y = -minDiff.y;
	else if (maxDiff.y > 0.f)
		offset.y = -maxDiff.y;

	if (minDiff.z < 0.f)
		offset.z = -minDiff.z;
	else if (maxDiff.z > 0.f)
		offset.z = -maxDiff.z;

	return offset;
}

/**
 * Snap offset to snapAmount, but only by increasing the magnitude of offset
 */
static float snapOffset( float offset, float snapAmount )
{
	if (snapAmount > 0.f)
	{
		float v = offset;
		Snap::value( v, snapAmount );

		if (offset > 0.f)
		{
			if (v < offset)
				v += snapAmount;
		}
		else if (offset < 0)
		{
			if (v > offset)
				v -= snapAmount;
		}

		return v;

	}
	return offset;
}

// Used by items (ie, EditorChunkTerrain) to load their binary data from.
DataSectionPtr prefabBinarySection;

PyObject* ChunkPlacer::py_loadChunkPrefab( PyObject * args )
{
	// get the arguments
	char * prefabName = 0;
	PyObject * pPyLoc = NULL;
	if (!PyArg_ParseTuple( args, "sO", &prefabName, &pPyLoc) ||
		!ToolLocator::Check(pPyLoc))
	{
		PyErr_SetString( PyExc_TypeError, "BigBang.loadChunkPrefab() "
			"expects a string and a ToolLocator" );
		return NULL;
	}

	// get the data section for it
	DataSectionPtr prefabSect = BWResource::openSection( prefabName );
	if (!prefabSect)
	{
		PyErr_SetString( PyExc_TypeError, "BigBang.loadChunkPrefab() "
			"couldn't open prefab" );
		return NULL;
	}

	BoundingBox prefabBB = BoundingBox(
		prefabSect->readVector3( "boundingBox/min", Vector3( FLT_MAX, FLT_MAX, FLT_MAX ) ),
		prefabSect->readVector3( "boundingBox/max", Vector3( -FLT_MAX, -FLT_MAX, -FLT_MAX ) ) );

	Vector3 positionSnaps = prefabSect->readVector3( "snaps/position" );

	DataSectionPtr prefabChunkSect = prefabSect->openSection( "chunks" );
	DataSectionPtr prefabItemSect = prefabSect->openSection( "items" );

	// If there's no chunk tag, likely it's an old format prefab file with only
	// chunks, use the root section in this case
	if (!prefabChunkSect)
		prefabChunkSect = prefabSect;

	if (prefabItemSect->findChild("terrain"))
	{
		prefabBinarySection = BWResource::openSection(
			BWResource::changeExtension( prefabName, ".pdata" ) );
	}

	// find out where it goes from the locator
	ToolLocator * pLoc = static_cast<ToolLocator*>( pPyLoc );
	Vector3 locatorPosition = pLoc->transform().applyToOrigin();
	Snap::vector3( locatorPosition, positionSnaps );

	if (!BigBang::instance().isPointInWriteableChunk( locatorPosition ))
		Py_Return;

	BoundingBox spaceBB( ChunkManager::instance().cameraSpace()->gridBounds() );

	if (!(prefabBB == BoundingBox::s_insideOut_))
	{
		BoundingBox bb = BoundingBox( prefabBB.minBounds() + locatorPosition - Vector3( 1.f, 1.f, 1.f ),
			prefabBB.maxBounds() + locatorPosition + Vector3( 1.f, 1.f, 1.f ) );

		if ( !(spaceBB.intersects( bb.minBounds() ) &&
				spaceBB.intersects( bb.maxBounds() )) )
		{
			// User hasn't placed the bb in the world, find the offset
			Vector3 offset = findBoundingBoxOffset( bb, spaceBB );

			offset.x = snapOffset( offset.x, positionSnaps.x );
			offset.y = snapOffset( offset.y, positionSnaps.y );
			offset.z = snapOffset( offset.z, positionSnaps.z );

			locatorPosition += offset;
		}
	}

	// Ugly ugly hack! Set the placement position to y=0 if there's a terrain
	// block in the prefab, which we detect by the x snaps = 100. Normal snaps
	// don't express enough to allow us to do this properly, as terrain always
	// sits at y=0, rather than on some multiple
	if (fabsf( positionSnaps.x - 100.f ) < 0.001f)
		locatorPosition.y = 0.f;


	// select the items that are placed
	ChunkItemRevealer::ChunkItems newItems;

	bool success = true;

	// create new data sections
	std::list<DataSectionPtr> newChunkSections;
	std::list<std::string> newChunkSectionNames;
	DataSectionIterator it;
	for (it = prefabChunkSect->begin(); it != prefabChunkSect->end(); ++it)
	{
		std::string newChunkName;
		DataSectionPtr pDS = utilCreateInsideChunkDataSection(
			ChunkManager::instance().cameraSpace()->column( locatorPosition )->pOutsideChunk(),
			newChunkName );
		if (!pDS)
			Py_Return;

		utilCloneChunkDataSection( *it, pDS, newChunkName );

		newChunkSections.push_back( pDS );
		newChunkSectionNames.push_back( newChunkName );
	}

	std::list<DataSectionPtr> newItemSections;
	if (prefabItemSect)
	{
		for (it = prefabItemSect->begin(); it != prefabItemSect->end(); ++it)
		{
			DataSectionPtr pDS = new XMLSection( (*it)->sectionName() );
			if (!pDS)
				Py_Return;

			pDS->copy( *it );

			newItemSections.push_back( pDS );
		}
	}

	// toggle this to keep/remove links
	static const bool keepLinks = true;

	// preprocessing: convert to new values
	IdMapping clusterMarkerIDmap;
	IdMapping stationIDmap;
	IdMapping stationGraphIDmap;
	std::list<DataSectionPtr>::iterator i;
	for (i = newChunkSections.begin(); i != newChunkSections.end(); ++i)
	{
		DataSectionPtr pDS = *i;

		// move it to the locator's position
		Matrix chunkTransform = pDS->readMatrix34( "transform" );
		chunkTransform.postTranslateBy( locatorPosition );
		pDS->writeMatrix34( "transform", chunkTransform );

		// move the bb to the new location
		Vector3 bbMin = pDS->readVector3( "boundingBox/min" );
		bbMin += locatorPosition;
		pDS->writeVector3( "boundingBox/min", bbMin );

		Vector3 bbMax = pDS->readVector3( "boundingBox/max" );
		bbMax += locatorPosition;
		pDS->writeVector3( "boundingBox/max", bbMax );


		if (keepLinks)
		{			
			// look inside the chunk prefab
			DataSectionIterator iChild;
			for(iChild = pDS->begin(); iChild != pDS->end(); iChild++)
			{
				DataSectionPtr pChildDS = *iChild;

				rewriteLinks( pChildDS, clusterMarkerIDmap, stationIDmap, stationGraphIDmap );
			}
		}
	}

	// Preprocess the items
	for (i = newItemSections.begin(); i != newItemSections.end(); ++i)
	{
		if (keepLinks)
		{
			rewriteLinks( *i, clusterMarkerIDmap, stationIDmap, stationGraphIDmap );
		}
	}

	if (keepLinks)
	{
		// preprocessing: check links between clusters and markers
		for (i = newChunkSections.begin(); i != newChunkSections.end(); ++i)
		{
			DataSectionPtr pDS = *i;

			// look inside the chunk prefab
			std::vector<DataSectionPtr> sectionsToErase;
			DataSectionIterator iChild;
			for(iChild = pDS->begin(); iChild != pDS->end(); iChild++)
			{
				DataSectionPtr pChildDS = *iChild;

				verifyLinks( pChildDS, sectionsToErase, clusterMarkerIDmap,
					stationIDmap, stationGraphIDmap );
			}

			// erase things not needed
			for (std::vector<DataSectionPtr>::iterator ei = sectionsToErase.begin();
				ei != sectionsToErase.end(); ei++)
			{
				pDS->delChild(*ei);
			}
		}

		std::vector<DataSectionPtr> sectionsToErase;
		for (i = newItemSections.begin(); i != newItemSections.end(); i++)
		{
			verifyLinks( *i, sectionsToErase, clusterMarkerIDmap,
				stationIDmap, stationGraphIDmap );
		}
		for (std::vector<DataSectionPtr>::iterator ei = sectionsToErase.begin();
			ei != sectionsToErase.end(); ei++)
		{
			newItemSections.erase( std::find( newItemSections.begin(),
				newItemSections.end(), *ei ) );
		}

	}

	// load all the chunks in the section, with new chunk names
	std::list<std::string>::iterator ni;
	for (i = newChunkSections.begin(), ni = newChunkSectionNames.begin();
		 i != newChunkSections.end();
		 i++, ni++)
	{
		DataSectionPtr pDS = *i;
		std::string& chunkName = *ni;
		Chunk* pChunk = utilCreateChunk( pDS, chunkName );
		if (!pChunk)
			return NULL;


		// if not in the space -> can't place this chunk
		if ( !(spaceBB.intersects( pChunk->boundingBox().minBounds() ) &&
				spaceBB.intersects( pChunk->boundingBox().maxBounds() )) )
		{
			success = false;
			break;
		}

		ChunkSpace::Column* col = ChunkManager::instance().cameraSpace()->column( pChunk->centre(), false );
		if (!col)
		{
			INFO_MSG( "Couldn't find column at %f, %f, %f\n", pChunk->centre().x,
				pChunk->centre().y, pChunk->centre().z );
			success = false;
			break;
		}

		if (!col->pOutsideChunk())
		{
			INFO_MSG( "Couldn't find outside chunk at %f, %f, %f\n", pChunk->centre().x,
				pChunk->centre().y, pChunk->centre().z );
			success = false;
			break;
		}

		// and tell the cache that it has arrived!
		EditorChunkCache::instance( *pChunk ).edArrive( true );

		// now add an undo which deletes it
		UndoRedo::instance().add(
			new ChunkExistenceOperation( pChunk, false ) );

		// tell the chunk we just cloned it (true = don't give new IDs to markers / stations)
		EditorChunkCache::instance( *pChunk ).edPostClone( keepLinks );

		// select the appropriate shell model
		newItems.push_back( EditorChunkCache::instance(*pChunk).getShellModel() );

		// This is a bit of a hack to enable moving multiple chunks at once
		// Otherwise, the HullTree doesn't get updated, so the portals
		// can't find the chunk they're suppossed to connect to
		// ie, without this moving > 1 chunk at a time will result in
		// unresolved portals
		ChunkManager::instance().camera( Moo::rc().invView(), ChunkManager::instance().cameraSpace() );
	}

	if (success)
	{
		// Place the items
		for (i = newItemSections.begin(); i != newItemSections.end(); i++)
		{
			DataSectionPtr itemPrefabSection = *i;
			bool isTerrain = itemPrefabSection->sectionName() == "terrain";
			bool isVLO = itemPrefabSection->sectionName() == "vlo";
			Matrix itemTransform = itemPrefabSection->readMatrix34( "prefabTransform" );
			itemTransform.postTranslateBy( locatorPosition );

			Chunk* pChunk = NULL;
			if ( !isTerrain )
			{
				pChunk = ChunkManager::instance().cameraSpace()->findChunkFromPoint(
					itemTransform.applyToOrigin() );
			}
			else
			{
				pChunk = EditorChunk::findOutsideChunk( itemTransform.applyToOrigin() );
			}

			if (!pChunk)
			{
				PyErr_SetString( PyExc_ValueError, "BigBang.loadChunkPrefab() "
					"couldn't find chunk to place item." );
				success = false;
				break;
			}

			Matrix localTransform;
			localTransform.multiply( itemTransform, pChunk->transformInverse() );

			DataSectionPtr chunkSection = EditorChunkCache::instance( *pChunk ).pChunkSection();

			//if it is a terrain section, then remove the old one,
			//we don't want more than one copy in the chunk file.
			if ( isTerrain )
			{
				// delete the old chunk terrain entry from xml
				chunkSection->delChild( "terrain" );
				// delete the old chunk terrain item
				EditorChunkTerrain* pEct = static_cast<EditorChunkTerrain*>(
					ChunkTerrainCache::instance( *pChunk ).pTerrain());
				pChunk->delStaticItem( pEct );
				// set up an undo which creates it
				UndoRedo::instance().add(
					new ChunkItemExistenceOperation( pEct, pChunk ) );
			}

			DataSectionPtr newSection = chunkSection->newSection(
					itemPrefabSection->sectionName() );
			newSection->copy( itemPrefabSection );
			newSection->delChild( "prefabTransform" );

			if (newSection->openSection( "transform" ))
				newSection->writeMatrix34( "transform", localTransform );
			if (newSection->openSection( "position" ))
				newSection->writeVector3( "position", localTransform.applyToOrigin() );

			// create the new item
			ChunkItemFactory::Result result = pChunk->loadItem( newSection );
			if ( result && result.onePerChunk() ) 
 			{ 
				// output a warning, and create an undo operation.
 				WARNING_MSG( result.errorString().c_str() ); 
 				UndoRedo::instance().add( 
 					new ChunkItemExistenceOperation( result.item(), pChunk ) ); 
		 	
				// delete the old light from the list of items to select
				ChunkItemRevealer::ChunkItems::iterator oldItem =
					std::find( newItems.begin(), newItems.end(), result.item() );
				if ( oldItem != newItems.end() )
					newItems.erase( oldItem );

				// replace it with the new one
 				result = pChunk->loadItem( newSection ); 
 			}
			if (!result)
			{
				PyErr_SetString( PyExc_ValueError, "BigBang.loadChunkPrefab() "
					"error creating item from given section" );
				success = false;
				break;
			}

			// get the new item out of the chunk
			ChunkItemPtr pItem = result.item();
			if (!pItem)
			{
				PyErr_SetString( PyExc_EnvironmentError, "BigBang.loadChunkPrefab() "
					"Couldn't create Chunk Item" );
				success = false;
				break;
			}

			BigBang::instance().changedChunk( pChunk );


			// ok, everyone's happy then. so add an undo which deletes it
			UndoRedo::instance().add(
				new ChunkItemExistenceOperation( pItem, NULL ) );

			if (!isVLO)
			{
				// Tell pitem we just cloned it from item
				if (keepLinks)
				{
					if ( (pItem->edDescription() != "marker") &&
						(pItem->edDescription() != "marker cluster") &&
						(pItem->edDescription() != "patrol node") )
					{
						pItem->edPostClone( NULL );
					}
				}
				else
				{
					pItem->edPostClone( NULL );
				}
			}

			newItems.push_back( pItem );
		}
	}

	BigBang::instance().setSelection( newItems );

	// set a barrier
	UndoRedo::instance().barrier( "load prefab", false );

	// if no good, undo what was just done
	if ( !success )
	{
		UndoRedo::instance().undo();
		return NULL;
	}

	BoundingBox bb = BoundingBox( prefabBB.minBounds() + locatorPosition - Vector3( 1.f, 1.f, 1.f ),
		prefabBB.maxBounds() + locatorPosition + Vector3( 1.f, 1.f, 1.f ) );
	BigBang::instance().markTerrainShadowsDirty( bb );
	return new ChunkItemGroup( newItems );
}
