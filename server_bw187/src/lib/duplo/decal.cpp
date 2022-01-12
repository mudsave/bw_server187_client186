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
#include "decal.hpp"

#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk_obstacle.hpp"
#include "pyscript/script.hpp"
#include "pyscript/pyobject_plus.hpp"
#include "physics2/worldtri.hpp"
#include "moo/effect_material.hpp"
#include "moo/texture_manager.hpp"
#include "moo/render_context.hpp"
#include "moo/base_texture.hpp"
#include "moo/texture_manager.hpp"
#include "resmgr/auto_config.hpp"

DECLARE_DEBUG_COMPONENT2( "duplo", 0 )

static AutoConfigString s_decalEffect( "system/decalEffect" );

// -----------------------------------------------------------------------------
// Section: Decal
// -----------------------------------------------------------------------------

int Decal_token = 1;
const uint32 N_DECAL_TRIANGLES = 5000;

const uint32 COMPRESSED_TILE_SIZE = 16;

static void blockFill( void* destination, uint32 size )
{
	uint8* dest = (uint8*)destination;
	const uint32 block[4] = {0xffffffff, 0xffffffff, 0x0, 0x0 };
	size = size / COMPRESSED_TILE_SIZE;
	for (uint32 i = 0; i < size; i++)
	{
		CopyMemory( dest, block, 16 );
		dest += 16;
	}
}
/**
 * This class implements a texture built up of several other textures.
 */
class TileTexture : public Moo::BaseTexture, public Moo::DeviceCallback
{
public:
	TileTexture( uint32 uTiles, uint32 vTiles, uint32 tileSize) : 
	  tileSize_( tileSize ),
	  width_( uTiles * tileSize ),
	  height_( vTiles * tileSize ),
	  uTiles_( uTiles ),
	  vTiles_( vTiles ),
	  uSize_( 1.f / float( uTiles ) ),
	  vSize_( 1.f / float( vTiles ) ),
	  nMipMaps_( 0 ),
	  pTexture_( NULL )
	{
		subTextures_.resize( uTiles * vTiles );
		
	}
	~TileTexture()
	{
	}
	virtual DX::BaseTexture*	pTexture( ) { return pTexture_; };
	virtual uint32			width( ) const { return width_; };
	virtual uint32			height( ) const { return height_; };
	virtual D3DFORMAT		format( ) const { return D3DFMT_DXT3; };
	virtual uint32			textureMemoryUsed( ) { return 0; } ;
	virtual const std::string& resourceID( ) const 
	{ 
		static std::string name = "tile_texture";
		return name; 
	};

	struct	SubTexture
	{
		std::string name_;
		float		uOffset_;
		float		vOffset_;
	};
	typedef std::vector< SubTexture > SubTextures;

	int textureIndex( const std::string& textureName );
	const SubTexture& subTexture( int index ) const { return subTextures_[ index ]; };
	float uSize() const { return uSize_; }
	float vSize() const { return vSize_; }

	virtual void deleteUnmanagedObjects( );
	virtual void createUnmanagedObjects( );
private:
	int addSubTexture( const std::string& textureName );
	void copySubTexture( int index, DX::Texture* pTex );
	DX::Texture*	pTexture_;
	uint8*			pBits_;
	uint32			width_;
	uint32			height_;
	uint32			uTiles_;
	uint32			vTiles_;
	uint32			tileSize_;
	uint32			nMipMaps_;
	float			uSize_;
	float			vSize_;
	SubTextures		subTextures_;

	TileTexture(const TileTexture&);
	TileTexture& operator=(const TileTexture&);
};

static SmartPointer<TileTexture> s_decalTexture = new TileTexture(4, 4, 64);

class SubTexFinder
{
public:
	SubTexFinder( const std::string& name ) { name_ = name; }
	bool operator ()( const TileTexture::SubTexture& subTex ) 
		{ return name_ == subTex.name_; }
	std::string name_;
};


/**
 *
 */
