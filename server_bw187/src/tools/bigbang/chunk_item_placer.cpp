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

#include "chunk_item_placer.hpp"

#include "big_bang.hpp"
#include "snaps.hpp"
#include "autosnap.hpp"
#include "item_view.hpp"
#include "chunk_placer.hpp"
#include "editor_chunk_station.hpp"
#include "appmgr/options.hpp"
#include "gizmo/tool_manager.hpp"
#include "gizmo/current_general_properties.hpp"
#include "gizmo/general_properties.hpp"
#include "gizmo/item_functor.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk.hpp"
#include "chunks/editor_chunk.hpp"
#include "pyscript/py_data_section.hpp"
#include "resmgr/bwresource.hpp"

#include "placement_presets.hpp"


DECLARE_DEBUG_COMPONENT2( "Editor", 0 )

std::set<CloneNotifier*>* CloneNotifier::notifiers_ = NULL;

/**
 * Snap two chunks together, by finding their closest matching portals
 *
 * We expect two ChunkItemRevealers as the arguments, and the first one must
 * contain the current item we're editing
 */
static PyObject * py_autoSnap( PyObject * args )
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
//static PyObject* autoSnap( ChunkItemRevealer* chunkRevealer, ChunkItemRevealer* snapToRevealer )
//{

	// make sure there's only one
	std::vector<Chunk*> snapChunks = extractChunks( chunkRevealer );
	std::vector<Chunk*> snapToChunks = extractChunks( snapToRevealer );
	if (snapChunks.size() != 1 || snapToChunks.size() != 1)
	{
		PyErr_Format( PyExc_ValueError, "BigBang.autoSnap() "
			"Must snap exactly one item to another item" );
		return NULL;
	}

	if (CurrentPositionProperties::properties().empty())
	{
		PyErr_Format( PyExc_ValueError, "BigBang.autoSnap() "
			"No current editor" );
		return NULL;
	}

	Chunk* snapToChunk = snapToChunks.front();

	Chunk* chunk = snapChunks.front();

	Matrix autoSnapTransform = findAutoSnapTransform( chunk, snapToChunk );

	if (autoSnapTransform != Matrix::identity)
	{
		// Apply the new transform to the chunk
		Matrix m = chunk->transform();

		UndoRedo::instance().add(
			new ChunkMatrixOperation( chunk, m ) );

		EditorChunkCache::instance( *chunk ).edTransformClone( autoSnapTransform );

		// set a meaningful barrier name
		UndoRedo::instance().barrier( "Auto Snap", false );

		return Py_BuildValue("i", 1);
	}
	return Py_BuildValue("i", 0);
}
PY_MODULE_FUNCTION( autoSnap, BigBang )


/*~ function BigBang.rotateSnap
 *
 *	This function rotate snap a chunk
 *
 *	@param chunks	A ChunkItemRevealer object of the chunk that is currently being rotate-snapped
 */
