/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// implementation file
//

#include "pch.hpp"
#include "bigbang.h"
#include "big_bang.hpp"
#include "appmgr/options.hpp"
#include "guimanager/gui_manager.hpp"
#include "controls/resource_file_dialog.hpp"
#include "page_terrain_texture.hpp"
#include "python_adapter.hpp"
#include "user_messages.hpp"
#include <atlimage.h>
#include "panel_manager.hpp"
#include "foldersetter.hpp"
#include <algorithm>
#include "resmgr/auto_config.hpp"

BEGIN_MESSAGE_MAP(CRCStatic, CStatic)
	ON_WM_RBUTTONDOWN()
END_MESSAGE_MAP()

static PageTerrainTexture* instance = NULL;
static AutoConfigString s_notFoundBmp( "system/notFoundBmp" );

void CRCStatic::OnRButtonDown(UINT nFlags, CPoint point)
{
	instance->onTextureChannelRightClick( this );
}
// PageTerrainTexture


// GUITABS content ID ( declared by the IMPLEMENT_BASIC_CONTENT macro )
const std::string PageTerrainTexture::contentID = "PageTerrainTexture";

PageTerrainTexture::PageTerrainTexture()
	: PageTerrainBase(PageTerrainTexture::IDD),
	pageReady_( false )
{
	instance = this;
    sizeEdit_.SetNumericType(controls::EditNumeric::ENT_INTEGER);
	strengthEdit_.SetNumericType(controls::EditNumeric::ENT_INTEGER);
	currentChannel_ = -1;
	textureChannels_[ 0 ] = &mTextureChannel1;
	textureChannels_[ 1 ] = &mTextureChannel2;
	textureChannels_[ 2 ] = &mTextureChannel3;
	textureChannels_[ 3 ] = &mTextureChannel4;
}

PageTerrainTexture::~PageTerrainTexture()
{
	if( monoBmp_ )
		waterMark_.SelectObject( monoBmp_ );
}

void PageTerrainTexture::DoDataExchange(CDataExchange* pDX)
{
	PageTerrainBase::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TERRAIN_SIZE_SLIDER, sizeSlider_);
	DDX_Control(pDX, IDC_TERRAIN_SIZE_EDIT, sizeEdit_);
	DDX_Control(pDX, IDC_TERRAIN_STRENGTH_SLIDER, strengthSlider_);
	DDX_Control(pDX, IDC_TERRAIN_STRENGTH_EDIT, strengthEdit_);
	DDX_Control(pDX, IDC_SIZERANGE, mSizeRange);
	DDX_Control(pDX, IDC_STRENGTHRANGE, mStrengthRange);
	DDX_Control(pDX, IDC_TEXTURECHANNEL1, mTextureChannel1);
	DDX_Control(pDX, IDC_TEXTURECHANNEL2, mTextureChannel2);
	DDX_Control(pDX, IDC_TEXTURECHANNEL3, mTextureChannel3);
	DDX_Control(pDX, IDC_TEXTURECHANNEL4, mTextureChannel4);
	DDX_Control(pDX, IDC_TEXTURESINCHUNK, mTexturesInChunk);
}


