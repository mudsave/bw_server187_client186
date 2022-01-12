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
#include "property_list.hpp"
#include "user_messages.hpp"
#include "foldersetter.hpp"

DECLARE_DEBUG_COMPONENT2( "GUI", 2 )


PropertyItem* PropertyItem::selectedItem_ = NULL;

PropertyItem::PropertyItem(const CString& name) 
	: name_( name )
	, stringValue_( "" )
	, selectable_( true )
	, changeBuddy_( NULL )
	, parent_( NULL )
	, groupDepth_( 0 )
{
}

void PropertyItem::setGroup( const std::string& group )
{
//	MF_ASSERT( getType() != Type_Group );
	MF_ASSERT( (group.size() == 0) || (group.at(0) != '/') );

	// trim extra '/' from tail
	group_ = group;
	while ( (group_.end() != group_.begin()) &&
			(*(--group_.end()) == '/') )
	{
		group_.erase( --group_.end() );
	}

	if (group_.empty())
		groupDepth_ = 0;
	else
		groupDepth_ = std::count( group_.begin( ), group_.end( ), '/' ) + 1;
}


// GroupPropertyItem
GroupPropertyItem::GroupPropertyItem(const CString& name, int depth)
	: PropertyItem( name )
	, expanded_( true )
{
	groupDepth_ = depth;
}

GroupPropertyItem::~GroupPropertyItem()
{
}

void GroupPropertyItem::create(CWnd* parent)
{
	parent_ = parent;
}

void GroupPropertyItem::select(CRect rect)
{
	selectedItem_ = this;
}

void GroupPropertyItem::deselect()
{
	if (selectedItem_ != this)
		return;

	selectedItem_ = NULL;
}

void GroupPropertyItem::addChild( PropertyItem * child )
{
	children_.push_back( child );
}


// LabelPropertyItem
LabelPropertyItem::LabelPropertyItem(const CString& name, bool highlight)
	: PropertyItem( name )
	, highlight_( highlight )
{
}

LabelPropertyItem::~LabelPropertyItem()
{
}

void LabelPropertyItem::create(CWnd* parent)
{
	parent_ = parent;
}

void LabelPropertyItem::select(CRect rect)
{
	selectedItem_ = this;
}

void LabelPropertyItem::deselect()
{
	if (selectedItem_ != this)
		return;

	selectedItem_ = NULL;
}

PropertyItem::ItemType LabelPropertyItem::getType()
{
	if (highlight_)
        return Type_Label_Highlight;
	else
		return Type_Label;
}


// StringPropertyItem
CEdit StringPropertyItem::edit_;
CButton StringPropertyItem::button_;

StringPropertyItem::StringPropertyItem(const CString& name, const CString& currentValue, bool readOnly)
	: PropertyItem( name )
	, readOnly_( readOnly )
{
	stringValue_ = currentValue;
}

StringPropertyItem::~StringPropertyItem()
{
}

void StringPropertyItem::create(CWnd* parent)
{
	parent_ = parent;

	if (!edit_)
	{
		CRect rect(10, 10, 10, 10);
		DWORD style = ES_LEFT | ES_AUTOHSCROLL | WS_CHILD | WS_BORDER;
		edit_.Create(style, rect, parent, IDC_PROPERTYLIST_STRING);

		CFont* pFont = parent->GetParent()->GetFont();
		edit_.SetFont(pFont);
	}

	if (!button_)
	{
		CRect rect(10, 10, 10, 10);
		DWORD style = BS_PUSHBUTTON | WS_CHILD | WS_BORDER;
		button_.Create( "...", style, rect, parent, IDC_PROPERTYLIST_BROWSE);

		CFont* pFont = parent->GetParent()->GetFont();
		button_.SetFont(pFont);
	}
}

void StringPropertyItem::select(CRect rect)
{
	// check the readonly attribute
	// note: must use SendMessage, ModifyStyle does not work on CEdit
	if (readOnly_)
		edit_.SendMessage(EM_SETREADONLY, TRUE, 0);
	else
		edit_.SendMessage(EM_SETREADONLY, FALSE, 0);

	const int BUTTON_WIDTH = ( ( fileFilter_.size() == 0 && !isColor() ) || readOnly_ ) ? 0 : 20;

	// display edit box
	rect.bottom -= 1;		// keep it inside the selection
	rect.right -= BUTTON_WIDTH;
	edit_.MoveWindow(rect);

	edit_.SetWindowText(stringValue_);
	edit_.ShowWindow(SW_SHOW);
	edit_.SetFocus();
	edit_.SetSel(0, -1);

	rect.left = rect.right;
	rect.right += BUTTON_WIDTH;

	button_.MoveWindow(rect);

	button_.ShowWindow(SW_SHOW);
	button_.SetWindowPos( &CWnd::wndTop, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE );

	selectedItem_ = this;
}

void StringPropertyItem::deselect()
{
	if (selectedItem_ != this)
		return;

	CString newStr;
	edit_.GetWindowText(newStr);
	if (stringValue_ != newStr)
	{
		stringValue_ = newStr;
		parent_->SendMessage(WM_CHANGE_PROPERTYITEM, 0, (LPARAM)changeBuddy_);
	}

	edit_.ShowWindow(SW_HIDE);
	edit_.Invalidate();

	button_.ShowWindow(SW_HIDE);
	button_.Invalidate();

	selectedItem_ = NULL;
}

void StringPropertyItem::itemRectChange( CRect rect )
{
	if( selectedItem_ == this )
	{
		const int BUTTON_WIDTH = ( ( fileFilter_.size() == 0 && !isColor() ) || readOnly_ ) ? 0 : 20;

		// display edit box
		rect.bottom -= 1;		// keep it inside the selection
		rect.right -= BUTTON_WIDTH;
		edit_.MoveWindow(rect);

		rect.left = rect.right;
		rect.right += BUTTON_WIDTH;

		button_.MoveWindow(rect);
	}
}

void StringPropertyItem::set(const std::string& value)
{
	stringValue_ = value.c_str();

	if (selectedItem_ == this)
	{
		edit_.SetWindowText(stringValue_);
		edit_.SetSel(0, -1);
	}

	// force the list to redraw 
	// TODO: only redraw the particular item
	if (parent_)
		parent_->Invalidate();
}

std::string StringPropertyItem::get()
{
	// don't need this to dynamically change
	return stringValue_.GetBuffer(); 
}

