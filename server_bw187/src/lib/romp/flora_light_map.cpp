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
#include "flora_light_map.hpp"
#include "flora.hpp"
#include "moo/terrain_renderer.hpp"
#include "resmgr/auto_config.hpp"
#include "time_of_day.hpp"

static float s_lightU = 0.f;
static float s_lightV = 0.f;


/**
 *	This class exposes a 2x4 matrix to the effect file engine, which
 *	transforms from world position to texture coordinate space.
 */
class FloraLightMapTransformSetter : public Moo::EffectConstantValue, public Aligned
{
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{		
		pEffect->SetVectorArray(constantHandle, (const Vector4*)&worldToLight_, 2);
		return true;
	}

public:
	void worldToLight( const Vector4* tr )
	{		
		worldToLight_[0] = tr[0];
		worldToLight_[1] = tr[1];
	}	

private:
	Vector4 worldToLight_[2];
};


/**
 *	Constructor.
 */
FloraLightMap::FloraLightMap( Flora* flora ):	
	EffectLightMap( "FloraLightMap" ),
	inited_( false ),	
	needsUpdate_( true ),
	flora_( flora )
{
	//Create the watchers once only
	static bool s_createdWatchers = false;
	if (!s_createdWatchers)
	{
		MF_WATCH( "Client Settings/Flora/Light Offset U",
			s_lightU,
			Watcher::WT_READ_WRITE,
			"U offset for calculating the world -> lightmap transform. "
			"Changing this will offset where lighting information is drawn.");
		MF_WATCH( "Client Settings/Flora/Light Offset V",
			s_lightV,
			Watcher::WT_READ_WRITE,
			"V offset for calculating the world -> lightmap transform. "
			"Changing this will offset where lighting information is drawn.");
		s_createdWatchers = true;
	}

	for ( int i=0; i<4; i++ )
		blocks_[i] = NULL;

	this->createUnmanagedObjects();

	// moved initialization to constructor 
	// to avoid loading files while the game 
	// simulation is already running.
	//DataSectionPtr pSection = BWResource::openSection( s_data );
	if (flora_->data())
	{
		if (EffectLightMap::init(flora_->data()->openSection("light_map")))
		{			
			inited_ = true;
		}
	}
}


/**
 *	Destructor.
 */
FloraLightMap::~FloraLightMap()
{
}


void FloraLightMap::createUnmanagedObjects()
{
	// we will have to 
	// recreate the light map
	needsUpdate_ = true;	
}


void FloraLightMap::deleteUnmanagedObjects()
{}


void FloraLightMap::update( float gameTime )
{
	if ( !inited_ )
	{
		return;
	}	

	//Find the 4 relevant terrain blocks.
	Vector3 center = Moo::rc().invView().applyToOrigin();
	Vector3 relative;
	
	//check all 4	
	Vector3 pos( center );
	for ( int i=0; i<4; i++ )
	{
		//note - is important that i=3 gets (-50,-50)
		pos.x = center.x + ((i&1) ? -50.f : 50.f);
		pos.z = center.z + ((i&2) ? -50.f : 50.f);

		Moo::TerrainBlockPtr pBlock = flora_->getTerrainBlock(pos, relative);
		if ( blocks_[i] != pBlock )
		{
			blocks_[i] = pBlock;			
			needsUpdate_ = true;
		}
	}

	//this is a workaround for a weird lighting problem.  If the time of day
	//changes dramatically, e.g. when caps+[ is pressed, the lighting only
	//appears to be updated the next frame.  So we need to wait one frame here.
	needsUpdate_ |= delayedUpdate_;		
	delayedUpdate_ = LightMap::needsUpdate(gameTime);

	if (!needsUpdate_)
		return;	

	if (!material_.get() || !material_->pEffect() || !material_->pEffect()->pEffect())
		return;

	//calculate the world -> light map transform
	//note - from the checking all 4 terrain blocks, relative is left with the relative position
	//of the test point from the origin of the bottom,left block.  This is ensured by the
	//comment above noting that i=3 gets (-50,-50)
	//this translation moves the bottom,left point of the flora to the
	//bottom-left point of the bottom-left terrain block.
	float scale = 1.f/200.f;
	Vector4 worldToLight[2];
	worldToLight[0].set( scale, 0.f, 0.f, scale*(relative.x - (center.x - 50.f) + s_lightU) );
	worldToLight[1].set( 0.f, 0.f, scale, scale*(relative.z - (center.z - 50.f) + s_lightV) );	
	Moo::EffectConstantValue* ecv = &*transformSetter_;
	FloraLightMapTransformSetter* slmts = static_cast<FloraLightMapTransformSetter*>(ecv);	
	slmts->worldToLight( worldToLight );

	if ( pRT_->valid() && pRT_->push() )
	{
		Moo::rc().device()->Clear( 0, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_TARGET, 0x80808080, 1, 0 );

		DX::Viewport oldViewport;
		Moo::rc().device()->GetViewport( &oldViewport );
		DX::Viewport newViewport = oldViewport;
		newViewport.Width /= 2;
		newViewport.Height /= 2;	

		Moo::TerrainRenderer::instance().clear();

		for ( int j=0; j<4; j++ )
		{
			newViewport.X = (j&1) ? 0 : width_/2;
			newViewport.Y = (j&2) ? 0 : height_/2;
			Moo::rc().device()->SetViewport( &newViewport );
			Moo::TerrainBlockPtr pBlock = blocks_[j];
			if ( pBlock )
			{									
				this->setProjection(pBlock);
				Moo::TerrainRenderer::instance().addBlock(Matrix::identity,&*pBlock);
				Moo::TerrainRenderer::instance().draw(material_.get());
				Moo::TerrainRenderer::instance().clear();
			}
		}

		Moo::rc().device()->SetViewport( &oldViewport );
		pRT_->pop();
	}

	needsUpdate_ = false;
}


/**
 *	This method calculates and sets the orthogonal projection matrix on the
 *	effect.
 */
void FloraLightMap::setProjection(Moo::TerrainBlockPtr pValidBlock)
{	
	Matrix m;	
	
	float xExtent = pValidBlock->spacing() * pValidBlock->blocksWidth();
	float zExtent = pValidBlock->spacing() * pValidBlock->blocksHeight();

	LightMap::orthogonalProjection(xExtent,zExtent,m);
	EffectLightMap::setLightMapProjection(m);
}


/**
 *	This method implements the LightMap base class interface and
 *	creates a FloraLightMapTransformSetter.
 */
void FloraLightMap::createTransformSetter()
{
	transformSetter_ = new FloraLightMapTransformSetter;
}