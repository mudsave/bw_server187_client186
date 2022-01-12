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
#include "chunk_overlapper.hpp"

#include "chunk/chunk.hpp"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk_manager.hpp"

#include "chunks/editor_chunk.hpp"

#include "appmgr/options.hpp"

#include "big_bang.hpp"

#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT2( "Editor", 0 )


// -----------------------------------------------------------------------------
// Section: ChunkOverlapper
// -----------------------------------------------------------------------------

#undef IMPLEMENT_CHUNK_ITEM_ARGS
#define IMPLEMENT_CHUNK_ITEM_ARGS (pSection, pChunk, &errorString)
IMPLEMENT_CHUNK_ITEM( ChunkOverlapper, overlapper, 0 )

bool ChunkOverlapper::s_drawAlways_ = false;
uint32 ChunkOverlapper::s_settingsMark_ = -16;

int ChunkOverlapper_token = 0;


std::vector<Chunk*> ChunkOverlapper::drawList;

/**
 *	Constructor.
 */
ChunkOverlapper::ChunkOverlapper() :
	pOverlapper_( NULL ),
	bound_( false )
{
}


/**
 *	Destructor.
 */
ChunkOverlapper::~ChunkOverlapper()
{
}


/**
 *	Load method. Creates an unratified chunk for our overlapper.
 */
bool ChunkOverlapper::load( DataSectionPtr pSection, Chunk * pChunk, std::string* errorString )
{
	pOwnSect_ = pSection;

	std::string ovChunkName = pSection->asString();
	if (ovChunkName.empty())
	{
		if ( errorString )
		{
			*errorString = "Failed to load chunk overlapper";
		}
		return false;
	}

	pOverlapper_ = new Chunk( ovChunkName, pChunk->mapping() );
	return true;

	// Note: the '@otherspace' chunk referencing syntax is not supported
	// here 'coz if it overlaps us then it must be in our space.
}


/**
 *	Toss method. If we get moved to another chunk that is online
 *	(or we are created in it) then we can do our bind action now.
 */
void ChunkOverlapper::toss( Chunk * pChunk )
{
	if (pChunk_ != NULL) ChunkOverlappers::instance( *pChunk_ ).del( this );

	ChunkItem::toss( pChunk );

	if (pChunk_ != NULL) ChunkOverlappers::instance( *pChunk_ ).add( this );

	if (pChunk_ != NULL && pChunk_->online())
	{
		this->bindStuff();
	}
}

/**
 *	Draw method. We add the chunk we refer to to the fringe drawing list
 *	if chunk overlappers are being drawn.
 */
void ChunkOverlapper::draw()
{
	if (pOverlapper_ == NULL || !pOverlapper_->online()) return;

	ChunkManager & cmi = ChunkManager::instance();

	if (cmi.cameraChunk()->drawMark() != s_settingsMark_)
	{
		s_drawAlways_ = Options::getOptionInt(
			"render/shells/gameVisibility", s_drawAlways_ ? 0 : 1 ) == 0;
		s_settingsMark_ = cmi.cameraChunk()->drawMark();
	}

	if (ChunkOverlapper::s_drawAlways_)
	{
		if (pOverlapper_->drawMark() != cmi.cameraChunk()->drawMark() &&
			pOverlapper_->fringePrev() == NULL)
				drawList.push_back( pOverlapper_ );
	}
}


/**
 *	Lend method. We use this as a notification that the chunk has been
 *	bound and we are running in the main thread. This kind of machinery
 *	would normally go in the chunk itself (except it is editor specific),
 *	so I don't really feel a need to add a 'bind' method to ChunkItem.
 */
void ChunkOverlapper::lend( Chunk * pLender )
{
	this->bindStuff();
}


/**
 *	This method does the stuff we want to do when the chunk is bound,
 *	i.e. resolve our stub chunk and add it to the load queue if necessary.
 */
void ChunkOverlapper::bindStuff()
{
	if (bound_) return;

	pOverlapper_ = pOverlapper_->space()->findOrAddChunk( pOverlapper_ );
	bound_ = true;

	if (!pOverlapper_->online())
	{
		ChunkManager::instance().loadChunkExplicitly(
			pOverlapper_->identifier(), BigBang::instance().chunkDirMapping() );
	}
}



// -----------------------------------------------------------------------------
// Section: ChunkOverlappers
// -----------------------------------------------------------------------------

/**
 *	Constructor
 */
ChunkOverlappers::ChunkOverlappers( Chunk & chunk ) :
	pChunk_( &chunk )
{
}

/**
 *	Destructor.
 */
ChunkOverlappers::~ChunkOverlappers()
{
}


/**
 *	Add this overlapper item to our collection
 */
void ChunkOverlappers::add( ChunkOverlapperPtr pOverlapper )
{
	items_.push_back( pOverlapper );
}

/**
 *	Remove this overlapper item from our collection
 */
void ChunkOverlappers::del( ChunkOverlapperPtr pOverlapper )
{
	Items::iterator found =
		std::find( items_.begin(), items_.end(), pOverlapper );
	if (found != items_.end())
	{
		items_.erase( found );
	}
}


/**
 *	Make a new overlapper item in the chunk we are a cache for to
 *	specify the input chunk as an overlapper
 */
void ChunkOverlappers::form( Chunk * pOverlapper )
{
	// make the datasection element
	DataSectionPtr pSect =
		EditorChunkCache::instance( *pChunk_ ).pChunkSection();
	pSect = pSect->newSection( "overlapper" );
	pSect->setString( pOverlapper->identifier() );

	// we don't use the normal chunk item creation pathway here 'coz we
	// don't want to the normal undo/redo baggage.

	// now load that item, which will automatically add itself to our list
	MF_ASSERT( pChunk_->loadItem( pSect ) );

	// and flag ourselves as dirty
	BigBang::instance().changedChunk( pChunk_ );
}

/**
 *	Get rid of the overlapper item in the chunk we are a cache for
 *	that specified the input chunk as an overlapper
 */
void ChunkOverlappers::cut( Chunk * pOverlapper )
{
	// find the item that points to this chunk (if any)
	for (Items::iterator it = items_.begin(); it != items_.end(); it++)
	{
		if ((*it)->pOverlapper() == pOverlapper)
		{
			// flag ourselves as dirty
			BigBang::instance().changedChunk( pChunk_ );

			// delete its datasection
			DataSectionPtr pParent =
				EditorChunkCache::instance( *pChunk_ ).pChunkSection();
			pParent->delChild( (*it)->pOwnSect() );

			// and delete the item itself
			pChunk_->delStaticItem( *it );

			// and it's gone, so get out
			//  (our iterator is stuffed now anyway)
			return;
		}
	}

	// we didn't find one. this is ok for now, but should be upgraded to
	// an error when all overlapping chunks have an 'overlapper' item in
	// the chunk they overlap.
	WARNING_MSG( "ChunkOverlappers::cut: "
		"No overlapper item in %s points to %s\n",
		pChunk_->identifier().c_str(), pOverlapper->identifier().c_str() );
}


/// Static instance accessor initialiser
ChunkCache::Instance<ChunkOverlappers> ChunkOverlappers::instance;


// chunk_overlapper.cpp
