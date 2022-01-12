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

#include "primitive.hpp"
#include "resmgr/bwresource.hpp"

#include "cstdmf/debug.hpp"

#include "render_context.hpp"
#include "resmgr/primitive_file.hpp"

#ifndef CODE_INLINE
#include "primitive.ipp"
#endif

DECLARE_DEBUG_COMPONENT2( "Moo", 0 )

namespace Moo
{

typedef struct
{
	char	indexFormat[ 64 ];
	int		nIndices;
	int		nTriangleGroups;
}IndexHeader;

Primitive::Primitive( const std::string& resourceID )
: resourceID_( resourceID ),
  maxVertices_( 0 ),
  primType_( D3DPT_TRIANGLELIST ),
  nIndices_( 0 )
{
}

Primitive::~Primitive()
{
	// let the manager know we're gone
	PrimitiveManager::del( this );
}

bool Primitive::verifyIndices( uint32 maxVertex )
{
    bool res = true;

	for (uint32 i = 0; i < primGroups_.size(); i++)
	{
		PrimGroup& pg = primGroups_[i];
		MF_ASSERT( uint32(pg.startVertex_ + pg.nVertices_) <= maxVertex );
		for (uint32 j = 0; j < uint32(pg.nPrimitives_ * 3); j++)
		{
			MF_ASSERT( (pg.startIndex_ + j) < indices_.size() );
			DWORD index = indices_[ pg.startIndex_ + j ];
			MF_ASSERT( index < DWORD(pg.startVertex_ + pg.nVertices_ ) );
			MF_ASSERT( index >= DWORD(pg.startVertex_) );
		}
	}

	return res;
}

HRESULT Primitive::load( )
{
	release();
	HRESULT res = E_FAIL;

	// Is there a valid device pointer?
	if( Moo::rc().device() == (void*)NULL )
	{
		return res;
	}

	// find our data
	BinaryPtr indices;
	uint noff = resourceID_.find( ".primitives/" );
	if (noff < resourceID_.size())
	{
		// everything is normal
		noff += 11;
		DataSectionPtr sec = PrimitiveFile::get( resourceID_.substr( 0, noff ) );
		if ( sec )
			indices = sec->readBinary( resourceID_.substr( noff+1 ) );
	}
	else
	{
		// find out where the data should really be stored
		std::string fileName, partName;
		splitOldPrimitiveName( resourceID_, fileName, partName );

		// read it in from this file
		indices = fetchOldPrimitivePart( fileName, partName );
	}

	// Open the binary resource for this primitive
	//BinaryPtr indices = BWResource::instance().rootSection()->readBinary( resourceID_ );
	if( indices )
	{
		// Get the index header
		const IndexHeader* ih = reinterpret_cast< const IndexHeader* >( indices->data() );

		// Create the right type of primitive
		if( std::string( ih->indexFormat ) == "list" || std::string( ih->indexFormat ) == "list32" )
		{
			D3DFORMAT format = std::string( ih->indexFormat ) == "list" ? D3DFMT_INDEX16 : D3DFMT_INDEX32;
			primType_ = D3DPT_TRIANGLELIST;

			if (Moo::rc().maxVertexIndex() <= 0xffff &&
				format == D3DFMT_INDEX32 )
			{
				ERROR_MSG( "Primitives::load - unable to create index buffer as 32 bit indices "
					"were requested and only 16 bit indices are supported\n" );
				return res;
			}

			DWORD usageFlag = rc().mixedVertexProcessing() ? D3DUSAGE_SOFTWAREPROCESSING : 0;

			// Create the index buffer
			if( SUCCEEDED( res = indexBuffer_.create( ih->nIndices, format, usageFlag, D3DPOOL_MANAGED ) ) )
			{
				// store the number of elements in the index buffer
				nIndices_ = ih->nIndices;

				// Get the indices
				const char* pSrc = reinterpret_cast< const char* >( ih + 1 );

				indices_.assign(pSrc, ih->nIndices, format);

			    IndicesReference ind = indexBuffer_.lock();

				// Fill in the index buffer.
				ind.fill( pSrc, ih->nIndices );
				res = indexBuffer_.unlock( );

				pSrc += ih->nIndices * ind.entrySize();

				// Get the primitive groups
				const PrimGroup* pGroup = reinterpret_cast< const PrimGroup* >( pSrc );
				for( int i = 0; i < ih->nTriangleGroups; i++ )
				{
					primGroups_.push_back( *(pGroup++) );
				}
			
				maxVertices_ = 0;
				
				// Go through the primitive groups to find the number of vertices referenced by the index buffer.
				PrimGroupVector::iterator it = primGroups_.begin();
				PrimGroupVector::iterator end = primGroups_.end();

				while( it != end )
				{
					maxVertices_ = max( (uint32)( it->startVertex_ + it->nVertices_ ), maxVertices_ );
					it++;
				}
			}
		}
	}
	else
	{
		ERROR_MSG( "Failed to read binary resource: %s\n", resourceID_.c_str() );
	}
	return res;
}

HRESULT Primitive::setPrimitives( )
{
	if( !indexBuffer_.valid() )
	{
		HRESULT hr = load();
		if( FAILED ( hr ) )
			return hr;
	}

	MF_ASSERT( indexBuffer_.valid() );
	return indexBuffer_.set();
}

HRESULT Primitive::drawPrimitiveGroup( uint32 groupIndex )
{
#ifdef _DEBUG
	MF_ASSERT( Moo::rc().device() != NULL );
	MF_ASSERT( groupIndex < primGroups_.size() );
	MF_ASSERT( indexBuffer_.isCurrent() );
	if( !indexBuffer_.isCurrent() )
		return E_FAIL;
#endif

	// Draw the primitive group
	PrimGroup& pg = primGroups_[ groupIndex ];

	if( pg.nVertices_ && pg.nPrimitives_ )
	{
		Moo::rc().addToPrimitiveCount( pg.nPrimitives_ );
		return Moo::rc().drawIndexedPrimitive( primType_, 0, pg.startVertex_, pg.nVertices_, pg.startIndex_, pg.nPrimitives_ );
	}
	return S_OK;
}

HRESULT Primitive::release( )
{
	maxVertices_ = 0;
	primGroups_.clear();
	indexBuffer_.release();
	return S_OK;
}

std::ostream& operator<<(std::ostream& o, const Primitive& t)
{
	o << "Primitive\n";
	return o;
}

}

// primitive.cpp
