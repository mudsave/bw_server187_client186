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

#include "editor_chunk.hpp"
#include "autosnap.hpp"
#include "chunk_item_placer.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk_light.hpp"
#include "chunk/chunk_terrain.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/bin_section.hpp"
#include "resmgr/xml_section.hpp"
#include "resmgr/data_section_census.hpp"
#include "appmgr/options.hpp"
#include "gizmo/general_properties.hpp"
#include "chunk_editor.hpp"
#include "big_bang.hpp"
#include "chunk_overlapper.hpp"
#include "cvswrapper.hpp"
#include "snaps.hpp"
#include "static_lighting.hpp"
#include "editor_chunk_model.hpp"
#include "editor_chunk_portal.hpp"
#include "project/chunk_photographer.hpp"
#include "project/space_helpers.hpp"
#include "project/bigbangd_connection.hpp"


#include <set>

#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT2( "Editor", 0 )


// -----------------------------------------------------------------------------
// Section: EditorChunk
// -----------------------------------------------------------------------------

/**
 *	This method finds the outside chunk at the given position if it is focussed.
 */
ChunkPtr EditorChunk::findOutsideChunk( const Vector3 & position,
	bool assertExistence )
{
	ChunkSpacePtr pSpace = ChunkManager::instance().cameraSpace();
	if (!pSpace) return NULL;

	ChunkSpace::Column * pColumn = pSpace->column( position, false );

	if (pColumn != NULL && pColumn->pOutsideChunk() != NULL)
		return pColumn->pOutsideChunk();
	else if (assertExistence)
	{
		CRITICAL_MSG( "EditorChunk::findOutsideChunk: "
			"No focussed outside chunk at (%f,%f,%f) when required\n",
			position.x, position.y, position.z );
	}

	return NULL;
}


/**
 *	This method finds all the focussed outside chunks within the given bounding
 *	box, and adds them to the input vector. The vector is cleared first.
 *	@return The count of chunks in the vector.
 */
int EditorChunk::findOutsideChunks(
		const BoundingBox & bb,
		ChunkPtrVector & outVector,
		bool assertExistence )
{
	outVector.clear();

	ChunkSpacePtr pSpace = ChunkManager::instance().cameraSpace();
	if (!pSpace) return 0;

	// go through all the columns that overlap this bounding box
	for (int x = ChunkSpace::pointToGrid( bb.minBounds().x );
		x <= ChunkSpace::pointToGrid( bb.maxBounds().x );
		x++)
		for (int z = ChunkSpace::pointToGrid( bb.minBounds().z );
			z <= ChunkSpace::pointToGrid( bb.maxBounds().z );
			z++)
	{
		const Vector3 apt( ChunkSpace::gridToPoint( x ) + GRID_RESOLUTION*0.5f,
			0, ChunkSpace::gridToPoint( z ) + GRID_RESOLUTION*0.5f );

		// extract their outside chunk
		ChunkSpace::Column * pColumn = pSpace->column( apt, false );
		if (pColumn != NULL && pColumn->pOutsideChunk() != NULL)
			outVector.push_back( pColumn->pOutsideChunk() );
		else if (assertExistence)
		{
			CRITICAL_MSG( "EditorChunk::findOutsideChunks: "
				"No focussed outside chunk at (%f,%f,%f) when required\n",
				apt.x, apt.y, apt.z );
		}
	}

	return outVector.size();
}


/**
 *	This method determines whether or not the outside chunks at the given
 *	position exists, is loaded, and is writeable.
 */
bool EditorChunk::outsideChunkWriteable( const Vector3 & position )
{
	ChunkSpacePtr pSpace = ChunkManager::instance().cameraSpace();
	if (!pSpace) return false;

	ChunkSpace::Column * pColumn = pSpace->column( position, false );

	if (pColumn == NULL) return false;
	if (pColumn->pOutsideChunk() == NULL) return false;

	return EditorChunkCache::instance( *pColumn->pOutsideChunk() ).
		edIsWriteable();
}

/**
 *	This method determines whether or not all the outside chunks in the given
 *	bounding box exist, are loaded, and are writeable.
 */
bool EditorChunk::outsideChunksWriteable( const BoundingBox & bb )
{
	ChunkSpacePtr pSpace = ChunkManager::instance().cameraSpace();
	if (!pSpace) return false;

	// go through all the columns that overlap this bounding box
	for (int x = ChunkSpace::pointToGrid( bb.minBounds().x );
		x <= ChunkSpace::pointToGrid( bb.maxBounds().x );
		x++)
		for (int z = ChunkSpace::pointToGrid( bb.minBounds().z );
			z <= ChunkSpace::pointToGrid( bb.maxBounds().z );
			z++)
	{
		const Vector3 apt( ChunkSpace::gridToPoint( x ) + GRID_RESOLUTION*0.5f,
			0, ChunkSpace::gridToPoint( z ) + GRID_RESOLUTION*0.5f );

		// extract their outside chunk and make sure it is writeable
		ChunkSpace::Column * pColumn = pSpace->column( apt, false );
		if (pColumn == NULL) return false;
		if (pColumn->pOutsideChunk() == NULL) return false;
		if (!EditorChunkCache::instance( *pColumn->pOutsideChunk() ).
			edIsWriteable()) return false;
	}

	return true;
}




// -----------------------------------------------------------------------------
// Section: ChunkMatrixOperation
// -----------------------------------------------------------------------------

