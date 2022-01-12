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
#include "item_view.hpp"
#include "big_bang.hpp"
#include "autosnap.hpp"
#include "snaps.hpp"
#include "chunks/editor_chunk.hpp"
#include "appmgr/options.hpp"
#include "gizmo/tool_locator.hpp"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk_manager.hpp"
#include "moo/visual_manager.hpp"
#include "resmgr/bwresource.hpp"


DECLARE_DEBUG_COMPONENT2( "Editor", 0 )


// -----------------------------------------------------------------------------
// Section: ChunkPlacer
// -----------------------------------------------------------------------------

PY_MODULE_STATIC_METHOD( ChunkPlacer, createChunk, BigBang )
PY_MODULE_STATIC_METHOD( ChunkPlacer, cloneAndAutoSnap, BigBang )
//PY_MODULE_STATIC_METHOD( ChunkPlacer, deleteChunk, BigBang )
PY_MODULE_STATIC_METHOD( ChunkPlacer, createInsideChunkName, BigBang )
PY_MODULE_STATIC_METHOD( ChunkPlacer, chunkDataSection, BigBang )
PY_MODULE_STATIC_METHOD( ChunkPlacer, createInsideChunkDataSection, BigBang )
//PY_MODULE_STATIC_METHOD( ChunkPlacer, cloneChunkDataSection, BigBang )
//PY_MODULE_STATIC_METHOD( ChunkPlacer, cloneChunks, BigBang )
PY_MODULE_STATIC_METHOD( ChunkPlacer, recreateChunks, BigBang )
PY_MODULE_STATIC_METHOD( ChunkPlacer, saveChunkTemplate, BigBang )


/**
 * Create and return the Chunk, but don't notify the editor,
 * or setup an undo operation, etc
 *
 * ie, don't expose this to the world at large
 *
 * NULL is returned if anything went wrong
 */
Chunk* ChunkPlacer::utilCreateChunk( DataSectionPtr pDS, const std::string& chunkID, Matrix* m/* = NULL*/ )
{
	std::vector<DataSectionPtr> sbounds;
	pDS->openSections( "boundary", sbounds );
	if (!pDS->openSection( "transform" ) ||
		sbounds.size() < 4)
	{
		PyErr_SetString( PyExc_ValueError, "utilCreateChunk() "
			"expects a data section of a chunk. It needs at least "
			"a chunk name including '.chunk' as section name, "
			"a transform, and 4 boundary sections" );
		return NULL;
	}

	BoundingBox bb( pDS->readVector3( "boundingBox/min" ), pDS->readVector3( "boundingBox/max" ) );
	if( m )
	{
		Matrix transform = pDS->readMatrix34( "transform" );
		transform.invert();
		transform.postMultiply( *m );
		bb.transformBy( transform );
	}
	if( !EditorChunk::outsideChunksWriteable( bb ) )
	{
		PyErr_SetString( PyExc_ValueError, "utilCreateChunk() "
			"The newly created chunk is not inside the editable area" );
		return NULL;
	}

	// make sure that name doesn't already exist
	ChunkSpacePtr pSpace = ChunkManager::instance().cameraSpace();
	if (pSpace->chunks().find( chunkID ) != pSpace->chunks().end())
	{
		PyErr_Format( PyExc_ValueError, "utilCreateChunk() "
			"Chunk %s already known to space", chunkID.c_str() );
		return NULL;
	}

	// ok, here we go then... make the chunk
	Chunk * pChunk = new Chunk( chunkID, BigBang::instance().chunkDirMapping() );

	// ratify it
	pChunk = pSpace->findOrAddChunk( pChunk );

	// load it from this data section
	// TODO: make sure no assumptions about running in OTHER thread!
	pChunk->load( pDS );

	return pChunk;
}

/**
 *	This method allows scripts to create a chunk
 */
PyObject * ChunkPlacer::py_createChunk( PyObject * args )
{
	// get the arguments
	PyObject * pPyDS, * pPyLoc = NULL;
	char * chunkName;
	if (!PyArg_ParseTuple( args, "Os|O", &pPyDS, &chunkName, &pPyLoc) ||
		!PyDataSection::Check(pPyDS) ||
		(pPyLoc && !ToolLocator::Check(pPyLoc)))
	{
		PyErr_SetString( PyExc_TypeError, "BigBang.createChunk() "
			"expects a PyDataSection, the full name of the chunk, and an optional ToolLocator" );
		return NULL;
	}

	// get the data section for it
	DataSectionPtr pSect = static_cast<PyDataSection*>( pPyDS )->pSection();

	// find out where it goes
	Matrix pose = Matrix::identity;
	if (pPyLoc != NULL)
	{
		// we have a locator, check section transform is identity
		Matrix sectPose = pSect->readMatrix34( "transform" );
		bool treq = true;
		for (int i = 0; i < 16; i++)
		{
			treq &= ( ((float*)sectPose)[i] == ((float*)pose)[i] );
		}
		if (!treq)
		{
			PyErr_SetString( PyExc_ValueError, "BigBang.createChunk() "
				"expected data section to have an identity transform "
				"since ToolLocator was given." );
			return NULL;
		}

		// now use the locator's
		ToolLocator * pLoc = static_cast<ToolLocator*>( pPyLoc );
		pose = pLoc->transform();

		// snap the transform
		Vector3 t = pose.applyToOrigin();
		Vector3 snapAmount = Options::getOptionVector3( "shellSnaps/movement",
			Vector3( 1.f, 1.f, 1.f ) );
		Snap::vector3( t, snapAmount );
		pose.translation( t );
	}

	if (!BigBang::instance().isPointInWriteableChunk( pose.applyToOrigin() ))
		Py_Return;

	Chunk* pChunk = utilCreateChunk( pSect, chunkName, &pose );
	if (!pChunk)
		return NULL;

	// move it to the locator's transform (so creator needn't repeat
	// work of calculating offset boundaries and all that)
	if (pPyLoc != NULL)
	{
		pChunk->transform( pose );
	}

	// and tell the cache that it has arrived!
	EditorChunkCache::instance( *pChunk ).edArrive( true );

	// this is mainly for indoor chunks, create a static lighting data section.
	EditorChunkCache::instance( *pChunk ).edRecalculateLighting();

	// now add an undo which deletes it
	UndoRedo::instance().add(
		new ChunkExistenceOperation( pChunk, false ) );

	ChunkItemPtr modelItem = EditorChunkCache::instance(*pChunk).getShellModel();
	std::vector<ChunkItemPtr> newItems;
	newItems.push_back(modelItem);
	BigBang::instance().setSelection( newItems );

	// set a meaningful barrier name
	UndoRedo::instance().barrier( "Create Chunk " + pChunk->identifier(), false );

	// check to see if in the space, if not -> can't place this chunk, undo create
	BoundingBox spaceBB(ChunkManager::instance().cameraSpace()->gridBounds());
	if ( !(spaceBB.intersects( pChunk->boundingBox().minBounds() ) &&
			spaceBB.intersects( pChunk->boundingBox().maxBounds() )) )
	{
		UndoRedo::instance().undo();
		Py_Return;
	}

	BigBang::instance().markTerrainShadowsDirty( pChunk->boundingBox() );

	// and that's it then. return a group that contains the chunk's item
	ChunkItemGroup * pRes = new ChunkItemGroup();
	pRes->add( modelItem );
	return pRes;
}

