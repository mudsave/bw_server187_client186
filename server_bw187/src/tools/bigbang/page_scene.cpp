/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// page_errors.cpp : implementation file
//

#include "pch.hpp"
#include "bigbang.h"
#include "page_scene.hpp"
#include "python_adapter.hpp"
#include "user_messages.hpp"
#include "big_bang.hpp"
#include "editor_group.hpp"
#include "selection_filter.hpp"

#include "chunk/chunk.hpp"
#include "chunk/chunk_item.hpp"

#include "big_bang_camera.hpp"

/**
 *	32 bit wide value that can hold either a ChunkItem* or an EditorGroup*
 *	
 *	We take advantage of ia32 pointers being 4 byte aligned, and thus hijack
 *	the least significant bit to indicate which type of pointer we have.
 *
 *	This is used to store the pointer for each item in the scene tree. It's
 *	eaiser than using the heaps and having to free memory etc.
 */
class ItemData
{
public:
	ItemData( DWORD_PTR item ) : ptr_( (uint32) item ) {}
	ItemData( ChunkItem* item ) : ptr_( (uint32) item )
	{
		MF_ASSERT( isChunkItem() );
	}
	ItemData( EditorGroup* group ) : ptr_( (uint32) group | 1 )
	{
		MF_ASSERT( isGroup() );
	}

	bool isChunkItem() { return (ptr_ & 1) == 0; }
	bool isGroup() { return !isChunkItem(); }

	operator DWORD_PTR() { return (DWORD_PTR) ptr_; }

	operator ChunkItem*() { return isChunkItem() ? (ChunkItem*) ptr_ : 0; }
	operator EditorGroup*() { return isGroup() ? (EditorGroup*) (ptr_ & ~1) : 0; }

private:
	uint32 ptr_;
};



IMPLEMENT_DYNAMIC(SceneTreeCtrl, CTreeCtrlEx)

// This is needed to enter and esc work when editing the label on an item
BOOL SceneTreeCtrl::PreTranslateMessage(MSG* pMsg)
{
	if( pMsg->message == WM_KEYDOWN )
	{
		// When an item is being edited make sure the edit control
		// receives certain important key strokes
		if (GetEditControl() 
			&& (pMsg->wParam == VK_RETURN 
					|| pMsg->wParam == VK_DELETE 
					|| pMsg->wParam == VK_ESCAPE
					|| GetKeyState( VK_CONTROL) 
					)
				)
		{
			::TranslateMessage(pMsg);
			::DispatchMessage(pMsg);
			return TRUE;                            // Don't process further
		}
	}

	return CTreeCtrlEx::PreTranslateMessage(pMsg);
}

void PageScene::OnBegindragTree1(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LPNMTREEVIEW lpnmtv = (LPNMTREEVIEW)pNMHDR;
	*pResult = 0;	// allow drag

	CImageList* piml = NULL;		/* handle of image list */
	POINT ptOffset;
	RECT rcItem;
	
	if ((piml = sceneTree_.CreateDragImage(lpnmtv->itemNew.hItem)) == NULL)
		return;

	/* get the bounding rectangle of the item being dragged (rel to top-left of control) */
	if (sceneTree_.GetItemRect(lpnmtv->itemNew.hItem, &rcItem, TRUE))
	{
		CPoint ptDragBegin;
		int nX, nY;
		/* get offset into image that the mouse is at */
		/* item rect doesn't include the image */
		ptDragBegin = lpnmtv->ptDrag;
		ImageList_GetIconSize(piml->GetSafeHandle(), &nX, &nY);
		ptOffset.x = (ptDragBegin.x - rcItem.left) + (nX - (rcItem.right - rcItem.left));
		ptOffset.y = (ptDragBegin.y - rcItem.top) + (nY - (rcItem.bottom - rcItem.top));
		/* convert the item rect to screen co-ords, for use later */
		MapWindowPoints(NULL, &rcItem);
	}
	else
	{
		GetWindowRect(&rcItem);
		ptOffset.x = ptOffset.y = 8;
	}

	BOOL bDragBegun = piml->BeginDrag(0, ptOffset);
	if (! bDragBegun)
	{
		delete piml;
		return;
	}

	CPoint ptDragEnter = lpnmtv->ptDrag;
	ClientToScreen(&ptDragEnter);
	if (!piml->DragEnter(NULL, ptDragEnter))
	{
		delete piml;
		return;
	}

	delete piml;

	/* set the focus here, so we get a WM_CANCELMODE if needed */
	SetFocus();

	/* redraw item being dragged, otherwise it remains (looking) selected */
	InvalidateRect(&rcItem, TRUE);
	UpdateWindow();

	/* Hide the mouse cursor, and direct mouse input to this window */
	SetCapture(); 
	m_hItemDrag = lpnmtv->itemNew.hItem;
}

