/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

/**
 *	UalCallback: Contains all callback/functor related code
 */


#ifndef UAL_CALLBACK_HPP
#define UAL_CALLBACK_HPP

#include "asset_info.hpp"
#include "popup_menu.hpp"

// Forward
class UalDialog;
class UalManager;


// Callback Functors & Data
class UalItemInfo
{
public:
	// For now, it only accepts initialization when being constructed
	// or using the default assignment operator
	UalItemInfo() :
		dialog_( 0 ),
		x_( 0 ),
		y_( 0 ),
		isFolder_( false ),
		folderExtraData_( 0 ),
		next_( 0 )
	{};
	UalItemInfo(
		UalDialog* dialog,
		AssetInfo assetInfo,
		int x,
		int y,
		bool isFolder = false,
		void* folderData = 0 ) :
		dialog_( dialog ),
		assetInfo_( assetInfo ),
		x_( x ),
		y_( y ),
		isFolder_( isFolder ),
		folderExtraData_( folderData ),
		next_( 0 )
	{};
	~UalItemInfo() { if ( next_ ) delete next_; };

	const UalDialog* dialog() const { return dialog_; };
	const AssetInfo& assetInfo() const { return assetInfo_; };
	int x() const { return x_; };
	int y() const { return y_; };
	bool isFolder() const { return isFolder_; };
	const std::string& type() const { return assetInfo_.type(); };
	const std::string& text() const { return assetInfo_.text(); };
	const std::string& longText() const { return assetInfo_.longText(); };
	const std::string& thumbnail() const { return assetInfo_.thumbnail(); };
	const std::string& description() const { return assetInfo_.description(); };

	UalItemInfo* getNext() const { return next_; };

	UalItemInfo operator=( const UalItemInfo& other )
	{
		dialog_ = other.dialog_;
		assetInfo_ = other.assetInfo_;
		x_ = other.x_;
		y_ = other.y_;
		isFolder_ = other.isFolder_;
		folderExtraData_ = other.folderExtraData_;
		next_ = 0;
		return *this;
	};
	bool operator==( const UalItemInfo& other )
	{
		return dialog_ == other.dialog_ &&
			assetInfo_.equalTo( other.assetInfo_ ) &&
			isFolder_ == other.isFolder_ &&
			folderExtraData_ == other.folderExtraData_;
	};
private:
	friend UalDialog;
	friend UalManager;
	UalDialog* dialog_;
	AssetInfo assetInfo_;
	int x_;
	int y_;
	bool isFolder_;
	void* folderExtraData_;
	UalItemInfo* next_;

	void setNext( UalItemInfo* next ) { next_ = next; };
};

// 0 param callbacks/functors

class UalCallback0 : public ReferenceCount
{
public:
	virtual void operator()() = 0;
};

template< class C >
class UalFunctor0 : public UalCallback0
{
public:
	typedef void (C::*Method)();
	UalFunctor0( C* instance, Method method )
		: instance_( instance )
		, method_( method )
	{};
	void operator()()
	{
		(instance_->*method_)();
	}
private:
	C* instance_;
	Method method_;
};


// 1 param callbacks/functors

template< typename P1 >
class UalCallback1 : public ReferenceCount
{
public:
	virtual void operator()( P1 param1 ) = 0;
};

template< class C, typename P1 >
class UalFunctor1 : public UalCallback1< P1 >
{
public:
	typedef void (C::*Method)( P1 );
	UalFunctor1( C* instance, Method method )
		: instance_( instance )
		, method_( method )
	{};
	void operator()( P1 param1 )
	{
		(instance_->*method_)( param1 );
	}
private:
	C* instance_;
	Method method_;
};



// 2 param callbacks/functors

template< typename P1, typename P2 >
class UalCallback2 : public ReferenceCount
{
public:
	virtual void operator()( P1 param1, P2 param2 ) = 0;
};

template< class C, typename P1, typename P2 >
class UalFunctor2 : public UalCallback2< P1, P2 >
{
public:
	typedef void (C::*Method)( P1, P2 );
	UalFunctor2( C* instance, Method method )
		: instance_( instance )
		, method_( method )
	{};
	void operator()( P1 param1, P2 param2 )
	{
		(instance_->*method_)( param1, param2 );
	}
private:
	C* instance_;
	Method method_;
};


// Typedefs
// types for the right-click callback ( use static methods in PopupMenu to add
// items to the vector )
typedef PopupMenu::Item UalPopupMenuItem;
typedef PopupMenu::Items UalPopupMenuItems;

//	drag&drop and click on an item in the list ( item info )
typedef UalCallback1< UalItemInfo* > UalItemCallback;

//	right-click on an item in the list ( item info, map to be filled with data )
typedef UalCallback2< UalItemInfo*, UalPopupMenuItems& > UalStartPopupMenuCallback;
typedef UalCallback2< UalItemInfo*, int > UalEndPopupMenuCallback;

// send errors to app ( error message string )
typedef UalCallback1< const std::string& > UalErrorCallback;

// onSetFocus/KillFocus ( UalDialog ptr and focus state )
typedef UalCallback2< UalDialog*, bool > UalFocusCallback;



#endif // UAL_CALLBACK_HPP
