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
#include "box_attachment.hpp"
#include "romp/custom_mesh.hpp"
#include "moo/material.hpp"
#include "moo/vertex_formats.hpp"

DECLARE_DEBUG_COMPONENT2( "Duplo", 0 )



// -----------------------------------------------------------------------------
// Section: BoxAttachment
// -----------------------------------------------------------------------------

/*~	class BigWorld.BoxAttachment
 *
 *	This class implements a bounding box that can be attached to a node
 *	to perform hit testing
 */
PY_TYPEOBJECT( BoxAttachment )

PY_BEGIN_METHODS( BoxAttachment )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( BoxAttachment )
/*~
 * attribute BoxAttachment.minBounds
 * the minimum bounds of the hit box
 */
	PY_ATTRIBUTE( minBounds )
/*~
 * attribute BoxAttachment.maxBounds
 * the maximum bounds of the hit box
 */
	PY_ATTRIBUTE( maxBounds )
/*~
 * attribute BoxAttachment.name
 * the name of the box attachment
 */
	PY_ATTRIBUTE( name )
/*~
 * attribute BoxAttachment.worldTransform
 * the world transform of the box attachment
 */
	PY_ATTRIBUTE( worldTransform )
/*~
 * attribute BoxAttachment.hit
 * whether the box attachment was hit in the last hit test
 */
	PY_ATTRIBUTE( hit )
PY_END_ATTRIBUTES()

PY_SCRIPT_CONVERTERS( BoxAttachment )


PY_FACTORY( BoxAttachment, BigWorld )

static bool s_displayImpactBoxes = false;
static std::string s_lastBox;


/*
 *	This helper class creates a list of indices mapping from
 *	names to index.
 */
class NameList
{
public:

	typedef std::vector<std::string> Names;

	NameList()
	{
		names_.push_back( "" );
	}
	~NameList()
	{

	}
	const std::string& get( uint16 index )
	{
		return names_[index];
	}
	const uint16 index( const std::string& name )
	{
		uint16 ind = names_.size();
		Names::iterator it = std::find( names_.begin(), names_.end(), name );
		if (it != names_.end())
		{
			ind = it - names_.begin();
		}
		else
		{
			names_.push_back( name );
		}
		return ind;
	}


private:
	Names names_;

};

static NameList s_nameList;


/**
 *	Constructor.
 */
BoxAttachment::BoxAttachment(PyTypePlus * pType) :
	PyAttachment( pType ),
	minBounds_( 0, 0, 0 ),
	maxBounds_( 0, 0, 0 ),
	hit_( false ),
	nameIndex_( 0 )
{
	static bool firstTime = true;
	if (firstTime)
	{
		MF_WATCH( "Client Settings/displayImpactBoxes", s_displayImpactBoxes, Watcher::WT_READ_WRITE, "Display boxes that have been hit with collision tests for this frame." );
		MF_WATCH( "Client Settings/lastBoxHit", s_lastBox, Watcher::WT_READ_WRITE, "The name of the last box hit by a collision test" );
		firstTime = false;
	}
}


/**
 *	Destructor.
 */
BoxAttachment::~BoxAttachment()
{
}


void BoxAttachment::tick( float dTime )
{
}

/**
 *	This method stores the world transform of the box attachment
 *	it also performs debug rendering of the attachment if needed
 *	@param worldTransform the world transform to render at
 *	@param lod unused parameter
 */
void BoxAttachment::draw( const Matrix & worldTransform, float lod )
{
	worldTransform_ = worldTransform;
	static Moo::Material mat;
	if (!mat.numTextureStages())
	{
		Moo::TextureStage ts;
		ts.colourOperation( Moo::TextureStage::SELECTARG2 );
		mat.addTextureStage( ts );
		mat.addTextureStage( Moo::TextureStage() );
		mat.fogged( false );
	}

	if (hit_ && s_displayImpactBoxes)
	{
		const DWORD colours[3] = { 0xff0000, 0x00ff00, 0x0000ff };
		CustomMesh<Moo::VertexXYZL> mesh(D3DPT_TRIANGLELIST);
		for (uint32 dominant = 0; dominant < 3; dominant++)
		{
			uint32 ahead = (dominant + 1) % 3;
			uint32 across = (dominant + 2) % 3;

			Moo::VertexXYZL vert;
			vert.colour_ = colours[ dominant ];
			vert.pos_[dominant] = maxBounds_[dominant];

			//0
			vert.pos_[ahead] = minBounds_[ahead];
			vert.pos_[across] = minBounds_[across];
			mesh.push_back( vert );
			//2
			vert.pos_[ahead] = maxBounds_[ahead];
			vert.pos_[across] = maxBounds_[across];
			mesh.push_back( vert );
			//1
			vert.pos_[ahead] = maxBounds_[ahead];
			vert.pos_[across] = minBounds_[across];
			mesh.push_back( vert );

			//1
			vert.pos_[ahead] = maxBounds_[ahead];
			vert.pos_[across] = minBounds_[across];
			mesh.push_back( vert );
			//2
			vert.pos_[ahead] = maxBounds_[ahead];
			vert.pos_[across] = maxBounds_[across];
			mesh.push_back( vert );
			//3
			vert.pos_[ahead] = minBounds_[ahead];
			vert.pos_[across] = maxBounds_[across];
			mesh.push_back( vert );

			vert.pos_[dominant] = minBounds_[dominant];

			//0
			vert.pos_[ahead] = minBounds_[ahead];
			vert.pos_[across] = minBounds_[across];
			mesh.push_back( vert );
			//2
			vert.pos_[ahead] = maxBounds_[ahead];
			vert.pos_[across] = maxBounds_[across];
			mesh.push_back( vert );
			//1
			vert.pos_[ahead] = minBounds_[ahead];
			vert.pos_[across] = maxBounds_[across];
			mesh.push_back( vert );

			//1
			vert.pos_[ahead] = minBounds_[ahead];
			vert.pos_[across] = maxBounds_[across];
			mesh.push_back( vert );
			//2
			vert.pos_[ahead] = maxBounds_[ahead];
			vert.pos_[across] = maxBounds_[across];
			mesh.push_back( vert );
			//3
			vert.pos_[ahead] = maxBounds_[ahead];
			vert.pos_[across] = minBounds_[across];
			mesh.push_back( vert );
		}
		mat.set();
		DX::Device* pDevice = Moo::rc().device();
		pDevice->SetTransform( D3DTS_VIEW, &Moo::rc().view() );
		pDevice->SetTransform( D3DTS_PROJECTION, &Moo::rc().projection() );
		pDevice->SetTransform( D3DTS_WORLD, &worldTransform );

		// if there's a non-xbox stipple setting, then use it here
		mesh.draw();
	}
}