void PageScene::OnMouseMove(UINT nFlags, CPoint point) 
{
	if (m_hItemDrag != NULL)
	{
		CPoint pt;

		/* drag the item to the current position */
		pt = point;
		ClientToScreen(&pt);

		CImageList::DragMove(pt);
		CImageList::DragShowNolock(FALSE);

		if (CWnd::WindowFromPoint(pt) != &sceneTree_)
			SetCursor(AfxGetApp()->LoadStandardCursor(IDC_NO));
		else
		{
			SetCursor(AfxGetApp()->LoadStandardCursor(IDC_ARROW));
			TVHITTESTINFO tvhti;
			tvhti.pt = pt;
			sceneTree_.ScreenToClient(&tvhti.pt);
			HTREEITEM hItemSel = sceneTree_.HitTest(&tvhti);
			sceneTree_.SelectDropTarget(tvhti.hItem);
		}

		CImageList::DragShowNolock(TRUE);
	}

	CDialog::OnMouseMove(nFlags, point);
}

void PageScene::OnCancelMode() 
{
	/* stop dragging */
	FinishDragging(TRUE);
	// redraw
	RedrawWindow();

	CDialog::OnCancelMode();
}

void PageScene::OnLButtonUp(UINT nFlags, CPoint point)
{
 	if (m_hItemDrag != NULL)
		OnEndDrag(nFlags, point);
	else
		CDialog::OnLButtonUp(nFlags, point);
}

// copy our lParam, which happens to be a CString pointer
static LPARAM CopyData(const CTreeCtrl& tree, HTREEITEM hItem, LPARAM lParam)
{
	return lParam;
/*
	if (lParam == 0)
		return 0;
	CString* ps = (CString*)lParam;
	CString* psNew = new CString(*ps);
	return (LPARAM)psNew;
*/
}

void PageScene::OnEndDrag(UINT nFlags, CPoint point)
{
	if (m_hItemDrag == NULL)
		return;

	CPoint pt;

	pt = point;
	ClientToScreen(&pt);

	// do drop
	HTREEITEM hItemDrop = sceneTree_.GetDropHilightItem();
	if (hItemDrop == NULL)
		hItemDrop = (HTREEITEM) EditorGroup::rootGroup()->treeItemHandle();

	// only drop on groups
	if (ItemData( sceneTree_.GetItemData( hItemDrop ) ).isChunkItem())
	{
		FinishDragging(TRUE);
		RedrawWindow();
		return;
	}

	EditorGroup* destGroup = ItemData( sceneTree_.GetItemData( hItemDrop ) );

	sceneTree_.SelectDropTarget( NULL );

	std::vector<ChunkItemPtr> items;

	HTREEITEM cur = sceneTree_.GetFirstSelectedItem();
	while (cur)
	{
		ChunkItem* chunkItem = ItemData( sceneTree_.GetItemData( cur ) );

		if (chunkItem)
		{
			chunkItem->edGroup( destGroup );
			items.push_back( chunkItem );
		}

		cur = sceneTree_.GetNextSelectedItem( cur );
	}

	FinishDragging( TRUE );

	updateSelection( items );
}

void PageScene::FinishDragging(BOOL bDraggingImageList)
{
	if (m_hItemDrag != NULL)
	{
		if (bDraggingImageList)
		{
			CImageList::DragLeave(NULL);
			CImageList::EndDrag();
		}

		ReleaseCapture();
		ShowCursor(TRUE);
		m_hItemDrag = NULL;
		sceneTree_.SelectDropTarget(NULL);
	}
}

