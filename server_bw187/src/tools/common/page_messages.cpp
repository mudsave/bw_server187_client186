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

#include "shlwapi.h"
#include "atlimage.h"

#include "user_messages.hpp"

#include "python_adapter.hpp"

#include "appmgr/commentary.hpp"

#include "guimanager/gui_manager.hpp"
#include "guimanager/gui_menu.hpp"
#include "guimanager/gui_toolbar.hpp"
#include "guimanager/gui_functor.hpp"
#include "guimanager/gui_functor_option.hpp"

#include "utilities.hpp"
#include "delay_redraw.hpp"

#include "string_utils.hpp"

#include "controls/user_messages.hpp"

#include "page_messages.hpp"

#include "chunk/chunk.hpp"
#include "chunk/chunk_item.hpp"

DECLARE_DEBUG_COMPONENT( 0 )

struct PageMessagesImpl: public SafeReferenceCount
{
	static class PageMessages* s_currPage;

	MsgsImpl* msgsImpl;

	bool inited;
	bool ready;

	CFrameWnd* mainFrame;

	CButton showDateChk;
	CButton showTimeChk;
	CButton showPriorityChk;
	CButton errorMsgsChk;
	CButton warningMsgsChk;
	CButton noticeMsgsChk;
	CButton infoMsgsChk;
	CButton assetMsgsChk;

	int showDate;
	int showTime;
	int showPriority;
	int errorMsgs;
	int warningMsgs;
	int noticeMsgs;
	int infoMsgs;
	int assetMsgs;

    int dateWidth_;                                                             
    int timeWidth_;                                                             
    int priorityWidth_;                                                         
    int msgWidth_;                                                              

	bool widthIncreased_;                                                       

	int lastMessage;

	CImageList imageList;

	std::vector< Message* > tempData;
};

/*static*/ PageMessages* PageMessagesImpl::s_currPage = NULL;

MsgHandler::MsgHandler():
	newMessages_( false ),
	outFile_( NULL ),
	needsRedraw_( false )
{
	DebugFilter::instance().addMessageCallback( this );
}

MsgHandler::~MsgHandler()
{
	DebugFilter::instance().deleteMessageCallback( this );
}

/*static*/ MsgHandler& MsgHandler::instance()
{
	static MsgHandler inst;
	return inst;
}

void MsgHandler::clear()
{
	SimpleMutexHolder smh( mutex_ );
	messages_.clear();
	newMessages_ = true;
}

const bool MsgHandler::updateMessages() 
{
	SimpleMutexHolder smh( mutex_ );
	bool temp = newMessages_;
	newMessages_ = false;
	return temp;
}

const bool MsgHandler::forceRedraw()  
{ 
	SimpleMutexHolder smh( mutex_ ); 
 	bool temp = needsRedraw_; 
 	needsRedraw_ = false; 
	return temp; 
} 

// returns a copy of the array because it's not a thread-safe vector
const std::vector< MessagePtr > MsgHandler::messages() const
{
	SimpleMutexHolder smh( mutex_ );
	return messages_;
}

bool MsgHandler::addAssetErrorMessage( std::string msg, Chunk* chunk, ChunkItem* item, const char* stackTrace )
{
	SimpleMutexHolder smh( mutex_ );

	//Replace any newline characters with spaces
	std::replace( msg.begin(), msg.end(), '\n', ' ' );
	
	assetErrorList_[ msg ]++;

	int messagePriority = MESSAGE_PRIORITY_ASSET;
	const char * priorityStr = messagePrefix( (DebugMessagePriority)messagePriority );
	
	//Get the current date and time
	char dateStr [9];
    char timeStr [9];
    _strdate( dateStr );
    _strtime( timeStr );

	if ( messages_.size() == messages_.capacity() )                             
        messages_.reserve( messages_.size() + 256 );                            
	
	messages_.push_back( new Message( messagePriority, dateStr, timeStr, priorityStr, msg.c_str(), chunk, item, stackTrace ));
	newMessages_ = true;

	return true;
}
	
