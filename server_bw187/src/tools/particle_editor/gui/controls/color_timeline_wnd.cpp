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
#include "main_frame.hpp"
#include "gui/controls/color_timeline_wnd.hpp"
#include "gui/gui_utilities.hpp"

// ColorTimelineWnd
void AFXAPI DDX_ColorTimelineWnd(CDataExchange *pDX, int nIDC, Vector4 &color, float & time)
{
    HWND hWndCtrl = pDX->PrepareCtrl(nIDC);
    ASSERT (hWndCtrl != NULL);                
    
//    CColorButton* pColourButton = (CColorButton*) CWnd::FromHandle(hWndCtrl);
    if (pDX->m_bSaveAndValidate)
    {
		// read out of control
    }
    else // initializing
    {
		// set into the control
    }

//	DDX_CButtonColor(pDX, IDC_PSA_TINT_COLORS, theColor, time);
}


IMPLEMENT_DYNAMIC(ColorTimelineWnd, CWnd)
ColorTimelineWnd::ColorTimelineWnd()
	: alphaGradientWidth_(20)
	, gradientSpacerWidth_(10)
	, colorGradientWidth_(45)
	, markWidth_(50)
	, indicatorWidth_(5)
	, indicatorHeight_(3)
	, unitHeightTimeIncrement_(0.0f)
	, indicatorColorSwabWidth_(30)
	, indicatorColorSwabHeight_(20)
	, colorGradientHeight_(0)
	, buttonWidth_(14)
	, staticWidth_(30)
	, totalScheduleTime_(0.0f)
	, addTintAtLButton_(false)
{
	verticalBorder_ = indicatorColorSwabHeight_ / 2;
}

ColorTimelineWnd::~ColorTimelineWnd()
{
}

void ColorTimelineWnd::DoDataExchange(CDataExchange* pDX)
{
	CWnd::DoDataExchange(pDX);

	GetParent()->UpdateData(pDX->m_bSaveAndValidate);
}


BOOL ColorTimelineWnd::create(DWORD dwStyle, CRect rcPos, CWnd* pParent, const colorScheduleItems & colorSchedule)
{
	LPVOID lp = (LPVOID)NULL;
	if(!CreateEx(0,
		AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursor(NULL, IDC_ARROW)), 
		_T("ColorTimelineWndWnd"),
		dwStyle,
		rcPos.left,
		rcPos.top,
		rcPos.Width(),
		rcPos.Height(),
		pParent->GetSafeHwnd(),
		(HMENU)(4232),
		lp))
		
	{
		TRACE0("Failed to create ColorTimelineWnd\n");
		return FALSE;
	}


	// since know the size of the window,
	// setup the mapping to window coordinates
	CRect rectClient;			// draw area
	GetClientRect(rectClient);
	colorGradientHeight_ = rectClient.Height() - verticalBorder_ * 2;
	unitHeightTimeIncrement_ = 1.0f / colorGradientHeight_;

	// filling the color schedule
	colorScheduleItems::const_iterator iter1 = colorSchedule.begin();
	colorScheduleItems::const_iterator iterEnd = colorSchedule.end();
	for ( ; iter1 != iterEnd; iter1++)
	{
		colorScheduleItem newItem;
		newItem.normalisedTime_ = iter1->normalisedTime_;
		newItem.color_ = iter1->color_;
		newItem.heightPos_ = verticalBorder_ + static_cast<int>(iter1->normalisedTime_ / unitHeightTimeIncrement_);

		newItem.timeStatic_ = new CStatic;
		newItem.timeStatic_->Create(_T("0"), 0, CRect(0,0,0,0), this, 2);
		
		const int maxStringLength = 50;	// pretty big!
		char charString[maxStringLength];
		sprintf(charString, "%.2f", newItem.normalisedTime_ * totalScheduleTime_);
		newItem.timeStatic_->SetWindowText(charString);

		colorSchedule_.push_back(newItem);
	}

	colorScheduleIterSelected_ = colorSchedule_.end();

	ShowWindow(SW_SHOW);
    UpdateWindow();
	RedrawWindow();
    return TRUE;
}

