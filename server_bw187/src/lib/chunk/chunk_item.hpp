/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CHUNK_ITEM_HPP
#define CHUNK_ITEM_HPP

#include "cstdmf/smartpointer.hpp"
#include "resmgr/datasection.hpp"
#include "umbra_config.hpp"

class Chunk;
class ChunkSpace;
class Vector3;
class BoundingBox;

#if UMBRA_ENABLE
#include "umbra_proxies.hpp"
#endif

/**
 *	This class is the base class of all the things that can live in a chunk.
 *	It declares only the methods common to the client and the editor for the
 *	management of a chunk.
 *
 *	@see EditorChunkItem
 *	@see ClientChunkItem
 */
class ChunkItemBase
{
public:
#ifdef _RELEASE
	ChunkItemBase( int wantFlags = 0 ) :
		__count( 0 ), wantFlags_( wantFlags ), pChunk_( NULL ) {}
	ChunkItemBase( const ChunkItemBase & oth ) :
		__count( 0 ), wantFlags_( oth.wantFlags_ ), pChunk_( oth.pChunk_ ) {}
	virtual ~ChunkItemBase() {}
#else
	ChunkItemBase( int wantFlags = 0 );
	ChunkItemBase( const ChunkItemBase & oth );
	virtual ~ChunkItemBase();
#endif

	virtual void toss( Chunk * pChunk ) { this->chunk( pChunk ); }
	virtual void draw() { }
	virtual void tick( float /*dTime*/ ) { }
	virtual void sway( const Vector3 & /*src*/, const Vector3 & /*dst*/, const float /*radius*/ ) { }
	virtual void lend( Chunk * /*pLender*/ ) { }
	virtual void nest( ChunkSpace * /*pSpace*/ ) { }

	virtual const char * label() const				{ return ""; }

	virtual void incRef() const		{ ++__count; }
	virtual void decRef() const		{ if(--__count == 0) delete this; }


    Chunk * chunk() const			{ return pChunk_; }
	void chunk( Chunk * pChunk )	{ pChunk_ = pChunk; }

	bool wantsDraw() const { return !!(wantFlags_ & 1); }
	bool wantsTick() const { return !!(wantFlags_ & 2); }
	bool wantsSway() const { return !!(wantFlags_ & 4); }
	bool wantsNest() const { return !!(wantFlags_ & 8); }

	uint32 drawMark() const { return drawMark_; }
	void drawMark( uint32 val )
		{ drawMark_ = val; }

	char userFlags() const { return char(wantFlags_ >> 8); }

	virtual bool addYBounds( BoundingBox& bb ) const	{	return false;	}
#if UMBRA_ENABLE
	UmbraObjectProxyPtr pUmbraObject() const { return pUmbraObject_; }
#endif
private:
	ChunkItemBase & operator=( const ChunkItemBase & other );	// undefined
	mutable int __count;
	int	wantFlags_;
	uint32 drawMark_;

protected:
	Chunk * pChunk_;

#if UMBRA_ENABLE
	UmbraModelProxyPtr	pUmbraModel_;
	UmbraObjectProxyPtr	pUmbraObject_;
#endif

	void lendByBoundingBox( Chunk * pLender, const BoundingBox & worldbb );
};


#ifdef EDITOR_ENABLED
#include "editor_chunk_item.hpp"
#else

	/**
	 *	This class declares the extra data and methods that the client
	 *	requires all its chunk items to have. Currently it is empty.
	 */
	class ClientChunkItem : public ChunkItemBase
	{
	public:
		ClientChunkItem( int wantFlags = 0 ) : ChunkItemBase( wantFlags ) { }
	};

	typedef ClientChunkItem SpecialChunkItem;
#endif

class ChunkItem : public SpecialChunkItem
{
public:
	ChunkItem( int wantFlags = 0 ) : SpecialChunkItem( wantFlags ) { }
	virtual void syncInit() {}
};

typedef SmartPointer<ChunkItem>	ChunkItemPtr;



