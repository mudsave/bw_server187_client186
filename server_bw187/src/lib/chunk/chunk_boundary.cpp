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

#include "chunk_boundary.hpp"


#ifndef MF_SERVER
#include "moo/render_context.hpp"
#include "romp/geometrics.hpp"
#include "chunk_manager.hpp"
#endif

#include "cstdmf/memory_counter.hpp"
#include "math/portal2d.hpp"
#include "chunk.hpp"
#include "chunk_space.hpp"

#include "physics2/worldpoly.hpp"	// a bit unfortunate...

#if UMBRA_ENABLE
#include <umbraModel.hpp>
#include <umbraObject.hpp>
#include "chunk_umbra.hpp"
#endif

#include "cstdmf/config.hpp"
#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT2( "Chunk", 0 );

memoryCounterDeclare( chunk );

// Static initialiser.
bool ChunkBoundary::Portal::drawPortals_ = false;


// ----------------------------------------------------------------------------
// Section: ChunkBoundary
// ----------------------------------------------------------------------------


/**
 *	Constructor and loader
 */
ChunkBoundary::ChunkBoundary( DataSectionPtr pSection,
							 ChunkDirMapping * pMapping,
							 const std::string & ownerChunkName )
{
	memoryCounterAdd( chunk );
	memoryClaim( this );

	// make sure something's there
	if (!pSection)
	{
		plane_ = PlaneEq( Vector3(0,0,0), 0 );
		return;
	}

	// read the plane
	Vector3 normal = pSection->readVector3( "normal" );
	float d = pSection->readFloat( "d" );
	float normalLen = normal.length();
	normal /= normalLen;
	d /= normalLen;
	plane_ = PlaneEq( normal, d );

	// prepare for some error checking
	bool someInternal = false;
	bool someExternal = false;

	// read any portals
	DataSectionIterator end = pSection->end();
	for (DataSectionIterator it = pSection->begin(); it != end; it++)
	{
		if ((*it)->sectionName() != "portal") continue;

		Portal * newPortal = new Portal( *it, plane_,pMapping,ownerChunkName );
		memoryClaim( newPortal );

		if (newPortal->isHeaven() || newPortal->isEarth())
			boundPortals_.push_back( newPortal );
		else
			unboundPortals_.push_back( newPortal );

		if (newPortal->internal)
			someInternal = true;
		else
			someExternal = true;
	}

	// make sure no-one stuffed up
	MF_ASSERT(!(someInternal && someExternal))

	memoryClaim( boundPortals_ );
	memoryClaim( unboundPortals_ );
}

/**
 *	Destructor
 */
ChunkBoundary::~ChunkBoundary()
{
	memoryCounterSub( chunk );

	for (Portals::iterator pit = boundPortals_.begin();
		pit != boundPortals_.end();
		pit++)
	{
		memoryClaim( *pit );
		delete *pit;
	}
	memoryClaim( boundPortals_ );

	for (Portals::iterator pit = unboundPortals_.begin();
		pit != unboundPortals_.end();
		pit++)
	{
		memoryClaim( *pit );
		delete *pit;
	}
	memoryClaim( unboundPortals_ );

	memoryClaim( this );
}

/**
 *	Move the given portal to the bound list
 */
void ChunkBoundary::bindPortal( uint32 unboundIndex )
{
	bool willGrow = boundPortals_.size() == boundPortals_.capacity();
	if (willGrow)
	{
		memoryCounterSub( chunk );
		memoryClaim( boundPortals_ );
	}
	Portal * pPortal = unboundPortals_[unboundIndex];
	boundPortals_.push_back( pPortal );
	unboundPortals_.erase( unboundPortals_.begin() + unboundIndex );
	if (willGrow)
	{
		memoryCounterAdd( chunk );
		memoryClaim( boundPortals_ );
	}
}

/**
 *	Move the given portal to the unbound list
 */
void ChunkBoundary::loosePortal( uint32 boundIndex )
{
	bool willGrow = unboundPortals_.size() == unboundPortals_.capacity();
	if (willGrow)
	{
		memoryCounterSub( chunk );
		memoryClaim( unboundPortals_ );
	}
	Portal * pPortal = boundPortals_[boundIndex];
	unboundPortals_.push_back( pPortal );
	boundPortals_.erase( boundPortals_.begin() + boundIndex );
	if (willGrow)
	{
		memoryCounterAdd( chunk );
		memoryClaim( unboundPortals_ );
	}
}

/**
 *	This method adds a new invasive portal to the list of unbound portals
 *	in this boundary. Used only by editors.
 */
void ChunkBoundary::addInvasivePortal( Portal * pPortal )
{
	unboundPortals_.push_back( pPortal );
}

/**
 *	This method splits the identified invasive portal if necessary,
 *	i.e. if it overlaps chunks other than the one it points to.
 *	Also used only by editors. Note that this function is called on
 *	the inside chunk that already has an invasive portal, wheres the
 *	@see addInvasivePortal function is called on the invaded outside chunk.
 */
