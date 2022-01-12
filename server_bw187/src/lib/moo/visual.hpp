/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef VISUAL_HPP
#define VISUAL_HPP

#include <iostream>
#include "moo_math.hpp"
#include "managed_texture.hpp"
#include "node.hpp"
#include "vertices.hpp"
#include "primitive.hpp"
#include "material.hpp"
#include "shader_manager.hpp"
#include "animation.hpp"
#include "math/boundbox.hpp"
#include "cstdmf/smartpointer.hpp"
#include "cstdmf/vectornodest.hpp"
#include "vertex_formats.hpp"
#include "cstdmf/avector.hpp"
#include "effect_material.hpp"

class BSPTree;
class BSP;
class WorldTriangle;
typedef std::vector<WorldTriangle> RealWTriangleSet;
typedef AVectorNoDestructor< Matrix >	DrawVector;

namespace Moo
{

/**
 *	Data defining the perimeter of an exported portal
 */
class PortalData : public std::vector<Vector3>
{
public:
	PortalData() : flags_( 0 )	{ }

	int flags() const			{ return flags_; }
	void flags( int f )			{ flags_ = f; }

	bool isInvasive() const		{ return !!(flags_ & 1); }

private:
	int	flags_;
};

/**
 *	A proxy to a boolean value
 */
class BoolProxy : public ReferenceCount
{
public:
	BoolProxy() : state_( true ) { }
	operator bool() { return state_; }
	bool state_;
};

typedef SmartPointer< BoolProxy > BoolProxyPointer;

/**
 *	A helper class to provide indirect references to BSPs
 */
class BSPProxy : public ReferenceCount
{
public:
	BSPProxy( BSPTree * pTree );
	~BSPProxy();

	operator BSPTree * () const { return pTree_; }
	BSPTree * pTree() const	{ return pTree_; }

private:
	BSPProxy( const BSPProxy & );
	BSPProxy & operator =( const BSPProxy & );
	BSPTree * pTree_;
};

typedef SmartPointer< BSPProxy > BSPProxyPtr;

typedef SmartPointer< class Visual > VisualPtr;


/*
 *	This class is a helper class for the visual draw methods to set
 *	up common properties before drawing.
 */
class VisualHelper : public Aligned
{
public:
	VisualHelper();
	bool shouldDraw( bool ignoreBoundingBox, const BoundingBox& bb );
	void start( bool dynamicOnly, const BoundingBox& bb, bool rendering = true );
	void fini( bool rendering = true );
	const BoundingBox& worldSpaceBB() { return wsBB_; }
	const Matrix& worldViewProjection() { return worldViewProjection_; }

private:
	LightContainerPtr pRCLC_;
	LightContainerPtr pRCSLC_;
	LightContainerPtr pLC_;
	LightContainerPtr pSLC_;
	BoundingBox wsBB_;
	Matrix worldViewProjection_;
};

/**
 *	This class is the basic drawable shaded mesh object in Moo.
 *
 *	It collects indices and vertices into Geometry structures,
 *	and organises those with common nodes into RenderSets,
 *	such that all the Geometry in a RenderSet may be drawn with the
 *	the same node matrices in the GPU's vertex shader constants.
 *
 *	It also maintains a number of related ancillary structures,
 *	which are derived from the mesh and used by other BigWorld systems.
 */
class Visual : public ReferenceCount
{
public:
	explicit Visual( const std::string& resourceID, bool validateNodes = true );
	~Visual();

	/**
	 *	This class is the interface for overriding visual's draw method.
	 *	If a DrawOverride has been set Visual::draw and Visual::batch will
	 *	call the draw method in the DrawOverride rather than continuing
	 *	with normal rendering.
	 */
	class DrawOverride
	{
	public:
		virtual HRESULT draw( Visual* pVisual, bool ignoreBoundingBox = false ) = 0;
	};

	static DrawOverride*		s_pDrawOverride;

	HRESULT						draw( bool ignoreBoundingBox = false );

	HRESULT						batch( bool ignoreBoundingBox = false );

	static HRESULT				drawBatches();

	///sets no states apart from indices / vertices.  draws all mesh parts, no sub-transforms allowed.
	void						justDrawPrimitives();

	///copies visual into vertex / index buffer, using the current transform.
	///destination buffers are created ( and must be cleaned up by the caller )
	bool						createCopy( Moo::VertexXYZNUV *& retPVertices, IndicesHolder& retPIndices,
								uint32 & retNumVertices, uint32 & retNumIndices, EffectMaterial *& material );
	uint32						nVertices() const;
	uint32						nTriangles() const;

	NodePtr						rootNode( ) const;