int TileTexture::addSubTexture( const std::string& textureName )
{
	SubTextures::iterator it = std::find_if( subTextures_.begin(), 
		subTextures_.end(), SubTexFinder( "" ) );

	int index = -1;
	if (it != subTextures_.end())
	{
		Moo::BaseTexturePtr baseTex = 
			Moo::TextureManager::instance()->getSystemMemoryTexture( textureName );
		if (baseTex.hasObject())
		{
			DX::BaseTexture* pBase = baseTex->pTexture();
            if (pBase->GetType() == D3DRTYPE_TEXTURE)
			{
				DX::Texture* pTex = (DX::Texture*)pBase;
				D3DSURFACE_DESC desc;
				pTex->GetLevelDesc( 0, &desc );
				if (desc.Format == D3DFMT_DXT3 && 
					desc.Width == tileSize_ &&
					desc.Height == tileSize_)
				{
					index = it - subTextures_.begin();
					copySubTexture( index, pTex );
					it->name_ = textureName;
					uint32 uOff = index % uTiles_;
					uint32 vOff = index / uTiles_;
					it->uOffset_ = float(uOff) * uSize_;
					it->vOffset_ = float(vOff) * vSize_;
				}
				else
				{
					if (desc.Format != D3DFMT_DXT3)
					{
						ERROR_MSG( "decal %s has the wrong format, it must "
									"be DXT3\n", textureName.c_str() );
					}
					else
					{
						ERROR_MSG( "decal %s has the wrong size, it must "
									"be 64 x 64\n", textureName.c_str() );
					}
				}
			}
		}
	}
	return index;
}

/**
 *
 */
void TileTexture::copySubTexture( int index, DX::Texture* pTex )
{
	if (!pTexture_)
	{
		createUnmanagedObjects();
	}
	if (pTexture_)
	{
		uint32 uOff = index % uTiles_;
		uint32 vOff = index / uTiles_;
		uint32 width = width_;
		uint32 height = height_;
		uint32 tileSize = tileSize_;
		RECT srcRect;
		srcRect.left = 0;
		srcRect.right = tileSize_;
		srcRect.top = 0;
		srcRect.bottom = tileSize_;

		POINT destPt;
		destPt.x = (index % uTiles_) * tileSize_;
		destPt.y = (index / uTiles_) * tileSize_;

		for (uint32 mip = 0; mip < nMipMaps_; mip++)
		{
			DX::Surface* pDestSurface;
			HRESULT hr = pTexture_->GetSurfaceLevel( mip, &pDestSurface );

			DX::Surface* pSrcSurface;
			hr = pTex->GetSurfaceLevel( mip, &pSrcSurface );

			//copy data from small map's mip-map to large map's mip-map
			hr = Moo::rc().device()->UpdateSurface( pSrcSurface, &srcRect, pDestSurface, &destPt );

			//advance to next mip-map
			destPt.x/=2;	
			destPt.y/=2;	
			srcRect.right = max( (srcRect.right/2), 1L );
			srcRect.bottom = max( (srcRect.bottom/2), 1L );
			pDestSurface->Release();
			pSrcSurface->Release();
		}
	}
}

void TileTexture::deleteUnmanagedObjects( )
{
	if (pTexture_)
	{
		pTexture_->Release();
		pTexture_ = NULL;
	}
	pBits_ = NULL;

}

void TileTexture::createUnmanagedObjects( )
{
	nMipMaps_ = 0;
	uint32 compressedTiles = 0;
	uint32 mipMapTiles = (tileSize_ * tileSize_) / 16;
	while (mipMapTiles != 0)
	{
		compressedTiles += mipMapTiles;
		nMipMaps_++;
		mipMapTiles /= 4;
	}

	uint32 surfaceSize = compressedTiles * uTiles_ * vTiles_ * COMPRESSED_TILE_SIZE ;
	HRESULT hr = Moo::rc().device()->CreateTexture( width_, height_, nMipMaps_, 0, D3DFMT_DXT3, D3DPOOL_DEFAULT, &pTexture_, NULL );
	if SUCCEEDED( hr )
	{
		DX::Texture* sysCopy;
		if( SUCCEEDED( Moo::rc().device()->CreateTexture( width_, height_, nMipMaps_, 0, 
			D3DFMT_DXT3, D3DPOOL_SYSTEMMEM, &sysCopy, NULL ) ) )
		{
			uint32 nLevels = sysCopy->GetLevelCount( );
			for (uint32 i = 0; i < nLevels; i++)
			{
				D3DSURFACE_DESC sd;
				sysCopy->GetLevelDesc( i, &sd );
				D3DLOCKED_RECT lr;
				sysCopy->LockRect( i, &lr, NULL, 0 );
				// DXT3 surfaces use one byte per texel, so width x height gives us the surface size
				blockFill( lr.pBits, sd.Width * sd.Height );
				// This fixes a bug on nvidia cards, they don't like compressed dxt3 textures 
				// to be initialised to all full alpha...
				*(char*)lr.pBits = 0;
				sysCopy->UnlockRect( i );
			}
			Moo::rc().device()->UpdateTexture( sysCopy, pTexture_ );
			sysCopy->Release();
			sysCopy = NULL;
		}
	}
	else
	{
		if (hr == D3DERR_INVALIDCALL)
			DEBUG_MSG( "TileTexture::createTexture - could not create texture map ( invalid call )\n");
		else
			DEBUG_MSG( "TileTexture::createTexture - could not create texture map ( error %lx )\n", hr );
	}
	SubTextures::iterator it = subTextures_.begin();
	SubTextures::iterator end = subTextures_.end();
	while (it != end)
	{
		std::string name = it->name_;
		it->name_ = "";
		if (name.length())
		{
			addSubTexture( name );
		}
		it++;
	}
}

