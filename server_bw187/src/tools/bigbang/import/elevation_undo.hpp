/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef ELEVATION_UNDO_HPP
#define ELEVATION_UNDO_HPP


#include "gizmo/undoredo.hpp"
#include <list>


// Use to store grid positions
struct ElevationUndoPos
{
    int16           x_;
    int16           y_;
    float           *data_;

    ElevationUndoPos(int16 x, int16 y);
    ElevationUndoPos(ElevationUndoPos const &other);
    ~ElevationUndoPos();
    ElevationUndoPos &operator=(ElevationUndoPos const &other);
};


class ElevationUndo : public UndoRedo::Operation
{
public:
    typedef std::list<ElevationUndoPos>  ElevationUndoPosList;

    explicit ElevationUndo(ElevationUndoPosList const &positions);

    /*virtual*/ void undo();

    /*virtual*/ bool iseq(UndoRedo::Operation const &other) const;

private:
    ElevationUndoPosList    positions_;
};


#endif // ELEVATION_UNDO_HPP