// defined below
void createBoundarySections( DataSectionPtr pSec, Moo::VisualPtr pVis, const Matrix & wantWorld );
bool visualFromModel(
	const DataSectionPtr& pModelDS,
	std::string& visualName,
	DataSectionPtr& visualDS,
	Moo::VisualPtr& pVis);
void rewriteBoundaryInformation( const DataSectionPtr& pChunkSection,
	DataSectionPtr& pVisualDS,
	const Moo::VisualPtr& pVis );

PyObject * ChunkPlacer::py_recreateChunks( PyObject * args )
{
	// get args
	PyObject * pPyRev;
	if (!PyArg_ParseTuple( args, "O", &pPyRev ) ||
		!ChunkItemRevealer::Check( pPyRev ))
	{
		PyErr_SetString( PyExc_ValueError, "BigBang.recreateChunks() "
			"expects a ChunkItemRevealer" );
		return NULL;
	}


	ChunkItemRevealer* pRevealer = static_cast<ChunkItemRevealer*>( pPyRev );
	ChunkItemRevealer::ChunkItems items;
	pRevealer->reveal( items );

	ChunkItemRevealer::ChunkItems::iterator i = items.begin();
	for (; i != items.end(); ++i)
	{
		ChunkItemPtr pItem = *i;
		Chunk* pChunk = pItem->chunk();

		if (!pItem->isShellModel())
		{
			WARNING_MSG( "Trying to recreate something that isn't a shell\n" );
			continue;
		}

		MF_ASSERT( pChunk );

		if (!pChunk->online())
		{
			WARNING_MSG( "Chunk isn't online, can't recreate\n" );
			continue;
		}

		DataSectionPtr pChunkSection = EditorChunkCache::instance( *pChunk ).pChunkSection();
		MF_ASSERT( pChunkSection );
		std::string modelName = pItem->pOwnSect()->readString( "resource" );

		DataSectionPtr pModelDS = BWResource::openSection( modelName );

		std::string visualName;
		DataSectionPtr visualDS;
		Moo::VisualPtr pVis;
		if ( !visualFromModel(pModelDS, visualName, visualDS, pVis) )
		{
			//above function sets the PyErr string.
			continue;
		}

		rewriteBoundaryInformation( pChunkSection, visualDS , pVis );
		EditorChunkCache::instance( *pChunk ).reloadBounds();
		BigBang::instance().changedChunk( pChunk );
	}

	// and that's it
	Py_Return;
}

PyObject * ChunkPlacer::py_saveChunkTemplate( PyObject * args )
{
	// get args
	PyObject * pPyRev;
	if (!PyArg_ParseTuple( args, "O", &pPyRev ) ||
		!ChunkItemRevealer::Check( pPyRev ))
	{
		PyErr_SetString( PyExc_ValueError, "BigBang.recreateChunks() "
			"expects a ChunkItemRevealer" );
		return NULL;
	}


	ChunkItemRevealer* pRevealer = static_cast<ChunkItemRevealer*>( pPyRev );
	ChunkItemRevealer::ChunkItems items;
	pRevealer->reveal( items );

	bool gotOneShell = false;
	ChunkItemRevealer::ChunkItems::iterator i = items.begin();
	for (; i != items.end(); ++i)
	{
		ChunkItemPtr pItem = *i;
		Chunk* pChunk = pItem->chunk();

		MF_ASSERT( pChunk );

		if (pChunk->isOutsideChunk())
		{
			WARNING_MSG( "Chunk isn't indoors, can't save as template\n" );
			continue;
		}

		if (!pChunk->online())
		{
			WARNING_MSG( "Chunk isn't online, can't save as template\n" );
			continue;
		}
		if( !pItem->isShellModel() )
			continue;

		gotOneShell = true;
		DataSectionPtr pChunkSection = EditorChunkCache::instance( *pChunk ).pChunkSection();
		std::string modelName = pItem->pOwnSect()->readString( "resource" );

		MF_ASSERT( !modelName.empty() );

		std::string templateName = modelName + ".template";

        // Create a new file in the models dir
		std::string templateDir = BWResource::getFilePath( templateName );

		DataSectionPtr dir = BWResource::openSection( templateDir );
		if (!dir)
		{
			ERROR_MSG( "saveChunkTemplate() - Couldn't open dir %s\n",
				templateDir.c_str() );
			continue;
		}

		DataSectionPtr pDS = dir->newSection( BWResource::getFilename( templateName ) );
		if (!pDS)
		{
			ERROR_MSG( "saveChunkTemplate() - Couldn't create file\n" );
			continue;
		}

		// Only save lights and flares
		pDS->copySections( pChunkSection, "omniLight" );
		pDS->copySections( pChunkSection, "spotLight" );
		pDS->copySections( pChunkSection, "directionalLight" );
		pDS->copySections( pChunkSection, "ambientLight" );
		pDS->copySections( pChunkSection, "flare" );
		pDS->copySections( pChunkSection, "pulseLight" );


		/*
		// Copy everything from the chunks datasection
		pDS->copy( pChunkSection );

		// Strip the chunk specific stuff out of it
		pDS->deleteSections( "waypointGenerationTime" );
		pDS->deleteSections( "waypointSet" );
		pDS->deleteSections( "boundary" );
		pDS->deleteSections( "boundingBox" );
		pDS->deleteSections( "transform" );
		// Stations too, we don't want them connecting to the same graph
		pDS->deleteSections( "station" );

		std::vector<DataSectionPtr> modelSections;
		pDS->openSections( "model", modelSections );
		std::vector<DataSectionPtr>::iterator i = modelSections.begin();
		for (; i != modelSections.end(); ++i)
			(*i)->delChild( "lighting" );
		*/

		pDS->save();

		INFO_MSG( "Saved chunk template %s\n", templateName.c_str() );
	}

	if( !gotOneShell )
	{
		BigBang::instance().addCommentaryMsg( "Only shells could be saved as templates" );
	}
	else
		BigBang::instance().addCommentaryMsg( "Saved Shell template" );

	Py_Return;
}

