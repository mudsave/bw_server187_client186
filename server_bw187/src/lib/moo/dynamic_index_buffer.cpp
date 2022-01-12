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

#include "dynamic_index_buffer.hpp"

#include "render_context.hpp"

namespace Moo
{

/**
 *	Constructor.
 */
DynamicIndexBufferBase& DynamicIndexBufferBase::instance(D3DFORMAT format)
{
	if (format == D3DFMT_INDEX16)
	{
		if( Moo::rc().mixedVertexProcessing() )
			return DynamicSoftwareIndexBuffer16::instance();
		return DynamicIndexBuffer16::instance();
	}
	if( Moo::rc().mixedVertexProcessing() )
		return DynamicSoftwareIndexBuffer32::instance();
	return DynamicIndexBuffer32::instance();
}

/**
 *	Constructor.
 *
 *	@param usage		D3D usage flags.
 */
DynamicIndexBufferBase::DynamicIndexBufferBase( DWORD usage, D3DFORMAT format )
:	locked_( false ),
	lockBase_( 0 ),
	lockIndex_( 0 ),
	maxIndices_( 2000 ),
	usage_( usage ),
	reset_( true ),
	format_( format )
{}

/**
 *	Destructor.
 */
DynamicIndexBufferBase::~DynamicIndexBufferBase()
{}

/**
 *	Release the index buffers D3D resources.
 */
void DynamicIndexBufferBase::release()
{
	indexBuffer_.release();
}

/**
 *	Release the index buffers D3D resources.
 */
void DynamicIndexBufferBase::deleteUnmanagedObjects()
{
	release();
}

/**
 *	Flag a reset/recreation of the index buffer.
 */
void DynamicIndexBufferBase::resetLock()
{
	reset_ = true;
}

/**
 *	Lock the index buffer for the required size / format. This method will
 *	not grow the existing size but mearly re-create it.
 *
 *	@param nLockIndices			Number of indices to lock.
 *	@param format				Format to lock this buffer to.
 *	@return A reference to the locked portion of memory representing the buffer.
 */
IndicesReference DynamicIndexBufferBase::lock( uint32 nLockIndices )
{
	MF_ASSERT( nLockIndices != 0 );
	MF_ASSERT( locked_ == false );

	IndicesReference result;
	// Only try to lock if the device is ready.
	if( Moo::rc().device() != NULL )
	{
		bool lockBuffer = true;

		// Can we fit the number of elements into the indexbuffer?
		if( maxIndices_ < nLockIndices )
		{
			// If not release the buffer so it can be recreated with the right size.
			release();
			maxIndices_ = nLockIndices;
		}

		// Do we have a buffer?
		if( !indexBuffer_.valid() )
		{
			// Create a new buffer
			if( FAILED( indexBuffer_.create( maxIndices_, format_, usage_, D3DPOOL_DEFAULT ) ) )
			{
				// Something went wrong, do not try to lock the buffer.
				lockBuffer = false;
			}
		}

		// Should we attempt to lock?
		if( lockBuffer )
		{
			DWORD lockFlags = 0;

			// Can we fit our vertices in the so far unused portion of the indexbuffer?
			if( ( lockBase_ + nLockIndices ) <= maxIndices_ )
			{
				// Lock from the current position
				lockFlags = D3DLOCK_NOOVERWRITE;
			}
			else
			{
				// Lock from the beginning.
				lockFlags = D3DLOCK_DISCARD;
				lockBase_ = 0;
			}

			// Try to lock the buffer, if we succeed update the return value to use the locked vertices
			result = indexBuffer_.lock( lockBase_, nLockIndices, lockFlags );
			if( result.size() )
			{
				lockIndex_ = lockBase_;
				lockBase_ += nLockIndices;
				locked_ = true;
			}
		}
	}
	return result;

}

/**
 *	Lock the index buffer for the required size / format. This method WILL
 *	grow the existing size of the buffer unlike lock(..).
 *
 *	@param nLockIndices			Number of indices to lock.
 *	@param format				Format to lock this buffer to.
 *	@return A reference to the locked portion of memory representing the buffer.
 */
IndicesReference DynamicIndexBufferBase::lock2( uint32 nLockIndices )
{
	MF_ASSERT( nLockIndices != 0 );
	MF_ASSERT( locked_ == false );

	IndicesReference result;
	// Only try to lock if the device is ready.
	if( Moo::rc().device() != NULL )
	{
		bool lockBuffer = true;

		// Can we fit the number of elements into the indexbuffer?
		if( reset_ )
		{
			lockBase_ = 0;
		}
		
		if( maxIndices_ < nLockIndices + lockBase_ )
		{
			int newSize = nLockIndices + lockBase_;

			// If not release the buffer so it can be recreated with the right size.
			release();
			maxIndices_ = newSize;
		}

		// Do we have a buffer?
		if( !indexBuffer_.valid() )
		{
			// Create a new buffer
			if( FAILED( indexBuffer_.create( maxIndices_, format_, usage_, D3DPOOL_DEFAULT ) ) )
			{
				// Something went wrong, do not try to lock the buffer.
				lockBuffer = false;
			}
		}

		// Should we attempt to lock?
		if( lockBuffer )
		{
			DWORD lockFlags = 0;

			// Can we fit our vertices in the sofar unused portion of the indexbuffer?
			if( (!reset_) && (( lockBase_ + nLockIndices ) <= maxIndices_) )
			{
				// Lock from the current position
				lockFlags = D3DLOCK_NOOVERWRITE;


				// Try to lock the buffer, if we succeed update the return value to use the locked vertices
				result = indexBuffer_.lock( lockBase_, nLockIndices, lockFlags );
				if( result.size() )
				{
					lockIndex_ = lockBase_;
					lockBase_ += nLockIndices;
					locked_ = true;
				}
			}
			else
			{

				// Lock from the beginning.
				lockFlags = D3DLOCK_DISCARD;
				lockBase_ = 0;
				reset_ = false;

				// Try to lock the buffer, if we succeed update the return value to use the locked vertices
				//if( SUCCEEDED( vertexBuffer_->Lock( lockBase_ * vertexSize_, nLockElements * vertexSize_, (void**)&vertices, lockFlags ) ) )
				result = indexBuffer_.lock( lockBase_, nLockIndices, lockFlags );
				if( result.size() )
				{
					lockIndex_ = lockBase_;
					lockBase_ += nLockIndices;
					locked_ = true;
				}
			}

			
		}
	}
	return result;
}

/**
 *	Unlock a previously locked index buffer.
 *
 *	@return The result of the lock.
 */
HRESULT DynamicIndexBufferBase::unlock()
{
	MF_ASSERT( locked_ == true );
	locked_ = false;
	return indexBuffer_.unlock();
}

DynamicIndexBuffer16& DynamicIndexBuffer16::instance()
{
	static DynamicIndexBuffer16 instance;
	return instance;
}

DynamicSoftwareIndexBuffer16& DynamicSoftwareIndexBuffer16::instance()
{
	static DynamicSoftwareIndexBuffer16 instance;
	return instance;
}

DynamicIndexBuffer32& DynamicIndexBuffer32::instance()
{
	static DynamicIndexBuffer32 instance;
	return instance;
}

DynamicSoftwareIndexBuffer32& DynamicSoftwareIndexBuffer32::instance()
{
	static DynamicSoftwareIndexBuffer32 instance;
	return instance;
}

}

/*dynamic_index_buffer.cpp*/
