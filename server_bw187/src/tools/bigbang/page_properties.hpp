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
#include "property_list.hpp"
#include "afxcmn.h"
#include "controls/slider.hpp"

#include "guitabs/guitabs_content.hpp"


class BaseView;
class PropertyItem;

// PageProperties

class PageProperties : public CDialog, public GUITABS::Content
{
	IMPLEMENT_BASIC_CONTENT( "Properties", "Properties", 290, 380, NULL )

public:
	PageProperties();
	virtual ~PageProperties();

	virtual BOOL OnInitDialog();

	virtual BOOL PreTranslateMessage(MSG* pMsg);

	// Dialog Data
	enum { IDD = IDD_PAGE_PROPERTIES };

	static PageProperties& instance();
	bool active() const { return active_; }

	void addView(BaseView* view);
	void clear();

	void setSliderColour(Moo::Colour c);
	Moo::Colour getSliderColour();
	void setOldSliderColour(Moo::Colour c) { oldSliderColour_ = c; }
	Moo::Colour getOldSliderColour() { return oldSliderColour_; }

	float getSliderMultiplier();
	void setSliderMultiplier( float f );
	float getOldSliderMultiplier() { return oldSliderMultiplier_; }
	void setOldSliderMultiplier( float f ) { oldSliderMultiplier_ = f; }

	void adviseSelectedId( std::string id );

	void OnListAddItem();
	void OnListItemRemoveItem();

	void enableMultiplier();
	void disableMultiplier();

	void enableColour();
	void disableColour();

	bool isTracking()	{	return tracking_;	}
private:
	bool tracking_;
	int multiplierCount_;
	int colourCount_;
	void addItemsForView(BaseView* view);
	void addItems();

	static PageProperties * instance_;
	bool active_;

	PropertyList propertyList_;

	typedef std::list<BaseView*> ViewList;
	ViewList viewList_;

	Moo::Colour oldSliderColour_;

	float oldSliderMultiplier_;

	PropertyItem * rclickItem_;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	afx_msg LRESULT OnUpdateControls(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnSelectPropertyItem(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnChangePropertyItem(WPARAM wParam, LPARAM lParam);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg LRESULT OnDblClkPropertyItem(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnRClkPropertyItem(WPARAM wParam, LPARAM lParam);
	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg void OnClose();
	DECLARE_MESSAGE_MAP()

	void InitPage();
	void EndPage();

	void resizePage();

	controls::Slider redSlider_;
	controls::Slider greenSlider_;
	controls::Slider blueSlider_;
	controls::Slider multiplierSlider_;
public:
	CStatic multiplierStatic_;
	CStatic mStaticColourRed;
	CStatic mStaticColourGreen;
	CStatic mStaticColourBlue;
	CStatic mStaticColourAdjust;
};

IMPLEMENT_CDIALOG_CONTENT_FACTORY( PageProperties, PageProperties::IDD )
