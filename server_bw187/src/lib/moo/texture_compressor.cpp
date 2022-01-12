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
#include "texture_compressor.hpp"
#include "resmgr/multi_file_system.hpp"
#include "resmgr/bwresource.hpp"

DECLARE_DEBUG_COMPONENT2( "Moo", 0 )

#ifndef CODE_INLINE
#include "texture_compressor.ipp"
#endif

#ifndef ReleasePpo
#define ReleasePpo(ppo)			\
	if (*(ppo) != NULL)			\
	{							\
		(*(ppo))->Release();	\
		*(ppo) = NULL;			\
	}							\
	else (VOID)0
#endif

// -----------------------------------------------------------------------------
// Section: Construction/Destruction
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
TextureCompressor::TextureCompressor( DX::Texture* src, D3DFORMAT fmt, int numMipLevels ):
	fmtTo_( fmt ),
	pSrcTexture_( src ),
	pDestTexture_( NULL ),
	numSrcMipLevels_( 0 ),
	numDestMipLevels_( numMipLevels )
{
}


/**
 *	Destructor.
 */
TextureCompressor::~TextureCompressor()
{
	if ( pDestTexture_ )
	{
		pDestTexture_->Release();
		pDestTexture_ = NULL;
	}
}


// -----------------------------------------------------------------------------
// Section: Texture Compression Utility Routines.
// -----------------------------------------------------------------------------


/**
 *	This method converts the given source texture into the given
 *	texture format, and saves it to the given relative filename.
 *
 *	@param	filename	a resource tree relative filename.
 *
 *	@return True if there were no problems.
 */
bool TextureCompressor::save( const std::string & filename )
{
	bool result = false;

	//this call blts the source to destination texture, and changes formats.
	HRESULT hr = this->changeFormat();

	if ( SUCCEEDED( hr ) )
	{
		ID3DXBuffer* pBuffer = NULL;
		hr = D3DXSaveTextureToFileInMemory( &pBuffer,
			D3DXIFF_DDS, pDestTexture_, NULL );

		if ( SUCCEEDED( hr ) )
		{
			//DEBUG_MSG( "TextureCompressor : Saved DDS file %s\n", fullDDSName.c_str() );

			//have to release dest texture, given contract defined in this->changeFormat()
			ReleasePpo( &pDestTexture_ );

			BinaryPtr bin = new BinaryBlock( pBuffer->GetBufferPointer(),
				pBuffer->GetBufferSize() );
			result = BWResource::instance().fileSystem()->writeFile(
				filename, bin, true );

			pBuffer->Release();
		}
	}

	//This FAILED section catches any problems in the above code.
	if ( FAILED( hr ) && !result )
	{
		ERROR_MSG( "Could not save file %s.  Error code %lx\n", filename.c_str(), hr );
	}

	return result;
}


/**
 *	This method converts the given source texture into the given
 *	texture format, and stows it in the given data section.
 *
 *	This method does not save the data section to disk.
 *
 *	@param	pSection	a Data Section smart pointer.
 *	@param	childTag	tag within the given section.
 *
 *	@return True if there were no problems.
 */
bool TextureCompressor::stow( DataSectionPtr pSection, const std::string & childTag )
{
	//Save a temporary DDS file
	const std::string tempName = "temp_texture_compressor.dds";
	if ( !this->save( tempName ) )
		return false;

	//Create a binary block copy of this dds file.
	DataSectionPtr pDDSFile = BWResource::openSection( tempName );
	BinaryPtr ddsData = pDDSFile->asBinary();
	BinaryPtr binaryBlock = new BinaryBlock(ddsData->data(), ddsData->len());

	//Clean temporary file
	ddsData = NULL;
	pDDSFile = NULL;
	BWResource::instance().purge( tempName );
	BWResource::instance().fileSystem()->eraseFileOrDirectory( tempName );

	//Stick the DDS into a binary section, but don't save the file to disk.
	pSection->delChild( childTag );
	DataSectionPtr textureSection = pSection->openSection( childTag, true );
	textureSection->setParent(pSection);

	if ( !pSection->writeBinary( childTag, binaryBlock ) )
	{
		CRITICAL_MSG( "TextureCompressor::stow: error while writing BinSection in \"%s\"\n", childTag.c_str());
		return false;
	}

	textureSection->save();

	return true;
}


