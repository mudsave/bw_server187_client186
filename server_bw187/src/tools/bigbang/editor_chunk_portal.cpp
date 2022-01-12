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

#include "editor_chunk_portal.hpp"

#include "chunk/chunk_space.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_obstacle.hpp"

#include "chunk/chunk_model_obstacle.hpp"
#include "chunk/chunk_model.hpp"

#include "physics2/worldtri.hpp"

#include "resmgr/xml_section.hpp"

#include "appmgr/options.hpp"

#include "item_editor.hpp"
#include "item_properties.hpp"
#include "big_bang.hpp"
#include "appmgr/module_manager.hpp"
#include "project/project_module.hpp"
#include "selection_filter.hpp"
#include "common/material_utility.hpp"
#include "resmgr/auto_config.hpp"
#include <sstream>

static AutoConfigString s_portalSelectionFx( "selectionfx/portal" );
// -----------------------------------------------------------------------------
// Section: PortalObstacle
// -----------------------------------------------------------------------------

/**
 *	This class is the obstacle that a ChunkPortal puts in the collision scene
 */
class PortalObstacle : public ChunkObstacle
{
public:
	PortalObstacle( EditorChunkPortalPtr cpp );
	~PortalObstacle() {}

	virtual bool collide( const Vector3 & source, const Vector3 & extent,
		CollisionState & state ) const;
	virtual bool collide( const WorldTriangle & source, const Vector3 & extent,
		CollisionState & state ) const;

	void buildTriangles();

private:
	EditorChunkPortalPtr	cpp_;
	BoundingBox				bb_;

	mutable std::vector<WorldTriangle>	ltris_;
};


/**
 *	Constructor
 */
PortalObstacle::PortalObstacle( EditorChunkPortalPtr cpp ) :
	ChunkObstacle( cpp->chunk()->transform(), &bb_, cpp ),
	cpp_( cpp )
{
	// now calculate our bb. fortunately the ChunkObstacle constructor
	// doesn't do anything with it except store it.
	const ChunkBoundary::Portal * pPortal = cpp_->pPortal_;
	MF_ASSERT(pPortal);

	// extend 10cm into the chunk (the normal is always normalised)
	Vector3 ptExtra = pPortal->plane.normal() * 0.10f;

	// build up the bb from the portal points
	for (uint i = 0; i < pPortal->points.size(); i++)
	{
		Vector3 pt =
			pPortal->uAxis * pPortal->points[i][0] +
			pPortal->vAxis * pPortal->points[i][1] +
			pPortal->origin;
		if (!i)
			bb_ = BoundingBox( pt, pt );
		else
			bb_.addBounds( pt );
		bb_.addBounds( pt + ptExtra );
	}

	// and figure out the triangles (a similar process)
	this->buildTriangles();
}


/**
 *	Build the 'world' triangles to collide with
 */
void PortalObstacle::buildTriangles()
{
	ltris_.clear();

	const ChunkBoundary::Portal * pPortal = cpp_->pPortal_;
	MF_ASSERT(pPortal);

	// extend 5cm into the chunk
	Vector3 ptExOri = pPortal->origin + pPortal->plane.normal() * 0.05f;

	// build the wt's from the points
	Vector3 pto, pta, ptb(0.f,0.f,0.f);
	for (uint i = 0; i < pPortal->points.size(); i++)
	{
		// shuffle and find the next pt
		pta = ptb;
		ptb =
			pPortal->uAxis * pPortal->points[i][0] +
			pPortal->vAxis * pPortal->points[i][1] +
			ptExOri;

		// stop if we don't have enough for a triangle
		if (i < 2)
		{
			// start all triangles from pt 0.
			if (i == 0) pto = ptb;
			continue;
		}

		// make a triangle then
		ltris_.push_back( WorldTriangle( pto, pta, ptb ) );

		// make a second tri to test the portal both ways 
		// (for mouse selection which is all this is used for)
		ltris_.push_back( WorldTriangle( ptb, pta, pto ) );
	}
}


