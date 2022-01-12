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

#include "resmgr/bwresource.hpp"

#include "chunk_loader.hpp"
#include "chunk_manager.hpp"
#include "chunk_space.hpp"
#include "chunk.hpp"

#include "cstdmf/debug.hpp"
#include "cstdmf/diary.hpp"

DECLARE_DEBUG_COMPONENT2( "Chunk", 0 );

/// constructor. This runs in the main thread
ChunkLoader::ChunkLoader():
thread_( NULL )
{
}


/// destructor. This runs in the main thread
ChunkLoader::~ChunkLoader()
{
}


/// public start method. This runs in the main thread
bool ChunkLoader::start()
{
	thread_ = new SimpleThread( ChunkLoader::s_start, this );

//	return (thread_ != NULL && thread_ != HANDLE(0xFFFFFFFF));
	return true;
}

/// public stop method. This runs in the main thread
void ChunkLoader::stop()
{
	// send ourselves the breaking condition
	LoadOrder nolo;
	nolo.func_ = NULL;
	nolo.del_  = NULL;
	this->load( nolo );

	// and wait for the thread to terminate
	delete thread_;

	TRACE_MSG( "ChunkLoader: stopped.\n" );
}

/// thread entry point
void ChunkLoader::s_start( void * arg )
{
	TRACE_MSG( "ChunkLoader: started.\n" );

	ChunkLoader * pLoader = (ChunkLoader*)arg;
	pLoader->run();
}


/// main loop
void ChunkLoader::run()
{
	DiaryEntry & de = Diary::instance().add( "Chunk Loader" );
	de.stop();

#ifndef _WIN32
	nice(10);
#endif

	bool hell_not_frozen_over = true;

	while (hell_not_frozen_over)
	{
		// wait until there's something in the list
		semaphore_.pull();

		// pull the front off the list
		mutex_.grab();
		LoadOrder lo = loadList_.front();
		loadList_.pop_front();
		mutex_.give();

		// if it's null then our time is up
		if (lo.func_ == NULL)
		{
			break;
		}

		// ok, it's not - load away then
		this->loadNow( lo );
	}

	while (!loadList_.empty())
	{
		// pull the front off the list
		mutex_.grab();
		LoadOrder lo = loadList_.front();
		loadList_.pop_front();
		mutex_.give();

		if (lo.del_ != NULL)
		{
			(*lo.del_)( lo.arg_ );
		}
	}
}

/**
 *	Load the resource identified by the given load order.
 *
 *	This is called from the main thread
 */
void ChunkLoader::load( const LoadOrder & lo )
{
	// put it on the back of the list
	mutex_.grab();
	//loadList_.push_back( lo );
	if( lo.func_ )
	{
		LoadList::reverse_iterator it;
		for (it = loadList_.rbegin(); it != loadList_.rend(); it++)
		{
			if (lo.priority_ >= it->priority_) break;
		}
		loadList_.insert( it.base(), lo );
	}
	else
		loadList_.insert( loadList_.begin(), lo );
	mutex_.give();
	// if we didn't want to do this in fifo order, we could do all this
	// without the mutex ... using InterlockedCompareExchangePointer.

	// and let it know there's something there
	semaphore_.push();
}

/**
 *	Load the empty chunk pChunk (its identifier is set).
 *
 *	This is called from the main thread
 */
void ChunkLoader::load( Chunk * pChunk, int priority )
{
	LoadOrder lo;
	lo.func_ = &ChunkLoader::loadChunkNow;
	lo.arg_ = pChunk;
	lo.del_ = NULL;
	lo.priority_ = priority;
	this->load( lo );
}

void ChunkLoader::loadNow( Chunk * pChunk )
{
	LoadOrder lo;
	lo.func_ = &ChunkLoader::loadChunkNow;
	lo.arg_ = pChunk;
	lo.del_ = NULL;
	this->loadNow( lo );
}

struct FindSeedArgs
{
	ChunkSpacePtr	pSpace_;
	Vector3			where_;
	Chunk			** ppChunk_;
};

/**
 *	Create a seed chunk for the given space at the given location.
 */
void ChunkLoader::findSeed( ChunkSpace * pSpace, const Vector3 & where,
	Chunk *& rpChunk )
{
	FindSeedArgs * fsa = new FindSeedArgs;
	fsa->pSpace_ = pSpace;
	fsa->where_ = where;
	fsa->ppChunk_ = &rpChunk;

	LoadOrder lo;
	lo.func_ = &ChunkLoader::findSeedNow;
	lo.del_ = &ChunkLoader::delSeedNow;
	lo.arg_ = fsa;
	lo.priority_ = 1;
	this->load( lo );
}


/**
 *	Execute the given load order now.
 *
 *	This is called from the loading thread
 */
void ChunkLoader::loadNow( const LoadOrder & lo )
{
	(*lo.func_)( lo.arg_ );
}

/**
 *	Load the empty chunk pChunk now.
 *
 *	This is called from the loading thread
 */
void ChunkLoader::loadChunkNow( void * arg )
{
	Chunk * pChunk = (Chunk*)arg;

	DiaryEntry & de = Diary::instance().add( "Load " + pChunk->identifier() );
	DiaryEntry & de2 = Diary::instance().add( "cs" );
	DataSectionPtr	pDS = BWResource::openSection(
		pChunk->resourceID() );
	de2.stop();

	pChunk->load( pDS );
	de.stop();

	TRACE_MSG( "ChunkLoader: Loaded chunk '%s'\n",
		pChunk->resourceID().c_str() );
}

/**
 *	Create the seed chunk for the given location now.
 *
 *	This is called from the loading thread
 */
void ChunkLoader::findSeedNow( void * arg )
{
	FindSeedArgs * fsa = (FindSeedArgs*)arg;
	*fsa->ppChunk_ = fsa->pSpace_->guessChunk( fsa->where_ );
	delete fsa;
}

void ChunkLoader::delSeedNow( void * arg )
{
	FindSeedArgs * fsa = (FindSeedArgs*)arg;
	delete fsa;
}

// chunk_loader.cpp
