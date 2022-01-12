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
#include "bigbang.h"
#include "mainframe.hpp"
#include "big_bang.hpp"
#include "appmgr/options.hpp"
#include "guitabs/guitabs.hpp"
#include "python_adapter.hpp"
#include "user_messages.hpp"
#include "page_objects.hpp"
#include "page_terrain_texture.hpp"
#include "page_terrain_height.hpp"
#include "page_terrain_filter.hpp"
#include "page_terrain_mesh.hpp"
#include "page_terrain_import.hpp"
#include "page_project.hpp"
#include "page_properties.hpp"
#include "page_options_general.hpp"
#include "page_options_weather.hpp"
#include "page_options_environment.hpp"
#include "page_options_histogram.hpp"
#include "common/page_messages.hpp"
#include "common/cooperative_moo.hpp"
#include "page_message_impl.hpp"
#include "placement_ctrls_dlg.hpp"
#include "ual/ual_history.hpp"
#include "ual/ual_manager.hpp"
#include "ual/ual_dialog.hpp"
#include "panel_manager.hpp"

PanelManager* PanelManager::instance_ = 0;


PanelManager::PanelManager() :
	mainFrame_( 0 ),
	ready_( false ),
	GUI::ActionMaker<PanelManager>( "doDefaultPanelLayout", &PanelManager::loadDefaultPanels ),
	GUI::ActionMaker<PanelManager, 1>( "doShowSidePanel", &PanelManager::showSidePanel ),
	GUI::ActionMaker<PanelManager, 2>( "doHideSidePanel", &PanelManager::hideSidePanel ),
	GUI::ActionMaker<PanelManager, 3>( "doLoadPanelLayout", &PanelManager::loadLastPanels ),
	GUI::UpdaterMaker<PanelManager>( "updateSidePanel", &PanelManager::updateSidePanel ),
	GUI::UpdaterMaker<PanelManager, 1>( "disableEnablePanels", &PanelManager::disableEnablePanels )
{
	// tools container
	panelNames_.insert( StrPair( "Tools", GUITABS::ContentContainer::contentID ) );

	// actual tools panels
	panelNames_.insert( StrPair( "Objects", PageObjects::contentID ) );
	panelNames_.insert( StrPair( "TerrainTexture", PageTerrainTexture::contentID ) );
	panelNames_.insert( StrPair( "TerrainHeight", PageTerrainHeight::contentID ) );
	panelNames_.insert( StrPair( "TerrainFilter", PageTerrainFilter::contentID ) );
	panelNames_.insert( StrPair( "TerrainMesh", PageTerrainMesh::contentID ) );
    panelNames_.insert( StrPair( "TerrainImpExp", PageTerrainImport::contentID ) );
	panelNames_.insert( StrPair( "Project", PageProject::contentID ) );    

	// Other panels
	panelNames_.insert( StrPair( "UAL", UalDialog::contentID ) );
	panelNames_.insert( StrPair( "Properties", PageProperties::contentID ) );
	panelNames_.insert( StrPair( "Options", PageOptionsGeneral::contentID ) );
	panelNames_.insert( StrPair( "Weather", PageOptionsWeather::contentID ) );
    panelNames_.insert( StrPair( "Environment", PageOptionsEnvironment::contentID ) );
	panelNames_.insert( StrPair( "Histogram", PageOptionsHistogram::contentID ) );
	panelNames_.insert( StrPair( "Messages", PageMessages::contentID ) );

	ignoredObjectTypes_.insert( "bmp" );
	ignoredObjectTypes_.insert( "tga" );
	ignoredObjectTypes_.insert( "jpg" );
	ignoredObjectTypes_.insert( "dds" );
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
	GUITABS::Manager::instance()->showPanel( PageObjects::contentID, true );

	PageMessages* msgs = (PageMessages*)(GUITABS::Manager::instance()->getContent(PageMessages::contentID ));
	if (msgs)
	{
		msgs->mainFrame( mainFrame_ );

		//This will get deleted in the PageMessages destructor
		msgs->msgsImpl( new BBMsgImpl( msgs ) );
	}

	ready_ = true;

	// set the default tool
	setDefaultToolMode();
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
		if ( path.find("worldeditor") != -1 )
			continue;
		UalManager::instance()->addPath( path );
	}
	UalManager::instance()->setConfigFile(
		Options::getOptionString(
			"ualConfigPath",
			"resources/ual/ual_config.xml" ) );

	UalManager::instance()->setItemClickCallback(
		new UalFunctor1<PanelManager, UalItemInfo*>( instance_, &PanelManager::ualItemClick ) );
	UalManager::instance()->setItemDblClickCallback(
		new UalFunctor1<PanelManager, UalItemInfo*>( instance_, &PanelManager::ualItemClick ) );
	UalManager::instance()->setPopupMenuCallbacks(
		new UalFunctor2<PanelManager, UalItemInfo*, UalPopupMenuItems&>( instance_, &PanelManager::ualStartPopupMenu ),
		new UalFunctor2<PanelManager, UalItemInfo*, int>( instance_, &PanelManager::ualEndPopupMenu )
		);

	UalManager::instance()->setStartDragCallback(
		new UalFunctor1<PanelManager, UalItemInfo*>( instance_, &PanelManager::ualStartDrag ) );
	UalManager::instance()->setUpdateDragCallback(
		new UalFunctor1<PanelManager, UalItemInfo*>( instance_, &PanelManager::ualUpdateDrag ) );
	UalManager::instance()->setEndDragCallback(
		new UalFunctor1<PanelManager, UalItemInfo*>( instance_, &PanelManager::ualEndDrag ) );

	UalManager::instance()->setErrorCallback(
		new UalFunctor1<PanelManager, const std::string&>( instance_, &PanelManager::addSimpleError ) );

	GUITABS::Manager::instance()->registerFactory( new UalDialogFactory() );


	// other panels setup
	GUITABS::Manager::instance()->registerFactory( new PageObjectsFactory() );
	GUITABS::Manager::instance()->registerFactory( new PageTerrainTextureFactory() );
	GUITABS::Manager::instance()->registerFactory( new PageTerrainHeightFactory() );
	GUITABS::Manager::instance()->registerFactory( new PageTerrainFilterFactory() );
	GUITABS::Manager::instance()->registerFactory( new PageTerrainMeshFactory() );
    GUITABS::Manager::instance()->registerFactory( new PageTerrainImportFactory() );
	GUITABS::Manager::instance()->registerFactory( new PageProjectFactory() );

	GUITABS::Manager::instance()->registerFactory( new PagePropertiesFactory() );
	GUITABS::Manager::instance()->registerFactory( new PageOptionsGeneralFactory() );
	GUITABS::Manager::instance()->registerFactory( new PageOptionsWeatherFactory() );
    GUITABS::Manager::instance()->registerFactory( new PageOptionsEnvironmentFactory() );
	GUITABS::Manager::instance()->registerFactory( new PageOptionsHistogramFactory() );
	GUITABS::Manager::instance()->registerFactory( new PageMessagesFactory() );
	
	GUITABS::Manager::instance()->registerFactory( new PlacementCtrlsDlgFactory() );

	if ( ((MainFrame*)mainFrame_)->verifyBarState( "TBState" ) )
		mainFrame_->LoadBarState( "TBState" );

	if ( !GUITABS::Manager::instance()->load() || !allPanelsLoaded() )
		loadDefaultPanels( 0 );

	finishLoad();

	return true;
}

