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

#include "user_messages.hpp"
#include "python_adapter.hpp"

#include "main_frm.h"
#include "me_shell.hpp"
#include "me_app.hpp"
#include "main_frm.h"

#include "utilities.hpp"

#include "moo/graphics_settings.hpp"

#include "romp/time_of_day.hpp"

#include "appmgr/options.hpp"

#include "guimanager/gui_manager.hpp"

#include "common/string_utils.hpp"
#include "ual/ual_drop_manager.hpp"
#include "controls/user_messages.hpp"

#include "controls/slider.hpp"

#include "page_display.hpp"

DECLARE_DEBUG_COMPONENT( 0 )

// PageDisplay

struct PageDisplayImpl: public SafeReferenceCount
{
	bool inited;
	bool ready;
	
	int last_shadow;
	int last_bkg;
	int last_flora;
	std::string last_time;

	Moo::GraphicsSetting::GraphicsSettingPtr floraSettings;
	
	CWnd generalBox;
	CButton showAxes;
	CButton checkForSparkles;
	CButton showBloom;
	CButton showShimmer;

	CWnd modelBox;
	CButton showModel;
	CButton showWireframe;
	CButton showSkeleton;
	CComboBox shadowing;
	CButton showBsp;
	CButton showBoundingBox;
	CButton showPortals;

	CButton groundModel;
	CButton centreModel;

	CWnd bkgBox;
	CComboBox bkg;
	CComboBox flora;
	CButton bkgColour;
	CButton floorTexture;

	CWnd timeOfDayBox;
	controls::Slider timeOfDaySlider;
	CEdit timeOfDayEdit;

	unsigned numFloraOptions;

};

//ID string required for the tearoff tab manager
const std::string PageDisplay::contentID = "PageDisplayID";

IMPLEMENT_DYNCREATE(PageDisplay, CFormView)

PageDisplay::PageDisplay()
	: CFormView(PageDisplay::IDD)
{
	pImpl_ = new PageDisplayImpl;

	pImpl_->inited = false;
	pImpl_->ready = false;

	pImpl_->last_shadow = -1;
	pImpl_->last_bkg = -1;
	pImpl_->last_flora = -1;
	pImpl_->last_time = "";

	pImpl_->floraSettings = Moo::GraphicsSetting::getFromLabel("FLORA_DENSITY");
	pImpl_->numFloraOptions = 0;
}

PageDisplay::~PageDisplay()
{}

void PageDisplay::DoDataExchange(CDataExchange* pDX)
{
	CFormView::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_DISPLAY_GENERAL_BOX, pImpl_->generalBox );
	DDX_Control(pDX, IDC_SHOW_AXES, pImpl_->showAxes );
	DDX_Control(pDX, IDC_CHECK_FOR_SPARKLES, pImpl_->checkForSparkles);
	DDX_Control(pDX, IDC_SHOW_BLOOM, pImpl_->showBloom);
	DDX_Control(pDX, IDC_SHOW_SHIMMER, pImpl_->showShimmer);

	DDX_Control(pDX, IDC_DISPLAY_MODEL_BOX, pImpl_->modelBox );
	DDX_Control(pDX, IDC_SHOW_MODEL, pImpl_->showModel);
	DDX_Control(pDX, IDC_SHOW_WIREFRAME, pImpl_->showWireframe);
	DDX_Control(pDX, IDC_SHOW_SKELETON, pImpl_->showSkeleton);
	DDX_Control(pDX, IDC_SHADOWING, pImpl_->shadowing);
	DDX_Control(pDX, IDC_SHOW_BSP, pImpl_->showBsp);
	DDX_Control(pDX, IDC_SHOW_BOUNDING_BOX, pImpl_->showBoundingBox);
	DDX_Control(pDX, IDC_SHOW_PORTALS, pImpl_->showPortals);
	DDX_Control(pDX, IDC_GROUND_MODEL, pImpl_->groundModel);
	DDX_Control(pDX, IDC_CENTRE_MODEL, pImpl_->centreModel);

	DDX_Control(pDX, IDC_DISPLAY_BKG_BOX, pImpl_->bkgBox );
	DDX_Control(pDX, IDC_DISPLAY_BKG, pImpl_->bkg);
	DDX_Control(pDX, IDC_DISPLAY_FLORA, pImpl_->flora);
	DDX_Control(pDX, IDC_DISPLAY_CHOOSE_BKG_COLOUR, pImpl_->bkgColour);
	DDX_Control(pDX, IDC_DISPLAY_CHOOSE_FLOOR_TEXTURE, pImpl_->floorTexture);

	DDX_Control(pDX, IDC_DISPLAY_TIMEOFDAY_BOX, pImpl_->timeOfDayBox );
	DDX_Control(pDX, IDC_TIMEOFDAY_TEXT, pImpl_->timeOfDayEdit);
	DDX_Control(pDX, IDC_TIMEOFDAY_SLIDER, pImpl_->timeOfDaySlider);

	pImpl_->inited = true;
}

