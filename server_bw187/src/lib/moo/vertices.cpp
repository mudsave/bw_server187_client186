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

#include "vertices.hpp"
#include "vertex_formats.hpp"

#include "cstdmf/debug.hpp"
#include "cstdmf/memory_counter.hpp"

#include "resmgr/primitive_file.hpp"
#include "resmgr/bwresource.hpp"

#include "render_context.hpp"

#include "software_skinner.hpp"

#ifndef CODE_INLINE
#include "vertices.ipp"
#endif

DECLARE_DEBUG_COMPONENT2( "Moo", 0 )

#ifndef EDITOR_ENABLED
memoryCounterDefine( vertices, Geometry );
#endif

namespace Moo
{

typedef struct
{
	char	vertexFormat[ 64 ];
	int		nVertices;
}VertexHeader;

Vertices::Vertices( const std::string& resourceID, int numNodes )
: resourceID_( resourceID ),
  nVertices_( 0 ),
  pSkinnerVertexBuffer_( NULL ),
  vbBumped_( false ),
  pDecl_( NULL ),
  pStaticDecl_( NULL ),
  numNodes_( numNodes * 3 /* indices are pre-multiplied by 3 */ )
{

}

Vertices::~Vertices()
{
	// let the manager know we're gone
	VerticesManager::del( this );
}

template <class VertexType>
void copyVertexPositions( Vertices::VertexPositions& vertexPositions, const VertexType* pVerts, uint32 nVertices )
{
	vertexPositions.resize( nVertices );
	Vertices::VertexPositions::iterator it = vertexPositions.begin();
	Vertices::VertexPositions::iterator end = vertexPositions.end();
	while (it != end)
	{
		*(it++) = (pVerts++)->pos_;
	}
}

#ifdef DEBUG_NORMALS

template <class VertexType>
void copyVertexNormals( Vertices::VertexNormals& vertexNormals, const VertexType* pVerts, uint32 nVertices )
{
	vertexNormals.resize( nVertices );
	Vertices::VertexNormals::iterator it = vertexNormals.begin();
	Vertices::VertexNormals::iterator end = vertexNormals.end();
	while (it != end)
	{
		*(it++) = (pVerts++)->normal_;
	}
}

template <class VertexType>
void copyVertexNormals2( Vertices::VertexPositions& vertexNormals, const VertexType* pVerts, uint32 nVertices )
{
	vertexNormals.resize( nVertices );
	Vertices::VertexPositions::iterator it = vertexNormals.begin();
	Vertices::VertexPositions::iterator end = vertexNormals.end();
	while (it != end)
	{
		*(it++) = (pVerts++)->normal_;
	}
}

template <class VertexType>
void copyTangentSpace( Vertices::VertexNormals& vertexNormals, const VertexType* pVerts, uint32 nVertices )
{
	vertexNormals.resize( nVertices * 3 );
	Vertices::VertexNormals::iterator it = vertexNormals.begin();
	Vertices::VertexNormals::iterator end = vertexNormals.end();
	while (it != end)
	{
		*(it++) = (pVerts)->normal_;
		*(it++) = (pVerts)->tangent_;
		*(it++) = (pVerts)->binormal_;
		pVerts++;
	}
}
#endif //DEBUG_NORMALS

class RigidVertexSnapshot : public VertexSnapshot, public Aligned
{
public:
	virtual ~RigidVertexSnapshot() {}

	void init( VerticesPtr pVertices, const Matrix& worldTransform )
	{
		pVertices_ = pVertices;
		worldViewProj_.multiply( worldTransform, Moo::rc().viewProjection() );
	}

	virtual void getVertexDepths( uint32 startVertex, uint32 nVertices, float* pOutDepths )
	{
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
		pVertices_->setVertices( false, staticLighting );
		return startVertex;
	}
protected:
	VerticesPtr	pVertices_;
	Matrix		worldViewProj_;

};

class SkinnedVertexSnapshot : public VertexSnapshot
{
public:
	virtual ~SkinnedVertexSnapshot() {}