static PyObject * py_rotateSnap( PyObject * args )
{
	// get args
	PyObject *pPyRev1, *pPyRev2 = NULL;
	float dz;
	if (!PyArg_ParseTuple( args, "Of|O", &pPyRev1, &dz, &pPyRev2 ) ||
		!ChunkItemRevealer::Check( pPyRev1 ) ||
		(pPyRev2&&!ChunkItemRevealer::Check( pPyRev2 )))
	{
		PyErr_SetString( PyExc_ValueError, "BigBang.rotateSnap() "
			"expects one or two ChunkItemRevealer(s) and a float" );
		return NULL;
	}

	ChunkItemRevealer* rotateRevealer = static_cast<ChunkItemRevealer*>( pPyRev1 );
	ChunkItemRevealer* referenceRevealer = static_cast<ChunkItemRevealer*>( pPyRev2 );

	SnappedChunkSetSet snapChunks( extractChunks( rotateRevealer ) );
	std::vector<Chunk*> referenceChunks;
	if( referenceRevealer )
		referenceChunks = extractChunks( referenceRevealer );

	if (CurrentPositionProperties::properties().empty())
	{
		PyErr_Format( PyExc_ValueError, "BigBang.rotateSnap() "
			"No current editor" );
		return NULL;
	}

	Chunk* referenceChunk = NULL;
	
	if( referenceRevealer && !referenceChunks.empty() )
		referenceChunk = referenceChunks.front();

	int done = 0;
	for( unsigned int i = 0; i < snapChunks.size(); ++i )
	{
		SnappedChunkSet chunks = snapChunks.item( i );
		Matrix rotateSnapTransform = findRotateSnapTransform( chunks, dz > 0.f, referenceChunk );

		if (rotateSnapTransform != Matrix::identity)
		{
			for( unsigned int j = 0; j < chunks.chunkSize(); ++j )
			{
				// Apply the new transform to the chunk
				Chunk* snapChunk = chunks.chunk( j );
				Matrix m = snapChunk->transform();

				UndoRedo::instance().add(
					new ChunkMatrixOperation( snapChunk, m ) );

				m.postMultiply( rotateSnapTransform );

				EditorChunkCache::instance( *snapChunk ).edTransformClone( m );

				// set a meaningful barrier name
				UndoRedo::instance().barrier( "Auto Snap", false );
			}
			done = 1;
		}
	}
	return Py_BuildValue("i", done);
}
PY_MODULE_FUNCTION( rotateSnap, BigBang )


// This is causing problems, ergo it's commented out

//PY_AUTO_MODULE_FUNCTION( RETOWN, autoSnap,
//	NZARG( ChunkItemRevealer*, NZARG( ChunkItemRevealer*, END ) ), BigBang )


// -----------------------------------------------------------------------------
// Section: ChunkItemExistenceOperation
// -----------------------------------------------------------------------------

void ChunkItemExistenceOperation::undo()
{
	// Invalid op.
	if (!pItem_)
		return;

	// If we're removing the item, check that it's ok with that
	if (!pOldChunk_ && !pItem_->edCanDelete())
		return;

	// first add the current state of this item to the undo/redo list
	UndoRedo::instance().add(
		new ChunkItemExistenceOperation( pItem_, pItem_->chunk() ) );

	std::vector<ChunkItemPtr> selection = BigBang::instance().selectedItems();

	// now add or delete it
	if (pOldChunk_ != NULL)
	{
		pOldChunk_->addStaticItem( pItem_ );
		pItem_->edPostCreate();

			selection.push_back( pItem_ );
	}
	else
	{
		pItem_->edPreDelete();
		if (pItem_->chunk()) //a vlo reference could have been tossed out...checking
			pItem_->chunk()->delStaticItem( pItem_ );

		std::vector<ChunkItemPtr>::iterator i =
			std::find( selection.begin(), selection.end(), pItem_ );

		if (i != selection.end())
			selection.erase( i );
	}
}

// -----------------------------------------------------------------------------
// Section: VLOExistenceOperation
// -----------------------------------------------------------------------------
void VLOExistenceOperation::undo()
{
	// Invalid op.
	if (!pItem_)
		return;

	// If we're removing the item, check that it's ok with that
	if (!pOldChunk_ && !pItem_->edCanDelete())
		return;

	// first add the current state of this item to the undo/redo list
	UndoRedo::instance().add(
		new VLOExistenceOperation( pItem_, pItem_->chunk() ) );

	// now add or delete it
	if (pOldChunk_ != NULL)
	{
		pOldChunk_->addStaticItem( pItem_ );
	}
	else
	{
		if (pItem_->chunk()) //a vlo reference could have been tossed out...checking
			pItem_->chunk()->delStaticItem( pItem_ );
	}
}

// -----------------------------------------------------------------------------
// Section: ChunkItemPlacer
// -----------------------------------------------------------------------------