bool PanelManager::allPanelsLoaded()
{
	bool res = true;

	for ( std::map<std::string, std::string>::iterator i = panelNames_.begin();
		res && i != panelNames_.end(); ++i )
	{
		res = res && !!GUITABS::Manager::instance()->getContent(
			(*i).second );
	}

	return res;
}

bool PanelManager::loadDefaultPanels( GUI::ItemPtr item )
{
	CWaitCursor wait;
	bool isFirstCall = ( item == 0 );
	if ( ready_ )
	{
		if ( MessageBox( mainFrame_->GetSafeHwnd(),
			"Are you sure you want to load the default panel layout?",
			"Load Default Panel Layout",
			MB_YESNO|MB_ICONQUESTION ) != IDYES )
			return false;

		ready_ = false;
	}

	if ( item != 0 )
	{
		// not first panel load, so rearrange the toolbars
		((MainFrame*)BigBang2App::instance().mainWnd())->defaultToolbarLayout();
	}

	// clean it up first, just in case
	GUITABS::Manager::instance()->removePanels();


	// Remember to add ALL INSERTED PANELS to the allPanelsLoaded() method above
	GUITABS::PanelHandle p = 0;
	p = GUITABS::Manager::instance()->insertPanel( UalDialog::contentID, GUITABS::RIGHT, p );
	p = GUITABS::Manager::instance()->insertPanel( PageProperties::contentID, GUITABS::TAB, p );
	p = GUITABS::Manager::instance()->insertPanel( PageOptionsGeneral::contentID, GUITABS::TAB, p );
	p = GUITABS::Manager::instance()->insertPanel( PageOptionsWeather::contentID, GUITABS::TAB, p );
    p = GUITABS::Manager::instance()->insertPanel( PageOptionsEnvironment::contentID, GUITABS::TAB, p );
	p = GUITABS::Manager::instance()->insertPanel( PageOptionsHistogram::contentID, GUITABS::TAB, p );
	p = GUITABS::Manager::instance()->insertPanel( PageMessages::contentID, GUITABS::TAB, p );

	p = GUITABS::Manager::instance()->insertPanel(
		GUITABS::ContentContainer::contentID, GUITABS::TOP, p );
	GUITABS::Manager::instance()->insertPanel( PageObjects::contentID, GUITABS::SUBCONTENT, p );
	GUITABS::Manager::instance()->insertPanel( PageTerrainTexture::contentID, GUITABS::SUBCONTENT, p );
	GUITABS::Manager::instance()->insertPanel( PageTerrainHeight::contentID, GUITABS::SUBCONTENT, p );
	GUITABS::Manager::instance()->insertPanel( PageTerrainFilter::contentID, GUITABS::SUBCONTENT, p );
	GUITABS::Manager::instance()->insertPanel( PageTerrainMesh::contentID, GUITABS::SUBCONTENT, p );
	GUITABS::Manager::instance()->insertPanel( PageTerrainImport::contentID, GUITABS::SUBCONTENT, p );
	GUITABS::Manager::instance()->insertPanel( PageProject::contentID, GUITABS::SUBCONTENT, p );

	if ( !isFirstCall )
		finishLoad();

	return true;
}

