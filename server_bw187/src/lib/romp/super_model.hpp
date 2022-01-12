/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef SUPER_MODEL_HPP
#define SUPER_MODEL_HPP


#include "cstdmf/pch.hpp"
#include "cstdmf/smartpointer.hpp"
#include "cstdmf/stringmap.hpp"
#include "cstdmf/concurrency.hpp"
#include "network/basictypes.hpp"	// this include is most unfortunate...
#include "resmgr/datasection.hpp"
#include "moo/moo_math.hpp"
#include "moo/material.hpp"
#include "moo/effect_material.hpp"
#include "physics2/worldtri.hpp"
#include "moo/visual.hpp"

namespace Moo
{
	class BaseTexture;
	class Node;
	typedef SmartPointer<Node> NodePtr;
};

typedef std::vector< Moo::NodePtr > MooNodeChain;

#include <vector>


class BSPTree;
class BoundingBox;
class StaticLightValues;


/**
 *	Helper class for sorting pointers to strings by their string value
 */
class StringOrderProxy
{
public:
	StringOrderProxy( const std::string * pString ) : pString_( pString ) { }

	bool operator <( const StringOrderProxy & other ) const
		{ return (*pString_) < (*other.pString_); }
	bool operator ==( const StringOrderProxy & other ) const
		{ return (*pString_) == (*other.pString_); }
private:
	const std::string * pString_;
};


class AnimLoadCallback : public ReferenceCount
{
public:
	AnimLoadCallback() {}

	virtual void execute() = 0;
};

template< class C >
class AnimLoadFunctor: public AnimLoadCallback
{
public:
	typedef void (C::*Method)();
	
	AnimLoadFunctor( C* instance, Method method ):
		instance_(instance),
		method_(method)
	{}

	void execute()
	{
		if ((instance_) && (method_))
			(instance_->*method_)();
	}
private:
	C* instance_;
	Method method_;
};

/**
 *	This class manages a model. There is only one instance
 *	of this class for each model file, as it represents only
 *	the static data of a model. Dynamic data is stored in the
 *	referencing SuperModel file.
 */
class Model : public SafeReferenceCount
{
public:
	static void clearReportedErrors();
	static SmartPointer<Model> get( const std::string & resourceID,
		bool loadIfMissing = true );
	SmartPointer<Model> reload( DataSectionPtr pFile = NULL, bool reloadChildren = true ) const;

	virtual ~Model();

#ifdef EDITOR_ENABLED
	static void setAnimLoadCallback( SmartPointer< AnimLoadCallback > pAnimLoadCallback ) { s_pAnimLoadCallback_ = pAnimLoadCallback; }
#endif

	virtual bool valid() const = 0;

	void soakDyes();
	virtual void dress() = 0;
	virtual void draw( bool checkBB ) = 0;

	virtual Moo::VisualPtr getVisual() { return NULL; }
	
	virtual const BSPTree * decompose() const		{ return NULL; }

	virtual const BoundingBox & boundingBox() const = 0;
	virtual const BoundingBox & visibilityBox() const = 0;
	virtual bool hasNode( Moo::Node * pNode,
		MooNodeChain * pParentChain ) const		{ return false; }

	const std::string & resourceID() const		{ return resourceID_; }

	// intentionally not returning a smart pointer here 'coz we're
	//  the only ones that should be keeping references to our parent
	//  (when not keeping references to us)
	Model * parent()							{ return parent_.getObject(); }
	float extent() const						{ return extent_; }

	typedef StringHashMap< Moo::NodePtr >	NodeCatalogue;
	static Moo::Node * findOrAddNodeInCatalogue( Moo::NodePtr node );
	static Moo::NodePtr findNodeInCatalogue( const char * identifier );
		
#ifdef NOT_MULTITHREADED
	static NodeCatalogue & nodeCatalogue()		{ return s_nodeCatalogue_; }
#endif

	int getAnimation( const std::string & name );
	void tickAnimation( int index, float dtime, float otime, float ntime );
	void playAnimation( int index, float time, float blendRatio, int flags );

	/**
	 *	Inner class to represent the base Model's animation
	 */
	class Animation : public ReferenceCount
	{
	public:
		Animation();
		virtual ~Animation();

