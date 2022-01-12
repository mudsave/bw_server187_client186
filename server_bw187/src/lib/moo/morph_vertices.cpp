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
#include "morph_vertices.hpp"
#include "vertex_formats.hpp"
#include "resmgr/primitive_file.hpp"
#include "cstdmf/profiler.hpp"
#include "cstdmf/timestamp.hpp"
#include "cstdmf/debug.hpp"
#include "resmgr/bwresource.hpp"

#include "render_context.hpp"
#include "dynamic_vertex_buffer.hpp"

DECLARE_DEBUG_COMPONENT2( "Moo", 0 )

#ifndef EDITOR_ENABLED
memoryCounterDefine( morphVerts, Geometry );
#endif

namespace Moo
{

uint32	MorphVertices::globalCookie_ = 0;

/**
 *	Constructor
 */
VertexListBase::VertexListBase()
{
	memoryCounterAdd( morphVerts );
	memoryClaim( this );
}

/**
 *	Destructor
 */
VertexListBase::~VertexListBase()
{
	memoryCounterSub( morphVerts );
	memoryClaim( this );
}
template <class VertexType>
class VertexList : public VertexListBase, public std::vector<VertexType>
{
public:
	~VertexList()
	{
		memoryCounterSub( morphVerts );
		memoryClaim( *this );
	}
	void * vertices()
	{
		return &this->front();
	}
	uint32 nVertices()
	{
		return this->size();
	}
protected:
};

StringHashMap<MorphVertices::MorphValue> MorphVertices::morphValues_;

struct VertexHeader
{
	char	vertexFormat[ 64 ];
	int		nVertices;
};

/**
 *	Constructor
 */
MorphTargetBase::MorphTargetBase( const MorphVertex* vertices, int nVertices,
		const std::string & identifier, int index ) :
	identifier_( identifier ),
	index_( index )
{
	morphVertices_.assign( vertices, vertices + nVertices );

	memoryCounterAdd( morphVerts );
	memoryClaim( this );
	memoryClaim( identifier_ );
	memoryClaim( morphVertices_ );
}

/**
 *	Destructor
 */
MorphTargetBase::~MorphTargetBase()
{
	memoryCounterSub( morphVerts );
	memoryClaim( morphVertices_ );
	memoryClaim( identifier_ );
	memoryClaim( this );
}

// -----------------------------------------------------------------------------
// Section: MorphVertices
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
MorphVertices::MorphVertices(const std::string& resourceID, int numNodes)
: Vertices( resourceID, numNodes )
{
}


/**
 *	Destructor.
 */
MorphVertices::~MorphVertices()
{
	memoryCounterSub( morphVerts );
	memoryClaim( morphTargets_ );
}

/*
 * Helper function to copy the position out of a vertex.
 */
template <class VertexType>
void fillVertexPositions(Vertices::VertexPositions& vList, VertexType* pVertices, uint32 nVertices)
{
	vList.resize(nVertices);
	for (uint32 i = 0; i < nVertices;i++)
	{
		vList[i] = (pVertices++)->pos_;
	}
}

/**
 *	Load method
 */
HRESULT MorphVertices::load( )
{
	HRESULT res = E_FAIL;
	release();

	// Is there a valid device pointer?
	if( Moo::rc().device() == (void*)NULL )
	{
		return res;
	}

	// find our data
	BinaryPtr vertices;
	uint noff = resourceID_.find( ".primitives/" );
	if (noff < resourceID_.size())
	{
		// everything is normal
		noff += 11;
		vertices = PrimitiveFile::get( resourceID_.substr( 0, noff ) )->
			readBinary( resourceID_.substr( noff+1 ) );
	}
	else
	{
		// find out where the data should really be stored
		std::string fileName, partName;
		splitOldPrimitiveName( resourceID_, fileName, partName );

		// read it in from this file
		vertices = fetchOldPrimitivePart( fileName, partName );
	}

	// Open the binary resource for these vertices
	//BinaryPtr vertices = BWResource::instance().rootSection()->readBinary( resourceID_ );
	if( vertices )
	{
		memoryCounterAdd( morphVerts );

		// Get the vertex header
		const VertexHeader* vh = reinterpret_cast< const VertexHeader* >( vertices->data() );
		nVertices_ = vh->nVertices;

		_strlwr( const_cast< char* >( vh->vertexFormat ) );

		format_ = vh->vertexFormat;

		pDecl_ = VertexDeclaration::get( vh->vertexFormat );

		if (format_ == "xyznuv")
		{
			VertexList<VertexXYZNUV>* vl = new VertexList<VertexXYZNUV>();
			vertexList_ = vl;
			vertexStride_ = sizeof( VertexXYZNUV );
			const VertexXYZNUV* verts = reinterpret_cast< const VertexXYZNUV* >( vh + 1 );
			vl->assign( verts, verts + nVertices_ );
			fillVertexPositions( vertexPositions_, verts, nVertices_ );
			memoryClaim( *vl );
			const MorphHeader* mh = reinterpret_cast< const MorphHeader* >( verts + nVertices_ );
			const MorphTargetHeader* mth = reinterpret_cast< const MorphTargetHeader* >( mh + 1 );
			for (int i = 0; i < mh->nMorphTargets_; i++)
			{
				const MorphTargetBase::MorphVertex* mv = reinterpret_cast< const MorphTargetBase::MorphVertex* >( mth + 1 );
				MorphTargetPtr mt = new MorphTarget< VertexXYZNUV >(
					mv, mth->nVertices_, mth->identifier_, mth->channelIndex_ );

				morphTargets_.push_back( mt );
				mth = reinterpret_cast< const MorphTargetHeader* >( mv + mth->nVertices_ );
			}
		}
		else if (format_ == "xyznuvi")
		{
			VertexList<VertexXYZNUVI>* vl = new VertexList<VertexXYZNUVI>();
			vertexList_ = vl;
			vertexStride_ = sizeof( VertexXYZNUVI );
			ScopedVertexArray<Moo::VertexXYZNUVI> sva;
			Moo::VertexXYZNUVI* verts = sva.init(
				reinterpret_cast< const Moo::VertexXYZNUVI* >( vh + 1 ), nVertices_ );
			if ( !verifyIndices1<VertexXYZNUVI>( verts, nVertices_ ) )
			{
				ERROR_MSG( "Moo::MorphVertices::load: Vertices in %s contain invalid bone indices\n", resourceID_.c_str() );
			}
			vl->assign( verts, verts + nVertices_ );

			SoftwareSkinner< RigidSkinVertex >* pSkinner = new SoftwareSkinner< RigidSkinVertex >;
			pSkinner->init( &vl->front(), vl->size() );
			pSoftwareSkinner_ = pSkinner;

			fillVertexPositions( vertexPositions_, verts, nVertices_ );
			memoryClaim( *vl );
			const Moo::VertexXYZNUVI* verts2 = reinterpret_cast< const Moo::VertexXYZNUVI* >( vh + 1 );
			const MorphHeader* mh = reinterpret_cast< const MorphHeader* >( verts2 + nVertices_ );
			const MorphTargetHeader* mth = reinterpret_cast< const MorphTargetHeader* >( mh + 1 );
			for (int i = 0; i < mh->nMorphTargets_; i++)
			{
				const MorphTargetBase::MorphVertex* mv = reinterpret_cast< const MorphTargetBase::MorphVertex* >( mth + 1 );
				MorphTargetPtr mt = new MorphTarget< VertexXYZNUVI >(
					mv, mth->nVertices_, mth->identifier_, mth->channelIndex_ );

				morphTargets_.push_back( mt );
				mth = reinterpret_cast< const MorphTargetHeader* >( mv + mth->nVertices_ );
			}
		}
		else if (format_ == "xyznuviiiww")
		{
			VertexList<VertexXYZNUVIIIWWPC>* vl = new VertexList<VertexXYZNUVIIIWWPC>();
			vertexList_ = vl;
			vertexStride_ = sizeof( VertexXYZNUVIIIWWPC );
			ScopedVertexArray<Moo::VertexXYZNUVIIIWW> sva;
			Moo::VertexXYZNUVIIIWW* verts = sva.init(
				reinterpret_cast< const Moo::VertexXYZNUVIIIWW* >( vh + 1 ), nVertices_ );
			if ( !verifyIndices3<VertexXYZNUVIIIWW>( verts, nVertices_ ) )
			{
				ERROR_MSG( "Moo::MorphVertices::load: Vertices in %s contain invalid bone indices\n", resourceID_.c_str() );
			}
			vl->assign( verts, verts + nVertices_ );

			SoftwareSkinner< SoftSkinVertex >* pSkinner = new SoftwareSkinner< SoftSkinVertex >;
			pSkinner->init( &vl->front(), vl->size() );
			pSoftwareSkinner_ = pSkinner;

			fillVertexPositions( vertexPositions_, verts, nVertices_ );
			memoryClaim( *vl );
			const Moo::VertexXYZNUVIIIWW* verts2 = reinterpret_cast< const Moo::VertexXYZNUVIIIWW* >( vh + 1 );
			const MorphHeader* mh = reinterpret_cast< const MorphHeader* >( verts2 + nVertices_ );
			const MorphTargetHeader* mth = reinterpret_cast< const MorphTargetHeader* >( mh + 1 );
			for (int i = 0; i < mh->nMorphTargets_; i++)
			{
				const MorphTargetBase::MorphVertex* mv = reinterpret_cast< const MorphTargetBase::MorphVertex* >( mth + 1 );
				MorphTargetPtr mt = new MorphTarget< VertexXYZNUVIIIWWPC >(
					mv, mth->nVertices_, mth->identifier_, mth->channelIndex_ );
				morphTargets_.push_back( mt );
				mth = reinterpret_cast< const MorphTargetHeader* >( mv + mth->nVertices_ );
			}
		}
		else if (format_ == "xyznuvtb")
		{
			VertexList<VertexXYZNUVTBPC>* vl = new VertexList<VertexXYZNUVTBPC>();
			vertexList_ = vl;
			vertexStride_ = sizeof( VertexXYZNUVTBPC );
			const VertexXYZNUVTB* verts = reinterpret_cast< const VertexXYZNUVTB* >( vh + 1 );
			vl->assign( verts, verts + nVertices_ );
			fillVertexPositions( vertexPositions_, verts, nVertices_ );
			memoryClaim( *vl );
			const MorphHeader* mh = reinterpret_cast< const MorphHeader* >( verts + nVertices_ );
			const MorphTargetHeader* mth = reinterpret_cast< const MorphTargetHeader* >( mh + 1 );
			for (int i = 0; i < mh->nMorphTargets_; i++)
			{
				const MorphTargetBase::MorphVertex* mv = reinterpret_cast< const MorphTargetBase::MorphVertex* >( mth + 1 );
				MorphTargetPtr mt = new MorphTarget< VertexXYZNUVTBPC >(
					mv, mth->nVertices_, mth->identifier_, mth->channelIndex_ );
				morphTargets_.push_back( mt );
				mth = reinterpret_cast< const MorphTargetHeader* >( mv + mth->nVertices_ );
			}
		}
		else if (format_ == "xyznuviiiwwtb")
		{
			VertexList<VertexXYZNUVIIIWWTBPC>* vl = new VertexList<VertexXYZNUVIIIWWTBPC>();
			vertexList_ = vl;
			vertexStride_ = sizeof( VertexXYZNUVIIIWWTBPC );
			ScopedVertexArray<Moo::VertexXYZNUVIIIWWTB> sva;
			Moo::VertexXYZNUVIIIWWTB* verts = sva.init(
				reinterpret_cast< const Moo::VertexXYZNUVIIIWWTB* >( vh + 1 ), nVertices_ );
			if ( !verifyIndices3<VertexXYZNUVIIIWWTB>( verts, nVertices_ ) )
			{
				ERROR_MSG( "Moo::MorphVertices::load: Vertices in %s contain invalid bone indices\n", resourceID_.c_str() );
			}
			vl->assign( verts, verts + nVertices_ );

			SoftwareSkinner< SoftSkinBumpVertex >* pSkinner = new SoftwareSkinner< SoftSkinBumpVertex >;
			pSkinner->init( &vl->front(), vl->size() );
			pSoftwareSkinner_ = pSkinner;

			fillVertexPositions( vertexPositions_, verts, nVertices_ );
			memoryClaim( *vl );
			const Moo::VertexXYZNUVIIIWWTB* verts2 = reinterpret_cast< const Moo::VertexXYZNUVIIIWWTB* >( vh + 1 );
			const MorphHeader* mh = reinterpret_cast< const MorphHeader* >( verts2 + nVertices_ );
			const MorphTargetHeader* mth = reinterpret_cast< const MorphTargetHeader* >( mh + 1 );
			for (int i = 0; i < mh->nMorphTargets_; i++)
			{
				const MorphTargetBase::MorphVertex* mv = reinterpret_cast< const MorphTargetBase::MorphVertex* >( mth + 1 );
				MorphTargetPtr mt = new MorphTarget< VertexXYZNUVIIIWWTBPC >(
					mv, mth->nVertices_, mth->identifier_, mth->channelIndex_ );
				morphTargets_.push_back( mt );
				mth = reinterpret_cast< const MorphTargetHeader* >( mv + mth->nVertices_ );
			}
		}
		else if (format_ == "xyznuvitb")
		{
			VertexList<VertexXYZNUVITBPC>* vl = new VertexList<VertexXYZNUVITBPC>();
			vertexList_ = vl;
			vertexStride_ = sizeof( VertexXYZNUVITBPC );
			ScopedVertexArray<Moo::VertexXYZNUVITB> sva;
			Moo::VertexXYZNUVITB* verts = sva.init(
				reinterpret_cast< const Moo::VertexXYZNUVITB* >( vh + 1 ), nVertices_ );
			if ( !verifyIndices1<VertexXYZNUVITB>( verts, nVertices_ ) )
			{
				ERROR_MSG( "Moo::MorphVertices::load: Vertices in %s contain invalid bone indices\n", resourceID_.c_str() );
			}
			vl->assign( verts, verts + nVertices_ );

			SoftwareSkinner< RigidSkinBumpVertex >* pSkinner = new SoftwareSkinner< RigidSkinBumpVertex >;
			pSkinner->init( &vl->front(), vl->size() );
			pSoftwareSkinner_ = pSkinner;

			fillVertexPositions( vertexPositions_, verts, nVertices_ );
			memoryClaim( *vl );
			const Moo::VertexXYZNUVITB* verts2 = reinterpret_cast< const Moo::VertexXYZNUVITB* >( vh + 1 );
			const MorphHeader* mh = reinterpret_cast< const MorphHeader* >( verts2 + nVertices_ );
			const MorphTargetHeader* mth = reinterpret_cast< const MorphTargetHeader* >( mh + 1 );
			for (int i = 0; i < mh->nMorphTargets_; i++)
			{
				const MorphTargetBase::MorphVertex* mv = reinterpret_cast< const MorphTargetBase::MorphVertex* >( mth + 1 );
				MorphTargetPtr mt = new MorphTarget< VertexXYZNUVITBPC >(
					mv, mth->nVertices_, mth->identifier_, mth->channelIndex_ );

				morphTargets_.push_back( mt );
				mth = reinterpret_cast< const MorphTargetHeader* >( mv + mth->nVertices_ );
			}
		}
		else
		{
			ERROR_MSG( "Failed to recognise vertex format: %s\n", vh->vertexFormat );
			return res;
		}
		memoryClaim( morphTargets_ );

		res = D3D_OK;
	}
	else
	{
		ERROR_MSG( "Failed to read binary resource: %s\n", resourceID_.c_str() );
	}

	return res;
}

/**
 *	Release method
 */
HRESULT MorphVertices::release( )
{
	vertexList_ = NULL;
	nVertices_ = 0;
	return S_OK;
}

PROFILER_DECLARE( MorphVertices_setVertices, "MorphVertices SetVertices" );

HRESULT MorphVertices::setVertices( bool software, bool staticLighting )
{
	PROFILER_SCOPED( MorphVertices_setVertices );

	HRESULT hr = E_FAIL;

	// Does our vertexbuffer exist.
	if( &*vertexList_ == NULL )
	{
		// If not load it
		hr = load();

		if( FAILED( hr ) )
			return hr;
	}

	hr = Moo::rc().setVertexDeclaration( pDecl_->declaration() );
	
	if ( SUCCEEDED(hr) )
	{
		if (format_ == "xyznuv")
			return setVerticesType<VertexXYZNUV>( software );
		else if (format_ == "xyznuvi")
			return setVerticesType<VertexXYZNUVI>( software );
		else if (format_ == "xyznuviiiww")
			return setVerticesType<VertexXYZNUVIIIWWPC>( software );
		else if (format_ == "xyznuvitb")
			return setVerticesType<VertexXYZNUVITBPC>( software );
		else if (format_ == "xyznuvtb")
			return setVerticesType<VertexXYZNUVTBPC>( software );
		else if (format_ == "xyznuviiiwwtb")
			return setVerticesType<VertexXYZNUVIIIWWTBPC>( software );
	}

	// failed, return error code
	return hr;
}

template <typename VT>
void copyVertexPositions( const VT* start, uint32 nVertices, VectorNoDestructor<Vector3>& out )
{
	out.clear();
	const VT* it = start;

	for (uint32 i = 0; i < nVertices; i++)
	{
		out.push_back( (it++)->pos_ );
	}
}

template <typename VT>
void copyVertexPositions( const VT* start, uint32 nVertices, Vector3* pOut )
{
	const VT* it = start;
	Vector3* pit = pOut;

	for (uint32 i = 0; i < nVertices; i++)
	{
		 *(pit++) = (it++)->pos_;
	}
}

PROFILER_DECLARE( MorphVertices_setTransformedVertices1, "MorphVertices SetTransformedVertices 1" );

uint32	MorphVertices::setTransformedVertices( uint32 startVertex, uint32 nVertices, bool tb, const Matrix* pTransforms )
{
	PROFILER_SCOPED( MorphVertices_setTransformedVertices1 );

	uint32 vertexBase = startVertex;
	if (pSoftwareSkinner_)
	{
		static VectorNoDestructor< Vector3 > verts;

		if (format_ == "xyznuvi")
			copyVertexPositions( (VertexXYZNUVI*)vertexList_->vertices(),
				nVertices_, verts );
		else if (format_ == "xyznuviiiww")
			copyVertexPositions( (VertexXYZNUVIIIWWPC*)vertexList_->vertices(),
				nVertices_, verts );
		else if (format_ == "xyznuvitb")
			copyVertexPositions( (VertexXYZNUVITBPC*)vertexList_->vertices(),
				nVertices_, verts );
		else if (format_ == "xyznuviiiwwtb")
			copyVertexPositions( (VertexXYZNUVIIIWWTBPC*)vertexList_->vertices(),
				nVertices_, verts );
		else
			return E_FAIL;


		MorphTargetVector::iterator it = morphTargets_.begin();
		MorphTargetVector::iterator end = morphTargets_.end();
		while (it != end)
		{
			float val = morphValues_[(*it)->identifier()].value( globalCookie_);
			if (val > 0.001f)
				(*it)->applyPosOnly( &verts.front(), val );
			++it;
		}

		if (tb)
		{
			static VertexDeclaration* pDecl = VertexDeclaration::get( "xyznuvtb" );
			Moo::rc().setVertexDeclaration( pDecl->declaration() );

			//DynamicVertexBuffer
			Moo::DynamicVertexBufferBase2<VertexXYZNUVTBPC>& vb = Moo::DynamicVertexBufferBase2<VertexXYZNUVTBPC>::instance();
			VertexXYZNUVTBPC* pVerts = vb.lock( nVertices_ );
			pSoftwareSkinner_->transformVertices( pVerts, startVertex, nVertices, pTransforms, &verts.front() + startVertex );
			vb.unlock();
			Moo::rc().device()->SetStreamSource( 0, pSkinnerVertexBuffer_, 0, sizeof( VertexXYZNUVTBPC ) );
		}
		else
		{
			Moo::rc().setFVF( D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1 );

			//DynamicVertexBuffer
			Moo::DynamicVertexBufferBase2<VertexXYZNUV>& vb = Moo::DynamicVertexBufferBase2<VertexXYZNUV>::instance();
			VertexXYZNUV* pVerts = vb.lock( nVertices_ );
			pSoftwareSkinner_->transformVertices( pVerts, startVertex, nVertices, pTransforms, &verts.front() + startVertex );
			vb.unlock();

			//TODO: any reason why the vertex buffer being filled isnt being set?
			Moo::rc().device()->SetStreamSource( 0, pSkinnerVertexBuffer_, 0, sizeof( VertexXYZNUV ) );
		}
	}
	else
	{
		return setVertices( false );
	}
	return vertexBase;
}

PROFILER_DECLARE( MorphVertices_setTransformedVertices2, "MorphVertices SetTransformedVertices 2" );

HRESULT	MorphVertices::setTransformedVertices( bool tb, const NodePtrVector& nodes )
{
	PROFILER_SCOPED( MorphVertices_setTransformedVertices2 );

	if (pSoftwareSkinner_)
	{
		static VectorNoDestructor< Vector3 > verts;

		if (format_ == "xyznuvi")
			copyVertexPositions( (VertexXYZNUVI*)vertexList_->vertices(),
				nVertices_, verts );
		else if (format_ == "xyznuviiiww")
			copyVertexPositions( (VertexXYZNUVIIIWWPC*)vertexList_->vertices(),
				nVertices_, verts );
		else if (format_ == "xyznuvitb")
			copyVertexPositions( (VertexXYZNUVITBPC*)vertexList_->vertices(),
				nVertices_, verts );
		else if (format_ == "xyznuviiiwwtb")
			copyVertexPositions( (VertexXYZNUVIIIWWTBPC*)vertexList_->vertices(),
				nVertices_, verts );
		else
			return E_FAIL;


		MorphTargetVector::iterator it = morphTargets_.begin();
		MorphTargetVector::iterator end = morphTargets_.end();
		while (it != end)
		{
			float val = morphValues_[(*it)->identifier()].value( globalCookie_);
			if (val > 0.001f)
				(*it)->applyPosOnly( &verts.front(), val );
			++it;
		}

		if (tb)
		{
			static VertexDeclaration* pDecl = VertexDeclaration::get( "xyznuvtb" );
			if (!pSkinnerVertexBuffer_ || vbBumped_ == false)
			{
				Moo::rc().setVertexDeclaration( pDecl->declaration() );
				if (rc().mixedVertexProcessing())
				{
					DynamicSoftwareVertexBuffer<VertexXYZNUVTBPC>& vb = DynamicSoftwareVertexBuffer<VertexXYZNUVTBPC>::instance();
					VertexXYZNUVTBPC* pVerts = vb.lock( nVertices_ );
					pSoftwareSkinner_->transformVertices( pVerts, 0, nVertices_, nodes, &verts.front() );
					vb.unlock();
					pSkinnerVertexBuffer_ = vb.pVertexBuffer();
				}
				else
				{
					DynamicVertexBuffer<VertexXYZNUVTBPC>& vb = DynamicVertexBuffer<VertexXYZNUVTBPC>::instance();
					VertexXYZNUVTBPC* pVerts = vb.lock( nVertices_ );
					pSoftwareSkinner_->transformVertices( pVerts, 0, nVertices_, nodes, &verts.front() );
					vb.unlock();
					pSkinnerVertexBuffer_ = vb.pVertexBuffer();
				}
				vbBumped_ = true;
			}
			return Moo::rc().device()->SetStreamSource( 0, pSkinnerVertexBuffer_, 0, sizeof( VertexXYZNUVTBPC ) );
		}
		else
		{
			if (!pSkinnerVertexBuffer_ || vbBumped_ == true)
			{
				Moo::rc().setFVF( D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1 );
				if (rc().mixedVertexProcessing())
				{
					DynamicSoftwareVertexBuffer<VertexXYZNUV>& vb = DynamicSoftwareVertexBuffer<VertexXYZNUV>::instance();
					VertexXYZNUV* pVerts = vb.lock( nVertices_ );
					pSoftwareSkinner_->transformVertices( pVerts, 0, nVertices_, nodes, &verts.front() );
					vb.unlock();
					pSkinnerVertexBuffer_ = vb.pVertexBuffer();
				}
				else
				{
					DynamicVertexBuffer<VertexXYZNUV>& vb = DynamicVertexBuffer<VertexXYZNUV>::instance();
					VertexXYZNUV* pVerts = vb.lock( nVertices_ );
					pSoftwareSkinner_->transformVertices( pVerts, 0, nVertices_, nodes, &verts.front() );
					vb.unlock();
					pSkinnerVertexBuffer_ = vb.pVertexBuffer();
				}
				vbBumped_ = false;
			}
			return Moo::rc().device()->SetStreamSource( 0, pSkinnerVertexBuffer_, 0, sizeof( VertexXYZNUV ) );
		}
	}
	else
	{
		return setVertices( false );
	}
	return S_OK;
}


const Vertices::VertexPositions& MorphVertices::vertexPositions() const
{
	vertexPositions_.resize( nVertices_ );
	if (format_ == "xyznuvi")
		copyVertexPositions( (VertexXYZNUVI*)vertexList_->vertices(),
			nVertices_, &vertexPositions_.front() );
	else if (format_ == "xyznuviiiww")
		copyVertexPositions( (VertexXYZNUVIIIWWPC*)vertexList_->vertices(),
			nVertices_, &vertexPositions_.front() );
	else if (format_ == "xyznuvitb")
		copyVertexPositions( (VertexXYZNUVITBPC*)vertexList_->vertices(),
			nVertices_, &vertexPositions_.front() );
	else if (format_ == "xyznuviiiwwtb")
		copyVertexPositions( (VertexXYZNUVIIIWWTBPC*)vertexList_->vertices(),
			nVertices_, &vertexPositions_.front() );
	else
		return vertexPositions_;

	MorphTargetVector::const_iterator it = morphTargets_.begin();
	MorphTargetVector::const_iterator end = morphTargets_.end();
	while (it != end)
	{
		float val = morphValues_[(*it)->identifier()].value( globalCookie_);
		if (val > 0.001f)
			(*it)->applyPosOnly( &vertexPositions_.front(), val );
		++it;
	}

	return vertexPositions_;
}

typedef SmartPointer<MorphVertices> MorphVerticesPtr;

class MorphValueCache
{
public:
	void init(MorphVerticesPtr pMV)
	{
		const MorphVertices::MorphTargetVector& targets =
			pMV->morphTargets();
		morphValues_.resize(targets.size());
		morphValues_.clear();
		for (uint32 i = 0; i < targets.size(); i++)
		{
			morphValues_.push_back(std::make_pair(targets[i]->identifier(),
				MorphVertices::morphValue( targets[i]->identifier() ) ) );
		}
	}
	void playback()
	{
		MorphVertices::incrementCookie();
		for (uint32 i = 0; i < morphValues_.size(); i++)
		{
			MorphVertices::addMorphValue( morphValues_[i].first,
				morphValues_[i].second, 1.f );
		}
	}
private:
	VectorNoDestructor< std::pair<std::string, float> > morphValues_;
};

class RigidMorphVertexSnapshot : public VertexSnapshot, public Aligned
{
public:
	virtual ~RigidMorphVertexSnapshot() {}

