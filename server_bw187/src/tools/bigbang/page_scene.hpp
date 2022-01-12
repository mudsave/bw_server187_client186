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
#include "resource.h"

#include "tree_ctrl_ex.hpp"

#include <set>


// PageScene

class EditorGroup;
class ChunkItem;
typedef SmartPointer<ChunkItem>	ChunkItemPtr;


class SceneTreeCtrl : public CTreeCtrlEx
{
	DECLARE_DYNAMIC(SceneTreeCtrl)

protected:
	BOOL PreTranslateMessage(MSG* pMsg);
};

class PageScene : public CPropertyPage
{
	DECLARE_DYNAMIC(PageScene)

public:
	PageScene();
	virtual ~PageScene();

	static PageScene & instance() { return *s_instance_; }

	void rebuildTree();
	void addItemToGroup( ChunkItem* item, EditorGroup* group );
	void removeItemFromGroup( ChunkItem* item, EditorGroup* group );
	void update();

	void OnPopupInsert();
	void OnPopupDelete();
	void OnPopupRename();
	void OnPopupMoveSelectionHere();

	/** Set the selection in the tree view to the given items */
	void updateSelection( const std::vector<ChunkItemPtr>& items );

// Dialog Data
	enum { IDD = IDD_PAGE_SCENE };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	void FinishDragging(BOOL bDraggingImageList);
	void OnEndDrag(UINT nFlags, CPoint point);
	HICON m_hIcon;
	HTREEITEM m_hItemDrag;

	// Generated message map functions
	//{{AFX_MSG(PageScene)
	afx_msg void OnBegindragTree1(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnCancelMode();
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnDeleteitemTree1(NMHDR* pNMHDR, LRESULT* pResult);
	virtual void OnCancel();
	virtual void OnOK();
	afx_msg void OnNMRClickSceneTree(NMHDR *pNMHDR, LRESULT *pResult);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

	void adviseSelectedId( std::string id );

private:
	static PageScene* s_instance_;
	bool active_;

	// Selection is being programatically changed, ignore it
	bool isUpdatingSelection_;

	// Items to be removed from the tree during the next update
	std::map< EditorGroup*, std::set<ChunkItem*> > itemsToRemove_;

	std::vector< std::pair<ChunkItem*, EditorGroup*> > itemsToAdd_;

	/** Add the group, it's items, and all it's child groups under parent */
	void addGroup( HTREEITEM parent, EditorGroup* group );

	/** Find the item for the given chunkItem */
	HTREEITEM findItem( ChunkItem* chunkItem );

	/** Select all chunk items under the given group */
	void selectAllRecursivly( HTREEITEM from );

	/** Makes sure only items, not groups are selected */
	void normaliseSelection();

	/**
	 * Make sure the group, and it's parents exist in the tree.
	 * Doesn't do anything with items.
	 */
	void ensureGroupExists( EditorGroup* group );

public:
	virtual BOOL OnSetActive();
	virtual BOOL OnKillActive();
	virtual BOOL OnInitDialog();

	afx_msg LRESULT OnResizePage(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnUpdateControls(WPARAM wParam, LPARAM lParam);

	SceneTreeCtrl sceneTree_;
	afx_msg void OnTvnBeginlabeleditSceneTree(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnTvnEndlabeleditSceneTree(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnTvnSelchangedSceneTree(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnNMDblclkSceneTree(NMHDR *pNMHDR, LRESULT *pResult);
};
