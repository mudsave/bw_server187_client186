/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CHUNK_BOUNDARY_HPP
#define CHUNK_BOUNDARY_HPP

#include "math/boundbox.hpp"
#include "math/planeeq.hpp"
#include "math/vector2.hpp"
#include "moo/moo_math.hpp"

#include <vector>

#include "cstdmf/smartpointer.hpp"
#include "resmgr/datasection.hpp"


class Chunk;
class ChunkSpace;
class Portal2D;
class ChunkDirMapping;

#include "umbra_config.hpp"

#if UMBRA_ENABLE
namespace Umbra
{
	class Model;
	class PhysicalPortal;
}
#endif

/**
 *	This class wraps a Portal2D so it can be reused when no longer needed.
 */
class Portal2DRef
{
public:
	Portal2DRef( bool valid = true );
	Portal2DRef( const Portal2DRef & other );
	Portal2DRef & operator =( const Portal2DRef & other );
	~Portal2DRef();

	bool valid()			{ return (int)pVal_ != -1; }
	const Portal2D * pVal() const { return pVal_; }
	Portal2D * operator->()	{ return pVal_; }
	Portal2D & operator*()	{ return *pVal_; }

private:
	Portal2D * pVal_;

	friend class Portal2DStore;
};


typedef SmartPointer<struct ChunkBoundary>	ChunkBoundaryPtr;

/**
 *	This class defines a boundary plane of a chunk.
 *	It includes any portals to other chunks in that plane.
 */
struct ChunkBoundary : public ReferenceCount
{
	ChunkBoundary(	DataSectionPtr pSection,
					ChunkDirMapping * pMapping,
					const std::string & ownerChunkName = "" );
	~ChunkBoundary();

	const PlaneEq & plane() const		{ return plane_; }

	typedef std::vector<Vector2>	V2Vector;


	/**
	 *	This class defines a portal.
	 *
	 *	A portal is a polygon plus a reference to the chunk that is
	 *	visible through that polygon. It includes a flag to indicate
	 *	whether it permits objects to pass through it, and another flag
	 *	to indicate whether its plane is internal to another chunk.
	 */
	struct Portal
	{
		Portal(	DataSectionPtr pSection,
				const PlaneEq & plane,
				ChunkDirMapping * pMapping,
				const std::string & ownerChunkName = "" );
		~Portal();

		void save( DataSectionPtr pSection,
			ChunkDirMapping * pOwnMapping ) const;
		bool resolveExtern( Chunk * pOwnChunk );
		void objectSpacePoint( int idx, Vector3& ret );
		Vector3 objectSpacePoint( int idx );
		void objectSpacePoint( int idx, Vector4& ret );

		bool			internal;
		bool			permissive;
		Chunk			* pChunk;
		V2Vector		points;
		Vector3	uAxis;		// in local space
		Vector3	vAxis;		// in local space
		Vector3 origin;		// in local space
		Vector3	lcentre;	// in local space
		Vector3	centre;		// in world space
		PlaneEq			plane;		// same as plane in boundary
		std::string		label;

#ifndef MF_SERVER
		static BoundingBox bbFrustum_;
		static void updateFrustumBB();

		Portal2DRef traverse(
			const Matrix & world,
			const Matrix & worldInverse,
			Portal2DRef pClipPortal ) const;
#endif // MF_SERVER

		enum
		{
			NOTHING = 0,
			HEAVEN = 1,
			EARTH = 2,
			INVASIVE = 3,
			EXTERN = 4,
			LAST_SPECIAL = 15
		};

		bool hasChunk() const
			{ return uint32(pChunk) > uint32(LAST_SPECIAL); }

		bool isHeaven() const
			{ return uint32(pChunk) == uint32(HEAVEN); }

		bool isEarth() const
			{ return uint32(pChunk) == uint32(EARTH); }

		bool isInvasive() const
			{ return uint32(pChunk) == uint32(INVASIVE); }

		bool isExtern() const
			{ return uint32(pChunk) == uint32(EXTERN); }

#ifndef MF_SERVER
		void display( const Matrix & transform,
			const Matrix & transformInverse, float inset = 0.f ) const;
#endif // MF_SERVER

		bool inside( const Vector3& point ) const;
		uint32 outcode( const Vector3& point ) const;

#if UMBRA_ENABLE
		void		createUmbraPortal( Chunk* pOwner );
		Umbra::Model* createUmbraPortalModel();
		void		releaseUmbraPortal();

		Umbra::PhysicalPortal* pUmbraPortal_;
#endif
		static bool drawPortals_;
	};

	typedef std::vector<Portal*>	Portals;

	void bindPortal( uint32 unboundIndex );
	void loosePortal( uint32 boundIndex );

	void addInvasivePortal( Portal * pPortal );
	void splitInvasivePortal( Chunk * pChunk, uint i );


// variables
	PlaneEq		plane_;

	Portals		boundPortals_;
	Portals		unboundPortals_;
};

typedef std::vector<ChunkBoundaryPtr>	ChunkBoundaries;


// Note: chunks in unbound portals are created by the loading
// thread when it loads. They are only added to the space's list
// when the chunk is bound, and if they're not already there.
// Originally I had two different types of portal - BoundPortal
// and UnboundPortal, but this was easier in a lot of ways.
// This also means that chunks need a way of going back to being
// 'unloaded' after they've been loaded. So they probably want
// a reference count too. We could maybe use a smart pointer for
// this actually, because the ChunkBoundaries will all be cleared
// when a chunk is unloaded, and the references will go away.



#endif // CHUNK_BOUNDARY_HPP
