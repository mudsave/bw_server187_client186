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

#include "public_key_cipher.hpp"

#include "openssl/err.h"
#include "openssl/rand.h"


// See RSA_public_encrypt(3) for the origin of the magic 41.
// RSA_PKCS1_OAEP_PADDING padding mode is used.
#define	RSA_PADDING	41	


namespace Mercury
{

// -----------------------------------------------------------------------------
// Section: PublicKeyCipher
// -----------------------------------------------------------------------------

/**
 *  Sets the key to be used by this object.
 */
bool PublicKeyCipher::setKey( const std::string & key )
{
	this->cleanup();

	// Construct an in-memory BIO to the key data
	BIO *bio = BIO_new( BIO_s_mem() );
	BIO_puts( bio, key.c_str() );

	// Read the key into the RSA struct
	pRSA_ = hasPrivate_ ?
		PEM_read_bio_RSAPrivateKey( bio, NULL, NULL, NULL ) :
		PEM_read_bio_RSA_PUBKEY( bio, NULL, NULL, NULL );

	BIO_free( bio );

	if (pRSA_)
	{
		this->setReadableKey();
		
		INFO_MSG( "PublicKeyCipher::setKey: "
			"Loaded %d-bit %s key\n",
			this->numBits(), this->type() );
		
		return true;
	}
	else
	{
		ERROR_MSG( "PublicKeyCipher::setKey: "
			"Failed to initialise RSA object: %s\n",
			this->err_str() );
		
		return false;
	}
}


/**
 *  This method encrypts a string using this key.  It returns the size of the
 *  encrypted stream, or -1 on error.
 */
int PublicKeyCipher::encrypt( BinaryIStream & clearStream,
	BinaryOStream & cipherStream, EncryptionFunctionPtr pEncryptionFunc )
{

	// Size of the modulus used in the RSA algorithm.
	int rsaModulusSize = RSA_size( pRSA_ );	

	// If the size of cleartext is equals to or greater than (the size 
	// of RSA modulus - RSA_PADDING), cleartext will be split into smaller 
	// parts.  These smaller parts are encrypted and written to 
	// cipherStream.  
	
	// Ensures each part to be encrypted is less than the size of the RSA 
	// modulus minus padding.  partSize must be less than 
	// (rsaModulusSize - RSA_PADDING).
	int partSize = rsaModulusSize - RSA_PADDING - 1;


	// Encrypt clearText.
	int encryptLen = 0;
	while (clearStream.remainingLength())
	{
		// If remaining cleartext to be encrypted is too alrge, 
		// we retrieve what can be encrypted in a block.
		int numBytesToRetrieve =
			std::min( partSize, clearStream.remainingLength() );

		unsigned char * clearText = 
			(unsigned char*)clearStream.retrieve( numBytesToRetrieve );

		unsigned char * cipherText =
			(unsigned char *)cipherStream.reserve( rsaModulusSize );

		int curEncryptLen = 
			(*pEncryptionFunc)( 
				numBytesToRetrieve, clearText, cipherText, pRSA_, 
				RSA_PKCS1_OAEP_PADDING );
	
		MF_ASSERT( curEncryptLen == RSA_size( pRSA_ ) );

		encryptLen += curEncryptLen; 
	}

	
	return encryptLen;
}


/**
 *  This method decrypts data encrypted with this key.  It returns the length of
 *  the decrypted stream, or -1 on error.
 */
int PublicKeyCipher::decrypt( BinaryIStream & cipherStream,
	BinaryOStream & clearStream, EncryptionFunctionPtr pEncryptionFunc )
{

	// Size of the modulus used in the RSA algorithm.
	int rsaModulusSize = RSA_size( pRSA_ );	

	unsigned char * clearText = 
		(unsigned char*) clearStream.reserve( cipherStream.remainingLength() );

	// Decrypt the encrypted blocks and write cleartext to clearStream.
	int clearTextSize = 0;
	while (cipherStream.remainingLength())	
	{
		unsigned char * cipherText =
			(unsigned char *)cipherStream.retrieve( rsaModulusSize  );

		if (cipherStream.error())
		{
			ERROR_MSG( "PrivateKeyCipher::privateDecrypt: "
				"Not enough data on stream for encrypted data chunk\n" );

			return -1;
		}	

		int bytesDecrypted = 
			(*pEncryptionFunc)( RSA_size( pRSA_ ), cipherText, clearText,
				pRSA_, RSA_PKCS1_OAEP_PADDING );	

		clearTextSize += bytesDecrypted;
		clearText += bytesDecrypted;
	}


	if (clearTextSize == 0)
	{
		return -1;
	}

	return  clearTextSize;
}


/**
 *  This method frees memory used by this object and makes it unusable.
 */
void PublicKeyCipher::cleanup()
{
	if (pRSA_)
	{
		RSA_free( pRSA_ );
		pRSA_ = NULL;
	}
}


/**
 *  This method writes the standard base64 encoded representation of the public
 *  key into readableKey_.
 */
void PublicKeyCipher::setReadableKey()
{
	BIO * bio = BIO_new( BIO_s_mem() );
	PEM_write_bio_RSA_PUBKEY( bio, pRSA_ );

	char buf[ 1024 ];
	while (BIO_gets( bio, buf, sizeof( buf ) - 1 ) > 0)
	{
		readableKey_.append( buf );
	}

	BIO_free( bio );
}


/**
 *  This method returns the most recent OpenSSL error message.
 */
const char * PublicKeyCipher::err_str() const
{
	static bool errorStringsLoaded = false;

	if (!errorStringsLoaded)
	{
		ERR_load_crypto_strings();
	}

	return ERR_error_string( ERR_get_error(), 0 );
}

} // namespace Mercury

// public_key_cipher.cpp
