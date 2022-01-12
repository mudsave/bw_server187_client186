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
#include "item_view.hpp"
#include "ashes/simple_gui.hpp"
#include "ashes/text_gui_component.hpp"
#include "chunk/chunk.hpp"
#include "chunk/chunk_item.hpp"
#include "romp/geometrics.hpp"
#include "gizmo/general_properties.hpp"
#include "chunk/chunk_vlo.hpp"

// -----------------------------------------------------------------------------
// Section: ChunkItemRevealer
// -----------------------------------------------------------------------------

static PySequenceMethods ChunkItemRevealer_seqfns =
{
	ChunkItemRevealer::_pySeq_length,	/* sq_length */
	ChunkItemRevealer::_pySeq_concat,	/* sq_concat */
	0,									/* sq_repeat */
	ChunkItemRevealer::_pySeq_item,		/* sq_item */
	ChunkItemRevealer::_pySeq_slice,	/* sq_slice */
	0,									/* sq_ass_item */
	0,									/* sq_ass_slice */
	ChunkItemRevealer::_pySeq_contains,	/* sq_contains */
	0,									/* sq_inplace_concat */
	0,									/* sq_inplace_repeat */
};

PY_TYPEOBJECT_WITH_SEQUENCE( ChunkItemRevealer, &ChunkItemRevealer_seqfns )

PY_BEGIN_METHODS( ChunkItemRevealer )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( ChunkItemRevealer )
PY_END_ATTRIBUTES()

PY_SCRIPT_CONVERTERS( ChunkItemRevealer )


/**
 *	Get an attribute for python
 */
PyObject * ChunkItemRevealer::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();
	return PyObjectPlus::pyGetAttribute( attr );
}

/**
 *	Set an attribute for python
 */
int ChunkItemRevealer::pySetAttribute( const char * attr, PyObject * value )
{
	PY_SETATTR_STD();
	return PyObjectPlus::pySetAttribute( attr, value );
}


/**
 *	Python sequence length method
 */
int ChunkItemRevealer::pySeq_length()
{
	ChunkItems items;
	this->reveal( items );
	return items.size();
}

/**
 *	Python sequence concat method
 */
PyObject * ChunkItemRevealer::pySeq_concat( PyObject * pOther )
{
	if (!ChunkItemRevealer::Check( pOther ))
	{
		PyErr_SetString( PyExc_TypeError, "ChunkItemRevealer: "
			"Argument to + must be a ChunkItemRevealer" );
		return NULL;
	}

	ChunkItems ourItems;
	this->reveal( ourItems );
	ChunkItems newItems;
	((ChunkItemRevealer*)pOther)->reveal( newItems );

	ChunkItems items = ourItems;
	items.insert( items.end(), newItems.begin(), newItems.end() );
	return new ChunkItemGroup( items );
}


/**
 *	Python sequence index method
 */
PyObject * ChunkItemRevealer::pySeq_item( int index )
{
	ChunkItems items;
	this->reveal( items );
	if (index < 0 || items.size() <= uint(index) )
	{
		PyErr_SetString( PyExc_IndexError,
			"ChunkItemRevealer index out of range" );
		return NULL;
	}

	ChunkItems solo( 1, items[index] );
	return new ChunkItemGroup( solo );
}

/**
 *	Python sequence slice method
 */
PyObject * ChunkItemRevealer::pySeq_slice( int startIndex, int endIndex )
{
	ChunkItems items;
	this->reveal( items );	

	if (startIndex < 0)
		startIndex = 0;

	if (uint(endIndex) > items.size())
		endIndex = items.size();

	if (endIndex < startIndex)
		endIndex = startIndex;

	int length = endIndex - startIndex;
	if (length == items.size())
	{
		Py_INCREF( this );
		return this;
	}

	items.erase( items.begin() + endIndex, items.end() );
	items.erase( items.begin(), items.begin() + startIndex );
	return new ChunkItemGroup( items );
}



/**
 *	See if the given object is in the sequence
 */
int ChunkItemRevealer::pySeq_contains( PyObject * pObject )
{
	if (!ChunkItemRevealer::Check( pObject )) return 0;

	ChunkItems ourItems;
	this->reveal( ourItems );
	ChunkItems othItems;
	((ChunkItemRevealer*)pObject)->reveal( othItems );

	if (othItems.empty()) return 0;	// weird I know

	// check each one then
	for (uint i = 0; i < othItems.size(); i++)
	{
		if (std::find( ourItems.begin(), ourItems.end(), othItems[i] ) ==
			ourItems.end()) return 0;
	}
	return 1;	// all there!
}




