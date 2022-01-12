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
#include "resource.h"

#include "guitabs/guitabs_content.hpp"

#include "controls/range_slider_ctrl.hpp"
#include "controls/edit_numeric.hpp"
#include "controls/auto_tooltip.hpp"

#include "tree_list.hpp"

#include <set>

typedef std::pair < std::string , std::string > StringPair;



class SliderContainer : public ReferenceCount
{
public:
	SliderContainer( controls::RangeSliderCtrl* slider, controls::EditNumeric* minEd, controls::EditNumeric* maxEd, int group = 0 );

	controls::RangeSliderCtrl* slider_;
	controls::EditNumeric* minEd_;
	controls::EditNumeric* maxEd_;
	int group_;
};

typedef SmartPointer<SliderContainer> SliderContainerPtr;

// PageActions

class PageActions: public TreeList, public GUITABS::Content
{
	DECLARE_DYNCREATE(PageActions)

	IMPLEMENT_BASIC_CONTENT( "Actions", "Actions", 285, 886, NULL )

	DECLARE_AUTO_TOOLTIP( PageActions, TreeList )

public:
	PageActions();
	virtual ~PageActions();
	
	static PageActions* currPage();

	//These are exposed to python as:
	bool isLockedAct();	// isActLocked()
	void actNew();		// newAct()
	void actRemove();	// removeAct()
	void actPromote();	// promoteAct()
	void actDemote();	// demoteAct()

// Dialog Data
	enum { IDD = IDD_ACTIONS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

private:

	SmartPointer< struct PageActionsImpl > pImpl_;
	
	void OnGUIManagerCommand(UINT nID);
	void OnGUIManagerCommandUpdate(CCmdUI * cmdUI);
	afx_msg LRESULT OnShowTooltip(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnHideTooltip(WPARAM wParam, LPARAM lParam);

	SliderContainerPtr find( void* ptr );
	void writeEditNum( controls::EditNumeric* edit, float num, int group = 0 );
	void sliderFromEdits( SliderContainerPtr slider );
	LRESULT OnRangeSliderChanged(WPARAM wParam, LPARAM lParam);
	LRESULT OnRangeSliderTrack(WPARAM wParam, LPARAM lParam);
	
	void initMatcherField( controls::EditNumeric& field, int dec = 0, float min = 0.f, float max = 100.f, bool allowNeg = false, bool allowEmpty = true );
		
	virtual BOOL OnInitDialog();

	std::string getNamedCaps( const std::string& capsStr );
	std::map< int, std::string > capsNames_;

	void OnSize(UINT nType, int cx, int cy);

	void updateCheck( CButton& button, const StringPair& actID, const std::string& flagName );
	void disableCheck( CButton& button );
	void updateMatchField( controls::EditNumeric& field, const StringPair& actID, const std::string& typeName, const std::string& flagName, bool lockedParents = false, bool canBeNeg = true );
	void disableField( CEdit& field );

	afx_msg LRESULT OnUpdateControls(WPARAM wParam, LPARAM lParam);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnEnChangeActName();
	afx_msg void updateName();
	afx_msg void OnChangeActAnim();
	void updateFlags();
	afx_msg void updateTrack();
	std::string getFieldText( CEdit& field );
	void updateTriggerFlags();
	void updateTriggerFlags( bool& shouldUndo );
	void updateCancelFlags();
	void updateCancelFlags( bool& shouldUndo );
	void updateMatchFlags();
	afx_msg void OnBnClickedActTriggerAction();

	
public:
	afx_msg void OnBnClickedActTriggerCapsOnSel();
	afx_msg void OnBnClickedActTriggerCapsOffSel();
	afx_msg void OnBnClickedActCancelCapsOnSel();
	afx_msg void OnBnClickedActCancelCapsOffSel();
	afx_msg void OnBnClickedActTriggerFalling();
	afx_msg void OnBnClickedActCancelFalling();
};

IMPLEMENT_BASIC_CONTENT_FACTORY( PageActions )
