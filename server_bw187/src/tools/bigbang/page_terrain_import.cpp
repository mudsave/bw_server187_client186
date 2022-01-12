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
#include "page_terrain_import.hpp"
#include "bigbang.h"
#include "big_bang.hpp"
#include "user_messages.hpp"
#include "python_adapter.hpp"
#include "height/height_module.hpp"
#include "appmgr/options.hpp"
#include "guimanager/gui_manager.hpp"
#include <afxpriv.h>

const std::string PageTerrainImport::contentID = "PageTerrainImport";
PageTerrainImport *PageTerrainImport::s_instance_ = NULL;


namespace
{
    // Mode combobox data:
    std::pair<int, unsigned int> const ModeData[] =
    {
        std::make_pair((int)ElevationBlit::REPLACE    , IDS_EBM_REPLACE    ),
        std::make_pair((int)ElevationBlit::ADDITIVE   , IDS_EBM_ADDITIVE   ),
        std::make_pair((int)ElevationBlit::SUBTRACTIVE, IDS_EBM_SUBTRACTIVE),
        std::make_pair((int)ElevationBlit::OVERLAY    , IDS_EBM_OVERLAY    ),
        std::make_pair((int)ElevationBlit::MIN        , IDS_EBM_MIN        ),
        std::make_pair((int)ElevationBlit::MAX        , IDS_EBM_MAX        ),
        std::make_pair(-1, 0) // -1 is terminus
    };

    // Any import that requires more than this amount of memory for undo
    // will not be undoable and will affect things on disk:
    const size_t PROMPT_LARGE_IMPORT_SIZE = 15*1024*1024;

    // Any import/export that requires more than this amount of memory changed
    // will need a progress bar:
    const size_t PROGRESS_LARGE_IMPORT_SIZE = 1024*1024;
}


BEGIN_MESSAGE_MAP(PageTerrainImport, CFormView)
	ON_WM_SIZE()
	ON_MESSAGE (WM_ACTIVATE_TOOL, OnActivateTool)
	ON_MESSAGE(WM_UPDATE_CONTROLS, OnUpdateControls)
	ON_WM_HSCROLL()
    ON_BN_CLICKED(IDC_TERIMP_BROWSE_BTN, OnBrowseBtn)
    ON_EN_CHANGE(IDC_TERIMP_MIN_EDIT     , OnEditMin     )
    ON_EN_CHANGE(IDC_TERIMP_MAX_EDIT     , OnEditMax     )
    ON_EN_CHANGE(IDC_TERIMP_STRENGTH_EDIT, OnEditStrength)
    ON_CBN_SELCHANGE(IDC_TERIMP_MODE_CB  , OnModeSel)
    ON_MESSAGE(WM_RANGESLIDER_CHANGED, OnHeightSliderChanged)
    ON_MESSAGE(WM_RANGESLIDER_TRACK  , OnHeightSliderChanged)
    ON_BN_CLICKED(IDC_TERIMP_ANTICLOCKWISE_BTN, OnAnticlockwiseBtn)
    ON_BN_CLICKED(IDC_TERIMP_CLOCKWISE_BTN    , OnClockwiseBtn    )
    ON_BN_CLICKED(IDC_TERIMP_FLIPX_BTN        , OnFlipXBtn        )
    ON_BN_CLICKED(IDC_TERIMP_FLIPY_BTN        , OnFlipYBtn        )
    ON_BN_CLICKED(IDC_TERIMP_FLIPH_BTN        , OnFlipHeightBtn   )
    ON_BN_CLICKED(IDC_TERIMP_RECALCCLR_BTN    , OnRecalcClrBtn    )
    ON_UPDATE_COMMAND_UI(IDC_TERIMP_ANTICLOCKWISE_BTN, OnAnticlockwiseBtnEnable)
    ON_UPDATE_COMMAND_UI(IDC_TERIMP_CLOCKWISE_BTN    , OnClockwiseBtnEnable    )
    ON_UPDATE_COMMAND_UI(IDC_TERIMP_FLIPX_BTN        , OnFlipXBtnEnable        )
    ON_UPDATE_COMMAND_UI(IDC_TERIMP_FLIPY_BTN        , OnFlipYBtnEnable        )
    ON_UPDATE_COMMAND_UI(IDC_TERIMP_FLIPH_BTN        , OnFlipHeightBtnEnable   )
    ON_UPDATE_COMMAND_UI(IDC_TERIMP_RECALCCLR_BTN    , OnRecalcClrBtnEnable    )
    ON_BN_CLICKED(IDC_TERIMP_PLACE_BTN        , OnPlaceBtn        )
    ON_BN_CLICKED(IDC_TERIMP_CANCEL_BTN       , OnCancelImportBtn )
    ON_BN_CLICKED(IDC_TERIMP_EXPORT_BTN       , OnExportBtn       )
	ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTW, 0, 0xFFFF, OnToolTipText)
	ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTA, 0, 0xFFFF, OnToolTipText)
