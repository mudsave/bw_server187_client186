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

#include "gui_utilities.hpp"
#include "math/vector4.hpp"


// CButtonColor
void AFXAPI DDX_CButtonColor(CDataExchange *pDX, int nIDC, Vector4& crColour);


class CButtonColor : public CButton
{
	DECLARE_DYNAMIC(CButtonColor)

public:
	CButtonColor();
	virtual ~CButtonColor();

	Vector4		getColor(void) const;
	void		setColor(Vector4 color);

	bool		toDelete() const { return toDelete_; }

private:
	Vector4		color_;
	bool		clicked_;	// has the button been clicked
	bool		toDelete_;

protected:
	DECLARE_MESSAGE_MAP()
	virtual void PreSubclassWindow();
	virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);

public:
	afx_msg void OnBnClicked();
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnColorbuttonAlpha();
	afx_msg void OnColorbuttonColor();
	afx_msg void OnColorbuttonDelete();
};
