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

#include "water.hpp"

#include "cstdmf/debug.hpp"
#include "cstdmf/dogwatch.hpp"
#include "cstdmf/memory_counter.hpp"

#include "moo/dynamic_vertex_buffer.hpp"
#include "moo/dynamic_index_buffer.hpp"
#include "moo/vertex_formats.hpp"
#include "moo/effect_material.hpp"
#include "moo/effect_visual_context.hpp"
#include "moo/terrain_renderer.hpp"
#include "moo/graphics_settings.hpp"
#if UMBRA_ENABLE
#include "moo/terrain_block.hpp"
#endif //UMBRA_ENABLE

#include "enviro_minder.hpp"
#ifdef EDITOR_ENABLED
#include "appmgr/commentary.hpp"
#include "fog_controller.hpp"
#endif

#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk_obstacle.hpp"
#include "chunk/chunk.hpp"
#if UMBRA_ENABLE
#include "chunk/chunk_terrain.hpp"
#endif //UMBRA_ENABLE

#include "speedtree/speedtree_renderer.hpp"

#include "cstdmf/watcher.hpp"
#include "cstdmf/bgtask_manager.hpp"
#include "cstdmf/concurrency.hpp"

#include "resmgr/auto_config.hpp"
#include "resmgr/bin_section.hpp"
#include "resmgr/data_section_census.hpp"

#ifndef CODE_INLINE
#include "water.ipp"
#endif

memoryCounterDefine( water, Scene );

DECLARE_DEBUG_COMPONENT2( "Romp", 0 )

// ----------------------------------------------------------------------------
// Statics
// ----------------------------------------------------------------------------

uint32 Waters::s_nextMark_ = 1;
uint32 Waters::s_lastDrawMark_ = -1;
static AutoConfigString s_waterEffect( "environment/waterEffect" );
static AutoConfigString s_simulationEffect( "environment/waterSimEffect" );
static AutoConfigString s_screenFadeMap( "environment/waterScreenFadeMap" );
static AutoConfigString s_foamMap( "environment/waterFoamMap" );

// Water statics
Water::VertexBufferPtr			Water::s_quadVertexBuffer_;
Water::WaterRenderTargetMap		Water::s_renderTargetMap_;

// Water statics
#ifndef EDITOR_ENABLED
bool                            Water::s_backgroundLoad_	= true;
#else
bool                            Water::s_backgroundLoad_	= false;
#endif

// Waters statics
Moo::EffectMaterialPtr			Waters::s_effect_			= NULL;
Moo::EffectMaterialPtr			Waters::s_simulationEffect_ = NULL;
bool							Waters::s_drawWaters_		= true;
bool							Waters::s_drawRefraction_	= true;
bool							Waters::s_drawReflection_	= true;
bool							Waters::s_simulationEnabled_ = true;
int								Waters::s_simulationLevel_	= 2;

#ifdef EDITOR_ENABLED
bool							Waters::s_projectView_		= false;
#endif

// ----------------------------------------------------------------------------
// Defines
// ----------------------------------------------------------------------------
#define MAX_SIM_TEXTURE_SIZE 256
#define MAX_SIM_MOVEMENTS 10
// defines the distance from the edge of a cell that a movement will activate
// its neighbour
#define ACTIVITY_THRESHOLD 0.3f

// ----------------------------------------------------------------------------
// Section: WaterCell
// ----------------------------------------------------------------------------

/**
 *	WaterCell contructor
 */
WaterCell::WaterCell() : 
		indexBuffer_( NULL ),
		nIndices_( 0 ),
		water_( 0 ),
		xMax_( 0 ),
		zMax_( 0 ),
		xMin_( 0 ),
		zMin_( 0 ),
		min_(0,0),
		max_(0,0),		
		size_(0,0)
{
	for (int i=0; i<4; i++)
	{
		edgeCells_[i]=NULL;
	}
}

char* neighbours[4] = {	"neighbourHeightMap0",
						"neighbourHeightMap1",
						"neighbourHeightMap2",
						"neighbourHeightMap3" };

/**
 *	Binds the specified neighbour cell.
 */
void WaterCell::bindNeighbour( Moo::EffectMaterialPtr effect, int edge )
{
	MF_ASSERT(edge<4 && edge>=0);
	if (edgeCells_[edge])
		edgeCells_[edge]->bindAsNeighbour( effect, neighbours[edge] );
	else
		effect->pEffect()->pEffect()->SetTexture(neighbours[edge], NULL);
}


/**
 *	Checks for movement activity within a cell and activates/deactivates 
 *	accordingly
 */
void WaterCell::checkActivity(	SimulationCellPtrList& activeList,
								WaterCellPtrList& edgeList )
{
	if (hasMovements())
		perturbed(true);
	else if ( !edgeActivation_ ) //edge-activated cells shouldnt be disabled here
		perturbed(false);

	if (shouldActivate())
	{
		activate();
		activeList.push_back( this );
	}
	else if (shouldDeactivate())
	{
		deactivate();
		activeList.remove( this );
		edgeActivation_=false;
	}	

	//inter-cell simulation....
	if (Waters::instance().simulationLevel() > 1) // only do for high detail settings
	{
		if (isActive())
		{
			if (edgeActivation_ == false)
				edge(0);

			int edges = getActiveEdge(ACTIVITY_THRESHOLD);

			if (movements().size())
			{
				this->edge(edges);
				if (edges > 0)
				{
					for (int i=0; i<4; i++)
					{
						if (edgeCells_[i] && (edges & (1<<i)))
						{
							edgeCells_[i]->addEdge((i+2) % 4);
							edgeCells_[i]->resetIdleTimer();
							edgeCells_[i]->edgeActivation_ = true;
							edgeCells_[i]->perturbed( true );

							edgeList.push_back( edgeCells_[i] );

							edgeActivation_ = true;
						}
					}
				}
			}

			if (edgeActivation_)
			{
				edgeList.push_back( this );

				bool deactivate=!hasMovements();
				for (int i=0; i<4 && deactivate; i++)
				{
					if ( edgeCells_[i] && 
							edgeCells_[i]->isActive() )
					{
						if ( edgeCells_[i]->hasMovements() )
							deactivate = false;
					}
				}
				if (deactivate)
					perturbed( false );
			}
		}
		else if (edgeActivation_)
		{
			perturbed( false );
			edgeList.push_back( this );
		}
	}
}


/**
 *	Activity check when activated by a neighbour
 */
void WaterCell::checkEdgeActivity( SimulationCellPtrList& activeList )
{
	if (edgeActivation_)
	{
		if (hasMovements())
			perturbed(true);			
		if (shouldActivate())
		{
			activate();
			activeList.push_back( this );
		}
		else if (shouldDeactivate())
		{
			edgeActivation_ = false;
			deactivate();
			activeList.remove( this );
		}
	}
}


/**
 *	Initialise a water cell
 */
bool WaterCell::init( Water* water, Vector2 start, Vector2 size )
{ 
	if (!water)
		return false;

	water_ = water;

	min_ = start;
	max_ = (start+size);
	size_ = size;

	xMin_ = int( ceilf( min().x / water_->config_.tessellation_)  );
	zMin_ = int( ceilf( min().y / water_->config_.tessellation_)  );

	xMax_ = int( ceilf( (max().x) / water_->config_.tessellation_)  ) + 1;
	zMax_ = int( ceilf( (max().y) / water_->config_.tessellation_)  ) + 1;

	if (xMax_ > int( water_->gridSizeX_ ))
		xMax_ = int( water_->gridSizeX_ );

	if (zMax_ > int( water_->gridSizeZ_ ))
		zMax_ = int( water_->gridSizeZ_ );

	return true;
}


/**
 *	Release all managed data
 */
void WaterCell::deleteManaged()
{
	indexBuffer_ = NULL;
}

/**
 *	Take the supplied vertex index and map it to a spot in a new
 *	vertex buffer.
 */
uint32 Water::remapVertex( uint32 index )
{
	uint32 newIndex = index;
	if (remappedVerts_.size())
	{
		std::map<uint32, uint32>& currentMap = remappedVerts_.back();
		std::map<uint32,uint32>::iterator it = currentMap.find(newIndex);
		// check if it's already mapped
		if (it == currentMap.end())
		{
			// not found, remap
			newIndex = (uint32)currentMap.size();
			currentMap[index] = newIndex;
		}
		else
		{
			newIndex = it->second;
		}
	}
	return newIndex;
}

/**
 *	Remaps a list of vertex indices to ensure they are contained within a
 *	single vertex buffer.
 */
template< class T >
uint32 Water::remap( std::vector<T>& dstIndices,
					const std::vector<uint32>& srcIndices )
{
	uint32 maxVerts = Moo::rc().maxVertexIndex();
	// Make a new buffer
	if (remappedVerts_.size() == 0)
	{
		remappedVerts_.resize(1);
	}
	// Allocate the destination index buffer.
	dstIndices.resize(srcIndices.size());

	// Transfer all the indices, remapping as necesary.
	for (uint i=0; i<srcIndices.size() ; i++)
	{
		dstIndices[i] = (T)this->remapVertex( srcIndices[i] );
	}

	// check if the current buffer has overflowed
	if ( remappedVerts_.back().size() > maxVerts)
	{
		// overflow, create new buffer + remap again
		remappedVerts_.push_back(std::map<uint32, uint32>());	
		for (uint i=0; i<srcIndices.size() ; i++)
		{
			dstIndices[i] = (T)this->remapVertex( srcIndices[i] );
		}

		// If it's full again then it's an error (too many verts)
		if ( remappedVerts_.back().size() > maxVerts )
		{
			ERROR_MSG("Water::remap( ): Maximum vertex count "
				"exceeded, please INCREASE the \"Mesh Size\" parameter.\n" );
			remappedVerts_.pop_back();
			return 0;
		}
	}
	return remappedVerts_.size();
}

