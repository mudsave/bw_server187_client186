/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef TERRAIN_FUNCTOR_HPP_
#define TERRAIN_FUNCTOR_HPP_

#include "gizmo/tool_functor.hpp"
#include "moo/terrain_block.hpp"
#include "pyscript/pyobject_plus.hpp"
#include <set>


/**
 *	This class handles mouse dragging painting, that is, holding down the mouse
 *	and detecting when the mouse buttons has been pressed or released outside
 *	the main view window. This class was created as a fix for bug 5786.
 */
class MouseDragHandler
{
public:
	enum draggingEnum
	{
		KEY_LEFTMOUSE = 0,
		KEY_MIDDLEMOUSE = 1,
		KEY_RIGHTMOUSE = 2
	};

	MouseDragHandler();

	void setDragging( const draggingEnum key, const bool val );
	bool isDragging( const draggingEnum key );
private:
	bool updateDragging( const bool currentState, bool& stateVar ); 
	bool draggingLeftMouse_;
	bool draggingMiddleMouse_;
	bool draggingRightMouse_;
};


/**
 *	This base class applies a function to all height poles.
 */
class HeightPoleFunctor : public ToolFunctor
{
public:
	HeightPoleFunctor( PyTypePlus * pType = &s_type_ );
	virtual bool handleKeyEvent( const KeyEvent & event, Tool& tool );
	virtual bool handleMouseEvent( const MouseEvent & event, Tool& tool ) { return false; };
protected:
	void apply( Tool& t, bool allChunkVertices = false );

	virtual void perChunkFunction( class EditorChunkTerrain& ) = 0;
	virtual void perHeightPoleFunction( class Moo::TerrainBlock::Pole&,
	const Vector3& relativePosition, int xIdx, int zIdx ) = 0;
	virtual void onFinish( Tool& t ) = 0;

	MouseDragHandler dragHandler;
};


/**
 *	This is a utility class that copes with Moo::TerrainBlock's 
 *	alpha blending format.
 *
 *	The 4 blending factors are stored as a packed uint32:
 *		0-255		level 0
 *		128-255		level 1
 *		128-255		level 2
 *		128-255		level 3
 *
 *	These values, treated as floats, must always add up to 1
 */
class PoleAlpha
{
public:
	PoleAlpha( uint32 alpha );

	void addToAlphaLevel( float amount, int level );
	const Vector4& normalisedAlpha() const	{ return alpha_; }
	uint32 alpha();
private:
	Vector4	alpha_;
};

/**
 * This class applies the given textureName to the given
 * blendLevel ( alpha channel ).
 * If channel 2 or 3, then the relevant chunks will have
 * their texture name modified to be consistent with the
 * brush.
 */
class HeightPoleAlphaFunctor : public HeightPoleFunctor
{
	Py_Header( HeightPoleAlphaFunctor, HeightPoleFunctor )
public:
	HeightPoleAlphaFunctor( PyTypePlus * pType = &s_type_ );

	void	update( float dTime, Tool& t );
	bool	handleKeyEvent( const KeyEvent & event, Tool& t );
	bool	applying() const					{ return applying_; }
	void	perHeightPoleFunction( class Moo::TerrainBlock::Pole&,
									const Vector3& relativePosition,
									int xIdx, int zIdx );
	const std::string& textureName() const		{ return textureName_; }
	void	textureName( const std::string& n ) { textureName_ = n; }
	void	onFinish( Tool& t )	{};

	int		blendLevel() const					{ return blendLevel_; }
	void	blendLevel( int level );

	PyObject *		pyGetAttribute( const char * attr );
	int				pySetAttribute( const char * attr, PyObject * value );

	PY_RW_ACCESSOR_ATTRIBUTE_DECLARE( std::string, textureName, texture )
	PY_RW_ATTRIBUTE_DECLARE( channelTexture0_, channelTexture0 )
	PY_RW_ATTRIBUTE_DECLARE( channelTexture1_, channelTexture1 )
	PY_RW_ATTRIBUTE_DECLARE( channelTexture2_, channelTexture2 )
	PY_RW_ATTRIBUTE_DECLARE( channelTexture3_, channelTexture3 )
	PY_RW_ACCESSOR_ATTRIBUTE_DECLARE( int, blendLevel, channel )

	PY_FACTORY_DECLARE()
private:
	void perChunkFunction( class EditorChunkTerrain& pEct );
	const std::string& sampleTextureName( Tool& tool, int level );

	int blendLevel_;
	std::string textureName_;
	std::string channelTexture0_;
	std::string channelTexture1_;
	std::string channelTexture2_;
	std::string channelTexture3_;
	float falloff_;
	float strength_;

	bool	applying_;

	FUNCTOR_FACTORY_DECLARE( HeightPoleAlphaFunctor() )
};

typedef SmartPointer<HeightPoleAlphaFunctor>	HeightPoleAlphaFunctorPtr;



class ChunkItem;
typedef SmartPointer<ChunkItem> ChunkItemPtr;

/**
 *	This class applies a filter to height poles' height
 */
class HeightPoleHeightFilterFunctor : public HeightPoleFunctor
{
	Py_Header( HeightPoleHeightFilterFunctor, HeightPoleFunctor )
public:
	HeightPoleHeightFilterFunctor( PyTypePlus * pType = &s_type_ );
	~HeightPoleHeightFilterFunctor();

	void	update( float dTime, Tool& t );
	bool	handleKeyEvent( const KeyEvent & event, Tool& t );
	bool	applying() const					{ return applying_; }
	void	perChunkFunction( class EditorChunkTerrain& );
	void	perHeightPoleFunction( class Moo::TerrainBlock::Pole&,
									const Vector3& relativePosition,
									int xIdx, int zIdx );
	void	onFinish( Tool& t );

