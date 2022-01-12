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
#include "editor_chunk_flare.hpp"
#include "editor_chunk_substance.ipp"

#include "big_bang.hpp"
#include "appmgr/options.hpp"
#include "item_editor.hpp"
#include "item_properties.hpp"
#include "romp/super_model.hpp"
#include "chunk/chunk_model.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunks/editor_chunk.hpp"


// -----------------------------------------------------------------------------
// Section: EditorChunkFlare
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
EditorChunkFlare::EditorChunkFlare()
{
}


/**
 *	Destructor.
 */
EditorChunkFlare::~EditorChunkFlare()
{
}


/**
 *	This method saves the data section pointer before calling its
 *	base class's load method
 */
bool EditorChunkFlare::load( DataSectionPtr pSection, Chunk* pChunk, std::string* errorString )
{
	bool ok = this->EditorChunkSubstance<ChunkFlare>::load( pSection, pChunk );
	if (ok)
	{
		flareRes_ = pSection->readString( "resource" );
	}
	else if ( errorString )
	{
		*errorString = "Failed to load flare '" + pSection->readString( "resource" ) + "'";
	}
	return ok;
}



/**
 *	Save any property changes to this data section
 */
bool EditorChunkFlare::edSave( DataSectionPtr pSection )
{
	if (!edCommonSave( pSection ))
		return false;

	pSection->writeString( "resource", flareRes_ );
	pSection->writeVector3( "position", position_ );
	pSection->delChild( "colour" );
	if (colourApplied_) pSection->writeVector3( "colour", colour_ );

	return true;
}


/**
 *	Get the current transform
 */
const Matrix & EditorChunkFlare::edTransform()
{
	transform_.setIdentity();
	transform_.translation( position_ );

	return transform_;
}


/**
 *	Change our transform, temporarily or permanently
 */
bool EditorChunkFlare::edTransform( const Matrix & m, bool transient )
{
	// it's permanent, so find out where we belong now
	Chunk * pOldChunk = pChunk_;
	Chunk * pNewChunk = this->edDropChunk( m.applyToOrigin() );
	if (pNewChunk == NULL) return false;

	// if this is only a temporary change, keep it in the same chunk
	if (transient)
	{
		transform_ = m;
		position_ = transform_.applyToOrigin();
		return true;
	}

	// make sure the chunks aren't readonly
	if (!EditorChunkCache::instance( *pOldChunk ).edIsWriteable() 
		|| !EditorChunkCache::instance( *pNewChunk ).edIsWriteable())
		return false;

	// ok, accept the transform change then
	transform_.multiply( m, pOldChunk->transform() );
	transform_.postMultiply( pNewChunk->transformInverse() );
	position_ = transform_.applyToOrigin();

	// note that both affected chunks have seen changes
	BigBang::instance().changedChunk( pOldChunk );
	BigBang::instance().changedChunk( pNewChunk );

	// and move ourselves into the right chunk. we have to do this
	// even if it's the same chunk so the col scene gets recreated
	pOldChunk->delStaticItem( this );
	pNewChunk->addStaticItem( this );

	return true;
}




/**
 *	This class checkes whether or not a data section is a suitable flare
 *	resource.
 */
class FlareResourceChecker : public ResourceProperty::Checker
{
public:
	virtual bool check( DataSectionPtr pRoot ) const
	{
		return !!pRoot->openSection( "Flare" );
	}

	static FlareResourceChecker instance;
};

FlareResourceChecker FlareResourceChecker::instance;


/**
 *	This helper class wraps up a flare's colour property
 */
class FlareColourWrapper : public UndoableDataProxy<ColourProxy>
{
public:
	explicit FlareColourWrapper( EditorChunkFlarePtr pItem ) :
		pItem_( pItem )
	{
	}

	virtual Moo::Colour EDCALL get() const
	{
		return pItem_->colour();
	}

	virtual void EDCALL setTransient( Moo::Colour v )
	{
		pItem_->colour( v );
	}

	virtual bool EDCALL setPermanent( Moo::Colour v )
	{
		// make it valid
		if (v.r < 0.f) v.r = 0.f;
		if (v.r > 1.f) v.r = 1.f;
		if (v.g < 0.f) v.g = 0.f;
		if (v.g > 1.f) v.g = 1.f;
		if (v.b < 0.f) v.b = 0.f;
		if (v.b > 1.f) v.b = 1.f;
		v.a = 1.f;

		// set it
		this->setTransient( v );

		// flag the chunk as having changed
		BigBang::instance().changedChunk( pItem_->chunk() );

		// update its data section
		pItem_->edSave( pItem_->pOwnSect() );

		return true;
	}

	virtual std::string EDCALL opName()
	{
		return "Set " + pItem_->edDescription() + " colour";
	}

private:
	EditorChunkFlarePtr	pItem_;
};


/**
 *	Add the properties of this flare to the given editor
 */
bool EditorChunkFlare::edEdit( class ChunkItemEditor & editor )
{
	editor.addProperty( new ResourceProperty(
		"flare",
		new SlowPropReloadingProxy<EditorChunkFlare,StringProxy>(
			this, "flare resource", 
			&EditorChunkFlare::flareResGet, 
			&EditorChunkFlare::flareResSet ),
		".xml",
		FlareResourceChecker::instance ) );

	MatrixProxy * pMP = new ChunkItemMatrix( this );
	editor.addProperty( new GenPositionProperty( "position", pMP ) );

	editor.addProperty( new ColourProperty( "colour",
		new FlareColourWrapper( this ) ) );

	// colourApplied_ is set to (colour_ != 'white')

	return true;
}


/**
 *	This method gets our colour as a moo colour
 */
Moo::Colour EditorChunkFlare::colour() const
{
	Vector4 v4col = colourApplied_ ?
		Vector4( colour_ / 255.f, 1.f ) :
		Vector4( 1.f, 1.f, 1.f, 1.f );

	return Moo::Colour( v4col );
}

/**
 *	This method sets our colour (and colourApplied flag) from a moo colour
 */
void EditorChunkFlare::colour( const Moo::Colour & c )
{
	colour_ = Vector3( c.r, c.g, c.b ) * 255.f;
	colourApplied_ = !(c.r == 1.f && c.g == 1.f && c.b == 1.f);
}


/**
 *	Return a modelptr that is the representation of this chunk item
 */
ModelPtr EditorChunkFlare::reprModel() const
{
	static ModelPtr flareModel =
		Model::get( "resources/models/flare.model" );
	return flareModel;
}

/// Write the factory statics stuff
#undef IMPLEMENT_CHUNK_ITEM_ARGS
#define IMPLEMENT_CHUNK_ITEM_ARGS (pSection, pChunk, &errorString)
IMPLEMENT_CHUNK_ITEM( EditorChunkFlare, flare, 1 )


// editor_chunk_flare.cpp
