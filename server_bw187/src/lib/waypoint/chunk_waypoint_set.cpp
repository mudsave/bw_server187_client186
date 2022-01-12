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

#include "chunk_waypoint_set.hpp"
#include "chunk/chunk_space.hpp"

#include "waypoint/waypoint.hpp"

DECLARE_DEBUG_COMPONENT2( "Waypoint", 0 )

// -----------------------------------------------------------------------------
// Section: ChunkWaypoint
// -----------------------------------------------------------------------------

/**
 *	This method indicates whether or not the waypoint contains the input point.
 */
bool ChunkWaypoint::contains( const Vector3 & point ) const
{
	if (point.y < minHeight_ - 0.1f) return false;
	if (point.y > maxHeight_ + 0.1f) return false;

	return this->containsProjection( point );
}

/**
 *	This method indicates whether the input point lie within the waypoint
 *	polygon in x-z plane.
 */

bool ChunkWaypoint::containsProjection( const Vector3 & point ) const
{
	float u, v, xd, zd, c;
	bool inside = true;
	Edges::const_iterator eit = edges_.begin();
	Edges::const_iterator end = edges_.end();

	const Vector2 * pLastPoint = &edges_.back().start_;

	int i = 0;
	while (eit != end)
	{
		i = eit - edges_.begin();
		const Vector2 & thisPoint = eit++->start_;

		u = thisPoint.x - pLastPoint->x;
		v = thisPoint.y - pLastPoint->y;

		xd = point.x - pLastPoint->x;
		zd = point.z - pLastPoint->y;

		c = xd * v - zd * u;

		// since lpoint now clips to the edge add a
		// fudge factor to accommodate floating point error.
		inside &= (c > -0.01f);
		if (!inside)
		{
			return false;
		}
		pLastPoint = &thisPoint;
	}

	return inside;
	//return true;
}


/**
 *	This method returns the squared distance from waypoint to input point.
 */
float ChunkWaypoint::distanceSquared( const Chunk* chunk, const Vector3 & lpoint ) const
{
	// TODO: This is a fairly inefficient. We may want to look at optimising it.
	Vector3 clipPoint( lpoint );
	this->clip( chunk, clipPoint );

	return (lpoint - clipPoint).lengthSquared();
}

namespace
{

// Get the projected point on line segment delimited by start and end.
// Return false if the projected point is not on this segment
bool projectPointToLine( const Vector2& start, const Vector2& end, Vector2& point )
{
	Vector2 dir = end - start;
	Vector2 project = point - start;
	float length = dir.length();

	dir.normalise();

	float dot = project.dotProduct( dir );

	if (0.f <= dot && dot <= length)
	{
		point = start + dir * dot;
		return true;
	}

	return false;
}

}

/**
 *	This method clips the lpoint to the edge of the waypoint.
 */
void ChunkWaypoint::clip( const Chunk* chunk, Vector3 & lpoint ) const
{
	Edges::const_iterator eit = edges_.begin();
	Edges::const_iterator end = edges_.end();

	const Vector2 * pPrevPoint = &edges_.back().start_;

	while (eit != end)
	{
		const Vector2 & thisPoint = eit->start_;

		Vector2 edgeVec( thisPoint - *pPrevPoint );
		Vector2 pointVec( lpoint.x - pPrevPoint->x, lpoint.z - pPrevPoint->y );

		bool isOutsidePoly = edgeVec.crossProduct( pointVec ) > 0.f;

		if (isOutsidePoly)
			break;

		pPrevPoint = &thisPoint;
		++eit;
	}

	if (eit != end)
	{
		Vector2 p2d( lpoint.x, lpoint.z );
		float bestDistSquared = FLT_MAX;
		Edges::const_iterator bestIt;

		for (eit = edges_.begin(); eit != end; ++eit)
		{
			const Edge* edge = &*eit;

			float distSquared = ( edge->start_ - p2d ).lengthSquared();

			if (distSquared < bestDistSquared)
			{
				bestDistSquared = distSquared;
				bestIt = eit;
			}
		}

		Vector2 prev = ( bestIt == edges_.begin() ) ? edges_.back().start_ : ( bestIt - 1 )->start_;

		if (!projectPointToLine( bestIt->start_, prev, p2d ))
		{
			Vector2 next = ( bestIt + 1 == end ) ? edges_.front().start_ : ( bestIt + 1 )->start_;

			if (!projectPointToLine( bestIt->start_, next, p2d ))
			{
				p2d = bestIt->start_;
			}
		}

		lpoint.x = p2d.x;
		lpoint.z = p2d.y;
	}

	const BoundingBox& bb = chunk->boundingBox();

	lpoint.y = bb.centre().y;

	if (!bb.intersects( lpoint ))
	{
		for (eit = edges_.begin(); eit != end; ++eit)
		{
			const Edge* edge = &*eit;

			if (!edge->adjacentToChunk())
			{
				const Edge* next = ( eit + 1 == end ) ? &edges_.front() : &*( eit + 1 );

				Vector3 start( edge->start_.x, bb.centre().y, edge->start_.y );
				Vector3 end( next->start_.x, bb.centre().y, next->start_.y );

				bb.clip( start, end );
				Vector3 middle = ( start + end ) / 2;
				bb.clip( middle, lpoint );

				Vector3 dir( middle - lpoint );
				dir.normalise();
				lpoint += dir * 0.01f; // move in a bit

				break;
			}
		}
	}

	lpoint.y = maxHeight_;
}

