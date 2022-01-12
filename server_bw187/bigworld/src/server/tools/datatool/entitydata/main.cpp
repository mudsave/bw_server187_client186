/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/
#include "entitydb.hpp"
DECLARE_DEBUG_COMPONENT( 0 )
#include <Python.h>

int main( int argc, char * argv[] )
{
    INFO_MSG( "BEGIN....\n" );

    //Script::init("entities/db", "database");
    Py_Initialize();
    ptest();
    Py_Finalize();

    INFO_MSG( "END...\n" );

    return 0;
}
// main.cpp