/**
 *	Cosntructor
 */
ChunkMatrixOperation::ChunkMatrixOperation( Chunk * pChunk, const Matrix & oldPose ) :
		UndoRedo::Operation( int(typeid(ChunkMatrixOperation).name()) ),
		pChunk_( pChunk ),
		oldPose_( oldPose )
{
	addChunk( pChunk_ );
}

void ChunkMatrixOperation::undo()
{
	// first add the current state of this block to the undo/redo list
	UndoRedo::instance().add( new ChunkMatrixOperation(
		pChunk_, pChunk_->transform() ) );

	// now change the matrix back
	EditorChunkCache::instance( *pChunk_ ).edTransform( oldPose_ );
}

bool ChunkMatrixOperation::iseq( const UndoRedo::Operation & oth ) const
{
	return pChunk_ ==
		static_cast<const ChunkMatrixOperation&>( oth ).pChunk_;
}



// -----------------------------------------------------------------------------
// Section: ChunkMatrix
// -----------------------------------------------------------------------------


/**
 *	This class handles the internals of moving a chunk around
 */
class ChunkMatrix : public MatrixProxy, public Aligned
{
public:
	ChunkMatrix( Chunk * pChunk );
	~ChunkMatrix();

	virtual void EDCALL getMatrix( Matrix & m, bool world );
	virtual void EDCALL getMatrixContext( Matrix & m );
	virtual void EDCALL getMatrixContextInverse( Matrix & m );
	virtual bool EDCALL setMatrix( const Matrix & m );

	virtual void EDCALL recordState();
	virtual bool EDCALL commitState( bool revertToRecord, bool addUndoBarrier );

	virtual bool EDCALL hasChanged();

private:
	Chunk *			pChunk_;
	Matrix			origPose_;
	Matrix			curPose_;
};


/**
 *	Constructor.
 */
ChunkMatrix::ChunkMatrix( Chunk * pChunk ) :
	pChunk_( pChunk ),
	origPose_( Matrix::identity ),
	curPose_( Matrix::identity )
{
}

/**
 *	Destructor
 */
ChunkMatrix::~ChunkMatrix()
{
}


void ChunkMatrix::getMatrix( Matrix & m, bool world )
{
	m = pChunk_->transform();
}

void ChunkMatrix::getMatrixContext( Matrix & m )
{
	m = Matrix::identity;
}

void ChunkMatrix::getMatrixContextInverse( Matrix & m )
{
	m = Matrix::identity;
}

bool ChunkMatrix::setMatrix( const Matrix & m )
{
	curPose_ = m;
	EditorChunkCache::instance( *pChunk_ ).edTransform( m, true );
	return true;
}

void ChunkMatrix::recordState()
{
	origPose_ = pChunk_->transform();
	curPose_ = pChunk_->transform();
}

bool ChunkMatrix::commitState( bool revertToRecord, bool addUndoBarrier )
{
	// reset the transient transform first regardless of what happens next
	EditorChunkCache::instance( *pChunk_ ).edTransform( origPose_, true );

	// ok, see if we're going ahead with this
	if (revertToRecord)
		return false;

	// if we're not reverting check a few things
	bool okToCommit = true;
	{
		BoundingBox spaceBB(ChunkManager::instance().cameraSpace()->gridBounds());
		BoundingBox chunkBB(pChunk_->localBB());
		chunkBB.transformBy(curPose_);
		if ( !(spaceBB.intersects( chunkBB.minBounds() ) &&
				spaceBB.intersects( chunkBB.maxBounds() )) )
		{
			okToCommit = false;
		}

		// make sure it's not an immovable outside chunk
		//  (this test probably belongs somewhere higher)
		if (pChunk_->isOutsideChunk())
		{
			okToCommit = false;
		}
	}

	// add the undo operation for it
	UndoRedo::instance().add(
		new ChunkMatrixOperation( pChunk_, origPose_ ) );

	// set the barrier with a meaningful name
	if (addUndoBarrier)
	{
		UndoRedo::instance().barrier(
			"Move Chunk " + pChunk_->identifier(), false );
		// TODO: Don't always say 'Move ' ...
		//  figure it out from change in matrix
	}

	// check here, so push on an undo for multiselect
	if ( !okToCommit )
		return false;

	// and finally set the matrix permanently
	EditorChunkCache::instance( *pChunk_ ).edTransform( curPose_, false );
	return true;
}


bool ChunkMatrix::hasChanged()
{
	return !(origPose_ == pChunk_->transform());
}


// -----------------------------------------------------------------------------
// Section: EditorChunkCache
// -----------------------------------------------------------------------------
std::set<Chunk*> EditorChunkCache::chunks_;
static SimpleMutex chunksMutex;

void EditorChunkCache::lock()
{
	chunksMutex.grab();
}

void EditorChunkCache::unlock()
{
	chunksMutex.give();
}

/**
 *	Constructor
 */
EditorChunkCache::EditorChunkCache( Chunk & chunk ) :
	chunk_( chunk ),
	present_( true ),
	deleted_( false ),
	pChunkSection_( NULL ),
	pCDataSection_( NULL ),
	updateFlags_(),
	navmeshDirty_( true )
{
	SimpleMutexHolder permission( chunksMutex );
	chunks_.insert( &chunk );
	chunkResourceID_ = chunk_.resourceID();
}