uint16 ChunkWaypoint::s_nextMark_ = 256;


// -----------------------------------------------------------------------------
// Section: ChunkWaypointSetData
// -----------------------------------------------------------------------------

extern void NavmeshPopulation_remove( const std::string & source );


/**
 *	Constructor
 */
ChunkWaypointSetData::ChunkWaypointSetData() :
	girth_( 0.f ),
	source_(),
	edges_( NULL )
{
}

/**
 *	Destructor
 */
ChunkWaypointSetData::~ChunkWaypointSetData()
{
	if (!source_.empty())
	{
		NavmeshPopulation_remove( source_ );
	}

	if (edges_) delete [] edges_;
}



/**
 *	Load the set from the given data section
 *
 * @param pSection  chunk file DataSectionPtr.
 * @param sectionName  will be waypoint (from IMPLEMENT_CHUNK_ITEM macro). This
 *   param is here so that ChunkNavPolySet can derive from ChunkWaypointSet and
 *   load navPoly sections.
 */
bool ChunkWaypointSetData::loadFromXML( DataSectionPtr pSection,
	const std::string & sectionName )
{
	girth_ = pSection->readFloat( "girth", 0.5f );

	typedef ChunkWaypoint::Edge Edge;
	std::vector<Edge>	edgesBuf;
	std::map<int,int>	wpids;

	DataSectionIterator dit = pSection->begin();
	DataSectionIterator dnd = pSection->end();
	while (dit != dnd)
	{
		// keep going around until we find a navPoly (or legacy waypoint)
		// section.
		DataSectionPtr pWaypoint = *dit++;
		if ( pWaypoint->sectionName() != sectionName ) continue;

		waypoints_.push_back( ChunkWaypoint() );
		ChunkWaypoint & cw = waypoints_.back();

		wpids[ pWaypoint->asInt() ] = waypoints_.size()-1;

		float defHeight = pWaypoint->readFloat( "height", 0.f );
		cw.minHeight_ = pWaypoint->readFloat( "minHeight", defHeight );
		cw.maxHeight_ = pWaypoint->readFloat( "maxHeight", defHeight );
		(uintptr&)cw.edges_ = sizeof(Edge)*edgesBuf.size();
		cw.edgeCount_ = 0;

		DataSectionIterator vit = pWaypoint->begin();
		DataSectionIterator vnd = pWaypoint->end();
		while (vit != vnd)
		{
			DataSectionPtr pVertex = *vit++;
			if (pVertex->sectionName() != "vertex") continue;

			edgesBuf.push_back( ChunkWaypoint::Edge() );
			ChunkWaypoint::Edge & ce = edgesBuf.back();
			cw.edgeCount_++;

			Vector3 v = pVertex->asVector3();
			ce.start_.x = v.x;
			ce.start_.y = v.y;
			int32 vzi = int(v.z);

			// if there is adjacentChunk section (legacy), we're
			// next to a chunk.
			if (pVertex->openSection( "adjacentChunk" ))
			{
				ce.neighbour_ = 65535;
			}
			// else if there adjacentChunk magic constant, we're
			// next to a chunk.
			else if (vzi == CHUNK_ADJACENT_CONSTANT)
			{
				ce.neighbour_ = 65535;
			}
			// otherwise this is a normal navpoly adjacency or
			// annotation.
			else
			{
				ce.neighbour_ = vzi > 0 ? vzi : ~-vzi;
			}

		}
		cw.calcCentre();

		MF_ASSERT( cw.edges_.size() >= 3 )

		cw.mark_ = cw.s_nextMark_ - 16;
	}

	// now that we know how many edges we have, put them into one big block
	edges_ = new Edge[edgesBuf.size()];
	memcpy( edges_, &edgesBuf.front(), sizeof(Edge)*edgesBuf.size() );

	// fix up waypoint ids and edge pointers
	ChunkWaypoints::iterator wit;
	ChunkWaypoint::Edges::iterator eit;
	std::map<int,int>::iterator nit;
	for (wit = waypoints_.begin(); wit != waypoints_.end(); wit++)
	{
		(uintptr&)wit->edges_ += (uintptr)edges_;

		for (eit = wit->edges_.begin(); eit != wit->edges_.end(); eit++)
		{
			if (eit->neighbouringWaypoint() >= 0)
			{
				nit = wpids.find( eit->neighbour_ );
				if (nit == wpids.end())
				{
					ERROR_MSG( "ChunkWaypointSet::load: "
						"Cannot find neighbouring waypoint %d "
						"on edge %d of waypoint index %d\n",
						eit->neighbour_,
						eit - wit->edges_.begin(),
						wit - waypoints_.begin() );
					return false;
				}
				MF_ASSERT( nit != wpids.end() );
				eit->neighbour_ = nit->second;
			}
		}
	}

	return true;
}