/**
 *	This class is a factory for chunk items. It is up to the
 *	factory's 'create' method to actually add the item to the
 *	chunk (as a dynamic or static item - or not at all).
 *
 *	This class may be used either by deriving from it and overriding
 *	the 'create' method, or by passing a non-NULL creator function
 *	into the constructor.
 */
class ChunkItemFactory
{
public:
	class Result
	{
		bool succeeded_;
		bool onePerChunk_;
		ChunkItemPtr item_;
		std::string errorString_;
	protected:
		Result() : succeeded_( true )
		{}
	public:
		explicit Result( ChunkItemPtr item,
				const std::string& errorString = "",
				bool onePerChunk = false ) :
			succeeded_( item ),
			onePerChunk_( onePerChunk ),
			item_( item ),
			errorString_( errorString )
		{}
		operator bool() const
		{
			return succeeded_;
		}
		ChunkItemPtr operator->()
		{
			return item_;
		}
		ChunkItemPtr item()
		{
			return item_;
		}
		bool onePerChunk() const
		{
			return onePerChunk_;
		}
		const std::string& errorString() const
		{
			return errorString_;
		}
	};
	// This class is used when you return a value which indicates
	// a successful loading while no item is returned
	class SucceededWithoutItem : public Result
	{};

	typedef Result (*Creator)( Chunk * pChunk, DataSectionPtr pSection );

	ChunkItemFactory( const std::string & section,
		int priority = 0,
		Creator creator = NULL );
	virtual ~ChunkItemFactory() {};

	virtual Result create( Chunk * pChunk, DataSectionPtr pSection ) const;

	int priority() const							{ return priority_; }
private:
	int		priority_;
	Creator creator_;
};


/**
 *	This macro is used to declare a class as a chunk item. It is used to
 *	implement the factory functionality. It should appear in the declaration of
 *	the class.
 *
 *	Classes using this macro should implement the load method and also use
 *	the IMPLEMENT_CHUNK_ITEM macro.
 */
#define DECLARE_CHUNK_ITEM( CLASS )														\
	static ChunkItemFactory::Result create( Chunk * pChunk, DataSectionPtr pSection );	\
	static ChunkItemFactory factory_;													\

/// declare an alias of a existing class
#define DECLARE_CHUNK_ITEM_ALIAS( CLASS, ALIAS )										\
	static ChunkItemFactory ALIAS##factory_;

/**
 *	This macro is used to implement some of the chunk item functionality of a
 *	class that has used the DECLARE_CHUNK_ITEM macro.
 *  NOTE: the load method should set the 'errorString' when it fails by passing
 *	a reference to the errorString variable in IMPLEMENT_CHUNK_ITEM_ARGS
 */
#define IMPLEMENT_CHUNK_ITEM( CLASS, LABEL, PRIORITY )									\
	ChunkItemFactory CLASS::factory_( #LABEL, PRIORITY, CLASS::create );				\
																						\
	ChunkItemFactory::Result CLASS::create( Chunk * pChunk, DataSectionPtr pSection )	\
	{																					\
		CLASS * pItem = new CLASS();													\
																						\
		std::string errorString; 														\
		if (pItem->load IMPLEMENT_CHUNK_ITEM_ARGS)										\
		{																				\
			pChunk->addStaticItem( pItem );												\
			return ChunkItemFactory::Result( pItem );									\
		}																				\
																						\
		delete pItem;																	\
		return ChunkItemFactory::Result( NULL, errorString );							\
	}																					\

/// This macro can be redefined if your load method takes different arguments
#define IMPLEMENT_CHUNK_ITEM_ARGS (pSection)

/// implement an alias of a existing class
#define IMPLEMENT_CHUNK_ITEM_ALIAS( CLASS, LABEL, PRIORITY )							\
	ChunkItemFactory CLASS::LABEL##factory_( #LABEL, PRIORITY, CLASS::create );

#endif // CHUNK_ITEM_HPP