void MsgHandler::removeAssetErrorMessages() 
{ 
 	SimpleMutexHolder smh( mutex_ ); 
 	
 	assetErrorList_.clear(); 
 	
 	std::vector< MessagePtr >::iterator it = messages_.begin(); 
 	while ( it != messages_.end() ) 
 	{ 
 		if ( (*it)->priority() == MESSAGE_PRIORITY_ASSET ) 
 			it = messages_.erase(it); 
 		else 
 			++it; 
 	} 
 	needsRedraw_ = true; 
 	newMessages_ = true; 
} 

bool MsgHandler::handleMessage( int componentPriority, int messagePriority, const char* format, va_list argPtr )
{
	SimpleMutexHolder smh( mutex_ );

	int filterLevel;
	
	//Anything that is not an error, warning or notice should be filtered with info
	if ((messagePriority != MESSAGE_PRIORITY_ERROR) &&
		(messagePriority != MESSAGE_PRIORITY_WARNING) &&
		(messagePriority != MESSAGE_PRIORITY_NOTICE))
	{
		filterLevel = MESSAGE_PRIORITY_INFO;
	}
	else
	{
		filterLevel = messagePriority;
	}

	const char * priorityStr = messagePrefix( (DebugMessagePriority)messagePriority );

   //Get the current date and time
	char dateStr [9];
    char timeStr [9];
    _strdate( dateStr );
    _strtime( timeStr );
	
	class TempString
	{
	public:
		TempString( int size )
		{
			buffer_ = new char[ size + 1 ];
		}
		~TempString()
		{
			delete[] buffer_;
		}
	    char* buffer_;
	private:
		// unimplemented, hidden methods
		TempString( const TempString& other ) {};
		TempString& operator=( const TempString& other ) { return *this; };
	};

	int size = _vscprintf( format, argPtr );
	
	TempString tempString( size );

    size = vsprintf( tempString.buffer_, format, argPtr );

	if ( tempString.buffer_[ size - 1 ] == '\n' ) // If the message ends with a newline...
		tempString.buffer_[ size - 1 ] = 0;		// remove it
	else
		tempString.buffer_[ size ] = 0;

	if ( messages_.size() == messages_.capacity() )                             
        messages_.reserve( messages_.size() + 256 );                            

	messages_.push_back( new Message( filterLevel, dateStr, timeStr, priorityStr, tempString.buffer_ ));
	newMessages_ = true;

	if (messagePriority == MESSAGE_PRIORITY_ERROR)
	{
		Commentary::instance().addMsg( tempString.buffer_, Commentary::ERROR_LEVEL );
		if (Commentary::instance().logFile()) return false;
	}

	if (outFile_)
	{
		*outFile_ << priorityStr << ": " << tempString.buffer_ << std::endl;
	}

	return false;
}

int MsgHandler::numAssetMsgs( const std::string& msg )
{
	SimpleMutexHolder smh( mutex_ );

	return (assetErrorList_.find( msg ) == assetErrorList_.end()) ? 0 : assetErrorList_[ msg ];
}

// PageMessages

//ID string required for the tearoff tab manager
const std::string PageMessages::contentID = "PageMessagesID";

IMPLEMENT_DYNCREATE(PageMessages, CFormView)

PageMessages::PageMessages():
	CFormView( PageMessages::IDD )
{
	pImpl_ = new PageMessagesImpl;

	pImpl_->mainFrame = NULL;

	pImpl_->inited = false;
	pImpl_->ready = false;

	pImpl_->showDate = true;
	pImpl_->showTime = true;

    pImpl_->dateWidth_ = strlen("Date");                
    pImpl_->timeWidth_ = strlen("Time");                
    pImpl_->priorityWidth_ = strlen("Priority");        
    pImpl_->msgWidth_ = strlen("Message");              

    pImpl_->widthIncreased_ = true;                                             

	pImpl_->lastMessage = 0;

	pImpl_->s_currPage = this;	

	pImpl_->msgsImpl = NULL;

	selected_ = "";
}