/**
 *	Build the indices to match the templated value
 *  NOTE: only valid for uint32 and uint16
 */
template< class T >
void WaterCell::buildIndices( )
{
	#define WIDX_CHECKMAX  maxIndex = indices_32.back() > maxIndex ? \
									indices_32.back() : maxIndex;

	std::vector< T >		indices;
	std::vector< uint32 >	indices_32;

	int lastIndex = -1;
	bool instrip = false;
	uint32 gridIndex = 0;
	uint32 maxIndex=0;
	uint32 maxVerts = Moo::rc().maxVertexIndex();
	D3DFORMAT format;
	uint32 size = sizeof(T);

	if (size == 2)
		format = D3DFMT_INDEX16;
	else
		format = D3DFMT_INDEX32;

	// Build the master index list first.
	// then if any of the verts exceed the max... remap them all into a 
	// new vertex buffer.
	gridIndex = xMin_ + (water_->gridSizeX_*zMin_);
	for (uint z = uint(zMin_); z < uint(zMax_) - 1; z++)
	{
		for (uint x = uint(xMin_); x < uint(xMax_); x++)
		{
			if (!instrip &&
				water_->wIndex_[ gridIndex ] >= 0 &&
				water_->wIndex_[ gridIndex + 1 ] >= 0 &&
				water_->wIndex_[ gridIndex + water_->gridSizeX_ ] >= 0 &&
				water_->wIndex_[ gridIndex + water_->gridSizeX_ + 1 ] >= 0
				)
			{
				if (lastIndex == -1) //first
					lastIndex = water_->wIndex_[ gridIndex ];

				indices_32.push_back( uint32( lastIndex ) );

				indices_32.push_back( uint32( water_->wIndex_[ gridIndex ] ));
				WIDX_CHECKMAX

				indices_32.push_back( uint32( water_->wIndex_[ gridIndex ] ));
				WIDX_CHECKMAX

				indices_32.push_back( uint32( water_->wIndex_[ gridIndex + water_->gridSizeX_] ));
				WIDX_CHECKMAX

				instrip = true;
			}
			else
			{
				if (water_->wIndex_[ gridIndex ] >= 0 &&
					water_->wIndex_[ gridIndex + water_->gridSizeX_] >= 0  &&
					instrip)
				{
					indices_32.push_back( uint32(water_->wIndex_[ gridIndex ] ) );
					WIDX_CHECKMAX

					indices_32.push_back( uint32(water_->wIndex_[ gridIndex + water_->gridSizeX_]));
					WIDX_CHECKMAX

					lastIndex = indices_32.back();
				}
				else
					instrip = false;
			}

			if (x == xMax_ - 1)
				instrip = false;

			++gridIndex;
		}
		gridIndex += (water_->gridSizeX_ - xMax_) + xMin_;
	}

	// Process the indices
	bool remap = (maxIndex > maxVerts);
	if (remap)
	{
		vBufferIndex_ = water_->remap<T>( indices, indices_32 );
		if (vBufferIndex_ == 0) //error has occured.
		{
			nIndices_ = 0;		
			return;
		}
	}
	else
	{
		vBufferIndex_ = 0;
		indices.resize( indices_32.size() );
		for (uint i=0; i<indices_32.size() ; i++)
		{
			indices[i] = (T)indices_32[i];		
		}
	}

	// Build the D3D index buffer.
	if (indices.size() != 0)
	{
		indices.erase( indices.begin(), indices.begin() + 2 );
		nIndices_ = indices.size();

		// Create the index buffer
		// The index buffer consists of one big strip covering the whole water
		// cell surface, Does not include sections of water that is under the
		// terrain. made up of individual rows of strips connected by degenerate
		// triangles.
		DX::IndexBuffer* pBuffer = NULL;
		if( SUCCEEDED( Moo::rc().device()->CreateIndexBuffer( nIndices_ * size,
			D3DUSAGE_WRITEONLY, format, D3DPOOL_MANAGED, &pBuffer, NULL ) ) )
		{
			DWORD* pIndices;
			indexBuffer_ = pBuffer;
			pBuffer->Release();
			if(SUCCEEDED( indexBuffer_->Lock(	0,
												nIndices_ * size,
												(void**)&pIndices, 
												0 )))
			{
				memcpy( pIndices, &indices.front(), size * nIndices_ );
				indexBuffer_->Unlock();
			}
		}
		memoryCounterAdd( water );
		memoryClaim( &*indexBuffer_ );
	}
#undef WIDX_CHECKMAX
}

/**
 *	Create the water cells managed index buffer
 */
void WaterCell::createManagedIndices()
{
	if (!water_)
		return;
	if (Moo::rc().maxVertexIndex() <= 0xffff)
		buildIndices<uint16>();
	else
		buildIndices<uint32>();
}


// ----------------------------------------------------------------------------
// Section: Water
// ----------------------------------------------------------------------------

namespace { //anonymous

static SimpleMutex * deletionLock_ = NULL;

};


/**
 * Constructor for water.
 *
 * @param config a structure containing the configuration data for the water surface.
 * @param pCollider the collider to use to intersect the water with the scene
 */
Water::Water( const WaterState& config, RompColliderPtr pCollider )
:	config_( config ),
	gridSizeX_( int( ceilf( config.size_.x / config.tessellation_ ) + 1 ) ),
	gridSizeZ_( int( ceilf( config.size_.y / config.tessellation_ ) + 1 ) ),
	time_( 0 ),
	lastDTime_( 1.f ),
	backgroundTask1_( NULL ),
	backgroundTask2_( NULL ),
	normalTexture_( NULL ),
	screenFadeTexture_( NULL ),
	foamTexture_( NULL ),
	pCollider_( pCollider ),
	waterScene_( NULL),
	drawSelf_( true ),
	simulationEffect_( NULL ),
	effect_( NULL ),
	reflectionCleared_( false ),
#ifdef EDITOR_ENABLED
	drawRed_( false ),
#endif
	createdCells_( false ),
	initialised_( false ),
	enableSim_( true ),
	drawMark_( 0 ),
	texAnim_( 0.0f )
	/*,edgeWaves_( new WaterEdgeWaves() )*/
{

	if (!deletionLock_)
		deletionLock_ = new SimpleMutex;

	Waters::instance().push_back(this);

	// Resize the water buffers.
	wRigid_.resize( gridSizeX_ * gridSizeZ_, false );
	wAlpha_.resize( gridSizeX_ * gridSizeZ_, 0 );

	// Create the water transforms.
	transform_.setRotateY( config_.orientation_ );
	transform_.postTranslateBy( config_.position_ );
	invTransform_.invert( transform_ );

    if (Water::backgroundLoad())
    {
	    // do heavy setup stuff in the background
	    this->backgroundTask1_ = new BackgroundTask( 
			    &Water::doCreateTables, this,
			    &Water::onCreateTablesComplete, this );
	    BgTaskManager::instance()->addTask( *backgroundTask1_ );
    }
    else
    {
	    Water::doCreateTables( this );
	    Water::onCreateTablesComplete( this );
    }
}


/**
 * Destructor.
 */
Water::~Water()
{
	memoryCounterSub( water );
	memoryClaim( wRigid_ );
	memoryClaim( wAlpha_ );
	memoryClaim( wIndex_ );
	memoryClaim( this );

	releaseWaterScene();

#if UMBRA_ENABLE
	terrainItems_.clear();
#endif //UMBRA_ENABLE

	effect_ = NULL;
	simulationEffect_ = NULL;

	//if (edgeWaves_)
	//{
	//	delete edgeWaves_;
	//	edgeWaves_ = NULL;
	//}
}



/**
 *	Check the water pointer is valid for usage.
 *	Used by the background tasks to avoid bad pointers.
 *
 *	@param	water	Pointer to check.
 *
 *	@return			Validity of the pointer.
 */
bool Water::stillValid(Water* water)
{
	if (Water::backgroundLoad())
	{
		if (deletionLock_)
		{
			deletionLock_->grab();

			std::vector< Water* >::iterator it =
				std::find(	Waters::instance().begin(),
							Waters::instance().end(), water );	

			if (it == Waters::instance().end()) // deleted
			{
				deletionLock_->give();
				return false;
			}
		}
		else
			return false;
	}
	return true;
}

/**
 *	Delete a water object. Controlled destruction is required by the 
 *	background tasks.
 *
 *	The water destructor is private so all destruction should come
 *	through here.
 *
 *	@param	water	Water object to destroy.
 *
 */
void Water::deleteWater(Water* water)
{
	if (deletionLock_)
	{
		while(!deletionLock_->grabTry()) {}	
		std::vector< Water* >::iterator it = std::find( Waters::instance().begin( ), Waters::instance().end( ), water );
		Waters::instance().erase(it);

		delete water;

		bool deleteMutex = (Waters::instance().size() == 0);
		if (deleteMutex)
		{
			delete deletionLock_;
			deletionLock_ = NULL;
		}
		else
			deletionLock_->give();
	}
	else
	{
		std::vector< Water* >::iterator it = std::find( Waters::instance().begin( ), Waters::instance().end( ), water );
		Waters::instance().erase(it);

		delete water;
	}
}


