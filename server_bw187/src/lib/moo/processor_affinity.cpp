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
#include "moo/processor_affinity.hpp"


DECLARE_DEBUG_COMPONENT2( "Moo", 0 )


namespace
{
    unsigned int s_affinity = 1;
}


/**
 *  This sets the affinity for the app to the given processor.  Note that the
 *  processor numbers are 1, 2, 3 ... (there is no 0).
 */
void Moo::processorAffinity(unsigned int processor)
{
    s_affinity = processor;
    SetProcessAffinityMask(GetCurrentProcess(), s_affinity);
}


/**
 *  This gets the processor number that the application is running on.
 */
unsigned int Moo::processorAffinity()
{
    return s_affinity;
}


/**
 *  This makes sure that the process is running on the right processor.
 */
void Moo::updateProcessorAffinity()
{
    processorAffinity(s_affinity);
}
