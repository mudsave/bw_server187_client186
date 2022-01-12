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
#include "speedtree_renderer.hpp"
#include "speedtree_config.hpp"

// BW Tech Hearders
#include "cstdmf/debug.hpp"
#include "math/boundbox.hpp"
#include "physics2/bsp.hpp"

// set ENABLE_BB_OPTIMISER to 0 to disable
// the billboard optimiser from the build
#define ENABLE_BB_OPTIMISER 1

// Set ENABLE_MATRIX_OPT to 0 to disable
// the matrix array optimisaton. Don't
// forget to do the same in speedtree.fx
#define ENABLE_MATRIX_OPT 1

// Set DONT_USE_DUPLO to 1 to prevent all calls
// to the dublo lib. This avoids having to link
// against duplo (which, in turn, depends on a
// lot of other libs). It is useful for testing.
#define DONT_USE_DUPLO 0

DECLARE_DEBUG_COMPONENT2("SpeedTree", 0)


#if SPEEDTREE_SUPPORT // -------------------------------------------------------

// Speedtree Lib Headers
#include "speedtree_collision.hpp"
#include "billboard_optimiser.hpp"

// BW Tech Hearders
#include "cstdmf/dogwatch.hpp"
#include "cstdmf/concurrency.hpp"
#include "cstdmf/smartpointer.hpp"
#include "resmgr/bwresource.hpp"
#include "moo/material.hpp"
#include "moo/render_context.hpp"
#include "moo/texture_manager.hpp"
#include "moo/effect_material.hpp"
#include "moo/effect_visual_context.hpp"
#include "romp/enviro_minder.hpp"
#include "romp/time_of_day.hpp"
#include "romp/fog_controller.hpp"
#include "romp/geometrics.hpp"
#include "romp/weather.hpp"
#include "duplo/shadow_caster.hpp"


// SpeedTree API
#include <SpeedTreeRT.h>
#include <SpeedWind2.h>

// DX Headers
#include <d3dx9math.h>

// DX Wrappers
#include "vertex_buffer.hpp"
#include "index_buffer.hpp"

// STD Headers
#include <sstream>
#include <map>

// Import dx wrappers
using dxwrappers::IndexBuffer;
using dxwrappers::VertexBuffer;
using Moo::BaseTexturePtr;


namespace speedtree {

namespace { // anonymous

// Named contants
const int VSCO_WindMatricies  = 14;
const int VSCO_LeafBillboard  = 46;
const float c_maxDTime        = 0.05f;
const float c_maxWindVelocity = 15.0f;

#include "speedtree_renderer_util.hpp"

} // namespace anonymous


/**
 *	Holds the actual data needed to render the tree model. Each speedtree
 *	file loaded (.SPT), or different trees generated from the same SPT but
 *	using a different seed number, will be handled by a single
 *	TSpeedTreeType
 */
struct TSpeedTreeType
{
	typedef SmartPointer< TSpeedTreeType > TreeTypePtr;

	struct LodData
	{
		int     branchLod;
		float   branchAlpha;
		bool    branchDraw;

		int     frondLod;
		float   frondAlpha;
		bool    frondDraw;

		int     leafLods[2];
		float   leafAlphaValues[2];
		int     leafLodCount;
		bool    leafDraw;

		float   billboardFadeValue;
		bool    billboardDraw;

		bool    model3dDraw;
	};

	/**
	 *	Rendering of trees are, by default, batched to minimize state changes.
	 *	During the scene traversal, all data needed to render a tree is stored
	 *	into DeferData structs. This data will later be used to do the actual
	 *	rendering of the trees in a state sorted fashion.
	 */
	struct DeferData : Aligned
	{
		Matrix  world;
		Matrix  rotation;
		Matrix  projection;
		Matrix  worldViewProj;
		Matrix  scaleTranslateView;
		Matrix  scaleTranslateViewProj;
		Vector4 fogColour;
		float   fogNear;
		float   fogFar;
		float	treeScale;

		LodData lod;
	};

	TSpeedTreeType();
	~TSpeedTreeType();

	static void fini();

	// tick
	static void tick(float dTime);
	static void update();
	static bool recycleTreeTypeObjects();
	static bool doSyncInit();
	static void clearDXResources();

	// Animation
	void updateWind();

	// Deferred drawing
	void deferTree(const DeferData & deferData);

	void computeDeferData(
		const Matrix & world,
		DeferData    & o_deferData);

	static void uploadRenderConstants(
		const DeferData & deferData, bool commit = true);

	void computeLodData(LodData &o_lod);

	// Actual drawing
	static void drawDeferredTrees(bool drawBillboards=true);

	void drawDeferredBranches();
	void drawDeferredFronds();
	void drawDeferredLeaves();
	void drawDeferredBillboards();

	void drawBranch(const DeferData & deferData);
	void drawFrond(const DeferData & deferData);
	void drawLeaf(const DeferData & deferData);
	void drawBillboard(const DeferData & deferData);
	void drawBSPTree(const Matrix & world) const;

	void clearDeferredTrees();

	// Rendering states
	static void prepareRenderContext();

	void setCompositeRenderStates() const;
	void setBranchRenderStates() const;
	void setFrondRenderStates();
	void setLeafRenderStates();
	void setBillboardRenderStates();

	static void beginPass();
	static void endPass();

	// Models
	void setBranchModel() const;
	void setFrondModel() const;

	// Shaders constants
	static void saveWindInformation(EnviroMinder * envMinder);
	static void saveLightInformation(EnviroMinder * envMinder);

	void uploadWindMatrixConstants();

	// Setup
	static TreeTypePtr getTreeTypeObject(const char * filename, uint seed);

	void assyncInit();
	void syncInit();
	void releaseResources();

	static void updateRenderGroups();

	typedef std::vector<TSpeedTreeType *> InitVector;
	static InitVector s_syncInitList;
	static SimpleMutex s_syncInitLock;

	// Deferred trees
	typedef std::avector< DeferData > DeferDataVector;
	DeferDataVector deferredTrees;

	// Tree definition data
	typedef std::auto_ptr< CSpeedTreeRT > CSpeedTreeRTPtr;
	typedef std::auto_ptr< BSPTree > BSPTreePtr;
	typedef std::auto_ptr< Moo::EffectMaterial > EffectPtr;
	CSpeedTreeRTPtr speedTree;
	CSpeedWind2     speedWind;
	BoundingBox     boundingBox;
	BSPTreePtr      bspTree;

	Moo::VertexXYZL * bspVertices;
	int               bspVerticesCount;

	BranchRenderData    branches;
	BranchRenderData    fronds;
	LeafRenderData      leaves;
	BillboardRenderData billboards;

	float leafRockScalar;
	float leafRustleScalar;

	int   bbTreeType;

	uint        seed;
	std::string filename;
	float       lastTicktime;

	// reference counting
	void incRef() const;
	void decRef() const;
	int  refCount() const;

	mutable int refCounter;

	// Static tree
	// definitions map
	typedef std::map<std::string, TSpeedTreeType *> TreeRendererMap;
	static TreeRendererMap s_rendererMap;

	static bool            s_frameStarted;
	static float           s_time;

	// Render groups
	typedef std::vector< TSpeedTreeType* > RendererVector;
	typedef std::vector< RendererVector > RendererGroupVector;
	static RendererGroupVector s_renderGroups;

	// Materials
	static EffectPtr s_effect;
	static EffectPtr s_shadowsFX;

	// Auxiliary static data
	static float s_windVelX;
	static float s_windVelY;

	static ShadowCaster * s_curShadowCaster;

	static Matrix      s_invView;
	static Matrix      s_view;
	static Matrix      s_projection;
	static Moo::Colour s_sunDiffuse;
	static Moo::Colour s_sunAmbient;
	static Vector3     s_sunDirection;

	// Watched parameters
	static bool  s_enviroMinderLight;
	static bool  s_drawBoundingBox;
	static bool  s_drawBSPTree;
	static bool  s_batchRendering;
	static bool  s_drawTrees;
	static bool  s_drawLeaves;
	static bool  s_drawBranches;
	static bool  s_drawFronds;
	static bool  s_drawBillboards;
	static bool  s_texturedTrees;
	static bool  s_playAnimation;
	static float s_leafRockFar;
	static float s_lodMode;
	static float s_lodNear;
	static float s_lodFar;

#if ENABLE_BB_OPTIMISER
		static bool  s_optimiseBillboards;
#endif

	static int s_speciesCount;
	static int s_uniqueCount;
	static int s_visibleCount;
	static int s_deferredCount;
	static int s_totalCount;

	static std::string s_fxFile;
	static std::string s_shadowsFXFile;
	static std::string s_speedWindFile;