/**
 *	Apply given transform. The Y axis (tr[1]) must point straight up.
 */
void ChunkWaypointSetData::transform( const Matrix & tr )
{
	ChunkWaypoints::iterator wit;
	ChunkWaypoint::Edges::iterator eit;
	const Vector3 & ytrans = tr[1];
	const float yoff = tr.applyToOrigin()[1];

	for (wit = waypoints_.begin(); wit != waypoints_.end(); wit++)
	{
		wit->minHeight_ = ytrans.y * wit->minHeight_ + yoff;
		wit->maxHeight_ = ytrans.y * wit->maxHeight_ + yoff;

		for (eit = wit->edges_.begin(); eit != wit->edges_.end(); eit++)
		{
			Vector3 v = tr.applyPoint(
				Vector3( eit->start_.x, 0.f, eit->start_.y ) );
			eit->start_.x = v.x; eit->start_.y = v.z;
		}

		wit->calcCentre();
	}
}


/**
 *	Find the waypoint that contains the given point.
 *	@param lpoint	The point that is used to find the waypoint.
 *	@param ignoreHeight	Flag indicates vertical range should be
 *					considered in finding waypoint. If not, best
 *					waypoint that is closest to give point is
 *					selected (of cause the waypoint should contain
 *					projection of the given point regardless.)
 *
 *	@return the index of the waypoint, or -1 if not found
 */
int ChunkWaypointSetData::find( const Vector3 & lpoint, bool ignoreHeight )
{
	int bestWaypoint = -1;
	float bestHeightDiff = FLT_MAX;

	ChunkWaypoints::iterator wit;
	for (wit = waypoints_.begin(); wit != waypoints_.end(); ++wit)
	{
		if (ignoreHeight)
		{
			if (wit->containsProjection( lpoint ))
			{
				if (lpoint.y > wit->minHeight_ - 0.1f &&
						lpoint.y < wit->maxHeight_ + 0.1f)
				{
					return wit - waypoints_.begin();
				}
				else // find best fit
				{
					float wpAvgDiff = fabs( lpoint.y -
						(wit->maxHeight_ + wit->minHeight_) / 2 );
					if (bestHeightDiff > wpAvgDiff)
					{
						bestHeightDiff = wpAvgDiff;
						bestWaypoint = wit - waypoints_.begin();
					}
				}
			}
		}
		else
		{
			if (wit->contains( lpoint ))
				return wit - waypoints_.begin();
		}
	}
	return bestWaypoint;
}


/**
 *	This method finds the waypoint closest to the given point.
 *
 *	@param lpoint	The point that is used to find the closest waypoint.
 *	@param bestDistanceSquared
 *		The point must be closer than this to the waypoint.
 *		It is updated to the new best distance if a better waypoint is found.
 *	@return the index of the waypoint, or -1 if not found
 */
int ChunkWaypointSetData::find( const Chunk* chunk, const Vector3 & lpoint,
	float & bestDistanceSquared )
{
	int bestWaypoint = -1;
	ChunkWaypoints::iterator wit;
	for (wit = waypoints_.begin(); wit != waypoints_.end(); ++wit)
	{
		float distanceSquared = wit->distanceSquared( chunk, lpoint );
		if (bestDistanceSquared > distanceSquared)
		{
			bestDistanceSquared = distanceSquared;
			bestWaypoint = wit - waypoints_.begin();
		}
	}
	return bestWaypoint;
}


/**
 *	Get the index of the given edge into this set data's edges array.
 */
int ChunkWaypointSetData::getAbsoluteEdgeIndex(
		const ChunkWaypoint::Edge & edge ) const
{
	return &edge - edges_;
}

// -----------------------------------------------------------------------------
// Section: ChunkWaypointSet
// -----------------------------------------------------------------------------

// yucky yucky macros.
#undef IMPLEMENT_CHUNK_ITEM_ARGS
#define IMPLEMENT_CHUNK_ITEM_ARGS ( pChunk, pSection, "waypoint", true )
IMPLEMENT_CHUNK_ITEM( ChunkWaypointSet, waypointSet, 0 )	// in world coords


/**
 *	Constructor
 */
ChunkWaypointSet::ChunkWaypointSet():
	data_( NULL ),
	connections_(),
	edgeLabels_(),
	backlinks_()
{
}

/**
 *	Destructor
 */
ChunkWaypointSet::~ChunkWaypointSet()
{
}