void ChunkBoundary::splitInvasivePortal( Chunk * pChunk, uint i )
{
	return;
	Portal & p = *unboundPortals_[i];
	Chunk * pDest = p.pChunk;

	// get our plane in world coordinates
	const PlaneEq & srcLocalPlane = plane_;
	Vector3 ndtr = pChunk->transform().applyPoint(
		srcLocalPlane.normal() * srcLocalPlane.d() );
	Vector3 ntr = pChunk->transform().applyVector( srcLocalPlane.normal() );
	PlaneEq srcWorldPlane( ntr, ntr.dotProduct( ndtr ) );

	// make matrices to convert from portal space to world space and back again
	Matrix portalToWorld = Matrix::identity;
	portalToWorld[0] = p.uAxis;
	portalToWorld[1] = p.vAxis;
	portalToWorld[2] = p.plane.normal();
	portalToWorld[3] = p.origin;
	portalToWorld.postMultiply( pChunk->transform() );
	Matrix worldToPortal;
	worldToPortal.invert( portalToWorld );

	WorldPolygon srcPortalPoly;
	bool srcPortalPolyValid = false;

	// slice off a new portal for every boundary that intersects us
	for (ChunkBoundaries::iterator bit = pDest->bounds().begin();
		bit != pDest->bounds().end();
		++bit)
	{
		// get other plane in world coordinates
		const PlaneEq & dstLocalPlane = (*bit)->plane_;
		ndtr = pDest->transform().applyPoint(
			dstLocalPlane.normal() * dstLocalPlane.d() );
		ntr = pDest->transform().applyVector( dstLocalPlane.normal() );
		PlaneEq dstWorldPlane( ntr, ntr.dotProduct( ndtr ) );

		// if parallel or almost so then ignore it
		if (fabsf( dstWorldPlane.normal().dotProduct(
			srcWorldPlane.normal() ) ) > 0.99f)
				continue;

		Portal * pNewPortal = NULL;

		// see which points lie outside this boundary, and add them
		// to newPortal (under pChunk's transform) if we found any...
		// ... remove them from 'p' while we're at it.
		// binary/linear splits are fine - we can split either side again
		// if we have to (either later in this loop, or when this function is
		// called for pNewPortal)

		// we're going to use WorldPolygon to do our dirty work for us,
		// so first turn our current portal into a world coords if necessary
		if (!srcPortalPolyValid)
		{
			srcPortalPoly.clear();
			Vector3 worldPoint;
			for (uint j = 0; j < p.points.size(); ++j)
			{
				//pChunk->transform().applyPoint(
				//	worldPoint, p.origin +
				//		p.uAxis * p.points[j][0] + p.vAxis * (p.points[j][1] );
				Vector3 ptAug( p.points[j][0], p.points[j][1], 0.f );
				portalToWorld.applyPoint( worldPoint, ptAug );
				srcPortalPoly.push_back( worldPoint );
			}
			srcPortalPolyValid = true;
		}

		// ask the WorldPolygon to cleave itself in twain
		WorldPolygon insidePoly, outsidePoly;
		srcPortalPoly.split( dstWorldPlane, insidePoly, outsidePoly );

		// create the new portal and update the old
		if (!outsidePoly.empty())
		{
			MF_ASSERT( !insidePoly.empty() );

			Vector3 ptAug;
			Vector2 ptAvg;

			// new portal
			pNewPortal = new Portal( p );	// use copy constructor
			pNewPortal->points.resize( outsidePoly.size() );
			ptAvg.set( 0.f, 0.f );
			for (uint j = 0; j < outsidePoly.size(); ++j)
			{
				worldToPortal.applyPoint( ptAug, outsidePoly[j] );
				pNewPortal->points[j] = Vector2( ptAug[0], ptAug[1] );
				ptAvg += pNewPortal->points[j];
			}
			ptAvg /= float(pNewPortal->points.size());
			pNewPortal->lcentre = pNewPortal->uAxis * ptAvg.x +
				pNewPortal->vAxis * ptAvg.y + pNewPortal->origin;
			pChunk->transform().applyPoint(
				pNewPortal->centre, pNewPortal->lcentre );

			// old portal
			p.points.resize( insidePoly.size() );
			ptAvg.set( 0.f, 0.f );
			for (uint j = 0; j < insidePoly.size(); ++j)
			{
				worldToPortal.applyPoint( ptAug, insidePoly[j] );
				p.points[j] = Vector2( ptAug[0], ptAug[1] );
				ptAvg += p.points[j];
			}
			ptAvg /= float(p.points.size());
			p.lcentre = p.uAxis * ptAvg.x + p.vAxis * ptAvg.y + p.origin;
			pChunk->transform().applyPoint( p.centre, p.lcentre );
			srcPortalPolyValid = false;

			// don't really like these double-transformations, hmm
			// maybe better to bring plane into portal space and
			// pretend it's world space... still would need to
			// augment p.points to Vector3s .. 'tho it'd be trivial now...
		}

		// if we made a portal then add it
		if (pNewPortal != NULL)
		{
			DEBUG_MSG( "ChunkBoundary::splitInvasivePortal: "
				"Split portal in %s since it extends outside first hit %s\n",
				pChunk->identifier().c_str(), pDest->identifier().c_str() );

			pNewPortal->pChunk = (Chunk*)Portal::INVASIVE;
			unboundPortals_.push_back( pNewPortal );
		}
	}
}


// ----------------------------------------------------------------------------
// Section: ChunkBoundary::Portal
// ----------------------------------------------------------------------------

/**
 *	Constructor and loader
 */
ChunkBoundary::Portal::Portal( DataSectionPtr pSection,
							  const PlaneEq & iplane,
							  ChunkDirMapping * pMapping,
							  const std::string & ownerChunkName ) :
	internal( false ),
	permissive( true ),
	pChunk( NULL ),
	plane( iplane )