	void init( VerticesPtr pVertices, const NodePtrVector& nodes, bool useSoftwareSkinner, bool bumpMapped )
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
	}

	void init( VerticesPtr pVertices, const std::avector<Matrix>& transforms, bool useSoftwareSkinner, bool bumpMapped )
	{
		pVertices_ = pVertices;
		worldTransforms_.resize( transforms.size() );
		worldTransforms_.assign( transforms.begin(), transforms.end() );		
		useSoftwareSkinner_ = useSoftwareSkinner;
		bumpMapped_ = bumpMapped;
	}

	virtual void getVertexDepths( uint32 startVertex, uint32 nVertices, float* pOutDepths )
	{
		if (bumpMapped_)
			getDepths<VertexXYZNUVTBPC>( transformedVertsTB_, startVertex, nVertices, pOutDepths);
		else
			getDepths<VertexXYZNUV>( transformedVerts_, startVertex, nVertices, pOutDepths);
	}

	virtual uint32 setVertices( uint32 startVertex, uint32 nVertices, bool staticLighting )
	{
		uint32 vertexBase = startVertex;
		if (!useSoftwareSkinner_)
		{
			pVertices_->setVertices( false, staticLighting );
		}
		else if (pVertices_->softwareSkinner())
		{
			BaseSoftwareSkinnerPtr pSkinner = pVertices_->softwareSkinner();
			if (bumpMapped_)
			{
				static VertexDeclaration* pDecl = VertexDeclaration::get( "xyznuvtb" );
				Moo::rc().setVertexDeclaration( pDecl->declaration() );

				//DynamicVertexBuffer 
 				Moo::DynamicVertexBufferBase2<VertexXYZNUVTBPC>& vb = Moo::DynamicVertexBufferBase2<VertexXYZNUVTBPC>::instance(); 
 				VertexXYZNUVTBPC* pVerts = vb.lock2( nVertices ); 
 				pSkinner->transformVertices( pVerts, startVertex, nVertices, &worldTransforms_.front() );                
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
				pSkinner->transformVertices( pVerts, startVertex, nVertices, &worldTransforms_.front() ); 
				vb.unlock();                 
				vertexBase = vb.lockIndex(); 
				vb.set( 0 ); 
			}
		}
		transformedVerts_.clear();
		transformedVertsTB_.clear();
		return vertexBase;
	}
protected:
	VerticesPtr	pVertices_;
	std::avector<Matrix> worldTransforms_;
	bool	useSoftwareSkinner_;
	bool	bumpMapped_;

private:
	template <class T> void getDepths( std::vector<T>& verts, uint32 startVertex, uint32 nVertices, float* pOutDepths )
	{
		Vector3 eye (Moo::rc().invView().row(3).x, Moo::rc().invView().row(3).y, Moo::rc().invView().row(3).z);
		float* pDepth = pOutDepths;
		BaseSoftwareSkinnerPtr pSkinner = pVertices_->softwareSkinner();
		verts.resize( nVertices );
		pSkinner->transformVertices( &verts.front(), startVertex, nVertices, &worldTransforms_.front() );
		for (uint i=0; i<verts.size(); i++)
		{
			*(pDepth++) = (verts[i].pos_ - eye).lengthSquared();
		}
	}

	// These are used for the sorting
	VectorNoDestructor<VertexXYZNUVTBPC>	transformedVertsTB_;
	VectorNoDestructor<VertexXYZNUV>		transformedVerts_;	
};


VertexSnapshotPtr Vertices::getSnapshot( const NodePtrVector& nodes, bool skinned, bool bumpMapped )
{
	VertexSnapshot* pSnapshot = NULL;

	if (pSoftwareSkinner_)
	{
		SkinnedVertexSnapshot* pSS = new SkinnedVertexSnapshot;
		pSS->init( this, nodes, !skinned, bumpMapped );
		pSnapshot = pSS;
	}
	else
	{
		RigidVertexSnapshot* pSS = new RigidVertexSnapshot;
		pSS->init( this, nodes.front()->worldTransform() );
		pSnapshot = pSS;
	}

	return VertexSnapshotPtr(pSnapshot);
}