bool PanelManager::loadLastPanels( GUI::ItemPtr item )
{
	CWaitCursor wait;
	if ( MessageBox( mainFrame_->GetSafeHwnd(),
		"Are you sure you want to load the last saved panel layout?",
		"Load Most Recent Panel Layout",
		MB_YESNO|MB_ICONQUESTION ) != IDYES )
		return false;

	ready_ = false;

	if ( ((MainFrame*)mainFrame_)->verifyBarState( "TBState" ) )
		mainFrame_->LoadBarState( "TBState" );

	if ( !GUITABS::Manager::instance()->load() || !allPanelsLoaded() )
		loadDefaultPanels( 0 );

	finishLoad();

	return true;
}

bool PanelManager::ready()
{
	return ready_;
}

const std::string PanelManager::getContentID( const std::string pyID )
{ 
	std::map<std::string, std::string>::iterator idIt = panelNames_.find( pyID );
	if ( idIt != panelNames_.end() )
		return (*idIt).second;

	return pyID; // it they passed in the GUITABS contentID of the panel, then use it
}

const std::string PanelManager::getPythonID( const std::string contentID )
{
	for ( std::map<std::string, std::string>::iterator i = panelNames_.begin();
		i != panelNames_.end(); ++i )
	{
		if ( (*i).second == contentID )
			return (*i).first;
	}
	return contentID; // it they passed in the GUITABS python ID of the panel, then use it
}