/**
 * Remove the references to the water scene
 */
void Water::releaseWaterScene()
{
	if (waterScene_)
	{
		waterScene_->removeOwner(this);	
		if ( s_renderTargetMap_.size() && waterScene_->refCount() == 1 )
		{
			float key = config_.position_.y;
			key = floorf(key+0.5f);
			WaterRenderTargetMap::iterator it = s_renderTargetMap_.find(key);
			WaterScene *(&a) = (it->second);
			a = NULL; //a ref
		}
		waterScene_->decRef();
		waterScene_ = NULL;
	}
}


/**
 * Recreate the water surface render data.
 */
void Water::rebuild( const WaterState& config )
{
	bool recreateScene = config.position_.y != config_.position_.y;

	initialised_ = false;
	vertexBuffers_.clear();

	deleteManagedObjects();

	if (recreateScene)
		releaseWaterScene();

	config_ = config;
	gridSizeX_ = int( ceilf( config.size_.x / config.tessellation_ ) + 1 );
	gridSizeZ_ = int( ceilf( config.size_.y / config.tessellation_ ) + 1 );

	// Create the water transforms.
	transform_.setRotateY( config_.orientation_ );
	transform_.postTranslateBy( config_.position_ );
	invTransform_.invert( transform_ );

	if( wRigid_.size() != gridSizeX_ * gridSizeZ_ )
	{// don't clear it
		wRigid_.clear();
		wAlpha_.clear();

		wRigid_.resize( gridSizeX_ * gridSizeZ_, false );
		wAlpha_.resize( gridSizeX_ * gridSizeZ_, 0 );
	}

	if (config_.useEdgeAlpha_)
		buildTransparencyTable();

	wIndex_.clear();
	cells_.clear();
	activeSimulations_.clear();
	nVertices_ = createIndices();
	
//	if (edgeWaves_)
//		edgeWaves_->rebuild();

#ifdef EDITOR_ENABLED
	enableSim_ = true; // Since this can be toggled by the tools dynamically
#else
	enableSim_ = Waters::simulationEnabled();
#endif

	enableSim_ = enableSim_ && Moo::RenderContext::instance().supportsTextureFormat( D3DFMT_A16B16G16R16F ) && config_.useSimulation_;

	createCells();

	setup2ndPhase();
}


/**
 * Builds the index table for a water surface.
 * Creates multiple vertex buffers when the vertices go over the 
 * max vertex index
 */
uint32 Water::createIndices( )
{
	// Create the vertex index table for the water cell.
	uint32 index = 0;
	int32 waterIndex = 0;
	for (int zIndex = 0; zIndex < int( gridSizeZ_ ); zIndex++ )
	{
		for (int xIndex = 0; xIndex < int( gridSizeX_ ); xIndex++ )
		{
			int realNeighbours = 0;
			for (int cz = zIndex - 1; cz <= zIndex + 1; cz++ )
			{
				for (int cx = xIndex - 1; cx <= xIndex + 1; cx++ )
				{
					if (( cx >= 0 && cx < int( gridSizeX_ ) ) &&
						( cz >= 0 && cz < int( gridSizeZ_ ) ))
					{
						if (!wRigid_[ cx + ( cz * gridSizeX_ ) ])
						{
							realNeighbours++;
						}
					}
				}
			}

			if (realNeighbours > 0)
				wIndex_.push_back(waterIndex++);
			else
				wIndex_.push_back(-1);
		}
	}
	memoryClaim( wIndex_ );

#ifdef EDITOR_ENABLED
	if (waterIndex > 0xffff)
	{
		Commentary::instance().addMsg( "WARNING! Water surface contains excess"
				" vertices, please INCREASE the \"Mesh Size\" parameter", 1 );
	}
#endif //EDITOR

	return waterIndex;
}

#define RLE_KEY_VALUE 53

#ifdef EDITOR_ENABLED

/**
 *	Delete the transparency/rigidity file.
 */
void Water::deleteData( )
{
	std::string fileName = config_.transparencyTable_;
	::DeleteFile( BWResource::resolveFilename( fileName ).c_str() );
}


/**
 *	Little helper function to write out some data to a string..
 */
template<class C> void writeVector( std::string& data, std::vector<C>& v )
{
	int clen = sizeof( C );
	int vlen = clen * v.size();
	data.append( (char*) &v.front(), vlen );
}


/**
 *	Writes a 32 bit uint to a string (in 4 chars)
 */
void writeValue(std::string& data, uint32 value )
{
	data.push_back( char( (value & (0xff << 0 ))    ) );
	data.push_back( char( (value & (0xff << 8 ))>>8 ) );
	data.push_back( char( (value & (0xff << 16))>>16) );
	data.push_back( char( (value & (0xff << 24))>>24) );
}


/**
 *	Writes a bool to a string
 */
void writeValue(std::string& data, bool value )
{
	data.push_back( char( value ) );
}


/**
 *	Compresses a vector using very simple RLE
 */
template <class C>
void compressVector( std::string& data, std::vector<C>& v )
{
	MF_ASSERT(v.size());
	uint32 imax = v.size();	
	uint32 prev_i = 0;
	bool first = true;

	C cur_v = v[0];
	C prev_v = !cur_v;
	for (uint32 i=0; i<imax; i++)
	{
		cur_v = v[i];
		uint32 cur_i = i;    
		uint32 c = cur_i - prev_i;

		// test for end of run cases
		if ( (cur_v != prev_v) || (c > 254) || (i==(imax-1)) )
		{
			if (prev_v == static_cast<C>(RLE_KEY_VALUE)) // exception for keyvalue
			{
				data.push_back(static_cast<char>(RLE_KEY_VALUE));
				writeValue(data, prev_v);
				data.push_back( static_cast<char>(c) );
			}
			else
			{
				if (c > 2)
				{
					data.push_back(static_cast<char>(RLE_KEY_VALUE));					
					writeValue(data, prev_v);
					data.push_back( static_cast<char>(c) );
				}
				else
				{
					if (first==false)
					{
						if ( c > 1 )
							writeValue(data, prev_v);
						writeValue(data, prev_v);
					}
				}
			}
			prev_i = cur_i;
		}
		prev_v = cur_v;
		first=false;
	}
	writeValue(data, cur_v);
}


/**
 *	Save rigidity and alpha tables to a file.
 */
void Water::saveTransparencyTable( )
{
	std::string data1;
	uint maxSize = data1.max_size();

	// Build the required data...
	compressVector<uint32>(data1, wAlpha_);
	std::string data2;
	compressVector<bool>(data2, wRigid_);

	BinaryPtr binaryBlockAlpha = new BinaryBlock( data1.data(), data1.size() );
	BinaryPtr binaryBlockRigid = new BinaryBlock( data2.data(), data2.size() );

	//Now copy it into the data file
	std::string dataName = config_.transparencyTable_;
	DataSectionPtr pSection = BWResource::openSection( dataName, false );
	if ( !pSection )
	{
		// create a section
		size_t lastSep = dataName.find_last_of('/');
		std::string parentName = dataName.substr(0, lastSep);
		DataSectionPtr parentSection = BWResource::openSection( parentName );
		MF_ASSERT(parentSection);

		std::string tagName = dataName.substr(lastSep + 1);

		// make it
		pSection = new BinSection( tagName, new BinaryBlock( NULL, 0 ) );
		pSection->setParent( parentSection );
		pSection = DataSectionCensus::add( dataName, pSection );
	}


	MF_ASSERT( pSection );
	pSection->delChild( "alpha" );
	DataSectionPtr alphaSection = pSection->openSection( "alpha", true );
	alphaSection->setParent(pSection);
	if ( !pSection->writeBinary( "alpha", binaryBlockAlpha ) )
	{
		CRITICAL_MSG( "Water::saveTransparencyTable - error while writing BinSection in %s/alpha\n", dataName.c_str() );
		return;
	}

	pSection->delChild( "rigid" );
	DataSectionPtr rigidSection = pSection->openSection( "rigid", true );
	rigidSection->setParent(pSection);
	if ( !pSection->writeBinary( "rigid", binaryBlockRigid ) )
	{
		CRITICAL_MSG( "Water::saveTransparencyTable - error while writing BinSection in %s/rigid\n", dataName.c_str() );
		return;
	}

	pSection->delChild( "version" );
	DataSectionPtr versionSection = pSection->openSection( "version", true );
	int version = 2;
	BinaryPtr binaryBlockVer = new BinaryBlock( &version, sizeof(int) );
	versionSection->setParent(pSection);
	if ( !pSection->writeBinary( "version", binaryBlockVer ) )
	{
		CRITICAL_MSG( "Water::saveTransparencyTable - error while writing BinSection in %s/version\n", dataName.c_str() );
		return;
	}
	versionSection->save();

	// Now actually save...
	alphaSection->save();
	rigidSection->save();
	pSection->save();
}

#endif //EDITOR_ENABLED


/**
 *	Read a uint32 from a 4 chars in a block
 *	- updates a referenced index.
 */