	PyObject *		pyGetAttribute( const char * attr );
	int				pySetAttribute( const char * attr, PyObject * value );

	PY_RW_ATTRIBUTE_DECLARE( applyRate_, applyRate );
	PY_RW_ATTRIBUTE_DECLARE( useFalloff_, falloff );
	PY_RW_ATTRIBUTE_DECLARE( useStrength_, strengthMod );
	PY_RW_ATTRIBUTE_DECLARE( useDTime_, framerateMod );

	PY_RW_ACCESSOR_ATTRIBUTE_DECLARE( int, filterIndex, index )

	int filterIndex() const	{ return filterIndex_; }
	void filterIndex( int i );

	PY_RW_ATTRIBUTE_DECLARE( filter_.constant_, constant )
	PY_RO_ATTRIBUTE_DECLARE( filter_.name_, name )

	PY_FACTORY_DECLARE()

private:
	float	falloff_;
	float	strength_;

	bool	applying_;
	float	appliedFor_;

	float	applyRate_;		// apply this many times per second
	int		useFalloff_;	// should use falloff circle? (no;linear;squared)
	bool	useStrength_;	// should multiply constant by strength?
	bool	useDTime_;		// should multiply constant by dtime?

	int	filterIndex_;

	struct FilterDef
	{
		float	constant_;
		float	kernel_[10];	// 3x3 + 1 for constant
		float	kernelSum_;
		bool	included_;
		std::string name_;
	};
	FilterDef	filter_;

	static std::vector<FilterDef>	s_filters_;

	float	* pHeightPoleCache_;

	typedef std::set<ChunkItemPtr>	ChunkItemSet;
	ChunkItemSet	affectedItems_;

	FUNCTOR_FACTORY_DECLARE( HeightPoleHeightFilterFunctor() )
};

/**
 *	This class sets the height to a constant value
 */
class HeightPoleSetHeightFunctor : public HeightPoleFunctor
{
	Py_Header( HeightPoleSetHeightFunctor, HeightPoleFunctor )
public:
	HeightPoleSetHeightFunctor( PyTypePlus * pType = &s_type_ );
	~HeightPoleSetHeightFunctor();

	void	update( float dTime, Tool& t );
	bool	applying() const					{ return applying_; }
	void	perChunkFunction( class EditorChunkTerrain& pEct );
	void	perHeightPoleFunction( class Moo::TerrainBlock::Pole&,
									const Vector3& relativePosition,
									int xIdx, int zIdx );
	void	onFinish( Tool& t );

	PyObject *		pyGetAttribute( const char * attr );
	int				pySetAttribute( const char * attr, PyObject * value );

	PY_RW_ATTRIBUTE_DECLARE( relative_, relative );
	PY_RW_ATTRIBUTE_DECLARE( height_, height );

	PY_FACTORY_DECLARE()

private:
	float	height_;
	int		relative_;

	bool	applying_;

	typedef std::set<ChunkItemPtr>	ChunkItemSet;
	ChunkItemSet	affectedItems_;

	std::set< std::pair< void*, std::pair<int, int> > > poles_;


	FUNCTOR_FACTORY_DECLARE( HeightPoleSetHeightFunctor() )
};

/**
 *	This class either cuts out or fills in a height poles' hole.
 */
class HeightPoleHoleFunctor : public HeightPoleFunctor
{
	Py_Header( HeightPoleHoleFunctor, HeightPoleFunctor )
public:
	HeightPoleHoleFunctor( PyTypePlus * pType = &s_type_ );

	void	update( float dTime, Tool& t );
	bool	applying() const					{ return applying_; }
	void	perChunkFunction( class EditorChunkTerrain& pEct );
	void	perHeightPoleFunction( class Moo::TerrainBlock::Pole&,
									const Vector3& relativePosition,
									int xIdx, int zIdx );
	void	onFinish( Tool& t ) { }

	PyObject *		pyGetAttribute( const char * attr );
	int				pySetAttribute( const char * attr, PyObject * value );

	PY_RW_ATTRIBUTE_DECLARE( fillNotCut_, fillNotCut )

	PY_FACTORY_DECLARE()
private:
	float	falloff_;

	bool	fillNotCut_;

	bool	applying_;

	FUNCTOR_FACTORY_DECLARE( HeightPoleHoleFunctor() )
};


/**
 * This class generates ecotype information for each height pole.
 */
class HeightPoleEcotypeFunctor : public HeightPoleFunctor
{
	Py_Header( HeightPoleEcotypeFunctor, HeightPoleFunctor )
public:
	HeightPoleEcotypeFunctor( PyTypePlus * pType = &s_type_ );
	~HeightPoleEcotypeFunctor();

	bool	handleKeyEvent( const KeyEvent & event, Tool& t );
	void	perChunkFunction( class EditorChunkTerrain& pEct );
	void	perHeightPoleFunction( class Moo::TerrainBlock::Pole&,
									const Vector3& relativePosition,
									int xIdx, int zIdx );
	void	onFinish( Tool& t );

	//exposed entry point
	void	calculateEcotypes( Tool& t );

	PY_FACTORY_DECLARE()
private:
	///ecotype script object
	PyObject * pScriptObject_;

	void	retrieveScriptObject( PyObject * pModule );
	void	reloadScript();

	EditorChunkTerrain* pEct_;
	std::vector<float> relativeElevations_;
	int		w_;
	int		h_;

	class ProgressDisplay* pProgress_;
	class ProgressTask* pChunkProgress_;

	FUNCTOR_FACTORY_DECLARE( HeightPoleEcotypeFunctor() )
};

typedef SmartPointer<HeightPoleEcotypeFunctor>	HeightPoleEcotypeFunctorPtr;

#endif