/**
 *
 */
int TileTexture::textureIndex( const std::string& textureName )
{
	SubTextures::iterator it = std::find_if( subTextures_.begin(), 
		subTextures_.end(), SubTexFinder( textureName ) );

	int index = -1;
	if (it != subTextures_.end())
	{
		index = it - subTextures_.begin();
	}
	else
	{
		index = addSubTexture( textureName );
	}
	return index;
}

/**
 *	This class accumulates any triangles passed into it
 */
class DecalTriAcc : public CollisionCallback
{
public:
	VectorNoDestructor<WorldTriangle> tris_;
	Vector3 normal_;
private:
	virtual int operator()( const ChunkObstacle & obstacle,
		const WorldTriangle & triangle, float dist )
	{
		if (!(triangle.flags() & (TRIANGLE_TRANSPARENT | TRIANGLE_DOOR)) && 
			 !(obstacle.pItem()->userFlags() & 4))
		{
			WorldTriangle wt(
				obstacle.transform_.applyPoint( triangle.v0() ),
				obstacle.transform_.applyPoint( triangle.v1() ),
				obstacle.transform_.applyPoint( triangle.v2() ),
				triangle.flags() );
			Vector3 norm = wt.normal();
			norm.normalise();
			const float cosAngle = cosf( DEG_TO_RAD( 45.f ) );
			if (normal_.dotProduct(norm) < cosAngle)
				return 1 | 2;

			// only add it if we don't already have it
			// (not sure why this happens, but it does)
			uint sz = tris_.size();
			uint i;
			for (i = 0; i < sz; i++)
			{
				if (tris_[i].v0() == wt.v0() &&
					tris_[i].v1() == wt.v1() &&
					tris_[i].v2() == wt.v2()) break;
			}
			if (i >= sz) tris_.push_back( wt );
		}

		return COLLIDE_ALL;
	}
};

static DecalTriAcc s_triacc;

class ClosestTriangle : public CollisionCallback
{
public:
	virtual int operator()( const ChunkObstacle & obstacle,
		const WorldTriangle & triangle, float /*dist*/ )
	{
		// transform into world space
		hit_ = WorldTriangle(
			obstacle.transform_.applyPoint( triangle.v0() ),
			obstacle.transform_.applyPoint( triangle.v1() ),
			obstacle.transform_.applyPoint( triangle.v2() ) );
		return COLLIDE_BEFORE;
	}

	WorldTriangle hit_;
};

static ClosestTriangle s_closesttri;

/**
 *	Constructor.
 */
Decal::Decal() :
	nVertices_( N_DECAL_TRIANGLES * 3 ),
	decalVertexBase_( 0 ),
	pVertices_( NULL ),
	pVertexBuffer_( NULL ),
	pMaterial_( NULL )
{
	MainLoopTasks::root().add( this, "Flora/Decal", "<App", NULL );
}


struct OutcodeDef
{
	Outcode outcode;
	uint32	axis;
	float	value;
};

OutcodeDef outcodeDefs[4] = 
{
	OUTCODE_LEFT, 0, 0.f,\
	OUTCODE_RIGHT, 0, 1.f,\
	OUTCODE_TOP, 1, 0.f,\
	OUTCODE_BOTTOM, 1, 1.f
};