END_MESSAGE_MAP()


PageTerrainImport::PageTerrainImport()
: 
CFormView(PageTerrainImport::IDD),
pageReady_(false),
filterControls_(0),
loadedTerrain_(false)
{
    s_instance_ = this;
}


PageTerrainImport::~PageTerrainImport()
{
    s_instance_ = NULL;
}


void PageTerrainImport::setImportFile(char const *filename)
{
    importFileEdit_.SetWindowText(filename);
}


void PageTerrainImport::setQueryHeightRange(float minv, float maxv)
{    
    ++filterControls_;
    float theMinV = minEdit_.GetValue();
    float theMaxV = maxEdit_.GetValue();
    if
    (
        minv != -std::numeric_limits<float>::max()
        &&
        minv != +std::numeric_limits<float>::max()
    )
    {
        theMinV = minv;
    }
    if
    (
        maxv != -std::numeric_limits<float>::max()
        &&
        maxv != +std::numeric_limits<float>::max()
    )
    {
        theMaxV = maxv;
    }
    if (theMinV > theMaxV) theMaxV = theMinV;
    if (theMaxV < theMinV) theMinV = theMaxV;
    minEdit_.SetValue(theMinV);
    maxEdit_.SetValue(theMaxV);
    heightSlider_.setThumbValues(theMinV, theMaxV);
    updateHMImportOptions();
    --filterControls_;
}


/*static*/ PageTerrainImport *PageTerrainImport::instance() 
{ 
    return s_instance_; 
}


void PageTerrainImport::DoDataExchange(CDataExchange* pDX)
{
	CFormView::DoDataExchange(pDX);

    DDX_Control(pDX, IDC_TERIMP_IMPORT_GROUP        , importGrp_              );
    DDX_Control(pDX, IDC_TERIMP_FILE_STATIC         , fileStatic_             );
    DDX_Control(pDX, IDC_TERIMP_IMPORT_FILE         , importFileEdit_         );
    DDX_Control(pDX, IDC_TERIMP_BROWSE_BTN          , browseButton_           );
    DDX_Control(pDX, IDC_TERIMP_EXPORT_BTN          , exportBtn_              );
    DDX_Control(pDX, IDC_TERIMP_MIN_EDIT            , minEdit_                );
    DDX_Control(pDX, IDC_TERIMP_HEIGHT_SLIDER       , heightSlider_           );
    DDX_Control(pDX, IDC_TERIMP_MAX_EDIT            , maxEdit_                );
    DDX_Control(pDX, IDC_TERIMP_STRENGTH_EDIT       , strengthEdit_           );
    DDX_Control(pDX, IDC_TERIMP_STRENGTH_SLIDER     , strengthSlider_         );
    DDX_Control(pDX, IDC_TERIMP_MODE_CB             , modeCB_                 );

    DDX_Control(pDX, IDC_TERIMP_PLACE_BTN           , placeBtn_               );
    DDX_Control(pDX, IDC_TERIMP_CANCEL_BTN          , cancelBtn_              );

    DDX_Control(pDX, IDC_TERIMP_HANDDRAWN_GROUP     , handrawnGrp_            );
    DDX_Control(pDX, IDC_TERIMP_MAP_ALPHA_SLIDER    , blendSlider_            );
}