/**
 * The creation and deletion of chunk items is handled with a pair of functions
 * in the BigBang module, rather than with a functor. It didn't really make
 * sense to have a functor that immediately popped itself. The controlling
 * script could jump through some hoops to create and delete items, or we
 * could just let them do what they want.
 *
 *
 */
class ChunkItemPlacer
{
	typedef ChunkItemPlacer This;

public:
	PY_MODULE_STATIC_METHOD_DECLARE( py_createChunkItem )
	PY_MODULE_STATIC_METHOD_DECLARE( py_deleteChunkItems )
	PY_MODULE_STATIC_METHOD_DECLARE( py_cloneChunkItems )

private:
	static void removeFromSelected( ChunkItemPtr pItem );
};


PY_MODULE_STATIC_METHOD( ChunkItemPlacer, createChunkItem, BigBang )
PY_MODULE_STATIC_METHOD( ChunkItemPlacer, deleteChunkItems, BigBang )
PY_MODULE_STATIC_METHOD( ChunkItemPlacer, cloneChunkItems, BigBang )


/**
 *	Copy the chunk items in the given revealer
 */
PyObject * ChunkItemPlacer::py_cloneChunkItems( PyObject * args )
{
	CloneNotifier::Guard guard;

	// get args
	PyObject * pPyRev;
	PyObject * pPyLoc;

	if (!PyArg_ParseTuple( args, "OO", &pPyRev, &pPyLoc ) ||
		!ChunkItemRevealer::Check( pPyRev ) ||
		!ToolLocator::Check( pPyLoc ) )
	{
		PyErr_SetString( PyExc_ValueError, "BigBang.cloneChunkItem() "
			"expects a ChunkItemRevealer and a ToolLocator" );
		return NULL;
	}

	ChunkItemRevealer* pRevealer = static_cast<ChunkItemRevealer*>( pPyRev );
	ToolLocator* locator = static_cast<ToolLocator*>( pPyLoc );

	// make sure there's only one
	ChunkItemRevealer::ChunkItems items;
	pRevealer->reveal( items );

	std::vector<ChunkItemPtr> newItems;
    std::vector<EditorChunkStationNodePtr> copiedNodes;
    std::vector<EditorChunkStationNodePtr> newNodes;

	// now get all the positions
	Vector3 centrePos = CurrentPositionProperties::centrePosition();
	Vector3 locPos = locator->transform().applyToOrigin();
	Vector3 newPos = locPos - centrePos;
	SnapProvider::instance()->snapPositionDelta( newPos );

	Matrix offset;
	offset.setTranslate( newPos );

	// calculate all the transformation matrices first
	std::vector<Matrix> itemMatrices(items.size());
	ChunkItemPtr item;
	Chunk* pChunk;

	ChunkItemRevealer::ChunkItems::iterator i = items.begin();
	std::vector<Matrix>::iterator j = itemMatrices.begin();	
	for (; i != items.end(); ++i, ++j)
	{
		item = *i;
		pChunk = item->chunk();

		Matrix m;
		m.multiply( item->edTransform(), pChunk->transform() );
		m.postMultiply( offset );

		if ( BigBang::instance().terrainSnapsEnabled() )
		{
			Vector3 pos( m.applyToOrigin() );
			//snap to terrain only
			pos = Snap::toGround( pos );
			m.translation( pos );
		}
		else if ( BigBang::instance().obstacleSnapsEnabled() && items.size() == 1 )
		{
			Vector3 normalOfSnap = SnapProvider::instance()->snapNormal( m.applyToOrigin() );
			Vector3 yAxis( 0, 1, 0 );
			yAxis = m.applyVector( yAxis );

			Vector3 binormal = yAxis.crossProduct( normalOfSnap );

			normalOfSnap.normalise();
			yAxis.normalise();
			binormal.normalise();

			float angle = acosf( yAxis.dotProduct( normalOfSnap ) );

			Quaternion q( binormal.x * sinf( angle / 2.f ),
				binormal.y * sinf( angle / 2.f ),
				binormal.z * sinf( angle / 2.f ),
				cosf( angle / 2.f ) );

			q.normalise();

			Matrix rotation;
			rotation.setRotate( q );

			Vector3 pos( m.applyToOrigin() );

			m.translation( Vector3( 0.f, 0.f, 0.f ) );
			m.postMultiply( rotation );

			m.translation( pos );
		}
		*j = m;
	}
	
	for (i = items.begin(), j = itemMatrices.begin(); i != items.end(); ++i, ++j)
	{
		item = *i;
		pChunk = item->chunk();

		Matrix m = *j;

		if (item->isShellModel())
		{
			// Clone the chunk
			ChunkItemPtr pItem = ChunkPlacer::cloneChunk( pChunk, m );
			if (!pItem)
				continue;

			// select the appropriate shell model
			newItems.push_back( pItem );
		}
		else
		{
			BoundingBox lbb( Vector3(0.f,0.f,0.f), Vector3(1.f,1.f,1.f) );
			item->edBounds( lbb );
			Chunk* pNewChunk = pChunk->space()->findChunkFromPoint( m.applyPoint( lbb.centre() ) );
			if( pNewChunk == NULL )
				continue;

			m.postMultiply( pNewChunk->transformInverse() );

			// Clone the chunk item
			DataSectionPtr sect = item->pOwnSect();

			if (!sect)
			{
				PyErr_Format( PyExc_ValueError, "BigBang.cloneChunkItem() "
					"Item does not expose a DataSection" );
				return NULL;
			}

			// Copy the DataSection
			DataSectionPtr chunkSection = EditorChunkCache::instance( *pNewChunk ).pChunkSection();

			DataSectionPtr newSection = chunkSection->newSection( sect->sectionName() );
			item->edCloneSection( pNewChunk, m, newSection );

			// create the new item
			ChunkItemFactory::Result result = pNewChunk->loadItem( newSection );
			if ( result && result.onePerChunk() ) 
 			{ 
 				WARNING_MSG( result.errorString().c_str() ); 
 				UndoRedo::instance().add( 
 					new ChunkItemExistenceOperation( result.item(), pNewChunk ) ); 
		 	
				std::vector<ChunkItemPtr>::iterator oldItem =
					std::find( newItems.begin(), newItems.end(), result.item() );
				if ( oldItem != newItems.end() )
					newItems.erase( oldItem );
		 	
 				result = pNewChunk->loadItem( newSection ); 
 			}
			if (!result)
			{
				PyErr_SetString( PyExc_ValueError, "BigBang.cloneChunkItem() "
					"error creating item from given section" );
				return NULL;
			}

			// get the new item out of the chunk
			ChunkItemPtr pItem = result.item();
			if (!pItem)
			{
				PyErr_SetString( PyExc_EnvironmentError, "BigBang.cloneChunkItem() "
					"Couldn't create Chunk Item" );
				return NULL;
			}

			newItems.push_back( pItem );

			// call the main thread load
			pItem->edChunkBind();

			BigBang::instance().changedChunk( pNewChunk );

			// ok, everyone's happy then. so add an undo which deletes it
			UndoRedo::instance().add(
				new ChunkItemExistenceOperation( pItem, NULL ) );

			// Tell pitem we just cloned it from item
			pItem->edPostClone( &*item );

            // Add to the list of cloned nodes, if this is a node, we do some
            // special processing below:
            if (item->isEditorChunkStationNode())
            {
                EditorChunkStationNode *copiedNode = 
                    static_cast<EditorChunkStationNode *>(item.getObject());
                copiedNodes.push_back(copiedNode);
                EditorChunkStationNode *newNode =
                    static_cast<EditorChunkStationNode *>(pItem.getObject());
                newNodes.push_back(newNode);
            }
		}
	}

    if (copiedNodes.size() != 0)
    {
        EditorChunkStationNode::linkClonedNodes(copiedNodes, newNodes);
    }

    // Cloning is always done as part of a clone & move tool, don't set a barrier here	
	/*
	// set a meaningful barrier name
	if (items.size() == 1)
		UndoRedo::instance().barrier( "Clone " + newItems.front()->edDescription(), false );
	else
		UndoRedo::instance().barrier( "Clone items", false );
	*/

	// and that's it then. return a group containing the new items
	BigBang::instance().setSelection( newItems );

	return new ChunkItemGroup( newItems );
}