void StringPropertyItem::onBrowse()
{
	if( isColor() )
	{
		struct MapHex
		{
			int h2d( char ch )
			{
				if( ch >= '0' && ch <= '9' )
					return ch - '0';
				if( ch >= 'A' && ch <= 'F' )
					return ch - 'A' + 10;
				return ch - 'a' + 10;
			}
			int operator()( LPCTSTR str )
			{
				int result = 0;
				while( *str )
				{
					result *= 16;
					result += h2d( *str );
					++str;
				}
				return result;
			}
		}
		MapHex;
		struct SwapRB
		{
			COLORREF operator()( COLORREF ref )
			{
				return ( ( ref / 65536 ) & 0xff ) + ( ( ref / 256 ) & 0xff ) * 256 + ( ref & 0xff ) * 65536;
			}
		}
		SwapRB;
		CString color;
		edit_.GetWindowText( color );
		CColorDialog colorDlg( SwapRB( MapHex( (LPCTSTR)color + 1 ) ) );
		if( colorDlg.DoModal() == IDOK )
		{
			CString s;
			s.Format( "#%06x", SwapRB( colorDlg.GetColor() ) & 0xffffff );
			edit_.SetWindowText( s );
			parent_->SendMessage(WM_CHANGE_PROPERTYITEM, 0, (LPARAM)changeBuddy_);
		}
	}
	else
	{
		ASSERT( fileFilter_.size() );
		CFileDialog fileDialog( TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, fileFilter_.c_str(), edit_.GetParent() );

		static FolderSetter FolderSetter;

		FolderGuard FolderGuard( FolderSetter );

		if(fileDialog.DoModal()==IDOK)
		{
			stringValue_=fileDialog.GetPathName();
			std::string disolvedFileName = BWResource::dissolveFilename( (LPCTSTR)stringValue_ );
			if(disolvedFileName[1]!=':'&&disolvedFileName.size())
			{
				edit_.SetWindowText( disolvedFileName.c_str() );
				parent_->SendMessage(WM_CHANGE_PROPERTYITEM, 0, (LPARAM)changeBuddy_);
			}
			else
				edit_.GetParent()->MessageBox( "You can only select a texture in one of the resource folders","Warning", MB_OK | MB_ICONWARNING );
		}
	}
}

PropertyItem::ItemType StringPropertyItem::getType()
{
	if (readOnly_)
		return Type_String_ReadOnly;

	return Type_String;
}


// IDPropertyItem
CEdit IDPropertyItem::edit_;

IDPropertyItem::IDPropertyItem(const CString& name, const CString& currentValue)
	: PropertyItem( name )
{
	stringValue_ = currentValue;
}

IDPropertyItem::~IDPropertyItem()
{
}

void IDPropertyItem::create(CWnd* parent)
{
	parent_ = parent;

	if (!edit_)
	{
		CRect rect(10, 10, 10, 10);
		DWORD style = ES_LEFT | ES_AUTOHSCROLL | WS_CHILD | WS_BORDER | EM_SETREADONLY;
		edit_.Create(style, rect, parent, IDC_PROPERTYLIST_STRING);

		CFont* pFont = parent->GetParent()->GetFont();
		edit_.SetFont(pFont);
	}
}

void IDPropertyItem::select(CRect rect)
{
	edit_.SendMessage(EM_SETREADONLY, TRUE, 0);

	// display edit box
	rect.bottom -= 1;		// keep it inside the selection
	edit_.MoveWindow(rect);

	edit_.SetWindowText(stringValue_);
	edit_.ShowWindow(SW_SHOW);
	edit_.SetFocus();
	edit_.SetSel(0, -1);

	selectedItem_ = this;
}

void IDPropertyItem::deselect()
{
	if (selectedItem_ != this)
		return;

	CString newStr;
	edit_.GetWindowText(newStr);
	if (stringValue_ != newStr)
	{
		stringValue_ = newStr;
		parent_->SendMessage(WM_CHANGE_PROPERTYITEM, 0, (LPARAM)changeBuddy_);
	}

	edit_.ShowWindow(SW_HIDE);
	edit_.Invalidate();

	selectedItem_ = NULL;
}

void IDPropertyItem::itemRectChange( CRect rect )
{
	if( selectedItem_ == this )
	{
		rect.bottom -= 1;		// keep it inside the selection
		edit_.MoveWindow(rect);
	}
}

void IDPropertyItem::set(const std::string& value)
{
	stringValue_ = value.c_str();

	if (selectedItem_ == this)
	{
		edit_.SetWindowText(stringValue_);
		edit_.SetSel(0, -1);
	}

	// force the list to redraw 
	// TODO: only redraw the particular item
	if (parent_)
		parent_->Invalidate();
}

std::string IDPropertyItem::get()
{
	// don't need this to dynamically change
	return stringValue_.GetBuffer(); 
}

PropertyItem::ItemType IDPropertyItem::getType()
{
	return Type_ID;
}


// ComboPropertyItem
CComboBox ComboPropertyItem::comboBox_;

ComboPropertyItem::ComboPropertyItem(const CString& name, CString currentValue,
	const std::vector<std::string>& possibleValues)
	: PropertyItem( name )
	, possibleValues_( possibleValues )
{
	std::string currValue = currentValue.GetBuffer();
//	MF_ASSERT( std::find( possibleValues_.begin(), possibleValues_.end(), currValue ) != possibleValues_.end() );
	stringValue_ = currentValue;
}

ComboPropertyItem::ComboPropertyItem(const CString& name, int currentValueIndex,
	const std::vector<std::string>& possibleValues)
	: PropertyItem( name )
	, possibleValues_( possibleValues )
{
	MF_ASSERT( currentValueIndex < (int) possibleValues_.size() );
	stringValue_ = possibleValues_[currentValueIndex].c_str();
}

ComboPropertyItem::~ComboPropertyItem()
{
}

void ComboPropertyItem::create(CWnd* parent)
{
	parent_ = parent;

	if (!comboBox_)
	{
		CRect rect(10, 10, 10, 10);
		comboBox_.Create(CBS_DROPDOWNLIST | CBS_DISABLENOSCROLL |  
					WS_CHILD | WS_BORDER | WS_VSCROLL,
					rect, parent, IDC_PROPERTYLIST_LIST);

		CFont* pFont = parent->GetParent()->GetFont();
		comboBox_.SetFont(pFont);
	}
}

void ComboPropertyItem::select(CRect rect)
{
	// add the possible values and get the size of the longest
	int biggestCX = 0;
	CDC* pDC = comboBox_.GetDC();

	comboBox_.ResetContent();
	for (std::vector<std::string>::iterator it = possibleValues_.begin();
		it != possibleValues_.end();
		it++)
	{
		comboBox_.AddString(it->c_str());

		CSize stringSize = pDC->GetTextExtent(it->c_str());
		if (stringSize.cx > biggestCX)
			biggestCX = stringSize.cx;
	}

	comboBox_.ReleaseDC(pDC);

	rect.top -= 2;
	rect.bottom += 100;	// extend the bottom to fit a reasonable number of choices

	// extend the left to fit all the text
	int widthDiff = biggestCX - rect.Width();
	rect.left -= widthDiff > 0 ? widthDiff : 0; 
	comboBox_.MoveWindow(rect);

	comboBox_.SelectString(-1, stringValue_);
	comboBox_.ShowWindow(SW_SHOW);
	comboBox_.SetFocus();
	comboBox_.ShowDropDown();

	selectedItem_ = this;
}

