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
#include "afxcmn.h"
#include "python_adapter.hpp"
#include "pyscript/script.hpp"
#include "resmgr/bwresource.hpp"
#include "limit_slider.hpp"


DECLARE_DEBUG_COMPONENT2( "PythonAdapter", 0 )

PythonAdapter* PythonAdapter::instance_ = NULL;

PythonAdapter::PythonAdapter()
: pScriptObject_(NULL)
, proActive_(true)
{
	MF_ASSERT(!instance_);
	instance_ = this;

	std::string scriptFile = "UIAdapter";

    PyObject * pModule = PyImport_ImportModule(
    		const_cast<char *>( scriptFile.c_str()) );

    if (PyErr_Occurred())
    {
		ERROR_MSG( "PythonAdapter - Failed to init adapter\n" );
        PyErr_Print();
	}

    if (pModule != NULL)
    {
        Py_XDECREF( pScriptObject_ );

        pScriptObject_ = pModule;
        DEBUG_MSG( "PythonAdapter was loaded correctly\n" );
    }
    else
    {
        ERROR_MSG( "PythonAdapter - Could not get module %s\n", scriptFile.c_str() );
        PyErr_Print();
    }
}


PythonAdapter::~PythonAdapter()
{
	MF_ASSERT(instance_);
	instance_ = NULL;
}


PythonAdapter* PythonAdapter::instance()
{
	return instance_; 
}


void PythonAdapter::reloadUIAdapter()
{
	std::string scriptFile = "UIAdapter";

    // get rid of script object
	Py_XDECREF( pScriptObject_ );
	pScriptObject_ = NULL;

	//Check if python has ever loaded the module
	PyObject *modules = PyImport_GetModuleDict();
	PyObject *m;

	// if so, reload it
	if ((m = PyDict_GetItemString(modules,
    		const_cast<char*>( scriptFile.c_str() ))) != NULL &&
	    	PyModule_Check(m))
	{
		pScriptObject_ = PyImport_ReloadModule( m );
	}
	else
	{
		pScriptObject_ = PyImport_ImportModule(
        	const_cast<char*>( scriptFile.c_str() ) );
	}

    if ( pScriptObject_ == NULL )
    {
    	ERROR_MSG( "PythonAdapter - Could not load UIAdapter Module\n" );
        PyErr_Print();
    }
}


bool PythonAdapter::call(const std::string& fnName)
{
	if (!pScriptObject_)
		return false;

	bool handled = false;

	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
							const_cast<char*>( fnName.c_str() ) );

	if ( pFunction != NULL )
	{
		handled = Script::call( pFunction,
							PyTuple_New(0),
							"PythonAdapter::call: " );
	}
	else
	{
		ERROR_MSG( "PythonAdapter::call - no script for [%s]\n", fnName.c_str());
		PyErr_Print();
	}

	return handled;
}


bool PythonAdapter::ActionScriptExecute(const std::string & actionName)
{
	if (!pScriptObject_)
		return false;

	bool handled = false;

	std::string fnName = actionName + "Execute";

	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
							const_cast<char*>( fnName.c_str() ) );

	if ( pFunction != NULL )
	{
		handled = Script::call( pFunction,
							PyTuple_New(0),
							"PythonAdapter::ActionScriptExecute: " );
	}
	else
	{
		// use the default handler
		PyObject * pDefault = PyObject_GetAttrString( pScriptObject_,
							"defaultActionExecute" );

		if (pDefault != NULL)
		{
			handled = Script::call(	pDefault,
							Py_BuildValue( "(s)", actionName.c_str() ),
							"UI::defaultActionExecute: " );
		}
		else
		{
			ERROR_MSG( "PythonAdapter::ActionScriptExecute - "
					"No default action handler (defaultActionExecute).\n" );
			PyErr_Print();
		}
	}

	return handled;
}


