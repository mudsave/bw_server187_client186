/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CHUNK_WAYPOINT_SET_HPP
#define CHUNK_WAYPOINT_SET_HPP

#include "chunk/chunk_item.hpp"
#include "chunk/chunk_boundary.hpp"
#include "chunk/chunk.hpp"

#include <map>
#include <vector>

/*
struct DependentArrayAlloc
{
	DependentArrayAlloc( uint32 blsz ):
		block_( (Block*)malloc(blsz) ),
		blsz_( blsz ),
		used_( sizeof(Block) )
	{
		block_->prev = NULL;
	}
	~DependentArrayAlloc()
	{
		while (block_ != NULL)
		{
			Block * prev = block_->prev;
			free( block_ );
			block_ = prev;
		}
	}

	void * get( uint32 amt )
	{
		if (used_ + amt > blsz_)
		{
			if (amt > blsz_-sizeof(Block)) return NULL;

			Block * nblk = (Block*)malloc(blsz_);
			nblk->prev = block_;
			block_ = nblk;
			used_ = sizeof(Block);
		}
		uint8 * ret = ((uint8*)block_) + used_;
		used_ += amt;
		return (void*)ret;
	}

	struct Block
	{
		Block * prev;
	};
	Block * block_;
	uint32	blsz_;
	uint32	used_;
};
extern DependentArrayAlloc * g_DependentArrayAlloc;
*/

/**
 *	Array whose data lives inside another block.
 *	Must be following in memory by a uint16 size.
 */
template <class C> class DependentArray
{
public:
	typedef C value_type;
	typedef value_type * iterator;
	typedef const value_type * const_iterator;
	typedef typename std::vector<C>::size_type size_type;

	DependentArray() :
		data_( 0 )			{ _size() = 0; }
	DependentArray( iterator beg, iterator end ) :
		data_( beg )		{ _size() = end-beg; }
	DependentArray( const DependentArray & oth ) :
		data_( oth.data_ )	{ _size() = oth._size(); }
	DependentArray & operator=( const DependentArray & oth )
		{ new (this) DependentArray( oth ); return *this; }
	~DependentArray()
		{ }
/*
	void resize( size_type newsz )
	{
		data_ = (value_type*)
			g_DependentArrayAlloc->get( newsz*sizeof(value_type) );
		_size() = newsz;
	}

	void push_back( const value_type & val ) { }	// for now
*/

	size_type size() const	{ return _size(); }

	const value_type & operator[]( int i ) const { return data_[i]; }
	value_type & operator[]( int i ){ return data_[i]; }

	const value_type & front() const{ return *data_; }
	value_type & front()			{ return *data_; }
	const value_type & back() const { return data_[_size()-1]; }
	value_type & back()				{ return data_[_size()-1]; }

	const_iterator begin() const	{ return data_; }
	const_iterator end() const		{ return data_+_size(); }
	iterator begin()				{ return data_; }
	iterator end()					{ return data_+_size(); }

private:
	value_type * data_;

	uint16 _size() const	{ return *(uint16*)(this+1); }
	uint16 & _size()		{ return *(uint16*)(this+1); }
};


/**
 *	This class is a waypoint as it exists in a chunk, when fully loaded.
 */
class ChunkWaypoint
{
public:
	bool contains( const Vector3 & point ) const;
	bool containsProjection( const Vector3 & point ) const;
	float distanceSquared( const Chunk* chunk, const Vector3 & point ) const;
	void clip( const Chunk* chunk, Vector3& lpoint ) const;

	float minHeight_;
	float maxHeight_;
	Vector2 centre_;

	struct Edge
	{
		Vector2		start_;
		uint32		neighbour_;

		int neighbouringWaypoint() const		// index into waypoints_
			{ return neighbour_ < 32768 ? int(neighbour_) : -1; }

		bool adjacentToChunk() const
			{ return neighbour_ >= 32768 && neighbour_ <= 65535; }

