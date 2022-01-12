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

#include "cstdmf/concurrency.hpp"
#include "cstdmf/dogwatch.hpp"
#include "cstdmf/memory_counter.hpp"
#include "math/mathdef.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/auto_config.hpp"
#include "resmgr/sanitise_helper.hpp"
#include "moo/moo_math.hpp"
#include "math/blend_transform.hpp"
#include "moo/node.hpp"
#include "moo/visual.hpp"
#include "moo/visual_manager.hpp"
#include "moo/base_texture.hpp"
#include "moo/texture_manager.hpp"
#include "moo/animation_manager.hpp"
#include "moo/discrete_animation_channel.hpp"
#include "moo/interpolated_animation_channel.hpp"
#include "moo/streamed_animation_channel.hpp"
#include "moo/render_context.hpp"
#include "moo/morph_vertices.hpp"
#include "moo/render_target.hpp"
#include "moo/visual_manager.hpp"
#include "moo/visual.hpp"
#include "moo/visual_channels.hpp"
#include "model_common.hpp"
#ifdef EDITOR_ENABLED
#include "../../tools/common/material_properties.hpp"
#endif

#include "physics2/bsp.hpp"

#include "romp/static_light_values.hpp"

#include "super_model.hpp"

#include "cstdmf/debug.hpp"
DECLARE_DEBUG_COMPONENT2( "Romp", 0 )

#include <set>
#include <strstream>

template <class TYPE>
uint32 contentSize( const std::vector< TYPE >& vector )
{
	return vector.size() * sizeof( TYPE );
}

template <class TYPE>
uint32 contentSize( const StringHashMap< TYPE >& vector )
{
	return vector.size() * sizeof( TYPE );
}

memoryCounterDefine( model, Geometry );
memoryCounterDefine( modelGlobl, Geometry );
memoryCounterDefine( superModel, Entity );


/**
 *	This is a useful utility class that holds an array, and can
 *	be reference counted, so you can easily have smart pointers to
 *	structures that are just an array. It will likely be moved into
 *	cstdmf at some stage.
 */
template <class OF> class ArrayHolder : public ReferenceCount
{
public:
	explicit ArrayHolder( int sz ) : e_( new OF[sz] ) {}
	~ArrayHolder()			{ delete [] e_; }

	operator OF * () const	{ return e_; }

	bool isOK () { return true; }

private:
	OF * e_;
};


static AutoConfigString s_billboardMfmName( "environment/billboardMaterial" );
static AutoConfigString s_billboardVisualName( "environment/billboardVisual" );

static int initMatter_NewVisual( Model::Matter & m, Moo::Visual & bulk );
static bool initTint_NewVisual( Model::Tint & t, DataSectionPtr matSect );

#ifdef EDITOR_ENABLED
/*static*/ SmartPointer< AnimLoadCallback > Model::s_pAnimLoadCallback_ = NULL;
#endif

// ----------------------------------------------------------------------------
// Section: Model class specialisations
// ----------------------------------------------------------------------------

static BoundingBox s_emptyBoundingBox( Vector3(0,0,0), Vector3(0,0,0) );

// enable this to enable debugging of mesh normals
//#define DEBUG_NORMALS
// NOTE: it's not a robust solution for drawning normals
// so use it at your own risk :P
// also needs to be defined in : "moo\vertices.hpp"

#ifdef DEBUG_NORMALS
#include "romp/geometrics.hpp"
bool draw_norms = false;
float scale_norms = 0.1f;
#endif


/**
 *	Class for models whose bulk is a visual with nodes
 */
class NodefullModel : public Model, public Aligned
{
public:
	NodefullModel( const std::string & resourceID, DataSectionPtr pFile );
	~NodefullModel();

	virtual bool valid() const	{ return (bulk_) && (bulk_->isOK()); }

	virtual void dress();
	virtual void draw( bool checkBB );

	virtual const BSPTree * decompose() const;

	virtual const BoundingBox & boundingBox() const;
	virtual const BoundingBox & visibilityBox() const;
	virtual bool hasNode( Moo::Node * pNode,
		MooNodeChain * pParentChain ) const;

	virtual Model::NodeTreeIterator nodeTreeBegin() const;
	virtual Model::NodeTreeIterator nodeTreeEnd() const;

	void	batched( bool state ) { batched_ = state; }
	bool	batched( ) const { return batched_; }

	virtual MaterialOverride overrideMaterial( const std::string& identifier, Moo::EffectMaterial* material );
	virtual int gatherMaterials( const std::string & materialIdentifier, std::vector< Moo::EffectMaterial** > & uses, const Moo::EffectMaterial ** ppOriginal = NULL )
	{
		return bulk_->gatherMaterials( materialIdentifier, uses, ppOriginal );
	}

	Moo::VisualPtr getVisual() { return bulk_; }

private:
	bool						batched_;
	MaterialOverrides			materialOverrides_;
	SmartPointer<Moo::Visual>	bulk_;

	BoundingBox					visibilityBox_;

	typedef std::vector<NodeTreeData>	NodeTree;
	NodeTree	nodeTree_;


	Matrix		world_;	// set when model is dressed (traversed)


	typedef Model::Animation Model_Animation;

	/**
	 *	NodefullModel's animation
	 */
	class Animation : public Model_Animation, public Aligned
	{
	public:
		Animation( NodefullModel & rOwner, Moo::AnimationPtr anim,
			DataSectionPtr pAnimSect );
		virtual ~Animation();

		virtual void tick( float dtime, float otime, float ntime );
		virtual void play( float time, float blendRatio, int flags );

		virtual void flagFactor( int flags, Matrix & mOut ) const;
		virtual const Matrix & flagFactorBit( int bit ) const;

		virtual uint32 sizeInBytes() const;

		virtual Moo::AnimationPtr getMooAnim() { return anim_; }

	private:

		void precalcItinerant( int flags );

		NodefullModel			& rOwner_;	// needed for fn above
		Moo::AnimationPtr		anim_;		//  (but not terribly bad)

		int			firstFrame_;	///< Starts at beginning of this frame
		int			lastFrame_;		///< Stops  at beginning of this frame
		float		frameRate_;		///< This many frames per second
		int			frameCount_;	///< This many frames in total

		Moo::NodeAlphas				* alphas_;

		Moo::AnimationChannelPtr	alternateItinerants_[8];
		Matrix						flagFactors_[3];
		Moo::AnimationChannelPtr	flagFactor2_;

		std::string					cognate_;	// for coordination


		Animation( const Animation & rOther );
		Animation & operator=( const Animation & rOther );
	};

	friend class Animation;

	/**
	 *	Nodefull model's default animation
	 */
	class DefaultAnimation : public Model_Animation
	{
	public:
		DefaultAnimation( NodeTree & rTree,
			int itinerantNodeIndex,
			std::avector< Matrix > & transforms );
		virtual ~DefaultAnimation();

		virtual void play( float time, float blendRatio, int flags );

		virtual void flagFactor( int flags, Matrix & mOut ) const;

		std::avector< Matrix > & cTransforms() { return cTransforms_; }

	private:
		NodeTree		& rTree_;
		int				itinerantNodeIndex_;

		std::vector< BlendTransform >			bTransforms_;
		std::avector< Matrix >					cTransforms_;
	};
	friend class DefaultAnimation;

	Moo::StreamedDataCache	* pAnimCache_;

	virtual int initMatter( Matter & m )
		{ return initMatter_NewVisual( m, *bulk_ ); }
	virtual bool initTint( Tint & t, DataSectionPtr matSect )
		{ return initTint_NewVisual( t, matSect ); }
};

/**
 *	Helper class for models whose animations can only switch their bulk
 */
template < class BULK > class SwitchedModel : public Model
{
public:
	typedef BULK (*BulkLoader)( Model & m, const std::string & resourceID );

	SwitchedModel( const std::string & resourceID,
		DataSectionPtr pFile );

	virtual bool valid() const	{ return (bulk_) && (bulk_->isOK()); }

	virtual void dress();

	void setFrame( BULK frame ) { frameDress_ = frame; }

	float				blend( int blendCookie )
		{ return (blendCookie == blendCookie_) ? blendRatio_ : 0.f; }
	void				blend( int blendCookie, float blendRatio )
		{ blendCookie_ = blendCookie; blendRatio_ = blendRatio; }

protected:
	bool wireSwitch(
		DataSectionPtr pFile,
		BulkLoader loader,
		const std::string & mainLabel,
		const std::string & frameLabel );

	BULK					bulk_;
	BULK					frameDress_;
	BULK					frameDraw_;
	// if we want nodeless models to do the animation inheritance thing
	// then these 'frame' things will have to be a static array (elt per part)
	// or the anim play will have to take the model it's operating on or ... ?
	// (atm this is inconsistent because animations that we inherit from
	// a parent switched model do not effect this model unless we respec them)

	typedef Model::Animation Model_Animation;

	/**
	 *	SwitchedModel's animation
	 */
	class Animation : public Model_Animation
	{
	public:
		Animation( SwitchedModel<BULK> & owner, const std::string & name, float frameRate ) :
			owner_( owner ), identifier_( name ), frameRate_( frameRate )
		{
			memoryCounterAdd( model );
			memoryClaim( identifier_ );
		}

		virtual ~Animation()
		{
			memoryCounterSub( model );
			memoryClaim( frames_ );
			memoryClaim( identifier_ );
		}

		virtual void play( float time, float blendRatio, int /*flags*/ )
		{
			// Animations should not be added to the list if
			//  they have no frames_!
			if (owner_.blend( s_blendCookie_ ) < blendRatio)
			{
				owner_.setFrame( frames_[ uint32(time*frameRate_) % frames_.size() ] );
				owner_.blend( s_blendCookie_, blendRatio );
			}
		}

		void addFrame( BULK frame )
		{
			{
				memoryCounterSub( model );
				memoryClaim( frames_ );
			}
			frames_.push_back( frame );
			duration_ = float(frames_.size()) / frameRate_;
			looped_ = true;
			{
				memoryCounterAdd( model );
				memoryClaim( frames_ );
			}
		}

		int numFrames() { return frames_.size(); }

		std::vector<BULK> & frames() { return frames_; }

	private:
		SwitchedModel<BULK> & owner_;

		std::string			identifier_;
		float				frameRate_;
		std::vector<BULK>	frames_;
	};

	float	blendRatio_;
	int		blendCookie_;
};


/**
 *	Class for models whose bulk is a visual without nodes
 */
class NodelessModel : public SwitchedModel< Moo::VisualPtr >
{
public:
	NodelessModel( const std::string & resourceID, DataSectionPtr pFile );

	virtual void draw( bool checkBB );
	virtual void dress();

	virtual const BSPTree * decompose() const;

	virtual const BoundingBox & boundingBox() const;
	virtual const BoundingBox & visibilityBox() const;

	virtual StaticLightingPtr getStaticLighting( const DataSectionPtr section );

	virtual MaterialOverride overrideMaterial( const std::string& identifier, Moo::EffectMaterial* material )
	{
		if( materialOverrides_.find( identifier ) == materialOverrides_.end() )
		{// create a new one
			MaterialOverride mo;
			mo.identifier_ = identifier;
			bulk_->gatherMaterials( identifier, mo.effectiveMaterials_, NULL );
			materialOverrides_[ identifier ] = mo;
		}
		materialOverrides_[ identifier ].update( material );
	//	bulk_->overrideMaterial( identifier, *material );
		return materialOverrides_[ identifier ];
	}

	Moo::VisualPtr getVisual() { return bulk_; }

private:
	virtual int gatherMaterials( const std::string & materialIdentifier, std::vector< Moo::EffectMaterial** > & uses, const Moo::EffectMaterial ** ppOriginal = NULL )
	{
		return bulk_->gatherMaterials( materialIdentifier, uses, ppOriginal );
	}
	MaterialOverrides materialOverrides_;
	bool		batched_;

	static Moo::VisualPtr loadVisual( Model & m, const std::string & resourceID )
	{
		std::string visualName = resourceID + ".static.visual";
		Moo::VisualPtr vis = Moo::VisualManager::instance()->get( visualName );
		if (!vis)
		{
			visualName = resourceID + ".visual";
			vis = Moo::VisualManager::instance()->get( visualName );
		}
		if (vis)
		{
			// complain if this visual has nodes
			if (vis->rootNode()->nChildren() != 0)
			{
				WARNING_MSG( "NodelessModel::loadVisual: "
					"Visual %s has multiple nodes! (attachments broken)\n",
					resourceID.c_str() );
			}
			else
			{
				// but all have a root node
				Moo::Node * pCatNode =
					Model::findOrAddNodeInCatalogue( vis->rootNode() );
				if (!s_sceneRootNode_) s_sceneRootNode_ = pCatNode;
				MF_ASSERT( s_sceneRootNode_ == pCatNode );

				SimpleMutexHolder smh( s_nodeCatalogueLock_ );
				vis->useExistingNodes( s_nodeCatalogue_ );
			}


			/* Can't do this because then would need to do our own traversal
				like NodefullModel does. Which would suck.
			std::vector<Moo::NodePtr>	stack;
			stack.push_back( vis->rootNode() );
			while (!stack.empty())
			{
				Moo::NodePtr pNode = stack.back();
				stack.pop_back();

				Model::findOrAddNodeInCatalogue( pNode );

				for (int i = pNode->nChildren() - 1; i >= 0 ; i--)
					stack.push_back( pNode->child( i ) );
			}

			SimpleMutexHolder smh( s_nodeCatalogueLock_ );
			vis->useExistingNodes( s_nodeCatalogue_ );
			*/
		}
		return vis;
	}


	/**
	 *	This class is our own version of static lighting
	 */
	class StaticLighting : public Model::StaticLighting
	{
	public:
		StaticLighting( Moo::VisualPtr bulk, StaticLightValues * pSLV ) :
			bulk_( bulk ), pSLV_( pSLV ) { }
		virtual ~StaticLighting() { delete pSLV_; }

		virtual void set();
		virtual void unset();

		virtual StaticLightValues* staticLightValues() { return pSLV_; }

	private:
		Moo::VisualPtr			bulk_;
		StaticLightValues *		pSLV_;
	};

	virtual int initMatter( Matter & m )
		{ return initMatter_NewVisual( m, *bulk_ ); }
	virtual bool initTint( Tint & t, DataSectionPtr matSect )
		{ return initTint_NewVisual( t, matSect ); }

	static Moo::NodePtr s_sceneRootNode_;
};

Moo::NodePtr NodelessModel::s_sceneRootNode_ = NULL;

typedef SmartPointer< ArrayHolder< Moo::BaseTexturePtr > > TextureArrayPtr;

/**
 *	Class for models whose bulk is just a billboard
 */
class BillboardModel : public SwitchedModel< TextureArrayPtr >, public Aligned
{
public:
	BillboardModel( const std::string & resourceID, DataSectionPtr pFile );
	~BillboardModel();

	virtual void dress();
	virtual void draw( bool checkBB );

	virtual const BoundingBox & boundingBox() const
		{ return bb_; }

	virtual const BoundingBox & visibilityBox() const
		{ return bb_; }

	virtual void soak( int dye );

private:
	static TextureArrayPtr loadTextureCombo( Model & m,
		const std::string & resourceID );

	std::string expressComboIndex( int comboIndex,
		SuperModel * sm = NULL, FashionVector * fv = NULL );

	static void addDyesFromSelections( const DyeSelections & ds,
		SuperModel & sm, FashionVector & fv );

	void createTexturesFromSource();

	Moo::BaseTexturePtr createOneTexture( std::string & resourceID,
		SuperModel & sm, FashionVector & fv,
		BoundingBox & bb, DX::BaseTexture * pTex );


//	void activateSource( DataSectionPtr pFile );

	friend class BBSourcePLCB;

	int		dyeComboCount_;
	int		dyeComboIndexDress_;
	int		dyeComboIndexDraw_;
	int		unused_;

	Matrix			scaleToBox_;	// align?
	BoundingBox		bb_;

	std::string		bulkResourceID_;

	std::vector< std::string >	sourceModels_;
	int							sourceWidth_;
	int							sourceHeight_;
	DyeSelections				sourceDyes_;

	static Moo::VisualPtr			billboardVisual_;
	static Moo::EffectMaterial*		billboardMaterial_;
	static Moo::EffectMaterial **	billboardVisualMaterial_;
	static Moo::EffectPropertyPtr	billboardTextureProperty_;
};


// ----------------------------------------------------------------------------
// Section: PostLoadCallBack
// ----------------------------------------------------------------------------

/**
 *	This class defines and manages callbacks which occur at various
 *	times in the model loading process.
 */
class PostLoadCallBack
{
public:
	virtual ~PostLoadCallBack() {};

	virtual void operator()() = 0;

	static void add( PostLoadCallBack * plcb, bool global = false );
	static void run( bool global = false );

	static void enter();
	static void leave();

private:

	/**
	 *	Helper struct to store the callbacks for one thread
	 */
	struct PLCBs
	{
		PLCBs() : enterCount( 0 ) { }

		int									enterCount;
		std::vector< PostLoadCallBack * >	globals;
		std::vector< PostLoadCallBack * >	locals;
	};

	typedef std::map< uint32, PLCBs > PLCBmap;
	static PLCBmap		threads_;
	static SimpleMutex	threadsLock_;
};

PostLoadCallBack::PLCBmap PostLoadCallBack::threads_;
SimpleMutex PostLoadCallBack::threadsLock_;

/**
 *	This method adds a callback object to either the global or local list
 */
void PostLoadCallBack::add( PostLoadCallBack * plcb, bool global )
{
	SimpleMutexHolder smh( threadsLock_ );

	PLCBs & plcbs = threads_[OurThreadID()];
	if (!global)	plcbs.locals.push_back ( plcb );
	else			plcbs.globals.push_back( plcb );
}

/**
 *	This method runs all callbacks from one of the lists
 */
void PostLoadCallBack::run( bool global )
{
	SimpleMutexHolder smh( threadsLock_ );
	std::vector< PostLoadCallBack * > * cbs;

	PLCBs & plcbs = threads_[OurThreadID()];
	cbs = global ? &plcbs.globals : &plcbs.locals;

	while (!cbs->empty())
	{
		PostLoadCallBack * lf = cbs->front();
		cbs->erase( cbs->begin() );

		threadsLock_.give();
		(*lf)();
		delete lf;
		threadsLock_.grab();

		PLCBs & plcbs = threads_[OurThreadID()];
		cbs = global ? &plcbs.globals : &plcbs.locals;
	}
}


/**
 *	Enter a level of model loads in the current thread.
 */
void PostLoadCallBack::enter()
{
	SimpleMutexHolder smh( threadsLock_ );
	threads_[OurThreadID()].enterCount++;
}