bool PythonAdapter::ActionScriptUpdate(const std::string & actionName, int & enabled, int & checked)
{
	if ( !pScriptObject_ )
		return false;

	bool handled = false;

	std::string fnName = actionName + "Update";

	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	PyObject * pResult = NULL;

	if ( pFunction != NULL )
	{
		pResult = Script::ask( pFunction,
				PyTuple_New(0),
				"PythonAdapter::ActionScriptUpdate: " );
	}
	else
	{
		// use the default handler
		PyErr_Clear();
		PyObject * pDefaultFunction =
			PyObject_GetAttrString( pScriptObject_,
				"defaultActionUpdate" );

		if (pDefaultFunction != NULL)
		{
			pResult = Script::ask(
				pDefaultFunction,
				Py_BuildValue( "(s)", actionName.c_str() ),
				"PythonAdapter::ActionScriptUpdate: " );
		}
		else
		{
			PyErr_Clear();
		}
	}

	if (pResult != NULL)
	{
		if (PyArg_ParseTuple( pResult, "ii", &enabled, &checked ))
		{
			handled = true;
		}
		else
		{
			ERROR_MSG( "PythonAdapter::ActionScriptUpdate - "
						"%s did not return a tuple of two integers.\n",
						actionName.c_str() );
			PyErr_Print();
		}

		Py_DECREF( pResult );
	}

	return handled;
}


void PythonAdapter::onBrowserObjectItemSelect( const std::string& theTabName, 
										const std::string& itemName )
{
	if ( !proActive_ || !pScriptObject_ )
		return;

    std::string fnName( "brwObject" );
	fnName += theTabName + "ItemSelect";
    std::string selectedFile( itemName );

    if ( !itemName.empty() )
    {
        // Get the relative path, but preserve the case on the filename
		selectedFile = BWResource::getFilePath(
            BWResource::dissolveFilename( itemName )) +
            BWResource::getFilename( itemName );
    }


	// tell the python which object is selected
	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
                const_cast<char*>( fnName.c_str() ) );

    if (pFunction != NULL)
    {
		Script::call(
	       	pFunction,
            Py_BuildValue( "(s)",
            selectedFile.c_str() ),
            "PythonAdapter::onBrowserObjectItemSelect: " );
    }
    else
	{
		PyErr_Clear();
	    Script::call(
			PyObject_GetAttrString( pScriptObject_,
				"brwObjectItemSelect" ),
			Py_BuildValue( "(ss)",
                theTabName.c_str(),
                selectedFile.c_str() ),
			"PythonAdapter::onBrowserObjectItemSelect: " );
    }


	// update the selection criteria
	std::string tabName = "tabObject" + theTabName;
	Script::call( 
		PyObject_GetAttrString( pScriptObject_,
			"pgcObjectsTabSelect" ),
		Py_BuildValue( "(s)",
			tabName.c_str() ),
		"PythonAdapter::onBrowserObjectItemSelect: " );
}

void PythonAdapter::onBrowserObjectItemAdd()
{
	if ( !proActive_ || !pScriptObject_ )
		return;

	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_, "brwObjectItemAdd");

    if (pFunction != NULL)
    {
		Script::call(
	       	pFunction,
            Py_BuildValue( "()" ),
            "PythonAdapter::onBrowserObjectItemAdd: " );
    }
}

void PythonAdapter::onPageControlTabSelect( const std::string& fnPrefix, const std::string& theTabName )
{
	if ( !proActive_ || !pScriptObject_ )
		return;

    const std::string fnName = fnPrefix + "TabSelect";
	const std::string tabName = "tab" + theTabName;

    PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
                const_cast<char*>( fnName.c_str() ) );

    if ( pFunction != NULL )
    {
        Script::call( pFunction,
                   		Py_BuildValue( "(s)",
               			tabName.c_str() ),
                   		"PythonAdapter::onPageControlTabSelect: " );
    }
    else
	{
		PyErr_Clear();
        Script::call(
            PyObject_GetAttrString( pScriptObject_,
                "pgcAllToolsTabSelect" ),
            Py_BuildValue( "(s)",
                tabName.c_str() ),
            "PythonAdapter::onPageControlTabSelect: " );
    }
}

void PythonAdapter::onBrowserTextureItemSelect( const std::string& itemName )
{
	if ( !proActive_ || !pScriptObject_ )
		return;

	const std::string fnName( "brwTexturesItemSelect" );
    std::string selectedFile( itemName );

    if ( !itemName.empty() )
    {
        // Get the relative path, but preserve the case on the filename
		selectedFile = BWResource::getFilePath(
            BWResource::dissolveFilename( itemName )) +
            BWResource::getFilename( itemName );
    }


	// tell the python which object is selected
	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
                const_cast<char*>( fnName.c_str() ) );

    if (pFunction != NULL)
    {
		Script::call(
	       		pFunction,
				Py_BuildValue( "(s)",
				selectedFile.c_str() ),
				"PythonAdapter::onBrowserTextureItemSelect: " );
    }
    else
	{
		ERROR_MSG( "script call [%s] does not exist\n", fnName.c_str() );
		PyErr_Clear();
    }
}