EditorChunkCache::~EditorChunkCache()
{
	SimpleMutexHolder permission( chunksMutex );
	chunks_.erase( chunks_.find( &chunk_ ) );
	// Make sure next time the chunk is loaded, it'll be loaded from disk,
	// because the editor changes the chunk's data section in memory while
	// editing.
	BWResource::instance().purge( chunkResourceID_ );
}

/**
 *	Load this chunk. We just save the data section pointer
 */
bool EditorChunkCache::load( DataSectionPtr pSec )
{
	// Don't let any chunks get unloaded by the editor, simply makes things faster for the artists
//	chunk_.removable(false);

	updateFlags_.lighting_ = updateFlags_.shadow_ = updateFlags_.thumbnail_ = 0;
	if( DataSectionPtr flagSec = pCDataSection()->openSection( "dirtyFlags" ) )
	{
		BinaryPtr bp = flagSec->asBinary();
		if( bp->len() == sizeof( updateFlags_ ) )
			updateFlags_ = *(UpdateFlags*)bp->cdata();
	}
	if( DataSectionPtr navmeshSec = pCDataSection()->openSection( "navmeshDirty" ) )
	{
		BinaryPtr bp = navmeshSec->asBinary();
		if( bp->len() == sizeof( NavMeshDirtyType ) )
			navmeshDirty_ = *(NavMeshDirtyType*)bp->cdata();
	}
	pChunkSection_ = pSec;

	MF_ASSERT( pSec );

	// Remove the sections marked invalid from the load.
	std::vector<DataSectionPtr>::iterator it = invalidSections_.begin();
	for (;it!=invalidSections_.end();it++)
	{
		pChunkSection_->delChild( (*it) );
	}
	invalidSections_.clear();
	return true;
}

void EditorChunkCache::addInvalidSection( DataSectionPtr section )
{
	invalidSections_.push_back( section );
}

void EditorChunkCache::bind( bool looseNotBind )
{
	// Mark us as dirty if we weren't brought fully up to date in a previous session
	if( !looseNotBind )
		BigBang::instance().checkUpToDate( &chunk_ );

	MatrixMutexHolder lock( &chunk_ );
	std::vector<ChunkItemPtr>::iterator i = chunk_.selfItems_.begin();
	for (; i != chunk_.selfItems_.end(); ++i)
		(*i)->edChunkBind();
}

/**
 *	Reload the bounds of this chunk.
 */
void EditorChunkCache::reloadBounds()
{
	Matrix xform = chunk_.transform();

	this->takeOut();

	Chunk* pChunk = &chunk_;

	// Remove the portal items, as they refer to the boundary objects we're
	// about to delete
	{
		MatrixMutexHolder lock( &chunk_ );
		for (int i = chunk_.selfItems_.size()-1; i >= 0; i--)
		{
			DataSectionPtr ds = chunk_.selfItems_[i]->pOwnSect();
			if (ds && ds->sectionName() == "portal")
				chunk_.delStaticItem( chunk_.selfItems_[i] );
		}
	}

	chunk_.bounds_.clear();
	chunk_.joints_.clear();


	chunk_.boundingBox_.setBounds(
							pChunkSection_->readVector3( "boundingBox/min" ),
							pChunkSection_->readVector3( "boundingBox/max" ) );
	chunk_.boundingBox_.transformBy( chunk_.pMapping_->mapper() );

	chunk_.localBB_ = chunk_.boundingBox_;
	chunk_.localBB_.transformBy( chunk_.transformInverse_ );

	chunk_.loadBoundaries( pChunkSection_ );



	chunk_.transform( xform );
	updateDataSectionWithTransform();

	// ensure the focus grid is up to date
	ChunkManager::instance().camera( Moo::rc().invView(),
		ChunkManager::instance().cameraSpace() );

	ChunkPyCache::instance( chunk_ ).createPortalsAgain();

	this->putBack();
}

/**
 *	Touch this chunk. We make sure there's one of us in every chunk.
 */
void EditorChunkCache::touch( Chunk & chunk )
{
	EditorChunkCache::instance( chunk );
}


/**
 *	Save this chunk and any items in it back to the XML file
 */