void PageScene::OnCancel() 
{
	if (m_hItemDrag == NULL)
		CDialog::OnCancel();
	else	// cancel drag operation
		SendMessage(WM_CANCELMODE);
}

void PageScene::OnOK() 
{
	// do nothing
}





void PageScene::OnNMRClickSceneTree(NMHDR *pNMHDR, LRESULT *pResult)
{
	CMenu menu;
	menu.LoadMenu( IDR_SCENE_POPUP );
	CMenu* pPopup = menu.GetSubMenu(0);

	POINT pt;
	::GetCursorPos( &pt );

	POINT lpt = pt;
	::ScreenToClient(sceneTree_.m_hWnd, &lpt);

	// Make sure the item under the cursor is now selected
	UINT uFlags;
	HTREEITEM hItem = sceneTree_.HitTest( lpt, &uFlags );

	if ((hItem != NULL) && (TVHT_ONITEM & uFlags))
	{
		if (ItemData( sceneTree_.GetItemData( hItem ) ).isGroup())
		{
			isUpdatingSelection_ = true;
			sceneTree_.ClearSelection();
			sceneTree_.SelectItem( NULL );
			sceneTree_.Select( hItem, TVGN_CARET );
			isUpdatingSelection_ = false;
		}
		else
		{
			//sceneTree_.Select( hItem, TVGN_CARET );
			//sceneTree_.SelectItemEx( hItem, TRUE );
		}
	}



	/*
	HTREEITEM selected = sceneTree_.GetSelectedItem();

	if (selected == NULL)
		return;

	ItemData id = sceneTree_.GetItemData( selected );
	*/

	if (hItem == NULL)
		return;

	ItemData id = sceneTree_.GetItemData( hItem );

	// Can only delete groups
	if (id.isChunkItem())
	{
		pPopup->EnableMenuItem( ID_SCENE_POPUP_DELETE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED );
	}
	else
	{
		pPopup->EnableMenuItem( ID_SCENE_POPUP_DELETE, MF_BYCOMMAND | MF_ENABLED );
	}

	pPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, AfxGetMainWnd() );

	*pResult = 0;
}

void PageScene::OnPopupInsert()
{
	HTREEITEM selected = sceneTree_.GetSelectedItem();

	EditorGroup* parentGroup = NULL;

	if (selected != NULL)
	{
		ItemData id = sceneTree_.GetItemData( selected );
		
		if (id.isGroup())
			parentGroup = id;
		else
			parentGroup = ((ChunkItem*) (id))->edGroup();
	}

	if (!parentGroup)
		parentGroup = EditorGroup::rootGroup();

	EditorGroup* g = parentGroup->findOrCreateChild( "New Group" );
	if (g->treeItemHandle() == 0)
	{
		addGroup( (HTREEITEM) parentGroup->treeItemHandle(), g );
		sceneTree_.Invalidate( FALSE );
	}

	// Move the selected items to the new group
	/*
	HTREEITEM cur = sceneTree_.GetFirstSelectedItem();
	while (cur)
	{
		ChunkItem* chunkItem = ItemData( sceneTree_.GetItemData( cur ) );

		if (chunkItem)
			chunkItem->edGroup( g );

		cur = sceneTree_.GetNextSelectedItem( cur );
	}
	*/

	const std::vector<ChunkItemPtr>& selectedItems = BigBang::instance().selectedItems();
	std::vector<ChunkItemPtr>::const_iterator i = selectedItems.begin();
	for (; i != selectedItems.end(); ++i)
		(*i)->edGroup( g );

	update();


	HTREEITEM newItem = (HTREEITEM) g->treeItemHandle(); 

	sceneTree_.ClearSelection();
	sceneTree_.SelectItem( newItem );
	sceneTree_.Expand( newItem, TVE_EXPAND );
	sceneTree_.EditLabel( newItem );
}

void PageScene::OnPopupDelete()
{
	HTREEITEM selected = sceneTree_.GetSelectedItem();

	if (selected == NULL)
		return;

	ItemData id = sceneTree_.GetItemData( selected );

	if (id.isChunkItem())
		return;

	EditorGroup* group = id;

	if (group == EditorGroup::rootGroup())
		return;

	group->parent()->removeChildGroup( group );
	sceneTree_.DeleteItem( selected );
}