void readValue( const char* pCompressedData, uint32& index, uint32& value )
{
	char p1 = pCompressedData[index]; index++;	
	char p2 = pCompressedData[index]; index++;
	char p3 = pCompressedData[index]; index++;
	char p4 = pCompressedData[index]; index++;

	value = p1 & 0xff;
	value |= ((p2 << 8  ) & 0xff00);
	value |= ((p3 << 16 ) & 0xff0000);
	value |= ((p4 << 24 ) & 0xff000000);
}

#pragma warning( push )
#pragma warning( disable : 4800 ) //ignore the perf warning..

/**
 *	Read a bool from a char in a block
 *	- updates a referenced index.
 */
void readValue( const char* pCompressedData, uint32& index, bool& value )
{
	value = static_cast<bool>(pCompressedData[index]); index++;	
}

#pragma warning( pop )


/**
 *	Decompress a char block to vector using the RLE above..
 */
template< class C >
void decompressVector( const char* pCompressedData, uint32 length, std::vector<C>& v )
{
	uint32 i = 0;
	while (i < length)
	{
		// identify the RLE key
		if (pCompressedData[i] == char(RLE_KEY_VALUE))
		{
			i++;
			C val=0;
			readValue(pCompressedData, i, val);
			uint c = uint( pCompressedData[i] & 0xff ); 
			i++;

			// unfold the run..
			for (uint j=0; j < c; j++)
			{
				v.push_back(val);
			}
		}
		else
		{
			C val=0;
			readValue(pCompressedData, i, val);
			v.push_back( val );
		}
	}
}

/**
 *	Load the previously saved transparency/rigidity data.
 */
bool Water::loadTransparencyTable( )
{
	std::string sectionName = config_.transparencyTable_;
	DataSectionPtr pSection =
			BWResource::instance().rootSection()->openSection( sectionName );

	if (!pSection)
		return false;

	BinaryPtr pVersionData = pSection->readBinary( "version" );
	int fileVersion=-1;
	if (pVersionData)
	{
		const int* pVersion = reinterpret_cast<const int*>(pVersionData->data());
		fileVersion = pVersion[0];
	}

	BinaryPtr pAlphaData = pSection->readBinary( "alpha" );
	if (fileVersion < 2)
	{
		const uint32* pAlphaValues = reinterpret_cast<const uint32*>(pAlphaData->data());
		int nAlphaValues = pAlphaData->len() / sizeof(uint32);
		wAlpha_.assign( pAlphaValues, pAlphaValues + nAlphaValues );
	}
	else
	{
		wAlpha_.clear();
		const char* pCompressedValues = reinterpret_cast<const char*>(pAlphaData->data());
		decompressVector<uint32>(pCompressedValues, pAlphaData->len(), wAlpha_);
	}

	BinaryPtr pRigidData = pSection->readBinary( "rigid" );
	if (fileVersion < 2)
	{
		const bool* pRigidValues = reinterpret_cast<const bool*>(pRigidData->data());
		int nRigidValues = pRigidData->len() / sizeof(bool); //not needed.....remove..
		wRigid_.assign( pRigidValues, pRigidValues + nRigidValues );
	}
	else
	{
		wRigid_.clear();
		const char* pCompressedValues = reinterpret_cast<const char*>(pRigidData->data());
		decompressVector<bool>(pCompressedValues, pRigidData->len(), wRigid_);
	}

	return true;
}


/**
 *	Create rigidity and alpha tables.
 */
void Water::buildTransparencyTable( )
{
	float z = -config_.size_.y * 0.5f;
	uint32 index = 0;
	bool solidEdges =	(config_.size_.x <= (config_.tessellation_*2.f)) ||
						(config_.size_.y <= (config_.tessellation_*2.f));


	waterDepth_ = 10.f;

	for (uint32 zIndex = 0; zIndex < gridSizeZ_; zIndex++ )
	{
		if ((zIndex+1) == gridSizeZ_)
			z = config_.size_.y * 0.5f;

		//todo: revisit these calculations for large bodies of water..
		float x = -config_.size_.x * 0.5f;
		for (uint32 xIndex = 0; xIndex < gridSizeX_; xIndex++ )
		{
			if ((xIndex+1) == gridSizeX_)
				x = config_.size_.x * 0.5f;

			Vector3 v = transform_.applyPoint( Vector3( x, 0.1f, z ) );

			//TODO: determine the depth automatically
			//float height = pCollider_ ? pCollider_->ground( v ) : (v.y - 100);
			//if (height > waterDepth_

			if (xIndex == 0 ||
				zIndex == 0 ||
				xIndex == ( gridSizeX_ - 1 ) ||
				zIndex == ( gridSizeZ_ - 1 ))
			{
				// Set all edge cases to be completely transparent and rigid.				
				if (solidEdges)
				{
					wRigid_[ index ] = false;
					wAlpha_[ index ] = ( 255UL ) << 24;
				}
				else
				{
					wRigid_[ index ] = true;				
					wAlpha_[ index ] = 0x00000000;
				}
			}
			else
			{
				// Get the terrain height, and calculate the transparency of this water grid point
				// to be the height above the terrain.
				float height = pCollider_ ? pCollider_->ground( v ) : (v.y - 100);
				if ( height > v.y )
				{
					wRigid_[ index ] = true;
					wAlpha_[ index ] = 0x00000000;
				}
				else
				{
					wRigid_[ index ] = false;
					float h = 155.f + 100.f * min( 1.f, max( 0.f, (1.f - ( v.y - height )) ) );
					h = h/255.f;					
					h = Math::clamp<float>( 0.f, (h-0.5f)*2.f, 1.f );
					h = 1.f-h;
					h = powf(h,2.f)*2.f;
					h = min( h, 1.f);
					wAlpha_[ index ] = uint32( h*255.f ) << 24;
				}
			}

			++index;
			x += config_.tessellation_;
		}
		z += config_.tessellation_;
	}
}

/**
 *	Generate the required simulation cell divisions.
 */
void Water::createCells()
{
	if ( enableSim_ )
	{
		float simGridSize =  ( ceilf( config_.simCellSize_ / config_.tessellation_ ) * config_.tessellation_);

		for (float y=0.f;y<config_.size_.y; y+=simGridSize)
		{
			Vector2 actualSize(simGridSize,simGridSize);
			if ( (y+simGridSize) > config_.size_.y )
				actualSize.y = (config_.size_.y - y);

			//TODO: if the extra bit is really small... just enlarge the current cell??? hmmmm
			for (float x=0.f;x<config_.size_.x; x+=simGridSize)			
			{
				WaterCell newCell;
				if ( (x+simGridSize) > config_.size_.x )
					actualSize.x = (config_.size_.x - x);

				newCell.init( this, Vector2(x,y), actualSize );
				cells_.push_back( newCell );
			}
		}
	}
	else
	{
		// If the simulation is disabled, dont divide the surface.. just use one cell..
		Vector2 actualSize(config_.size_.x, config_.size_.y);
		WaterCell newCell;
		newCell.init( this, Vector2(0.f,0.f), actualSize );
		cells_.push_back( newCell );
	}
}


/**
 *	Create the data tables
 */
void Water::doCreateTables( void* self )
{
	Water * selfWater = static_cast< Water * >( self );
	if (Water::stillValid(selfWater))
	{
		// if in the editor: create the transparency table.. else load it...
#ifdef EDITOR_ENABLED

		// Build the transparency information
		if (!selfWater->loadTransparencyTable())
		{
			selfWater->buildTransparencyTable();
		}

		selfWater->enableSim_ = true; // Since this can be toggled by the tools dynamically

#else

		if (!selfWater->loadTransparencyTable())
		{
			selfWater->config_.useEdgeAlpha_ = false;
		}

		selfWater->enableSim_ = Waters::simulationEnabled();

#endif

		// Build the water cells
		selfWater->nVertices_ = selfWater->createIndices();
	//	if (selfWater->edgeWaves_)
	//		selfWater->edgeWaves_createIndices();

		selfWater->enableSim_ = selfWater->enableSim_ && Moo::RenderContext::instance().supportsTextureFormat( D3DFMT_A16B16G16R16F ) && selfWater->config_.useSimulation_;

		// Create the FX shaders in the background too..
		bool res=false;
		static bool first=true;

		if (Waters::s_simulationEffect_ == NULL)
			selfWater->enableSim_ = false;
		else
			selfWater->simulationEffect_ = Waters::s_simulationEffect_;

		if (Waters::s_effect_ == NULL)
			selfWater->drawSelf_ = false;
		else
			selfWater->effect_ = Waters::s_effect_;

		selfWater->createCells();

		if (Water::backgroundLoad())
			deletionLock_->give();
	}
}


/**
 *	This is called on the main thread, after the rigidity and alpha tables
 *	have been computed. Starts the second phase of the water setup.
 */
void Water::onCreateTablesComplete( void * self )
{
	// second step setup
	Water * selfWater = static_cast< Water * >( self );
	if (Water::stillValid(selfWater))
	{
		selfWater->setup2ndPhase();

		if (Water::backgroundLoad())
		{
			delete selfWater->backgroundTask1_;
			selfWater->backgroundTask1_ = NULL;
		}
		if (Water::backgroundLoad())
			deletionLock_->give();
	}
}


/**
 *	Second phase of the water setup.
 */