BOOL PageTerrainTexture::OnInitDialog()
{
	PageTerrainBase::OnInitDialog();
	LoadWaterMark();

	const char* exts[] = { "bmp", "tga", "jpg", "dds" };
	for( int i = 0; i < TEXTURE_CHANNEL_NUM; ++i )
		for( int j = 0; j < sizeof( exts ) / sizeof( *exts ); ++j )
			UalDropManager::instance().add( new UalDropFunctor<PageTerrainTexture>
				( textureChannels_[ i ], exts[ j ], this, &PageTerrainTexture::OnUalDrop ) );
	sizeSlider_.setRangeLimit( Options::getOptionFloat( "terrain/texture/minsizelimit", 1 ),
		Options::getOptionFloat( "terrain/texture/maxsizelimit", 4000 ) );
	sizeSlider_.setRange( Options::getOptionFloat( "terrain/texture/minsize", 1 ),
		Options::getOptionFloat( "terrain/texture/maxsize", 800 ) );
	sizeSlider_.setValue( Options::getOptionFloat( "terrain/texture/size", 1 ) );

	strengthSlider_.setRangeLimit( Options::getOptionFloat( "terrain/texture/minstrengthlimit", 1 ),
		Options::getOptionFloat( "terrain/texture/maxstrengthlimit", 4000 ) );
	strengthSlider_.setRange( Options::getOptionFloat( "terrain/texture/minstrength", 1 ),
		Options::getOptionFloat( "terrain/texture/maxstrength", 800 ) );
	strengthSlider_.setValue( Options::getOptionFloat( "terrain/texture/strength", 1 ) );

	std::vector<CString> paths;
	for (int i = 0; ; i++)
	{
		CString path = BWResource::getPath(i).c_str();
		if (path.IsEmpty())
			break;
		if (path.Find("worldeditor") != -1)
			continue;
		paths.push_back(path);
	}

	for( int i = 0; i < TEXTURE_CHANNEL_NUM; ++i )
	{
		char name[] = "terrain/texture/channel0/texture";
		name[ strlen( "terrain/texture/channel0" ) - 1 ] += i;
		std::vector<std::string> textures;
		Options::pRoot()->readStrings( name, textures, DS_TrimWhitespace );
		for( std::vector<std::string>::iterator iter = textures.begin(); iter != textures.end(); ++iter )
		{
			if( BWResource::fileExists( *iter ) )
			{
				channelTextureFileName_[ i ] = *iter;
				textureChannels_[ i ]->InvalidateRect( NULL );
				break;
			}
		}
	}
	OnUpdateControls(0, 0);
	updateSliderEdits();
	selectChannel( 0 );

	mSizeRange.SetWindowPos( NULL, 0, 0, 16, 16, SWP_NOMOVE | SWP_NOZORDER );
	mStrengthRange.SetWindowPos( NULL, 0, 0, 16, 16, SWP_NOMOVE | SWP_NOZORDER );
	mSizeRange.setBitmapID( IDB_RANGESLIDER, IDB_RANGESLIDER );
	mStrengthRange.setBitmapID( IDB_RANGESLIDER, IDB_RANGESLIDER );
	pageReady_ = true;

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}


BEGIN_MESSAGE_MAP(PageTerrainTexture, PageTerrainBase)
	ON_WM_SETFOCUS()
	ON_WM_SIZE()
	ON_WM_HSCROLL()
	ON_MESSAGE (WM_ACTIVATE_TOOL, OnActivateTool)
	ON_MESSAGE (WM_UPDATE_CONTROLS, OnUpdateControls)
	ON_EN_CHANGE(IDC_TERRAIN_SIZE_EDIT, OnEnChangeTerrainSizeEdit)
	ON_EN_CHANGE(IDC_TERRAIN_STRENGTH_EDIT, OnEnChangeTerrainStrengthEdit)
	ON_BN_CLICKED(IDC_SIZERANGE, OnBnClickedSizerange)
	ON_BN_CLICKED(IDC_STRENGTHRANGE, OnBnClickedStrengthrange)
	ON_WM_DRAWITEM()
	ON_STN_CLICKED(IDC_TEXTURECHANNEL1, OnStnClickedTexturechannel1)
	ON_STN_CLICKED(IDC_TEXTURECHANNEL2, OnStnClickedTexturechannel2)
	ON_STN_CLICKED(IDC_TEXTURECHANNEL3, OnStnClickedTexturechannel3)
	ON_STN_CLICKED(IDC_TEXTURECHANNEL4, OnStnClickedTexturechannel4)
END_MESSAGE_MAP()


// PageTerrainTexture message handlers

void PageTerrainTexture::OnSetFocus( CWnd* pOldWnd )
{
	//if ( BigBang2App::instance().panelsReady() )
	//{
	//	BigBang::instance().updateUIToolMode( PageTerrainTexture::contentID );
	//	OnActivateTool( 0, 0 );
	//}
}

LRESULT PageTerrainTexture::OnActivateTool(WPARAM wParam, LPARAM lParam)
{
	if (PythonAdapter::instance())
		PythonAdapter::instance()->onPageControlTabSelect("pgcTerrain", "TerrainTextures");

	return 0;
}

