/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef PROCESSOR_AFFINITY_HPP
#define PROCESSOR_AFFINITY_HPP

namespace Moo
{
    void processorAffinity(unsigned int processor);

    unsigned int processorAffinity();

    void updateProcessorAffinity();
}

#endif // PROCESSOR_AFFINITY_HPP
