/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef SPEEDTREE_BB_OPTIMISER_HPP
#define SPEEDTREE_BB_OPTIMISER_HPP

// BW Tech Hearders
#include "cstdmf/smartpointer.hpp"
#include "moo/effect_material.hpp"
#include "moo/texture_aggregator.hpp"
#include "moo/moo_math.hpp"
#include "math/Matrix.hpp"

// DX Headers
#include <d3dx9math.h>

// DX Wrappers
#include "vertex_buffer.hpp"

// STD Headers
#include <vector>
#include <map>

// Forwad declarations
class Chunk;
namespace Moo { class BaseTexture; }

namespace speedtree {

/**
 *	The vertex type used to render 
 *	normal billboards (non optimized).
 */
struct BillboardVertex
{
	Vector3 position;
	Vector3 lightNormal;
	FLOAT	alphaNormal[3];
	FLOAT   texCoords[2];

	static DWORD fvf()
	{
		return
			D3DFVF_XYZ |
			D3DFVF_NORMAL |
			D3DFVF_TEXCOORDSIZE3(0) |
			D3DFVF_TEXCOORDSIZE2(1) |
			D3DFVF_TEX2;
	}

	static const float s_minAlpha;
	static const float s_maxAlpha;
};

typedef dxwrappers::VertexBuffer< BillboardVertex > BBVertexBuffer;

/**
 *	The billboard optimizer. Optimizes billboard rendering by compiling all
 *	billboards placed in the same chunk as a single vertex buffer, minimising
 *	the draw call overhead. The different textures of each tree type are all
 *	bunched together into a single texture atlas. Alpha test values, the only 
 *	remaining unique feature that differentiates each tree billboard are 
 *	uploaded as shader constants and processed in the vertex shader. Each 
 *	isntance of BillboardOptimiser caters for a single chunk.
 */
class BillboardOptimiser
{
public:
	typedef SmartPointer<BillboardOptimiser> BillboardOptimiserPtr;

	/**
	 *	Holds all camera and light setup information 
	 *	needed to render a single frame.
	 */
	struct FrameData
	{
		FrameData(		
				const Matrix  & vpj,
				const Matrix  & ivw,
				const Vector3 & sdr,
				const Moo::Colour sdf,
				const Moo::Colour sam,
				bool              texTrees) :
			viewProj(vpj),
			invView(ivw),
			sunDirection(sdr),
			sunDiffuse(sdf),
			sunAmbient(sam),
			texturedTrees(texTrees)
		{}

		const Matrix  & viewProj;
		const Matrix  & invView;
		const Vector3 & sunDirection;
		const Moo::Colour sunDiffuse;
		const Moo::Colour sunAmbient;
		bool              texturedTrees;
	};

	static void init(DataSectionPtr section, Moo::EffectMaterial * effect);
	static void fini();
	
	static BillboardOptimiserPtr retrieve(const Chunk & chunk);	

	static int addTreeType(
		Moo::BaseTexturePtr    texture, 
		const BBVertexBuffer & vertices,
		const char           * treeFilename);
		
	static void delTreeType(int treeTypeId);
	
	int addTree(
		int             treeTypeId,
		const Matrix &  world, 
		float           alphaValue,
		const Vector4 & fogColour,
		float           fogNear,
		float           fogFar);
		
	void removeTree(int treeID);
	void release(int treeID);
	
	void updateTreeAlpha(
		int             treeID, 
		float           alphaValue, 
		const Vector4 & fogColour,
		float           fogNear,
		float           fogFar);
		
	static void update();
	static void drawAll(const FrameData & frameData);

	void releaseBuffer();
	
	void incRef() const;
	void decRef() const;
	int  refCount() const;

private:
	typedef std::vector<int> IntVector;
	
	/**
	 * Represents a tree type in the optimizer
	 */
	struct TreeType
	{
		IntVector              tileIds;
		const BBVertexBuffer * vertices;
	};

	/**
	 * Represents a single tree in a chunk.
	 */
	struct TreeInstance : Aligned
	{
		int            typeId;
		Matrix         world;
	};

	typedef std::map<int, TreeType> TreeTypeMap;
	typedef std::avector<TreeInstance> TreeInstanceVector;
	typedef std::vector<float> AlphaValueVector;

	BillboardOptimiser(const Chunk * chunk);
	~BillboardOptimiser();

	static void getNearestSuitableTexCoords(
		DX::Texture	          * texture,
		const BillboardVertex * originalVertices,
		RECT                  & o_srcRect, 
		Vector2               * o_coordsOffsets);

	static void getNextBBBTSlot(
		const RECT & srcRect, 
		POINT      & o_dstPoint,
		Vector2    * o_dstTexCoords);
		
	static void renderIntoBBBT(
		DX::Texture	* texture,
		const RECT  & srcRect, 
		const POINT & dstPoint);

	// forbid copy
	BillboardOptimiser(const BillboardOptimiser &);
	const BillboardOptimiser & operator = (const BillboardOptimiser &);

	class BillboardsVBuffer * vbuffer_;
	const Chunk             * chunk_;

	TreeInstanceVector trees_;
	AlphaValueVector   alphas_;
	IntVector          freeSlots_;
	
	mutable int refCount_;

	static int         s_atlasMinSize;
	static int         s_atlasMaxSize;
	static int         s_atlasMipLevels;
	static int         s_nextTreeTypeId;	
	static TreeTypeMap s_treeTypes;

	typedef std::map<const Chunk *, BillboardOptimiser*> ChunkBBMap;
	static SimpleMutex s_chunkBBMapLock;
	static ChunkBBMap  s_chunkBBMap;
	
	static bool                   s_saveTextureAtlas;
	static Moo::TextureAggregator* s_texAggregator;
	
	friend class BillboardsVBuffer;
};

} // namespace speedtree
#endif // SPEEDTREE_BB_OPTIMISER_HPP
