/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "common/page_messages.hpp"

class BBMsgImpl: public MsgsImpl
{
public:

	BBMsgImpl( PageMessages* msgs );

	void OnNMClickMsgList(NMHDR *pNMHDR, LRESULT *pResult);
	void OnNMCustomdrawMsgList(NMHDR *pNMHDR, LRESULT *pResult);

private:
	PageMessages* msgs_;
	HFONT boldFont;
};