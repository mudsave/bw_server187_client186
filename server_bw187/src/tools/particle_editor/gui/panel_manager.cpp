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
#include "panel_manager.hpp"
#include "user_messages.hpp"
#include "main_frame.hpp"
#include "action_selection.hpp"
#include "particle_editor.hpp"
#include "modeleditor/app/python_adapter.hpp"
#include "gui/action_selection.hpp"
#include "guitabs/guitabs.hpp"
#include "appmgr/options.hpp"
#include "ual/ual_history.hpp"
#include "ual/ual_manager.hpp"
#include "ual/ual_dialog.hpp"
#include "ual/ual_callback.hpp"
#include "ual/ual_drop_manager.hpp"

using namespace std;

/*static*/ PanelManager *PanelManager::instance()
{
    static PanelManager s_instance;

    return &s_instance;
}

PanelManager::~PanelManager()
{
}

bool PanelManager::initDock(CFrameWnd* mainFrame, CWnd* mainView)
{
    m_mainFrame = mainFrame;
    GUITABS::Manager::instance()->insertDock(mainFrame, mainView);
    return true;
}

bool PanelManager::killDock()
{
    m_ready = false;
    GUITABS::Manager::instance()->removeDock();
	ThumbnailManager::fini();
    return true;
}

bool PanelManager::initPanels()
{
    if (m_ready)
        return false;

    CWaitCursor wait;

    // UAL Setup
    for (int i = 0; i < BWResource::getPathNum(); i++)
    {
        string path = BWResource::getPath(i);
        if ( path.find("particleeditor") != -1 )
            continue;
        UalManager::instance()->addPath(path);
    }
    UalManager::instance()->setConfigFile
    (
        Options::getOptionString
        (
            "ualConfigPath",
            "resources/ual/ual_config.xml" 
        ) 
    );

    UalManager::instance()->setItemDblClickCallback
    (
        new UalFunctor1<PanelManager, UalItemInfo*>(instance(), &PanelManager::ualItemDblClick)
    );
    UalManager::instance()->setStartDragCallback
    (
        new UalFunctor1<PanelManager, UalItemInfo*>(instance(), &PanelManager::ualStartDrag)
    );
    UalManager::instance()->setUpdateDragCallback
    (
        new UalFunctor1<PanelManager, UalItemInfo*>(instance(), &PanelManager::ualUpdateDrag)
    );
    UalManager::instance()->setEndDragCallback
    (
        new UalFunctor1<PanelManager, UalItemInfo*>(instance(), &PanelManager::ualEndDrag)
    );
    
    GUITABS::Manager::instance()->registerFactory(new UalDialogFactory());

    // Setup the map which is used for python
    m_contentID["Particle Editor1"] = ActionSelection::contentID;

    // other panels setup
    GUITABS::Manager::instance()->registerFactory(new ActionSelectionFactory);

    if ( ( (MainFrame*)m_mainFrame )->verifyBarState( "TBState" ) )
        m_mainFrame->LoadBarState( "TBState" );

    if (!GUITABS::Manager::instance()->load())
    {
        loadDefaultPanels(NULL);
    }

    // Show the particle editor panel by default:
    GUITABS::Manager::instance()->showPanel(ActionSelection::contentID, true);

    // BAn - this is something of a hack - ActionSelection1::OnInitialUpdate is
    // not called.  I do this manually:
    ActionSelection *actionSelection =
            (ActionSelection*)GUITABS::Manager::instance()->getContent
            (
                ActionSelection::contentID
            );

    finishLoad();

    return true;
}

bool PanelManager::ready()
{
    return m_ready;
}

void PanelManager::showPanel(string const &pyID, int show)
{
    GUITABS::Manager::instance()->showPanel(pyID, show != 0);
}

int PanelManager::isPanelVisible(string const& pyID)
{
    return GUITABS::Manager::instance()->isContentVisible(pyID);
}

bool PanelManager::showSidePanel(GUI::ItemPtr item)
{
    bool isDockVisible = GUITABS::Manager::instance()->isDockVisible();

    if (!isDockVisible)
    {
        GUITABS::Manager::instance()->showDock(!isDockVisible);
        GUITABS::Manager::instance()->showFloaters(!isDockVisible);
    }
    return true;
}

bool PanelManager::hideSidePanel(GUI::ItemPtr item)
{
    bool isDockVisible = GUITABS::Manager::instance()->isDockVisible();

    if (isDockVisible)
    {
        GUITABS::Manager::instance()->showDock(!isDockVisible);
        GUITABS::Manager::instance()->showFloaters(!isDockVisible);
    }
    return true;
}

unsigned int PanelManager::updateSidePanel(GUI::ItemPtr item)
{
    if (GUITABS::Manager::instance()->isDockVisible())
        return 0;
    else
        return 1;
}

void PanelManager::updateControls()
{
    GUITABS::Manager::instance()->broadcastMessage(WM_UPDATE_CONTROLS, 0, 0);
    ActionSelection *actionSelection =
        (ActionSelection*)GUITABS::Manager::instance()->getContent
        (
            ActionSelection::contentID
        );
    actionSelection->SendMessageToDescendants
    (
        WM_IDLEUPDATECMDUI,
        (WPARAM)TRUE, 
        0, 
        TRUE, 
        TRUE
    );
}