/**
 *	Leave a level of model loads in the current thread.
 *	If it was the last level, call the global callbacks.
 */
void PostLoadCallBack::leave()
{
	threadsLock_.grab();
	bool wasLastLevel = (--threads_[OurThreadID()].enterCount == 0);
	threadsLock_.give();

	if (wasLastLevel) run( true );
}


// ----------------------------------------------------------------------------
// Section: Model
// ----------------------------------------------------------------------------


Model::NodeCatalogue Model::s_nodeCatalogue_;
SimpleMutex Model::s_nodeCatalogueLock_;
Model::PropCatalogue Model::s_propCatalogue_;
SimpleMutex Model::s_propCatalogueLock_;
int Model::s_blendCookie_ = 0x08000000;


/**
 *	This class is a map of all the currently loaded models.
 */
class ModelMap
{
public:
	ModelMap()
	{
		memoryCounterAdd( modelGlobl );
		memoryClaim( map_ );
		memoryClaim( Model::s_nodeCatalogue_ );
		memoryClaim( Model::s_propCatalogue_ );
	}
	~ModelMap()
	{
		memoryCounterSub( modelGlobl );
		memoryClaim( Model::s_nodeCatalogue_ );
		memoryClaim( Model::s_propCatalogue_ );
		memoryClaim( map_ );
	}

	void add( Model * pModel, const std::string & resourceID );
	void del( const std::string& resourceID );
	void del( Model * pModel );

	ModelPtr find( const std::string & resourceID );

	void findChildren( const std::string & parentResID,
		std::vector< ModelPtr > & children );
private:
	typedef	std::map<std::string,Model*> Map;

	Map					map_;
	SimpleMutex			sm_;
};


/**
 *	Add a model to the map
 */
void ModelMap::add( Model * pModel, const std::string & resourceID )
{
	SimpleMutexHolder smh( sm_ );

	//map_[ resourceID ] = pModel;
	Map::iterator found = map_.find( resourceID );
	if (found == map_.end())
	{
		found = map_.insert( std::make_pair( resourceID, pModel ) ).first;
		memoryCounterAdd( modelGlobl );
		memoryClaim( found->first );
		memoryClaim( map_, found );
	}
	else
	{
		found->second = pModel;
	}
}

/**
 *	Delete a model from the map, using resourceID
 */
void ModelMap::del( const std::string& resourceID )
{
	SimpleMutexHolder smh( sm_ );

	Map::iterator it = map_.find( resourceID);
	if (it != map_.end())
	{
		memoryCounterSub( modelGlobl );
		memoryClaim( it->first );
		memoryClaim( map_, it );
		map_.erase( it );
	}
}

/**
 *	Delete a model from the map
 */
void ModelMap::del( Model * pModel )
{
	SimpleMutexHolder smh( sm_ );

	for (Map::iterator it = map_.begin(); it != map_.end(); it++)
	{

		if (it->second == pModel)
		{
			/*   // Removed this debug message since it clutters
                        //      the message window very quickly.
                        DEBUG_MSG( "ModelMap::del: %s\n",
#ifdef USE_STRINGMAP
				it->first );
#else
				it->first.c_str() );
#endif                    */
			memoryCounterSub( modelGlobl );
			memoryClaim( it->first );
			memoryClaim( map_, it );
			map_.erase( it );
			return;
		}
	}

	/* // Removed this message since it is often incorrectly reported
        //    and looks ugly ;-)
        if (map_.size())
	{
		WARNING_MSG( "ModelMap::del: "
			"Could not find model '%s' at 0x%08X to delete it "
			"(only ok if it was reloaded or was invalid)\n",
			pModel->resourceID().c_str(), pModel );
	}
        */
}


/**
 *	Find a model in the map
 */
ModelPtr ModelMap::find( const std::string & resourceID )
{
	SimpleMutexHolder smh( sm_ );

	Map::iterator found = map_.find( resourceID.c_str() );
	if (found == map_.end()) return NULL;

	return ModelPtr( found->second, ModelPtr::FALLIBLE );
	// if ref count was zero then someone is blocked on our
	// sm_ mutex waiting to delete it.
}


/**
 *	Find all the models in the map that are the immediate children of
 *	a model with the given resource ID
 */
void ModelMap::findChildren( const std::string & parentResID,
	std::vector< ModelPtr > & children )
{
	SimpleMutexHolder smh( sm_ );

	children.clear();

	for (Map::iterator it = map_.begin(); it != map_.end(); it++)
	{
		Model * chPar = it->second->parent();
		if (chPar != NULL && chPar->resourceID() == parentResID)
		{
			ModelPtr mp( it->second, ModelPtr::FALLIBLE );
			// see note in find about the necessity of this
			if (mp) children.push_back( mp );
		}
	}
}



/**
 *	This class lets us use a singleton of another class in a safe way
 */
template <class C> class OneClassPtr
{
public:
	C * operator->()
		{ if (pC == NULL) pC = new C(); return pC; }
	C & operator*()
		{ return *this->operator->(); }
	~OneClassPtr()
		{ if (pC != NULL) { delete pC; pC = NULL; } }
private:
	static C	* pC;
};

typedef OneClassPtr<ModelMap> OneModelMapPtr;

template <> ModelMap * OneClassPtr<ModelMap>::pC = NULL;
//ModelMap * OneModelMapPtr::pC = NULL;

static OneModelMapPtr s_pModelMap;

/*static*/ std::vector< std::string > Model::s_warned_;
/**
 * Static method to clear the list of model load errors already reported
 */
/*static*/ void Model::clearReportedErrors()
{
	s_warned_.clear();
}

/**
 *	Static function to get the model with the given resource ID
 */
ModelPtr Model::get( const std::string & resourceID, bool loadIfMissing )
{
	ModelPtr pFound = s_pModelMap->find( resourceID );
	if (pFound) return pFound;

	if (!loadIfMissing) return NULL;

	DataSectionPtr pFile = BWResource::openSection( resourceID );
	if (!pFile)
	{
		std::vector< std::string >::iterator entry = std::find( s_warned_.begin(), s_warned_.end(), resourceID );
		if (entry == s_warned_.end())
		{
			
			ERROR_MSG( "Model::get: Could not open model resource %s\n",
				resourceID.c_str() );
			s_warned_.push_back( resourceID );
		}
		return NULL;
	}

	PostLoadCallBack::enter();

	ModelPtr mp = Model::load( resourceID, pFile );

	if (mp)
	{
		// must take reference to model before adding to map, or else
		// another thread looking for it could think it's been deleted
		s_pModelMap->add( &*mp, resourceID );
	}

	PostLoadCallBack::leave();

	return mp;
}

/**
 *	This method returns the size of this Model in bytes.
 */
uint32 Model::sizeInBytes() const
{
	uint32 size = sizeof(*this) + resourceID_.length();
	uint parentSize;

#ifdef USE_STRINGMAP
	size += animations_.sizeInBytes();
	parentSize = parent_ ? parent_->animations_.size() : 0;
	for (uint i = parentSize; i < animations_.size(); i++)
		size += (animations_.begin()+i)->second->sizeInBytes();
#else
	AnimationsIndex::const_iterator it;

	size += animations_.capacity() * sizeof(animations_[0]);
	parentSize = parent_ ? parent_->animations_.size() : 0;
	for (it = animationsIndex_.begin(); it != animationsIndex_.end(); it++)
	{
		size += 16;	// about the size of a tree node, excluding value_type
		size += sizeof(it) + it->first.length();
	}
	for (uint i = parentSize; i < animations_.size(); i++)
		size += animations_[i]->sizeInBytes();
#endif

#ifdef USE_STRINGMAP
	size += actions_.sizeInBytes();
	parentSize = parent_ ? parent_->actions_.size() : 0;
	for (uint i = parentSize; i < actions_.size(); i++)
		size += (actions_.begin()+i)->second->sizeInBytes();
#else
	size += actions_.capacity() * sizeof(actions_[0]);
	Actions::const_iterator it2;
	for (it2 = actions_.begin(); it2 != actions_.end(); it2++)
	{
		size += 16;										// map node overhead
		size += sizeof(ActionsIndex::const_iterator);	// map node size

		size += (*it2)->sizeInBytes();
	}
#endif

//#ifdef USE_STRINGMAP
	size += matters_.sizeInBytes();	// we share no matters with our parent
//#endif
	Matters::const_iterator it3;
	for (it3 = matters_.begin(); it3 != matters_.end(); it3++ )
	{
//#ifndef USE_STRINGMAP
//		size += 16;
//		size += sizeof(it3) + it3->first.length();
//#endif
		const Matter & m = it3->second;
		size += sizeof(m);
		size += m.tints_.sizeInBytes();
		size += m.replaces_.length();
		size += contentSize(m.usesOld_);
		size += contentSize(m.usesNew_);
		for (uint j = 0; j < m.tints_.size(); j++)
		{
			const TintPtr t = (m.tints_.begin()+j)->second;
			size -= sizeof(Moo::Material);
			size -= sizeof(Moo::EffectMaterial);
			size += contentSize(t->properties_);
			size += contentSize(t->sourceDyes_);
		}
	}
	return size;
}

/**
 *	This method reloads the referenced model - and returns a new model.
 *	The old model is not deleted, but future calls to the static 'get'
 *	method (and thus future SuperModel instantiations) will return
 *	the new model.
 *
 *	@note The same model can be reloaded more than once, i.e.
 *	this model needn't be the one in the model map used by 'get'
 */
ModelPtr Model::reload( DataSectionPtr pFile /* = NULL */, bool reloadChildren /* = true */ ) const
{
	if (!pFile) pFile = BWResource::openSection( resourceID_ );
	if (!pFile)
	{
		ERROR_MSG( "Model::reload: Could not open model resource %s\n",
			resourceID_.c_str() );
		return NULL;
	}

	PostLoadCallBack::enter();

    // must use a strong ref, or if we have children, the model will be deleted
    // before the end of this method, as at this point the children are the only
	// other thing that has a strong ref to this model.
	ModelPtr mp = Model::load( resourceID_, pFile );

	if (mp)
	{
		// also need a strong ref before we call add
		s_pModelMap->add( &*mp, resourceID_ );
		// intentionally replacing existing entry
	}

	if (reloadChildren)
	{
		// now go through and reload any models that have us as a parent
		std::vector< ModelPtr > children;
		s_pModelMap->findChildren( resourceID_, children );
		// due to the doxygen note above, we can't just see if the parent
		// pointer is the this pointer - because it could be pointing
		// to an intermediate old model. So we compare their resource IDs.

		for (uint i = 0; i < children.size(); i++)
		{
			children[i]->reload();
		}
	}

	PostLoadCallBack::leave();

	return mp;
}

// This factory class is passed to Moo::createModelFromFile()
class ModelFactory
{
	const std::string& 	resourceID_;
	DataSectionPtr& 	pFile_;
public:
	typedef Model ModelBase;

	ModelFactory( const std::string& resourceID, DataSectionPtr& pFile ) :
		resourceID_( resourceID ), pFile_( pFile )
	{}

	NodefullModel* newNodefullModel()
	{
		return new NodefullModel( resourceID_, pFile_ );
	}

	NodelessModel* newNodelessModel()
	{
		return new NodelessModel( resourceID_, pFile_ );
	}

	Model* newBillboardModel()
	{
		return new BillboardModel( resourceID_, pFile_ );
	}
};


/**
 *	This private static method performs operations common to
 *	both the 'get' and 'reload' methods.
 */
Model * Model::load( const std::string & resourceID, DataSectionPtr pFile )
{
	ModelFactory factory( resourceID, pFile );
	std::auto_ptr<Model> mp( Moo::createModelFromFile( pFile, factory ) );

	if (!mp.get())
	{
		ERROR_MSG( "Model::load: Could not determine type of model"
			" in resource %s\n", resourceID.c_str() );
	}

	PostLoadCallBack::run();

	if (mp.get() && !mp->valid())
	{
		// an error will already have been generated if this is the case
		mp.reset();
	}

	return mp.release();
}


/**
 *	This class is an object for calling a model's postLoad function
 */
class ModelPLCB : public PostLoadCallBack
{
public:
	ModelPLCB( Model & m, DataSectionPtr p ) : m_( m ), p_( p ) {};

private:
	virtual void operator()() { m_.postLoad( p_ ); }

	Model & m_;
	DataSectionPtr p_;
};

/**
 *	Constructor
 */
Model::Model( const std::string & resourceID, DataSectionPtr pFile ) :
	resourceID_( resourceID ),
	parent_( NULL ),
	extent_( 0.f ),
	pDefaultAnimation_( NULL )
{
	// read the parent
	std::string parentResID = pFile->readString( "parent" );
	if (!parentResID.empty()) parent_ = Model::get( parentResID + ".model" );

	// read the extent
	extent_ = pFile->readFloat( "extent", 10000.f );

	// copy our parent's stuff
	if (parent_)
	{
		animations_ = parent_->animations_;
#ifdef USE_STRINGMAP
		actions_ = parent_->actions_;
#endif

		Matters & pm = parent_->matters_;
		for (Matters::iterator it = pm.begin(); it != pm.end(); it++)
		{
			matters_.insert(
				std::make_pair( it->first, it->second ) );
			// we don't want its uses because we use a different visual
			(matters_.end()-1)->second.usesOld_.clear();
			(matters_.end()-1)->second.usesNew_.clear();
		}
	}

	// schedule postLoad to be called
	PostLoadCallBack::add( new ModelPLCB( *this, pFile ) );
}


/**
 *	Destructor
 */
Model::~Model()
{
	// subtract the memory we use from the counter
	memoryCounterSub( model );

	memoryClaim( matters_ );
	for (uint i = 0; i < matters_.size(); i++)
	{
		Matter & m = (matters_.begin() + i)->second;
		memoryClaim( m.tints_ );
		for (uint j = 0; j < m.tints_.size(); j++)
		{
			TintPtr t = (m.tints_.begin() + j)->second;
			memoryClaim( t->properties_ );
			memoryClaim( t->sourceDyes_ );
			for (uint k = 0; k < t->sourceDyes_.size(); k++)
			{
				DyeSelection & d = t->sourceDyes_[k];
				memoryClaim( d.matterName_ );
				memoryClaim( d.tintName_ );
				memoryClaim( d.properties_ );
			}
		}
		memoryClaim( m.replaces_ );
		memoryClaim( m.usesOld_ );
		memoryClaim( m.usesNew_ );
	}

#ifndef USE_STRINGMAP

	memoryClaim( actionsIndex_ );
	for (ActionsIndex::iterator it = actionsIndex_.begin();
		it != actionsIndex_.end();
		it++)
	{
		memoryClaim( actionsIndex_, it );
	}
	memoryClaim( actions_ );

	memoryClaim( animations_ );
	for (AnimationsIndex::iterator it = animationsIndex_.begin();
		it != animationsIndex_.end();
		it++)
	{
		memoryClaim( animationsIndex_, it );
		memoryClaim( it->first );
	}
	memoryClaim( animationsIndex_ );

#endif


	memoryClaim( resourceID_ );
	memoryClaim( this );

	// remove ourselves from the map of loaded models
	s_pModelMap->del( resourceID_ );
}


/**
 *	This method loads a model's dyes, if it has them. If multipleMaterials is
 *	true, then it calls the derived model to search for searches for material
 *	names in its bulk. Otherwise the dyes are read as if for a billboard model,
 *	which can have only one material (and thus dyes must each be composited).
 */