void PythonAdapter::onSliderAdjust( const std::string& name, float pos, float min, float max )
{
	if ( !proActive_ || !pScriptObject_ )
		return;

	std::string fnName = name + "Adjust";

	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	if ( pFunction != NULL )
	{
		Script::call( pFunction,
			Py_BuildValue(
				"(fff)",
				(float)pos,
				(float)min,
				(float)max ),
			"PythonAdapter::onSliderAdjust: " );
	}
	else
	{
		PyErr_Clear();
		Script::call(
			PyObject_GetAttrString( pScriptObject_,
				"onSliderAdjust" ),
			Py_BuildValue(
				"(sfff)",
				name.c_str(),
				(float)pos,
				(float)min,
				(float)max ),
			"PythonAdapter::onSliderAdjust: " );
	}
}

void PythonAdapter::sliderUpdate( CSliderCtrl* slider, const std::string& sliderName )
{
	if ( !pScriptObject_ )
		return;

    proActive_ = false;
    
	std::string fnName = sliderName + "Update";

	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	PyObject * pResult = NULL;

	if ( pFunction != NULL )
	{
		pResult = Script::ask( pFunction,
				PyTuple_New(0),
				"PythonAdapter::sliderUpdate: " );
	}
	else
	{
		PyErr_Clear();
	}

	if (pResult != NULL)
	{
		if (PyFloat_Check( pResult ))
		{
			slider->SetPos((int)PyFloat_AsDouble( pResult ));
		}
		else if (PyInt_Check( pResult ))
		{
			slider->SetPos((int)PyInt_AsLong( pResult ));
		}
		else
		{
			float value;
			float min;
			float max;

			if (PyArg_ParseTuple( pResult, "fff", &value, &min, &max ))
			{
				slider->SetRangeMin((int)min);
				slider->SetRangeMax((int)max);
				slider->SetPos((int)value);
			}
			else
			{
				ERROR_MSG( "PythonAdapter::sliderUpdate - %s did not return a float (or three).\n",
					sliderName.c_str() );
				PyErr_Clear();
			}
		}

		Py_DECREF( pResult );
	}

    proActive_ = true;
}

void PythonAdapter::onLimitSliderAdjust( const std::string& name, float pos, float min, float max )
{
	if ( !proActive_ || !pScriptObject_ )
		return;

	std::string fnName = name + "Adjust";

	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	if ( pFunction != NULL )
	{
		Script::call( pFunction,
			Py_BuildValue(
				"(fff)",
				(float)pos,
				(float)min,
				(float)max ),
			"PythonAdapter::onSliderAdjust: " );
	}
	else
	{
		PyErr_Clear();
		Script::call(
			PyObject_GetAttrString( pScriptObject_,
				"onSliderAdjust" ),
			Py_BuildValue(
				"(sfff)",
				name.c_str(),
				(float)pos,
				(float)min,
				(float)max ),
			"PythonAdapter::onSliderAdjust: " );
	}
}

void PythonAdapter::limitSliderUpdate( LimitSlider* control, const std::string& controlName )
{
	if ( !pScriptObject_ )
		return;

    proActive_ = false;
    
	std::string fnName = controlName + "Update";

	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	PyObject * pResult = NULL;

	if ( pFunction != NULL )
	{
		pResult = Script::ask( pFunction,
				PyTuple_New(0),
				"PythonAdapter::sliderUpdate: " );
	}
	else
	{
		PyErr_Clear();
	}

	if (pResult != NULL)
	{
		if (PyFloat_Check( pResult ))
		{
			control->setValue((float)PyFloat_AsDouble( pResult ));
		}
		else if (PyInt_Check( pResult ))
		{
			control->setValue((float)PyInt_AsLong( pResult ));
		}
		else
		{
			float value;
			float min;
			float max;

			if (PyArg_ParseTuple( pResult, "fff", &value, &min, &max ))
			{
				control->setRange( min, max );
				control->setValue( value);
			}
			else
			{
				ERROR_MSG( "PythonAdapter::sliderUpdate - %s did not return a float (or three).\n",
					controlName.c_str() );
				PyErr_Clear();
			}
		}

		Py_DECREF( pResult );
	}

    proActive_ = true;
}

