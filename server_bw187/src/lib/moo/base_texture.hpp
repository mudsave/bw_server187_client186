/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef BASE_TEXTURE_HPP
#define BASE_TEXTURE_HPP

#include <iostream>


//Forward declaration to avoid errors later
//class ChunkTerrainObstacle;

#include "cstdmf/stdmf.hpp"
//#include "chunk/chunk_terrain.hpp"
#include "cstdmf/smartpointer.hpp"
#include "moo_dx.hpp"
#include "com_object_wrap.hpp"


namespace Moo
{
typedef SmartPointer< class BaseTexture> BaseTexturePtr;

class BaseTexture : public SafeReferenceCount
{
public:
	BaseTexture();
	virtual ~BaseTexture();

	virtual DX::BaseTexture*	pTexture( ) = 0;
	virtual uint32			width( ) const = 0;
	virtual uint32			height( ) const = 0;
	virtual D3DFORMAT		format( ) const = 0;
	virtual uint32			textureMemoryUsed( ) = 0;
	virtual const std::string& resourceID( ) const;

	virtual HRESULT load()		{ return 0; }
	virtual HRESULT release()	{ return 0; }
	virtual HRESULT				reload( ) { return 0; }
	virtual HRESULT				reload(const std::string & resourceID) { return 0; }

	virtual uint32	maxMipLevel() const { return 0; }
	virtual void	maxMipLevel( uint32 level ) {}

	virtual uint32	mipSkip() const { return 0; }
	virtual void	mipSkip( uint32 mipSkip ) {}

	virtual bool    isCubeMap() { return false; }

	virtual bool    isAnimated() { return false; }

protected:
	uint32	textureMemoryUsed( DX::Texture* );

	void	delFromManager();

private:
	BaseTexture(const BaseTexture&);
	BaseTexture& operator=(const BaseTexture&);

	friend std::ostream& operator<<(std::ostream&, const BaseTexture&);
};

}

#ifdef CODE_INLINE
#include "base_texture.ipp"
#endif




#endif // BASE_TEXTURE_HPP
