/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef EDITOR_CHUNK_TERRAIN_PROJECTOR_HPP
#define EDITOR_CHUNK_TERRAIN_PROJECTOR_HPP

typedef SmartPointer<class EditorChunkTerrain>	EditorChunkTerrainPtr;
typedef std::vector<EditorChunkTerrainPtr>	EditorChunkTerrainPtrs;
namespace Moo
{
	class VertexDeclaration;
	class EffectMaterial;	
}

class EditorChunkTerrainProjector
{
public:	
	static EditorChunkTerrainProjector& instance();

	typedef std::pair<Matrix, EditorChunkTerrainPtr >	BlockInPlace;
	typedef AVectorNoDestructor< BlockInPlace >		BlockVector;
	void projectTexture( const Moo::BaseTexturePtr& pTexture,
		float scale, float rotation, const Vector3& offset, bool wrap,
		EditorChunkTerrainPtrs& chunks, bool showHoles );
private:
	EditorChunkTerrainProjector();
	void setInitialRenderStates(const Moo::BaseTexturePtr& pTexture, bool wrap);
	void renderBlocks(BlockVector& blocks);	
	void textureTransform( const Matrix& world, Matrix& ret, const Vector3& offset );
	void ffTextureTransform( const Matrix& world, Matrix& ret, const Vector3& offset );

	float scale_;
	float rotation_;
	Vector3 offset_;
	bool showHoles_;
	class Moo::VertexDeclaration* pDecl_;
	class Moo::EffectMaterial* material_;
};

#endif