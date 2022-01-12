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

#include "shlwapi.h"

#include "user_messages.hpp"
#include "python_adapter.hpp"

#include "main_frm.h"
#include "me_shell.hpp"
#include "me_app.hpp"
#include "main_frm.h"

#include "guimanager/gui_manager.hpp"
#include "guimanager/gui_menu.hpp"
#include "guimanager/gui_toolbar.hpp"
#include "guimanager/gui_functor.hpp"
#include "guimanager/gui_functor_option.hpp"

#include "ual/ual_drop_manager.hpp"

#include "mru.hpp"
#include "undo_redo.hpp"
#include "utilities.hpp"
#include "delay_redraw.hpp"

#include "controls/user_messages.hpp"
#include "controls/edit_commit.hpp"
#include "controls/edit_numeric.hpp"
#include "controls/slider.hpp"

#include "page_animations_impl.hpp"

#include "page_animations.hpp"

DECLARE_DEBUG_COMPONENT( 0 )

class CMultiFileDialog : public CFileDialog
{
    static const size_t BUFFER_SIZE_CHARS = 32 * 1024;

    TCHAR *m_pBuffer;
    TCHAR *m_pOldBuffer;

public:
    CMultiFileDialog( LPCTSTR lpszFilter = NULL )
        : CFileDialog( TRUE, "", "", OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT, lpszFilter )
    {
        m_pOldBuffer = GetOFN().lpstrFile;

        GetOFN().nMaxFile = BUFFER_SIZE_CHARS;
        m_pBuffer = (TCHAR *)malloc(BUFFER_SIZE_CHARS * sizeof(TCHAR));
        memset(m_pBuffer, 0, BUFFER_SIZE_CHARS * sizeof(TCHAR));
        GetOFN().lpstrFile = m_pBuffer;
    }

    ~CMultiFileDialog()
    {
        GetOFN().lpstrFile = m_pOldBuffer;
        free(m_pBuffer);
    }
};

PageAnimations* PageAnimationsImpl::s_currPage = NULL;

// PageAnimations

//ID string required for the tearoff tab manager
const std::string PageAnimations::contentID = "PageAnimationsID";

IMPLEMENT_DYNCREATE(PageAnimations, TreeList)

PageAnimations::PageAnimations():
	TreeList( PageAnimations::IDD, MeApp::instance().mutant()->animTree(), "animations" )
	
{
	pImpl_ = new PageAnimationsImpl;
	
	pImpl_->ready = false;
	pImpl_->inited = false;
	pImpl_->updating = false;

	pImpl_->updateCount = -1;

	pImpl_->lastItem = NULL;

	pImpl_->wasPlaying = false;

	pImpl_->lastLockedParents = -1;

	pImpl_->modelName = "";
	pImpl_->fileName = "";
	pImpl_->lastAnim = "";
	
	pImpl_->nodeName = "";

	pImpl_->lastFrameNum = 0;
	
	pImpl_->first.autoSelect( true );
	pImpl_->last.autoSelect( true );

	pImpl_->frameRate.SetNumericType( controls::EditNumeric::ENT_INTEGER );
	pImpl_->frameRate.SetMinimum( 1 );
	pImpl_->frameRate.SetMaximum( 60 );
	pImpl_->frameRate.SetAllowNegative( false );

	pImpl_->frameNum.SetNumericType( controls::EditNumeric::ENT_INTEGER );
	pImpl_->frameNum.SetMinimum( 1 );
	pImpl_->frameNum.SetMaximum( 100 );
	pImpl_->frameNum.SetAllowNegative( false );

	pImpl_->blend.SetNumericType( controls::EditNumeric::ENT_FLOAT );
	pImpl_->blend.SetMinimum( 0.f );
	pImpl_->blend.SetMaximum( 999.f );
	pImpl_->blend.SetAllowNegative( false );

	pImpl_->compChanged = false;
	pImpl_->animChanged = false;

	pImpl_->s_currPage = this;
}

PageAnimations::~PageAnimations()
{
}

/*static*/ PageAnimations* PageAnimations::currPage()
{
	return PageAnimationsImpl::s_currPage;
}