// -----------------------------------------------------------------------------
// Section: ChunkItemGroup
// -----------------------------------------------------------------------------

static PySequenceMethods ChunkItemGroup_seqfns =
{
	ChunkItemGroup::_pySeq_length,		/* sq_length */
	ChunkItemGroup::_pySeq_concat,		/* sq_concat */
	0,									/* sq_repeat */
	ChunkItemGroup::_pySeq_item,		/* sq_item */
	ChunkItemGroup::_pySeq_slice,		/* sq_slice */
	ChunkItemGroup::_pySeq_ass_item,	/* sq_ass_item */
	ChunkItemGroup::_pySeq_ass_slice,	/* sq_ass_slice */
	ChunkItemGroup::_pySeq_contains,	/* sq_contains */
	0,									/* sq_inplace_concat */
	0,									/* sq_inplace_repeat */
};

PY_TYPEOBJECT_WITH_SEQUENCE( ChunkItemGroup, &ChunkItemGroup_seqfns )

PY_BEGIN_METHODS( ChunkItemGroup )
	PY_METHOD( add )
	PY_METHOD( rem )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( ChunkItemGroup )
	PY_ATTRIBUTE( size )
PY_END_ATTRIBUTES()

PY_FACTORY( ChunkItemGroup, BigBang )

PY_SCRIPT_CONVERTERS( ChunkItemGroup )


/**
 *	Constructor
 */
ChunkItemGroup::ChunkItemGroup( ChunkItems items, PyTypePlus * pType ) :
	ChunkItemRevealer( pType ),
	items_( items )
{
}

/**
 *	Adds an item to the group
 *
 *	@return true if the item wasn't already there
 */
bool ChunkItemGroup::add( ChunkItemPtr pNewbie )
{
    if (pNewbie && !pNewbie->edCanAddSelection())
        return false;
	ChunkItems::iterator found =
		std::find( items_.begin(), items_.end(), pNewbie );
	if (found == items_.end())
	{
		if( strcmp( pNewbie->edClassName(), "ChunkVLO" ) == 0 )
		{
			VeryLargeObject::updateSelectionMark();
			for( ChunkItems::iterator iter = items_.begin(); iter != items_.end(); ++iter )
				(*iter)->edCheckMark( VeryLargeObject::selectionMark() );
			if( pNewbie->edCheckMark( VeryLargeObject::selectionMark() ) )
				items_.push_back( pNewbie );
		}
		else
			items_.push_back( pNewbie );
		return true;
	}

	return false;
}


/**
 *	Get an attribute for python
 */
PyObject * ChunkItemGroup::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();

	return ChunkItemRevealer::pyGetAttribute( attr );
}

/**
 *	Set an attribute for python
 */
int ChunkItemGroup::pySetAttribute( const char * attr, PyObject * value )
{
	PY_SETATTR_STD();

	return ChunkItemRevealer::pySetAttribute( attr, value );
}


/**
 *	Python sequence length method (overidden for speed)
 */
int ChunkItemGroup::pySeq_length()
{
	return items_.size();
}



/**
 *	Python sequence item assignment method
 */
int ChunkItemGroup::pySeq_ass_item( int index, PyObject * pItem )
{
	// check index in range
	if (index < 0 || uint(index) >= items_.size())
	{
		PyErr_SetString( PyExc_IndexError,
			"ChunkItemGroup assignment index out of range" );
		return -1;
	}

	return this->pySeq_ass_slice( index, index+1, pItem );
}


/**
 *	Python sequence slice assignment method
 */
