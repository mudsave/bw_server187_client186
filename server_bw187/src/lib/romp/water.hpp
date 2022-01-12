/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef _WATER_HPP
#define _WATER_HPP

#include <iostream>
#include <vector>

// splashing disabled for now:
//#ifndef EDITOR_ENABLED
//#define SPLASH_ENABLED
//#endif //EDITOR_ENABLED

#include "cstdmf/smartpointer.hpp"
#include "moo/device_callback.hpp"
#include "moo/moo_math.hpp"
#include "moo/moo_dx.hpp"
#include "moo/com_object_wrap.hpp"
#include "moo/effect_material.hpp"
#include "moo/effect_visual_context.hpp"
#include "moo/render_target.hpp"
#include "moo/render_context.hpp"
#include "chunk/chunk_item.hpp"

#include "water_scene_renderer.hpp"
#ifdef SPLASH_ENABLED
#include "water_splash.hpp"
#endif

#include "math/boundbox.hpp"
#include "resmgr/datasection.hpp"
#include "romp_collider.hpp"

#include "speedtree/vertex_buffer.hpp"
using dxwrappers::VertexBuffer;

#define VERTEX_TYPE Moo::VertexXYZDUV

#include "water_simulation.hpp"

namespace Moo
{
	class Material;
	class BaseTexture;
	typedef SmartPointer< BaseTexture > BaseTexturePtr;
}

class Water;
typedef SmartPointer< Water > WaterPtr;
class Waters;
class BackgroundTask;
class ChunkTerrain;
typedef SmartPointer<ChunkTerrain> ChunkTerrainPtr;

class WaterEdgeWaves;

/**
 *	This class is represents a division of a water surface
 */
class WaterCell : public SimulationCell
{
public:
	typedef std::list< SimulationCell* > SimulationCellPtrList;
	typedef std::list< WaterCell* > WaterCellPtrList;
	typedef std::vector< WaterCell > WaterCellVector;

	// Construction
	WaterCell();
	
	// Initialisation / deletion
	bool init( Water* water, Vector2 start, Vector2 size );
	void initEdge( int index, WaterCell* cell );
	void createManagedIndices();
	void deleteManaged();

	// Drawing
	bool bind();
	void draw( int nVerts );

	// Simulation activity
	int indexCount() const;
	virtual void deactivate();
	bool simulationActive() const;
	void initSimulation( int size, float cellSize );
	virtual void bindNeighbour( Moo::EffectMaterialPtr effect, int edge );
	void checkEdgeActivity( SimulationCellPtrList& activeList );
	Moo::RenderTargetPtr simTexture();// Retrieve the simulation texture
	void checkActivity( SimulationCellPtrList& activeList, WaterCellPtrList& edgeList );
	uint32 bufferIndex() const { return vBufferIndex_; }

	// General
	const Vector2& min();
	const Vector2& max();
	const Vector2& size();
private:
	template< class T >
	void buildIndices( );

	Vector2								min_, max_, size_;//TODO: replace with bounding box?
	WaterCell*							edgeCells_[4];
	ComObjectWrap< DX::IndexBuffer >	indexBuffer_;
	
	uint32								vBufferIndex_;
	uint32								nIndices_;
	Water*								water_;
	int									xMax_, zMax_;
	int									xMin_, zMin_;
};

/**
 *	This class is a body of water which moves and displays naturally.
 */
class Water : public ReferenceCount, public Moo::DeviceCallback, public Aligned
{
public:
	typedef SmartPointer< dxwrappers::VertexBuffer< VERTEX_TYPE > > VertexBufferPtr;	