/**
 *	Load the set from the given data section
 *
 * @param pChunk the chunk the ChunkWaypointSet is in.
 * @param pSection  chunk file DataSectionPtr.
 * @param sectionName  will be waypoint (from IMPLEMENT_CHUNK_ITEM macro). This
 *   param is here so that ChunkNavPolySet can derive from ChunkWaypointSet and
 *   load navPoly sections.
 * @param inWorldCoords indicates whether or not the coordinates are in world
 *	coordinates.
 */
bool ChunkWaypointSet::load( Chunk * pChunk, DataSectionPtr pSection,
	const char * sectionName, bool inWorldCoords )
{
	std::string sectionNameStr = sectionName;

	ChunkWaypointSetDataPtr cwsd = new ChunkWaypointSetData();
	bool ok = cwsd->loadFromXML( pSection, sectionNameStr );
	if (!ok) return false;

	if (inWorldCoords)
		cwsd->transform( pChunk->transformInverse() );

	data_ = cwsd;
	return true;
}


/**
 * If we have any waypoints with any edges that link to other waypont sets,
 * remove our labels.  Remove us from the backlinks in the waypointSets we
 * linked to, then delete our connections list.
 */
void ChunkWaypointSet::removeOurConnections()
{
	// first set all our external edge labels back to 65535.
	edgeLabels_.clear();

	// now remove us from all the backlinks of the chunks we put
	// ourselves in when we set up those connections.
	ChunkWaypointConns::iterator i = connections_.begin();
	for (; i != connections_.end(); ++i)
	{
		i->first->removeBacklink( this );
	}

	connections_.clear();
}



/**
 * remove the connection with index conNum from connections_
 * and set all edges corresponding to this connection to 65535.
 * also remove us from the backlinks_ list in the waypointSet
 * the connection was to.
 */
void ChunkWaypointSet::deleteConnection( ChunkWaypointSetPtr pSet )
{
	ChunkWaypointConns::iterator found;
	if ((found = connections_.find( pSet )) == connections_.end())
	{
		ERROR_MSG( "ChunkWaypointSet::deleteConnection: connection from %d/%s "
				"to %d/%s does not exist\n",
			this->chunk()->space()->id(),
			this->chunk()->identifier().c_str(),
			pSet->chunk()->space()->id(),
			pSet->chunk()->identifier().c_str() );
		return;
	}

	// (1) remove our edge labels for this chunk waypoint set
	std::vector<WaypointEdgeIndex> removingEdges;
	ChunkWaypointEdgeLabels::iterator edgeLabel = edgeLabels_.begin();
	for (;edgeLabel != edgeLabels_.end(); ++edgeLabel)
	{
		if (edgeLabel->second == pSet)
		{
			// add to removingEdges to remove it from the map after loop
			removingEdges.push_back( edgeLabel->first );
		}
	}
	while (removingEdges.size())
	{
		connections_.erase( edgeLabels_[removingEdges.back()] );
		edgeLabels_.erase( removingEdges.back() );
		removingEdges.pop_back();
	}

	// (2) now remove from backlinks list of chunk we connected to.
	pSet->removeBacklink( this );


	// (3) now remove from us, and all trace of connection is gone!
	// connections_.erase( connections_.begin() + conNum );
	connections_.erase( pSet );

}


/**
 * go through all waypoint sets that link to us, and
 * tell them not to link to us anymore.
 */
void ChunkWaypointSet::removeOthersConnections()
{
	// loop around all our backlinks_ [waypoint sets that contain
	// edges that link back to us].

	while (backlinks_.size())
	{
		ChunkWaypointSets::iterator bcIter = backlinks_.begin();
		ChunkWaypointSetPtr pSet = backlinks_.front();

		// loop around the connections in this waypointSet with
		// a backlink to us looking for it..
		bool found = false;
		ChunkWaypointConns::iterator otherConn = pSet->connectionsBegin();
		for (;otherConn != pSet->connectionsEnd(); ++otherConn)
		{
			if (otherConn->first == this)
			{
				found = true;
				break;
			}

		}

		if (!found)
		{
			ERROR_MSG( "ChunkWaypointSet::removeOthersConnections: "
				"Back connection not found.\n" );
			backlinks_.erase( bcIter );
			continue;
		}

		// will delete the current backlink from our list.
		(*bcIter)->deleteConnection( ChunkWaypointSetPtr( this ) );
	}

}


/**
 * Add a waypointSet to our backlink list.
 */
void ChunkWaypointSet::addBacklink( ChunkWaypointSetPtr pWaypointSet )
{
	backlinks_.push_back( pWaypointSet );
}


/**
 * Remove a waypointSet from our backlinks list.
 */
void ChunkWaypointSet::removeBacklink( ChunkWaypointSetPtr pWaypointSet )
{
	ChunkWaypointSets::iterator blIter = backlinks_.begin();
	for( ; blIter != backlinks_.end(); ++blIter )
	{
		if ( *blIter == pWaypointSet )
		{
			backlinks_.erase( blIter );
			return;
		}
	}

	ERROR_MSG( "ChunkWaypointSet::removeBacklink: "
		"trying to remove backlink that doesn't exist\n" );

}

