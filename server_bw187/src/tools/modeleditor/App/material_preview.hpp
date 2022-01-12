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

class MaterialPreview
{
public:
	MaterialPreview():
		hasNew_( false ),
		needsUpdate_( false ),
		updating_( false )
	{}
	
	static MaterialPreview& instance()
	{
		static MaterialPreview inst;
		return inst;
	}

	void needsUpdate( bool needed ) { needsUpdate_ = needed; }

	bool hasNew() { return hasNew_; }
	void hasNew( bool has ) { hasNew_ = has; }

	void update();

private:
	bool hasNew_;
	bool needsUpdate_;
	bool updating_;

	SmartPointer<Moo::RenderTarget> rt_;
};