void ChunkPlacer::utilCloneChunkDataSection( Chunk* chunk, DataSectionPtr ds, const std::string& newChunkName )
{
	DataSectionPtr sourceChunkSection = EditorChunkCache::instance( *chunk ).pChunkSection();
	utilCloneChunkDataSection(sourceChunkSection, ds, newChunkName);
}

void ChunkPlacer::utilCloneChunkDataSection( DataSectionPtr sourceChunkSection, DataSectionPtr ds, const std::string& newChunkName )
{
	ds->copy( sourceChunkSection );

	//NOTE : this algorithm only works for INDOOR chunks.  The portal
	//binding will not be correct if this method is called for OUTDOOR chunks.
	//This is ok because presently you cannot clone an outdoor chunk.
	for (DataSectionIterator ci = ds->begin(); ci != ds->end(); ++ci)
		if ((*ci)->sectionName() == "boundary")
			for (DataSectionIterator bi = (*ci)->begin(); bi != (*ci)->end(); ++bi)
				if ((*bi)->sectionName() == "portal")
				{
					//NOTE : These special portal names are from
					//the list in chunk_boundary.cpp.
					//Make sure that the known portal names are up to date.
					std::string c = (*bi)->readString( "chunk" );
					if ( c == "heaven" )
					{
						//do nothing
					}
					else if ( c == "earth" )
					{
						//do nothing
					}
					else if ( c == "extern" )
					{
						//do nothing
					}
					else if ( c == "invasive" )
					{
						//do nothing
					}
					else if (c != "" && c[c.length() - 1] == 'o')
					{
						//unbind the link from indoor to outdoor chunks
						//and write "invasive" instead
						(*bi)->writeString( "chunk", "invasive" );
					}
					else
					{
						//otherwise just unbind the inside-inside link.
						//when reloaded, the geographical locality will
						//be enough to rebind.
						(*bi)->deleteSection( "chunk" );
					}
				}
}

/**
 * Clone the given chunk, and snap it to the second
 *
 * Take no action if we can't find a match to the second
 */
PyObject* ChunkPlacer::py_cloneAndAutoSnap( PyObject * args )
{
	// get args
	PyObject *pPyRev1, *pPyRev2;
	if (!PyArg_ParseTuple( args, "OO", &pPyRev1, &pPyRev2 ) ||
		!ChunkItemRevealer::Check( pPyRev1 ) ||
		!ChunkItemRevealer::Check( pPyRev2 ))
	{
		PyErr_SetString( PyExc_ValueError, "BigBang.autoSnap() "
			"expects two ChunkItemRevealers" );
		return NULL;
	}

	ChunkItemRevealer* chunkRevealer = static_cast<ChunkItemRevealer*>( pPyRev1 );
	ChunkItemRevealer* snapToRevealer = static_cast<ChunkItemRevealer*>( pPyRev2 );

	ChunkItemRevealer::ChunkItems sourceChunkItems;
	ChunkItemRevealer::ChunkItems snapToChunkItems;
	chunkRevealer->reveal( sourceChunkItems );
	snapToRevealer->reveal( snapToChunkItems );

	if (sourceChunkItems.size() != 1)
	{
		PyErr_Format( PyExc_ValueError, "BigBang.cloneAndAutoSnap() "
			"Can only clone from one item" );
		return NULL;
	}
	if (!sourceChunkItems.front()->isShellModel())
	{
		PyErr_Format( PyExc_ValueError, "BigBang.cloneAndAutoSnap() "
			"Can only clone from a chunk" );
		return NULL;
	}

	Chunk* chunk = sourceChunkItems.front()->chunk();

	std::vector<Chunk*> snapToChunks;
	ChunkItemRevealer::ChunkItems::iterator i = snapToChunkItems.begin();
	for (; i != snapToChunkItems.end(); ++i)
	{
		if ((*i)->isShellModel())
			snapToChunks.push_back( (*i)->chunk() );
	}

	if (snapToChunks.empty())
	{
		PyErr_Format( PyExc_ValueError, "BigBang.cloneAndAutoSnap() "
			"No chunks to snap to" );
		return NULL;
	}



	// make sure there's only one chunk that we're copying from
	/*
	std::vector<Chunk*> sourceChunks = extractChunks( chunkRevealer );
	std::vector<Chunk*> snapToChunks = extractChunks( snapToRevealer );
	if (sourceChunks.size() != 1)
	{
		PyErr_Format( PyExc_ValueError, "BigBang.cloneAndAutoSnap() "
			"Can only clone from one item" );
		return NULL;
	}
	Chunk* chunk = sourceChunks.front();
	*/


	Matrix autoSnapTransform = findAutoSnapTransform( chunk, snapToChunks );

	if (autoSnapTransform != chunk->transform())
	{
		std::string newChunkName;
		DataSectionPtr pDS = utilCreateInsideChunkDataSection( snapToChunks[0], newChunkName );
		if (!pDS)
			Py_Return;

		utilCloneChunkDataSection( chunk, pDS, newChunkName );

		Chunk* pChunk = utilCreateChunk( pDS, newChunkName, &autoSnapTransform );
		if( !pChunk )
			return NULL;

		// move it to the autoSnap position
		pChunk->transform( autoSnapTransform );

		// and tell the cache that it has arrived!
		EditorChunkCache::instance( *pChunk ).edArrive( true );

		// tell the chunk we just cloned it
		EditorChunkCache::instance( *pChunk ).edPostClone();

		// now add an undo which deletes it
		UndoRedo::instance().add(
			new ChunkExistenceOperation( pChunk, false ) );

		// set a meaningful barrier name
		UndoRedo::instance().barrier( "Auto Clone Chunk " + pChunk->identifier(), false );

		// select the appropriate shell model
		ChunkItemPtr modelItem = EditorChunkCache::instance(*pChunk).getShellModel();

		// and that's it then. return a group that contains the chunk's item
		ChunkItemGroup * pRes = new ChunkItemGroup();
		pRes->add( modelItem );
		return pRes;
	}
	else
		Py_Return;
}

