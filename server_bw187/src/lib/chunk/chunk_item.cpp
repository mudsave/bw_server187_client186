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

#include "chunk_item.hpp"
#include "chunk.hpp"

#ifndef _RELEASE

#include "cstdmf/memory_counter.hpp"

memoryCounterDeclare( chunk );

/**
 *	Constructor
 */
ChunkItemBase::ChunkItemBase( int wantFlags ) :
	__count( 0 ), 
	wantFlags_( wantFlags ), 
	drawMark_(0),
	pChunk_( NULL )
{
	if (!(wantFlags_ & 65536))
	{
		memoryCounterAdd( chunk );
		memoryClaim( this );
	}
}

/**
 *	Copy constructor
 */
ChunkItemBase::ChunkItemBase( const ChunkItemBase & oth ) :
	__count( 0 ), 
	wantFlags_( oth.wantFlags_ ), 
	drawMark_(0),
	pChunk_( oth.pChunk_ )
{
	if (!(wantFlags_ & 65536))
	{
		memoryCounterAdd( chunk );
		memoryClaim( this );
	}
}

/**
 *	Destructor
 */
ChunkItemBase::~ChunkItemBase()
{
	if (!(wantFlags_ & 65536))
	{
		memoryCounterSub( chunk );
		memoryClaim( this );
	}
}

#endif // !_RELEASE


/**
 *	Utility method to implement 'lend' given a (world space) bounding box
 */
void ChunkItemBase::lendByBoundingBox( Chunk * pLender,
	const BoundingBox & worldbb )
{
	int allInOwnChunk = pChunk_->isOutsideChunk() ? 0 : -1;
	// assume it's not all within its own chunk if the item
	// is in an outside chunk (i.e. if bb test passes then that's
	// good enough to loan for us)

	// go through every bound portal
	Chunk::piterator pend = pLender->pend();
	for (Chunk::piterator pit = pLender->pbegin(); pit != pend; pit++)
	{
		if (!pit->hasChunk()) continue;

		Chunk * pConsider = pit->pChunk;

		// if it's not in that chunk's bounding box
		// then it definitely doesn't want it
		if (!worldbb.intersects( pConsider->boundingBox() )) continue;

		// if that's an outside chunk and the item is completely
		// within its own chunk then it also doesn't want it
		if (pConsider->isOutsideChunk())
		{
			// don't bother checking this for inside chunks since they're
			// not allowed to have interior chunks (i.e. bb is good enough)
			if (allInOwnChunk < 0)
			{
				// should really check if it's not completely within the union
				// of all interior chunks, but checking just its own is an OK
				// approximation ... if we had the hull tree at this stage then
				// we could do a different test using findChunkFromPoint, but we
				// don't, and it would miss some cases too, so this will do
				Vector3 bbpts[2] = { worldbb.minBounds(), worldbb.maxBounds() };
				Vector3 tpoint;	// this simple algorithm obviously only works
				int i;			// if our own chunk has no interior chunks
				for (i = 0; i < 8; i++)
				{
					tpoint.x = bbpts[(i>>0)&1].x;
					tpoint.y = bbpts[(i>>1)&1].y;
					tpoint.z = bbpts[(i>>2)&1].z;
					if (!pChunk_->contains( tpoint )) break;
				}
				allInOwnChunk = (i == 8);
				// if we are all in our own chunk (and we are in an inside
				// chunk, which is the only way we get here), then we can't
				// be in this chunk too... and furthermore we can't be any
				// any other chunks at all, so we can just stop here
				if (allInOwnChunk) break;
			}
			// if it's all within its own chunk then it can't be in this one
			//if (allInOwnChunk) continue;
			// ... but since we only calculate allInOwnChunk if our chunk is
			// an inside chunk, and if it were true we would have stopped
			// the loop already, then there's no point checking it again here,
			// in fact allInOwnChunk can only be zero here.
			MF_ASSERT( !allInOwnChunk );
			// could make the code a bit neater but this way is more logical
		}

		// ok so that chunk does really want this item then
		pConsider->addLoanItem( static_cast<ChunkItem*>(this) );
	}
}


/**
 *	Constructor. Registers with chunk's static factory registry
 */
ChunkItemFactory::ChunkItemFactory(
		const std::string & section,
		int priority,
		Creator creator ) :
	priority_( priority ),
	creator_( creator )
{
	Chunk::registerFactory( section, *this );
}


/**
 *	This virtual method calls the creator function that was passed in,
 *	as long as it's not NULL. It is called by a Chunk when it encounters
 *	the section name.
 *
 *	@return true if succeeded
 */
ChunkItemFactory::Result ChunkItemFactory::create( Chunk * pChunk,
	DataSectionPtr pSection ) const
{
	if (creator_ == NULL)
	{	
		std::string errorStr = "No item factory found for section ";
		if ( pSection )
		{
			errorStr += "'" + pSection->sectionName() + "'";
		}
		else
		{
			errorStr += "<unknown>";
		}
		return ChunkItemFactory::Result( NULL, errorStr );
	}
	return (*creator_)( pChunk, pSection );
}

// chunk_item.cpp