bool EditorChunkCache::edSave()
{
	if (!pChunkSection_)
	{
		BigBang::instance().addError( &chunk_, NULL,
			"EditorChunkCache::edSave: "
			"Tried to save chunk %s without recorded DataSection!",
			chunk_.identifier().c_str() );
		return false;
	}

	if (!edIsLocked())
	{
		BigBang::instance().addError( &chunk_, NULL,
			"EditorChunkCache::edSave: "
			"Tried to save chunk %s when it's not locked!",
			chunk_.identifier().c_str() );
		return false;
	}

	// figure out what resource this chunk lives in
	std::string resourceID = chunk_.resourceID();

	// see if we're deleting it
	if (deleted_ && present_)
	{
		// delete the resource
		BigBang::instance().eraseAndRemoveFile( resourceID );

		// also check for deletion of the corresponding .cdata file
		std::string binResourceID = chunk_.binFileName();
		if (BWResource::fileExists( binResourceID ))
		{
			BigBang::instance().eraseAndRemoveFile( binResourceID );
		}

		// record that it's not here
		present_ = false;
		return true;
	}
	// see if we deleted it in the same session we created it
	else if (deleted_ && !present_)
	{
		return true;
	}
	// see if we're creating it
	else if (!deleted_ && !present_)
	{
		// it'll get saved to the right spot below

		// the data section cache and census will be well out of whack,
		// but that's OK because everything should be using our own
		// stored datasection variable and bypassing those.

		// record that it's here
		present_ = true;
	}

	// first rewrite the boundary information
	//  (due to portal connection changes, etc)

	// delete all existing sections
	DataSectionPtr pDS;
	while( (pDS = pChunkSection_->openSection( "boundary" )) )
	{
		pChunkSection_->delChild( pDS );
	}

	// write all the joints
	std::set<ChunkBoundaryPtr>	gotBounds;
	for (ChunkBoundaries::iterator bit = chunk_.joints().begin();
		bit != chunk_.joints().end();
		bit++)
	{
		pDS = pChunkSection_->newSection( "boundary" );
		pDS->writeVector3( "normal", (*bit)->plane_.normal() );
		pDS->writeFloat( "d", (*bit)->plane_.d() );

		for (ChunkBoundary::Portals::iterator pit = (*bit)->unboundPortals_.begin();
			pit != (*bit)->unboundPortals_.end(); 
			pit++)
		{
			ChunkBoundary::Portal* ptl = *pit;
			ptl->save( pDS, chunk_.mapping() );
		}

		// TODO: modify size of bound portals to the smallest common area
		for (ChunkBoundary::Portals::iterator pit = (*bit)->boundPortals_.begin();
			pit != (*bit)->boundPortals_.end(); 
			pit++)
		{
			ChunkBoundary::Portal* ptl = *pit;
			ptl->save( pDS, chunk_.mapping() );
		}

/*
		if (pit == (*bit)->unboundPortals_.end())
			pit = (*bit)->boundPortals_.begin();
		for (; pit != (*bit)->boundPortals_.end(); pit++)
		{
			ChunkBoundary::Portal* ptl = *pit;
			ptl->save( pDS, chunk_.mapping() );

			if (pit == (*bit)->unboundPortals_.end() - 1)
				pit = (*bit)->boundPortals_.begin() - 1;
		}
*/
		gotBounds.insert( *bit );
	}

	// make sure we didn't miss any bounds with no joints
	for (ChunkBoundaries::iterator bit = chunk_.bounds().begin();
		bit != chunk_.bounds().end();
		bit++)
	{
		if (gotBounds.find( *bit ) != gotBounds.end()) continue;

		pDS = pChunkSection_->newSection( "boundary" );
		pDS->writeVector3( "normal", (*bit)->plane_.normal() );
		pDS->writeFloat( "d", (*bit)->plane_.d() );
	}

	// give the items a chance to save any changes
	{
		MatrixMutexHolder lock( &chunk_ );
		std::vector<ChunkItemPtr>::iterator i = chunk_.selfItems_.begin();
		for (; i != chunk_.selfItems_.end(); ++i)
			(*i)->edChunkSave();
	}

	// update the bounding box and transform
	updateDataSectionWithTransform();

	// if we don't have a .terrain file, make sure to cvs remove it
	if (!ChunkTerrainCache::instance( chunk_ ).pTerrain() && chunk_.isOutsideChunk())
	{
		std::string terrainResource = chunk_.mapping()->path() +
			ChunkTerrain::resourceID( chunk_.identifier() );

		if (BWResource::fileExists( terrainResource ))
			BigBang::instance().eraseAndRemoveFile( terrainResource );
	}


	// now save out the datasection to the file
	//  (with any changes made by items to themselves)

	bool add = !BWResource::fileExists( resourceID );

	if (add)
		CVSWrapper( BigBang::instance().getCurrentSpace() ).addFile( chunk_.identifier() + ".chunk", false );

	pChunkSection_->save( resourceID );

	if (add)
		CVSWrapper( BigBang::instance().getCurrentSpace() ).addFile( chunk_.identifier() + ".chunk", false );


	// save the binary data
	return edSaveCData();
}


/*
 *	Save the binary data, such as lighting to the .cdata file
 */
bool EditorChunkCache::edSaveCData()
{
	// retrieve (and possibly create) our .cData file
	DataSectionPtr cData = this->pCDataSection();

	// delete lighting section, if any
	if ( cData->findChild( "lighting" ) )
		cData->delChild( "lighting" );

	MF_ASSERT(cData);

	{
		MatrixMutexHolder lock( &chunk_ );
		std::vector<ChunkItemPtr>::iterator i = chunk_.selfItems_.begin();
		for (; i != chunk_.selfItems_.end(); ++i)
			(*i)->edChunkSaveCData(cData);
	}

	// save thumbnail, if it exists
	if (cData->openSection( "thumbnail.dds" ) )
	{
		DataSectionPtr tSect = cData->openSection( "thumbnail.dds" );
		if ( tSect )
		{
			tSect->setParent( cData );
			tSect->save();
		}
	}

	if( DataSectionPtr flagSec = cData->openSection( "dirtyFlags", true ) )
	{
		flagSec->setBinary( new BinaryBlock( &updateFlags_, sizeof( updateFlags_ ) ) );
		flagSec->setParent( cData );
		flagSec->save();
	}

	if( DataSectionPtr navmeshSec = cData->openSection( "navmeshDirty", true ) )
	{
		navmeshSec->setBinary( new BinaryBlock( &navmeshDirty_, sizeof( NavMeshDirtyType ) ) );
		navmeshSec->setParent( cData );
		navmeshSec->save();
	}

	// check to see if need to save to dist (if already exists or there is data)
	if (cData->asBinary()->len() > 0)
	{
		const std::string fileName = chunk_.binFileName();
		bool add = !BWResource::fileExists( fileName );

		if (add)	// just in case its been deleted without cvs knwledge
			CVSWrapper( BigBang::instance().getCurrentSpace() ).addFile( chunk_.identifier() + ".cdata", true );

		// save to disk
		if ( !cData->save(fileName) )
		{
			BigBang::instance().addError( &chunk_, NULL,
				"EditorChunkCache::edSaveCData: Unable to open file \"%s\"", fileName.c_str() );
			return false;
		}

		if (add)	// let cvs know about the file
			CVSWrapper( BigBang::instance().getCurrentSpace() ).addFile( chunk_.identifier() + ".cdata", true );
	}
	return true;
}