/**
 *	This method allows scripts to delete a chunk item
 */
/*
PyObject * ChunkPlacer::py_deleteChunk( PyObject * args )
{
	// get args
	PyObject * pPyRev;
	if (!PyArg_ParseTuple( args, "O", &pPyRev ) ||
		!ChunkItemRevealer::Check( pPyRev ))
	{
		PyErr_SetString( PyExc_ValueError, "BigBang.deleteChunk() "
			"expects a ChunkItemRevealer" );
		return NULL;
	}

	ChunkItemRevealer* pRevealer = static_cast<ChunkItemRevealer*>( pPyRev );

	ChunkItemRevealer::ChunkItems items;
	pRevealer->reveal( items );

	ChunkItemRevealer::ChunkItems::iterator i = items.begin();
	for (; i != items.end(); ++i)
	{
		ChunkItemPtr pItem = *i;
		Chunk* pChunk = pItem->chunk();

		if (pChunk == NULL)
		{
			PyErr_Format( PyExc_ValueError, "BigBang.deleteChunk() "
				"Item has already been deleted!" );
			return NULL;
		}

		// see if it wants to be deleted
		if (!EditorChunkCache::instance( *pChunk ).edCanDelete())
			continue;

		// tell the chunk we're going to delete it
		EditorChunkCache::instance( *pChunk ).edPreDelete();

		// ok, now delete its chunk
		EditorChunkCache::instance( *pChunk ).edDepart();

		// set up an undo which creates it
		UndoRedo::instance().add(
			new ChunkExistenceOperation( pChunk, true ) );
	}

	// set a meaningful barrier name
	if (items.size() == 1)
		UndoRedo::instance().barrier(
			"Delete Chunk " + items.front()->chunk()->identifier(), false );
	else
		UndoRedo::instance().barrier( "Delete Chunks", false );


	// and that's it
	Py_Return;
}
*/
bool ChunkPlacer::deleteChunk( Chunk * pChunk )
{
	// see if it wants to be deleted
	if (!EditorChunkCache::instance( *pChunk ).edCanDelete())
		return false;

	// tell the chunk we're going to delete it
	EditorChunkCache::instance( *pChunk ).edPreDelete();

	// ok, now delete its chunk
	EditorChunkCache::instance( *pChunk ).edDepart();

	// set up an undo which creates it
	UndoRedo::instance().add(
		new ChunkExistenceOperation( pChunk, true ) );

	return true;
}

std::string ChunkPlacer::utilCreateInsideChunkName( Chunk * pNearbyChunk )
{
	static int lastNumber = 0;

	char result[512];
	char chunkName[256];
	char backupChunkName[256];
	result[0] = 0;

	std::string spacesubdir;
	uint lslash = pNearbyChunk->identifier().find_last_of( '/' );
	if (lslash < pNearbyChunk->identifier().length())
		spacesubdir = pNearbyChunk->identifier().substr( 0, lslash+1 );

	sprintf( result, "error" );

	std::string space = Options::getOptionString( "space/mru0" );
	std::string userTag = Options::getOptionString( "userTag", "00" );

	if ( userTag.length() == 2 )
	{
		if ( space != "" )
		{
			std::string path = space + "/";

			bool found = false;
			while (!found && lastNumber < 65536)
			{
				sprintf( chunkName, "%s00%s%04xi.chunk",
					spacesubdir.c_str(), userTag.c_str(), lastNumber );
				sprintf( backupChunkName, "%s00%s%04xi.~chunk~",
					spacesubdir.c_str(), userTag.c_str(), lastNumber );
				++lastNumber;

				// check for file existence
				char buf[512];
				sprintf( buf, "%s%s", path.c_str(), chunkName );

				char backup_buf[512];
				sprintf( backup_buf, "%s%s", path.c_str(), backupChunkName );

				if ( !BWResource::openSection( buf ) && !BWResource::openSection( backup_buf ) )
				{
					sprintf( result, "%s", chunkName );
					found = true;
					DEBUG_MSG( "Found unused name - %s\n", buf );
				}
				else
				{
					DEBUG_MSG( "Skipping %s - file exists\n", buf );
					continue;
				}

				// sanity check, make sure it's not known to the space
				std::string chunkFileName = chunkName;
				std::string chunkIdentifier = chunkFileName.substr( 0, chunkFileName.size() - 6 );
				ChunkSpacePtr pSpace = ChunkManager::instance().cameraSpace();
				if (pSpace->chunks().find( chunkIdentifier ) != pSpace->chunks().end())
				{
					found = false;
					WARNING_MSG( "Chunk %s doesn't exist in the file system, but is known to the space\n", chunkName );
				}

			}
		}
		else
		{
			ERROR_MSG( "createInsideChunkName - Invalid universe or space name" );
		}
	}
	else
	{
		ERROR_MSG( "createInsideChunkName - User tag must have 2 characters" );
	}

	return result;
}


/**
 *	This method returns a new unique chunk name.
 *
 *	@return		a chunk name, or "error" if the function failed.
 */
PyObject * ChunkPlacer::py_createInsideChunkName( PyObject * args )
{
	std::string name = utilCreateInsideChunkName( ChunkManager::instance().cameraChunk() );

	return PyString_FromString( name.c_str() );
}

/**
 *	Get the DataSection for the given chunk
 */
PyObject * ChunkPlacer::py_chunkDataSection( PyObject * args )
{
	// get args
	PyObject * pPyRev;
	if (!PyArg_ParseTuple( args, "O", &pPyRev ) ||
		!ChunkItemRevealer::Check( pPyRev ))
	{
		PyErr_SetString( PyExc_ValueError, "BigBang.deleteChunkItem() "
			"expects a ChunkItemRevealer" );
		return NULL;
	}

	ChunkItemRevealer* pRevealer = static_cast<ChunkItemRevealer*>( pPyRev );

	// make sure there's only one
	ChunkItemRevealer::ChunkItems items;
	pRevealer->reveal( items );
	if (items.size() != 1)
	{
		PyErr_Format( PyExc_ValueError, "BigBang.deleteChunk() "
			"Revealer must reveal exactly one item, not %d", items.size() );
		return NULL;
	}

	Chunk* pChunk = items.front()->chunk();

	return new PyDataSection( EditorChunkCache::instance( *pChunk ).pChunkSection() );
}

