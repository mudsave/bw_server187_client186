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

#include <string>
#include "appmgr/scripted_module.hpp"

class CSliderCtrl;
class LimitSlider;

class PythonAdapter
{
public:
	PythonAdapter();
	~PythonAdapter();

	static PythonAdapter* instance();

	void reloadUIAdapter();

	bool call(const std::string& fnName);

	bool ActionScriptExecute(const std::string& functionName);
	bool ActionScriptUpdate(const std::string& actionName, int & enabled, int & checked);

	void onPageControlTabSelect( const std::string& fnPrefix, const std::string& theTabName );

	void onBrowserObjectItemSelect( const std::string& tabName, 
							const std::string& itemName );
	void onBrowserObjectItemAdd();

	void onBrowserTextureItemSelect( const std::string& itemName );
	
	void onSliderAdjust( const std::string& name, float pos, float min, float max );
	void sliderUpdate( CSliderCtrl* slider, const std::string& sliderName );

	void onLimitSliderAdjust( const std::string& name, float pos, float min, float max );
	void limitSliderUpdate( LimitSlider* control, const std::string& controlName );

	void onListItemSelect( const std::string& name, int index );
	void onListSelectUpdate( CListBox* listBox, const std::string& listBoxName );

	void selectFilterChange( const std::string& value );
	void selectFilterUpdate( CComboBox* comboList );

	void coordFilterChange( const std::string& value );
	void coordFilterUpdate( CComboBox* comboList );

	void projectLock(const std::string& commitMessage);
	void commitChanges(const std::string& commitMessage, bool keepLocks);
	void discardChanges(const std::string& commitMessage, bool keepLocks);
	void updateSpace();
	void calculateMap();
	void exportMap();

	bool canSavePrefab();
	void saveSelectionPrefab( std::string fileName );
	std::vector<std::string> channelTextureUpdate();

	void contextMenuGetItems( const std::string& type, const std::string path,
		std::map<int,std::string>& items );
	void contextMenuHandleResult( const std::string& type, const std::string path, int result );

private:
	void fillFilterKeys( CComboBox* comboList );
	void fillCoordFilterKeys( CComboBox* comboList );

	PyObject* pScriptObject_;
    bool proActive_;

	static PythonAdapter* instance_;
};