void PageScene::OnPopupRename()
{
	HTREEITEM selected = sceneTree_.GetSelectedItem();

	if (selected == NULL)
		return;

	ItemData id = sceneTree_.GetItemData( selected );

	EditorGroup* group = id;
	if (!group)
		group = ((ChunkItem*) id)->edGroup();

	if (group == EditorGroup::rootGroup())
		return;

	sceneTree_.EditLabel( (HTREEITEM) group->treeItemHandle() );
}

void PageScene::OnPopupMoveSelectionHere()
{
	HTREEITEM selected = sceneTree_.GetSelectedItem();

	EditorGroup* parentGroup = NULL;

	if (selected != NULL)
	{
		ItemData id = sceneTree_.GetItemData( selected );
		
		if (id.isGroup())
			parentGroup = id;
		else
			parentGroup = ((ChunkItem*) (id))->edGroup();
	}

	if (!parentGroup)
		parentGroup = EditorGroup::rootGroup();


	const std::vector<ChunkItemPtr>& selectedItems = BigBang::instance().selectedItems();
	std::vector<ChunkItemPtr>::const_iterator i = selectedItems.begin();
	for (; i != selectedItems.end(); ++i)
		(*i)->edGroup( parentGroup );

	update();
}













// PageScene
PageScene * PageScene::s_instance_ = NULL;

IMPLEMENT_DYNAMIC(PageScene, CPropertyPage)
PageScene::PageScene()
	: CPropertyPage(PageScene::IDD)
	, active_( false )
	, isUpdatingSelection_( false )
{
	ASSERT(s_instance_ == NULL);
	s_instance_ = this;
}

PageScene::~PageScene()
{
	ASSERT(s_instance_);
	s_instance_ = NULL;
}

void PageScene::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_SCENE_TREE, sceneTree_);
}


BOOL PageScene::OnSetActive()
{
	active_ = true;
	OnResizePage(0, 0);

	if (CPropertyPage::OnSetActive())
	{
		rebuildTree();
		return true;
	}
	else
	{
		return false;
	}
}

BOOL PageScene::OnKillActive()
{
	active_ = false;

	return CPropertyPage::OnKillActive();
}


BEGIN_MESSAGE_MAP(PageScene, CPropertyPage)
	//{{AFX_MSG_MAP(PageScene)
	ON_MESSAGE(WM_RESIZEPAGE, OnResizePage)
	ON_MESSAGE(WM_UPDATE_CONTROLS, OnUpdateControls)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_NOTIFY(TVN_BEGINDRAG, IDC_SCENE_TREE, OnBegindragTree1)
	ON_WM_MOUSEMOVE()
	ON_WM_CANCELMODE()
	ON_WM_LBUTTONUP()
	ON_NOTIFY(NM_RCLICK, IDC_SCENE_TREE, OnNMRClickSceneTree)
	//}}AFX_MSG_MAP
	ON_NOTIFY(TVN_BEGINLABELEDIT, IDC_SCENE_TREE, OnTvnBeginlabeleditSceneTree)
	ON_NOTIFY(TVN_ENDLABELEDIT, IDC_SCENE_TREE, OnTvnEndlabeleditSceneTree)
	ON_NOTIFY(TVN_SELCHANGED, IDC_SCENE_TREE, OnTvnSelchangedSceneTree)
	ON_NOTIFY(NM_DBLCLK, IDC_SCENE_TREE, OnNMDblclkSceneTree)
END_MESSAGE_MAP()


// PageScene message handlers

