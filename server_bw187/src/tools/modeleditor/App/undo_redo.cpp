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

#include "resmgr/xml_section.hpp"

#include "App/me_app.hpp"

#include "undo_redo.hpp"


/**
 *	Constructor
 */
UndoRedoOp::UndoRedoOp( int kind, DataSectionPtr data, bool materialFlagChange /*= false*/, StringPair item /*= StringPair()*/ ):
	UndoRedo::Operation(kind),
	kind_(kind),
	data_(data),
	state_(NULL),
	materialFlagChange_(materialFlagChange),
	item_(item)
{
	if (data_ != NULL)
	{
		state_ = new XMLSection("undo_state");
		state_->copy( data_ );
	}
}


/**
 *	Destructor
 */
UndoRedoOp::~UndoRedoOp()
{
	data_ = NULL;
}


/**
 *	This method is called by the underlying undo system.
 *	Here we load our stored XML data into the model.
 */
void UndoRedoOp::undo()
{
	// first add the current state of this block to the undo/redo list
	UndoRedo::instance().add( new UndoRedoOp( kind_, data_, materialFlagChange_, item_));

	if (data_ != NULL)
	{
		//Remove the current data
		data_->delChildren();
				
		//Copy the old state back
		data_->copy( state_ );

		if ( item_ != StringPair() )
		{
			MeApp::instance().mutant()->cleanAnim( item_ );
		}

		MeApp::instance().mutant()->reloadAllLists();
		
		if (materialFlagChange_)
		{
			MeApp::instance().mutant()->reloadBSP();
		}
	}
}


/**
 *	This method checks whether two UndoRedo operations are the same,
 *	so duplicate entries can be discarded.  It simply checks if the
 *	data in the XML sections are the same.  It ignores the state of
 *	the gui, since that is for user convenience, but not so the user
 *	can undo/redo changes in list boxes etc.
 */
bool UndoRedoOp::iseq( const UndoRedo::Operation & oth ) const
{
	return false;
}