void Decal::clip( std::vector<ClipVertex>& cvs )
{
	Outcode combined = 0;
    for (uint32 i = 0; i < cvs.size(); i++)
	{
		cvs[i].calcOutcode();
		combined |= cvs[i].oc_;
	}

	if (combined)
	{
		Outcode mask = 0;
		for (uint32 oci = 0; oci < 4; oci++)
		{
			const OutcodeDef& ocd = outcodeDefs[oci];
			mask |= ocd.outcode;
			if ((ocd.outcode & combined) && 
				(cvs.size() > 2))
			{
				cvs.push_back( cvs.front() );
				std::vector<ClipVertex>::iterator it = cvs.begin() + 1;
				while (it != cvs.end())
				{
					std::vector<ClipVertex>::iterator preIt = it;
					preIt--;
					if ((preIt->oc_ & ocd.outcode) != 
						(it->oc_ & ocd.outcode))
					{
						ClipVertex tclip;
						float scale = (ocd.value - it->uv_[ocd.axis]) / 
							(preIt->uv_[ocd.axis] - it->uv_[ocd.axis]);
						tclip.pos_ = it->pos_ + ((preIt->pos_ - it->pos_) * scale);
						tclip.uv_ = it->uv_ + ((preIt->uv_ - it->uv_) * scale);
						tclip.calcOutcode();
						tclip.oc_ &= ~mask;
						it = cvs.insert( it, tclip );
						it++;
					}
					it++;
				}
				cvs.pop_back();
				it = cvs.begin();
				while (it != cvs.end())
				{
					if (it->oc_ & ocd.outcode)
					{
						it = cvs.erase( it );
					}
					else
					{
						it++;
					}
				}
			}
		}
	}
}

/**
 *	This function attempts to add a decal to the scene, given a world ray.
 *	If the world ray collides with scene geometry, a decal is placed over
 *	the intersecting region.  Try not to have too large an extent for the
 *	decal collision ray, for reasons of performance.
 *
 *	@param start	The start of the collision ray.
 *	@param end		The end of the collision ray.
 *	@param size		The size of the resultant decal.
 *	@param type		The index of the decal texture.  @see BigWorld.decalTextureIndex
 *
 */
void Decal::add( const Vector3& start, const Vector3& end, float size, int type )
{
	if (type == -1)
		return;

	ChunkSpacePtr pSpace = ChunkManager::instance().cameraSpace();
	if ( !pSpace.exists() )
		return;

	const TileTexture::SubTexture& subTex = s_decalTexture->subTexture( type );


	float dist = pSpace->collide( start, end, s_closesttri );

	if (dist <= 0.1f)
		return;

	float halfSize = size * 0.5f;
	Vector3 direction = end - start;
	direction.normalise();
	Vector3 pos = start + direction * dist;
	s_triacc.normal_ = s_closesttri.hit_.normal();
	s_triacc.normal_.normalise();
	direction = s_triacc.normal_ * -1.f;
	pos -= direction * 0.5f * size;

	const int HALF_RAND_MAX = RAND_MAX / 2;

    Vector3 up(float(rand() - HALF_RAND_MAX), float(rand() - HALF_RAND_MAX), float(rand() - HALF_RAND_MAX));
	up.normalise();
	while (fabs(up.dotProduct(direction)) > 0.98f)
	{
		up.set(float(rand() - HALF_RAND_MAX), float(rand() - HALF_RAND_MAX), float(rand() - HALF_RAND_MAX));
		up.normalise();
	}
	Vector3 across;
	across.crossProduct( direction, up );
	across.normalise();
	up.crossProduct( across, direction );
	up *= halfSize;
	across *= halfSize;
	Vector3 ext = direction * size;
	Vector3 topLeft = pos - across + up;
	Vector3 topRight = pos + across + up;
	Vector3 bottomLeft = pos - across - up;
	Vector3 bottomRight = pos + across - up;

	WorldTriangle tri1( topLeft, topRight, bottomLeft );
	WorldTriangle tri2( topRight, bottomRight, bottomLeft );
	
	s_triacc.tris_.clear();
	pSpace->collide( tri1, ext + tri1.v0(), s_triacc  );
	size_t firstCount = s_triacc.tris_.size();
	pSpace->collide( tri2, ext + tri2.v0(), s_triacc  );

	Matrix m;
	m.translation( bottomLeft );
	m[0] = bottomRight - bottomLeft;
	m[1] = topLeft - bottomLeft;
	m[2] = direction * -1.f;
	m.invert();

	// Lock the VB
	// D3DLOCK_NOOVERWRITE does not seem to work on gf4mx cards
	DWORD flags = D3DLOCK_NOOVERWRITE;
	if (Moo::rc().deviceInfo( Moo::rc().deviceIndex() ).compatibilityFlags_ & Moo::COMPATIBILITYFLAG_NOOVERWRITE)
	{
		flags = 0;
	}
	HRESULT hr = pVertexBuffer_->Lock( 0, 0, (void**)&pVertices_, flags );
	if ( FAILED(hr) )
	{
		DEBUG_MSG( "Decal::add - could not lock vertex buffer - error code %lx\n", hr );
		return;
	}

	for (uint32 i = 0; i < s_triacc.tris_.size(); i++)
	{
		std::vector<ClipVertex> cvs;
		ClipVertex cv;

		const WorldTriangle& wt = s_triacc.tris_[i];
		Vector3 uv;
		
        cv.pos_ = wt.v0();
		uv = m.applyPoint(wt.v0());
		cv.uv_.x = uv.x;
		cv.uv_.y = 1.f - uv.y;
		cvs.push_back( cv );

        cv.pos_ = wt.v1();
		uv = m.applyPoint(wt.v1());
		cv.uv_.x = uv.x;
		cv.uv_.y = 1.f - uv.y;
		cvs.push_back( cv );

        cv.pos_ = wt.v2();
		uv = m.applyPoint(wt.v2());
		cv.uv_.x = uv.x;
		cv.uv_.y = 1.f - uv.y;
		cvs.push_back( cv );

		clip( cvs );

		if (cvs.size() > 2)
			for (uint32 i = 1; i < (cvs.size()-1);i++)
			{
				if (decalVertexBase_ >= nVertices_)
				{
					decalVertexBase_ = 0;
				}
				pVertices_[decalVertexBase_].pos_ = cvs[0].pos_;
				pVertices_[decalVertexBase_++].uv_.set( 
					cvs[0].uv_.x * s_decalTexture->uSize() + subTex.uOffset_,
					cvs[0].uv_.y * s_decalTexture->vSize() + subTex.vOffset_ );
				pVertices_[decalVertexBase_].pos_ = cvs[i].pos_;
				pVertices_[decalVertexBase_++].uv_.set( 
					cvs[i].uv_.x * s_decalTexture->uSize() + subTex.uOffset_,
					cvs[i].uv_.y * s_decalTexture->vSize() + subTex.vOffset_ );
				pVertices_[decalVertexBase_].pos_ = cvs[i + 1].pos_;
				pVertices_[decalVertexBase_++].uv_.set( 
					cvs[i+1].uv_.x * s_decalTexture->uSize() + subTex.uOffset_,
					cvs[i+1].uv_.y * s_decalTexture->vSize() + subTex.vOffset_ );
			}
	}
	hr = pVertexBuffer_->Unlock();
}


