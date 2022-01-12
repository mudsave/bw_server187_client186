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

#include "model_editor.h"
#include "me_shell.hpp"
#include "material_properties.hpp"

#include "appmgr/options.hpp"
#include "appmgr/commentary.hpp"

#include "pyscript/py_data_section.hpp"

DECLARE_DEBUG_COMPONENT( 0 )
#include "me_error_macros.hpp"

#include "me_app.hpp"

MeApp* MeApp::s_instance_ = NULL;

MeApp::MeApp()
{
	ASSERT(s_instance_ == NULL);
	s_instance_ = this;
	
	//We need to call this so that we can set material properties
	runtimeInitMaterialProperties();

	bool groundModel = !!Options::getOptionInt( "settings/groundModel", 0 );
	bool centreModel = !!Options::getOptionInt( "settings/centreModel", 0 );

	floor_ = new Floor ( Options::getOptionString("settings/floorTexture", "" ) );
	mutant_ = new Mutant( groundModel, centreModel );
	lights_ = new Lights;

	blackLight_ = new Moo::LightContainer;
	blackLight_->ambientColour( Moo::Colour( 0.f ,0.f, 0.f, 1.f ) );

	whiteLight_ = new Moo::LightContainer;
	whiteLight_->ambientColour( Moo::Colour( 1.f, 1.f, 1.f, 1.f ) );

	initCamera();

	//Expose the options.xml to python, this will be used for nearly all settings changes
	PyObject * pMod = PyImport_AddModule( "ModelEditor" );
	PyObject_SetAttrString( pMod, "opts", SmartPointer<PyObject>(
		new PyDataSection( Options::pRoot() ), true ).getObject() );

}

MeApp::~MeApp()
{
	delete floor_;
	delete mutant_;
	delete lights_;

	s_instance_ = NULL;
}

void MeApp::initCamera()
{
	camera_ = new ToolsCamera();
	camera_->windowHandle( MeShell::instance().hWndGraphics() );
	std::string speedName = Options::getOptionString( "camera/speed" );
	camera_->speed( Options::getOptionFloat( "camera/speed/" + speedName ) );
	camera_->turboSpeed( Options::getOptionFloat( "camera/speed/" + speedName + "/turbo" ) );
	camera_->mode( Options::getOptionInt( "camera/mode", 0 ) );
	camera_->invert( !!Options::getOptionInt( "camera/invert", 0 ) );
	camera_->rotDir( Options::getOptionInt( "camera/rotDir", -1 ) );
	camera_->orbitSpeed( Options::getOptionFloat( "camera/orbitSpeed", 1.f ) );

	if (!!Options::getOptionInt( "startup/loadLastModel", 1 ))
	{
		camera_->origin( Options::getOptionVector3(
			"startup/lastOrigin",
			camera_->origin() ) );
	}
	
	camera_->view( Options::pRoot()->readMatrix34(
		"startup/lastView",
		camera_->view() ));

	camera_->setAnimateZoom( !!Options::getOptionInt( "settings/animateZoom", 1 ) );

	camera_->render();
}

Floor* MeApp::floor()
{
	return floor_;
}

Mutant* MeApp::mutant()
{
	return mutant_;
}

Lights* MeApp::lights()
{
	return lights_;
}

ToolsCamera* MeApp::camera()
{
	return camera_;
}

void MeApp::saveModel()
{
	char buf[256];
	sprintf( buf, "Saving Model: \"%s\"\n", mutant_->modelName().c_str() );
	ME_INFO_MSG( buf );

	// Regen the visibility box if needed...
	if (mutant_->visibilityBoxDirty())
	{
		// Make sure we get a redraw first
		AfxGetApp()->m_pMainWnd->Invalidate();
		AfxGetApp()->m_pMainWnd->UpdateWindow();
		
		// Now regen the visibility box
		CModelEditorApp::instance().OnFileRegenBoundingBox(); 
	}

	mutant_->save();
}

bool MeApp::canExit( bool quitting )
{
	//Hmm... A bit of a hack but fields are updated on loss of focus,
	//thus we need to do this focus juggle to commit any changes made.
	HWND currItem = GetFocus();
	SetFocus( AfxGetApp()->m_pMainWnd->GetSafeHwnd() );
	SetFocus( currItem );

	if (mutant_->dirty())
	{
		int result = ::MessageBox( AfxGetApp()->m_pMainWnd->GetSafeHwnd(),
			"Changes were made to the model.\nDo you want to save changes before closing?",
			"Model Changed", MB_YESNOCANCEL | MB_ICONWARNING );
		if( result == IDCANCEL )
		{
			return false;
		}
		if( result == IDYES )
		{
			saveModel();
		}
		if( result == IDNO )
		{
			ME_WARNING_MSG( "Closing without saving changes to model.\n" );
		}
	}

	if ((quitting) && (lights_->dirty()))
	{
		int result = ::MessageBox( AfxGetApp()->m_pMainWnd->GetSafeHwnd(),
			"Changes were made to the lighting setup.\nDo you want to save changes before closing?",
			"Lights Changed", MB_YESNOCANCEL | MB_ICONWARNING );
		if( result == IDCANCEL )
		{
			return false;
		}
		if( result == IDYES )
		{
			return lights_->save();
		}
		if( result == IDNO )
		{
			ME_WARNING_MSG( "Closing without saving changes to lights.\n" );
		}
	}

	if (quitting)
	{
		ME_INFO_MSG( "Exiting ModelEditor...\n" );
	}

	return true;
}

//-------

static PyObject * py_isModelLoaded( PyObject * args )
{
	return PyInt_FromLong( MeApp::instance().mutant()->modelName() != "" );
}
PY_MODULE_FUNCTION( isModelLoaded, ModelEditor )

//-------

static PyObject * py_isModelDirty( PyObject * args )
{
	return PyInt_FromLong( MeApp::instance().mutant()->dirty() );
}
PY_MODULE_FUNCTION( isModelDirty, ModelEditor )

//-------

static PyObject * py_revertModel( PyObject * args )
{
	int result = ::MessageBox( AfxGetApp()->m_pMainWnd->GetSafeHwnd(),
		"Are you sure you want to revert the model?\n"
		"All changes since the last save will be lost.",
		"Revert Model", MB_YESNO | MB_ICONWARNING );
	if( result == IDYES )
	{	
		MeApp::instance().mutant()->revertModel();
		ME_INFO_MSG( "Reverting to last saved model.\n" );
	}
	Py_Return;
}
PY_MODULE_FUNCTION( revertModel, ModelEditor )

//-------

static PyObject * py_saveModel( PyObject * args )
{
	//Hmm... A bit of a hack but fields are updated on loss of focus,
	//thus we need to do this focus juggle to commit any changes made.
	HWND currItem = GetFocus();
	SetFocus( AfxGetApp()->m_pMainWnd->GetSafeHwnd() );
	SetFocus( currItem );
	
	MeApp::instance().saveModel();

	Py_Return;
}
PY_MODULE_FUNCTION( saveModel, ModelEditor )

//-------

static PyObject * py_zoomToExtents( PyObject * args )
{
	MeApp::instance().camera()->zoomToExtents( true,
		MeApp::instance().mutant()->zoomBoundingBox());

	Py_Return;
}
PY_MODULE_FUNCTION( zoomToExtents, ModelEditor )
