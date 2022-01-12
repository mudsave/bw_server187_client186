/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef UNDO_REDO_HPP
#define UNDO_REDO_HPP

#include "gizmo/undoredo.hpp"

typedef SmartPointer< class XMLSection > XMLSectionPtr;
typedef std::pair< std::string, std::string > StringPair;

class UndoRedoOp : public UndoRedo::Operation
{
public:
	UndoRedoOp( int kind, DataSectionPtr data, bool materialFlagChange = false, StringPair item = StringPair() );
	~UndoRedoOp();
	virtual void undo();
	virtual bool iseq( const UndoRedo::Operation & oth ) const;
	const DataSectionPtr data() const	{ return data_; }
private:
	int kind_;
	DataSectionPtr data_;

	XMLSectionPtr state_;
	bool materialFlagChange_;
	StringPair item_;
};

#endif // UNDO_REDO_HPP