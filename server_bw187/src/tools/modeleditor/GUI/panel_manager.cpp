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

#include "appmgr/options.hpp"
#include "guitabs/guitabs.hpp"
#include "user_messages.hpp"
#include "python_adapter.hpp"

#include "model_editor.h"
#include "main_frm.h"

#include "page_display.hpp"
#include "page_object.hpp"
#include "page_animations.hpp"
#include "page_actions.hpp"
#include "page_lod.hpp"
#include "page_lights.hpp"
#include "page_materials.hpp"
#include "page_messages.hpp"

#include "ual/ual_history.hpp"
#include "ual/ual_manager.hpp"
#include "ual/ual_dialog.hpp"
#include "ual/ual_drop_manager.hpp"

#include "controls/user_messages.hpp"

#include "panel_manager.hpp"

PanelManager* PanelManager::instance_ = NULL;

PanelManager::PanelManager() :
	mainFrame_( NULL ),
	ready_( false ),
	GUI::ActionMaker<PanelManager>( "doDefaultPanelLayout", &PanelManager::loadDefaultPanels ),
	GUI::ActionMaker<PanelManager, 1>( "doShowSidePanel", &PanelManager::showSidePanel ),
	GUI::ActionMaker<PanelManager, 2>( "doHideSidePanel", &PanelManager::hideSidePanel ),
	GUI::ActionMaker<PanelManager, 3>( "doLoadPanelLayout", &PanelManager::loadLastPanels ),
	GUI::UpdaterMaker<PanelManager>( "updateSidePanel", &PanelManager::updateSidePanel )
{
}

PanelManager* PanelManager::instance()
{
	if ( !instance_ )
		instance_ = new PanelManager();

	return instance_;
}

bool PanelManager::initDock( CFrameWnd* mainFrame, CWnd* mainView )
{
	mainFrame_ = mainFrame;
	GUITABS::Manager::instance()->insertDock( mainFrame, mainView );
	return true;
}

bool PanelManager::killDock()
{
	ready_ = false;
	GUITABS::Manager::instance()->removeDock();
	ThumbnailManager::fini();
	return true;
}

void PanelManager::finishLoad()
{
	// show the default panels
	GUITABS::Manager::instance()->showPanel( UalDialog::contentID, true );
	
	PageMessages* msgs = (PageMessages*)(GUITABS::Manager::instance()->getContent(PageMessages::contentID ));
	if (msgs)
	{
		msgs->mainFrame( mainFrame_ );
	}

	ready_ = true;
}

bool PanelManager::initPanels()
{
	if ( ready_ )
		return false;

	CWaitCursor wait;

	// UAL Setup
	for ( int i = 0; i < BWResource::getPathNum(); i++ )
	{
		std::string path = BWResource::getPath( i );
		if ( path.find("modeleditor") != -1 )
			continue;
		UalManager::instance()->addPath( path );
	}
	UalManager::instance()->setConfigFile(
		Options::getOptionString(
			"ualConfigPath",
			"resources/ual/ual_config.xml" ) );

	UalManager::instance()->setItemDblClickCallback(
		new UalFunctor1<PanelManager, UalItemInfo*>( instance_, &PanelManager::ualItemDblClick ) );
	UalManager::instance()->setStartDragCallback(
		new UalFunctor1<PanelManager, UalItemInfo*>( instance_, &PanelManager::ualStartDrag ) );
	UalManager::instance()->setUpdateDragCallback(
		new UalFunctor1<PanelManager, UalItemInfo*>( instance_, &PanelManager::ualUpdateDrag ) );
	UalManager::instance()->setEndDragCallback(
		new UalFunctor1<PanelManager, UalItemInfo*>( instance_, &PanelManager::ualEndDrag ) );
	UalManager::instance()->setPopupMenuCallbacks(
		new UalFunctor2<PanelManager, UalItemInfo*, UalPopupMenuItems&>( instance_, &PanelManager::ualStartPopupMenu ),
		new UalFunctor2<PanelManager, UalItemInfo*, int>( instance_, &PanelManager::ualEndPopupMenu ));
	
	GUITABS::Manager::instance()->registerFactory( new UalDialogFactory() );

	// Setup the map which is used for python
	contentID_["UAL"] = UalDialog::contentID;
	contentID_["Display"] = PageDisplay::contentID;
	contentID_["Object"] = PageObject::contentID;
	contentID_["Animations"] = PageAnimations::contentID;
	contentID_["Actions"] = PageActions::contentID;
	contentID_["LOD"] = PageLOD::contentID;
	contentID_["Lights"] = PageLights::contentID;
	contentID_["Materials"] = PageMaterials::contentID;
	contentID_["Messages"] = PageMessages::contentID;

	// other panels setup
	GUITABS::Manager::instance()->registerFactory( new PageDisplayFactory );
	GUITABS::Manager::instance()->registerFactory( new PageObjectFactory );
	GUITABS::Manager::instance()->registerFactory( new PageAnimationsFactory );
	GUITABS::Manager::instance()->registerFactory( new PageActionsFactory );
	GUITABS::Manager::instance()->registerFactory( new PageLODFactory );
	GUITABS::Manager::instance()->registerFactory( new PageLightsFactory );
	GUITABS::Manager::instance()->registerFactory( new PageMaterialsFactory );
	GUITABS::Manager::instance()->registerFactory( new PageMessagesFactory );
	
	if ( ( (CMainFrame*)mainFrame_ )->verifyBarState( "TBState" ) )
		mainFrame_->LoadBarState( "TBState" );

	if ( !GUITABS::Manager::instance()->load() )
	{
		loadDefaultPanels( NULL );
	}

	finishLoad();

	return true;
}