/**
 *	Change the transform of this chunk, either transiently or permanently,
 *  either clear snapping history or not
 */
bool EditorChunkCache::doTransform( const Matrix & m, bool transient, bool cleanSnappingHistory )
{
	Matrix invChunkTransform = chunk_.transform();
	invChunkTransform.invert();
	std::set<ChunkItemPtr> itemsToMoveManually;
	std::map<ChunkItemPtr, Matrix> itemsMatrices;
	std::set<ChunkItemPtr> itemsToRemove;
	for( Chunk::Items::iterator iter = chunk_.selfItems_.begin();
		iter != chunk_.selfItems_.end(); ++iter )
	{
		if( !(*iter)->edIsPositionRelativeToChunk() )
		{
			if( (*iter)->edBelongToChunk() )
			{
				itemsToMoveManually.insert( *iter );
				itemsMatrices[ *iter ] = (*iter)->edTransform();
			}
			else
			{
				itemsToRemove.insert( *iter );
			}
		}
	}

	// if it's transient that's easy
	if (transient)
	{
		BoundingBox chunkBB = chunk_.localBB();
		chunkBB.transformBy( m );

		BoundingBox spaceBB(ChunkManager::instance().cameraSpace()->gridBounds());

		if ( !(spaceBB.intersects( chunkBB.minBounds() ) &&
			spaceBB.intersects( chunkBB.maxBounds() )) )
			return false;

		chunk_.transformTransiently( m );

		for( std::set<ChunkItemPtr>::iterator iter = itemsToMoveManually.begin();
			iter != itemsToMoveManually.end(); ++iter )
		{
			(*iter)->edTransform( itemsMatrices[ *iter ], true );
		}

		return true;
	}

	if( cleanSnappingHistory )
		clearSnapHistory();

	// check that our source and destination are both loaded and writeable
	// (we are currently limited to movements within the focus grid...)
	if (!EditorChunk::outsideChunksWriteable( chunk_.boundingBox() ))
		return false;

	// ok, let's do the whole deal then

	int oldLeft = ChunkSpace::pointToGrid( chunk_.boundingBox().minBounds().x );
	int oldTop = ChunkSpace::pointToGrid( chunk_.boundingBox().minBounds().z );

	BoundingBox newbb = chunk_.localBB();
	newbb.transformBy( m );
	if (!EditorChunk::outsideChunksWriteable( newbb ))
		return false;

	int newLeft = ChunkSpace::pointToGrid( newbb.minBounds().x );
	int newTop = ChunkSpace::pointToGrid( newbb.minBounds().z );

	BWLock::BigBangdConnection* conn = BigBang::instance().connection();
	if( conn )
		conn->linkPoint( oldLeft, oldTop, newLeft, newTop );

	// make our lights mark the chunks they influence as dirty, provided we're
	// actually connected to something
	if (chunk_.pbegin() != chunk_.pend())
		StaticLighting::StaticChunkLightCache::instance( chunk_ ).markInfluencedChunksDirty();

	for( std::set<ChunkItemPtr>::iterator iter = itemsToRemove.begin();
		iter != itemsToRemove.end(); ++iter )
	{
		// delete it now
		chunk_.delStaticItem( *iter );

		// set up an undo which creates it
		UndoRedo::instance().add(
			new VLOExistenceOperation( *iter, &chunk_ ) );
	}
	// take it out of this space
	this->takeOut();

	// move it
	chunk_.transform( m );

	updateDataSectionWithTransform();

	// flag it as dirty
	BigBang::instance().changedChunk( &chunk_ );
	BigBang::instance().markTerrainShadowsDirty( chunk_.boundingBox() );

	// put it back in the space
	this->putBack();

	for( std::set<ChunkItemPtr>::iterator iter = itemsToMoveManually.begin();
		iter != itemsToMoveManually.end(); ++iter )
	{
		(*iter)->edTransform( itemsMatrices[ *iter ], false );
	}

	// make our lights mark the chunks they now influence as dirty provided we're
	// actually connected to something
	if (chunk_.pbegin() != chunk_.pend())
		StaticLighting::StaticChunkLightCache::instance( chunk_ ).
			markInfluencedChunksDirty();
	return true;
}


/**
 *	Change the transform of this chunk, either transiently or permanently,
 */
bool EditorChunkCache::edTransform( const Matrix & m, bool transient )
{
	return doTransform( m, transient, !transient );
}