/**
 * Collide method, works out wether the ray intersects with this box attachment
 *
 * @param start the start point in world space of the ray
 * @param direction the direction of the ray
 * @param distance if this value is > 0 when passed in, it is the distance to 
 *	the end of the ray, it is also the return value for the distance to the
 *	collision impact
 *	@return true if the ray collided
 */
bool BoxAttachment::collide( const Vector3& start, const Vector3& direction, float& distance )
{
	hit_ = false;
	Matrix inv = worldTransform_;
    inv.invert();

	Vector3 lStart = inv.applyPoint( start );
	Vector3 lEnd;
	if (distance > 0)
		lEnd = direction * distance + start;
	else
		lEnd = direction * Moo::rc().camera().farPlane() + start;
	lEnd = inv.applyPoint( lEnd );
	BoundingBox bb;
	bb.setBounds( minBounds_, maxBounds_ );
	if (bb.clip(lStart, lEnd))
	{
		Vector3 end = worldTransform_.applyPoint(lStart);
		distance = (end - start).length();
		hit_ = true;
		s_lastBox = name();
	}
	return hit_;
}

/**
 *	This method saves the box attachment to a datasection
 */
void BoxAttachment::save( DataSectionPtr pSection )
{
	DataSectionPtr pSect = pSection->newSection( "BoxAttachment" );
	pSect->writeString( "name", name() );
	pSect->writeVector3( "minBounds", minBounds_ );
	pSect->writeVector3( "maxBounds", maxBounds_ );
}

/**
 *	This method loads the box attachment from a datasection
 */
void BoxAttachment::load( DataSectionPtr pSection )
{
	name(pSection->readString( "name" ));
	minBounds_ = pSection->readVector3( "minBounds" );
	maxBounds_ = pSection->readVector3( "maxBounds" );
}

/**
 *	This method sets the name of this box attachment
 *	@param name the name of the attachment
 */
void BoxAttachment::name( const std::string& name )
{
	nameIndex_ = s_nameList.index( name );
}

/**
 *	This method gets the name of the box attachment
 *	@return the name of the attachment
 */
const std::string& BoxAttachment::name() const
{
	return s_nameList.get( nameIndex_ );
}

/*
 *	Python method to return the name
 */
PyObject * BoxAttachment::pyGet_name()
{
	return PyString_FromString( name().c_str() );
}

/*
 *	Python method to set the name
 */
int BoxAttachment::pySet_name( PyObject * pValue )
{
	if (PyString_Check( pValue ))
		name( PyString_AsString( pValue ) );
	else
	{
		PyErr_Format( PyExc_TypeError,
			"Bad type for BoxAttachment.name, string expected" );
		return -1;
	}
	return 0;
}


/**
 *	Standard get attribute method.
 */
PyObject * BoxAttachment::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();

	return PyAttachment::pyGetAttribute( attr );
}


/**
 *	Standard set attribute method.
 */
int BoxAttachment::pySetAttribute( const char * attr, PyObject * value )
{
	PY_SETATTR_STD();

	return PyAttachment::pySetAttribute( attr, value );
}


/**
 *	Static python factory method
 */
PyObject * BoxAttachment::pyNew( PyObject * args )
{
	if (PyTuple_Size( args ) != 0)
	{
		PyErr_Format( PyExc_TypeError,
			"BigWorld.BoxAttachment brooks no arguments (%d given)",
			PyTuple_Size( args ) );
		return NULL;
	}

	return new BoxAttachment( );
}

// box_attachment.cpp
