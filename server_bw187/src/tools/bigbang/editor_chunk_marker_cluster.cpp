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
#include "editor_chunk_marker_cluster.hpp"
#include "editor_chunk_substance.ipp"
#include "editor_chunk_item_tree_node.hpp"

#include "big_bang.hpp"
#include "item_editor.hpp"
#include "item_properties.hpp"

#include "appmgr/options.hpp"
#include "chunk/chunk_model.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_marker.hpp"
#include "chunk/chunk_marker_cluster.hpp"
#include "chunks/editor_chunk.hpp"
#include "resmgr/bwresource.hpp"
#include "romp/geometrics.hpp"
#include "romp/super_model.hpp"
#include "gizmo/undoredo.hpp"

DECLARE_DEBUG_COMPONENT2( "EditorChunk", 0 )



// -----------------------------------------------------------------------------
// Section: EditorChunkMarkerClusterOperation
// -----------------------------------------------------------------------------


class EditorChunkMarkerClusterOperation : public UndoRedo::Operation
{
public:
	EditorChunkMarkerClusterOperation( EditorChunkMarkerClusterPtr n, bool removed )
		: UndoRedo::Operation( int(typeid(EditorChunkMarkerClusterOperation).name()) )
		, node_( n )
		, removed_( removed )
	{
		// store the markers the cluster references
		 node_->getCopyOfChildren(children_);
		 parent_ = node_->getParent();

		 availableMarkers_ = node_->getAvailableMarkers();

		 addChunk( node_->chunk() );
	}

	virtual void undo()
	{
		// First add a redo of this undo operation
		UndoRedo::instance().add( new EditorChunkMarkerClusterOperation(node_, !removed_, children_, parent_, availableMarkers_) );

		if (removed_)
		{
			// add links to clusters
			for (std::list<ChunkItemTreeNodePtr>::iterator it = children_.begin();
				it != children_.end();
				it++)
			{
				(*it)->setParent(node_);
			}

			// tell parent about us
			node_->setParent(parent_);
			node_->setAvailableMarkers( availableMarkers_ );
		}
		else
		{
			// removed links to clusters
			for (std::list<ChunkItemTreeNodePtr>::iterator it = children_.begin();
				it != children_.end();
				it++)
			{
				MF_ASSERT((*it)->getParent());
				(*it)->setParent(NULL);
			}

			// tell parent about us
			node_->setParent(NULL);
			node_->setAvailableMarkers( 0 );
		}
	}

	virtual bool iseq( const UndoRedo::Operation & oth ) const
	{
		const EditorChunkMarkerClusterOperation& o = static_cast<const EditorChunkMarkerClusterOperation&>( oth );

		if (node_ != o.node_)
			return false;

		if (removed_ != o.removed_)
			return false;

		if (parent_ != o.parent_) 
			return false;

		if (availableMarkers_ != o.availableMarkers_)
			return false;

		for (std::list<ChunkItemTreeNodePtr>::const_iterator it1 = children_.begin();
            it1 != children_.end();
			it1++)
		{
			bool found = false;
			for (std::list<ChunkItemTreeNodePtr>::const_iterator it2 = o.children_.begin();
				it2 != o.children_.end();
				it2++)
			{
				if (*it1 == *it2)
					found = true;
			}	
			if (!found)
				return false;
		}

		return true;
	}

private:
	EditorChunkMarkerClusterOperation( EditorChunkMarkerClusterPtr n, bool removed, 
		const std::list<ChunkItemTreeNodePtr>& c, ChunkItemTreeNodePtr p, int availMarkers )
		: UndoRedo::Operation( int(typeid(EditorChunkMarkerClusterOperation).name()) )
		, removed_( removed )
		, node_( n )
		, parent_( p )
		, availableMarkers_( availMarkers )
	{
		children_.clear();
		for (std::list<ChunkItemTreeNodePtr>::const_iterator it = c.begin();
            it != c.end();
			it++)
		{
			children_.push_back(*it);
		}
	}

	EditorChunkMarkerClusterPtr node_;
	bool removed_;
	std::list<ChunkItemTreeNodePtr> children_;
	ChunkItemTreeNodePtr parent_;
	int availableMarkers_;
};



// -----------------------------------------------------------------------------
// Section: EditorChunkMarkerCluster
// -----------------------------------------------------------------------------


/**
 *	Constructor.
 */
EditorChunkMarkerCluster::EditorChunkMarkerCluster()
{
	transform_.setIdentity();
}

/**
 *	Destructor.
 */
EditorChunkMarkerCluster::~EditorChunkMarkerCluster()
{
}