/**
 *	Change the transform of this chunk, called from snapping functions
 */
bool EditorChunkCache::edTransformClone( const Matrix & m )
{
	return doTransform( m, false, false );
}


/** Write the current transform out to the datasection */
void EditorChunkCache::updateDataSectionWithTransform()
{
	pChunkSection_->delChild( "transform" );
	pChunkSection_->writeMatrix34( "transform", chunk_.transform() );
	pChunkSection_->delChild( "boundingBox" );
	DataSectionPtr pDS = pChunkSection_->newSection( "boundingBox" );
	pDS->writeVector3( "min", chunk_.boundingBox().minBounds() );
	pDS->writeVector3( "max", chunk_.boundingBox().maxBounds() );
}


/**
 *	This method is called when a chunk arrives on the scene.
 *	It saves out a file for it from its data section (assumed to already
 *	be in pChunkSection_) then binds it into its space.
 */
void EditorChunkCache::edArrive( bool fromNowhere )
{
	// clear the present flag if this is a brand new chunk
	if (fromNowhere) present_ = false;

	// clear the delete on save flag
	deleted_ = false;

	// flag the chunk as dirty
	BigBang::instance().changedChunk( &chunk_ );

	// and add it back in to the space
	this->putBack();

	// We need to do this, as the chunk may be transformed before
	// being added (ie, when creating it), but we can't call edTransform
	// before edArrive, thus we simply save the transform here
	updateDataSectionWithTransform();


	// We also need to put this here for a hack when creating multiple
	// chunks in a single frame (ie, when undoing a delete operation)
	// otherwise the portals won't be connected
	ChunkManager::instance().camera( Moo::rc().invView(), ChunkManager::instance().cameraSpace() );

	// if we have any lights in the chunk (eg, we just got cloned ) then
	// mark us and our surrounds as dirty
	StaticLighting::StaticChunkLightCache::instance( chunk_ ).markInfluencedChunksDirty();
}

/**
 *	This method is called when a chunk departs from the scene.
 */
void EditorChunkCache::edDepart()
{
	// take it out of the space
	this->takeOut();

	// flag the chunk as dirty
	BigBang::instance().changedChunk( &chunk_ );

	// set the chunk to delete on save
	deleted_ = true;
}

/**
 * Check that all our items are cool with being deleted
 */
bool EditorChunkCache::edCanDelete()
{
	MatrixMutexHolder lock( &chunk_ );
	std::vector<ChunkItemPtr>::iterator i = chunk_.selfItems_.begin();
	for (; i != chunk_.selfItems_.end(); ++i)
		if (!(*i)->edCanDelete())
			return false;

	return true;
}

/**
 * Inform our items that they'll be deleted
 */
void EditorChunkCache::edPreDelete()
{
    // We cannot simply iterate through selfItems_ and call edPreDelete on 
    // each.  This is because some items (such as patrol path nodes) can delete
    // items (such as links) in selfItems_.  Iterating through selfItems_ then
    // becomes invalid.  Instead we create a second copy of selfItems_ and 
    // iterate through it.  For each item we check that the item is still in 
    // selfItems_ before calling edPreDelete.
	MatrixMutexHolder lock( &chunk_ );
    std::vector<ChunkItemPtr> &items = chunk_.selfItems_;
    std::vector<ChunkItemPtr> origItems = items;
	for 
    (
        std::vector<ChunkItemPtr>::iterator i = origItems.begin(); 
        i != origItems.end(); 
        ++i
    )
    {
        if (std::find(items.begin(), items.end(), *i) != items.end())
		{
			if( (*i)->edBelongToChunk() )
				(*i)->edPreDelete();
			else
			{
				// delete it now
				chunk_.delStaticItem( (*i) );

				// set up an undo which creates it
				UndoRedo::instance().add(
					new VLOExistenceOperation( (*i), &chunk_ ) );
			}
		}
    }
}

void EditorChunkCache::edPostClone(bool keepLinks)
{
	MatrixMutexHolder lock( &chunk_ );
	if (keepLinks)
	{
		std::vector<ChunkItemPtr>::iterator i = chunk_.selfItems_.begin();
		for (; i != chunk_.selfItems_.end(); ++i)
		{
			if ( ((*i)->edDescription() != "marker") &&
				((*i)->edDescription() != "marker cluster") &&
				((*i)->edDescription() != "patrol node") )
			{
				(*i)->edPostClone( NULL );
			}
		}
	}
	else
	{
		std::vector<ChunkItemPtr>::iterator i = chunk_.selfItems_.begin();
		for (; i != chunk_.selfItems_.end(); ++i)
			(*i)->edPostClone( NULL );
	}
}

/**
 *	This method takes a chunk out of its space
 */
void EditorChunkCache::takeOut()
{
	// flag all chunks it's connected to as dirty
	for (Chunk::piterator pit = chunk_.pbegin(); pit != chunk_.pend(); pit++)
	{
		if (pit->hasChunk())
		{
			// should not be in bound portals list if it's not online!
			MF_ASSERT( pit->pChunk->online() );

			BigBang::instance().changedChunk( pit->pChunk );
		}
	}

	// go through all the outside chunks we overlap
	ChunkPtrVector outsideChunks;
	EditorChunk::findOutsideChunks( chunk_.boundingBox(), outsideChunks, true );
	for (uint i = 0; i < outsideChunks.size(); i++)
	{
		// delete the overlapper item pointing to it (if present)
		ChunkOverlappers::instance( *outsideChunks[i] ).cut( &chunk_ );
	}

	// cut it loose from its current position
	chunk_.loose( true );

	// ensure the focus grid is up to date
	ChunkManager::instance().camera( Moo::rc().invView(),
		ChunkManager::instance().cameraSpace() );
}