/**
 *	Collision test with an extruded point
 */
bool PortalObstacle::collide( const Vector3 & source, const Vector3 & extent,
	CollisionState & state ) const
{
	const ChunkBoundary::Portal * pPortal = cpp_->pPortal_;
	MF_ASSERT(pPortal);

	// see if we let anyone through
	//if (pPortal->permissive) return false;

	// check if selectable (unbound and belongo to an indoor chunk)
	if (pChunk()->isOutsideChunk())
	{
		return false;
	}

	// ok, see if they collide then
	// (chances are very high if they're in the bb!)
	Vector3 tranl = extent - source;
	for (uint i = 0; i < ltris_.size(); i++)
	{
		// see if it intersects (both ways)
		float rd = 1.0f;
		if (!ltris_[i].intersects( source, tranl, rd ) ) continue;

		// see how far we really travelled (handles scaling, etc.)
		float ndist = state.sTravel_ + (state.eTravel_-state.sTravel_) * rd;

		if (state.onlyLess_ && ndist > state.dist_) continue;
		if (state.onlyMore_ && ndist < state.dist_) continue;
		state.dist_ = ndist;

		// call the callback function
		ltris_[i].flags( uint8( cpp_->triFlags_ ) );
		int say = state.cc_( *this, ltris_[i], state.dist_ );

		// see if any other collisions are wanted
		if (!say) return true;

		// some are wanted ... see if it's only one side
		state.onlyLess_ = !(say & 2);
		state.onlyMore_ = !(say & 1);
	}

	return false;
}

/**
 *	Collision test with an extruded triangle
 */
bool PortalObstacle::collide( const WorldTriangle & source, const Vector3 & extent,
	CollisionState & state ) const
{
	const ChunkBoundary::Portal * pPortal = cpp_->pPortal_;
	MF_ASSERT(pPortal);

	// see if we let anyone through
	//if (pPortal->permissive) return false;

	// check if selectable (unbound and belongo to an indoor chunk)
	if (pChunk()->isOutsideChunk())
	{
		return false;
	}

	// ok, see if they collide then
	// (chances are very high if they're in the bb!)
	const Vector3 tranl = extent - source.v0();
	for (uint i = 0; i < ltris_.size(); i++)
	{
		// see if it intersects (both ways)
		if (!ltris_[i].intersects( source, tranl ) ) continue;

		// see how far we really travelled
		float ndist = state.sTravel_;

		if (state.onlyLess_ && ndist > state.dist_) continue;
		if (state.onlyMore_ && ndist < state.dist_) continue;
		state.dist_ = ndist;

		// call the callback function
		ltris_[i].flags( uint8( cpp_->triFlags_ ) );
		int say = state.cc_( *this, ltris_[i], state.dist_ );

		// see if any other collisions are wanted
		if (!say) return true;

		// some are wanted ... see if it's only one side
		state.onlyLess_ = !(say & 2);
		state.onlyMore_ = !(say & 1);
	}

	return false;
}



// -----------------------------------------------------------------------------
// Section: namespace Script
// -----------------------------------------------------------------------------

/**
 *	This is a special converter to represent a chunk pointer in python.
 *	It handles 'heaven' and 'earth' pseudo-pointers too.
 */
PyObject * Script::getData( const Chunk * pChunk )
{
	if (uint32(pChunk) > ChunkBoundary::Portal::LAST_SPECIAL )
	{
		std::string fullid = pChunk->identifier() + "@" +
			pChunk->mapping()->name();
		return PyString_FromString( fullid.c_str() );
	}

	switch (uint32(pChunk))
	{
	case 0:
		Py_Return;
	case ChunkBoundary::Portal::HEAVEN:
		return PyString_FromString( "heaven" );
	case ChunkBoundary::Portal::EARTH:
		return PyString_FromString( "earth" );
	case ChunkBoundary::Portal::INVASIVE:
		return PyString_FromString( "invasive" );
	case ChunkBoundary::Portal::EXTERN:
		return PyString_FromString( "extern" );
	}

	return PyString_FromString( "unknown_special" );
}