DataSectionPtr ChunkPlacer::utilCreateInsideChunkDataSection( Chunk * pNearbyChunk, std::string& retChunkID )
{
	if (pNearbyChunk == NULL)
	{
		ERROR_MSG( "ChunkPlacer::createInsideChunkDataSection() - camera chunk is NULL\n" );
		return NULL;
	}
	std::string newChunkName = utilCreateInsideChunkName( pNearbyChunk );
	if (newChunkName == "error")
	{
		ERROR_MSG( "ChunkPlacer::createInsideChunkDataSection() - Couldn't create chunk name\n" );
		return NULL;
	}

	std::string dirName = Options::getOptionString("space/mru0");

	DataSectionPtr dir = BWResource::openSection(dirName);
	if (!dir)
	{
		ERROR_MSG( "ChunkPlacer::createInsideChunkDataSection() - Couldn't open %s\n", dirName.c_str() );
		return NULL;
	}

	DataSectionPtr pDS = dir->openSection( newChunkName, /*makeNewSection:*/true );
	if (!pDS)
		ERROR_MSG( "ChunkPlacer::createInsideChunkDataSection() - Couldn't create DataSection\n" );

	//strip off the .chunk and return the identifier.
	retChunkID = newChunkName.substr(0,newChunkName.size()-6);
	return pDS;
}


PyObject* ChunkPlacer::py_createInsideChunkDataSection( PyObject * args )
{
	std::string newChunkName;
	DataSectionPtr pDS = utilCreateInsideChunkDataSection(
		ChunkManager::instance().cameraChunk(),
		newChunkName );

	PyObject* tuple = PyTuple_New(2);
	if( pDS )
		PyTuple_SetItem( tuple, 0, new PyDataSection(pDS) );
	else
		PyTuple_SetItem( tuple, 0, Py_None );
	PyTuple_SetItem( tuple, 1, PyString_FromString( newChunkName.c_str() ) );

	return tuple;
}


/*PyObject* ChunkPlacer::py_cloneChunkDataSection( PyObject * args )
{
	// get args
	PyObject *pPyRev, *pPyDS;
	if (!PyArg_ParseTuple( args, "OO", &pPyRev, &pPyDS ) ||
		!ChunkItemRevealer::Check( pPyRev ) ||
		!PyDataSection::Check(pPyDS))
	{
		PyErr_SetString( PyExc_ValueError, "BigBang.cloneChunkDataSection() "
			"expects a ChunkItemRevealer and a DataSection" );
		return NULL;
	}

	ChunkItemRevealer* pRevealer = static_cast<ChunkItemRevealer*>( pPyRev );

	// make sure there's only one
	ChunkItemRevealer::ChunkItems items;
	pRevealer->reveal( items );
	if (items.size() != 1)
	{
		PyErr_Format( PyExc_ValueError, "BigBang.deleteChunk() "
			"Revealer must reveal exactly one item, not %d", items.size() );
		return NULL;
	}

	Chunk* pChunk = items.front()->chunk();

	DataSectionPtr pSect = static_cast<PyDataSection*>( pPyDS )->pSection();

	utilCloneChunkDataSection( pChunk, pSect );

	Py_Return;
}*/

/*
PyObject* ChunkPlacer::py_cloneChunks( PyObject * args )
{
	// get args
	PyObject * pPyRev;
	if (!PyArg_ParseTuple( args, "O", &pPyRev ) ||
		!ChunkItemRevealer::Check( pPyRev ))
	{
		PyErr_SetString( PyExc_ValueError, "BigBang.deleteChunkItem() "
			"expects a ChunkItemRevealer" );
		return NULL;
	}

	ChunkItemRevealer* pRevealer = static_cast<ChunkItemRevealer*>( pPyRev );
	ChunkItemRevealer::ChunkItems items;
	pRevealer->reveal( items );

	ChunkItemRevealer::ChunkItems newItems;

	ChunkItemRevealer::ChunkItems::iterator i = items.begin();
	for (; i != items.end(); ++i )
	{
		Chunk* chunk = (*i)->chunk();

		DataSectionPtr pDS = utilCreateInsideChunkDataSection();
		if (!pDS)
			Py_Return;

		utilCloneChunkDataSection( chunk, pDS );

		Chunk* pChunk = utilCreateChunk( pDS );

		// and tell the cache that it has arrived!
		EditorChunkCache::instance( *pChunk ).edArrive( true );

		// now add an undo which deletes it
		UndoRedo::instance().add(
			new ChunkExistenceOperation( pChunk, false ) );

		// tell the chunk we just cloned it
		EditorChunkCache::instance( *pChunk ).edPostClone();

		// select the appropriate shell model
		newItems.push_back( EditorChunkCache::instance(*pChunk).getShellModel() );
	}

	// Cloning is always done as part of a clone & move tool, don't set a barrier here
#if 0
	// set a meaningful barrier name
	if (items.size() == 1)
		UndoRedo::instance().barrier( "Clone Chunk " + items.front()->chunk()->identifier(), false );
	else
		UndoRedo::instance().barrier( "Clone Chunks", false );
#endif

	return new ChunkItemGroup( newItems );
}
*/
ChunkItem * ChunkPlacer::cloneChunk( Chunk * pChunk, const Matrix& newTransform )
{
	std::string newChunkName;
	DataSectionPtr pDS = ChunkPlacer::utilCreateInsideChunkDataSection( pChunk, newChunkName );
	if (!pDS)
		return 0;

	BoundingBox bb;
	EditorChunkCache::instance( *pChunk ).getShellModel()->edBounds( bb );
	bb.transformBy( newTransform );

	ChunkPlacer::utilCloneChunkDataSection( pChunk, pDS, newChunkName );

	std::vector<ChunkItemPtr> chunkItems;
	EditorChunkCache::instance( *pChunk ).allItems( chunkItems );
	for( std::vector<ChunkItemPtr>::iterator iter = chunkItems.begin(); iter != chunkItems.end() ; ++iter )
		(*iter)->edPreChunkClone( pChunk, newTransform, pDS );

	pDS->writeMatrix34( "transform", newTransform );
	pDS->writeVector3( "boundingBox/min", bb.minBounds() );
	pDS->writeVector3( "boundingBox/max", bb.maxBounds() );

	Chunk* pNewChunk = ChunkPlacer::utilCreateChunk( pDS, newChunkName );

	if( !pNewChunk )
		return 0;

	// and tell the cache that it has arrived!
	EditorChunkCache::instance( *pNewChunk ).edArrive( true );

	// now add an undo which deletes it
	UndoRedo::instance().add(
		new ChunkExistenceOperation( pNewChunk, false ) );

	// tell the chunk we just cloned it
	EditorChunkCache::instance( *pNewChunk ).edPostClone();

	// select the appropriate shell model
	return &*EditorChunkCache::instance( *pNewChunk ).getShellModel();
}


