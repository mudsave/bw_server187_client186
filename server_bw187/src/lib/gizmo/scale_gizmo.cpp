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
#include "scale_gizmo.hpp"
#include "tool.hpp"
#include "tool_manager.hpp"
#include "general_properties.hpp"
#include "current_general_properties.hpp"
#include "item_functor.hpp"
#include "moo/dynamic_index_buffer.hpp"
#include "moo/dynamic_vertex_buffer.hpp"
#include "moo/effect_visual_context.hpp"
#include "moo/visual_channels.hpp"
#include "input/input.hpp"
#include "resmgr/auto_config.hpp"

extern Moo::Colour g_unlit;
static AutoConfigString s_gizmoVisual( "editor/scaleGizmo" );
extern bool g_showHitRegion;

class ScaleShapePart : public ShapePart
{
public:
	ScaleShapePart( const Moo::Colour& col, int axis, bool isPlane ) 
	:	colour_( col ),
		isPlane_( isPlane ),
		axis_( axis )
	{
//		Vector3 normal( 0, 0, 0 );
//		normal[axis] = 1.f;
//		planeEq_ = PlaneEq( normal, 0 );
	}
/*	ScaleShapePart( const Moo::Colour& col, const Vector3& direction ) 
	:	colour_( col ),
		isPlane_( false ),
		direction_( direction )
	{
	}*/
	
	const Moo::Colour& colour() const { return colour_; }
//	const PlaneEq & plane() const{ return planeEq_; }
//	const Vector3& direction() const { return direction_; };

	bool isPlane() const {return isPlane_;};
	int  axis() const {return axis_;}
private:
	bool		isPlane_;
	Moo::Colour	colour_;
	int			axis_;
};