// -----------------------------------------------------------------------------
// Section: EditorChunkPortal
// -----------------------------------------------------------------------------

PY_TYPEOBJECT( EditorChunkPortal )

PY_BEGIN_METHODS( EditorChunkPortal )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( EditorChunkPortal )
	PY_ATTRIBUTE( home )
	PY_ATTRIBUTE( triFlags )
	PY_ATTRIBUTE( internal )
	PY_ATTRIBUTE( permissive )
	PY_ATTRIBUTE( chunk )
	PY_ATTRIBUTE( points )
	PY_ATTRIBUTE( uAxis )
	PY_ATTRIBUTE( vAxis )
	PY_ATTRIBUTE( origin )
	PY_ATTRIBUTE( lcentre )
	PY_ATTRIBUTE( centre )
	PY_ATTRIBUTE( plane_n )
	PY_ATTRIBUTE( plane_d )
	PY_ATTRIBUTE( label )
PY_END_ATTRIBUTES()



/**
 *	Constructor.
 */
EditorChunkPortal::EditorChunkPortal( ChunkBoundary::Portal * pPortal, PyTypePlus * pType ):
	PyObjectPlus( pType ),
	pPortal_( pPortal ),
	triFlags_( 0 ),
	xform_( Matrix::identity )
{
}


/**
 *	Destructor.
 */
EditorChunkPortal::~EditorChunkPortal()
{
}



/**
 *	Python get attribute method
 */
PyObject * EditorChunkPortal::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();

	return this->PyObjectPlus::pyGetAttribute( attr );
}

/**
 *	Python set attribute method
 */
int EditorChunkPortal::pySetAttribute( const char * attr, PyObject * value )
{
	PY_SETATTR_STD();

	return this->PyObjectPlus::pySetAttribute( attr, value );
}


/**
 *	Get the points that form the boundary of this chunk
 */
PyObject * EditorChunkPortal::pyGet_points()
{
	int sz = pPortal_->points.size();
	PyObject * pTuple = PyTuple_New( sz );

	for (int i = 0; i < sz; i++)
	{
		PyTuple_SetItem( pTuple, i, Script::getData( pPortal_->points[i] ) );
	}

	return pTuple;
}



#include "moo/render_context.hpp"

#include "moo/visual_channels.hpp"
#include "moo/render_context.hpp"

class ProjSetter : public Moo::EffectConstantValue
{
public:
	bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle)
	{
		pEffect->SetMatrix( constantHandle, &Moo::rc().projection() );
		return true;
	}
};

class PortalDrawItem : public Moo::ChannelDrawItem
{
public:
	PortalDrawItem( const Vector3* pRect, Moo::EffectMaterial* pMaterial, uint32 colour )
	: pMaterial_( pMaterial ),
	  colour_( colour )
	{
		memcpy( rect_, pRect, sizeof(Vector3) * 4 );
		distance_ = (pRect[0].z + pRect[1].z + pRect[2].z + pRect[3].z) / 4.f;
		pEffectConstantValue_ = Moo::EffectConstantValue::get( "Projection" );
		setter_ = new ProjSetter;
	}
	~PortalDrawItem()
	{		
	}

	void draw()
	{
		*pEffectConstantValue_ = setter_;
		Moo::rc().setFVF( Moo::VertexXYZL::fvf() );

		Moo::VertexXYZL pVerts[4];			
		for (int i = 0; i < 4; i++)
		{
			pVerts[i].colour_ = colour_;
		}
		pVerts[0].pos_ = rect_[0];
		pVerts[1].pos_ = rect_[1];
		pVerts[2].pos_ = rect_[3];
		pVerts[3].pos_ = rect_[2];

		pMaterial_->begin();
		for ( uint32 i=0; i<pMaterial_->nPasses(); i++ )
		{
			pMaterial_->beginPass(i);   
			Moo::rc().drawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, pVerts, sizeof( pVerts[0] ) );
			pMaterial_->endPass();
		}
		pMaterial_->end();
	}
	void fini()
	{
		delete this;
	}
