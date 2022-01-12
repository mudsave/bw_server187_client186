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

#include "controls/edit_numeric.hpp"
#include "controls/slider.hpp"

// PropertyItem
class PropertyItem
{
public:
	PropertyItem(const CString& name);
	virtual ~PropertyItem() {}

	virtual void create(CWnd* parent) = 0;
	virtual void select(CRect rect) = 0;
	virtual void deselect() = 0;
	virtual void loseFocus() { deselect(); }

	CString name() { return name_; }
	CString value() { return stringValue_; }

	void setSelectable(bool option) { selectable_ = option; }
	bool getSelectable() { return selectable_; }

	void setChangeBuddy(void* buddy) { changeBuddy_ = buddy; }
	void * getChangeBuddy() { return changeBuddy_; }

	static PropertyItem* selectedItem() { return selectedItem_; }

	virtual void comboChange() {}
	virtual void onBrowse() {}
	virtual void sliderChange( int value, bool transient ) {}
	virtual void editChange() {}
	virtual void onDefault() {}
	virtual void itemRectChange( CRect rect ) {}

	enum ItemType
	{
		Type_Unknown,
		Type_Group,
		Type_Label,
		Type_Label_Highlight,
		Type_String,
		Type_String_ReadOnly,
		Type_ID
	};
	virtual ItemType getType() { return Type_Unknown; }

	void setGroup( const std::string& group );
	std::string getGroup() { return group_; }
	
	int getGroupDepth() { return groupDepth_; }

protected:
	CString name_;
	CString stringValue_;

	bool selectable_;
	static PropertyItem* selectedItem_;

	CWnd* parent_;
	void* changeBuddy_;

	std::string group_;
	int groupDepth_;
};

typedef std::vector< PropertyItem * > PropertyItemVector;


class GroupPropertyItem : public PropertyItem
{
public:
	GroupPropertyItem(const CString& name, int depth);
	virtual ~GroupPropertyItem();

	virtual void create(CWnd* parent);
	virtual void select(CRect rect);
	virtual void deselect();

	virtual ItemType getType() { return Type_Group; }

	void addChild( PropertyItem * item );

	PropertyItemVector & getChildren() { return children_; }

	void setExpanded( bool option ) { expanded_ = option; }
	bool getExpanded() { return expanded_; }

	void setGroupDepth( int depth ) { groupDepth_ = depth; }

private:
	PropertyItemVector children_;
	bool expanded_;
};


class LabelPropertyItem : public PropertyItem
{
public:
	LabelPropertyItem(const CString& name, bool highlight = false);
	virtual ~LabelPropertyItem();

	virtual void create(CWnd* parent);
	virtual void select(CRect rect);
	virtual void deselect();

	virtual ItemType getType();

private:
	bool highlight_;
};


class StringPropertyItem : public PropertyItem
{
public:
	StringPropertyItem(const CString& name, const CString& currentValue, bool readOnly = false);
	virtual ~StringPropertyItem();

	virtual void create(CWnd* parent);
	virtual void select(CRect rect);
	virtual void deselect();
	virtual void itemRectChange( CRect rect );

	void set(const std::string& value);
	std::string get();

	void setFileFilter(const std::string& value)	{	fileFilter_ = value;	}
	virtual void onBrowse();

	virtual ItemType getType();
	bool isColor() const	{	return stringValue_.GetLength() == 7 && stringValue_[0] == '#';	}
private:
	static CEdit edit_;
	static CButton button_;
	std::string fileFilter_;
	bool readOnly_;
};


class IDPropertyItem : public PropertyItem
{
public:
	IDPropertyItem(const CString& name, const CString& currentValue);
	virtual ~IDPropertyItem();

	virtual void create(CWnd* parent);
	virtual void select(CRect rect);
	virtual void deselect();
	virtual void itemRectChange( CRect rect );

	void set(const std::string& value);
	std::string get();

	virtual ItemType getType();

private:
	static CEdit edit_;
};