// -----------------------------------------------------------------------------
// Section: ScaleGizmo
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
ScaleGizmo::ScaleGizmo( MatrixProxyPtr pMatrix, int enablerModifier, float scaleSpeedFactor ) 
:	active_( false ),
	currentPart_( NULL ),
	pMatrix_( pMatrix ),
	drawMesh_( NULL ),
	lightColour_( 0, 0, 0, 0 ),
	enablerModifier_( enablerModifier ),
	scaleSpeedFactor_(scaleSpeedFactor)
{
	if ( !s_gizmoVisual.value().empty() )
	{
		drawMesh_ = Moo::VisualManager::instance()->get( s_gizmoVisual );
	}	

	Matrix m;
	m.setIdentity();
	selectionMesh_.transform( m );

	float radius = 0.2f;
	float boxRadius = 0.4f;
	float length = 3.f;

	Vector3 min( -boxRadius, -boxRadius, length );
	Vector3 max( boxRadius, boxRadius, length + boxRadius*2.f );

	selectionMesh_.addSphere( Vector3( 0, 0, 0 ), radius, 0xffffffff );

	selectionMesh_.addCylinder( Vector3( 0, 0, length ), radius, -length, false, true,0xffff0000, new ScaleShapePart( Moo::Colour( 1, 0, 0, 0 ), 2, false ) );
	selectionMesh_.addBox( min, max, 0xffff0000, new ScaleShapePart( Moo::Colour( 1, 0, 0, 0 ), 2, false ) );
	m.setRotateY( DEG_TO_RAD( 90 ) );
	selectionMesh_.transform( m );
	selectionMesh_.addCylinder( Vector3( 0, 0, length ), radius, -length, false, true,0xff00ff00, new ScaleShapePart( Moo::Colour( 0, 1, 0, 0 ), 0, false ) );
	selectionMesh_.addBox( min, max, 0xff00ff00, new ScaleShapePart( Moo::Colour( 0, 1, 0, 0 ), 0, false ) );
	m.setRotateX( DEG_TO_RAD( -90 ) );
	selectionMesh_.transform( m );
	selectionMesh_.addCylinder( Vector3( 0, 0, length ), radius, -length, false, true,0xff0000ff, new ScaleShapePart( Moo::Colour( 0, 0, 1, 0 ), 1, false ) );
	selectionMesh_.addBox( min, max, 0xff0000ff, new ScaleShapePart( Moo::Colour( 0, 0, 1, 0 ), 1, false ) );

	float offset = length / 6.f;
	float boxSize = length / 12.f;
	float boxHeight = length / 12.f;
	if (drawMesh_ != NULL)
	{
		static float s_boxSize = 0.6f;
		static float s_boxHeight = 0.01f;
		static float s_offset = 0.f;
		static float s_length = 0.f;

		//increase hit region to cover mesh.
		boxHeight = s_boxHeight;
		boxSize = s_boxSize;
		offset = s_offset;
		length = s_length;
	}
	float pos1 = length + offset;
	float pos2 = length + offset + boxSize;
	float pos3 = length + offset + boxSize + boxSize;
	Vector3 min1( pos1, -boxHeight/2.f, pos2 );
	Vector3 max1( pos3, boxHeight/2.f, pos3 );

	Vector3 min2( pos2, -boxHeight/2.f, pos1 );
	Vector3 max2( pos3, boxHeight/2.f, pos2 );

	selectionMesh_.transform( Matrix::identity );
	selectionMesh_.addBox( min1, max1, 0xffffff00, new ScaleShapePart( Moo::Colour( 1, 1, 0, 0 ), 1, true ) );
	selectionMesh_.addBox( min2, max2, 0xffffff00, new ScaleShapePart( Moo::Colour( 1, 1, 0, 0 ), 1, true ) );

	m.setRotateZ( DEG_TO_RAD( 90 ) );
	selectionMesh_.transform( m );
	selectionMesh_.addBox( min1, max1, 0xffff00ff, new ScaleShapePart( Moo::Colour( 1, 0, 1, 0 ), 0, true ) );
	selectionMesh_.addBox( min2, max2, 0xffff00ff, new ScaleShapePart( Moo::Colour( 1, 0, 1, 0 ), 0, true ) );

	m.setRotateX( DEG_TO_RAD( -90 ) );
	selectionMesh_.transform( m );
	selectionMesh_.addBox( min1, max1, 0xff00ffff, new ScaleShapePart( Moo::Colour( 0, 1, 1, 0 ), 2, true ) );
	selectionMesh_.addBox( min2, max2, 0xff00ffff, new ScaleShapePart( Moo::Colour( 0, 1, 1, 0 ), 2, true ) );
}


/**
 *	Destructor.
 */
ScaleGizmo::~ScaleGizmo()
{

}

