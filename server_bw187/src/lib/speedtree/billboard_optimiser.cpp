/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// Module Interface
#include "billboard_optimiser.hpp"
#include "speedtree_config.hpp"

// BW Tech Hearders
#include "cstdmf/debug.hpp"
#include "cstdmf/concurrency.hpp"
#include "moo/material.hpp"

DECLARE_DEBUG_COMPONENT2("SpeedTree", 0)


#if SPEEDTREE_SUPPORT // -------------------------------------------------------

// BW Tech Hearders
#include "cstdmf/avector.hpp"
#include "cstdmf/smartpointer.hpp"
#include "moo/render_context.hpp"
#include "moo/sys_mem_texture.hpp"

// DX Wrappers
#include "vertex_buffer.hpp"
#include "d3dx9core.h"

// STD Headers
#include <vector>
#include <list>

// Import dx wrappers
using dxwrappers::VertexBuffer;

// Forwad declarations
class Chunk;

namespace speedtree {
namespace { // anonymous

/**
 *	The vertex type used to render optimized billboards.
 */
struct AlphaBillboardVertex
{
	Vector3 position;
	Vector3 lightNormal;
	FLOAT   alphaNormal[3];
	FLOAT   texCoords[2];
	FLOAT   alphaIndex;
	FLOAT   alphaMask[4];