void Water::setup2ndPhase()
{
	static DogWatch w3( "Material" );
	w3.start();

	DEBUG_MSG( "Water::Water: using %d vertices out of %d\n",
		nVertices_, gridSizeX_ * gridSizeZ_ );

	float simGridSize =   ceilf( config_.simCellSize_ / config_.tessellation_ ) * config_.tessellation_;


	int cellX = int(ceilf(config_.size_.x / simGridSize));
	int cellY = int(ceilf(config_.size_.y / simGridSize)); //TODO: need to use the new cell size! :D
	uint cellCount = cells_.size();

	for (uint i=0; i<cellCount; i++)
	{
		cells_[i].initSimulation( MAX_SIM_TEXTURE_SIZE, config_.simCellSize_ );

		//TODO: fix this initialisation...
		if (enableSim_)
		{
			int col = (i/cellX)*cellX;
			int negX = i - 1;
			int posX = i + 1;
			int negY = i - cellX;
			int posY = i + cellX;

			cells_[i].initEdge( 1, (negX < col)					?	NULL	: &cells_[negX] );
			cells_[i].initEdge( 0, (negY < 0)					?	NULL	: &cells_[negY] );
			cells_[i].initEdge( 3, (posX >= (col+cellX))		?	NULL	: &cells_[posX] );
			cells_[i].initEdge( 2, (uint(posY) >= cellCount)	?	NULL	: &cells_[posY] );
		}
	}

	// Create the render targets
	// The WaterSceneRenderer are now grouped based on the height of the 
	//	water plane (maybe extend to using the whole plane?)
	float key = config_.position_.y;
	key = floorf(key+0.5f);
	if ( s_renderTargetMap_.find(key) == s_renderTargetMap_.end())
	{
		s_renderTargetMap_[key] = NULL;
	}

	WaterRenderTargetMap::iterator it = s_renderTargetMap_.find(key);

	WaterScene *(&a) = (it->second);
	if (a == NULL)
		a = new WaterScene( config_.position_[1]);

	waterScene_ = (it->second);
	waterScene_->incRef();
	waterScene_->addOwner(this);

    if (Water::backgroundLoad())
    {
	    // load resources in the background
	    this->backgroundTask2_ = new BackgroundTask( 
			    &Water::doLoadResources, this,
			    &Water::onLoadResourcesComplete, this );
	    BgTaskManager::instance()->addTask( *backgroundTask2_ );
    }
    else
    {
	    Water::doLoadResources( this );
	    Water::onLoadResourcesComplete( this );
    }

	//	// Create the bounding box.
	bb_.setBounds(	
		Vector3(	-config_.size_.x * 0.5f,
					-1.f,
					-config_.size_.y * 0.5f ),
		Vector3(	config_.size_.x * 0.5f,
					1.f,
					config_.size_.y * 0.5f ) );

	// And the volumetric bounding box ... the water is 10m deep
	//TODO: modify this to use the actual depth...... 

	//TODO: determine the depth of the water and use that for the BB
	//go through all the points and find the lowest terrain point.... use that for the BB depth

	bbDeep_ = bb_;
	bbDeep_.setBounds( bbDeep_.minBounds() - Vector3(0.f,10.f,0.f), bbDeep_.maxBounds() );

	waterDepth_ = 10.f;
	bbActual_.setBounds(	
		Vector3(	-config_.size_.x * 0.5f,
					-waterDepth_,
					-config_.size_.y * 0.5f ),
		Vector3(	config_.size_.x * 0.5f,
					0.f,
					config_.size_.y * 0.5f ) );



	w3.stop();
	static DogWatch w4( "Finalization" );
	w4.start();

	createUnmanagedObjects();

	// Create the managed objects.
	createManagedObjects();

	memoryCounterAdd( water );
	memoryClaim( this );
	memoryClaim( wRigid_ );
	memoryClaim( wAlpha_ );
	memoryClaim( wIndex_ );

	w4.stop();
}


/**
 *	Loads all resources needed by the water. To avoid stalling 
 *	the main thread, this should be done in a background task.
 */
void Water::doLoadResources( void * self )
{
	Water * selfWater = static_cast< Water * >( self );
	if (Water::stillValid(selfWater))
	{

		selfWater->normalTexture_ = Moo::TextureManager::instance()->get(selfWater->config_.waveTexture_);
		selfWater->screenFadeTexture_ = Moo::TextureManager::instance()->get(s_screenFadeMap);
		selfWater->foamTexture_ = Moo::TextureManager::instance()->get(s_foamMap);
//		if (selfWater->edgeWaves_)
//			selfWater->edgeWaves_->loadResources();
		SimulationManager::instance().loadResources();

		if (Water::backgroundLoad())
			deletionLock_->give();
	}
}


/**
 *	This is called on the main thread, after the resources 
 *	have been loaded. Sets up the texture stages.
 */
void Water::onLoadResourcesComplete( void * self )
{
	Water * selfWater = static_cast< Water * >( self );
	if (Water::stillValid(selfWater))
	{

		ComObjectWrap<ID3DXEffect> pEffect = selfWater->effect_->pEffect()->pEffect();

		if (selfWater)
		{
			if (selfWater->normalTexture_)
				pEffect->SetTexture("normalMap", selfWater->normalTexture_->pTexture());
			if (selfWater->screenFadeTexture_)
				pEffect->SetTexture("screenFadeMap", selfWater->screenFadeTexture_->pTexture());
			if (selfWater->foamTexture_)
				pEffect->SetTexture("foamTexture", selfWater->foamTexture_->pTexture());

		    if (Water::backgroundLoad())
		    {
			    delete selfWater->backgroundTask2_;
			    selfWater->backgroundTask2_ = NULL;
		    }
		}
		if (Water::backgroundLoad())
			deletionLock_->give();
	}
}


/**
 *	
 */
void Water::deleteUnmanagedObjects( )
{
}


/**
 *	Create all unmanaged resources
 */
void Water::createUnmanagedObjects( )
{
	if ( effect_ && effect_->pEffect() )
	{
		ComObjectWrap<ID3DXEffect> pEffect = effect_->pEffect()->pEffect();
		SimulationCell::createUnmanaged();

		WaterCell::SimulationCellPtrList::iterator cellIt = activeSimulations_.begin();
		for (; cellIt != activeSimulations_.end(); cellIt++)
		{
			//activeList.remove( this );
			(*cellIt)->deactivate();
			(*cellIt)->edgeActivation(false);
		}
		activeSimulations_.clear();
		if ( waterScene_ && !waterScene_->recreate() )
		{
			ERROR_MSG(	"Water::createUnmanagedObjects()"
							" couldn't setup render targets" );	
		}
	}
}


/**
 *	Delete managed objects
 */
void Water::deleteManagedObjects( )
{
	for (uint i=0; i<cells_.size(); i++)
	{
		cells_[i].deleteManaged();
	}
	s_quadVertexBuffer_ = NULL;		
}


/**
 *	Create managed objects
 */
void Water::createManagedObjects( )
{
	if (Moo::rc().device())
	{
		for (uint i=0; i<cells_.size(); i++)
		{
			cells_[i].createManagedIndices();
		}
		createdCells_ = true;

		//Create a quad for simulation renders
		typedef VertexBuffer< VERTEX_TYPE > VBufferType;
		s_quadVertexBuffer_ = new VBufferType;

		if (s_quadVertexBuffer_->reset( 4 ) && 
			s_quadVertexBuffer_->lock())
		{
			VBufferType::iterator vbIt  = s_quadVertexBuffer_->begin();
			VBufferType::iterator vbEnd = s_quadVertexBuffer_->end();

			// Load the vertex data
			vbIt->pos_.set(-1.f, 1.f, 0.f);
			vbIt->colour_ = 0xffffffff;
			vbIt->uv_.set(0,0);
			++vbIt;

			vbIt->pos_.set(1.f, 1.f, 0.f);
			vbIt->colour_ = 0xffffffff;
			vbIt->uv_.set(1,0);
			++vbIt;

			vbIt->pos_.set(1,-1, 0);
			vbIt->colour_ = 0xffffffff;
			vbIt->uv_.set(1,1);
			++vbIt;

			vbIt->pos_.set(-1,-1,0);
			vbIt->colour_ = 0xffffffff;
			vbIt->uv_.set(0,1);
			++vbIt;

			s_quadVertexBuffer_->unlock();
		}
		else
		{
			ERROR_MSG(
				"Water::createManagedObjects: "
				"Could not create/lock vertex buffer\n");
		}

		MF_ASSERT( s_quadVertexBuffer_.getObject() != 0 );

//		if (edgeWaves_)
//			edgeWaves_->createManagedObjects();
	}
	initialised_=false;
	vertexBuffers_.clear();
}


/**
 *	Render the simulations for all the active cells in this water surface.
 */