void ComboPropertyItem::deselect()
{
	if (selectedItem_ != this)
		return;

	if ( comboBox_.GetCurSel() >= 0 )
	{
		// update the data
		CString newValue;
		comboBox_.GetLBText(comboBox_.GetCurSel(), newValue);
		if (stringValue_ != newValue)
		{
			comboChange();
		}
	}

	// hide the edit
	comboBox_.ShowWindow(SW_HIDE);
	comboBox_.Invalidate();

	selectedItem_ = NULL;
}

void ComboPropertyItem::itemRectChange( CRect rect )
{
	if( selectedItem_ == this )
	{
		int biggestCX = 0;
		CDC* pDC = comboBox_.GetDC();

		for (std::vector<std::string>::iterator it = possibleValues_.begin();
			it != possibleValues_.end();
			it++)
		{
			CSize stringSize = pDC->GetTextExtent(it->c_str());
			if (stringSize.cx > biggestCX)
				biggestCX = stringSize.cx;
		}

		comboBox_.ReleaseDC(pDC);

		rect.top -= 2;
		rect.bottom += 100;	// extend the bottom to fit a reasonable number of choices

		// extend the left to fit all the text
		int widthDiff = biggestCX - rect.Width();
		rect.left -= widthDiff > 0 ? widthDiff : 0; 
		comboBox_.MoveWindow(rect);
	}
}

void ComboPropertyItem::loseFocus()
{
	if (selectedItem_ != this)
		return;

	if (comboBox_.GetDroppedState())
	{
		// ignore any current selection

		comboBox_.ShowWindow(SW_HIDE);
		comboBox_.Invalidate();

		selectedItem_ = NULL;
	}
	else
	{
		// accept the selection
		deselect();
	}
}

void ComboPropertyItem::set(const std::string& value)
{
	std::vector<std::string>::iterator it = std::find( possibleValues_.begin(),
											possibleValues_.end(),
											value);
	// validate string
	if (it == possibleValues_.end())
		return;

	stringValue_ = it->c_str();

	if (selectedItem_ == this)
		comboBox_.SelectString(-1, stringValue_);

	// force the list to redraw 
	// TODO: only redraw the particular item
	if (parent_)
		parent_->Invalidate();
}

void ComboPropertyItem::set(int index)
{
	// validate index
	if (index >= (int)(possibleValues_.size()))
		return;

	stringValue_ = possibleValues_.at(index).c_str();

	if (selectedItem_ == this)
		comboBox_.SelectString(-1, stringValue_);

	// force the list to redraw 
	// TODO: only redraw the particular item
	if (parent_)
		parent_->Invalidate();
}

std::string ComboPropertyItem::get()
{
	// state already dynamically updated with callback
	return stringValue_.GetBuffer(); 
}

void ComboPropertyItem::comboChange()
{
	comboBox_.GetLBText(comboBox_.GetCurSel(), stringValue_);
	parent_->SendMessage(WM_CHANGE_PROPERTYITEM, 0, (LPARAM)changeBuddy_);
}


// BoolPropertyItem
CComboBox BoolPropertyItem::comboBox_;

BoolPropertyItem::BoolPropertyItem(const CString& name, int currentValue)
	: PropertyItem( name )
{
	value_ = currentValue;
}

BoolPropertyItem::~BoolPropertyItem()
{
}

void BoolPropertyItem::create(CWnd* parent)
{
	parent_ = parent;

	if (!comboBox_)
	{
		CRect rect(10, 10, 10, 10);
		comboBox_.Create(CBS_DROPDOWNLIST | CBS_NOINTEGRALHEIGHT |  
					WS_CHILD | WS_BORDER,
					rect, parent, IDC_PROPERTYLIST_BOOL);

		CFont* pFont = parent->GetParent()->GetFont();
		comboBox_.SetFont(pFont);

		// add the possible values
		comboBox_.ResetContent();
		comboBox_.InsertString(0, "False");
		comboBox_.InsertString(1, "True");
	}

	// initialise the stringValue_
	comboBox_.SetCurSel(value_);
	comboBox_.GetLBText(value_, stringValue_);
}

void BoolPropertyItem::select(CRect rect)
{
	rect.top -= 2;
	rect.bottom += 100;	// extend the bottom to fit in all the choices
	comboBox_.MoveWindow(rect);

	comboBox_.SetCurSel(value_);
	comboBox_.GetLBText(value_, stringValue_);

	comboBox_.SetFocus();
	comboBox_.ShowWindow(SW_SHOW);
	comboBox_.ShowDropDown();

	selectedItem_ = this;
}

void BoolPropertyItem::deselect()
{
	if (selectedItem_ != this)
		return;

	// update data if required
	CString newValue;
	comboBox_.GetLBText(comboBox_.GetCurSel(), newValue);
	if (newValue != stringValue_)
	{
		value_ = comboBox_.GetCurSel();
		stringValue_ = newValue;
		parent_->SendMessage(WM_CHANGE_PROPERTYITEM, 0, (LPARAM)changeBuddy_);
	}

	// hide the edit
	comboBox_.ShowWindow(SW_HIDE);
	comboBox_.Invalidate();

	selectedItem_ = NULL;
}

void BoolPropertyItem::itemRectChange( CRect rect )
{
	if( selectedItem_ == this )
	{
		rect.top -= 2;
		rect.bottom += 100;	// extend the bottom to fit in all the choices
		comboBox_.MoveWindow(rect);
	}
}

void BoolPropertyItem::loseFocus()
{
	if (selectedItem_ != this)
		return;

	if (comboBox_.GetDroppedState())
	{
		// ignore any changes to the selection
		comboBox_.ShowWindow(SW_HIDE);
		comboBox_.Invalidate();

		selectedItem_ = NULL;
	}
	else
	{
		// accept the selection
		deselect();
	}
}

void BoolPropertyItem::set(bool value)
{
	value_ = (int)value;
	stringValue_ = value ? "True" : "False";

	if (selectedItem_ == this)
	{
		comboBox_.SetCurSel(value_);
		comboBox_.GetLBText(value_, stringValue_);
	}

	// force the list to redraw 
	// TODO: only redraw the particular item
	if (parent_)
		parent_->Invalidate();
}

bool BoolPropertyItem::get()
{
	if (selectedItem_ == this)
		return comboBox_.GetCurSel() != 0;
	else
		return value_ != 0;
}


// FloatPropertyItem
controls::EditNumeric FloatPropertyItem::editNumeric_;
controls::EditNumeric FloatPropertyItem::editNumericFormatting_;
controls::Slider FloatPropertyItem::slider_;
CButton FloatPropertyItem::button_;