/**
 *	This method puts a chunk back in its space
 */
void EditorChunkCache::putBack()
{
	// bind it to its new position (a formative bind)
	chunk_.bind( true );

	// go through all the outside chunks we overlap
	ChunkPtrVector outsideChunks;
	EditorChunk::findOutsideChunks( chunk_.boundingBox(), outsideChunks, true );
	for (uint i = 0; i < outsideChunks.size(); i++)
	{
		// create an overlapper item pointing to it
		ChunkOverlappers::instance( *outsideChunks[i] ).form( &chunk_ );
	}

	// flag all the new connections as dirty too
	for (Chunk::piterator pit = chunk_.pbegin(); pit != chunk_.pend(); pit++)
	{
		if (pit->hasChunk())
		{
			// should not be in bound portals list if it's not online!
			MF_ASSERT( pit->pChunk->online() );

			BigBang::instance().changedChunk( pit->pChunk );
		}
	}

	// ensure the focus grid is up to date
	ChunkManager::instance().camera( Moo::rc().invView(),
		ChunkManager::instance().cameraSpace() );
}


/**
 *	Add the properties of this chunk to the given editor
 */
void EditorChunkCache::edEdit( ChunkEditor & editor )
{
	editor.addProperty( new StaticTextProperty(
		"identifier", new ConstantDataProxy<StringProxy>(
			chunk_.identifier() ) ) );

	editor.addProperty( new StaticTextProperty(
		"description", new ConstantDataProxy<StringProxy>(
			"Chunk identifier " + chunk_.identifier() ) ) );

	MatrixProxy * pMP = new ChunkMatrix( &chunk_ );
	editor.addProperty( new GenPositionProperty( "position", pMP ) );
	editor.addProperty( new GenRotationProperty( "rotation", pMP ) );
}


/**
 *	Return the top level data section for this chunk
 */
DataSectionPtr EditorChunkCache::pChunkSection()
{
	return pChunkSection_;
}


/**
 *	Return and possibly create the .cdata section for this chunk.
 *
 *	@return the binary cData section for the chunk
 */
DataSectionPtr	EditorChunkCache::pCDataSection()
{
	if ( pCDataSection_ )
		return pCDataSection_;

	// check to see if file already exists
	// first clear the lighting data
	const std::string fileName = chunk_.binFileName();
	DataSectionPtr cData = BWResource::openSection( fileName, false );

	if (!cData)
	{
		// create a section
		size_t lastSep = fileName.find_last_of('/');
		std::string parentName = fileName.substr(0, lastSep);
		DataSectionPtr parentSection = BWResource::openSection( parentName );
		MF_ASSERT(parentSection);

		std::string tagName = fileName.substr(lastSep + 1);

		// make it
		cData = new BinSection( tagName, new BinaryBlock( NULL, 0 ) );
		cData->setParent( parentSection );
		cData = DataSectionCensus::add( fileName, cData );
	}

	pCDataSection_ = cData;
	return cData;
}


/**
 *	Return the first static item
 *	(for internal chunks, this should be the shell model)
 */
ChunkItemPtr EditorChunkCache::getShellModel() const
{
	MF_ASSERT( !chunk_.isOutsideChunk() );
	MatrixMutexHolder lock( &chunk_ );
	if (!chunk_.selfItems_.size()) 
		return NULL;

	return chunk_.selfItems_.front();
}

/**
 * Return all chunk items in the chunk
 */
std::vector<ChunkItemPtr> EditorChunkCache::staticItems() const
{
	return chunk_.selfItems_;
}

/**
 *	Get all the items in this chunk
 */
void EditorChunkCache::allItems( std::vector<ChunkItemPtr> & items ) const
{
	items.clear();
	MatrixMutexHolder lock( &chunk_ );
	items = chunk_.selfItems_;
	items.insert( items.end(),
		chunk_.dynoItems_.begin(), chunk_.dynoItems_.end() );
}


/**
 *	Recalculate the lighting for this chunk
 */
bool EditorChunkCache::edRecalculateLighting()
{
	MF_ASSERT( chunk_.online() );
	MF_ASSERT( pChunkSection() );
	MF_ASSERT( !chunk_.isOutsideChunk() );

	INFO_MSG( "recalculating lighting for chunk %s\n", chunk_.identifier().c_str() );

	// #1: Find all the lights influencing this chunk
	StaticLighting::StaticLightContainer lights;
	lights.ambient( ChunkLightCache::instance( chunk_ ).pOwnLights()->ambientColour() );

	if (!StaticLighting::findLightsInfluencing( &chunk_, &chunk_, &lights))
		return false;

	// #2: Get all the EditorChunkModels to recalculate their lighting
	// Create a copy of the vector first, incase any items are removed while
	// recalculating the lighting
	std::vector<ChunkItemPtr> chunkItems;
	{
		MatrixMutexHolder lock( &chunk_ );
		chunkItems = chunk_.selfItems_;
	}
	std::vector<ChunkItemPtr>::iterator i = chunkItems.begin();
	for (; i != chunkItems.end() && BigBang::instance().isWorkingChunk( &chunk_ ); ++i)
	{
		MF_ASSERT( *i );
		if ((*i)->pOwnSect() &&
			( (*i)->pOwnSect()->sectionName() == "model" ||
			(*i)->pOwnSect()->sectionName() == "shell" ) )
			static_cast<EditorChunkModel*>( &*(*i) )->edRecalculateLighting( lights );
		BigBang::instance().fiberPause();
	}

	// #3: Mark ourself as changed
	lightingUpdated( true );
	BigBang::instance().changedChunk( &chunk_ );

	INFO_MSG( "finished calculating lighting for %s\n", chunk_.identifier().c_str() );

	return true;
}


