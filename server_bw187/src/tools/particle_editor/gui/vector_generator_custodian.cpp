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
#include "resource.h"
#include "gui/vector_generator_custodian.hpp"

using namespace std;

namespace
{
    string LoadString(int id)
    {
        CString result;
        result.LoadString(id);
        return result.GetBuffer();
    }
}

string VecGenGUIStrFromID(string const &str)
{
    if (str == PointVectorGenerator::nameID_.c_str())
        return LoadString(IDS_POINT);
    else if (str == LineVectorGenerator::nameID_.c_str())
        return LoadString(IDS_LINE);
    else if (str == CylinderVectorGenerator::nameID_.c_str())
        return LoadString(IDS_CYLINDER);
    else if (str == SphereVectorGenerator::nameID_.c_str())
        return LoadString(IDS_SPHERE);
    else if (str == BoxVectorGenerator::nameID_.c_str())
        return LoadString(IDS_BOX);
    else
        return string();
}

string VecGenIDFromGuiStr(string const &str)
{
    if (str == LoadString(IDS_POINT))
        return PointVectorGenerator::nameID_.c_str();
    else if (str == LoadString(IDS_LINE))
        return LineVectorGenerator::nameID_.c_str();
    else if (str == LoadString(IDS_CYLINDER))
        return CylinderVectorGenerator::nameID_.c_str();
    else if (str == LoadString(IDS_SPHERE))
        return SphereVectorGenerator::nameID_.c_str();
    else if (str == LoadString(IDS_BOX))
        return BoxVectorGenerator::nameID_.c_str();
    else
        return string();
}
