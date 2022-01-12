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

#include "controls/image_button.hpp"

class PageAnimations;

struct PageAnimationsImpl: public SafeReferenceCount
{
	static PageAnimations* s_currPage;

	bool ready;
	bool inited;
	bool updating;
	int updateCount;

	bool wasPlaying;

	int lastLockedParents;

	std::string modelName;
	std::string fileName;
	std::string lastAnim;

	int lastFrameNum;
	
	HTREEITEM lastItem;

	HTREEITEM nodeItem;
	std::string nodeName;

	CToolBarCtrl toolbar;
	controls::EditCommit name;
	CEdit source;
	CButton change_anim;
	controls::EditNumeric frameRate;
	
	controls::EditCommit first;
	controls::EditCommit last;
	controls::EditNumeric frameNum;
	controls::Slider frameNumSlider;

	controls::Slider frameRateSlider;
	CButton frameRateSave;

	CWnd nodeBox;
	CTreeCtrl nodeTree;
	controls::EditNumeric blend;
	controls::Slider blendSlider;
	CButton blendRemove;

	CWnd compBox;
	controls::Slider compPosSldr;
	controls::Slider compRotSldr;
	controls::Slider compScaleSldr;

	controls::ImageButton compPosMinus;
	controls::ImageButton compRotMinus;
	controls::ImageButton compScaleMinus;

	controls::ImageButton compPosPlus;
	controls::ImageButton compRotPlus;
	controls::ImageButton compScalePlus;

	CStatic compPos;
	CStatic compRot;
	CStatic compScale;

	CStatic compTotal;

	CToolBarCtrl compToolbar;

	bool compChanged;
	bool animChanged;
};