FloatPropertyItem::FloatPropertyItem(const CString& name, float currentValue)
	: PropertyItem( name )
	, value_( currentValue )
	, ranged_( false )
	, changing_( false )
	, hasDef_( false )
{
}

FloatPropertyItem::~FloatPropertyItem()
{
}

void FloatPropertyItem::create(CWnd* parent)
{
	parent_ = parent;

	if (!editNumeric_)
	{
		CRect rect(10, 10, 10, 10);
		editNumeric_.Create(ES_LEFT | ES_AUTOHSCROLL | WS_CHILD | WS_BORDER,
						rect, parent, IDC_PROPERTYLIST_FLOAT);

		CFont* pFont = parent->GetParent()->GetFont();
		editNumeric_.SetFont(pFont);

		editNumericFormatting_.Create(ES_LEFT | ES_AUTOHSCROLL | WS_CHILD | WS_BORDER,
						rect, parent, IDC_PROPERTYLIST_FLOAT);
	}
	if( !slider_ )
	{
		CRect rect(10, 10, 10, 10);
		slider_.Create(
			TBS_HORZ | TBS_NOTICKS | WS_CHILD | WS_VISIBLE | WS_TABSTOP, rect,
			parent, IDC_PROPERTYLIST_SLIDER );
	}
	if(!button_)
	{
		CRect rect(10, 10, 10, 10);
		DWORD style = BS_PUSHBUTTON | WS_CHILD | WS_BORDER;
		button_.Create( "*", style, rect, parent, IDC_PROPERTYLIST_DEFAULT);

		CFont* pFont = parent->GetParent()->GetFont();
		button_.SetFont(pFont);
	}

	editNumericFormatting_.SetValue(value_);
	stringValue_ = editNumericFormatting_.GetStringForm();
}

void FloatPropertyItem::select(CRect rect)
{
	// display edit box
	rect.bottom -= 1;		// keep it inside the selection

	static const int MIN_SLIDER_WIDTH = 60;
	static const int EDIT_WIDTH = 40;
	const int BUTTON_WIDTH = hasDef_ ? 10 : 0;
	const int SLIDER_WIDTH = ( !ranged_ || rect.Width() - BUTTON_WIDTH < EDIT_WIDTH + MIN_SLIDER_WIDTH ) ? 0 : rect.Width() - BUTTON_WIDTH - EDIT_WIDTH;

	rect.right -= BUTTON_WIDTH + SLIDER_WIDTH;
	editNumeric_.MoveWindow(rect);

	rect.left = rect.right;
	rect.right += SLIDER_WIDTH;
	if( SLIDER_WIDTH != 0 )
	{
		slider_.MoveWindow(rect);
		slider_.SetRange( (int)( min_ * pow( 10.f, digits_ ) ), (int)( max_ * pow( 10.f, digits_ ) ) );
		slider_.SetPos( (int)( value_ * pow( 10.f, digits_ ) ) );
		slider_.ShowWindow(SW_SHOW);
		slider_.GetParent()->InvalidateRect(NULL);
		editNumeric_.SetMinimum(min_);
		editNumeric_.SetMaximum(max_);
	}
	else
	{
		editNumeric_.SetMinimum(-FLT_MAX);
		editNumeric_.SetMaximum(FLT_MAX);
	}

	editNumeric_.SetValue(value_);
	stringValue_ = editNumeric_.GetStringForm();

	editNumeric_.ShowWindow(SW_SHOW);
	editNumeric_.SetFocus();
	editNumeric_.SetSel(0, -1);

	rect.left = rect.right;
	rect.right += BUTTON_WIDTH;
	if( BUTTON_WIDTH != 0 )
	{
		button_.MoveWindow(rect);
		button_.ShowWindow(SW_SHOW);
	}

	selectedItem_ = this;
}

void FloatPropertyItem::deselect()
{
	if (selectedItem_ != this)
		return;

	// this is called at the end of editing
	// store the value for next time
	CString newValue;
	editNumeric_.GetWindowText(newValue);
	if (newValue != stringValue_ || value_ != editNumeric_.GetValue() )
	{
		value_ = editNumeric_.GetValue();
		stringValue_ = newValue;
		parent_->SendMessage(WM_CHANGE_PROPERTYITEM, 0, (LPARAM)changeBuddy_);
	}

	// close the edit window
	editNumeric_.SendMessage(WM_KILLFOCUS);
	editNumeric_.ShowWindow(SW_HIDE);
	editNumeric_.Invalidate();

	slider_.ShowWindow(SW_HIDE);
	slider_.Invalidate();

	button_.ShowWindow(SW_HIDE);
	button_.Invalidate();

	selectedItem_ = NULL;
}

void FloatPropertyItem::itemRectChange( CRect rect )
{
	if (selectedItem_ == this)
	{
		rect.bottom -= 1;		// keep it inside the selection

		static const int MIN_SLIDER_WIDTH = 60;
		static const int EDIT_WIDTH = 40;
		const int BUTTON_WIDTH = hasDef_ ? 10 : 0;
		const int SLIDER_WIDTH = ( !ranged_ || rect.Width() - BUTTON_WIDTH < EDIT_WIDTH + MIN_SLIDER_WIDTH ) ? 0 : rect.Width() - BUTTON_WIDTH - EDIT_WIDTH;

		rect.right -= BUTTON_WIDTH + SLIDER_WIDTH;
		editNumeric_.MoveWindow(rect);

		rect.left = rect.right;
		rect.right += SLIDER_WIDTH;
		if( SLIDER_WIDTH != 0 )
			slider_.MoveWindow(rect);

		rect.left = rect.right;
		rect.right += BUTTON_WIDTH;
		if( BUTTON_WIDTH != 0 )
			button_.MoveWindow(rect);
	}
}

void FloatPropertyItem::setRange( float min, float max, int digits )
{
	ranged_ = true;
	min_ = min;
	max_ = max;
	digits_ = digits;
}

void FloatPropertyItem::setDefault( float def )
{
	hasDef_ = true;
	def_ = def;
}

void FloatPropertyItem::set(float value)
{
	value_ = value;

	editNumericFormatting_.SetValue(value);
	stringValue_ = editNumericFormatting_.GetStringForm();
	
	if (selectedItem_ == this)
	{
		editNumeric_.SetValue(value);
		editNumeric_.SetSel(0, -1);
	}

	// force the list to redraw 
	// TODO: only redraw the particular item
	if (parent_)
		parent_->Invalidate();
}

float FloatPropertyItem::get()
{
	if (selectedItem_ == this)
		return editNumeric_.GetValue();
	else
		return value_; 
}

