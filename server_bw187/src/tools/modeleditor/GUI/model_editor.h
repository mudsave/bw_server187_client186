// ModelEditor.h : main header file for the ModelEditor application
//
#pragma once

#ifndef __AFXWIN_H__
#error include 'stdafx.h' before including this file for PCH
#endif

#include "appmgr/app.hpp"

#include "resource.h"       // main symbols

#include "me_shell.hpp"
#include "me_app.hpp"
#include "mru.hpp"
#include "python_adapter.hpp"

#include "guimanager/gui_manager.hpp"
#include "guimanager/gui_functor_cpp.hpp"
#include "guimanager/gui_functor_option.hpp"

class UalItemInfo;
class PageLights;

class CModelEditorApp :
	public CWinApp,
	GUI::ActionMaker<CModelEditorApp,0>,
	GUI::ActionMaker<CModelEditorApp,1>,
	GUI::ActionMaker<CModelEditorApp,2>,
	GUI::ActionMaker<CModelEditorApp,3>,
	GUI::ActionMaker<CModelEditorApp,4>,
	GUI::ActionMaker<CModelEditorApp,5>,
	GUI::OptionMap
{
public:
	static DogWatch s_updateWatch;
	
	CModelEditorApp();
	~CModelEditorApp();

	void fini();

	static UINT loadErrorMsg( LPVOID lpvParam );

	static CModelEditorApp & instance() { return *s_instance_; }
	CWnd* mainWnd() { return m_pMainWnd; }
	App* mfApp() { return mfApp_; }
	bool initDone() { return initDone_; };

	bool loadFile( UalItemInfo* item );
		
	//Interface for python calls
	void loadModel( const char* modelName );
	void addModel( const char* modelName );
	void loadLights( const char* lightName );
		
	//The menu commands
	void OnFileOpen();
	void OnFileAdd();
	void OnFileReloadTextures();
	void OnFileRegenBoundingBox();
	void OnAppPrefs();
		
	bool OnAppAbout( GUI::ItemPtr item );
	bool openHelpFile( const std::string& name, const std::string& defaultFile );
	bool OnToolsReferenceGuide( GUI::ItemPtr item );
	bool OnContentCreation( GUI::ItemPtr item );
	bool OnShortcuts( GUI::ItemPtr item );
	
	void updateRecentList( const std::string& kind );

// Overrides
public:
	virtual BOOL InitInstance();
	virtual BOOL OnIdle(LONG lCount);
	virtual int ExitInstance(); 

private:
	static CModelEditorApp * s_instance_;

	App* mfApp_;
	bool initDone_;

	MeShell*	meShell_;
	MeApp*		meApp_;

	PageLights* lightPage_;

	PythonAdapter* pythonAdapter_;

	std::string modelToLoad_;
	std::string modelToAdd_;

	BOOL parseCommandLineMF();
		
	bool recent_models( GUI::ItemPtr item );
	bool recent_lights( GUI::ItemPtr item );

	virtual std::string get( const std::string& key ) const;
	virtual bool exist( const std::string& key ) const;
	virtual void set( const std::string& key, const std::string& value );

	DECLARE_MESSAGE_MAP()
};

extern CModelEditorApp theApp;