PageMessages::~PageMessages()
{
	delete pImpl_->msgsImpl;
}

/*static*/ PageMessages* PageMessages::currPage()
{
	return PageMessagesImpl::s_currPage;
}

void PageMessages::mainFrame( CFrameWnd* mf )
{
	pImpl_->mainFrame = mf;
}

bool PageMessages::showFilter( int priority )
{
	if (msgFilter_.find( priority ) != msgFilter_.end())
	{
		return msgFilter_[ priority ];
	}
	else
	{
		return false;
	}
}

bool PageMessages::setPriority( int priority, bool enabled )
{
	msgFilter_[ priority ] = enabled;

	redrawList();

	return msgFilter_[ priority ];
}

int PageMessages::autoSizeColumn( int i )
{
	DelayRedraw temp( &msgList_ );
	
	msgList_.SetColumnWidth( i, LVSCW_AUTOSIZE );
	int columnWidth = msgList_.GetColumnWidth( i );

	msgList_.SetColumnWidth( i, LVSCW_AUTOSIZE_USEHEADER );
	int headerWidth = msgList_.GetColumnWidth( i ); 

	int maxWidth = max( columnWidth, headerWidth );

	msgList_.SetColumnWidth( i, maxWidth );

	return maxWidth;
}

bool PageMessages::addItem( MessagePtr message )
{
	std::string msg = message->msgStr();
		
	bool createIndex = false;
	bool willShow = true;
	bool subItem = true;
	int index = 0;
	int numInst = MsgHandler::instance().numAssetMsgs( msg );
	if ((numInst > 0) && (message->priority() == MESSAGE_PRIORITY_ASSET))
	{
		if (index_.find( msg ) == index_.end())
		{
			index_[ msg ] = 0;
			createIndex = true;
		}
		else
		{
			Message* itemData = (Message*)( msgList_.GetItemData( index_[ msg ] - 1 ));
			if (itemData)
			{
				const char* plural = numInst > 1 ? "s" : "";
				char buf[256];
				sprintf( buf, "%s (%d instance%s)", itemData->msgStr(), numInst, plural );
				msgList_.SetItemText( index_[ msg ] - 1, MSG_COL, buf );
			}
		}

		if (!expanded_[ msg ])
		{
			willShow = false;
		}

		index = index_[ msg ];
		
	}
	else
	{
		subItem = false;
	}
	
	if (willShow)
	{
		// Advance all indexes after the new entry
		std::map< std::string, int >::iterator it = index_.begin();
		std::map< std::string, int >::iterator end = index_.end();
		for (; it != end; ++it)
		{
			if ((*it).second > index)
			{
				(*it).second++;
			}
		}
		
		const char* indent = subItem ? "    " : "";
		char buf[1024];
		int nIndex = msgList_.InsertItem( index, "" );
		bw_snprintf( buf, sizeof( buf ), "%s%s", indent, message->dateStr() );
		msgList_.SetItemText( nIndex, DATE_COL, buf );
		bw_snprintf( buf, sizeof( buf ), "%s%s", indent, message->timeStr() );
		msgList_.SetItemText( nIndex, TIME_COL, buf );
		bw_snprintf( buf, sizeof( buf ), "%s%s", indent, message->priorityStr() );
		msgList_.SetItemText( nIndex, PRIORITY_COL, buf );
		bw_snprintf( buf, sizeof( buf ), "%s%s", indent, msg.c_str() );
		msgList_.SetItemText( nIndex, MSG_COL, buf );

		msgList_.SetItemData( nIndex, (DWORD_PTR)&*message );
	}

	if (createIndex)
	{
		Message* entry = new Message( MESSAGE_PRIORITY_ASSET, "", "", "", msg.c_str() );

		const char* plural = numInst > 1 ? "s" : "";
		char buf[256];
		sprintf( buf, "%s (%d instance%s)", msg.c_str(), numInst, plural );

		int nIndex = msgList_.InsertItem( index, "", 1 + willShow );
		msgList_.SetItemText( nIndex, DATE_COL, message->dateStr() );
		msgList_.SetItemText( nIndex, TIME_COL, message->timeStr() );
		msgList_.SetItemText( nIndex, PRIORITY_COL, message->priorityStr() );
		msgList_.SetItemText( nIndex, MSG_COL, buf );

		msgList_.SetItemData( nIndex, (DWORD_PTR)&*entry );
		if ( pImpl_->tempData.size() == pImpl_->tempData.capacity() )           
            pImpl_->tempData.reserve( pImpl_->tempData.size() + 256 );          
		pImpl_->tempData.push_back( entry );

		if (msg == selected_)
		{
			msgList_.SetItemState( nIndex, LVIS_SELECTED, LVIS_SELECTED );
		}

		// Advance all indexes after the new entry
		std::map< std::string, int >::iterator it = index_.begin();
		std::map< std::string, int >::iterator end = index_.end();
		for (; it != end; ++it)
		{
			if ((*it).second >= index)
			{
				(*it).second++;
			}
		}
	}

	if ( (int)strlen(message->dateStr()) > pImpl_->dateWidth_ )                 
    {                                                                           
        pImpl_->dateWidth_ = strlen(message->dateStr());                        
        pImpl_->widthIncreased_ = true;                                         
    }                                                                           

    if ( (int)strlen(message->timeStr()) > pImpl_->timeWidth_ )                 
    {                                                                           
        pImpl_->timeWidth_ = strlen(message->timeStr());                        
        pImpl_->widthIncreased_ = true;                                         
    }                                                                           

    if ( (int)strlen(message->priorityStr()) > pImpl_->priorityWidth_)          
    {                                                                           
        pImpl_->priorityWidth_ = strlen(message->priorityStr());                
        pImpl_->widthIncreased_ = true;                                         
    }                                                                           

    if ( (int)strlen(message->msgStr()) > pImpl_->msgWidth_)                    
    {                                                                           
        pImpl_->msgWidth_ = strlen(message->msgStr());                          
        pImpl_->widthIncreased_ = true;                                         
    }                                                                           

	return willShow || createIndex;
}