void Water::renderSimulation(float dTime, Waters& group, bool raining)
{
	if (!simulationEffect_)
		return;

	static DogWatch simulationWatch( "Simulation" );
	simulationWatch.start();

	Vector4 camPos = Moo::rc().invView().row(3);
	static bool first = true;
	if (first || SimulationCell::s_hitTime==-2.0)
	{
		resetSimulation();
		first = false;
	}

	//TODO: move the rain simulation stuff out of the individual water simulation and
	// expose the resulting texture for use elsewhere....
	if (raining)
	{
		SimulationManager::instance().simulateRain( dTime, Waters::instance().rainAmount(), simulationEffect_ );		
	}

	//TODO:
	// -pertubations should not be passed to a cell if its too far away from the camera
	// -cell with the closest perturbation to the camera should take priority (along with its neighbours)

	WaterCell::WaterCellPtrList edgeList;

	WaterCell::WaterCellVector::iterator cellIt = cells_.begin();
	WaterCell::WaterCellPtrList::iterator wCellIt;
	for (; cellIt != cells_.end(); cellIt++)
	{
		(*cellIt).checkActivity( activeSimulations_, edgeList );
	}

	// this list is used to activate neighbouring cells
	for (wCellIt = edgeList.begin(); wCellIt != edgeList.end(); wCellIt++)
	{
		(*wCellIt)->checkEdgeActivity( activeSimulations_ );
	}

	ComObjectWrap<ID3DXEffect> pEffect = simulationEffect_->pEffect()->pEffect();
	pEffect->SetVector("psSimulationPositionWeighting", 
						&D3DXVECTOR4((config_.consistency_+1.f), config_.consistency_, 0.0f, 0.0f));

	WaterCell::SimulationCellPtrList::iterator it = activeSimulations_.begin(); 	
	for (; it != activeSimulations_.end(); it++)
	{
		if ((*it))
		{
			(*it)->simulate( simulationEffect_, dTime, group );
			(*it)->tick();
		}
	}
	simulationWatch.stop();
}


/**
 *	This method resets all the simulation cells.
 */
void Water::resetSimulation()
{
	for (uint i=0; i<cells_.size(); i++)
		cells_[i].clear();
}

/**
 * This clear the reflection render target.
 */
void Water::clearRT()
{
	Moo::RenderTargetPtr rt = waterScene_->reflectionTexture();

	if ((rt) && (rt->valid()) && (rt->push()))
	{
		Moo::rc().beginScene();

		Moo::rc().device()->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
			RGB( 255, 255, 255 ), 1, 0 );

		Moo::rc().endScene();

		rt->pop();
	}
}


void Waters::createSplashSystem( )
{
#ifdef SPLASH_ENABLED
	splashManager_.init();
#endif
}


/**
 * This method draws the water.
 * @param group the group of waters to draw.
 * @param dTime the time in seconds since the water was last drawn.
 */
void Water::draw( Waters & group, float dTime )
{

#ifdef EDITOR_ENABLED

	if (Waters::drawReflection())
	{
		reflectionCleared_ = false;
	}
	else
	{
		if (!reflectionCleared_)
		{
			clearRT();
			reflectionCleared_ = true;
		}
	}

#endif // EDITOR_ENABLED

	if (!cells_.size() || !createdCells_ || (nVertices_==0)) return;	

	static DogWatch waterWatch( "Water" );
	static DogWatch waterCalc( "Calculations" );
	static DogWatch waterDraw( "Draw" );

	waterWatch.start();
	waterCalc.start();

	// Update the water at 30 fps.
	lastDTime_ = max( 0.001f, dTime );

	static bool changed = false;
	bool raining = false;
	if (enableSim_ && group.simulationEnabled())
	{
		raining = (Waters::instance().rainAmount() || SimulationManager::instance().rainActive());
		time_ += dTime * 30.f;
		while (time_ >= 1 )
		{
			time_ -= floorf( time_ );

			renderSimulation(1.f/30.f, group, raining);
		}
		changed = true;
	}
	else if (changed)
	{
		SimulationManager::instance().resetSimulations();
		changed = false;
	}

	waterCalc.stop();

#ifdef EDITOR_ENABLED
	if (drawRed())
	{
		//BigBang::instance().setReadOnlyFog();
		// Set the fog to a constant red colour
		float fnear = -10000.f;
		float ffar = 10000.f;
		DWORD colour = 0x00AA0000;

		Moo::rc().setRenderState( D3DRS_FOGCOLOR, colour );
		Moo::rc().setRenderState( D3DRS_FOGENABLE, true );

		Moo::rc().fogNear( fnear );
		Moo::rc().fogFar( ffar );

		Moo::Material::standardFogColour( colour );
	}
#endif //EDITOR_ENABLED

	// Set up the transforms.
	Moo::rc().push();
	Moo::rc().preMultiply( transform_ );

	Matrix m (Moo::rc().viewProjection());
	m.preMultiply( Moo::rc().world() );

	bb_.calculateOutcode( m );

	Vector3 camPos(Moo::rc().invView().row(3).x,Moo::rc().invView().row(3).y,Moo::rc().invView().row(3).z);
	camPos = invTransform_.applyPoint( camPos );
	bool insideVol = bbActual_.distance(camPos) == 0.f;
	Waters::instance().insideVolume( insideVol || Waters::instance().insideVolume() );
//TODO: need to move this calculation before the main render loop (before the Fogcontroller commit)
//	so I can change the fog settings if inside a water volume (just check them all)



	if (!bb_.combinedOutcode())
	{
		waterDraw.start();

		if (!initialised_)
		{
			initialised_= true;
			vertexBuffers_.clear();
			vertexBuffers_.resize(remappedVerts_.size() + 1);

			// build multiple vertex buffers based on the remapping data.
			// if nVertices_ is greater than the max allowed, break up the buffer.

			typedef VertexBuffer< VERTEX_TYPE > VBufferType;

			for (uint i=0; i<vertexBuffers_.size(); i++)
			{
				vertexBuffers_[i] = new VBufferType;
			}
			VertexBufferPtr pVerts0 = vertexBuffers_[0];
			uint32 nVert0=0;
			if (remappedVerts_.size())
				nVert0 = 0xffff + 1;
			else
				nVert0 = nVertices_;

			bool success = pVerts0->reset( nVert0 ) && pVerts0->lock();
			for ( uint i=1; i<vertexBuffers_.size(); i++ )
			{
				success = success &&
					vertexBuffers_[i]->reset(remappedVerts_[i-1].size()) &&
					vertexBuffers_[i]->lock();
			}
			if (success)
			{
				VBufferType::iterator vb0It  = pVerts0->begin();
				VBufferType::iterator vb01End = pVerts0->end();

				uint32 index = 0;
				WaterAlphas::iterator waterAlpha = wAlpha_.begin();

				float z = -config_.size_.y * 0.5f;
				float zT = 0.f;

				for (uint32 zIndex = 0; zIndex < gridSizeZ_; zIndex++)
				{
					if ((zIndex+1) == gridSizeZ_)
					{
						z = config_.size_.y * 0.5f;
						zT = config_.size_.y;
					}

					float x = -config_.size_.x * 0.5f;
					float xT = 0.f;
					for (uint32 xIndex = 0; xIndex < gridSizeX_; xIndex++)
					{
						if ((xIndex+1) == gridSizeX_)
						{
							x = config_.size_.x * 0.5f;
							xT = config_.size_.x;
						}

						if (wIndex_[ index ] >= 0)
						{
							//check if it's been re-mapped....
							// and put it in the second buffer it it has
							// if it's less than the current max.. add it here too.
							for ( uint vIdx=0; vIdx<remappedVerts_.size(); vIdx++ )
							{
								std::map<uint32, uint32>& mapping =
														remappedVerts_[vIdx];
								VertexBufferPtr pVertsX = vertexBuffers_[vIdx+1];

								std::map<uint32, uint32>::iterator it = 
											mapping.find( wIndex_[ index ] );
								if ( it != mapping.end() )
								{					
									//copy to buffer
									uint32 idx = it->second;
									VERTEX_TYPE&  pVert = (*pVertsX)[idx];
									
									// Set the position of the water point.
									pVert.pos_.set( x, 0, z );
									if (config_.useEdgeAlpha_)
										pVert.colour_ = *(waterAlpha);
									else
										pVert.colour_ = uint32(0xffffffff);

									pVert.uv_.set( xT / config_.textureTessellation_, zT / config_.textureTessellation_);
								}
							}
							if ( wIndex_[ index ] <= (int32)Moo::rc().maxVertexIndex() )
							{
								//copy to buffer one.
								// Set the position of the water point.
								vb0It->pos_.set( x, 0, z );

								if (config_.useEdgeAlpha_)
									vb0It->colour_ = *(waterAlpha);
								else
									vb0It->colour_ = uint32(0xffffffff);

								vb0It->uv_.set( xT / config_.textureTessellation_, zT / config_.textureTessellation_);
								++vb0It;
							}
						}

						++waterAlpha;
						++index;
						x += config_.tessellation_;
						xT += config_.tessellation_;
					}
					z += config_.tessellation_;
					zT += config_.tessellation_;
				}
				for ( uint i=0; i<vertexBuffers_.size(); i++ )
				{
					vertexBuffers_[i]->unlock();
				}

			}
			else
			{
				ERROR_MSG(
					"Water::createManagedObjects: "
					"Could not create/lock vertex buffer\n");
			}

			MF_ASSERT( vertexBuffers_[0].getObject() != 0 );

			remappedVerts_.clear();

			//if (edgeWaves_)
			//	edgeWaves_->init();

		}

		Moo::EffectVisualContext::instance().initConstants();

		// Set our renderstates and material.
		DWORD fillmode = 0;
		Moo::rc().device()->GetRenderState( D3DRS_FILLMODE, &fillmode );
		Moo::rc().setRenderState( D3DRS_FILLMODE,
			group.drawWireframe_ ? D3DFILL_WIREFRAME : D3DFILL_SOLID );

		ComObjectWrap<ID3DXEffect> pEffect = effect_->pEffect()->pEffect();

#if EDITOR_ENABLED
		if (Waters::projectView())
			effect_->hTechnique("water_proj", 0);
		else
			effect_->hTechnique("water_rt", 0);
#else
		effect_->hTechnique("water_rt", 0);
#endif
		

		Matrix wvp;
		wvp.multiply( Moo::rc().world(), 
					  Moo::rc().viewProjection() );
		pEffect->SetMatrix( "worldViewProj", &wvp );
		pEffect->SetMatrix( "world", &Moo::rc().world() );

		Vector4 x1(config_.waveScale_.x,0,0,0);
		Vector4 y1(0,config_.waveScale_.x,0,0);
		Vector4 x2(config_.waveScale_.y,0,0,0);
		Vector4 y2(0,config_.waveScale_.y,0,0);

		texAnim_ += dTime * config_.windVelocity_;

		x1.w = texAnim_*config_.scrollSpeed1_.x;
		y1.w = texAnim_*config_.scrollSpeed1_.y;

		x2.w = texAnim_*config_.scrollSpeed2_.x;
		y2.w = texAnim_*config_.scrollSpeed2_.y;

		//TODO: move this ugly state setting stuff to it's own function
		//TODO: use the parameter cache system.

		pEffect->SetVector( "bumpTexCoordTransformX", &x1 );
		pEffect->SetVector( "bumpTexCoordTransformY", &y1 );
		pEffect->SetVector( "bumpTexCoordTransformX2", &x2 );
		pEffect->SetVector( "bumpTexCoordTransformY2", &y2 );

		//TODO: branch off and setup the surface for simple env. map. reflection if the quality is set to low...

		Vector4 distortionScale( config_.reflectionScale_, config_.reflectionScale_,
								 config_.refractionScale_, config_.refractionScale_ );
		pEffect->SetVector( "scale", &distortionScale );

		pEffect->SetFloat( "fresnelExp", config_.fresnelExponent_ );
		pEffect->SetFloat( "fresnelConstant", config_.fresnelConstant_ );

		pEffect->SetFloat( "fresnelExp", config_.fresnelExponent_ );
		pEffect->SetFloat( "fresnelConstant", config_.fresnelConstant_ );

		pEffect->SetVector( "reflectionTint", &config_.reflectionTint_ );
		pEffect->SetVector( "refractionTint", &config_.refractionTint_ );

		pEffect->SetFloat( "sunPower", config_.sunPower_ );
		//pEffect->SetFloat( "sunScale", config_.sunPower_ );

		//TODO: setup a smoothness parameter for the surfaces....
		pEffect->SetFloat( "smoothness", config_.smoothness_ );

		pEffect->SetTexture("reflectionMap", waterScene_->reflectionTexture()->pTexture() );

		if (Waters::s_drawRefraction_)
			pEffect->SetTexture("refractionMap", waterScene_->refractionTexture()->pTexture() );

		pEffect->SetBool("useRefraction", Waters::s_drawRefraction_ );

		Matrix tTrans(Moo::rc().invView());
		tTrans.postRotateX( DEG_TO_RAD( 90 ) );
		Matrix mScale;
		mScale.setScale( 0.5f, 0.5f, 0.5f );
		tTrans.postMultiply( mScale );
		tTrans.translation( Vector3( 0.5f, 0.5f, 0.5f ) );
		pEffect->SetMatrix( "tex", &tTrans );

		// Set the transforms.
		Moo::rc().setFVF( VERTEX_TYPE::fvf() );

		vertexBuffers_[0]->activate();
		uint32 currentVBuffer = 0;

		for (uint i=0; i<cells_.size(); i++)
		{
			if (currentVBuffer != cells_[i].bufferIndex())
			{
				currentVBuffer = cells_[i].bufferIndex();
				vertexBuffers_[ currentVBuffer ]->activate();
			}

			//TODO: try get the rain to mix in with the regular sim.
			if (raining)
			{
				pEffect->SetBool("useSimulation", true);
				pEffect->SetFloat("simulationTiling", 10.f);
				pEffect->SetTexture("simulationMap", SimulationManager::instance().rainTexture()->pTexture());
			}
			else if (cells_[i].simulationActive())
			{
				pEffect->SetBool("useSimulation", true);
				pEffect->SetFloat("simulationTiling", 1.f);
				pEffect->SetTexture("simulationMap", cells_[i].simTexture()->pTexture());
			}
			else
				pEffect->SetBool("useSimulation", false);

			//TODO: clean this up...
			float miny = cells_[i].min().y;
			float maxy = cells_[i].max().y;
			float minx = cells_[i].min().x;
			float maxx = cells_[i].max().x;

			float simGridSize =   ceilf( config_.simCellSize_ / config_.tessellation_ ) * config_.tessellation_;

			float simGridSizeX = cells_[i].size().x;
			float simGridSizeY = cells_[i].size().y;

			float sx = (config_.textureTessellation_) / (simGridSizeX);
			float sy = (config_.textureTessellation_) / (simGridSizeY);
			float s = (config_.textureTessellation_) / (simGridSize);

			float cx = -(minx + (simGridSize)*0.5f);
			float cy = -(miny + (simGridSize)*0.5f);

			x1.set( s, 0.f, 0.f, cx / config_.textureTessellation_ );
			y1.set( 0.f, s, 0.f, cy / config_.textureTessellation_ );
			pEffect->SetVector( "simulationTransformX", &x1 );
			pEffect->SetVector( "simulationTransformY", &y1 );

			if (cells_[i].bind())
			{
				effect_->begin();
				for (uint32 pass=0; pass<effect_->nPasses();pass++)
				{
					effect_->beginPass(pass);
					cells_[i].draw( vertexBuffers_[ currentVBuffer ]->size() );
					effect_->endPass();
				}
				effect_->end();

				// Add the water primitivecount to the total primitivecount.
				Moo::rc().addToPrimitiveCount( cells_[i].indexCount() - 2 );
			}
		}
		//if (edgeWaves_)
		//	edgeWaves_->draw(effect_);

		// Reset some states to their defaults.
		Moo::rc().setRenderState( D3DRS_FILLMODE, fillmode );

		waterDraw.stop();
	}
	waterWatch.stop();
	Moo::rc().pop();

#ifdef EDITOR_ENABLED
	if( drawRed() )
	{
		FogController::instance().commitFogToDevice();
		//drawRed(false);
	}
#endif //EDITOR_ENABLED
}


