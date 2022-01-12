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
#include "big_bang_progress_bar.hpp"
#include "ashes/matrix_gui_shader.hpp"

//note : this whole class is a hard-coded hack.
//cannot use this class with other progress bar models,
//thus an explicit name is passed into the base class.
BigBangProgressBar::BigBangProgressBar():
SuperModelProgressDisplay( "resources/maps/gui/loader2.model" )
{
	taskAttachment_ = new GuiAttachment();
	taskText_ = new TextGUIComponent();
	taskText_->materialFX( SimpleGUIComponent::FX_ADD );
	taskText_->colour( 0xffC76535 );//199,101,53
	//0xfffbb657
	taskNode_ = superModel_->findNode( "Rectangle18" );
	MatrixGUIShader* mgs = new MatrixGUIShader();
	textTransform_ = new PyMatrix();
	textTransform_->setScale(32,32,1);
	textTransform_->postRotateX(DEG_TO_RAD(90));
	textTransform_->translation(Vector3(4,-0.5,0));
	mgs->target( textTransform_ );
	textPosition_ = mgs;
	taskText_->addShader( "transform", &*textPosition_ );
	taskAttachment_->component( taskText_ );
}


BigBangProgressBar::~BigBangProgressBar()
{
	taskAttachment_->component( NULL );
	taskText_ = NULL;
	taskNode_ = NULL;
}


void BigBangProgressBar::drawOther(float dTime)
{
	ProgressDisplay::ProgressNode& pn = tasks_[tasks_.size()-1];
	taskText_->slimLabel( pn.name );
	taskText_->update( dTime );
	taskText_->applyShaders( dTime );
	taskAttachment_->draw( taskNode_->worldTransform(), 0 );
}