void PageTerrainImport::InitPage()
{
    if (pageReady_)
        return;

    ++filterControls_;

	blendSlider_.SetRangeMin(1);
	blendSlider_.SetRangeMax(100);
	blendSlider_.SetPageSize(0);

    minEdit_     .SetValue(1.0f); // dummy set forces redraw
    minEdit_     .SetValue(0.0f);
    heightSlider_.setRange(-2000.0f, +2000.0f);
    heightSlider_.setThumbValues(0.0f, 50.0f);
    maxEdit_     .SetValue(50.0f);

    strengthEdit_  .SetValue(100.0f);
    strengthSlider_.SetRange(0, 100, TRUE);
    strengthSlider_.SetPos  (100);

    modeCB_.Clear();
    for (int i = 0; ModeData[i].first >= 0; ++i)
    {
        CString str((LPCSTR)ModeData[i].second);
        modeCB_.AddString(str);
    }
    modeCB_.SetCurSel(0);

    // NOTE: Tooltip initialization should be done before creating the toolbar
    // below.  If it isn't then a tooltip is created for the toolbar itself
    // which can contain junk.
    INIT_AUTO_TOOLTIP();

    // Initialize the toolbar with flip, rotate etc: 
    actionTB_.CreateEx
    (
        this, 
        TBSTYLE_FLAT, 
        WS_CHILD | WS_VISIBLE | CBRS_ALIGN_TOP
    );
    actionTB_.LoadToolBarEx(IDR_IMP_TER_TOOLBAR, IDR_IMP_TER_DIS_TOOLBAR);
    actionTB_.SetBarStyle(CBRS_ALIGN_TOP | CBRS_TOOLTIPS | CBRS_FLYBY);              
    actionTB_.Subclass(IDC_TERIMP_ACTIONTB);     
    actionTB_.ShowWindow(SW_SHOW);

    updateHMImportOptions();

    --filterControls_;    

	pageReady_ = true;
}


LRESULT PageTerrainImport::OnActivateTool(WPARAM wParam, LPARAM lParam)
{
	if (PythonAdapter::instance())
		PythonAdapter::instance()->onPageControlTabSelect("pgc", "TerrainImport");
    updateState();

	return 0;
}