	void init( MorphVerticesPtr pVertices, const Matrix& worldTransform )
	{
		pVertices_ = pVertices;
		worldViewProj_.multiply( worldTransform, Moo::rc().viewProjection() );
		mvc_.init( pVertices );
	}

	virtual void getVertexDepths( uint32 startVertex, uint32 nVertices, float* pOutDepths )
	{
		mvc_.playback();
		const Vertices::VertexPositions& positions = pVertices_->vertexPositions();

		Vector3 vec( worldViewProj_.row(0).w, worldViewProj_.row(1).w,
			worldViewProj_.row(2).w );
        float d = worldViewProj_.row(3).w;

		float* pDepth = pOutDepths;
		Vertices::VertexPositions::const_iterator it = positions.begin() + startVertex;
		for (uint32 i = 0; i < nVertices; i++)
		{
			*(pDepth++) = vec.dotProduct(*(it++)) + d;
		}
	}

	virtual uint32 setVertices( uint32 startVertex, uint32 nVertices, bool staticLighting )
	{
		mvc_.playback();
		pVertices_->setVertices( false, staticLighting );
		return startVertex;
	}
protected:
	MorphVerticesPtr	pVertices_;
	Matrix		worldViewProj_;
	MorphValueCache mvc_;
};

class SkinnedMorphVertexSnapshot : public VertexSnapshot
{
public:
	virtual ~SkinnedMorphVertexSnapshot() {}