#if UMBRA_ENABLE
	,
	pUmbraPortal_( NULL )
#endif
{
	// make sure there's something there
	if (!pSection) return;

	// set the label if it's got one
	label = pSection->asString();

	// read in our flags
	internal = pSection->readBool( "internal", internal );
	permissive = pSection->readBool( "permissive", permissive );

	// find out what to set pChunk to
	std::string	chunkName = pSection->readString( "chunk" );
	if (chunkName == "")
	{
		pChunk = NULL;
	}
	else if (chunkName == "heaven")
	{
		pChunk = (Chunk*)HEAVEN;
	}
	else if (chunkName == "earth")
	{
		pChunk = (Chunk*)EARTH;
	}
	else if (chunkName == "invasive")
	{
		pChunk = (Chunk*)INVASIVE;
	}
	else if (chunkName == "extern")
	{
		pChunk = (Chunk*)EXTERN;
	}
	else
	{
		pChunk = new Chunk( chunkName, pMapping );
	}


	// read in the polygon points
	Vector2	avg(0,0);

	std::vector<Vector3>	v3points;
	pSection->readVector3s( "point", v3points );
	for (uint i=0; i<v3points.size(); i++)
	{
		Vector2 next( v3points[i].x, v3points[i].y );

		avg += next;

		points.push_back( next );
		// TODO: These probably have to be in some sort of order...
	}
	memoryCounterAdd( chunk );
	memoryClaim( points );

	// read in the axes
	uAxis = pSection->readVector3( "uAxis" );
	vAxis = plane.normal().crossProduct( uAxis );
	origin = plane.normal() * plane.d() / plane.normal().lengthSquared();

#ifdef EDITOR_ENABLED
	const float EPSILON = 0.5;
	if( !chunkName.empty() && *chunkName.rbegin() == 'o' &&
		!ownerChunkName.empty() && *ownerChunkName.rbegin() == 'o' &&
		points.size() == 4 )
	{
		if( uAxis[ 1 ] >= EPSILON || uAxis[ 1 ] <= -EPSILON )
		{
			for( int i = 0; i < 4; ++i )
			{
				if( points[ i ][ 0 ] > EPSILON )
					points[ i ][ 0 ] = 2000000;
				else if( points[ i ][ 0 ] < -EPSILON )
					points[ i ][ 0 ] = -2000000;
			}
		}
		else
		{
			for( int i = 0; i < 4; ++i )
			{
				if( points[ i ][ 1 ] > EPSILON )
					points[ i ][ 1 ] = 2000000;
				else if( points[ i ][ 1 ] < -EPSILON )
					points[ i ][ 1 ] = -2000000;
			}
		}
	}
#endif //EDITOR_ENABLED

	// figure out the centre of our portal (local coords)
	avg /= float(points.size());
	lcentre = uAxis * avg.x + vAxis * avg.y + origin;
	centre = lcentre;

	PlaneEq testPlane(
		points[0][0] * uAxis + points[0][1] * vAxis + origin,
		points[1][0] * uAxis + points[1][1] * vAxis + origin,
		points[2][0] * uAxis + points[2][1] * vAxis + origin );
	Vector3 n1 = plane.normal();
	Vector3 n2 = testPlane.normal();
	n1.normalise();	n2.normalise();
	if ((n1 + n2).length() < 1.f)	// should be 2 if equal
	{
		std::reverse( points.begin(), points.end() );
	}
}


/**
 *	Destructor
 */
ChunkBoundary::Portal::~Portal()
{
#if UMBRA_ENABLE
	releaseUmbraPortal();
#endif

	memoryCounterSub( chunk );
	memoryClaim( points );
}

/**
 *	This method saves a description of this portal into the given data section.
 */
void ChunkBoundary::Portal::save( DataSectionPtr pSection,
	ChunkDirMapping * pOwnMapping ) const
{
	if (!pSection) return;

	DataSectionPtr pPS = pSection->newSection( "portal" );
        if (!label.empty()) pPS->setString( label );
	if (internal) pPS->writeBool( "internal", true );
	if (pChunk != NULL)
	{
		pPS->writeString( "chunk",
			isHeaven() ? "heaven" :
			isEarth() ? "earth" :
			isInvasive() ? "invasive" :
			isExtern() ? "extern" :
			pChunk->mapping() != pOwnMapping ? "extern" :
			pChunk->identifier() );
	}
	pPS->writeVector3( "uAxis", uAxis );
	for (uint i = 0; i < points.size(); i++)
	{
		pPS->newSection( "point" )->setVector3(
			Vector3( points[i][0], points[i][1], 0 ) );
	}

}

/**
 *	This method attempts to resolve an extern portal to find the chunk that
 *	it should be connected to - regardless of what mapping it is in.
 *	Note: if an appropriate chunk is found, it is returned holding a
 *	reference to its ChunkDirMapping.
 */
bool ChunkBoundary::Portal::resolveExtern( Chunk * pOwnChunk )
{
	Vector3 conPt = pOwnChunk->transform().applyPoint(
		lcentre + plane.normal() * -0.1f );
	Chunk * pExternChunk = pOwnChunk->space()->guessChunk( conPt, true );
	if (pExternChunk != NULL)
	{
		if (pExternChunk->mapping() != pOwnChunk->mapping())
		{
			pChunk = pExternChunk;
			return true;
		}
		else
		{
			// we don't want it because it's not extern
			// (although technically it should be allowed...)
			delete pExternChunk;
			pOwnChunk->mapping()->decRef();
		}
	}

	return false;
}


