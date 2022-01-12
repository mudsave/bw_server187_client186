/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// mainframe.hpp : interface of the MainFrame class
//


#pragma once

#include "guimanager/gui_functor_cpp.hpp"
#include "common/base_mainframe.hpp"


// MainFrame
class MainFrame : public BaseMainFrame,
	GUI::ActionMaker<MainFrame>, // save prefab
	GUI::ActionMaker<MainFrame, 1>, // show toolbar
	GUI::ActionMaker<MainFrame, 2>, // hide toolbar
	GUI::ActionMaker<MainFrame, 3>, // show status bar
	GUI::ActionMaker<MainFrame, 4>, // hide status bar
	GUI::ActionMaker<MainFrame, 5>, // show player preview
	GUI::ActionMaker<MainFrame, 6>, // hide player preview
	GUI::UpdaterMaker<MainFrame>, // update show toolbar
	GUI::UpdaterMaker<MainFrame, 1>, // update show status bar
	GUI::UpdaterMaker<MainFrame, 2>, // update player preview
	GUI::UpdaterMaker<MainFrame, 3> // update tool mode
{
	
protected: // create from serialization only
	MainFrame();
	DECLARE_DYNCREATE(MainFrame)

// Attributes
public:

// Operations
public:

// Overrides
public:
// Implementation
public:
	virtual ~MainFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	void frameUpdate( bool forceRedraw = false );

public:  
	CStatusBar	m_wndStatusBar;

	bool resizing() { return resizing_; }

private:
	typedef struct _object PyObject;
	PyObject * pScriptObject_;
	bool resizing_;

	void onButtonClick();

	std::string GetActionName(UINT nID);

	CString triangles_;
	
	bool initialised_;

// Generated message map functions
protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	DECLARE_MESSAGE_MAP()

	virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext);
	virtual BOOL OnWndMsg(UINT message, WPARAM wParam, LPARAM lParam, LRESULT* pResult);

public:
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg LRESULT OnEnterSizeMove (WPARAM, LPARAM);
	afx_msg LRESULT OnExitSizeMove (WPARAM, LPARAM);
	afx_msg void OnUpdateIndicatorTriangles(CCmdUI *pCmdUI);
	afx_msg void OnUpdateIndicatorSnaps(CCmdUI *pCmdUI);
	afx_msg void OnUpdateIndicatorPosition(CCmdUI *pCmdUI);
	afx_msg void OnUpdateIndicatorMemoryLoad(CCmdUI *pCmdUI);
	afx_msg void OnUpdateIndicatorFrameRate(CCmdUI *pCmdUI);
	afx_msg void OnUpdateIndicatorChunks(CCmdUI *pCmdUI);
	virtual void GetMessageString(UINT nID, CString& rMessage) const;
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg void OnPopupInsert();
	afx_msg void OnPopupDelete();
	afx_msg void OnPopupRename();
	afx_msg void OnPopupMoveSelectionHere();
	afx_msg void OnPopupPropertyListAddItem();
	afx_msg void OnPopupPropertyListItemRemoveItem();
	afx_msg void OnClose();
	afx_msg void OnGUIManagerCommand(UINT nID);
	afx_msg void OnMenuSelect( UINT nItemID, UINT nFlags, HMENU hSysMenu );
	afx_msg void OnExitMenuLoop( BOOL bIsTrackPopupMenu );
	afx_msg void OnToolbarHotItemChange( UINT id, NMHDR * pNotifyStruct, LRESULT * result );
	afx_msg void OnSysColorChange();
protected:
	virtual LRESULT DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam);

	bool saveSelectionAsPrefab( GUI::ItemPtr item );

	bool showToolbar( GUI::ItemPtr item );
	bool hideToolbar( GUI::ItemPtr item );
	unsigned int updateToolbar( GUI::ItemPtr item );

	bool showStatusBar( GUI::ItemPtr item );
	bool hideStatusBar( GUI::ItemPtr item );
	unsigned int updateStatusBar( GUI::ItemPtr item );

	bool showPlayerPreview( GUI::ItemPtr item );
	bool hidePlayerPreview( GUI::ItemPtr item );
	unsigned int updatePlayerPreview( GUI::ItemPtr item );
	unsigned int updateToolMode( GUI::ItemPtr item );
};