bool ScaleGizmo::draw( bool force )
{
	active_ = false;
	if ( !force && (InputDevices::modifiers() & enablerModifier_) == 0 )
		return false;
	active_ = true;

	Moo::RenderContext& rc = Moo::rc();
	DX::Device* pDev = rc.device();

	if (drawMesh_)
	{
		Moo::EffectVisualContext::instance().fogEnabled(false);
		rc.setRenderState( D3DRS_FOGENABLE, false );
		Moo::LightContainerPtr pOldLC = rc.lightContainer();
		Moo::LightContainerPtr pLC = new Moo::LightContainer;
		pLC->ambientColour( lightColour_ );
		rc.lightContainer( pLC );
		rc.setPixelShader( NULL );		

		rc.push();
		rc.world( gizmoTransform() );
		drawMesh_->draw();
		rc.pop();

		rc.lightContainer( pOldLC );
		Moo::SortedChannel::draw();
	}
	
	if (!drawMesh_ || g_showHitRegion)
	{
		rc.setRenderState( D3DRS_NORMALIZENORMALS, TRUE );
		Moo::Material::setVertexColour();
		rc.setRenderState( D3DRS_FOGENABLE, FALSE );
		rc.setRenderState( D3DRS_LIGHTING, FALSE );
		rc.setTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
		rc.setTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );
		rc.setTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_TFACTOR );
		rc.setTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
		rc.setTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );

		uint32 tfactor = lightColour_;
		rc.setRenderState( D3DRS_TEXTUREFACTOR, tfactor );

		pDev->SetTransform( D3DTS_WORLD, &gizmoTransform() );
		pDev->SetTransform( D3DTS_VIEW, &Moo::rc().view() );
		pDev->SetTransform( D3DTS_PROJECTION, &Moo::rc().projection() );
		Moo::rc().setPixelShader( NULL );
		Moo::rc().setVertexShader( NULL );
		Moo::rc().setFVF( Moo::VertexXYZND::fvf() );
		
		Moo::DynamicIndexBufferBase& dib = Moo::DynamicIndexBufferBase::instance( D3DFMT_INDEX16 );

		Moo::IndicesReference ind = dib.lock( selectionMesh_.indices().size() );

		if (ind.size())
		{
			ind.fill( &selectionMesh_.indices().front(), selectionMesh_.indices().size() );
			uint32 ibLockIndex = 0;
			dib.unlock();
			ibLockIndex = dib.lockIndex();

			Moo::VertexXYZND* verts = Moo::rc().mixedVertexProcessing()	?	Moo::DynamicSoftwareVertexBuffer< Moo::VertexXYZND >::instance().lock( selectionMesh_.verts().size() )
																		:	Moo::DynamicVertexBuffer< Moo::VertexXYZND >::instance().lock( selectionMesh_.verts().size() );
			if (verts)
			{
				memcpy( verts, &selectionMesh_.verts().front(), sizeof( Moo::VertexXYZND ) * selectionMesh_.verts().size() );
				uint32 vbLockIndex = 0;
				if( Moo::rc().mixedVertexProcessing() )
				{
					Moo::DynamicSoftwareVertexBuffer< Moo::VertexXYZND >::instance().unlock();
					vbLockIndex = Moo::DynamicSoftwareVertexBuffer< Moo::VertexXYZND >::instance().lockIndex();
				}
				else
				{
					Moo::DynamicVertexBuffer< Moo::VertexXYZND >::instance().unlock();
					vbLockIndex = Moo::DynamicVertexBuffer< Moo::VertexXYZND >::instance().lockIndex();
				}

				pDev->SetStreamSource( 0,
					Moo::rc().mixedVertexProcessing()	?	Moo::DynamicSoftwareVertexBuffer< Moo::VertexXYZND >::instance().pVertexBuffer()
														:	Moo::DynamicVertexBuffer< Moo::VertexXYZND >::instance().pVertexBuffer()
					, 0, sizeof( Moo::VertexXYZND ) );
				dib.indexBuffer().set();

				rc.drawIndexedPrimitive( D3DPT_TRIANGLELIST, vbLockIndex, 0, selectionMesh_.verts().size(),
					ibLockIndex, selectionMesh_.indices().size() / 3 );
			}
		}
	}

	return true;
}

bool ScaleGizmo::intersects( const Vector3& origin, const Vector3& direction, float& t )
{
	if ( !active_ )
	{
		currentPart_ = NULL;
		return false;
	}

	lightColour_ = g_unlit;
	Matrix m = gizmoTransform();
	m.invert();
	
	Vector3 lo = m.applyPoint( origin );
	Vector3 ld = m.applyVector( direction );
	float l = ld.length();
	t *= l;
	ld /= l;;

	currentPart_ = (ScaleShapePart*)selectionMesh_.intersects(
		m.applyPoint( origin ),
		m.applyVector( direction ),
		&t );

	t /= l;

	return currentPart_ != NULL;
}