	static DogWatch s_globalWatch;
	static DogWatch s_prepareWatch;
	static DogWatch s_drawWatch;
	static DogWatch s_primitivesWatch;
};

// Macros
#define FOREACH_TREETYPE_BEGIN(dogWatcher)                                \
	{                                                                   \
	ScopedDogWatch watcherHolder(dogWatcher);                           \
	typedef TSpeedTreeType::TreeRendererMap::iterator iterator; \
	iterator renderIt = TSpeedTreeType::s_rendererMap.begin();  \
	iterator renderEnd = TSpeedTreeType::s_rendererMap.end();   \
	while (renderIt != renderEnd) {                                     \
		TSpeedTreeType * RENDER = renderIt->second;

#define FOREACH_TREETYPE_END ++renderIt; }}

#define FOREACH_DEFEREDTREE_BEGIN                                             \
	{                                                                         \
	typedef TSpeedTreeType::DeferDataVector::const_iterator iterator; \
	iterator treeIt = this->deferredTrees.begin();                            \
	iterator treeEnd = this->deferredTrees.end();                             \
	while (treeIt != treeEnd)                                                 \
	{                                                                         \
		const DeferData & DEFERDATA = *treeIt;

#define FOREACH_DEFERREDTREE_END ++treeIt; }}

// ---------------------------------------------------------- SpeedTreeRenderer

/**
 *	Default constructor.
 */
SpeedTreeRenderer::SpeedTreeRenderer() :
	treeType_(NULL),
	bbOptimiser_(NULL),
	treeID_(-1)
{
	++TSpeedTreeType::s_totalCount;
}

/**
 *	Destructor.
 */
SpeedTreeRenderer::~SpeedTreeRenderer()
{
	--TSpeedTreeType::s_totalCount;

#if ENABLE_BB_OPTIMISER
		// try to release bb optimizer
		if (this->bbOptimiser_.exists())
		{
			this->resetTransform(NULL);
			this->bbOptimiser_ = NULL;
		}
#endif
}

/**
 *	Sets the LOD mode. Returns flag state before call.
 */
float SpeedTreeRenderer::lodMode(float newValue)
{
	float oldValue = TSpeedTreeType::s_lodMode;
	TSpeedTreeType::s_lodMode = newValue;
	return oldValue;
}

/**
 *	Sets the envirominder as the source for lighting
 *	information mode. Returns flag state before call.
 */
bool SpeedTreeRenderer::enviroMinderLighting(bool newValue)
{
	bool oldValue = TSpeedTreeType::s_enviroMinderLight;
	TSpeedTreeType::s_enviroMinderLight = newValue;
	return oldValue;
}

/**
 *	Enables disables drawing of trees. Returns flag state before call.
 */
bool SpeedTreeRenderer::drawTrees(bool newValue)
{
	bool oldValue = TSpeedTreeType::s_drawTrees;
	TSpeedTreeType::s_drawTrees = newValue;
	return oldValue;
}

/**
 *	Initializes the speedtree module.
 *
 *	@param	section	XML section containing all initialization settings for
 *					the module (usually read from speedtree.xml).
 */
void SpeedTreeRenderer::init(DataSectionPtr section)
{
	CSpeedTreeRT::Authorize("your license key goes here");

	if (!CSpeedTreeRT::IsAuthorized())
	{
		ERROR_MSG("SpeedTree authorisation failed\n");
	}
	
	if (!section.exists())
	{
		CRITICAL_MSG("SpeedTree settings file not found");
		return;
	}

	if (TSpeedTreeType::s_effect.get() != NULL)
	{
		CRITICAL_MSG("SpeedTree already initialized");
		return;
	}

	CSpeedTreeRT::SetNumWindMatrices(4);
	CSpeedTreeRT::SetDropToBillboard(true);

	MF_WATCH("SpeedTree/Envirominder Lighting",
		TSpeedTreeType::s_enviroMinderLight, Watcher::WT_READ_WRITE,
		"Toggles use of envirominder as the source for lighting "
		"information, instead of Moo's light container.");

	MF_WATCH("SpeedTree/Bounding boxes",
		TSpeedTreeType::s_drawBoundingBox, Watcher::WT_READ_WRITE,
		"Toggles rendering of trees' bounding boxes on/off");

	MF_WATCH("SpeedTree/BSP Trees",
		TSpeedTreeType::s_drawBSPTree, Watcher::WT_READ_WRITE,
		"Toggles rendering of trees' BSP-trees on/off");

	MF_WATCH("SpeedTree/Batched rendering",
		TSpeedTreeType::s_batchRendering, Watcher::WT_READ_WRITE,
		"Toggles batched rendering on/off");

	MF_WATCH("SpeedTree/Draw trees",
		TSpeedTreeType::s_drawTrees, Watcher::WT_READ_WRITE,
		"Toggles speedtree rendering on/off");

	MF_WATCH("SpeedTree/Draw leaves",
		TSpeedTreeType::s_drawLeaves, Watcher::WT_READ_WRITE,
		"Toggles rendering of leaves on/off");

	MF_WATCH("SpeedTree/Draw branches",
		TSpeedTreeType::s_drawBranches, Watcher::WT_READ_WRITE,
		"Toggles rendering of branches on/off");

	MF_WATCH("SpeedTree/Draw fronds",
		TSpeedTreeType::s_drawFronds, Watcher::WT_READ_WRITE,
		"Toggles rendering of fronds on/off");

	MF_WATCH("SpeedTree/Draw billboards",
		TSpeedTreeType::s_drawBillboards, Watcher::WT_READ_WRITE,
		"Toggles rendering of billboards on/off");

#if ENABLE_BB_OPTIMISER
		TSpeedTreeType::s_optimiseBillboards = section->readBool(
			"billboardOptimiser/enabled", TSpeedTreeType::s_optimiseBillboards );
		MF_WATCH("SpeedTree/Optimise billboards",
			TSpeedTreeType::s_optimiseBillboards, Watcher::WT_READ_WRITE,
			"Toggles billboards optimizations on/off (requires batching to be on)" );
#endif

	MF_WATCH("SpeedTree/Texturing",
		TSpeedTreeType::s_texturedTrees, Watcher::WT_READ_WRITE,
		"Toggles texturing on/off");

	MF_WATCH("SpeedTree/Play animation",
		TSpeedTreeType::s_playAnimation, Watcher::WT_READ_WRITE,
		"Toggles wind animation on/off");

	TSpeedTreeType::s_leafRockFar = section->readFloat(
		"leafRockFar", TSpeedTreeType::s_leafRockFar);
	MF_WATCH("SpeedTree/Leaf Rock Far Plane",
		TSpeedTreeType::s_leafRockFar, Watcher::WT_READ_WRITE,
		"Sets the maximun distance to which "
		"leaves will still rock and rustle");

	TSpeedTreeType::s_lodMode = section->readFloat(
		"lodMode", TSpeedTreeType::s_lodMode);
	MF_WATCH("SpeedTree/LOD Mode", TSpeedTreeType::s_lodMode, Watcher::WT_READ_WRITE,
		"LOD Mode: -1: dynamic (near and far LOD defined per tree);  "
		"-2: dynamic (use global near and far LOD values);  "
		"[0.0 - 1.0]: force static." );

	TSpeedTreeType::s_lodNear = section->readFloat(
		"lodNear", TSpeedTreeType::s_lodNear);
	MF_WATCH("SpeedTree/LOD near",
		TSpeedTreeType::s_lodNear, Watcher::WT_READ_WRITE,
		"Global LOD near value (distance for maximum level of detail");

	TSpeedTreeType::s_lodFar = section->readFloat(
		"lodFar", TSpeedTreeType::s_lodFar);
	MF_WATCH("SpeedTree/LOD far",
		TSpeedTreeType::s_lodFar, Watcher::WT_READ_WRITE,
		"Global LOD far value (distance for minimum level of detail)");

	MF_WATCH("SpeedTree/Counters/Unique",
		TSpeedTreeType::s_uniqueCount, Watcher::WT_READ_ONLY,
		"Counter: number of unique tree models currently loaded");

	MF_WATCH("SpeedTree/Counters/Species",
		TSpeedTreeType::s_speciesCount, Watcher::WT_READ_ONLY,
		"Counter: number of speed tree files currently loaded");

	MF_WATCH("SpeedTree/Counters/Visible",
		TSpeedTreeType::s_visibleCount, Watcher::WT_READ_ONLY,
		"Counter: number of tree currently visible in the scene");

	MF_WATCH("SpeedTree/Counters/Instances",
		TSpeedTreeType::s_totalCount, Watcher::WT_READ_ONLY,
		"Counter: total number of trees instantiated");

	TSpeedTreeType::s_speedWindFile = section->readString(
		"speedWindINI", TSpeedTreeType::s_speedWindFile);

	// init render effect material
	TSpeedTreeType::s_fxFile = section->readString(
		"fxFile", TSpeedTreeType::s_fxFile);

	if (TSpeedTreeType::s_fxFile.empty())
	{
		CRITICAL_MSG("Effect file not defined in speedtree config file");
	}

	TSpeedTreeType::EffectPtr effect(new Moo::EffectMaterial);
	if (!effect->initFromEffect(TSpeedTreeType::s_fxFile))
	{
		CRITICAL_MSG(
			"Could not load speedtree effect file: \"%s\"",
			TSpeedTreeType::s_fxFile.c_str());
	}
	TSpeedTreeType::s_effect = effect;

	// init shadows effect material
	TSpeedTreeType::s_shadowsFXFile = section->readString(
		"shadowsFXFile", TSpeedTreeType::s_shadowsFXFile);

	if (TSpeedTreeType::s_shadowsFXFile.empty())
	{
		CRITICAL_MSG("Shadows Effect file not defined in speedtree config file");
	}

	TSpeedTreeType::EffectPtr shadowsFX(new Moo::EffectMaterial);
	if (shadowsFX->initFromEffect(TSpeedTreeType::s_shadowsFXFile))
	{
		TSpeedTreeType::s_shadowsFX = shadowsFX;
	}

#if ENABLE_BB_OPTIMISER
		// init billboard optimiser
		BillboardOptimiser::init(section,
			TSpeedTreeType::s_effect.get());
#endif
}

/**
 *	Finilises the speedtree module.
 */
void SpeedTreeRenderer::fini()
{
	TSpeedTreeType::s_shadowsFX.reset();
	TSpeedTreeType::s_effect.reset();

	TSpeedTreeType::fini();

#if ENABLE_BB_OPTIMISER
		BillboardOptimiser::fini();
#endif
}

/**
 *	Loads a SPT file.
 *
 *	@param	filename	name of SPT file to load.
 *	@param	seed		seed number to use when generating tree geometry.
 *	@param	chunk		pointer chunk owning this tree (if NULL, tree won't
 *						try to use billboard optimizations).
 *	@note				will throw std::runtime_error if file can't be loaded.
 */
void SpeedTreeRenderer::load(
	const char   * filename,
	uint           seed,
	const Chunk  * chunk)
{
	this->treeType_ = TSpeedTreeType::getTreeTypeObject(filename, seed);
	this->resetTransform(chunk);
}

/**
 *	Tickss the speedtree module.
 *
 *	@param	dTime	time elapsed since last tick.
 */
void SpeedTreeRenderer::tick(float dTime)
{
	ScopedDogWatch watcher1(TSpeedTreeType::s_globalWatch);
	TSpeedTreeType::tick(dTime);
}

void SpeedTreeRenderer::update()
{
	TSpeedTreeType::update();
#if ENABLE_BB_OPTIMISER
		BillboardOptimiser::update();
#endif
}

/**
 *	Marks begining of new frame. No draw call can be done
 *	before this method is called.
 *	@param envMinder	pointer to the current environment minder object
 *						(if NULL, will use constant lighting and wind).
 */
void SpeedTreeRenderer::beginFrame(
	EnviroMinder * envMinder,
	ShadowCaster * caster)
{
	TSpeedTreeType::s_curShadowCaster = caster;

	static DogWatch clearWatch("Clear Deferred");
	ScopedDogWatch watcher1(TSpeedTreeType::s_globalWatch);
	ScopedDogWatch watcher2(TSpeedTreeType::s_prepareWatch);

	// store view and projection
	// matrices for future use
	TSpeedTreeType::s_view = Moo::rc().view();
	TSpeedTreeType::s_projection = Moo::rc().projection();

	Matrix & invView = TSpeedTreeType::s_invView;
	D3DXMatrixInverse(&invView, NULL, &TSpeedTreeType::s_view);

	// update speed tree camera (in speedtree, z is up)
	D3DXVECTOR4 pos;
	D3DXVec3Transform(&pos, &D3DXVECTOR3(0, 0, 0), &invView);

	D3DXVECTOR4 one;
	D3DXVec3Transform(&one, &D3DXVECTOR3(0, 0, -1), &invView);

	D3DXVECTOR3 dir;
	D3DXVec3Subtract(&dir,
		&D3DXVECTOR3(one.x, one.y, one.z),
		&D3DXVECTOR3(pos.x, pos.y, pos.z));

	// TODO: remove temp hack to avoid crash
	static CSpeedTreeRT crashHack_NeverUsed;
	
	const float cam_pos[] = {pos.x, pos.y, pos.z};
	const float cam_dir[] = {dir.x, dir.y, dir.z};
	CSpeedTreeRT::SetCamera(cam_pos,cam_dir);

	TSpeedTreeType::saveWindInformation(envMinder);
	TSpeedTreeType::saveLightInformation(envMinder);

	TSpeedTreeType::s_frameStarted = true;
	TSpeedTreeType::s_visibleCount = 0;
}

/**
 *	Marks end of current frame.  No draw call can be done after
 *	this method (and before beginFrame) is called. It's at this point
 *	that all deferred trees will actually be sent down the rendering
 *	pipeline, if batch rendering is enabled (it is by default).
 */
void SpeedTreeRenderer::endFrame()
{
	ScopedDogWatch watcher1(TSpeedTreeType::s_globalWatch);
	TSpeedTreeType::s_frameStarted = false;

	if (TSpeedTreeType::s_visibleCount > 0)
	{
		ScopedDogWatch watcher(TSpeedTreeType::s_drawWatch);
		TSpeedTreeType::prepareRenderContext();
		TSpeedTreeType::drawDeferredTrees();
	}
}

void SpeedTreeRenderer::flush()
{
	ScopedDogWatch watcher1(TSpeedTreeType::s_globalWatch);

	if (TSpeedTreeType::s_deferredCount)
	{
		ScopedDogWatch watcher(TSpeedTreeType::s_drawWatch);
		TSpeedTreeType::prepareRenderContext();
		TSpeedTreeType::drawDeferredTrees(false);

		TSpeedTreeType::s_deferredCount = 0;
	}
}

/**
 *	Draw an instance of this tree at the given world transform or computes
 *	and stores it's assossiated deffered draw data for later rendering,
 *	if batch rendering is enabled (it is by default).
 *
 *	@param	world		transform where to render tree in.
 */
void SpeedTreeRenderer::draw(const Matrix & worldt)
{
	// set to 1 if you want
	// to debug trees' rotation
#if 0
		Matrix world = worldt;
		Matrix rot;
		rot.setRotateY(TSpeedTreeType::s_time*0.3f);
		world.preMultiply(rot);
#else
		const Matrix & world = worldt;
#endif

	MF_ASSERT(TSpeedTreeType::s_frameStarted);
	ScopedDogWatch watcher1(TSpeedTreeType::s_globalWatch);

	if (TSpeedTreeType::s_curShadowCaster == NULL)
	{
		this->drawRenderPass(world);
	}
	else
	{
		this->drawShadowPass(world);
	}
}

void SpeedTreeRenderer::drawRenderPass(const Matrix & world)
{
	++TSpeedTreeType::s_visibleCount;

	// Render bounding box
	if (TSpeedTreeType::s_drawBoundingBox)
	{
		Moo::rc().push();
		Moo::rc().world(world);
		Moo::Material::setVertexColour();
		Geometrics::wireBox(
			this->treeType_->boundingBox,
			Moo::Colour(1.0, 0.0, 0.0, 0.0));
		Moo::rc().pop();
	}

	// Render bsp tree
	if (TSpeedTreeType::s_drawBSPTree)
	{
		this->treeType_->drawBSPTree(world);
	}

	// quit if tree rendering is not yet initialized,
	// disabled or if there is no effect setup
	if (!TSpeedTreeType::s_drawTrees ||
		TSpeedTreeType::s_effect.get() == NULL ||
		TSpeedTreeType::s_effect->nEffects() == 0)
	{
		return;
	}

	TSpeedTreeType::DeferData deferData;
	{
		ScopedDogWatch watcher2(TSpeedTreeType::s_prepareWatch);
		this->treeType_->computeDeferData(world, deferData);
	}

	if (TSpeedTreeType::s_batchRendering)
	{
		ScopedDogWatch watcher2(TSpeedTreeType::s_prepareWatch);

#if ENABLE_BB_OPTIMISER
			// optimised billboard
			if (TSpeedTreeType::s_optimiseBillboards &&
				this->treeType_->bbTreeType != -1    &&
				this->bbOptimiser_.exists())
			{
				static DogWatch drawBBoardsWatch("Billboards");
				ScopedDogWatch watcher(drawBBoardsWatch);

				if (deferData.lod.billboardDraw)
				{
					const float & alphaValue = deferData.lod.billboardFadeValue;

					if (this->treeID_ != -1)
					{
						// update tree's alpha test
						// value with the bb optimiser
						this->bbOptimiser_->updateTreeAlpha(
							this->treeID_, alphaValue, deferData.fogColour,
							deferData.fogNear, deferData.fogFar);
					}
					else
					{
						// If tree have not yet been registered with
						// the bb optimiser (and the billboard vertices
						// have already been calculated), do it now.
						this->treeID_ = this->bbOptimiser_->addTree(
							this->treeType_->bbTreeType, world,
							alphaValue, deferData.fogColour,
							deferData.fogNear, deferData.fogFar);
					}
				}
				if (deferData.lod.model3dDraw)
				{
					this->treeType_->deferTree(deferData);
				}
			}
			else
#endif // ENABLE_BB_OPTIMISER
			{
				this->treeType_->deferTree(deferData);
			}
	}
	else
	{
#if ENABLE_BB_OPTIMISER
			TSpeedTreeType::s_optimiseBillboards = false;
#endif // ENABLE_BB_OPTIMISER
		{
			ScopedDogWatch watcher2(TSpeedTreeType::s_prepareWatch);
			TSpeedTreeType::prepareRenderContext();
		}

		ScopedDogWatch watcher2(TSpeedTreeType::s_drawWatch);
		if (deferData.lod.model3dDraw)
		{
			this->treeType_->updateWind();
			this->treeType_->uploadWindMatrixConstants();
			TSpeedTreeType::uploadRenderConstants(deferData);

			if (TSpeedTreeType::s_drawBranches && deferData.lod.branchDraw)
			{
				this->treeType_->setBranchModel();
				this->treeType_->setBranchRenderStates();
				TSpeedTreeType::beginPass();
				this->treeType_->drawBranch(deferData);
				TSpeedTreeType::endPass();
			}

			this->treeType_->setCompositeRenderStates();

			if (TSpeedTreeType::s_drawFronds && deferData.lod.frondDraw)
			{
				this->treeType_->setFrondModel();
				this->treeType_->setFrondRenderStates();
				TSpeedTreeType::beginPass();
				this->treeType_->drawFrond(deferData);
				TSpeedTreeType::endPass();
			}

			if (TSpeedTreeType::s_drawLeaves && deferData.lod.leafDraw)
			{
				this->treeType_->setLeafRenderStates();
				TSpeedTreeType::beginPass();
				this->treeType_->drawLeaf(deferData);
				TSpeedTreeType::endPass();
			}
		}
		else
		{
			this->treeType_->setCompositeRenderStates();
			TSpeedTreeType::uploadRenderConstants(deferData);
		}

		if (TSpeedTreeType::s_drawBillboards && deferData.lod.billboardDraw)
		{
			// always try to render the billboards. They will
			// not be actually rendered if they are not active
			this->treeType_->setBillboardRenderStates();
			TSpeedTreeType::beginPass();
			this->treeType_->drawBillboard(deferData);
			TSpeedTreeType::endPass();
		}
	}
}


void SpeedTreeRenderer::drawShadowPass(const Matrix & world)
{
	// TODO: compute deferData only
	//       once per tree per frame
	TSpeedTreeType::DeferData deferData;
	{
		ScopedDogWatch watcher2(TSpeedTreeType::s_prepareWatch);
		this->treeType_->computeDeferData(world, deferData);
	}

	ScopedDogWatch watcher2(TSpeedTreeType::s_drawWatch);
	if (deferData.lod.model3dDraw &&
		TSpeedTreeType::s_shadowsFX.get() != NULL)
	{
#if !DONT_USE_DUPLO
		TSpeedTreeType::s_curShadowCaster->setupConstants(
			*TSpeedTreeType::s_shadowsFX);
#endif // DONT_USE_DUPLO

		this->treeType_->updateWind();
		this->treeType_->uploadWindMatrixConstants();
		TSpeedTreeType::uploadRenderConstants(deferData);

		if (TSpeedTreeType::s_drawBranches && deferData.lod.branchDraw)
		{
			this->treeType_->setBranchRenderStates();
			this->treeType_->setBranchModel();

			TSpeedTreeType::beginPass();
			this->treeType_->drawBranch(deferData);
			TSpeedTreeType::endPass();
		}
		if (TSpeedTreeType::s_drawLeaves && deferData.lod.leafDraw)
		{
			this->treeType_->setLeafRenderStates();

			TSpeedTreeType::beginPass();
			this->treeType_->drawLeaf(deferData);
			TSpeedTreeType::endPass();
		}
	}
}

/**
 *	Notifies this tree that it's position has been updated. Calling this every
 *	time the transform of a tree changes is necessary because some optimizations
 *	assume the tree to be static most of the time. Calling this allows the tree
 *	to refresh all cached data that becomes obsolete whenever a change occours.
 *
 *	@param	chunk		pointer chunk owning this tree (if NULL, tree won't
 *						try to use billboard optimizations).
 */
void SpeedTreeRenderer::resetTransform(const Chunk * chunk)
{
#if ENABLE_BB_OPTIMISER
		if (this->treeID_ != -1)
		{
			this->bbOptimiser_->removeTree(this->treeID_);
			this->treeID_ = -1;
		}
		if (chunk != NULL)
		{
			this->bbOptimiser_ = BillboardOptimiser::retrieve(*chunk);
		}
#endif
}

/**
 *	Returns tree bounding box.
 */
const BoundingBox & SpeedTreeRenderer::boundingBox() const
{
	return this->treeType_->boundingBox;
}

/**
 *	Returns tree BSP-Tree.
 */
const BSPTree & SpeedTreeRenderer::bsp() const
{
	return *this->treeType_->bspTree;
}

/**
 *	Returns name of SPT file used to generate this tree.
 */
const char * SpeedTreeRenderer::filename() const
{
	return this->treeType_->filename.c_str();
}

/**
 *	Returns seed number used to generate this tree.
 */
uint SpeedTreeRenderer::seed() const
{
	return this->treeType_->seed;
}

// ------------------------------------------------------ TSpeedTreeType

/**
 *	Default constructor.
 */
TSpeedTreeType::TSpeedTreeType() :
	speedTree(NULL),
	speedWind(),
	boundingBox(BoundingBox::s_insideOut_),
	bspTree(NULL),
	bspVertices(NULL),
	bspVerticesCount(0),
	branches(),
	fronds(),
	leaves(),
	billboards(),
	leafRockScalar(0.0f),
	leafRustleScalar(0.0f),
	bbTreeType(-1),
	seed(1),
	filename(),
	lastTicktime(-1),
	refCounter(0)
{}

/**
 *	Default constructor.
 */
TSpeedTreeType::~TSpeedTreeType()
{
#if ENABLE_BB_OPTIMISER
		if (this->bbTreeType != -1)
		{
			BillboardOptimiser::delTreeType(this->bbTreeType);
		}
#endif
}


/**
 *	Finalizes the TSpeedTreeType class (static).
 */
void TSpeedTreeType::fini()
{
	TSpeedTreeType::recycleTreeTypeObjects();
	TSpeedTreeType::clearDXResources();

	if (!TSpeedTreeType::s_rendererMap.empty())
	{
		WARNING_MSG(
			"TSpeedTreeType::fini: tree types still loaded: %d\n",
			TSpeedTreeType::s_rendererMap.size());
	}
}

/**
 *	Ticks (static).
 *
 *	@param	dTime	time elapsed since last tick.
 */
void TSpeedTreeType::tick(float dTime)
{
	TSpeedTreeType::s_time += std::min(dTime, c_maxDTime);
}

/**
 *	Prepares trees to be rendered (static).
 */
void TSpeedTreeType::update()
{
	bool initializedATree = TSpeedTreeType::doSyncInit();
	bool deletedATree = TSpeedTreeType::recycleTreeTypeObjects();
	if (deletedATree || initializedATree)
	{
		TSpeedTreeType::s_uniqueCount = TSpeedTreeType::s_rendererMap.size();
		TSpeedTreeType::updateRenderGroups();
	}
}

/**
 *	Deletes tree type objects no longer being used and also clear
 *	all deferred trees for those that are still being used (static).
 */
bool TSpeedTreeType::recycleTreeTypeObjects()
{
	bool deletedAnyTreeType = false;
	typedef TSpeedTreeType::TreeRendererMap::iterator iterator;
	iterator renderIt = TSpeedTreeType::s_rendererMap.begin();
	while (renderIt != TSpeedTreeType::s_rendererMap.end())
	{
		if (renderIt->second->refCount() == 0)
		{
			delete renderIt->second;
			renderIt = TSpeedTreeType::s_rendererMap.erase(renderIt);
			deletedAnyTreeType = true;
		}
		else
		{
			++renderIt;
		}
	}

	return deletedAnyTreeType;
}

/**
 *	Do the pending synchronous initialization
 *	on newly loaded tree type objects (static).
 */
bool TSpeedTreeType::doSyncInit()
{
	SimpleMutexHolder mutexHolder(TSpeedTreeType::s_syncInitLock);
	const int listSize = TSpeedTreeType::s_syncInitList.size();
	typedef TSpeedTreeType::InitVector::const_iterator citerator;
	citerator initIt  = TSpeedTreeType::s_syncInitList.begin();
	citerator initEnd = TSpeedTreeType::s_syncInitList.end();
	while (initIt != initEnd)
	{
		(*initIt)->syncInit();

		// now that initialization is
		// finished, register renderer
		const char * filename = (*initIt)->filename.c_str();
		std::string treeDefName = createTreeDefName(filename, (*initIt)->seed);
		TSpeedTreeType::s_rendererMap.insert(
			std::make_pair(treeDefName, *initIt));

		++initIt;
	}
	TSpeedTreeType::s_syncInitList.clear();

	return listSize > 0;
}

void TSpeedTreeType::releaseResources()
{
	this->branches.lod.clear();
	this->branches.texture = NULL;

	this->fronds.lod.clear();
	this->fronds.texture = NULL;

	this->leaves.lod.clear();
	this->leaves.texture = NULL;

	this->billboards.lod.clear();
	this->billboards.texture = NULL;
}

/**
 *	Deletes all tree type objects in the pending initi alization list (static).
 */
void TSpeedTreeType::clearDXResources()
{
	{
		SimpleMutexHolder mutexHolder(TSpeedTreeType::s_syncInitLock);
		while (!s_syncInitList.empty())
		{
			s_syncInitList.back()->releaseResources();
			s_syncInitList.pop_back();
		}
	}

	static DogWatch releaseWatch("release");
	FOREACH_TREETYPE_BEGIN(releaseWatch)
	{
		RENDER->releaseResources();
	}
	FOREACH_TREETYPE_END
}

/**
 *	Updates all wind related data for this tree type.
 */
void TSpeedTreeType::updateWind()
{
	if (TSpeedTreeType::s_playAnimation &&
		this->lastTicktime != TSpeedTreeType::s_time)
	{
		const float & windVelX = TSpeedTreeType::s_windVelX;
		const float & windVelY = TSpeedTreeType::s_windVelY;
		Vector3 wind(windVelX, windVelY, 0);
		float strength = std::min(wind.length()/c_maxWindVelocity, 1.0f);
		wind.normalise();
		this->speedWind.SetWindStrengthAndDirection(
			strength, wind.x, wind.z, wind.y);

		float camDir[3], camPos[3];
		CSpeedTreeRT::GetCamera(camPos, camDir);
		const float & time = TSpeedTreeType::s_time;
		this->speedWind.Advance(time, true, true,
			camDir[0], camDir[1], camDir[2]);

		this->lastTicktime = time;
	}
}

/**
 *	Add an instance of this tree to the list of deferred
 *	trees for later rendering.
 *
 *	@param	deferData	the DeferData struct for this instance.
 */
void TSpeedTreeType::deferTree(const DeferData & deferData)
{
	this->deferredTrees.push_back(deferData);
	++TSpeedTreeType::s_deferredCount;
}

/**
 *	Computes the DeferData struct to render a tree at the given world transform.
 *
 *	@param	world		transform where tree should be rendered.
 *	@param	o_deferData	(out) will store the defer data struct.
 */
void TSpeedTreeType::computeDeferData(
	const Matrix & world,
	DeferData    & o_deferData)
{
	static DogWatch computeLODWatch("LOD");
	static DogWatch computeMatricesWatch("Matrices");
	static DogWatch computeLeavesWatch("Leaves");
	static DogWatch computeBBWatch("Billboards");

	{
		ScopedDogWatch watcher(computeMatricesWatch);
		o_deferData.world        = world;
		o_deferData.projection   = TSpeedTreeType::s_projection;

		Matrix worldView;
		D3DXMatrixMultiply(&worldView, &world, &TSpeedTreeType::s_view);
		D3DXMatrixMultiply(
			&o_deferData.worldViewProj,
			&worldView,
			&o_deferData.projection);

		o_deferData.rotation.setRotate(world.yaw(), world.pitch(), world.roll());

		float scaleX = world.applyToUnitAxisVector(0).length();
		float scaleY = world.applyToUnitAxisVector(1).length();
		float scaleZ = world.applyToUnitAxisVector(2).length();

		Matrix scaleTranslate;
		scaleTranslate.setScale(scaleX, scaleY, scaleZ);
		scaleTranslate.postTranslateBy(world.applyToOrigin());

		D3DXMatrixMultiply(
			&o_deferData.scaleTranslateView,
			&scaleTranslate,
			&TSpeedTreeType::s_view);

		D3DXMatrixMultiply(
			&o_deferData.scaleTranslateViewProj,
			&o_deferData.scaleTranslateView,
			&o_deferData.projection);

		o_deferData.treeScale = 0.333f * (scaleX + scaleY + scaleZ);
	}

	{
		ScopedDogWatch watcher(computeLODWatch);

		this->speedTree->SetTreePosition(
			world._41, world._43, world._42);

		if (TSpeedTreeType::s_lodMode == -1.0)
		{
			this->speedTree->ComputeLodLevel();
		}
		else if (TSpeedTreeType::s_lodMode == -2.0)
		{
			D3DXVECTOR4 pos;
			const Matrix & invView = TSpeedTreeType::s_invView;
			D3DXVec3Transform(&pos, &D3DXVECTOR3(0, 0, 0), &invView);

			const float & lodNear = TSpeedTreeType::s_lodNear;
			const float & lodFar  = TSpeedTreeType::s_lodFar;
			float distance = (world.applyToOrigin() - Vector3(pos.x, pos.y, pos.z)).length();
			float lodLevel = 1 - (distance - lodNear) / (lodFar - lodNear);
			lodLevel = std::min(lodLevel, 1.0f);
			lodLevel = std::max(lodLevel, 0.0f);
			// Manually rounding very small lod values to avoid problems
			// in the speedtree lod calculation.(avoiding problem, not fixing)
			if (lodLevel < 0.0001f)
				lodLevel = 0.0;
			this->speedTree->SetLodLevel(lodLevel);
		}
		else
		{
			this->speedTree->SetLodLevel(s_lodMode);
		}
	}

	// set to 1 if you want
	// to debug trees' lod
#if 0
		float lodLevel = 0.5f * (1+sin(TSpeedTreeType::s_time*0.3f));
		this->speedTree->SetLodLevel(lodLevel);
#endif

	{
		ScopedDogWatch watcher(computeLeavesWatch);
		this->computeLodData(o_deferData.lod);
	}

#ifdef EDITOR_ENABLED
		int32 fog = Moo::rc().getRenderState(D3DRS_FOGCOLOR);
		o_deferData.fogColour.z = ((fog & (0xff << 8*0)) >> 8*0) / 255.0f;
		o_deferData.fogColour.y = ((fog & (0xff << 8*1)) >> 8*1) / 255.0f;
		o_deferData.fogColour.x = ((fog & (0xff << 8*2)) >> 8*2) / 255.0f;
		o_deferData.fogColour.w = ((fog & (0xff << 8*3)) >> 8*3) / 255.0f;
		o_deferData.fogNear   = Moo::rc().fogNear();
		o_deferData.fogFar    = Moo::rc().fogFar();
#endif
}

/**
 *	Upload transform matrice and other render constants to the rendering device.
 *
 *	@param	deferData		the deferred rendering data for this tree.
 *	@param	commit			true if changes should be commited (false can be
 *							used whenever a subsequent processing will need
 *							to commit other constants to the effect file).
 */
void TSpeedTreeType::uploadRenderConstants(const DeferData & deferData, bool commit)
{
	static DogWatch computeLODWatch("renderConsts");
	static DogWatch commitLODWatch("commit");
	ScopedDogWatch watcher(computeLODWatch);

	ShadowCaster * caster = TSpeedTreeType::s_curShadowCaster;
	ComObjectWrap<ID3DXEffect> pEffect = (caster == NULL)
		? TSpeedTreeType::s_effect->pEffect()->pEffect()
		: TSpeedTreeType::s_shadowsFX->pEffect()->pEffect();

	// for optimisation sake, we're using an array of
	// matrices instead of uploading each matrix at a time.
#if ENABLE_MATRIX_OPT
		pEffect->SetMatrixArray("g_matrices", (D3DXMATRIX*)&deferData.world, 6);
#else
		pEffect->SetMatrix("g_world", &deferData.world);
		pEffect->SetMatrix("g_rotation", &deferData.rotation);
		pEffect->SetMatrix("g_projection", &deferData.projection);
		pEffect->SetMatrix("g_worldViewProj", &deferData.worldViewProj);
		pEffect->SetMatrix("g_scaleTranslateView", &deferData.scaleTranslateView);
		pEffect->SetMatrix("g_scaleTranslateViewProj", &deferData.scaleTranslateViewProj);
#endif

#ifdef EDITOR_ENABLED
		pEffect->SetVector("fogColour", &deferData.fogColour );
		pEffect->SetFloat("fogStart", deferData.fogNear );
		pEffect->SetFloat("fogEnd", deferData.fogFar );
#endif

	if (commit)
	{
		ScopedDogWatch watcher(commitLODWatch);
		pEffect->CommitChanges();
	}
}

/**
 *	Computes all LOD data needed to render this tree. Assumes the speedtree
 *	object have already been updated in regard to its current LOD level.
 *
 *	@param	o_lod	(out) will store all lod data
 */
void TSpeedTreeType::computeLodData(LodData & o_lod)
{
	CSpeedTreeRT::SLodValues sLod;
	this->speedTree->GetLodValues(sLod);

	o_lod.branchLod   = sLod.m_nBranchActiveLod;
	o_lod.branchDraw  = sLod.m_nBranchActiveLod >= 0;
	o_lod.branchAlpha = sLod.m_fBranchAlphaTestValue;

	o_lod.frondLod    = sLod.m_nFrondActiveLod;
	o_lod.frondDraw   = sLod.m_nFrondActiveLod >= 0;
	o_lod.frondAlpha  = sLod.m_fFrondAlphaTestValue;

	o_lod.leafLods[0] = sLod.m_anLeafActiveLods[0];
	o_lod.leafLods[1] = sLod.m_anLeafActiveLods[1];
	o_lod.leafAlphaValues[0] = sLod.m_afLeafAlphaTestValues[0];
	o_lod.leafAlphaValues[1] = sLod.m_afLeafAlphaTestValues[1];
	o_lod.leafLodCount =
		(sLod.m_anLeafActiveLods[0] >= 0 ? 1 : 0) +
		(sLod.m_anLeafActiveLods[1] >= 0 ? 1 : 0);
	o_lod.leafDraw     = o_lod.leafLodCount > 0;

	o_lod.billboardFadeValue = sLod.m_fBillboardFadeOut;
	o_lod.billboardDraw      = sLod.m_fBillboardFadeOut > 0;

	o_lod.model3dDraw =
		o_lod.branchDraw ||
		o_lod.frondDraw  ||
		o_lod.leafDraw;
}

/**
 *	Draw all deferred trees sorting them according to their required
 *	rendering states. Sorting is performed by iterating through all types
 *	of trees, setting the rendering states for it and drawing all deferred
 *	tress for that type. In fact, this nested iteration is performed four
 *	times, one for each part of a tree model: branches, fronds, leaves and
 *	billboards.
 */
void TSpeedTreeType::drawDeferredTrees(bool drawBillboards)
{
	static DogWatch windWatch("Wind");
	static DogWatch branchesWatch("Branches");
	static DogWatch compositeWatch("Composite");
	static DogWatch renderWatch("Render");
	static DogWatch groupStateWatch("Group State");
	static DogWatch partStateWatch("Part State");

	if (TSpeedTreeType::s_rendererMap.empty()  ||
		TSpeedTreeType::s_effect.get() == NULL ||
		TSpeedTreeType::s_effect->nEffects() == 0)
	{
		return;
	}

	Moo::EffectVisualContext::instance().isOutside( true );

	// first, update wind animation
	FOREACH_TREETYPE_BEGIN(windWatch)
	{
		if (!RENDER->deferredTrees.empty())
		{
			RENDER->updateWind();
		}
	}
	FOREACH_TREETYPE_END

	// then, render trees' branches
	std::string lastFilename = "";
	FOREACH_TREETYPE_BEGIN(branchesWatch)
	{
		if (!RENDER->deferredTrees.empty())
		{
			RENDER->setBranchModel();
			if (RENDER->filename != lastFilename)
			{
				RENDER->setBranchRenderStates();
				lastFilename = RENDER->filename;
			}
			RENDER->uploadWindMatrixConstants();
			RENDER->drawDeferredBranches();
		}
	}
	FOREACH_TREETYPE_END

	// then, for each tree in each group,
	// render the three remaining tree parts
	typedef void (TSpeedTreeType::*RenderMethodType)(void);
	static const int numTreeParts = 3;
	RenderMethodType drawFuncs[numTreeParts] =
	{
		&TSpeedTreeType::drawDeferredLeaves,
		&TSpeedTreeType::drawDeferredFronds,
		&TSpeedTreeType::drawDeferredBillboards,
	};

	typedef void (TSpeedTreeType::*StateMethodType)(void);
	StateMethodType stateFuncs[numTreeParts] =
	{
		&TSpeedTreeType::setLeafRenderStates,
		&TSpeedTreeType::setFrondRenderStates,
		&TSpeedTreeType::setBillboardRenderStates,
	};

	compositeWatch.start();
	RendererGroupVector & renderGroups = TSpeedTreeType::s_renderGroups;
	RendererGroupVector::iterator groupIt = renderGroups.begin();
	RendererGroupVector::iterator groupEnd = renderGroups.end();

#if ENABLE_BB_OPTIMISER
		const bool & optimiseBB = TSpeedTreeType::s_optimiseBillboards;
#else
		static const bool optimiseBB = false;
#endif

	// don't do billboards if
	// optimization is enabled
	while (groupIt != groupEnd)
	{
		// make sure there is at least one tree
		// to render for this group before proceeding
		RendererVector::iterator renderIt = groupIt->begin();
		RendererVector::iterator renderEnd = groupIt->end();
		while (renderIt != renderEnd)
		{
			if (!(*renderIt)->deferredTrees.empty())
			{
				break;
			}
			++renderIt;
		}
		if (renderIt != renderEnd)
		{
			groupStateWatch.start();
			groupIt->back()->setCompositeRenderStates();
			groupStateWatch.stop();

			for (int part=0; part<numTreeParts; ++part)
			{
				partStateWatch.start();
				(groupIt->back()->*stateFuncs[part])();
				partStateWatch.stop();

				renderWatch.start();
				RendererVector::iterator renderIt = groupIt->begin();
				RendererVector::iterator renderEnd = groupIt->end();
				while (renderIt != renderEnd)
				{
					if (!(*renderIt)->deferredTrees.empty())
					{
						(*renderIt)->uploadWindMatrixConstants();
						((*renderIt)->*drawFuncs[part])();
					}
					++renderIt;
				}
				renderWatch.stop();
			}
		}
		++groupIt;
	}
	compositeWatch.stop();

	// now, if optimization is
	// enabled, do the billboards.
#if ENABLE_BB_OPTIMISER
		if (drawBillboards && TSpeedTreeType::s_optimiseBillboards)
		{
			static DogWatch drawBBoardsWatch("Billboards");
			ScopedDogWatch watcher(drawBBoardsWatch);

			Matrix viewProj;
			D3DXMatrixMultiply(&viewProj,
				&TSpeedTreeType::s_view,
				&TSpeedTreeType::s_projection);

			BillboardOptimiser::FrameData frameData(
				viewProj,
				TSpeedTreeType::s_invView,
				TSpeedTreeType::s_sunDirection,
				TSpeedTreeType::s_sunDiffuse,
				TSpeedTreeType::s_sunAmbient,
				TSpeedTreeType::s_texturedTrees);

			BillboardOptimiser::drawAll(frameData);
		}
#endif

	// clear deferred trees
	typedef TSpeedTreeType::TreeRendererMap::iterator iterator;
	iterator renderIt  = TSpeedTreeType::s_rendererMap.begin();
	iterator renderEnd = TSpeedTreeType::s_rendererMap.end();
	while (renderIt != renderEnd)
	{
		renderIt->second->clearDeferredTrees();
		++renderIt;
	}
}

/**
 *	Iterates through all deferred trees for this tree type, drawing
 *	their branches. Assumes all render states have already been set.
 */
void TSpeedTreeType::drawDeferredBranches()
{
	static DogWatch drawBranchesWatch("Branches");
	ScopedDogWatch watcher(drawBranchesWatch);

	if (TSpeedTreeType::s_drawBranches)
	{
		TSpeedTreeType::beginPass();
		FOREACH_DEFEREDTREE_BEGIN
		{
			if (DEFERDATA.lod.branchDraw)
			{
				TSpeedTreeType::uploadRenderConstants(DEFERDATA);
				this->drawBranch(DEFERDATA);
			}
		}
		FOREACH_DEFERREDTREE_END
		TSpeedTreeType::endPass();
	}
}

/**
 *	Iterates through all deferred trees for this tree type, drawing
 *	their fronds. Assumes all render states have already been set.
 */
void TSpeedTreeType::drawDeferredFronds()
{
	static DogWatch drawFrondsWatch("Fronds");
	ScopedDogWatch watcher(drawFrondsWatch);

	if (TSpeedTreeType::s_drawFronds)
	{
		this->setFrondModel();
		TSpeedTreeType::beginPass();
		FOREACH_DEFEREDTREE_BEGIN
		{
			if (DEFERDATA.lod.frondDraw)
			{
				TSpeedTreeType::uploadRenderConstants(DEFERDATA);
				this->drawFrond(DEFERDATA);
			}
		}
		FOREACH_DEFERREDTREE_END
		TSpeedTreeType::endPass();
	}
}

/**
 *	Iterates through all deferred trees for this tree type, drawing
 *	their leaves. Assumes all render states have already been set.
 */
void TSpeedTreeType::drawDeferredLeaves()
{
	static DogWatch drawLeavesWatch("Leaves");
	ScopedDogWatch watcher(drawLeavesWatch);

	if (TSpeedTreeType::s_drawLeaves)
	{
		TSpeedTreeType::beginPass();
		FOREACH_DEFEREDTREE_BEGIN
		{
			if (DEFERDATA.lod.leafDraw)
			{
				TSpeedTreeType::uploadRenderConstants(DEFERDATA, false);
				this->drawLeaf(DEFERDATA);
			}
		}
		FOREACH_DEFERREDTREE_END
		TSpeedTreeType::endPass();
	}
}

/**
 *	Iterates through all deferred trees for this tree type, drawing
 *	their billboards. Assumes all render states have already been set.
 */
void TSpeedTreeType::drawDeferredBillboards()
{
	// if billboard optimization is on and
	// this tree has it setup, skip this call
#if ENABLE_BB_OPTIMISER
	if (!TSpeedTreeType::s_optimiseBillboards ||
		this->bbTreeType == -1)
#endif
	{
		static DogWatch drawBBoardsWatch("Billboards");
		ScopedDogWatch watcher(drawBBoardsWatch);

		if (TSpeedTreeType::s_drawBillboards)
		{
			TSpeedTreeType::beginPass();
			FOREACH_DEFEREDTREE_BEGIN
			{
				if (DEFERDATA.lod.billboardDraw)
				{
					TSpeedTreeType::uploadRenderConstants(DEFERDATA, false);
					this->drawBillboard(DEFERDATA);
				}
			}
			FOREACH_DEFERREDTREE_END
			TSpeedTreeType::endPass();
		}
	}
}

/**
 *	Draws the branches of a single tree of this type, using the given
 *	DeferData structure. Assumes all render states have already been set.
 *
 *	@param	deferData	the DeferData struct for the tree to be drawn.
 */
void TSpeedTreeType::drawBranch(const DeferData & deferData)
{
	if (this->branches.lod.empty())
	{
		return;
	}

	const int & lod = deferData.lod.branchLod;
	typedef BranchRenderData::IndexBufferVector::const_iterator citerator;
	citerator stripIt  = this->branches.lod[lod].strips.begin();
	citerator stripEnd = this->branches.lod[lod].strips.end();
	while (stripIt != stripEnd)
	{
		if ((*stripIt)->isValid())
		{
			Moo::rc().setRenderState(
				D3DRS_ALPHAREF,
				(DWORD)deferData.lod.branchAlpha);

			(*stripIt)->activate();
			int primitivesCount = (*stripIt)->size() - 2;
			{
				ScopedDogWatch watcher(TSpeedTreeType::s_primitivesWatch);
				Moo::rc().drawIndexedPrimitive(
					D3DPT_TRIANGLESTRIP, 0, 0,
					this->branches.lod[0].vertices->size(),
					0, primitivesCount);
			}
			Moo::rc().addToPrimitiveCount(primitivesCount);
			Moo::rc().addToPrimitiveGroupCount();
		}
		++stripIt;
	}
}

/**
 *	Draws the fronds of a single tree of this type, using the given
 *	DeferData structure. Assumes all render states have already been set.
 *
 *	@param	deferData	the DeferData struct for the tree to be drawn.
 */
void TSpeedTreeType::drawFrond(const DeferData & deferData)
{
	if (this->fronds.lod.empty())
	{
		return;
	}

	const int & lod = deferData.lod.frondLod;
	typedef BranchRenderData::IndexBufferVector::const_iterator citerator;
	citerator stripIt  = this->fronds.lod[lod].strips.begin();
	citerator stripEnd = this->fronds.lod[lod].strips.end();
	while (stripIt != stripEnd)
	{
		if ((*stripIt)->isValid())
		{
			Moo::rc().setRenderState(
				D3DRS_ALPHAREF,
				(DWORD)deferData.lod.frondAlpha);

			(*stripIt)->activate();
			int primitivesCount = (*stripIt)->size() - 2;
			{
				ScopedDogWatch watcher(TSpeedTreeType::s_primitivesWatch);
				Moo::rc().drawIndexedPrimitive(
					D3DPT_TRIANGLESTRIP, 0, 0,
					this->fronds.lod[0].vertices->size(),
					0, primitivesCount);
			}
			Moo::rc().addToPrimitiveCount(primitivesCount);
			Moo::rc().addToPrimitiveGroupCount();
		}
		++stripIt;
	}
}

/**
 *	Draws the leaves of a single tree of this type, using the given
 *	DeferData structure. Assumes all render states have already been set.
 *
 *	@param	deferData	the DeferData struct for the tree to be drawn.
 */
void TSpeedTreeType::drawLeaf(const DeferData & deferData)
{
	if (this->leaves.lod.empty())
	{
		return;
	}

	ShadowCaster * caster = TSpeedTreeType::s_curShadowCaster;
	ComObjectWrap<ID3DXEffect> pEffect = (caster == NULL)
		? TSpeedTreeType::s_effect->pEffect()->pEffect()
		: TSpeedTreeType::s_shadowsFX->pEffect()->pEffect();

	pEffect->SetFloat("g_treeScale", deferData.treeScale);
	pEffect->CommitChanges();

	// overriding the culling for a mirror
	if (Moo::rc().mirroredTransform())
		Moo::rc().setRenderState( D3DRS_CULLMODE, D3DCULL_CW);

	for (int i=0; i<deferData.lod.leafLodCount; ++i)
	{
		const int & lod = deferData.lod.leafLods[i];
		if (this->leaves.lod[lod].vertices->isValid())
		{
			Moo::rc().setRenderState(
				D3DRS_ALPHAREF,
				(DWORD)deferData.lod.leafAlphaValues[i]);

			this->leaves.lod[lod].vertices->activate();
			int primitivesCount = this->leaves.lod[lod].vertices->size() / 3;
			{
				ScopedDogWatch watcher(TSpeedTreeType::s_primitivesWatch);
				Moo::rc().drawPrimitive(D3DPT_TRIANGLELIST, 0, primitivesCount);
			}
			Moo::rc().addToPrimitiveCount(primitivesCount);
			Moo::rc().addToPrimitiveGroupCount();
		}
	}
}

/**
 *	Draws the billboard of a single tree of this type, using the given
 *	DeferData structure. Assumes all render states have already been set.
 *
 *	@param	deferData	the DeferData struct for the tree to be drawn.
 */
void TSpeedTreeType::drawBillboard(const DeferData & deferData)
{
	if (!this->billboards.lod.empty() &&
		this->billboards.lod[0].vertices->isValid())
	{
		ComObjectWrap<ID3DXEffect> pEffect =
			TSpeedTreeType::s_effect->pEffect()->pEffect();

		const float & minAlpha  = BillboardVertex::s_minAlpha;
		const float & maxAlpha  = BillboardVertex::s_maxAlpha;
		const float & fadeValue = deferData.lod.billboardFadeValue;
		const float   alphaRef  = 1 + fadeValue * (minAlpha/maxAlpha -1);
		pEffect->SetFloat("g_bbAlphaRef", alphaRef);
		pEffect->CommitChanges();

		int triCount = this->billboards.lod[0].vertices->size() / 3;
		{
			ScopedDogWatch watcher(TSpeedTreeType::s_primitivesWatch);
			this->billboards.lod[0].vertices->activate();
			Moo::rc().drawPrimitive(D3DPT_TRIANGLELIST, 0, triCount);
		}
		Moo::rc().addToPrimitiveCount(triCount);
		Moo::rc().addToPrimitiveGroupCount();
	}
}

/**
 *	Draws the BSP-Tree for this tree type, at the given world transform.
 *
 *	@param	world	world transform of tree whose bounding box is to be drawn.
 */
void TSpeedTreeType::drawBSPTree(const Matrix & world) const
{
	Moo::Material::setVertexColour();
	Moo::rc().setVertexShader(NULL);
	Moo::rc().setFVF(Moo::VertexXYZL::fvf());
	Moo::rc().setRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	Moo::rc().setRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	Moo::rc().setRenderState(D3DRS_LIGHTING, FALSE);
	Moo::rc().setRenderState(D3DRS_ZWRITEENABLE, TRUE);
	Moo::rc().setRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
	Moo::rc().setRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
	Moo::rc().setRenderState(D3DRS_FOGENABLE, FALSE);
	Moo::rc().setTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	Moo::rc().setTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
	Moo::rc().setTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	Moo::rc().setTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	Moo::rc().setTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	Moo::rc().device()->SetTransform(D3DTS_WORLD, &world);
	Moo::rc().device()->SetTransform(D3DTS_VIEW, &Moo::rc().view());
	Moo::rc().device()->SetTransform(D3DTS_PROJECTION, &Moo::rc().projection());
	Moo::rc().drawPrimitiveUP(D3DPT_TRIANGLELIST,
		this->bspVerticesCount / 3,
		this->bspVertices, sizeof(Moo::VertexXYZL));
}

/**
 *	Clears the list of deferred trees for this tree type.
 */
void TSpeedTreeType::clearDeferredTrees()
{
	//Calling erase instead of clear on the vector because
	//it's faster.
	this->deferredTrees.erase(
		this->deferredTrees.begin(),
		this->deferredTrees.end());
}

/**
 *	Sets the global render states for drawing the trees.
 */
void TSpeedTreeType::prepareRenderContext()
{
	// update lighting setup
	const Moo::Colour & diffuse   = TSpeedTreeType::s_sunDiffuse;
	const Moo::Colour & ambient   = TSpeedTreeType::s_sunAmbient;
	const Vector3     & direction = TSpeedTreeType::s_sunDirection;
	float lighting[] =
	{
		direction.x, direction.y, direction.z, 1.0f,
		diffuse.r, diffuse.g, diffuse.b, 1.0f,
		ambient.r, ambient.g, ambient.b, 1.0f,
	};

	ComObjectWrap<ID3DXEffect> pEffect =
		TSpeedTreeType::s_effect->pEffect()->pEffect();
	pEffect->SetVectorArray("g_sun", (D3DXVECTOR4*)lighting, 3);

#ifndef EDITOR_ENABLED
		pEffect->SetVector("fogColour", &FogController::instance().v4colour() );
		pEffect->SetFloat("fogStart", Moo::rc().fogNear() );
		pEffect->SetFloat("fogEnd", Moo::rc().fogFar() );
#endif

	Moo::EffectVisualContext::instance().initConstants();
}

/**
 *	Sets the render states for drawing the fronds,
 *	leaves, and billboards of this type of tree.
 */
void TSpeedTreeType::setCompositeRenderStates() const
{
	ComObjectWrap<ID3DXEffect> pEffect =
		TSpeedTreeType::s_effect->pEffect()->pEffect();

	pEffect->SetTexture("g_diffuseMap", this->leaves.texture->pTexture());
	pEffect->SetBool("g_texturedTrees", TSpeedTreeType::s_texturedTrees);
	pEffect->CommitChanges();
}

/**
 *	Sets the render states for drawing the branches of this type of tree.
 */
void TSpeedTreeType::setBranchRenderStates() const
{
	ComObjectWrap<ID3DXEffect> pEffect;
	if (TSpeedTreeType::s_curShadowCaster == NULL)
	{
		TSpeedTreeType::s_effect->hTechnique("branches", 0);
		pEffect = TSpeedTreeType::s_effect->pEffect()->pEffect();

		const float* branchMaterial = this->speedTree->GetBranchMaterial();
		float material[] =
		{
			branchMaterial[0], branchMaterial[1],
			branchMaterial[2], 1.0f, // branch diffuse color
			branchMaterial[3], branchMaterial[4],
			branchMaterial[5], 1.0f, // branch ambient color
		};

		pEffect->SetVectorArray("g_material", (const D3DXVECTOR4*)&material[0], 2);
		pEffect->SetTexture("g_diffuseMap", this->branches.texture->pTexture());
		pEffect->SetBool("g_texturedTrees", TSpeedTreeType::s_texturedTrees);
		pEffect->SetBool("g_cullEnabled", true);
	}
	else
	{
		TSpeedTreeType::s_shadowsFX->hTechnique("branches", 0);
		pEffect = TSpeedTreeType::s_shadowsFX->pEffect()->pEffect();
	}

	pEffect->CommitChanges();

	Moo::rc().setFVF(BranchVertex::fvf());
}

/**
 *	Activates the vertex buffer to draw the branches of this type of tree.
 */
void TSpeedTreeType::setBranchModel() const
{
	if (this->branches.lod[0].vertices->isValid())
	{
		this->branches.lod[0].vertices->activate();
	}
}

/**
 *	Sets the render states for drawing the fronds of this type of tree.
 */
void TSpeedTreeType::setFrondRenderStates()
{
	TSpeedTreeType::s_effect->hTechnique("branches", 0);

	const float* frondMaterial = this->speedTree->GetFrondMaterial();
    float material[] =
    {
        frondMaterial[0], frondMaterial[1], frondMaterial[2], 1.0f,	// branch diffuse color
        frondMaterial[3], frondMaterial[4], frondMaterial[5], 1.0f,	// branch ambient color
    };

	ComObjectWrap<ID3DXEffect> pEffect =
		TSpeedTreeType::s_effect->pEffect()->pEffect();

	pEffect->SetVectorArray("g_material", (const D3DXVECTOR4*)&material[0], 2);
	pEffect->SetTexture("g_diffuseMap", this->leaves.texture->pTexture());
	pEffect->SetBool("g_texturedTrees", TSpeedTreeType::s_texturedTrees);
	pEffect->SetBool("g_cullEnabled", false);
	pEffect->CommitChanges();

	Moo::rc().setFVF(BranchVertex::fvf());
}

/**
 *	Activates the vertex buffer to draw the fronds of this type of tree.
 */
void TSpeedTreeType::setFrondModel() const
{
	if (this->fronds.lod[0].vertices->isValid())
	{
		this->fronds.lod[0].vertices->activate();
	}
}

/**
 *	Sets the render states for drawing the leaves of this type of tree.
 */
void TSpeedTreeType::setLeafRenderStates()
{
	ComObjectWrap<ID3DXEffect> pEffect;
	if (TSpeedTreeType::s_curShadowCaster == NULL)
	{
		TSpeedTreeType::s_effect->hTechnique("leaves", 0);
		pEffect = TSpeedTreeType::s_effect->pEffect()->pEffect();

		// leaf material
		const float* leafMaterial = this->speedTree->GetLeafMaterial();
		float material[] =
		{
			leafMaterial[0], leafMaterial[1],
			leafMaterial[2], 1.0f,	// branch diffuse color
			leafMaterial[3], leafMaterial[4],
			leafMaterial[5], 1.0f,	// branch ambient color
		};
		pEffect->SetVectorArray("g_material", (D3DXVECTOR4*)material, 2);

		// light adjustment
		float adjustment = this->speedTree->GetLeafLightingAdjustment();
		pEffect->SetFloat("g_leafLightAdj", adjustment);

		// texture, draw leaf flag
		pEffect->SetTexture("g_diffuseMap", this->leaves.texture->pTexture());
		pEffect->SetBool("g_texturedTrees", TSpeedTreeType::s_texturedTrees);
	}
	else
	{
		TSpeedTreeType::s_shadowsFX->hTechnique("leaves", 0);
		pEffect = TSpeedTreeType::s_shadowsFX->pEffect()->pEffect();
		pEffect->SetMatrix("g_invView", (D3DXMATRIX*)&TSpeedTreeType::s_invView);
	}

	// leaf billboard tables
	int matricesCount = this->speedWind.GetNumWindMatrices();
	for (int i=0; i<matricesCount; ++i)
	{
		const float * windMatrix = this->speedWind.GetWindMatrix(i);
		D3DXHANDLE hWindMatrix = pEffect->GetParameterElement("g_windMatrices", i);
		pEffect->SetMatrix(hWindMatrix, (D3DXMATRIX*)windMatrix);
	}

	// upload leaf angle table
	const int leafAnglesCount = this->speedWind.GetNumLeafAngleMatrices();
	static wind_float_vector windRockAngles(leafAnglesCount);
	static wind_float_vector windRustleAngles(leafAnglesCount);

	bool success =
		this->speedWind.GetRockAngles(windRockAngles) &&
		this->speedWind.GetRustleAngles(windRustleAngles);
	if (success)
	{
		typedef std::vector<float> FloatVector;
		static FloatVector anglesTable(leafAnglesCount*4);
		for (int i=0; i<leafAnglesCount; ++i)
		{
			anglesTable[i*4+0] = DEG_TO_RAD(windRockAngles[i]);
			anglesTable[i*4+1] = DEG_TO_RAD(windRustleAngles[i]);
		}

		pEffect->SetVectorArray("g_leafAngles", (D3DXVECTOR4*)&anglesTable[0], leafAnglesCount);
	}

	Vector4 leafAngleScalars(this->leafRockScalar, this->leafRustleScalar, 0, 0);
	pEffect->SetVector("g_leafAngleScalars", (D3DXVECTOR4*)&leafAngleScalars);
	pEffect->SetFloat("g_LeafRockFar", TSpeedTreeType::s_leafRockFar);

	Moo::rc().setFVF(LeafVertex::fvf());
}

/**
 *	Sets the render states for drawing the billboards of this type of tree.
 */
void TSpeedTreeType::setBillboardRenderStates()
{
	// if billboard optimization is on and
	// this tree has it setup, skip this call
#if ENABLE_BB_OPTIMISER
	if (!TSpeedTreeType::s_optimiseBillboards ||
		this->bbTreeType == -1)
#endif
	{
		TSpeedTreeType::s_effect->hTechnique("billboards", 0);

		ComObjectWrap<ID3DXEffect> pEffect =
			TSpeedTreeType::s_effect->pEffect()->pEffect();

		const Matrix & invView = TSpeedTreeType::s_invView;
		float cameraDir[] = { invView._31, invView._32, invView._33, 1.0f };
		pEffect->SetVector("g_cameraDir", (D3DXVECTOR4*)cameraDir);

		pEffect->CommitChanges();
		Moo::rc().setFVF(BillboardVertex::fvf());
	}
}

/**
 *	Begin the effect render pass.
 */
void TSpeedTreeType::beginPass()
{
	if (TSpeedTreeType::s_curShadowCaster == NULL)
	{
		TSpeedTreeType::s_effect->begin();
		TSpeedTreeType::s_effect->beginPass(0);
	}
	else
	{
		TSpeedTreeType::s_shadowsFX->begin();
		TSpeedTreeType::s_shadowsFX->beginPass(0);
	}
}

/**
 *	End the effect render pass.
 */
void TSpeedTreeType::endPass()
{
	if (TSpeedTreeType::s_curShadowCaster == NULL)
	{
		TSpeedTreeType::s_effect->endPass();
		TSpeedTreeType::s_effect->end();
	}
	else
	{
		TSpeedTreeType::s_shadowsFX->endPass();
		TSpeedTreeType::s_shadowsFX->end();
	}
}

/**
 *	Stores the current wind information. It will
 *	later be used to animate the trees to the wind.
 *
 *	@param	envMinder	the current EnvironMander object.
 */
void TSpeedTreeType::saveWindInformation(EnviroMinder * envMinder)
{
	if (envMinder != NULL)
	{
		TSpeedTreeType::s_windVelX = envMinder->weather()->settings().windX;
		TSpeedTreeType::s_windVelY = envMinder->weather()->settings().windY;
	}
}

/**
 *	Stores the current sun light information. It will
 *	later be used to setup the lighting for drawing the trees.
 *
 *	@param	envMinder	the current EnvironMander object.
 */
void TSpeedTreeType::saveLightInformation(EnviroMinder * envMinder)
{
	// Save for future use
	Vector3   & direction = TSpeedTreeType::s_sunDirection;
	Moo::Colour & diffuse = TSpeedTreeType::s_sunDiffuse;
	Moo::Colour & ambient = TSpeedTreeType::s_sunAmbient;

	if (TSpeedTreeType::s_enviroMinderLight && envMinder != NULL)
	{
		const OutsideLighting lighting = envMinder->timeOfDay()->lighting();
		direction = -1.f * lighting.mainLightDir();
		diffuse   = Moo::Colour(
			lighting.sunColour.x, lighting.sunColour.y,
			lighting.sunColour.z, lighting.sunColour.w);

		ambient = Moo::Colour(
			lighting.ambientColour.x, lighting.ambientColour.y,
			lighting.ambientColour.z, lighting.ambientColour.w);
	}
	else if (Moo::rc().lightContainer().exists() &&
		Moo::rc().lightContainer()->nDirectionals() > 0)
	{

		direction = Moo::rc().lightContainer()->directional(0)->direction();
		diffuse = Moo::rc().lightContainer()->directional(0)->colour();
		ambient = Moo::rc().lightContainer()->ambientColour();
	}
	else
	{
		diffuse   = Moo::Colour(0.58f, 0.51f, 0.38f, 1.0f);
		ambient   = Moo::Colour(0.5f, 0.5f, 0.5f, 1.0f);
		direction = Vector3(0.0f, -1.0f, 0.0f);
	}
}

/**
 *	Uploads wind matrix constants to the rendering device.
 */
void TSpeedTreeType::uploadWindMatrixConstants()
{
	static DogWatch windWatch("Wind Matrix");
	ScopedDogWatch watcher(windWatch);

	ShadowCaster * caster = TSpeedTreeType::s_curShadowCaster;
	ComObjectWrap<ID3DXEffect> pEffect = (caster == NULL)
		? TSpeedTreeType::s_effect->pEffect()->pEffect()
		: TSpeedTreeType::s_shadowsFX->pEffect()->pEffect();

	D3DXHANDLE bwind = pEffect->GetParameterByName(NULL, "g_windMatrices");
	unsigned int numMatrices = this->speedWind.GetNumWindMatrices();
	for (unsigned int i=0; i<numMatrices; ++i)
	{
		const float * windMatrix = this->speedWind.GetWindMatrix(i);
		D3DXHANDLE elemHandle = pEffect->GetParameterElement(bwind, i);
		if (elemHandle)
		{
			pEffect->SetMatrix(elemHandle, (const D3DXMATRIX*)windMatrix);
		}
	}
	pEffect->CommitChanges();
}

/**
 *	Retrieves a TSpeedTreeType instance for the given filename and
 *	seed. If a matching tree has already been loaded, return that
 *	one. Otherwise, create and return a new TSpeedTreeType object.
 *
 *	@param	filename	name of source SPT file from where to generate tree.
 *	@param	seed		seed number to use when generation tree.
 *	@return				the requested tree type object.
 *	@note	will throw std::runtime_error if object can't be created.
 */
TSpeedTreeType::TreeTypePtr TSpeedTreeType::getTreeTypeObject(
	const char * filename, uint seed)
{
	START_TIMER(loadGlobal);

	TreeTypePtr result;

	// first, look at list of
	// existing renderer objects
	START_TIMER(searchExisting);
	std::string treeDefName = createTreeDefName(filename, seed);
	{
		SimpleMutexHolder mutexHolder(TSpeedTreeType::s_syncInitLock);
		TreeRendererMap & rendererMap = TSpeedTreeType::s_rendererMap;
		TreeRendererMap::const_iterator rendererIt = rendererMap.find(treeDefName);
		if (rendererIt != rendererMap.end())
		{
			result = rendererIt->second;
		}
		// it may still be in the delayed init
		// list. Look for it there, then.
		else
		{
			// list should be small. A linear search
			// here is actually not at all that bad.
			typedef TSpeedTreeType::InitVector::const_iterator citerator;
			citerator initIt  = TSpeedTreeType::s_syncInitList.begin();
			citerator initEnd = TSpeedTreeType::s_syncInitList.end();
			while (initIt != initEnd)
			{
				if ((*initIt)->seed == seed &&
					(*initIt)->filename == filename)
				{
					result = *initIt;
					break;
				}
				++initIt;
			}
		}
	}
	STOP_TIMER(searchExisting, "");

	// if still not found,
	// create a new one
	if (!result.exists())
	{
		START_TIMER(actualLoading);
		typedef std::auto_ptr< TSpeedTreeType > RendererAutoPtr;
		RendererAutoPtr renderer(new TSpeedTreeType);

		// not using speedtree's clone function here because
		// it prevents us from saving from memory by calling
		// Delete??Geometry after generating all geometry.

		bool success = false;
		CSpeedTreeRTPtr speedTree;
		try
		{
			DataSectionPtr rootSection = BWResource::instance().rootSection();
			BinaryPtr sptData = rootSection->readBinary(filename);
			if (sptData.exists())
			{
				speedTree.reset(new CSpeedTreeRT);
				speedTree->SetTextureFlip(true);
				success = speedTree->LoadTree(
					(uchar*)sptData->data(),
					sptData->len());
			}
		}
		catch (...)
		{}

		if (!success)
		{
			std::string errorMsg = filename;
			errorMsg += ": " + std::string(CSpeedTreeRT::GetCurrentError());
			throw std::runtime_error(errorMsg);
		}
		STOP_TIMER(actualLoading, "");

		START_TIMER(computeTree);
		renderer->seed     = seed;
		renderer->filename = filename;
		speedTree->SetBranchLightingMethod(CSpeedTreeRT::LIGHT_DYNAMIC);
		speedTree->SetLeafLightingMethod(CSpeedTreeRT::LIGHT_DYNAMIC);
		speedTree->SetFrondLightingMethod(CSpeedTreeRT::LIGHT_DYNAMIC);

		speedTree->SetBranchWindMethod(CSpeedTreeRT::WIND_GPU);
		speedTree->SetLeafWindMethod(CSpeedTreeRT::WIND_GPU);
		speedTree->SetFrondWindMethod(CSpeedTreeRT::WIND_GPU);

		speedTree->SetNumLeafRockingGroups(4);

		// generate tree geometry
		if (!speedTree->Compute(NULL, seed))
		{
			std::string errorMsg = filename;
			errorMsg += ": Could not compute tree geometry [";
			errorMsg += std::string(CSpeedTreeRT::GetCurrentError()) + "]";
			throw std::runtime_error(errorMsg);
		}

		// get boundingbox
		float bounds[6];
		speedTree->GetBoundingBox(bounds);
		renderer->boundingBox.setBounds( Vector3(	bounds[0],
													bounds[2],
													bounds[1]),
										 Vector3(	bounds[3],
													bounds[5],
													bounds[4] ) );

		// first look for a speedwind.ini file
		// in the same directory as the tree.
		std::string datapath = BWResource::getFilePath(filename);

		// wind
		setupSpeedWind(
			renderer->speedWind, *speedTree, datapath,
			TSpeedTreeType::s_speedWindFile);
		STOP_TIMER(computeTree, "");

		// vertex buffers
		START_TIMER(bspTree);
		renderer->bspTree = createBSPTree(
			speedTree.get(),
			renderer->filename.c_str(),
			renderer->boundingBox);
		renderer->bspTree->createVertexList(
			renderer->bspVertices,
			renderer->bspVerticesCount,
			Moo::Colour(0.4f, 0.4f, 0.4f, 1.0f));
		STOP_TIMER(bspTree, "");

		// finally, bind data
		START_TIMER(assyncInit);
		renderer->speedTree = speedTree;
		renderer->assyncInit();
		result = renderer.release();
		STOP_TIMER(assyncInit, "");

		// Becuase it involves a lot of D3D work, the last bit
		// of initialization must be done in the main thread.
		// Put render in deferred initialization list so that
		// all pending initizalition tasks will be carried
		// next beginFrame call.
		START_TIMER(syncInitList);
		{
			SimpleMutexHolder mutexHolder(TSpeedTreeType::s_syncInitLock);
			TSpeedTreeType::s_syncInitList.push_back(result.getObject());
		}
		STOP_TIMER(syncInitList, "");
	}

	STOP_TIMER(loadGlobal, filename);

	return result;
}

/**
 *	Performs assynchronous initialization. Any initialization
 *	that can be done in the loading thread should be done here.
 */
void TSpeedTreeType::assyncInit()
{
	std::string datapath = BWResource::getFilePath(this->filename);

	// these may throw. For exeception safety, wait until
	// all are completed successfully before assigning them
	START_TIMER(localBranches);
	BranchRenderData localBranches =
		createPartRenderData< BranchTraits >(*this->speedTree, datapath.c_str());
	STOP_TIMER(localBranches, "");

	START_TIMER(localFronds);
	BranchRenderData localFronds =
		createPartRenderData< FrondTraits >(*this->speedTree, datapath.c_str());
	STOP_TIMER(localFronds, "");

	START_TIMER(localLeaves);
	const int windAnglesCount = this->speedWind.GetNumLeafAngleMatrices();
	LeafRenderData localLeaves = createLeafRenderData(
		*this->speedTree, this->filename, datapath,
		windAnglesCount, this->leafRockScalar, this->leafRustleScalar);
	STOP_TIMER(localLeaves, "");

	// billboards have to be done in the rendering
	// thread. It sets the camera position to build the
	// billboards and that affects rendering of leaves
	START_TIMER(localBillboards);
	BillboardRenderData localBillboards = createBillboardRenderData(
		*this->speedTree, datapath.c_str());

	this->branches   = localBranches;
	this->fronds     = localFronds;
	this->leaves     = localLeaves;
	this->billboards = localBillboards;
	STOP_TIMER(localBillboards, "");

	START_TIMER(deleteGeometry);
	const float * material = this->speedTree->GetLeafMaterial();

	// release geometry data
	// held internally by speedtree
	this->speedTree->DeleteBranchGeometry();
	this->speedTree->DeleteFrondGeometry();
	this->speedTree->DeleteLeafGeometry();
	this->speedTree->DeleteTransientData();
	STOP_TIMER(deleteGeometry, "");
}

/**
 *	Performs synchronous initialization. Any initialization that
 *	has got to be carried in the rendering thread can be done here.
 */
void TSpeedTreeType::syncInit()
{
	START_TIMER(addTreeType);
#if ENABLE_BB_OPTIMISER
		if (TSpeedTreeType::s_optimiseBillboards)
		{
			this->bbTreeType = BillboardOptimiser::addTreeType(
				this->leaves.texture,
				*this->billboards.lod.back().vertices,
				this->filename.c_str());
		}
#endif
	STOP_TIMER(addTreeType, "");
}

/**
 *	Updates the render groups vector. Trees generated from the same SPT file,
 *	but using different seed numbers have their own TSpeedTreeType object each.
 *	Nonetheless, they still share all rendering states. Render group is a list
 *	of all existing TSpeedTreeType objects sorted by their source SPT filenames.
 *	This list is used when drawing trees to avoid unecessary state changes.
 */
void TSpeedTreeType::updateRenderGroups()
{
	static DogWatch groupsWatch("Groups");

	RendererGroupVector & renderGroups = TSpeedTreeType::s_renderGroups;
	renderGroups.clear();

	std::string lastFilename;
	TSpeedTreeType::s_speciesCount = 0;
	FOREACH_TREETYPE_BEGIN(groupsWatch)
	{
		if (RENDER->filename != lastFilename)
		{
			lastFilename = RENDER->filename;
			renderGroups.push_back(RendererVector());
			++TSpeedTreeType::s_speciesCount;
		}
		renderGroups.back().push_back(RENDER);
	}
	FOREACH_TREETYPE_END
}

/**
 *	Increments the reference counter.
 */
void TSpeedTreeType::incRef() const
{
	++this->refCounter;
}

/**
 *	Increments the reference count. DOES NOT delete the object if
 *	the reference counter reaches zero. The recycleTreeTypeObjects
 *	method will take care of that (for multithreading sanity).
 */
void TSpeedTreeType::decRef() const
{
	--this->refCounter;
	MF_ASSERT(this->refCounter>=0);
}

/**
 *	Returns the current reference counter value.
 */
int  TSpeedTreeType::refCount() const
{
	return this->refCounter;
}

TSpeedTreeType::InitVector TSpeedTreeType::s_syncInitList;
SimpleMutex TSpeedTreeType::s_syncInitLock;

TSpeedTreeType::TreeRendererMap TSpeedTreeType::s_rendererMap;

bool  TSpeedTreeType::s_frameStarted = false;
float TSpeedTreeType::s_time         = 0.f;

TSpeedTreeType::EffectPtr TSpeedTreeType::s_effect;
TSpeedTreeType::EffectPtr TSpeedTreeType::s_shadowsFX;
TSpeedTreeType::RendererGroupVector TSpeedTreeType::s_renderGroups;

float TSpeedTreeType::s_windVelX = 0;
float TSpeedTreeType::s_windVelY = 0;

ShadowCaster * TSpeedTreeType::s_curShadowCaster = NULL;

Matrix      TSpeedTreeType::s_invView;
Matrix      TSpeedTreeType::s_view;
Matrix      TSpeedTreeType::s_projection;
Moo::Colour TSpeedTreeType::s_sunAmbient;
Moo::Colour TSpeedTreeType::s_sunDiffuse;
Vector3     TSpeedTreeType::s_sunDirection;

bool  TSpeedTreeType::s_enviroMinderLight  = true;
bool  TSpeedTreeType::s_drawBoundingBox    = false;
bool  TSpeedTreeType::s_drawBSPTree        = false;
bool  TSpeedTreeType::s_batchRendering     = true;
bool  TSpeedTreeType::s_drawTrees          = true;
bool  TSpeedTreeType::s_drawLeaves         = true;
bool  TSpeedTreeType::s_drawBranches       = true;
bool  TSpeedTreeType::s_drawFronds         = true;
bool  TSpeedTreeType::s_drawBillboards     = true;
bool  TSpeedTreeType::s_texturedTrees      = true;
bool  TSpeedTreeType::s_playAnimation      = true;
float TSpeedTreeType::s_leafRockFar        = 300.f;
float TSpeedTreeType::s_lodMode            = -2;
float TSpeedTreeType::s_lodNear            = 50.f;
float TSpeedTreeType::s_lodFar             = 450.f;

#if ENABLE_BB_OPTIMISER
	bool  TSpeedTreeType::s_optimiseBillboards = true;
#endif

int TSpeedTreeType::s_speciesCount = 0;
int TSpeedTreeType::s_uniqueCount  = 0;
int TSpeedTreeType::s_visibleCount = 0;
int TSpeedTreeType::s_deferredCount = 0;
int TSpeedTreeType::s_totalCount   = 0;

std::string TSpeedTreeType::s_fxFile        = "";
std::string TSpeedTreeType::s_shadowsFXFile = "";
std::string TSpeedTreeType::s_speedWindFile =
	"speedtree/SpeedWind.ini";

DogWatch TSpeedTreeType::s_globalWatch("SpeedTree");
DogWatch TSpeedTreeType::s_prepareWatch("Peparation");
DogWatch TSpeedTreeType::s_drawWatch("Draw");
DogWatch TSpeedTreeType::s_primitivesWatch("Primitives");

} // namespace speedtree

#ifdef EDITOR_ENABLED
	// forces linkage of
	// thumbnail provider
	extern int SpeedTreeThumbProv_token;
	int total = SpeedTreeThumbProv_token;
#endif // EDITOR_ENABLED

#endif // SPEEDTREE_SUPPORT ----------------------------------------------------