void PythonAdapter::onListItemSelect( const std::string& name, int index )
{
	if ( proActive_ && pScriptObject_ )
    {
    	std::string fnName = name + "ItemSelect";

      PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
                	const_cast<char*>( fnName.c_str() ) );

        if ( pFunction != NULL )
        {
        	Script::call(
        		pFunction,
                Py_BuildValue( "(i)", index ),
                "PythonAdapter::onListItemSelect: " );
        }
        else
		{
			PyErr_Clear();
    		Script::call(
				PyObject_GetAttrString( pScriptObject_,
					"onListItemSelect" ),
				Py_BuildValue( "(si)", name.c_str(), index ),
				"PythonAdapter::onListItemSelect: " );
        }
    }
}

void PythonAdapter::onListSelectUpdate( CListBox* listBox, const std::string& listBoxName )
{
	if ( !pScriptObject_ )
		return;

    proActive_ = false;
    
	std::string fnName = listBoxName + "Update";

	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	PyObject * pResult = NULL;

	if ( pFunction != NULL )
	{
		pResult = Script::ask( pFunction,
				PyTuple_New(0),
				"PythonAdapter::onListSelectUpdate: " );
	}
	else
	{
		PyErr_Clear();
	}

	if (pResult != NULL)
	{
		if (PyInt_Check( pResult ))
		{
			if( (int)PyInt_AsLong( pResult ) != -1 )
				listBox->SetCurSel((int)PyInt_AsLong( pResult ));
		}
		else
		{
			ERROR_MSG( "PythonAdapter::onListSelectUpdate - %s did not return a int.\n",
				listBoxName.c_str() );
			PyErr_Clear();
		}

		Py_DECREF( pResult );
	}

    proActive_ = true;
}

void PythonAdapter::selectFilterChange( const std::string& value )
{
	if ( proActive_ && pScriptObject_ )
	{
		const std::string fnName( "cmbSelectFilterChange" );

		PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
					const_cast<char*>( fnName.c_str() ) );

		if ( pFunction != NULL )
		{
			Script::call( pFunction,
					Py_BuildValue( "(s)",
						value.c_str() ),
					"PythonAdapter::selectFilterChange: " );
		}
	}
}

void PythonAdapter::selectFilterUpdate( CComboBox* comboList )
{
	if ( !pScriptObject_ )
		return;

    proActive_ = false;
    
	if (comboList->GetCount() == 0)
		fillFilterKeys( comboList );

	std::string fnName = "cmbSelectFilterUpdate";

	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	PyObject * pResult = NULL;

	if ( pFunction != NULL )
	{
		pResult = Script::ask( pFunction,
				PyTuple_New(0),
				"PythonAdapter::selectFilterUpdate: " );
	}
	else
	{
		PyErr_Clear();
	}

	if (pResult != NULL)
	{
		char * filterValue;
		if (PyArg_ParseTuple( pResult, "s", &filterValue ))
		{
			int index = comboList->FindStringExact( -1, filterValue );
			if( index != comboList->GetCurSel() )
				comboList->SetCurSel( index );
		}
		else
		{
			ERROR_MSG( "PythonAdapter::selectFilterUpdate - "
						"%s did not return a tuple of one string.\n",
						fnName.c_str() );
			PyErr_Clear();
		}

		Py_DECREF( pResult );
	}

    proActive_ = true;
}

void PythonAdapter::fillFilterKeys( CComboBox* comboList )
{
	if ( !pScriptObject_ )
		return;

    proActive_ = false;
    
	std::string fnName = "cmbSelectFilterKeys";

	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	PyObject * pResult = NULL;

	if ( pFunction != NULL )
	{
		pResult = Script::ask( pFunction,
				PyTuple_New(0),
				"PythonAdapter::selectFilterKeys: " );
	}
	else
	{
		PyErr_Clear();
	}

	if (pResult != NULL)
	{
		MF_ASSERT(PyTuple_Check( pResult ));
		int size = PyTuple_Size( pResult );
		for (int i = 0; i < size; i++)
		{
			PyObject * pItem = PyTuple_GetItem( pResult, i );
			MF_ASSERT(PyTuple_Check( pItem ));
			comboList->InsertString( i, PyString_AsString( PyTuple_GetItem( pItem, 0 ) ) );
		}

		Py_DECREF( pResult );
	}

    proActive_ = true;
}