/**
 *	This method changes the source texture to the desired texture format.
 *	The result is stored in pDestTexture, which must be released after
 *	using this fn.
 *
 *	@return HRESULT indicating the outcome of this operation.
 */
HRESULT TextureCompressor::changeFormat()
{
	MF_ASSERT( pSrcTexture_ );

	D3DSURFACE_DESC desc;
	HRESULT hr = pSrcTexture_->GetLevelDesc( 0, &desc );
	numSrcMipLevels_ = pSrcTexture_->GetLevelCount();

	if (!Moo::rc().supportsTextureFormat( fmtTo_ ))
	{
		CRITICAL_MSG( "TextureCompressor: This device does not support "
			"the desired compressed texture format: %d ('%c%c%c%c')\n", fmtTo_,
			char(fmtTo_>>24), char(fmtTo_>>16), char(fmtTo_>>8), char(fmtTo_) );
		fmtTo_ = D3DFMT_A8R8G8B8;
	}

	if (SUCCEEDED( hr ))
	{
		pDestTexture_ = NULL;

		hr = Moo::rc().device()->CreateTexture( desc.Width, desc.Height, numDestMipLevels_,
             0, fmtTo_, D3DPOOL_SYSTEMMEM, &pDestTexture_, NULL);

        if (FAILED(hr))
		{
			ERROR_MSG( "TextureCompressor::changeFormat:"
					" Could not create texture. Error code %lx\n", hr );
            return hr;
		}

		if ( numDestMipLevels_ == 0 )
			calculateDestMipLevels();

		hr = this->bltAllLevels();

        if (FAILED(hr))
            return hr;
	}

	return S_OK;
}


/**
 *	This method BLTs all mip map levels from the source to destination
 *	textures, using the texture format baked into the destination
 *	texture resource.
 *
 *	@return HRESULT indicating the outcome of this operation.
 */
HRESULT TextureCompressor::bltAllLevels()
{
    HRESULT hr;
    DX::Texture* pmiptexSrc = (DX::Texture*)pSrcTexture_;
    DX::Texture* pmiptexDest = (DX::Texture*)pDestTexture_;

    DWORD iLevel;

    for (iLevel = 0; int(iLevel) < numSrcMipLevels_; iLevel++)
    {
		DX::Surface* psurfSrc = NULL;
		DX::Surface* psurfDest = NULL;
        hr = pmiptexSrc->GetSurfaceLevel(iLevel, &psurfSrc);
        hr = pmiptexDest->GetSurfaceLevel(iLevel, &psurfDest);
        hr = D3DXLoadSurfaceFromSurface(psurfDest, NULL, NULL,
            psurfSrc, NULL, NULL, D3DX_FILTER_TRIANGLE, 0);
        ReleasePpo(&psurfSrc);
        ReleasePpo(&psurfDest);
    }


	//Now, create further desired mip map levels
	if ( numDestMipLevels_ > numSrcMipLevels_ )
	{
		D3DSURFACE_DESC desc;
		HRESULT hr = pSrcTexture_->GetLevelDesc( 0, &desc );
		DX::Surface* psurfSrc = NULL;
		hr = pmiptexSrc->GetSurfaceLevel(0, &psurfSrc);

		for ( int level = iLevel; level < numDestMipLevels_; level++ )
		{
			DX::Surface* psurfDest = NULL;
			hr = pmiptexDest->GetSurfaceLevel(level, &psurfDest);
			hr = D3DXLoadSurfaceFromSurface(psurfDest, NULL, NULL,
				psurfSrc, NULL, NULL, D3DX_FILTER_TRIANGLE | D3DX_FILTER_MIRROR, 0);
			ReleasePpo(&psurfDest);
		}

		ReleasePpo(&psurfSrc);
	}

    return S_OK;
}


/**
 *	This method calculates the number of mip map levels to produce,
 *	given the width and height of the source surface.
 *	The result is stored in numDestMipLevels_
 */
void TextureCompressor::calculateDestMipLevels()
{
	numDestMipLevels_ = pDestTexture_->GetLevelCount();
}


// -----------------------------------------------------------------------------
// Section: Streaming
// -----------------------------------------------------------------------------

/**
 *	Output streaming operator for TextureCompressor.
 */
std::ostream& operator<<(std::ostream& o, const TextureCompressor& t)
{
	o << "TextureCompressor\n";
	return o;
}

// texture_compressor.cpp
