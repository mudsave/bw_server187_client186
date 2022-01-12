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
#include "chunk_tree.hpp"
#include "chunk_obstacle.hpp"
#include "physics2/bsp.hpp"

#include "moo/render_context.hpp"
#include "speedtree/speedtree_renderer.hpp"

#include "romp/water_scene_renderer.hpp"

#include "umbra_config.hpp"

#if UMBRA_ENABLE
#include <umbraModel.hpp>
#include <umbraObject.hpp>
#endif

DECLARE_DEBUG_COMPONENT2( "Chunk", 0 )

// -----------------------------------------------------------------------------
// Section: ChunkTree
// -----------------------------------------------------------------------------

int ChunkTree_token;

/**
 *	Constructor.
 */
ChunkTree::ChunkTree() :
	BaseChunkTree(),
	tree_(NULL),
	reflectionVisible_(false),
	errorInfo_(NULL)
{}


/**
 *	Destructor.
 */
ChunkTree::~ChunkTree()
{}


/**
 *	Load method
 */
bool ChunkTree::load(DataSectionPtr pSection, Chunk * pChunk, std::string* errorString )
{
	// we want transform and reflection to be set even
	// if the tree fails to load. They may be used to
	// render a place holder model in case load fails

	// rotation inverse will be 
	// updated automatically
	this->setTransform( pSection->readMatrix34( "transform", Matrix::identity ) );
	this->reflectionVisible_ = pSection->readBool(
		"reflectionVisible", reflectionVisible_);

	uint seed = pSection->readInt("seed", 1);
	std::string filename = pSection->readString("spt");

	bool result = this->loadTree(filename.c_str(), seed, pChunk);
	if ( !result && errorString )
	{
		*errorString = "Failed to load tree " + filename;
	}
	return result;
}


bool ChunkTree::loadTree(const char * filename, int seed, Chunk * chunk)
{
	try
	{
		// load the speedtree
		using speedtree::SpeedTreeRenderer;
		std::auto_ptr< SpeedTreeRenderer > speedTree(new SpeedTreeRenderer);
		speedTree->load(filename, seed, chunk);		
		this->tree_ = speedTree;

		// update collision data
		this->BaseChunkTree::setBoundingBox(this->tree_->boundingBox());
		this->BaseChunkTree::setBSPTree(&this->tree_->bsp());
		this->errorInfo_.reset(NULL);
	}
	catch (const std::runtime_error &err)
	{
		this->errorInfo_.reset(new ErrorInfo);
		this->errorInfo_->what     = err.what();
		this->errorInfo_->filename = filename;
		this->errorInfo_->seed     = seed;

		ERROR_MSG( "Error loading tree: %s\n", err.what() );
	}
	return this->errorInfo_.get() == NULL;
}


bool ChunkTree::addYBounds(BoundingBox& bb) const
{	
	BoundingBox treeBB = this->boundingBox();
	treeBB.transformBy(this->transform());
	bb.addYBounds(treeBB.minBounds().y);
	bb.addYBounds(treeBB.maxBounds().y);

	return true;
}


void ChunkTree::syncInit()
{
#if UMBRA_ENABLE
	pUmbraModel_ = UmbraModelProxy::getObbModel( boundingBox().minBounds(), boundingBox().maxBounds() );
	pUmbraObject_ = UmbraObjectProxy::get( pUmbraModel_ );

	pUmbraObject_->object()->setUserPointer( (void*)this );

	Matrix m = this->pChunk_->transform();
	m.preMultiply( transform() );
	pUmbraObject_->object()->setObjectToCellMatrix( (Umbra::Matrix4x4&)m );
	pUmbraObject_->object()->setCell( this->pChunk_->getUmbraCell() );
#endif
}

//bool use_water_culling = true;

/**
 *	Draw (and update) this tree
 */
void ChunkTree::draw()
{
	if (this->tree_.get() != NULL && 
		(!Moo::rc().reflectionScene() || reflectionVisible_))
	{	
		if (Moo::rc().reflectionScene())
		{
			float height = WaterSceneRenderer::currentScene()->waterHeight();

			Matrix world( pChunk_->transform() );	
			BoundingBox bounds( this->boundingBox() );

			world.preMultiply(this->transform());

			//TODO: check to see if this transform is needed at all to get the height range info..
			bounds.transformBy( world );

			bool onPlane = bounds.minBounds().y == height || bounds.maxBounds().y == height;

			bool minAbovePlane = bounds.minBounds().y > height;			
			bool maxAbovePlane = bounds.maxBounds().y > height;

			bool abovePlane = minAbovePlane && maxAbovePlane;
			bool belowPlane = !minAbovePlane && !maxAbovePlane;
			if (!onPlane)
			{
				if (Moo::rc().mirroredTransform() && belowPlane) //reflection
					return;

				if (!Moo::rc().mirroredTransform() && abovePlane) //refraction
					return;
			}
		}

		Matrix world = Moo::rc().world();
		world.preMultiply(this->transform());
		this->tree_->draw(world);
	}
}


/**
 *	overridden lend method
 */
void ChunkTree::lend(Chunk * pLender)
{
	if (pChunk_ != NULL)
	{
		Matrix world(pChunk_->transform());
		world.preMultiply(this->transform());

		BoundingBox bb = this->boundingBox();
		bb.transformBy(world);

		this->lendByBoundingBox(pLender, bb);
	}
}


void ChunkTree::setTransform(const Matrix & transform)
{
	this->BaseChunkTree::setTransform(transform);
	if (this->tree_.get() != NULL)
	{
		this->tree_->resetTransform(this->pChunk_);
	}
}


uint ChunkTree::seed() const
{
	MF_ASSERT(this->tree_.get() != NULL || this->errorInfo_.get() != NULL);
	return this->tree_.get() != NULL
		? this->tree_->seed()
		: this->errorInfo_->seed;
}


bool ChunkTree::seed(uint seed)
{
	bool result = false;
	if (this->tree_.get() != NULL)
	{
		result = this->loadTree(
			this->tree_->filename(), seed,
			this->ChunkItemBase::chunk());
	}
	return result;
}


const char * ChunkTree::filename() const
{
	MF_ASSERT(this->tree_.get() != NULL || this->errorInfo_.get() != NULL);
	return this->tree_.get() != NULL
		? this->tree_->filename()
		: this->errorInfo_->filename.c_str();
}


bool ChunkTree::filename(const char * filename)
{
	MF_ASSERT(this->tree_.get() != NULL || this->errorInfo_.get() != NULL);
	int seed = this->tree_.get() != NULL 
		? this->tree_->seed() 
		: this->errorInfo_->seed;

	return this->loadTree(filename, seed, this->ChunkItemBase::chunk());
}


bool ChunkTree::getReflectionVis() const
{
	return this->reflectionVisible_;
}


bool ChunkTree::setReflectionVis(const bool & reflVis) 
{ 
	this->reflectionVisible_= reflVis;
	return true;
}


bool ChunkTree::loadFailed() const
{
	return this->errorInfo_.get() != NULL;
}


const char * ChunkTree::lastError() const
{
	return this->errorInfo_.get() != NULL
		? this->errorInfo_->what.c_str()
		: "";
}

/// Static factory initialiser
#undef IMPLEMENT_CHUNK_ITEM_ARGS
#define IMPLEMENT_CHUNK_ITEM_ARGS (pSection, pChunk, &errorString)
IMPLEMENT_CHUNK_ITEM(ChunkTree, speedtree, 0)