// -----------------------------------------------------------------------------
// Section: Client only
// -----------------------------------------------------------------------------

#ifndef MF_SERVER

/**
 *	This class stores up ready-made portals
 */
class Portal2DStore
{
public:
	static Portal2DRef grab()
	{
		if (s_portalStore_.empty())
		{
			Portal2D * pPortal = new Portal2D();
			for (int i = 0; i < 8; i++) pPortal->addPoint( Vector2::zero() );
			s_portalStore_.push_back( pPortal );
		}

		Portal2DRef pref;
		pref.pVal_ = s_portalStore_.back();
		s_portalStore_.pop_back();
		Portal2DStore::grab( pref.pVal_ );
		pref->erasePoints();
		return pref;
	}

private:
	static void grab( Portal2D * pPortal )
	{
		pPortal->refs( pPortal->refs() + 1 );
	}

	static void give( Portal2D * pPortal )
	{
		if (pPortal == NULL) return;
		pPortal->refs( pPortal->refs() - 1 );
		if (pPortal->refs() == 0)
		{
			s_portalStore_.push_back( pPortal );
		}
	}

	static VectorNoDestructor<Portal2D*>		s_portalStore_;

	friend class Portal2DRef;
};

VectorNoDestructor<Portal2D*> Portal2DStore::s_portalStore_;

Portal2DRef::Portal2DRef( bool valid ) :
	pVal_( valid ? NULL : (Portal2D*)-1 )
{
}

Portal2DRef::Portal2DRef( const Portal2DRef & other ) :
	pVal_( other.pVal_ )
{
	if (uint(pVal_)+1 > 1)
		Portal2DStore::grab( pVal_ );
}

Portal2DRef & Portal2DRef::operator =( const Portal2DRef & other )
{
	if (uint(pVal_)+1 > 1)
		Portal2DStore::give( pVal_ );
	pVal_ = other.pVal_;
	if (uint(pVal_)+1 > 1)
		Portal2DStore::grab( pVal_ );
	return *this;
}

Portal2DRef::~Portal2DRef()
{
	if (uint(pVal_)+1 > 1)
		Portal2DStore::give( pVal_ );
}

// E3'04 option to turn off clipping outside to portals
// ... needed until it works better! (portal aggregation)
static struct ClipOutsideToPortal
{
	ClipOutsideToPortal() : b_( false )
	{ 
		MF_WATCH( "Render/clipOutsideToPortal", b_, Watcher::WT_READ_WRITE,
			"Clip outdoor chunks to indoor portals" ); 
	}
	operator bool() { return b_; }
	bool b_;
} s_clipOutsideToPortal;


BoundingBox ChunkBoundary::Portal::bbFrustum_;

void ChunkBoundary::Portal::updateFrustumBB()
{
	static std::vector<Vector4> clipSpaceFrustum;
	if( !clipSpaceFrustum.size() )
	{
		clipSpaceFrustum.push_back( Vector4( -1.f, -1.f, 0.f, 1.f ) );
		clipSpaceFrustum.push_back( Vector4( +1.f, -1.f, 0.f, 1.f ) );
		clipSpaceFrustum.push_back( Vector4( -1.f, +1.f, 0.f, 1.f ) );
		clipSpaceFrustum.push_back( Vector4( +1.f, +1.f, 0.f, 1.f ) );
		clipSpaceFrustum.push_back( Vector4( -1.f, -1.f, 1.f, 1.f ) );
		clipSpaceFrustum.push_back( Vector4( +1.f, -1.f, 1.f, 1.f ) );
		clipSpaceFrustum.push_back( Vector4( -1.f, +1.f, 1.f, 1.f ) );
		clipSpaceFrustum.push_back( Vector4( +1.f, +1.f, 1.f, 1.f ) );
	}
	std::vector<Vector4> worldSpaceFrustum( clipSpaceFrustum.size() );

	Matrix m;
	m.invert( Moo::rc().viewProjection() );
	for( std::vector<Vector4>::size_type i = 0; i < clipSpaceFrustum.size(); ++i )
	{
		m.applyPoint( worldSpaceFrustum[i], clipSpaceFrustum[i] );
		if( worldSpaceFrustum[i].w < 0.0000001f && worldSpaceFrustum[i].w > -0.0000001f )
			worldSpaceFrustum[i].x = 0.0000001f;
		worldSpaceFrustum[i] = worldSpaceFrustum[i] / worldSpaceFrustum[i].w;
	}

	bbFrustum_ = BoundingBox( Vector3( worldSpaceFrustum[0].x, worldSpaceFrustum[0].y, worldSpaceFrustum[0].z ),
		 Vector3( worldSpaceFrustum[0].x, worldSpaceFrustum[0].y, worldSpaceFrustum[0].z ) );
	for( std::vector<Vector4>::size_type i = 1; i < worldSpaceFrustum.size(); ++i )
	{
		bbFrustum_.addBounds( Vector3( worldSpaceFrustum[i].x, worldSpaceFrustum[i].y, worldSpaceFrustum[i].z ) );
	}
}

/**
 *	This method checks if the point is inside the portal
 *	@param point the point to check, assumed to already be on the portal plane
 *	@return true if the point is inside the portal
 */