// -----------------------------------------------------------------------------
// Section: PyLoadedChunks
// -----------------------------------------------------------------------------

#include "chunk_editor.hpp"

/**
 *	This class provides access to all loaded chunks through a map-like interface
 */
class PyLoadedChunks : public PyObjectPlus
{
	Py_Header( PyLoadedChunks, PyObjectPlus )

public:
	PyLoadedChunks( PyTypePlus * pType = &s_type_ );

	PyObject * pyGetAttribute( const char * attr );

	int has_key( PyObjectPtr pObject );
	PY_AUTO_METHOD_DECLARE( RETDATA, has_key, ARG( PyObjectPtr, END ) )

	PY_INQUIRY_METHOD( pyMap_length )
	PY_BINARY_FUNC_METHOD( pyMap_subscript )
};

static PyMappingMethods PyLoadedChunks_mapfns =
{
	PyLoadedChunks::_pyMap_length,		// mp_length
	PyLoadedChunks::_pyMap_subscript,	// mp_subscript
	0									// mp_ass_subscript
};

PY_TYPEOBJECT_WITH_MAPPING( PyLoadedChunks, &PyLoadedChunks_mapfns )

PY_BEGIN_METHODS( PyLoadedChunks )
	PY_METHOD( has_key )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( PyLoadedChunks )
PY_END_ATTRIBUTES()


/**
 *	Constructor
 */
PyLoadedChunks::PyLoadedChunks( PyTypePlus * pType ) :
	PyObjectPlus( pType )
{
}

/**
 *	Python get attribute method
 */
PyObject * PyLoadedChunks::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();
	return this->PyObjectPlus::pyGetAttribute( attr );
}

int PyLoadedChunks::has_key( PyObjectPtr pObject )
{
	PyObject * pRet = this->pyMap_subscript( &*pObject );
	PyErr_Clear();
	Py_XDECREF( pRet );

	return pRet != NULL ? 1 : 0;
}

int PyLoadedChunks::pyMap_length()
{
	ChunkSpacePtr pSpace = ChunkManager::instance().cameraSpace();
	if (!pSpace) return 0;

	int len = 0;
	for (ChunkMap::iterator i = pSpace->chunks().begin();
		i != pSpace->chunks().end();
		i++)
	{
		//len += i->second.size();
		std::vector<Chunk*> & homonyms = i->second;

		for (std::vector<Chunk*>::iterator j = homonyms.begin();
			j != homonyms.end();
			j++)
		{
			if ((*j)->online()) len++;
		}
	}

	return len;
}

PyObject * PyLoadedChunks::pyMap_subscript( PyObject * pKey )
{
	ChunkSpacePtr pSpace = ChunkManager::instance().cameraSpace();
	if (!pSpace)
	{
		PyErr_SetString( PyExc_EnvironmentError, "PyLoadedChunks[] "
			"Camera is not in any chunk space!" );
		return NULL;
	}

	Chunk * pFound = NULL;

	// try it as an index
	int index = -1;
	if (pFound == NULL && Script::setData( pKey, index ) == 0)
	{
		for (ChunkMap::iterator i = pSpace->chunks().begin();
			i != pSpace->chunks().end() && index >= 0;
			i++)
		{
			std::vector<Chunk*> & homonyms = i->second;

			for (std::vector<Chunk*>::iterator j = homonyms.begin();
				j != homonyms.end() && index >= 0;
				j++)
			{
				if ((*j)->online())
				{
					if (index-- == 0) pFound = *j;
				}
			}
		}
	}

	// try it as a string
	std::string chunkName;
	if (pFound == NULL && Script::setData( pKey, chunkName ) == 0)
	{
		ChunkMap::iterator i = pSpace->chunks().find( chunkName );
		if (i != pSpace->chunks().end())
		{
			std::vector<Chunk*> & homonyms = i->second;
			for (std::vector<Chunk*>::iterator j = homonyms.begin();
				j != homonyms.end();
				j++)
			{
				if ((*j)->online())
				{
					pFound = *j;
					break;
				}
			}
		}
	}

	// if we didn't find it by here then it's not there
	if (pFound != NULL)
	{
		return new ChunkEditor( pFound );
	}
	else
	{
		PyErr_SetString( PyExc_KeyError,
			"PyLoadedChunks: No such loaded chunk" );
		return NULL;
	}
}


/**
 *	This attribute is an object that provides access to all currently
 *	loaded chunks
 */
PY_MODULE_ATTRIBUTE( BigBang, chunks, new PyLoadedChunks() )



// -----------------------------------------------------------------------------
// Section: Misc python functions
// -----------------------------------------------------------------------------


/**
 *	This is a helper class culled from rhino2's portal.hpp
 */
class PrivPortal
{
public:
	PrivPortal() : hasPlaneEquation_( false ), flags_( 0 ) { }
	~PrivPortal() { }

	const PlaneEq&			getPlaneEquation( void ) const { return planeEquation_; }

	void					addPoint( const Vector3 &v );
	void					addPoints( const Moo::PortalData & pd );

	const Vector3&			getPoint( uint32 i ) const	{ return points_[i]; }
	uint32					getNPoints() const			{ return points_.size(); }

	void					transform( const class Matrix& transform );

