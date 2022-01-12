/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/


#ifndef TERRAIN_RENDERER_HPP
#define TERRAIN_RENDERER_HPP

#include <iostream>
#include "cstdmf/vectornodest.hpp"
#include "math/matrix.hpp"
#include "device_callback.hpp"
#include "vertex_declaration.hpp"
#include "managed_effect.hpp"

namespace Moo
{

// forward declerations
class TerrainBlock;
typedef SmartPointer<class ShaderSet>		ShaderSetPtr;
class EffectMaterial;
class EffectFileTextureSetter;

/**
 *	This class renders the chunked terrain.
 *	It chooses a rendering method depending on the available hardware.
 *	The terrain chunks adds themselves to this class when they are visible.
 */
class TerrainRenderer : public DeviceCallback
{
public:
	~TerrainRenderer();

	static	TerrainRenderer&	instance();
	static	void				fini();

	void	draw(Moo::EffectMaterial* alternateMaterial=NULL);
	void	addBlock( const Matrix& transform, TerrainBlock * pBlock );
	void	clear();

	bool	canSeeTerrain() const;

	void	createUnmanagedObjects();
	void	deleteUnmanagedObjects();

	static bool s_enableSpecular;
	static void	enableSpecular(bool b)	{s_enableSpecular=b;}

protected:

	typedef std::pair<Matrix, TerrainBlock *>		BlockInPlace;
	typedef AVectorNoDestructor< BlockInPlace >		BlockVector;	

	class Renderer
	{
	public:
		virtual ~Renderer();
		virtual void draw( BlockVector& blocks, Moo::EffectMaterial* alternateMaterial=NULL ) = 0;
	protected:
	};

	class EffectFileRenderer : public Renderer
	{
	public:
		EffectFileRenderer();
		~EffectFileRenderer();

		void			draw( BlockVector& blocks, Moo::EffectMaterial* alternateEffect=NULL );		
		bool			init();
	protected:
		void			setInitialRenderStates();
		void			sortBlocks( BlockVector& blocks );
		void			renderBlocks( BlockVector& blocks );
		void			textureTransform( const Matrix& world, Matrix& ret ) const;
		VertexDeclaration* pDecl_;
		EffectMaterial*	material_;
		EffectFileTextureSetter* textureSetter_;				
	};	

	BlockVector		blocks_;
	Renderer*		renderer_;		
	bool			isVisible_;

	TerrainRenderer();
	TerrainRenderer( const TerrainRenderer& );
	TerrainRenderer& operator=( const TerrainRenderer& );

	static Moo::EffectMacroSetting * s_terrainSpeculerSetting;
	static void configureKeywordSetting(Moo::EffectMacroSetting & setting);
};

} //namespace Moo

#ifdef CODE_INLINE
#include "terrain_renderer.ipp"
#endif

#endif // TERRAIN_RENDERER_HPP
