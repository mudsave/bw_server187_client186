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
 *	UalManager: Manages interaction between the App and the Ual Dialog(s)
 */


#ifndef UAL_MANAGER_HPP
#define UAL_MANAGER_HPP

#include "ual_callback.hpp"
#include "guimanager/gui_functor_cpp.hpp"


// Forward
class UalDialog;
class UalDialogFactory;
class UalHistory;


// UalManager
class UalManager : GUI::ActionMaker<UalManager>, // refresh
	GUI::ActionMaker<UalManager,1> // change layout
{
public:
	// singleton instance
	static UalManager* instance();

	// Used by the app to add default search paths
	void addPath( const std::string& path );

	// Used by the app to set the config file path
	// (should be called before creating any ual dialogs)
	void setConfigFile( std::string config );

	// Get the config file path
	const std::string getConfigFile();

	// Used by the app to set the corresponding callbacks
	// Note: Memory is disposed by the UalManager, so set callbacks using new
	// see comments for each one. Also callbacks that receive UalItemInfo pointers
	// must handle NULL UalItemInfo pointer, and may also handle multiple items via
	// the getNext method of the UalItemInfo. UalItemInfo objects are temporary, valid
	// only inside the callback.
	//  - UalFunctor1<YourClass, UalItemInfo*>
	void setItemClickCallback( UalItemCallback* callback ) { itemClickCallback_ = callback; };
	void setItemDblClickCallback( UalItemCallback* callback ) { itemDblClickCallback_ = callback; };
	void setPopupMenuCallbacks( UalStartPopupMenuCallback* callback1, UalEndPopupMenuCallback* callback2 )
		{ startPopupMenuCallback_ = callback1; endPopupMenuCallback_ = callback2; };
	void setStartDragCallback( UalItemCallback* callback ) { startDragCallback_ = callback; };
	void setUpdateDragCallback( UalItemCallback* callback ) { updateDragCallback_ = callback; };
	void setEndDragCallback( UalItemCallback* callback ) { endDragCallback_ = callback; };
	//  - UalFunctor2<YourClass, UalDialog*, bool>
	void setFocusCallback( UalFocusCallback* callback ) { focusCallback_ = callback; };
	//  - UalFunctor1<YourClass, const std::string&>
	void setErrorCallback( UalErrorCallback* callback ) { errorCallback_ = callback; };

	// methods used internally, but can also be used by the app if needed
	const std::string getPath( int i );
	int getNumPaths();

	// Returns the currently active UalDialog
	UalDialog* getActiveDialog();

	// Forces a refresh a single item in all dialogs
	void updateItem( const std::string& longText );

	// Selects the vfolder and item specified
	void showItem( const std::string& vfolder, const std::string& longText );

	// call this method before exiting in order to clean up before exit
	void fini();

private:
	UalManager();
	~UalManager();

	static UalManager* instance_;
	std::vector<std::string> paths_;
	std::vector<UalDialog*> dialogs_;
	typedef std::vector<UalDialog*>::iterator DialogsItr;
	std::string configFile_;
	SmartPointer<UalItemCallback> itemClickCallback_;
	SmartPointer<UalItemCallback> itemDblClickCallback_;
	SmartPointer<UalStartPopupMenuCallback> startPopupMenuCallback_;
	SmartPointer<UalEndPopupMenuCallback> endPopupMenuCallback_;
	SmartPointer<UalItemCallback> startDragCallback_;
	SmartPointer<UalItemCallback> updateDragCallback_;
	SmartPointer<UalItemCallback> endDragCallback_;
	SmartPointer<UalFocusCallback> focusCallback_;
	SmartPointer<UalErrorCallback> errorCallback_;

	friend UalDialog;
	friend UalDialogFactory;
	friend UalHistory;

	void favouritesCallback();
	void historyCallback();

	UalItemCallback* itemClickCallback() { return itemClickCallback_.getObject(); };
	UalItemCallback* itemDblClickCallback() { return itemDblClickCallback_.getObject(); };
	UalStartPopupMenuCallback* startPopupMenuCallback() { return startPopupMenuCallback_.getObject(); };
	UalEndPopupMenuCallback* endPopupMenuCallback() { return endPopupMenuCallback_.getObject(); };
	UalItemCallback* startDragCallback() { return startDragCallback_.getObject(); };
	UalItemCallback* updateDragCallback() { return updateDragCallback_.getObject(); };
	UalItemCallback* endDragCallback() { return endDragCallback_.getObject(); };
	UalFocusCallback* focusCallback() { return focusCallback_.getObject(); };
	UalErrorCallback* errorCallback() { return errorCallback_.getObject(); };

	void registerDialog( UalDialog* dialog );
	void unregisterDialog( UalDialog* dialog );

	bool guiActionRefresh( GUI::ItemPtr item );
	bool guiActionLayout( GUI::ItemPtr item );

	void cancelDrag();
	UalDialog* updateDrag( const UalItemInfo& itemInfo, bool endDrag );
	void copyVFolder( UalDialog* srcUal, UalDialog* dstUal, const UalItemInfo& ii );
};



#endif UAL_MANAGER_HPP
