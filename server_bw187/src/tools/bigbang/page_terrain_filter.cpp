/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// implementation file
//

#include "pch.hpp"
#include "bigbang.h"
#include "big_bang.hpp"
#include "appmgr/options.hpp"
#include "guimanager/gui_manager.hpp"
#include "page_terrain_filter.hpp"
#include "python_adapter.hpp"
#include "user_messages.hpp"
#include "resmgr/dataresource.hpp"

DECLARE_DEBUG_COMPONENT( 0 )

// PageTerrainFilter


// GUITABS content ID ( declared by the IMPLEMENT_BASIC_CONTENT macro )
const std::string PageTerrainFilter::contentID = "PageTerrainFilter";


PageTerrainFilter::PageTerrainFilter()
	: PageTerrainBase(PageTerrainFilter::IDD),
	pageReady_( false ),
	lastFilterIndex_( -1 )
{
    sizeEdit_.SetNumericType(controls::EditNumeric::ENT_INTEGER);
}

PageTerrainFilter::~PageTerrainFilter()
{
}

void PageTerrainFilter::DoDataExchange(CDataExchange* pDX)
{
	PageTerrainBase::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TERRAIN_SIZE_EDIT, sizeEdit_);
	DDX_Control(pDX, IDC_TERRAIN_FALLOFF_LINEAR, falloffLinear_);
	DDX_Control(pDX, IDC_TERRAIN_FALLOFF_CURVE, falloffCurve_);
	DDX_Control(pDX, IDC_TERRAIN_FALLOFF_FLAT, falloffFlat_);
	DDX_Control(pDX, IDC_TERRAIN_FILTERS_LIST, filtersList_);
	DDX_Control(pDX, IDC_TERRAIN_SIZE_SLIDER, sizeSlider_);
	DDX_Control(pDX, IDC_SIZERANGE, mSizeRange);
}


BOOL PageTerrainFilter::OnInitDialog()
{
	PageTerrainBase::OnInitDialog();

	sizeSlider_.setRangeLimit( Options::getOptionFloat( "terrain/filter/minsizelimit", 1 ),
		Options::getOptionFloat( "terrain/filter/maxsizelimit", 4000 ) );
	sizeSlider_.setRange( Options::getOptionFloat( "terrain/filter/minsize", 1 ),
		Options::getOptionFloat( "terrain/filter/maxsize", 1000 ) );
	sizeSlider_.setValue( Options::getOptionFloat( "terrain/filter/size", 1 ) );

	loadFilters();

	OnUpdateControls(0, 0);
	updateSliderEdits();

	mSizeRange.SetWindowPos( NULL, 0, 0, 16, 16, SWP_NOMOVE | SWP_NOZORDER );
	mSizeRange.setBitmapID( IDB_RANGESLIDER, IDB_RANGESLIDER );

	pageReady_ = true;

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}


void PageTerrainFilter::loadFilters()
{
	filtersList_.ResetContent();
    
	DataResource filters;
    if ( filters.load( "resources/data/filters.xml" ) != DataHandle::DHE_NoError)
    {
    	ERROR_MSG( "PageTerrainFilter::loadFilters() - Could not open resources/data/filters.xml\n" );
		return;
    }

	DataSectionPtr pSection = filters.getRootSection();
	if ( !pSection )
    {
    	ERROR_MSG( "PageTerrainFilter::loadFilters() - Could not open resources/data/filters.xml\n" );
		return;
    }

    std::vector<DataSectionPtr>	pSections;
    pSection->openSections( "filter", pSections );

	std::vector<DataSectionPtr>::iterator it = pSections.begin();
    std::vector<DataSectionPtr>::iterator end = pSections.end();
	int i = 0;
    while ( it != end )
    {
        DataSectionPtr pFilter = *it++;
		if( pFilter->readBool( "included", true ) )
			filtersList_.InsertString( i++, pFilter->readString( "name", "unknown filter" ).c_str() );
		else
			break;
    }
}



BEGIN_MESSAGE_MAP(PageTerrainFilter, PageTerrainBase)
	ON_WM_SETFOCUS()
	ON_WM_SIZE()
	ON_WM_HSCROLL()
	ON_MESSAGE (WM_ACTIVATE_TOOL, OnActivateTool)
	ON_MESSAGE (WM_UPDATE_CONTROLS, OnUpdateControls)
	ON_LBN_SELCHANGE(IDC_TERRAIN_FILTERS_LIST, OnLbnSelchangeTerrainFiltersList)
	ON_WM_MOUSEMOVE()
	ON_EN_CHANGE(IDC_TERRAIN_SIZE_EDIT, OnEnChangeTerrainSizeEdit)
	ON_BN_CLICKED(IDC_SIZERANGE, OnBnClickedSizerange)
