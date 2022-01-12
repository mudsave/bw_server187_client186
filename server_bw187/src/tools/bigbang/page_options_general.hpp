/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#pragma once
#include "afxwin.h"
#include "afxcmn.h"
#include "controls/auto_tooltip.hpp"
#include "controls/slider.hpp"
#include "controls/edit_numeric.hpp"
#include "guitabs/guitabs_content.hpp"
#include <vector>


// OptionsShowTree

class OptionsShowTree : public CTreeCtrl
{
	DECLARE_DYNAMIC(OptionsShowTree)

public:
	OptionsShowTree();
	virtual ~OptionsShowTree();

	void populate( DataSectionPtr data, HTREEITEM item = NULL );
	void execute( HTREEITEM item );
	void update();

private:
	void update(HTREEITEM item, std::string name);

	std::vector< std::string* > itemData_;

	CPoint mousePoint_;

protected:
	DECLARE_MESSAGE_MAP()

public:
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnNMClick(NMHDR *pNMHDR, LRESULT *pResult);
};



// PageOptionsGeneral


class PageOptionsGeneral : public CFormView, public GUITABS::Content
{
	IMPLEMENT_BASIC_CONTENT( "Options", "General Options", 290, 425, NULL )

public:
	PageOptionsGeneral();
	virtual ~PageOptionsGeneral();

// Dialog Data
	enum { IDD = IDD_PAGE_OPTIONS_GENERAL };

private:
	bool pageReady_;
	void updateSliderEdits();

	bool dontUpdateFarClipEdit_;
	bool farClipEdited_;

	bool dontUpdateHeightEdit_;
	bool cameraHeightEdited_;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_AUTO_TOOLTIP(PageOptionsGeneral,CFormView)
	DECLARE_MESSAGE_MAP()

public:
	void InitPage();

	OptionsShowTree showTree_;
	CButton standardLighting_;
	CButton dynamicLighting_;
	CButton specularLighting_;
	controls::Slider farPlaneSlider_;
	controls::EditNumeric farPlaneEdit_;
	controls::Slider cameraHeightSlider_;
	controls::EditNumeric cameraHeightEdit_;
	CButton readOnlyMode_;

	int lastLightsType_;

	afx_msg LRESULT OnUpdateControls(WPARAM wParam, LPARAM lParam);
	afx_msg void OnBnClickedOptionsLightingStandard();
	afx_msg void OnBnClickedOptionsLightingDynamic();
	afx_msg void OnBnClickedOptionsLightingSpecular();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg LRESULT OnChangeEditNumeric(WPARAM mParam, LPARAM lParam);
	afx_msg void OnBnClickedOptionsRememberSelectionFilter();
	afx_msg void OnBnClickedReadOnlyMode();
	afx_msg void OnEnChangeOptionsCameraHeightEdit();
	CButton isPlayerPreviewModeEnabled_;
	afx_msg void OnBnClickedPlayerPreviewMode();
	afx_msg void OnShowWindow( BOOL bShow, UINT nStatus );
	afx_msg void OnEnChangeOptionsFarplaneEdit();
};

IMPLEMENT_BASIC_CONTENT_FACTORY( PageOptionsGeneral )