		virtual void tick( float dtime, float otime, float ntime ) { }
		virtual void play( float time = 0.f, float blendRatio = 1.f,
			int flags = 0 ) = 0;

		virtual void flagFactor( int flags, Matrix & mOut ) const
			{ mOut.setIdentity(); }
		virtual const Matrix & flagFactorBit( int bit ) const
			{ return Matrix::identity; }

		virtual uint32 sizeInBytes() const
			{ return sizeof(*this); }

		virtual Moo::AnimationPtr getMooAnim() { return NULL; }

		float	duration_;
		bool	looped_;
	};

	// Don't try to store the pointer this returns, unless you guarantee
	//  the model will be around.
	Animation * lookupLocalAnimation( int index );
	Animation * lookupLocalDefaultAnimation()
		{ return pDefaultAnimation_.getObject(); }

	/**
	 *	Inner class to represent the base Model's action
	 */
	class Action : public ReferenceCount
	{
	public:
		Action( DataSectionPtr pSect );
		~Action();

		uint32 sizeInBytes() const
			{ return sizeof(*this) + name_.length() + animation_.length(); }

		std::string	name_;
		std::string	animation_;
		float		blendInTime_;
		float		blendOutTime_;
		int			track_;
		bool		filler_;
		bool		isMovement_;
		bool		isCoordinated_;
		bool		isImpacting_;

		int			flagSum_;

		struct MatchInfo
		{
			MatchInfo( DataSectionPtr sect );

			struct Constraints
			{
				Constraints( bool matchAll );

				void load( DataSectionPtr sect );

				bool satisfies( const Capabilities & caps,
								float speed,
								float yaw,
								float aux1 ) const
				{
					return caps.match( capsOn, capsOff ) &&
						minEntitySpeed <= speed && speed <= maxEntitySpeed &&
						inRange( yaw, minModelYaw, maxModelYaw ) &&
						inRange( aux1, minEntityAux1, maxEntityAux1 );
				}

				float	minEntitySpeed;
				float	maxEntitySpeed;
				float	minEntityAux1;
				float	maxEntityAux1;
				float	minModelYaw;
				float	maxModelYaw;

				Capabilities	capsOn;
				Capabilities	capsOff;

			private:
				static bool inRange( float angle,
					float minAngle, float maxAngle )
				{
						return (angle < minAngle ?
									angle + 2.f*MATH_PI : angle) <= maxAngle;
				}
			};

			Constraints		trigger;
			Constraints		cancel;

			bool	scalePlaybackSpeed;
			bool	feetFollowDirection;
			bool	oneShot;
			bool	promoteMotion;
		};

		bool		isMatchable_;
		MatchInfo	matchInfo_;
	};

	const Action * getAction( const std::string & name ) const;

#ifdef USE_STRINGMAP
	const Action * lookupLocalAction( int index ) const;
#else
	class ActionsIterator
	{
	public:
		ActionsIterator( const Model * pModel );
		const Action & operator*()	{ return *pModel_->actions_[ index_ ]; }
		const Action * operator->()	{ return &*pModel_->actions_[ index_ ]; }
		void operator++( int );
		bool operator==( const ActionsIterator & other ) const;
		bool operator!=( const ActionsIterator & other ) const;
	private:
		const Model * pModel_;
		int index_;
	};
	friend class ActionsIterator;
	ActionsIterator lookupLocalActionsBegin() const;
	ActionsIterator lookupLocalActionsEnd() const;
#endif


	typedef StringMap< Vector4 > PropCatalogue;
	static int getPropInCatalogue( const char * name );
	static const char * lookupPropName( int index );

	// use with care... lock all accesses to the catalogue
	static PropCatalogue & propCatalogueRaw()	{ return s_propCatalogue_; }
	static SimpleMutex & propCatalogueLock()	{ return s_propCatalogueLock_; }


	/**
	 *	Inner class to represent a dye property
	 */
	class DyeProperty
	{
	public:
		int				index_;
		int				controls_;
		int				mask_;
		int				future_;
		Vector4			default_;
	};

