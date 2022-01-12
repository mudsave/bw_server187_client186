/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef TERRAIN_BLOCK_HPP
#define TERRAIN_BLOCK_HPP

#include "base_terrain_block.hpp"
#include "com_object_wrap.hpp"
#include "moo_dx.hpp"

namespace Moo
{

class TerrainTextureSetter;

/**
 *	This class implements a rectangular block of heightmapped terrain.
 *	It maintains the textures, vertices and indices for it.
 */
class TerrainBlock : public BaseTerrainBlock
{
public:
	~TerrainBlock();

	void	createManagedObjects();
	void	deleteManagedObjects();

	virtual void	draw( TerrainTextureSetter* tts = NULL );
	uint32	nLayers() const { return textures_.size(); };

	static TerrainBlockPtr loadBlock( const std::string& resourceName );
	const std::vector<float> & heightMap() const	{ return heightMap_; }
	static float						penumbraSize();
	static void							penumbraSize( float );

protected:
	TerrainBlock();
	ComObjectWrap< DX::VertexBuffer >	vertexBuffer_;
	ComObjectWrap< DX::IndexBuffer >	indexBuffer_;

	uint32								nVertices_;
	uint32								nIndices_;
	uint32								nPrimitives_;

	static float						penumbraSize_;

	TerrainBlock( const TerrainBlock& );
	TerrainBlock& operator=( const TerrainBlock& );

};



} //namespace Moo

#ifdef CODE_INLINE
#include "terrain_block.ipp"
#endif

#endif // TERRAIN_BLOCK_HPP