void PageAnimations::DoDataExchange(CDataExchange* pDX)
{	
	TreeList::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_ANIM_NAME, pImpl_->name);
	DDX_Control(pDX, IDC_ANIM_SOURCE, pImpl_->source);
	DDX_Control(pDX, IDC_ANIM_CHANGE_ANIM, pImpl_->change_anim);
	DDX_Control(pDX, IDC_ANIM_FRAME_RATE, pImpl_->frameRate);
	DDX_Control(pDX, IDC_ANIM_FRAME_RATE_SLIDER, pImpl_->frameRateSlider);
	DDX_Control(pDX, IDC_ANIM_FIRST, pImpl_->first);
	DDX_Control(pDX, IDC_ANIM_LAST, pImpl_->last);
	DDX_Control(pDX, IDC_ANIM_FRAME, pImpl_->frameNum);
	DDX_Control(pDX, IDC_ANIM_FRAME_SLIDER, pImpl_->frameNumSlider);
	DDX_Control(pDX, IDC_ANIM_SAVE_FRAME_RATE, pImpl_->frameRateSave);

	DDX_Control(pDX, IDC_ANIM_NODES_BOX, pImpl_->nodeBox);
	DDX_Control(pDX, IDC_ANIM_NODES, pImpl_->nodeTree);
	DDX_Control(pDX, IDC_ANIM_BLEND, pImpl_->blend);
	DDX_Control(pDX, IDC_ANIM_BLEND_SLIDER, pImpl_->blendSlider);
	DDX_Control(pDX, IDC_ANIM_REMOVE_BLEND, pImpl_->blendRemove);

	DDX_Control(pDX, IDC_ANIM_COMP_BOX, pImpl_->compBox);
		
	DDX_Control(pDX, IDC_ANIM_COMP_POS_SLDR, pImpl_->compPosSldr);
	DDX_Control(pDX, IDC_ANIM_COMP_ROT_SLDR, pImpl_->compRotSldr);
	DDX_Control(pDX, IDC_ANIM_COMP_SCALE_SLDR, pImpl_->compScaleSldr);

	DDX_Control(pDX, IDC_ANIM_COMP_POS_MINUS, pImpl_->compPosMinus);
	DDX_Control(pDX, IDC_ANIM_COMP_ROT_MINUS, pImpl_->compRotMinus);
	DDX_Control(pDX, IDC_ANIM_COMP_SCALE_MINUS, pImpl_->compScaleMinus);

	DDX_Control(pDX, IDC_ANIM_COMP_POS_PLUS, pImpl_->compPosPlus);
	DDX_Control(pDX, IDC_ANIM_COMP_ROT_PLUS, pImpl_->compRotPlus);
	DDX_Control(pDX, IDC_ANIM_COMP_SCALE_PLUS, pImpl_->compScalePlus);

	DDX_Control(pDX, IDC_ANIM_COMP_POS, pImpl_->compPos);
	DDX_Control(pDX, IDC_ANIM_COMP_ROT, pImpl_->compRot);
	DDX_Control(pDX, IDC_ANIM_COMP_SCALE, pImpl_->compScale);

	DDX_Control(pDX, IDC_ANIM_COMP_TOTAL, pImpl_->compTotal);

	if (pImpl_->ready) return;

	pImpl_->frameRateSlider.SetRangeMin(1); 
	pImpl_->frameRateSlider.SetRangeMax(60);

	pImpl_->frameNumSlider.SetRangeMin(1); 
	pImpl_->frameNumSlider.SetRangeMax(100);

	pImpl_->blendSlider.SetRangeMin(0); 
	pImpl_->blendSlider.SetRangeMax(20);

	pImpl_->compPosSldr.SetRangeMin(0); 
	pImpl_->compPosSldr.SetRangeMax(400);

	pImpl_->compRotSldr.SetRangeMin(0); 
	pImpl_->compRotSldr.SetRangeMax(400);

	pImpl_->compScaleSldr.SetRangeMin(0); 
	pImpl_->compScaleSldr.SetRangeMax(400);

	//------
	
	pImpl_->toolbar.Create( CCS_NODIVIDER | CCS_NORESIZE | CCS_NOPARENTALIGN |
		TBSTYLE_FLAT | WS_CHILD | WS_VISIBLE | TBSTYLE_TOOLTIPS | CBRS_TOOLTIPS,
		CRect(0,0,0,0), this, 0 );

	GUI::Manager::instance()->add( new GUI::Toolbar( "AnimToolbar", pImpl_->toolbar ) );

	CWnd toolbarPos;
	DDX_Control(pDX, IDC_ANIM_TOOLBAR, toolbarPos);
	
	CRect toolbarRect;
    toolbarPos.GetWindowRect (&toolbarRect);
    ScreenToClient (&toolbarRect);

	pImpl_->toolbar.MoveWindow(toolbarRect);

	//------
	
	pImpl_->compToolbar.Create( CCS_NODIVIDER | CCS_NORESIZE | CCS_NOPARENTALIGN |
		TBSTYLE_FLAT | WS_CHILD | WS_VISIBLE | TBSTYLE_TOOLTIPS | CBRS_TOOLTIPS,
		CRect(0,0,0,0), this, 0 );

	GUI::Manager::instance()->add( new GUI::Toolbar( "AnimCompToolbar", pImpl_->compToolbar ) );

	CWnd compToolbarPos;
	DDX_Control(pDX, IDC_ANIM_COMP_TOOLBAR, compToolbarPos);
	
	CRect compToolbarRect;
    compToolbarPos.GetWindowRect (&compToolbarRect);
    ScreenToClient (&compToolbarRect);

	pImpl_->compToolbar.MoveWindow(compToolbarRect);

	//------

	pImpl_->ready = true;
}

