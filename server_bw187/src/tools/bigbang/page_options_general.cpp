/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// page_options.cpp : implementation file
//

#include "pch.hpp"
#include "bigbang.h"
#include "page_options_general.hpp"
#include "python_adapter.hpp"
#include "user_messages.hpp"
#include "appmgr/options.hpp"
#include "big_bang.hpp"
#include "romp/time_of_day.hpp"
#include "controls/user_messages.hpp"
#include "cstdmf/debug.hpp"
#include "cstdmf/Watcher.hpp"

DECLARE_DEBUG_COMPONENT2( "BigBang", 0 )

// OptionsShowTree

IMPLEMENT_DYNAMIC(OptionsShowTree, CTreeCtrl)
OptionsShowTree::OptionsShowTree()
	: mousePoint_(0, 0)
{
}

OptionsShowTree::~OptionsShowTree()
{
	//Clean up the item data
	for (unsigned i=0; i<itemData_.size(); i++)
	{
		delete itemData_[i];
	}
}

void OptionsShowTree::populate( DataSectionPtr data, HTREEITEM item /* = NULL */  )
{
	if (data == NULL) return;

	int numChildren = data->countChildren();

	for (int i=0; i<numChildren; i++)
	{
		std::string itemName = data->childSectionName(i);
		std::replace( itemName.begin(), itemName.end(), '_' , ' ');

		HTREEITEM newItem = InsertItem( itemName.c_str(), item );
		SetItemState(newItem, TVIS_EXPANDED, TVIS_EXPANDED);

		//We will store the action script name in the item data
		DataSectionPtr newData = data->openChild( i );
		std::string* itemData = new std::string( newData->asString() );
		SetItemData( newItem, (DWORD)itemData );
			
		//Push it onto a vector so we can delete it later
		itemData_.push_back( itemData );

		populate( newData, newItem );
	}
}

BEGIN_MESSAGE_MAP(OptionsShowTree, CTreeCtrl)
	ON_WM_MOUSEMOVE()
	ON_NOTIFY_REFLECT(NM_CLICK, OnNMClick)
	ON_NOTIFY_REFLECT(NM_DBLCLK, OnNMClick)
END_MESSAGE_MAP()


void OptionsShowTree::OnMouseMove(UINT nFlags, CPoint point)
{
	mousePoint_ = point;

	CTreeCtrl::OnMouseMove(nFlags, point);
}

void OptionsShowTree::execute( HTREEITEM item )
{
	std::string* pyCommand = (std::string*)GetItemData( item );

	if ((pyCommand) && (*pyCommand != ""))
	{
		int enabled = 0;
		int checked = 0;

		bool propagateUp =
			((PythonAdapter::instance()->ActionScriptUpdate( *pyCommand, enabled, checked )) &&
			(!checked));

		PythonAdapter::instance()->ActionScriptExecute( *pyCommand );

		if ( propagateUp )
		{
			item = GetParentItem( item );
			while ( item != NULL )
			{
				std::string* pyCommand = (std::string*)GetItemData( item );
				if ((pyCommand) &&
					(*pyCommand != "") &&
					(PythonAdapter::instance()->ActionScriptUpdate( *pyCommand, enabled, checked)) &&
					(!checked))
					{
						// If the parent is not checked and the child is then check it...
						PythonAdapter::instance()->ActionScriptExecute( *pyCommand );
					}
				item = GetParentItem( item );
			}
		}
	}
}

void OptionsShowTree::OnNMClick(NMHDR *pNMHDR, LRESULT *pResult)
{
	CPoint point = mousePoint_;

	UINT uFlags;
	HTREEITEM hItem = HitTest(point, &uFlags);

	if ((hItem == NULL) || !( TVHT_ONITEMSTATEICON & uFlags ))
	{
		return;
	}

	execute( hItem );

	*pResult = 0;
}