int Model::readDyes( DataSectionPtr pFile, bool multipleMaterials )
{
	int dyeProduct = 1;

	// read our own dyes
	std::vector<DataSectionPtr> vDyeSects;
	pFile->openSections( "dye", vDyeSects );
	for (uint i = 0; i < vDyeSects.size(); i++)
	{
		DataSectionPtr pDyeSect = vDyeSects[i];

		std::string matter = pDyeSect->readString( "matter", "" );
		std::string replaces = pDyeSect->readString( "replaces", "" );

		//Ignore any empty "dye" sections
		if ((matter == "") && (replaces == "")) continue;

		// see if we already have a matter of that name
		Matters::iterator miter = matters_.find( matter );
		if (miter == matters_.end())
		{
			miter = matters_.insert(
				std::make_pair( matter.c_str(), Matter() ) );
			miter->second.tints_.insert(
				std::make_pair( static_cast<char*>("Default"), new Tint( true ) ) );
		}

		// set it up
		Matter * m = &miter->second;
		m->replaces_ = replaces;

		// get all instances of replaces from the visual,
		// and set the default tint from the original material
		if (multipleMaterials)	// not for billboards
		{
			if (this->initMatter( *m ) == 0)
			{
				ERROR_MSG( "Model::readDyes: "
					"In model \"%s\", for dye matter \"%s\", "
					"the material id \"%s\" isn't in the visual\n",
					resourceID_.c_str(), matter.c_str(), replaces.c_str() );

				continue;
			}
		}
		else					// for billboards
		{
			miter->second.tints_["Default"]->oldMaterial_->textureFactor( 0 );
		}

		// add the other tints
		std::vector<DataSectionPtr> vTintSects;
		pDyeSect->openSections( "tint", vTintSects );
		for (uint j = 0; j < vTintSects.size(); j++)
		{
			DataSectionPtr pTintSect = vTintSects[j];

			std::string tintName = pTintSect->readString( "name", "" );
			if (tintName.empty())
			{
				ERROR_MSG( "Model::readDyes: "
					"In model %s, for dye matter %s, "
					"tint section index %d has no name.\n",
					resourceID_.c_str(), matter.c_str(), j );
				continue;
			}

			// if it's already there then use that one
			Tints::iterator titer = m->tints_.find( tintName );
			int tindex = titer - m->tints_.begin();	// ok for 'end'

			if (titer == m->tints_.end())
			{		// otherwise make a new one
				titer = m->tints_.insert(
					std::make_pair( tintName.c_str(), new Tint() ) );
			}

			TintPtr t = titer->second;

			// if it's not a billboard...
			if (multipleMaterials)
			{
				// clear source dyes that we don't use
				t->sourceDyes_.clear();

				// read the material
				DataSectionPtr matSect = pTintSect->openSection( "material" );
				if (!this->initTint( *t, matSect ))
				{
					WARNING_MSG( "Model::readDyes: "
						"In model %s, for dye matter %s tint %s, "
						"there %s <material> section.\n",
						resourceID_.c_str(), matter.c_str(), tintName.c_str(),
						(!!matSect) ? "was a problem loading the" : "is no" );
				}

				// read the properties
				std::vector<DataSectionPtr> vPropSects;
				pTintSect->openSections( "property", vPropSects );
				for (uint k = 0; k < vPropSects.size(); k++)
				{
					DataSectionPtr pPropSect = vPropSects[k];

					std::string propName = pPropSect->readString( "name", "" );
					if (propName.empty())
					{
						ERROR_MSG( "Model::readDyes: "
							"In model %s, for dye matter %s tint %s, "
							"property section index %d has no name.\n",
							resourceID_.c_str(), matter.c_str(),
							tintName.c_str(), k );
						continue;
					}

					s_propCatalogueLock_.grab();
					std::stringstream catalogueName;
					catalogueName << matter << "." << tintName << "." << propName;
					PropCatalogue::iterator pit =
						s_propCatalogue_.find( catalogueName.str().c_str() );
					if (pit == s_propCatalogue_.end())
					{
						{
							memoryCounterSub( modelGlobl );
							memoryClaim( s_propCatalogue_ );
						}
						pit = s_propCatalogue_.insert(
							std::make_pair( catalogueName.str().c_str(), Vector4() ) );
						{
							memoryCounterAdd( modelGlobl );
							memoryClaim( s_propCatalogue_ );
						}
					}

					DyeProperty dp;
					dp.index_ = pit - s_propCatalogue_.begin();
					s_propCatalogueLock_.give();
					dp.controls_ = pPropSect->readInt( "controls", 0 );
					dp.mask_ = pPropSect->readInt( "mask", 0 );
					dp.future_ = pPropSect->readInt( "future", 0 );
					dp.default_ = pPropSect->readVector4( "default",
						Vector4(0,0,0,0) );

					if (dp.mask_ == -1) dp.mask_ = 0;
					// look for a corresponding D3DXHANDLE
					// (if newMaterial is NULL, won't have any props anyway)
					if (t->newMaterial_->nEffects()>0)
					{
						D3DXHANDLE propH;
						for (uint32 i=0; i<t->newMaterial_->nEffects(); i++)
						{
							ID3DXEffect* effect = t->newMaterial_->pEffect(i)->pEffect();

							propH = effect->GetParameterByName( NULL, propName.c_str() );
							if (propH != NULL)
							{
								Moo::EffectPropertyFunctor * factory = Moo::g_effectPropertyProcessors.find( "Vector4" )->second;
								Moo::EffectPropertyPtr modifiableProperty = factory->create( propH, effect );
								modifiableProperty->be( dp.default_ );
								t->newMaterial_->replaceProperty( propName, modifiableProperty );

								dp.controls_ = (int)propH;
								dp.mask_ = -1;
								break;
							}
						}
					}

					uint l;
					for (l = 0; l < t->properties_.size(); l++)
					{
						if (t->properties_[l].index_ == dp.index_) break;
					}
					if (l < t->properties_.size())
						t->properties_[l] = dp;
					else
						t->properties_.push_back( dp );

					// end of properties
				}
			}
			// if it's a billboard...
			else
			{
				// clear some things
				if ( t->oldMaterial_ == NULL )
				{
					t->oldMaterial_ = new Moo::Material();
				}
				t->properties_.clear();
				t->sourceDyes_.clear();

				// set the dye combo index component into the
				//  alpha reference of the tint material
				t->oldMaterial_->textureFactor( dyeProduct * tindex );

				// read the source dye selections
				std::vector<DataSectionPtr> vSourceDyes;
				pTintSect->openSections( "dye", vSourceDyes );
				for (uint k = 0; k < vSourceDyes.size(); k++)
				{
					DataSectionPtr pSourceDye = vSourceDyes[k];
					DyeSelection dsel;

					if (!this->readSourceDye( pSourceDye, dsel ))
					{
						ERROR_MSG( "Model::readDyes: "
							"In model %s, for dye matter %s tint %s, "
							"source dye selection section index %d is missing "
							"either a matter ('%s') or tint ('%s') name.\n",
							resourceID_.c_str(), matter.c_str(),
							tintName.c_str(), k,
							dsel.matterName_.c_str(), dsel.tintName_.c_str() );
					}

					t->sourceDyes_.push_back( dsel );
				}


			}

			// end of tints
		}

		// keep a running total of the dye product
		dyeProduct *= m->tints_.size();

		MF_ASSERT( dyeProduct );

		// end of matters (dyes)
	}

	// end of method
	return dyeProduct;
}


/**
 *	Init the material and its default tint and return how many we found of it.
 */
static int initMatter_NewVisual( Model::Matter & m, Moo::Visual & bulk )
{
	const Moo::EffectMaterial * pOriginal = NULL;
	std::vector< Moo::EffectMaterial ** > uses;
	int num = bulk.gatherMaterials( m.replaces_, uses, &pOriginal );
	if (num == 0) return num;

	m.usesNew_ = uses;
	MF_ASSERT( pOriginal != NULL );
	m.tints_["Default"]->newMaterial_ = const_cast<Moo::EffectMaterial *>(pOriginal);
	return num;
}

/**
 *	Init the tint from the given material section, return true if ok.
 *	Should always leave the Tint in a good state.
 */
static bool initTint_NewVisual( Model::Tint & t, DataSectionPtr matSect )
{
	if (t.newMaterial_ == NULL)
	{
		t.newMaterial_ = new Moo::EffectMaterial();
	}
	if (!matSect) return false;

	return t.newMaterial_->load( matSect );
}

/**
 *	This method reads the source dye described in the given data section
 *	into a DyeSelection object.
 *
 *	@return true for success
 */
bool Model::readSourceDye( DataSectionPtr pSourceDye, DyeSelection & dsel )
{
	for (DataSectionIterator il = pSourceDye->begin();
		il != pSourceDye->end();
		il++)
	{
		const std::string & sName = (*il)->sectionName();
		if (sName == "matter")
			dsel.matterName_ = sName;
		else if (sName == "tint")
			dsel.tintName_ = sName;
		else	// general property
		{
			DyePropSetting	dps;

			s_propCatalogueLock_.grab();
			PropCatalogue::iterator pit =
				s_propCatalogue_.find( sName.c_str() );
			if (pit == s_propCatalogue_.end())
			{
				{
					memoryCounterSub( modelGlobl );
					memoryClaim( s_propCatalogue_ );
				}
				pit = s_propCatalogue_.insert(
					std::make_pair( sName.c_str(), Vector4() ) );
				{
					memoryCounterAdd( modelGlobl );
					memoryClaim( s_propCatalogue_ );
				}
			}

			dps.index_ = pit - s_propCatalogue_.begin();
			s_propCatalogueLock_.give();
			dps.value_ = (*il)->asVector4();
			dsel.properties_.push_back( dps );
		}
	}

	return !(dsel.matterName_.empty() || dsel.tintName_.empty());
}


/**
 *	This method loads stuff that is common to all model types but must
 *	be done after the rest of the model has been loaded - i.e. actions.
 */
void Model::postLoad( DataSectionPtr pFile )
{
	// load the actions
	std::vector<DataSectionPtr> vActionSects;
	pFile->openSections( "action", vActionSects );
	for (uint i = 0; i < vActionSects.size(); i++)
	{
		DataSectionPtr pActionSect = vActionSects[i];

		Action * pAction = new Action( pActionSect );

		if (pAction->name_.empty())
		{
			delete pAction;
			continue;
		}

#ifdef USE_STRINGMAP
		actions_.insert( std::make_pair( pAction->name_.c_str(), pAction ) );
#else
		actionsIndex_.insert(
			std::make_pair( &pAction->name_, actions_.size() ) );
		actions_.push_back( pAction );
#endif
	}

	memoryCounterAdd( model );
	memoryClaim( this );
	memoryClaim( resourceID_ );

	memoryClaim( animations_ );
	memoryClaim( actions_ );

#ifndef USE_STRINGMAP
	for (AnimationsIndex::iterator it = animationsIndex_.begin();
		it != animationsIndex_.end();
		it++)
	{
		memoryClaim( animationsIndex_, it );
		memoryClaim( it->first );
	}
	memoryClaim( animationsIndex_ );


	for (ActionsIndex::iterator it = actionsIndex_.begin();
		it != actionsIndex_.end();
		it++)
	{
		memoryClaim( actionsIndex_, it );
	}
	memoryClaim( actionsIndex_ );
#endif

	memoryClaim( matters_ );
	for (uint i = 0; i < matters_.size(); i++)
	{
		Matter & m = (matters_.begin() + i)->second;
		memoryClaim( m.tints_ );
		for (uint j = 0; j < m.tints_.size(); j++)
		{
			TintPtr t = (m.tints_.begin() + j)->second;
			memoryClaim( t->properties_ );
			memoryClaim( t->sourceDyes_ );
			for (uint k = 0; k < t->sourceDyes_.size(); k++)
			{
				DyeSelection & d = t->sourceDyes_[k];
				memoryClaim( d.matterName_ );
				memoryClaim( d.tintName_ );
				memoryClaim( d.properties_ );
			}
		}
		memoryClaim( m.replaces_ );
		memoryClaim( m.usesOld_ );
		memoryClaim( m.usesNew_ );
	}
}



/**
 *	This method soaks the Model's materials in the currently emulsified dyes,
 *	or the default for each matter if it has no emulsion. It is a utility
 *	method called by models that use multiple materials to implement their dyes.
 */
void Model::soakDyes()
{
	for (Matters::iterator it = matters_.begin(); it != matters_.end(); it++)
	{
		it->second.soak();
	}
}


/**
 *	Static method to find or add a node in the Model node catalogue
 */
Moo::Node * Model::findOrAddNodeInCatalogue( Moo::NodePtr pNode )
{
	Moo::Node * pStorageNode = NULL;

	SimpleMutexHolder smh( s_nodeCatalogueLock_ );

	NodeCatalogue::iterator found =
		s_nodeCatalogue_.find( pNode->identifier() );
	if (found != s_nodeCatalogue_.end())
	{
		pStorageNode = found->second.getObject();
	}
	else
	{
		pStorageNode = pNode.getObject();

		{
			memoryCounterSub( modelGlobl );
			memoryClaim( s_nodeCatalogue_ );
		}
		s_nodeCatalogue_.insert( std::make_pair(
			pNode->identifier(),
			pNode ) );
		{
			memoryCounterAdd( modelGlobl );
			memoryClaim( s_nodeCatalogue_ );
		}
	}

	return pStorageNode;
}

/**
 *	Static method to find a node by name in the Model node catalogue
 */
Moo::NodePtr Model::findNodeInCatalogue( const char * identifier )
{
	SimpleMutexHolder smh( s_nodeCatalogueLock_ );

	NodeCatalogue::iterator found = s_nodeCatalogue_.find( identifier );

	if (found != s_nodeCatalogue_.end()) return found->second;

	return NULL;
}



/**
 *	This method gets the index of the input animation name in this model's
 *	table. The index is also valid for all the ancestors and descendants
 *	of this model.
 *
 *	@return the index of the animation, or -1 if not found
 */
int Model::getAnimation( const std::string & name )
{
#ifdef USE_STRINGMAP
	Animations::iterator found = animations_.find( name );
	return (found != animations_.end()) ? found - animations_.begin() : -1;
#else
	for (Model * pModel = this; pModel != NULL; pModel = &*pModel->parent_)
	{
		AnimationsIndex::iterator found =
			pModel->animationsIndex_.find( name );
		if (found != pModel->animationsIndex_.end())
			return found->second;
	}
	return -1;
#endif
}

/**
 *	This method ticks the animation with the given index if it exists.
 */
void Model::tickAnimation( int index, float dtime, float otime, float ntime )
{
	if (uint(index) >= animations_.size()) return;
	SmartPointer<Animation> & anim =
#ifdef USE_STRINGMAP
		(animations_.begin() + index)->second;
#else
		animations_[ index ];
#endif

	anim->tick( dtime, otime, ntime );
}

/**
 *	This method plays the animation with the given index. If the animation
 *	doesn't exist for this model, then the default animation is played instead.
 */
void Model::playAnimation( int index, float time, float blendRatio, int flags )
{
	SmartPointer<Animation> & anim = (uint(index) < animations_.size()) ?
#ifdef USE_STRINGMAP
		(animations_.begin() + index)->second : pDefaultAnimation_;
#else
		animations_[ index ] : pDefaultAnimation_;
#endif

	if (time > anim->duration_) time = anim->looped_ ?
		fmodf( time, anim->duration_ ) : anim->duration_;
	// only not doing this in Animation::play because its virtual.

	anim->play( time, blendRatio, flags );
}


/**
 *	This method gets the animation that is most local to this model
 *	 for the given index. It does not return the default animation
 *	 if the index cannot be found - instead it returns NULL.
 */
Model::Animation * Model::lookupLocalAnimation( int index )
{
	if (index >= 0 && (uint)index < animations_.size())
	{
#ifdef USE_STRINGMAP
		return (animations_.begin() + index)->second.getObject();
#else
		return animations_[ index ].getObject();
#endif
	}

	return NULL;
}


/**
 *	This method gets the pointer to the input action name in this model's
 *	table. Actions only make sense in the context of the most derived model
 *	in a SuperModel (i.e. the top lod one), so we don't have to do any
 *	tricks with indices here. Of course, the animations referenced by these
 *	actions still do this.
 *
 *	@return the action, or NULL if not found
 */
const Model::Action * Model::getAction( const std::string & name ) const
{
#ifdef USE_STRINGMAP
	Actions::const_iterator found = actions_.find( name );
	return (found != actions_.end()) ? found->second.getObject() : NULL;
#else
	for (const Model * pModel = this;
		pModel != NULL;
		pModel = &*pModel->parent_)
	{
		ActionsIndex::const_iterator found =
			pModel->actionsIndex_.find( &name );
		if (found != pModel->actionsIndex_.end())
			return pModel->actions_[ found->second ].getObject();
	}
	return NULL;
#endif
}


/**
 *	This method looks up the local action of the given index
 */
#ifdef USE_STRINGMAP
const Model::Action * Model::lookupLocalAction( int index ) const
{
	if (index >= 0 && (uint)index < actions_.size())
	{
		return (actions_.begin() + index)->second.getObject();
	}

	return NULL;
}
#else
Model::ActionsIterator Model::lookupLocalActionsBegin() const
{
	return ActionsIterator( this );
}

Model::ActionsIterator Model::lookupLocalActionsEnd() const
{
	return ActionsIterator( NULL );
}

Model::ActionsIterator::ActionsIterator( const Model * pModel ) :
	pModel_( pModel )
{
	while (pModel_ != NULL)
	{
		index_ = 0;
		if (pModel_->actions_.size() != 0) break;

		pModel_ = &*pModel_->parent_;
	}
}

void Model::ActionsIterator::operator++( int )
{
	index_++;
	while (index_ == pModel_->actions_.size())
	{
		pModel_ = &*pModel_->parent_;
		if (pModel_ == NULL) break;

		index_ = 0;
	}
}

bool Model::ActionsIterator::operator==( const ActionsIterator & other ) const
{
	return pModel_ == other.pModel_ &&
		(pModel_ == NULL || index_ == other.index_);
}

bool Model::ActionsIterator::operator!=( const ActionsIterator & other ) const
{
	return !this->operator==( other );
}
#endif // USE_STRINGMAP


/**
 *	This static method returns the index of the property with the given
 *	name in the global property catalogue. It returns -1 if a property
 *	of that name does not exist.
 */
int Model::getPropInCatalogue( const char * name )
{
	SimpleMutexHolder smh( s_propCatalogueLock_ );
	PropCatalogue::iterator found = s_propCatalogue_.find( name );
	if (found == s_propCatalogue_.end()) return -1;
	return found - s_propCatalogue_.begin();
}

/**
 *	This static method looks up the name of the given property in the
 *	global property catalogue. There is no method to look up the value
 *	and return a pointer to it as that would not be thread safe.
 *	Returns NULL if the index is out of range.
 */
const char * Model::lookupPropName( int index )
{
	SimpleMutexHolder smh( s_propCatalogueLock_ );
	if (uint(index) >= s_propCatalogue_.size()) return NULL;
	return (s_propCatalogue_.begin() + index)->first;
	// safe to return this pointer as props are never deleted from
	// the catalogue and even if they move around in the tables or
	// vector the string memory stays put.
}


/**
 *	This method gets a (packed) index of the dye formed by the
 *	given matter and tint names. If ppTint is not NULL, and the
 *	dye is found, then the pointer to the tint in the local model
 *	is written into ppTint.
 */
int Model::getDye( const std::string & matter, const std::string & tint,
	Tint ** ppTint )
{
	int matterIndex = -1;
	int tintIndex = -1;

	Matters::iterator miter = matters_.find( matter );
	if (miter != matters_.end())
	{
		matterIndex = miter - matters_.begin();

		Tints & tints = miter->second.tints_;
		Tints::iterator titer = tints.find( tint );
		if (titer != tints.end())
		{
			tintIndex = titer - tints.begin();
			if (ppTint != NULL) *ppTint = &*titer->second;
		}
	}

	return (matterIndex << 16) | uint16(tintIndex);
}


/**
 *	This method soaks this model in the given (packed) dye index.
 *	It is a default method for model classes that use multiple materials.
 */
void Model::soak( int dye )
{
	// unpack
	int matter = dye >> 16;
	int tint = uint16( dye );	// 65535 should be big enough for -1

	// look up the matter
	if (uint(matter) >= matters_.size()) return;

	// tell it to tint
	(matters_.begin() + matter)->second.emulsify( tint );
}


/**
 *	This method looks up the local matter of the given index
 */
const std::pair< const char *, Model::Matter > *
	Model::lookupLocalMatter( int index ) const
{
	if (index >= 0 && (uint)index < matters_.size())
	{
		return &*(matters_.begin() + index);
	}

	return NULL;
}


/**
 *	Constructor for Model's Animation
 */
Model::Animation::Animation() : duration_( 0.f ), looped_( false )
{
	memoryCounterAdd( model );
	memoryClaim( this );
}

/**
 *	Destructor for Model's Animation
 */
Model::Animation::~Animation()
{
	memoryCounterSub( model );
	memoryClaim( this );
}

/**
 *	Constructor for Model's Action
 */
Model::Action::Action( DataSectionPtr pSect ) :
	name_( pSect->readString( "name" ) ),
	animation_( pSect->readString( "animation" ) ),
	blendInTime_( pSect->readFloat( "blendInTime", 0.3f ) ),
	blendOutTime_( pSect->readFloat( "blendOutTime", 0.3f ) ),
	filler_( pSect->readBool( "filler", false ) ),
	track_( pSect->readBool( "blended", false ) ? -1 : 0 ),
	isMovement_( pSect->readBool( "isMovement", false ) ),
	isCoordinated_( pSect->readBool( "isCoordinated", false ) ),
	isImpacting_( pSect->readBool( "isImpacting", false ) ),
	isMatchable_( bool(pSect->openSection( "match" )) ),
	matchInfo_( pSect->openSection( "match" ) )
{
	track_ = pSect->readInt( "track", track_ );

	flagSum_ =
		(int(isMovement_)<<0)		|
		(int(isCoordinated_)<<1)	|
		(int(isImpacting_)<<2);

	memoryCounterAdd( model );
	memoryClaim( this );
	memoryClaim( name_ );
	memoryClaim( animation_ );
}