void ColorTimelineWnd::PostNcDestroy()
{
	colorScheduleItems::iterator iter1 = colorSchedule_.begin();
	colorScheduleItems::iterator iterEnd = colorSchedule_.end();
	for ( ; iter1 != iterEnd; iter1++)
	{
		delete iter1->timeStatic_;
	}

	CWnd::PostNcDestroy();
}


void ColorTimelineWnd::setColorScheduleItemSelectedColor(const Vector4 & color)
{
	if (getColorScheduleItemSelected())
	{
		getColorScheduleItemSelected()->color_ = color;
		InvalidateRect(NULL, FALSE);
	}
}

Vector4 ColorTimelineWnd::getColorScheduleItemSelectedColor()
{
	if (getColorScheduleItemSelected())
		return getColorScheduleItemSelected()->color_;

	return Vector4::zero();
}


colorScheduleItem * ColorTimelineWnd::getColorScheduleItemSelected() const
{
	if (colorScheduleIterSelected_ == colorSchedule_.end())
		return NULL;

	return &(*colorScheduleIterSelected_);
}


bool ColorTimelineWnd::removeSelectedTint()
{
	if (colorScheduleIterSelected_ == colorSchedule_.end())
		return false;

	// can't delete first or last tints!
	colorScheduleItems::iterator lastTint = colorSchedule_.end();
	lastTint--;
	if (colorScheduleIterSelected_ == lastTint)
		return false;

	colorScheduleItems::iterator firstTint = colorSchedule_.begin();
	if (colorScheduleIterSelected_ == firstTint)
		return false;

	delete colorScheduleIterSelected_->timeStatic_;
	colorSchedule_.erase(colorScheduleIterSelected_);
	colorScheduleIterSelected_ = colorSchedule_.end();
	InvalidateRect(NULL, FALSE);

	return true;
}

void ColorTimelineWnd::addTintAtLButton( )
{
	addTintAtLButton_ = true;
}

// blend between colors
static COLORREF ColorTween(COLORREF color1, COLORREF color2, float tween)
{
	float r = GetRValue(color1) + tween * (GetRValue(color1) - GetRValue(color2));
	float g = GetGValue(color1) + tween * (GetGValue(color1) - GetGValue(color2));
	float b = GetBValue(color1) + tween * (GetBValue(color1) - GetBValue(color2));

	return RGB((BYTE)(r*255), (BYTE)(g*255), (BYTE)(b*255));
}

void ColorTimelineWnd::OnPaint() 
{
	// initialise
	CPaintDC dcPaint(this);		// device context for painting
	CDC dcMem;						// support raster operations
	dcMem.CreateCompatibleDC(&dcPaint);

	CRect rectClient;			// draw area
	GetClientRect(rectClient);

	CBitmap bitmap;				// hook up a bitmap
	bitmap.CreateCompatibleBitmap(&dcPaint, rectClient.Width(), rectClient.Height());
	CBitmap* oldBitmap = dcMem.SelectObject(&bitmap);

	// fill background first
	CBrush bgBrush(GetSysColor(COLOR_BTNFACE));
	dcMem.FillRect(&rectClient, &bgBrush);

	// fill color gradient
	colorScheduleItems::iterator iterEnd = colorSchedule_.end();
	colorScheduleItems::iterator iter1 = colorSchedule_.begin();
	if (iter1 == iterEnd)
	{
		// nothing to draw
		Vector4 currentColor;
		COLORREF fillColor = RGBtoCOLORREF(currentColor);
		dcMem.FillSolidRect(alphaGradientWidth_ + gradientSpacerWidth_, verticalBorder_, colorGradientWidth_, colorGradientHeight_, fillColor);
	}
	else
	{
		colorScheduleItems::iterator iter2 = iter1;
		iter2++;

		Vector4 currentColor = iter1->color_;
		float colorIncrement = 1.0f / (iter2->normalisedTime_ - iter1->normalisedTime_) * unitHeightTimeIncrement_;

		// draw first marker
		drawColorMark(dcMem, verticalBorder_, RGBtoCOLORREF(currentColor), *iter1);

		// draw line by line
		for (int i = verticalBorder_; i < colorGradientHeight_ + verticalBorder_; i++)
		{
			if (i == iter2->heightPos_)
			{
				iter1 = iter2;
				iter2++;
				if (iter2 == iterEnd)
					break;

				if (iter1->heightPos_ == iter2->heightPos_)
				{
					// increment again
					iter1 = iter2;
					iter2++;

					if (iter2 == iterEnd)
						break;
				}
				colorIncrement = min(1.0f, 1.0f / (iter2->normalisedTime_ - iter1->normalisedTime_) * unitHeightTimeIncrement_);
				currentColor = iter1->color_;

				// draw marker
				drawColorMark(dcMem, i, RGBtoCOLORREF(currentColor), *iter1);
			}
			else
			{
				Vector4 alphaColor(currentColor.w, currentColor.w, currentColor.w, 1.0f);

				COLORREF fillColor = RGBtoCOLORREF(alphaColor);
				dcMem.FillSolidRect(staticWidth_ + gradientSpacerWidth_, i, alphaGradientWidth_, 1, fillColor);

				fillColor = RGBtoCOLORREF(currentColor);
				dcMem.FillSolidRect(staticWidth_ + alphaGradientWidth_ + 2 * gradientSpacerWidth_, i, colorGradientWidth_, 1, fillColor);
			}

			currentColor += (iter2->color_ - iter1->color_) * colorIncrement;
		}

		// draw marker for last one
		if (iter2 != iterEnd)
		{
			drawColorMark(dcMem, colorGradientHeight_ + verticalBorder_, RGBtoCOLORREF(iter2->color_), *iter2);
		}
	}

	// render!
	CWnd::DefWindowProc( WM_PAINT, (WPARAM)dcMem.m_hDC, 0 );
	dcPaint.BitBlt(rectClient.left, rectClient.top,	rectClient.Width(), rectClient.Height(), &dcMem, 0, 0, SRCCOPY);

	// cleanup
	dcMem.SelectObject(oldBitmap);
	dcMem.DeleteDC();
	bitmap.DeleteObject();
}

