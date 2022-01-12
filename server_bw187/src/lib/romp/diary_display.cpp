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
#include "diary_display.hpp"

#include "geometrics.hpp"
#include "font.hpp"
#include "moo/render_context.hpp"
#include "math/colour.hpp"

float DD_XSCALE = 1.f;
float DD_YSLICE = 0.04f;


// -----------------------------------------------------------------------------
// Section: DiaryDisplay
// -----------------------------------------------------------------------------

/**
 *	Static display all method
 */
void DiaryDisplay::displayAll()
{
	static int shouldDisplayDiaries = -1;
	if (shouldDisplayDiaries == -1)
	{
		shouldDisplayDiaries = 0;
		MF_WATCH( "Diaries/enabled", Diary::s_enabled_, Watcher::WT_READ_WRITE,
			"Enable event diary" );
		MF_WATCH( "Diaries/display", shouldDisplayDiaries, Watcher::WT_READ_WRITE,
			"Display event diary" );
		MF_WATCH( "Diaries/xscale", DD_XSCALE, Watcher::WT_READ_WRITE,
			"The x axis scale of the diary" );
		MF_WATCH( "Diaries/yslice", DD_YSLICE, Watcher::WT_READ_WRITE,
			"The y axis scale of the diary" );
	}
	if (!shouldDisplayDiaries) return;

	std::vector<Diary*> diaries;
	Diary::look( diaries );

	uint disz = diaries.size();
	float heightEach = 2.f / float(disz+1);
	float height = 1.f - heightEach * 0.5f;

	DiaryEntries entries;
	for (uint i = 0; i < disz; i++)
	{
		diaries[i]->look( entries );
		DiaryDisplay::display( entries, height );
		height -= heightEach;
	}
}


static Moo::Colour s_lineColours[8] = {
	Moo::Colour( 1.f, 0.f, 0.f, 1.f ),
	Moo::Colour( 0.f, 1.f, 0.f, 1.f ),
	Moo::Colour( 0.f, 0.f, 1.f, 1.f ),
	Moo::Colour( 1.f, 0.f, 1.f, 1.f ),
	Moo::Colour( 1.f, 1.f, 0.f, 1.f ),
	Moo::Colour( 0.f, 1.f, 1.f, 1.f ),
	Moo::Colour( 0.f, 0.f, 0.f, 1.f ),
	Moo::Colour( 1.f, 1.f, 1.f, 1.f )
};


/**
 *	Static display method
 */
void DiaryDisplay::display( const DiaryEntries & entries,
	float height, uint64 now )
{
	double sps = 1.0 / stampsPerSecondD();
	float hsw = Moo::rc().screenWidth()/2;
	float hsh = Moo::rc().screenHeight()/2;

	static FontPtr myfont = FontManager::instance().get( "default_medium.font" );

	for (int i = int(entries.size())-1; i >= 0; i--)
	{
		DiaryEntry & de = *entries[i];

		float start = float( double(int64(now - de.start_)) * sps );
		float stop = (de.stop_ == 0) ? 0.f :
			float( double(int64(now - de.stop_)) * sps );

		float startX = 1.f - start * DD_XSCALE;
		float stopX  = 1.f - stop  * DD_XSCALE;
		if ((stopX < -1.f) || (startX > 1.f) || (stopX - startX < 2.f / 1000.f))
			continue;

		float level = height - float(de.level_) * DD_YSLICE;
		int cidx = de.colour_ & 7;

		Geometrics::drawRect(
			Vector2( (startX + 1.f) * hsw, (1.f - (level)) * hsh ),
			Vector2( (stopX  + 1.f) * hsw, (1.f - (level - DD_YSLICE)) * hsh ),
			s_lineColours[ cidx ] );

		myfont->colour( Colour::getUint32FromNormalised(
			(Vector4&)s_lineColours[ cidx ] ) );
		FontManager::instance().setMaterialActive( myfont );
		myfont->drawString( de.desc_,
			int(max(startX+1.f,0.f) * hsw),
			int((1.f - (level - DD_YSLICE/2)) * hsh));
	}
}

// diary_display.cpp