void PageMessages::redrawList()
{
	int initPos = msgList_.GetScrollPos( SB_VERT );

	{
		DelayRedraw temp( &msgList_ );

		//Delete all the temp message data
		std::vector< Message* >::iterator it =  pImpl_->tempData.begin();
		std::vector< Message* >::iterator end =  pImpl_->tempData.end();
		for (; it != end; ++it)
		{
			delete *it;
		}
		pImpl_->tempData.clear();
	
		//Clear the list
		msgList_.DeleteAllItems();

		index_.clear();
			
		std::vector< MessagePtr > messages = MsgHandler::instance().messages();

		pImpl_->lastMessage = messages.size();

		for (int i=0; i<pImpl_->lastMessage; i++)
		{
			if (showFilter( messages[i]->priority() ))
			{
				addItem( messages[i] );
			}
		}
	}

	DelayRedraw temp( &msgList_ );
	
	updateColumnWidths( true );
		
	for (int i=0; i<initPos; i++)
	{
		::SendMessage(msgList_.m_hWnd, WM_VSCROLL, SB_LINEDOWN, 0);
	}

	msgList_.UpdateWindow();
}

void PageMessages::DoDataExchange(CDataExchange* pDX)
{
	CFormView::DoDataExchange( pDX );

	DDX_Control( pDX, IDC_MSG_LIST, msgList_ );

	msgList_.SetExtendedStyle( LVS_EX_FULLROWSELECT );

	msgList_.InsertColumn( TREE_COL, "", LVCFMT_LEFT, 16 );
	msgList_.InsertColumn( DATE_COL, "Date", LVCFMT_LEFT, 64 );
	msgList_.InsertColumn( TIME_COL, "Time", LVCFMT_LEFT, 64 );
	msgList_.InsertColumn( PRIORITY_COL, "Priority", LVCFMT_LEFT, 128 );
	msgList_.InsertColumn( MSG_COL, "Message", LVCFMT_LEFT, 128 );

	// Create 16 colour image lists
	HIMAGELIST hList = ImageList_Create( 9, 14, ILC_COLOR4 | ILC_MASK, 3, 1);
	pImpl_->imageList.Attach( hList );

	//Load the image from file
	HBITMAP hBitmap = 
        (HBITMAP)::LoadImage
        (
            NULL,
            BWResource::resolveFilename( "resources/data/tree_ctrl.bmp" ).c_str(),
            IMAGE_BITMAP,
            0,
            0,
            LR_LOADFROMFILE
        );

	//Create a CBitmap using the loaded resource
	CBitmap treeCtrlBmp;
	treeCtrlBmp.Attach( hBitmap );

	//Ad the bitmap to the image list
	pImpl_->imageList.Add( &treeCtrlBmp, RGB(255, 0, 255) );

	//Apply this image list to the list control
	msgList_.SetImageList( &pImpl_->imageList, LVSIL_SMALL );

	DDX_Control(pDX, IDC_MSG_SHOW_DATE, pImpl_->showDateChk);
	DDX_Control(pDX, IDC_MSG_SHOW_TIME, pImpl_->showTimeChk);
	DDX_Control(pDX, IDC_MSG_SHOW_PRIORITY, pImpl_->showPriorityChk);

	DDX_Control(pDX, IDC_MSG_ERROR, pImpl_->errorMsgsChk);
	DDX_Control(pDX, IDC_MSG_WARNING, pImpl_->warningMsgsChk);
	DDX_Control(pDX, IDC_MSG_NOTICE, pImpl_->noticeMsgsChk);
	DDX_Control(pDX, IDC_MSG_INFO, pImpl_->infoMsgsChk);
	DDX_Control(pDX, IDC_MSG_ASSETS, pImpl_->assetMsgsChk);

	pImpl_->inited = true;
}