void ColorTimelineWnd::drawColorMark(CDC & dcMem, int markHeight, COLORREF color, colorScheduleItem & item)
{
	// Create the pen
	COLORREF colorrefGrey = RGB(102, 102, 102);
	CPen black (PS_SOLID, 1, RGB(0, 0, 0));
	CPen dgrey (PS_SOLID, 1, colorrefGrey);
	CPen*old = dcMem.SelectObject(&black);

	// Draw the thumb
	CPoint offset(alphaGradientWidth_ + 2 * gradientSpacerWidth_ + staticWidth_, 0);

	if ((colorScheduleIterSelected_ != colorSchedule_.end()) && 
		(item.heightPos_ == colorScheduleIterSelected_->heightPos_))	// heightPos_ should be unique
	{
		dcMem.SelectObject(&dgrey);
		dcMem.MoveTo(CPoint(staticWidth_, markHeight-1));
		dcMem.LineTo(CPoint(markWidth_, markHeight-1) + offset);
		dcMem.MoveTo(CPoint(staticWidth_, markHeight+1));
		dcMem.LineTo(CPoint(markWidth_, markHeight+1) + offset);
	}
	dcMem.SelectObject(&black);
	dcMem.MoveTo(CPoint(staticWidth_, markHeight));
	dcMem.LineTo(CPoint(markWidth_, markHeight) + offset);

	// draw indicator
	CPoint tri1(markWidth_, markHeight);
	CPoint tri2(indicatorWidth_ + markWidth_, indicatorHeight_ + markHeight);
	CPoint tri3(indicatorWidth_ + markWidth_, -indicatorHeight_ + markHeight);

	tri1 += offset;
	tri2 += offset;
	tri3 += offset;

	// shadow
	CPoint shadowOffset(1, 1);
	dcMem.SelectObject (&dgrey);
	dcMem.MoveTo(tri1 + shadowOffset);
	dcMem.LineTo(tri2 + shadowOffset);
	dcMem.LineTo(tri3 + shadowOffset);
	dcMem.LineTo(tri1 + shadowOffset);

	// triangle
	dcMem.SelectObject (&black);
	dcMem.MoveTo(tri1);
	dcMem.LineTo(tri2);
	dcMem.LineTo(tri3);
	dcMem.LineTo(tri1);

	int x1 = tri2.x;
	int y1 = markHeight - indicatorColorSwabHeight_ / 2;
	int x2 = x1 + indicatorColorSwabWidth_;
	int y2 = y1 + indicatorColorSwabHeight_;
	dcMem.FillSolidRect(x1, y1, indicatorColorSwabWidth_, indicatorColorSwabHeight_, color);

	if ((colorScheduleIterSelected_ != colorSchedule_.end()) &&
		(item.heightPos_ == colorScheduleIterSelected_->heightPos_))
	{
//		CRgn swabRgn;
//		swabRgn.CreateRectRgn(x1, y1, x2, y2);
//		CBrush brush;
//		brush.CreateSolidBrush( RGB(255, 0, 0) );
//		dcMem.FrameRgn(&swabRgn, &brush, 1, 1);

		// draw an indicator near the button
		dcMem.FillSolidRect(x2 + 2, y1, 6, indicatorColorSwabHeight_, colorrefGrey);
	}

	item.timeStatic_->SetWindowPos(&CWnd::wndTop, 0, markHeight - indicatorColorSwabHeight_ / 2 + offset.y, staticWidth_, indicatorColorSwabHeight_, SWP_SHOWWINDOW | SWP_FRAMECHANGED);
}