	bool					isHeaven() const	{ return !!(flags_ & (1<<1)); }
	bool					isEarth() const		{ return !!(flags_ & (1<<2)); }
	bool					isInvasive() const	{ return !!(flags_ & (1<<3)); }

private:
	PlaneEq					planeEquation_;
	bool					hasPlaneEquation_;

	int						flags_;

	std::vector< Vector3 >	points_;
};

/**
 *	This method adds a point such that it remains a convex 2d (planar) portal.
 */
void PrivPortal::addPoint( const Vector3 &v )
{
	const float IN_PORTAL_PLANE			= 0.2f;

	if( !hasPlaneEquation_ )
	{
		points_.push_back( v );

		if( points_.size() >= 3 )
		{
			uint32 i;
			PlaneEq p;
			for( i = 0; i < ( points_.size() - 2 ); i++ )
			{
				p.init( points_[ i ], points_[ i + 1 ], points_[ i + 2 ] );

				if( ! ((p.normal()[0] == 0) && (p.normal()[1] == 0) && (p.normal()[2] == 0)) )
				{
					hasPlaneEquation_ = true;
					break;
				}
			}
			if( hasPlaneEquation_ )
			{
				planeEquation_ = p;
				Vector3 p1( points_[i] );
				Vector3 p2( points_[i+1] );
				Vector3 p3( points_[i+2] );

				points_.erase( points_.begin() + i );
				points_.erase( points_.begin() + i );
				points_.erase( points_.begin() + i );

				std::vector< Vector3 > ps;

				while( points_.size() > 0 )
				{
					ps.push_back( points_.back() );
					points_.pop_back();
				}

				points_.push_back( p1 );
				points_.push_back( p2 );
				points_.push_back( p3 );

				while( ps.size() > 0 )
				{
					this->addPoint( ps.back() );
					ps.pop_back();
				}
			}
		}
	}
	else
	{
		float d = planeEquation_.distanceTo( v );

		Vector3 n = planeEquation_.normal();

		if( fabs( d ) > IN_PORTAL_PLANE )
			return;

		bool firstLastIncluded = false;
		bool removePrevious = false;

		Vector3 vLine( points_.front() - points_.back() );
		Vector3 vNormal;
		vNormal.crossProduct( n, vLine );
		if( vNormal.dotProduct( points_.front() ) >= vNormal.dotProduct( v ) )
		{
			firstLastIncluded = true;
			removePrevious = true;
		}

		for( uint32 i = 0; i < ( points_.size() -1); i++ )
		{
			vLine = points_[i+1] - points_[i];
			vNormal.crossProduct( n, vLine );
			if( vNormal.dotProduct( points_[i] ) >= vNormal.dotProduct( v ) )
			{
				if( removePrevious == true )
				{
					points_[i] = v;
				}
				else
				{
					points_.insert( points_.begin() + (i + 1), v);
					i++;
				}
				removePrevious = true;
			}
			else
			{
				removePrevious = false;
			}
		}

		if( removePrevious && firstLastIncluded )
			points_.back() = v;
		if( firstLastIncluded )
			points_.push_back( v );

		std::vector< Vector3 >::iterator it, it2;
		if( ( it = std::find( points_.begin(), points_.end(), v ) ) != points_.end() )
		{
			while( (it2 = std::find( it+1, points_.end(), v)) != points_.end() )
			{
				points_.erase( it2 );
			}
		}
	}
}


/**
 *	Add all the points for a portal (extracted by a visual) into this one
 */
void PrivPortal::addPoints( const Moo::PortalData & pd )
{
	flags_ = pd.flags();

	for (uint i = 0; i < pd.size(); i++)
	{
		this->addPoint( pd[i] );
	}
}


/**
 *	This method applies the given transform to this portal
 */
void PrivPortal::transform( const class Matrix& transform )
{
	Vector3 pos = transform.applyPoint( planeEquation_.normal() * planeEquation_.d() );
	Vector3 norm = transform.applyVector( planeEquation_.normal() );
	norm.normalise();
	planeEquation_ = PlaneEq( norm, pos.dotProduct( norm ) );

	for( uint32 i = 0; i < points_.size(); i++ )
	{
		points_[ i ] = transform.applyPoint( points_[ i ] );
	}
}

float roundFloat( float f )
{
	return Snap::value( f, 0.1f );
}

Vector3 roundVector3( Vector3 v )
{
	Snap::vector3( v, Vector3( 0.1f, 0.1f, 0.1f ) );
	return v;
}


/**
 *	This static method creates boundary sections from the portals in
 *	a visual. It needs to be updated to not assume all the portals are
 *	always on a boundary, since some aren't. Also we have to do the
 *	whole splitting chunks over internal portals thing, but that can
 *	come later methinks.
 *
 *	This method exists now only to support legacy .visual files.  New
 *	.visual files have the boundary information written out directly
 *	from MAX.  We should never have to guess the boundaries.
 */