		uint32 neighbouringVista() const		// vista bit flags
			{ return int(neighbour_) < 0 ? ~neighbour_ : 0; }
	};
	void print()
	{
		DEBUG_MSG( "MinHeight: %g\tMaxHeight: %g\tEdgeNum:%d\n",
			minHeight_, maxHeight_, edges_.size()  );

		for (uint16 i = 0; i < edges_.size(); ++i)
		{
			DEBUG_MSG( "\t%d (%g, %g) %d - %s\n", i,
				edges_[ i ].start_.x, edges_[ i ].start_.y, edges_[ i ].neighbouringWaypoint(),
				edges_[ i ].adjacentToChunk() ? "chunk" : "no chunk" );
		}
	}

	//typedef std::vector<Edge>	Edges;
	typedef DependentArray<Edge> Edges;
	Edges	edges_;
	uint16	edgeCount_; // DO NOT move this away! DependentArray depends on
						// edgeCount_ to exist on its construction.

	mutable uint16	mark_;

	static uint16 s_nextMark_;

	void calcCentre()
	{
		float totalLength = 0.f;

		centre_ = Vector2( 0, 0 );

		for (uint16 i = 0; i < edgeCount_; ++i)
		{
			Vector2 start = edges_[ i ].start_;
			Vector2 end = edges_[ ( i + 1 ) % edgeCount_ ].start_;
			float length = ( end - start ).length();

			centre_ += ( end + start ) / 2 * length;
			totalLength += length;
		}

		centre_ /= totalLength;
	}
};
/*
template <int I> struct SizeCatch { };
template <> struct SizeCatch<12> { typedef int Check; };
extern SizeCatch<sizeof(ChunkWaypoint::Edge)>::Check tester;
*/

typedef std::vector<ChunkWaypoint> ChunkWaypoints;
//typedef DependentArray<ChunkWaypoint> ChunkWaypoints;

typedef uint32 WaypointEdgeIndex;

/**
 *	This class contains the data read in from a waypoint set.
 *	It may be shared between chunks. (In which case it is in local co-ords.)
 */
class ChunkWaypointSetData : public SafeReferenceCount
{
public:
	ChunkWaypointSetData();
	virtual ~ChunkWaypointSetData();

	bool loadFromXML( DataSectionPtr pSection, const std::string & sectionName);
	void transform( const Matrix & tr );

	int find( const Vector3 & lpoint, bool ignoreHeight = false );
	int find( const Chunk* chunk, const Vector3 & lpoint, float & bestDistanceSquared );

	int getAbsoluteEdgeIndex( const ChunkWaypoint::Edge & edge ) const;

private:
	float						girth_;
	ChunkWaypoints				waypoints_;
	std::string					source_;
	ChunkWaypoint::Edge 	* edges_;

	friend class ChunkWaypointSet;
	friend class ChunkNavPolySet;
};

typedef SmartPointer<ChunkWaypointSetData> ChunkWaypointSetDataPtr;



class ChunkWaypointSet;
typedef SmartPointer<ChunkWaypointSet> ChunkWaypointSetPtr;
typedef std::vector<ChunkWaypointSetPtr> ChunkWaypointSets;

typedef std::map<ChunkWaypointSetPtr, ChunkBoundary::Portal *>
	ChunkWaypointConns;
typedef std::map<WaypointEdgeIndex, ChunkWaypointSetPtr>
	ChunkWaypointEdgeLabels;


/**
 *	This class is a set of connected waypoints in a chunk.
 *	It may have connections to other waypoint sets when its chunk is bound.
 */
class ChunkWaypointSet : public ChunkItem
{
	DECLARE_CHUNK_ITEM( ChunkWaypointSet )

public:
	ChunkWaypointSet();
	~ChunkWaypointSet();

	bool load( Chunk * pChunk, DataSectionPtr pSection,
		const char * sectionName, bool inWorldCoords );

	virtual void toss( Chunk * pChunk );

	void bind();

	int find( const Vector3 & lpoint, bool ignoreHeight = false )
		{ return data_->find( lpoint, ignoreHeight ); }
	int find( const Vector3 & lpoint, float & bestDistanceSquared )
	{
		return data_->find( chunk(), lpoint, bestDistanceSquared );
	}

	float girth() const
		{ return data_->girth_; }

