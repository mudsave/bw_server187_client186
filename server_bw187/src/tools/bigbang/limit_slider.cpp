/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// limit_slider.cpp : implementation file
//

#include "pch.hpp"
#include "resource.h"
#include "limit_slider.hpp"
#include "controls/user_messages.hpp"

// LimitSliderDlg dialog

IMPLEMENT_DYNAMIC(LimitSliderDlg, CDialog)
LimitSliderDlg::LimitSliderDlg(CWnd* pParent /*=NULL*/)
	: CDialog(LimitSliderDlg::IDD, pParent), mChanging( false )
{
}

LimitSliderDlg::~LimitSliderDlg()
{
}

BOOL LimitSliderDlg::OnInitDialog()
{
	BOOL ret = CDialog::OnInitDialog();
	INIT_AUTO_TOOLTIP();
	return ret;
}

void LimitSliderDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_MINEDIT, mMin);
	DDX_Text(pDX, IDC_MAXEDIT, mMax);
//	DDX_Control(pDX, IDC_MINLIMIT, mMinLimit);
//	DDX_Control(pDX, IDC_MAXLIMIT, mMaxLimit);
	DDX_Control(pDX, IDC_SLIDER, mSlider);
}

BEGIN_MESSAGE_MAP(LimitSliderDlg, CDialog)
	ON_WM_ACTIVATE()
	ON_MESSAGE( WM_RANGESLIDER_CHANGED, OnRangeSliderChanged )
	ON_MESSAGE( WM_RANGESLIDER_TRACK, OnRangeSliderChanged )
	ON_EN_KILLFOCUS(IDC_MINEDIT, OnEnKillFocusMinEdit)
	ON_EN_KILLFOCUS(IDC_MAXEDIT, OnEnKillFocusMaxEdit)
	ON_EN_CHANGE(IDC_MINEDIT, OnEnChangeMinEdit)
	ON_EN_CHANGE(IDC_MAXEDIT, OnEnChangeMaxEdit)
END_MESSAGE_MAP()

CString LimitSliderDlg::formatValue( float value )
{
	CString result, format;
	format.Format( "%%.%df", mDigits );
	result.Format( format, value );
	return result;
}

CRect LimitSliderDlg::getBestRect( CSize size, CPoint p )
{
	MONITORINFO monitorInfo = { sizeof( monitorInfo ) };
	GetMonitorInfo( MonitorFromPoint( p, MONITOR_DEFAULTTONEAREST ), &monitorInfo );
	CRect monitor = monitorInfo.rcMonitor;

	static const int offset = 10;
	CPoint leftTop( offset, offset );
	CPoint bottomRight = leftTop + size;
	while( ( leftTop + p ).x < monitor.left )
		leftTop.x += offset, bottomRight.x += offset;
	while( ( leftTop + p ).y < monitor.top )
		leftTop.y += offset, bottomRight.y += offset;
	while( ( bottomRight + p ).x > monitor.right )
		leftTop.x -= offset, bottomRight.x -= offset;
	while( ( bottomRight + p ).y > monitor.bottom )
		leftTop.y -= offset, bottomRight.y -= offset;

	return CRect( leftTop + p, bottomRight + p );
}

void LimitSliderDlg::internalShow( float minLimit, float maxLimit, float min, float max,
	unsigned int digits, LimitSlider* limitSlider )
{
	mDigits = digits;
	mLimitSlider = limitSlider;

	mMin = formatValue( min );
	mMax = formatValue( max );
	UpdateData( FALSE );

//	mMinLimit.SetWindowText( formatValue( minLimit ) );
//	mMaxLimit.SetWindowText( formatValue( maxLimit ) );

	mSlider.setRange( minLimit, maxLimit, (int)mDigits );
	mSlider.setThumbValues( min, max );

	CPoint cursor;
	GetCursorPos( &cursor );

	CRect client;
	GetWindowRect( &client );

	MoveWindow( &getBestRect( client.Size(), cursor ) );
	ShowWindow( SW_SHOW );
	SetActiveWindow();
	UpdateWindow();
}

void LimitSliderDlg::show( float minLimit, float maxLimit, float min, float max,
	unsigned int digits, LimitSlider* limitSlider )
{
	static LimitSliderDlg* limitSliderDlg = NULL;
	if( !limitSliderDlg )
	{
		limitSliderDlg = new LimitSliderDlg;
		limitSliderDlg->Create( IDD, AfxGetMainWnd() );
	}

	limitSliderDlg->internalShow( minLimit, maxLimit, min, max, digits, limitSlider );
}

// LimitSliderDlg message handlers
void LimitSliderDlg::OnActivate( UINT nState, CWnd*, BOOL )
{
	if( nState == WA_INACTIVE )
	{
		ShowWindow( SW_HIDE );
		UpdateWindow();
	}
}

LRESULT LimitSliderDlg::OnRangeSliderChanged(WPARAM wParam, LPARAM lParam)
{
	if( mChanging )
		return 0;
	mChanging = true;
	float min, max;
	mSlider.getThumbValues( min, max );
	mMin = formatValue( min );
	mMax = formatValue( max );
	UpdateData( FALSE );
	mLimitSlider->setRange( min, max );
	mChanging = false;
	return 1;
}

void LimitSliderDlg::OnEnKillFocusMinEdit()
{
	if( mChanging )
		return;
	float minLimit, maxLimit, min, max;
	mSlider.getRange( minLimit, maxLimit );
	mSlider.getThumbValues( min, max );

	UpdateData( TRUE );
	float f;
	sscanf( mMin, "%f", &f );

	if( f < minLimit )
	{
		mMin = formatValue( minLimit );
		UpdateData( FALSE );
	}
	else if( f > max )
	{
		mMin = formatValue( max );
		UpdateData( FALSE );
	}
	mChanging = true;
	mSlider.setThumbValues( f, max );
	mChanging = false;
}