/**
 *	Destructor.
 */
Decal::~Decal()
{
}

void Decal::createUnmanagedObjects()
{
	//TODO : create a vertex buffer in AGP memory
	pVertices_ = NULL;
	//TODO : for cards that have no vertex shaders, the flag D3DUSAGE_SOFTWAREPROCESSING will have to be used in creation of the VB
	// TO DO: check decal vertex size for PC
	HRESULT hr = Moo::rc().device()->CreateVertexBuffer( nVertices_ * sizeof( DecalVertex ), 
		D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, DecalVertex::fvf(), D3DPOOL_DEFAULT, &pVertexBuffer_, NULL );
	if FAILED( hr )
	{
		DEBUG_MSG( "Decal::init - could not create vertex buffer ( error %lx )\n", hr );
		pVertexBuffer_ = NULL;
		return;
	}
	this->clear();
}

void Decal::deleteUnmanagedObjects()
{
    if (pVertexBuffer_)
	{
		pVertexBuffer_->Release();
		pVertexBuffer_ = NULL;
		pVertices_ = NULL;
	}
}


bool Decal::init()
{
	if (!pVertexBuffer_)
	{
		createUnmanagedObjects();
	}
	pMaterial_ = new Moo::EffectMaterial;
	bool res = pMaterial_->initFromEffect( s_decalEffect );

	if (!res)
	{
		ERROR_MSG( "Decal::init - unable to init material\n" );
		delete pMaterial_;
		pMaterial_ = NULL;
	}


	return res;
}

void Decal::fini()
{
	if (pMaterial_ != NULL)
	{
		delete pMaterial_;
		pMaterial_ = NULL;
	}
	deleteUnmanagedObjects();
}