afx_msg void PageTerrainTexture::OnSize( UINT nType, int cx, int cy )
{
	PageTerrainBase::OnSize(nType, cx, cy);

	if ( !pageReady_ )
		return;

	// move the controls as appropriate
	// stretch the file selector to be long enough to fit into the window
	CRect thisRect;
	GetClientRect(thisRect);

	static const int MARGIN = 10;
	int newWidth = std::max(1, thisRect.Width() - MARGIN * 2 );

	CRect sliderRect, buttonRect;
	sizeSlider_.GetWindowRect(sliderRect);
	mSizeRange.GetWindowRect(buttonRect);
	ScreenToClient( &sliderRect );
	ScreenToClient( &buttonRect );

	mSizeRange.SetWindowPos(NULL, thisRect.Width() - buttonRect.Width(), buttonRect.top,
		0, 0, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE);

	sizeSlider_.SetWindowPos(NULL, 0, 0,
		newWidth - buttonRect.Width() - sliderRect.left, sliderRect.Height(),
		SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);

	strengthSlider_.GetWindowRect(sliderRect);
	mStrengthRange.GetWindowRect(buttonRect);
	ScreenToClient( &sliderRect );
	ScreenToClient( &buttonRect );

	mStrengthRange.SetWindowPos(NULL, thisRect.Width() - buttonRect.Width(), buttonRect.top,
		0, 0, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE);

	strengthSlider_.SetWindowPos(NULL, 0, 0,
		newWidth - buttonRect.Width() - sliderRect.left, sliderRect.Height(),
		SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);


	CRect rect;
	textureChannels_[ 0 ]->GetWindowRect( &rect );
	ScreenToClient( &rect );

	for( int i = 0; i < TEXTURE_CHANNEL_NUM; ++i )
	{
		textureChannels_[ i ]->SetWindowPos( NULL,
			rect.left + 140 * ( i % 2 ),
			rect.top + 140 * ( i / 2 ),
			136, 136, SWP_NOREPOSITION );
		rect.UnionRect( &rect,
			&CRect( CPoint( rect.left + 140 * ( i % 2 ), rect.top + 140 * ( i / 2 ) ),
				CSize( 140, 140 ) ) );
	}
	rect.InflateRect( 8, 8 );
	rect.top -= 12;
	mTexturesInChunk.MoveWindow( &rect );
}


void PageTerrainTexture::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	// this captures all the scroll events for the page
	// upadate all the slider buddy windows
	updateSliderEdits();

	Options::setOptionFloat( "terrain/texture/size", sizeSlider_.getValue() );
	Options::setOptionFloat( "terrain/texture/strength", strengthSlider_.getValue() );

	PageTerrainBase::OnHScroll(nSBCode, nPos, pScrollBar);
}

void PageTerrainTexture::updateSliderEdits()
{
	int new_val = (int)sizeSlider_.getValue();
	int old_val = (int)sizeEdit_.GetIntegerValue();
	if (GetFocus() != &sizeEdit_ && new_val != old_val)
	{
		sizeEdit_.SetIntegerValue( new_val );
	}
	new_val = (int)strengthSlider_.getValue();
	old_val = (int)strengthEdit_.GetIntegerValue();
	if (GetFocus() != &strengthEdit_ && new_val != old_val)
	{
		strengthEdit_.SetIntegerValue( new_val );
	}
}

afx_msg LRESULT PageTerrainTexture::OnUpdateControls(WPARAM wParam, LPARAM lParam)
{
	MF_ASSERT(PythonAdapter::instance());
	std::vector<std::string> channelTextures = PythonAdapter::instance()->channelTextureUpdate();
	if( channelTextures_ != channelTextures )
	{
		channelTextures_ = channelTextures;
		for( int i = 0; i < TEXTURE_CHANNEL_NUM; ++i )
			textureChannels_[ i ]->InvalidateRect( NULL );
		if( currentChannel_ != -1 )
			selectChannel( currentChannel_ );
	}

	sizeSlider_.setValue( Options::getOptionFloat( "terrain/texture/size", 1 ) );
	strengthSlider_.setValue( Options::getOptionFloat( "terrain/texture/strength", 1 ) );

	updateSliderEdits();

	Options::setOptionFloat( "terrain/texture/minsize", sizeSlider_.getMinRange() );
	Options::setOptionFloat( "terrain/texture/maxsize", sizeSlider_.getMaxRange() );

	Options::setOptionFloat( "terrain/texture/minstrength", strengthSlider_.getMinRange() );
	Options::setOptionFloat( "terrain/texture/maxstrength", strengthSlider_.getMaxRange() );

	return 0;
}