BOOL PageAnimations::OnInitDialog()
{
	MeApp::instance().mutant()->playAnim( true );
	MeApp::instance().mutant()->loopAnim( true );

	//Add some drop acceptance functors
	UalDropManager::instance().add(
		new UalDropFunctor< PageAnimations >( &tree(), "animation", this, &PageAnimations::addAnims ) );

	pImpl_->compPosMinus.setBitmapID( IDB_MINUS, IDB_MINUS, RGB(255, 0, 255) );
	pImpl_->compRotMinus.setBitmapID( IDB_MINUS, IDB_MINUS, RGB(255, 0, 255) );
	pImpl_->compScaleMinus.setBitmapID( IDB_MINUS, IDB_MINUS, RGB(255, 0, 255) );

	pImpl_->compPosPlus.setBitmapID( IDB_PLUS, IDB_PLUS, RGB(255, 0, 255) );
	pImpl_->compRotPlus.setBitmapID( IDB_PLUS, IDB_PLUS, RGB(255, 0, 255) );
	pImpl_->compScalePlus.setBitmapID( IDB_PLUS, IDB_PLUS, RGB(255, 0, 255) );
		
	INIT_AUTO_TOOLTIP();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

BEGIN_MESSAGE_MAP(PageAnimations, TreeList)

	ON_WM_SIZE()
	
	ON_MESSAGE(WM_UPDATE_CONTROLS, OnUpdateControls)

	ON_WM_HSCROLL()
	
	ON_EN_CHANGE(IDC_ANIM_NAME, OnEnChangeAnimName)

	ON_BN_CLICKED(IDC_ANIM_CHANGE_ANIM, OnChangeAnim)

	ON_EN_KILLFOCUS(IDC_ANIM_NAME, updateName)
	ON_EN_KILLFOCUS(IDC_ANIM_FRAME_RATE, updateFrameRate)
	ON_EN_KILLFOCUS(IDC_ANIM_FIRST, updateFirst)
	ON_EN_KILLFOCUS(IDC_ANIM_LAST, updateLast)
	ON_EN_KILLFOCUS(IDC_ANIM_FRAME, updateFrameNum)

	ON_NOTIFY(TVN_SELCHANGED, IDC_ANIM_NODES, OnTvnSelChangedAnimNodes)
	ON_BN_CLICKED(IDC_ANIM_REMOVE_BLEND, OnBnClickedAnimRemoveBlend)
	
	ON_EN_KILLFOCUS(IDC_ANIM_BLEND, updateBlend)

	ON_EN_SETFOCUS(IDC_ANIM_FRAME, animStop)
	ON_BN_CLICKED(IDC_ANIM_SAVE_FRAME_RATE, OnBnClickedAnimSaveFrameRate)

	ON_COMMAND_RANGE(GUI_COMMAND_START, GUI_COMMAND_END, OnGUIManagerCommand)
	ON_UPDATE_COMMAND_UI_RANGE(GUI_COMMAND_START, GUI_COMMAND_END, OnGUIManagerCommandUpdate)
	
	ON_BN_CLICKED(IDC_ANIM_COMP_POS_MINUS, OnBnClickedAnimCompPosMinus)
	ON_BN_CLICKED(IDC_ANIM_COMP_POS_PLUS, OnBnClickedAnimCompPosPlus)
	ON_BN_CLICKED(IDC_ANIM_COMP_ROT_MINUS, OnBnClickedAnimCompRotMinus)
	ON_BN_CLICKED(IDC_ANIM_COMP_ROT_PLUS, OnBnClickedAnimCompRotPlus)
	ON_BN_CLICKED(IDC_ANIM_COMP_SCALE_MINUS, OnBnClickedAnimCompScaleMinus)
	ON_BN_CLICKED(IDC_ANIM_COMP_SCALE_PLUS, OnBnClickedAnimCompScalePlus)

	ON_MESSAGE(WM_SHOW_TOOLTIP, OnShowTooltip)
	ON_MESSAGE(WM_HIDE_TOOLTIP, OnHideTooltip)
END_MESSAGE_MAP()

// PageAnimations message handlers

void PageAnimations::OnGUIManagerCommand(UINT nID)
{
	pImpl_->s_currPage = this;
	GUI::Manager::instance()->act( nID );
}

void PageAnimations::OnGUIManagerCommandUpdate(CCmdUI * cmdUI)
{
	pImpl_->s_currPage = this;
	if( !cmdUI->m_pMenu )                                                   
		GUI::Manager::instance()->update( cmdUI->m_nID );
}

afx_msg LRESULT PageAnimations::OnShowTooltip(WPARAM wParam, LPARAM lParam)
{
	LPSTR* msg = (LPSTR*)wParam;
	CMainFrame::instance().SetMessageText( *msg );
	return 0;
}

afx_msg LRESULT PageAnimations::OnHideTooltip(WPARAM wParam, LPARAM lParam)
{
	CMainFrame::instance().SetMessageText( "" );
	return 0;
}

int PageAnimations::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	//We might use this later...
	return 1;
}