void EditorChunkMarkerCluster::toss( Chunk * pChunk )
{
	EditorChunkSubstance<ChunkMarkerCluster>::toss( pChunk );

	MF_ASSERT(id_ != UniqueID::zero())
}


/**
 *	Draw: draw the visual representation.
 *	also draw the lines out to each marker
 */
void EditorChunkMarkerCluster::draw()
{
	if (!edShouldDraw())
		return;

	if( BigBang::instance().drawSelection() )
		Moo::rc().setRenderState( D3DRS_TEXTUREFACTOR, (DWORD)this );

	Moo::rc().push();

	Matrix transform = this->edTransform();
	Moo::rc().preMultiply( transform );

	ModelPtr mp = this->reprModel();
	mp->dress();	// should really be using a supermodel...
	mp->draw( true );

	Moo::rc().pop();

	if( BigBang::instance().drawSelection() )
		return;

	// draw a line back to the parent (if applicable)
	Moo::rc().push();
	Moo::rc().world( Matrix::identity );

	if (getParent())
	{
		const Moo::Colour lineColour = 0xff0000ff;

		Vector3 from = chunk()->transform().applyPoint( edTransform().applyToOrigin() )
			+ Vector3(0.f, 0.1f, 0.f);
		Vector3 to = getParent()->chunk()->transform().applyPoint( getParent()->edTransform().applyToOrigin() )
			+ Vector3(0.f, 0.1f, 0.f);

		Geometrics::drawLine( from, to, lineColour, false );	// false = z-buffer the lines

		// draw a little arrow to indicate which is parent
		Vector3 direction = to - from;
		const float distance = direction.length();
		direction.normalise();

		Vector3 up(0.f, 1.f, 0.f);
		if (direction.dotProduct(up) > 0.9f)
			up = Vector3(1.f, 0.f, 0.f);

		const Vector3 rightAngleVector = direction.crossProduct(up);

		float arrowDisp = 0.2f;
		const float crossOver = 5.f;
		if (distance < crossOver)
			arrowDisp = 0.5f;

		float arrowDistanceAway = arrowDisp * distance;
		if (arrowDistanceAway > 5.f)
			arrowDistanceAway = 5.f;

		const float arrowWidth = 0.3f;
		const float arrowLength = 0.5f;
		Vector3 arrowHead = arrowDistanceAway * direction + from;
		Vector3 arrowBase1 = arrowHead - arrowLength * direction;
		Vector3 arrowBase2 = arrowBase1 - rightAngleVector * arrowWidth;
		arrowBase1 += rightAngleVector * arrowWidth;

		Geometrics::drawLine( arrowHead, arrowBase1, lineColour, false );
		Geometrics::drawLine( arrowBase1, arrowBase2, lineColour, false );
		Geometrics::drawLine( arrowBase2, arrowHead, lineColour, false );

		arrowHead = -arrowDistanceAway * direction + to;
		arrowBase1 = arrowHead - arrowLength * direction;
		arrowBase2 = arrowBase1 - rightAngleVector * arrowWidth;
		arrowBase1 += rightAngleVector * arrowWidth;

		Geometrics::drawLine( arrowHead, arrowBase1, lineColour, false );
		Geometrics::drawLine( arrowBase1, arrowBase2, lineColour, false );
		Geometrics::drawLine( arrowBase2, arrowHead, lineColour, false );
	}

	Moo::rc().pop();
}

/*
 *	Load: call the base load and then perform editor initialisations
 */
bool EditorChunkMarkerCluster::load( DataSectionPtr pSection, Chunk * pChunk, std::string* errorString )
{
	bool ok = this->EditorChunkSubstance<ChunkMarkerCluster>::load( pSection, pChunk );
	if (ok)
	{
		transform_.translation( pSection->readVector3( "position" ) );
	}
	else if ( errorString )
	{
		*errorString = "Marker Cluster load failed";
	}

	return ok;
}


/**
 *	Save any property changes to this data section
 */
bool EditorChunkMarkerCluster::edSave( DataSectionPtr pSection )
{
	if (!edCommonSave( pSection ))
		return false;

	ChunkItemTreeNode::save( pSection );

	pSection->writeVector3( "position", transform_.applyToOrigin() );
	pSection->writeInt( "available_markers", availableMarkers_ );
	return true;
}


/**
 *	Get the current transform
 */
const Matrix & EditorChunkMarkerCluster::edTransform()
{
	return transform_;
}


/**
 *	Change our transform, temporarily or permanently
 */