private:
	Vector3 rect_[4];
	Moo::EffectMaterial* pMaterial_;
	Moo::Colour colour_;
	Moo::EffectConstantValuePtr* pEffectConstantValue_;
	SmartPointer<ProjSetter> setter_;
};

static bool s_drawHeavenAndEarth = false;
/**
 *	overridden edShouldDraw method
 */
bool EditorChunkPortal::edShouldDraw()
{
	if( !ChunkItem::edShouldDraw() )
		return false;
	if( Chunk::hideIndoorChunks_ )
		return false;

	// see if we should draw
	if (Moo::rc().frameTimestamp() != s_settingsMark_)
	{
		s_drawAlways_ = Options::getOptionBool(
			"render/drawChunkPortals", s_drawAlways_ ) &&
			Options::getOptionInt( "render/scenery", 1 );

		bool drawFlagOn = Options::getOptionInt(
			"render/misc/drawHeavenAndEarth", 0 ) != 0;
		drawFlagOn &= !!Options::getOptionInt(
			"render/misc", 0 );
		bool projectModule = ProjectModule::currentInstance() == ModuleManager::instance().currentModule();
		s_drawHeavenAndEarth = (drawFlagOn && !projectModule);

		s_settingsMark_ = Moo::rc().frameTimestamp();
	}
	return s_drawAlways_;
}

/**
 *	Draw method to debug portal states
 */
void EditorChunkPortal::draw()
{
	if( !edShouldDraw() )
		return;

	// get the material
	static Moo::EffectMaterialPtr mym;
	if (mym == NULL)
	{
		mym = new Moo::EffectMaterial();
		mym->load( BWResource::openSection( "resources/materials/editor_chunk_portal.mfm" ));
		MaterialUtility::viewTechnique( mym.getObject(), "editorChunkPortal" );
	}

	// figure out what colour to draw it in
	uint32 colour = 0;
	if ( BigBang::instance().isItemSelected(this) )
	{
		// portal is selected
		colour = 0xff008800;
	}
	else if (pPortal_->pChunk == NULL)
	{
		colour = 0xff000077;
	}
	else if (pPortal_->isExtern())
	{
		colour = 0xff888800;
	}
	else if (!pPortal_->label.empty())
	{
		colour = pPortal_->permissive ? 0xff003300 : 0xff550000;
	}
	// Only draw heaven for outside chunks
	else if (pPortal_->isHeaven() && s_drawHeavenAndEarth)
	{
		colour = 0xffaa00aa;
	}
	else if(chunk() && !chunk()->isOutsideChunk() && SelectionFilter::canSelect( this ))
	{
		colour = 0xff000077;
	}
	else if (pPortal_->isEarth() && s_drawHeavenAndEarth)
	{
		colour = 0xffff0000;
	}

	if (colour == 0) return;

	if( BigBang::instance().drawSelection() )
	{
		Vector3 prect[4];
		for (int i = 0; i < 4; i++)
		{
			prect[i] = Vector3( pPortal_->uAxis * pPortal_->points[i][0] +
				pPortal_->vAxis * pPortal_->points[i][1] + pPortal_->origin );
		}

		static Moo::EffectMaterialPtr selection_mym;
		if (selection_mym == NULL)
		{
			selection_mym = new Moo::EffectMaterial();
			selection_mym->initFromEffect( s_portalSelectionFx );
		}

		Moo::rc().setRenderState( D3DRS_TEXTUREFACTOR, (DWORD)this );

		Moo::rc().setFVF( Moo::VertexXYZ::fvf() );
		Moo::rc().device()->SetTransform( D3DTS_WORLD, &pChunk_->transform() );

		Moo::VertexXYZ pVerts[4];

		pVerts[0].pos_ = prect[0];
		pVerts[1].pos_ = prect[1];
		pVerts[2].pos_ = prect[3];
		pVerts[3].pos_ = prect[2];

		selection_mym->begin();
		for ( uint32 i=0; i<selection_mym->nPasses(); i++ )
		{
			selection_mym->beginPass(i);
			Moo::rc().drawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, pVerts, sizeof( pVerts[0] ) );
			selection_mym->endPass();
		}
		selection_mym->end();

		Moo::rc().device()->SetTransform( D3DTS_WORLD, &Moo::rc().world() );
	}
	else
	{
		// get the transformation matrix
		Matrix tran;
		tran.multiply( Moo::rc().world(), Moo::rc().view() );

		// transform all the points
		Vector3 prect[4];
		for (int i = 0; i < 4; i++)
		{
			// project the point straight into clip space
			tran.applyPoint( prect[i], Vector3(
				pPortal_->uAxis * pPortal_->points[i][0] +
				pPortal_->vAxis * pPortal_->points[i][1] +
				pPortal_->origin ) );
		}

		Moo::SortedChannel::addDrawItem( new PortalDrawItem( prect, mym.getObject(), colour ) );
	}
}