void LimitSliderDlg::OnEnKillFocusMaxEdit()
{
	if( mChanging )
		return;
	float minLimit, maxLimit, min, max;
	mSlider.getRange( minLimit, maxLimit );
	mSlider.getThumbValues( min, max );

	UpdateData( TRUE );
	float f;
	sscanf( mMax, "%f", &f );

	if( f > maxLimit )
	{
		mMax = formatValue( maxLimit );
		UpdateData( FALSE );
	}
	else if( f < min )
	{
		mMax = formatValue( min );
		UpdateData( FALSE );
	}
	mChanging = true;
	mSlider.setThumbValues( min, f );
	mChanging = false;
}

void LimitSliderDlg::OnEnChangeMinEdit()
{
	if( mChanging )
		return;
	float minLimit, maxLimit, min, max;
	mSlider.getRange( minLimit, maxLimit );
	mSlider.getThumbValues( min, max );

	UpdateData( TRUE );
	float f;
	sscanf( mMin, "%f", &f );

	if( f >= minLimit && f <= max )
	{
		mChanging = true;
		mSlider.setThumbValues( f, max );
		mLimitSlider->setRange( f, max );
		mChanging = false;
	}
}

void LimitSliderDlg::OnEnChangeMaxEdit()
{
	if( mChanging )
		return;
	float minLimit, maxLimit, min, max;
	mSlider.getRange( minLimit, maxLimit );
	mSlider.getThumbValues( min, max );

	UpdateData( TRUE );
	float f;
	sscanf( mMax, "%f", &f );

	if( f <= maxLimit && f >= min )
	{
		mChanging = true;
		mSlider.setThumbValues( min, f );
		mLimitSlider->setRange( min, f );
		mChanging = false;
	}
}

// LimitSlider

IMPLEMENT_DYNAMIC(LimitSlider, CSliderCtrl)
LimitSlider::LimitSlider()
	:mDigits( 0 ), mMin( 0 ), mMax( 100 ), mMinLimit( 0 ), mMaxLimit( 1000 )
{}

LimitSlider::~LimitSlider()
{}

float LimitSlider::getValue() const
{
	int pos = GetPos();
	float fpos = float( ( float( pos ) ) / powf( 10.f, (float)mDigits ) );
	if( fpos <= mMin )
		fpos = mMin;
	if( fpos >= mMax )
		fpos = mMax;
	return fpos;
}

void LimitSlider::setValue( float value )
{
	if( value > mMaxLimit )
		value = mMaxLimit;
	if( value < mMinLimit )
		value = mMinLimit;
	if( value > mMax )
		setRange( mMin, value );
	if( value < mMin )
		setRange( value, mMax );
	SetPos( int( value * powf( 10.f, (float)mDigits ) >= 0 ? ( value * powf( 10.f, (float)mDigits ) + 0.5 )
		: ( value * powf( 10.f, (float)mDigits ) - 0.5 ) ) );
	GetParent()->PostMessage( WM_HSCROLL, SB_THUMBPOSITION, NULL );
}

void LimitSlider::setDigits( unsigned int digits )
{
	if( mDigits != digits )
	{
		mDigits = digits;
		int min = int( mMin * powf( 10.f, (float)mDigits ) >= 0 ? ( mMin * powf( 10.f, (float)mDigits ) + 0.5 )
			: ( mMin * powf( 10.f, (float)mDigits ) - 0.5 ) );
		int max = int( mMax * powf( 10.f, (float)mDigits ) >= 0 ? ( mMax * powf( 10.f, (float)mDigits ) + 0.5 )
			: ( mMax * powf( 10.f, (float)mDigits ) - 0.5 ) );
		SetRange( min, max );
	}
}

void LimitSlider::setRange( float min, float max )
{
	ASSERT( min >= mMinLimit );
	ASSERT( max <= mMaxLimit );
	if( min != mMin || max != mMax )
	{
		float value = getValue();
		mMin = min;
		mMax = max;
		int sliderMin = int( mMin * powf( 10.f, (float)mDigits ) >= 0 ? ( mMin * powf( 10.f, (float)mDigits ) + 0.5 )
			: ( mMin * powf( 10.f, (float)mDigits ) - 0.5 ) );
		int sliderMax = int( mMax * powf( 10.f, (float)mDigits ) >= 0 ? ( mMax * powf( 10.f, (float)mDigits ) + 0.5 )
			: ( mMax * powf( 10.f, (float)mDigits ) - 0.5 ) );
		SetRange( sliderMin, sliderMax, TRUE );
		if( value < min )
			value = min;
		if( value > max )
			value = max;
		setValue( value );
	}
}

void LimitSlider::setRangeLimit( float min, float max )
{
	mMinLimit = min;
	mMaxLimit = max;
	if( mMin < min || mMax < max )
	{
		setRange(	mMin < min	?	min	:	mMin,
			mMax > max	?	max	:	mMax );
	}
}

float LimitSlider::getMinRange() const
{
	return mMin;
}

float LimitSlider::getMaxRange() const
{
	return mMax;
}

void LimitSlider::beginEdit()
{
	LimitSliderDlg::show( mMinLimit, mMaxLimit, mMin, mMax, mDigits, this );
}

BEGIN_MESSAGE_MAP(LimitSlider, CSliderCtrl)
	ON_WM_RBUTTONDOWN()
END_MESSAGE_MAP()

// LimitSlider message handlers
void LimitSlider::OnRButtonDown( UINT nFlags, CPoint point )
{
	LimitSliderDlg::show( mMinLimit, mMaxLimit, mMin, mMax, mDigits, this );
}