bool ChunkBoundary::Portal::inside( const Vector3& point ) const
{
	Vector3 rel = point - origin;
	Vector2 p( rel.dotProduct( uAxis ), rel.dotProduct( vAxis ) );

	for (uint32 i = 0; i < points.size(); i++)
	{
		const Vector2& p1 = points[i];
		const Vector2& p2 = points[(i+1) % points.size()];

		Vector2 diff(p1.y - p2.y, p2.x - p1.x);

		if (diff.dotProduct( p - p1 ) < 0.f)
		{
			return false;
		}
	}
	return true;
}

/**
 *	This method calculates the object-space point of our portal points.
 *	Our portal points are defined along a plane and thus are not necessarily
 *	useful for those needing to project them into the world.
 *	@param	i		index of the point to transform
 *	@param	ret		return vector, is set with a true object space position.
 */
void ChunkBoundary::Portal::objectSpacePoint( int i, Vector3& ret )
{
	ret = uAxis * points[i][0] + vAxis * points[i][1] + origin;
}

/**
 *	This method calculates the object-space point of our portal points.
 *	Our portal points are defined along a plane and thus are not necessarily
 *	useful for those needing to project them into the world.
 *	@param	i		index of the point to transform
 *	@param	ret		return vector, is set with a true object space position.
 */
void ChunkBoundary::Portal::objectSpacePoint( int i, Vector4& ret )
{
	ret = Vector4(uAxis * points[i][0] + vAxis * points[i][1] + origin,1);
}

/**
 *	This method calculates the object-space point of our portal points.
 *	Our portal points are defined along a plane and thus are not necessarily
 *	useful for those needing to project them into the world.
 *	@param	i		index of the point to transform
 *	@return			return vector, is set with a true object space position.
 */
Vector3 ChunkBoundary::Portal::objectSpacePoint( int i )
{
	return uAxis * points[i][0] + vAxis * points[i][1] + origin;
}

/**
 *	This method calculates which edges on a portal a point is outside
 *	useful for calculating if a number of points are inside a portal.
 *	@param point the point to check
 *	@return the outcode mask for the point each bit represents one edge
 *	if the bit is set, the point is outside that edge
 */
uint32 ChunkBoundary::Portal::outcode( const Vector3& point ) const
{
	uint32 res = 0;
	Vector3 rel = point - origin;
	Vector2 p( rel.dotProduct( uAxis ), rel.dotProduct( vAxis ) );

	for (uint32 i = 0; i < points.size(); i++)
	{
		const Vector2& p1 = points[i];
		const Vector2& p2 = points[(i+1) % points.size()];

		Vector2 diff(p1.y - p2.y, p2.x - p1.x);

		if (diff.dotProduct( p - p1 ) < 0.f)
		{
			res |= 1 << i;
		}
	}
	return res;
}

#if UMBRA_ENABLE
/**
 *	This method creates a portal pointing from the current chunk to a target chunk
 *	@param pOwner the owner of this portal
 */
void ChunkBoundary::Portal::createUmbraPortal(Chunk* pOwner)
{
	// Make sure we release the current umbra portal
	releaseUmbraPortal();

	// If the target chunk and the owner exist in the same cell
	// there is no point to this portal
	if (pOwner->getUmbraCell() == pChunk->getUmbraCell())
		return;

	// Create the portal model
	Umbra::Model* model = createUmbraPortalModel();
	model->autoRelease();

	// Create the portal and set up it's data
	pUmbraPortal_ = Umbra::PhysicalPortal::create( model, pChunk->getUmbraCell() );
	pUmbraPortal_->set(Umbra::Object::INFORM_PORTAL_ENTER, true);
	pUmbraPortal_->set(Umbra::Object::INFORM_PORTAL_EXIT, true);
	UmbraPortal* umbraPortal = new UmbraPortal(pOwner);
	pUmbraPortal_->setUserPointer( (void*)umbraPortal );
	pUmbraPortal_->setObjectToCellMatrix( (Umbra::Matrix4x4&)pOwner->transform() );
	pUmbraPortal_->setCell( pOwner->getUmbraCell() );
}

/**
 *	This method releases the umbra portal and associated objects
 */
void ChunkBoundary::Portal::releaseUmbraPortal()
{
	if (pUmbraPortal_)
	{
		UmbraPortal* up = (UmbraPortal*)pUmbraPortal_->getUserPointer();
		delete up;
		pUmbraPortal_->release();
		pUmbraPortal_ = NULL;
	}
}

/**
 *	This method creates the umbra portal model for a portal
 *	@return the umbra portal model for this portal
 */
Umbra::Model* ChunkBoundary::Portal::createUmbraPortalModel()
{
	// Collect the vertices for the portal
	uint32 nVertices = points.size();

	std::vector<Vector3> vertices;
	vertices.reserve(nVertices);

	for( uint32 i = 0; i < nVertices; i++ )
	{
		vertices.push_back(uAxis * points[i][0] + vAxis * points[i][1] + origin);
	}

	// Set up the triangles for the portal model
	int nTriangles = nVertices - 2;

	std::vector<uint32> triangles;
	triangles.reserve(nTriangles*3);

	MF_ASSERT(nTriangles > 0);

	for (uint32 c = 2; c < nVertices; c++)
	{
		triangles.push_back( 0 );
		triangles.push_back( c-1 );
		triangles.push_back( c );
	}

	// Create the umbra model and set it up to be backface cullable so that we can only see
	// through one end of the portal
	Umbra::Model* model = Umbra::MeshModel::create((Umbra::Vector3*)&vertices.front(), (Umbra::Vector3i*)&triangles.front(), nVertices, nTriangles);
	model->set(Umbra::Model::BACKFACE_CULLABLE, true);
	return model;
}
#endif

