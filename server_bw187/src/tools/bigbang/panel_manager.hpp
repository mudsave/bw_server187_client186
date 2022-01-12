/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef PANEL_MANAGER_HPP
#define PANEL_MANAGER_HPP

#include "ual/ual_callback.hpp"
#include <set>

class UalDialog;

class PanelManager :
	GUI::ActionMaker<PanelManager>,		// load default panels
	GUI::ActionMaker<PanelManager,1>,	// show panels
	GUI::ActionMaker<PanelManager,2>,	// hide panels
	GUI::ActionMaker<PanelManager,3>,	// load last panels
	GUI::UpdaterMaker<PanelManager>,	// update show/hide panels
	GUI::UpdaterMaker<PanelManager,1>	// disable/enable show/hide panels
{
public:
	~PanelManager() {};

	static PanelManager* instance();

	// panel stuff
	bool initDock( CFrameWnd* mainFrame, CWnd* mainView );
	bool killDock();
	bool initPanels();
	bool ready();
	void updateUIToolMode( const std::string& pyID );
	void setToolMode( const std::string pyID );
	void setDefaultToolMode();
	void showPanel( const std::string pyID, int show );
	int isPanelVisible( const std::string pyID );
	const std::string currentTool();
	bool isCurrentTool( const std::string& id );
	bool showSidePanel( GUI::ItemPtr item );
	bool hideSidePanel( GUI::ItemPtr item );
	unsigned int updateSidePanel( GUI::ItemPtr item );
	unsigned int disableEnablePanels( GUI::ItemPtr item );
	void updateControls();
	void onClose();
    void onNewSpace();
    void onBeginSave();
    void onEndSave();
	void showItemInUal( const std::string& vfolder, const std::string& longText );

	// UAL callbacks
	void ualAddItemToHistory( std::string str, std::string type );

private:
	PanelManager();

	static PanelManager* instance_;

	std::map<std::string,std::string> panelNames_; // pair( python ID, GUITABS contentID )
	typedef std::pair<std::string,std::string> StrPair;
	std::string currentTool_;
	CFrameWnd* mainFrame_;
	CWnd* mainView_;
	bool ready_;
	std::set<std::string> ignoredObjectTypes_;

	// panel stuff
	void finishLoad();
	bool allPanelsLoaded();
	bool loadDefaultPanels( GUI::ItemPtr item );
	bool loadLastPanels( GUI::ItemPtr item );
	const std::string getContentID( const std::string pyID );
	const std::string getPythonID( const std::string contentID );

	// UAL callbacks
	void ualItemClick( UalItemInfo* ii );
	void ualStartPopupMenu( UalItemInfo* ii, UalPopupMenuItems& menuItems );
	void ualEndPopupMenu( UalItemInfo* ii, int result );

	void ualStartDrag( UalItemInfo* ii );
	void ualUpdateDrag( UalItemInfo* ii );
	void ualEndDrag( UalItemInfo* ii );

	// other
	void addSimpleError( const std::string& msg );
};


#endif // PANEL_MANAGER_HPP
