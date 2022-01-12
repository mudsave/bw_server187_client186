/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef VISUAL_CHANNELS_HPP
#define VISUAL_CHANNELS_HPP

#include "visual.hpp"
#include "effect_state_manager.hpp"

namespace Moo
{
/**
 * This class is the base class and the manager for the visual channels.
 */
class VisualChannel
{
public:
	virtual ~VisualChannel(){};
	virtual void addItem( VertexSnapshotPtr pVSS, PrimitivePtr pPrimitives, 
		EffectMaterial* pMaterial, uint32 primitiveGroup, float zDistance, 
		DX::VertexBuffer* pStaticVertexColours ) = 0;

	virtual void clear() = 0;

	static void add( const std::string& name, VisualChannel* pChannel );
	static void remove( const std::string& name );
	static VisualChannel* get( const std::string& name );
	static void initChannels();
	static void clearChannelItems();
	static bool enabled() { return enabled_; }
	static void enabled(bool enabled) { enabled_ = enabled; }
private:
	typedef std::map< std::string, VisualChannel* > VisualChannels;
	static VisualChannels visualChannels_;
protected:
	static bool enabled_;
};


/* This is the base class for the primitives drawn by the visual channels.
 */
class ChannelDrawItem
{
public:
	virtual ~ChannelDrawItem(){}
	virtual void draw() = 0;
	virtual void fini() {};
	float distance() const { return distance_; }
	void alwaysVisible(bool val) {bAlwaysVisible_=val;}
	bool alwaysVisible() const {return bAlwaysVisible_;}
protected:
	float			distance_;
	bool			bAlwaysVisible_;
};

/* This class implements the draw item used by visuals.
 */
 
class VisualDrawItem : public ChannelDrawItem
{
public:
	void init( VertexSnapshotPtr pVSS, PrimitivePtr pPrimitives, 
		SmartPointer<StateRecorder> pStateRecorder, uint32 primitiveGroup, 
		float distance, DX::VertexBuffer* pStaticVertexColours = NULL, 
		bool sortInternal = false );
	virtual void draw();
	virtual void fini();

	static VisualDrawItem* get();
	static bool s_overrideZWrite_;
	static bool overrideZWrite() { return s_overrideZWrite_; }
	static void overrideZWrite(bool val) { s_overrideZWrite_=val; }
protected:
	void drawSorted();
	void drawUnsorted();
	VertexSnapshotPtr pVSS_;
	PrimitivePtr	pPrimitives_;
	uint32			primitiveGroup_;
	Vector3			partialWorldView_;
	SmartPointer<StateRecorder> pStateRecorder_;
	ComObjectWrap< DX::VertexBuffer > pStaticVertexColours_;

	bool			sortInternal_;

	static uint32 s_nextAlloc_;
	static std::vector< VisualDrawItem* > s_drawItems_;
};

class ShimmerDrawItem : public VisualDrawItem
{
public:
	void draw();
	static ShimmerDrawItem* get();
protected:
	static uint32 s_nextAlloc_;
	static std::vector< ShimmerDrawItem* > s_drawItems_;
};

/*
 * This class implements the shimmer channel, the shimmer channel is used to add a wavy effect to the scene,
 * shimmer visuals are rendered after the main scene draw but before the sorted channel is rendered.
 */
class ShimmerChannel : public VisualChannel
{
public:
	virtual void addItem( VertexSnapshotPtr pVSS, PrimitivePtr pPrimitives, 
		EffectMaterial* pMaterial, uint32 primitiveGroup, float zDistance, 
		DX::VertexBuffer* pStaticVertexColours );
	virtual void clear();
	static void draw();
	static void addDrawItem( ChannelDrawItem* pItem );
protected:
	typedef VectorNoDestructor< ChannelDrawItem* >	DrawItems;
	static DrawItems s_drawItems_;
	static uint32 s_timeStamp_;
};

/*
 * This class implements the sorted channel, sorted objects are rendered after the main scene and the shimmer.
 */
class SortedChannel : public VisualChannel
{
public:
	virtual void addItem( VertexSnapshotPtr pVSS, PrimitivePtr pPrimitives, 
		EffectMaterial* pMaterial, uint32 primitiveGroup, float zDistance, 
		DX::VertexBuffer* pStaticVertexColours );
	static void draw(bool clear=true);
	virtual void clear();
	static void addDrawItem( ChannelDrawItem* pItem );

	static void push();
	static void pop();

	static void reflecting(bool val) { s_reflecting_ = val; }
protected:
	typedef VectorNoDestructor< ChannelDrawItem* >	DrawItems;
	typedef std::vector< DrawItems >				DrawItemStack;
	static DrawItems& drawItems() { return s_drawItems_.back(); }
	static DrawItemStack	s_drawItems_;
	static uint32 s_timeStamp_;
	static bool s_reflecting_;
};

/*
 * This class implements the internal sorted channel, the internal sorted channel is rendered with the sorted channel,
 * with it's triangles sorted agains eachother.
 */
class InternalSortedChannel : public SortedChannel
{
public:
	virtual void addItem( VertexSnapshotPtr pVSS, PrimitivePtr pPrimitives, 
		EffectMaterial* pMaterial, uint32 primitiveGroup, float zDistance, 
		DX::VertexBuffer* pStaticVertexColours );

protected:
};

/*
 * This class implements the sorted shimmer channel, this channel draws to both the main buffer and the
 * alpha buffer.
 */
class SortedShimmerChannel : public SortedChannel
{
public:
	virtual void addItem( VertexSnapshotPtr pVSS, PrimitivePtr pPrimitives, 
		EffectMaterial* pMaterial, uint32 primitiveGroup, float zDistance, 
		DX::VertexBuffer* pStaticVertexColours );
};


}
#endif // VISUAL_CHANNELS_HPP