bool EditorChunkMarkerCluster::edTransform( const Matrix & m, bool transient )
{
	// it's permanent, so find out where we belong now
	Chunk * pOldChunk = pChunk_;
	Chunk * pNewChunk = this->edDropChunk( m.applyToOrigin() );
	if (pNewChunk == NULL) return false;

	// if this is only a temporary change, keep it in the same chunk
	if (transient)
	{
		transform_ = m;
		return true;
	}

	// make sure the chunks aren't readonly
	if (!EditorChunkCache::instance( *pOldChunk ).edIsWriteable() 
		|| !EditorChunkCache::instance( *pNewChunk ).edIsWriteable())
		return false;

	// ok, accept the transform change then
	transform_.multiply( m, pOldChunk->transform() );
	transform_.postMultiply( pNewChunk->transformInverse() );

	// note that both affected chunks have seen changes
	BigBang::instance().changedChunk( pOldChunk );
	BigBang::instance().changedChunk( pNewChunk );

	// and move ourselves into the right chunk. we have to do this
	// even if it's the same chunk so the col scene gets recreated
	pOldChunk->delStaticItem( this );
	pNewChunk->addStaticItem( this );

	return true;
}

/**
 *	Add the properties of this item to the given editor
 */
bool EditorChunkMarkerCluster::edEdit( class ChunkItemEditor & editor )
{
	if (!allChildrenLoaded())
		return false;

	MatrixProxy * pMP = new ChunkItemMatrix( this );
	editor.addProperty( new GenPositionProperty( "position", pMP ) );
	editor.addProperty( new GenIntProperty( "number spawned",
		new AccessorDataProxy<EditorChunkMarkerCluster, IntProxy>(
			this, "number spawned", 
			&EditorChunkMarkerCluster::getAvailableMarkers, 
			&EditorChunkMarkerCluster::setAvailableMarkers ) ) );

	editor.addProperty( new StaticTextProperty(
		"number children", new ConstantDataProxy<StringProxy>(
			numberChildrenAsString() ) ) );

	return true;
}


/**
 * Return false if any of the markers are not yet loaded or parent not fully loaded
 */
bool EditorChunkMarkerCluster::edCanDelete()
{
	if (allChildrenLoaded())
		if ((getParent() && getParent()->allChildrenLoaded()) || (!getParent() && (parentID_ == UniqueID::zero())))
			return true;

	return false;
}

/**
 * Tell the markers they are no longer part of a cluster
 */
void EditorChunkMarkerCluster::edPreDelete()
{
	if (isNodeConnected())
	{
		UndoRedo::instance().add( new EditorChunkMarkerClusterOperation( this, true ) );
		removeThisNode();
	}
}

void EditorChunkMarkerCluster::edPostClone( EditorChunkItem* srcItem )
{
	// copy nothing, set as empty
	availableMarkers_ = 0;

	// extract from any existing links, assign new ID
	removeThisNode();
	setNewNode();
}

/**
 *	Return a modelptr that is the representation of this chunk item
 */
ModelPtr EditorChunkMarkerCluster::reprModel() const
{
	static ModelPtr psModel = Model::get( "helpers/markers/marker_cluster.model" );
	return psModel;
}

/// Write the factory statics stuff
#undef IMPLEMENT_CHUNK_ITEM_ARGS
#define IMPLEMENT_CHUNK_ITEM_ARGS (pSection, pChunk, &errorString)
IMPLEMENT_CHUNK_ITEM( EditorChunkMarkerCluster, marker_cluster, 1 )


bool EditorChunkMarkerCluster::setAvailableMarkers(const uint32& number)
{
	// check don't exceed the number of markers in the cluster
	if (number > (uint32)numberChildren())
		availableMarkers_ = numberChildren();
	else
		availableMarkers_ = number; 

	return true;
}

std::string EditorChunkMarkerCluster::numberChildrenAsString()
{
	char string[20];
	sprintf(string, "%d", numberChildren());
	return string;
}


void EditorChunkMarkerCluster::setParent(ChunkItemTreeNodePtr parent)
{
	if (!edCanDelete())
		return;

	// Save the changes
	ChunkItemTreeNodePtr oldParent = getParent();
	ChunkItemTreeNode::setParent(parent);
	BigBang::instance().changedChunk( chunk() );

	if (oldParent)
	{
		oldParent->edSave( oldParent->pOwnSect() );
		BigBang::instance().changedChunk( oldParent->chunk() );
	}

	if (parent)
	{
		parent->edSave( parent->pOwnSect() );
		BigBang::instance().changedChunk( parent->chunk() );
	}

	edSave( pOwnSect() );
}


void EditorChunkMarkerCluster::onRemoveChild()
{
	// check available is <= total
	if (availableMarkers_ > numberChildren())
	{
		INFO_MSG( "Resetting parent cluster available markers\n" );
		BigBang::instance().addCommentaryMsg( "Resetting parent cluster available markers" );

		availableMarkers_ = 0;
	}
}