/**
 *  Is water loading done in the background?
 */
/*static*/ bool Water::backgroundLoad()
{
    return s_backgroundLoad_;
}

/**
 *  Set whether loading is done in the background.  You should do this at the
 *  start of the app; doing it while the app is running will lead to problems.
 */
/*static*/ void Water::backgroundLoad(bool background)
{
    s_backgroundLoad_ = background;
}


/**
 *	Adds a movement to this water body, if passes through it
 */
void Water::addMovement( const Vector3& from, const Vector3& to, const float diameter )
{
	//TODO: wheres the best place to put this movements reset?
	WaterCell::WaterCellVector::iterator cellIt = cells_.begin();
	for (; cellIt != cells_.end(); cellIt++)
	{
		(*cellIt).updateMovements();
	}
	//TODO: add a constant pulsing movement when standing still

	// Only add it if it's in our bounding box
	Vector3 cfrom = invTransform_.applyPoint( from );
	Vector3 cto = invTransform_.applyPoint( to );
	if ((from != to) && bbDeep_.clip( cfrom, cto ) )
	{
#ifdef SPLASH_ENABLED
		// a copy of the movements for the splashes
		Vector4 impact(cto.x,cto.y,cto.z,0);
		impact.w = (cto-cfrom).length();
		Waters::instance().addSplash(impact, transform_);
#endif
	
		//add the movement to the right cell... (using "to" for now)
		Vector3 halfsize( config_.size_.x*0.5f, 0, config_.size_.y*0.5f );
		cto += halfsize;
		cfrom += halfsize;

		float simGridSize = ceilf( config_.simCellSize_ / config_.tessellation_ ) * config_.tessellation_;
		float invSimSize=1.f/simGridSize;

		int xcell = int( cto.x * invSimSize);
		int ycell = int( cto.z * invSimSize);
		uint cell = xcell + int((ceilf(config_.size_.x * invSimSize)))*ycell;
		if (cell < cells_.size())
		{

			float displacement = (to - from).length();

//			float invSimSizeX=1.f/cells_[cell].size().x;
//			float invSimSizeY=1.f/cells_[cell].size().y;


			//Calculate the position inside this cell
			cto.x = (cto.x*invSimSize - xcell);
			cto.z = (cto.z*invSimSize - ycell);
			cfrom.x = (cfrom.x*invSimSize - xcell);
			cfrom.z = (cfrom.z*invSimSize - ycell);

			cto.y = displacement;

			cells_[cell].addMovement( cfrom, cto, diameter );
		}
	}
}


#if UMBRA_ENABLE
/**
 *	Add a terrain chunk item to the list for umbra culling purposes.
 */
void Water::addTerrainItem( ChunkTerrain* item )
{
	TerrainVector::iterator found = std::find(	terrainItems_.begin(),
												terrainItems_.end(),
												item );
	if (found == terrainItems_.end())
		terrainItems_.push_back(item);
}


/**
 *	Remove a terrain chunk item from the occluder list.
 */
void Water::eraseTerrainItem( ChunkTerrain* item )
{
	TerrainVector::iterator found = std::find(	terrainItems_.begin(),
												terrainItems_.end(),
												item );
	if (found != terrainItems_.end())
		terrainItems_.erase(found);
}

/**
 *	Disable all the terrain Umbra occluders.
 */