VertexSnapshotPtr Vertices::getSnapshot( const std::avector<Matrix>& transforms, bool skinned, bool bumpMapped )
{
	VertexSnapshot* pSnapshot = NULL;

	if (pSoftwareSkinner_)
	{
		SkinnedVertexSnapshot* pSS = new SkinnedVertexSnapshot;
		pSS->init( this, transforms, !skinned, bumpMapped );
		pSnapshot = pSS;
	}
	else
	{
		RigidVertexSnapshot* pSS = new RigidVertexSnapshot;
		pSS->init( this, transforms[0] );
		pSnapshot = pSS;
	}

	return VertexSnapshotPtr(pSnapshot);
}


HRESULT Vertices::load( )
{
	HRESULT res = E_FAIL;
	release();

	// Is there a valid device pointer?
	if (Moo::rc().device() == (void*)NULL)
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
		DataSectionPtr pPrimFile =
			PrimitiveFile::get( resourceID_.substr( 0, noff ) );
		if (pPrimFile) vertices = pPrimFile->readBinary(
			resourceID_.substr( noff+1 ) );
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
	if (vertices)
	{
		DWORD usageFlag = rc().mixedVertexProcessing() ? D3DUSAGE_SOFTWAREPROCESSING : 0;

		// Get the vertex header
		const VertexHeader* vh = reinterpret_cast< const VertexHeader* >( vertices->data() );
		nVertices_ = vh->nVertices;

		_strlwr( const_cast< char* >( vh->vertexFormat ) );

		format_ = vh->vertexFormat;
		// create the right type of vertex
		if (std::string( vh->vertexFormat ) == "xyznuv")
		{
			DX::VertexBuffer* pVertexBuffer;

			// Create the vertex buffer
			if (SUCCEEDED( res = Moo::rc().device()->CreateVertexBuffer( 
					nVertices_ * sizeof( Moo::VertexXYZNUV ), usageFlag, 0, 
					D3DPOOL_MANAGED, &pVertexBuffer, NULL ) ))
			{
				
				// Set up the smartpointer to the vertex buffer
				vertexBuffer_.pComObject( pVertexBuffer );

				// Decrement the reference count, because we only want the smart pointer to reference the buffer.
				pVertexBuffer->Release( );

				// Get the vertices.
				const Moo::VertexXYZNUV* pSrcVertices = reinterpret_cast< const Moo::VertexXYZNUV* >( vh + 1 );
				Moo::VertexXYZNUV* pVertices;
				
				copyVertexPositions( vertexPositions_, pSrcVertices, nVertices_ );
#ifdef DEBUG_NORMALS
				copyVertexNormals2( vertexNormals2_, pSrcVertices, nVertices_ );
#endif //DEBUG_NORMALS

				// Try to lock the vertexbuffer.
				if (SUCCEEDED( res = vertexBuffer_->Lock( 0, nVertices_*sizeof(*pVertices), (void**)&pVertices, 0 ) ))
				{
					// Fill the vertexbuffer.
					memcpy( pVertices, pSrcVertices, sizeof( Moo::VertexXYZNUV ) * nVertices_ );

					vertexBuffer_->Unlock();
				}
				vertexStride_ = sizeof( Moo::VertexXYZNUV );
			}
		}
		else if (std::string( vh->vertexFormat ) == "xyznduv")
		{
			DX::VertexBuffer* pVertexBuffer;

			// Create the vertex buffer
			if (SUCCEEDED( res = Moo::rc().device()->CreateVertexBuffer( 
					nVertices_ * sizeof( Moo::VertexXYZNDUV ), usageFlag, 0, 
					D3DPOOL_MANAGED, &pVertexBuffer, NULL ) ))
			{
				
				// Set up the smartpointer to the vertex buffer
				vertexBuffer_.pComObject( pVertexBuffer );

				// Decrement the reference count, because we only want the smart pointer to reference the buffer.
				pVertexBuffer->Release( );

				// Get the vertices.
				const Moo::VertexXYZNDUV* pSrcVertices = reinterpret_cast< const Moo::VertexXYZNDUV* >( vh + 1 );
				Moo::VertexXYZNDUV* pVertices;
				
				copyVertexPositions( vertexPositions_, pSrcVertices, nVertices_ );
#ifdef DEBUG_NORMALS
				copyVertexNormals2( vertexNormals2_, pSrcVertices, nVertices_ );
#endif //DEBUG_NORMALS

				// Try to lock the vertexbuffer.
				if (SUCCEEDED( res = vertexBuffer_->Lock( 0, nVertices_*sizeof(*pVertices), (void**)&pVertices, 0 ) ))
				{
					// Fill the vertexbuffer.
					memcpy( pVertices, pSrcVertices, sizeof( Moo::VertexXYZNDUV ) * nVertices_ );

					vertexBuffer_->Unlock();
				}
				vertexStride_ = sizeof( Moo::VertexXYZNDUV );
			}
		}
		else if (std::string( vh->vertexFormat ) == "xyznuvtb")
		{
			DX::VertexBuffer* pVertexBuffer;

			// Create the vertex buffer
			if (SUCCEEDED( res = Moo::rc().device()->CreateVertexBuffer( nVertices_ * sizeof( Moo::VertexXYZNUVTBPC ), usageFlag, 0, 
					D3DPOOL_MANAGED, &pVertexBuffer, NULL ) ))
			{
				
				// Set up the smartpointer to the vertex buffer
				vertexBuffer_.pComObject( pVertexBuffer );

				// Decrement the reference count, because we only want the smart pointer to reference the buffer.
				pVertexBuffer->Release( );

				// Get the vertices.
				const Moo::VertexXYZNUVTB* pSrcVertices = reinterpret_cast< const Moo::VertexXYZNUVTB* >( vh + 1 );
				Moo::VertexXYZNUVTBPC* pVertices;

				copyVertexPositions( vertexPositions_, pSrcVertices, nVertices_ );
#ifdef DEBUG_NORMALS
				copyTangentSpace( vertexNormals_, pSrcVertices, nVertices_ );
#endif //DEBUG_NORMALS

				// Try to lock the vertexbuffer.
				if (SUCCEEDED( res = vertexBuffer_->Lock( 0, nVertices_*sizeof(*pVertices), (void**)&pVertices, 0 ) ))
				{
					// Fill the vertexbuffer.
					for (uint32 i = 0; i < nVertices_; i++)
					{
						pVertices[i] = pSrcVertices[i];
					}

					vertexBuffer_->Unlock();
				}
				vertexStride_ = sizeof( Moo::VertexXYZNUVTBPC );
			}
		}
		else if (std::string( vh->vertexFormat ) == "xyznuviiiww")
		{
			DX::VertexBuffer* pVertexBuffer;

			// Create the vertex buffer
			if (SUCCEEDED( res = Moo::rc().device()->CreateVertexBuffer( nVertices_ * sizeof( Moo::VertexXYZNUVIIIWWPC ), usageFlag, 0, 
					D3DPOOL_MANAGED, &pVertexBuffer, NULL ) ))
			{
				// Set up the smartpointer to the vertex buffer
				vertexBuffer_.pComObject( pVertexBuffer );

				// Decrement the reference count, because we only want the smart pointer to reference the buffer.
				pVertexBuffer->Release( );

				// Get the vertices.
				ScopedVertexArray<Moo::VertexXYZNUVIIIWW> sva;
				Moo::VertexXYZNUVIIIWW* pSrcVertices = sva.init(
					reinterpret_cast< const Moo::VertexXYZNUVIIIWW* >( vh + 1 ), nVertices_ );
				Moo::VertexXYZNUVIIIWWPC* pVertices;

				copyVertexPositions( vertexPositions_, pSrcVertices, nVertices_ );
#ifdef DEBUG_NORMALS				
				copyVertexNormals( vertexNormals3_, pSrcVertices, nVertices_ );
#endif //DEBUG_NORMALS
				if ( !verifyIndices3<VertexXYZNUVIIIWW>( pSrcVertices, nVertices_ ) )
				{
					ERROR_MSG( "Moo::Vertices::load: Vertices in %s contain invalid bone indices\n", resourceID_.c_str() );
				}
				SoftwareSkinner< SoftSkinVertex >* pSkinner = new SoftwareSkinner< SoftSkinVertex >;
				pSkinner->init( pSrcVertices, nVertices_ );
				pSoftwareSkinner_ = pSkinner;
                
				// Try to lock the vertexbuffer.
				if (SUCCEEDED( res = vertexBuffer_->Lock( 0, nVertices_*sizeof(*pVertices), (void**)&pVertices, 0 ) ))
				{
					// Fill the vertexbuffer.
					for (uint32 i = 0; i < nVertices_; i++)
					{
						pVertices[i] = pSrcVertices[i];
					}
					vertexBuffer_->Unlock();
				}
				vertexStride_ = sizeof( Moo::VertexXYZNUVIIIWWPC );
			}
		}
		else if (std::string( vh->vertexFormat ) == "xyznuviiiwwtb")
		{
			DX::VertexBuffer* pVertexBuffer;

			// Create the vertex buffer
			if (SUCCEEDED( res = Moo::rc().device()->CreateVertexBuffer( nVertices_ * sizeof( Moo::VertexXYZNUVIIIWWTBPC ), usageFlag, 0, 
					D3DPOOL_MANAGED, &pVertexBuffer, NULL ) ))
			{
				
				// Set up the smartpointer to the vertex buffer
				vertexBuffer_.pComObject( pVertexBuffer );

				// Decrement the reference count, because we only want the smart pointer to reference the buffer.
				pVertexBuffer->Release( );

				// Get the vertices.
				ScopedVertexArray<Moo::VertexXYZNUVIIIWWTB> sva;
				Moo::VertexXYZNUVIIIWWTB* pSrcVertices = sva.init(
					reinterpret_cast< const Moo::VertexXYZNUVIIIWWTB* >( vh + 1 ), nVertices_ );
				Moo::VertexXYZNUVIIIWWTBPC* pVertices;

				copyVertexPositions( vertexPositions_, pSrcVertices, nVertices_ );
#ifdef DEBUG_NORMALS				
				copyTangentSpace( vertexNormals_, pSrcVertices, nVertices_ );
#endif //DEBUG_NORMALS

				if ( !verifyIndices3<VertexXYZNUVIIIWWTB>( pSrcVertices, nVertices_ ) )
				{
					ERROR_MSG( "Moo::Vertices::load: Vertices in %s contain invalid bone indices\n", resourceID_.c_str() );
				}
				SoftwareSkinner< SoftSkinBumpVertex >* pSkinner = new SoftwareSkinner< SoftSkinBumpVertex >;
				pSkinner->init( pSrcVertices, nVertices_ );
				pSoftwareSkinner_ = pSkinner;

				// Try to lock the vertexbuffer.
				if (SUCCEEDED( res = vertexBuffer_->Lock( 0, nVertices_*sizeof(*pVertices), (void**)&pVertices, 0 ) ))
				{
					// Fill the vertexbuffer.
					for (uint32 i = 0; i < nVertices_; i++)
					{
						pVertices[i] = pSrcVertices[i];
					}
					vertexBuffer_->Unlock();
				}
				vertexStride_ = sizeof( Moo::VertexXYZNUVIIIWWTBPC );
			}
		}
		else if (std::string( vh->vertexFormat ) == "xyznuvitb")
		{
			DX::VertexBuffer* pVertexBuffer;

			// Create the vertex buffer
			if (SUCCEEDED( res = Moo::rc().device()->CreateVertexBuffer( nVertices_ * sizeof( Moo::VertexXYZNUVITBPC ), usageFlag, 0, 
					D3DPOOL_MANAGED, &pVertexBuffer, NULL ) ))
			{
				
				// Set up the smartpointer to the vertex buffer
				vertexBuffer_.pComObject( pVertexBuffer );

				// Decrement the reference count, because we only want the smart pointer to reference the buffer.
				pVertexBuffer->Release( );

				// Get the vertices.
				ScopedVertexArray<Moo::VertexXYZNUVITB> sva;
				Moo::VertexXYZNUVITB* pSrcVertices = sva.init(
					reinterpret_cast< const Moo::VertexXYZNUVITB* >( vh + 1 ), nVertices_ );
				Moo::VertexXYZNUVITBPC* pVertices;

				copyVertexPositions( vertexPositions_, pSrcVertices, nVertices_ );
				if ( !verifyIndices1<VertexXYZNUVITB>( pSrcVertices, nVertices_ ) )
				{
					ERROR_MSG( "Moo::Vertices::load: Vertices in %s contain invalid bone indices\n", resourceID_.c_str() );
				}
				SoftwareSkinner< RigidSkinBumpVertex >* pSkinner = new SoftwareSkinner< RigidSkinBumpVertex >;
				pSkinner->init( pSrcVertices, nVertices_ );
				pSoftwareSkinner_ = pSkinner;

				// Try to lock the vertexbuffer.
				if (SUCCEEDED( res = vertexBuffer_->Lock( 0, nVertices_*sizeof(*pVertices), (void**)&pVertices, 0 ) ))
				{
					// Fill the vertexbuffer.
					for (uint32 i = 0; i < nVertices_; i++)
					{
						
						pVertices[i] = pSrcVertices[i];
					}

					vertexBuffer_->Unlock();
				}
				vertexStride_ = sizeof( Moo::VertexXYZNUVITBPC );
			}
		}
		else if (std::string( vh->vertexFormat ) == "xyznuvi")
		{
			DX::VertexBuffer* pVertexBuffer;

			// Create the vertex buffer
			if (SUCCEEDED( res = Moo::rc().device()->CreateVertexBuffer( nVertices_ * sizeof( Moo::VertexXYZNUVI ), usageFlag, 0, 
					D3DPOOL_MANAGED, &pVertexBuffer, NULL ) ))
			{
				
				// Set up the smartpointer to the vertex buffer
				vertexBuffer_.pComObject( pVertexBuffer );

				// Decrement the reference count, because we only want the smart pointer to reference the buffer.
				pVertexBuffer->Release( );

				// Get the vertices.
				ScopedVertexArray<Moo::VertexXYZNUVI> sva;
				Moo::VertexXYZNUVI* pSrcVertices = sva.init(
					reinterpret_cast< const Moo::VertexXYZNUVI* >( vh + 1 ), nVertices_ );
				Moo::VertexXYZNUVI* pVertices;

				copyVertexPositions( vertexPositions_, pSrcVertices, nVertices_ );
				
				// PCWJ - removed index verification check for Mesh Particle System type vertices.  Nobody should
				// be using this vertex format any more except for mesh particles.
				//if ( !verifyIndices1<VertexXYZNUVI>( pSrcVertices, nVertices_ ) )
				//{
				//	ERROR_MSG( "Moo::Vertices::load: Vertices in %s contain invalid bone indices\n", resourceID_.c_str() );
				//}
				SoftwareSkinner< RigidSkinVertex >* pSkinner = new SoftwareSkinner< RigidSkinVertex >;
				pSkinner->init( pSrcVertices, nVertices_ );
				pSoftwareSkinner_ = pSkinner;

				// Try to lock the vertexbuffer.
				if (SUCCEEDED( res = vertexBuffer_->Lock( 0, nVertices_*sizeof(*pVertices), (void**)&pVertices, 0 ) ))
				{
					// Fill the vertexbuffer.
					memcpy( pVertices, pSrcVertices, sizeof( Moo::VertexXYZNUVI ) * nVertices_ );
					vertexBuffer_->Unlock();
				}
				vertexStride_ = sizeof( Moo::VertexXYZNUVI );
			}
		}
		else
		{
			ERROR_MSG( "Failed to recognise vertex format: %s\n", vh->vertexFormat );
		}
		
		pDecl_ = VertexDeclaration::get( vh->vertexFormat );
		pStaticDecl_ = VertexDeclaration::get( std::string(vh->vertexFormat) + std::string("_d") );
	}
	else
	{
		ERROR_MSG( "Failed to read binary resource: %s\n", resourceID_.c_str() );
	}

	return res;
}