	typedef struct _WaterState
	{
		Vector3		position_;
		Vector2		size_;
		float		orientation_;		
		float		tessellation_;
		float		textureTessellation_; // wave frequency???
		float		consistency_;
		float		fresnelConstant_;
		float		fresnelExponent_;
		float		reflectionScale_; //combine into one waveAmplitude?
		float		refractionScale_;
		Vector2		scrollSpeed1_;
		Vector2		scrollSpeed2_;
		Vector2		waveScale_;
		float		sunPower_;
		float		windVelocity_;
		Vector4		reflectionTint_;
		Vector4		refractionTint_;
		float		simCellSize_;
		float		smoothness_;
		bool		useEdgeAlpha_;
		bool		useSimulation_;
		std::string	waveTexture_;
		std::string transparencyTable_;
	} WaterState;

	// Construction / Destruction
	Water( const WaterState& config, RompColliderPtr pCollider = NULL );
	//~Water();

	// Initialisation / deletion
	void rebuild( const WaterState& config );
	void deleteManagedObjects( );
	void createManagedObjects( );
	void deleteUnmanagedObjects( );
	void createUnmanagedObjects( );	
	uint32	 createIndices( );
	int	 createWaveIndices( );	
	void createCells( );
	static void deleteWater(Water* water);
	static bool stillValid(Water* water);

	uint32 remapVertex( uint32 index );

	template <class T>
	uint32 remap(std::vector<T>& dstIndices,
					const std::vector<uint32>& srcIndices);


	// a list of vertex buffers with their related mappings.
	std::vector< std::map<uint32, uint32> > remappedVerts_;
	
#if UMBRA_ENABLE
	// Umbra specific
	void enableOccluders() const;
	void disableOccluders() const;
	void addTerrainItem( ChunkTerrain* item );
	void eraseTerrainItem( ChunkTerrain* item );
#endif //UMBRA_ENABLE
	
#ifdef EDITOR_ENABLED
	void deleteData( );
	void saveTransparencyTable( );

	void drawRed(bool value) { drawRed_ = value; }
	bool drawRed() const { return drawRed_; }
#endif //EDITOR_ENABLED

	void addMovement( const Vector3& from, const Vector3& to, const float diameter );

	// Drawing
	uint32 drawMark() const		{ return drawMark_; }
	void drawMark( uint32 m )	{ drawMark_ = m; }

	void clearRT();
	void draw( Waters & group, float dTime );
	bool shouldDraw() const;

	// General
	const Vector3&	position() const;
	const Vector2&	size() const;

	static VertexBufferPtr s_quadVertexBuffer_;

    static bool backgroundLoad();
    static void backgroundLoad(bool background);

private:
	typedef std::vector< uint32 > WaterAlphas;
	typedef std::vector< bool > WaterRigid;
	typedef std::vector< int32 > WaterIndices;
	typedef std::map<float, WaterScene*> WaterRenderTargetMap;

	~Water();

	WaterState							config_;
	
	Matrix								transform_;
	Matrix								invTransform_;

	uint32								gridSizeX_;
	uint32								gridSizeZ_;

	float								time_;
	float								lastDTime_;
	float								waterDepth_;

	WaterRigid							wRigid_;
	WaterAlphas							wAlpha_;
	WaterIndices						wIndex_;

	WaterEdgeWaves*						edgeWaves_; //edge wave experiment

	std::vector<VertexBufferPtr>		vertexBuffers_;

	uint32								nVertices_;

	Moo::EffectMaterialPtr				effect_;	
	Moo::EffectMaterialPtr				simulationEffect_;

	static WaterRenderTargetMap			s_renderTargetMap_;

	WaterCell::WaterCellVector			cells_;
	WaterCell::SimulationCellPtrList	activeSimulations_;
	
	WaterScene*							waterScene_;

#ifdef EDITOR_ENABLED
	bool								drawRed_;
#endif

	bool								drawSelf_;
	bool								initialised_;
	bool								enableSim_;
	bool								reflectionCleared_;
	bool								createdCells_;
	static bool                         s_backgroundLoad_;

	BoundingBox							bb_;
	BoundingBox							bbDeep_;
	BoundingBox							bbActual_;

