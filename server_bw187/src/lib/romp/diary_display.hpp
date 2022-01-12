/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef DIARY_DISPLAY_HPP
#define DIARY_DISPLAY_HPP

#include "cstdmf/diary.hpp"

/**
 *	This class displays the contents of a diary
 */
class DiaryDisplay
{
public:
	static void displayAll();

	static void display( const DiaryEntries & entries, float height,
		uint64 now = timestamp() );
};


#endif // DIARY_DISPLAY_HPP