void OptionsShowTree::update()
{
	if (!PythonAdapter::instance())
		return;

	HTREEITEM item = GetRootItem();

	while (item != NULL)
	{
		std::string* pyCommand = (std::string*)GetItemData( item );

		int enabled = 0;
		int checked = 0;
		if (PythonAdapter::instance()->ActionScriptUpdate( *pyCommand, enabled, checked ))
		{
			SetCheck(item, checked);
		}

		item = GetNextItem(item, TVGN_NEXTVISIBLE);
	}
}


// PageOptionsGeneral

// GUITABS content ID ( declared by the IMPLEMENT_BASIC_CONTENT macro )
const std::string PageOptionsGeneral::contentID = "PageOptionsGeneral";


PageOptionsGeneral::PageOptionsGeneral()
	: CFormView(PageOptionsGeneral::IDD),
	pageReady_( false ),
	dontUpdateHeightEdit_(false),
	dontUpdateFarClipEdit_( false ),
	cameraHeightEdited_(false),
	farClipEdited_( false ),
	lastLightsType_( -1 )
{
	farPlaneEdit_.SetNumericType(controls::EditNumeric::ENT_INTEGER);
	cameraHeightEdit_.SetNumericType(controls::EditNumeric::ENT_INTEGER);
}

PageOptionsGeneral::~PageOptionsGeneral()
{
}

void PageOptionsGeneral::DoDataExchange(CDataExchange* pDX)
{
	CFormView::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_OPTIONS_SHOW_TREE, showTree_);

	DDX_Control(pDX, IDC_OPTIONS_LIGHTING_STANDARD, standardLighting_);
	DDX_Control(pDX, IDC_OPTIONS_LIGHTING_DYNAMIC, dynamicLighting_);
	DDX_Control(pDX, IDC_OPTIONS_LIGHTING_SPECULAR, specularLighting_);

	DDX_Control(pDX, IDC_OPTIONS_FARPLANE_SLIDER, farPlaneSlider_);
	DDX_Control(pDX, IDC_OPTIONS_FARPLANE_EDIT, farPlaneEdit_);

	DDX_Control(pDX, IDC_OPTIONS_CAMERAHEIGHT_SLIDER, cameraHeightSlider_);
	DDX_Control(pDX, IDC_OPTIONS_CAMERAHEIGHT_EDIT, cameraHeightEdit_);
	DDX_Control(pDX, IDC_OPTIONS_READ_ONLY_MODE, readOnlyMode_);
	DDX_Control(pDX, IDC_PLAYER_PREVIEW_MODE, isPlayerPreviewModeEnabled_);
}


void PageOptionsGeneral::InitPage()
{
	INIT_AUTO_TOOLTIP();
	CString name;
	GetWindowText(name);
	if (PythonAdapter::instance())
		PythonAdapter::instance()->onPageControlTabSelect("pgc", name.GetBuffer());

	std::string optionsPage = Options::getOptionString( "tools/optionsPage", "resources/data/options_page.xml" );
	
	DataSectionPtr optionsData = BWResource::openSection( optionsPage );

	if (optionsData != NULL)
	{
		showTree_.populate( optionsData );
	}
	else
	{
		ERROR_MSG("There was a problem loading \"%s\"", optionsPage.c_str());
	}

	farPlaneSlider_.SetRangeMin(50);
	farPlaneSlider_.SetRangeMax(2500);
	farPlaneSlider_.SetPageSize(0);

	cameraHeightSlider_.SetRangeMin(1);
	cameraHeightSlider_.SetRangeMax(200);
	cameraHeightSlider_.SetPageSize(0);

	// set initial position and override any python feelings
	farPlaneSlider_.SetPos(Options::getOptionInt( "graphics/farclip", 500 ));
	farPlaneEdit_.SetNumericType( controls::EditNumeric::ENT_INTEGER );
	farPlaneEdit_.SetAllowNegative( false );
	farPlaneEdit_.SetMinimum( 50 );
	farPlaneEdit_.SetMaximum( 2500 );

	cameraHeightEdit_.SetNumericType( controls::EditNumeric::ENT_FLOAT );
	cameraHeightEdit_.SetAllowNegative( false );
	cameraHeightEdit_.SetMinimum( 1 );
	cameraHeightEdit_.SetMaximum( 200 );
	cameraHeightEdit_.SetValue( Options::getOptionFloat( "graphics/cameraHeight", 2.f ) );
	cameraHeightSlider_.SetPos( (int)Options::getOptionFloat( "graphics/cameraHeight", 2.f ) );

	MF_ASSERT(PythonAdapter::instance());
	PythonAdapter::instance()->onSliderAdjust("slrFarPlane", 
											(float)farPlaneSlider_.GetPos(), 
											(float)farPlaneSlider_.GetRangeMin(), 
											(float)farPlaneSlider_.GetRangeMax());

//	OnUpdateControls(0, 0);
	updateSliderEdits();
}