	/**
	 *	Inner class to represent a dye property setting
	 */
	class DyePropSetting
	{
	public:
		DyePropSetting() {}
		DyePropSetting( int i, const Vector4 & v ) : index_( i ), value_( v ) {}

		enum
		{
			PROP_TEXTURE_FACTOR,
			PROP_UV
		};

		int			index_;
		Vector4		value_;
	};
	typedef std::vector< DyePropSetting > DyePropSettings;

	/**
	 *	Inner class to represent a dye selection (for billboards)
	 */
	class DyeSelection
	{
	public:
		std::string		matterName_;
		std::string		tintName_;
		std::vector< DyePropSetting > properties_;
	};
	typedef std::vector< DyeSelection > DyeSelections;

	/**
	 *	Inner class to represent a tint
	 */
	class Tint: public ReferenceCount
	{
	public:
		explicit Tint( bool defaultTint = false );
		~Tint();

		void applyTint();

		// these should probably be put into a union or derived per-model class
		Moo::Material*		oldMaterial_;				// visual
		Moo::EffectMaterial*	newMaterial_;				// newvisual
		std::vector< DyeProperty >		properties_;	// *visual
		DyeSelections	sourceDyes_;					// textural
	private:
		bool default_;
		Tint( const Tint & );
		Tint & operator=( const Tint & );
	};

	typedef SmartPointer< Tint > TintPtr;

	typedef StringMap< TintPtr > Tints;

	/**
	 *	Inner class to represent the base Model's dye definition
	 */
	class Matter
	{
	public:
		Matter();
		Matter( const Matter & other );
		Matter & operator= ( const Matter & other );
		~Matter();

		void emulsify( int tint = 0 );
		void soak();

		Tints			tints_;

		std::string		replaces_;
		std::vector< Moo::Material** >			usesOld_;
		std::vector< Moo::EffectMaterial** >	usesNew_;

	private:
		int		emulsion_;
		int		emulsionCookie_;
	};

	int getDye( const std::string & matter, const std::string & tint,
		Tint ** ppTint = NULL );

	virtual void soak( int dye );

	const std::pair< const char *, Matter > *
		lookupLocalMatter( int index ) const;

	/**
	 *	Inner class to represent material override
	 */
	class MaterialOverride
	{
	public:
		std::string identifier_;
		std::vector< Moo::EffectMaterial** >	effectiveMaterials_;
		std::vector< Moo::EffectMaterial* >		savedMaterials_;
		void update( Moo::EffectMaterial* newMat );
		void reverse();
	};
	typedef StringMap< MaterialOverride >		MaterialOverrides;

public:
	virtual MaterialOverride overrideMaterial( const std::string& identifier, Moo::EffectMaterial* material )
	{
		return MaterialOverride();
	};
	virtual int gatherMaterials( const std::string & materialIdentifier, std::vector< Moo::EffectMaterial** > & uses, const Moo::EffectMaterial ** ppOriginal = NULL )
	{
		return 0;
	}


	/**
	 *	Inner class to represent static lighting
	 */
	class StaticLighting : public ReferenceCount
	{
	public:
		virtual void set() = 0;
		virtual void unset() = 0;
		/** Get the values used for the static lighting */
		virtual StaticLightValues* staticLightValues() = 0;
	};
	typedef SmartPointer<StaticLighting> StaticLightingPtr;

	virtual StaticLightingPtr getStaticLighting( const DataSectionPtr section );
	void setStaticLighting( StaticLightingPtr pLighting );

	uint32 sizeInBytes() const;

	/**
	 *	The NodeTreeData struct.
	 */
	struct NodeTreeData
	{
		NodeTreeData( Moo::Node * pN, int nc ) : pNode( pN ), nChildren( nc ) {}
		Moo::Node	* pNode;
		int			nChildren;
	};
	
	/**
	 *	NodeTreeDataItem struct gives access to the current node data during 
	 *	a node tree traversal (using NodeTreeIterator). The node data can be 
	 *	accessed through the pData member pointer, A nodes's parent world 
	 *	transformation matrix is available through the pParentTransform 
	 *	member pointer. 
	 */
	struct NodeTreeDataItem
	{
		const struct NodeTreeData *	pData;
		const Matrix *				pParentTransform;
	};