	AnimationPtr				animation( uint32 i ) const;
	uint32						nAnimations( ) const;
	void						addAnimation( AnimationPtr animation );
	AnimationPtr				addAnimation( const std::string& animResourceID );
	AnimationPtr				findAnimation( const std::string& identifier ) const;

	const BoundingBox&			boundingBox() const;
	void						boundingBox( const BoundingBox& bb );

	uint32						nPortals( void ) const;
	const PortalData&			portal( uint32 i ) const;
	const BSPTree *				pBSPTree() const { return pBSP_ ? (BSPTree*)(*pBSP_) : NULL; }



	void						overrideMaterial( const std::string& materialIdentifier, const EffectMaterial& mat );
	int							gatherMaterials( const std::string & materialIdentifier, std::vector< EffectMaterial** > & uses, const EffectMaterial ** ppOriginal = NULL );
	int							collateOriginalMaterials( std::vector< EffectMaterial* > & rppMaterial );

	void						useExistingNodes( StringHashMap<NodePtr> & nodeCatalogue );

	void						staticVertexColours( DX::VertexBuffer* vb ) { staticVertexColours_ = vb; };

	const std::string &         resourceID() const		{ return resourceID_; }
#ifdef EDITOR_ENABLED
	bool						updateBSP();
#endif

	static uint32				drawCount_;

	// Typedefs needed by VisualLoader
	typedef EffectMaterial 	Material;

	// Class needed by VisualLoader. Caches BSP trees.
	class BSPCache
	{
	public:
		void add( const std::string& , BSPProxyPtr& pBSP );
		BSPProxyPtr& find( const std::string& );
		static BSPCache& instance();
	};

	// Class needed by VisualLoader. A bit of a dodgy class to
	// automatically convert a BSPTree* to a BSPProxyPtr.
	class BSPTreePtr : public BSPProxyPtr
	{
	public:
		BSPTreePtr( BSPProxyPtr& other ) : BSPProxyPtr( other ) {}
		BSPTreePtr( BSPTree* pTree = NULL) : 
			BSPProxyPtr( pTree ? new BSPProxy( pTree ) : NULL )
		{}
	};

	// Class needed by VisualLoader. Stores EffectMaterial.
	class Materials : public std::vector<EffectMaterial*>
	{
	public:
		const EffectMaterial* find( const std::string& identifier ) const;
	};

	struct PrimitiveGroup
	{
		uint32					groupIndex_;
		EffectMaterial*			material_;
	};

	///copies vertPtr, primPtr and material.  used by mesh particle renderer
	bool						primGroup0(
		Moo::VerticesPtr & retVerts,
		Moo::PrimitivePtr & retPrim,
		Moo::Visual::PrimitiveGroup* &retPPrimGroup );

	typedef std::vector< PrimitiveGroup > PrimitiveGroupVector;

	struct Geometry
	{
		VerticesPtr vertices_;
		PrimitivePtr primitives_;
		PrimitiveGroupVector primitiveGroups_;

		uint32 nTriangles() const;
	};

	typedef std::vector< Geometry > GeometryVector;

	class RenderSet : public Aligned
	{
	public:
		bool treatAsWorldSpaceObject_;
		NodePtrVector transformNodes_;
		GeometryVector geometry_;
		Matrix				firstNodeStaticWorldTransform_;
	};

	typedef std::avector< RenderSet > RenderSetVector;
	typedef std::vector< AnimationPtr > AnimationVector;

	RenderSetVector& renderSets() { return renderSets_; }

	bool						isOK() const { return isOK_; }

	static void					disableBatching( bool val ) { disableBatching_ = val; };
	static bool					disableBatching() { return disableBatching_; }

	// Methods needed by VisualLoader.
	static BSPProxyPtr loadBSPVisual( const std::string& resourceID );

private:
	HRESULT						drawBatch();
	void						addLightsInModelSpace( const Matrix& invWorld );
	void						addLightsInWorldSpace( );

	AnimationVector				animations_;
	NodePtr						rootNode_;
	RenderSetVector				renderSets_;
	std::vector<PortalData>		portals_;
	BoundingBox					bb_;
	bool						usingExistingNodes_;
	Materials					ownMaterials_;

	DX::VertexBuffer*			staticVertexColours_;
	BSPProxyPtr					pBSP_;
	bool						validateNodes_;
	class VisualBatcher*		pBatcher_;
    std::string                 resourceID_;
	bool						isOK_;

	static VectorNoDestructor< Visual* > s_batches_;
	static bool					disableBatching_;

	Visual( const Visual & );
	Visual & operator=( const Visual & );
};

}

#ifdef CODE_INLINE
#include "visual.ipp"
#endif




#endif // VISUAL_HPP
