/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef VISUAL_COMPOUND_HPP
#define VISUAL_COMPOUND_HPP

#include "moo/device_callback.hpp"
#include "moo/primitive.hpp"
#include "moo/vertex_declaration.hpp"
#include "moo/effect_material.hpp"

namespace Moo
{

struct VertexXYZNUVTBPC;


class TransformHolder;

/**
 *	This class implements the visual compound, the visual compound tries to 
 *	combine visual instances into one object to limit the number
 *	of draw calls made by the engine.
 *
 */
class VisualCompound : public Aligned, public DeviceCallback
{
public:
	VisualCompound();
	~VisualCompound();

	bool init( const std::string& visualResourceName );
	const std::string& resourceName() { return resourceName_; }

	TransformHolder* addTransform( const Matrix& transform, uint32 batchCookie );
	bool dirty() const { return dirty_; }
	void dirty( bool state ) { dirty_ = state; }
	
	bool draw( EffectMaterial* pMaterialOverride = NULL );
	void updateDrawCookie();

	bool valid() const { return valid_; };

	void deleteUnmanagedObjects();

	typedef std::vector< TransformHolder* > TransformHolders;

	class Batch
	{
	public:
		Batch( VisualCompound* pVisualCompound ) : 
		  drawCookie_( -1 ),
		  pVisualCompound_( pVisualCompound ) {}
		~Batch();
		void draw( uint32 sequence );
		void clearSequences();
		uint32 drawCookie() const { return drawCookie_; }
		TransformHolder* add( const Matrix& transform );
		void del( TransformHolder* transformHolder );
		std::vector<Primitive::PrimGroup> primitiveGroups_;
		TransformHolders transformHolders_;
		uint32	drawCookie_;
		std::vector<uint8> sequences_;
	private:
		VisualCompound* pVisualCompound_;
	};

	class MutexHolder
	{
	public:
		MutexHolder();
		~MutexHolder();
	};

	static VisualCompound* get( const std::string& visualName );
	static TransformHolder* add( const std::string& visualName, const Matrix& transform, uint32 batchCookie );
	static void drawAll( EffectMaterial* pMaterialOverride = NULL );
	static void fini();
	
	static void disable( bool val ) { disable_ = val; }
	static bool disable( ) { return disable_; }
	typedef std::vector< VertexXYZNUVTBPC > VerticesHolder;
private:
	VerticesHolder		sourceVerts_;
	IndicesHolder		sourceIndices_;
	uint32				nSourceVerts_;
	uint32				nSourceIndices_;
	std::vector< Primitive::PrimGroup > sourcePrimitiveGroups_;
	BoundingBox			sourceBB_;
	BoundingBox			bb_;

	VertexDeclaration*	pDecl_;

	bool loadVertices( BinaryPtr pVertexData );
	bool loadIndices( BinaryPtr pIndexData );
	bool loadMaterials( DataSectionPtr pGeometrySection );

	bool update();
	
	typedef std::map< uint32, Batch* > BatchMap;
	BatchMap		batchMap_;
	bool dirty_;

	bool valid_;

	std::vector< EffectMaterial* > materials_;

	typedef std::vector< Batch* > Batches;

	struct RenderBatch
	{
		RenderBatch(){};
		ComObjectWrap< DX::VertexBuffer > pVertexBuffer_;
		IndexBuffer indexBuffer;
		Batches	batches_;
	};

	std::vector< RenderBatch > renderBatches_;

	uint32		drawCookie_;

	void		invalidate();

	std::string resourceName_;

	static bool disable_;
	VisualCompound( const VisualCompound& );
	VisualCompound& operator=( const VisualCompound& );
};

class TransformHolder : public Aligned
{
	public:
		TransformHolder( const Matrix& transform, VisualCompound::Batch* pBatch, uint32 sequence ) : 
			transform_( transform ),
			pBatch_( pBatch ),
			sequence_( sequence ) {}
		VisualCompound::Batch* pBatch() const { return pBatch_; }
		void pBatch( VisualCompound::Batch* pBatch ) { pBatch_ = pBatch; }
		const Matrix& transform() { return transform_; }
		void draw() { pBatch_->draw( sequence_ ); }
		void del();
		uint32 sequence() const { return sequence_; }
		void sequence( uint32 val ) { sequence_ = val; }
	private:
        Matrix	transform_;
		uint32 sequence_;
		VisualCompound::Batch*	pBatch_;
};

}


#endif // VISUAL_COMPOUND_HPP