void FloatPropertyItem::sliderChange( int value, bool transient )
{
	if( !changing_ ) 
	{
		changing_ = true;
		editNumeric_.SetValue((float)( value/pow(10.f, digits_) ));
		stringValue_ = editNumeric_.GetStringForm();
		parent_->SendMessage(WM_CHANGE_PROPERTYITEM, transient, (LPARAM)changeBuddy_);
		changing_ = false;
	}
}

void FloatPropertyItem::onDefault()
{
	if( !changing_ ) 
	{
		changing_ = true;
		editNumeric_.SetValue((float)( def_/pow(10.f, digits_) ));
		parent_->SendMessage(WM_CHANGE_PROPERTYITEM, 0, (LPARAM)changeBuddy_);
		if( ranged_ )
			slider_.SetPos( (int)( editNumeric_.GetValue() * pow(10.f, digits_) ) );
		changing_ = false;
	}
}

void FloatPropertyItem::editChange()
{
	if( !changing_ ) 
	{
		changing_ = true;
		if( ranged_ )
			slider_.SetPos( (int)( editNumeric_.GetValue() * pow(10.f, digits_) ) );
		changing_ = false;
	}
}

// IntPropertyItem
controls::EditNumeric IntPropertyItem::editNumeric_;
controls::EditNumeric IntPropertyItem::editNumericFormatting_;
controls::Slider IntPropertyItem::slider_;

IntPropertyItem::IntPropertyItem(const CString& name, int currentValue)
	: PropertyItem( name )
	, value_( currentValue )
	, ranged_( false )
	, changing_( false )
{
}

IntPropertyItem::~IntPropertyItem()
{
}

void IntPropertyItem::create(CWnd* parent)
{
	parent_ = parent;

	if (!editNumeric_)
	{
        editNumeric_.SetNumericType( controls::EditNumeric::ENT_INTEGER );

		CRect rect(10, 10, 10, 10);
		editNumeric_.Create(ES_LEFT | ES_AUTOHSCROLL | WS_CHILD | WS_BORDER,
						rect, parent, IDC_PROPERTYLIST_INT);

		CFont* pFont = parent->GetParent()->GetFont();
		editNumeric_.SetFont(pFont);

		editNumericFormatting_.SetNumericType( controls::EditNumeric::ENT_INTEGER );
		editNumericFormatting_.Create(ES_LEFT | ES_AUTOHSCROLL | WS_CHILD | WS_BORDER,
						rect, parent, IDC_PROPERTYLIST_INT);
	}

	if( !slider_ )
	{
		CRect rect(10, 10, 10, 10);
		slider_.Create(
			TBS_HORZ | TBS_NOTICKS | WS_CHILD | WS_VISIBLE | WS_TABSTOP, rect,
			parent, IDC_PROPERTYLIST_SLIDER );
	}

	editNumericFormatting_.SetIntegerValue(value_);
	stringValue_ = editNumericFormatting_.GetStringForm();
}

void IntPropertyItem::select(CRect rect)
{
	// display edit box
	rect.bottom -= 1;		// keep it inside the selection

	static const int MIN_SLIDER_WIDTH = 60;
	static const int EDIT_WIDTH = 40;
	const int SLIDER_WIDTH = ( !ranged_ || rect.Width() < EDIT_WIDTH + MIN_SLIDER_WIDTH ) ? 0 : rect.Width() - EDIT_WIDTH;

	rect.right -= SLIDER_WIDTH;
	editNumeric_.MoveWindow(rect);

	rect.left = rect.right;
	rect.right += SLIDER_WIDTH;
	if( SLIDER_WIDTH != 0 )
	{
		slider_.MoveWindow(rect);
		slider_.SetRange( min_, max_ );
		slider_.SetPos( value_ );
		slider_.ShowWindow(SW_SHOW);
		editNumeric_.SetMinimum((float)min_);
		editNumeric_.SetMaximum((float)max_);
	}
	else
	{
		editNumeric_.SetMinimum(-FLT_MAX);
		editNumeric_.SetMaximum(FLT_MAX);
	}

	editNumeric_.SetIntegerValue(value_);
	stringValue_ = editNumeric_.GetStringForm();

	editNumeric_.ShowWindow(SW_SHOW);
	editNumeric_.SetFocus();
	editNumeric_.SetSel(0, -1);

	selectedItem_ = this;
}

void IntPropertyItem::deselect()
{
	if (selectedItem_ != this)
		return;

	CString newValue = editNumeric_.GetStringForm();
	if (newValue != stringValue_ || value_ != editNumeric_.GetIntegerValue() )
	{
		value_ = editNumeric_.GetIntegerValue();
		stringValue_ = newValue;
		parent_->SendMessage(WM_CHANGE_PROPERTYITEM, 0, (LPARAM)changeBuddy_);
	}

	// kill the edit window
	editNumeric_.SendMessage(WM_KILLFOCUS);
	editNumeric_.ShowWindow(SW_HIDE);
	editNumeric_.Invalidate();

	slider_.ShowWindow(SW_HIDE);
	slider_.Invalidate();
	selectedItem_ = NULL;
}

void IntPropertyItem::itemRectChange( CRect rect )
{
	if( selectedItem_ == this )
	{
		rect.bottom -= 1;		// keep it inside the selection

		static const int MIN_SLIDER_WIDTH = 60;
		static const int EDIT_WIDTH = 40;
		const int SLIDER_WIDTH = ( !ranged_ || rect.Width() < EDIT_WIDTH + MIN_SLIDER_WIDTH ) ? 0 : rect.Width() - EDIT_WIDTH;

		rect.right -= SLIDER_WIDTH;
		editNumeric_.MoveWindow(rect);

		rect.left = rect.right;
		rect.right += SLIDER_WIDTH;
		if( SLIDER_WIDTH != 0 )
			slider_.MoveWindow(rect);
	}
}

void IntPropertyItem::setRange( int min, int max )
{
	ranged_ = true;
	min_ = min;
	max_ = max;
}

void IntPropertyItem::set(int value)
{
	value_ = value;

	editNumericFormatting_.SetIntegerValue(value);
	stringValue_ = editNumericFormatting_.GetStringForm();

	if (selectedItem_ == this)
	{
		editNumeric_.SetIntegerValue(value);
		editNumeric_.SetSel(0, -1);
	}

	// force the list to redraw 
	// TODO: only redraw the particular item
	if (parent_)
		parent_->Invalidate();
}

int IntPropertyItem::get()
{
	if (selectedItem_ == this)
		return editNumeric_.GetIntegerValue();
	else
		return value_; 
}

void IntPropertyItem::sliderChange( int value, bool transient )
{
	if( !changing_ )
	{
		changing_ = true;
		editNumeric_.SetIntegerValue(value);
		stringValue_ = editNumeric_.GetStringForm();
		parent_->SendMessage(WM_CHANGE_PROPERTYITEM, transient, (LPARAM)changeBuddy_);
		changing_ = false;
	}
}