BOOL PageDisplay::OnInitDialog()
{
	if (pImpl_->floraSettings)
	{
		Moo::GraphicsSetting::StringBoolPairVector opts = pImpl_->floraSettings->options();

		pImpl_->numFloraOptions = opts.size();
			
		pImpl_->flora.ResetContent();
		for (unsigned i=0; i<pImpl_->numFloraOptions; i++)
		{
			std::string label; 
			
			if (i != pImpl_->numFloraOptions - 1)
			{
				label = opts[i].first;
				
				StringUtils::toMixedCase( label );

				label += " Flora Density";
			}
			else
			{
				label = "Disable Flora";
			}

			pImpl_->flora.InsertString( 0, label.c_str() );
		}

		pImpl_->floraSettings->selectOption(
			Options::getOptionInt( "settings/floraDensity", pImpl_->numFloraOptions ));
	}
	else
	{
		//Disable the flora tab if we can't get settings
		pImpl_->flora.InsertString( 0, "Flora Disabled" );
		pImpl_->flora.SetCurSel( 0 );
		pImpl_->flora.ModifyStyle(0, WS_DISABLED);
		pImpl_->flora.RedrawWindow();
	}
	
	UalDropManager::instance().add(
		new UalDropFunctor< PageDisplay >( &(pImpl_->floorTexture), "bmp", this, &PageDisplay::floorTextureDrop ) );

	UalDropManager::instance().add(
		new UalDropFunctor< PageDisplay >( &(pImpl_->floorTexture), "tga", this, &PageDisplay::floorTextureDrop ) );

	UalDropManager::instance().add(
		new UalDropFunctor< PageDisplay >( &(pImpl_->floorTexture), "jpg", this, &PageDisplay::floorTextureDrop ) );

	UalDropManager::instance().add(
		new UalDropFunctor< PageDisplay >( &(pImpl_->floorTexture), "dds", this, &PageDisplay::floorTextureDrop ) );

	UalDropManager::instance().add(
		new UalDropFunctor< PageDisplay >( &(pImpl_->floorTexture), "texanim", this, &PageDisplay::floorTextureDrop ) );

	pImpl_->timeOfDaySlider.SetRangeMin(0); 
	pImpl_->timeOfDaySlider.SetRangeMax(60*24);

	MeShell::instance().timeOfDay()->setTimeOfDayAsString(
		Options::getOptionString( "settings/gameTime",
			MeShell::instance().timeOfDay()->getTimeOfDayAsString() ));	

	INIT_AUTO_TOOLTIP();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

BEGIN_MESSAGE_MAP(PageDisplay, CFormView)

	ON_WM_SIZE()

	ON_WM_HSCROLL()

	ON_MESSAGE(WM_UPDATE_CONTROLS, OnUpdateControls)

	ON_BN_CLICKED(IDC_SHOW_AXES, OnBnClickedShowAxes)
	ON_BN_CLICKED(IDC_CHECK_FOR_SPARKLES, OnBnClickedCheckForSparkles)
	ON_BN_CLICKED(IDC_SHOW_BLOOM, OnBnClickedShowBloom)
	ON_BN_CLICKED(IDC_SHOW_SHIMMER, OnBnClickedShowShimmer)

	ON_BN_CLICKED(IDC_SHOW_MODEL, OnBnClickedShowModel)
	ON_BN_CLICKED(IDC_SHOW_WIREFRAME, OnBnClickedShowWireframe)
	ON_BN_CLICKED(IDC_SHOW_SKELETON, OnBnClickedShowSkeleton)
	ON_CBN_SELCHANGE(IDC_SHADOWING, OnCbnChangeShadowing)
	ON_BN_CLICKED(IDC_SHOW_BSP, OnBnClickedShowBsp)
	ON_BN_CLICKED(IDC_SHOW_BOUNDING_BOX, OnBnClickedShowBoundingBox)
	ON_BN_CLICKED(IDC_SHOW_PORTALS, OnBnClickedShowPortals)
	ON_BN_CLICKED(IDC_GROUND_MODEL, OnBnClickedGroundModel)
	ON_BN_CLICKED(IDC_CENTRE_MODEL, OnBnClickedCentreModel)

	ON_BN_CLICKED(IDC_DISPLAY_CHOOSE_BKG_COLOUR, OnBnClickedDisplayChooseBkgColour)
	ON_CBN_SELCHANGE(IDC_DISPLAY_BKG, OnCbnSelchangeDisplayBkg)
	ON_BN_CLICKED(IDC_DISPLAY_CHOOSE_FLOOR_TEXTURE, OnBnClickedDisplayChooseFloorTexture)

	ON_MESSAGE(WM_SHOW_TOOLTIP, OnShowTooltip)
	ON_MESSAGE(WM_HIDE_TOOLTIP, OnHideTooltip)

	ON_CBN_SELCHANGE(IDC_DISPLAY_FLORA, OnCbnSelchangeDisplayFlora)
END_MESSAGE_MAP()

afx_msg LRESULT PageDisplay::OnShowTooltip(WPARAM wParam, LPARAM lParam)
{
	LPSTR* msg = (LPSTR*)wParam;
	CMainFrame::instance().SetMessageText( *msg );
	return 0;
}

afx_msg LRESULT PageDisplay::OnHideTooltip(WPARAM wParam, LPARAM lParam)
{
	CMainFrame::instance().SetMessageText( "" );
	return 0;
}

void PageDisplay::OnSize(UINT nType, int cx, int cy)
{
	if (!pImpl_->inited) return;
	
	Utilities::stretchToRight( this, pImpl_->generalBox, cx, 6 );
	Utilities::stretchToRight( this, pImpl_->modelBox, cx, 6 );
	Utilities::stretchToRight( this, pImpl_->bkgBox, cx, 6 );
	Utilities::stretchToRight( this, pImpl_->timeOfDayBox, cx, 6 );

	Utilities::stretchToRight( this, pImpl_->shadowing, cx, 12 );
	Utilities::stretchToRight( this, pImpl_->bkg, cx, 12 );
	Utilities::stretchToRight( this, pImpl_->flora, cx, 12 );
	Utilities::stretchToRight( this, pImpl_->bkgColour, cx, 12 );
	Utilities::stretchToRight( this, pImpl_->floorTexture, cx, 12 );
	Utilities::stretchToRight( this, pImpl_->timeOfDaySlider, cx, 12 );
	
	pImpl_->shadowing.RedrawWindow();
	pImpl_->bkg.RedrawWindow();
	pImpl_->flora.RedrawWindow();
	pImpl_->timeOfDaySlider.RedrawWindow();
	
	CFormView::OnSize( nType, cx, cy );
}

void PageDisplay::updateCheck( CButton& button, const std::string& actionName )
{
	int enabled = 0;
	int checked = 0;
	PythonAdapter::instance()->ActionScriptUpdate( actionName, enabled, checked );
	button.SetCheck( checked ? BST_CHECKED : BST_UNCHECKED );
}

afx_msg LRESULT PageDisplay::OnUpdateControls(WPARAM wParam, LPARAM lParam)
{
	if (!pImpl_->ready)
	{
		OnInitDialog();

		pImpl_->ready = true;
	}
	
	if (PythonAdapter::instance())
	{
		updateCheck( pImpl_->showAxes,              "actShowAxes" );
		updateCheck( pImpl_->checkForSparkles,      "actCheckForSparkles");
		updateCheck( pImpl_->showBloom,             "actShowBloom");
		updateCheck( pImpl_->showShimmer,           "actShowShimmer");

		updateCheck( pImpl_->showModel,             "actShowModel");
		updateCheck( pImpl_->showWireframe,         "actShowWireframe");
		updateCheck( pImpl_->showSkeleton,          "actShowSkeleton");
		updateCheck( pImpl_->showBsp,               "actShowBsp");
		updateCheck( pImpl_->showBoundingBox,       "actShowBoundingBox");
		updateCheck( pImpl_->showPortals,           "actShowPortals");
		updateCheck( pImpl_->groundModel,           "actGroundModel");
		updateCheck( pImpl_->centreModel,           "actCentreModel");

		int enabled = 0;
		int index = 0;

		PythonAdapter::instance()->ActionScriptUpdate( "actGetShadowIndex", enabled, index );
		if (index != pImpl_->last_shadow)
		{
			pImpl_->shadowing.SetCurSel( index );
			OnCbnChangeShadowing();
			pImpl_->last_shadow = index;
		}

		PythonAdapter::instance()->ActionScriptUpdate( "actGetBkgIndex", enabled, index );
		if (index != pImpl_->last_bkg)
		{
			pImpl_->bkg.SetCurSel( index );
			OnCbnSelchangeDisplayBkg();
			pImpl_->last_bkg = index;
		}

		if (pImpl_->numFloraOptions)
		{
			index = pImpl_->numFloraOptions - pImpl_->floraSettings->activeOption() - 1;
			if (index != pImpl_->last_flora)
			{
				pImpl_->flora.SetCurSel( index );
				pImpl_->last_flora = index;
			}
		}

		std::string time = MeShell::instance().timeOfDay()->getTimeOfDayAsString();
		if (time != pImpl_->last_time)
		{
			pImpl_->timeOfDayEdit.SetWindowText(time.c_str());
			pImpl_->timeOfDaySlider.SetPos( (int)(
				pImpl_->timeOfDaySlider.GetRangeMin() +
				(pImpl_->timeOfDaySlider.GetRangeMax() - pImpl_->timeOfDaySlider.GetRangeMin())
				* MeShell::instance().timeOfDay()->gameTime() / 24.f ));
			Options::setOptionString( "settings/gameTime", time );
			pImpl_->last_time = time;
		}
	}

	return 0;
}

void PageDisplay::OnBnClickedShowAxes()
{
	if (PythonAdapter::instance())
		PythonAdapter::instance()->ActionScriptExecute("actShowAxes");
}

void PageDisplay::OnBnClickedCheckForSparkles()
{
	if (PythonAdapter::instance())
		PythonAdapter::instance()->ActionScriptExecute("actCheckForSparkles");
}

void PageDisplay::OnBnClickedShowBloom()
{
	if (PythonAdapter::instance())
		PythonAdapter::instance()->ActionScriptExecute("actShowBloom");
}

void PageDisplay::OnBnClickedShowShimmer()
{
	if (PythonAdapter::instance())
		PythonAdapter::instance()->ActionScriptExecute("actShowShimmer");
}

void PageDisplay::OnBnClickedShowModel()
{
	if (PythonAdapter::instance())
		PythonAdapter::instance()->ActionScriptExecute("actShowModel");
}

void PageDisplay::OnBnClickedShowWireframe()
{
	if (PythonAdapter::instance())
		PythonAdapter::instance()->ActionScriptExecute("actShowWireframe");
}

void PageDisplay::OnBnClickedShowSkeleton()
{
	if (PythonAdapter::instance())
		PythonAdapter::instance()->ActionScriptExecute("actShowSkeleton");
}

void PageDisplay::OnBnClickedShowBsp()
{
	if (PythonAdapter::instance())
		PythonAdapter::instance()->ActionScriptExecute("actShowBsp");
}

void PageDisplay::OnBnClickedShowBoundingBox()
{
	if (PythonAdapter::instance())
		PythonAdapter::instance()->ActionScriptExecute("actShowBoundingBox");
	GUI::Manager::instance()->update(); // Need this since it will cause a toolbar update
}

void PageDisplay::OnBnClickedShowPortals()
{
	if (PythonAdapter::instance())
		PythonAdapter::instance()->ActionScriptExecute("actShowPortals");
}

void PageDisplay::OnBnClickedGroundModel()
{
	if (PythonAdapter::instance())
		PythonAdapter::instance()->ActionScriptExecute("actGroundModel");
}

void PageDisplay::OnBnClickedCentreModel()
{
	if (PythonAdapter::instance())
		PythonAdapter::instance()->ActionScriptExecute("actCentreModel");
}

void PageDisplay::OnCbnChangeShadowing()
{
	if (PythonAdapter::instance())
	{
		switch( pImpl_->shadowing.GetCurSel() )
		{
		case 0:
		default:
			PythonAdapter::instance()->ActionScriptExecute("actShadowOff");
		break;
		case 1:
			PythonAdapter::instance()->ActionScriptExecute("actShadowLowQuality");
		break;
		case 2:
			PythonAdapter::instance()->ActionScriptExecute("actShadowMedQuality");
		break;
		case 3:
			PythonAdapter::instance()->ActionScriptExecute("actShadowHighQuality");
		break;
		};
	}
}

void PageDisplay::OnCbnSelchangeDisplayBkg()
{
	CWaitCursor wait; // This could take a little while...

	if (PythonAdapter::instance())
	{
		switch( pImpl_->bkg.GetCurSel() )
		{
		case 0:
			PythonAdapter::instance()->ActionScriptExecute("actBkgNone");
			pImpl_->flora.ModifyStyle( 0, WS_DISABLED );
			pImpl_->bkgColour.ModifyStyle( WS_DISABLED, 0 );
			pImpl_->floorTexture.ModifyStyle( 0, WS_DISABLED );
		break;
		case 1:
			PythonAdapter::instance()->ActionScriptExecute("actBkgFloor");
			pImpl_->flora.ModifyStyle( 0, WS_DISABLED );
			pImpl_->bkgColour.ModifyStyle( WS_DISABLED, 0 );
			pImpl_->floorTexture.ModifyStyle( WS_DISABLED, 0 );
		break;
		case 2:
		default:
			PythonAdapter::instance()->ActionScriptExecute("actBkgTerrain");
			pImpl_->flora.ModifyStyle(
				pImpl_->floraSettings ? WS_DISABLED : 0,
				pImpl_->floraSettings ? 0 : WS_DISABLED );
			pImpl_->bkgColour.ModifyStyle( 0, WS_DISABLED );
			pImpl_->floorTexture.ModifyStyle( 0, WS_DISABLED );
		break;
		};
		pImpl_->flora.RedrawWindow();
		pImpl_->bkgColour.RedrawWindow();
		pImpl_->floorTexture.RedrawWindow();
	}
}

void PageDisplay::OnCbnSelchangeDisplayFlora()
{
	if (pImpl_->floraSettings)
	{
		int index = pImpl_->numFloraOptions - pImpl_->flora.GetCurSel() - 1;
		pImpl_->floraSettings->selectOption( index );
		Options::setOptionInt( "settings/floraDensity", index );
	}
}

void PageDisplay::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	if (PythonAdapter::instance())
	{
		PythonAdapter::instance()->onSliderAdjust("slrCurrentTime", 
												pImpl_->timeOfDaySlider.GetPos(), 
												pImpl_->timeOfDaySlider.GetRangeMin(), 
												pImpl_->timeOfDaySlider.GetRangeMax());

		std::string currentTime = MeShell::instance().timeOfDay()->getTimeOfDayAsString();
		pImpl_->timeOfDayEdit.SetWindowText( currentTime.c_str() );
	}

	CFormView::OnHScroll(nSBCode, nPos, pScrollBar);
}