/**
 *	Toss method. Not that this should be called dynamically.
 */
void EditorChunkPortal::toss( Chunk * pChunk )
{
	if (pChunk_ != NULL)
	{
		ChunkPyCache::instance( *pChunk_ ).del( pPortal_->label );
		ChunkModelObstacle::instance( *pChunk_ ).delObstacles( this );
	}

	this->ChunkItem::toss( pChunk );

	if (pChunk_ != NULL)
	{
		ChunkPyCache::instance( *pChunk_ ).add( pPortal_->label, this );
		ChunkModelObstacle::instance( *pChunk_ ).addObstacle(
			new PortalObstacle( this ) );
	}
}

bool EditorChunkPortal::edEdit( ChunkItemEditor & editor )
{
	MatrixProxy * pMP = new ChunkItemMatrix( this );

	editor.addProperty( new StaticTextProperty(
		"position.x",
		new AccessorDataProxy< EditorChunkPortal, StringProxy >(
			this, "position.x", 
			&EditorChunkPortal::getX, 
			NULL ) ) );

	editor.addProperty( new StaticTextProperty(
		"position.y",
		new AccessorDataProxy< EditorChunkPortal, StringProxy >(
			this, "position.y", 
			&EditorChunkPortal::getY, 
			NULL ) ) );

	editor.addProperty( new StaticTextProperty(
		"position.z",
		new AccessorDataProxy< EditorChunkPortal, StringProxy >(
			this, "position.z", 
			&EditorChunkPortal::getZ, 
			NULL ) ) );

	editor.addProperty( new StaticTextProperty(
		"extern", 
		new AccessorDataProxy< EditorChunkPortal, StringProxy >(
			this, "extern", 
			&EditorChunkPortal::getExtern, 
			NULL ) ) );

	editor.addProperty( new StaticTextProperty(
		"otherChunk", 
		new ConstantChunkNameProxy<EditorChunkPortal>( 
			this, &EditorChunkPortal::otherChunk ) ) );

	return true;
}

void EditorChunkPortal::edBounds( BoundingBox& bbRet ) const
{
	const std::vector<Vector2> & points = pPortal_->points;
	bbRet = BoundingBox::s_insideOut_;

	// first find the average in portal space again
	Vector2 avg( 0.f, 0.f );
	for (uint i = 0; i < points.size(); i++)
		avg += points[i];
	avg /= float( points.size() );

	// now build up the bounding box (also in portal space)
	for (uint i = 0; i < points.size(); i++)
	{
		const Vector2 & apt = points[i];
		bbRet.addBounds( Vector3( apt.x - avg.x, apt.y - avg.y, 0.f ) );
	}

	// and add a bit of depth
	bbRet.addBounds( Vector3( 0.f, 0.f, 0.2f ) );
}

const Matrix & EditorChunkPortal::edTransform()
{
	//xform_.setTranslate( pPortal_->lcentre );
	xform_[0] = pPortal_->uAxis;
	xform_[1] = pPortal_->vAxis;
	xform_[2] = pPortal_->plane.normal();
	xform_[3] = pPortal_->lcentre;	// note: not pPortal->origin
	// we use the portal space centre for the matrix translation
	// instead of the origin ... causes a bit of trouble in edBounds
	// above, but overall it keeps everything a bit saner.
	return xform_;
}