afx_msg void PageTerrainImport::OnSize( UINT nType, int cx, int cy )
{
	CFormView::OnSize(nType, cx, cy);

	if ( !pageReady_ )
		return;

	static const int MARGIN = 10;
	CRect rect;
	GetWindowRect(rect);

    const int padding = 3;
	CRect itemRect;

    importGrp_.GetWindowRect(itemRect);
    ScreenToClient(itemRect);
    importGrp_.SetWindowPos
    (
        NULL,
        padding, itemRect.top,
        cx - 2*padding, itemRect.Height(),
        SWP_NOZORDER | SWP_NOACTIVATE        
    );
    fileStatic_.GetWindowRect(itemRect);
    ScreenToClient(itemRect);
    int fileRight = itemRect.right;
    importFileEdit_.GetWindowRect(itemRect);
    ScreenToClient(itemRect);
    importFileEdit_.SetWindowPos
    (
        NULL,
        fileRight + padding, itemRect.top,
        cx - fileRight - 3*padding, itemRect.Height(),
        SWP_NOZORDER | SWP_NOACTIVATE 
    );
    minEdit_.GetWindowRect(itemRect);
    ScreenToClient(itemRect);
    int minEditLeft  = itemRect.left;    
    int minEditRight = itemRect.right;
    maxEdit_.GetWindowRect(itemRect);
    ScreenToClient(itemRect);
    int maxEditRight = cx - 2*padding;
    maxEdit_.SetWindowPos
    (
        NULL,
        cx - itemRect.Width() - 4*padding, itemRect.top,
        itemRect.Width(), itemRect.Height(),
        SWP_NOZORDER | SWP_NOACTIVATE 
    );
    heightSlider_.GetWindowRect(itemRect);
    ScreenToClient(itemRect);
    heightSlider_.SetWindowPos
    (
        NULL,
        minEditLeft, itemRect.top,
        maxEditRight - minEditLeft, itemRect.Height(),
        SWP_NOZORDER | SWP_NOACTIVATE 
    );
    strengthSlider_.GetWindowRect(itemRect);
    ScreenToClient(itemRect);
    strengthSlider_.SetWindowPos
    (
        NULL,
        minEditRight + padding, itemRect.top,
        cx - minEditRight - 3*padding, itemRect.Height(),
        SWP_NOZORDER | SWP_NOACTIVATE 
    );

    handrawnGrp_.GetWindowRect(itemRect);
    ScreenToClient(itemRect);
    handrawnGrp_.SetWindowPos
    (
        NULL,
        padding, itemRect.top,
        cx - 2*padding, itemRect.Height(),
        SWP_NOZORDER | SWP_NOACTIVATE
    );
	blendSlider_.GetWindowRect(itemRect);
    ScreenToClient(itemRect);
	blendSlider_.SetWindowPos
    (
        NULL, 
        padding, itemRect.top,
		cx - 4*padding, itemRect.Height(),
		SWP_NOZORDER | SWP_NOACTIVATE 
    );   
}


void PageTerrainImport::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	if ( !pageReady_ )
		InitPage();

    if (pScrollBar != NULL)
    {
        if (pScrollBar->GetSafeHwnd() == blendSlider_.GetSafeHwnd())
        {
	        if (PythonAdapter::instance())
	        {
		        PythonAdapter::instance()->onSliderAdjust
                (
                    "slrProjectMapBlend", 
				    (float)blendSlider_.GetPos(), 
				    (float)blendSlider_.GetRangeMin(), 
				    (float)blendSlider_.GetRangeMax()
                );		
	        }
        }

        if (pScrollBar->GetSafeHwnd() == strengthSlider_.GetSafeHwnd())
        {
            OnStrengthSlider(nSBCode == TB_ENDTRACK);
        }
    }

	CFormView::OnHScroll(nSBCode, nPos, pScrollBar);
}


afx_msg LRESULT PageTerrainImport::OnUpdateControls(WPARAM wParam, LPARAM lParam)
{
	if ( !pageReady_ )
		InitPage();

	if ( !IsWindowVisible() )
		return 0;

	MF_ASSERT(PythonAdapter::instance());
	PythonAdapter::instance()->sliderUpdate
    (
        &blendSlider_, 
        "slrProjectMapBlend"
    );

    SendMessageToDescendants
    (
        WM_IDLEUPDATECMDUI,
        (WPARAM)TRUE, 
        0, 
        TRUE, 
        TRUE
    );

	return 0;
}


