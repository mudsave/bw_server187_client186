/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef INDEX_BUFFER_HPP
#define INDEX_BUFFER_HPP

#include "render_context.hpp"

namespace Moo
{

class IndicesReference
{
protected:
	void* indices_;
	unsigned int size_;
	D3DFORMAT format_;
public:
	IndicesReference() : size_( 0 ), indices_( 0 ), format_( D3DFMT_INDEX16 )
	{}
	virtual ~IndicesReference(){}
	virtual void assign( void* buffer, unsigned int numOfIndices, D3DFORMAT format )
	{
		size_ = numOfIndices;
		indices_ = buffer;
		format_ = format;
	}
	bool valid() const
	{
		return !!indices_;
	}
	void fill( const void* buffer, unsigned int numOfIndices )
	{
		memcpy( indices_, buffer, numOfIndices * entrySize() );
	}
	void pull( void* buffer, unsigned int numOfIndices ) const
	{
		memcpy( buffer, indices_, numOfIndices * entrySize() );
	}
	void copy( const IndicesReference& src, int count, int start = 0, int srcStart = 0, unsigned int vertexBase = 0 )
	{
		for( int i = 0; i < count; i++ )
			set( start + i, src[ srcStart + i ] + vertexBase );
	}
	D3DFORMAT format() const
	{
		return format_;
	}
	int entrySize() const
	{
		return bestEntrySize( format_ );
	}
	const void* indices() const
	{
		return indices_;
	}
	unsigned int size() const
	{
		return size_;
	}
	unsigned int operator[]( int index ) const
	{
		return format_ == D3DFMT_INDEX16 ? ( (unsigned short*)indices_ )[ index ] :
			( (unsigned int*)indices_ )[ index ];
	}
	void set( int index, unsigned int value )
	{
		if( format_ == D3DFMT_INDEX16 )
			( (unsigned short*)indices_ )[ index ] = value;
		else
			( (unsigned int*)indices_ )[ index ] = value;
		
	}
	static int bestEntrySize( D3DFORMAT format )
	{
		return format == D3DFMT_INDEX16 ? 2 : 4;
	}
	static D3DFORMAT bestFormat( unsigned int vertexNum )
	{
		return vertexNum <= 65536 ? D3DFMT_INDEX16 : D3DFMT_INDEX32;
	}
};

class IndicesHolder : public IndicesReference
{
	IndicesHolder( const IndicesHolder& );
	void operator= ( const IndicesHolder& );
public:
	IndicesHolder( D3DFORMAT format = D3DFMT_INDEX16, unsigned int entryNum = 0 )
	{
		format_ = format;
		size_ = entryNum;
		indices_ = new unsigned char[ size() * entrySize() ];
	}
	~IndicesHolder()
	{
		delete[] indices_;
	}
	void setSize( unsigned int entryNum, D3DFORMAT format )
	{
		delete[] indices_;
		indices_ = 0;
		format_ = format;
		if( entryNum != 0 )
		{
			size_ = entryNum;
			indices_ = new unsigned char[ size() * entrySize() ];
		}
	}
	virtual void assign( const void* buffer, unsigned int entryNum, D3DFORMAT format )
	{// exception unsafe
		delete[] indices_;
		size_ = entryNum;
		format_ = format;
		indices_ = new unsigned char[ size() * entrySize() ];
		memcpy( indices_, buffer, size() * entrySize() );
	}
};

class IndexBuffer
{
	ComObjectWrap<DX::IndexBuffer> indexBuffer_;
	D3DFORMAT format_;
	DWORD numOfIndices_;
public:
	IndexBuffer() : format_( D3DFMT_INDEX16 ), numOfIndices_( 0 )
	{}
	HRESULT create( int numOfIndices, D3DFORMAT format, DWORD usage, D3DPOOL pool )
	{
		HRESULT hr = E_FAIL;
		
		// Fail if we try to create a 32 bit index buffer on a device that
		// does not support it
		if (format == D3DFMT_INDEX32 && 
			rc().maxVertexIndex() <= 0xffff)
			return hr;

		ComObjectWrap<DX::IndexBuffer> temp;

		// Try to create the indexbuffer with maxIndices_ number of indices
		if( SUCCEEDED( hr = Moo::rc().device()->CreateIndexBuffer( 
			numOfIndices * IndicesReference::bestEntrySize( format ),
			usage, format, pool, &temp, NULL ) ) )
		{
			format_ = format;
			numOfIndices_ = numOfIndices;
			indexBuffer_ = temp;
		}
		return hr;
	}
	HRESULT set() const
	{
		return Moo::rc().setIndices( indexBuffer_ );
	}
	bool valid() const
	{
		return indexBuffer_.pComObject() != NULL;
	}
	void release()
	{
		indexBuffer_.pComObject( NULL );
		format_ = D3DFMT_INDEX16;
		numOfIndices_ = 0;
	}
	IndicesReference lock( DWORD flags = 0 )
	{
		IndicesReference result;

		BYTE* indices = NULL;

		// Try to lock the buffer, if we succeed update the return value to use the locked vertices
		if( SUCCEEDED( indexBuffer_.pComObject()->Lock( 0, 0, (void**)&indices, flags ) ) )
			result.assign( indices, numOfIndices_, format_ );
		return result;
	}
	IndicesReference lock( UINT offset, UINT numOfIndices, DWORD flags )
	{
		IndicesReference result;

		BYTE* indices = NULL;

		// Try to lock the buffer, if we succeed update the return value to use the locked vertices
		if( SUCCEEDED( indexBuffer_.pComObject()->Lock(
			offset * IndicesReference::bestEntrySize( format_ ),
			numOfIndices * IndicesReference::bestEntrySize( format_ ), (void**)&indices, flags ) ) )
			result.assign( indices, numOfIndices, format_ );
		return result;
	}
	D3DFORMAT format() const
	{
		return format_;
	}
	bool isCurrent() const
	{
		return ( Moo::rc().getIndices() == indexBuffer_ );
	}
	HRESULT unlock()
	{
		return indexBuffer_.pComObject()->Unlock();
	}
};

};

#endif//INDEX_BUFFER_HPP
