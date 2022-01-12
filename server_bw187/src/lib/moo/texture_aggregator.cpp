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
#include "texture_aggregator.hpp"

#include "wingdi.h"

DECLARE_DEBUG_COMPONENT2("Moo", 0)


namespace Moo
{

// -----------------------------------------------------------------------------
// Section: TextureAggregatorPimpl
// -----------------------------------------------------------------------------

struct TextureAggregatorPimpl : DeviceCallback, Aligned
{
	TextureAggregatorPimpl();		

	struct Slot : public ReferenceCount
	{
		Slot(int xx, int xy, int ww, int hh) :
			x(xx),
			y(xy),
			width(ww),
			height(hh)
		{}
		
		void scale(int scaleX, int scaleY)
		{
			x /= scaleX;
			y /= scaleY;
			width  /= scaleX;
			height /= scaleY;
		}
	
		int x;
		int y;
		int width;
		int height;
	};
	typedef SmartPointer<Slot> SlotPtr;
	
	struct Tile
	{
		Tile() {}
		Tile(BaseTexturePtr tx, const Vector2 & mn, const Vector2 & mx, SlotPtr sl) :
			tex(tx),
			min(mn),
			max(mx),
			slot(sl)
		{}
		
		BaseTexturePtr tex;
		Vector2        min;
		Vector2        max;
		SlotPtr        slot;
	};

	typedef std::vector<SlotPtr> SlotVector;
	typedef std::map<int, SlotVector> SlotVectorMap;
	typedef std::map<int, Tile> TilesMap;
	
	typedef ComObjectWrap<DX::Texture> DXTexturePtr;
	
	SlotPtr getSlot(int log2);
	void makeSlots(int log2);
	bool needsToGrow(int log2) const;
	SlotVector growTexture(int log2Hint);
	DXTexturePtr createTexture(int width, int height);
	void copyWholeTexture(DXTexturePtr src, DXTexturePtr dst);

	int registerTile(
		BaseTexturePtr tex, const Vector2 & min, 
		const Vector2 & max, SlotPtr slot);
		
	void renderTile(
		BaseTexturePtr  tex, const Vector2 & min, 
		const Vector2 & max, SlotPtr slot);

	void recycleSlot(SlotPtr slot);
	bool areNeightboursFree(SlotPtr slot, SlotVector & slots);
	bool isFree(SlotPtr slot);
	SlotPtr findSlot(int log2, int coordx, int coordy);
	void killSlot(SlotPtr slot);
	float textureUsage() const;

	void repackLater();
	void repackCheck();
	void repack();
	
	virtual void createUnmanagedObjects();
	virtual void deleteUnmanagedObjects();
	
	DXTexturePtr  texture;
	Matrix        transform;
	SlotVectorMap freeSlots;
	TilesMap      tiles;
	int           nextId;
	bool		  tilesReset;
	bool          repackPending;
	int           minSize;
	int           maxSize;
	int           mipLevels;
	