BOOL PageMessages::OnInitDialog()
{
	INIT_AUTO_TOOLTIP();

	return TRUE;
}

BEGIN_MESSAGE_MAP(PageMessages, CFormView)

	ON_WM_CREATE()

	ON_WM_SIZE()

	ON_MESSAGE(WM_UPDATE_CONTROLS, OnUpdateControls)

	ON_COMMAND_RANGE(GUI_COMMAND_START, GUI_COMMAND_END, OnGUIManagerCommand)
	ON_UPDATE_COMMAND_UI_RANGE(GUI_COMMAND_START, GUI_COMMAND_END, OnGUIManagerCommandUpdate)

	ON_BN_CLICKED(IDC_MSG_SHOW_DATE, OnBnClickedErrorsShowDate)
	ON_BN_CLICKED(IDC_MSG_SHOW_TIME, OnBnClickedErrorsShowTime)
	ON_BN_CLICKED(IDC_MSG_SHOW_PRIORITY, OnBnClickedErrorsShowPriority)

	ON_BN_CLICKED(IDC_MSG_ERROR, OnBnClickedErrorsError)
	ON_BN_CLICKED(IDC_MSG_WARNING, OnBnClickedErrorsWarning)
	ON_BN_CLICKED(IDC_MSG_NOTICE, OnBnClickedErrorsNotice)
	ON_BN_CLICKED(IDC_MSG_INFO, OnBnClickedErrorsInfo)

	ON_MESSAGE(WM_SHOW_TOOLTIP, OnShowTooltip)
	ON_MESSAGE(WM_HIDE_TOOLTIP, OnHideTooltip)

	ON_NOTIFY(NM_CLICK, IDC_MSG_LIST, &PageMessages::OnNMClickMsgList)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_MSG_LIST, &PageMessages::OnNMCustomdrawMsgList)

	ON_BN_CLICKED(IDC_MSG_ASSETS, &PageMessages::OnBnClickedMsgAssets)
