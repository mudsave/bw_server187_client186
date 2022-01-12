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
#include "pe_app.hpp"
#include "pyscript/py_data_section.hpp"
#include "appmgr/options.hpp"

PeApp *PeApp::s_instance = NULL;

PeApp::PeApp()
{
    PyObject *module = PyImport_AddModule("ParticleEditor");
    PyObject_SetAttrString
    (
        module, 
        "opts", 
        SmartPointer<PyObject>
        (
            new PyDataSection(Options::pRoot()), 
            true
        ).getObject()
    );
    s_instance = this;
}

PeApp::~PeApp()
{
    PyObject *module = PyImport_AddModule("ParticleEditor");
    PyObject_DelAttrString(module, "opts");
    s_instance = NULL;
}


/*static*/ PeApp &PeApp::instance()
{
    return *s_instance;
}
