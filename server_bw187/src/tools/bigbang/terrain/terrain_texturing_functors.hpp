/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef TERRAIN_TEXTURING_FUNCTORS_HPP_
#define TERRAIN_TEXTURING_FUNCTORS_HPP_

#include "terrain_functor.hpp"
#include "pyscript/py_data_section.hpp"
#include <set>

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
	PY_RW_ACCESSOR_ATTRIBUTE_DECLARE( int, blendLevel, channel )

	PY_FACTORY_DECLARE()
private:
	void perChunkFunction( class EditorChunkTerrain& pEct );
	const std::string& sampleTextureName( Tool& tool );
	void HeightPoleAlphaFunctor::nextBlendLevelTexture( Tool& tool );

	int blendLevel_;
	std::string textureName_;
	float falloff_;
	float strength_;

	bool	applying_;

	FUNCTOR_FACTORY_DECLARE( HeightPoleAlphaFunctor() )
};

typedef SmartPointer<HeightPoleAlphaFunctor>	HeightPoleAlphaFunctorPtr;


/**
 * This class applies procedural texturing rules to the
 * underlying terrain.
 */
class ProceduralTexturingFunctor : public HeightPoleFunctor
{
	Py_Header( ProceduralTexturingFunctor, HeightPoleFunctor )
public:
	ProceduralTexturingFunctor( PyTypePlus * pType = &s_type_ );

	bool	load( PyDataSectionPtr pSect );
	void	update( float dTime, Tool& t );
	bool	handleKeyEvent( const KeyEvent & event, Tool& t );
	bool	applying() const					{ return applying_; }
	void	perHeightPoleFunction( class Moo::TerrainBlock::Pole&,
									const Vector3& relativePosition,
									int xIdx, int zIdx );
	void	onFinish( Tool& t )	{};

	PyObject *		pyGetAttribute( const char * attr );
	int				pySetAttribute( const char * attr, PyObject * value );

	PY_AUTO_METHOD_DECLARE( RETDATA, load, ARG( PyDataSectionPtr, END ) );
	PY_RW_ATTRIBUTE_DECLARE( noFalloff_, noFalloff )
	PY_RW_ATTRIBUTE_DECLARE( enabled_, enabled )

	PY_FACTORY_DECLARE()
private:
	void perChunkFunction( class EditorChunkTerrain& pEct );
	bool	applying_;
	float falloff_;
	float strength_;
	bool  noFalloff_;
	bool  enabled_;

	class Rule
	{
	public:
		Rule( int blendLevel );
		bool		load( DataSectionPtr pSection, const std::string& suffix );
		float		match( float value );
		int			blendLevel() const	{ return blendLevel_; }
		const std::string& textureName() const {return textureName_;}
		const std::string& suffix() const		{ return suffix_; }
	private:
		std::string textureName_;
		std::string	suffix_;
		int			blendLevel_;
		float		min_;
		float		max_;
		float		mid_;
		float		rng_;
	};

	typedef std::vector<Rule> Rules;
	Rules rules_;
	std::vector<std::string>  ruleTypes_;

	EditorChunkTerrain* ect_;

	FUNCTOR_FACTORY_DECLARE( ProceduralTexturingFunctor() )
};

typedef SmartPointer<ProceduralTexturingFunctor>	ProceduralTexturingFunctorPtr;

#endif