	void init( MorphVerticesPtr pVertices, const NodePtrVector& nodes, bool useSoftwareSkinner, bool bumpMapped )
	{
		pVertices_ = pVertices;
		worldTransforms_.resize( nodes.size() );
		std::avector<Matrix>::iterator mit = worldTransforms_.begin();
		NodePtrVector::const_iterator it = nodes.begin();
		while (it != nodes.end())
		{
			*mit = (*it)->worldTransform();
			++it;
			++mit;
		}
		useSoftwareSkinner_ = useSoftwareSkinner;
		bumpMapped_ = bumpMapped;
		mvc_.init( pVertices );
	}

	virtual void getVertexDepths( uint32 startVertex, uint32 nVertices, float* pOutDepths )
	{
		mvc_.playback();
		static	VectorNoDestructor<Vector4> partialWorldView;
		partialWorldView.resize(worldTransforms_.size());

		const Vertices::VertexPositions& positions = pVertices_->vertexPositions();

		const Matrix& viewProj = rc().viewProjection();

		VectorNoDestructor<Vector4>::iterator vit = partialWorldView.begin();
		std::avector<Matrix>::iterator it = worldTransforms_.begin();
		while (it != worldTransforms_.end())
		{
			const Vector4 v( it->row(0).w,  it->row(1).w,  it->row(2).w,  it->row(3).w );
			vit->x = viewProj.row(0).dotProduct(v);
			vit->y = viewProj.row(1).dotProduct(v);
			vit->z = viewProj.row(2).dotProduct(v);
			vit->w = viewProj.row(3).dotProduct(v);
			it++;
			vit++;
		}
		BaseSoftwareSkinnerPtr pSkinner = pVertices_->softwareSkinner();
		pSkinner->outputDepths( pOutDepths, startVertex, nVertices,
			&partialWorldView.front(), &positions.front() + startVertex );
	}