	TextureAggregator::ResetNotifyFunc resetNotifyFunc;
};

typedef TextureAggregatorPimpl::SlotPtr SlotPtr;

namespace // anonymous
{

// Name constants
const int c_RefSize           = 1024;
const int c_MinSize           = 128;
const int c_MaxSize           = 4096;
const int c_MipLevels         = 4;
const int c_TileBorder        = 1;
const float c_RepackThreshold = 0.35f;

// Helper functions
int sizeToLog2(int size);
int log2ToSize(int log2);
TextureAggregatorPimpl::SlotVector subdivideSlot(SlotPtr slot);
int texWidth(TextureAggregatorPimpl::DXTexturePtr tex);
int texHeight(TextureAggregatorPimpl::DXTexturePtr tex);
void copyTile(
	int               srcWidth, 
	int               srcHeight,
	DX::BaseTexture * src, 
	const Vector2   & pSrcMin, 
	const Vector2   & pSrcMax,
	DX::Texture     * dst, 
	const Vector2   & pDstMin, 
	const Vector2   & pDstMax,
	bool              cropBorders);


} // namespace anonymous

// -----------------------------------------------------------------------------
// Section: TextureAggregator
// -----------------------------------------------------------------------------

TextureAggregator::TextureAggregator(ResetNotifyFunc notifyFunc) :
	pimpl_(new TextureAggregatorPimpl)
{
	this->pimpl_->resetNotifyFunc = notifyFunc;
}

TextureAggregator::~TextureAggregator()
{}

int TextureAggregator::addTile(
	BaseTexturePtr  tex, 
	const Vector2 & min, 
	const Vector2 & max)
{
	this->pimpl_->repackCheck();

	int texWidth  = tex->width();
	int texHeight = tex->height();
	int tileX = int(min.x * texWidth);
	int tileY = int(min.y * texHeight);
	int tileWidth  = int((max.x-min.x) * texWidth);
	int tileHeight = int((max.y-min.y) * texHeight);
	
	if (tileWidth != tileHeight || tileWidth <= 0)
	{
		throw InvalidArgumentError("Texture tile must be non-empty and square.");
	}
	
	int log2 = sizeToLog2(tileWidth);
	SlotPtr slot = this->pimpl_->getSlot(log2);
	
	return this->pimpl_->registerTile(tex, min, max, slot);
}

void TextureAggregator::getTileCoords(int id, Vector2 & min, Vector2 & max) const
{
	TextureAggregatorPimpl::TilesMap::const_iterator tileIt;
	tileIt = this->pimpl_->tiles.find(id);
	MF_ASSERT(tileIt != this->pimpl_->tiles.end());
	SlotPtr slot = tileIt->second.slot;
	
	min.x = float(slot->x);
	min.y = float(slot->y);
	max.x = float(slot->x+slot->width);
	max.y = float(slot->y+slot->height);
}
	
void TextureAggregator::delTile(int id)
{
	TextureAggregatorPimpl::TilesMap::iterator tileIt;
	tileIt = this->pimpl_->tiles.find(id);
	MF_ASSERT(tileIt != this->pimpl_->tiles.end());
	SlotPtr slot = tileIt->second.slot;
	this->pimpl_->tiles.erase(tileIt);
	this->pimpl_->recycleSlot(slot);

	// there is no need to repack the 
	// texture if it is not currently active
	if (this->pimpl_->texture.hasComObject())
	{
#define DEBUG_DEL_TILE 0
#if DEBUG_DEL_TILE
			Vector2 dstMin(
				float(slot->x), 
				float(slot->y));
			Vector2 dstMax(
				float(slot->x + slot->width), 
				float(slot->y + slot->height));
				
			copyTile(
				16, 16,
				NULL, Vector2(0, 0), Vector2(16, 16),
				this->pimpl_->texture.pComObject(),
				dstMin, dstMax, true);
#endif
		
		int height = texHeight(this->pimpl_->texture);
		if (this->pimpl_->textureUsage() < c_RepackThreshold && height > c_MinSize)
		{
			// do it later because we are not inside 
			// a begin/end scene block at this point
			this->pimpl_->repackLater();
		}
	}
}

void TextureAggregator::repack()
{
	this->pimpl_->repack();
}

bool TextureAggregator::tilesReset() const
{
	bool result = this->pimpl_->tilesReset;
	this->pimpl_->tilesReset = false;
	return result;
}

DX::Texture * TextureAggregator::texture() const
{
	return this->pimpl_->texture.pComObject();
}

const Matrix & TextureAggregator::transform() const
{
	return this->pimpl_->transform;
}

int TextureAggregator::minSize() const
{
	return this->pimpl_->minSize;
}

void TextureAggregator::setMinSize(int minSize)
{
	this->pimpl_->minSize = minSize;
}

int TextureAggregator::maxSize() const
{
	return this->pimpl_->maxSize;
}

void TextureAggregator::setMaxSize(int maxSize)
{
	this->pimpl_->maxSize = maxSize;
}

int TextureAggregator::mipLevels() const
{
	return this->pimpl_->mipLevels;
}

void TextureAggregator::setMipLevels(int mipLevels)
{
	this->pimpl_->mipLevels = mipLevels;
}

// -----------------------------------------------------------------------------
// Section: TextureAggregatorPimpl
// -----------------------------------------------------------------------------

TextureAggregatorPimpl::TextureAggregatorPimpl() :
	texture(NULL),
	transform(),
	freeSlots(),
	tiles(),
	nextId(0),
	tilesReset(false),
	repackPending(false),
	minSize(c_MinSize),
	maxSize(c_MaxSize),
	mipLevels(c_MipLevels),
	resetNotifyFunc(NULL)
{}

SlotPtr TextureAggregatorPimpl::getSlot(int log2)
{
	TextureAggregatorPimpl::SlotVectorMap::iterator slotVecIt;
	slotVecIt = this->freeSlots.find(log2);
	if (slotVecIt == this->freeSlots.end() || slotVecIt->second.empty())
	{
		this->makeSlots(log2);
		slotVecIt = this->freeSlots.find(log2);
		MF_ASSERT(slotVecIt != this->freeSlots.end());
	}
	SlotPtr slot = slotVecIt->second.back();
	slotVecIt->second.pop_back();
	return slot;
}

void TextureAggregatorPimpl::makeSlots(int log2)
{
	if (this->needsToGrow(log2))
	{
		SlotVector newSlots = this->growTexture(log2);
		int newLog2 = sizeToLog2(newSlots[0]->width);
		SlotVector & slots = this->freeSlots[newLog2];
		slots.insert(slots.end(), newSlots.begin(), newSlots.end());
		if (newLog2 != log2)
		{
			this->makeSlots(log2);
		}
	}
	else
	{
		SlotPtr biggerSlot = this->getSlot(log2+1);
		SlotVector newSlots = subdivideSlot(biggerSlot);
		SlotVector & slots = this->freeSlots[log2];
		slots.insert(slots.end(), newSlots.begin(), newSlots.end());
	}
}

bool TextureAggregatorPimpl::needsToGrow(int log2) const
{
	return
		!this->texture.hasComObject() ||
		log2ToSize(log2) >= int(texWidth(this->texture));
}

TextureAggregatorPimpl::SlotVector 
TextureAggregatorPimpl::growTexture(int log2Hint)
{
	TextureAggregatorPimpl::SlotVector result;
	
	if (!this->texture.hasComObject())
	{
		int sizeHint    = log2ToSize(log2Hint);
		int size        = std::max(c_MinSize, sizeHint);
		this->texture   = this->createTexture(size, size);

		float divFactor = 1.0f/size;
		this->transform = Matrix::identity;
		this->transform._11 = divFactor;
		this->transform._22 = divFactor;
		
		result.push_back(new Slot(0, 0, size, size));
	}
	else
	{
		int scaleX = 1;
		int scaleY = 1;
		int width  = texWidth(this->texture);
		int height = texHeight(this->texture);
		if (width == height)
		{
			scaleY = 2;
		}
		else
		{
			scaleX = 2;
		}
			
		DXTexturePtr newTexture = this->createTexture(
			width * scaleX, height * scaleY);
			
		this->copyWholeTexture(this->texture, newTexture);
		this->texture = newTexture;

		if (scaleX == 1)
		{
			result.push_back(new Slot(0, height, width, width));
			this->transform._22 *= 0.5f;
		}
		else
		{
			result.push_back(new Slot(width, width, width, width));
			result.push_back(new Slot(width, 0, width, width));
			this->transform._11 *= 0.5f;
		}
	}
	return result;
}

TextureAggregatorPimpl::DXTexturePtr TextureAggregatorPimpl::createTexture(
	int width, int height)
{
	DXTexturePtr texture;

	if (width <= this->maxSize && height <= this->maxSize)
	{
		HRESULT hr = rc().device()->CreateTexture(
			width, height, c_MipLevels, D3DUSAGE_RENDERTARGET, 
			D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &texture, NULL);
			
		if (SUCCEEDED(hr))
		{
			for (int i=0; i<c_MipLevels; ++i)
			{
				DX::Surface *surface;
				HRESULT hr = texture->GetSurfaceLevel(i, &surface);
				if (SUCCEEDED(hr))
				{
					bool result = rc().pushRenderTarget();
					MF_ASSERT(result)
					
					hr = rc().device()->SetRenderTarget(0, surface);
					if (SUCCEEDED(hr))
					{
						rc().device()->SetDepthStencilSurface(NULL);
						rc().device()->Clear( 0, NULL, D3DCLEAR_TARGET, 
							D3DCOLOR_ARGB(0, 0, 0, 0), 1, 0 );
					}
					else
					{
						WARNING_MSG(
							"TextureAggregator::createTexture: "
							"Unable to set render target on device (ERRORCODE=%x)\n", hr);
					}
					surface->Release();
					rc().popRenderTarget();
				}
				else
				{
					ERROR_MSG(
						"TextureAggregator::createTexture: "
						"Could not get surface for texture aggregator (ERRORCODE=%x)\n", hr);
				}
			}
		}
		else
		{
			ERROR_MSG(
				"TextureAggregator::createTexture: "
				"Could not create texture (%dx%d, ERRORCODE=%x)\n", width, height, hr);
		}
	}
	else
	{
		ERROR_MSG(
			"TextureAggregator::createTexture: "
			"Texture reched maximum size (%d)\n", this->maxSize);
	}
	
	if (!texture.hasComObject())
	{
		throw TextureAggregator::DeviceError(
			"TextureAggregator::createTexture: "
			"Unable to create texture");
	}
	
	return texture;
}

void TextureAggregatorPimpl::copyWholeTexture(
	DXTexturePtr src, DXTexturePtr dst)
{
	if (src.hasComObject() && dst.hasComObject())
	{
		int srcWidth  = texWidth(src);
		int srcHeight = texHeight(src);

		int dstWidth  = texWidth(dst);
		int dstHeight = texHeight(dst);
		
		Vector2 minp(0, 0);
		Vector2 maxp(1, 1);
		Vector2 maxo((float)srcWidth, (float)srcHeight);

		copyTile(
			srcWidth, srcHeight,
			src.pComObject(), minp, maxp,
			dst.pComObject(), minp, maxo,
			false);
	}
}

int TextureAggregatorPimpl::registerTile(
	BaseTexturePtr  tex, const Vector2 & min, 
	const Vector2 & max, SlotPtr slot)
{
	this->renderTile(tex, min, max, slot);
	
	int id = this->nextId++; // post-increment is intentional
	this->tiles[id] = Tile(tex, min, max, slot);
	return id;
}

void TextureAggregatorPimpl::renderTile(
	BaseTexturePtr  tex, const Vector2 & min, 
	const Vector2 & max, SlotPtr slot)
{
	if (this->texture.hasComObject())
	{
		Vector2 dstMin(
			float(slot->x), 
			float(slot->y));
		Vector2 dstMax(
			float(slot->x + slot->width),
			float(slot->y + slot->height));

		copyTile(
			tex->width(), tex->height(),
			tex->pTexture(), min, max,
			this->texture.pComObject(), 
			dstMin, dstMax, true);
	}
}

void TextureAggregatorPimpl::recycleSlot(SlotPtr slot)
{
	int log2 = sizeToLog2(slot->width);
	this->freeSlots[log2].push_back(slot);
		
	SlotVector slots;
	if (this->areNeightboursFree(slot, slots))
	{
		SlotVector::iterator slotIt  = slots.begin();
		SlotVector::iterator slotEnd = slots.end();
		while (slotIt != slotEnd)
		{
			this->killSlot(*slotIt);
			++slotIt;
		}
		int log2 = sizeToLog2(slot->width*2);
		SlotPtr baseSlot = slots.front();
		this->recycleSlot(new Slot(
			baseSlot->x, baseSlot->y, 
			baseSlot->width*2, baseSlot->width*2));
	}
}

bool TextureAggregatorPimpl::areNeightboursFree(
	SlotPtr slot, SlotVector & slots)
{
	int width   = slot->width;
	int log2    = sizeToLog2(width);
	int coordx  = (slot->x / (width*2))*2;
	int coordy  = (slot->y / (width*2))*2;
	bool isFree = true;
	for (int i=0; i<2; ++i)
	{
		for (int j=0; j<2; ++j)
		{
			SlotPtr slot = this->findSlot(log2, coordx+i, coordy+j);
			isFree = isFree && slot.exists() && this->isFree(slot);
			slots.push_back(slot);
		}
	}
	return isFree;
}

bool TextureAggregatorPimpl::isFree(SlotPtr slot)
{
	int log2 = sizeToLog2(slot->width);
	SlotVector slots = this->freeSlots[log2];
	return std::find(slots.begin(), slots.end(), slot) != slots.end();
}

SlotPtr TextureAggregatorPimpl::findSlot(int log2, int coordx, int coordy)
{
	int width = log2ToSize(log2);
	int x = coordx * width;
	int y = coordy * width;
	SlotPtr result = NULL;
	
	SlotVector slots = this->freeSlots[log2];
	SlotVector::const_iterator slotIt  = slots.begin();
	SlotVector::const_iterator slotEnd = slots.end();
	while (slotIt != slotEnd)
	{
		if ((*slotIt)->x == x && (*slotIt)->y == y)
		{
			result = *slotIt;
			break;
		}
		++slotIt;
	}
	return result;
}

void TextureAggregatorPimpl::killSlot(SlotPtr slot)
{
	int log2 = sizeToLog2(slot->width);
	SlotVector & slots = this->freeSlots[log2];
	SlotVector::iterator slotIt  = slots.begin();
	SlotVector::iterator slotEnd = slots.end();
	while (slotIt != slotEnd)
	{
		if (*slotIt == slot)
		{	
			(*slotIt) = slots.back();
			slots.pop_back();
			break;
		}
		++slotIt;
		}
}

float TextureAggregatorPimpl::textureUsage() const
{
	float result = 0;
	if (this->texture.hasComObject())
	{
		int width  = texWidth(this->texture);
		int height = texHeight(this->texture);

		int totalArea = width * height;
		int usedArea  = 0;
		
		typedef TextureAggregatorPimpl::TilesMap::const_iterator iterator;
		iterator tileIt  = this->tiles.begin();
		iterator tileEnd = this->tiles.end();
		while (tileIt != tileEnd)
		{
			usedArea += tileIt->second.slot->width * tileIt->second.slot->height;
			++tileIt;
		}
		result = float(usedArea) / totalArea;
	}	
	return result;
}

void TextureAggregatorPimpl::repackLater()
{
	this->repackPending = true;
}

void TextureAggregatorPimpl::repackCheck()
{
	if (this->repackPending)
	{
		// repack resets repackPending
		this->repack();
	}
}

void TextureAggregatorPimpl::repack()
{
	this->texture = NULL;
	this->freeSlots.clear();
	
	typedef TextureAggregatorPimpl::TilesMap::iterator iterator;
	iterator tileIt  = this->tiles.begin();
	iterator tileEnd = this->tiles.end();
	while (tileIt != tileEnd)
	{
		int log2 = sizeToLog2(tileIt->second.slot->width);
		
		try
		{
			tileIt->second.slot = this->getSlot(log2);
		}
		catch (const TextureAggregator::DeviceError &)
		{
			ERROR_MSG(
				"TextureAggregatorPimpl::repack: "
				"getSlot failed during repack.\n"
				"Not enough video memory is the most likely cause."
				"You may experience weird rendering artifacts.");
			MF_ASSERT(false);
		}
		
		this->renderTile(
			tileIt->second.tex, tileIt->second.min, 
			tileIt->second.max, tileIt->second.slot);
		++tileIt;
	}

	if (this->resetNotifyFunc != NULL)
	{
		this->resetNotifyFunc();	
	}
	else
	{
		this->tilesReset = true;
	}
	this->repackPending = false;
}

void TextureAggregatorPimpl::deleteUnmanagedObjects()
{
	this->texture = NULL;
	this->freeSlots.clear();
}

void TextureAggregatorPimpl::createUnmanagedObjects()
{
	rc().beginScene();
	this->repack();
	rc().endScene();
}

namespace // anonymous
{

// -----------------------------------------------------------------------------
// Section: Helper functions
// -----------------------------------------------------------------------------

int sizeToLog2(int size)
{
	int result = 1;
	while ((1 << result) < size)
	{
		++result;
	}
	return result;
}

int log2ToSize(int log2)
{
	return 1 << log2;
}

TextureAggregatorPimpl::SlotVector subdivideSlot(SlotPtr slot)
{
	int height = slot->height >> 1;
	int width  = slot->width  >> 1;
	int x = slot->x;
	int y = slot->y;
	
	typedef TextureAggregatorPimpl::Slot Slot;
	SlotPtr slot1 = new Slot(x+width, y, height, width);
	SlotPtr slot2 = new Slot(x, y, height, width);
	SlotPtr slot3 = new Slot(x+width, y+height, height, width);
	SlotPtr slot4 = new Slot(x, y+height, height, width);
	
	TextureAggregatorPimpl::SlotVector result;
	result.push_back(slot1);
	result.push_back(slot2);
	result.push_back(slot3);
	result.push_back(slot4);
	return result;
}

int texWidth(TextureAggregatorPimpl::DXTexturePtr tex)
{
	D3DSURFACE_DESC desc;
	tex->GetLevelDesc(0, &desc);
	return desc.Width;
}

int texHeight(TextureAggregatorPimpl::DXTexturePtr tex)
{
	D3DSURFACE_DESC desc;
	tex->GetLevelDesc(0, &desc);
	return desc.Height;
}

void copyTile(
	int               srcWidth, 
	int               srcHeight,
	DX::BaseTexture * src, 
	const Vector2   & pSrcMin, 
	const Vector2   & pSrcMax,
	DX::Texture     * dst, 
	const Vector2   & pDstMin, 
	const Vector2   & pDstMax,
	bool              cropBorders)
{
	if (rc().device()->TestCooperativeLevel() != D3D_OK)
	{
		ERROR_MSG("TextureAggregator: device lost when to copying tile\n");
		return;
	}

	float w0d2 = texWidth(dst)/2.0f;
	float h0d2 = texHeight(dst)/2.0f;
				
	int srcLevelCount = src != NULL ? src->GetLevelCount() : 0xffff;
	int texMipLevels = std::min(srcLevelCount, int(dst->GetLevelCount()));
	for (int i=0; i<texMipLevels; ++i)
	{
		float tileBorder = cropBorders ? float(c_TileBorder << i) : 0.0f;
		float wPixel     = tileBorder / (srcWidth << i);
		float hPixel     = tileBorder / (srcHeight << i);
		
		Vector2 srcMin(pSrcMin);
		Vector2 srcMax(pSrcMax);
		srcMin.x += wPixel;
		srcMin.y += hPixel;
		srcMax.x -= wPixel;
		srcMax.y -= hPixel;

		Vector2 dstMin(pDstMin);
		Vector2 dstMax(pDstMax);
		dstMin.x += tileBorder;
		dstMin.y += tileBorder;
		dstMax.x -= tileBorder;
		dstMax.y -= tileBorder;

		DX::Surface *surface;
		HRESULT hr = dst->GetSurfaceLevel(i, &surface);
		if (SUCCEEDED(hr))
		{
			bool result = rc().pushRenderTarget();
			MF_ASSERT(result)
			
			hr = rc().device()->SetRenderTarget(0, surface);
			if (SUCCEEDED(hr))
			{
				rc().device()->SetDepthStencilSurface(NULL);
				
				// set view port
				D3DSURFACE_DESC desc;
				dst->GetLevelDesc(i, &desc);
				DX::Viewport vp;
				vp.X = vp.Y = 0;
				vp.MinZ = 0;
				vp.MaxZ = 1;
				vp.Width = desc.Width;
				vp.Height = desc.Height;
				rc().device()->SetViewport(&vp);
				
				// set render states
				rc().setVertexShader(NULL);
				rc().setPixelShader(NULL);

				rc().setRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
				rc().setRenderState(D3DRS_ZENABLE, FALSE);
				rc().setRenderState(D3DRS_LIGHTING, FALSE);

				// TODO: use EffectMaterial here
				Material::setVertexColour();
				rc().setRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
				rc().setRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
				rc().setRenderState(D3DRS_ALPHATESTENABLE, FALSE);
				rc().setRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
				rc().setRenderState(D3DRS_LIGHTING, FALSE);
				rc().setRenderState(D3DRS_ZWRITEENABLE, FALSE);
				rc().setRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
				rc().setRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
				rc().setRenderState(D3DRS_FOGENABLE, FALSE);
				
				rc().setTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
				rc().setTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
				
				rc().setSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
				rc().setSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
				rc().setSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
				rc().setSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
				rc().setSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_POINT);

				// set transforms
				rc().device()->SetTransform(D3DTS_VIEW, &Matrix::identity);
				rc().device()->SetTransform(D3DTS_WORLD, &Matrix::identity);
				rc().device()->SetTransform(D3DTS_TEXTURE0, &Matrix::identity);
				
				Matrix projection;
				projection.orthogonalProjection(
					w0d2*2.0f, h0d2*2.0f, -1.0f, +1.0f);
				
				rc().device()->SetTransform(D3DTS_PROJECTION, &projection);

				DWORD writeState = rc().getRenderState(D3DRS_COLORWRITEENABLE);
				rc().setRenderState(D3DRS_COLORWRITEENABLE, 
					D3DCOLORWRITEENABLE_ALPHA | D3DCOLORWRITEENABLE_RED | 
					D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
					
				// paint background black (it
				// may contain some left-overs)
				{
					typedef VertexXYZL VertexType;
					static VertexType v[4];
					v[0].pos_    = Vector3(pDstMin.x-w0d2, h0d2-pDstMax.y, 0);
					v[0].colour_ = 0;
					v[1].pos_    = Vector3(pDstMin.x-w0d2, h0d2-pDstMin.y, 0);
					v[1].colour_ = 0;
					v[2].pos_    = Vector3(pDstMax.x-w0d2, h0d2-pDstMin.y, 0);
					v[2].colour_ = 0;
					v[3].pos_    = Vector3(pDstMax.x-w0d2, h0d2-pDstMax.y, 0);
					v[3].colour_ = 0;

					rc().setTextureStageState(0, 
						D3DTSS_COLOROP, D3DTOP_SELECTARG1);
					rc().setTextureStageState(0, 
						D3DTSS_COLORARG1, D3DTA_DIFFUSE);
							
					rc().setTextureStageState(0, 
						D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
					rc().setTextureStageState(0, 
						D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);

					rc().setFVF(VertexType::fvf());
					rc().drawPrimitiveUP(
						D3DPT_TRIANGLEFAN, 2, 
						v, sizeof(VertexType));
				}

				// set texture
				{
					rc().setTexture(0, src);

					// render quad
					typedef VertexXYZDUV VertexType;
					static VertexType v[4];
					float offsetX       = 0.5f/srcWidth;
					float offsetY       = 0.5f/srcHeight;
					v[0].pos_    = Vector3(dstMin.x-w0d2, h0d2-dstMax.y, 0);
					v[0].uv_     = Vector2(srcMin.x+offsetX, (srcMax.y+offsetY));
					v[0].colour_ = 0;
					v[1].pos_    = Vector3(dstMin.x-w0d2, h0d2-dstMin.y, 0);
					v[1].uv_     = Vector2(srcMin.x+offsetX, (srcMin.y+offsetY));
					v[1].colour_ = 0;
					v[2].pos_    = Vector3(dstMax.x-w0d2, h0d2-dstMin.y, 0);
					v[2].uv_     = Vector2(srcMax.x+offsetX, (srcMin.y+offsetY));
					v[2].colour_ = 0;
					v[3].pos_    = Vector3(dstMax.x-w0d2, h0d2-dstMax.y, 0);
					v[3].uv_     = Vector2(srcMax.x+offsetX, (srcMax.y+offsetY));
					v[3].colour_ = 0;

					rc().setTextureStageState(0, 
						D3DTSS_COLOROP, D3DTOP_SELECTARG1);
					rc().setTextureStageState(0, D3DTSS_COLORARG1, 
						src != NULL 
							? D3DTA_TEXTURE
							: D3DTA_DIFFUSE);
							
					rc().setTextureStageState(0, 
						D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
					rc().setTextureStageState(0, D3DTSS_ALPHAARG1,
						src != NULL 
							? D3DTA_TEXTURE
							: D3DTA_DIFFUSE);

					rc().setFVF(VertexType::fvf());
					rc().drawPrimitiveUP(
						D3DPT_TRIANGLEFAN, 2, 
						v, sizeof(VertexType));
				}
				
				rc().setRenderState(D3DRS_COLORWRITEENABLE, writeState);
			}
			else
			{
				WARNING_MSG(
					"TextureAggregator::renderTile : "
					"Unable to set render target on device\n");
			}
			surface->Release();
			rc().popRenderTarget();
		}
		else
		{
			WARNING_MSG(
				"TextureAggregator::renderTile : "
				"Could not get surface for texture aggregator\n");
		}
	}
}

} // namespace anonymous

} // namespace moo