/**
 *	This method allows scripts to create a chunk item
 */
PyObject * ChunkItemPlacer::py_createChunkItem( PyObject * args )
{
	// get snaps
	Vector3 snaps = BigBang::instance().movementSnaps();

	// get the arguments
	PyObject * pPyDS, * pPyLoc;
	int useLocPos = 1;
	if (!PyArg_ParseTuple( args, "OO|i", &pPyDS, &pPyLoc, &useLocPos) ||
		!PyDataSection::Check(pPyDS) ||
		!ToolLocator::Check(pPyLoc))
	{
		// we need the locator to find out what chunk to create it
		// in, even if it hasn't got a transform
		PyErr_SetString( PyExc_TypeError, "BigBang.createChunkItem() "
			"expects a PyDataSection, a ToolLocator, and an optional bool" );
		return NULL;
	}

	// find out what chunk it goes in
	ToolLocator * pLoc = static_cast<ToolLocator*>( pPyLoc );
	Vector3 wpos = pLoc->transform().applyToOrigin();
	if ( BigBang::instance().snapsEnabled() )
	{
		Snap::vector3( wpos, snaps );
	}
	if ( BigBang::instance().terrainSnapsEnabled() )
	{
		wpos = Snap::toGround( wpos );
	}
	Chunk * pChunk = ChunkManager::instance().cameraSpace()->
		findChunkFromPoint( wpos );
	if (!pChunk)
	{
		PyErr_Format( PyExc_ValueError, "BigBang.createChunkItem() "
			"cannot find chunk at point (%f,%f,%f)",
			wpos.x, wpos.y, wpos.z );
		return NULL;
	}

	if (!EditorChunkCache::instance( *pChunk ).edIsWriteable())
	{
		BigBang::instance().addCommentaryMsg(
			"Could not add asset on non-locked chunk. Please go to Project panel to lock it before editing." );
		Py_Return;
	}
	
	// ok, simply create the chunk item from the data section then
	DataSectionPtr pSection = static_cast<PyDataSection*>( pPyDS )->pSection();
	ChunkItemFactory::Result result = pChunk->loadItem( pSection );
	if ( result && result.onePerChunk() ) 
 	{ 
 		ERROR_MSG( result.errorString().c_str() ); 
 		UndoRedo::instance().add( 
 			new ChunkItemExistenceOperation( result.item(), pChunk ) ); 
 	
 		removeFromSelected( result.item() ); 
 	
 		result = pChunk->loadItem( pSection ); 
 	}
	if (!result)
	{
		PyErr_SetString( PyExc_ValueError, "BigBang.createChunkItem() "
			"error creating item from given section" );
		BigBang::instance().addError( NULL, NULL,
			"Could not add asset. Either the asset is corrupt or it is not an asset that can be placed by WorldEditor." );
		return NULL;
	}

	ChunkItemPtr pItem = result.item();
	if (!pItem)
	{
		PyErr_SetString( PyExc_EnvironmentError, "BigBang.createChunkItem() "
			"Couldn't create Chunk Item" );
		BigBang::instance().addError( NULL, NULL,
			"Could not add asset. Either the asset is corrupt or it is not an asset that can be placed by WorldEditor." );
		return NULL;
	}

	// call the main thread load
	pItem->edChunkBind();

	BigBang::instance().changedChunk( pChunk );

	BoundingBox bb;
	pItem->edBounds( bb );
	Vector3 dim( bb.maxBounds()-bb.minBounds() );

	// now move it to the matrix of the locator, if desired
	if (useLocPos)
	{
		Matrix localPose;
		localPose.multiply( pLoc->transform(), pChunk->transformInverse() );
		Matrix newPose;

		if (useLocPos == 2)
		{
			newPose = localPose;
		}
		else
		{
			newPose = pItem->edTransform();
			newPose.translation( localPose.applyToOrigin() );
		}

		if ( BigBang::instance().snapsEnabled() )
		{
			Snap::vector3( *(Vector3*)&newPose._41, snaps );
		}

		if ( BigBang::instance().terrainSnapsEnabled() )
		{
			Vector3 worldPos(
				pChunk->transform().applyPoint(
					newPose.applyToOrigin() ) );
			
			Snap::toGround( worldPos );
			newPose.translation( pChunk->transformInverse().applyPoint( worldPos ) );
		}

		// check for random rotation placement
		if ( ( pSection->sectionName() == "model" || pSection->sectionName() == "speedtree" ) &&
			!PlacementPresets::instance()->defaultPresetCurrent() )
		{
			// get the random rotation placement values from the current preset
			float minRotX = PlacementPresets::instance()->getCurrentPresetData(
				PlacementPresets::ROTATION, PlacementPresets::X_AXIS, PlacementPresets::MIN );
			float maxRotX = PlacementPresets::instance()->getCurrentPresetData(
				PlacementPresets::ROTATION, PlacementPresets::X_AXIS, PlacementPresets::MAX );
			float minRotY = PlacementPresets::instance()->getCurrentPresetData(
				PlacementPresets::ROTATION, PlacementPresets::Y_AXIS, PlacementPresets::MIN );
			float maxRotY = PlacementPresets::instance()->getCurrentPresetData(
				PlacementPresets::ROTATION, PlacementPresets::Y_AXIS, PlacementPresets::MAX );
			float minRotZ = PlacementPresets::instance()->getCurrentPresetData(
				PlacementPresets::ROTATION, PlacementPresets::Z_AXIS, PlacementPresets::MIN );
			float maxRotZ = PlacementPresets::instance()->getCurrentPresetData(
				PlacementPresets::ROTATION, PlacementPresets::Z_AXIS, PlacementPresets::MAX );

			// rotation
			// yaw/pitch/roll rotation: X = yaw, Pitch = Y, Roll = Z, should change it )
			Matrix work = Matrix::identity;
			work.setRotate(
				DEG_TO_RAD( ( rand() % int( maxRotX - minRotX + 1 ) ) + minRotX ),
				DEG_TO_RAD( ( rand() % int( maxRotY - minRotY + 1 ) ) + minRotY ),
				DEG_TO_RAD( ( rand() % int( maxRotZ - minRotZ + 1 ) ) + minRotZ ) );
			newPose.preMultiply( work );
/*				// per-axis rotation
			if ( maxRotY - minRotY > 0 )
			{
				work.setRotateY( DEG_TO_RAD(
					( rand() % int( maxRotY - minRotY ) ) + minRotY  ) );
				result.preMultiply( work );
			}
			if ( maxRotX - minRotX > 0 )
			{
				work.setRotateX( DEG_TO_RAD(
					( rand() % int( maxRotX - minRotX ) ) + minRotX  ) );
				result.preMultiply( work );
			}
			if ( maxRotZ - minRotZ > 0 )
			{
				work.setRotateZ( DEG_TO_RAD(
					( rand() % int( maxRotZ - minRotZ ) ) + minRotZ ) );
				result.preMultiply( work );
			} */
		}

		// check for random scale placement
		if ( ( pSection->sectionName() == "model" || pSection->sectionName() == "speedtree" ) )
		{
			// get the random scale placement values from the current preset
			float minScaX = PlacementPresets::instance()->getCurrentPresetData(
				PlacementPresets::SCALE, PlacementPresets::X_AXIS, PlacementPresets::MIN );
			float maxScaX = PlacementPresets::instance()->getCurrentPresetData(
				PlacementPresets::SCALE, PlacementPresets::X_AXIS, PlacementPresets::MAX );
			float minScaY = PlacementPresets::instance()->getCurrentPresetData(
				PlacementPresets::SCALE, PlacementPresets::Y_AXIS, PlacementPresets::MIN );
			float maxScaY = PlacementPresets::instance()->getCurrentPresetData(
				PlacementPresets::SCALE, PlacementPresets::Y_AXIS, PlacementPresets::MAX );
			float minScaZ = PlacementPresets::instance()->getCurrentPresetData(
				PlacementPresets::SCALE, PlacementPresets::Z_AXIS, PlacementPresets::MIN );
			float maxScaZ = PlacementPresets::instance()->getCurrentPresetData(
				PlacementPresets::SCALE, PlacementPresets::Z_AXIS, PlacementPresets::MAX );

			// scale
			float largestEdge = max( dim.x, dim.z );
			if ( largestEdge < 0.01f )
				largestEdge = 0.01f;
			float largestScale = 99.99f / largestEdge;
			float scaleX = randInRange( min( largestScale, minScaX ), min( largestScale, maxScaX ) );
			float scaleY = randInRange( min( largestScale, minScaY ), min( largestScale, maxScaY ) );
			float scaleZ = randInRange( min( largestScale, minScaZ ), min( largestScale, maxScaZ ) );

			if ( PlacementPresets::instance()->isCurrentDataUniform( PlacementPresets::SCALE ) )
			{
				scaleY = scaleX;
				scaleZ = scaleX;
			}

			Matrix work = Matrix::identity;
			work.setScale( scaleX, scaleY, scaleZ );
			newPose.preMultiply( work );
		}

		// it's ok if this fails, eg, for ambient lights
		pItem->edTransform( newPose, false );

		/*
		if (!pItem->edTransform( newPose, false ))
		{
			// couldn't move it there, so throw it away then
			pItem->chunk()->delStaticItem( pItem );

			PyErr_SetString( PyExc_ValueError, "BigBang.createChunkItem() "
				"could not move item to desired transform" );
			return NULL;
		}
		*/
	}

	// tell the item it was just created
	pItem->edPostCreate();


	float minEdge = min( min( dim.x, dim.y ), dim.z );
	if( minEdge < 0.001 )
	{
		PyErr_SetString( PyExc_EnvironmentError, "BigBang.createChunkItem() "
			"the item is too small in at least one axis, please check its bounding box" );
		BigBang::instance().addError( NULL, NULL,
			"the item is too small in at least one axis, please check its bounding box" );

		if (pItem->isShellModel())
			ChunkPlacer::deleteChunk( pChunk );
		else
		{
			// see if it wants to be deleted
			if (pItem->edCanDelete())
			{
				// tell it it's going to be deleted
				pItem->edPreDelete();

				// delete it now
				pChunk->delStaticItem( pItem );
				BigBang::instance().changedChunk( pChunk );
			}
		}
		return NULL;
	}
	// ok, everyone's happy then. so add an undo which deletes it
	UndoRedo::instance().add(
		new ChunkItemExistenceOperation( pItem, NULL ) );

	std::vector<ChunkItemPtr> items;
	items.push_back( pItem );
	BigBang::instance().setSelection( items );

	// set a meaningful barrier name
	UndoRedo::instance().barrier( "Create " + pItem->edDescription(), false );

	// and that's it then. return a group that contains it
	ChunkItemGroup * pRes = new ChunkItemGroup();
	pRes->add( pItem );
	return pRes;
}


