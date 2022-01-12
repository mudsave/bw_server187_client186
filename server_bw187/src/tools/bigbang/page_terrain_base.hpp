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

#include "controls/auto_tooltip.hpp"

class PageTerrainBase : public CDialog
{
public:
	PageTerrainBase(UINT nIDTemplate);

	// windows member overrides
//	virtual void PostNcDestroy();

protected:
	BOOL OnInitDialog();

	DECLARE_AUTO_TOOLTIP(PageTerrainBase, CDialog);
	DECLARE_MESSAGE_MAP()
};