void ColorTimelineWnd::totalScheduleTime(float time)
{
	totalScheduleTime_ = time;

	// filling the new times
	const int maxStringLength = 50;	// pretty big!
	char charString[maxStringLength];

	colorScheduleItems::const_iterator iter1 = colorSchedule_.begin();
	colorScheduleItems::const_iterator iterEnd = colorSchedule_.end();
	for ( ; iter1 != iterEnd; iter1++)
	{
		sprintf(charString, "%.2f", iter1->normalisedTime_ * totalScheduleTime_);
		iter1->timeStatic_->SetWindowText(charString);
	}
}

void ColorTimelineWnd::OnLButtonDown( UINT nFlags, CPoint point )
{
	colorScheduleIterSelected_ = colorSchedule_.end();

	// test proximity to the color marks
	if (point.x < 0)
		return;

	if (point.x > alphaGradientWidth_ + gradientSpacerWidth_ + 	colorGradientWidth_ + markWidth_ + indicatorWidth_ + indicatorColorSwabWidth_)
		return;

	colorScheduleItems::iterator iter = colorSchedule_.begin();
	colorScheduleItems::iterator iterEnd = colorSchedule_.end();
	colorScheduleItems::iterator nextIter = iter;
	nextIter++;

	for ( ; iter != iterEnd; iter++)
	{
		int heightFromCenter = 3;
		if (point.x > alphaGradientWidth_ + gradientSpacerWidth_ + 	colorGradientWidth_)
			heightFromCenter = indicatorColorSwabHeight_ / 2;

		if ((point.y > iter->heightPos_ - heightFromCenter) && (point.y < iter->heightPos_ + heightFromCenter))
		{
			colorScheduleIterSelected_ = iter;

			// remember mouse point
			mousePosition_ = point;

			// force redraw
			InvalidateRect(NULL, FALSE);

			// tell picker what its color should be
			UpdateData(false);

			break;
		}


		if ((point.y > iter->heightPos_ + heightFromCenter) && (point.y < nextIter->heightPos_ - heightFromCenter))
			break; // past any point

		nextIter++;
	}

	if 
    (
        addTintAtLButton_ 
        && 
        colorScheduleIterSelected_ == colorSchedule_.end()
        &&
        iter != colorSchedule_.end()
    )
	{
		MainFrame::instance().PotentiallyDirty
        (
            true,
            UndoRedoOp::AK_NPARAMETER,
            "Add Tint"
        );

		// add new tint here
		colorScheduleItem newItem;
		newItem.heightPos_ = point.y;
		newItem.normalisedTime_ = (newItem.heightPos_ - verticalBorder_) * unitHeightTimeIncrement_;
		
		// approx tween the existing colors
		Vector4 colorDifference = nextIter->color_ - iter->color_;
		float proportion = (float)(newItem.heightPos_ - iter->heightPos_) / (nextIter->heightPos_ - iter->heightPos_);
		newItem.color_ = iter->color_ + proportion * colorDifference;

		newItem.timeStatic_ = new CStatic;
		newItem.timeStatic_->Create(_T("0"), 0, CRect(0,0,0,0), this, 2);
		
		const int maxStringLength = 50;	// pretty big!
		char charString[maxStringLength];
		sprintf(charString, "%.2f", newItem.normalisedTime_ * totalScheduleTime_);
		newItem.timeStatic_->SetWindowText(charString);

		colorScheduleIterSelected_ = colorSchedule_.insert(nextIter, newItem);
		addTintAtLButton_ = false;

		mousePosition_ = point;			// remember mouse point
		InvalidateRect(NULL, FALSE);	// force redraw
		UpdateData(false);
		UpdateData(true);
	}
}