void PanelManager::setToolMode( const std::string id )
{
	if ( !ready_ )
		return;

	std::string pyID = getPythonID( id );

	if ( currentTool_ == pyID )
		return;

	std::string contentID = getContentID( id );

	if ( contentID.empty() )
		return;

	updateUIToolMode( pyID );

	GUITABS::Manager::instance()->showPanel( contentID, true );
	GUITABS::Manager::instance()->sendMessage( contentID, WM_ACTIVATE_TOOL, 0, 0 );

	// hide tool-dependent popups:
	GUITABS::Manager::instance()->showPanel( PlacementCtrlsDlg::contentID, false );
}

void PanelManager::setDefaultToolMode()
{
	setToolMode( PageObjects::contentID );
} 

const std::string PanelManager::currentTool()
{
	return currentTool_;
}

bool PanelManager::isCurrentTool( const std::string& id )
{
	return currentTool() == getPythonID( id );
}

void PanelManager::updateUIToolMode( const std::string& pyID )
{
	std::string pythonID = getPythonID( pyID );
	if ( pythonID.empty() )
		currentTool_ = pyID;
	else
		currentTool_ = pythonID; // the pyID was actually a contentID, so setting the obtained pythonID
	GUI::Manager::instance()->update();
}

void PanelManager::showPanel( const std::string pyID, int show )
{
	std::string contentID = getContentID( pyID );

	if ( !contentID.empty() )
		GUITABS::Manager::instance()->showPanel( contentID, show != 0 );
}

int PanelManager::isPanelVisible( const std::string pyID )
{
	std::string contentID = getContentID( pyID );

	if ( !contentID.empty() )
		return GUITABS::Manager::instance()->isContentVisible( contentID );
	else
		return 0;
}

void PanelManager::ualItemClick( UalItemInfo* ii )
{
	if ( !PythonAdapter::instance() || !ii )
		return;

	UalHistory::instance()->prepareItem( ii->assetInfo() );
	PythonAdapter::instance()->onBrowserObjectItemSelect( "Ual" , ii->longText().c_str() );
}

void PanelManager::ualStartPopupMenu( UalItemInfo* ii, UalPopupMenuItems& menuItems )
{
	if ( !PythonAdapter::instance() || !ii )
		return;

	HMENU menu = ::CreatePopupMenu();

	std::map<int,std::string> pyMenuItems;
	PythonAdapter::instance()->contextMenuGetItems(
		ii->type(),
		ii->longText().c_str(),
		pyMenuItems );

	if ( !pyMenuItems.size() )
		return;

	for( std::map<int,std::string>::iterator i = pyMenuItems.begin();
		i != pyMenuItems.end(); ++i )
	{
		menuItems.push_back( UalPopupMenuItem( (*i).second, (*i).first ) );
	}
}

void PanelManager::ualEndPopupMenu( UalItemInfo* ii, int result )
{
	if ( !PythonAdapter::instance() || !ii )
		return;

	PythonAdapter::instance()->contextMenuHandleResult(
		ii->type(),
		ii->longText().c_str(),
		result );
}


void PanelManager::ualStartDrag( UalItemInfo* ii )
{
	if ( !PythonAdapter::instance() || !ii )
		return;

	// Start drag
	UalDropManager::instance().start( 
		BWResource::getExtension( ii->longText() ));
}

void PanelManager::ualUpdateDrag( UalItemInfo* ii )
{
	if ( !ii )
		return;

	SmartPointer< UalDropCallback > dropable = UalDropManager::instance().test( ii );
	bool onGraphicWindow = ::WindowFromPoint( CPoint( ii->x(), ii->y() ) ) == BigBang::instance().hwndGraphics(); 
	std::string filename = ii->text();
	std::string ext = filename.rfind( '.' ) == filename.npos ?	""	:
						filename.substr( filename.rfind( '.' ) + 1 );
	bool isIgnored = ignoredObjectTypes_.find( ext ) != ignoredObjectTypes_.end()
		|| currentTool_ == "TerrainImpExp" || currentTool_ == "Project";
	if ( ii->isFolder() || dropable || ( onGraphicWindow && !isIgnored ) )
		SetCursor( AfxGetApp()->LoadStandardCursor( IDC_ARROW ) );
	else
		SetCursor( AfxGetApp()->LoadStandardCursor( IDC_NO ) );

	if ( !ii->isFolder() && CooperativeMoo::activate() )
		BigBang2App::instance().mfApp()->updateFrame( false );
}