bool PanelManager::loadDefaultPanels( GUI::ItemPtr item )
{
	CWaitCursor wait;
	bool isFirstCall = true;
	if ( ready_ )
	{
		if ( MessageBox( mainFrame_->GetSafeHwnd(),
			"Are you sure you want to load the default panel layout?",
			"MaterialEditor - Load Default Panel Layout",
			MB_YESNO | MB_ICONQUESTION ) != IDYES )
			return false;

		ready_ = false;
		isFirstCall = false;
		// already has something in it, so clean up first
		GUITABS::Manager::instance()->removePanels();
	}

	if ( item != 0 )
	{
		// not first panel load, so rearrange the toolbars
		((CMainFrame*)mainFrame_)->defaultToolbarLayout();
	}

	GUITABS::PanelHandle basePanel = GUITABS::Manager::instance()->insertPanel( UalDialog::contentID, GUITABS::RIGHT );
	GUITABS::Manager::instance()->insertPanel( PageObject::contentID, GUITABS::TAB, basePanel );
	GUITABS::Manager::instance()->insertPanel( PageDisplay::contentID, GUITABS::TAB, basePanel );
	GUITABS::Manager::instance()->insertPanel( PageAnimations::contentID, GUITABS::TAB, basePanel );
	GUITABS::Manager::instance()->insertPanel( PageActions::contentID, GUITABS::TAB, basePanel );
	GUITABS::Manager::instance()->insertPanel( PageLOD::contentID, GUITABS::TAB, basePanel );
	GUITABS::Manager::instance()->insertPanel( PageLights::contentID, GUITABS::TAB, basePanel );
	GUITABS::Manager::instance()->insertPanel( PageMaterials::contentID, GUITABS::TAB, basePanel );
	GUITABS::Manager::instance()->insertPanel( PageMessages::contentID, GUITABS::TAB, basePanel );

	if ( !isFirstCall )
		finishLoad();

	return true;
}

bool PanelManager::loadLastPanels( GUI::ItemPtr item )
{
	CWaitCursor wait;
	if ( MessageBox( mainFrame_->GetSafeHwnd(),
		"Are you sure you want to load the last saved panel layout?",
		"MaterialEditor - Load Most Recent Panel Layout",
		MB_YESNO | MB_ICONQUESTION ) != IDYES )
		return false;

	ready_ = false;

	if ( ( (CMainFrame*)mainFrame_ )->verifyBarState( "TBState" ) )
		mainFrame_->LoadBarState( "TBState" );

	if ( !GUITABS::Manager::instance()->load() )
		loadDefaultPanels( NULL );

	finishLoad();

	return true;
}

bool PanelManager::ready()
{
	return ready_;
}

void PanelManager::showPanel( std::string& pyID, int show /* = 1 */ )
{
	std::string contentID = contentID_[pyID];

	if ( contentID.length()>0 )
	{
		GUITABS::Manager::instance()->showPanel( contentID, show != 0 );
	}
}

int PanelManager::isPanelVisible( std::string& pyID )
{
	std::string contentID = contentID_[pyID];

	if ( contentID.length()>0 )
	{
		return GUITABS::Manager::instance()->isContentVisible( contentID );
	}
	return 0;
}

void PanelManager::ualItemDblClick( UalItemInfo* ii )
{
	if ( !ii ) return;

	if ( PythonAdapter::instance() )
		PythonAdapter::instance()->callString( "openFile", 
			BWResource::dissolveFilename( ii->longText() ) );
}

