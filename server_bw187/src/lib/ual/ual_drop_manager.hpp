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

#include "ual/ual_callback.hpp"
#include <map>

class UalDropCallback : public ReferenceCount
{
public:
	UalDropCallback() {}

	UalDropCallback(CWnd* wnd, const std::string& ext, bool canAdd ):
		wnd_(wnd),
		ext_(ext),
		canAdd_(canAdd)
	{}

	virtual bool execute( UalItemInfo* ii ) = 0;

	CWnd* cWnd() { return wnd_;  }

	HWND hWnd() { return wnd_->GetSafeHwnd(); }

	const std::string& ext() { return ext_; }

	bool canAdd() { return canAdd_; }

	virtual CRect test( UalItemInfo* ii ) { return CRect(-1,-1,-1,-1); }

protected:
	CWnd* wnd_;
	std::string ext_;
	bool canAdd_;
};

template< class C >
class UalDropFunctor: public UalDropCallback
{
public:
	typedef bool (C::*Method)( UalItemInfo* ii );
	typedef CRect (C::*Method2)( UalItemInfo* ii );
	
	UalDropFunctor() {}
	
	UalDropFunctor( CWnd* wnd, const std::string& ext, C* instance, Method method, bool canAdd = false, Method2 test = NULL ):
		UalDropCallback( wnd, ext, canAdd ),
		instance_(instance),
		method_(method),
		test_(test)
	{
	}

	bool execute( UalItemInfo* ii )
	{
		return (instance_->*method_)( ii );
	}

	CRect test( UalItemInfo* ii )
	{
		if (test_ != NULL)
			return (instance_->*test_)( ii );
		else
			return CRect(-1,-1,-1,-1);

	}
private:
	C* instance_;
	Method method_;
	Method2 test_;
};

typedef std::pair < HWND , SmartPointer < UalDropCallback > > DropType;
typedef std::multimap < HWND , SmartPointer < UalDropCallback > > DropMap;
typedef DropMap::const_iterator DropMapIter;

class UalDropManager
{
private:

	static const int DRAG_BORDER = 3;
	static const DWORD DRAG_COLOUR = RGB( 96,96,96 );

public:

	static UalDropManager& instance();
	
	UalDropManager();
	
	void add( SmartPointer< UalDropCallback > dropping );
	
	void start( const std::string& ext);
	
	void highlight( const CRect& rect, bool lit );
	
	SmartPointer< UalDropCallback > test( HWND hwnd, UalItemInfo* ii );
		
	SmartPointer< UalDropCallback > test( UalItemInfo* ii );

	bool end( UalItemInfo* ii );

private:

	DropMap droppings_;
	std::string ext_;
	CWnd* desktop_;
	CDC* dc_;
	CPen pen_;
	CPen* oldPen_;
	int oldROP_;
	CRect highlightRect_;
	bool highlighted_;
};