void PageTerrainImport::OnBrowseBtn()
{
    HeightModule *hm = HeightModule::currentInstance();
    if (hm == NULL)
        return;

	static char szFilter[] = 
        "Greyscale Heightmaps (*.r16;*.raw;*.bmp)|*.r16;*.raw;*.bmp|"
        "Terragen Files (*.ter)|*.ter|"
        "DTED2 Files (*.dt2)|*.dt2|"
        "All Files (*.*)|*.*||";

	CFileDialog openFile(TRUE, "r16", NULL, OFN_FILEMUSTEXIST, szFilter);

	if (openFile.DoModal() != IDOK)
		return;

	CString filename = openFile.GetPathName();
    SmartPointer<ElevationImage> image = new ElevationImage();
    float left   = std::numeric_limits<float>::max();
    float top    = std::numeric_limits<float>::max();
    float right  = std::numeric_limits<float>::max();
    float bottom = std::numeric_limits<float>::max();
    ElevationCodec::LoadResult loadResult = 
        image->load(filename.GetBuffer(), &left, &top, &right, &bottom, true);
    switch (loadResult)
    {
    case ElevationCodec::LR_OK:
        {
        setImportFile(filename.GetBuffer());
        loadedTerrain_ = true;
        ++filterControls_;
        float minv, maxv;
        image->range(minv, maxv);
        minEdit_.SetValue(minv);
        maxEdit_.SetValue(maxv);
        heightSlider_.setThumbValues(minv, maxv);
        --filterControls_;
        image->normalize();
        updateHMImportOptions();
        hm->setElevationData
        (
            image,
            left   != std::numeric_limits<float>::max() ? &left   : NULL,
            top    != std::numeric_limits<float>::max() ? &top    : NULL,
            right  != std::numeric_limits<float>::max() ? &right  : NULL,
            bottom != std::numeric_limits<float>::max() ? &bottom : NULL
        );
        }
        break;
    case ElevationCodec::LR_BAD_FORMAT:
        AfxMessageBox(IDS_FAILED_IMPORT_TERRAIN, MB_OK);
        setImportFile("");
        loadedTerrain_ = false;
        hm->setElevationData(NULL);
        break;
    case ElevationCodec::LR_CANCELLED:            
        setImportFile("");
        loadedTerrain_ = false;
        hm->setElevationData(NULL);
        break;
    case ElevationCodec::LR_UNKNOWN_CODEC:
        AfxMessageBox(IDS_BADCODEC_IMPORT_TERRAIN, MB_OK);
        setImportFile("");
        loadedTerrain_ = false;
        hm->setElevationData(NULL);
        break;
    }
    updateState();
}


void PageTerrainImport::OnEditMin()
{
    if (filterControls_ == 0)
    {
        ++filterControls_;
        heightSlider_.setThumbValues(minEdit_.GetValue(), maxEdit_.GetValue());
        updateHMImportOptions();
        --filterControls_;
    }
}


void PageTerrainImport::OnEditMax()
{
    if (filterControls_ == 0)
    {
        ++filterControls_;
        heightSlider_.setThumbValues(minEdit_.GetValue(), maxEdit_.GetValue());
        updateHMImportOptions();
        --filterControls_;
    }
}


void PageTerrainImport::OnEditStrength()
{
    if (filterControls_ == 0)
    {
        ++filterControls_;
        strengthSlider_.SetPos(static_cast<int>(maxEdit_.GetValue()));
        updateHMImportOptions();
        --filterControls_;
    }
}


void PageTerrainImport::OnModeSel()
{
    if (filterControls_ == 0)
    {
        ++filterControls_;
        updateHMImportOptions();
        --filterControls_;
    }
}


void PageTerrainImport::OnAnticlockwiseBtn()
{
    HeightModule *hm = HeightModule::currentInstance();
    if (hm != NULL)
    {
        hm->rotateElevationData(false); // false = anticlockwise
    }
}


void PageTerrainImport::OnClockwiseBtn()
{
    HeightModule *hm = HeightModule::currentInstance();
    if (hm != NULL)
    {
        hm->rotateElevationData(true); // true = clockwise
    }
}


void PageTerrainImport::OnFlipXBtn()
{
    HeightModule *hm = HeightModule::currentInstance();
    if (hm != NULL)
    {
        hm->flipElevationData(HeightModule::FLIP_X);
    }
}


void PageTerrainImport::OnFlipYBtn()
{
    HeightModule *hm = HeightModule::currentInstance();
    if (hm != NULL)
    {
        hm->flipElevationData(HeightModule::FLIP_Y);
    }
}


