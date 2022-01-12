/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef SUPER_MODEL_PROGRESS_HPP
#define SUPER_MODEL_PROGRESS_HPP


#include "progress.hpp"
#include "super_model.hpp"

class SuperModelProgressDisplay : public ProgressDisplay
{
public:
	SuperModelProgressDisplay( const std::string& modelName );
	virtual ~SuperModelProgressDisplay();

	virtual bool draw( bool force = false );
protected:
	//callback for derived classes
	virtual void drawOther( float dTime )	{};

	SuperModel*	superModel_;
	SuperModelAnimationPtr anim_;
	FashionVector fashions_;
};

#endif