/**
 *	This method traverses this portal and draws the chunk it's connected to, if
 *	the portal is visible. Assumes that 'hasChunk' would return true.
 *	It returns the portal to be used to traverse any portals inside that chunk.
 *	An invlaid Portal2DRef is returned if the chunk was not drawn.
 */
//#define CHUNK_TRAVERSE_OPTIMIZATION_OFF
#ifdef CHUNK_TRAVERSE_OPTIMIZATION_OFF
Portal2DRef ChunkBoundary::Portal::traverse(
	const Matrix & transform,
	const Matrix & transformInverse,
	Portal2DRef pClipPortal ) const
{
	// find the matrix to transform from local space to clip space
	Matrix		objectToClip;
	objectToClip.multiply( transform, Moo::rc().viewProjection() );

	// outside stuff ... see if this is a big portal between outside chunks
	bool outside = (points.size() == 4 &&
		(points[0] - points[2]).lengthSquared() > 100.f*100.f);

	// find the matrix to transform from camera space to local space
	Matrix		cameraToObject;
	cameraToObject.multiply( Moo::rc().invView(), transformInverse );

	// find the camera position on the near plane in local space. we use
	// this position to determine whether or not this portal is visible.
	Vector3 cameraOnNearPlane = cameraToObject.applyPoint(
		Vector3( 0, 0, Moo::rc().camera().nearPlane() ) );

	// our plane is in object space, so now we can find the distance
	// of the camera from it
	if (plane.distanceTo( cameraOnNearPlane ) <= (outside ? -100.f : -2.0f) )
	{									// -100.f : -0.1f
		ChunkManager::drawTreeBranch( pChunk, " - CAMERA" );
		ChunkManager::drawTreeReturn();
		return Portal2DRef( false );
	}
	// if the polygon crosses the near plane inside the view volume,
	// then we really want to intersect it with the near plane...

	// yay, we can see it. or could if there were no other portals

	// set up two outcode variables. new outcodes are accumulated into
	// outProduct by 'and' operations, and into outSum by 'or' operations.
	Outcode outProduct = OUTCODE_MASK;
	Outcode outSum = 0;	// Remember circuit analysis: SoP and PoS?

	Portal2DRef ourClipPortal = Portal2DStore::grab();

	Vector4 clipPoint;

	// check all the points in our portal - if any lie
	// inside the view volume, add them to ourClipPortal
	for( uint32 i = 0; i < points.size(); i++ )
	{
		// project the point into clip space
#ifndef EDITOR_ENABLED
		objectToClip.applyPoint( clipPoint, Vector4(
			uAxis * points[i][0] + vAxis * points[i][1] + origin, 1 ) );
#else
		Vector4 objectPoint(
			uAxis * points[i][0] + vAxis * points[i][1] + origin, 1 );
		// raise up outside portals to at least the height of the camera
		if (outside && objectPoint.y > 0.f && 
			objectPoint.y < cameraToObject[3].y)
				objectPoint.y = cameraToObject[3].y + 5.f;
		objectToClip.applyPoint( clipPoint, objectPoint );
#endif

		// see where it lies
		Outcode ocPoint = clipPoint.calculateOutcode();

		// if it's not too close add it to our clip portal
		if (( ocPoint & OUTCODE_NEAR ) == 0)
		{
			float oow = 1.f/clipPoint.w;
			ourClipPortal->addPoint(
				Vector2( clipPoint.x*oow, clipPoint.y*oow ) );
		}

		outSum |= ocPoint;

		// it's not near as long as w isn't too negative
		//  (why I don't know - copied from rhino's portal.cpp)
		if (clipPoint.w >= -1)
			ocPoint &= ~OUTCODE_NEAR;

		outProduct &= ocPoint;
	}

	// if all the points have at least one outcode bit in common,
	// then the whole portal must be out of the volume, so ignore it.
	if (outProduct != 0)
	{
		ChunkManager::drawTreeBranch( pChunk, " - BBOX" );
		ChunkManager::drawTreeReturn();
		return Portal2DRef( false );
	}

	// if we're going to the outside, do something different here
	if (outside)
	{
		// if we're looking through a smaller portal, make sure there's
		// something worth drawing.
		// E3'04: don't clip the outside to just one inside portal...
		// far too error prone in somewhere with multiple heavenly portals
		// like say the city - we really need portal aggregation for this.
		if (&*pClipPortal != NULL && !(outSum & OUTCODE_NEAR) &&
			s_clipOutsideToPortal)
		{
			Portal2DRef combinedP2D = Portal2DStore::grab();
			if (!combinedP2D->combine( &*pClipPortal, &*ourClipPortal ))
			{
				ChunkManager::drawTreeBranch( pChunk, " - COMBINED" );
				ChunkManager::drawTreeReturn();
				return Portal2DRef( false );
			}
		}

		// ok, either we're outside looking outside, or we're
		// inside looking outside and we can see this portal.
		// either way, we want to draw the chunk ... so draw it!
		ChunkManager::drawTreeBranch( pChunk, " + outside" );
		//pChunk->draw( pClipPortal );
		pChunk->drawBeg();
		return pClipPortal;
	}
	else
	{
		// ok, we're not going to the outside.

		// so, are any of our clip points near?
		if (outSum & OUTCODE_NEAR)
		{
			// ok, at least one was before the volume, so don't attempt to
			// do any fancy polygon intersection and just draw it with
			// the same Portal2D that we got given.
			ChunkManager::drawTreeBranch( pChunk, " + close" );
			//pChunk->draw( pClipPortal );
			pChunk->drawBeg();
			return pClipPortal;
		}
		else
		{
			// ok, let's combine them Portal2D's
			Portal2DRef combinedP2D = ourClipPortal;
			/*
			if (&*pClipPortal == NULL)
			{
				static Portal2D defP2D;
				if (defP2D.points().empty()) defP2D.setDefaults();

				combinedP2D->combine( &defP2D, &*ourClipPortal );

				// TODO: Why don't we just use ourClipPortal
				// straight off here? Instead of combining it
				// with the default one?
			}
			*/
			if (&*pClipPortal != NULL)
			{
				combinedP2D = Portal2DStore::grab();
				if (!combinedP2D->combine( &*pClipPortal, &*ourClipPortal ))
				{
					ChunkManager::drawTreeBranch( pChunk, " - COMBINED" );
					ChunkManager::drawTreeReturn();
					return Portal2DRef( false );
				}
			}

			ChunkManager::drawTreeBranch( pChunk, " + combined" );
			//pChunk->draw( &combinedP2D );
			pChunk->drawBeg();

			if (drawPortals_)
			{
#if ENABLE_DRAW_PORTALS
					Geometrics::drawLinesInClip(
						(Vector2*)&*combinedP2D->points().begin(),
						combinedP2D->points().size() );
#endif
			}
			return combinedP2D;
		}
	}
}
#else//CHUNK_TRAVERSE_OPTIMIZATION_OFF
Portal2DRef ChunkBoundary::Portal::traverse(
	const Matrix & transform,
	const Matrix & transformInverse,
	Portal2DRef pClipPortal ) const
{
	float distance = pChunk->boundingBox().distance( Moo::rc().invView()[3] );
	if( distance < 0.f )
		distance = -distance;
	if( distance > Moo::rc().camera().farPlane() +
		( pChunk->boundingBox().minBounds() - pChunk->boundingBox().maxBounds() ).length() / 2 )
	{
		ChunkManager::drawTreeBranch( pChunk, " - PORTAL TOO FAR" );
		ChunkManager::drawTreeReturn();
		return Portal2DRef( false );
	}

	// find the matrix to transform from local space to clip space
	Matrix		objectToClip;
	objectToClip.multiply( transform, Moo::rc().viewProjection() );

	// outside stuff ... see if this is a big portal between outside chunks
	bool outside = (points.size() == 4 &&
		(points[0] - points[2]).lengthSquared() > 100.f*100.f);

	// find the matrix to transform from camera space to local space
	Matrix		cameraToObject;
	cameraToObject.multiply( Moo::rc().invView(), transformInverse );

	// find the camera position on the near plane in local space. we use
	// this position to determine whether or not this portal is visible.
	Vector3 cameraOnNearPlane = cameraToObject.applyPoint(
		Vector3( 0, 0, Moo::rc().camera().nearPlane() ) );

	// our plane is in object space, so now we can find the distance
	// of the camera from it
	if (plane.distanceTo( cameraOnNearPlane ) <= (outside ? -100.f : -2.0f) )
	{									// -100.f : -0.1f
		ChunkManager::drawTreeBranch( pChunk, " - CAMERA" );
		ChunkManager::drawTreeReturn();
		return Portal2DRef( false );
	}
	// if the polygon crosses the near plane inside the view volume,
	// then we really want to intersect it with the near plane...

	// yay, we can see it. or could if there were no other portals

	// set up two outcode variables. new outcodes are accumulated into
	// outProduct by 'and' operations, and into outSum by 'or' operations.
	Outcode outProduct = OUTCODE_MASK;
	Outcode outSum = 0;	// Remember circuit analysis: SoP and PoS?

	Portal2DRef ourClipPortal = Portal2DStore::grab();

	Vector4 clipPoint;

	BoundingBox bbPortal;

	// check all the points in our portal - if any lie
	// inside the view volume, add them to ourClipPortal
	for( uint32 i = 0; i < points.size(); i++ )
	{
		Vector4 worldPoint;
		// project the point into clip space

		Vector4 objectPoint( uAxis * points[i][0] + vAxis * points[i][1] + origin, 1 );		
		// raise up outside portals to at least the height of the camera
		if (outside && objectPoint.y > 0.f && 
			objectPoint.y < 1000000.f)
				objectPoint.y = 1000000.f + 5.f;
		objectToClip.applyPoint( clipPoint, objectPoint );
		transform.applyPoint( worldPoint, objectPoint );

		worldPoint = worldPoint / worldPoint.w;
		Vector3 worldPoint3( worldPoint.x, worldPoint.y, worldPoint.z );
		if( i == 0 )
			bbPortal = BoundingBox( worldPoint3, worldPoint3 );
		else
			bbPortal.addBounds( worldPoint3 );

		// see where it lies
		Outcode ocPoint = clipPoint.calculateOutcode();

		// if it's not too close add it to our clip portal
		if (( ocPoint & OUTCODE_NEAR ) == 0)
		{
			float oow = 1.f/clipPoint.w;
			ourClipPortal->addPoint(
				Vector2( clipPoint.x*oow, clipPoint.y*oow ) );
		}

		outSum |= ocPoint;

		// it's not near as long as w isn't too negative
		//  (why I don't know - copied from rhino's portal.cpp)
		if (clipPoint.w >= -1)
			ocPoint &= ~OUTCODE_NEAR;

		outProduct &= ocPoint;
	}

	// if all the points have at least one outcode bit in common,
	// then the whole portal must be out of the volume, so ignore it.
	if (outProduct != 0)
	{
		ChunkManager::drawTreeBranch( pChunk, " - BBOX" );
		ChunkManager::drawTreeReturn();
		return Portal2DRef( false );
	}
	pChunk->boundingBox().calculateOutcode( Moo::rc().viewProjection() );
	if( pChunk->boundingBox().combinedOutcode() != 0 )
	{
		ChunkManager::drawTreeBranch( pChunk, " - BBOX" );
		ChunkManager::drawTreeReturn();
		return pClipPortal;
	}
	if( !pChunk->boundingBox().intersects( bbFrustum_ ) )
	{
		ChunkManager::drawTreeBranch( pChunk, " - BOUNDINGBOX NOT INTERSECTED" );
		ChunkManager::drawTreeReturn();
		return pClipPortal;
	}
	// if we're going to the outside, do something different here
	if (outside)
	{
		// if we're looking through a smaller portal, make sure there's
		// something worth drawing.
		// E3'04: don't clip the outside to just one inside portal...
		// far too error prone in somewhere with multiple heavenly portals
		// like say the city - we really need portal aggregation for this.
		if (&*pClipPortal != NULL && !(outSum & OUTCODE_NEAR) &&
			s_clipOutsideToPortal)
		{
			Portal2DRef combinedP2D = Portal2DStore::grab();
			if (!combinedP2D->combine( &*pClipPortal, &*ourClipPortal ))
			{
				ChunkManager::drawTreeBranch( pChunk, " - COMBINED" );
				ChunkManager::drawTreeReturn();
				return Portal2DRef( false );
			}
		}

		// ok, either we're outside looking outside, or we're
		// inside looking outside and we can see this portal.
		// either way, we want to draw the chunk ... so draw it!
		ChunkManager::drawTreeBranch( pChunk, " + outside" );
		//pChunk->draw( pClipPortal );
		pChunk->drawBeg();
		return pClipPortal;
	}
	else
	{
		// ok, we're not going to the outside.

		// so, are any of our clip points near?
		if (outSum & OUTCODE_NEAR)
		{
			// ok, at least one was before the volume, so don't attempt to
			// do any fancy polygon intersection and just draw it with
			// the same Portal2D that we got given.
			ChunkManager::drawTreeBranch( pChunk, " + close" );
			//pChunk->draw( pClipPortal );
			pChunk->drawBeg();
			return pClipPortal;
		}
		else
		{
			// ok, let's combine them Portal2D's
			Portal2DRef combinedP2D = ourClipPortal;
			/*
			if (&*pClipPortal == NULL)
			{
				static Portal2D defP2D;
				if (defP2D.points().empty()) defP2D.setDefaults();

				combinedP2D->combine( &defP2D, &*ourClipPortal );

				// TODO: Why don't we just use ourClipPortal
				// straight off here? Instead of combining it
				// with the default one?
			}
			*/
			if (&*pClipPortal != NULL)
			{
				combinedP2D = Portal2DStore::grab();
				if (!combinedP2D->combine( &*pClipPortal, &*ourClipPortal ))
				{
					ChunkManager::drawTreeBranch( pChunk, " - COMBINED" );
					ChunkManager::drawTreeReturn();
					return Portal2DRef( false );
				}
			}

			ChunkManager::drawTreeBranch( pChunk, " + combined" );
			//pChunk->draw( &combinedP2D );

			pChunk->drawBeg();

			if (drawPortals_)
			{
#if ENABLE_DRAW_PORTALS
					Geometrics::drawLinesInClip(
						(Vector2*)&*combinedP2D->points().begin(),
						combinedP2D->points().size() );
#endif
			}
			return combinedP2D;
		}
	}
}
#endif// CHUNK_TRAVERSE_OPTIMIZATION_OFF