void PanelManager::ualStartDrag( UalItemInfo* ii )
{
	if ( !ii ) return;

	UalDropManager::instance().start( 
		BWResource::getExtension( ii->longText() ));
}

void PanelManager::ualUpdateDrag( UalItemInfo* ii )
{
	if ( !ii ) return;

	SmartPointer< UalDropCallback > dropable = UalDropManager::instance().test( ii );
		
	if ((ii->isFolder()) || (dropable))
	{
		if ((ii->isFolder()) ||
			((dropable && dropable->canAdd() &&
				((GetAsyncKeyState(VK_LCONTROL) < 0) ||
				(GetAsyncKeyState(VK_RCONTROL) < 0) ||
				(GetAsyncKeyState(VK_LMENU) < 0) ||
				(GetAsyncKeyState(VK_RMENU) < 0)))))
		{
			SetCursor( AfxGetApp()->LoadCursor( IDC_ADD_CURSOR ));
		}
		else
		{
			SetCursor( AfxGetApp()->LoadStandardCursor( IDC_ARROW ) );
		}
	}
	else
		SetCursor( AfxGetApp()->LoadStandardCursor( IDC_NO ) );
}

void PanelManager::ualEndDrag( UalItemInfo* ii )
{
	SetCursor( AfxGetApp()->LoadStandardCursor( IDC_ARROW ) );

	if ( !ii ) return;

	if ( ii->isFolder() )
	{
		// folder drag
		CPoint pt;
		GetCursorPos( &pt );
		AfxGetMainWnd()->ScreenToClient( &pt );
		GUITABS::Manager::instance()->clone( (GUITABS::Content*)( ii->dialog() ),
			pt.x - 5, pt.y - 5 );
	}
	else
	{
		UalDropManager::instance().end(ii);
	}
}

void PanelManager::ualStartPopupMenu( UalItemInfo* ii, UalPopupMenuItems& menuItems )
{
	if (!ii) return;
	
	if ( !PythonAdapter::instance() ) return;

	std::map<int,std::string> pyMenuItems;
	PythonAdapter::instance()->contextMenuGetItems(
		ii->type(),
		BWResource::dissolveFilename( ii->longText() ).c_str(),
		pyMenuItems );

	if ( !pyMenuItems.size() ) return;

	for( std::map<int,std::string>::iterator i = pyMenuItems.begin();
		i != pyMenuItems.end(); ++i )
	{
		menuItems.push_back( UalPopupMenuItem( (*i).second, (*i).first ) );
	}
}

void PanelManager::ualEndPopupMenu( UalItemInfo* ii, int result )
{
	if (!ii) return;
	
	PythonAdapter::instance()->contextMenuHandleResult(
		ii->type(),
		BWResource::dissolveFilename( ii->longText() ).c_str(),
		result );
}

void PanelManager::ualAddItemToHistory( std::string filePath )
{
	// called from python
	std::string fname = BWResource::getFilename( filePath );
	std::string longText = BWResource::resolveFilename( filePath );
	std::replace( longText.begin(), longText.end(), '/', '\\' );
	UalHistory::instance()->add( AssetInfo( "FILE", fname, longText ));
}


bool PanelManager::showSidePanel( GUI::ItemPtr item )
{
	bool isDockVisible = GUITABS::Manager::instance()->isDockVisible();

	if ( !isDockVisible )
	{
		GUITABS::Manager::instance()->showDock( !isDockVisible );
		GUITABS::Manager::instance()->showFloaters( !isDockVisible );
	}
	return true;
}

bool PanelManager::hideSidePanel( GUI::ItemPtr item )
{
	bool isDockVisible = GUITABS::Manager::instance()->isDockVisible();

	if ( isDockVisible )
	{
		GUITABS::Manager::instance()->showDock( !isDockVisible );
		GUITABS::Manager::instance()->showFloaters( !isDockVisible );
	}
	return true;
}

unsigned int PanelManager::updateSidePanel( GUI::ItemPtr item )
{
	if ( GUITABS::Manager::instance()->isDockVisible() )
		return 0;
	else
		return 1;
}

void PanelManager::updateControls()
{
	GUITABS::Manager::instance()->broadcastMessage( WM_UPDATE_CONTROLS, 0, 0 );
}

void PanelManager::onClose()
{
	if ( Options::getOptionBool( "panels/saveLayoutOnExit", true ) )
	{
		GUITABS::Manager::instance()->save();
		mainFrame_->SaveBarState( "TBState" );
	}
	GUITABS::Manager::instance()->showDock( false );
}