	/**
	 *	Use the NodeTreeIterator class to traverse the model's node hierarchy 
	 *	(skeleton). The class models a unidirectional iterator and allows 
	 *	linear traversal of the node tree, providing access to each node in 
	 *	the tree through the NodeTreeDataItem class (returned by the and -> 
	 *	operators). See pymodel.cpp for and example of how to use the iterator.
	 */
	class NodeTreeIterator
	{
	public:
		NodeTreeIterator( 
			const NodeTreeData * begin, 
			const NodeTreeData * end, 
			const Matrix * root );
		
		const NodeTreeDataItem & operator*() const;
		const NodeTreeDataItem * operator->() const;
		
		void operator++( int );
		bool operator==( const NodeTreeIterator & other ) const;
		bool operator!=( const NodeTreeIterator & other ) const;

	private:
		const NodeTreeData *	curNodeData_;
		const NodeTreeData *	endNodeData_;
		NodeTreeDataItem		curNodeItem_;
		int	sat_;

		struct 
		{
			const Matrix *	trans;
			int				pop;
		} stack_[128];
	};
	
	virtual NodeTreeIterator nodeTreeBegin() const;
	virtual NodeTreeIterator nodeTreeEnd() const;

protected:
	static Model * load( const std::string & resourceID, DataSectionPtr pFile );
	Model( const std::string & resourceID, DataSectionPtr pFile );

	virtual int initMatter( Matter & m )					  { return 0; }
	virtual bool initTint( Tint & t, DataSectionPtr matSect ) { return false; }

	int  readDyes( DataSectionPtr pFile, bool multipleMaterials );
	bool readSourceDye( DataSectionPtr pSourceDye, DyeSelection & dsel );
	void postLoad( DataSectionPtr pFile );

	friend class ModelMap;
	friend class ModelPLCB;

	

	std::string				resourceID_;
	SmartPointer<Model>		parent_;
	float					extent_;

#ifdef USE_STRINGMAP
	typedef StringMap< SmartPointer<Animation> >	Animations;
	Animations				animations_;
#else
	typedef std::vector< SmartPointer<Animation> >	Animations;
	Animations				animations_;
	typedef std::map< std::string, int >	AnimationsIndex;
	AnimationsIndex			animationsIndex_;
#endif

	SmartPointer<Animation>			pDefaultAnimation_;

#ifdef EDITOR_ENABLED
	static SmartPointer< AnimLoadCallback >		s_pAnimLoadCallback_;
#endif

#ifdef USE_STRINGMAP
	typedef StringMap< SmartPointer<Action> >	Actions;
	Actions					actions_;
#else
	typedef std::vector< SmartPointer<Action> >	Actions;
	typedef std::map< StringOrderProxy, int >	ActionsIndex;
	Actions					actions_;
	ActionsIndex			actionsIndex_;
#endif

//#ifdef USE_STRINGMAP
	typedef StringMap< Matter >		Matters;
//#else
//	typedef std::map< std::string, Matter >		Matters;
//#endif
	Matters					matters_;


	static NodeCatalogue	s_nodeCatalogue_;
	static SimpleMutex		s_nodeCatalogueLock_;

	static PropCatalogue	s_propCatalogue_;
	static SimpleMutex		s_propCatalogueLock_;

	static std::vector< std::string > s_warned_; 

public:

	// incremented first thing in each SuperModel::draw call.
	static int				s_blendCookie_;
};

typedef SmartPointer<Model> ModelPtr;



/**
 *	This abstract class represents a fashion in which a supermodel
 *	may be drawn. It could be an animation at a certain time
 *	and blend ratio, or it could be a material override, or
 *	it could be a morph with associated parameters
 */
class Fashion : public ReferenceCount
{
public:
	Fashion();
	virtual ~Fashion();

protected:
	virtual void dress( class SuperModel & superModel ) = 0;
	virtual void undress( class SuperModel & superModel ){};

	friend class SuperModel;
};

typedef SmartPointer<Fashion> FashionPtr;
typedef std::vector<FashionPtr> FashionVector;

// so supermodelfashions are animations, material overrides, morphs, etc.
// ... and actions now too.