void PageTerrainTexture::OnBnClickedTerrainAlphaChannel1()
{
	if (PythonAdapter::instance())
		PythonAdapter::instance()->ActionScriptExecute("actTerrainTextureChannel1");
}

void PageTerrainTexture::OnBnClickedTerrainAlphaChannel2()
{
	if (PythonAdapter::instance())
		PythonAdapter::instance()->ActionScriptExecute("actTerrainTextureChannel2");
}

void PageTerrainTexture::OnBnClickedTerrainAlphaChannel3()
{
	if (PythonAdapter::instance())
		PythonAdapter::instance()->ActionScriptExecute("actTerrainTextureChannel3");
}

void PageTerrainTexture::OnBnClickedTerrainAlphaChannel4()
{
	if (PythonAdapter::instance())
		PythonAdapter::instance()->ActionScriptExecute("actTerrainTextureChannel4");
}

void PageTerrainTexture::OnEnChangeTerrainSizeEdit()
{
	sizeSlider_.setValue( static_cast<float>(sizeEdit_.GetIntegerValue()) );
	Options::setOptionFloat( "terrain/texture/size", sizeSlider_.getValue() );
}

void PageTerrainTexture::OnEnChangeTerrainStrengthEdit()
{
	strengthSlider_.setValue( static_cast<float>(strengthEdit_.GetIntegerValue()) );
	Options::setOptionFloat( "terrain/texture/strength", strengthSlider_.getValue() );
}

void PageTerrainTexture::OnBnClickedSizerange()
{
	sizeSlider_.beginEdit();
}

void PageTerrainTexture::OnBnClickedStrengthrange()
{
	strengthSlider_.beginEdit();
}

bool PageTerrainTexture::OnUalDrop( UalItemInfo* ii )
{
	if( !ii->isFolder() )
	{
		POINT cursor;
		GetCursorPos( &cursor );
		CStatic* target = DYNAMIC_DOWNCAST( CStatic, WindowFromPoint( cursor ) );
		for( int i = 0; i < TEXTURE_CHANNEL_NUM; ++i )
			if( target == textureChannels_[ i ] )
			{
				channelTextureFileName_[ i ] = BWResource::dissolveFilename( ii->longText() );
				target->InvalidateRect( NULL );
				selectChannel( i );
				return true;
			}
	}
	return false;
}

std::string PageTerrainTexture::getThumbFileName( std::string filename )
{
	filename = BWResource::resolveFilename( filename );

	if( !BWResource::fileExists( filename ) )
		filename = BWResource::resolveFilename( s_notFoundBmp );

	static const char* THUMB_NAIL_SUFFIX = ".thumbnail.bmp";
	static const DWORD MIN_THUMBNAIL_FILE_SIZE = 0;
	std::string thumbFileName = filename + THUMB_NAIL_SUFFIX;
	if( BWResource::fileExists( thumbFileName ) )
	{
		uint64 imgTime = BWResource::modifiedTime( filename );
		uint64 imgThumbTime = BWResource::modifiedTime( thumbFileName );
		if( imgTime < imgThumbTime )
			return thumbFileName;
	}

	bool isDDS = filename.rfind( '.' ) != filename.npos &&
		filename.substr( filename.rfind( '.' ) + 1 ) == "dds";
	bool isTGA = filename.rfind( '.' ) != filename.npos &&
		filename.substr( filename.rfind( '.' ) + 1 ) == "tga";
	if( !isDDS && !isTGA )
	{
		WIN32_FIND_DATA findData;
		HANDLE find = FindFirstFile( filename.c_str(), &findData );
		FindClose( find );
		if( find != INVALID_HANDLE_VALUE && findData.nFileSizeLow <= MIN_THUMBNAIL_FILE_SIZE )
			return filename;
	}

	DX::Texture* pTexture;

	//load dds into a texture
	HRESULT hr = D3DXCreateTextureFromFileEx( Moo::rc().device(), filename.c_str(),
		128, 128, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, D3DX_DEFAULT,
		D3DX_DEFAULT, 0xFF000000, NULL, NULL, &pTexture );

	if( SUCCEEDED( hr ) )
	{
		D3DXSaveTextureToFile( thumbFileName.c_str(), D3DXIFF_BMP, pTexture, NULL );
		pTexture->Release();
	}
	return BWResource::dissolveFilename( thumbFileName );
}