BOOL PageScene::OnInitDialog()
{
	CPropertyPage::OnInitDialog();


	CImageList iml;
	iml.Create(16, 16, ILC_COLOR24 | ILC_MASK, 2, 2);

	CBitmap bm;
	bm.LoadBitmap(IDR_SCENE_ICONS);
	iml.Add(&bm, RGB(255, 0, 0));

	sceneTree_.SetImageList(&iml, TVSIL_NORMAL);
	iml.Detach();


	SetRedraw();

	return TRUE;	// return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

afx_msg LRESULT PageScene::OnResizePage(WPARAM wParam, LPARAM lParam)
{
	// resize the property pane to correspond with the size of the wnd
	CWnd* propSheetWnd = GetDlgItem(IDC_SCENE_TREE);
	if (!propSheetWnd)
		return 0;

	CRect rectPage;
	GetWindowRect(rectPage);

	// TODO: work out why these negative numbers are needed
	propSheetWnd->SetWindowPos(NULL, 
		5, 5,
		rectPage.Width() - 10, rectPage.Height() - 10,
		SWP_NOZORDER | SWP_NOACTIVATE);

	return 0;
}

afx_msg LRESULT PageScene::OnUpdateControls(WPARAM wParam, LPARAM lParam)
{
	return 0;
}

void PageScene::addGroup( HTREEITEM parentItem, EditorGroup* group )
{
	std::string caption = group->name();
	if (caption.empty())
		caption = "Root";

	HTREEITEM parent = sceneTree_.InsertItem( caption.c_str(), 1, 1, parentItem );
	sceneTree_.SetItemData( parent, ItemData( group ) );

	// Notify the group of its HTREEITEM
	group->treeItemHandle( (uint32) parent );

	std::vector<EditorGroup*>::const_iterator i = group->children().begin();

	for (; i != group->children().end(); ++i)
	{
		EditorGroup* g = *i;

		addGroup( parent, *i );
	}

	// Add all the items
	std::vector<ChunkItem*>::const_iterator j = group->items().begin();
	for (; j != group->items().end(); ++j)
	{
		ChunkItem* chunkItem = *j;

		HTREEITEM item = sceneTree_.InsertItem( chunkItem->edDescription().c_str(), 0, 0, parent );
		sceneTree_.SetItemData( item, ItemData( chunkItem ) );

		if (BigBang::instance().isItemSelected( chunkItem ))
		{
			isUpdatingSelection_ = true;
			sceneTree_.SelectItemEx( item, TRUE );
			isUpdatingSelection_ = false;
		}
	}
}

void PageScene::rebuildTree()
{
	if (!active_)
		return;

	isUpdatingSelection_ = true;

	// Delete all the items in the tree
	sceneTree_.SetRedraw(FALSE);

	sceneTree_.DeleteAllItems();


	addGroup( TVI_ROOT, EditorGroup::rootGroup() );
	sceneTree_.Expand( (HTREEITEM) EditorGroup::rootGroup()->treeItemHandle(), TVE_EXPAND );

	sceneTree_.SetRedraw(TRUE);

	isUpdatingSelection_ = false;
}

void PageScene::addItemToGroup( ChunkItem* chunkItem, EditorGroup* group )
{
	if (!active_)
		return;

	// Item has been scheduled to remove, just cancel that
	if (itemsToRemove_[group].count( chunkItem ))
	{
		itemsToRemove_[group].erase( chunkItem );
		return;
	}

	itemsToAdd_.push_back( std::make_pair( chunkItem, group ) );
}

void PageScene::removeItemFromGroup( ChunkItem* chunkItem, EditorGroup* group )
{
	if (!active_)
		return;

	// Schedule the item to be removed on the next update
	itemsToRemove_[group].insert( chunkItem );
}

void PageScene::ensureGroupExists( EditorGroup* group )
{
	if (group->treeItemHandle())
		return;

	if (!group->parent())
		return;

	ensureGroupExists( group->parent() );

	std::string caption = group->name();

	HTREEITEM groupItem = sceneTree_.InsertItem( caption.c_str(), 1, 1,
		(HTREEITEM) group->parent()->treeItemHandle() );
	sceneTree_.SetItemData( groupItem, ItemData( group ) );

	// Notify the group of its HTREEITEM
	group->treeItemHandle( (uint32) groupItem );
}

void PageScene::update()
{
	// Check explicitly, so SetRedraw doesn't make everything flash
	if (itemsToRemove_.empty() && itemsToAdd_.empty())
		return;

	sceneTree_.SetRedraw( FALSE );

	// Remove all scheduled items from the tree
	std::map< EditorGroup*, std::set<ChunkItem*> >::iterator it = itemsToRemove_.begin();
	for (; it != itemsToRemove_.end(); ++it)
	{
		EditorGroup* group = it->first;
		std::set<ChunkItem*>& chunkItems = it->second;

		HTREEITEM i = sceneTree_.GetChildItem( (HTREEITEM) group->treeItemHandle() );
		HTREEITEM tmp = i;
		while (tmp != NULL)
		{
			i = tmp;
			tmp = sceneTree_.GetNextItem(i, TVGN_NEXT);
		}
		while (i != NULL)
		{
			tmp = sceneTree_.GetNextItem(i, TVGN_PREVIOUS);

			ChunkItem* chunkItem = ItemData( sceneTree_.GetItemData( i ) );

			if (chunkItems.count( chunkItem ))
				sceneTree_.DeleteItem(i);

			i = tmp;
		}
	}
	itemsToRemove_.clear();

	// Add all scheduled items to the tree
	std::vector< std::pair<ChunkItem*, EditorGroup*> >::iterator it2 = itemsToAdd_.begin();	
	for (; it2 != itemsToAdd_.end(); ++it2)
	{
		ChunkItem* chunkItem = it2->first;
		EditorGroup* group = it2->second;

		ensureGroupExists( group );

		HTREEITEM item = sceneTree_.InsertItem( chunkItem->edDescription().c_str(), 0, 0, (HTREEITEM) group->treeItemHandle() );
		sceneTree_.SetItemData( item, ItemData( chunkItem ) );

		if (BigBang::instance().isItemSelected( chunkItem ))
		{
			isUpdatingSelection_ = true;
			sceneTree_.SelectItemEx( item, TRUE );
			isUpdatingSelection_ = false;
		}
	}
	itemsToAdd_.clear();

	sceneTree_.SetRedraw( TRUE );
}



void PageScene::OnTvnBeginlabeleditSceneTree(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMTVDISPINFO pTVDispInfo = reinterpret_cast<LPNMTVDISPINFO>(pNMHDR);

	HTREEITEM selected = pTVDispInfo->item.hItem;
	if (selected == NULL
		|| ItemData( sceneTree_.GetItemData( selected ) ).isChunkItem()
		|| ((EditorGroup*) ItemData( sceneTree_.GetItemData( selected ) )) == EditorGroup::rootGroup())
	{
		*pResult = 1;
	}
	else
	{
		*pResult = 0;
	}
}

void PageScene::OnTvnEndlabeleditSceneTree(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMTVDISPINFO pTVDispInfo = reinterpret_cast<LPNMTVDISPINFO>(pNMHDR);

	char* newName = pTVDispInfo->item.pszText;

	// Don't go ahead if null or empty
	if (newName == 0 || *newName == '\0')
		return;

	// Check for illegal characters
	for (char* c = newName; *c != '\0'; ++c)
		if (*c == '/' || *c < 32 || *c == '<' || *c == '>')
			return;

	HTREEITEM foo = pTVDispInfo->item.hItem;

	// ...now update the group's name
	EditorGroup* group = ItemData( sceneTree_.GetItemData( pTVDispInfo->item.hItem ) );

	if (!group)
		return;

	group->name( newName );
	sceneTree_.SetItemText( pTVDispInfo->item.hItem, newName );


	*pResult = 0;
}

HTREEITEM PageScene::findItem( ChunkItem* chunkItem )
{
	EditorGroup* group = chunkItem->edGroup();

	if (!group)
		return 0;

	HTREEITEM i = sceneTree_.GetChildItem( (HTREEITEM) group->treeItemHandle() );
	HTREEITEM tmp = i;
	while (tmp != NULL)
	{
		i = tmp;
		tmp = sceneTree_.GetNextItem( i, TVGN_NEXT );
	}
	while (i != NULL)
	{
		tmp = sceneTree_.GetNextItem( i, TVGN_PREVIOUS );

		if (sceneTree_.GetItemData( i ) == (DWORD) chunkItem)
			return i;
		i = tmp;
	}

	return 0;
}

void PageScene::updateSelection( const std::vector<ChunkItemPtr>& items )
{
	if (!active_)
		return;

	if (isUpdatingSelection_)
		return;

	isUpdatingSelection_ = true;

	sceneTree_.SetRedraw( FALSE );

	sceneTree_.ClearSelection();
	sceneTree_.SelectItem( NULL );

	std::vector<ChunkItemPtr>::const_iterator i = items.begin();
	for (; i != items.end(); ++i)
	{
		ChunkItem* chunkItem = &*(*i);
		EditorGroup* group = chunkItem->edGroup();

		HTREEITEM item = findItem( chunkItem );
		if (item == 0)
			continue;

		sceneTree_.SelectItemEx( item, TRUE );
	}

	sceneTree_.EnsureVisible( sceneTree_.GetFirstSelectedItem() );

	sceneTree_.SetRedraw( TRUE );

	isUpdatingSelection_ = false;
}

void PageScene::OnTvnSelchangedSceneTree(NMHDR *pNMHDR, LRESULT *pResult)
{
	if (isUpdatingSelection_)
		return;

	isUpdatingSelection_ = true;

	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);

	std::vector<ChunkItemPtr> chunkItems;

	HTREEITEM cur = sceneTree_.GetFirstSelectedItem();
	while (cur)
	{
		ChunkItem* chunkItem = ItemData( sceneTree_.GetItemData( cur ) );

		if (chunkItem)
			chunkItems.push_back( chunkItem );

		cur = sceneTree_.GetNextSelectedItem( cur );
	}

	if (!chunkItems.empty())
		BigBang::instance().setSelection( chunkItems );

	isUpdatingSelection_ = false;

	//normaliseSelection();

	*pResult = 0;
}

