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

#include "cstdmf/smartpointer.hpp"
#include "ashes/gui_attachment.hpp"
#include "ashes/text_gui_component.hpp"
#include "moo/node.hpp"
#include "romp/super_model_progress.hpp"

class BigBangProgressBar : public SuperModelProgressDisplay
{
public:
	BigBangProgressBar();
	~BigBangProgressBar();

	void drawOther(float dTime);
private:
	SmartPointer<Moo::Node>				taskNode_;
	SmartPointer<GuiAttachment>			taskAttachment_;
	SmartPointer<TextGUIComponent>		taskText_;
	SmartPointer<GUIShader>				textPosition_;
	SmartPointer<PyMatrix>				textTransform_;
};