void PageTerrainTexture::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	std::string filename;
	for( int i = 0; i < TEXTURE_CHANNEL_NUM; ++i )
	{
		if( lpDrawItemStruct->hwndItem == textureChannels_[ i ]->m_hWnd )
		{
			CDC* dc = CDC::FromHandle( lpDrawItemStruct->hDC );
			CRect rect;
			::GetClientRect( lpDrawItemStruct->hwndItem, &rect );
			dc->FillSolidRect( &rect, currentChannel_ == i ? GetSysColor( COLOR_HIGHLIGHT )
				: RGB( 0xff, 0xff, 0xff ) );
			rect.DeflateRect(4, 4);
			std::string filename;
			if( currentChannel_ == i )
				filename = BWResource::resolveFilename( getThumbFileName(
					channelTextureFileName_[ i ].empty()	?	channelTextures_[ i ] :
																channelTextureFileName_[ i ] ) );
			else
				filename = BWResource::resolveFilename( getThumbFileName( channelTextures_[ i ] ) );
			CImage image;
			if( SUCCEEDED( image.Load( filename.c_str() ) ) )
				image.BitBlt( lpDrawItemStruct->hDC, rect.TopLeft() );
			else
				dc->FillSolidRect( &rect, 0x0 );
			if( !channelTextures_.empty() && !channelTextureFileName_[ i ].empty() &&
				channelTextureFileName_[ i ] != channelTextures_[ i ] )
			{
				BLENDFUNCTION pixelblend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
				dc->AlphaBlend( rect.left, rect.top, rect.Width(), rect.Height(), &waterMark_,
					0, 0, waterMarkSize_.cx, waterMarkSize_.cy, pixelblend );
			}
			dc->SetBkMode( TRANSPARENT );
			char s[] = { '1' + i, 0 };
			dc->SetTextColor( 0 );
			dc->DrawText( s, &rect, 0 );
			dc->SetTextColor( 0xffffff );
			rect.DeflateRect( 1, 1 );
			dc->DrawText( s, &rect, 0 );
			return;
		}
	}
	PageTerrainBase::OnDrawItem( nIDCtl, lpDrawItemStruct );
}

void PageTerrainTexture::OnStnClickedTexturechannel1()
{
	if( currentChannel_ != 0 )
		selectChannel( 0 );
}

void PageTerrainTexture::OnStnClickedTexturechannel2()
{
	if( currentChannel_ != 1 )
		selectChannel( 1 );
}

void PageTerrainTexture::OnStnClickedTexturechannel3()
{
	if( currentChannel_ != 2 )
		selectChannel( 2 );
}

void PageTerrainTexture::OnStnClickedTexturechannel4()
{
	if( currentChannel_ != 3 )
		selectChannel( 3 );
}

void PageTerrainTexture::pickTerrainTexture()
{
	MF_ASSERT(PythonAdapter::instance());
	std::vector<std::string> channelTextures = PythonAdapter::instance()->channelTextureUpdate();
	channelTextures_ = channelTextures;
	if( currentChannel_ != -1 )
	{
		channelTextureFileName_[ currentChannel_ ] = channelTextures_[ currentChannel_ ];
		textureChannels_[ currentChannel_ ]->InvalidateRect( NULL );
		selectChannel( currentChannel_ );
	}
}

void PageTerrainTexture::selectChannel( int index )
{
	static const unsigned int MAX_TEXTURE_SAVED_PER_CHANNEL = 20;
	if( currentChannel_ != -1 && currentChannel_ != index )
		textureChannels_[ currentChannel_ ]->InvalidateRect( NULL );
	currentChannel_ = index;
	textureChannels_[ currentChannel_ ]->InvalidateRect( NULL );
	if( PythonAdapter::instance() )
	{
		char function[] = "actTerrainTextureChannel1";
		function[ strlen( function ) - 1 ] += index;
		PythonAdapter::instance()->ActionScriptExecute( function );
		if( !channelTextureFileName_[ index ].empty() )
			PythonAdapter::instance()->onBrowserTextureItemSelect( channelTextureFileName_[ index ] );
		else if( !channelTextures_[ index ].empty() )
			PythonAdapter::instance()->onBrowserTextureItemSelect( channelTextures_[ index ] );
	}
	char name[] = "terrain/texture/channel0/texture";
	name[ strlen( "terrain/texture/channel0" ) - 1 ] += index;
	std::vector<std::string> textures;
	Options::pRoot()->readStrings( name, textures, DS_TrimWhitespace );
	if( std::find( textures.begin(), textures.end(), channelTextureFileName_[ index ] )
		== textures.end() )
	{
		if( textures.size() >= MAX_TEXTURE_SAVED_PER_CHANNEL )
			textures.pop_back();
		textures.insert( textures.begin(), channelTextureFileName_[ index ] );
		Options::pRoot()->deleteSections( name );
		Options::pRoot()->writeStrings( name, textures );
	}

	// Make sure the texture is loaded from disk here instead of when starting
	// to paint, because it can take some time to load the texture and we don't
	// want the painting to have a long pause before starting.
	CWaitCursor wait;
	Moo::TextureManager::instance()->get( channelTextureFileName_[ index ] );
}