END_MESSAGE_MAP()

void PageMessages::OnGUIManagerCommand(UINT nID)
{
	pImpl_->s_currPage = this;
	GUI::Manager::instance()->act( nID );
}

void PageMessages::OnGUIManagerCommandUpdate(CCmdUI * cmdUI)
{
	pImpl_->s_currPage = this;
	if( !cmdUI->m_pMenu )
		GUI::Manager::instance()->update( cmdUI->m_nID );
}

afx_msg LRESULT PageMessages::OnShowTooltip(WPARAM wParam, LPARAM lParam)
{
	if (!pImpl_->mainFrame) return 0;
	LPSTR* msg = (LPSTR*)wParam;
	pImpl_->mainFrame->SetMessageText( *msg );
	return 0;
}

afx_msg LRESULT PageMessages::OnHideTooltip(WPARAM wParam, LPARAM lParam)
{
	if (!pImpl_->mainFrame) return 0;
	pImpl_->mainFrame->SetMessageText( "" );
	return 0;
}

// PageMessages message handlers

int PageMessages::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	//We might use this later...
	return 1;
}

void PageMessages::OnSize(UINT nType, int cx, int cy)
{
	if (!pImpl_->inited) return;
	
	if ( cx < 642 )
	{
		Utilities::stretchToBottomRight( this, msgList_, cx, 12, cy, 74 );

		Utilities::moveToBottomLeft( this, pImpl_->showDateChk, cy, 48, 12 );
		Utilities::moveToBottomLeft( this, pImpl_->showTimeChk, cy, 28, 12 );
		Utilities::moveToBottomLeft( this, pImpl_->showPriorityChk, cy, 8, 12 );

		Utilities::moveToBottomLeft( this, pImpl_->errorMsgsChk, cy, 48, 112 );
		Utilities::moveToBottomLeft( this, pImpl_->warningMsgsChk, cy, 28, 112 );
		Utilities::moveToBottomLeft( this, pImpl_->noticeMsgsChk, cy, 8, 112 );

		Utilities::moveToBottomLeft( this, pImpl_->assetMsgsChk, cy, 48, 180 );
		Utilities::moveToBottomLeft( this, pImpl_->infoMsgsChk, cy, 28, 180 );
	}
	else
	{
		Utilities::stretchToBottomRight( this, msgList_, cx, 12, cy, 34 );

		Utilities::moveToBottomLeft( this, pImpl_->showDateChk, cy, 8, 12 );
		Utilities::moveToBottomLeft( this, pImpl_->showTimeChk, cy, 8, 102 );
		Utilities::moveToBottomLeft( this, pImpl_->showPriorityChk, cy, 8, 192 );

		Utilities::moveToBottomLeft( this, pImpl_->errorMsgsChk, cy, 8, 292 );
		Utilities::moveToBottomLeft( this, pImpl_->warningMsgsChk, cy, 8, 362 );
		Utilities::moveToBottomLeft( this, pImpl_->noticeMsgsChk, cy, 8, 432 );

		Utilities::moveToBottomLeft( this, pImpl_->assetMsgsChk, cy, 8, 502 );
		Utilities::moveToBottomLeft( this, pImpl_->infoMsgsChk, cy, 8, 572 );
	}

	updateColumnWidths( true );
			
	CFormView::OnSize( nType, cx, cy );
	
	pImpl_->showDateChk.RedrawWindow();
	pImpl_->showTimeChk.RedrawWindow();
	pImpl_->showPriorityChk.RedrawWindow();
	pImpl_->errorMsgsChk.RedrawWindow();
	pImpl_->warningMsgsChk.RedrawWindow();
	pImpl_->noticeMsgsChk.RedrawWindow();
	pImpl_->infoMsgsChk.RedrawWindow();
	pImpl_->assetMsgsChk.RedrawWindow();
}