/**
 *	Debugging method for displaying a portal. It is drawn in purple if the
 *	camera is on the inside of the portal plane, and green if it's outside.
 */
void ChunkBoundary::Portal::display( const Matrix & transform,
	const Matrix & transformInverse, float inset ) const
{
	Vector3 worldPoint[16];

	// find the centre
	Vector2 avgPt(0.f,0.f);
	for( uint32 i = 0; i < points.size(); i++ ) avgPt += points[i];
	avgPt /= float(points.size());

	// transform all the points
	for( uint32 i = 0; i < points.size(); i++ )
	{
		// project the point into clip space
		transform.applyPoint( worldPoint[i], Vector3(
			uAxis * (points[i][0] + (points[i][0] < avgPt[0] ? inset : -inset)) +
			vAxis * (points[i][1] + (points[i][1] < avgPt[1] ? inset : -inset)) +
			origin ) );
	}

	// set the colour based on which side of the portal the camera is on
	uint32 colour = plane.distanceTo( transformInverse.applyPoint(
		Moo::rc().invView().applyToOrigin() ) ) < 0.f ? 0x0000ff00 : 0x00ff00ff;

	// draw the lines
	for( uint32 i = 0; i < points.size(); i++ )
	{
		Geometrics::drawLine( worldPoint[i], worldPoint[(i+1)%points.size()], colour );
	}
}
#endif // !MF_SERVER

// chunk_boundary.cpp