/**
 *	Destructor for Model's Action
 */
Model::Action::~Action()
{
	memoryCounterSub( model );
	memoryClaim( animation_ );
	memoryClaim( name_ );
	memoryClaim( this );
}


/**
 *	Constructor for a Model's Action's MatchInfo
 */
Model::Action::MatchInfo::MatchInfo( DataSectionPtr sect ):
	trigger( false ),
	cancel( true ),
	scalePlaybackSpeed( false ),
	feetFollowDirection( false ),
	oneShot( false ),
	promoteMotion( false )
{
	if (sect)
	{
		trigger = Constraints( false );
		trigger.load( sect->openSection( "trigger" ) );
		cancel = Constraints( true );
		cancel.load( sect->openSection( "cancel" ) );

		scalePlaybackSpeed = sect->readBool( "scalePlaybackSpeed", false );
		feetFollowDirection = sect->readBool( "feetFollowDirection", false );

		oneShot = sect->readBool( "oneShot", false );
		promoteMotion = sect->readBool( "promoteMotion", false );
	}
}


/**
 *	Constructor for a Model's Action's MatchInfo's Constraints (gasp)
 */
Model::Action::MatchInfo::Constraints::Constraints( bool matchAll )
{
	minEntitySpeed = -1000.f;
	maxEntitySpeed = matchAll ? 1000.f : -1.f;
	minEntityAux1 = -MATH_PI;
	maxEntityAux1 = matchAll ? MATH_PI : -10.f;
	minModelYaw = -MATH_PI;
	maxModelYaw = matchAll ? MATH_PI : -10.f;
}


/**
 *	This method loads action constraints
 */
void Model::Action::MatchInfo::Constraints::load( DataSectionPtr sect )
{
	if (!sect) return;

	minEntitySpeed = sect->readFloat( "minEntitySpeed", minEntitySpeed );
	maxEntitySpeed = sect->readFloat( "maxEntitySpeed", maxEntitySpeed );

	const float conv = float(MATH_PI/180.0);

	minEntityAux1 = sect->readFloat( "minEntityAux1", minEntityAux1/conv ) * conv;
	maxEntityAux1 = sect->readFloat( "maxEntityAux1", maxEntityAux1/conv ) * conv;

	minModelYaw = sect->readFloat( "minModelYaw", minModelYaw/conv ) * conv;
	maxModelYaw = sect->readFloat( "maxModelYaw", maxModelYaw/conv ) * conv;

	const char * delim = " ,\t\r\n";
	char * token;
	std::string	caps;

	caps = sect->readString( "capsOn", "" ).c_str();
	capsOn = Capabilities();
	for (token = strtok( const_cast<char*>(
			caps.c_str() ), delim );
		token != NULL;
		token = strtok( NULL, delim ) )
	{
		capsOn.add( atoi( token ) );
	}

	caps = sect->readString( "capsOff", "" ).c_str();
	capsOff = Capabilities();
	for (token = strtok( const_cast<char*>(
			caps.c_str() ), delim );
		token != NULL;
		token = strtok( NULL, delim ) )
	{
		capsOff.add( atoi( token ) );
	}
}

/**
 *	The constructor for the tint
 */
Model::Tint::Tint( bool defaultTint /*= false*/ ):
	oldMaterial_(NULL),
	newMaterial_(NULL),
	default_( defaultTint )
{
}

/**
 *	The destructor for the tint
 */
Model::Tint::~Tint()
{
	if( !default_ )
	{
		delete oldMaterial_;
		delete newMaterial_;
	}
}

/**
 *	Tint the material with the current settings of our properties
 */
void Model::Tint::applyTint()
{
	if (properties_.empty()) return;

	SimpleMutexHolder smh( Model::propCatalogueLock() );

	for (uint i = 0; i < properties_.size(); i++)
	{
		DyeProperty & dp = properties_[i];

		Vector4 & value =
			(Model::propCatalogueRaw().begin() + dp.index_)->second;

		if (dp.mask_ == -1)
		{
			// look for the property in this EffectMaterial...
			for (uint32 i=0; i<newMaterial_->nEffects(); i++)
			{
				Moo::EffectMaterial::Properties & eProps = newMaterial_->properties(i);
				Moo::EffectMaterial::Properties::iterator found =
					eProps.find( (D3DXHANDLE)dp.controls_ );
				// it might not always be there, as this property might
				// be defined in a parent model that uses a different Effect
				if (found != eProps.end())
				{
					// kind of sucks that we're looking it up all the time here too!
					// but this will do for the first version of it anyhow
					found->second->be( value );
					continue;
				}
			}
		}

		// now apply to the field of material_ indicated by
		// dp.controls_ and dp.mask_. Currently assume it's
		// the whole of the texture factor
		switch( dp.controls_ )
		{
		case DyePropSetting::PROP_TEXTURE_FACTOR:
			if (oldMaterial_)
			{
				oldMaterial_->textureFactor( Moo::Colour( value ) );
			}
			break;
		case DyePropSetting::PROP_UV:
			if (oldMaterial_)
			{
				oldMaterial_->uvTransform( value );
			}
			break;
		}
	}
}


/**
 *	Default constructor
 */
Model::Matter::Matter() :
	emulsion_( 0 ),
	emulsionCookie_( Model::s_blendCookie_ - 16 )
{
}

/**
 *	Copy constructor
 */
Model::Matter::Matter( const Matter & other ) :
	tints_( other.tints_ ),
	replaces_( other.replaces_ ),
	usesOld_( other.usesOld_ ),
	usesNew_( other.usesNew_ ),
	emulsion_( 0 ),
	emulsionCookie_( Model::s_blendCookie_ - 16 )
{
}

/**
 *	Assignment operator
 */
Model::Matter & Model::Matter::operator= ( const Model::Matter & other )
{
	tints_ = other.tints_;
	replaces_ = other.replaces_;
	usesOld_ = other.usesOld_;
	usesNew_ = other.usesNew_;
	emulsion_ = 0;
	emulsionCookie_ = Model::s_blendCookie_ - 16;

	return *this;
}

/**
 *	Destructor
 */
Model::Matter::~Matter()
{
}


/**
 *	This method selects the given tint as the one that the model should be
 *	soaked in. It is suspended in an emulsion.
 */
void Model::Matter::emulsify( int tint )
{
	if (uint(tint) >= tints_.size()) tint = 0;	// use default if not found
	emulsion_ = tint;
	emulsionCookie_ = Model::s_blendCookie_;
}

/**
 *	This method soaks the model that this matter lives in with the stored tint,
 *	or the default tint, if there is no emulsion.
 */
void Model::Matter::soak()
{
	int tint = (emulsionCookie_ == Model::s_blendCookie_) ? emulsion_ : 0;

	TintPtr rTint = (tints_.begin() + tint)->second;

	rTint->applyTint();
	for (uint i = 0; i < usesOld_.size(); i++)
	{
		*usesOld_[i] = rTint->oldMaterial_;
	}
	for (uint i = 0; i < usesNew_.size(); i++)
	{
		*usesNew_[i] = rTint->newMaterial_;
	}
}


/**
 *	This is the base class method to get static lighting.
 */
Model::StaticLightingPtr Model::getStaticLighting( const DataSectionPtr section )
{
	return NULL;
}

/**
 *	This is the base class method to set static lighting.
 */
void Model::setStaticLighting( StaticLightingPtr pLighting )
{
	if (pLighting) pLighting->set();
}

/**
 *	Default implementation. Returns empty node tree iterator.
 */
Model::NodeTreeIterator Model::nodeTreeBegin() const
{
	return Model::NodeTreeIterator( NULL, NULL, NULL );
}

/**
 *	Default implementation. Returns empty node tree iterator.
 */
Model::NodeTreeIterator Model::nodeTreeEnd() const
{
	return Model::NodeTreeIterator( NULL, NULL, NULL );
}

/**
 * Constructs a NodeTreeIterator that will traverse the given node tree.
 *
 * @param cur the current element in the node tree.
 * @param end   on past the last element in the node tree.
 * @param root  the world transformation matrix of the root node.
 */
Model::NodeTreeIterator::NodeTreeIterator(
		const NodeTreeData * cur,
		const NodeTreeData * end,
		const Matrix * root ) :
	curNodeData_( cur ),
	endNodeData_( end ),
	curNodeItem_(),
	sat_( 0 )
{
	curNodeItem_.pData = curNodeData_;
	curNodeItem_.pParentTransform = root;

	stack_[sat_].trans = root;
	stack_[sat_].pop = end - cur;
	//while (stack_[sat_].pop-- <= 0) --sat_;
}

/**
 * Returns current item.
 */
const Model::NodeTreeDataItem & Model::NodeTreeIterator::operator * () const
{
	return curNodeItem_;
}

/**
 * Returns current item.
 */
const Model::NodeTreeDataItem * Model::NodeTreeIterator::operator -> () const
{
	return &curNodeItem_;
}

/**
 * Advance to next node in tree. Call eot to check if
 * traversal has ended before accessing any node data.
 */
void Model::NodeTreeIterator::operator ++ ( int )
{
	const NodeTreeData & ntd = *(curNodeItem_.pData);
	if (ntd.nChildren > 0)
	{
		++sat_;
		stack_[sat_].trans = &ntd.pNode->worldTransform();
		stack_[sat_].pop = ntd.nChildren;
	}

	++curNodeData_;
	curNodeItem_.pData = curNodeData_;

	while (stack_[sat_].pop-- <= 0) --sat_;
	curNodeItem_.pParentTransform = stack_[sat_].trans;
}

/**
 * operator equals to
 */
bool Model::NodeTreeIterator::operator == ( const NodeTreeIterator & other ) const
{
	return other.curNodeData_ == this->curNodeData_;
}

/**
 * operator not equals to
 */
bool Model::NodeTreeIterator::operator != ( const NodeTreeIterator & other ) const
{
	return !(other == *this);
}

// ----------------------------------------------------------------------------
// Section: NodefullModel
// ----------------------------------------------------------------------------

//extern uint32 memUsed();
//extern uint32 memoryAccountedFor();

/**
 *	Constructor
 */
NodefullModel::NodefullModel( const std::string & resourceID, DataSectionPtr pFile ) :
	Model( resourceID, pFile ),
	pAnimCache_( NULL ),
	batched_( false )
{
	static std::vector < std::string > s_animWarned;

    // load our bulk
	std::string bulkName = pFile->readString( "nodefullVisual" ) + ".visual";

	// see if we're batching it
#ifndef EDITOR_ENABLED
	batched_ = pFile->readBool( "batched", false );
#endif

	bulk_ = Moo::VisualManager::instance()->get( bulkName );
	if (!bulk_)
	{
		ERROR_MSG( "NodefullModel::NodefullModel: "
			"Could not load visual resource '%s' as main bulk of model %s\n",
			bulkName.c_str(), resourceID_.c_str() );
		return;		// maybe we should load a placeholder
	}

	// Get the visibilty box for the model if it exists,
	// if it doesn't then use the visual's bounding box instead.
	DataSectionPtr bb = pFile->openSection("visibilityBox");
	if (bb)
	{
		Vector3 minBB = bb->readVector3("min", Vector3(0.f,0.f,0.f));
		Vector3 maxBB = bb->readVector3("max", Vector3(0.f,0.f,0.f));
		visibilityBox_ = BoundingBox( minBB, maxBB );
	}
	else
	{
		visibilityBox_ = bulk_->boundingBox();
	}

	std::avector<Matrix>	initialPose;

	// add any nodes found in our visual to the node table,
	// and build the node index tree at the same time
	std::vector<Moo::NodePtr>	stack;
	stack.push_back( bulk_->rootNode() );
	while (!stack.empty())
	{
		Moo::NodePtr pNode = stack.back();
		stack.pop_back();

		// save this node's initial transform
		initialPose.push_back( pNode->transform() );

		// add this node to our table, if it's not already there
		Moo::Node * pStorageNode =
			Model::findOrAddNodeInCatalogue( pNode );

		// do the same for the children
		for (int i = pNode->nChildren() - 1; i >= 0 ; i--)
		{
			stack.push_back( pNode->child( i ) );
		}

		// and fill in the tree
		nodeTree_.push_back( NodeTreeData( pStorageNode, pNode->nChildren() ) );
	}

	// let the visual know what the table looks like, so it can update
	//  its rendersets' node pointers to point to our nodes.
	// when we draw the visual, the nodes will already have been traversed
	//  (incorporating the effect of our super-hierarchy), so the modified
	//  draw function doesn't try to traverse them at all (i.e. it only
	//  needs their transforms, as we've taken over the hierarchy part)
	s_nodeCatalogueLock_.grab();
	bulk_->useExistingNodes( s_nodeCatalogue_ );
	s_nodeCatalogueLock_.give();

	/****	This note is no longer relevant after the shift to a
			global node table. But I'll for now as a reminder to
			the terrible mess I almost got us all into.
	// NOTE: The function above should check that its render sets haven't
	// already been told to use a different node table - i.e. that we
	// have exclusive access (at least among supermodels) to that visual.
	// If it is not the case, then it should duplicate itself (it can
	// share indices and vertices?) because the same visual is being used
	// for a different function in the model hierarchy (unlikely, but possible)
	****/

	// load dyes
	this->readDyes( pFile, true );

	// add the default 'initial pose' animation
	pDefaultAnimation_ = new DefaultAnimation( nodeTree_, 1, initialPose );
	// TODO: Figure out what the itinerant node index really is instead
	//  of just assuming that it's 1 (maybe get from an anim?). It is also
	//  possible that some visuals may not have an itinerant node at all.

	// read in the sections describing our animations
	std::vector<DataSectionPtr>	vAnimSects;
	pFile->openSections( "animation", vAnimSects );

	// set up an animation cache for this model, if we have any animations
	if (!vAnimSects.empty())
	{
		std::string animCacheName = resourceID;
		if (animCacheName.length() > 5)
			animCacheName.replace( animCacheName.length()-5, 5, "anca" );
		else
			animCacheName.append( ".anca" );
		pAnimCache_ = new Moo::StreamedDataCache( animCacheName, true );

		for( uint i = 0; i < vAnimSects.size(); ++i )
		{
			DataSectionPtr pAnimData;
			if( !( pAnimData = vAnimSects[ i ]->openSection( "nodes" ) ) )
				continue;// we'll revenge later

			std::string nodesRes = pAnimData->asString() + ".animation";
			uint64 modifiedTime = BWResource::modifiedTime( nodesRes );

			if( pAnimCache_->findEntryData( nodesRes, 0, modifiedTime ) == NULL )
			{
				pAnimCache_->deleteOnClose( true );
				delete pAnimCache_;
				pAnimCache_ = new Moo::StreamedDataCache( animCacheName, true );
				break;
			}
		}

#ifndef EDITOR_ENABLED
		Moo::StreamedAnimation::s_loadingAnimCache_ = pAnimCache_;
		Moo::StreamedAnimation::s_savingAnimCache_ = pAnimCache_;
#endif
	}

	// load our animations in the context of the global node table.
	for (uint i = 0; i < vAnimSects.size(); i++)
	{
		DataSectionPtr pAnimSect = vAnimSects[i];

		// read the name
		std::string animName = pAnimSect->readString( "name" );

		// now read the node stuff
		DataSectionPtr pAnimData;
		if (!(pAnimData = pAnimSect->openSection( "nodes" )))
		{
			ERROR_MSG( "Animation %s (section %d) in model %s has no 'nodes' section\n",
				animName.c_str(), i, resourceID.c_str() );
			continue;
		}

		std::string nodesRes = pAnimData->asString();

		if (animName.empty())
		{
        	const char * nameBeg = nodesRes.data() +
				nodesRes.find_last_of( '/' )+1;
        	const char * nameEnd = nodesRes.data() +
				nodesRes.length();
                animName.assign( nameBeg, nameEnd );
		}

		if (animName.empty())
		{
			ERROR_MSG( "Animation section %d in model %s has no name\n",
				i, resourceID.c_str() );
			continue;
		}

		// ok, now we know where to get it and what to call it,
		//  let's load the actual animation!
		nodesRes += ".animation";
		Moo::AnimationPtr pAnim = Moo::AnimationManager::instance().get(
			nodesRes, s_nodeCatalogue_, s_nodeCatalogueLock_ );

		// Make sure the animation is added to the current anca file 
		// in the case that it was already loaded.
#ifndef EDITOR_ENABLED
		uint64 modifiedTime = BWResource::modifiedTime( nodesRes );
		if( !pAnimCache_->findEntryData( nodesRes, 0, modifiedTime ) )
		{
			// this ultimately causes a save
			pAnim->load( nodesRes );
		}
#endif //EDITOR_ENABLED

		//static int omu = memUsed() - memoryAccountedFor();
		//int nmu = memUsed() - memoryAccountedFor();
		//dprintf( "Using %d more, %d total after %s\n", nmu-omu, nmu, nodesRes.c_str() );
		//omu = nmu;

#ifdef EDITOR_ENABLED
		if (s_pAnimLoadCallback_)
			s_pAnimLoadCallback_->execute();	
#endif

        if (!pAnim)
		{
			if (std::find( s_animWarned.begin(), s_animWarned.end(), nodesRes + animName + resourceID ) ==  s_animWarned.end())
            {
				std::string modelName = BWResource::getFilename( resourceID );
				ERROR_MSG( "Could not load the animation:\n\n"
					"  \"%s\"\n\n"
					"for \"%s\" of \"%s\".\n\n"
					"Please make sure this file exists or set a different one using ModelEditor.",
					nodesRes.c_str(), animName.c_str(), modelName.c_str() );
				s_animWarned.push_back( nodesRes + animName + resourceID );
            }
			continue;
		}

#ifdef USE_STRINGMAP
		animations_.insert( std::make_pair(
			animName.c_str(), new Animation(
				*this, pAnim, pAnimSect ) ) );
#else
		Animation * pNewAnim = new Animation( *this, pAnim, pAnimSect );
		int existingIndex = this->getAnimation( animName );
		if (existingIndex == -1)
		{
			animationsIndex_.insert(
				std::make_pair( animName, animations_.size() ) );
			animations_.push_back( pNewAnim );
		}
		else
		{
			animations_[ existingIndex ] = pNewAnim;
		}
#endif
	}

	// reset the static animation cache pointers
	Moo::StreamedAnimation::s_loadingAnimCache_ = NULL;
	Moo::StreamedAnimation::s_savingAnimCache_ = NULL;

	// delete it if it's empty
	if (pAnimCache_ && pAnimCache_->numEntries() == 0)
	{
		pAnimCache_->deleteOnClose( true );
		delete pAnimCache_;
		pAnimCache_ = NULL;
	}

	memoryCounterAdd( model );
	memoryClaim( nodeTree_ );
}