bool EditorChunkCache::calculateThumbnail()
{
	return ChunkPhotographer::photograph( chunk_ );
}


namespace
{
	int worldToGridCoord( float w )
	{
		int g = int(w / GRID_RESOLUTION);
		
		if (w < 0.f)
			g--;

		return g;
	}
}

bool EditorChunkCache::edIsLocked()
{
	BWLock::BigBangdConnection* conn = BigBang::instance().connection();

	if( !conn )
		return true;

	// Use bb.centre, as the chunk may not be online, which means it's own
	// centre won't be valid
	Vector3 centre = chunk_.boundingBox().centre();
	ChunkDirMapping* dirMap = BigBang::instance().chunkDirMapping();
	centre = dirMap->invMapper().applyPoint( chunk_.centre() );
	int gridX = worldToGridCoord( centre.x );
	int gridZ = worldToGridCoord( centre.z );

	return conn->isLockedByMe( gridX, gridZ );
}

bool EditorChunkCache::edIsWriteable()
{
	BWLock::BigBangdConnection* conn = BigBang::instance().connection();

	struct Cache : BWLock::BWLockdCache
	{
		virtual void invalidate()
		{
			isWritable_.clear();
		}
		std::map<std::pair<int, int>,bool> isWritable_;
	};
	static Cache cache;

	if( !conn )
		return true;

	// Use bb.centre, as the chunk may not be online, which means it's own
	// centre won't be valid
	Vector3 centre = chunk_.boundingBox().centre();
	ChunkDirMapping* dirMap = BigBang::instance().chunkDirMapping();
	centre = dirMap->invMapper().applyPoint( chunk_.centre() );
	int gridX = worldToGridCoord( centre.x );
	int gridZ = worldToGridCoord( centre.z );;

	if( cache.isWritable_.find( std::make_pair( gridX, gridZ ) ) != cache.isWritable_.end() )
		return cache.isWritable_[ std::make_pair( gridX, gridZ ) ];

	static const int dist = (int)( ( MAX_TERRAIN_SHADOW_RANGE + 1.f ) / GRID_RESOLUTION );

	for (int x = -dist; x < dist + 1; ++x)
	{
		for (int y = -1; y < 2; ++y)
		{
			int curX = gridX + x;
			int curY = gridZ + y;

			if (!conn->isLockedByMe(curX,curY))
			{
				cache.isWritable_[ std::make_pair( gridX, gridZ ) ] = false;
				return false;
			}
		}
	}
	cache.isWritable_[ std::make_pair( gridX, gridZ ) ] = true;
	return true;
}


/**
 *	Inform the terrain cache of the first terrain item in the chunk
 *
 *	This is required as the cache only knows of a single item, and
 *	the editor will sometimes allow multiple blocks in the one chunk
 *	(say, when cloning).
 */
void EditorChunkCache::fixTerrainBlocks()
{
	if (ChunkTerrainCache::instance( chunk_ ).pTerrain())
		return;

	MatrixMutexHolder lock( &chunk_ );
	std::vector<ChunkItemPtr>::iterator i = chunk_.selfItems_.begin();
	for (; i != chunk_.selfItems_.end(); ++i)
	{
		ChunkItemPtr item = *i;
		if (item->edClassName() == std::string("ChunkTerrain"))
		{
			item->toss(item->chunk());
			break;
		}
	}
}





/// Static instance accessor initialiser
ChunkCache::Instance<EditorChunkCache> EditorChunkCache::instance;






// -----------------------------------------------------------------------------
// Section: ChunkExistenceOperation
// -----------------------------------------------------------------------------

void ChunkExistenceOperation::undo()
{
	// first add the redo operation
	UndoRedo::instance().add(
		new ChunkExistenceOperation( pChunk_, !create_ ) );

	std::vector<ChunkItemPtr> selection = BigBang::instance().selectedItems();

	// now create or delete it
	if (create_)
	{
		EditorChunkCache::instance( *pChunk_ ).edArrive();

		selection.push_back( EditorChunkCache::instance( *pChunk_ ).getShellModel() );
	}
	else
	{
		EditorChunkCache::instance( *pChunk_ ).edDepart();

		std::vector<ChunkItemPtr>::iterator i =
			std::find( selection.begin(), selection.end(),
				EditorChunkCache::instance( *pChunk_ ).getShellModel() );

		if (i != selection.end())
			selection.erase( i );
	}

	BigBang::instance().setSelection( selection, false );
}

bool ChunkExistenceOperation::iseq( const UndoRedo::Operation & oth ) const
{
	// these operations never replace each other
	return false;
}