void IntPropertyItem::editChange()
{
	if( !changing_ )
	{
		changing_ = true;
		if( ranged_ )
			slider_.SetPos( editNumeric_.GetIntegerValue() );
		changing_ = false;
	}
}

// PropertyList

PropertyList::PropertyList()
{
}

PropertyList::~PropertyList()
{
}


BEGIN_MESSAGE_MAP(PropertyList, CListBox)
	ON_CONTROL_REFLECT(LBN_SELCHANGE, OnSelchange)
	ON_WM_LBUTTONUP()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_MESSAGE(WM_CHANGE_PROPERTYITEM, OnChangePropertyItem)
	ON_WM_VSCROLL()
	ON_WM_KILLFOCUS()
	ON_CBN_SELCHANGE(IDC_PROPERTYLIST_LIST, OnComboChange)
	ON_WM_LBUTTONDBLCLK()
	ON_WM_RBUTTONUP()
	ON_COMMAND( IDC_PROPERTYLIST_BROWSE, OnBrowse )
	ON_COMMAND( IDC_PROPERTYLIST_DEFAULT, OnDefault )
	ON_WM_HSCROLL()
	ON_EN_CHANGE( IDC_PROPERTYLIST_FLOAT, OnEditChange )
	ON_EN_CHANGE( IDC_PROPERTYLIST_INT, OnEditChange )
END_MESSAGE_MAP()


// PropertyList message handlers

void PropertyList::PreSubclassWindow() 
{
	dividerPos_ = 0;
	dividerMove_ = false;
	dividerTop_ = 0;
	dividerBottom_ = 0;
	dividerLastX_ = 0;

	selected_ = 1;

	cursorSize_ = AfxGetApp()->LoadStandardCursor(IDC_SIZEWE);
	cursorArrow_ = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
}


void PropertyList::MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct) 
{
	lpMeasureItemStruct->itemHeight = 20;
}

const int childIndent = 16;

void PropertyList::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct) 
{
	UINT index = lpDrawItemStruct->itemID;
	if (index == (UINT)-1)
		return;

	PropertyItem* item = (PropertyItem*)GetItemDataPtr( index );

	// draw two rectangles, one for the property lable, one for the value
	// (unless is a group)
	int nCrBackground, nCrText;
	if( (lpDrawItemStruct->itemAction | ODA_SELECT) && 
		(lpDrawItemStruct->itemState & ODS_SELECTED) &&
		(item->getType() != PropertyItem::Type_Group) )
	{
		nCrBackground = COLOR_HIGHLIGHT;
		nCrText = COLOR_HIGHLIGHTTEXT;
	}
	else
	{
		nCrBackground = COLOR_WINDOW;
		nCrText = COLOR_WINDOWTEXT;
	}
	COLORREF crBackground = ::GetSysColor(nCrBackground);
	COLORREF crText = ::GetSysColor(nCrText);

	CRect rectItem = lpDrawItemStruct->rcItem;

	CDC dc;
	dc.Attach( lpDrawItemStruct->hDC );
	dc.FillSolidRect(rectItem, crBackground);

	int border = 1;
	rectItem.right -= border;
	rectItem.left += border;
	rectItem.top += border;
	rectItem.bottom -= border;

	CRect rectLabel = lpDrawItemStruct->rcItem;
	CRect rectValue = lpDrawItemStruct->rcItem;

	rectLabel.left =  item->getGroupDepth() * childIndent;
	if (item->getType() == PropertyItem::Type_Group)
	{
		// change the background colour
		nCrBackground = COLOR_INACTIVECAPTIONTEXT;
		crBackground = ::GetSysColor(nCrBackground);
		dc.FillSolidRect(rectLabel, crBackground);

		// + / -
		CRect rcSign(lpDrawItemStruct->rcItem);
		rcSign.top = (int)(0.5f * (rcSign.bottom - rcSign.top)) + rcSign.top;
		rcSign.bottom = rcSign.top;
		rcSign.right = rectLabel.left - (int)(childIndent * 0.5f);
		rcSign.left = rcSign.right;
		rcSign.InflateRect(5, 5, 7, 7);

		dc.DrawEdge( rcSign, EDGE_ETCHED, BF_RECT );
		
		CPoint ptCenter(rcSign.CenterPoint());
		ptCenter.x -= 1;
		ptCenter.y -= 1;

		// minus		
		dc.MoveTo(ptCenter.x - 3, ptCenter.y);
		dc.LineTo(ptCenter.x + 4, ptCenter.y);

		GroupPropertyItem * gItem = (GroupPropertyItem *)(item);
		if (!gItem->getExpanded())
		{
			// plus
			dc.MoveTo(ptCenter.x, ptCenter.y - 3);
			dc.LineTo(ptCenter.x, ptCenter.y + 4);
		}
	}
	else if (item->getType() == PropertyItem::Type_Label )
	{
		// do nothing
	}
	else if (item->getType() == PropertyItem::Type_Label_Highlight )
	{
		// use highlighting
		// change the background colour
		nCrBackground = COLOR_INACTIVECAPTIONTEXT;
		crBackground = ::GetSysColor(nCrBackground);
		dc.FillSolidRect(rectLabel, crBackground);
	}
	else
	{
		if (dividerPos_ == 0)
			dividerPos_ = rectValue.Width() / 2;
		rectValue.left = dividerPos_;
		rectLabel.right = rectValue.left - 1;
	}


	dc.DrawEdge( rectLabel, EDGE_ETCHED,BF_BOTTOMRIGHT );
	if (item->getType() != PropertyItem::Type_Group &&
			item->getType() != PropertyItem::Type_Label &&
			item->getType() != PropertyItem::Type_Label_Highlight)
		dc.DrawEdge( rectValue, EDGE_ETCHED,BF_BOTTOM );

	border = 3;
	rectLabel.right -= border;
	rectLabel.left += border;
	rectLabel.top += border;
	rectLabel.bottom -= border;

	rectValue.right -= border;
	rectValue.left += border;
	rectValue.top += border;
	rectValue.bottom -= border;

	// write the property name in the first rectangle
	// value in the second rectangle
	COLORREF crOldBkColor = dc.SetBkColor(crBackground);
	COLORREF crOldTextColor = dc.SetTextColor(crText);

	dc.DrawText(item->name(), rectLabel, DT_LEFT | DT_SINGLELINE);
	if (item->getType() != PropertyItem::Type_Group &&
			item->getType() != PropertyItem::Type_Label &&
			item->getType() != PropertyItem::Type_Label_Highlight)
		dc.DrawText(item->value(), rectValue, DT_LEFT | DT_SINGLELINE);

	dc.SetTextColor(crOldTextColor);
	dc.SetBkColor(crOldBkColor);

	dc.Detach();
}