/**
 *	This method allows scripts to delete a chunk item
 */
PyObject * ChunkItemPlacer::py_deleteChunkItems( PyObject * args )
{
	// get args
	PyObject * pPyRev;
	if (!PyArg_ParseTuple( args, "O", &pPyRev ) ||
		!ChunkItemRevealer::Check( pPyRev ))
	{
		PyErr_SetString( PyExc_ValueError, "BigBang.deleteChunkItems() "
			"expects a ChunkItemRevealer" );
		return NULL;
	}
	
	ChunkItemRevealer* pRevealer = static_cast<ChunkItemRevealer*>( pPyRev );

	ChunkItemRevealer::ChunkItems items;
	pRevealer->reveal( items );

	BigBang::instance().setSelection( std::vector<ChunkItemPtr>() );

	ChunkItemRevealer::ChunkItems::iterator i = items.begin();
	for (; i != items.end(); ++i)
	{
		ChunkItemPtr pItem = *i;
		Chunk* pChunk = pItem->chunk();


		// make sure it hasn't already been 'deleted'
		if (pChunk == NULL)
		{
			PyErr_Format( PyExc_ValueError, "BigBang.deleteChunkItems() "
				"Item has already been deleted!" );
			return NULL;
		}


		if (pItem->isShellModel())
		{
			std::vector<ChunkItemPtr> chunkItems;
			EditorChunkCache::instance( *pChunk ).allItems( chunkItems );
			bool gotOne = false;
			for( std::vector<ChunkItemPtr>::iterator iter = chunkItems.begin(); iter != chunkItems.end() && !gotOne; ++iter )
			{
				if( !(*iter)->edIsPositionRelativeToChunk() )
				{
					if( (*iter)->edBelongToChunk() )
					{
						ChunkItemRevealer::ChunkItems::iterator it = items.begin();
						while( it != items.end() )
						{
							if( *it == *iter )
								break;
							++it;
						}
						if( it == items.end() )
						{
							gotOne = true;
							i = items.insert( i, *iter );
							pItem = *i;
							pChunk = pItem->chunk();
						}
					}
				}
			}
			if( gotOne )
			{
				// Delete the item

				// see if it wants to be deleted
				if (!pItem->edCanDelete())
					continue;

				// tell it it's going to be deleted
				pItem->edPreDelete();

				// delete it now
				pChunk->delStaticItem( pItem );
				BigBang::instance().changedChunk( pChunk );

				// set up an undo which creates it
				UndoRedo::instance().add(
					new ChunkItemExistenceOperation( pItem, pChunk ) );
			}
			// Delete the item's chunk
			else if (!ChunkPlacer::deleteChunk( pChunk ))
				continue;
		}
		else
		{
			// Delete the item

			// see if it wants to be deleted
			if (!pItem->edCanDelete())
				continue;

			// tell it it's going to be deleted
			pItem->edPreDelete();

			// delete it now
			pChunk->delStaticItem( pItem );
			BigBang::instance().changedChunk( pChunk );

			// set up an undo which creates it
			UndoRedo::instance().add(
				new ChunkItemExistenceOperation( pItem, pChunk ) );
		}
	}

	// set a meaningful barrier name
	if (items.size() == 1)
		UndoRedo::instance().barrier( "Delete " + items[0]->edDescription(), false );
	else
		UndoRedo::instance().barrier( "Delete Items", false );

	// and that's it
	Py_Return;
}

/*static*/ void ChunkItemPlacer::removeFromSelected( ChunkItemPtr pItem ) 
{ 
	//remove it from the selection 
	std::vector<ChunkItemPtr> selection = BigBang::instance().selectedItems(); 

	std::vector<ChunkItemPtr>::iterator i = 
	std::find( selection.begin(), selection.end(), pItem ); 

	if (i != selection.end()) 
		selection.erase( i ); 

	BigBang::instance().setSelection( selection, false ); 
} 
