/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef DYNAMIC_VERTEX_BUFFER_HPP
#define DYNAMIC_VERTEX_BUFFER_HPP

#include <iostream>
#include <list>

#include "cstdmf/stdmf.hpp"
#include "moo_dx.hpp"
#include "com_object_wrap.hpp"
#include "device_callback.hpp"

namespace Moo
{

/**
 *	This class implements the common elements of all dynamic vertex buffers.
 */
class DynamicVertexBufferBase : public DeviceCallback
{
public:
	~DynamicVertexBufferBase();

	HRESULT						unlock( );
	HRESULT						create( );

	DX::VertexBuffer*			pVertexBuffer( );

	static void					releaseAll( );
	static void					resetLockAll( );

protected:

	DynamicVertexBufferBase();
	uint32						lockIndex_;
	int							vertexSize_;
	bool						lockModeDiscard_;
	bool						lockFromStart_;
	bool						softwareBuffer_;
	bool						readOnly_;
	BYTE*						lock( uint32 nLockElements );
	BYTE*						lock2( uint32 nLockElements );

	void						release( );
	void						resetLock( );

	void						deleteUnmanagedObjects( );

private:
	ComObjectWrap< DX::VertexBuffer > vertexBuffer_;

	bool						reset_;
	bool						locked_;
	int							lockBase_;
	uint						maxElements_;

	DynamicVertexBufferBase(const DynamicVertexBufferBase&);
	DynamicVertexBufferBase& operator=(const DynamicVertexBufferBase&);

	friend std::ostream& operator<<(std::ostream&, const DynamicVertexBufferBase&);

	static std::list< DynamicVertexBufferBase* > dynamicVBS_;
};



/**
 *	This templetised class implements dynamic vertex buffers stored in video memory.
 *	It always tries to grow the vertex buffer to the size passed into lock or lock2.
 */
template <class VertexType>
class DynamicVertexBufferBase2 : public DynamicVertexBufferBase
{
public:
	typedef VertexType Vertex;
	static DynamicVertexBufferBase2<VertexType>&	instance();

	/**
	 *	This method locks, transfers and then unlocks a buffer,
	 *	returning the lock index.
	 *	@param pSrc source vertices.
	 *	@param count vertex count.
	 *  @return lock index
	 */
	uint32 lockAndLoad( const Vertex* pSrc, uint32 count )
	{
		Vertex* pVertex = lock2( count );
		memcpy(pVertex, pSrc, count * sizeof(Vertex));
		unlock();							
		return lockIndex();
	}

	/**
	 *	This method returns a pointer to the start of a vertex buffer
	 *	big enough to contain nLockElements vertices.
	 *	@param nLockElements the number of vertices to allocate
	 *	@return pointer to the vertices that have been allocated.
	 */
	Vertex*	lock( uint32 nLockElements )
	{
		return (Vertex*)DynamicVertexBufferBase::lock( nLockElements );
	}

	/**
	 *	This method returns a pointer to a write only vertex buffer
	 *	big enough to contain nLockElements. These vertices will
	 *	not necessarily be at the start of the vertex buffer. To get
	 *	the index of the first locked vertex use the method lockIndex.
	 *	It implements the best practices locking for dynamic vertex
	 *	buffers.
	 *	@param nLockElements the number of vertices to allocate
	 *	@return pointer to the vertices that have been allocated.
	 */
	Vertex*	lock2( uint32 nLockElements )
	{
		return (Vertex*)DynamicVertexBufferBase::lock2( nLockElements );
	}

	/**
	 *	This method binds the vertex buffer to the specified
	 *	stream.
	 *	@param stream stream id.
	 *  @return result
	 */
	HRESULT set( uint32 stream = 0 )
	{
		return Moo::rc().device()->SetStreamSource(stream,
						pVertexBuffer(), 0, sizeof(Vertex)); 
	}

	HRESULT unset( uint32 stream = 0 )
	{
		return Moo::rc().device()->SetStreamSource(stream, 0, 0, 0);
	}

	/**
	 *	This method returns the lock index for this buffer.
	 *  @return lock index.
	 */
	uint32 lockIndex() const
	{
		return lockIndex_;
	}
};

/**
 *	This templetised class implements dynamic vertex buffers stored in video memory.
 *	It always tries to grow the vertex buffer to the size passed into lock or lock2.
 */
template <class VertexType>
class DynamicVertexBuffer : public DynamicVertexBufferBase2<VertexType>
{
public:
	static DynamicVertexBuffer<VertexType>&	instance();

	~DynamicVertexBuffer()
	{

	}
private:
	DynamicVertexBuffer()
	{
		vertexSize_ = sizeof( Vertex );
		softwareBuffer_ = false;
		lockModeDiscard_ = true;
		lockFromStart_ = true;
		readOnly_ = true;
	}
};

/**
 *	This templetised class implements dynamic vertex buffers stored in system memory.
 *	It always tries to grow the vertex buffer to the size passed into lock or lock2.
 */
template <class VertexType>
class DynamicSoftwareVertexBuffer : public DynamicVertexBufferBase2<VertexType>
{
public:
	static DynamicSoftwareVertexBuffer<VertexType>&	instance();

	~DynamicSoftwareVertexBuffer()
	{

	}
private:
	DynamicSoftwareVertexBuffer()
	{
		vertexSize_ = sizeof( Vertex );
		softwareBuffer_ = true;
		lockModeDiscard_ = true;
		lockFromStart_ = true;
		readOnly_ = false;
	}
};

}

#ifdef CODE_INLINE
#include "dynamic_vertex_buffer.ipp"
#endif


#endif
/*dynamic_vertex_buffer.hpp*/