/**
 * Connect edge in the current waypointSet to pWaypointSet.
 *
 * (1) if connection doesn't exist in connections_, this is
 *     created together with a backlink to us in pWaypointSet.
 *
 * (2) update the edge Label
 */
void ChunkWaypointSet::connect(
						ChunkWaypointSetPtr pWaypointSet,
						ChunkBoundary::Portal * pPortal,
						ChunkWaypoint::Edge & edge )
{
	WaypointEdgeIndex edgeIndex = data_->getAbsoluteEdgeIndex( edge );

	if (edge.neighbour_ != 65535)
	{
		WARNING_MSG( "ChunkWaypointSet::connect called on "
			"non chunk-adjacent edge\n" );
		return;
	}

	ChunkWaypointConns::iterator found = connections_.find( pWaypointSet );

	if (found == connections_.end())
	{
		connections_[pWaypointSet] = pPortal;

		// now add backlink to chunk connected to.
		pWaypointSet->addBacklink( this );

	}
	edgeLabels_[edgeIndex] = pWaypointSet;
}

/**
 *	Toss into the given chunk
 */
void ChunkWaypointSet::toss( Chunk * pChunk )
{
	if (pChunk == pChunk_) return;

	// out with the old
	if (pChunk_ != NULL)
	{

		// Note: program flow arrives here when pChunk_ is
		// being ejected (with pChunk = NULL)

		//this->toWorldCoords();
		this->removeOthersConnections();
		this->removeOurConnections();

		ChunkNavigator::instance( *pChunk_ ).del( this );
	}

	this->ChunkItem::toss( pChunk );

	// and in with the new
	if (pChunk_ != NULL)
	{
		if (pChunk_->online())
		{
			CRITICAL_MSG( "ChunkWaypointSet::toss: "
				"Tossing after loading is not supported\n" );
		}

		//this->toLocalCoords() // ... now already there

		// now that we are in local co-ords we can add ourselves to the
		// cache maintained by ChunkNavigator
		ChunkNavigator::instance( *pChunk_ ).add( this );
	}
}


/**
 *	Bind any unbound waypoints
 */
void ChunkWaypointSet::bind()
{
	// we would like to first make sure that all our existing connections
	// still exist. unfortunately we can't tell what is being unbound,
	// so we have to delay this until the set we are connected to is tossed
	// out of its chunk.

	// now make new connections
	ChunkWaypoints::iterator wit;
	ChunkWaypoint::Edges::iterator eit;

	for (wit = data_->waypoints_.begin(); wit != data_->waypoints_.end(); ++wit)
	{
		float wymin = wit->minHeight_;
		float wymax = wit->maxHeight_;
		float wyavg = (wymin + wymax) * 0.5f + 0.1f;

		for (eit = wit->edges_.begin(); eit != wit->edges_.end(); ++eit)
		{
			if (eit->neighbour_ != 65535) continue;

			ChunkWaypoint::Edges::iterator nextEdgeIter = eit + 1;
			if (nextEdgeIter == wit->edges_.end())
				nextEdgeIter = wit->edges_.begin();

			Vector3 v = Vector3( (eit->start_.x + nextEdgeIter->start_.x)/2.f,
					0.f, (eit->start_.y + nextEdgeIter->start_.y)/2.f );

			Vector3 wv = v;
			wv.y = wyavg;
			Vector3 lwv = pChunk_->transformInverse().applyPoint( wv );

			Chunk::piterator pit = pChunk_->pbegin();
			Chunk::piterator pnd = pChunk_->pend();
			ChunkBoundary::Portal * cp = NULL;
			while (pit != pnd)
			{
				if (pit->hasChunk() && pit->pChunk->boundingBox().intersects( wv ))
				{
					// only use test for minimum distance from portal plane
					// for indoor chunks
					float minDist = pit->pChunk->isOutsideChunk() ?
						0.f : 1.f;
					if (Chunk::findBetterPortal( cp, minDist, &*pit, lwv ))
					{
						cp = &*pit;
					}
				}
				pit++;
			}
			if (cp == NULL)
			{
				// try a second time with height set to maxHeight_ since there
				// is a chance that one chunk contains steep slope avg waypoint
				// height lies just outside of the portal.
				wv.y = wymax + 0.1f;
				lwv = pChunk_->transformInverse().applyPoint( wv );

				Chunk::piterator pit = pChunk_->pbegin();
				Chunk::piterator pnd = pChunk_->pend();
				while (pit != pnd)
				{
					if (pit->hasChunk() && pit->pChunk->boundingBox().intersects( wv ))
					{
						// only use test for minimum distance from portal plane
						// for indoor chunks
						float minDist = pit->pChunk->isOutsideChunk() ?
							0.f : 1.f;
						if (Chunk::findBetterPortal( cp, minDist, &*pit, lwv ))
						{
							cp = &*pit;
						}
					}
					pit++;
				}
				if (cp == NULL)
				{
					continue;
				}
			}
			Chunk * pConn = cp->pChunk;

			// make sure we have a corresponding waypoint on the other side
			// of the portal.
			ChunkNavigator::Result cnr;
			
			//try middle point
			wv.y = wyavg;
			if (!ChunkNavigator::instance(*pConn).findExactly(
				wv, girth(), cnr))
			{
				//try a little offest from start point to end point
				wv.set(eit->start_.x, wyavg, eit->start_.y);
				
				Vector3 dir(nextEdgeIter->start_.x - wv.x, 0, 
							nextEdgeIter->start_.y - wv.z);
				dir.normalise();
				
				wv += dir * 0.08f;
				if (!ChunkNavigator::instance(*pConn).findExactly(
						wv, girth(), cnr))
				{
					continue;
				}
			}
#if 0
			ChunkWaypoint & wp = cnr.pSet_->waypoint( cnr.waypoint_ );
			ChunkWaypoint::Edges::iterator beit = wp.edges_.begin();

			// This check is may be too restrictive. It has problem
			// with odd nav triangle with one vertex cross portal
			// It is not marked as adjacent to other chunk ATM (I still
			// think it should be marked).
			while (beit != wp.edges_.end() && !beit->adjacentToChunk())
				++beit;
			if (beit != wp.edges_.end())
#endif
				this->connect( &*cnr.pSet_, cp, *eit );
		}
	}

}