END_MESSAGE_MAP()


// PageTerrainFilter message handlers

void PageTerrainFilter::OnSetFocus( CWnd* pOldWnd )
{
	//if ( BigBang2App::instance().panelsReady() )
	//{
	//	BigBang::instance().updateUIToolMode( PageTerrainFilter::contentID );
	//	OnActivateTool( 0, 0 );
	//}
}

LRESULT PageTerrainFilter::OnActivateTool(WPARAM wParam, LPARAM lParam)
{
	if (PythonAdapter::instance())
		PythonAdapter::instance()->onPageControlTabSelect("pgcTerrain", "TerrainFilters");

	return 0;
}


afx_msg void PageTerrainFilter::OnSize( UINT nType, int cx, int cy )
{
	PageTerrainBase::OnSize( nType, cx, cy );

	if ( !pageReady_ )
		return;

	// move the controls as appropriate
	// stretch the file selector to be long enough to fit into the window
	CRect rect;
	GetClientRect(rect);
	static const int MARGIN = 10;
	const int newWidth = std::max( 1, rect.Width() - MARGIN * 2 );
	
	CRect sliderRect, buttonRect;
	sizeSlider_.GetWindowRect(sliderRect);
	mSizeRange.GetWindowRect(buttonRect);
	ScreenToClient( &sliderRect );
	ScreenToClient( &buttonRect );

	mSizeRange.SetWindowPos(NULL, rect.Width() - MARGIN - buttonRect.Width(), buttonRect.top,
		0, 0, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE);

	sizeSlider_.SetWindowPos(NULL, 0, 0,
		newWidth - buttonRect.Width() - sliderRect.left, sliderRect.Height(),
		SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);

	filtersList_.GetWindowRect(sliderRect);
	ScreenToClient( &sliderRect );
	filtersList_.SetWindowPos(NULL, 0, 0,
		newWidth, sliderRect.Height(), SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
}

void PageTerrainFilter::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	// this captures all the scroll events for the page
	// upadate all the slider buddy windows
	updateSliderEdits();

	Options::setOptionFloat( "terrain/filter/size", sizeSlider_.getValue() );

	PageTerrainBase::OnHScroll(nSBCode, nPos, pScrollBar);
}

void PageTerrainFilter::updateSliderEdits()
{
	int new_val = (int)sizeSlider_.getValue();
	int old_val = (int)sizeEdit_.GetIntegerValue();
	if (GetFocus() != &sizeEdit_ && new_val != old_val)
	{
		sizeEdit_.SetIntegerValue( new_val );
	}
}

afx_msg LRESULT PageTerrainFilter::OnUpdateControls(WPARAM wParam, LPARAM lParam)
{
	sizeSlider_.setValue( Options::getOptionFloat( "terrain/filter/size", 1 ) );

	if( lastFilterIndex_ != Options::getOptionInt( "terrain/filter/index", 0 ) )
	{
		lastFilterIndex_ = Options::getOptionInt( "terrain/filter/index", 0 );
		filtersList_.SetCurSel( Options::getOptionInt( "terrain/filter/index", 0 ) );
	}

	updateSliderEdits();

	Options::setOptionFloat( "terrain/filter/minsize", sizeSlider_.getMinRange() );
	Options::setOptionFloat( "terrain/filter/maxsize", sizeSlider_.getMaxRange() );

	return 0;
}

void PageTerrainFilter::OnLbnSelchangeTerrainFiltersList()
{
	Options::setOptionInt( "terrain/filter/index", filtersList_.GetCurSel() );
	lastFilterIndex_ = filtersList_.GetCurSel();
}

BOOL PageTerrainFilter::PreTranslateMessage(MSG* pMsg)
{
	if (GetFocus() == &filtersList_)
	{
		if(pMsg->message == WM_KEYDOWN)
		{
			if(pMsg->wParam == VK_TAB)
			{
				int current = filtersList_.GetCurSel();
				if (++current >= filtersList_.GetCount())
					current = 0;
                filtersList_.SetCurSel( current );
				return true;
			}
		}
	}

	return PageTerrainBase::PreTranslateMessage(pMsg);
}

void PageTerrainFilter::OnMouseMove(UINT nFlags, CPoint point)
{
	// TODO: Add your message handler code here and/or call default

	__super::OnMouseMove(nFlags, point);
}

void PageTerrainFilter::OnEnChangeTerrainSizeEdit()
{
	sizeSlider_.setValue( static_cast<float>(sizeEdit_.GetIntegerValue()) );
	Options::setOptionFloat( "terrain/filter/size", sizeSlider_.getValue() );
}

void PageTerrainFilter::OnBnClickedSizerange()
{
	sizeSlider_.beginEdit();
}
