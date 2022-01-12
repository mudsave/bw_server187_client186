/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef BASE_TERRAIN_BLOCK_HPP
#define BASE_TERRAIN_BLOCK_HPP

#ifndef MF_SERVER
#include "device_callback.hpp"
#endif

#include "cstdmf/smartpointer.hpp"
#include "cstdmf/debug.hpp"

#include "math/matrix.hpp"

#include <string>
#include <vector>

namespace Moo
{

typedef SmartPointer<class TerrainBlock>		TerrainBlockPtr;
typedef SmartPointer<class BaseTerrainBlock>	BaseTerrainBlockPtr;
#ifndef MF_SERVER
typedef SmartPointer<class BaseTexture>			BaseTexturePtr;
#endif // MF_SERVER

/**
 *	This class implements a rectangular block of heightmapped terrain.
 *	It maintains the textures, vertices and indices for it.
 */
class BaseTerrainBlock
#ifndef MF_SERVER
	: public DeviceCallback, public ReferenceCount
#else
	// The server shares TerrainBlocks
	: public SafeReferenceCount
#endif
{
public:
	// Make sure it has a virtual function table.
	virtual ~BaseTerrainBlock();

	static struct Dummy {
		operator float () { return -1000000.f; }
	} NO_TERRAIN;
	bool	load( const std::string& resourceName );

	struct FindDetails
	{
		FindDetails() : pBlock_( NULL ), pMatrix_( NULL ), pInvMatrix_( NULL )
		{
		}

		union
		{
			TerrainBlock * pBlock_;
			BaseTerrainBlock * pBaseBlock_;
		};
		const Matrix * pMatrix_;
		const Matrix * pInvMatrix_;
	};

	static FindDetails findOutsideBlock( const Vector3 & pos );
	static float getHeight(float x, float z);

	class TerrainFinder
	{
	public:
		virtual ~TerrainFinder() {};
		virtual FindDetails findOutsideBlock( const Vector3 & pos ) = 0;
	};

	static void setTerrainFinder( TerrainFinder & terrainFinder );

	struct TerrainBlockHeader
	{
		uint32		version_;
		uint32		heightMapWidth_;
		uint32		heightMapHeight_;
		float		spacing_;
		uint32		nTextures_;
		uint32		textureNameSize_;
		uint32		detailWidth_;
		uint32		detailHeight_;
		uint32		padding_[64 - 8];
	};

	/**
	 *	This class provides access to a pole in the terrain block,
	 *	and any surrounding ones.
	 */
	class Pole
	{
	public:
		Pole( BaseTerrainBlock & tb, int i ) :
			tb_( tb ), i_( i ) { }
		Pole( BaseTerrainBlock & tb, int x, int z ) :
			tb_( tb ), i_( x+1+(z+1)*tb.width_ ) { }

		void move ( int a )					{ i_ += a; }
		void move ( int x, int z )			{ i_ += x+z*tb_.width_; }

		int x( int a = 0 ) const			{ return (i_+a) % tb_.width_ - 1; }
		int z( int a = 0 ) const			{ return (i_+a) / tb_.width_ - 1; }

		float height( int a = 0 ) const		{ MF_ASSERT_DEBUG( size_t(i_+a) < tb_.heightMap_.size() ); return tb_.heightMap_[i_+a]; }
		float & height( int a = 0 )			{ MF_ASSERT_DEBUG( size_t(i_+a) < tb_.heightMap_.size() ); return tb_.heightMap_[i_+a]; }

		uint32 blend( int a = 0 ) const		{ MF_ASSERT_DEBUG( size_t(i_+a) < tb_.blendValues_.size() ); return tb_.blendValues_[i_+a]; }
		uint32 & blend( int a = 0 )			{ MF_ASSERT_DEBUG( size_t(i_+a) < tb_.blendValues_.size() ); return tb_.blendValues_[i_+a]; }

		uint16 shadow( int a = 0 ) const	{ MF_ASSERT_DEBUG( size_t(i_+a) < tb_.shadowValues_.size() ); return tb_.shadowValues_[i_+a]; }
		uint16 & shadow( int a = 0 )		{ MF_ASSERT_DEBUG( size_t(i_+a) < tb_.shadowValues_.size() ); return tb_.shadowValues_[i_+a]; }

		void hole( bool v )
			{ tb_.holes_[ x() + z() * tb_.blocksWidth_ ] = v; }
		bool hole() const
		{
			unsigned int index = x() + z() * tb_.blocksWidth_;
			if (index < 0 || index >= tb_.holes_.size())
				return false;
			return tb_.holes_[ index ];
		}

		const BaseTerrainBlock & block() const	{ return tb_; }
		BaseTerrainBlock & block()				{ return tb_; }

	private:
		BaseTerrainBlock &	tb_;
		int					i_;
	};
	friend class Pole;


	float spacing() const							{ return spacing_; }

	int blocksWidth() const							{ return blocksWidth_; }
	int blocksHeight() const						{ return blocksHeight_; }
	int verticesWidth() const						{ return verticesWidth_; }
	int verticesHeight() const						{ return verticesHeight_; }
	int polesWidth() const							{ return width_; }
	int	polesHeight() const							{ return height_; }
	const std::vector<float> & heightMap() const	{ return heightMap_; }

	uint8 detailID( float xf, float zf ) const;
	float heightAt( float x, float z ) const;
	Vector3 normalAt( float x, float z ) const;
	uint32 shadowAt( float x, float z ) const;

	bool noHoles() const							{ return noHoles_; }
	bool allHoles() const							{ return allHoles_; }

	uint32 materialKind( uint32 i ) const { return i >= materialKinds_.size() ? 0 : materialKinds_[i ]; }

protected:
	BaseTerrainBlock();

#ifndef MF_SERVER
	std::vector< BaseTexturePtr >		textures_;
#endif // MF_SERVER
	std::vector<float>					heightMap_;
	std::vector<uint32>					blendValues_;
	std::vector<uint16>					shadowValues_;
	std::vector<bool>					holes_;
	std::vector<uint8>					detailIDs_;
	std::vector<uint32>					materialKinds_;

	float								spacing_;
	uint32								width_;
	uint32								height_;
	uint32								blocksWidth_;
	uint32								blocksHeight_;
	uint32								verticesWidth_;
	uint32								verticesHeight_;
	uint32								detailWidth_;
	uint32								detailHeight_;

	float								heightPole( uint32 x, uint32 z ) const;
	uint32								heightPoleBlend( uint32 x, uint32 z ) const;
	uint32								heightPoleShadow( uint32 x, uint32 z ) const;
	Vector3								heightPoleNormal( uint32 x, uint32 z ) const;

	mutable int							referenceCount_;

	bool								noHoles_;
	bool								allHoles_;

	static TerrainFinder *				pTerrainFinder_;

	BaseTerrainBlock( const BaseTerrainBlock& );
	BaseTerrainBlock& operator=( const BaseTerrainBlock& );

};


} //namespace Moo

#ifdef CODE_INLINE
#include "base_terrain_block.ipp"
#endif

#endif // TERRAIN_BLOCK_HPP