//A macro for laying out an animation compression slider set
#define SIZE_ANIM_SLIDER( name ) \
	Utilities::stretchToRight( this, pImpl_->comp##name##Sldr, cx, 100 );\
	Utilities::moveToRight( this, pImpl_->comp##name##Minus, cx, 74 );\
	Utilities::moveToRight( this, pImpl_->comp##name##Plus, cx, 48 );\
	Utilities::moveToRight( this, pImpl_->comp##name, cx, 12 );\
	pImpl_->comp##name##Minus.RedrawWindow();\
	pImpl_->comp##name##Plus.RedrawWindow();\
	pImpl_->comp##name.RedrawWindow();

void PageAnimations::OnSize(UINT nType, int cx, int cy)
{
	if (!pImpl_->ready) return;
	
	Utilities::stretchToRight( this, pImpl_->name, cx, 12 );
	Utilities::stretchToRight( this, pImpl_->source, cx, 40 );
	Utilities::moveToRight( this, pImpl_->change_anim, cx, 12 );
	Utilities::stretchToRight( this, pImpl_->frameNumSlider, cx, 12 );
	Utilities::stretchToRight( this, pImpl_->frameRateSlider, cx, 12 );

	Utilities::stretchToRight( this, pImpl_->nodeBox, cx, 0 );
	Utilities::stretchToRight( this, pImpl_->nodeTree, cx, 12 );
	Utilities::stretchToRight( this, pImpl_->blendSlider, cx, 12 );
	
	Utilities::stretchToRight( this, pImpl_->compBox, cx, 0 );
	SIZE_ANIM_SLIDER( Pos );
	SIZE_ANIM_SLIDER( Rot );
	SIZE_ANIM_SLIDER( Scale );

	PathSetDlgItemPath( this->GetSafeHwnd(), IDC_ANIM_SOURCE, pImpl_->fileName.c_str() );

	TreeList::OnSize( nType, cx, cy );
}

void PageAnimations::updateCheck( CButton& button, const std::string& actionName )
{
	int enabled = 0;
	int checked = 0;
	PythonAdapter::instance()->ActionScriptUpdate( actionName, enabled, checked );
	button.SetCheck( checked ? BST_CHECKED : BST_UNCHECKED );
}

unsigned PageAnimations::addNodeTree( DataSectionPtr root, HTREEITEM parent, float blend, unsigned nodeCount )
{
	std::vector<DataSectionPtr> nodeSections;
    root->openSections( "node", nodeSections );
    std::vector<DataSectionPtr>::iterator it = nodeSections.begin();
    std::vector<DataSectionPtr>::iterator end = nodeSections.end();

    char buf[256];
	
	while( it != end )
    {
        nodeCount++;
		
		DataSectionPtr ds = *it++;

		std::string boneName = ds->readString( "identifier" );

		bool changed = true;
		
		float weight = MeApp::instance().mutant()->animBoneWeight( selID(), boneName );
		if (weight == -1.0)
		{
			weight = blend;
			changed = false;
		}

		sprintf( buf, " %s (%g)", boneName.c_str(), weight );
		HTREEITEM child = pImpl_->nodeTree.InsertItem( buf, parent );
		pImpl_->nodeTree.SetItemState( child,
			TVIS_EXPANDED | (changed ? TVIS_BOLD : 0),
			TVIS_EXPANDED | (changed ? TVIS_BOLD : 0) );

		//Select the node if it was previously selected
		if (boneName == pImpl_->nodeName)
		{
			pImpl_->nodeItem = child;
			pImpl_->nodeTree.SelectItem( pImpl_->nodeItem );
		}

		nodeCount = addNodeTree( ds, child, weight, nodeCount );
	}

	return nodeCount;

}

void PageAnimations::disableField( CEdit& field )
{
	field.SetWindowText( "" );
	field.SetReadOnly( true );
	field.ModifyStyle( 0, WS_DISABLED );
}

#define compSliderState( name, state ) \
	pImpl_->comp##name##Sldr.ModifyStyle( state ? WS_DISABLED : 0,\
		state ? 0 : WS_DISABLED );\
	pImpl_->comp##name##Minus.ModifyStyle( state ? WS_DISABLED : 0,\
		state ? 0 : WS_DISABLED );\
	pImpl_->comp##name##Plus.ModifyStyle( state ? WS_DISABLED : 0,\
		state ? 0 : WS_DISABLED );\
	if (!state)\
	{\
		pImpl_->comp##name##Sldr.SetPos( 0 );\
		pImpl_->comp##name##.SetWindowText( "" );\
	}

afx_msg LRESULT PageAnimations::OnUpdateControls(WPARAM wParam, LPARAM lParam)
{
	pImpl_->updating = pImpl_->updateCount != MeApp::instance().mutant()->updateCount("Anim");
	pImpl_->updateCount = MeApp::instance().mutant()->updateCount("Anim");

	if (!pImpl_->inited)
	{
		OnInitDialog();
		
		pImpl_->inited = true;
	}
	
	if (MeApp::instance().mutant())
	{
		bool playing = MeApp::instance().mutant()->playAnim();
		if (playing != pImpl_->wasPlaying)	
		{
			GUI::Manager::instance()->update(); // Update the animation toolbar
			pImpl_->wasPlaying = playing;
		}
		
		//Has the model changed?
		std::string modelName = MeApp::instance().mutant()->modelName();
		if (pImpl_->updating || (modelName != pImpl_->modelName))
		{
			OnUpdateTreeList();
			pImpl_->lastAnim == ""; // Reset the starting animation folder

			if ((modelName == "") || (!MeApp::instance().mutant()->canAnim( selID().second )))
			{
				pImpl_->toolbar.ModifyStyle( 0, WS_DISABLED );
			}
			else
			{
				pImpl_->toolbar.ModifyStyle( WS_DISABLED, 0 );
			}
			GUI::Manager::instance()->update();
			pImpl_->toolbar.RedrawWindow();

			// Ensure that the lock state will be updated
			int lockLod = Options::getOptionInt( "settings/lockLodParents", 0 );
			pImpl_->lastLockedParents = !lockLod;

			pImpl_->modelName = modelName;
		}
	}

	// Check if the current frame number has changed
	int animFrameNum = MeApp::instance().mutant()->frameNum( selID() );
	if (pImpl_->updating || (animFrameNum != pImpl_->lastFrameNum))
	{
		pImpl_->frameNum.SetIntegerValue( animFrameNum );
		pImpl_->frameNumSlider.SetPos( animFrameNum );
		pImpl_->lastFrameNum = animFrameNum;
	}

	// Ensure that the parent lock is enabled
	int lockLod = Options::getOptionInt( "settings/lockLodParents", 0 );
	if ( lockLod != pImpl_->lastLockedParents )
	{
		locked( !!lockLod );
		pImpl_->lastLockedParents = lockLod;
	}

	// Check whether either the animation (or model) selection has changed
	if (pImpl_->updating || (pImpl_->lastItem != selItem()))
	{
		StringPair animId = selID();

		bool canAnim = MeApp::instance().mutant()->canAnim( animId.second );

		bool animSlidersEnabled = false;

		if ((tree().GetParentItem(selItem())) && (MeApp::instance().mutant()->hasAnims( animId.second )))
		{
			bool lockedParents = locked();
			
			char buf[8];
				
			const std::string& animName = MeApp::instance().mutant()->animName( animId );
			pImpl_->name.SetWindowText( animName.c_str() );

			pImpl_->name.SetReadOnly( lockedParents );
			pImpl_->name.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0);

			pImpl_->fileName = MeApp::instance().mutant()->animFile( selID() );

			pImpl_->change_anim.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0);

			Moo::AnimationPtr anim = MeApp::instance().mutant()->getMooAnim( animId );
			
			if (anim)
			{
				bool foundInterpolated = false;
				
				for (uint i = 0; i < anim->nChannelBinders(); i++)
				{
					Moo::AnimationChannelPtr channel = anim->channelBinder( i ).channel();

					MF_ASSERT( channel );

					if (channel->type() == 1 || channel->type() == 4)
					{
						Moo::InterpolatedAnimationChannel* iac =
							static_cast<Moo::InterpolatedAnimationChannel*>(&*channel);

						setSlidersCompressionErrorFactors(
							iac->scaleCompressionError(),
							iac->rotationCompressionError(),
							iac->positionCompressionError());

						foundInterpolated = true;
					}
				}

				if (foundInterpolated)
				{
					// Make sure to update the compression settings
					animSlidersEnabled = true;
					pImpl_->compChanged = true; 
					pImpl_->animChanged = true; 
				}
			}

			float animFrameRate = MeApp::instance().mutant()->localFrameRate( animId );

			MeApp::instance().mutant()->lastFrameRate( animFrameRate );
			pImpl_->frameRate.SetValue( animFrameRate );
			pImpl_->frameRate.SetReadOnly( false );
			pImpl_->frameRate.ModifyStyle(WS_DISABLED, 0);

			pImpl_->frameRateSlider.SetPos( (int)(animFrameRate + 0.5f) );
			pImpl_->frameRateSlider.ModifyStyle(WS_DISABLED, 0);

			int animFirst = MeApp::instance().mutant()->firstFrame( animId );
			if (animFirst != -1)
			{
				sprintf( buf, "%d", animFirst );
				pImpl_->first.SetWindowText( buf );
			}
			else
			{
				pImpl_->first.SetWindowText("");
			}
			pImpl_->first.SetReadOnly( lockedParents );
			pImpl_->first.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0);

			int animLast = MeApp::instance().mutant()->lastFrame( animId );
			if (animLast != -1)
			{
				sprintf( buf, "%d", animLast );
				pImpl_->last.SetWindowText( buf );
			}
			else
			{
				pImpl_->last.SetWindowText("");
			}
			pImpl_->last.SetReadOnly( lockedParents );
			pImpl_->last.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0);

			int animFrameNum = MeApp::instance().mutant()->frameNum( animId );
			int animNumFrames = MeApp::instance().mutant()->numFrames( animId );
			pImpl_->frameNum.SetMaximum( (float)animNumFrames );
			pImpl_->frameNum.SetIntegerValue( animFrameNum );
			pImpl_->frameNum.SetReadOnly( false );
			pImpl_->frameNum.ModifyStyle(WS_DISABLED, 0);
			pImpl_->frameNumSlider.SetRangeMax( animNumFrames );
			pImpl_->frameNumSlider.SetPos( animFrameNum );
			pImpl_->frameNumSlider.ModifyStyle(WS_DISABLED, 0);

			pImpl_->frameRateSave.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0);

			pImpl_->blend.SetReadOnly( lockedParents );
			pImpl_->blend.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0);
			pImpl_->blendSlider.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0);

			pImpl_->blendRemove.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0);

			MeApp::instance().mutant()->setAnim( (size_t)this, animId );

			MeApp::instance().mutant()->playAnim( true );
		}
		else // We are a model...
		{
			HTREEITEM model = tree().GetParentItem( selItem() );
			if (!model) model = selItem();

			disableField( pImpl_->name);
			char buf[256];
			sprintf( buf, "%s (model)", tree().GetItemText(model) );
			pImpl_->name.SetWindowText( buf );
				
			pImpl_->fileName = animId.second;

			pImpl_->change_anim.ModifyStyle(0, WS_DISABLED);

			disableField( pImpl_->first );

			disableField( pImpl_->last );

			disableField( pImpl_->frameNum );
			pImpl_->frameNumSlider.ModifyStyle(0, WS_DISABLED);

			disableField( pImpl_->frameRate );
			pImpl_->frameRateSlider.ModifyStyle(0, WS_DISABLED);

			pImpl_->frameRateSave.ModifyStyle(0, WS_DISABLED);

			disableField( pImpl_->blend );
			pImpl_->blendSlider.ModifyStyle(0, WS_DISABLED);

			pImpl_->blendRemove.ModifyStyle(0, WS_DISABLED);

			MeApp::instance().mutant()->stopAnim( (size_t)this );
		}

		//Enable or disable the compression sliders
		if (!animSlidersEnabled)
		{
			pImpl_->compTotal.SetWindowText( "" );
		}
		
		compSliderState( Pos, animSlidersEnabled );
		compSliderState( Rot, animSlidersEnabled );
		compSliderState( Scale, animSlidersEnabled );

		PathSetDlgItemPath( this->GetSafeHwnd(), IDC_ANIM_SOURCE, pImpl_->fileName.c_str() );
			
		{
			DelayRedraw temp( &(pImpl_->nodeTree) );

			pImpl_->nodeTree.DeleteAllItems();
		
			if (MeApp::instance().mutant()->visual())
			{
				unsigned numNodes =
					addNodeTree( MeApp::instance().mutant()->visual()->openSection("node") );

				char buf[256];
				sprintf( buf, "%d nodes", numNodes );
				CMainFrame::instance().setStatusText( ID_INDICATOR_NODES, buf );
			}
		}

		if (!canAnim)
		{
			pImpl_->toolbar.ModifyStyle( 0, WS_DISABLED );
		}
		else
		{
			pImpl_->toolbar.ModifyStyle( WS_DISABLED, 0 );
		}

		GUI::Manager::instance()->update();
		this->RedrawWindow();
		
		pImpl_->lastItem = selItem();
	}
	
	//Now catch any updates from pressing enter in a field or moving a slider
	if (pImpl_->name.doUpdate())
	{
		updateName();
	}
	if (pImpl_->first.doUpdate())
	{
		updateFirst();
	}
	if (pImpl_->last.doUpdate())
	{
		updateLast();
	}
	if ((pImpl_->frameRate.doUpdate()) || (pImpl_->frameRateSlider.doUpdate()))
	{
		updateFrameRate();
	}
	if ((pImpl_->frameNum.doUpdate()) || (pImpl_->frameNumSlider.doUpdate()))
	{
		updateFrameNum();
	}
	if ((pImpl_->blend.doUpdate()) || (pImpl_->blendSlider.doUpdate()))
	{
		updateBlend();
	}
	if (pImpl_->compChanged)
	{
		// Update the compression
		updateCompression();

		// Reload the model
		if ( !pImpl_->animChanged )
			MeApp::instance().mutant()->reloadModel();

		// Clear the compression changed flag
		pImpl_->compChanged = false;

		// Clear the animChanged flag
		pImpl_->animChanged = false;
	}
	
	return 0;
}