int ChunkItemGroup::pySeq_ass_slice( int indexA, int indexB, PyObject * pOther )
{
	// make sure we're setting it to a sequence
	if (!ChunkItemRevealer::Check( pOther ))
	{
		PyErr_Format( PyExc_TypeError, "ChunkItemRevealer slices "
			"can only be assigned to a ChunkItemRevealer" );
		return -1;
	}

	ChunkItems newItems;
	((ChunkItemRevealer*)pOther)->reveal( newItems );

	int sz = items_.size();
	int osz = newItems.size();

	// put indices in range (slices don't generate index errors)
	if (indexA > sz ) indexA = sz;
	if (indexA < 0) indexA = 0;
	if (indexB > sz ) indexB = sz;
	if (indexB < 0) indexB = 0;

	// only erase if there's something to erase
	if (indexA < indexB)
		items_.erase( items_.begin() + indexA, items_.begin() + indexB );

	VeryLargeObject::updateSelectionMark();
	for( ChunkItems::iterator iter = items_.begin(); iter != items_.end(); ++iter )
		(*iter)->edCheckMark( VeryLargeObject::selectionMark() );
	for( ChunkItems::iterator iter = newItems.begin(); iter != newItems.end(); ++iter )
		if( strcmp( (*iter)->edClassName(), "ChunkVLO" ) == 0 && !(*iter)->edCheckMark( VeryLargeObject::selectionMark() ) )
		{
			newItems.erase( iter );
			--iter;
		}
	// and add the new ones
	items_.insert( items_.begin() + indexA, newItems.begin(), newItems.end() );

	return 0;
}


/**
 *	Adds the items from the given revealer to this group
 */
PyObject * ChunkItemGroup::py_add( PyObject * args )
{
	PyObject * pToAdd;
	if (!PyArg_ParseTuple( args, "O", &pToAdd) ||
		!ChunkItemRevealer::Check( pToAdd ))
	{
		PyErr_SetString( PyExc_TypeError, "ChunkItemGroup.add "
			"expects a ChunkItemRevealer" );
		return NULL;
	}

	ChunkItemRevealer * pRevealer = static_cast<ChunkItemRevealer*>( pToAdd );

	std::vector< ChunkItemPtr > items;
	pRevealer->reveal( items );

	for (uint i = 0; i < items.size(); i++)
	{
		this->add( items[i] );
	}

	Py_Return;
}

/**
 *	Removes the items from the given revealer from this group
 */
PyObject * ChunkItemGroup::py_rem( PyObject * args )
{
	PyObject * pToDel;
	if (!PyArg_ParseTuple( args, "O", &pToDel) ||
		!ChunkItemRevealer::Check( pToDel ))
	{
		PyErr_SetString( PyExc_TypeError, "ChunkItemGroup.rem "
			"expects a ChunkItemRevealer" );
		return NULL;
	}

	ChunkItemRevealer * pRevealer = static_cast<ChunkItemRevealer*>( pToDel );

	std::vector< ChunkItemPtr > items;
	pRevealer->reveal( items );

	for (uint i = 0; i < items.size(); i++)
	{
		ChunkItems::iterator found =
			std::find( items_.begin(), items_.end(), items[i] );
		if (found != items_.end())
		{
			items_.erase( found );
		}
	}

	Py_Return;
}



/**
 *	Factory method
 */
PyObject * ChunkItemGroup::pyNew( PyObject * args )
{
	if (PyTuple_Size( args ) != 0)
	{
		PyErr_SetString( PyExc_TypeError, "BigBang.ChunkItemGroup() "
			"expects no arguments" );
		return NULL;
	}

	return new ChunkItemGroup();
}


/**
 *	This method reveals the items in this group
 */
void ChunkItemGroup::reveal( std::vector< ChunkItemPtr > & items )
{
	items = items_;
}




// -----------------------------------------------------------------------------
// Section: ChunkItemView
// -----------------------------------------------------------------------------

PY_TYPEOBJECT( ChunkItemView )

PY_BEGIN_METHODS( ChunkItemView )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( ChunkItemView )
	PY_ATTRIBUTE( revealer )
PY_END_ATTRIBUTES()

PY_FACTORY( ChunkItemView, View )

/**
 *	Constructor.
 */
ChunkItemView::ChunkItemView( PyTypePlus * pType ) :
	ToolView( pType )
{
}


/**
 *	Destructor.
 */
ChunkItemView::~ChunkItemView()
{
}


/**
 *	This method draws the appropriate item selected
 */