static void createBoundarySections( DataSectionPtr pSec, Moo::VisualPtr pVis,
	const Matrix & wantWorld )
{
	// set up some matrices
	Matrix parentWorld = Matrix::identity;
	Matrix wantWorldInv; wantWorldInv.invert( wantWorld );

	// get the (world) bounding box
	BoundingBox bb = pVis->boundingBox();
	bb.transformBy( wantWorld );
	Vector3 bbMin = bb.minBounds();
	Vector3 bbMax = bb.maxBounds();

	std::vector<PrivPortal *>		portals[6];

	// now look at all our portals, and assign them to a boundary
	for (uint i = 0; i < pVis->nPortals(); i++)
	{
		PrivPortal  * bpNew = new PrivPortal();
		bpNew->addPoints( pVis->portal( i ) );
		PrivPortal & bp = *bpNew;
		bp.transform( parentWorld );

		const PlaneEq & peq = bp.getPlaneEquation();
		Vector3 normal = peq.normal();
		Vector3 point = normal * peq.d();

		// figure out which side it's on
		int side = 0;
		Vector3 anormal( fabsf(normal[0]), fabsf(normal[1]), fabsf(normal[2]) );
		if (anormal[0] > max(anormal[1], anormal[2]))
		{	// on yz plane (l or r)
			side = 0 + int(fabs(point[0] - bbMin[0]) > fabsf(point[0] - bbMax[0]));
		}
		else if (anormal[1] > max(anormal[0], anormal[2]))
		{	// on xz plane (d or u)
			side = 2 + int(fabs(point[1] - bbMin[1]) > fabsf(point[1] - bbMax[1]));
		}
		else
		{	// on xy plane (f or b)
			side = 4 + int(fabs(point[2] - bbMin[2]) > fabsf(point[2] - bbMax[2]));
		}

		// add it to that side's list
		portals[side].push_back( &bp );
	}

	// now write out the boundary
	for (int b = 0; b < 6; b++)
	{
		DataSectionPtr pBoundary = pSec->newSection( "boundary" );

		// figure out the boundary plane in world coordinates
		int sign = 1 - (b&1)*2;
		int axis = b>>1;
		Vector3 normal(
			sign * ((axis==0)?1.f:0.f),
			sign * ((axis==1)?1.f:0.f),
			sign * ((axis==2)?1.f:0.f) );
		float d = sign > 0 ? bbMin[axis] : -bbMax[axis];

		Vector3 localCentre = wantWorldInv.applyPoint( normal * d );
		Vector3 localNormal = wantWorldInv.applyVector( normal );
		localNormal.normalise();
		PlaneEq localPlane( localNormal, localNormal.dotProduct( localCentre ) );

		pBoundary->writeVector3( "normal", roundVector3( localPlane.normal() ) );
		pBoundary->writeFloat( "d", roundFloat( localPlane.d() ) );

		for (uint i = 0; i < portals[b].size(); i++)
		{
			PrivPortal  & bp = *portals[b][i];

			// write out the link
			DataSectionPtr pPortal = pBoundary->newSection( "portal" );

			if (bp.isHeaven())
				pPortal->writeString( "chunk", "heaven" );
			else if (bp.isEarth())
				pPortal->writeString( "chunk", "earth" );
			else if (bp.isInvasive())
				pPortal->writeString( "chunk", "invasive" );

			// figure out a uAxis and a vAxis ... for calculation purposes,
			// make (uAxis, vAxis, normal) a basis in world space.
			Vector3	uAxis(
				(axis==1)?1.f:0.f,
				(axis==2)?1.f:0.f,
				(axis==0)?1.f:0.f );
			Vector3 vAxis = normal.crossProduct( uAxis );

			// but write out a uAxis that turns the 2D points into local space
			pPortal->writeVector3( "uAxis", roundVector3( wantWorldInv.applyVector( uAxis ) ) );

			// now transform and write out the points
			Matrix basis;	// (actually using matrix for ordinary maths!)
			basis[0] = uAxis;
			basis[1] = vAxis;
			basis[2] = normal;		// so error from plane is in the z.
			basis.translation( normal * d + wantWorld.applyToOrigin() );
			Matrix invBasis; invBasis.invert( basis );

			for (uint j = 0; j < bp.getNPoints(); j++)
			{
				pPortal->newSection( "point" )->setVector3(
					invBasis.applyPoint( bp.getPoint(j) ) );
			}

			delete portals[b][i];
		}
	}

	// woohoo!
}


/**
 *	This utility function finds the visual name, datasection
 *	and the visual itself, given a .model file data section.
 *
 *	It returns false if the visual could not be found, and
 *	sets the PyErr string.
 */
static bool visualFromModel(
	const DataSectionPtr& pModelDS,
	std::string& visualName,
	DataSectionPtr& pVisualDS,
	Moo::VisualPtr& pVis)
{
	// find the visual and its datasection
	std::string visName =
		pModelDS->readString( "nodelessVisual" );
	if (visName.size()<=7) visName =
		pModelDS->readString( "nodefullVisual" );

	if (visName.empty())
	{
		PyErr_SetString( PyExc_ValueError, "BigBang.chunkFromModel() "
			"could not find nodeless or nodefull visual in model" );
		return false;
	}

	// load it as a .static.visual
	visualName = visName + ".static.visual";
	pVis = Moo::VisualManager::instance()->get(visualName);

	// try adding a .visual
	if (!pVis)
	{
		visualName = visName + ".visual";
		pVis = Moo::VisualManager::instance()->get( visualName );
	}

	if (!pVis)
	{
		PyErr_SetString( PyExc_ValueError, "BigBang.visualFromModel() "
			"could not load nodeless or nodefull visual from resource specified in model" );
		return false;
	}

	// grab the xml file, from the visual that was found.
	pVisualDS = BWResource::openSection( visualName );
	MF_ASSERT( pVisualDS );

	return true;
}


static void rewriteBoundaryInformation( const DataSectionPtr& pChunkSection,
	DataSectionPtr& pVisualDS,
	const Moo::VisualPtr& pVis )
{
	// create the boundaries section.
	if ( pVisualDS->findChild("boundary") )
	{
		pChunkSection->deleteSections("boundary");
		pChunkSection->copySections(pVisualDS, "boundary");
	}
	else
	{
		// ok, let's just make up some boundaries then.
		createBoundarySections( pChunkSection, pVis, Matrix::identity );
	}
}


/**
 *	This utility function creates a chunk datasection from a model file
 */
static PyObject * chunkFromModel( PyDataSectionPtr pChunkDS,
	PyDataSectionPtr pModelDS )
{
	// grab the visual	
	std::string visName;
	DataSectionPtr pVisualDS;
	Moo::VisualPtr pVis;

	if ( !visualFromModel( pModelDS->pSection(), visName, pVisualDS, pVis ) )
	{
		//above function sets the PyErr string.
		return NULL;
	}

	// make sure it has portals
	/*if (pVis->nPortals() == 0)
	{
		PyErr_Format( PyExc_ValueError, "BigBang.chunkFromModel() "
			"no portals in visual %s", visName.c_str() );
		return NULL;
	}*/

	// grab the destination chunk data section.
	DataSectionPtr pSec = pChunkDS->pSection();

	// write out the boundaries
	rewriteBoundaryInformation( pSec, pVisualDS, pVis );

	// and write out the rest of the file.
	pSec->writeMatrix34("transform", Matrix::identity);
	const BoundingBox& bb = pVis->boundingBox();
	pSec->writeVector3( "boundingBox/min", roundVector3(bb.minBounds()) );
	pSec->writeVector3( "boundingBox/max", roundVector3(bb.maxBounds()) );

	Py_Return;
}
PY_AUTO_MODULE_FUNCTION( RETOWN, chunkFromModel,
	NZARG( PyDataSectionPtr, NZARG( PyDataSectionPtr, END ) ), BigBang )