void PanelManager::ualEndDrag( UalItemInfo* ii )
{
	SetCursor( AfxGetApp()->LoadStandardCursor( IDC_ARROW ) );

	if ( !ii )
		return;

	if ( ii->isFolder() )
	{
		// folder drag successfull
		CPoint pt;
		GetCursorPos( &pt );
		mainFrame_->ScreenToClient( &pt );
		GUITABS::Manager::instance()->clone( (GUITABS::Content*)( ii->dialog() ),
			pt.x - 5, pt.y - 5 );
	}
	else
	{
		// item drag successfull
		if ( ::WindowFromPoint( CPoint( ii->x(), ii->y() ) ) == BigBang::instance().hwndGraphics() &&
			currentTool_ != "TerrainImpExp" && currentTool_ != "Project" )
		{
			setToolMode( "Objects" );
			BigBang::instance().update( 0 );
			UalHistory::instance()->prepareItem( ii->assetInfo() );

			SetCursor( AfxGetApp()->LoadStandardCursor( IDC_WAIT ) );
			if ( PythonAdapter::instance() )
				PythonAdapter::instance()->onBrowserObjectItemAdd();

			SetCursor( AfxGetApp()->LoadStandardCursor( IDC_ARROW ) );
		}
		else
			UalDropManager::instance().end(ii);
	}
}

void PanelManager::ualAddItemToHistory( std::string str, std::string type )
{
	// called from python
	UalHistory::instance()->addPreparedItem();
}

void PanelManager::addSimpleError( const std::string& msg )
{
	BigBang::instance().addError( 0, 0, msg.c_str() );
}

bool PanelManager::showSidePanel( GUI::ItemPtr item )
{
	bool isDockVisible = GUITABS::Manager::instance()->isDockVisible();

	if ( !isDockVisible )
	{
		GUITABS::Manager::instance()->showDock( !isDockVisible );
		GUITABS::Manager::instance()->showFloaters( !isDockVisible );
//		GetMenu()->GetSubMenu( 2 )->CheckMenuItem( ID_MENU_SHOWSIDEPANEL, MF_BYCOMMAND | MF_CHECKED );
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
//		GetMenu()->GetSubMenu( 2 )->CheckMenuItem( ID_MENU_SHOWSIDEPANEL, MF_BYCOMMAND | MF_UNCHECKED );
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

unsigned int PanelManager::disableEnablePanels( GUI::ItemPtr item )
{
	if ( GUITABS::Manager::instance()->isDockVisible() )
		return 1;
	else
		return 0;
}

void PanelManager::updateControls()
{
	GUITABS::Manager::instance()->broadcastMessage( WM_UPDATE_CONTROLS, 0, 0 );
}

void PanelManager::onClose()
{
	CWaitCursor wait;
	if ( Options::getOptionBool( "panels/saveLayoutOnExit", true ) )
	{
		GUITABS::Manager::instance()->save();
		mainFrame_->SaveBarState( "TBState" );
	}
	GUITABS::Manager::instance()->showDock( false );
	UalManager::instance()->fini();
}

void PanelManager::onNewSpace()
{
    GUITABS::Manager::instance()->broadcastMessage( WM_NEW_SPACE, 0, 0 );
}

void PanelManager::onBeginSave()
{
    GUITABS::Manager::instance()->broadcastMessage( WM_BEGIN_SAVE, 0, 0 );
}

void PanelManager::onEndSave()
{
    GUITABS::Manager::instance()->broadcastMessage( WM_END_SAVE, 0, 0 );
}

void PanelManager::showItemInUal( const std::string& vfolder, const std::string& longText )
{
	UalManager::instance()->showItem( vfolder, longText );
	showPanel( "UAL", true ); // make sure it's visible
}