void PropertyList::OnLButtonUp(UINT nFlags, CPoint point) 
{
	if (dividerMove_)
	{
		// redraw columns for new divider position
		dividerMove_ = false;

		//if mouse was captured then release it
		if (GetCapture() == this)
			::ReleaseCapture();

		::ClipCursor(NULL);

		DrawDivider(point.x);	// remove the divider
		
		//set the divider position to the new value
		dividerPos_ = point.x;

		//redraw
		Invalidate();

		selected_ = GetCurSel();
		if( selected_ != LB_ERR )
		{
			CRect rect;
			GetItemRect(selected_, rect);
			rect.left = dividerPos_;

			PropertyItem* item = (PropertyItem*) GetItemDataPtr(selected_);
			if (item->getSelectable())
				item->itemRectChange(rect);
		}
	}
	else
	{
		// select the item under the cursor
		BOOL out;
		UINT index = ItemFromPoint(point,out);
		if (!out && index != (uint16)-1)
			Select( index );

		CListBox::OnLButtonUp(nFlags, point);
	}
}


void PropertyList::OnLButtonDown(UINT nFlags, CPoint point) 
{
	BOOL out;
	int index = ItemFromPoint(point,out);
	PropertyItem* item = NULL;
	if (!out && index != (uint16)(-1))
		item = (PropertyItem*) GetItemDataPtr(index);

	if( item  &&  item->getType() == PropertyItem::Type_Group )
	{
		int xBoundUpper = item->getGroupDepth() * childIndent;
		int xBoundLower = xBoundUpper - childIndent;
		if (point.x >= xBoundLower  &&  point.x <= xBoundUpper)
		{
			GroupPropertyItem * gItem = (GroupPropertyItem *)(item);
			if (gItem->getExpanded())
				collapseGroup( gItem, index + 1 );
			else
				expandGroup( gItem, index );
		}

		// select the item
		CListBox::OnLButtonDown(nFlags, point);
	}
	else if ((point.x >= dividerPos_ - 2) && (point.x <= dividerPos_ + 2))
	{
		// get ready to resize the divider
		::SetCursor(cursorSize_);

		// keep mouse inside the control
		CRect windowRect;
		GetWindowRect(windowRect);
		windowRect.left += 10; windowRect.right -= 10;
		::ClipCursor(windowRect);
		
		// TODO: hide all sub controls


		CRect clientRect;
		GetClientRect(clientRect);
		dividerMove_ = true;
		dividerTop_ = clientRect.top;
		dividerBottom_ = clientRect.bottom;
		dividerLastX_ = point.x;

		DrawDivider(dividerLastX_);

		//capture the mouse
		SetCapture();
		SetFocus();
	}
	else
	{
		dividerMove_ = false;

		// select the item
		CListBox::OnLButtonDown(nFlags, point);
	}
}


void PropertyList::OnMouseMove(UINT nFlags, CPoint point) 
{	
	BOOL out;
	UINT index = ItemFromPoint(point,out);
	PropertyItem* item = NULL;
	if (!out && index != (uint16)(-1))
		item = (PropertyItem*) GetItemDataPtr(index);

	// handle the resizing divider
	if (dividerMove_)
	{
		//move divider line
		DrawDivider(dividerLastX_);	// remove old
		DrawDivider(point.x);			// draw new

		dividerLastX_ = point.x;
	}
	else if( item && (item->getType() != PropertyItem::Type_Group) &&
			(item->getType() != PropertyItem::Type_Label) &&
			(item->getType() != PropertyItem::Type_Label_Highlight) &&
			(point.x >= dividerPos_ - 2) && (point.x <= dividerPos_ + 2) )
	{
		//set the cursor to a sizing cursor if the cursor is over the row divider
		::SetCursor(cursorSize_);
	}
	else
	{
		CListBox::OnMouseMove(nFlags, point);
	}
}

void PropertyList::DrawDivider(int xpos)
{
	CClientDC dc(this);
	int nOldMode = dc.SetROP2(R2_NOT);	// draw inverse of screen colour
	dc.MoveTo(xpos, dividerTop_);
	dc.LineTo(xpos, dividerBottom_);
	dc.SetROP2(nOldMode);
}

void PropertyList::establishGroup( PropertyItem* item )
{
	std::string group = item->getGroup();

	// build current group
	std::string currentGroup = "";
	for (uint i = 0; i < parentGroupStack_.size(); i++)
	{
		currentGroup += parentGroupStack_[i]->name().GetBuffer();
		currentGroup += "/";
	}
	if (currentGroup != "")
		currentGroup.erase( --currentGroup.end() );

	if (currentGroup == group)
		return;

	if (group.empty())
	{
		parentGroupStack_.clear();
		return;
	}

	if (parentGroupStack_.size() == 0)
	{
		makeSubGroup( group, item );
		return;
	}
	
	// look forward from the root
	uint stackIndex = 0;
	int startIndex = 0;
	int endIndex = group.find( "/" );
	bool stackTooBig = false;
	bool stackTooSmall = false;
	while (!stackTooBig && !stackTooSmall)
	{
		std::string name = group.substr( startIndex, 
								endIndex == -1 ? -1 : endIndex - startIndex );
		if (name != parentGroupStack_[stackIndex]->name().GetBuffer())
		{
			stackTooBig = true;
			stackTooSmall = true;
		}
		else
		{
			if (endIndex == -1)
				stackTooBig = true;

			startIndex = endIndex + 1;
			endIndex = group.find( "/", startIndex );
		}

		if (++stackIndex == parentGroupStack_.size())
		{
			stackTooSmall = true;
		}
	}

	if (stackTooBig)
	{
		// pop things off the stack
		MF_ASSERT( parentGroupStack_.size() > 0 );
		if( stackTooSmall )
			stackIndex--;
		int diff = parentGroupStack_.size() - stackIndex;
		for (int i = 0; i < diff; i++)
			parentGroupStack_.pop_back();
	}

	if (stackTooSmall)
	{
		// make new subgroups
		std::string subGroup = group.substr( startIndex );
		makeSubGroup( subGroup, item );
	}
}

void PropertyList::makeSubGroup( const std::string & subGroup, PropertyItem* item )
{
	int startIndex = 0;
	int endIndex = subGroup.find( "/" );
	while (endIndex != -1)
	{
		addGroupToStack( subGroup.substr( startIndex, endIndex - startIndex ) );
		startIndex = endIndex + 1;
		endIndex = subGroup.find( "/", startIndex );
	} 
	addGroupToStack( subGroup.substr( startIndex ), item );
}