	uint32								drawMark_;

	float								texAnim_;
	
	BackgroundTask*						backgroundTask1_;
	BackgroundTask*						backgroundTask2_;    

	Moo::BaseTexturePtr					normalTexture_;
	Moo::BaseTexturePtr					screenFadeTexture_;
	Moo::BaseTexturePtr					foamTexture_;

	RompColliderPtr						pCollider_;

#if UMBRA_ENABLE
	typedef std::vector<ChunkTerrainPtr> TerrainVector;
	TerrainVector						terrainItems_;
#endif //UMBRA_ENABLE	

	Water(const Water&);
	Water& operator=(const Water&);

	friend class Waters;
	friend class WaterCell;

	void buildTransparencyTable( );
	bool loadTransparencyTable( );
	void setup2ndPhase();	
	void renderSimulation(float dTime, Waters& group, bool raining);
	void resetSimulation();
	void releaseWaterScene();

	static void doCreateTables( void * self );
	static void onCreateTablesComplete( void * self );
	static void doLoadResources( void * self );
	static void onLoadResourcesComplete( void * self );	
};


/**
 *	This class helpfully collects a number of bodies of water that
 *	share common settings and methods.
 */
class Waters : public std::vector< Water* >
{
public:
	~Waters();
	static Waters& instance();

	void init( );
	void fini( );

	void drawDrawList( float dTime );
	
	int simulationLevel() const;
	void playerPos( Vector3 pos );
	
	float rainAmount() const;
	void rainAmount( float rainAmount );
	
	bool insideVolume() const;
	void drawWireframe( bool wire );
	
	void setQualityOption(int optionIndex);
	void setSimulationOption(int optionIndex);
	
	float movementImpact() const;
	void addMovement( const Vector3& from, const Vector3& to, const float diameter );

#ifdef SPLASH_ENABLED
	void addSplash( const Vector4& impact, const Matrix& trans ) { splashManager_.addSplash(impact,trans); }
#endif
	
	void createSplashSystem();

	static void drawWaters( bool draw );
	static void addToDrawList( Water * pWater );	

	static bool simulationEnabled();
	static void simulationEnabled( bool val);

	static void loadResources( void * self );
	static void loadResourcesComplete( void * self );

	static bool drawRefraction();
	static void drawRefraction( bool val );

	static bool drawReflection();
	static void drawReflection( bool val );
	
	static uint32		lastDrawMark() { return s_lastDrawMark_; }

#if EDITOR_ENABLED
	static void projectView( bool val ) { s_projectView_ = val; }
	static bool projectView( ) { return s_projectView_; }
	static bool			s_projectView_;
#endif

private:
	Waters();
	
	void insideVolume( bool val );

#ifdef SPLASH_ENABLED
	SplashManager	splashManager_;
#endif

	bool			insideWaterVolume_;		//eye inside a water volume?
	bool			drawWireframe_;			//force all water to draw in wireframe.
	float			movementImpact_;		//simulation impact scaling	
	float			rainAmount_;			//the amount of rain affecting the water.
	int				waterQuality_;			//quality: 0 - High, 1 - Mid, 2 - Low
	Vector3			playerPos_;
	
	BackgroundTask*	loadingTask1_;  

	static bool		s_drawWaters_;			//global water draw flag.	
	static bool		s_simulationEnabled_;	//global sim enable flag.
	static bool		s_drawReflection_;
	static bool		s_drawRefraction_;
	static int		s_simulationLevel_;		//0 - disabled, 1 - low, 2 - high
	static uint32	s_nextMark_;
	static uint32	s_lastDrawMark_;	

	static Moo::EffectMaterialPtr		s_effect_;
	static Moo::EffectMaterialPtr		s_simulationEffect_;

	friend class Water;
	friend class SimulationCell;
};

#ifdef CODE_INLINE
#include "water.ipp"
#endif

#endif //_WATER_HPP
/*water.hpp*/