class ComboPropertyItem : public PropertyItem
{
public:
	ComboPropertyItem(const CString& name, CString currentValue,
				const std::vector<std::string>& possibleValues);
	ComboPropertyItem(const CString& name, int currentValueIndex,
				const std::vector<std::string>& possibleValues);
	virtual ~ComboPropertyItem();

	virtual void create(CWnd* parent);
	virtual void select(CRect rect);
	virtual void deselect();
	virtual void itemRectChange( CRect rect );
	virtual void loseFocus();

	void set(const std::string& value);
	void set(int index);
	std::string get();

	virtual void comboChange();

private:
	static CComboBox comboBox_;
	std::vector<std::string> possibleValues_;
};


class BoolPropertyItem : public PropertyItem
{
public:
	BoolPropertyItem(const CString& name, int currentValue);
	virtual ~BoolPropertyItem();

	virtual void create(CWnd* parent);
	virtual void select(CRect rect);
	virtual void deselect();
	virtual void itemRectChange( CRect rect );
	virtual void loseFocus();

	void set(bool value);
	bool get();

private:
	static CComboBox comboBox_;
	int value_;
};


class FloatPropertyItem : public PropertyItem
{
public:
	FloatPropertyItem(const CString& name, float currentValue);
	virtual ~FloatPropertyItem();

	virtual void create(CWnd* parent);
	virtual void select(CRect rect);
	virtual void deselect();
	virtual void itemRectChange( CRect rect );

	void setRange( float min, float max, int digits );
	void setDefault( float def );

	void set(float value);
	float get();

	virtual void sliderChange( int value, bool transient );
	virtual void editChange();
	virtual void onDefault();

private:
    static controls::EditNumeric editNumeric_;
	static controls::EditNumeric editNumericFormatting_;
	static controls::Slider slider_;
	static CButton button_;

	float value_;
	float min_;
	float max_;
	int digits_;
	bool ranged_;
	bool changing_;
	float def_;
	bool hasDef_;
};


class IntPropertyItem : public PropertyItem
{
public:
	IntPropertyItem(const CString& name, int currentValue);
	virtual ~IntPropertyItem();

	virtual void create(CWnd* parent);
	virtual void select(CRect rect);
	virtual void deselect();
	virtual void itemRectChange( CRect rect );

	void setRange( int min, int max );
	void set(int value);
	int get();

	virtual void sliderChange( int value, bool transient );
	virtual void editChange();
private:
    static controls::EditNumeric editNumeric_;
	static controls::EditNumeric editNumericFormatting_;
	static controls::Slider slider_;
	int value_;
	int min_;
	int max_;
	bool ranged_;
	bool changing_;
};



// PropertyList window

class PropertyList : public CListBox
{
public:
	PropertyList();
	virtual ~PropertyList();

	int AddPropItem(PropertyItem* item);

	virtual void MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct);
	virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);

	void clear();

	void selectPrevItem();
	void selectNextItem();
	void deselectCurrentItem();

	PropertyItem * getHighlightedItem();

protected:
	virtual void PreSubclassWindow();

	afx_msg void OnSelchange();
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg LRESULT OnChangePropertyItem(WPARAM wParam, LPARAM lParam);
	afx_msg void OnComboChange();
	afx_msg void OnBrowse();
	afx_msg void OnDefault();
	afx_msg void OnEditChange( ); 

	DECLARE_MESSAGE_MAP()

private:
	void DrawDivider(int xpos);
	void Select(int selected);

	int selected_;

	int dividerPos_;
	int dividerTop_;
	int dividerBottom_;
	int dividerLastX_;
	bool dividerMove_;
	HCURSOR cursorArrow_;
	HCURSOR cursorSize_;

	void collapseGroup(GroupPropertyItem* gItem, int index);
	void expandGroup(GroupPropertyItem* gItem, int index);

	std::vector< GroupPropertyItem * > parentGroupStack_;

	void establishGroup( PropertyItem* item );
	void makeSubGroup( const std::string & subGroup, PropertyItem* item );
	void addGroupToStack( const std::string & label, PropertyItem* item = NULL );

public:
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point );
	afx_msg void OnRButtonUp( UINT, CPoint );
};