void PageMessages::updateColumnWidths( bool all /* = false */ )
{
	if (PythonAdapter::instance())
	{
		int enabled = 0;
		int checked = 0;
		
		PythonAdapter::instance()->ActionScriptUpdate( "actShowDate", enabled, checked );
		pImpl_->showDateChk.SetCheck( checked ? BST_CHECKED : BST_UNCHECKED );
		if (all || ( checked != pImpl_->showDate ))
		{
			if (checked)
			{
				autoSizeColumn( DATE_COL );
			}
			else
			{
				msgList_.SetColumnWidth( DATE_COL, 0 );
			}
			pImpl_->showDate = !!checked;
		}

		PythonAdapter::instance()->ActionScriptUpdate( "actShowTime", enabled, checked );
		pImpl_->showTimeChk.SetCheck( checked ? BST_CHECKED : BST_UNCHECKED );
		if (all || ( checked != pImpl_->showTime ))
		{
			if (checked)
			{
				autoSizeColumn( TIME_COL );
			}
			else
			{
				msgList_.SetColumnWidth( TIME_COL, 0 );
			}
			pImpl_->showTime = !!checked;
		}

		PythonAdapter::instance()->ActionScriptUpdate( "actShowPriority", enabled, checked );
		pImpl_->showPriorityChk.SetCheck( checked ? BST_CHECKED : BST_UNCHECKED );
		if (all || ( checked != pImpl_->showPriority ))
		{
			if (checked)
			{
				autoSizeColumn( PRIORITY_COL );
			}
			else
			{
				msgList_.SetColumnWidth( PRIORITY_COL, 0 );
			}
			pImpl_->showPriority = !!checked;
		}
	}

	if (all)
	{
		autoSizeColumn( TREE_COL );
		autoSizeColumn( MSG_COL );
	}
}

afx_msg LRESULT PageMessages::OnUpdateControls(WPARAM wParam, LPARAM lParam)
{
	if (!pImpl_->ready)
	{
		OnInitDialog();

		pImpl_->ready = true;
	}

	if (PythonAdapter::instance())
	{
		int enabled = 0;
		int checked = 0;

		PythonAdapter::instance()->ActionScriptUpdate( "actErrorMsgs", enabled, checked );
		pImpl_->errorMsgsChk.SetCheck( checked ? BST_CHECKED : BST_UNCHECKED );
		if ( checked != pImpl_->errorMsgs )
		{
			setPriority( MESSAGE_PRIORITY_ERROR, !!checked );
			pImpl_->errorMsgs = !!checked;
		}

		PythonAdapter::instance()->ActionScriptUpdate( "actWarningMsgs", enabled, checked );
		pImpl_->warningMsgsChk.SetCheck( checked ? BST_CHECKED : BST_UNCHECKED );
		if ( checked != pImpl_->warningMsgs )
		{
			setPriority( MESSAGE_PRIORITY_WARNING, !!checked );
			pImpl_->warningMsgs = !!checked;
		}

		PythonAdapter::instance()->ActionScriptUpdate( "actNoticeMsgs", enabled, checked );
		pImpl_->noticeMsgsChk.SetCheck( checked ? BST_CHECKED : BST_UNCHECKED );
		if ( checked != pImpl_->noticeMsgs )
		{
			setPriority( MESSAGE_PRIORITY_NOTICE, !!checked );
			pImpl_->noticeMsgs = !!checked;
		}

		PythonAdapter::instance()->ActionScriptUpdate( "actInfoMsgs", enabled, checked );
		pImpl_->infoMsgsChk.SetCheck( checked ? BST_CHECKED : BST_UNCHECKED );
		if ( checked != pImpl_->infoMsgs )
		{
			setPriority( MESSAGE_PRIORITY_INFO, !!checked );
			pImpl_->infoMsgs = !!checked;
		}	

		PythonAdapter::instance()->ActionScriptUpdate( "actAssetMsgs", enabled, checked );
		pImpl_->assetMsgsChk.SetCheck( checked ? BST_CHECKED : BST_UNCHECKED );
		if ( checked != pImpl_->assetMsgs )
		{
			setPriority( MESSAGE_PRIORITY_ASSET, !!checked );
			pImpl_->assetMsgs = !!checked;
		}	
	}

	if (MsgHandler::instance().updateMessages())
	{
		bool changed = false;
		
		std::vector< MessagePtr > messages = MsgHandler::instance().messages();
	
		int last = messages.size();

		//If for some reason there have been messages removed
		if (last < pImpl_->lastMessage || MsgHandler::instance().forceRedraw())
		{
			redrawList();
			changed = true;
			pImpl_->lastMessage = last;
		}

		for (int i = pImpl_->lastMessage; i<last; i++)
		{
			if (showFilter( messages[i]->priority() ))
			{
				changed = addItem( messages[i] ) || changed;
			}
		}
	
		if (changed && pImpl_->widthIncreased_)
		{
			updateColumnWidths( true );
			pImpl_->widthIncreased_ = false;
		}

		pImpl_->lastMessage = last;
	}

	return 0;
}