HRESULT Vertices::release( )
{
	vertexBuffer_ = NULL;
	nVertices_ = 0;
	return S_OK;
}

HRESULT Vertices::setTransformedVertices( bool tb, const NodePtrVector& nodes )
{
	if (pSoftwareSkinner_)
	{
		if (tb)
		{
			static VertexDeclaration* pDecl = VertexDeclaration::get( "xyznuvtb" );
			if (!pSkinnerVertexBuffer_ || vbBumped_ == false)
			{
				Moo::rc().setVertexDeclaration( pDecl->declaration() );

				//DynamicVertexBuffer
				Moo::DynamicVertexBufferBase2<VertexXYZNUVTBPC>& vb = Moo::DynamicVertexBufferBase2<VertexXYZNUVTBPC>::instance();
				VertexXYZNUVTBPC* pVerts = vb.lock( nVertices_ );
				pSoftwareSkinner_->transformVertices( pVerts, 0, nVertices_, nodes );
				vb.unlock();				
				pSkinnerVertexBuffer_ = vb.pVertexBuffer();
				vbBumped_ = true;
			}
			return Moo::rc().device()->SetStreamSource( 0, pSkinnerVertexBuffer_, 0, sizeof( VertexXYZNUVTBPC ) );
		}
		else
		{
			if (!pSkinnerVertexBuffer_ || vbBumped_ == true)
			{
				Moo::rc().setFVF( D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1 );

				//DynamicVertexBuffer
				Moo::DynamicVertexBufferBase2<VertexXYZNUV>& vb = Moo::DynamicVertexBufferBase2<VertexXYZNUV>::instance();
				VertexXYZNUV* pVerts = vb.lock( nVertices_ );
				pSoftwareSkinner_->transformVertices( pVerts, 0, nVertices_, nodes );
				vb.unlock();
				pSkinnerVertexBuffer_ = vb.pVertexBuffer();
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

HRESULT Vertices::setVertices( bool software, bool staticLighting )
{
	HRESULT hr = E_FAIL;

	// Does our vertexbuffer exist?
	if (vertexBuffer_.pComObject() == NULL)
	{
		// If not load it
		hr = load();

		if (FAILED( hr ))
			return hr;
	}

	// Choose a vertex declaration to use
	Moo::VertexDeclaration * d = pDecl_;

	if (staticLighting && pStaticDecl_)
	{
		d = pStaticDecl_;
	}

	// Set up the vertex declaration
	hr = Moo::rc().setVertexDeclaration( d->declaration() );
	
	if ( SUCCEEDED( hr ) )
	{
		// Set up the stream source(s).
		hr = Moo::rc().device()->SetStreamSource( 0, vertexBuffer_.pComObject(), 
													0, vertexStride_ );
	}
	
	return hr;
}


std::ostream& operator<<(std::ostream& o, const Vertices& t)
{
	o << "Vertices\n";
	return o;
}

}