void Water::disableOccluders() const
{
	TerrainVector::const_iterator it = terrainItems_.begin();
	for (; it != terrainItems_.end(); it++)
	{
		ChunkTerrainPtr terrain = (*it);
		terrain->disableOccluder();
	}	
}


/**
 *	Enable all the terrain Umbra occluders.
 */
void Water::enableOccluders() const
{
	TerrainVector::const_iterator it = terrainItems_.begin();
	for (; it != terrainItems_.end(); it++)
	{
		ChunkTerrainPtr terrain = (*it);
		terrain->enableOccluder();
	}	
}

#endif //UMBRA_ENABLE

// -----------------------------------------------------------------------------
// Section: Waters
// -----------------------------------------------------------------------------

/**
 *	A callback method for the graphics setting quality option
 */
void Waters::setQualityOption(int optionIndex)
{
	if (optionIndex == 0)
	{
		//High quality settings..
		WaterSceneRenderer::s_textureSize_ = 1024;
		WaterSceneRenderer::s_drawTrees_ = true;
		WaterSceneRenderer::s_drawDynamics_ = true;
		WaterSceneRenderer::s_drawPlayer_ = true;
		WaterSceneRenderer::s_simpleRefraction_ = false;
		WaterSceneRenderer::s_modifyClip_ = false;

		s_drawRefraction_=true;
	}
	else if (optionIndex == 1)
	{
		//Mid quality settings..
		WaterSceneRenderer::s_textureSize_ = 512;
		WaterSceneRenderer::s_drawTrees_ = true;
		WaterSceneRenderer::s_drawDynamics_ = false;
		WaterSceneRenderer::s_drawPlayer_ = true;
		WaterSceneRenderer::s_simpleRefraction_ = true;
		WaterSceneRenderer::s_modifyClip_ = false;

		s_drawRefraction_=true;
	}
	else if (optionIndex == 2)
	{
		//TODO: LOD trees....

		//Low quality settings..
		WaterSceneRenderer::s_textureSize_ = 256;
		WaterSceneRenderer::s_drawTrees_ = true;
		WaterSceneRenderer::s_drawDynamics_ = false;
		WaterSceneRenderer::s_drawPlayer_ = true;
		WaterSceneRenderer::s_simpleRefraction_ = true;
		WaterSceneRenderer::s_modifyClip_ = true;

		s_drawRefraction_=false;
	}
	else
	{
		//Lowest quality settings..
		WaterSceneRenderer::s_textureSize_ = 256;
		WaterSceneRenderer::s_drawTrees_ = false;
		WaterSceneRenderer::s_drawDynamics_ = false;
		WaterSceneRenderer::s_drawPlayer_ = false;
		WaterSceneRenderer::s_simpleRefraction_ = true;
		WaterSceneRenderer::s_modifyClip_ = true;

		s_drawRefraction_=false;
		s_simulationLevel_=0;
	}
}


/**
 *	A callback method for the graphics setting simulation quality option
 */
void Waters::setSimulationOption(int optionIndex)
{
	if (optionIndex==0)
	{
		s_simulationLevel_ = 2;
		//TODO: tweak the max sim textures...
		//SimulationManager::instance().setMaxTextures(4);
	}
	else if (optionIndex==1)
	{
		s_simulationLevel_ = 1;
		//SimulationManager::instance().setMaxTextures(3);
	}
	else
		s_simulationLevel_ = 0;

	//SimulationManager::instance().resetSimulations();
	//SimulationManager::instance().recreateBlocks();	
}


/**
 *	Retrieves the Waters object instance
 */
Waters& Waters::instance()
{
	static Waters inst;
	return inst;
}


/**
 *	Cleanup
 */
void Waters::fini( )
{
	s_simulationEffect_ = NULL;
	s_effect_ = NULL;

#ifdef SPLASH_ENABLED
	splashManager_.fini();
#endif
}


/**
 *	Load the required resources
 */
void Waters::loadResources( void * self )
{
	Waters * selfWaters = static_cast< Waters * >( self );

	bool res=false;
	s_simulationEffect_ = new Moo::EffectMaterial;
	res = s_simulationEffect_->initFromEffect( s_simulationEffect );		
	if (!res)
		s_simulationEffect_ = NULL;

	s_effect_ = new Moo::EffectMaterial;
	res = s_effect_->initFromEffect( s_waterEffect );
	if (!res)
	{
		CRITICAL_MSG( "Water::doCreateTables()"
		" couldn't load effect file "
		"%s\n", s_waterEffect.value().c_str() );			
	}
}


/**
 *	Called upon finishing the resource loading
 */
void Waters::loadResourcesComplete( void * self )
{
	Waters * selfWaters = static_cast< Waters * >( self );
	if (Water::backgroundLoad())
	{
		delete selfWaters->loadingTask1_;
		selfWaters->loadingTask1_ = NULL;
	}
}


/**
 *	Initialise the water settings
 */
void Waters::init( )
{
	//
	// Register graphics settings
	//		

	// water quality settings
	bool supported = Moo::rc().vsVersion() >= 0x200 && Moo::rc().psVersion() >= 0x200;

	typedef Moo::GraphicsSetting::GraphicsSettingPtr GraphicsSettingPtr;
	GraphicsSettingPtr qualitySettings = 
		Moo::makeCallbackGraphicsSetting(
			"WATER_QUALITY", *this, 
			&Waters::setQualityOption, 
			supported ? 0 : 2, false, false);


	qualitySettings->addOption("HIGH",  supported);
	qualitySettings->addOption("MEDIUM", supported);
	qualitySettings->addOption("LOW", true);
	qualitySettings->addOption("LOWEST", true);
	Moo::GraphicsSetting::add(qualitySettings);

	// simulation toggle
	typedef Moo::GraphicsSetting::GraphicsSettingPtr GraphicsSettingPtr;
	GraphicsSettingPtr simSettings = 
		Moo::makeCallbackGraphicsSetting(
			"WATER_SIMULATION", *this, 
			&Waters::setSimulationOption, 
			supported ? 0 : 2, false, false);

	bool simulationSupported = Moo::rc().psVersion() >= 0x300 && (Moo::RenderContext::instance().supportsTextureFormat( D3DFMT_A16B16G16R16F ));

	simSettings->addOption("HIGH",  simulationSupported);
	simSettings->addOption("LOW",  simulationSupported);
	simSettings->addOption("OFF", true);
	Moo::GraphicsSetting::add(simSettings);
	// number of reflections settings ??????

    if (Water::backgroundLoad())
    {
	    // do heavy setup stuff in the background
	    this->loadingTask1_ = new BackgroundTask( 
			    &Waters::loadResources, this,
			    &Waters::loadResourcesComplete, this );
	    BgTaskManager::instance()->addTask( *loadingTask1_ );
    }
    else
    {
	    loadResources(this);
	    loadResourcesComplete(this);
    }

	createSplashSystem();
}


/**
 *	Waters constructor
 */
Waters::Waters() :
	drawWireframe_( false ),
	movementImpact_( 20.0f ),
	rainAmount_( 0.f ),
	waterQuality_( 0 )
{
	static bool firstTime = true;
	if (firstTime)
	{
		MF_WATCH( "Client Settings/Water/draw", s_drawWaters_, 
					Watcher::WT_READ_WRITE,
					"Draw water?" );
		MF_WATCH( "Client Settings/Water/draw refraction", s_drawRefraction_,
					Watcher::WT_READ_WRITE,
					"Use/Draw water refraction?" );
		MF_WATCH( "Client Settings/Water/wireframe", drawWireframe_,
					Watcher::WT_READ_WRITE,
					"Draw water in wire frame mode?" );
		MF_WATCH( "Client Settings/Water/character impact", movementImpact_,
					Watcher::WT_READ_WRITE,
					"Character simulation-impact scale" );

		firstTime = false;
	}
}


/**
 *	Waters destructor
 */
Waters::~Waters()
{
	s_simulationEffect_ = NULL;
	s_effect_ = NULL;
}


/**
 * Adds a movement to all the waters.
 * @param from world space position, defining the start of the line moving through the water.
 * @param to world space position, defining the end of the line moving through the water.
 */
void Waters::addMovement( const Vector3& from, const Vector3& to, const float diameter )
{
	Waters::iterator it = this->begin();
	Waters::iterator end = this->end();
	while (it!=end)
	{
		(*it++)->addMovement( from, to, diameter );
	}
}


// Static list used to cache up all the water surfaces that need to be drawn
static VectorNoDestructor< Water * > s_waterDrawList;

/**
 *	This static method adds a water object to the list of waters to draw
 */
void Waters::addToDrawList( Water * pWater )
{
	if ( pWater->drawSelf_ && !(s_nextMark_ == pWater->drawMark()) )
	{
		s_waterDrawList.push_back( pWater );
		pWater->drawMark( s_nextMark_ );
	}
}


/**
 *	This method draws all the stored water objects under this object's
 *	auspices.
 */
void Waters::drawDrawList( float dTime )
{	
	insideVolume(false);

	if (s_drawWaters_)
	{
		for (uint i = 0; i < s_waterDrawList.size(); i++)
		{
			s_waterDrawList[i]->draw( *this, dTime );
		}
	}
	s_lastDrawMark_ = s_nextMark_;
	s_nextMark_++;
	s_waterDrawList.clear();

#ifdef SPLASH_ENABLED
	splashManager_.draw(dTime);
#endif
}

// water.cpp