/**
 *	Destructor
 */
NodefullModel::~NodefullModel()
{
	memoryCounterSub( model );
	memoryClaim( nodeTree_ );

	if (pAnimCache_ != NULL)
		delete pAnimCache_;
}


/**
 *	This method traverses this nodefull model, and soaks its dyes
 */
void NodefullModel::dress()
{
	// first animate
	Matrix * initialPose = &*static_cast<DefaultAnimation&>
		(*pDefaultAnimation_).cTransforms().begin();

	world_ = Moo::rc().world();

	static struct TravInfo
	{
		const Matrix	* trans;
		int					pop;
	} stack[128];
	int			sat = 0;

	// start with the root node
	stack[sat].trans = &world_;
	stack[sat].pop = nodeTree_.size();
	for (uint i = 0; i < nodeTree_.size(); i++)
	{
		// pop off any nodes we're through with
		while (stack[sat].pop-- <= 0) --sat;

		NodeTreeData & ntd = nodeTree_[i];

		// make sure it's valid for this supermodel
		if (ntd.pNode->blend( s_blendCookie_ ) == 0)
			ntd.pNode->blendClobber( s_blendCookie_, initialPose[i] );

		// calculate this one
		ntd.pNode->visitSelf( *stack[sat].trans );

		// and visit its children if it has any
		if (ntd.nChildren > 0)
		{
			++sat;
			stack[sat].trans = &ntd.pNode->worldTransform();
			stack[sat].pop = ntd.nChildren;
		}
	}
}



/**
 *	This method draws this nodefull model.
 */
void NodefullModel::draw( bool checkBB )
{
	Moo::rc().push();
	Moo::rc().world( world_ );
#ifndef EDITOR_ENABLED
	if (!batched_)
		bulk_->draw( !checkBB );
	else
		bulk_->batch( !checkBB );
#else
	bulk_->draw( !checkBB );
#endif

#ifdef DEBUG_NORMALS
	if (draw_norms)
	{
		const Moo::Visual::RenderSetVector& set = bulk_->renderSets();
		Moo::Visual::RenderSetVector::const_iterator it = set.begin();
		for (;it != set.end(); it++)
		{
			Moo::Visual::GeometryVector::const_iterator g_it = (*it).geometry_.begin();
			//for (uint x=0; x<(*it).geometry_.size(); x++)
			{
				//Moo::Visual::GeometryVector::const_iterator g_it = (*it).geometry_.begin();
				//const Moo::Visual::Geometry& geometry = (*it).geometry_[x];

				const Moo::Visual::Geometry& geometry = *g_it;
				const Moo::Vertices::VertexPositions &points = geometry.vertices_->vertexPositions();
				const Moo::Vertices::VertexNormals &tangentspace = geometry.vertices_->vertexNormals();

				const Moo::Vertices::VertexPositions &normals = geometry.vertices_->vertexNormals2();
				
				const Moo::Vertices::VertexNormals &packedNormals = geometry.vertices_->vertexNormals3();

				Moo::Vertices::VertexNormals::const_iterator n_it = tangentspace.begin();
				Moo::Vertices::VertexPositions::const_iterator n2_it = normals.begin();
				Moo::Vertices::VertexNormals::const_iterator n3_it = packedNormals.begin();

				bool drawTS = tangentspace.size() > 0;
				bool packed = packedNormals.size() > 0;
				bool regular = normals.size() > 0;
				//if (tangentspace.size())
				{
					Moo::Vertices::VertexPositions::const_iterator p_it = points.begin();
					//for (uint i = 0; i < points.size(); i++)
					for (;p_it != points.end(); p_it++)
					{						
						Moo::Colour ncol(1.f,1.f,0.f,1.f);
						Moo::Colour tcol(1.f,0.f,0.f,1.f);
						Moo::Colour bcol(0.f,0.f,1.f,1.f);

						//const Vector3& point = points[i];
						const Vector3& point = (*p_it);
						

						//Geometrics::drawLine( point, (point + Vector3(50,0,0)), col );

						Vector3 start = point;
						Vector3 normal;

						if (drawTS)
						{
							uint32 intNormal = (*n_it);
							normal = Moo::unpackNormal(intNormal);
							n_it++;
						}
						else if (packed)
						{
							uint32 intNormal = (*n3_it);
							normal = Moo::unpackNormal(intNormal);
							n3_it++;
						}
						else if (regular)
						{	
							normal = (*n2_it);
							n2_it++;
						}


						Vector3 end = point + normal*scale_norms;
						Geometrics::drawLine( start, end, ncol, false );


						if (drawTS)
						{

							uint32 intTangent = (*n_it);
							Vector3 tangent = Moo::unpackNormal(intTangent);
							n_it++;

							uint32 intBinormal = (*n_it);
							Vector3 binormal = Moo::unpackNormal(intBinormal);
							n_it++;
						
							end = point + tangent*scale_norms;
							Geometrics::drawLine( start, end, tcol, false );
							end = point + binormal*scale_norms;
							Geometrics::drawLine( start, end, bcol, false );
						}


					}
				}
			}
		}
	}
#endif //DEBUG_NORMALS

	Moo::rc().pop();
}


/**
 *	This method returns a BSPTree of this model when decomposed
 */
const BSPTree * NodefullModel::decompose() const
{
	return bulk_ ? bulk_->pBSPTree() : NULL;
}


/**
 *	This method returns the bounding box of this model
 */
const BoundingBox & NodefullModel::boundingBox() const
{
	if (!bulk_) return s_emptyBoundingBox;
	return bulk_->boundingBox();
}

/**
 *	This method returns the bounding box of this model
 */
const BoundingBox & NodefullModel::visibilityBox() const
{
	return visibilityBox_;
}

/**
 *  Returns note tree iterator pointing to first node in tree.
 */
Model::NodeTreeIterator NodefullModel::nodeTreeBegin() const
{
	const int size = nodeTree_.size();
	return size > 0
		? Model::NodeTreeIterator( &nodeTree_[0], (&nodeTree_[size-1])+1, &world_ )
		: Model::NodeTreeIterator( 0, 0, 0 );
}

/**
 *  Returns note tree iterator pointing to one past last node in tree.
 */
Model::NodeTreeIterator NodefullModel::nodeTreeEnd() const
{
	const int size = nodeTree_.size();
	return size > 0
		? Model::NodeTreeIterator( &nodeTree_[size-1]+1, (&nodeTree_[size-1])+1, &world_ )
		: Model::NodeTreeIterator( 0, 0, 0 );
}

/**
 *  override all materials with the same identifier, then return a MaterialOverride to make the action recoverable
 */
Model::MaterialOverride NodefullModel::overrideMaterial( const std::string& identifier, Moo::EffectMaterial* material )
{
	if( materialOverrides_.find( identifier ) == materialOverrides_.end() )
	{// create a new one
		MaterialOverride mo;
		mo.identifier_ = identifier;
		bulk_->gatherMaterials( identifier, mo.effectiveMaterials_, NULL );
		materialOverrides_[ identifier ] = mo;
	}
	materialOverrides_[ identifier ].update( material );
//	bulk_->overrideMaterial( identifier, *material );
	return materialOverrides_[ identifier ];
}

void Model::MaterialOverride::update( Moo::EffectMaterial* newMat )
{
	savedMaterials_.resize( effectiveMaterials_.size() );
	std::vector< Moo::EffectMaterial** >::iterator iter = effectiveMaterials_.begin();
	std::vector< Moo::EffectMaterial* >::iterator saveIter = savedMaterials_.begin();
	while( iter != effectiveMaterials_.end() )
	{
		*saveIter = **iter;
		**iter = newMat;
		++saveIter;
		++iter;
	}
}

void Model::MaterialOverride::reverse()
{
	if( savedMaterials_.size() )
	{
		std::vector< Moo::EffectMaterial** >::iterator iter = effectiveMaterials_.begin();
		std::vector< Moo::EffectMaterial* >::iterator saveIter = savedMaterials_.begin();
		while( iter != effectiveMaterials_.end() )
		{
			**iter = *saveIter;
			++saveIter;
			++iter;
		}
	}
}

/**
 *	This method returns whether or not the model has this node
 */
bool NodefullModel::hasNode( Moo::Node * pNode,
	MooNodeChain * pParentChain ) const
{
	MF_ASSERT( pNode != NULL );

	if (pParentChain == NULL || nodeTree_.empty())
	{
		for (NodeTree::const_iterator it = nodeTree_.begin();
			it != nodeTree_.end();
			it++)
		{
			if (it->pNode == pNode) return true;
		}
	}
	else
	{
		static VectorNoDestructor<NodeTreeData> stack;
		stack.clear();

		stack.push_back( NodeTreeData( NULL, 1 ) );	// dummy parent for root
		for (uint i = 0; i < nodeTree_.size(); i++)
		{
			// pop off any nodes we're through with
			while (stack.back().nChildren-- <= 0) stack.pop_back();

			// see if this is the one
			const NodeTreeData & ntd = nodeTree_[i];
			if (ntd.pNode == pNode)
			{
				pParentChain->clear();
				for (uint j = 0; j < stack.size(); j++)
					pParentChain->push_back( stack[i].pNode );
				return true;
			}

			// and visit its children if it has any
			if (ntd.nChildren > 0) stack.push_back( ntd );
		}
	}

	return false;
}



/**
 *	Constructor for NodefullModel's Animations
 */
NodefullModel::Animation::Animation( NodefullModel & rOwner,
		Moo::AnimationPtr anim, DataSectionPtr pAnimSect ) :
	rOwner_( rOwner ),
	anim_( anim ),
	firstFrame_( pAnimSect->readInt( "firstFrame", 0 ) ),
	lastFrame_( pAnimSect->readInt( "lastFrame", -1 ) ),
	frameRate_( pAnimSect->readFloat( "frameRate", 30.f ) ),
	alphas_( NULL ),
	cognate_( pAnimSect->readString( "cognate", "" ) )
{
	// load and check standard stuff
	int rawFrames = int(anim->totalTime());
	if (firstFrame_ < 0) firstFrame_ = rawFrames + firstFrame_ + 1;
	if (lastFrame_ < 0) lastFrame_  = rawFrames + lastFrame_ + 1;
	firstFrame_ = Math::clamp( 0, firstFrame_, rawFrames );
	lastFrame_ = Math::clamp( 0, lastFrame_, rawFrames );

	// calculate number of frames and duration
	frameCount_ = abs( lastFrame_ - firstFrame_ );
	duration_ = float( frameCount_ ) / frameRate_;
	looped_ = (duration_ > 0.f);	// zero-length => not looped

	memoryCounterAdd( model );
	memoryClaim( cognate_ );

	// Our animations now do node alpha stuff, because it makes
	// more sense for a node related thing to be in animations.
	// And only only ours do it because only ours have nodes.
	// This does mean that none of this stuff is inherited by overriding
	//  animations. But I think that's something we'll be able to live with.
	StringMap<float>	alphaNames;
	DataSectionPtr	aSection = pAnimSect->openSection( "alpha" );
	if (aSection)
	{
		for (DataSectionIterator iter = aSection->begin();
			iter != aSection->end();
			iter++)
		{
			DataSectionPtr pDS = *iter;

			// Need to unsanitise the section names
			std::string id = SanitiseHelper::substringReplace
				(
					pDS->sectionName(),
					SanitiseHelper::SANITISING_TOKEN,
					SanitiseHelper::SPACE_TOKEN
				);

			alphaNames[id.c_str()] = pDS->asFloat( 0.f );
		}
	}

	// if there's no alphas then that's all
	if (alphaNames.size() == 0) return;

	// figure out the alphas from our alphanames
	alphas_ = new Moo::NodeAlphas();	// initialise to fully on
	alphas_->insert( alphas_->end(), anim_->nChannelBinders(), 1.f );

	memoryClaim( alphas_ );
	memoryClaim( *alphas_ );

	// build a map of the nodes in this animation
	std::map<Moo::Node*,int> chanNodes;
	for (uint i=0; i < anim_->nChannelBinders(); i++)
	{
		chanNodes.insert( std::make_pair(
			anim_->channelBinder( i ).node().getObject(), i ) );

		std::string channelID = 
			anim_->channelBinder( i ).channel()->identifier();

		StringMap<float>::iterator it = alphaNames.find( channelID );
		if (it != alphaNames.end())
			(*alphas_)[i] = it->second;
	}

	// now do a standard traversal of our nodes
	static struct AlphaTravInfo
	{
		float	alpha;
		int		pop;
	} stack[128];
	int			sat = 0;

	stack[sat].alpha = 1.f;
	stack[sat].pop = rOwner.nodeTree_.size();
	for (uint i = 0; i < rOwner.nodeTree_.size(); i++)
	{
		while (stack[sat].pop-- <= 0) --sat;

		NodeTreeData & ntd = rOwner.nodeTree_[i];

		// find the alpha of this node
		StringMap<float>::iterator aiter =
			alphaNames.find( ntd.pNode->identifier().c_str() );
		float nalpha = aiter == alphaNames.end() ?
			stack[sat].alpha : aiter->second;

		// set it in the alpha vector if the anim has this node
		std::map<Moo::Node*,int>::iterator citer = chanNodes.find( ntd.pNode );
		if (citer != chanNodes.end()) (*alphas_)[ citer->second ] = nalpha;

		// and visit its children if it has any
		if (ntd.nChildren > 0)
		{
			++sat;
			stack[sat].alpha = nalpha;
			stack[sat].pop = ntd.nChildren;
		}
	}
}


/**
 *	Destructor
 */
NodefullModel::Animation::~Animation()
{
	memoryCounterSub( model );

	if (alphas_ != NULL)
	{
		memoryClaim( *alphas_ );
		memoryClaim( alphas_ );
		delete alphas_;
	}

	memoryClaim( cognate_ );
}


/**
 *	This method ticks the animation for the given time interval.
 */
void NodefullModel::Animation::tick( float dtime, float otime, float ntime )
{
	otime *= frameRate_;
	ntime *= frameRate_;
	if (firstFrame_ <= lastFrame_)
	{
		otime = firstFrame_ + otime;
		ntime = firstFrame_ + ntime;
	}
	else
	{
		otime = firstFrame_ - otime;
		ntime = firstFrame_ - ntime;
	}

	anim_->tick( dtime, otime, ntime, float(firstFrame_), float(lastFrame_) );
}


/**
 *	This method applies the animation with the given parameters
 *	to the global node table. If flags are positive, then the first
 *	bit indicates whether or not movement should be taken out of the
 *	animation, and the second bit indicates whether or not it should
 *	be played as a coordinated animation.
 */
void NodefullModel::Animation::play( float time, float blendRatio, int flags )
{
	Moo::ChannelBinder * pIR;

	float frame = time * frameRate_;
	frame = firstFrame_ <= lastFrame_ ?
		firstFrame_ + frame : firstFrame_ - frame;

	if (flags > 0)
	{
		flags &= 7;
		pIR = anim_->itinerantRoot();
		if (!alternateItinerants_[flags]) this->precalcItinerant( flags );
		pIR->channel( alternateItinerants_[flags] );
	}

	anim_->animate( s_blendCookie_, frame, blendRatio, alphas_ );

	if (flags > 0)
	{
		pIR->channel( alternateItinerants_[0] );
	}
}


/**
 *	This method precalculates the appropriate itinerant channel
 */
void NodefullModel::Animation::precalcItinerant( int flags )
{
	if (!flags) return;

	Moo::ChannelBinder * pIR = anim_->itinerantRoot();

	alternateItinerants_[0] = pIR->channel();

	Moo::AnimationChannel * pNewAC = NULL;

	// make a copy of the current channel
	pNewAC = pIR->channel()->duplicate();

	// apply the changes indicated by 'flags' to it
	if (pNewAC != NULL)
	{
		if (flags & 1)    // isMovement
		{
			Matrix first; pIR->channel()->result( float(firstFrame_),	first );
			Matrix final; pIR->channel()->result( float(lastFrame_),	final );

			final.invert();
			flagFactors_[0].multiply( final, first );

			SmartPointer<Moo::InterpolatedAnimationChannel> motion =
					new Moo::InterpolatedAnimationChannel();
				motion->addKey( float(firstFrame_), Matrix::identity );
				motion->addKey( float(lastFrame_),	flagFactors_[0] );
				motion->finalise();

			pNewAC->postCombine( *motion );
		}

		if (flags & 2)	// isCoordinated
		{
			// ok, what's the normal transform for this node
			//Matrix	originT = pIR->node()->transform();
			int cogIdx = rOwner_.getAnimation( cognate_ );
			Model::Animation * pCog = cogIdx < 0 ?
				rOwner_.lookupLocalDefaultAnimation() :
				rOwner_.lookupLocalAnimation( cogIdx );
			Matrix originT;
			pCog->flagFactor( -1, originT );

//			Vector3 oTo = originT.applyToOrigin();
//			Quaternion oTq = Quaternion( originT );
//			dprintf( "desired translation is (%f,%f,%f) q (%f,%f,%f,%f)\n",
//				oTo[0], oTo[1], oTo[2], oTq[0], oTq[1], oTq[2], oTq[3] );

			// and what's the transform used in this animation
			Matrix	offsetT; pIR->channel()->result( 0, offsetT );

			// find the transform by which offsetT should be
			// postmultiplied in order to get back to originT
			offsetT.invert();
			flagFactors_[1].multiply( offsetT, originT );

			SmartPointer<Moo::DiscreteAnimationChannel> coord =
				new Moo::DiscreteAnimationChannel();
			coord->addKey( 0, flagFactors_[1] );
			coord->finalise();

			pNewAC->postCombine( *coord );
		}

		if (flags & 4)	// isImpacting
		{
			// we save the current animation channel (pNewAC) in flagFactor2,
			// with the first frame transform removed from it, then we set
			// pNewAC to be just the first frame transform the whole time.

			Matrix firstT; pNewAC->result( 0, firstT );
			Matrix firstTI; firstTI.invert( firstT );

			flagFactor2_ = pNewAC;
			SmartPointer<Moo::DiscreteAnimationChannel> smooth =
				new Moo::DiscreteAnimationChannel();
			smooth->addKey( 0, firstTI );
			smooth->finalise();
			flagFactor2_->preCombine( *smooth );

			Moo::DiscreteAnimationChannel * pNewACDerived =
				new Moo::DiscreteAnimationChannel();
			pNewACDerived->addKey( 0, firstT );
			pNewACDerived->finalise();
			pNewAC = pNewACDerived;

			flagFactors_[2] = Matrix::identity;
		}
	}
	else
	{
		ERROR_MSG( "NodefullModel::Animation::precalcItinerant:"
			"Could not duplicate animation channel of type %d\n",
			pIR->channel()->type() );

		pNewAC = pIR->channel().getObject();
	}

	// and store it for later switching
	alternateItinerants_[flags] = pNewAC;
}