/**
 *	need a section name for the selection filter
 */
DataSectionPtr EditorChunkPortal::pOwnSect()
{
	static DataSectionPtr sect = new XMLSection( "portal" );

	return sect;
}

/**
 *	need a section name for the selection filter
 */
const DataSectionPtr EditorChunkPortal::pOwnSect()	const
{
	static DataSectionPtr sect = new XMLSection( "portal" );

	return sect;
}

bool EditorChunkPortal::edSave( DataSectionPtr pSection )
{
	// do nothing, portals are saved with the chunk
	return true;
}

/**
 *	Return "True" if this portal is an external one, otherwise return "False"
 *	@return	"True" if it is external, "False" if not
 */
std::string EditorChunkPortal::getExtern() const
{
	return pPortal_->isExtern() ? "True" : "False";
}

std::string EditorChunkPortal::getLabel() const
{
	return pPortal_->label;
}

bool EditorChunkPortal::setLabel( const std::string & v )
{
	pPortal_->label = v;
	return true;
}

/**
 *	Return x component of the portal's position as an std::string
 *	@return x component of the portal's position as an std::string
 */
std::string EditorChunkPortal::getX() const
{
	Vector3 position = pPortal_->lcentre;

	if (chunk() != NULL)
	{
		position = chunk()->transform().applyPoint( position );
	}

	std::ostringstream oss;
	oss << position.x;
	return oss.str();
}

/**
 *	Return y component of the portal's position as an std::string
 *	@return y component of the portal's position as an std::string
 */
std::string EditorChunkPortal::getY() const
{
	Vector3 position = pPortal_->lcentre;

	if (chunk() != NULL)
	{
		position = chunk()->transform().applyPoint( position );
	}

	std::ostringstream oss;
	oss << position.y;
	return oss.str();
}

/**
 *	Return z component of the portal's position as an std::string
 *	@return z component of the portal's position as an std::string
 */
std::string EditorChunkPortal::getZ() const
{
	Vector3 position = pPortal_->lcentre;

	if (chunk() != NULL)
	{
		position = chunk()->transform().applyPoint( position );
	}

	std::ostringstream oss;
	oss << position.z;
	return oss.str();
}

Chunk * EditorChunkPortal::otherChunk() const
{
	return pPortal_->pChunk;
}


// static initialisers
uint32 EditorChunkPortal::s_settingsMark_ = -16;
bool EditorChunkPortal::s_drawAlways_ = true;



// -----------------------------------------------------------------------------
// Section: ChunkPyCache
// -----------------------------------------------------------------------------


/**
 *	Constructor
 */
ChunkPyCache::ChunkPyCache( Chunk & chunk ) :
	chunk_( chunk ),
	bound_( false )
{
}

/**
 *	Destructor
 */
ChunkPyCache::~ChunkPyCache()
{
}


/**
 *	Add this python object to our list of exposed items for this chunk
 */
void ChunkPyCache::add( const std::string & name, PyObject * pObject )
{
	// this is safe when overwriting
	exposed_[ name ] = pObject;
}

/**
 *	Remove this python object from our list of exposed items for this chunk
 */
void ChunkPyCache::del( const std::string & name )
{
	// this is safe when absent
	exposed_.erase( name );
}


/**
 *	Get the python object with the given name from this chunk
 */
SmartPointer<PyObject> ChunkPyCache::get( const std::string & name )
{
	NamedPyObjects::iterator found = exposed_.find( name );
	return found != exposed_.end() ? found->second : NULL;
}


/**
 *	Static method to get the given chunk inhabitant
 */