void ScaleGizmo::click( const Vector3& origin, const Vector3& direction )
{
	// handle the click


	if (currentPart_ != NULL) 
	{
		/*ToolViewPtr pView = NULL;
		if ( BigBang::instance().tool() )
		{
			pView = BigBang::instance().tool()->view( "chunkViz" );
		}*/

		ToolFunctorPtr func;
		if (pMatrix_)
			func = ToolFunctorPtr( new MatrixScaler( pMatrix_, scaleSpeedFactor_, &ScaleFloatProxy_ ), true );
		else
			func = ToolFunctorPtr( new PropertyScaler( scaleSpeedFactor_, &ScaleFloatProxy_ ), true );


		if (currentPart_->isPlane())
		{
//			PlaneEq	peq( Vector3( 0.f, 0.f, 0.f ), 0.f );
//			peq = currentPart_->plane();
//			peq = PlaneEq( this->objectTransform().applyToOrigin(), peq.normal() );
			PlaneEq	peq( this->gizmoTransform().applyToOrigin(),
				this->gizmoTransform()[ currentPart_->axis() ] );

			ToolLocatorPtr locator( new PlaneToolLocator( &peq ), true );
			locator->transform( this->gizmoTransform() );
			ToolPtr moveTool( new Tool( locator,
				ToolViewPtr( new FloatVisualiser( &ScaleMatrixProxy_, &ScaleFloatProxy_,
					Moo::Colour( 1, 1, 1, 1 ), "scale", SimpleFormatter::s_def ), true ),
				func
				), true );
			ToolManager::instance().pushTool( moveTool );
		}
		else
		{
			Vector3 dir = this->gizmoTransform()[ currentPart_->axis() ];
			ToolLocatorPtr locator( new LineToolLocator( this->objectTransform().applyToOrigin(), dir), true );
			locator->transform( this->gizmoTransform() );
			ToolPtr moveTool( new Tool(	locator, 
				ToolViewPtr( new FloatVisualiser( &ScaleMatrixProxy_, &ScaleFloatProxy_,
					Moo::Colour( 1, 1, 1, 1 ), "scale", SimpleFormatter::s_def ), true ),
				func
				), true );
			ToolManager::instance().pushTool( moveTool );
		}
	}
}

void ScaleGizmo::rollOver( const Vector3& origin, const Vector3& direction )
{
	// roll it over.
	if (currentPart_ != NULL)
	{
		lightColour_ = currentPart_->colour();
	}
	else
	{
		lightColour_ = g_unlit;
	}
}

Matrix ScaleGizmo::getCoordModifier() const
{
	Matrix coord;
	if( CoordModeProvider::ins()->getCoordMode() == CoordModeProvider::COORDMODE_OBJECT )
		CurrentScaleProperties::properties()[0]->pMatrix()->getMatrix(coord);
	else if( CoordModeProvider::ins()->getCoordMode() == CoordModeProvider::COORDMODE_VIEW )
		coord = Moo::rc().invView();
	else
		coord.setIdentity();
	return coord;
}

Matrix ScaleGizmo::objectTransform() const
{
	Matrix m;

	if (pMatrix_)
	{
		pMatrix_->getMatrix(m);
	}
	else
	{
		m.setTranslate(CurrentPositionProperties::centrePosition());
	}

	return m;
}


Matrix ScaleGizmo::gizmoTransform()
{
	Vector3 pos = this->objectTransform()[3];

	Matrix m = getCoordModifier();

	m[3].setZero();
	m[0].normalise();
	m[1].normalise();
	m[2].normalise();

	float scale = ( Moo::rc().invView()[2].dotProduct( pos ) -
		Moo::rc().invView()[2].dotProduct( Moo::rc().invView()[3] ) );
	if (scale > 0.05)
		scale /= 25.f;
	else
		scale = 0.05f / 25.f;

	ScaleFloatProxy_.set( scale, true );

	Matrix scaleMat;
	scaleMat.setScale( scale, scale, scale );

	m.postMultiply( scaleMat );

	Matrix m2 = objectTransform();
	m2[0].normalise();
	m2[1].normalise();
	m2[2].normalise();

	m2.preMultiply( m );

	ScaleMatrixProxy_.setMatrix( m2 );
	return m2;
}


// scale_gizmo.cpp