void PageAnimations::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	//Do nothing if we are currrently updating
	if (pImpl_->updating) return;
	
	HWND target = pScrollBar->GetSafeHwnd();
	
	int pos, min, max;
	
	if (PythonAdapter::instance())
	{
		// We will (probably) do stuff here...
	}
	
	// Frame Rate Slider
	
	if (target == pImpl_->frameRateSlider.GetSafeHwnd())
	{
		float frameRate = (float)pImpl_->frameRateSlider.GetPos();
		pImpl_->frameRate.SetValue( frameRate );
		MeApp::instance().mutant()->localFrameRate( selID(), frameRate  );
	}

	// Frame Num Slider
	
	if (target == pImpl_->frameNumSlider.GetSafeHwnd())
	{
		animStop(); // Stop the animation
		int frameNum = pImpl_->frameNumSlider.GetPos();
		pImpl_->frameNum.SetIntegerValue( frameNum );
		MeApp::instance().mutant()->frameNum( selID(), frameNum  );
	}

	// Node Blend Slider
	
	if (target == pImpl_->blendSlider.GetSafeHwnd())
	{
		pos = pImpl_->blendSlider.GetPos(); 
		min = pImpl_->blendSlider.GetRangeMin(); 
		max = pImpl_->blendSlider.GetRangeMax();

		float blendVal = (float)(1.f * (pos-min) / (max-min)); // Scale 0.0 -> 1.0

		pImpl_->blend.SetValue( blendVal );
	}

	// Compression Sliders

	if ((target == pImpl_->compPosSldr.GetSafeHwnd()) ||
		(target == pImpl_->compRotSldr.GetSafeHwnd()) ||
		(target == pImpl_->compScaleSldr.GetSafeHwnd()))
	{
		pImpl_->compChanged = true; // We will update the compression next frame
	}

	TreeList::OnHScroll(nSBCode, nPos, pScrollBar);
}

