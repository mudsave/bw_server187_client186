/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef PAGE_TERRAIN_IMPORT_HPP
#define PAGE_TERRAIN_IMPORT_HPP


#include "guitabs/guitabs_content.hpp"
#include "resource.h"
#include "controls/edit_numeric.hpp"
#include "controls/slider.hpp"
#include "controls/range_slider_ctrl.hpp"
#include "controls/dialog_toolbar.hpp"
#include "controls/auto_tooltip.hpp"


class PageTerrainImport : public CFormView, public GUITABS::Content
{
public:
	IMPLEMENT_BASIC_CONTENT( "Tool: Import/Export", "Tool Options: Terrain Import/Export", 290, 390, NULL )

public:
    enum { IDD = IDD_PAGE_TERRAIN_IMPORT };

	PageTerrainImport();

	/*virtual*/ ~PageTerrainImport();

    void setImportFile(char const *filename);

    void setQueryHeightRange(float minv, float maxv);

    static PageTerrainImport *instance();

protected:
	void InitPage();

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	afx_msg LRESULT OnActivateTool(WPARAM wParam, LPARAM lParam);
	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg LRESULT OnUpdateControls(WPARAM wParam, LPARAM lParam);
    afx_msg void OnBrowseBtn();
    afx_msg void OnEditMin();
    afx_msg void OnEditMax();
    afx_msg void OnEditStrength();
    afx_msg void OnModeSel();
    afx_msg void OnAnticlockwiseBtn();
    afx_msg void OnClockwiseBtn    ();
    afx_msg void OnFlipXBtn        ();
    afx_msg void OnFlipYBtn        ();
    afx_msg void OnFlipHeightBtn   ();
    afx_msg void OnRecalcClrBtn    ();
    afx_msg void OnAnticlockwiseBtnEnable(CCmdUI *cmdui);
    afx_msg void OnClockwiseBtnEnable    (CCmdUI *cmdui);
    afx_msg void OnFlipXBtnEnable        (CCmdUI *cmdui);
    afx_msg void OnFlipYBtnEnable        (CCmdUI *cmdui);
    afx_msg void OnFlipHeightBtnEnable   (CCmdUI *cmdui);
    afx_msg void OnRecalcClrBtnEnable    (CCmdUI *cmdui);
    afx_msg void OnPlaceBtn();
    afx_msg void OnCancelImportBtn();
    afx_msg void OnExportBtn();
    afx_msg BOOL OnToolTipText(UINT, NMHDR* pNMHDR, LRESULT *result);
    DECLARE_MESSAGE_MAP()

    DECLARE_AUTO_TOOLTIP(PageTerrainImport, CFormView)

    LRESULT OnHeightSliderChanged(WPARAM wParam, LPARAM lParam);
    void OnStrengthSlider(bool endTrack);

    void updateState();

    void updateHMImportOptions();

private:
	bool                        pageReady_;
    CStatic                     importGrp_;
    CStatic                     fileStatic_;
    CEdit                       importFileEdit_;
    CButton                     browseButton_;
    CButton                     exportBtn_;
    controls::EditNumeric       minEdit_;
    controls::RangeSliderCtrl   heightSlider_;
    controls::EditNumeric       maxEdit_;
    controls::EditNumeric       strengthEdit_;
    controls::Slider            strengthSlider_;
    CComboBox                   modeCB_;
    controls::DialogToolbar     actionTB_;
    CButton                     placeBtn_;
    CButton                     cancelBtn_;    
    CStatic                     handrawnGrp_;
	controls::Slider            blendSlider_;
    size_t                      filterControls_; 
    bool                        loadedTerrain_;
    static PageTerrainImport    *s_instance_;
};


IMPLEMENT_BASIC_CONTENT_FACTORY(PageTerrainImport)


#endif // PAGE_TERRAIN_IMPORT_HPP
