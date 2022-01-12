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
#include "afxcmn.h"
#include "afxwin.h"
#include "page_terrain_base.hpp"
#include "limit_slider.hpp"
#include "controls/edit_numeric.hpp"
#include "controls/image_button.hpp"
#include "guitabs/guitabs_content.hpp"


// PageTerrainMesh

class PageTerrainMesh : public PageTerrainBase, public GUITABS::Content
{
	IMPLEMENT_BASIC_CONTENT( "Tool: Mesh", "Tool Options: Terrain Mesh Cut/Repair", 290, 200, NULL )

public:
	PageTerrainMesh();
	virtual ~PageTerrainMesh();

// Dialog Data
	enum { IDD = IDD_PAGE_TERRAIN_MESH };

private:
	bool pageReady_;
	void updateSliderEdits();

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CButton cut_;
	CButton repair_;
	LimitSlider sizeSlider_;
    controls::EditNumeric sizeEdit_;

	virtual BOOL OnInitDialog();
	afx_msg void OnSetFocus( CWnd* pOldWnd );
	afx_msg LRESULT OnActivateTool(WPARAM wParam, LPARAM lParam);
	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg LRESULT OnUpdateControls(WPARAM wParam, LPARAM lParam);
	afx_msg void OnBnClickedTerrainCut();
	afx_msg void OnBnClickedTerrainRepair();
	afx_msg void OnEnChangeTerrainSizeEdit();
	afx_msg void OnBnClickedSizerange();
	controls::ImageButton mSizeRange;
};

IMPLEMENT_CDIALOG_CONTENT_FACTORY( PageTerrainMesh, PageTerrainMesh::IDD )