void PropertyList::addGroupToStack( const std::string & label, PropertyItem* item )
{
	int groupDepth = parentGroupStack_.size() + 1;

	GroupPropertyItem * newItem = NULL;
	if (item && item->getType() == PropertyItem::Type_Group)
	{
		MF_ASSERT( label.c_str() == item->name() );
		newItem = (GroupPropertyItem *)item;
		newItem->setGroupDepth( groupDepth );
	}
	else
	{
		newItem = new GroupPropertyItem( label.c_str(), groupDepth );

		int index = InsertString( -1, "" );
		SetItemDataPtr(index, newItem);
		newItem->create(this);
	}

	if (parentGroupStack_.size() > 0)
		parentGroupStack_.back()->addChild( newItem );
	parentGroupStack_.push_back( newItem );
}

int PropertyList::AddPropItem(PropertyItem* item)
{
	establishGroup( item );

	if (parentGroupStack_.size() > 0  &&
		item->getType() != PropertyItem::Type_Group)
	{
		parentGroupStack_.back()->addChild( item );
	}

	int index = InsertString( -1, "" );
	SetItemDataPtr(index, item);
	item->create(this);

	if( GetCurSel() == LB_ERR )
		SetCurSel( index );
	return index;
}

void PropertyList::Select(int selected)
{
	selected_ = selected;
}


void PropertyList::OnSelchange() 
{
	selected_ = GetCurSel();
	CRect rect;
	GetItemRect(selected_, rect);
	rect.left = dividerPos_;

	if ( PropertyItem::selectedItem() )
		PropertyItem::selectedItem()->deselect();

	PropertyItem* item = (PropertyItem*) GetItemDataPtr(selected_);
	if (item->getSelectable())
	{
		item->select(rect);
		GetParent()->SendMessage(WM_SELECT_PROPERTYITEM, 0, (LPARAM)(item->getChangeBuddy()));
	}
	else
	{
		GetParent()->SendMessage(WM_SELECT_PROPERTYITEM, 0, 0);
	}
}


void PropertyList::clear()
{
	if (GetSafeHwnd() == NULL)
		return;

	// tell current item it is not selected
	if ( PropertyItem::selectedItem() )
		PropertyItem::selectedItem()->deselect();

	// empty the list box
	ResetContent();
	selected_ = 0;
	parentGroupStack_.clear();

	GetParent()->SendMessage(WM_SELECT_PROPERTYITEM, 0, 0);
}


afx_msg LRESULT PropertyList::OnChangePropertyItem(WPARAM wParam, LPARAM lParam)
{
	GetParent()->SendMessage(WM_CHANGE_PROPERTYITEM, wParam, lParam);
	return 0;
}

void PropertyList::selectPrevItem()
{
	if (GetCount() == 0)
		return;

	int nextItem = selected_;
	if( nextItem == 0)
		nextItem = GetCount() - 1;
	else
		--nextItem;
	Select( nextItem );

	SetCurSel(selected_);
	OnSelchange();
}

void PropertyList::selectNextItem()
{
	if (GetCount() == 0)
		return;

	int nextItem = selected_;
	nextItem += 1;
	if (nextItem >= GetCount())
		nextItem = 0;
	Select( nextItem );

	SetCurSel(selected_);
	OnSelchange();
}

void PropertyList::deselectCurrentItem()
{
	if ( PropertyItem::selectedItem() )
		PropertyItem::selectedItem()->deselect();
}

void PropertyList::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	if ( PropertyItem::selectedItem() )
	{
		CRect rect;
		GetItemRect(selected_, rect);
		rect.left = dividerPos_;
		PropertyItem::selectedItem()->itemRectChange(rect);
		InvalidateRect( NULL );
	}
	CListBox::OnVScroll(nSBCode, nPos, pScrollBar);
}

void PropertyList::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	if( pScrollBar->GetDlgCtrlID() == IDC_PROPERTYLIST_SLIDER &&
		nSBCode != SB_ENDSCROLL )
	{
		CSliderCtrl* slider = (CSliderCtrl*)pScrollBar;

		if( nSBCode ==  SB_THUMBTRACK )
			PropertyItem::selectedItem()->sliderChange( slider->GetPos(), true );
		else
			PropertyItem::selectedItem()->sliderChange( slider->GetPos(), false );
	}
	else
		CListBox::OnHScroll(nSBCode, nPos, pScrollBar);
}

void PropertyList::OnKillFocus(CWnd* pNewWnd)
{
	if ( PropertyItem::selectedItem() )
		PropertyItem::selectedItem()->deselect();

	CListBox::OnKillFocus(pNewWnd);
}

void PropertyList::OnComboChange()
{
	if ( PropertyItem::selectedItem() )
		PropertyItem::selectedItem()->comboChange();
}

void PropertyList::OnBrowse()
{
	PropertyItem::selectedItem()->onBrowse();
}

void PropertyList::OnDefault()
{
	PropertyItem::selectedItem()->onDefault();
}

void PropertyList::OnEditChange( )
{
	if( PropertyItem::selectedItem() )
		PropertyItem::selectedItem()->editChange();
}

PropertyItem * PropertyList::getHighlightedItem()
{
	return (PropertyItem*) GetItemDataPtr(selected_);
}

void PropertyList::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	BOOL out;
	int index = ItemFromPoint(point,out);
	PropertyItem* item = 0;
	if (!out && index != (uint16)(-1))
	{
		item = (PropertyItem*) GetItemDataPtr(index);
	}
	
	GetParent()->SendMessage(WM_DBLCLK_PROPERTYITEM, 0, (LPARAM)item);
}

void PropertyList::OnRButtonUp( UINT, CPoint point )
{
	BOOL out;
	int index = ItemFromPoint(point,out);
	PropertyItem* item = 0;
	if (!out && index != (uint16)(-1))
	{
		item = (PropertyItem*) GetItemDataPtr(index);
	}

	GetParent()->SendMessage(WM_RCLK_PROPERTYITEM, 0, (LPARAM)item);
}

void PropertyList::collapseGroup(GroupPropertyItem* gItem, int index)
{
	if (!gItem->getExpanded())
		return;

	PropertyItemVector & children = gItem->getChildren();

	// hide all the children
	for (PropertyItemVector::iterator it = children.begin();
		it != children.end();
		it++)
	{
		DeleteString(index);

		// remove all of their children
		if ((*it)->getType() == PropertyItem::Type_Group)
		{
			GroupPropertyItem * g = (GroupPropertyItem *)(*it);
			collapseGroup( g, index );
		}
	}

	gItem->setExpanded( false );
}

void PropertyList::expandGroup(GroupPropertyItem* gItem, int index)
{
	if (gItem->getExpanded())
		return;

	PropertyItemVector & children = gItem->getChildren();

	// show all the children (one 1 level)
	for (PropertyItemVector::iterator it = children.begin();
		it != children.end();
		it++)
	{
		index++;
		InsertString( index, "" );
		SetItemDataPtr(index, *it);
		(*it)->create(this);
	}

	gItem->setExpanded( true );
}