// static initialiser
int ChunkWaypointSet_token = 0;



// -----------------------------------------------------------------------------
// Section: ChunkNavigator
// -----------------------------------------------------------------------------

/**
 *  Constructor
 */
ChunkNavigator::ChunkNavigator( Chunk & chunk ) :
	chunk_( chunk )
{
  if (s_useGirthGrids_)
  {
	// set up the girth grid parameters
	const BoundingBox & bb = chunk_.boundingBox();
	float maxDim = std::max( bb.maxBounds().x - bb.minBounds().x,
		bb.maxBounds().z - bb.minBounds().z );
	float oneSqProp = 1.f / (GG_SIZE-2);	// extra grid square off each edge
	ggOrigin_ = Vector2( bb.minBounds().x - maxDim*oneSqProp,
		bb.minBounds().z - maxDim*oneSqProp );
	ggResolution_ = 1.f / (maxDim*oneSqProp);
  }
}

/**
 *  Destructor
 */
ChunkNavigator::~ChunkNavigator()
{
  if (s_useGirthGrids_)
  {
	for (GirthGrids::iterator git = girthGrids_.begin();
		git != girthGrids_.end();
		++git)
	{
		delete [] (*git).grid;
	}
	girthGrids_.clear();
  }
}


/**
 *	Bind method that tells all our waypoint sets about it
 */
void ChunkNavigator::bind( bool /*looseNotBind*/ )
{
	// TODO: Should this be being called in the looseNotBind == true case?
	ChunkWaypointSets::iterator it;
	for (it = wpSets_.begin(); it != wpSets_.end(); ++it)
	{
		(*it)->bind();
	}
}

/**
 *  Find the waypoint and its set to the given point,
 *  and of accommodating girth. We use this method when
 *  bind a ChunkWaypointSet to others.
 */
bool ChunkNavigator::findExactly( const Vector3 & lpoint, float girth, Result & res )
{
	float bestDistanceSquared = FLT_MAX;
	for (ChunkWaypointSets::iterator it = wpSets_.begin(); it != wpSets_.end(); ++it)
	{
		ChunkWaypointSetPtr pChunkWpSet = *it;
		if (pChunkWpSet->girth() != girth) 
		{
			continue;
		}
		int foundWaypointID = pChunkWpSet->find(lpoint, true);
		if (foundWaypointID >= 0)
		{
			const ChunkWaypoint& chunkWaypoint = pChunkWpSet->waypoint(foundWaypointID);
			
			float curDistance = lpoint.y - 
						(chunkWaypoint.minHeight_ + chunkWaypoint.maxHeight_) / 2.0f;
			
			float curDistanceSquared = curDistance * curDistance;
			if (bestDistanceSquared > curDistanceSquared)
			{
				bestDistanceSquared = curDistanceSquared;
				res.pSet_ = pChunkWpSet;
				res.waypoint_ = foundWaypointID;
				res.exactMatch_ = true;
			}
		}
	}
	return bestDistanceSquared != FLT_MAX;
}


/**
 *  Find the waypoint and its set closest to the given point,
 *  and of accommodating girth.
 */