void PageMessages::OnBnClickedErrorsShowDate()
{
	if (PythonAdapter::instance())
	{
		PythonAdapter::instance()->ActionScriptExecute("actShowDate");
		updateColumnWidths( true ); 
	}
}

void PageMessages::OnBnClickedErrorsShowTime()
{
	if (PythonAdapter::instance())
	{
		PythonAdapter::instance()->ActionScriptExecute("actShowTime");
		updateColumnWidths( true ); 
	}
}

void PageMessages::OnBnClickedErrorsShowPriority()
{
	if (PythonAdapter::instance())
	{
		PythonAdapter::instance()->ActionScriptExecute("actShowPriority");
		updateColumnWidths( true ); 
	}
}

void PageMessages::OnBnClickedErrorsError()
{
	if (PythonAdapter::instance())
		PythonAdapter::instance()->ActionScriptExecute("actErrorMsgs");
}

void PageMessages::OnBnClickedErrorsWarning()
{
	if (PythonAdapter::instance())
		PythonAdapter::instance()->ActionScriptExecute("actWarningMsgs");
}

void PageMessages::OnBnClickedErrorsNotice()
{
	if (PythonAdapter::instance())
		PythonAdapter::instance()->ActionScriptExecute("actNoticeMsgs");
}

void PageMessages::OnBnClickedErrorsInfo()
{
	if (PythonAdapter::instance())
		PythonAdapter::instance()->ActionScriptExecute("actInfoMsgs");
}
void PageMessages::OnBnClickedMsgAssets()
{
	if (PythonAdapter::instance())
		PythonAdapter::instance()->ActionScriptExecute("actAssetMsgs");
}

// The following methods are for tool specific implementations used by the Messages panel
// to implement click and redraw events.

void PageMessages::msgsImpl( MsgsImpl* msgsImpl )
{
	// Make sure we remove the old implementation if one exists
	delete pImpl_->msgsImpl; 

	pImpl_->msgsImpl = msgsImpl;
}

MsgsImpl* PageMessages::msgsImpl()
{
	return pImpl_->msgsImpl;
}

void PageMessages::OnNMClickMsgList(NMHDR *pNMHDR, LRESULT *pResult)
{
	if (pImpl_->msgsImpl)
	{
		pImpl_->msgsImpl->OnNMClickMsgList( pNMHDR, pResult );
	}
}

void PageMessages::OnNMCustomdrawMsgList(NMHDR *pNMHDR, LRESULT *pResult)
{
	if (pImpl_->msgsImpl)
	{
		pImpl_->msgsImpl->OnNMCustomdrawMsgList( pNMHDR, pResult );
	}
}
