/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// managed_texture.hpp

#ifndef MANAGED_TEXTURE_HPP
#define MANAGED_TEXTURE_HPP

#include <iostream>

#include "base_texture.hpp"

namespace Moo
{
typedef SmartPointer< class ManagedTexture > ManagedTexturePtr;

#define MANAGED_CUBEMAPS 1

class ManagedTexture : public BaseTexture
{
public:
	typedef ComObjectWrap< DX::BaseTexture > Texture;

	~ManagedTexture();

	DX::BaseTexture*	pTexture( );
	const std::string&	resourceID( ) const;

	/// Returns the width of the texture, not completely valid if the texture is
	/// a cubetexture, but what can you do...
	uint32				width( ) const;

	/// Returns the height of the texture, not completely valid if the texture is
	/// a cubetexture, but what can you do...
	uint32				height( ) const;

	D3DFORMAT			format( ) const;

	/// Returns the memory used by the texture.
	uint32				textureMemoryUsed( );

	bool				valid() const;

	static void			tick();

	virtual uint32		mipSkip() const { return this->mipSkip_; }
	virtual void		mipSkip( uint32 mipSkip ) { this->mipSkip_ = mipSkip; }

#if MANAGED_CUBEMAPS
	virtual bool        isCubeMap() { return cubemap_; }
#endif

	static bool					s_accErrs;
	static std::string			s_accErrStr;
	static void					accErrs( bool acc );
	static const std::string&	accErrStr();

	static uint32		totalFrameTexmem_;
private:

	uint32				width_;
	uint32				height_;
	D3DFORMAT			format_;
	uint32				mipSkip_;

	uint32				textureMemoryUsed_;
	uint32				originalTextureMemoryUsed_;

	ManagedTexture();
	ManagedTexture( const std::string& resourceID, bool mustExist, int mipSkip = 0 );

	HRESULT				reload( );
	HRESULT				reload(const std::string & resourceID);

	HRESULT				load( bool mustExist );
	virtual HRESULT		load()			{ return this->load( true ); }
	virtual HRESULT		release();

	Texture				texture_;
	std::string			resourceID_;
	bool				valid_;
	bool				failedToLoad_;

#if MANAGED_CUBEMAPS
	bool				cubemap_;
#endif

	uint32				localFrameTimestamp_;

	static uint32		frameTimestamp_;

/*	ManagedTexture(const ManagedTexture&);
	ManagedTexture& operator=(const ManagedTexture&);*/

	friend class TextureManager;
	friend std::ostream& operator<<(std::ostream&, const ManagedTexture&);
};

}

#ifdef CODE_INLINE
#include "managed_texture.ipp"
#endif




#endif
// managed_texture.hpp