void PythonAdapter::coordFilterChange( const std::string& value )
{
	if ( proActive_ && pScriptObject_ )
	{
		const std::string fnName( "cmbCoordFilterChange" );

		PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
					const_cast<char*>( fnName.c_str() ) );

		if ( pFunction != NULL )
		{
			Script::call( pFunction,
					Py_BuildValue( "(s)",
						value.c_str() ),
					"PythonAdapter::coordFilterChange: " );
		}
	}
}

void PythonAdapter::coordFilterUpdate( CComboBox* comboList )
{
	if ( !pScriptObject_ )
		return;

    proActive_ = false;
    
	if (comboList->GetCount() == 0)
		fillCoordFilterKeys( comboList );

	std::string fnName = "cmbCoordFilterUpdate";

	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	PyObject * pResult = NULL;

	if ( pFunction != NULL )
	{
		pResult = Script::ask( pFunction,
				PyTuple_New(0),
				"PythonAdapter::coordFilterUpdate: " );
	}
	else
	{
		PyErr_Clear();
	}

	if (pResult != NULL)
	{
		char * filterValue;
		if (PyArg_ParseTuple( pResult, "s", &filterValue ))
		{
			int index = comboList->FindStringExact( -1, filterValue );
			comboList->SetCurSel( index == CB_ERR ? 0 : index );
		}
		else
		{
			ERROR_MSG( "PythonAdapter::coordFilterUpdate - "
						"%s did not return a tuple of one string.\n",
						fnName.c_str() );
			PyErr_Clear();
		}

		Py_DECREF( pResult );
	}

    proActive_ = true;
}

void PythonAdapter::fillCoordFilterKeys( CComboBox* comboList )
{
	if ( !pScriptObject_ )
		return;

    proActive_ = false;
    
	std::string fnName = "cmbCoordFilterKeys";

	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	PyObject * pResult = NULL;

	if ( pFunction != NULL )
	{
		pResult = Script::ask( pFunction,
				PyTuple_New(0),
				"PythonAdapter::coordFilterKeys: " );
	}
	else
	{
		PyErr_Clear();
	}

	if (pResult != NULL)
	{
		MF_ASSERT(PyTuple_Check( pResult ));
		int size = PyTuple_Size( pResult );
		for (int i = 0; i < size; i++)
		{
			PyObject * pItem = PyTuple_GetItem( pResult, i );
			MF_ASSERT(PyTuple_Check( pItem ));
			comboList->InsertString( i, PyString_AsString( PyTuple_GetItem( pItem, 0 ) ) );
		}

		Py_DECREF( pResult );
	}

    proActive_ = true;
}

void PythonAdapter::projectLock( const std::string& commitMessage )
{
	const std::string fnName = "projectLock";
	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	if ( pFunction == NULL )
		return;

	Script::call( pFunction, Py_BuildValue( "(s)", commitMessage.c_str() ),
			"PageProject::OnBnClickedProjectSelectionLock: " );
}

void PythonAdapter::commitChanges(const std::string& commitMessage, bool keepLocks)
{
	const std::string fnName = "projectCommitChanges";
	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	if ( pFunction == NULL )
		return;

	Script::call( pFunction, Py_BuildValue( "(si)", commitMessage.c_str(), keepLocks ),
			"PageProject::OnBnClickedProjectCommitAll: " );
}

void PythonAdapter::discardChanges(const std::string& commitMessage, bool keepLocks)
{
	const std::string fnName = "projectDiscardChanges";
	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	if ( pFunction == NULL )
		return;

	Script::call( pFunction, Py_BuildValue( "(si)", commitMessage.c_str(), keepLocks ),
			"PageProject::OnBnClickedProjectDiscardAll: " );
}

void PythonAdapter::updateSpace()
{
	const std::string fnName = "projectUpdateSpace";
	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	if ( pFunction == NULL )
		return;

	Script::call( pFunction, PyTuple_New(0),
			"PageProject::OnBnClickedProjectUpdate: " );
}

void PythonAdapter::calculateMap()
{
	const std::string fnName = "projectCalculateMap";
	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	if ( pFunction != NULL )
	{
		Script::call( pFunction,
				PyTuple_New(0),
				"PythonAdapter::calculateMap: " );
	}
	else
	{
		PyErr_Clear();
	}
}


