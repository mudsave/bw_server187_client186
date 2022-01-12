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

#include "encryption_filter.hpp"
#include "packet.hpp"
#include "basictypes.hpp"

#include "cstdmf/profile.hpp"

#include "openssl/rand.h"

namespace Mercury
{

ProfileVal * EncryptionFilter::s_pProfileVal_ = NULL;

/**
 *  Create an encryption filter using the supplied key.
 */
EncryptionFilter::EncryptionFilter( const Key & key ) :
	key_( key ),
	keySize_( key.size() )
{
	if (this->initKey())
	{
		DEBUG_MSG( "Using Blowfish key: %s\n", this->readableKey() );
	}
}


/**
 *  Create an encryption filter with a randomly generated key.
 */
EncryptionFilter::EncryptionFilter( int keySize ) :
	key_( keySize, 0 ),
	keySize_( keySize )
{
	// Use OpenSSL's RAND function to generate random data.
	char * keyBytes = const_cast< char * >( key_.c_str() );
	RAND_bytes( (unsigned char*)keyBytes, key_.size() );

	if (this->initKey())
	{
		DEBUG_MSG( "Generated Blowfish key: %s\n", this->readableKey() );
	}
}


/**
 *  Destructor.
 */
EncryptionFilter::~EncryptionFilter()
{
	delete this->pBFKey();
}


/**
 *  This method initialises the BF_KEY struct from key_ and checks its validity.
 */
bool EncryptionFilter::initKey()
{
#if ENABLE_WATCHERS
// 	// Initialise the profiling object if it doesn't already exist.
// 	if (s_pProfileVal_ == NULL)
// 	{
// 		s_pProfileVal_ = new ProfileVal( "encrypt" );
// 	}
#endif
	// The default implementation of encrypt() and decrypt() relies on the block
	// size being 64-bit (since it uses uint64's for the XOR operation).  If a
	// larger block size is used, the implementation of the XOR operation needs
	// to be modified and this assertion can be changed.
	MF_ASSERT( BLOCK_SIZE == sizeof( uint64 ) );

	// Allocate a BF_KEY object
	pEncryptionObject_ = new BF_KEY;

	if ((MIN_KEY_SIZE <= keySize_) && (keySize_ <= MAX_KEY_SIZE))
	{
		BF_set_key( this->pBFKey(), key_.size(), (unsigned char*)key_.c_str() );
		isGood_ = true;
	}
	else
	{
		ERROR_MSG( "EncryptionFilter::initKey: "
			"Tried to initialise filter with key of invalid length %d\n",
			keySize_ );

		isGood_ = false;
	}

	return isGood_;
}


/**
 *  This method encrypts the packet and sends it to the provided address.
 */
Reason EncryptionFilter::send( Nub & nub, const Address & addr, Packet * pPacket )
{
	int len = 0;
	PacketPtr toSend = NULL;
	uint8 wastage = 0;

	{
// 		SCOPED_PROFILER( *s_pProfileVal_ );

		// Bail if this filter is invalid.
		if (!isGood_)
		{
			WARNING_MSG( "EncryptionFilter::send: "
				"Dropping packet to %s due to invalid filter\n",
				addr.c_str() );

			return REASON_GENERAL_NETWORK;
		}

		// Grab a new packet.  Remember we have to leave the packet in its
		// original state so we can't just modify its data in-place.
		toSend = new Packet();

		// Work out the number of pad bytes required to make the data size a
		// multiple of the key size (required for most block ciphers).  The
		// magic +1 is for the wastage count that we must write to the end of
		// the packet so the receiver can figure out how big the unencrypted
		// stream is.
		len = pPacket->totalSize();
		wastage = ((BLOCK_SIZE - ((len + 1) % BLOCK_SIZE)) % BLOCK_SIZE) + 1;

		// Grow input packet to be the required size for block encryption, and
		// set up the output packet to match.  We'll undo this later.
		pPacket->grow( wastage );
		toSend->msgEndOffset( pPacket->totalSize() );

		// Write the wastage count into the last byte of the input packet.
		// Since wastage >= 1, we are not writing over any of the original data.
		pPacket->data()[ pPacket->totalSize() - 1 ] = wastage;

		this->encrypt( (const unsigned char*)pPacket->data(),
			(unsigned char*)toSend->data(), pPacket->totalSize() );
	}

	Reason ret = this->PacketFilter::send( nub, addr, toSend.getObject() );

	// Restore the input packet to its original length
	pPacket->shrink( wastage );

	return ret;
}


/**
 *	This method processes an incoming encrypted packet.
 */
Reason EncryptionFilter::recv( Nub & nub, const Address & addr, Packet * pPacket )
{
	{
// 		SCOPED_PROFILER( *s_pProfileVal_ );

		// Bail if this filter is invalid.
		if (!isGood_)
		{
			WARNING_MSG( "EncryptionFilter::recv: "
				"Dropping packet from %s due to invalid filter\n",
				addr.c_str() );

			return REASON_GENERAL_NETWORK;
		}

		// Decrypt the data in place.  This is fine for Blowfish, but may not be
		// safe for other encryption algorithms.
		int decryptLen = this->decrypt( (const unsigned char*)pPacket->data(),
			(unsigned char*)pPacket->data(), pPacket->totalSize() );

		if (decryptLen == -1)
		{
			return REASON_CORRUPTED_PACKET;
		}

		// Read the wastage amount
		uint8 wastage = pPacket->data()[ pPacket->totalSize() - 1 ];

		// Sanity check the wastage
		if (wastage > BLOCK_SIZE || wastage > pPacket->totalSize())
		{
			WARNING_MSG( "EncryptionFilter::recv: "
				"Dropping packet from %s due to illegal wastage count (%d)\n",
				addr.c_str(), wastage );

			return REASON_CORRUPTED_PACKET;
		}

		// Set the packet length correctly
		pPacket->shrink( wastage );
	}

	return this->PacketFilter::recv( nub, addr, pPacket );
}


/**
 *  This method encrypts the data in the input stream and writes it to the
 *  output stream.  The reason it takes a MemoryOStream as the input parameter
 *  instead of a BinaryIStream like you'd expect is that it pads the input
 *  stream out to the correct size in place.
 */
void EncryptionFilter::encryptStream( MemoryOStream & clearStream,
	BinaryOStream & cipherStream )
{
	// Pad the input stream to the required size
	if (clearStream.remainingLength() % this->maxSpareSize() != 0)
	{
		int padSize = this->maxSpareSize() -
						(clearStream.remainingLength() % this->maxSpareSize());
		void * padding = clearStream.reserve( padSize );
		memset( padding, 0, padSize );
	}

	int size = clearStream.remainingLength();
	unsigned char * cipherText = (unsigned char*)cipherStream.reserve( size );

	this->encrypt( (unsigned char*)clearStream.data(), cipherText, size );
}


/**
 *  This method decrypts data from the input stream and writes it to the output
 *  stream.  Note that the data may well have been padded when it was encrypted
 *  and you may have dangling bytes left over.
 */
void EncryptionFilter::decryptStream( BinaryIStream & cipherStream,
	BinaryOStream & clearStream )
{
	int size = cipherStream.remainingLength();
	unsigned char * cipherText = (unsigned char*)cipherStream.retrieve( size );
	unsigned char * clearText =	(unsigned char*)clearStream.reserve( size );

	this->decrypt( cipherText, clearText, size );
}


/**
 *  This method returns the number of extra bytes that might be required when
 *  sending through this filter.
 */
int EncryptionFilter::maxSpareSize()
{
	// This is an allowance for networks that cannot handle "full-sized" UDP
	// packets. This can happen, for example, on a VPN at has some overhead per
	// packet. Ideally, this should be calculated dynamically per connection.
	const int MTU_ALLOWANCE = 200;

	// Might need BLOCK_SIZE more bytes when (packetlen % BLOCK_SIZE == 1)
	return BLOCK_SIZE + MTU_ALLOWANCE;
}


/**
 *  Returns a human readable representation of the key.
 */
const char * EncryptionFilter::readableKey() const
{
	static char buf[ 1024 ];

	char *c = buf;

	for (int i=0; i < keySize_; i++)
	{
		c += sprintf( c, "%02hhX ", key_[i] );
	}

	c[-1] = '\0';

	return buf;
}


/**
 *  This method encrypts the provided data.
 */
int EncryptionFilter::encrypt( unsigned const char * src, unsigned char * dest,
	int length )
{
	if (length % BLOCK_SIZE != 0)
	{
		CRITICAL_MSG( "EncryptionFilter::encrypt: "
			"Input length (%d) is not a multiple of block size (%d)\n",
			length, BLOCK_SIZE );
	}

	// We XOR each block after the first with the previous one, prior to
	// encryption.  This prevents people reassembling blocks from different
	// packets into new ones, i.e. it prevents replay attacks.
	uint64 * pPrevBlock = NULL;

	for (int i=0; i < length; i += BLOCK_SIZE)
	{
		if (pPrevBlock)
		{
			*(uint64*)(dest + i) = *(uint64*)(src + i) ^ (*pPrevBlock);
		}
		else
		{
			*(uint64*)(dest + i) = *(uint64*)(src + i);
		}

		BF_ecb_encrypt( dest + i, dest + i, this->pBFKey(), BF_ENCRYPT );
		pPrevBlock = (uint64*)(src + i);
	}

	return length;
}


/**
 *  This method decrypts the provided data.
 */
int EncryptionFilter::decrypt( const unsigned char * src, unsigned char * dest,
	int length )
{
	// If the packet length is not an exact multiple of the key length, abort.
	if (length % BLOCK_SIZE != 0)
	{
		WARNING_MSG( "EncryptionFilter::decrypt: "
			"Input stream size (%d) is not a multiple of the block size (%d)\n",
			length, BLOCK_SIZE );

		return -1;
	}

	// Inverse of the XOR logic in encrypt() above.
	uint64 * pPrevBlock = NULL;

	for (int i=0; i < length; i += BLOCK_SIZE)
	{
		BF_ecb_encrypt( src + i, dest + i, this->pBFKey(), BF_DECRYPT );

		if (pPrevBlock)
		{
			*(uint64*)(dest + i) ^= *pPrevBlock;
		}

		pPrevBlock = (uint64*)(dest + i);
	}

	return length;
}


} // namespace Mercury
