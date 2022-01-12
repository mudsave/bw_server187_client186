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
#include "directory_browser.hpp"
#include "file_image_list.hpp"
#include "limit_slider.hpp"
#include "controls/edit_numeric.hpp"
#include "controls/image_button.hpp"
#include "guitabs/guitabs_content.hpp"
#include "ual/ual_drop_manager.hpp"

class CRCStatic : public CStatic
{
protected:
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
};

// PageTerrainTexture

class PageTerrainTexture : public PageTerrainBase, public GUITABS::Content
{
	IMPLEMENT_BASIC_CONTENT( "Tool: Texture", "Tool Options: Terrain Texturing", 290, 380, NULL )
	static const int TEXTURE_CHANNEL_NUM = 4;
public:
	PageTerrainTexture();
	virtual ~PageTerrainTexture();

// Dialog Data
	enum { IDD = IDD_PAGE_TERRAIN_TEXTURE };

private:
	CBitmap waterMarkBmp_;
	CBitmap* monoBmp_;
	CSize waterMarkSize_;
	CDC waterMark_;
	bool pageReady_;
	void updateSliderEdits();
	CRCStatic mTextureChannel1;
	CRCStatic mTextureChannel2;
	CRCStatic mTextureChannel3;
	CRCStatic mTextureChannel4;
	int currentChannel_;
	CRCStatic* textureChannels_[ TEXTURE_CHANNEL_NUM ];
	static std::string getThumbFileName( std::string filename );

	std::vector<std::string> channelTextures_;
	std::string channelTextureFileName_[ TEXTURE_CHANNEL_NUM ];

	void pickTerrainTexture();
	void selectChannel( int index );
	void changeTextureTerrain( int direction );
	void changeTextureChannel( int direction );
	void LoadWaterMark();
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	LimitSlider sizeSlider_;
    controls::EditNumeric sizeEdit_;
	LimitSlider strengthSlider_;
	controls::EditNumeric strengthEdit_;

	virtual BOOL OnInitDialog();
	afx_msg void OnSetFocus( CWnd* pOldWnd );
	afx_msg LRESULT OnActivateTool(WPARAM wParam, LPARAM lParam);
	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg LRESULT OnUpdateControls(WPARAM wParam, LPARAM lParam);
	afx_msg void OnBnClickedTerrainAlphaChannel1();
	afx_msg void OnBnClickedTerrainAlphaChannel2();
	afx_msg void OnBnClickedTerrainAlphaChannel3();
	afx_msg void OnBnClickedTerrainAlphaChannel4();
	afx_msg void OnNMClickFileList(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnEnChangeTerrainSizeEdit();
	afx_msg void OnEnChangeTerrainStrengthEdit();
	afx_msg void OnBnClickedSizerange();
	afx_msg void OnBnClickedStrengthrange();
	controls::ImageButton mSizeRange;
	controls::ImageButton mStrengthRange;

private:
	bool OnUalDrop( UalItemInfo* ii );
public:
	afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
	afx_msg void OnStnClickedTexturechannel1();
	afx_msg void OnStnClickedTexturechannel2();
	afx_msg void OnStnClickedTexturechannel3();
	afx_msg void OnStnClickedTexturechannel4();

	void onTextureChannelRightClick( CRCStatic* channel );

	typedef PageTerrainTexture This;
	PY_MODULE_STATIC_METHOD_DECLARE( py_setTerrainTexture )
	PY_MODULE_STATIC_METHOD_DECLARE( py_setTerrainChannel )
	PY_MODULE_STATIC_METHOD_DECLARE( py_changeTextureTerrain )
public:
	CStatic mTexturesInChunk;
};

IMPLEMENT_CDIALOG_CONTENT_FACTORY( PageTerrainTexture, PageTerrainTexture::IDD )