bool ChunkNavigator::find( const Vector3 & lpoint, float girth,
								Result & res, bool ignoreHeight )
{
	GirthGrids::iterator git;
	for (git = girthGrids_.begin(); git != girthGrids_.end(); ++git)
	{
		if ((*git).girth == girth) break;
	}
	if (git != girthGrids_.end())
	{
		// we have an appropriate girth grid
		int xg = int((lpoint.x - ggOrigin_.x) * ggResolution_);
		int zg = int((lpoint.z - ggOrigin_.y) * ggResolution_);
		// no need to round above conversions as always +ve
		if (uint(xg) >= GG_SIZE || uint(zg) >= GG_SIZE)
		{
			return false;	// hmmm
		}
		GGList * gg = (*git).grid;

		// try exact match first
		if (gg[xg+zg*GG_SIZE].find( lpoint, res, ignoreHeight ))
		{
			return true;
		}

		// try to find closest one then
		float bestDistanceSquared = FLT_MAX;

		if (!ignoreHeight)
		{
			// When we cannot find a navmesh with exact height match,
			// we try to find one below it.
			// TODO: consider multi layer overlapped navmesh and 
			// refine this bit of code to support that.
			if (gg[xg+zg*GG_SIZE].find( lpoint, res, true ))
			{
				ChunkWaypoint & wp = res.pSet_->waypoint( res.waypoint_ );

				if (wp.minHeight_ < lpoint.y)
				{
					bestDistanceSquared = wp.maxHeight_ - lpoint.y;
					bestDistanceSquared *= bestDistanceSquared;
				}
			}
		}

		#define TRY_GRID( xi, zi )													\
		{																			\
			int x = (xi);															\
			int z = (zi);															\
			if (uint(x) < GG_SIZE && uint(z) < GG_SIZE)								\
				gg[x+z*GG_SIZE].find( &chunk_, lpoint, bestDistanceSquared, res );	\
		}																			\

		// first try the original grid square
		TRY_GRID( xg, zg )

		// now try ever-increasing rings around it
		for (int r = 1; r < 12; r++)
		{
			bool hadCandidate = !!res.pSet_;

			// r is the radius of our ever-increasing square
			int xgCorner = xg - r;
			int zgCorner = zg - r;
			for (int n = 0; n < r+r; n++)
			{
				TRY_GRID( xgCorner+n  , zg-r )
				TRY_GRID( xgCorner+n+1, zg+r )
				TRY_GRID( xg-r, zgCorner+n+1 )
				TRY_GRID( xg+r, zgCorner+n )
			}

			// if we found one in the _previous_ ring then we can stop now
			if (hadCandidate)
			{
				return true;
			}
			// NOTE: Actually, this is not entirely true, due to the
			// way that large triangular waypoints are added to the
			// grids. This should do until we go binary w/bsps however.
		}

		return false;
#undef TRY_GRID
	}

	ChunkWaypointSets::iterator it;
	for (it = wpSets_.begin(); it != wpSets_.end(); ++it)
	{
		if ((*it)->girth() != girth) continue;

		int found = (*it)->find( lpoint, ignoreHeight );
		if (found >= 0)
		{
			res.pSet_ = *it;
			res.waypoint_ = found;
			res.exactMatch_ = true;
			return true;
		}
	}

	// no exact match, so use the closest one then
	float bestDistanceSquared = FLT_MAX;
	for (it = wpSets_.begin(); it != wpSets_.end(); ++it)
	{
		if ((*it)->girth() != girth) continue;

		int found = (*it)->find( lpoint, bestDistanceSquared );
		if (found >= 0)
		{
			res.pSet_ = *it;
			res.waypoint_ = found;
			res.exactMatch_ = false;
		}
	}

	return !!res.pSet_;
}

/**
 *	This method finds a waypoint containing the point in the list of
 *	waypoints overlapping a given girth grid square.
 */
bool ChunkNavigator::GGList::find( const Vector3 & lpoint,
							Result & res, bool ignoreHeight )
{
	if (ignoreHeight)
	{
		const float INVALID_HEIGHT = 1000000.f;
		float bestHeight = INVALID_HEIGHT;
		Result r;

		for (iterator it = begin(); it != end(); ++it)
		{
			ChunkWaypoint & wp = (*it).pSet_->waypoint( (*it).waypoint_ );

			if (wp.containsProjection( lpoint ))
			{
				// Try to find a reasonable navmesh if there are more than
				// one navmeshes overlapped in the same x,z range.
				// We would like to pick a relatively close navmesh with
				// height range a bit lower than y coordinate of the given point.
				if (fabsf( wp.maxHeight_ - lpoint.y ) < fabsf( bestHeight - lpoint.y ))
				{
					bestHeight = wp.maxHeight_;
					r.pSet_ = (*it).pSet_;
					r.waypoint_ = (*it).waypoint_;
				}
			}
		}

		if (bestHeight != INVALID_HEIGHT)
		{
			r.exactMatch_ = true;
			res = r;

			return true;
		}
	}
	else
	{
		for (iterator it = begin(); it != end(); ++it)
		{
			ChunkWaypoint & wp = (*it).pSet_->waypoint( (*it).waypoint_ );

			if (wp.contains( lpoint ))
			{
				res.pSet_ = (*it).pSet_;
				res.waypoint_ = (*it).waypoint_;
				res.exactMatch_ = true;
				return true;
			}
		}
	}

	return false;
}

