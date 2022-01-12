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

#include "resmgr/datasection.hpp"
#include "chunk_item.hpp"
#include "editor_chunk_item.hpp"
#include "chunk.hpp"
#include "chunk_space.hpp"
#include "../../tools/bigbang/editor_group.hpp"
#include "../../tools/bigbang/chunks/editor_chunk.hpp"

#ifndef CODE_INLINE
#include "editor_chunk_item.ipp"
#endif

DECLARE_DEBUG_COMPONENT2( "Editor", 0 )

// -----------------------------------------------------------------------------
// Section: EditorChunkItem
// -----------------------------------------------------------------------------

bool EditorChunkItem::hideAllOutside_ = false;
/**
 *	Constructor.
 */
EditorChunkItem::EditorChunkItem( int wantFlags ) :
	ChunkItemBase( wantFlags ),
	pGroup_( NULL ),
	groupMember_( false ),
	hasLoaded_( false )
{
}


/**
 *	Destructor.
 */
EditorChunkItem::~EditorChunkItem()
{
}

void EditorChunkItem::edChunkBind()
{
	if (!hasLoaded_)
	{
		edMainThreadLoad();
		hasLoaded_ = true;
	}
}


/**
 *	Get a nice description for this item. Most items will not need
 *	to override this method.
 */
std::string EditorChunkItem::edDescription()
{
	std::string iDesc = std::string(this->edClassName()) + " item";
	const char * label = this->label();
	if (label != NULL && strlen(label) > 0)
		iDesc += std::string(" '") + label + "'";
	return iDesc;
}


/**
 *	Find the drop chunk for this item
 */
Chunk * EditorChunkItem::edDropChunk( const Vector3 & lpos )
{
	Vector3 npos = pChunk_->transform().applyPoint( lpos );

	Chunk * pNewChunk = pChunk_->space()->findChunkFromPoint( npos );
	if (pNewChunk == NULL)
	{
		ERROR_MSG( "Cannot move %s to (%f,%f,%f) "
			"because it is not in any loaded chunk!\n",
			this->edDescription().c_str(), npos.x, npos.y, npos.z );
		return NULL;
	}

	return pNewChunk;
}

bool EditorChunkItem::edCommonSave( DataSectionPtr pSection )
{
	if (edGroup())
	{
		groupName_ = edGroup()->fullName();
		pSection->writeString( "group", groupName_ );
	}

	return true;
}

bool EditorChunkItem::edCommonLoad( DataSectionPtr pSection )
{
	groupName_ = pSection->readString( "group" );

	if (!groupName_.empty())
	{
		groupMember_ = true;
		// We don't use edGroup() here, as pOwnSect may not yet be valid to call
		//edGroup( EditorGroup::findOrCreateGroup( groupName_ ) );
		//pGroup_ = EditorGroup::findOrCreateGroup( groupName_ );
		//pGroup_->enterGroup( (ChunkItem*) this );

		// Don't add ourself to the group untill toss()
	}


	return true;
}

void EditorChunkItem::toss( Chunk * pChunk )
{
	ChunkItemBase::toss( pChunk );

	if (groupMember_)
	{	
		if (pChunk)
		{
			// Add it back to its group if we're returning from nowhere
			if (!pGroup_)
			{
				// We don't call edGroup(), as we don't want to mark the chunk as dirty
				pGroup_ = EditorGroup::findOrCreateGroup( groupName_ );
				pGroup_->enterGroup( (ChunkItem*) this );
			}
		}
		else
		{
			// Item is being moved to nowhere, temp remove it from its group
			edGroup( NULL );
		}
	}
}

// Defined in big_bang.cpp
// TODO: we should use different ChunkItems in WE and ME
void changedChunk( Chunk * pChunk );

void EditorChunkItem::edGroup( EditorGroup * pGp )
{
	// NB, pGp may be the same as pGroup_, if it's name has changed or somesuch


	if (pGroup_)
		pGroup_->leaveGroup( (ChunkItem*) this );

	if (pGp == NULL)
	{
		pGroup_ = NULL;
	}
	else
	{
		groupMember_ = true;
		pGroup_ = pGp;
		groupName_ = pGroup_->fullName();
		pGroup_->enterGroup( (ChunkItem*) this );

		if ( pOwnSect() )
			edSave( pOwnSect() );
		if (chunk())
			changedChunk( chunk() );
	}
}

bool EditorChunkItem::edShouldDraw()
{
	return !hideAllOutside_ || !chunk()->isOutsideChunk();
}

// Sadly defined in big_bang.cpp
// TODO: we should use different ChunkItems in WE and ME
bool chunkWritable( Chunk * pChunk );

bool EditorChunkItem::edIsEditable()
{
	return chunk()	?	chunkWritable( chunk() )	:
						false;
}

void EditorChunkItem::hideAllOutside( bool hide )
{
	hideAllOutside_ = hide;
}

bool EditorChunkItem::hideAllOutside()
{
	return hideAllOutside_;
}

// editor_chunk_item.cpp