/**
 *	Get the factor matrix for the given flags.
 */
void NodefullModel::Animation::flagFactor( int flags, Matrix & mOut ) const
{
	float param = mOut[0][0];

	mOut.setIdentity();
	if (flags < 0)
	{
		if (flags == -1)
		{
			anim_->itinerantRoot()->channel()->result(
				float(firstFrame_), mOut );
		}
		return;
	}

	if (flags & (1<<0))
	{
		mOut.preMultiply( this->Animation::flagFactorBit(0) );
	}
	if (flags & (1<<1))
	{
		mOut.preMultiply( this->Animation::flagFactorBit(1) );
	}
	if (flags & (1<<2))
	{
		this->Animation::flagFactorBit(2);
		mOut.preMultiply(
			flagFactor2_->result( firstFrame_ + frameRate_ * param ) );
	}
}


/**
 *	Get the given flag factor part
 */
const Matrix & NodefullModel::Animation::flagFactorBit( int bit ) const
{
	if (!alternateItinerants_[ 1<<bit ])
		const_cast<NodefullModel::Animation*>(this)->precalcItinerant( 1<<bit );
	return flagFactors_[ bit ];
}


/**
 *	Get the amount of memory used by this animation
 */
uint32 NodefullModel::Animation::sizeInBytes() const
{
	uint32 sz = this->Model_Animation::sizeInBytes();
	sz += sizeof(*this) - sizeof(Model_Animation);
	if (alphas_ != NULL)
		sz += contentSize( *alphas_ );
	sz += cognate_.length();
	return sz;
}


/**
 *	Constructor for the default animation (i.e. initial pose)
 *	of a nodefull model.
 */
NodefullModel::DefaultAnimation::DefaultAnimation(
		NodefullModel::NodeTree & rTree,
		int itinerantNodeIndex,
		std::avector< Matrix > & transforms ) :
	rTree_( rTree ),
	itinerantNodeIndex_( itinerantNodeIndex ),
	cTransforms_( transforms )
{
	for (uint i = 0; i < transforms.size(); i++)
	{
		bTransforms_.push_back( BlendTransform( transforms[i] ) );
	}

	memoryCounterAdd( model );
	memoryClaim( bTransforms_ );
	memoryClaim( cTransforms_ );
}

/**
 *	Destructor for the default animation of a nodefull model.
 */
NodefullModel::DefaultAnimation::~DefaultAnimation()
{
	memoryCounterSub( model );
	memoryClaim( cTransforms_ );
	memoryClaim( bTransforms_ );
}


/**
 *	This method applies the default animation (i.e. initial pose) with the
 *	given parameters to the global node table.
 */
void NodefullModel::DefaultAnimation::play( float time, float blendRatio, int flags )
{
	/* Previously the doxygen comment above included:
	 *		"If flags are -1, then the animation is only applied to those nodes
	 *		 that have not yet been touched with the current value of
	 *		s_blendCookie_.",
	 *	but this behaivour has been
	 *	moved into the model's dress call because always adding the default
	 *	animation is a pain, and I can't think of any reason why you wouldn't
	 *	want to use it if nothing else set the node. Plus originally I only
	 *	intended this to be used with subordinate model parts, whereas it should
	 *	rightly be used with all, since not every animation moves every node.
	 */
	for (uint i = 0; i < bTransforms_.size(); i++)
	{
		Moo::Node * pNode = rTree_[i].pNode;

		float oldBr = pNode->blend( s_blendCookie_ );
		float sumBr = oldBr + blendRatio;

		if (oldBr == 0.f)
		{
			pNode->blendTransform() = bTransforms_[i];
		}
		else
		{
			pNode->blendTransform().blend(
				blendRatio / sumBr, bTransforms_[i] );
		}

		pNode->blend( s_blendCookie_, sumBr );
	}

	/*
	}
	else // if flags == -1
	{
		for (int i = 0; i < bTransforms_.size(); i++)
		{
			Moo::Node * pNode = rTree_[i].pNode;
			if (pNode->blend( s_blendCookie_ ) == 0.f)
			{
				pNode->blendTransform() = transforms_[i];
				pNode->blend( s_blendCookie_, blendRatio );
			}
			// hmmm.. should we add the appropriate blendratio to it anyway?
			//  or conversely, should we always pretend a blendratio of 0...?
		}
	}
	*/
}


/**
 *	Get the factor matrix for the given flags. If flags are -1,
 *	get the itinerant root node's initial transform
 */
void NodefullModel::DefaultAnimation::flagFactor( int flags, Matrix & mOut ) const
{
	if (flags == -1 && uint(itinerantNodeIndex_) < cTransforms_.size())
	{
		mOut = cTransforms_[itinerantNodeIndex_];
	}
	else
	{
		mOut.setIdentity();
	}
}

// ----------------------------------------------------------------------------
// Section: SwitchedModel
// ----------------------------------------------------------------------------

/**
 *	Constructor
 */
template <class BULK> SwitchedModel<BULK>::SwitchedModel(
		const std::string & resourceID, DataSectionPtr pFile ) :
	Model( resourceID, pFile ),
	bulk_( NULL ),
	frameDress_( NULL ),
	frameDraw_( NULL ),
	blendRatio_( 0.f ),
	blendCookie_( s_blendCookie_ - 16 )
{
}


/**
 *	Initialisation function (called during construction)
 */
template <class BULK> bool SwitchedModel<BULK>::wireSwitch(
	DataSectionPtr pFile,
	BulkLoader loader,
	const std::string & mainLabel,
	const std::string & frameLabel )
{
	// load our bulk
	std::string bulkName = pFile->readString( mainLabel );
	bulk_ = (*loader)( *this, bulkName );

	// make sure something is there
	if (!bulk_)
	{
		ERROR_MSG( "SwitchedModel::SwitchedModel: "
			"Could not load resource '%s' as main bulk of model %s\n",
			bulkName.c_str(), resourceID_.c_str() );
		return false;
	}

	// create and add the default animation
	Animation * pDefAnim = new Animation( *this, "", 1.f );
	pDefAnim->addFrame( bulk_ );
	pDefaultAnimation_ = pDefAnim;

	// load all the other animations
	std::vector<DataSectionPtr>	vAnimSects;
	pFile->openSections( "animation", vAnimSects );
	for (uint i = 0; i < vAnimSects.size(); i++)
	{
		DataSectionPtr pAnimSect = vAnimSects[i];

		// read the common data
		std::string animName = pAnimSect->readString( "name" );
		float frameRate = pAnimSect->readFloat( "frameRate", 30.f );

		if (animName.empty())
		{
			ERROR_MSG( "Animation section %d in model %s has no name\n",
				i, resourceID_.c_str() );
			continue;
		}

		// now read the frames out
		std::vector<std::string> frameNames;
		if (!frameLabel.empty())
		{
			pAnimSect->readStrings( frameLabel, frameNames );
		}
		else
		{
			int frameCount = pAnimSect->readInt( "frameCount" );
			char ssbuf[32];
			for (int j = 0; j < frameCount; j++ )
			{
				std::strstream ss( ssbuf, sizeof(ssbuf) );
				ss << j << std::ends;
				frameNames.push_back(
					bulkName + "." + animName + "." + ss.str() );
			}
		}
		if (frameNames.empty())
		{
			ERROR_MSG( "Animation %s in model %s has no frames\n",
				animName.c_str(), resourceID_.c_str() );
			continue;
		}

		// ok, start with an empty animation
		Animation * pAnim = new Animation( *this, animName, frameRate );

		// fill it in
		for (uint j = 0; j < frameNames.size(); j++)
		{
			BULK pBulk = (*loader)( *this, frameNames[j] );

			if (!pBulk)
			{
				ERROR_MSG( "SwitchedModel::SwitchedModel: "
					"Frame %d of animation %s in model %s is "
					"missing its resource '%s'\n", j, animName.c_str(),
					resourceID_.c_str(), frameNames[j].c_str() );
			}
			else
			{
				pAnim->addFrame( pBulk );
			}
		}

		if (pAnim->numFrames() == 0)
		{
			delete pAnim;
			continue;
		}

		// and tell the world about it
#ifdef USE_STRINGMAP
		animations_.insert( std::make_pair( animName.c_str(), pAnim ) );
#else
		int existingIndex = this->getAnimation( animName );
		if (existingIndex == -1)
		{
			animationsIndex_.insert(
				std::make_pair( animName, animations_.size() ) );
			animations_.push_back( pAnim );
		}
		else
		{
			animations_[ existingIndex ] = pAnim;
		}
#endif
	}

	return true;
}


/**
 *	This method dresses this switched model
 */
template <class BULK> void SwitchedModel<BULK>::dress()
{
	frameDraw_ = frameDress_;
	frameDress_ = NULL;

	if (!frameDraw_) frameDraw_ = bulk_;
}


// ----------------------------------------------------------------------------
// Section: NodelessModel
// ----------------------------------------------------------------------------

/**
 *	Constructor
 */
NodelessModel::NodelessModel( const std::string & resourceID, DataSectionPtr pFile ) :
	SwitchedModel<Moo::VisualPtr>( resourceID, pFile ),
	batched_( false )
{
	// load standard switched model stuff
	if (!this->wireSwitch( pFile, &loadVisual, "nodelessVisual", "visual" ))
				return;

#ifndef EDITOR_ENABLED
	// see if we're batching it
	batched_ = pFile->readBool( "batched", false );
#endif

	// load dyes
	this->readDyes( pFile, true );
}



/**
 *	This method dresses this nodeless model.
 */
void NodelessModel::dress()
{
	this->SwitchedModel<Moo::VisualPtr>::dress();
}


/**
 *	This method draws this nodeless model.
 */
void NodelessModel::draw( bool checkBB )
{
	if (s_sceneRootNode_ != NULL)
	{
		s_sceneRootNode_->blendClobber(
			Model::s_blendCookie_, Matrix::identity );
		s_sceneRootNode_->visitSelf( Moo::rc().world() );
	}
#ifndef EDITOR_ENABLED
	if (!batched_)
		frameDraw_->draw( !checkBB );
	else
		frameDraw_->batch( !checkBB );
#else
		frameDraw_->draw( !checkBB );
#endif

#ifdef DEBUG_NORMALS
	if (draw_norms)
	{
		const Moo::Visual::RenderSetVector& set = bulk_->renderSets();
		Moo::Visual::RenderSetVector::const_iterator it = set.begin();
		for (;it != set.end(); it++)
		{
			Moo::Visual::GeometryVector::const_iterator g_it = (*it).geometry_.begin();
			//for(; g_it != (*it).geometry_.end(); it++)
			//for (uint x=0; x<(*it).geometry_.size(); x++)
			{
				//Moo::Visual::GeometryVector::const_iterator g_it = (*it).geometry_.begin();

				//const Moo::Visual::Geometry& geometry = (*it).geometry_[x];

				const Moo::Visual::Geometry& geometry = *g_it;
				const Moo::Vertices::VertexPositions &points = geometry.vertices_->vertexPositions();
				const Moo::Vertices::VertexNormals &tangentspace = geometry.vertices_->vertexNormals();

				const Moo::Vertices::VertexPositions &normals = geometry.vertices_->vertexNormals2();

				Moo::Vertices::VertexNormals::const_iterator n_it = tangentspace.begin();
				Moo::Vertices::VertexPositions::const_iterator n2_it = normals.begin();

				bool drawTS = tangentspace.size() > 0;
				//if (tangentspace.size())
				{
					Moo::Vertices::VertexPositions::const_iterator p_it = points.begin();
					//for (uint i = 0; i < points.size(); i++)
					for (;p_it != points.end(); p_it++)
					{						
						Moo::Colour ncol(1.f,1.f,0.f,1.f);
						Moo::Colour tcol(1.f,0.f,0.f,1.f);
						Moo::Colour bcol(0.f,0.f,1.f,1.f);

						//const Vector3& point = points[i];
						const Vector3& point = (*p_it);
						

						//Geometrics::drawLine( point, (point + Vector3(50,0,0)), col );

						Vector3 start = point;
						Vector3 normal;

						if (drawTS)
						{
							uint32 intNormal = (*n_it);
							normal = Moo::unpackNormal(intNormal);
							n_it++;
						}
						else
						{	
							normal = (*n2_it);
							n2_it++;
						}


						Vector3 end = point + normal*scale_norms;
						Geometrics::drawLine( start, end, ncol, false );


						if (drawTS)
						{

							uint32 intTangent = (*n_it);
							Vector3 tangent = Moo::unpackNormal(intTangent);
							n_it++;

							uint32 intBinormal = (*n_it);
							Vector3 binormal = Moo::unpackNormal(intBinormal);
							n_it++;
						
							end = point + tangent*scale_norms;
							Geometrics::drawLine( start, end, tcol, false );
							end = point + binormal*scale_norms;
							Geometrics::drawLine( start, end, bcol, false );
						}


					}
				}
			}
		}
	}
#endif //DEBUG_NORMALS	
}


/**
 *	This method returns a BSPTree of this model when decomposed
 */
const BSPTree * NodelessModel::decompose() const
{
	return bulk_ ? bulk_->pBSPTree() : NULL;
}


/**
 *	This method returns the bounding box of this model
 */
const BoundingBox & NodelessModel::boundingBox() const
{
	if (!bulk_) return s_emptyBoundingBox;
	return bulk_->boundingBox();
}

/**
 *	This method returns the bounding box of this model
 */
const BoundingBox & NodelessModel::visibilityBox() const
{
	return boundingBox();
}


/**
 *	This method gets our kind of static lighting
 */
Model::StaticLightingPtr NodelessModel::getStaticLighting(
	const DataSectionPtr section )
{
	if (!section)
		return NULL;

	StaticLightValues * pSLV = new StaticLightValues();
	pSLV->init( section->asBinary() );

	// how do we check for an error?

	return new StaticLighting( bulk_, pSLV );
}

/**
 *	This method sets the static lighting for the main bulk of the nodeless
 *	model from which it was loaded.
 */
void NodelessModel::StaticLighting::set()
{
	bulk_->staticVertexColours( pSLV_->vb() );
}

void NodelessModel::StaticLighting::unset()
{
	bulk_->staticVertexColours( NULL );
}


// ----------------------------------------------------------------------------
// Section: BillboardModel
// ----------------------------------------------------------------------------


/**
 *	This little class calls the billboard model to get it source,
 *	after the current model loading chain is done
 *	(and when it is in the static model list itself)
 */
/*
class BBSourcePLCB : public PostLoadCallBack
{
public:
	BBSourcePLCB( BillboardModel & bbm, DataSectionPtr p ) :
		bbm_( bbm ), p_( p ) {}
private:
	virtual void operator()() { bbm_.activateSource( p_ ); }

	BillboardModel	bbm_;
	DataSectionPtr	p_;
};
*/

/// static initialisers
Moo::VisualPtr			BillboardModel::billboardVisual_ = NULL;
Moo::EffectMaterial*	BillboardModel::billboardMaterial_ = NULL;
Moo::EffectMaterial **	BillboardModel::billboardVisualMaterial_ = NULL;
Moo::EffectPropertyPtr	BillboardModel::billboardTextureProperty_ = NULL;


Moo::EffectPropertyPtr findTextureProperty( Moo::EffectMaterial* m )
{
	ComObjectWrap<ID3DXEffect> pEffect = NULL;
	if (m && m->nEffects() > 0)
	{
		for (uint32 i=0; i<m->nEffects(); i++)
		{
			Moo::ManagedEffectPtr pf = m->pEffect(i);
			if ( pf.getObject() != NULL )
			{
				pEffect = pf->pEffect();
				break;
			}
		}
	}

	if ( pEffect == NULL )
		return NULL;

	for (uint32 i=0; i<m->nEffects(); i++)
	{
		Moo::EffectMaterial::Properties& properties = m->properties(i);
		Moo::EffectMaterial::Properties::iterator it = properties.begin();
		Moo::EffectMaterial::Properties::iterator end = properties.end();

		while ( it != end )
		{
			MF_ASSERT( it->second );
			D3DXHANDLE hParameter = it->first;
			Moo::EffectPropertyPtr& pProperty = it->second;

			D3DXPARAMETER_DESC desc;
			HRESULT hr = pEffect->GetParameterDesc( hParameter, &desc );
			if ( SUCCEEDED(hr) )
			{
				if (desc.Class == D3DXPC_OBJECT &&
					desc.Type == D3DXPT_TEXTURE)
				{
					return pProperty;
				}
			}
			it++;
		}
	}
	return NULL;
}



/**
 *	Constructor
 */
