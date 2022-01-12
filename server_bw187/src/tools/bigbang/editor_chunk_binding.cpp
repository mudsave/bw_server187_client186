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
#include "editor_chunk_binding.hpp"
#include "editor_chunk_substance.ipp"
#include "editor_chunk_item_tree_node.hpp"

#include "big_bang.hpp"
#include "item_editor.hpp"
#include "item_properties.hpp"

#include "appmgr/options.hpp"
#include "chunk/chunk_model.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunks/editor_chunk.hpp"
#include "resmgr/bwresource.hpp"
#include "romp/geometrics.hpp"
#include "romp/super_model.hpp"


// -----------------------------------------------------------------------------
// Section: EditorChunkBinding
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
EditorChunkBinding::EditorChunkBinding()
: transform_(Matrix::identity)
{
}


/**
 *	Destructor.
 */
EditorChunkBinding::~EditorChunkBinding()
{
}

void EditorChunkBinding::draw()
{
	if (!edShouldDraw())
		return;

	if( BigBang::instance().drawSelection() )
		Moo::rc().setRenderState( D3DRS_TEXTUREFACTOR, (DWORD)this );
	else
	{
		// draw a line back to the cluster (if applicable)
		Moo::rc().push();
		Moo::rc().world( Matrix::identity );

		const Moo::Colour lineColour = 0xffff0000;

		Vector3 from = this->from()->chunk()->transform().applyPoint( this->from()->edTransform().applyToOrigin() )
			+ Vector3(0.f, 0.1f, 0.f);
		Vector3 to = this->to()->chunk()->transform().applyPoint( this->to()->edTransform().applyToOrigin() )
			+ Vector3(0.f, 0.1f, 0.f);

		Geometrics::drawLine( from, to, lineColour, false );	// false = z-buffer the lines

		Moo::rc().pop();
	}

	// draw binding item
	Moo::rc().push();
	Moo::rc().preMultiply( this->edTransform() );

	ModelPtr mp = this->reprModel();
	mp->dress();
	mp->draw( true );

	Moo::rc().pop();
}


bool EditorChunkBinding::load( DataSectionPtr pSection )
{
	bool ok = this->EditorChunkSubstance<ChunkBinding>::load( pSection );

	if (this->from())
	{
		calculateTransform(true);
	}

	return ok;
}


/**
 *	Save any property changes to this data section
 */
bool EditorChunkBinding::edSave( DataSectionPtr pSection )
{
	if (!ChunkBinding::save( pSection ))
		return false;

	if (!edCommonSave( pSection ))
		return false;

	return true;
}

/**
 *	Get the current transform
 */
const Matrix & EditorChunkBinding::edTransform()
{
	return transform_;
}


void EditorChunkBinding::calculateTransform(bool transient)
{
	if (!from() || !to())
		return;  // can't do anything, not fully loaded

	// place binding object a few meters away from the owner,
	// work in the binding's chunk space
	Matrix fromTrans = this->from()->edTransform();
	const Matrix& fromChunkTransform = this->from()->chunk()->transform();
	Matrix toTrans = this->to()->edTransform();
	const Matrix& toChunkTransform = this->to()->chunk()->transform();

	Chunk * myChunk = this->from()->chunk();
	if (chunk())
		myChunk = chunk();

	// convert the toTrans into the space of the fromTrans trans
	toTrans.postMultiply( toChunkTransform );
	toTrans.postMultiply( myChunk->transformInverse() );
	fromTrans.postMultiply( fromChunkTransform );
	fromTrans.postMultiply( myChunk->transformInverse() );

	Vector3 pos = 0.5f * (toTrans.applyToOrigin() - fromTrans.applyToOrigin()) 
					+ fromTrans.applyToOrigin() + Vector3(0.f, 0.1f, 0.f);

	// displacement is half way between
	Matrix final;
	final.setTranslate(pos);
	edTransform(final, transient);
}


/**
 *	Change our transform, temporarily or permanently
 */
bool EditorChunkBinding::edTransform( const Matrix & m, bool transient )
{
	// this is always a temporary change, as position of a binding is unimportant
	// if this is only a temporary change, keep it in the same chunk
	if (transient)
	{
		transform_ = m;
		return true;
	}


	// it's permanent, so find out where we belong now
	Chunk * pOldChunk = pChunk_;
	Chunk * pNewChunk = this->edDropChunk( m.applyToOrigin() );
	if (pNewChunk == NULL) return false;

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
bool EditorChunkBinding::edEdit( class ChunkItemEditor & editor )
{
	return true;
}

/**
 * Return false if any of the markers are not yet loaded
 */
bool EditorChunkBinding::edCanDelete()
{
	if (from() && to())
		return true;		// make sure the linked entities are loaded

	return false;
}

/**
 * Tell the entities/markers they are no longer part of this binding
 */
void EditorChunkBinding::edPreDelete()
{
    // TODO: write EditorChunkBinding::edPreDelete
}

/**
 *	Copy the category, nothing else
 */
void EditorChunkBinding::edPostClone( EditorChunkItem* srcItem )
{
	// TODO: write EditorChunkBinding::edPostClone
}


/**
 *	Return a modelptr that is the representation of this chunk item
 */
ModelPtr EditorChunkBinding::reprModel() const
{
	static ModelPtr psModel = Model::get( "resources/models/entity.model" );
	return psModel;
}

/// Write the factory statics stuff
IMPLEMENT_CHUNK_ITEM( EditorChunkBinding, binding, 1 )
