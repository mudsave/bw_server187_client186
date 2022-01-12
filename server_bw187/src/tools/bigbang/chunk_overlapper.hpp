/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CHUNK_OVERLAPPER_HPP
#define CHUNK_OVERLAPPER_HPP

#include "chunk/chunk_item.hpp"

class Chunk;
class ChunkSpace;


/**
 *	This class is a chunk item that records another chunk overlapping
 *	the one it is in.
 */
class ChunkOverlapper : public ChunkItem
{
	DECLARE_CHUNK_ITEM( ChunkOverlapper )

public:
	ChunkOverlapper();
	~ChunkOverlapper();

	bool load( DataSectionPtr pSection, Chunk * pChunk, std::string* errorString = NULL );

	virtual void toss( Chunk * pChunk );
	virtual void draw();
	virtual void lend( Chunk * pLender );

	Chunk *			pOverlapper()			{ return pOverlapper_; }
	DataSectionPtr	pOwnSect()				{ return pOwnSect_; }

	/**
	 * Chunks which should be drawn; must be cleared every frame
	 */
	static std::vector<Chunk*> drawList;

private:
	ChunkOverlapper( const ChunkOverlapper& );
	ChunkOverlapper& operator=( const ChunkOverlapper& );

	void bindStuff();

	Chunk *			pOverlapper_;
	DataSectionPtr	pOwnSect_;
	bool			bound_;

	static bool		s_drawAlways_;		// at options: render/shells/gameVisibility
	static uint32	s_settingsMark_;
};

typedef SmartPointer<ChunkOverlapper> ChunkOverlapperPtr;


#include "chunk/chunk.hpp"

/**
 *	This class keeps track of all the overlappers in a chunk,
 *	and can form and cut them when a chunk is moved.
 */
class ChunkOverlappers : public ChunkCache
{
public:
	ChunkOverlappers( Chunk & chunk );
	~ChunkOverlappers();

	static Instance<ChunkOverlappers> instance;

	void add( ChunkOverlapperPtr pOverlapper );
	void del( ChunkOverlapperPtr pOverlapper );

	void form( Chunk * pOverlapper );
	void cut( Chunk * pOverlapper );

	typedef std::vector< ChunkOverlapperPtr > Items;

	Items overlappers()
	{
		return items_;
	}

private:
	Chunk *				pChunk_;

	Items				items_;
};


#endif // CHUNK_OVERLAPPER_HPP