BillboardModel::BillboardModel( const std::string & resourceID, DataSectionPtr pFile ) :
	SwitchedModel< TextureArrayPtr >( resourceID, pFile ),
	dyeComboCount_( 0 ),
	dyeComboIndexDress_( 0 ),
	dyeComboIndexDraw_( 0 ),
	unused_( 0 ),
	bb_( Vector3( 0.f, 0.f, 0.f ), Vector3( 1.f, 1.f, 1.f ) ),
	sourceWidth_( 128 ),
	sourceHeight_( 128 )
{
	// clear out any existing tints in our matters
	for (uint i = 0; i < matters_.size(); i++)
	{
		Tints & ts = (matters_.begin()+i)->second.tints_;
		for (uint j = 0; j < ts.size(); j++)
		{
			(ts.begin()+j)->second->oldMaterial_->textureFactor( 0 );
		}
	}

	// load our way of defining dyes
	dyeComboCount_ = this->readDyes( pFile, false );

	// let the switched model do its stuff
	this->wireSwitch( pFile, &loadTextureCombo, "billboardVisual", "" );
	// (which is:
	//  - read the main bulk for all dye combinations
	//  - read the animations and load for all dye combinations
	// )

	// save the bulk resource id
	bulkResourceID_ = pFile->readString( "billboardVisual" );

	// read source info
	DataSectionPtr pSrc = pFile->openSection( "source" );
	if (pSrc)
	{
		pSrc->readStrings( "model", sourceModels_ );
		for (uint i = 0; i < sourceModels_.size(); i++)
			sourceModels_[i] += ".model";

		sourceWidth_  = pSrc->readInt( "width" , sourceWidth_  );
		sourceHeight_ = pSrc->readInt( "height", sourceHeight_ );

		std::vector<DataSectionPtr> vSourceDyes;
		pSrc->openSections( "dye", vSourceDyes );
		for (uint k = 0; k < vSourceDyes.size(); k++)
		{
			DataSectionPtr pSourceDye = vSourceDyes[k];
			DyeSelection dsel;

			if (!this->readSourceDye( pSourceDye, dsel ))
			{
				ERROR_MSG( "Model::readDyes: "
					"In model %s, "
					"source dye selection section index %d is missing "
					"either a matter ('%s') or tint ('%s') name.\n",
					resourceID_.c_str(), k,
					dsel.matterName_.c_str(), dsel.tintName_.c_str() );
			}

			sourceDyes_.push_back( dsel );
		}
	}

	// read bounding box if it's there
	DataSectionPtr pBB = pFile->openSection( "boundingBox" );
	if (pBB)
	{
		bb_.setBounds(	pBB->readVector3( "min", bb_.minBounds() ),
						pBB->readVector3( "max", bb_.maxBounds() ) );
	}

	// load the static class material for the billboard (most should not need this)
	if ( !billboardMaterial_ )
	{
		billboardMaterial_ = new Moo::EffectMaterial;
		DataSectionPtr mfmSection = BWResource::openSection( s_billboardMfmName );
		if ( !mfmSection )
		{
			WARNING_MSG( "Could not load billboard mfm %s\n", s_billboardMfmName.value().c_str() );
		}
		else
		{
			bool mgood = billboardMaterial_->load( mfmSection );
			if (!mgood)
			{
				WARNING_MSG( "BillboardModel::BillboadModel: "
					"Error processing material section in model %s\n",
					resourceID.c_str() );
			}
			else
			{
				//save the class static texture property pointer.
				billboardTextureProperty_ = findTextureProperty( billboardMaterial_ );
			}
		}
	}

	// activate our source ... after the current chain of model gets returns
	//PostLoadCallBack::add( new BBSourcePLCB( *this, pFile ), true );
	//  (probably going to scrap this way of doing things...)

	// calculate the scale to box matrix
	Vector3 scale( (bb_.maxBounds() - bb_.minBounds()) * 0.5f );
	scaleToBox_.setScale( scale );
	scaleToBox_.translation( (bb_.maxBounds() + bb_.minBounds()) * 0.5f );

	// load the class static billboard visual if it's not there
	if (!billboardVisual_)
	{
		billboardVisual_ = Moo::VisualManager::instance()->get(s_billboardVisualName);
		MF_ASSERT( billboardVisual_ );

		std::vector< Moo::EffectMaterial ** > bbvms;
		billboardVisual_->gatherMaterials( "billboard", bbvms );
		MF_ASSERT( !bbvms.empty() );

		billboardVisualMaterial_ = bbvms[0];
	}

	memoryCounterAdd( model );
	memoryClaim( bulkResourceID_ );
	memoryClaim( sourceModels_ );
	for (uint i = 0; i < sourceModels_.size(); i++)
		memoryClaim( sourceModels_[i] );
	memoryClaim( sourceDyes_ );
	for (uint i = 0; i < sourceDyes_.size(); i++)
	{
		memoryClaim( sourceDyes_[i].matterName_ );
		memoryClaim( sourceDyes_[i].tintName_ );
		memoryClaim( sourceDyes_[i].properties_ );
	}
}


/**
 *	Destructor
 */
BillboardModel::~BillboardModel()
{
	// remove the size of our objects from the model memory counter
	memoryCounterSub( model );
	memoryClaim( sourceDyes_ );
	for (uint i = 0; i < sourceDyes_.size(); i++)
	{
		memoryClaim( sourceDyes_[i].matterName_ );
		memoryClaim( sourceDyes_[i].tintName_ );
		memoryClaim( sourceDyes_[i].properties_ );
	}
	memoryClaim( sourceModels_ );
	for (uint i = 0; i < sourceModels_.size(); i++)
		memoryClaim( sourceModels_[i] );
	memoryClaim( bulkResourceID_ );

	// everything else happens automatically
}


/**
 *	This private method creates and loads a texture combo array whose
 *	resources start with the given resourceID.
 */
TextureArrayPtr BillboardModel::loadTextureCombo(
	Model & m, const std::string & resourceID )
{
	BillboardModel & bm = static_cast<BillboardModel&>( m );

	ArrayHolder< Moo::BaseTexturePtr > * btArray =
		new ArrayHolder< Moo::BaseTexturePtr >( bm.dyeComboCount_ );

	Moo::TextureManager * tMan = Moo::TextureManager::instance();
	for (int d = 0; d < bm.dyeComboCount_; d++)
	{
		// make up the string for this combo
		std::string dyeComboSpec = bm.expressComboIndex( d );

		// attempt to load this texture (failure is fine)
		(*btArray)[ d ] = tMan->get(
			resourceID + dyeComboSpec + "dds", true, false );
	}

	return btArray;
}


/**
 *	This method turns the given combo index into the string that is used to
 *	make the dye combo part of the texture name. In addition, if the
 *	supermodel and fashion vector pointers passed in are not NULL,
 *	the source dyes of each of the tints selected will fetched from the
 *	supermodel and added to the fashion vector.
 */
std::string BillboardModel::expressComboIndex( int comboIndex,
	SuperModel * sm, FashionVector * fv )
{
	int cd = comboIndex;
	int dyeProduct = 1;
	std::string dyeComboSpec = ".";
	for (uint i = 0; i < matters_.size(); i++)
	{
		Tints & ts = (matters_.begin()+i)->second.tints_;

		dyeProduct *= ts.size();
		int tindex = cd % dyeProduct;

		Tints::value_type & tv = *(ts.begin() + tindex);
		dyeComboSpec += tv.first;
		dyeComboSpec += ".";

		if (sm && fv)
		{
			this->addDyesFromSelections( tv.second->sourceDyes_, *sm, *fv );
		}

		cd -= tindex;
	}

	return dyeComboSpec;
}


/**
 *	This static method adds the specified dytes to the given fashion vector
 */
void BillboardModel::addDyesFromSelections( const DyeSelections & ds,
	SuperModel & sm, FashionVector & fv )
{
	for (uint i = 0; i < ds.size(); i++)
	{
		SuperModelDyePtr pSMD = sm.getDye( ds[i].matterName_, ds[i].tintName_ );
		if (!pSMD) continue;

		// we could do this the right way...
		/*
		for (int j = 0; j < ds.properties_.size(); j++)
		{
			DyePropSetting & dsrc = ds.properties_[j];

			for (int k = 0; k < pSMD->properties.size(); k++)
			{
				DyePropSetting & dset = pSMD->properties[k];
				if (dset.index_ == dsrc.index_)
				{
					dset.value_ = dsrc.value_;
				}
			}
		}
		*/

		// but this is much easier :)
		pSMD->properties = ds[i].properties_;

		fv.push_back( pSMD );
	}
}


/**
 *	This method dresses this billboard.
 */
void BillboardModel::dress()
{
	this->SwitchedModel< TextureArrayPtr >::dress();

	dyeComboIndexDraw_ = dyeComboIndexDress_;
	dyeComboIndexDress_ = 0;

	// make sure it's legal
	if (dyeComboIndexDraw_ > dyeComboCount_)
	{
		ERROR_MSG( "BillboardModel::dress: "
			"Tried to dye model %s in more than one tint "
			"on the same matter (dci %d, dcc %d)\n",
			resourceID_.c_str(), dyeComboIndexDraw_, dyeComboCount_);

		dyeComboIndexDraw_ = 0;
	}
}


/**
 *	This method draws this billboard.
 */
void BillboardModel::draw( bool /*checkBB*/ )
{
	Moo::BaseTexturePtr texture = (*frameDraw_)[ dyeComboIndexDraw_ ];

	if (!texture)
	{
		if (!sourceModels_.empty()) this->createTexturesFromSource();
		//  (but still draw nothing this frame 'coz we've lost the result
		//  of the animation frame switching stuff and wouldn't know which)
		return;
	}

	*billboardVisualMaterial_ = billboardMaterial_;

#ifndef EDITOR_ENABLED
	billboardTextureProperty_->be( texture );
#else
	MaterialTextureProxy* prop = static_cast<MaterialTextureProxy*>(&*billboardTextureProperty_);
	prop->set( texture->resourceID(), true );
#endif
	// TODO: really should make these materials the contents of the
	// combo arrays, because we can't fiddle around with materials
	// if they're going to be sorted (they'll all show up as the same one)

	// change transform from bounding box to our static mesh
	Moo::rc().push();
	Moo::rc().preMultiply( scaleToBox_ );

	// draw the mesh (currently a visual)
	billboardVisual_->draw( true );

	Moo::rc().pop();
}


/**
 *	This method soaks this billboard model in the given (packed) dye index.
 *
 *	@see Model::soak
 *	@see Model::Matter::soak
 */
void BillboardModel::soak( int dye )
{
	// unpack
	int matter = dye >> 16;
	int tint = uint16( dye );

	// look up the matter
	if (uint(matter) >= matters_.size()) return;

	// look up the tint
	Tints & ts = (matters_.begin() + matter)->second.tints_;
	if (uint(tint) > ts.size()) tint = 0;
	TintPtr rTint = (ts.begin() + tint)->second;

	// add its combo component to our index
	dyeComboIndexDress_ += rTint->oldMaterial_->textureFactor();
}


/**
 *	This method creates all the textures for the current dye combo index,
 *	by making and photographing a supermodel from the source specification.
 */
void BillboardModel::createTexturesFromSource()
{
	NOTICE_MSG( "BillboardModel '%s' "
		"photographing source models (for dye combo index %d)\n",
		resourceID_.c_str(), dyeComboIndexDraw_ );

	// create the render target
	Moo::RenderTarget rt( "BillboardModel_Surface" );
	if (!rt.create( sourceWidth_, sourceHeight_ ))
	{
		ERROR_MSG( "BillboardModel::createTexturesFromSource: "
			"Could not create render target of size (%d,%d)\n",
			sourceWidth_, sourceHeight_ );
		return;
	}

	// make the supermodel
	SuperModel sm( sourceModels_ );
	if (sm.nModels() == 0 || sm.nModels() != sourceModels_.size())
	{
		ERROR_MSG( "BillboardModel::createTexturesFromSource: In model %s, "
			"could not load all the source models (or none are defined)\n",
			resourceID_.c_str() );
		return;
	}

	// make the basic fashion vector
	FashionVector fv;
	this->addDyesFromSelections( sourceDyes_, sm, fv );

	// use any legacy dyes
	int i = 0;
	while (1)
	{
		SuperModelDyePtr pDye;
		char legName[128];
		sprintf( legName, "Legacy-%d", i );

		pDye = sm.getDye( legName, "MFO" );
		if (!pDye) break;

		fv.push_back( pDye );
		i++;
	}

	// figure out the combo string and fill in the rest of the
	//  fashion vector (based on selected tints) at the same time
	std::string dyeComboSpec =
		this->expressComboIndex( dyeComboIndexDraw_, &sm, &fv );

	// record how big it is without animations (previously thought I
	//  might be adding even more stuff to it, but this way still works)
	int fvKeepSize = fv.size();

	// find the supermodel's bounding box
	BoundingBox sbb = sm.topModel(0)->boundingBox();
	for (int i = 1; i < sm.nModels(); i++)
		sbb.addBounds( sm.topModel(i)->boundingBox() );

	// make it our own and write it out
	bb_ = sbb;
	DataSectionPtr pFile = BWResource::openSection( resourceID_ );
	pFile->writeVector3( "boundingBox/min", bb_.minBounds() );
	pFile->writeVector3( "boundingBox/max", bb_.maxBounds() );
	pFile->save();


	// Now do common setup for drawing the textures

	Moo::SortedChannel::draw();

	// push the render target
	rt.push();

	// Turn off shimmermaterials and set the colourmask to render to all
	// channels so that we get the alpha channel in our billboard texture.
	bool oldShimmer = Moo::Material::shimmerMaterials;
	Moo::Material::shimmerMaterials = false;
	Moo::rc().setRenderState( D3DRS_COLORWRITEENABLE,
		D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
		D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA );

	// set up the render context
	Moo::rc().screenWidth( sourceWidth_/2 );
	Moo::rc().screenHeight( sourceHeight_ );
	Moo::rc().push();

	// make our own lights
	Moo::LightContainerPtr oldPRCLC = Moo::rc().lightContainer();
	Moo::LightContainerPtr pRCLC = new Moo::LightContainer();
	Moo::rc().lightContainer( pRCLC );
	pRCLC->ambientColour( Moo::Colour( 0.5, 0.5, 0.5, 1 ) );
	//pRCLC->addDirectional( new Moo::DirectionalLight(
	//	Moo::Colour( 0.5f, .5, .5, 0.f ), Vector3( 0, 0, -1 ) ) );


	// create the unanimated texture
	//  (maybe should add the default animation to it?)
	(*bulk_)[ dyeComboIndexDraw_ ] = this->createOneTexture(
		bulkResourceID_ + dyeComboSpec + "dds", sm, fv, sbb, rt.pTexture() );

	// go through all the animations and create their textures
	for (uint i = 0; i < animations_.size(); i++)
	{
		// get the source supermodel's animation
#ifdef USE_STRINGMAP
		const char * animName = (animations_.begin()+i)->first;
#else
		Model * pModel = this;
		while (pModel->parent() && i <
			static_cast<BillboardModel*>(pModel->parent())->animations_.size())
				pModel = &*pModel->parent();
		const char * animName = NULL;
		BillboardModel * pNotABB = static_cast<BillboardModel*>( pModel );
		for (AnimationsIndex::iterator it = pNotABB->animationsIndex_.begin();
			it != pNotABB->animationsIndex_.end();
			it++)
		{
			if (it->second == i)
			{
				animName = it->first.c_str();
				break;
			}
		}
		MF_ASSERT( animName != NULL);
#endif
		SuperModelAnimationPtr sma = sm.getAnimation( animName );
		if (!sma) continue;
		sma->blendRatio = 1.0f;

		// get a source model's internal animation (for duration info)
		const Model::Animation * smma = sma->pSource( sm );
		if (!smma) continue;

		// TODO: fix our own frameRate field to make our duration
		//  the same as the source model's animation's - and write it out

		// fix up the fashion vector
		fv.erase( fv.begin() + fvKeepSize, fv.end() );
		fv.push_back( sma );

		// get our own animation
		Model::Animation * pOwnAnim =
#ifdef USE_STRINGMAP
			(animations_.begin()+i)->second.getObject();
#else
			animations_[ i ].getObject();
#endif
		std::vector< TextureArrayPtr > & ownFrames =
			static_cast<Animation *>( pOwnAnim )->frames();
		// TODO: remove the unsafe cast in the line above!

		// and create the texture for each frame
		char	ssbuf[32];
		for (uint j = 0; j < ownFrames.size(); j++)
		{
			sma->time = float(j) * smma->duration_ / float(ownFrames.size());

			std::strstream ss( ssbuf, sizeof(ssbuf) );
			ss << j << std::ends;

			(*ownFrames[j])[ dyeComboIndexDraw_ ] = this->createOneTexture(
				bulkResourceID_ + "." + animName + "." + ss.str() +
				dyeComboSpec + "dds", sm, fv, sbb, rt.pTexture() );
		}
	}

	// Undo common setup for drawing the textures

	// restore light container
	Moo::rc().lightContainer( oldPRCLC );

	// pop world matrix
	Moo::rc().pop();

	// Reset the shimmer materials property.
	Moo::Material::shimmerMaterials = oldShimmer;

	// pop render target stuff
	rt.pop();
}


/**
 *	This method creates one billboard texture of the given supermodel,
 *	and saves it under the given name. The given dyes are also applied
 *	(dyes in the second vector override those in the first)
 *
 *	@return the texture created
 */
Moo::BaseTexturePtr BillboardModel::createOneTexture( std::string & resourceID,
	SuperModel & sm, FashionVector & fv, BoundingBox & bb, DX::BaseTexture * pTex )
{
	// set up the world matrix
	const Vector3 & bbM = bb.maxBounds();
	const Vector3 & bbm = bb.minBounds();
	float nearZ = 0.5f;

	Moo::rc().world().setTranslate( (bbM + bbm) * -0.5f );

	// prepare viewport
	DX::Viewport viewport;
	viewport.X = 0;
	viewport.Y = 0;
	viewport.Width = sourceWidth_/2;
	viewport.Height = sourceHeight_;
	viewport.MinZ = 0.f;
	viewport.MaxZ = 1.f;

	// set up roataion matrix ... look from -Z then from -X
	Matrix rotation;
	rotation.setRotateY( MATH_PI / 2 );

	Matrix rotate45;
	rotate45.setRotateY( DEG_TO_RAD(45.f) );
	Moo::rc().postMultiply( rotate45 );

	// draw the supermodel in two orientations
	for (int i = 0; i < 2; i++)
	{
		// slide the hole over
		Moo::rc().device()->SetViewport( &viewport );

		// calculate the view and projection matrices
		float whichXZ = i == 0 ? (bbM.x - bbm.x) : (bbM.z - bbm.z);
		Moo::rc().view().setTranslate(
			0.f,
			0.f,
			whichXZ * 2.f + 100.f );

		Moo::rc().projection().orthogonalProjection(
			whichXZ * 1.2f,
			bbM.y - bbm.y,
			nearZ,
			whichXZ * 4.f + 1.f + 100.f );

		Moo::rc().updateViewTransforms();

		// draw the thing
		Moo::rc().device()->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
			0x00000000, 1.f, 0 );
		sm.draw( &fv, 0, 0.f );
		Moo::SortedChannel::draw();

		// set up for next time
		viewport.X += sourceWidth_ / 2;
		Moo::rc().postMultiply( rotation );
	}

	// write out the texture for later
	Moo::TextureManager::writeDDS( pTex, resourceID, D3DFMT_DXT1 );

	// and return
	return Moo::TextureManager::instance()->get( resourceID );

	// can't do line below 'coz it's the wrong type of texture
	//return rt.pTexture();
	// we return the rt's texture in case we are on a RO filesystem and the
	// save failed ... but TODO: check that the surface isn't special somehow
}



// ----------------------------------------------------------------------------
// Section: SuperModel
// ----------------------------------------------------------------------------

/**
 *	Fashion constructor
 */
Fashion::Fashion()
{
	memoryCounterAdd( superModel );
	memoryClaim( this );
}

/**
 *	Fashion destructor
 */
Fashion::~Fashion()
{
	memoryCounterSub( superModel );
	memoryClaim( this );
}


/**
 *	SuperModel constructor
 */