BEGIN_MESSAGE_MAP(PageOptionsGeneral, CFormView)
	ON_WM_SHOWWINDOW()
	ON_WM_HSCROLL()
	ON_MESSAGE(WM_UPDATE_CONTROLS, OnUpdateControls)
	ON_BN_CLICKED(IDC_OPTIONS_LIGHTING_STANDARD, OnBnClickedOptionsLightingStandard)
	ON_BN_CLICKED(IDC_OPTIONS_LIGHTING_DYNAMIC, OnBnClickedOptionsLightingDynamic)
	ON_BN_CLICKED(IDC_OPTIONS_LIGHTING_SPECULAR, OnBnClickedOptionsLightingSpecular)
	ON_BN_CLICKED(IDC_OPTIONS_READ_ONLY_MODE, OnBnClickedReadOnlyMode)
	ON_EN_CHANGE(IDC_OPTIONS_CAMERAHEIGHT_EDIT, OnEnChangeOptionsCameraHeightEdit)
	ON_BN_CLICKED(IDC_PLAYER_PREVIEW_MODE, OnBnClickedPlayerPreviewMode)
	ON_EN_CHANGE(IDC_OPTIONS_FARPLANE_EDIT, OnEnChangeOptionsFarplaneEdit)
END_MESSAGE_MAP()


// PageOptionsGeneral message handlers
afx_msg void PageOptionsGeneral::OnShowWindow( BOOL bShow, UINT nStatus )
{
	CFormView::OnShowWindow( bShow, nStatus );

	if ( bShow == FALSE )
	{
	}
	else
	{
		OnUpdateControls( 0, 0 );
	}
}

afx_msg LRESULT PageOptionsGeneral::OnUpdateControls(WPARAM wParam, LPARAM lParam)
{
	if ( !pageReady_ )
	{
		InitPage();
		pageReady_ = true;
	}

	if ( !IsWindowVisible() )
		return 0;

	MF_ASSERT(PythonAdapter::instance());
	showTree_.update();

	bool readOnly = Options::getOptionInt("objects/readOnlyMode", 0) != 0;
	readOnlyMode_.SetCheck(readOnly ? BST_CHECKED : BST_UNCHECKED);

	int enabled = 0;
	int checked = 0;

	//Ensure the correct light preview type is selected
	int lightType = Options::getOptionInt( "render/lighting");
	if ( lightType != lastLightsType_)
	{
		standardLighting_.SetCheck( lightType == 0 ? BST_CHECKED : BST_UNCHECKED );
		dynamicLighting_.SetCheck( lightType == 1 ? BST_CHECKED : BST_UNCHECKED );
		specularLighting_.SetCheck( lightType == 2 ? BST_CHECKED : BST_UNCHECKED );
		lastLightsType_ = lightType;
	}

	isPlayerPreviewModeEnabled_.SetCheck( BigBang::instance().isInPlayerPreviewMode() );

	static float s_lastCameraHeight = -1.f;
	float cameraHeight = Options::getOptionFloat( "graphics/cameraHeight", 2.f );
	if ( cameraHeightEdited_ )
	{
		s_lastCameraHeight = cameraHeight;
		cameraHeightEdited_ = false;
	}
	if (cameraHeight != s_lastCameraHeight)
	{
		cameraHeightSlider_.SetPos( (int)cameraHeight );
		cameraHeightEdit_.SetValue( cameraHeight );
		s_lastCameraHeight = cameraHeight;
	}

	static int s_lastFarClip = -1;
	int farClip = farPlaneSlider_.GetPos();
	if ( farClipEdited_ )
	{
		s_lastFarClip = farClip;
		farClipEdited_ = false;
	}
	if (farClip != s_lastFarClip)
	{
		farPlaneEdit_.SetIntegerValue( farClip );
		s_lastFarClip = farClip;
	}

	return 0;
}

