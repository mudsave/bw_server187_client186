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
#include "resource.h"
#include "cstdmf/concurrency.hpp"

#include "guitabs/guitabs_content.hpp"

#include "controls/auto_tooltip.hpp"

class Chunk;
class ChunkItem;

class Message: public SafeReferenceCount
{
public:
	Message( int priority, const char* dateStr, const char* timeStr, const char* priorityStr, const char* msgStr,
		Chunk* chunk = NULL, ChunkItem* item = NULL, const char* stackTrace = NULL ):
	priority_(priority),
	chunk_(chunk),
	item_(item),
	stackTrace_(NULL)
	{
		dateStr_ = new char[ strlen(dateStr) + 1 ];
		strcpy( dateStr_, dateStr );

		timeStr_ = new char[ strlen(timeStr) + 1 ];
		strcpy( timeStr_, timeStr );
		
		priorityStr_ = new char[ strlen(priorityStr) + 1 ];
		strcpy( priorityStr_, priorityStr );
		
		msgStr_ = new char[ strlen(msgStr) + 1 ];
		strcpy( msgStr_, msgStr );

		if (stackTrace)
		{
			stackTrace_ = new char[ strlen(stackTrace) + 1 ];
			strcpy( stackTrace_, stackTrace );
		}
	}
	~Message()
	{
		delete [] dateStr_;
		delete [] timeStr_;
		delete [] priorityStr_;
		delete [] msgStr_;
		delete [] stackTrace_;
	}
	int priority() { return priority_; }
	const char* dateStr() { return dateStr_; }
	const char* timeStr() { return timeStr_; }
	const char* priorityStr() { return priorityStr_; }
	const char* msgStr() { return msgStr_; }
	Chunk* chunk() { return chunk_; }
	ChunkItem* item() { return item_; }
	const char* stackTrace() { return stackTrace_; }
private:	
	int priority_;
	char* dateStr_;
	char* timeStr_;
	char* priorityStr_;
	char* msgStr_;
	Chunk* chunk_;
	ChunkItem* item_;
	char* stackTrace_;
};

typedef SmartPointer< Message > MessagePtr;

class MsgHandler: public DebugMessageCallback
{
public:
	MsgHandler();
	virtual ~MsgHandler();

	static MsgHandler& instance();

	void clear();

	void logFile( std::ostream* outFile ) { outFile_ = outFile; }

	const bool updateMessages();

	const bool forceRedraw(); 

	// returns a copy of the array because it's not a thread-safe vector
	const std::vector< MessagePtr > messages() const;

	bool addAssetErrorMessage( std::string msg, Chunk* chunk = NULL, ChunkItem* item = NULL, const char * stacktrace = NULL );

	void removeAssetErrorMessages(); 
		
	bool handleMessage( int componentPriority, int messagePriority, const char* format, va_list argPtr );

	int numAssetMsgs( const std::string& msg );

private:
	mutable SimpleMutex mutex_;
	std::vector< MessagePtr > messages_;
	bool newMessages_;
	int lastMessage_;
	std::ostream* outFile_;
	std::map< std::string, int > assetErrorList_;
	bool needsRedraw_; 
};

class MsgsImpl
{
public:
	virtual afx_msg void OnNMClickMsgList(NMHDR *pNMHDR, LRESULT *pResult) = 0;
	virtual afx_msg void OnNMCustomdrawMsgList(NMHDR *pNMHDR, LRESULT *pResult) = 0;
};

// PageMessages

class PageMessages: public CFormView, public GUITABS::Content
{
	DECLARE_DYNCREATE(PageMessages)

	IMPLEMENT_BASIC_CONTENT( "Messages", "BigWorld Messages", 285, 448, NULL )

	DECLARE_AUTO_TOOLTIP( PageMessages, CFormView )

public:
	PageMessages();
	virtual ~PageMessages();

	void mainFrame( CFrameWnd* mf );

	bool showFilter( int priority );
	bool setPriority( int priority, bool enabled );
	bool addItem( MessagePtr message );
	void redrawList();

	static PageMessages* currPage();

	void msgsImpl( MsgsImpl* msgsImpl );
	MsgsImpl* msgsImpl();

// Dialog Data
	enum { IDD = IDD_ERRORS };

protected:
	
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
	
public:
	
	virtual BOOL OnInitDialog();
	
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg LRESULT OnUpdateControls(WPARAM wParam, LPARAM lParam);

private:

	SmartPointer< struct PageMessagesImpl > pImpl_;

	void OnGUIManagerCommand(UINT nID);
	void OnGUIManagerCommandUpdate(CCmdUI * cmdUI);
	afx_msg LRESULT OnShowTooltip(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnHideTooltip(WPARAM wParam, LPARAM lParam);

	void OnSize( UINT nType, int cx, int cy );

	void showDateTime( bool show );
	void showPriority( bool show );

	int autoSizeColumn( int i );
	void updateColumnWidths( bool all = false );

	static const int TREE_COL = 0;
	static const int DATE_COL = 1;
	static const int TIME_COL = 2;
	static const int PRIORITY_COL = 3;
	static const int MSG_COL = 4;

public:
	CListCtrl msgList_;

	std::map< int, bool > msgFilter_;
	std::map< std::string, int > index_;
	std::map< std::string, bool > expanded_;

	std::string selected_;

	afx_msg void OnBnClickedErrorsShowDate();
	afx_msg void OnBnClickedErrorsShowTime();
	afx_msg void OnBnClickedErrorsShowPriority();
	
	afx_msg void OnBnClickedErrorsError();
	afx_msg void OnBnClickedErrorsWarning();
	afx_msg void OnBnClickedErrorsNotice();
	afx_msg void OnBnClickedErrorsInfo();
	afx_msg void OnBnClickedMsgAssets();

	afx_msg void OnNMClickMsgList(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnNMCustomdrawMsgList(NMHDR *pNMHDR, LRESULT *pResult);
};

IMPLEMENT_BASIC_CONTENT_FACTORY( PageMessages )
