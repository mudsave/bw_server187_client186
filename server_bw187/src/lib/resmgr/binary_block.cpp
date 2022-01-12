/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

/**
 * binary_block.cpp
 */

#include "pch.hpp"

#include "binary_block.hpp"
#include "cstdmf/debug.hpp"
#include "cstdmf/memory_counter.hpp"

DECLARE_DEBUG_COMPONENT2( "ResMgr", 0 )

memoryCounterDefineWithAlias( binaryBlock, Entity, "BinaryBlkSize" );


/**
 *	Constructor.
 *
 *	@param data		Pointer to a block of data
 *	@param len		Size of the data block
 *	@param pOwner	A smart pointer to the owner of this data. If not NULL, a reference
 *					is kept to the owner, otherwise a copy of the data is taken.
 */
BinaryBlock::BinaryBlock( const void * data, int len, BinaryPtr pOwner ) :
	len_( len ),
	pOwner_( pOwner )
{
	memoryCounterAdd( binaryBlock );
	memoryClaim( this );

	// if we're the owner of this block, then make a copy of it
	if (!pOwner)
	{
		data_ = new char[len];
		memoryClaim( data_ );
		//dprintf( "Make BB of size %d at 0x%08X\n", len_, this );

		if(data_ != NULL)
		{
			if (data != NULL)
			{
				memcpy(data_, data, len);
			}
			else
			{
				memset(data_, 0, len);
			}
		}
		else
		{
			WARNING_MSG( "BinaryBlock: Failed to alloc %d bytes", len );
			len_ = 0;
			return;
		}
	}
	else
	{
		data_ = const_cast<void*>( data );
	}
}

/**
 *	Constructor. Initialises binary block from stream content.
 *
 *	@param stream	Stream to get data from. 
 *	@param len		Size of the data block
 */
BinaryBlock::BinaryBlock( std::istream& stream, std::streamsize len ) :
	len_( len ),
	pOwner_( NULL )
{
	memoryCounterAdd( binaryBlock );
	memoryClaim( this );

	data_ = new char[len_];
	char * data = (char *) data_;

	std::streampos startPos = stream.tellg();
	std::streampos endPos = stream.seekg( 0, std::ios::end ).tellg();
	std::streamsize numAvail = endPos - startPos;
	
	stream.seekg(startPos);
	stream.read( data, std::min( numAvail, len_ ) );

	std::streamsize numRead = stream.gcount();

	if( numRead < len_ )
		memset( data + numRead, 0, len_ - numRead );

	len_ = numRead;
}

/**
 * Destructor
 */
BinaryBlock::~BinaryBlock()
{
	memoryCounterSub( binaryBlock );

	// if we made a copy of this block, then delete it
	if (!pOwner_)
	{
		memoryClaim( data_ );
		delete [] (char *)data_;
		//dprintf( "Free BB of size %d at 0x%08X\n", len_, this );
	}

	// otherwise simply let our pOwner smartpointer lapse and it'll
	// reduce the reference count on the actual owner (which may delete it)

	memoryClaim( this );
}