//-------

void PageAnimations::animAdd()
{
	static char BASED_CODE szFilter[] =	"Animation (*.animation)|*.animation||";
	CMultiFileDialog fileDlg ( szFilter );

	if (pImpl_->lastAnim == "")
	{
		MRU::instance().getDir( "models", pImpl_->lastAnim );
	}

	fileDlg.m_ofn.lpstrInitialDir = pImpl_->lastAnim.c_str();

	if ( fileDlg.DoModal() == IDOK )
	{
		CWaitCursor wait;
	
		POSITION pos ( fileDlg.GetStartPosition() );

		while( pos )
		{
			std::string animPath = BWResource::dissolveFilename( std::string( fileDlg.GetNextPathName( pos )));

			std::string::size_type first = animPath.rfind("/") + 1;
			std::string::size_type last = animPath.rfind(".");
			std::string animName = animPath.substr( first, last-first );
			animPath = animPath.substr( 0, last );

			StringPair animID ( animName , selID().second );
			
			selID() = MeApp::instance().mutant()->createAnim( animID, animPath );

			pImpl_->lastAnim = animPath;
		}

		OnUpdateTreeList();
	}
}
static PyObject * py_addAnim( PyObject * args )
{
	if (PageAnimations::currPage())
		PageAnimations::currPage()->animAdd();

	Py_Return;
}
PY_MODULE_FUNCTION( addAnim, ModelEditor )

//-------

