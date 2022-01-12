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

#include <list>
#include "particle_editor.hpp"

class colorScheduleItem
{
public:
	colorScheduleItem()
		: normalisedTime_(0.0f), color_() {}

	float			normalisedTime_;
	Vector4			color_;
	int				heightPos_;
	CStatic *		timeStatic_;

	void operator=( colorScheduleItem &o ) { normalisedTime_ = o.normalisedTime_; color_ = o.color_; heightPos_ = o.heightPos_; }
};
typedef std::list<colorScheduleItem> colorScheduleItems;



// ColorTimelineWnd
void AFXAPI DDX_ColorTimelineWnd(CDataExchange *pDX, int nIDC, Vector4 &color, float & time);

class ColorTimelineWnd : public CWnd
{
	DECLARE_DYNAMIC(ColorTimelineWnd)

public:
	ColorTimelineWnd();
	virtual ~ColorTimelineWnd();

	virtual void PostNcDestroy();

	BOOL create(DWORD dwStyle, CRect rcPos, CWnd* pParent, const colorScheduleItems & colorSchedule);
	void drawColorMark(CDC & pDC, int markHeight, COLORREF color, colorScheduleItem & item);

	colorScheduleItems				colorSchedule_;

	void							totalScheduleTime(float time);

	colorScheduleItem *				getColorScheduleItemSelected() const;

	Vector4							getColorScheduleItemSelectedColor();
	void							setColorScheduleItemSelectedColor(const Vector4 & color);

	bool							removeSelectedTint();
	void							addTintAtLButton();
	bool							waitingToAddTint() { return addTintAtLButton_; }

protected:
	//{{AFX_MSG(ColorTimelineWnd)
	afx_msg void OnPaint();
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

private:
	const int gradientSpacerWidth_;
	const int alphaGradientWidth_;
	const int colorGradientWidth_;
	const int markWidth_;
	const int indicatorWidth_;
	const int indicatorHeight_;
	const int indicatorColorSwabWidth_;
	const int indicatorColorSwabHeight_;
	const int buttonWidth_;
	const int staticWidth_;
	int verticalBorder_;
	int colorGradientHeight_;

	colorScheduleItems::iterator	colorScheduleIterSelected_;
	CPoint mousePosition_;

	float unitHeightTimeIncrement_;

	float totalScheduleTime_;

	bool addTintAtLButton_;
};