void PythonAdapter::exportMap()
{
	const std::string fnName = "projectExportMap";
	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	if ( pFunction != NULL )
	{
		Script::call( pFunction,
				PyTuple_New(0),
				"PythonAdapter::exportMap: " );
	}
	else
	{
		PyErr_Clear();
	}
}


bool PythonAdapter::canSavePrefab()
{
	if ( pScriptObject_ )
    {
		const std::string fnName = "canSavePrefab";

		PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
					const_cast<char*>( fnName.c_str() ) );

		if (pFunction != NULL)
		{
			PyObject * pResult = Script::ask(
				pFunction,
				Py_BuildValue( "()" ),
				"PythonAdapter::canSavePrefab: " );
				
			if (pResult != NULL)
			{
				if(PyInt_Check( pResult ))
				{
					return (int)PyInt_AsLong( pResult ) != 0;
				}
				else
				{
					ERROR_MSG( "PythonAdapter::canSavePrefab - canSavePrefab did not return an int.\n" );
					PyErr_Print();
				}
				Py_DECREF( pResult );
			}
		}
		else
		{
			PyErr_Clear();
		}
    }
	return false;
}

void PythonAdapter::saveSelectionPrefab( std::string fileName )
{
	if ( pScriptObject_ )
    {
		const std::string fnName = "savePrefab";

		PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
					const_cast<char*>( fnName.c_str() ) );

		if (pFunction != NULL)
		{
			Script::call(
				pFunction,
				Py_BuildValue( "(s)",
				fileName.c_str() ),
				"PythonAdapter::saveSelectionPrefab: " );
		}
    }
}

std::vector<std::string> PythonAdapter::channelTextureUpdate()
{
	if ( !pScriptObject_ )
		return std::vector<std::string>();

	std::string fnName = "actTerrainTextureUpdate";

	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	PyObject * pResult = NULL;

	if ( pFunction != NULL )
	{
		pResult = Script::ask( pFunction,
				PyTuple_New(0),
				"PythonAdapter::channelTextureUpdate: " );
	}
	else
	{
		PyErr_Clear();
	}

	std::vector<std::string> result;
	if (pResult != NULL)
	{
		MF_ASSERT(PyTuple_Check( pResult ));
		
		int size = PyTuple_Size( pResult );
		for (int i = 0; i < size; i++)
		{
			PyObject * pItem = PyTuple_GetItem( pResult, i );
			MF_ASSERT(PyString_Check( pItem ));
			result.push_back( PyString_AsString( pItem ) );
		}

		Py_DECREF( pResult );
	}
	return result;
}

void PythonAdapter::contextMenuGetItems( const std::string& type, const std::string path,
	std::map<int,std::string>& items )
{
	if ( !pScriptObject_ )
		return;

	const std::string fnName = "contextMenuGetItems";

	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	if (pFunction == NULL)
	{
		PyErr_Clear();
		return;
	}

	PyObject * pResult = Script::ask(
		pFunction,
		Py_BuildValue( "(ss)",
		type.c_str(), path.c_str() ),
		"PythonAdapter::contextMenuGetItems: " );

	if ( pResult == NULL )
		return;

	MF_ASSERT(PyList_Check( pResult ));
	int size = PyList_Size( pResult );
	for ( int i = 0; i < size; i++ )
	{
		PyObject * pItem = PyList_GetItem( pResult, i );
		MF_ASSERT(PyTuple_Check( pItem ));
		MF_ASSERT(PyTuple_Size( pItem ) == 2);
		items.insert( std::pair<int,std::string>(
			PyInt_AS_LONG( PyTuple_GetItem( pItem, 0 ) ),
			PyString_AsString( PyTuple_GetItem( pItem, 1 ) ) ) );
	}

	Py_DECREF( pResult );
}

void PythonAdapter::contextMenuHandleResult( const std::string& type, const std::string path, int result )
{
	if ( !pScriptObject_ )
		return;

	const std::string fnName = "contextMenuHandleResult";

	PyObject * pFunction = PyObject_GetAttrString( pScriptObject_,
				const_cast<char*>( fnName.c_str() ) );

	if (pFunction == NULL)
	{
		PyErr_Clear();
		return;
	}

	Script::call(
		pFunction,
		Py_BuildValue( "(ssi)",
		type.c_str(), path.c_str(), result ),
		"PythonAdapter::contextMenuHandleResult: " );
}
