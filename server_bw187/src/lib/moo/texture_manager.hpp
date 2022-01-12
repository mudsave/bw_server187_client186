/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef TEXTURE_MANAGER_HPP
#define TEXTURE_MANAGER_HPP


#include <list>
#include "cstdmf/concurrency.hpp"
#include "cstdmf/smartpointer.hpp"
#include "resmgr/datasection.hpp"
#include "device_callback.hpp"
#include "moo_dx.hpp"
#include "base_texture.hpp"
#include "graphics_settings.hpp"


namespace Moo
{

class BaseTexture;
typedef SmartPointer<BaseTexture> BaseTexturePtr;

class TextureDetailLevel;
typedef SmartPointer< TextureDetailLevel > TextureDetailLevelPtr;
typedef std::vector< TextureDetailLevelPtr > TextureDetailLevels;


/**
 *	This singleton class keeps track of and loads ManagedTextures.
 */
class TextureManager : public Moo::DeviceCallback
{
public:
	typedef void (*ProgessFunc)(float);
	typedef std::map< std::string, BaseTexture * > TextureMap;

	~TextureManager();

	static TextureManager*	instance();

	HRESULT					initTextures( );
	HRESULT					releaseTextures( );

	void					deleteManagedObjects( );
	void					createManagedObjects( );

	uint32					textureMemoryUsed( ) const;
	uint32					textureMemoryUsed( const std::string& resourceName ) const;

	void					fullHouse( bool noMoreEntries = true );
	
	static bool				writeDDS( DX::BaseTexture* texture, 
								const std::string& ddsName, D3DFORMAT format, int numDestMipLevels = 0 );
	static TextureDetailLevelPtr detailLevel( const std::string& originalName );
	static bool				convertToDDS( 
								const std::string& originalName, 
								const std::string& ddsName,
								      D3DFORMAT    format );
	void					initDetailLevels( DataSectionPtr pDetailLevels );
	void					reloadAllTextures();
	void					recalculateDDSFiles();
	void					reloadMipMaps(ProgessFunc progFunc);
	void					setFormat( const std::string & fileName, D3DFORMAT format );


	BaseTexturePtr	get( const std::string & resourceID,
		bool allowAnimation = true, bool mustExist = true,
		bool loadIfMissing = true );
	BaseTexturePtr	getUnique( 
		const std::string & sanitisedResourceID,
		bool allowAnimation = true, bool mustExist = true);

	BaseTexturePtr	getSystemMemoryTexture( const std::string& resourceID );

	const TextureMap&		textureMap() const { return textures_; };

	static void init();
	static void fini();

private:
	TextureManager();
	TextureManager(const TextureManager&);
	TextureManager& operator=(const TextureManager&);

	static void del( BaseTexture * pTexture );
	void addInternal( BaseTexture * pTexture, std::string resourceID = "" );
	void delInternal( BaseTexture * pTexture );

	std::string prepareResourceName( const std::string& resourceName );
	std::string prepareResource( const std::string& resourceName, bool forceConvert = false );

	BaseTexturePtr find( const std::string & resourceID );
	void setQualityOption(int optionIndex);
	void setCompressionOption(int optionIndex);
	void reloadDirtyTextures();

	TextureMap				textures_;
	SimpleMutex				texturesLock_;

    TextureDetailLevels		detailLevels_;

	DX::Device*				pDevice_;

	bool					fullHouse_;
	uint64					detailLevelsModTime_;
	int						lodMode_;
	
	GraphicsSetting::GraphicsSettingPtr		qualitySettings_;
	GraphicsSetting::GraphicsSettingPtr		compressionSettings_;
	
	typedef std::pair<BaseTexture *, std::string> TextureStringPair;
	typedef std::vector<TextureStringPair> TextureStringVector;
	TextureStringVector dirtyTextures_;

	friend BaseTexture;
	static TextureManager*	pInstance_;
};

class TextureDetailLevel : public ReferenceCount
{
public:
	TextureDetailLevel();
	~TextureDetailLevel();
	void		init( DataSectionPtr pSection );
	void		init( const std::string & fileName, D3DFORMAT format );
	bool		check( const std::string& resourceID );

	void		clear();

	void		widthHeight( uint32& width, uint32& height );
	
	D3DFORMAT	format() const { return format_; }
	void		format( D3DFORMAT format ) { format_ = format; }
	D3DFORMAT	formatCompressed() const { return formatCompressed_; }
	void		formatCompressed( D3DFORMAT formatCompressed ) { formatCompressed_ = formatCompressed; }
	uint32		maxDim() const { return maxDim_; }
	void		maxDim( uint32 maxDim ) { maxDim_ = maxDim; }
	uint32		minDim() { return minDim_; }
	void		minDim( uint32 minDim ) { minDim_ = minDim; }
	uint32		reduceDim() const { return reduceDim_; }
	void		reduceDim( uint32 reduceDim ) { reduceDim_ = reduceDim; }
	uint32		lodMode() const { return lodMode_; }
	void		lodMode( uint32 lodMode ) { lodMode_ = lodMode; }
	void		mipCount( uint32 count ) { mipCount_ = count; }
	uint32		mipCount() const { return mipCount_; }
	void		mipSize( uint32 size ) { mipSize_ = size; }
	uint32		mipSize() const { return mipSize_; }
	void		horizontalMips( bool val ) { horizontalMips_ = val; }
	bool		horizontalMips( ) const { return horizontalMips_; }
	
	typedef std::vector< std::string > StringVector;

	StringVector& postfixes() { return postfixes_; }
	StringVector& prefixes() { return prefixes_; }
	StringVector& contains() { return contains_; }
private:

	StringVector	prefixes_;
	StringVector	postfixes_;
	StringVector	contains_;
	uint32			maxDim_;
	uint32			minDim_;
	uint32			reduceDim_;
	uint32			lodMode_;
	uint32			mipCount_;
	uint32			mipSize_;
	bool			horizontalMips_;
	D3DFORMAT		format_;
	D3DFORMAT		formatCompressed_;
};

}

#ifdef CODE_INLINE
#include "texture_manager.ipp"
#endif


#endif // TEXTURE_MANAGER_HPP
