/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef TEXTURE_COMPRESSOR_HPP
#define TEXTURE_COMPRESSOR_HPP

#include <iostream>

/**
 *	This class has the ability to compress a texture to any given D3DFORMAT.
 *	This class is typically used with limited scope.
 *	Who knows what will happen if you keep this baby around.
 */
class TextureCompressor
{
public:
	TextureCompressor( DX::Texture* src = NULL, D3DFORMAT fmt = D3DFMT_UNKNOWN, int numDestMipLevels = 0 );
	~TextureCompressor();

	const DX::Texture*	pSrcTexture() const;
	void pSrcTexture( DX::Texture* );

	const D3DFORMAT destinationFormat() const;
	void destinationFormat( D3DFORMAT );

	int destinationMipMapCount() const;
	void destinationMipMapCount( int c );

	bool save( const std::string & filename );
	bool stow( DataSectionPtr pSection, const std::string & childTag );


private:
	HRESULT bltAllLevels();
	HRESULT changeFormat();

	void calculateDestMipLevels();

	TextureCompressor( const TextureCompressor& );
	TextureCompressor& operator=( const TextureCompressor& );

	BinaryFile*			textureFile_;
	D3DFORMAT			fmtTo_;
	DX::Texture*		pSrcTexture_;
	DX::Texture*		pDestTexture_;
	int					numSrcMipLevels_;
	int					numDestMipLevels_;

	friend std::ostream& operator<<( std::ostream&, const TextureCompressor& );
};

#ifdef CODE_INLINE
#include "texture_compressor.ipp"
#endif

#endif // TEXTURE_COMPRESSOR_HPP