void PageOptionsGeneral::OnBnClickedOptionsLightingStandard()
{
	if (PythonAdapter::instance())
		PythonAdapter::instance()->ActionScriptExecute("actLightingStd");
}

void PageOptionsGeneral::OnBnClickedOptionsLightingDynamic()
{
	if (PythonAdapter::instance())
		PythonAdapter::instance()->ActionScriptExecute("actLightingDynamic");
}

void PageOptionsGeneral::OnBnClickedOptionsLightingSpecular()
{
	if (PythonAdapter::instance())
		PythonAdapter::instance()->ActionScriptExecute("actLightingSpecular");
}

void PageOptionsGeneral::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	// this captures all the scroll events for the page
	// upadate all the slider buddy windows
	updateSliderEdits();

	if (PythonAdapter::instance())
	{
		PythonAdapter::instance()->onSliderAdjust("slrFarPlane", 
												(float)farPlaneSlider_.GetPos(), 
												(float)farPlaneSlider_.GetRangeMin(), 
												(float)farPlaneSlider_.GetRangeMax());

		Options::setOptionInt( "graphics/farclip", farPlaneSlider_.GetPos() );
		Options::setOptionInt( "graphics/cameraHeight", cameraHeightSlider_.GetPos() );

		if( !dontUpdateFarClipEdit_ )
			farPlaneEdit_.SetIntegerValue( farPlaneSlider_.GetPos() );
	}

	CFormView::OnHScroll(nSBCode, nPos, pScrollBar);
}

void PageOptionsGeneral::updateSliderEdits()
{
	farPlaneEdit_.SetIntegerValue(farPlaneSlider_.GetPos());

	if (!dontUpdateHeightEdit_)
	{
		cameraHeightEdit_.SetValue( (float)cameraHeightSlider_.GetPos() );
	}
}

void PageOptionsGeneral::OnBnClickedReadOnlyMode()
{
	bool readOnly = (readOnlyMode_.GetCheck() == BST_CHECKED);
	Options::setOptionInt("objects/readOnlyMode", readOnly);
}

void PageOptionsGeneral::OnEnChangeOptionsCameraHeightEdit()
{
	cameraHeightEdited_ = true;
	
	dontUpdateHeightEdit_ = true;
	cameraHeightSlider_.SetPos( cameraHeightEdit_.GetIntegerValue() );
	dontUpdateHeightEdit_ = false;

	if (PythonAdapter::instance())
	{
		Options::setOptionInt( "graphics/cameraHeight", (int)cameraHeightEdit_.GetValue() );
	}
}

void PageOptionsGeneral::OnBnClickedPlayerPreviewMode()
{
	BigBang::instance().setPlayerPreviewMode( isPlayerPreviewModeEnabled_.GetCheck() == TRUE );
}

void PageOptionsGeneral::OnEnChangeOptionsFarplaneEdit()
{
	farClipEdited_ = true;

	dontUpdateFarClipEdit_ = true;
	farPlaneSlider_.SetPos( farPlaneEdit_.GetIntegerValue() );
	dontUpdateFarClipEdit_ = false;
	PythonAdapter::instance()->onSliderAdjust("slrFarPlane", 
		(float)farPlaneSlider_.GetPos(), 
		(float)farPlaneSlider_.GetRangeMin(), 
		(float)farPlaneSlider_.GetRangeMax());
	Options::setOptionInt( "graphics/farclip", farPlaneSlider_.GetPos() );
	Options::setOptionInt( "graphics/cameraHeight", cameraHeightSlider_.GetPos() );
}