void PageTerrainImport::OnFlipHeightBtn()
{
    HeightModule *hm = HeightModule::currentInstance();
    if (hm != NULL)
    {
        hm->flipElevationData(HeightModule::FLIP_HEIGHT);
    }
}


void PageTerrainImport::OnRecalcClrBtn()
{
    HeightModule *hm = HeightModule::currentInstance();
    if (hm != NULL)
    {
        hm->updateMinMax();
    }
}


void PageTerrainImport::OnAnticlockwiseBtnEnable(CCmdUI *cmdui)
{
    HeightModule *hm = HeightModule::currentInstance();
    if (hm == NULL)
        return;
    BOOL importing = hm->getMode() == HeightModule::IMPORT_TERRAIN;
    cmdui->Enable(importing);
}


void PageTerrainImport::OnClockwiseBtnEnable(CCmdUI *cmdui)
{
    OnAnticlockwiseBtnEnable(cmdui); // these are enabled/disabled together
}


void PageTerrainImport::OnFlipXBtnEnable(CCmdUI *cmdui)
{
    OnAnticlockwiseBtnEnable(cmdui); // these are enabled/disabled together
}


void PageTerrainImport::OnFlipYBtnEnable(CCmdUI *cmdui)
{
    OnAnticlockwiseBtnEnable(cmdui); // these are enabled/disabled together
}


void PageTerrainImport::OnFlipHeightBtnEnable(CCmdUI *cmdui)
{
    OnAnticlockwiseBtnEnable(cmdui); // these are enabled/disabled together
}


void PageTerrainImport::OnRecalcClrBtnEnable(CCmdUI *cmdui)
{
    OnAnticlockwiseBtnEnable(cmdui); // these are enabled/disabled together
}


void PageTerrainImport::OnPlaceBtn()
{
    HeightModule *hm = HeightModule::currentInstance();
    if (hm != NULL)
    {
        size_t sizeUndo = hm->undoSize();
        bool doUndo   = sizeUndo < PROMPT_LARGE_IMPORT_SIZE;
        bool progress = sizeUndo > PROGRESS_LARGE_IMPORT_SIZE;
        bool doImport = true;
        if (!doUndo)
        {
            doImport =
                AfxMessageBox(IDS_LARGE_IMPORT, MB_YESNO)
                ==
                IDYES;
        }
        if (doImport)
        {
            CWaitCursor waitCursor;
            hm->doImport(doUndo, progress, doUndo);
            hm->setElevationData(NULL);
            importFileEdit_.SetWindowText("");
            updateState();
            GUI::Manager::instance()->update();
        }
    }
}


void PageTerrainImport::OnCancelImportBtn()
{
    HeightModule *hm = HeightModule::currentInstance();
    if (hm != NULL)
    {
        hm->setElevationData(NULL);
        importFileEdit_.SetWindowText("");
        updateState();
        hm->updateMinMax();
    }
} 


void PageTerrainImport::OnExportBtn()
{
    HeightModule *hm = HeightModule::currentInstance();
    if (hm != NULL)
    {
	    static char szFilter[] = 
			"RAW Files (*.r16;*.raw)|*.r16;*.raw|"
            "BMP Files (*.bmp)|*.bmp|"            
            "Terragen Files (*.ter)|*.ter|"
            "All Files (*.*)|*.*||";

	    CFileDialog openFile(FALSE, "r16", NULL, OFN_OVERWRITEPROMPT, szFilter);

        if (openFile.DoModal() != IDOK)
            return;

        size_t sizeUndo = hm->undoSize();
        bool   progress = sizeUndo > PROGRESS_LARGE_IMPORT_SIZE;
        
        CWaitCursor waitCursor;
        hm->doExport(openFile.GetPathName().GetBuffer(), progress);
    }
}