	static DWORD fvf()
	{
		return
			D3DFVF_XYZ |
			D3DFVF_NORMAL |
			D3DFVF_TEXCOORDSIZE3(0) |
			D3DFVF_TEXCOORDSIZE2(1) |
			D3DFVF_TEXCOORDSIZE1(2) |
			D3DFVF_TEXCOORDSIZE4(3) |
			D3DFVF_TEX4;
	}
};

// Helper functions
void saveTextureAtlas(bool save, DX::BaseTexture * texture);

} // namespace anonymous

// Billboard alpha fading constants
const float BillboardVertex::s_minAlpha = 84.0f;
const float BillboardVertex::s_maxAlpha = 255.0f;
static float s_mungeNormalDepth = -10.f;

/**
 *	Holds the vertex buffer to draw the billboards of a chunk/optimiser. 
 *	The number of BillboardsVBuffer are kept to a minimum to save memory.
 *	Buffers that have been invisible for some frames are reclycled to 
 *	work for currently visible chunks. Chunks not reclycled for some 
 *	more frames are deleted.
 */
class BillboardsVBuffer : 
	public ReferenceCount,
	private Aligned
{
public:
	typedef BillboardOptimiser::TreeTypeMap TreeTypeMap;
	typedef BillboardOptimiser::TreeInstanceVector TreeInstanceVector;
	typedef BillboardOptimiser::AlphaValueVector AlphaValueVector;
	typedef BillboardOptimiser::FrameData FrameData;
	
	static void init(DataSectionPtr section, Moo::EffectMaterial * effect);
	
	static BillboardsVBuffer * acquire();

	static void update();
	static void resetAll();
	static void recycleBuffers();
	static void drawVisible(
		const FrameData & frameData, 
		DX::Texture     * texture,
		const Matrix    & texTransform);
	
	void touch(const Vector4 & fogColour, float fogNear, float fogFar);
	void draw();
	
	bool isCompiled() const;
	void compile();
	
	void bind(
		BillboardOptimiser * owner, 
		TreeInstanceVector * billboards, 
		AlphaValueVector   * alphas);

	void reset();

private:
	BillboardsVBuffer();
	~BillboardsVBuffer();

	// forbid copy
	BillboardsVBuffer(const BillboardsVBuffer &);
	const BillboardsVBuffer & operator = (const BillboardsVBuffer &);
	
	int     frameStamp_;
	Vector4 fogColour_;
	float   fogNear_;
	float   fogFar_;
	
	BillboardOptimiser * owner_;

	TreeInstanceVector * trees_;
	AlphaValueVector   * alphas_;
	
	typedef VertexBuffer< AlphaBillboardVertex > AlphaBillboardVBuffer;
	AlphaBillboardVBuffer vbuffer_;

	typedef SmartPointer<BillboardsVBuffer> BillboardsVBufferPtr;
	typedef std::list<BillboardsVBufferPtr> BillboardsVBufferList;
	static BillboardsVBufferList s_buffersPool;
	BillboardsVBufferList::iterator poolIt_;

	static int s_curFrameStamp;
	static int s_framesToRecycle;
	static int s_framesToDelete;


	static int s_activeCounter;
	static int s_visibleCounter;
	static int s_poolSize;
	
	static Moo::EffectMaterial * s_effect;
};

// -------------------------------------------------------------- BillboardOptimiser

/**
 *	Retrieves the BillboardOptimiser instance for the given chunk. Either 
 *	returns one created on a previous call of create and return a new one.
 *
 *	@param	chunk	chunk which to retrieve the optmizer instance for.
 *	@return			the requested optimiser instance.
 */
BillboardOptimiser::BillboardOptimiserPtr 
	BillboardOptimiser::retrieve(const Chunk & chunk)
{
	BillboardOptimiserPtr result;
	BillboardOptimiser::s_chunkBBMapLock.grab();
	ChunkBBMap & chunkBBMap = BillboardOptimiser::s_chunkBBMap;
	ChunkBBMap::const_iterator it = chunkBBMap.find(&chunk);
	if (it != s_chunkBBMap.end())
	{
		result = it->second;
	}
	else 
	{
		result = new BillboardOptimiser(&chunk);
		s_chunkBBMap.insert(std::make_pair(&chunk, result.getObject()));
	}
	BillboardOptimiser::s_chunkBBMapLock.give();
	return result;	
}

/**
 *	Initialises the billboard optimiser engine (static).
 *
 *	@param	section	XML section containing all initialization settings for 
 *					the optimiser engine (usually read from speedtree.xml).
 *	@param	effect	the effect material to use when rendering the billboards.
 */
void BillboardOptimiser::init(
	DataSectionPtr       section, 
	Moo::EffectMaterial * effect)
{
	DataSectionPtr optSection = section->openSection("billboardOptimiser");
	if (optSection.exists())
	{
		DataSectionPtr texSection = optSection->openSection("textureAtlas");
		if (texSection.exists())
		{	
			// Minimum size that the texture atlas
			// can assume (at the highest mipmap level)
			BillboardOptimiser::s_atlasMinSize = texSection->readInt(
				"minSize", BillboardOptimiser::s_atlasMinSize);
				
			// Maximum size that the texture atlas
			// can assume (at the highest mipmap level)
			BillboardOptimiser::s_atlasMaxSize = texSection->readInt(
				"maxSize", BillboardOptimiser::s_atlasMaxSize);
				
			// Number of mipmap levels to 
			// use for the the texture atlas
			BillboardOptimiser::s_atlasMipLevels = texSection->readInt(
				"mipLevels", BillboardOptimiser::s_atlasMipLevels);
				
			// Save texure atlas into a dds file? (useful for debugging)
			BillboardOptimiser::s_saveTextureAtlas = texSection->readBool(
				"save", BillboardOptimiser::s_saveTextureAtlas);	
		}
	};

	if( !BillboardOptimiser::s_texAggregator )
		BillboardOptimiser::s_texAggregator = new Moo::TextureAggregator(&BillboardsVBuffer::resetAll);

	Moo::TextureAggregator & texaggreg = *BillboardOptimiser::s_texAggregator;
	texaggreg.setMinSize(BillboardOptimiser::s_atlasMinSize);
	texaggreg.setMaxSize(BillboardOptimiser::s_atlasMaxSize);
	texaggreg.setMipLevels(BillboardOptimiser::s_atlasMipLevels);
	
	BillboardsVBuffer::init(optSection, effect);
}

/**
 *	Finalises the billboard optimiser engine (static).
 */
void BillboardOptimiser::fini()
{
	if (!BillboardOptimiser::s_treeTypes.empty())
	{
		WARNING_MSG(
			"BillboardOptimizer::fini: tree types still registered: %d\n", 
			BillboardOptimiser::s_treeTypes.size());
	}
	if( BillboardOptimiser::s_texAggregator )
		delete BillboardOptimiser::s_texAggregator;
}

/**
 *	Adds a new tree type to the optimiser (static). Will copy the 
 *	billboard sub textures into the texture atlas.
 *
 *	@param	textureName		name of the original billboard texture map.
 *	@param	vertices		(in/out) vertice data. Will vertices with texture 
 *							coordinates modified to use the texture atlas.
 */
 
// this #pragma works around a crash. Looks like vc2005 c++ compiler
// is too aggressive with its optimisations and generates broken code.
#pragma optimize("", off)
int BillboardOptimiser::addTreeType(
	Moo::BaseTexturePtr    texture, 
	const BBVertexBuffer & vertices,
	const char *           treeFilename)
{
	int quadCount = vertices.size() / 6;
	
	TreeType treeType;
	treeType.vertices = &vertices;
	
	Moo::TextureAggregator & texaggreg = *BillboardOptimiser::s_texAggregator;
		
	// for each quad in billboard
	bool success = true;
	if (vertices.lock())
	{
		for (int i=0; i<quadCount; ++i)
		{
			// get original texture coordinates
			Vector2 min(
				vertices[i*6+1].texCoords[0], 
				1 + vertices[i*6+1].texCoords[1]);
			Vector2 max(
				vertices[i*6+5].texCoords[0], 
				1 + vertices[i*6+5].texCoords[1]);

			try
			{			
				int tileId = texaggreg.addTile(texture, min, max);
				treeType.tileIds.push_back(tileId);
			}
			catch (const Moo::TextureAggregator::InvalidArgumentError &)
			{
				speedtreeError(
					treeFilename,
					"Could not register speedtree billboard "
					"texture. Falling back to non-optimized billboards. "
					"Most likely cause is a wrong '.texformat' file.\n");
					
				success = false;
				break;
			}
			catch (const Moo::TextureAggregator::DeviceError &)
			{
				INFO_MSG(
					"TextureAggregator returned an error. "
					"Falling back to non-optimized billboards. "
					"Most likely cause is lack of available video memory.\n");
					
				success = false;
				break;
			}
		}
		vertices.unlock();
	}
	else
	{
		success = false;
		ERROR_MSG(
			"BillboardOptimiser::addTreeType: "
			"Could not create/lock vertex buffer\n");
	}

	int treeTypeId = -1;
	if (success)
	{	
		treeTypeId = s_nextTreeTypeId++; // post-increment is intentional
		s_treeTypes.insert(std::make_pair(treeTypeId, treeType));
		
		saveTextureAtlas(
			BillboardOptimiser::s_saveTextureAtlas, 
			texaggreg.texture());
	}
		
	return treeTypeId;
}


void BillboardOptimiser::delTreeType(int treeTypeId)
{
	TreeTypeMap & treeTypes = BillboardOptimiser::s_treeTypes;
	TreeTypeMap::iterator treeTypeIt = treeTypes.find(treeTypeId);
	MF_ASSERT(treeTypeIt != treeTypes.end());
	
	// delete tree's tiles from texture aggregator
	IntVector::const_iterator tileIt  = treeTypeIt->second.tileIds.begin();
	IntVector::const_iterator tileEnd = treeTypeIt->second.tileIds.end();
	while(tileIt != tileEnd)
	{
		BillboardOptimiser::s_texAggregator->delTile(*tileIt);
		++tileIt;
	}
	
	// unregister tree type	
	treeTypes.erase(treeTypeIt);
	
	saveTextureAtlas(
		BillboardOptimiser::s_saveTextureAtlas, 
		BillboardOptimiser::s_texAggregator->texture());
}
#pragma optimize("", on)

/**
 *	Adds a new tree to this optmiser instance.
 *
 *	@param	world		tree's world transform.
 *	@param	vertices	tree's modified vertex data (returned from addTreeType).
 *	@param	alphaValue	tree's current alpha test value.
 *	@return				treeID with this optmiser.
 */
int BillboardOptimiser::addTree(
	int             treeTypeId,
	const Matrix &  world,
	float           alphaValue,
	const Vector4 & fogColour,
	float           fogNear, 
	float           fogFar)
{
	TreeInstance treeInstance;
	treeInstance.world  = world;
	treeInstance.typeId = treeTypeId;
	
	int treeID = 0;
	if (this->freeSlots_.empty())
	{
		treeID = this->trees_.size();
		this->trees_.push_back(treeInstance);
		if (treeID % 4 == 0)
		{
			// because setVectorArray expects an array of 
			// float4, always insert four floats at once. 
			for (int i=0; i<4; ++i)
			{
				this->alphas_.push_back(1.0f);
			}
			
			// this is what I'd like to be using but, for some
			// strange reason, it doesn't seems to work in vc2005
			// this->alphas_.insert(this->alphas_.end(), 4, 1.0f);
		}
	}
	else
	{
		treeID = this->freeSlots_.back();
		this->freeSlots_.pop_back();
		this->trees_[treeID] = treeInstance;
	}
	this->updateTreeAlpha(treeID, alphaValue, fogColour, fogNear, fogFar);
	
	// reset buffer so that they 
	// get recompiled on next draw 
	this->vbuffer_->reset();
	
	return treeID;
}

/**
 *	Removes a tree from this optimiser.
 *
 *	@param	treeID	id of tree, as returned from addTree.
 */
#pragma optimize("", off)
void BillboardOptimiser::removeTree(int treeID)
{
	MF_ASSERT(treeID >= 0 && treeID < int(this->alphas_.size()));
	this->trees_[treeID].typeId = -1;
	this->alphas_[treeID]       = 1.0f;
	this->freeSlots_.push_back(treeID);	
	if (this->vbuffer_ != NULL)
	{
		this->vbuffer_->reset();
	}
}
#pragma optimize("", off)

/**
 *	Update tree's alpha test value.
 *
 *	@param	treeID	id of tree, as returned from addTree.
 *	@param	alphaValue	tree's current alpha test value.
 */
void BillboardOptimiser::updateTreeAlpha(
	int treeID, float alphaValue, 
	const Vector4 & fogColour,
	float fogNear, float fogFar)
{
	// this is our only chance to touch the 
	// buffer to make sure it is not recycled. 
	if (this->vbuffer_ == NULL)
	{
		// Acquire it first, if needed.
		this->vbuffer_ = BillboardsVBuffer::acquire();
		this->vbuffer_->bind(this, &this->trees_, &this->alphas_);
	}
	this->vbuffer_->touch(fogColour, fogNear, fogFar);

	// touch will reset the alphas vector if this is the first 
	// time this chunk is being updated since first last frame. 
	// So, setting the alpha value has got to be after touch.
	MF_ASSERT(treeID >= 0 && treeID < int(this->alphas_.size()));
	const float & minAlpha = BillboardVertex::s_minAlpha;
	const float & maxAlpha = BillboardVertex::s_maxAlpha;
	this->alphas_[treeID]  = 1 + alphaValue * (minAlpha/maxAlpha -1);
}

/**
 *	Updates the optimizer engine (static).
 */
void BillboardOptimiser::update()
{
	BillboardsVBuffer::update();
	if (BillboardOptimiser::s_treeTypes.empty())
	{
		BillboardOptimiser::s_texAggregator->repack();
	}
}

/**
 *	Draws all visible chunks' trees.
 *
 *	@param	frameData	FrameData struct full of this frame's information.
 */
void BillboardOptimiser::drawAll(const FrameData & frameData)
{
	DX::Texture * texture   = BillboardOptimiser::s_texAggregator->texture();
	const Matrix & texXForm = BillboardOptimiser::s_texAggregator->transform();
	BillboardsVBuffer::drawVisible(frameData, texture, texXForm);
}
		
/**
 *	Releases the buffer object.
 */
void BillboardOptimiser::releaseBuffer()
{
	this->vbuffer_ = NULL;
}

/**
 *	Increments the reference count.
 */
void BillboardOptimiser::incRef() const
{
	++this->refCount_;
}

/**
 *	Increments the reference count. If tree type is no 
 *	longer referenced, remove it from chunkBBMap and delete it.
 */ 
void BillboardOptimiser::decRef() const
{
	--this->refCount_;
	if (this->refCount_ == 0)
	{
		BillboardOptimiser::s_chunkBBMapLock.grab();
		ChunkBBMap & chunkBBMap = BillboardOptimiser::s_chunkBBMap;
		ChunkBBMap::iterator it = chunkBBMap.find(this->chunk_);
		if (it != s_chunkBBMap.end())
		{
			MF_ASSERT(it->second == this);	
			chunkBBMap.erase(it);		
		}
		BillboardOptimiser::s_chunkBBMapLock.give();
		delete const_cast<BillboardOptimiser*>(this);
	}		
}

/**
 *	Returns the current reference count.
 */
int BillboardOptimiser::refCount() const
{
	return this->refCount_;
}

/**
 *	Constructor (private).
 *
 *	@param	chunk	chunk which this optmizer instance is related to.
 */
BillboardOptimiser::BillboardOptimiser(const Chunk * chunk) :
	vbuffer_(NULL),
	chunk_(chunk),
	trees_(),
	alphas_(),
	freeSlots_(),
	refCount_(0)
{}

/**
 *	Destructor.
 */
BillboardOptimiser::~BillboardOptimiser()
{
	if (this->vbuffer_ != NULL)
	{
		this->vbuffer_->bind(0, 0, 0);
	}
}


BillboardOptimiser::ChunkBBMap BillboardOptimiser::s_chunkBBMap;
SimpleMutex BillboardOptimiser::s_chunkBBMapLock;

// Configuration parameters
int BillboardOptimiser::s_atlasMinSize   = 2048;
int BillboardOptimiser::s_atlasMaxSize   = 2048;
int BillboardOptimiser::s_atlasMipLevels = 4;
int BillboardOptimiser::s_nextTreeTypeId = 0;

BillboardOptimiser::TreeTypeMap BillboardOptimiser::s_treeTypes;

bool BillboardOptimiser::s_saveTextureAtlas = false;
Moo::TextureAggregator* BillboardOptimiser::s_texAggregator = NULL;

// ----------------------------------------------------------- BillboardsVBuffer

/**
 *	Initializes BillboardsVBuffer (static).
 *
 *	@param	section	XML section containing all initialization settings for 
 *					the optimiser engine (usually read from speedtree.xml).
 *	@param	effect	the effect material to use when rendering the billboards.
 */
void BillboardsVBuffer::init(
	DataSectionPtr        section, 
	Moo::EffectMaterial * effect)
{
	BillboardsVBuffer::s_effect = effect;
	

	MF_WATCH("SpeedTree/BB Optimiser/Visible buffers", 
		BillboardsVBuffer::s_visibleCounter, Watcher::WT_READ_ONLY, 
		"Number of buffers in pool that are currently visible" );
		
	MF_WATCH("SpeedTree/BB Optimiser/Active buffers", 
		BillboardsVBuffer::s_activeCounter, Watcher::WT_READ_ONLY, 
		"Number of buffers in pool that are currently "
		"active (these are not available for recycling).");

	MF_WATCH("SpeedTree/BB Optimiser/Pool size", 
		BillboardsVBuffer::s_poolSize, Watcher::WT_READ_ONLY, 
		"Total number of buffers in pool. This number "
		"minus the number of active buffers is the total "
		" of buffers currently available for recycling." );

	MF_WATCH("SpeedTree/BB Optimiser/Munge Normal Depth",
		s_mungeNormalDepth, Watcher::WT_READ_WRITE,
		"Depth of normal munging point - the point from which "
		"lighting normals are calculated for billboards." );

	if (section.exists())
	{
		DataSectionPtr buffSection = section->openSection("buffers");
		BillboardsVBuffer::s_framesToRecycle = buffSection->readInt(
			"framesToRecycle", BillboardsVBuffer::s_framesToRecycle);
			
		BillboardsVBuffer::s_framesToDelete = buffSection->readInt(
			"framesToDelete", BillboardsVBuffer::s_framesToDelete);
	}
			
	MF_WATCH("SpeedTree/BB Optimiser/Frames to recycle", 
		BillboardsVBuffer::s_framesToRecycle, Watcher::WT_READ_WRITE, 
		"Number of frames a billboard optimiser buffer "
		"needs to go untouched before it is set for recycling" );
	
	MF_WATCH("SpeedTree/BB Optimiser/Frames to delete", 
		BillboardsVBuffer::s_framesToDelete, Watcher::WT_READ_WRITE, 
		"Number of frames a billboard optimiser buffer "
		"needs to go untouched before it gets deleted" );
}

/**
 *	Acquires a buffer instace. First, tries to reuse an inactive buffer 
 *	(defined as one that's gone unused for more than s_framesToRecycle 
 *	frames). If none is available, creates a new one.
 */
BillboardsVBuffer * BillboardsVBuffer::acquire()
{
	// if pool is empty or all buffers in pool
	// are still active, add more buffers to it
	const int & curFrameStamp = BillboardsVBuffer::s_curFrameStamp;
	const int & framesToRecycle = BillboardsVBuffer::s_framesToRecycle;
	BillboardsVBufferList & pool = BillboardsVBuffer::s_buffersPool;
	if (pool.empty() || 
		pool.back()->frameStamp_ >= curFrameStamp-framesToRecycle)
	{
		pool.push_back(new BillboardsVBuffer);
	}

	BillboardsVBufferList::iterator poolIt = pool.end();
	--poolIt;
	BillboardsVBufferPtr buffers = *poolIt;
	buffers->poolIt_ = poolIt;
	return buffers.getObject();
}

/**
 *	Updates the billboards buffers (static).
 */
void BillboardsVBuffer::update()
{
	BillboardsVBuffer::recycleBuffers();
	++BillboardsVBuffer::s_curFrameStamp;
}

/**
 *	Resets all existing buffers.
 */
void BillboardsVBuffer::resetAll()
{
	typedef BillboardsVBufferList::iterator iterator;
	iterator buffersIt  = BillboardsVBuffer::s_buffersPool.begin();
	iterator buffersEnd = BillboardsVBuffer::s_buffersPool.end();
	while (buffersIt != buffersEnd)
	{
		(*buffersIt)->reset();
		++buffersIt;
	}
}

/**
 *	Recycles the billboards bufferss (static). Delete all buffers that 
 *	have done unused for more than framesToDelete frames.
 */
void BillboardsVBuffer::recycleBuffers()
{
	typedef BillboardsVBufferList::iterator iterator;
	BillboardsVBufferList & buffersPool = BillboardsVBuffer::s_buffersPool;
	iterator buffersIt  = buffersPool.begin();
	iterator buffersEnd = buffersPool.end();
	const int & curFrameStamp = BillboardsVBuffer::s_curFrameStamp;
	const int & framesToRecycle = BillboardsVBuffer::s_framesToRecycle;
	const int & framesToDelete  = BillboardsVBuffer::s_framesToDelete;
	int visibleCounter = 0;
	int activeCounter  = 0;
	while (buffersIt != buffersEnd)
	{
		if ((*buffersIt)->frameStamp_ == curFrameStamp)
		{
			++visibleCounter;
			++activeCounter;
		}
		else if ((*buffersIt)->frameStamp_ > curFrameStamp-framesToRecycle)
		{
			++activeCounter;
		}
		else if ((*buffersIt)->frameStamp_ < curFrameStamp-framesToDelete)
		{
			buffersPool.erase(buffersIt, buffersEnd);
			break;
		}
		++buffersIt;
	}
	BillboardsVBuffer::s_visibleCounter = visibleCounter;
	BillboardsVBuffer::s_activeCounter  = activeCounter;
	BillboardsVBuffer::s_poolSize       = buffersPool.size();
}

/**
 *	Draws all currently visible buffers (static).
 *
 *	@param	frameData	FrameData struct full of this frame's information.
 *	@param	texture		the texture atlas.
 */
void BillboardsVBuffer::drawVisible(
	const FrameData & frameData, 
	DX::Texture     * texture,
	const Matrix    & texTransform)
{
	BillboardsVBuffer::s_effect->hTechnique("billboards_opt", 0);

	ComObjectWrap<ID3DXEffect> pEffect = 
		BillboardsVBuffer::s_effect->pEffect()->pEffect();
		
	pEffect->SetTexture("g_diffuseMap", texture);
	pEffect->SetMatrix("g_viewProj", &frameData.viewProj);
	pEffect->SetBool("g_texturedTrees", frameData.texturedTrees);
	
	float uvScale[] = {texTransform._11, texTransform._22, 0, 0};
	pEffect->SetVector("g_UVScale", (D3DXVECTOR4*)uvScale);

	float cameraDir[] = { 
		frameData.invView._31, frameData.invView._32, 
		frameData.invView._33, 1.0f };
	pEffect->SetVector("g_cameraDir", (D3DXVECTOR4*)cameraDir);
	
	float sun[] = { 
		frameData.sunDirection.x, frameData.sunDirection.y, 
		frameData.sunDirection.z, 1.0f,
		frameData.sunDiffuse.r, frameData.sunDiffuse.g, 
		frameData.sunDiffuse.b, 1.0f,
		frameData.sunAmbient.r, frameData.sunAmbient.g, 
		frameData.sunAmbient.b, 1.0f };
	pEffect->SetVectorArray("g_sun", (D3DXVECTOR4*)sun, 3);

	Moo::rc().setFVF(AlphaBillboardVertex::fvf());

	const int & curFrameStamp = BillboardsVBuffer::s_curFrameStamp;
	typedef BillboardsVBufferList::const_iterator citerator;
	citerator buffersIt  = BillboardsVBuffer::s_buffersPool.begin();
	citerator buffersEnd = BillboardsVBuffer::s_buffersPool.end();
	while (buffersIt != buffersEnd && 
		(*buffersIt)->frameStamp_ == curFrameStamp)
	{
		(*buffersIt)->draw();
		++buffersIt;
	}
	
	pEffect->SetTexture("g_diffuseMap", NULL);
}

/**
 *	Touches buffer, effectively flagging it as visible.
 */
void BillboardsVBuffer::touch(
	const Vector4 & fogColour, float fogNear, float fogFar)
{
	if (this->frameStamp_ != BillboardsVBuffer::s_curFrameStamp)
	{
		BillboardsVBufferList & pool = BillboardsVBuffer::s_buffersPool;
		if (this->poolIt_ != pool.begin())
		{
			// move this buffer to the front of the pool
			BillboardsVBufferList::iterator nextIt = this->poolIt_;
			++nextIt;
			pool.splice(pool.begin(), pool, this->poolIt_, nextIt);
			this->poolIt_ = pool.begin();
		}
		std::fill(this->alphas_->begin(), this->alphas_->end(), 1.0f);
		this->frameStamp_ = BillboardsVBuffer::s_curFrameStamp;
		this->fogColour_ = fogColour;
		this->fogNear_ = fogNear;
		this->fogFar_  = fogFar;
	}
}

/**
 *	Draws buffer.
 */
void BillboardsVBuffer::draw()
{
	if (!this->isCompiled())
	{
		this->compile();
	}

	if (this->vbuffer_.isValid())
	{
		Moo::Material::setVertexColour();
		ComObjectWrap<ID3DXEffect> pEffect = 
			BillboardsVBuffer::s_effect->pEffect()->pEffect();

		if (this->alphas_->size())
		{
			D3DXVECTOR4* alphas = (D3DXVECTOR4*)&(*this->alphas_)[0];
			pEffect->SetVectorArray("g_bbAlphaRefs", alphas, 
				int(ceil(float(this->alphas_->size()/4))));
		}
		
		BillboardsVBuffer::s_effect->begin();
		BillboardsVBuffer::s_effect->beginPass(0);

#ifdef EDITOR_ENABLED
			pEffect->SetVector("fogColour", &this->fogColour_);
			pEffect->SetFloat("fogStart", this->fogNear_);
			pEffect->SetFloat("fogEnd",   this->fogFar_);
#endif

		pEffect->CommitChanges();

		this->vbuffer_.activate();
		int primCount = this->vbuffer_.size() / 3;
		Moo::rc().drawPrimitive(D3DPT_TRIANGLELIST, 0, primCount);
		Moo::rc().addToPrimitiveCount(primCount);
		Moo::rc().addToPrimitiveGroupCount();

		BillboardsVBuffer::s_effect->endPass();
		BillboardsVBuffer::s_effect->end();
	}
}

/**
 *	Returns true if buffer is compiled and ready to be drawn.
 */
bool BillboardsVBuffer::isCompiled() const
{
	return this->vbuffer_.isValid();
}

/**
 *	Compiles buffer, making it ready to be drawn. Iterates through all the
 *	trees added to it's owner, creating a vertex buffer in the process.
 */
void BillboardsVBuffer::compile()
{
	MF_ASSERT(!this->vbuffer_.isValid());

	const TreeTypeMap & treeTypes = BillboardOptimiser::s_treeTypes;
	
	// count number of vertices
	int verticesCount = 0;
	typedef TreeInstanceVector::const_iterator citerator;
	citerator bbIt    = this->trees_->begin();
	citerator bbEnd   = this->trees_->end();
	while (bbIt != bbEnd)
	{
		int typeId = bbIt->typeId;
		if (typeId != -1) // skip removed trees
		{
			TreeTypeMap::const_iterator treeTypeIt = treeTypes.find(typeId);
			MF_ASSERT(treeTypeIt != treeTypes.end());
			const BBVertexBuffer & vertices = *treeTypeIt->second.vertices;
			verticesCount += vertices.size();
		}
		++bbIt;
	}
	
	// create vertex buffer
	if (this->vbuffer_.reset(verticesCount) &&
		this->vbuffer_.lock())
	{
		typedef AlphaBillboardVBuffer::iterator abbIterator;
		abbIterator oBegin = this->vbuffer_.begin();
		abbIterator oIt    = oBegin;
		
		// populate vertex buffer
		citerator bbBegin = this->trees_->begin();
		bbIt    = bbBegin;
		bbEnd   = this->trees_->end();
		while (bbIt != bbEnd)
		{
			int bbIndex = std::distance(bbBegin, bbIt);
			typedef BBVertexBuffer::const_iterator bvvIterator;
			
			int typeId = bbIt->typeId;
			if (typeId != -1) // skip removed trees
			{
				TreeTypeMap::const_iterator treeTypeIt = treeTypes.find(typeId);
				const BBVertexBuffer & vertices = *treeTypeIt->second.vertices;
				
				if (vertices.lock())
				{
					bvvIterator iIt  = vertices.begin();
					bvvIterator iEnd = vertices.end();	
					int vertexIdx = 0;
					while (iIt != iEnd)
					{
						oIt->position = bbIt->world.applyPoint(iIt->position);

						//PCWJ - until we have normal mapped lighting on billboards,
						//change the lighting normals for all billboard vertices
						//so they point roughly in an upwards arc.  Do this by
						//taking the vector between an arbitrary point n metres below
						//the origin of the tree.
						
						//new code
						Vector3 mungedNormal( iIt->position );
						mungedNormal -= Vector3(0.f,s_mungeNormalDepth,0.f);
						mungedNormal.normalise();
						oIt->lightNormal = bbIt->world.applyVector(mungedNormal);
						oIt->lightNormal.normalise();
						
						//old code
						//oIt->lightNormal = bbIt->world.applyVector(iIt->lightNormal);
						//oIt->lightNormal.normalise();						
						
						Vector3 alphaNormal(
							iIt->alphaNormal[0], 
							iIt->alphaNormal[1], 
							iIt->alphaNormal[2]);
						alphaNormal = bbIt->world.applyVector(alphaNormal);
						alphaNormal.normalise();
						oIt->alphaNormal[0] = alphaNormal.x;
						oIt->alphaNormal[1] = alphaNormal.y;
						oIt->alphaNormal[2] = alphaNormal.z;
						
						oIt->alphaIndex   = FLOAT(bbIndex/4);
						oIt->alphaMask[0] = bbIndex%4 == 0 ? 1.f : 0.f;
						oIt->alphaMask[1] = bbIndex%4 == 1 ? 1.f : 0.f;
						oIt->alphaMask[2] = bbIndex%4 == 2 ? 1.f : 0.f;
						oIt->alphaMask[3] = bbIndex%4 == 3 ? 1.f : 0.f;

						Vector2 min;
						Vector2 max;		
						switch (vertexIdx % 6)
						{
							case 0:
							{
								// for each 6 vertices, retrieve the current 
								// texture coordinates from the texture aggregator.
								int id = treeTypeIt->second.tileIds[vertexIdx/6];
								BillboardOptimiser::s_texAggregator->getTileCoords(id, min, max);
								oIt->texCoords[0] = max.x;
								oIt->texCoords[1] = min.y;
								break;
							}
							case 1:
								oIt->texCoords[0] = min.x;
								oIt->texCoords[1] = min.y;
								break;
							case 2:
								oIt->texCoords[0] = min.x;
								oIt->texCoords[1] = max.y;
								break;
							case 3:
								oIt->texCoords[0] = max.x;
								oIt->texCoords[1] = min.y;
								break;
							case 4:
								oIt->texCoords[0] = min.x;
								oIt->texCoords[1] = max.y;
								break;
							case 5:
								oIt->texCoords[0] = max.x;
								oIt->texCoords[1] = max.y;
								break;
						};
						++vertexIdx;
						++iIt;
						++oIt;
					}
					vertices.unlock();
				}
				else
				{
					ERROR_MSG(
						"BillboardsVBuffer::compile: "
						"Could not lock vertex buffer\n");
				}
			}
			++bbIt;
		}
		MF_ASSERT(oIt == this->vbuffer_.end());
		this->vbuffer_.unlock();
	}
	else
	{
		ERROR_MSG(
			"BillboardsVBuffer::compile: "
			"Could not create/lock vertex buffer\n");
	}
}

/**
 *	Binds this buffer to the given owner and his data vectors.
 *
 *	@param	owner		the owning optmiser instance.
 *	@param	billboards	its billboard vector (will build the vertex buffer).
 *	@param	alphas		its alpha vector (will upload as shader constants).
 */
void BillboardsVBuffer::bind(
	BillboardOptimiser * owner, 
	TreeInstanceVector * billboards, 
	AlphaValueVector   * alphas)
{
	if (this->owner_ != NULL)
	{
		this->owner_->releaseBuffer();
	}
	this->owner_   = owner;
	this->trees_   = billboards;
	this->alphas_  = alphas;
	this->reset();	
}

/**
 *	Resets the buffer (it will require 
 *	compilation before it can be drawn again).
 */
void BillboardsVBuffer::reset()
{
	this->vbuffer_.reset();
}

/**
 *	Default constructor (private).
 */
BillboardsVBuffer::BillboardsVBuffer() :
	frameStamp_(0),
	fogColour_(1,1,1,1),
	fogNear_(0),
	fogFar_(0),
	owner_(NULL)
{}

/**
 *	Destructor (private).
 */
BillboardsVBuffer::~BillboardsVBuffer()
{
	if (this->owner_ != NULL)
	{
		this->owner_->releaseBuffer();
	}
}

// Static variables
int BillboardsVBuffer::s_curFrameStamp = 0;

// Watched variables
int BillboardsVBuffer::s_visibleCounter = 0;
int BillboardsVBuffer::s_activeCounter  = 0;
int BillboardsVBuffer::s_poolSize       = 0;

// Configuration parameters
int BillboardsVBuffer::s_framesToRecycle = 50;
int BillboardsVBuffer::s_framesToDelete  = 100;

BillboardsVBuffer::BillboardsVBufferList BillboardsVBuffer::s_buffersPool;
Moo::EffectMaterial * BillboardsVBuffer::s_effect = NULL;

namespace { // anonymous

// Helper functions
void saveTextureAtlas(bool save, DX::BaseTexture * texture)
{
	if (save)
	{
		HRESULT hresult = D3DXSaveTextureToFile(
			"bboptimizer.dds", D3DXIFF_DDS, texture, NULL);
	}
}

} // namespace anonymous
} // namespace speedtree

#endif // SPEEDTREE_SUPPORT ----------------------------------------------------