bool PageAnimations::isLockedAnim()
{
	return locked();
}
static PyObject * py_isAnimLocked( PyObject * args )
{
	if (PageAnimations::currPage())
		return PyInt_FromLong( PageAnimations::currPage()->isLockedAnim() );
	return PyInt_FromLong( 0 );
}
PY_MODULE_FUNCTION( isAnimLocked, ModelEditor )

//-------

void PageAnimations::animPlay()
{
	MeApp::instance().mutant()->playAnim( true );
}
static PyObject * py_playAnim( PyObject * args )
{
	if (PageAnimations::currPage())
		PageAnimations::currPage()->animPlay();

	Py_Return;
}
PY_MODULE_FUNCTION( playAnim, ModelEditor )

//-------

void PageAnimations::animStop()
{
	MeApp::instance().mutant()->playAnim( false );
}
static PyObject * py_stopAnim( PyObject * args )
{
	if (PageAnimations::currPage())
		PageAnimations::currPage()->animStop();

	Py_Return;
}
PY_MODULE_FUNCTION( stopAnim, ModelEditor )

//-------

void PageAnimations::animLoop()
{
	MeApp::instance().mutant()->loopAnim( ! MeApp::instance().mutant()->loopAnim() );
}
static PyObject * py_loopAnim( PyObject * args )
{
	if (PageAnimations::currPage())
		PageAnimations::currPage()->animLoop();

	Py_Return;
}
PY_MODULE_FUNCTION( loopAnim, ModelEditor )

//-------

void PageAnimations::animRemove()
{
	MeApp::instance().mutant()->removeAnim( selID() );

	OnUpdateTreeList();
}
static PyObject * py_removeAnim( PyObject * args )
{
	if (PageAnimations::currPage())
		PageAnimations::currPage()->animRemove();

	Py_Return;
}
PY_MODULE_FUNCTION( removeAnim, ModelEditor )

//-------

bool PageAnimations::animPlaying()
{
	return MeApp::instance().mutant()->playAnim();
}
static PyObject * py_animPlaying( PyObject * args )
{
	if (PageAnimations::currPage())
		return PyInt_FromLong( PageAnimations::currPage()->animPlaying() );
	return PyInt_FromLong( 0 );
}
PY_MODULE_FUNCTION( animPlaying, ModelEditor )

//-------

bool PageAnimations::animLooping()
{
	return MeApp::instance().mutant()->loopAnim();
}
static PyObject * py_animLooping( PyObject * args )
{
	if (PageAnimations::currPage())
		return PyInt_FromLong( PageAnimations::currPage()->animLooping() );
	return PyInt_FromLong( 0 );
}
PY_MODULE_FUNCTION( animLooping, ModelEditor )

//-------

void PageAnimations::OnEnChangeAnimName()
{
	//Do nothing if there is no parent item (i.e. we are a model, not an animation)
	//This is not necessary since the field should be readonly (but lets be safe)
	if (tree().GetParentItem(selItem()) == NULL)
		return;
	
	CString new_name;
	pImpl_->name.GetWindowText(new_name);
	tree().SetItemText( selItem(), new_name );
	
}

void PageAnimations::updateName()
{
	//Do nothing if we are currrently updating
	if (pImpl_->updating) return;
	
	//Do nothing if there is no parent item (i.e. we are a model, not an animation)
	if (tree().GetParentItem(selItem()) == NULL)
		return;
	
	CString new_name_cstr;
	pImpl_->name.GetWindowText(new_name_cstr);
	std::string new_name = std::string( new_name_cstr );

	std::string::size_type first = new_name.find_first_not_of(" ");
	std::string::size_type last = new_name.find_last_not_of(" ") + 1;
	if (first != std::string::npos)
	{
		new_name = new_name.substr( first, last-first );
	}
	else
	{
		pImpl_->name.SetWindowText( selID().first.c_str() );
		tree().SetItemText( selItem(), selID().first.c_str() );
		
		::MessageBox( AfxGetApp()->m_pMainWnd->GetSafeHwnd(),
			"Unable to rename the animation since no name was given.\nAt least one alphanumeric character is needed.",
			"Invalid animation name given", MB_OK | MB_ICONERROR );

		return;
	}
	
	if (MeApp::instance().mutant()->animName( selID(), new_name ))
	{
		pImpl_->name.SetWindowText( new_name.c_str() );
		tree().SetItemText( selItem(), new_name.c_str() );
		pImpl_->name.SetSel( last-first, last-first, 0 );
		
		selID().first = new_name;
	}
	else
	{
		pImpl_->name.SetWindowText( selID().first.c_str() );
		tree().SetItemText( selItem(), selID().first.c_str() );
		
		::MessageBox( AfxGetApp()->m_pMainWnd->GetSafeHwnd(),
			"Unable to rename the animation since there is already an animation with the same name.",
			"Animation name already used", MB_OK | MB_ICONERROR );
	}
}

void PageAnimations::OnChangeAnim()
{
	static char BASED_CODE szFilter[] =	"Animation (*.animation)|*.animation||";
	CFileDialog fileDlg ( TRUE, "", "", OFN_FILEMUSTEXIST, szFilter );

	if (pImpl_->lastAnim == "")
	{
		MRU::instance().getDir( "models", pImpl_->lastAnim );
	}

	fileDlg.m_ofn.lpstrInitialDir = pImpl_->lastAnim.c_str();

	if ( fileDlg.DoModal() == IDOK )
	{
		CWaitCursor wait;

		std::string animPath = BWResource::removeExtension(
			BWResource::dissolveFilename( std::string( fileDlg.GetPathName() )));
				
		MeApp::instance().mutant()->changeAnim( selID(), animPath );

		pImpl_->lastAnim = animPath;

		OnUpdateTreeList();
	}
}

