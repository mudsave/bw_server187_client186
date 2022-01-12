/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef BIG_BANG_SCRIPT_HPP
#define BIG_BANG_SCRIPT_HPP

#include <Python.h>

#include "resmgr/datasection.hpp"


/**
 *	This namespace contains functions relating to scripting BigBang.
 */
namespace BigBangScript
{
	bool init( DataSectionPtr pDataSection );
	void fini();

//	void tick( float timeNow );
}


#ifdef CODE_INLINE
#include "big_bang_script.ipp"
#endif


#endif // big_bang_script_hpp
