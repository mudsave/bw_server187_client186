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

class UalDialog;
//class UalItemInfo;

class PanelManager :
	GUI::ActionMaker<PanelManager>,		// load default panels
	GUI::ActionMaker<PanelManager,1>,	// show panels
	GUI::ActionMaker<PanelManager,2>,	// hide panels
	GUI::ActionMaker<PanelManager,3>,	// load last panels
	GUI::UpdaterMaker<PanelManager>		// update show/hide panels
{
public:
	~PanelManager() {};

	static PanelManager* instance();

	// panel stuff
	bool initDock( CFrameWnd* mainFrame, CWnd* mainView );
	bool killDock();
	bool initPanels();
	bool ready();
	void showPanel( std::string& pyID, int show = 1 );
	int isPanelVisible( std::string& pyID );
	int currentTool();
	bool showSidePanel( GUI::ItemPtr item );
	bool hideSidePanel( GUI::ItemPtr item );
	unsigned int updateSidePanel( GUI::ItemPtr item );
	void updateControls();
	void onClose();
	void ualAddItemToHistory( std::string filePath );

private:
	PanelManager();

	static PanelManager* instance_;

	std::map< std::string, std::string > contentID_;

	int currentTool_;
	CFrameWnd* mainFrame_;
	CWnd* mainView_;
	bool ready_;

	// panel stuff
	void finishLoad();
	bool loadDefaultPanels( GUI::ItemPtr item );
	bool loadLastPanels( GUI::ItemPtr item );
	std::string getContentID( std::string& pyID );

	// UAL callbacks
	void ualItemDblClick( UalItemInfo* ii );
	void ualStartDrag( UalItemInfo* ii );
	void ualUpdateDrag( UalItemInfo* ii );
	void ualEndDrag( UalItemInfo* ii );
	void ualStartPopupMenu( UalItemInfo* ii, UalPopupMenuItems& menuItems );
	void ualEndPopupMenu( UalItemInfo* ii, int result );

};


#endif // PANEL_MANAGER_HPP