	int waypointCount() const
		{ return data_->waypoints_.size(); }
	ChunkWaypoint & waypoint( int index )
		{ return data_->waypoints_[index]; }

	ChunkWaypointConns::iterator connectionsBegin()
		{ return connections_.begin(); }
	ChunkWaypointConns::const_iterator connectionsBegin() const
		{ return connections_.begin(); }
	ChunkWaypointConns::iterator connectionsEnd()
		{ return connections_.end(); }
	ChunkWaypointConns::const_iterator connectionsEnd() const
		{ return connections_.end(); }
	ChunkBoundary::Portal * connectionPortal( ChunkWaypointSetPtr pWaypointSet )
		{ return connections_[pWaypointSet]; }

	ChunkWaypointSetPtr connectionWaypoint(
			const ChunkWaypoint::Edge & edge )
		{ return edgeLabels_[data_->getAbsoluteEdgeIndex( edge )]; }

	void addBacklink( ChunkWaypointSetPtr pWaypointSet );
	void removeBacklink( ChunkWaypointSetPtr pWaypointSet );

	void print()
	{
		DEBUG_MSG( "ChunkWayPointSet: 0x%x - %s\tWayPointCount: %d\n", this, pChunk_->identifier().c_str(), waypointCount() );

		for (int i = 0; i < waypointCount(); ++i)
			waypoint( i ).print();

		for (ChunkWaypointConns::iterator iter = connectionsBegin();
			iter != connectionsEnd(); ++iter)
		{
			DEBUG_MSG( "**** connecting to 0x%x %s", iter->first.getObject(), iter->first->pChunk_->identifier().c_str() );
		}
	}

private:

	void deleteConnection( ChunkWaypointSetPtr pSet );

	void removeOurConnections();
	void removeOthersConnections();

	//int firstFreeLabel();

	void connect(
		ChunkWaypointSetPtr pWaypointSet,
		ChunkBoundary::Portal * pPortal,
		ChunkWaypoint::Edge & edge );

protected:

	ChunkWaypointSetDataPtr	data_;
	ChunkWaypointConns			connections_;
	ChunkWaypointEdgeLabels		edgeLabels_;
	ChunkWaypointSets			backlinks_;
};


/**
 *  This class is a cache of all the waypoint sets in a chunk.
 */
class ChunkNavigator : public ChunkCache
{
public:
	ChunkNavigator( Chunk & chunk );
	~ChunkNavigator();

	virtual void bind( bool looseNotBind );


	struct Result
	{
		Result() : pSet_( NULL ), waypoint_( -1 ) { }

		ChunkWaypointSetPtr	pSet_;
		int					waypoint_;
		bool				exactMatch_;
	};
	bool find( const Vector3 & lpoint, float girth,
				Result & res, bool ignoreHeight = false );
	bool findExactly( const Vector3 & lpoint, float girth, Result & res );

	bool isEmpty();
	bool hasNavPolySet( float girth );

	void add( ChunkWaypointSet * pSet );
	void del( ChunkWaypointSet * pSet );

	static Instance<ChunkNavigator> instance;
	static bool s_useGirthGrids_;

private:
	Chunk & chunk_;

	ChunkWaypointSets	wpSets_;

	// grid optimisation below here
	struct GGElement
	{
		ChunkWaypointSet	* pSet_;	// dumb pointer
		int					waypoint_;

		GGElement( ChunkWaypointSet	* pSet, int waypoint )
			: pSet_(pSet), waypoint_(waypoint)
		{}
	};

	class GGList : public std::vector<GGElement>
	{
	public:
		bool find( const Vector3 & lpoint, Result & res,
							bool ignoreHeight = false );
		void find( const Chunk* chunk, const Vector3 & lpoint,
			float & bestDistanceSquared, Result & res );
	};

	struct GirthGrid
	{
		float	girth;
		GGList	* grid;
	};
	typedef std::vector<GirthGrid> GirthGrids;

	GirthGrids	girthGrids_;
	Vector2		ggOrigin_;
	float		ggResolution_;
	static const uint GG_SIZE = 12;
};







#endif // CHUNK_WAYPOINT_SET_HPP