void PageTerrainTexture::changeTextureTerrain( int direction )
{
	if( currentChannel_ != -1 )
	{
		char name[] = "terrain/texture/channel0/texture";
		name[ strlen( "terrain/texture/channel0" ) - 1 ] += currentChannel_;
		std::vector<std::string> textures;
		Options::pRoot()->readStrings( name, textures, DS_TrimWhitespace );
		std::vector<std::string>::iterator iter = std::find( textures.begin(), textures.end(),
			channelTextureFileName_[ currentChannel_ ] );
		if( direction > 0 )
		{
			++iter;
			if( iter == textures.end() )
				iter = textures.begin();
		}
		else
		{
			if( iter == textures.begin() )
				iter = textures.begin() + textures.size() - 1;
			else
				--iter;
		}
		if( *iter != channelTextureFileName_[ currentChannel_ ] )
		{
			channelTextureFileName_[ currentChannel_ ] = *iter;
			textureChannels_[ currentChannel_ ]->InvalidateRect( NULL );
			PythonAdapter::instance()->onBrowserTextureItemSelect(
				channelTextureFileName_[ currentChannel_ ].empty()	?	channelTextures_[ currentChannel_ ]
																	:	channelTextureFileName_[ currentChannel_ ] );
		}
	}
}

void PageTerrainTexture::changeTextureChannel( int direction )
{
	if( currentChannel_ != -1 )
	{
		int channel = currentChannel_;
		if( direction > 0 )
			++channel;
		else
			--channel;
		channel = ( ( channel ) + TEXTURE_CHANNEL_NUM ) % TEXTURE_CHANNEL_NUM;
		selectChannel( channel );
	}
}

PY_MODULE_STATIC_METHOD( PageTerrainTexture, setTerrainTexture, BigBang )

PyObject * PageTerrainTexture::py_setTerrainTexture( PyObject * args )
{
	instance->pickTerrainTexture();

	Py_Return;
}

PY_MODULE_STATIC_METHOD( PageTerrainTexture, setTerrainChannel, BigBang )

PyObject * PageTerrainTexture::py_setTerrainChannel( PyObject * args )
{
	int index;
	if (!PyArg_ParseTuple( args, "i", &index ))
	{
		PyErr_SetString( PyExc_TypeError, "PageTerrainTexture::py_setTerrainChannel "
			"takes only an int index" );

		return NULL;
	}

	if( index == instance->currentChannel_ )
	{
		instance->changeTextureTerrain( 1 );
	}
	else
	{
		if( index == 0 )
			instance->OnStnClickedTexturechannel1();
		else if( index == 1 )
			instance->OnStnClickedTexturechannel2();
		else if( index == 2 )
			instance->OnStnClickedTexturechannel3();
		else if( index == 3 )
			instance->OnStnClickedTexturechannel4();
	}
	Py_Return;
}

PY_MODULE_STATIC_METHOD( PageTerrainTexture, changeTextureTerrain, BigBang )

PyObject * PageTerrainTexture::py_changeTextureTerrain( PyObject * args )
{
	int direction;
	int isChannel;
	if (!PyArg_ParseTuple( args, "ii", &direction, &isChannel ))
	{
		PyErr_SetString( PyExc_TypeError, "PageTerrainTexture::py_setTerrainChannel "
			"takes only an int direction and an int indicator ( 0 for texture, 1 for channel )" );

		return NULL;
	}
	if( isChannel )
		instance->changeTextureTerrain( direction );
	else
		instance->changeTextureChannel( direction );
	Py_Return;
}