void PanelManager::onClose()
{
    if (Options::getOptionBool("panels/saveLayoutOnExit", true))
    {
        GUITABS::Manager::instance()->save();
        m_mainFrame->SaveBarState( "TBState" );
    }
    GUITABS::Manager::instance()->showDock( false );
    GUITABS::Manager::instance()->removeDock();
}

void PanelManager::ualAddItemToHistory(string filePath)
{
    // called from python
    string fname    = BWResource::getFilename(filePath);
    string longText = BWResource::resolveFilename(filePath);
    replace(longText.begin(), longText.end(), '/', '\\');
    UalHistory::instance()->add(AssetInfo("FILE", fname, longText));
}

bool PanelManager::loadDefaultPanels(GUI::ItemPtr item)
{
    CWaitCursor wait;
    bool isFirstCall = true;
    if (m_ready)
    {
        if 
        (
            MessageBox
            (
                m_mainFrame->GetSafeHwnd(),
                "Are you sure you want to load the default panel layout?",
                "ParticleEditor - Load Default Panel Layout",
                MB_YESNO | MB_ICONQUESTION 
            ) != IDYES 
        )
        {
            return false;
        }

        m_ready = false;
        isFirstCall = false;
        // already has something in it, so clean up first
        GUITABS::Manager::instance()->removePanels();

        // not first panel load, so rearrange the toolbars
        ((MainFrame*)m_mainFrame)->defaultToolbarLayout();
    }

    GUITABS::Manager::instance()->insertPanel
    (
        ActionSelection::contentID, GUITABS::RIGHT
    );
    GUITABS::Manager::instance()->insertPanel
    (
        UalDialog::contentID, GUITABS::BOTTOM
    );

    if (!isFirstCall)
        finishLoad();

    return true;
}

bool PanelManager::loadLastPanels(GUI::ItemPtr item)
{
    CWaitCursor wait;
    if 
    ( 
        MessageBox
        ( 
            m_mainFrame->GetSafeHwnd(),
            "Are you sure you want to load the last saved panel layout?",
            "ParticleEditor - Load Most Recent Panel Layout",
            MB_YESNO | MB_ICONQUESTION 
        ) != IDYES 
    )
    {
        return false;
    }

    m_ready = false;

    if ( ( (MainFrame*)m_mainFrame )->verifyBarState( "TBState" ) )
        m_mainFrame->LoadBarState( "TBState" );

    if (!GUITABS::Manager::instance()->load())
        loadDefaultPanels(NULL);

    finishLoad();

    return true;
}

PanelManager::PanelManager() :
    GUI::ActionMaker<PanelManager   >
    (
        "doShowSidePanel", 
        &PanelManager::showSidePanel
    ),
    GUI::ActionMaker<PanelManager, 1>
    (
        "doHideSidePanel", 
        &PanelManager::hideSidePanel
    ),
    GUI::UpdaterMaker<PanelManager>
    (
        "updateSidePanel", 
        &PanelManager::updateSidePanel
    ),
    m_contentID(),
    m_currentTool(-1),
    m_mainFrame(NULL),
    m_mainView(NULL),
    m_ready(false),
    m_dropable(false)
{
}

void PanelManager::finishLoad()
{
    m_ready = true;
}

void PanelManager::ualItemDblClick(UalItemInfo* ii)
{
    string fullname = ii->longText();
    BWResource::dissolveFilename(fullname);
    string dir       = BWResource::getFilePath(fullname);
    string file      = BWResource::getFilename(fullname);
    string extension = BWResource::getExtension(fullname);
    if (strcmpi(extension.c_str(), "xml") == 0)
    {
        ParticleEditorApp *app = (ParticleEditorApp *)AfxGetApp();
        app->openDirectory(dir);
        file = BWResource::removeExtension(file);
        ActionSelection *as = ActionSelection::instance();
        if (as != NULL)
        {
            as->selectMetaParticleSystem(file);
            UalHistory::instance()->add( ii->assetInfo() );
        }
    }
}

void PanelManager::ualStartDrag(UalItemInfo* ii)
{
    if (ii == NULL)
        return;

    UalDropManager::instance().start
    ( 
        BWResource::getExtension( ii->longText())
    );
}

void PanelManager::ualUpdateDrag(UalItemInfo* ii)
{
    if (ii == NULL)
        return;

    CWnd *activeView = MainFrame::instance()->GetActiveView();
    if (activeView == NULL)
        return;

    bool inView = 
        ::WindowFromPoint
        (
            CPoint(ii->x(), ii->y())
        ) 
        == 
        activeView->m_hWnd;

    if ((ii->isFolder()) || (UalDropManager::instance().test(ii)))
        SetCursor(AfxGetApp()->LoadStandardCursor(IDC_ARROW));
    else
        SetCursor(AfxGetApp()->LoadStandardCursor(IDC_NO));
}

void PanelManager::ualEndDrag(UalItemInfo* ii)
{
    SetCursor(AfxGetApp()->LoadStandardCursor(IDC_ARROW));

    if (ii == NULL)
        return;

    if (ii->isFolder())
    {
        // folder drag
        CPoint pt;
        GetCursorPos(&pt);
        AfxGetMainWnd()->ScreenToClient(&pt);
        GUITABS::Manager::instance()->clone
        (
            (GUITABS::Content*)(ii->dialog()),
            pt.x - 5, 
            pt.y - 5 
        );
    }
    else
    {
        UalDropManager::instance().end(ii);
    }
}