void Decal::clear()
{
	if (pVertexBuffer_)
	{
		// D3DLOCK_NOOVERWRITE does not seem to work on gf4mx cards
		DWORD flags = D3DLOCK_NOOVERWRITE;
		if (Moo::rc().deviceInfo( Moo::rc().deviceIndex() ).compatibilityFlags_ & Moo::COMPATIBILITYFLAG_NOOVERWRITE)
		{
			flags = 0;
		}
		HRESULT hr = pVertexBuffer_->Lock( 0, 0, (void**)&pVertices_, flags );
		if ( FAILED(hr) )
		{
			ERROR_MSG( "Decal::clear - could not lock vertex buffer - error code %lx\n", hr );
			return;
		}
		ZeroMemory( pVertices_, nVertices_ * sizeof(DecalVertex) );
		hr = pVertexBuffer_->Unlock();
		decalVertexBase_ = 0;
	}
}

void Decal::draw()
{
	// Push planes out so geometry doesn't z-fight. This is more reliable than 
	// using ZBIAS offsetting the geometry.
	Moo::RenderContext& rc 	= Moo::rc();
	float savedNearPlane	= rc.camera().nearPlane();
	float savedFarPlane		= rc.camera().farPlane();

	rc.camera().nearPlane( savedNearPlane * 1.01f );
	rc.camera().farPlane(  savedFarPlane *	1.01f );
	rc.updateProjectionMatrix();
	rc.updateViewTransforms();

	// Begin decal draw
	Moo::rc().setFVF( DecalVertex::fvf() );

	if (Moo::rc().mixedVertexProcessing())
	{
		Moo::rc().setVertexShader( NULL );
		Moo::rc().device()->SetVertexShader( NULL );
		Moo::rc().device()->SetSoftwareVertexProcessing(FALSE);
	}

	Moo::rc().device()->SetStreamSource( 0, pVertexBuffer_, 0, sizeof( DecalVertex ) );
	pMaterial_->pEffect()->pEffect()->SetTexture( "decalMap", s_decalTexture->pTexture() );

	if (pMaterial_->begin())
	{
		for (uint32 i = 0; i < pMaterial_->nPasses(); i++)
		{
			pMaterial_->beginPass( i );
			Moo::rc().drawPrimitive( D3DPT_TRIANGLELIST, 0, nVertices_/3 );
			pMaterial_->endPass();
		}
		pMaterial_->end();
	}
				
	if (Moo::rc().mixedVertexProcessing())
		Moo::rc().device()->SetSoftwareVertexProcessing(TRUE);

	// Restore old planes
	rc.camera().nearPlane( savedNearPlane );
	rc.camera().farPlane( savedFarPlane );
	rc.updateProjectionMatrix();
	rc.updateViewTransforms();
}

static Decal s_decal;

/*~ function BigWorld.addDecal
 *
 *	@param beg The origin of the collision ray
 *	@param ext The extent of the collision ray
 *	@param size The size of the resultant decal
 *	@param index The index of the decal texture.  see BigWorld.decalTextureIndex
 *
 *	This function attempts to add a decal to the scene, given a world ray.
 *	If the world ray collides with scene geometry, a decal is placed over
 *	the intersecting region.  Try not to have too large an extent for the
 *	decal collision ray, for reasons of performance.
 */
/**
 *	Adds a decal to the scene.
 */
void addDecal( const Vector3& beg, const Vector3& ext, float size, int index )
{
	s_decal.add( beg, ext, size, index);
}

PY_AUTO_MODULE_FUNCTION( RETVOID, addDecal, ARG( Vector3, ARG( Vector3, ARG( float, ARG( int, END ) ) ) ) , BigWorld )


/*~ function BigWorld.decalTextureIndex
 *
 *	@param textureName The name of the texture to look up.
 *  @return The index of the texture
 *
 *	This function returns the texture index that is needed to pass into
 *	addDecal.  It takes the name of the texture, and returns the index,
 *	or -1 if none was found.
 */
/**
 *	Look up the decal texture index for the given texture name.
 */
int decalTextureIndex( const std::string& textureName  )
{
	return s_decalTexture->textureIndex( textureName );
}

PY_AUTO_MODULE_FUNCTION( RETDATA, decalTextureIndex, ARG( std::string, END ), BigWorld )


/*~ function BigWorld.clearDecals
 *
 *	This function clears the current list of decals.  This should be called
 *	when changing spaces, so that decals from another space are no longer drawn.
 */
/**
 *	Clear the current list of decals.
 */
void clearDecals()
{
	s_decal.clear();
}
PY_AUTO_MODULE_FUNCTION( RETVOID, clearDecals, END, BigWorld )

// decal.cpp
