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
#include "elevation_modify.hpp"
#include "chunk_overlapper.hpp"
#include "chunk/chunk_manager.hpp"
#include "big_bang.hpp"


/**
 *  This function begins getting accessing to terrain data in chunks.
 *  It turns off background calculation of shadows in BB and switches to
 *  asynchronous mode.
 *
 *  If there was a failure in setting things up then operator bool will
 *  return false (see below).
 */
/*explicit*/ ElevationModify::ElevationModify(bool readOnly /*= false*/)
:
ok_(true),
readOnly_(readOnly)
{
    // Make sure that we can change terrain:
    if (BigBang::instance().checkForReadOnly())
    {
        ok_ = false;
    }
    else
    {
        // Kill BigBang's background calculation so we don't clobber each 
        // other:
        BigBang::instance().stopBackgroundCalculation();
        ChunkOverlapper::drawList.clear();
        ChunkManager::instance().switchToSyncMode(true);
    }
}


/**
 *  This functions sets BB back into synchronous mode and undoes what the
 *  ctor did.
 */
ElevationModify::~ElevationModify()
{
    if (ok_)
    {
	    ChunkManager::instance().switchToSyncMode(false);
	    ChunkOverlapper::drawList.clear();
    }
}


/**
 *  This function returns true if we could get exclusive access to chunks.
 */
ElevationModify::operator bool() const
{
    return ok_;
}
