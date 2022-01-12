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
 * binary_block.hpp
 */

#ifndef _BINARY_BLOCK_HEADER
#define _BINARY_BLOCK_HEADER

#include "cstdmf/smartpointer.hpp"

#include <iostream>

class BinaryBlock;
typedef SmartPointer<BinaryBlock> BinaryPtr;

/**
 * This class is a simple wrapper around a block of binary data. It is designed
 * to be used so that blocks of memory can be reference counted and passed
 * around with smart pointers.
 */
class BinaryBlock : public ReferenceCount
{
public:
	BinaryBlock( const void* data, int len, BinaryPtr pOwner = 0 );
	BinaryBlock( std::istream& stream, std::streamsize len );

	~BinaryBlock();

	/// This method returns a pointer to the block of binary data
	const void *	data() const	{ return data_; }
	char *			cdata()			{ return (char *)data_; }

	/// This method returns the length of the binary data
	int				len() const		{ return len_; }

	BinaryPtr		pOwner()		{ return pOwner_; }

private:

	void*			data_;
	int				len_;
	BinaryPtr		pOwner_;

private:
	BinaryBlock(const BinaryBlock&);
	BinaryBlock& operator=(const BinaryBlock&);
};

/**
 *	Implements a streambuf that reads a binary data section.
 */
class BinaryInputBuffer : public std::streambuf
{
public:
	BinaryInputBuffer(BinaryPtr data) :
		data_(data)
	{
		this->std::streambuf::setg(
			this->data_->cdata(), this->data_->cdata(), 
			this->data_->cdata() + this->data_->len());
	}
	
private:
	BinaryPtr data_;
};

#endif
