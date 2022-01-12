/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CHUNK_LOADER_HPP
#define CHUNK_LOADER_HPP


#include <deque>

class Chunk;
class ChunkSpace;
class Vector3;

#include "cstdmf/concurrency.hpp"

#ifndef _WIN32
	typedef unsigned long HANDLE;
#endif


/**
 *	This class loads chunks using a background thread.
 */
class ChunkLoader
{
public:
	ChunkLoader();
	~ChunkLoader();

	bool start();
	void stop();

	struct LoadOrder
	{
		LoadOrder() :
			func_( NULL ),
			del_( NULL ),
			arg_( NULL ),
			priority_( 16 )
		{ }

		void (*func_)( void * );
		void (*del_)( void * );
		void * arg_;
		int priority_;
	};
	void load( const LoadOrder & lo );
	void load( Chunk * pChunk, int priority = 16 );
	void loadNow( Chunk * pChunk );
	void findSeed( ChunkSpace * pSpace, const Vector3 & where,
		Chunk *& rpChunk );

	SimpleThread * thread() const	{ return thread_; }
private:

	static void s_start( void * arg );

	void run();

	void loadNow( const LoadOrder & lo );
	static void loadChunkNow( void * arg );
	static void findSeedNow( void * arg );
	static void delSeedNow( void * arg );


	SimpleThread *		thread_;
	SimpleSemaphore		semaphore_;
	SimpleMutex			mutex_;

	typedef std::deque<LoadOrder>	LoadList;
	LoadList	loadList_;
};



#endif // CHUNK_LOADER_HPP