void PageAnimations::updateFrameRate()
{
	//Do nothing if we are currrently updating
	if (pImpl_->updating) return;

	float rate = pImpl_->frameRate.GetValue();

	pImpl_->frameRateSlider.SetPos( (int)rate );
	
	MeApp::instance().mutant()->localFrameRate( selID(), rate, true );
}

void PageAnimations::updateFirst()
{
	//Do nothing if we are currrently updating
	if (pImpl_->updating) return;
	
	//First lets get the value entered
	CString frame;
	pImpl_->first.GetWindowText( frame );

	int frameNum = -1;
	if (strcmp(frame,"")) // If something was entered
	{
		sscanf( frame, "%d", &frameNum );
	}
	MeApp::instance().mutant()->firstFrame( selID(), frameNum );

	int animNumFrames = MeApp::instance().mutant()->numFrames( selID() );
	pImpl_->frameNumSlider.SetRangeMax(animNumFrames);
}

void PageAnimations::updateLast()
{
	//Do nothing if we are currrently updating
	if (pImpl_->updating) return;
	
	//First lets get the value entered
	CString frame;
	pImpl_->last.GetWindowText( frame );

	int frameNum = -1;
	if (strcmp(frame,"")) // If something was entered
	{
		sscanf( frame, "%d", &frameNum );
	}
	MeApp::instance().mutant()->lastFrame( selID(), frameNum );

	int animNumFrames = MeApp::instance().mutant()->numFrames( selID() );
	pImpl_->frameNumSlider.SetRangeMax(animNumFrames);
}

void PageAnimations::updateFrameNum()
{
	int frame = pImpl_->frameNum.GetIntegerValue();

	pImpl_->frameNumSlider.SetPos( frame );

	MeApp::instance().mutant()->frameNum( selID(), frame );
}

void PageAnimations::OnTvnSelChangedAnimNodes(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);

	//Save the node item index
	pImpl_->nodeItem = pImpl_->nodeTree.GetSelectedItem();

	CString itemText = pImpl_->nodeTree.GetItemText( pImpl_->nodeItem );

	//Convert the CString to a char*
	int size = itemText.GetLength() + 1;
	char* buf = new char[ size ];
	memcpy(buf, itemText, size);

	//Remove the leading space
	char* text = buf + 1;
	
	text = strtok( text, "(");

	//Remove the trailing space
	text[strlen(text) - 1] = 0; 

	//Save the node name
	pImpl_->nodeName = std::string( text );

	text = strtok( NULL, ")");

	float blendVal = 1.0;
	
	sscanf( text, "%f", &blendVal );

	//Set the edit box
	pImpl_->blend.SetValue( blendVal );

	//Set the slider position
	int min = pImpl_->blendSlider.GetRangeMin(); 
	int max = pImpl_->blendSlider.GetRangeMax();
	int pos = (int)(min + blendVal*(max-min));
	pImpl_->blendSlider.SetPos( pos ); 

	delete buf;

	*pResult = 0;
}

void PageAnimations::OnBnClickedAnimRemoveBlend()
{
	if ((selID().first != "") && (selID().second != "") && (pImpl_->nodeName != ""))
	{
		DelayRedraw temp( &(pImpl_->nodeTree) );
		
		MeApp::instance().mutant()->removeAnimNode( selID(), pImpl_->nodeName );

		pImpl_->nodeTree.DeleteAllItems();
			
		addNodeTree( MeApp::instance().mutant()->visual()->openSection("node") );
	}
}

void PageAnimations::updateBlend()
{
	//First lets get the value entered
	CString blend;
	pImpl_->blend.GetWindowText( blend );

	float blendVal = -1;
	if (strcmp( blend, "" )) // If something was entered
	{
		DelayRedraw temp( &(pImpl_->nodeTree) );
		
		sscanf( blend, "%f", &blendVal );
		MeApp::instance().mutant()->animBoneWeight( selID(), pImpl_->nodeName, blendVal );

		pImpl_->nodeTree.DeleteAllItems();
			
		addNodeTree( MeApp::instance().mutant()->visual()->openSection("node") );
	}
}

void PageAnimations::OnBnClickedAnimSaveFrameRate()
{
	MeApp::instance().mutant()->saveFrameRate( selID() );
}

bool PageAnimations::addAnims( UalItemInfo* ii )
{
	if (locked())
	{
		::MessageBox( this->GetSafeHwnd(),
			"Unable to add the animation(s) since the selected model is locked.\n\n"
			"Either select the top-level model or unlock the parent models using the Preferences dialog.",
			"Model locked", MB_OK | MB_ICONERROR );
		return false;
	}
	
	CWaitCursor wait;
	
	while( ii )
	{
		std::string animPath = BWResource::dissolveFilename( ii->longText() );

		std::string::size_type first = animPath.rfind("/") + 1;
		std::string::size_type last = animPath.rfind(".");
		std::string animName = animPath.substr( first, last-first );
		animPath = animPath.substr( 0, last );

		StringPair animID ( animName , selID().second );
		
		selID() = MeApp::instance().mutant()->createAnim( animID, animPath );

		ii = ii->getNext();
	}
	OnUpdateTreeList();
	return true;
}