/**
 *	This method finds a waypoint closest to the point in the list of
 *	waypoints overlapping a given girth grid square.
 */
void ChunkNavigator::GGList::find( const Chunk* chunk, const Vector3 & lpoint,
	float & bestDistanceSquared, Result & res )
{
	for (iterator it = begin(); it != end(); ++it)
	{
		ChunkWaypoint & wp = (*it).pSet_->waypoint( (*it).waypoint_ );
		float distanceSquared = wp.distanceSquared( chunk, lpoint );
		if (bestDistanceSquared > distanceSquared)
		{
			bestDistanceSquared = distanceSquared;
			res.pSet_ = (*it).pSet_;
			res.waypoint_ = (*it).waypoint_;
		}
	}
	if (!!res.pSet_)
	{
		ChunkWaypoint & wp = res.pSet_->waypoint( res.waypoint_ );
		res.exactMatch_ = wp.contains( lpoint );
	}
}


/**
 *	Add the given waypoint set to our cache
 */
void ChunkNavigator::add( ChunkWaypointSet * pSet )
{
	wpSets_.push_back( pSet );

	// get out now if girth grids are disabled
	if (!s_useGirthGrids_) return;

	// only use grids on outside chunks
	if (!chunk_.isOutsideChunk()) return;

	// ensure a grid exists for this girth
	GirthGrids::iterator git;
	for (git = girthGrids_.begin(); git != girthGrids_.end(); ++git)
	{
		if ((*git).girth == pSet->girth()) break;
	}
	if (git == girthGrids_.end())
	{
		girthGrids_.push_back( GirthGrid() );
		git = girthGrids_.end()-1;
		(*git).girth = pSet->girth();
		(*git).grid = new GGList[GG_SIZE*GG_SIZE];
	}
	GGList * gg = (*git).grid;

	// add every waypoint to it
	for (int i = 0; i < pSet->waypointCount(); ++i)
	{
		ChunkWaypoint & wp = pSet->waypoint( i );

		float mx = 1000000.f, mz = 1000000.f, Mx = -1000000.f, Mz = -1000000.f;
		for (ChunkWaypoint::Edges::iterator eit = wp.edges_.begin();
			eit != wp.edges_.end();
			++eit)
		{
			Vector2 gf = (eit->start_ - ggOrigin_) * ggResolution_;
			if (mx > gf.x) mx = gf.x;
			if (mz > gf.y) mz = gf.y;
			if (Mx < gf.x) Mx = gf.x;
			if (Mz < gf.y) Mz = gf.y;
		}

		for (int xg = int(mx); xg <= int(Mx); ++xg)
			for (int zg = int(mz); zg <= int(Mz); ++zg)
		{
			if (uint(xg) < GG_SIZE && uint(zg) < GG_SIZE)
				gg[xg+zg*GG_SIZE].push_back( GGElement(pSet, i) );
		}
	}
}

/**
 *	Return true if our chunk doesn't have any navPolySets.
 */
bool ChunkNavigator::isEmpty()
{
	return wpSets_.size() == 0;
}

/**
 *	Return true if our chunk has any sets of the given girth
 */
bool ChunkNavigator::hasNavPolySet( float girth )
{
	ChunkWaypointSets::iterator it;
	bool hasGirth = false;
	for (it = wpSets_.begin(); it != wpSets_.end(); ++it)
	{
		if ((*it)->girth() != girth) continue;
		hasGirth = true;
		break;
	}
	return hasGirth;
}

/**
 *	Delete the given waypoint set from our cache
 */
void ChunkNavigator::del( ChunkWaypointSet * pSet )
{
	ChunkWaypointSets::iterator found = std::find(
		wpSets_.begin(), wpSets_.end(), pSet );
	MF_ASSERT( found != wpSets_.end() )
	wpSets_.erase( found );

	// get out now if girth grids are disabled
	if (!s_useGirthGrids_) return;

	// only use grids on outside chunks
	if (!chunk_.isOutsideChunk()) return;

	// find grid
	GirthGrids::iterator git;
	for (git = girthGrids_.begin(); git != girthGrids_.end(); ++git)
	{
		if ((*git).girth == pSet->girth()) break;
	}
	if (git != girthGrids_.end())
	{
		// remove all traces of this set from it
		GGList * gg = (*git).grid;
		for (uint i = 0; i < GG_SIZE*GG_SIZE; i++)
		{
			for (uint j = 0; j < gg[i].size(); ++j)
			{
				if (gg[i][j].pSet_ == pSet)
				{
					gg[i].erase( gg[i].begin() + j );
					--j;
				}
			}
		}
	}
}

// static initialisers
ChunkNavigator::Instance<ChunkNavigator> ChunkNavigator::instance;
bool ChunkNavigator::s_useGirthGrids_ = true;


// chunk_waypoint_set.cpp