	virtual uint32 setVertices( uint32 startVertex, uint32 nVertices, bool staticLighting )
	{
		mvc_.playback();
		uint32 vertexBase = startVertex;
		if (!useSoftwareSkinner_)
		{
			pVertices_->setVertices( false, staticLighting );
		}
		else if (pVertices_->softwareSkinner())
		{
			const Vertices::VertexPositions& positions = pVertices_->vertexPositions();
			DX::VertexBuffer* pVB = NULL;
			BaseSoftwareSkinnerPtr pSkinner = pVertices_->softwareSkinner();
			if (bumpMapped_)
			{
				static VertexDeclaration* pDecl = VertexDeclaration::get( "xyznuvtb" );
				Moo::rc().setVertexDeclaration( pDecl->declaration() );

				//DynamicVertexBuffer
				Moo::DynamicVertexBufferBase2<VertexXYZNUVTBPC>& vb = Moo::DynamicVertexBufferBase2<VertexXYZNUVTBPC>::instance();
				VertexXYZNUVTBPC* pVerts = vb.lock2( nVertices );
				pSkinner->transformVertices( pVerts, startVertex, nVertices, &worldTransforms_.front(), 
					&positions.front() + startVertex );
				vb.unlock();
				vertexBase = vb.lockIndex();
				vb.set( 0 );
			}
			else
			{
				Moo::rc().setFVF( D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1 );


				//DynamicVertexBuffer
				Moo::DynamicVertexBufferBase2<VertexXYZNUV>& vb = Moo::DynamicVertexBufferBase2<VertexXYZNUV>::instance();
				VertexXYZNUV* pVerts = vb.lock2( nVertices );
				pSkinner->transformVertices( pVerts, startVertex, nVertices, &worldTransforms_.front(), 
					&positions.front() + startVertex );
				vb.unlock();				
				vertexBase = vb.lockIndex();
				vb.set( 0 );			
			}

		}
		return vertexBase;
	}
protected:
	MorphVerticesPtr	pVertices_;
	std::avector<Matrix> worldTransforms_;
	bool	useSoftwareSkinner_;
	bool	bumpMapped_;
	MorphValueCache mvc_;
};

VertexSnapshotPtr	MorphVertices::getSnapshot( const NodePtrVector& nodes, bool skinned, bool bumpMapped )
{
	VertexSnapshot* pSnapshot = NULL;

	if (pSoftwareSkinner_)
	{
		SkinnedMorphVertexSnapshot* pSS = new SkinnedMorphVertexSnapshot;
		pSS->init( this, nodes, !skinned, bumpMapped );
		pSnapshot = pSS;
	}
	else
	{
		RigidMorphVertexSnapshot* pSS = new RigidMorphVertexSnapshot;
		pSS->init( this, nodes.front()->worldTransform() );
		pSnapshot = pSS;
	}

	return VertexSnapshotPtr(pSnapshot);
}

} // namespace Moo
