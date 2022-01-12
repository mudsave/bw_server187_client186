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

#include "ual/ual_manager.hpp"

class UalDialog;
class UalItemInfo;

class PanelManager :
	GUI::ActionMaker<PanelManager>  ,	// show panels
	GUI::ActionMaker<PanelManager,1>,	// hide panels
	GUI::UpdaterMaker<PanelManager>		// update show/hide panels
{
public:
    static PanelManager *instance();

    ~PanelManager();

	// panel stuff
    bool initDock(CFrameWnd* mainFrame, CWnd* mainView);
    bool killDock();
    bool initPanels();
    bool ready();
    void showPanel(std::string const &pyID, int show);
    int isPanelVisible(std::string const &pyID);
    bool showSidePanel(GUI::ItemPtr item);
    bool hideSidePanel(GUI::ItemPtr item);
    unsigned int updateSidePanel(GUI::ItemPtr item);
    void updateControls();
    void onClose();
    void ualAddItemToHistory(std::string filePath);
	bool loadDefaultPanels(GUI::ItemPtr item);
	bool loadLastPanels(GUI::ItemPtr item);

private:
    PanelManager();

    void finishLoad();

	// UAL callbacks
	void ualItemDblClick(UalItemInfo* ii);
	void ualStartDrag(UalItemInfo* ii);
	void ualUpdateDrag(UalItemInfo* ii);
	void ualEndDrag(UalItemInfo* ii);

private:
    std::map<std::string, std::string>  m_contentID;
    int                                 m_currentTool;
    CFrameWnd                           *m_mainFrame;
    CWnd                                *m_mainView;
    bool                                m_ready;
    bool                                m_dropable;
};

#endif // PANEL_MANAGER_HPP