void PageTerrainTexture::onTextureChannelRightClick( CRCStatic* channel )
{
	for( int i = 0; i < TEXTURE_CHANNEL_NUM; ++i )
	{
		if( channel == textureChannels_[ i ] )
		{
			CMenu menu;
			menu.CreatePopupMenu();
			menu.AppendMenu( MF_STRING, 1, "Browse for Texture..." );
			menu.AppendMenu( MF_STRING, 2, "Select Texture in Asset Browser..." );
			POINT p;
			GetCursorPos( &p );
			int result = menu.TrackPopupMenu( TPM_LEFTBUTTON | TPM_RETURNCMD, p.x, p.y, this, NULL );
			if( result == 1 )
			{
				std::string filename = BWResource::resolveFilename(
					channelTextureFileName_[ i ].empty()	?	channelTextures_[ i ]
															:	channelTextureFileName_[ i ] );
				std::replace( filename.begin(), filename.end(), '/', '\\' );

				ResourceFileDialog fileDialog( TRUE, NULL,
					filename.c_str(),
					OFN_FILEMUSTEXIST,
					"Texture Files(*.tga;*.bmp;*.jpg)|*.tga;*.bmp;*.jpg|All Files(*.*)|*.*||" );

				if( fileDialog.DoModal() == IDOK )
				{
					channelTextureFileName_[ i ] = BWResource::dissolveFilename( (LPCTSTR)fileDialog.GetPathName() );
					textureChannels_[ i ]->InvalidateRect( NULL );
					selectChannel( i );
				}
			}
			else if( result == 2 )
			{
				PanelManager::instance()->showItemInUal( "TexturesVFolder",
					BWResource::resolveFilename( channelTextureFileName_[ i ] ) );
			}
			break;
		}
	}
}

void PageTerrainTexture::LoadWaterMark()
{
#pragma pack(push, 1)
	typedef struct
	{
		BYTE    IDLength;
		BYTE    ColorMapType;
		BYTE    ImageType;
		WORD    CMapStart;
		WORD    CMapLength;
		BYTE    CMapDepth;
		WORD    XOffset;
		WORD    YOffset;
		WORD    Width;
		WORD    Height;
		BYTE    PixelDepth;
		BYTE    ImageDescriptor;
	}
	TGA_Header;
#pragma pack(pop)

	std::string filename = BWResource::resolveFilename( "resources/maps/excla.tga" );
	std::replace( filename.begin(), filename.end(), '/', '\\' );
	HANDLE handle = CreateFile( filename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, NULL );
	
	if( handle != INVALID_HANDLE_VALUE )
	{
		TGA_Header header;

		DWORD bytesRead = 0;
		ReadFile( handle, &header, sizeof( header ), &bytesRead, NULL );

		if( header.IDLength == 0 && header.ColorMapType == 0 && header.ImageType == 2 &&
			header.PixelDepth == 32 && header.ImageDescriptor == 8 )
		{
			BITMAPINFO bmp = { { sizeof( BITMAPINFOHEADER ), header.Width, header.Height, 1, 32 } };

			void* pBits = NULL;

			HBITMAP hBmp = CreateDIBSection( NULL, & bmp, DIB_RGB_COLORS, &pBits, NULL, NULL );

			if( hBmp != NULL )
			{
				ReadFile( handle, pBits, header.Width * header.Height * 4, &bytesRead, NULL );

				for( int y = 0; y < header.Height; ++y )
				{
					BYTE* pPixel = (BYTE*) pBits + header.Width * 4 * y;

					for( int x = 0; x < header.Width; ++x )
					{
						pPixel[0] = pPixel[0] * pPixel[3] / 255; 
						pPixel[1] = pPixel[1] * pPixel[3] / 255; 
						pPixel[2] = pPixel[2] * pPixel[3] / 255; 

						pPixel += 4;
					}
				}
				waterMarkBmp_.Attach( hBmp );
				CClientDC clientDC( this );
				waterMark_.CreateCompatibleDC( &clientDC );
				monoBmp_ = waterMark_.SelectObject( &waterMarkBmp_ );
				waterMarkSize_ = CSize( header.Width, header.Height );
			}
		}
		CloseHandle(handle);
	}
}