/**
 *	This class represents an animation from a SuperModel perspective.
 *
 *	@note This is a variable-length class, so don't try to make these
 *		at home, kids.
 */
class SuperModelAnimation : public Fashion
{
public:
	float	time;
	float	lastTime;
	float	blendRatio;

	/*const*/ Model::Animation * pSource( SuperModel & superModel ) const;

	void tick( SuperModel & superModel, float dtime );

private:
	int		index[1];

	SuperModelAnimation( SuperModel & superModel, const std::string & name );
	friend class SuperModel;

	virtual void dress( SuperModel & superModel );
};

typedef SmartPointer<SuperModelAnimation> SuperModelAnimationPtr;


/**
 *	This class represents an action from a SuperModel perspective.
 */
class SuperModelAction : public Fashion
{
public:
	float	passed;		///< Real time passed since action started
	float	played;		///< Effective time action has played at full speed
	float	finish;		///< Real time action should finish (-ve => none)
	float	lastPlayed;	///< Value of played when last ticked
	const Model::Action * pSource;	// source action (first if multiple)
	const Model::Animation * pFirstAnim;	// top model first anim

	float blendRatio() const;

	void tick( SuperModel & superModel, float dtime );

private:
	int		index[1];	// same deal as SuperModelAnimation

	SuperModelAction( SuperModel & superModel, const std::string & name );
	friend class SuperModel;

	virtual void dress( SuperModel & superModel );
};

typedef SmartPointer<SuperModelAction> SuperModelActionPtr;



/**
 *	This class represents a material override from the SuperModel
 *	perspective
 */
class SuperModelDye : public Fashion
{
public:
	Model::DyePropSettings		properties;

private:
	int		index[1];	// same deal as SuperModelAnimation

	SuperModelDye( SuperModel & superModel,
		const std::string & matter, const std::string & tint );
	friend class SuperModel;

	virtual void dress( SuperModel & superModel );

	bool	effective( const SuperModel & superModel );
};

typedef SmartPointer<SuperModelDye> SuperModelDyePtr;

/**
 *	This class defines a super model. It's not just a model,
 *	it's a whole lot more - discrete level of detail, billboards,
 *	continuous level of detail, static mesh animations, multi-part
 *	animated models - you name it, this baby's got it.
 */
class SuperModel
{
public:
	explicit SuperModel( const std::vector< std::string > & modelIDs );
	~SuperModel();

	// If you want to do stuff between the dressing and drawing,
	//  then make a class that derives from fashion and pass it in.
	float draw( const FashionVector * pFashions = NULL,
		int nLateFashions = 0, float atLod = -1.f, bool doDraw = true );

	void dressInDefault();

	Moo::NodePtr rootNode();
	Moo::NodePtr findNode( const std::string & nodeName,
		MooNodeChain * pParentChain = NULL );

	SuperModelAnimationPtr	getAnimation( const std::string & name );
	SuperModelActionPtr		getAction( const std::string & name );
	void getMatchableActions( std::vector<SuperModelActionPtr> & actions );
	SuperModelDyePtr		getDye( const std::string & matter, const std::string & tint );

	int nModels() const						{ return nModels_; }
	Model * curModel( int i )				{ return models_[i].curModel; }
	ModelPtr topModel( int i )				{ return models_[i].topModel; }

	float lastLod() const					{ return lod_; }

	void boundingBox( BoundingBox & bbRet ) const;
	void visibilityBox( BoundingBox & vbRet ) const;

	void checkBB( bool checkit = true )		{ checkBB_ = checkit; }
	void redress()							{ redress_ = true; }

	bool isOK() const						{ return isOK_; }

private:
	struct ModelStuff
	{
		ModelPtr	  topModel;
		Model		* preModel;
		Model		* curModel;
	};
	std::vector<ModelStuff>		models_;

	int							nModels_;

	float						lod_;
	float						lodNextUp_;
	float						lodNextDown_;

	bool	checkBB_;
	bool	redress_;	// HACK!

	bool	isOK_;
};



#ifdef CODE_INLINE
#include "super_model.ipp"
#endif


#endif // SUPER_MODEL_HPP