/*
PyObject * ChunkPyCache::chunkInhabitant( const std::string & label,
	const std::string & chunk, const std::string & space )
{
	ChunkManager & cm = ChunkManager::instance();

	// look up the space
	ChunkSpacePtr pSpace = NULL;
	if (space.empty())
	{
		pSpace = cm.cameraSpace();
	}
	else
	{
		pSpace = cm.spaceFromName( space );
		if (!pSpace)
		{
			PyErr_Format( PyExc_ValueError, "BigWorld.chunkInhabitant(): "
				"space '%s' not found", space.c_str() );
			return NULL;
		}
	}

	// look up the chunk
	Chunk * pChunk = pSpace->findChunk( chunk );
	if (pChunk == NULL)
	{
		PyErr_Format( PyExc_ValueError, "BigWorld.chunkInhabitant(): "
			"chunk '%s' not found", chunk.c_str() );
		return NULL;
	}

	// look up the inhabitant
	SmartPointer<PyObject> spPyObj =
		ChunkPyCache::instance( *pChunk ).get( label );
	if (!spPyObj)
	{
		PyErr_Format( PyExc_ValueError, "BigWorld.chunkInhabitant(): "
			"no inhabitant with label '%s' found in chunk '%s'",
			label.c_str(), chunk.c_str() );
		return NULL;
	}

	// and return it!
	PyObject * pPyObj = spPyObj.getObject();
	Py_INCREF( pPyObj );
	return pPyObj;
}
PY_MODULE_STATIC_METHOD( ChunkPyCache, chunkInhabitant, BigWorld )
*/


/**
 *	Static method to find a chunk from a point
 */
/*
PyObject * ChunkPyCache::findChunkFromPoint( const Vector3 & point,
	const std::string & space )
{
	ChunkManager & cm = ChunkManager::instance();

	// look up the space
	ChunkSpace * pSpace = NULL;
	if (space.empty())
	{
		pSpace = cm.cameraSpace();
	}
	else
	{
		pSpace = cm.spaceFromName( space );
		if (!pSpace)
		{
			PyErr_Format( PyExc_ValueError, "BigWorld.findChunkFromPoint(): "
				"space '%s' not found", space.c_str() );
			return NULL;
		}
	}

	// ask it to find the chunk
	Chunk * pChunk = pSpace->findChunkFromPoint( point );
	if (!pChunk)
	{
		PyErr_Format( PyExc_ValueError, "BigWorld.chunkInhabitant(): "
			"chunk at (%f,%f,%f) not found", point.x, point.y, point.z );
		return NULL;
	}

	// return the chunk identifier
	return Script::getData( pChunk->identifier() );
}
PY_MODULE_STATIC_METHOD( ChunkPyCache, findChunkFromPoint, BigWorld )
*/


/**
 *	Bind method
 */
void ChunkPyCache::bind( bool looseNotBind )
{
	// only do this once
	if (bound_) return;
	bound_ = true;

	// go through all portals and create items for named ones,
	// whether bound or not.
	ChunkBoundaries::iterator			bit;
	ChunkBoundary::Portals::iterator	pit;

	// go through all our joint boundaries
	for (bit = chunk_.joints().begin(); bit != chunk_.joints().end(); bit++)
	{
		// go through all their bound portals
		for (pit = (*bit)->boundPortals_.begin();
			pit != (*bit)->boundPortals_.end();
			pit++)
		{
			if (!(*pit)->internal)
				chunk_.addStaticItem( ChunkItemPtr(new EditorChunkPortal( *pit ), true) );
		}

		// go through all their unbound portals too
		for (pit = (*bit)->unboundPortals_.begin();
			pit != (*bit)->unboundPortals_.end();
			pit++)
		{
			if (!(*pit)->internal)
				chunk_.addStaticItem( ChunkItemPtr(new EditorChunkPortal( *pit ), true) );
		}
	}
}

/**
 *	Static touch method
 */
void ChunkPyCache::touch( Chunk & chunk )
{
	// make us exist in this chunk
	ChunkPyCache::instance( chunk );
}


/// Static instance accessor initialiser
ChunkCache::Instance<ChunkPyCache> ChunkPyCache::instance;

// editor_chunk_portal.cpp
