/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// bigbang2view.hpp : interface of the BigBang2View class
//


#pragma once


class BigBang2View : public CView
{
protected: // create from serialization only
	BigBang2View();
	DECLARE_DYNCREATE(BigBang2View)

	CRect lastRect_;

// Attributes
public:
	BigBang2Doc* GetDocument() const;

// Operations
public:

// Overrides
	public:
	virtual void OnDraw(CDC* pDC);  // overridden to draw this view
virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:

// Implementation
public:
	virtual ~BigBang2View();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg void OnPaint();
	DECLARE_MESSAGE_MAP()
	virtual void OnActivateView(BOOL bActivate, CView* pActivateView, CView* pDeactiveView);
	afx_msg BOOL OnSetCursor( CWnd*, UINT, UINT );
};

#ifndef _DEBUG  // debug version in bigbang2view.cpp
inline BigBang2Doc* BigBang2View::GetDocument() const
   { return reinterpret_cast<BigBang2Doc*>(m_pDocument); }
#endif