void PageScene::selectAllRecursivly( HTREEITEM from )
{
	HTREEITEM i = sceneTree_.GetChildItem( from );

	while (i)
	{
		if (ItemData( sceneTree_.GetItemData( i ) ).isChunkItem())
		{
			if (SelectionFilter::canSelect( ItemData( sceneTree_.GetItemData( i ) ) ))
				sceneTree_.SelectItemEx( i, TRUE );
		}
		else
		{
			selectAllRecursivly( i );
		}

		i = sceneTree_.GetNextItem( i, TVGN_NEXT );
	}
}

void PageScene::normaliseSelection()
{
	isUpdatingSelection_ = true;

	sceneTree_.SetRedraw( FALSE );

	// Make sure only items, and not groups are selected
	HTREEITEM cur = sceneTree_.GetFirstSelectedItem();
	while (cur)
	{
		ItemData itemData = ItemData( sceneTree_.GetItemData( cur ) );

		if (itemData.isGroup())
		{
			// De-select the group
			sceneTree_.SelectItemEx( cur, FALSE );

			// Select everything under the group, unless we're root
			if (itemData != EditorGroup::rootGroup())
				selectAllRecursivly( cur );
		}

		cur = sceneTree_.GetNextSelectedItem( cur );
	}

	sceneTree_.SetRedraw( TRUE );


	// Inform BB of the now selected items
	std::vector<ChunkItemPtr> chunkItems;

	cur = sceneTree_.GetFirstSelectedItem();
	while (cur)
	{
		ChunkItem* chunkItem = ItemData( sceneTree_.GetItemData( cur ) );

		if (chunkItem)
			chunkItems.push_back( chunkItem );

		cur = sceneTree_.GetNextSelectedItem( cur );
	}

	BigBang::instance().setSelection( chunkItems );


	isUpdatingSelection_ = false;
}

void PageScene::OnNMDblclkSceneTree(NMHDR *pNMHDR, LRESULT *pResult)
{
	HTREEITEM cur = sceneTree_.GetFirstSelectedItem();

	normaliseSelection();

	if (cur)
	{
		ItemData itemData = ItemData( sceneTree_.GetItemData( cur ) );

		if (itemData.isGroup())
			// As this gets called before the double click expands the group
			// as it normally would, we have to ensure the group isn't expand,
			// or the double click will collapse it for us
			sceneTree_.Expand( cur, TVE_COLLAPSE );
	}

	*pResult = 0;
}