BOOL PageTerrainImport::OnToolTipText(UINT, NMHDR* pNMHDR, LRESULT *result)
{
    // Allow top level routing frame to handle the message
    if (GetRoutingFrame() != NULL)
        return FALSE;

    // Need to handle both ANSI and UNICODE versions of the message
    TOOLTIPTEXTA* pTTTA = (TOOLTIPTEXTA*)pNMHDR;
    TOOLTIPTEXTW* pTTTW = (TOOLTIPTEXTW*)pNMHDR;
    TCHAR szFullText[256];
    CString cstTipText;
    CString cstStatusText;

    UINT nID = pNMHDR->idFrom;
    if 
    (
        pNMHDR->code == TTN_NEEDTEXTA 
        && 
        (pTTTA->uFlags & TTF_IDISHWND) 
        ||
        pNMHDR->code == TTN_NEEDTEXTW 
        && 
        (pTTTW->uFlags & TTF_IDISHWND)
    )
    {
        // idFrom is actually the HWND of the tool
        nID = ((UINT)(WORD)::GetDlgCtrlID((HWND)nID));
    }

    if (nID != 0) // will be zero on a separator
    {
        AfxLoadString(nID, szFullText);
            // this is the command id, not the button index
        cstTipText    = szFullText;
        cstStatusText = szFullText;
    }

    // Non-UNICODE Strings only are shown in the tooltip window...
    if (pNMHDR->code == TTN_NEEDTEXTA)
    {
        lstrcpyn
        (
            pTTTA->szText, 
            cstTipText,
            (sizeof(pTTTA->szText)/sizeof(pTTTA->szText[0]))            
        );
    }
    else
    {
        _mbstowcsz
        (
            pTTTW->szText, 
            cstTipText,
            (sizeof(pTTTW->szText)/sizeof(pTTTW->szText[0]))
        );
    }
    *result = 0;

    // bring the tooltip window above other popup windows
    ::SetWindowPos
    (
        pNMHDR->hwndFrom, 
        HWND_TOP, 
        0, 
        0, 
        0, 
        0,
        SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOMOVE
    );

    return TRUE;    // message was handled
}


void PageTerrainImport::OnStrengthSlider(bool /*endTrack*/)
{
    if (filterControls_ == 0)
    {
        ++filterControls_;
        float strength = static_cast<float>(strengthSlider_.GetPos());
        strengthEdit_.SetValue(strength);
        updateHMImportOptions();
        --filterControls_;
    }
}


LRESULT PageTerrainImport::OnHeightSliderChanged(WPARAM wParam, LPARAM lParam)
{
    if (filterControls_ == 0)
    {
        ++filterControls_;
        float minv, maxv;
        heightSlider_.getThumbValues(minv, maxv);
        minEdit_.SetValue(minv);
        maxEdit_.SetValue(maxv);
        updateHMImportOptions();
        --filterControls_;
    }
    return TRUE;
}


void PageTerrainImport::updateState()
{
    if (!pageReady_)
        InitPage();

    HeightModule *hm = HeightModule::currentInstance();
    if (hm == NULL)
        return;

    BOOL importing = hm->getMode() == HeightModule::IMPORT_TERRAIN;
    BOOL exporting = hm->getMode() == HeightModule::EXPORT_TERRAIN;

    strengthEdit_  .EnableWindow(importing);
    strengthSlider_.EnableWindow(importing);
    modeCB_        .EnableWindow(importing);
    placeBtn_      .EnableWindow(importing);
    cancelBtn_     .EnableWindow(importing);
    exportBtn_     .EnableWindow(exporting);
}


void PageTerrainImport::updateHMImportOptions()
{
    HeightModule *hm = HeightModule::currentInstance();
    if (hm == NULL)
        return;

    float minv     = minEdit_.GetValue();
    float maxv     = maxEdit_.GetValue();
    float strength = strengthEdit_.GetValue()/100.0f;
    ElevationBlit::Mode mode = 
        (ElevationBlit::Mode)ModeData[modeCB_.GetCurSel()].first;
    hm->setImportOptions(mode, minv, maxv, strength);
}