void PageDisplay::OnBnClickedDisplayChooseBkgColour()
{
	Vector3 bkgColour = Options::getOptionVector3("settings/bkgColour", Vector3( 255.f, 255.f, 255.f ) );
	CColorDialog colorDlg( RGB( bkgColour[0], bkgColour[1], bkgColour[2] ), CC_FULLOPEN );
	if( colorDlg.DoModal() == IDOK )
	{
		COLORREF col = colorDlg.GetColor();
		int r = (col) & 0xff;
		int g = (col / 256) & 0xff;
		int b = (col / 65536) & 0xff;
		Options::setOptionVector3("settings/bkgColour", Vector3((float) r, (float)g, (float)b ) );
	}
}

void PageDisplay::setFloorTexture( const std::string& texture )
{
	Options::setOptionString( "settings/floorTexture", texture );
	MeApp::instance().floor()->setTextureName( texture );
}

bool PageDisplay::floorTextureDrop( UalItemInfo* ii )
{
	std::string texture = BWResource::dissolveFilename( ii->longText() );
	setFloorTexture( texture );
	return true;
}

void PageDisplay::OnBnClickedDisplayChooseFloorTexture()
{
	static char BASED_CODE szFilter[] =	"Texture files(*.bmp;*.tga;*.jpg;*.dds;*.texanim)|*.bmp;*.tga;*.jpg;*.dds;*.texanim|"
										"Bitmap files(*.bmp)|*.bmp|"
										"Targa files(*.tga)|*.tga|"
										"Jpeg files(*.jpg)|*.jpg|"
										"DDS files(*.dds)|*.dds|"
										"Animated Textures (*.texanim)|*.texanim||";
	CFileDialog fileDlg (TRUE, "", "", OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, szFilter);

	std::string floorTextureDir = BWResource::resolveFilename( MeApp::instance().floor()->getTextureName() );
	std::replace( floorTextureDir.begin(), floorTextureDir.end(), '/', '\\' );
	fileDlg.m_ofn.lpstrInitialDir = floorTextureDir.c_str();

	if ( fileDlg.DoModal() == IDOK )
	{
		std::string floorTexture = BWResource::dissolveFilename( std::string( fileDlg.GetPathName() ));

		if (BWResource::validPath( floorTexture ))
		{
			setFloorTexture( floorTexture );
		}
		else
		{
			::MessageBox( AfxGetApp()->m_pMainWnd->GetSafeHwnd(),
				"You can only use a texture located in one of the resource folders.",
				"Unable to resolve floor texture path",
				MB_OK | MB_ICONWARNING );
		}
	}
}