void ColorTimelineWnd::OnMouseMove( UINT nFlags, CPoint point ) 
{
    CWnd::OnMouseMove (nFlags, point);

	if ( !(nFlags & MK_LBUTTON) )
		return;

	// if is the first or the end item, do not move it!
	colorScheduleItems::iterator iterStart = colorSchedule_.begin();
	colorScheduleItems::iterator iterEnd = colorSchedule_.end();
	iterEnd--;

	if (colorScheduleIterSelected_ == iterStart)
		return;

	if (colorScheduleIterSelected_ == iterEnd)
		return;


	const int verticalMouseDisplacement = point.y - mousePosition_.y;
	mousePosition_ = point;

	// test for valid mouse position
	int xExtent = alphaGradientWidth_ + 2 * gradientSpacerWidth_ + staticWidth_ + indicatorWidth_ + markWidth_ + indicatorColorSwabWidth_;
	if (point.x > xExtent)
		return;

	if ((point.y < verticalBorder_) || (point.y > (verticalBorder_ + colorGradientHeight_)))
		return;

	if (colorScheduleIterSelected_ != colorSchedule_.end())
	{
		// move the indicator
		colorScheduleIterSelected_->heightPos_ += verticalMouseDisplacement;
		colorScheduleIterSelected_->normalisedTime_ = (colorScheduleIterSelected_->heightPos_ - verticalBorder_) * unitHeightTimeIncrement_;
		const int maxStringLength = 50;	// pretty big!
		char charString[maxStringLength];
		sprintf(charString, "%.2f", colorScheduleIterSelected_->normalisedTime_ * totalScheduleTime_);
		colorScheduleIterSelected_->timeStatic_->SetWindowText(charString);

		// test for overlap and swap as required
		colorScheduleItems::iterator next = colorScheduleIterSelected_;
		next++;
		colorScheduleItems::iterator previous = colorScheduleIterSelected_;
		previous--;
		ASSERT(next != colorSchedule_.end());
		colorScheduleItems::iterator lastValid = colorSchedule_.end();
		lastValid--;

		if (colorScheduleIterSelected_->heightPos_ <= previous->heightPos_)
		{
			if (previous == colorSchedule_.begin())
			{
				// stop movement
				colorScheduleIterSelected_->heightPos_ = previous->heightPos_ + 1;
			}
			else
			{
				// put on the other side
				colorScheduleItem temp = *previous;
				*previous = *colorScheduleIterSelected_;
				*colorScheduleIterSelected_ = temp;

				colorScheduleIterSelected_ = previous;
				colorScheduleIterSelected_->heightPos_ -= 1;
			}
		}
		else if (colorScheduleIterSelected_->heightPos_ >= next->heightPos_)
		{
			if (next == lastValid)
			{
				// stop movement
				colorScheduleIterSelected_->heightPos_ = next->heightPos_ - 1;
			}
			else
			{
				// put on the other side
				colorScheduleItem temp = *next;
				*next = *colorScheduleIterSelected_;
				*colorScheduleIterSelected_ = temp;

				colorScheduleIterSelected_ = next;
				colorScheduleIterSelected_->heightPos_ += 1;
			}
		}

		// force redraw
		InvalidateRect(NULL, FALSE);
	}

	// update the real object
	GetParent()->SendMessage(WM_UPDATE_PROPERTIES);
}

void ColorTimelineWnd::OnLButtonUp( UINT nFlags, CPoint point )
{
	MainFrame::instance().OnBatchedUndoOperationEnd();
}

BOOL ColorTimelineWnd::OnEraseBkgnd(CDC* pDC) 
{
    return TRUE;
}


BEGIN_MESSAGE_MAP(ColorTimelineWnd, CWnd)
	//{{AFX_MSG_MAP(ColorTimelineWnd)
	ON_WM_PAINT()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_ERASEBKGND()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()



// ColorTimelineWnd message handlers