void ChunkItemView::render( const class Tool& tool )
{
	if (!revealer_) return;

	std::vector< ChunkItemPtr > items;
	revealer_->reveal( items );

	Moo::rc().push();

	for (uint i = 0; i < items.size(); i++)
	{
		ChunkItemPtr cip = items[i];

		if (cip->chunk() == NULL) continue;

		//cip->drawSelected();

		for (int j = 0; j < 4; j++)
		{
			Matrix scaleUp = cip->chunk()->transform();
			scaleUp.translation( scaleUp.applyToOrigin() +
				Vector3(0,1,0 ) );
			Moo::rc().world( scaleUp );
			cip->draw();
		}		
	}

	Moo::rc().pop();
}


/**
 *	Get an attribute for python
 */
PyObject * ChunkItemView::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();

	return ToolView::pyGetAttribute( attr );
}

/**
 *	Set an attribute for python
 */
int ChunkItemView::pySetAttribute( const char * attr, PyObject * value )
{
	PY_SETATTR_STD();

	return ToolView::pySetAttribute( attr, value );
}

/**
 *	Python factory method
 */
PyObject * ChunkItemView::pyNew( PyObject * args )
{
	PyObject * pRevealer = NULL;
	if (!PyArg_ParseTuple( args, "|O", &pRevealer ) ||
		pRevealer && !ChunkItemRevealer::Check( pRevealer ))
	{
		PyErr_SetString( PyExc_TypeError, "ChunkItemView() "
			"expects an optional Revealer argument" );
		return NULL;
	}

	SmartPointer<ChunkItemRevealer> spRevealer =
		static_cast<ChunkItemRevealer*>( pRevealer );

	ChunkItemView * civ = new ChunkItemView();
	civ->revealer_ = spRevealer;
	return civ;
}



// -----------------------------------------------------------------------------
// Section: ChunkItemBounds
// -----------------------------------------------------------------------------

PY_TYPEOBJECT( ChunkItemBounds )

PY_BEGIN_METHODS( ChunkItemBounds )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( ChunkItemBounds )
PY_END_ATTRIBUTES()

PY_FACTORY( ChunkItemBounds, View )


/**
 *	Constructor.
 */
ChunkItemBounds::ChunkItemBounds( PyTypePlus * pType ) :
	ChunkItemView( pType ),
	colour_( 0xffffffff )
{
}


/**
 *	This method draws the appropriate item selected
 */
void ChunkItemBounds::render( const class Tool& tool )
{
	if (!revealer_) return;

	std::vector< ChunkItemPtr > items;
	revealer_->reveal( items );

	Moo::rc().push();

	for (uint i = 0; i < items.size(); i++)
	{
		ChunkItemPtr cip = items[i];

		if (cip->chunk() == NULL) continue;

		BoundingBox bb;

		Moo::rc().setRenderState( D3DRS_LIGHTING, FALSE );

		Moo::rc().world( cip->chunk()->transform() );
		Moo::rc().preMultiply( cip->edTransform() );
		cip->edBounds( bb );
		Geometrics::wireBox( bb, colour_ );
	}

	Moo::rc().pop();
}


/**
 *	Python factory method
 */
ChunkItemBounds * ChunkItemBounds::New(
	SmartPointer<ChunkItemRevealer> spRevealer, uint32 col )
{
	/*
	PyObject * pRevealer = NULL;
	int col = 0xffffffff;

	if (!PyArg_ParseTuple( args, "|Oi", &pRevealer, &col ) ||
		pRevealer && !ChunkItemRevealer::Check( pRevealer ))
	{
		PyErr_SetString( PyExc_TypeError, "ChunkItemBounds() "
			"expects an optional Revealer argument" );
		return NULL;
	}

	SmartPointer<ChunkItemRevealer> spRevealer =
		static_cast<ChunkItemRevealer*>( pRevealer );
*/
	ChunkItemBounds * cib = new ChunkItemBounds();
	cib->revealer_ = spRevealer;
	cib->colour_ = col;
	return cib;
}

std::vector<Chunk*> extractChunks( std::vector<ChunkItemPtr> chunkItems )
{
	std::vector<Chunk*> v;

	for (std::vector<ChunkItemPtr>::iterator i = chunkItems.begin(); i != chunkItems.end(); ++i)
		v.push_back( (*i)->chunk() );

	return v;
}

std::vector<Chunk*> extractChunks( ChunkItemRevealer* revealer )
{
	std::vector<ChunkItemPtr> chunkItems;
	revealer->reveal( chunkItems );
	return extractChunks( chunkItems );
}

// item_view.cpp