SuperModel::SuperModel( const std::vector< std::string > & modelIDs ) :
	lod_( 0.f ),
	nModels_( 0 ),
	lodNextUp_( -1.f ),
	lodNextDown_( 1000000.f ),
	checkBB_( true ),
	redress_( false ),
	isOK_( true )
{
#ifdef DEBUG_NORMALS
	static bool first=true;
	if (first)
	{
		MF_WATCH( "Render/Draw Normals", draw_norms, Watcher::WT_READ_WRITE,
			"Draw the normals!" );

		MF_WATCH( "Render/Normals scale", scale_norms, Watcher::WT_READ_WRITE,
			"Draw the normals!" );
		first = false;
	}
#endif //DEBUG_NORMALS

	for (uint i = 0; i < modelIDs.size(); i++)
	{
		ModelPtr pModel = Model::get( modelIDs[i] );
		if (pModel == NULL)
		{
			isOK_ = false;
			continue;	// Model::get will already have reported an error
		}

		ModelStuff	ms;
		ms.topModel = pModel;
		ms.curModel = ms.topModel.getObject();
		ms.preModel = NULL;
		models_.push_back( ms );

		lodNextDown_ = min( lodNextDown_, ms.topModel->extent() );
	}
	nModels_ = models_.size();

	memoryCounterAdd( superModel );
	memoryClaim( this );
	memoryClaim( models_ );
}

/**
 *	SuperModel destructor
 */
SuperModel::~SuperModel()
{
	memoryCounterSub( superModel );
	memoryClaim( models_ );
	memoryClaim( this );
}

/**
 *	Draw this SuperModel at the current Moo::rc().world() transform
 */
float SuperModel::draw( const FashionVector * pFashions, int nLateFashions,
	float atLod, bool doDraw )
{
	Model * pModel;

	const Matrix & mooWorld = Moo::rc().world();
	const Matrix & mooView = Moo::rc().view();

	// Step 1: Figure out if our bounding box is visible
	// if (bb.calculateOutcode( Moo::rc().worldViewProjection() )) return;

	// Step 2: Figure out what LODs we want to use
	if (atLod < 0.f)
	{
		// we want the z component from the object-to-view matrix
		//Matrix objectToView = Moo::rc().world() x_multiply_x Moo::rc().view()
		//lod_ = objectToView[4-1][3-1] / max( Moo::rc().world().yscale, 1.f )
		float distance = mooWorld.applyToOrigin().dotProduct(
			Vector3( mooView._13, mooView._23, mooView._33 ) ) + mooView._43;
		float yscale = mooWorld.applyToUnitAxisVector(1).length();

		lod_ = (distance / max( 1.f, yscale )) * Moo::rc().zoomFactor();
	}
	else
	{
		lod_ = atLod;
	}

	if ((lod_ > lodNextDown_) || (lod_ <= lodNextUp_))
	{
		for (int i=0; i < nModels_; i++)
		{
			models_[i].preModel = NULL;
			Model* model = models_[i].topModel.getObject();

			while ((model) && (model->extent() != -1.f) && (model->extent() < lod_))
			{

				if ((model->extent() != 0.f) &&	// The current model is visible AND either
					((models_[i].preModel == NULL) ||	//the previous model doesn't exist OR
						(model->extent() > models_[i].preModel->extent())))	// it is closer than the current model
				{
					models_[i].preModel = model;	// Save it as the previous one...
				}

				model = model->parent();	// And move on to the next
			}

			models_[i].curModel = model;

		}

		lodNextUp_ = -1.f;
		lodNextDown_ = 1000000.f;

		for (int i=0; i < nModels_; i++)
		{
			ModelStuff & ms = models_[i];
			if (ms.preModel != NULL) lodNextUp_ =
				max( lodNextUp_, ms.preModel->extent() );
			if (ms.curModel != NULL) lodNextDown_ =
				min( lodNextDown_, ms.curModel->extent() );
		}
	}

	static DogWatch fashionTimer( "Fashion" );

	fashionTimer.start();


	// Step 3: Dress up in the latest fashion
	Model::s_blendCookie_ = (Model::s_blendCookie_+1) & 0x0ffffff;
	redress_ = false;

	// HACK: increment the morph animation cookie.
	Moo::MorphVertices::incrementCookie();

	FashionVector::const_iterator fashIt;
	if (pFashions != NULL)
	{
		FashionVector::const_iterator fashLate =
			pFashions->end() - nLateFashions;
		for (fashIt = pFashions->begin(); fashIt != fashLate; fashIt++)
		{
			(*fashIt)->dress( *this );
		}
	}

	for (int i = 0; i < nModels_; i++)
	{
		if ((pModel = models_[i].curModel) != NULL)
		{
			pModel->dress();
		}
	}

	if (pFashions != NULL)
	{
		FashionVector::const_iterator fashEnd = pFashions->end();
		for (; fashIt != fashEnd; fashIt++)
		{
			(*fashIt)->dress( *this );
		}
	}

	// HACK!
	if (redress_) for (int i = 0; i < nModels_; i++)
	{
		if ((pModel = models_[i].curModel) != NULL)
		{
			pModel->dress();
		}
	}
	// !HACK

	fashionTimer.stop();

	{
		static DogWatch modelDrawTimer( "ModelDraw" );

		modelDrawTimer.start();

		// Step 4: Draw all the models
		for (int i = 0; i < nModels_; i++)
		{
			if ((pModel = models_[i].curModel) != NULL)
			{
				// commit the dye setup per model.
				pModel->soakDyes();
				if (doDraw)
					pModel->draw( checkBB_ );
			}
		}
		modelDrawTimer.stop();
	}

	// Step 5: undress
	if( pFashions != NULL )
	{
		FashionVector::const_reverse_iterator fashRIt;
		for( fashRIt = pFashions->rbegin(); fashRIt != pFashions->rend(); fashRIt++ )
		{
			(*fashRIt)->undress( *this );
		}
	}

	// Step 6: Profit
	return lod_;
}


/**
 *	Dress this model up in its default fashions, but don't draw it.
 */
void SuperModel::dressInDefault()
{
	SuperModelAnimationPtr smap = this->getAnimation( "" );
	smap->blendRatio = 1.f;

	Moo::rc().push();
	Moo::rc().world( Matrix::identity );

	Model::s_blendCookie_ = (Model::s_blendCookie_+1) & 0x0ffffff;

	smap->dress( *this );

	for (int i = 0; i < nModels_; i++)
	{
		models_[i].topModel->dress();
	}

	Moo::rc().pop();
}


/**
 *	This method gets an animation from the SuperModel. The animation
 *	is only good for this supermodel - don't try to use it with other
 *	supermodels.
 */
SuperModelAnimationPtr SuperModel::getAnimation( const std::string & name )
{
	if (nModels_ == 0) return NULL;

	char * pBytes = new char[sizeof(SuperModelAnimation)+(nModels_-1)*sizeof(int)];
	// since this is a simple char array, I believe that we don't
	//  have to delete [] it.

	SuperModelAnimation * pAnim = new ( pBytes ) SuperModelAnimation( *this, name );

	return pAnim;
}


/**
 *	SuperModelAnimation constructor
 */
SuperModelAnimation::SuperModelAnimation( SuperModel & superModel,
		const std::string & name ) :
	time( 0.f ),
	lastTime( 0.f ),
	blendRatio( 0.f )
{
	for (int i = 0; i < superModel.nModels(); i++)
	{
		index[i] = superModel.topModel(i)->getAnimation( name );
	}
}


/**
 *	This method ticks the animations represented by this class in
 *	the input supermodel. It must be the same as it was created with.
 *	Only the top model's animations are ticked.
 *	This is a helper method, since ticks are discouraged in SuperModel code.
 */
void SuperModelAnimation::tick( SuperModel & superModel, float dtime )
{
	for (int i = 0; i < superModel.nModels(); i++)
	{
		Model * cM = &*superModel.topModel(i);
		if (cM != NULL) cM->tickAnimation( index[i], dtime, lastTime, time );
	}

	lastTime = time;
}


/**
 *	This method applies the animation represented by this class to
 *	the input supermodel. It must be the same as it was created with.
 */
void SuperModelAnimation::dress( SuperModel & superModel )
{
	for (int i = 0; i < superModel.nModels(); i++)
	{
		Model * cM = superModel.curModel(i);
		if (cM != NULL && index[i] != -1)
			cM->playAnimation( index[i], time, blendRatio, 0 );
	}
}


/**
 *	This method returns the given animation, for the top lod of the first
 *	model it exists in.
 *
 *	@return the animation, or NULL if none exists
 */
/*const*/ Model::Animation * SuperModelAnimation::pSource( SuperModel & superModel ) const
{
	for (int i = 0; i < superModel.nModels(); i++)
	{
		if (index[i] != -1)
		{
			return superModel.topModel(i)->lookupLocalAnimation( index[i] );
		}
	}

	return NULL;	// no models have this animation
}


/**
 *	This method gets an action from the SuperModel. The action
 *	is only good for this supermodel - don't try to use it with other
 *	supermodels.
 */
SuperModelActionPtr SuperModel::getAction( const std::string & name )
{
	if (nModels_ == 0) return NULL;

	char * pBytes = new char[sizeof(SuperModelAction)+(nModels_-1)*sizeof(int)];

	SuperModelActionPtr pAction = new ( pBytes ) SuperModelAction( *this, name );

	return pAction->pSource != NULL ? pAction : SuperModelActionPtr(NULL);
}


/**
 *	This method gets all the matchable actions in any of the Models in
 *	this SuperModel.
 */
void SuperModel::getMatchableActions(
	std::vector< SuperModelActionPtr > & actions )
{
	actions.clear();

	// make a set of strings
#ifdef USE_STRINGMAP
	StringMap<bool>					matchableNames;
#else
	std::vector<const std::string*>	matchableNames;
#endif

	// add the names of each model's matchable actions to it
#ifdef USE_STRINGMAP
	for (int i = 0; i < nModels_; i++)
	{
		Model * tM = models_[i].topModel.getObject();
		const Model::Action * pAct;

		for (int j = 0; (pAct = tM->lookupLocalAction( j )) != NULL; j++)
		{
			if (pAct->isMatchable_)
			{
				matchableNames.insert(
					std::pair<const char *,bool>( pAct->name_.c_str(), true ) );
			}
		}
	}
#else
	for (int i = nModels_ - 1; i >= 0; i--)
	{
		Model * tM = models_[i].topModel.getObject();
		const Model::Action * pAct;

		for (Model::ActionsIterator it = tM->lookupLocalActionsBegin();
			it != tM->lookupLocalActionsEnd() && (pAct = &*it) != NULL;
			it++)
		{
			if (pAct->isMatchable_)
			{
				matchableNames.push_back( &pAct->name_ );
			}
		}
	}
#endif



	// fetch them all in the normal way
#ifdef USE_STRINGMAP
	for (uint i = 0; i < matchableNames.size(); i++)
	{
		actions.push_back( this->getAction(
			(matchableNames.begin() + i)->first ) );
	}
#else
	// when we're not using a stringmap, the actions are provided to us
	// in exact reverse order to that when we do use the stringmap...
	// this is because we don't copy the actions into derived models,
	// and we don't want to have a stack in the ActionsIterator object
	// so we iterate over our own actions first then our parent's
	std::set<std::string> seenNames;
	for (int i = 0; i <= int(matchableNames.size())-1; i++)
	{
		// only add this one if we haven't seen it before
		if (seenNames.insert( *matchableNames[i] ).second == true)
		{
			actions.push_back( this->getAction( *matchableNames[i] ) );
		}
	}
#endif
}

/**
 *	SuperModelAction constructor
 */
SuperModelAction::SuperModelAction( SuperModel & superModel,
		const std::string & name ) :
	passed( 0.f ),
	played( 0.f ),
	finish( -1.f ),
	lastPlayed( 0.f ),
	pSource( NULL ),
	pFirstAnim( NULL )
{
	Model * pM = superModel.topModel(0).getObject();

	const Model::Action * pTop = pSource = pM->getAction( name );
	index[0] = pTop != NULL ? pM->getAnimation( pTop->animation_ ) : -1;
	int lui = 0;

	for (int i = 1; i < superModel.nModels(); i++)
	{
		pM = superModel.topModel(i).getObject();

		const Model::Action * pOwn = pM->getAction( name );
		index[i] = pOwn != NULL ? pM->getAnimation( pOwn->animation_ ) :
			pTop != NULL ? pM->getAnimation( pTop->animation_ ) : -1;

		if (pSource == NULL)
		{
			pSource = pOwn;
			lui = i;
		}
	}

	// get the animation pointer here too
	pM = superModel.topModel(lui).getObject();
	pFirstAnim = (index[lui] != -1) ?
		pM->lookupLocalAnimation( index[lui] ) :
		pM->lookupLocalDefaultAnimation();
}


/**
 *	This method ticks the animations referenced by this action in
 *	the input supermodel. It must be the same as it was created with.
 *	Only the top model's animations are ticked.
 *	This is a helper method, since ticks are discouraged in SuperModel code.
 */
void SuperModelAction::tick( SuperModel & superModel, float dtime )
{
	// tick corresponding animations
	for (int i = 0; i < superModel.nModels(); i++)
	{
		Model * cM = &*superModel.topModel(i);
		if (cM != NULL && index[i] != -1)
			cM->tickAnimation( index[i], dtime, lastPlayed, played );
	}

	// remember when we last ticked them
	lastPlayed = played;
}


/**
 *	This method applies the animations referenced by this action to
 *	the input supermodel. It must be the same as it was created with.
 */
void SuperModelAction::dress( SuperModel & superModel )
{
	// cache blend ratio
	float br = this->blendRatio();
	if (pSource->track_ != 0) br = br * br * 10.f;	// old action queue practice

	// configure flags
	int flags = pSource->flagSum_;

	// and play corresponding animations
 	for (int i = 0; i < superModel.nModels(); i++)
	{
		Model * cM = superModel.curModel(i);
		if (cM != NULL && index[i] != -1)
			cM->playAnimation( index[i], played, br, flags );
	}
}


/**
 *	This method gets a dye from the SuperModel. The dye is only good
 *	for this supermodel - don't try to use it with other supermodels.
 */
SuperModelDyePtr SuperModel::getDye( const std::string & matter,
	const std::string & tint)
{
	if (nModels_ == 0) return NULL;

	char * pBytes = new char[sizeof(SuperModelDye)+(nModels_-1)*sizeof(int)];

	SuperModelDyePtr pDye = new ( pBytes ) SuperModelDye( *this, matter, tint );

	return pDye->effective( *this ) ? pDye : SuperModelDyePtr(NULL);
}


/**
 *	SuperModelDye constructor
 */
SuperModelDye::SuperModelDye( SuperModel & superModel,
	const std::string & matter, const std::string & tint )
{
	std::set<int>	gotIndices;

	for (int i = 0; i < superModel.nModels(); i++)
	{
		// Get the dye
		Model::Tint * pTint = NULL;
		index[i] = superModel.topModel(i)->getDye( matter, tint, &pTint );

		// Add all its properties to our list
		if (pTint != NULL)
		{
			for (uint j = 0; j < pTint->properties_.size(); j++)
			{
				Model::DyeProperty & dp = pTint->properties_[j];

				if (gotIndices.insert( dp.index_ ).second)
				{
					properties.push_back( Model::DyePropSetting(
						dp.index_, dp.default_ ) );
				}
			}
		}
	}
}

/**
 *	This method applies the dyes represented by this class to the
 *	input supermodel. It must be the same as it was created with.
 */
void SuperModelDye::dress( SuperModel & superModel )
{
	// apply property settings to global table
	if (!properties.empty())
	{
		// totally sucks that we have to take this lock here!
		SimpleMutexHolder smh( Model::propCatalogueLock() );

		for (uint i = 0; i < properties.size(); i++)
		{
			Model::DyePropSetting & ds = properties[i];
			(Model::propCatalogueRaw().begin() + ds.index_)->second = ds.value_;
		}
	}

	// now soak the model in our dyes
	for (int i = 0; i < superModel.nModels(); i++)
	{
		Model * cM = superModel.curModel(i);
		if (cM != NULL && index[i] != -1) cM->soak( index[i] );
	}
}

/**
 *	This method returns whether or not this dye is at all effective
 *	on supermodels it was fetched for. Ineffictive dyes (i.e. dyes
 *	whose matter wasn't found in any of the models) are not returned
 *	to the user from SuperModel's getDye ... NULL is returned instead.
 */
bool SuperModelDye::effective( const SuperModel & superModel )
{
	for (int i = 0; i < superModel.nModels(); i++)
	{
		if (index[i] != -1) return true;
	}

	return false;
}



/**
 *	Get the root node. Assumes that all models have the same named
 *	root node as 'Scene Root'
 */
Moo::NodePtr SuperModel::rootNode()
{
	return Model::findNodeInCatalogue( "Scene Root" );
}

/**
 *	Get the node by name.
 */
Moo::NodePtr SuperModel::findNode( const std::string & nodeName,
	MooNodeChain * pParentChain )
{
	// see if any model has this node (and get its node pointer)
	Moo::NodePtr pFound = Model::findNodeInCatalogue( nodeName.c_str() );
	if (!pFound) return NULL;

	// see if one of our models has this node
	Moo::Node * pMaybe = pFound.getObject();
	for (int i = 0; i < nModels_; i++)
	{
		if (models_[i].topModel->hasNode( pMaybe, pParentChain ))
			return pFound;
	}

	// see if it's the root node (bit of a hack for nodeless models)
	if (nodeName == "Scene Root")
	{
		if (pParentChain != NULL) pParentChain->clear();
		return pFound;
	}

	// ok, no such node then
	return NULL;
}


/**
 *	Calculate the bounding box of this supermodel
 */
void SuperModel::boundingBox( BoundingBox & bbRet ) const
{
	if (nModels_ != 0)
	{
		bbRet = models_[0].topModel->boundingBox();

		for (int i = 1; i < nModels_; i++)
		{
			bbRet.addBounds( models_[i].topModel->boundingBox() );
		}
	}
	else
	{
		bbRet = BoundingBox( Vector3(0,0,0), Vector3(0,0,0) );
	}
}

/**
 *	Calculate the bounding box of this supermodel
 */
void SuperModel::visibilityBox( BoundingBox & vbRet ) const
{
	if (nModels_ != 0)
	{
		vbRet = models_[0].topModel->visibilityBox();

		for (int i = 1; i < nModels_; i++)
		{
			vbRet.addBounds( models_[i].topModel->visibilityBox() );
		}
	}
	else
	{
		vbRet = BoundingBox( Vector3(0,0,0), Vector3(0,0,0) );
	}
}

#ifndef CODE_INLINE
#include "super_model.ipp"
#endif
