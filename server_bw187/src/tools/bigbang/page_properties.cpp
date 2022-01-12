/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// page_properties.cpp : implementation file
//

#include "pch.hpp"
#include "bigbang.h"
#include "page_properties.hpp"
#include "python_adapter.hpp"
#include "user_messages.hpp"

#include "gizmo/gizmo_manager.hpp"
#include "gizmo/general_editor.hpp"
#include "gizmo/general_properties.hpp"


DECLARE_DEBUG_COMPONENT( 0 )

// the views
// these are the worldbuilder interface to selected item properties


class BaseView : public GeneralProperty::View
{
public:
	BaseView() {}

	typedef std::vector<PropertyItem*> PropertyItems;
	PropertyItems& propertyItems() { return propertyItems_; }

	virtual void onChange(bool transient) = 0;
	virtual void updateGUI() = 0;

	virtual void __stdcall deleteSelf()
	{
		for (PropertyItems::iterator it = propertyItems_.begin();
			it != propertyItems_.end();
			it++)
		{
			delete *it;
		}
		propertyItems_.clear();

		GeneralProperty::View::deleteSelf();
	}

	//	TODO: Shouldn't do this for each expel call. Just once when all are expelled.
	virtual void __stdcall expel() { PageProperties::instance().clear(); }

	virtual void __stdcall select() {};

	virtual void onSelect() = 0;

	virtual PropertyManagerPtr getPropertyManger() const { return NULL; }
	virtual void setToDefault(){}
	virtual bool isDefault(){	return false;	}
protected:
	PropertyItems propertyItems_;
};


// text
class TextView : public BaseView
{
public:
	TextView( TextProperty & property )
		: property_( property )
	{
	}

	StringPropertyItem* item() { return (StringPropertyItem*)&*(propertyItems_.front()); }

	virtual void __stdcall elect()
	{
		oldValue_ = getCurrentValue();

		StringPropertyItem* newItem = new StringPropertyItem(property_.name(), oldValue_.c_str());
		newItem->setGroup( property_.getGroup() );
		newItem->setChangeBuddy(this);
		newItem->setFileFilter( property_.fileFilter() );
		propertyItems_.push_back(newItem);
		PageProperties::instance().addView( this );
	}

	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange(bool transient)
	{
        std::string s = item()->get();

        if (s != oldValue_)
        {
            setCurrentValue( s );
            oldValue_ = s;
        }
	}

	virtual void updateGUI()
	{
        std::string s = getCurrentValue();

        if (s != oldValue_)
        {
            oldValue_ = s;
			item()->set(s);
        }
	}

	static TextProperty::View * create( TextProperty & property )
	{
		return new TextView( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

private:
    std::string getCurrentValue()
    {
		PyObject* pValue = property_.pyGet();
        if (!pValue)
            return "";

        std::string s = "";

        PyObject * pAsString = PyObject_Str( pValue );

		if (pAsString)
		{
			s = PyString_AsString( pAsString );

			Py_DECREF( pAsString );
		}

		Py_DECREF( pValue );

        return s;
    }

    void setCurrentValue(std::string s)
    {
        property_.pySet( Py_BuildValue( "s", s.c_str() ) );
    }


	TextProperty & property_;
	std::string oldValue_;

	class Enroller {
		Enroller() {
			TextProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};

TextView::Enroller TextView::Enroller::s_instance;



// static text
class StaticTextView : public BaseView
{
public:
	StaticTextView( StaticTextProperty & property )
		: property_( property )
	{
	}

	StringPropertyItem* item() { return (StringPropertyItem*)&*(propertyItems_.front()); }

	virtual void __stdcall elect()
	{
		oldValue_ = getCurrentValue();

		PropertyItem* newItem = new StringPropertyItem(property_.name(), oldValue_.c_str(), true);
		newItem->setGroup( property_.getGroup() );
		newItem->setChangeBuddy(this);
		propertyItems_.push_back(newItem);
		PageProperties::instance().addView( this );
	}

	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange(bool transient)
	{
        std::string s = item()->get();

        if (s != oldValue_)
        {
            setCurrentValue( s );
            oldValue_ = s;
        }
	}

	virtual void updateGUI()
	{
        std::string s = getCurrentValue();

        if (s != oldValue_)
        {
            oldValue_ = s;
			item()->set(s);
        }
	}

	static StaticTextProperty::View * create( StaticTextProperty & property )
	{
		return new StaticTextView( property );
	}

    void setCurrentValue(std::string s)
    {
        property_.pySet( Py_BuildValue( "s", s.c_str() ) );
    }

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

private:
    std::string getCurrentValue()
    {
		PyObject* pValue = property_.pyGet();
        if (!pValue)
            return "";

        std::string s = "";

        PyObject * pAsString = PyObject_Str( pValue );

		if (pAsString)
		{
			s = PyString_AsString( pAsString );

			Py_DECREF( pAsString );
		}

		Py_DECREF( pValue );

        return s;
    }

	StaticTextProperty & property_;
	std::string oldValue_;

	class Enroller {
		Enroller() {
			StaticTextProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};

StaticTextView::Enroller StaticTextView::Enroller::s_instance;



// static text
class TextLabelView : public BaseView
{
public:
	TextLabelView( TextLabelProperty & property )
		: property_( property )
	{
	}

	LabelPropertyItem* item() { return (LabelPropertyItem*)&*(propertyItems_.front()); }

	virtual void __stdcall elect()
	{
		PropertyItem* newItem = new LabelPropertyItem(property_.name(), property_.highlight());
		newItem->setGroup( property_.getGroup() );
		newItem->setChangeBuddy(this);
		propertyItems_.push_back(newItem);
		PageProperties::instance().addView( this );
	}

	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange(bool transient)
	{
	}

	virtual void updateGUI()
	{
	}

	static TextLabelProperty::View * create( TextLabelProperty & property )
	{
		return new TextLabelView( property );
	}

    void setCurrentValue(std::string s)
    {
    }

	void * getUserObject()
	{
		return property_.getUserObject();
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

private:
	TextLabelProperty & property_;

	class Enroller {
		Enroller() {
			TextLabelProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};

TextLabelView::Enroller TextLabelView::Enroller::s_instance;


// id view
class IDView : public BaseView
{
public:
	IDView( IDProperty & property )
		: property_( property )
	{
	}

	IDPropertyItem* item() { return (IDPropertyItem*)&*(propertyItems_.front()); }

	virtual void __stdcall elect()
	{
		oldValue_ = getCurrentValue();

		PropertyItem* newItem = new IDPropertyItem(property_.name(), oldValue_.c_str());
		newItem->setGroup( property_.getGroup() );
		newItem->setChangeBuddy(this);
		propertyItems_.push_back(newItem);
		PageProperties::instance().addView( this );
	}

	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange(bool transient)
	{
        std::string s = item()->get();

        if (s != oldValue_)
        {
            setCurrentValue( s );
            oldValue_ = s;
        }
	}

	virtual void updateGUI()
	{
        std::string s = getCurrentValue();

        if (s != oldValue_)
        {
            oldValue_ = s;
			item()->set(s);
        }
	}

	static IDProperty::View * create( IDProperty & property )
	{
		return new IDView( property );
	}

    void setCurrentValue(std::string s)
    {
        property_.pySet( Py_BuildValue( "s", s.c_str() ) );
    }

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

private:
    std::string getCurrentValue()
    {
		PyObject* pValue = property_.pyGet();
        if (!pValue)
            return "";

        std::string s = "";

        PyObject * pAsString = PyObject_Str( pValue );

		if (pAsString)
		{
			s = PyString_AsString( pAsString );

			Py_DECREF( pAsString );
		}

		Py_DECREF( pValue );

        return s;
    }

	IDProperty & property_;
	std::string oldValue_;

	class Enroller {
		Enroller() {
			IDProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};

IDView::Enroller IDView::Enroller::s_instance;



// group view
class GroupView : public BaseView
{
public:
	GroupView( GroupProperty & property )
		: property_( property )
	{
	}

	GroupPropertyItem* item() { return (GroupPropertyItem*)&*(propertyItems_.front()); }

	virtual void __stdcall elect()
	{
		PropertyItem* newItem = new GroupPropertyItem(property_.name(), -1);
		newItem->setGroup( property_.getGroup() );
		newItem->setChangeBuddy(this);
		propertyItems_.push_back(newItem);
		PageProperties::instance().addView( this );
	}

	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange(bool transient)
	{
	}

	virtual void updateGUI()
	{
	}

	static GroupProperty::View * create( GroupProperty & property )
	{
		return new GroupView( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

private:
	GroupProperty & property_;

	class Enroller {
		Enroller() {
			GroupProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};

GroupView::Enroller GroupView::Enroller::s_instance;


// list of text items
class ListTextView : public BaseView
{
public:
	ListTextView( ListTextProperty & property )
		: property_( property )
	{
	}

	ComboPropertyItem* item() { return (ComboPropertyItem*)&*(propertyItems_.front()); }

	virtual void __stdcall elect()
	{
		oldValue_ = getCurrentValue();

		PropertyItem* newItem = new ComboPropertyItem(property_.name(),
													oldValue_.c_str(),
													property_.possibleValues());
		newItem->setGroup( property_.getGroup() );
		newItem->setChangeBuddy(this);
		propertyItems_.push_back(newItem);
		PageProperties::instance().addView( this );
	}

	virtual void __stdcall expel()
	{
		item()->setChangeBuddy(NULL);
	}

	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange(bool transient)
	{
        std::string s = item()->get();

        if (s != oldValue_)
        {
            setCurrentValue( s );
            oldValue_ = s;
        }
	}

	virtual void updateGUI()
	{
        std::string s = getCurrentValue();

        if (s != oldValue_)
        {
            oldValue_ = s;
			item()->set(s);
        }
	}

	static ListTextProperty::View * create( ListTextProperty & property )
	{
		return new ListTextView( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

private:
    std::string getCurrentValue()
    {
		PyObject* pValue = property_.pyGet();
        if (!pValue)
            return "";

        std::string s = "";

        PyObject * pAsString = PyObject_Str( pValue );

		if (pAsString)
		{
			s = PyString_AsString( pAsString );

			Py_DECREF( pAsString );
		}

		Py_DECREF( pValue );

        return s;
    }

    void setCurrentValue(std::string s)
    {
        property_.pySet( Py_BuildValue( "s", s.c_str() ) );
    }


	ListTextProperty & property_;
	std::string oldValue_;

	class Enroller {
		Enroller() {
			ListTextProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};

ListTextView::Enroller ListTextView::Enroller::s_instance;


// list of text items associated to an int field
class ChoiceView : public BaseView
{
public:
	ChoiceView( ChoiceProperty & property )
		: property_( property )
	{
	}

	ComboPropertyItem* item() { return (ComboPropertyItem*)&*(propertyItems_.front()); }

	virtual void __stdcall elect()
	{
		oldValue_ = property_.pInt()->get();

		std::vector<std::string> possibleValues;

		std::string oldStringValue = "";

		DataSectionPtr choices = property_.pChoices();
		for (DataSectionIterator iter = choices->begin();
			iter != choices->end();
			iter++)
		{
			DataSectionPtr pDS = *iter;
			choices_[ pDS->sectionName().c_str() ] = pDS->asInt( 0 );
			possibleValues.push_back( pDS->sectionName() );

			if (pDS->asInt( 0 ) == oldValue_)
			{
				MF_ASSERT( oldStringValue.empty() );
				oldStringValue = pDS->sectionName().c_str();
			}
		}

		// make sure the old string value is valid, otherwise select the first valid value
		bool setDefault = oldStringValue.empty();
		if (setDefault)
		{
			oldStringValue = possibleValues.front();
			INFO_MSG( "Setting default value of %s for %s\n", oldStringValue.c_str(), property_.name() );
		}

		PropertyItem* newItem = new ComboPropertyItem(property_.name(),
														oldStringValue.c_str(),
														possibleValues);
		newItem->setGroup( property_.getGroup() );
		newItem->setChangeBuddy(this);
		propertyItems_.push_back(newItem);
		PageProperties::instance().addView( this );

		if (setDefault)
			onChange(false);		// update the actual object property
	}

	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange(bool transient)
	{
		std::string s = item()->get();
		int v = choices_.find( s )->second;

		if (v != oldValue_)
		{
			property_.pInt()->set( v, transient );
			oldValue_ = v;
		}
	}

	virtual void updateGUI()
	{
		const int newValue = property_.pInt()->get();

		if (newValue != oldValue_)
		{
			oldValue_ = newValue;

			// set the combobox based on string as the int values may be disparate
			std::string strValue = "";
			for ( std::map<std::string, int>::iterator it = choices_.begin();
				it != choices_.end();
				it++ )
			{
				if (it->second == newValue)
				{
					strValue = it->first;
					break;
				}
			}
			MF_ASSERT( !strValue.empty() );

			item()->set(strValue);
		}
	}

	static ChoiceProperty::View * create( ChoiceProperty & property )
	{
		return new ChoiceView( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

private:
	ChoiceProperty & property_;
	int oldValue_;
	std::map<std::string, int> choices_;

	class Enroller {
		Enroller() {
			ChoiceProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};

ChoiceView::Enroller ChoiceView::Enroller::s_instance;



// bool
class GenBoolView : public BaseView
{
public:
	GenBoolView( GenBoolProperty & property ) : property_( property )
	{
	}

	BoolPropertyItem* item() { return (BoolPropertyItem*)&*(propertyItems_.front()); }

	virtual void __stdcall elect()
	{
		oldValue_ = property_.pBool()->get();

		PropertyItem* newItem = new BoolPropertyItem(property_.name(), oldValue_);
		newItem->setGroup( property_.getGroup() );
		newItem->setChangeBuddy(this);
		propertyItems_.push_back(newItem);
		PageProperties::instance().addView( this );
	}

	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange(bool transient)
	{
		bool newValue = item()->get();
		property_.pBool()->set( newValue, transient );
		oldValue_ = newValue;
	}

	virtual void updateGUI()
	{
		const bool newValue = property_.pBool()->get();

		if (newValue != oldValue_)
		{
			oldValue_ = newValue;
			item()->set(newValue);
		}
	}

	static GenFloatProperty::View * create( GenBoolProperty & property )
	{
		return new GenBoolView( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

private:
	GenBoolProperty & property_;
	bool oldValue_;

	class Enroller {
		Enroller() {
			GenBoolProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};

GenBoolView::Enroller GenBoolView::Enroller::s_instance;


// float
class GenFloatView : public BaseView
{
public:
	GenFloatView( GenFloatProperty & property )
		: property_( property )
	{
	}

	FloatPropertyItem* item() { return (FloatPropertyItem*)&*(propertyItems_.front()); }

	virtual void __stdcall elect()
	{
		oldValue_ = property_.pFloat()->get();

		PropertyItem* newItem = new FloatPropertyItem(property_.name(), property_.pFloat()->get());
		newItem->setGroup( property_.getGroup() );
		newItem->setChangeBuddy(this);
		float min, max;
		int digits;
		if(property_.pFloat()->getRange( min, max, digits ))
			((FloatPropertyItem*)newItem)->setRange( min, max, digits );
		float def;
		if(property_.pFloat()->getDefault( def ))
			((FloatPropertyItem*)newItem)->setDefault( def );
		if( property_.name() == std::string("multiplier") )
			((FloatPropertyItem*)newItem)->setRange( 0, 3, 1 );
		propertyItems_.push_back(newItem);
		PageProperties::instance().addView( this );
	}

	virtual void setToDefault()
	{
		property_.pFloat()->setToDefault();
	}

	virtual bool isDefault()
	{
		return property_.pFloat()->isDefault();
	}

	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange(bool transient)
	{
		float newValue = item()->get();
		property_.pFloat()->set( newValue, transient );
		oldValue_ = newValue;
	}

	virtual void updateGUI()
	{
		const float newValue = property_.pFloat()->get();

		if (newValue != oldValue_)
		{
			oldValue_ = newValue;
			item()->set(newValue);
		}
	}

	static GenFloatProperty::View * create( GenFloatProperty & property )
	{
		return new GenFloatView( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

protected:

private:
	GenFloatProperty & property_;
	float oldValue_;

	class Enroller {
		Enroller() {
			GenFloatProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};

GenFloatView::Enroller GenFloatView::Enroller::s_instance;


// int
class GenIntView : public BaseView
{
public:
	GenIntView( GenIntProperty & property )
		: property_( property )
	{
	}

	IntPropertyItem* item() { return (IntPropertyItem*)&*(propertyItems_.front()); }

	virtual void __stdcall elect()
	{
		oldValue_ = property_.pInt()->get();

		PropertyItem* newItem = new IntPropertyItem(property_.name(), property_.pInt()->get());
		newItem->setGroup( property_.getGroup() );
		newItem->setChangeBuddy(this);
		int min, max;
		if(property_.pInt()->getRange( min, max ))
			((IntPropertyItem*)newItem)->setRange( min, max );
		propertyItems_.push_back(newItem);
		PageProperties::instance().addView( this );
	}

	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange(bool transient)
	{
		int newValue = item()->get();
		property_.pInt()->set( newValue, transient );
		oldValue_ = newValue;
	}

	virtual void updateGUI()
	{
		const int newValue = property_.pInt()->get();

		if (newValue != oldValue_)
		{
			oldValue_ = newValue;
			item()->set(newValue);
		}
	}

	static GenIntProperty::View * create( GenIntProperty & property )
	{
		return new GenIntView( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

protected:

private:
	GenIntProperty & property_;
	int oldValue_;

	class Enroller {
		Enroller() {
			GenIntProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};

GenIntView::Enroller GenIntView::Enroller::s_instance;


// vector3 position
class GenPositionView : public BaseView
{
public:
	GenPositionView( GenPositionProperty & property )
		: property_( property )
	{
	}

	FloatPropertyItem* itemX() { return (FloatPropertyItem*)&*(propertyItems_[0]); }
	FloatPropertyItem* itemY() { return (FloatPropertyItem*)&*(propertyItems_[1]); }
	FloatPropertyItem* itemZ() { return (FloatPropertyItem*)&*(propertyItems_[2]); }

	virtual void __stdcall elect()
	{
		Matrix matrix;
		property_.pMatrix()->getMatrix( matrix );
		oldValue_ = matrix.applyToOrigin();

		CString name = property_.name();
		propertyItems_.reserve(3);
		FloatPropertyItem* newItem = new FloatPropertyItem(name + ".x", oldValue_.x);
		newItem->setGroup( property_.getGroup() );
		newItem->setChangeBuddy(this);
		propertyItems_.push_back(newItem);
		newItem = new FloatPropertyItem(name + ".y", oldValue_.y);
		newItem->setGroup( property_.getGroup() );
		newItem->setChangeBuddy(this);
		propertyItems_.push_back(newItem);
		newItem = new FloatPropertyItem(name + ".z", oldValue_.z);
		newItem->setGroup( property_.getGroup() );
		newItem->setChangeBuddy(this);
		propertyItems_.push_back(newItem);

		PageProperties::instance().addView( this );
	}

	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange(bool transient)
	{
		Vector3 newValue;
		newValue.x = itemX()->get();
		newValue.y = itemY()->get();
		newValue.z = itemZ()->get();

		Matrix matrix, ctxInv;
		property_.pMatrix()->recordState();
		property_.pMatrix()->getMatrix( matrix, false );
		property_.pMatrix()->getMatrixContextInverse( ctxInv );
		matrix.translation( ctxInv.applyPoint( newValue ) );

		if( property_.pMatrix()->setMatrix( matrix ) )
			property_.pMatrix()->commitState();
		else
		{
			Matrix matrix;
			property_.pMatrix()->getMatrix( matrix );
			itemX()->set( matrix.applyToOrigin().x );
			itemY()->set( matrix.applyToOrigin().y );
			itemZ()->set( matrix.applyToOrigin().z );
		}
	}

	virtual void updateGUI()
	{
		Matrix matrix;
		property_.pMatrix()->getMatrix( matrix );
		const Vector3 & newValue = matrix.applyToOrigin();

		if (newValue != oldValue_)
		{
			oldValue_ = newValue;

			itemX()->set(newValue.x);
			itemY()->set(newValue.y);
			itemZ()->set(newValue.z);
		}
	}

	static GenPositionProperty::View * create( GenPositionProperty & property )
	{
		return new GenPositionView( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

private:
	GenPositionProperty & property_;
	Vector3 oldValue_;

	class Enroller {
		Enroller() {
			GenPositionProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};

GenPositionView::Enroller GenPositionView::Enroller::s_instance;


// rotation
class GenRotationView : public BaseView
{
public:
	GenRotationView( GenRotationProperty & property )
		: property_( property )
	{
	}

	FloatPropertyItem* itemPitch() { return (FloatPropertyItem*)&*(propertyItems_[0]); }
	FloatPropertyItem* itemYaw() { return (FloatPropertyItem*)&*(propertyItems_[1]); }
	FloatPropertyItem* itemRoll() { return (FloatPropertyItem*)&*(propertyItems_[2]); }

	virtual void __stdcall elect()
	{
		oldValue_ = rotation();

		CString name = property_.name();
		propertyItems_.reserve(3);
		FloatPropertyItem* newItem = new FloatPropertyItem(name + " Pitch", oldValue_.x);
		newItem->setGroup( property_.getGroup() );
		newItem->setChangeBuddy(this);
		propertyItems_.push_back(newItem);
		newItem = new FloatPropertyItem(name + " Yaw", oldValue_.y);
		newItem->setGroup( property_.getGroup() );
		newItem->setChangeBuddy(this);
		propertyItems_.push_back(newItem);
		newItem = new FloatPropertyItem(name + " Roll", oldValue_.z);
		newItem->setGroup( property_.getGroup() );
		newItem->setChangeBuddy(this);
		propertyItems_.push_back(newItem);

		PageProperties::instance().addView( this );
	}

	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange(bool transient)
	{
		Matrix prevMatrix;
		property_.pMatrix()->recordState();
		property_.pMatrix()->getMatrix( prevMatrix, false );

		Matrix newMatrix, temp;
		newMatrix.setScale(
			prevMatrix.applyToUnitAxisVector( X_AXIS ).length(),
			prevMatrix.applyToUnitAxisVector( Y_AXIS ).length(),
			prevMatrix.applyToUnitAxisVector( Z_AXIS ).length() );

		temp.setRotate(
			DEG_TO_RAD( itemYaw()->get() ),
			DEG_TO_RAD( itemPitch()->get() ),
			DEG_TO_RAD( itemRoll()->get() ) );
		newMatrix.postMultiply( temp );

		temp.setTranslate( prevMatrix.applyToOrigin() );

		newMatrix.postMultiply( temp );

		if( property_.pMatrix()->setMatrix( newMatrix ) )
			property_.pMatrix()->commitState();
		else
		{
			itemPitch()->set( rotation().x );
			itemYaw()->set( rotation().y );
			itemRoll()->set( rotation().z );
		}
	}

	virtual void updateGUI()
	{
		const Vector3 newValue = rotation();

		if (newValue != oldValue_)
		{
			oldValue_ = newValue;
			itemPitch()->set(newValue.x);
			itemYaw()->set(newValue.y);
			itemRoll()->set(newValue.z);
		}
	}

	static GenRotationProperty::View * create( GenRotationProperty & property )
	{
		return new GenRotationView( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

private:
	Vector3 rotation() const
	{
		Matrix matrix;
		property_.pMatrix()->getMatrix( matrix, false );

		return Vector3(
			RAD_TO_DEG( matrix.pitch() ),
			RAD_TO_DEG( matrix.yaw() ),
			RAD_TO_DEG( matrix.roll() ) );
	}

	GenRotationProperty & property_;
	Vector3 oldValue_;

	class Enroller {
		Enroller() {
			GenRotationProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};

GenRotationView::Enroller GenRotationView::Enroller::s_instance;


// scale
class GenScaleView : public BaseView
{
public:
	GenScaleView( GenScaleProperty & property )
		: property_( property )
	{
	}

	FloatPropertyItem* itemX() { return (FloatPropertyItem*)&*(propertyItems_[0]); }
	FloatPropertyItem* itemY() { return (FloatPropertyItem*)&*(propertyItems_[1]); }
	FloatPropertyItem* itemZ() { return (FloatPropertyItem*)&*(propertyItems_[2]); }

	virtual void __stdcall elect()
	{
		oldValue_ = scale();

		CString name = property_.name();
		propertyItems_.reserve(3);
		FloatPropertyItem* newItem = new FloatPropertyItem(name + ".x", oldValue_.x);
		newItem->setGroup( property_.getGroup() );
		newItem->setChangeBuddy(this);
		propertyItems_.push_back(newItem);
		newItem = new FloatPropertyItem(name + ".y", oldValue_.y);
		newItem->setGroup( property_.getGroup() );
		newItem->setChangeBuddy(this);
		propertyItems_.push_back(newItem);
		newItem = new FloatPropertyItem(name + ".z", oldValue_.z);
		newItem->setGroup( property_.getGroup() );
		newItem->setChangeBuddy(this);
		propertyItems_.push_back(newItem);

		PageProperties::instance().addView( this );
	}

	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange(bool transient)
	{
		Vector3 newValue;
		newValue.x = itemX()->get();
		newValue.y = itemY()->get();
		newValue.z = itemZ()->get();

		if (oldValue_.x != 0.f &&
			oldValue_.y != 0.f &&
			oldValue_.z != 0.f &&
			newValue.x != 0.f &&
			newValue.y != 0.f &&
			newValue.z != 0.f)
		{
			Matrix matrix;
			property_.pMatrix()->recordState();
			property_.pMatrix()->getMatrix( matrix, false );
			Matrix scaleMatrix;
			scaleMatrix.setScale(
				newValue.x/oldValue_.x,
				newValue.y/oldValue_.y,
				newValue.z/oldValue_.z );
			matrix.preMultiply( scaleMatrix );

			if( property_.pMatrix()->setMatrix( matrix ) )
				property_.pMatrix()->commitState();
			else
			{
				itemX()->set( scale().x );
				itemY()->set( scale().y );
				itemZ()->set( scale().z );
			}
		}
	}

	virtual void updateGUI()
	{
		const Vector3 newValue = this->scale();

		if (newValue != oldValue_)
		{
			oldValue_ = newValue;

			itemX()->set(newValue.x);
			itemY()->set(newValue.y);
			itemZ()->set(newValue.z);
		}
	}

	static GenScaleProperty::View * create( GenScaleProperty & property )
	{
		return new GenScaleView( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

private:
	Vector3 scale() const
	{
		Matrix matrix;
		property_.pMatrix()->getMatrix( matrix );
		return Vector3(
			matrix.applyToUnitAxisVector( X_AXIS ).length(),
			matrix.applyToUnitAxisVector( Y_AXIS ).length(),
			matrix.applyToUnitAxisVector( Z_AXIS ).length() );
	}

	GenScaleProperty & property_;
	Vector3 oldValue_;

	class Enroller {
		Enroller() {
			GenScaleProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};

GenScaleView::Enroller GenScaleView::Enroller::s_instance;


// colour
class ColourView : public BaseView
{
public:
	ColourView( ColourProperty & property )
		: property_( property ), transiented_( false )
	{
		PageProperties::instance().enableColour();
		selectedProperty_ = &property_;
	}

	~ColourView()
	{
		PageProperties::instance().disableColour();
	}

	StringPropertyItem* item() { return (StringPropertyItem*)&*(propertyItems_[0]); }

	virtual void __stdcall elect()
	{
		Moo::Colour c;
		if (!getCurrentValue( c ))
			return;

		oldValue_ = c;

		char buf[256];
		formatString( buf, c );
		StringPropertyItem* newItem = new StringPropertyItem(property_.name(), buf);
		newItem->setGroup( property_.getGroup() );
		newItem->setChangeBuddy(this);
		propertyItems_.push_back(newItem);

		PageProperties::instance().addView( this );
		PageProperties::instance().setSliderColour( c );
		PageProperties::instance().setOldSliderColour( c );
	}

	virtual void onSelect()
	{
		selectedProperty_ = &property_;

		property_.select();

		Moo::Colour c;
		if (!getCurrentValue( c ))
			return;

		PageProperties::instance().setSliderColour( c );
		PageProperties::instance().setOldSliderColour( c );
	}

	virtual void onChange(bool transient)
	{
		Moo::Colour c;
		if (!fromString( item()->get().c_str(), c ))
			return;

		if (!equal( c, oldValue_ ))
		{
			transiented_ = transient;
			setCurrentValue( c, transient );
			oldValue_ = c;

			PageProperties::instance().setSliderColour( c );
		}
	}

	virtual void updateGUI()
	{
		Moo::Colour c;
		if (!getCurrentValue( c ))
			return;

		if (selectedProperty_ != &property_)
			return;

		// Check if colour has been changed in the slider control
		Moo::Colour sliderColour = PageProperties::instance().getSliderColour();
		Moo::Colour oldSliderColour = PageProperties::instance().getOldSliderColour();

		if (equal( c, oldValue_ ) && !equal( sliderColour, oldSliderColour ))
		{
			if (!equal( sliderColour, oldValue_ ))
			{
				transiented_ = PageProperties::instance().isTracking();
				setCurrentValue( sliderColour, transiented_ );
			}
		}
		if( transiented_ && !PageProperties::instance().isTracking() )
		{
			transiented_ = false;
			setCurrentValue( sliderColour, false );
		}

		if (!equal( c, oldValue_ ))
		{
			oldValue_ = c;

			char buf[256];
			formatString( buf, c );

			item()->set(buf);
		}
	}

	static ColourProperty::View * create( ColourProperty & property )
	{
		return new ColourView( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

private:
	Vector3 toVector( Moo::Colour c )
	{
		return Vector3(c.r, c.g, c.b);
	}

	Moo::Colour toColour( Vector3& v )
	{
		Moo::Colour c;
		c.r = v.x;
		c.g = v.y;
		c.b = v.z;
		c.a = 255.f;
	}

	bool fromString( const char* s, Moo::Colour& c )
	{
		if (*s == '#')
			s++;

		if (strlen(s) != 6)
			return false;

		char* endptr;

		char buf[3];
		buf[2] = '\0';

		buf[0] = s[0];
		buf[1] = s[1];
		c.r = (float)strtol(buf, &endptr, 16);

		buf[0] = s[2];
		buf[1] = s[3];
		c.g = (float)strtol(buf, &endptr, 16);

		buf[0] = s[4];
		buf[1] = s[5];
		c.b = (float)strtol(buf, &endptr, 16);

		c.a = 255.f;

		return true;
	}

	void setCurrentValue(Moo::Colour& c, bool transient)
	{
		property_.pySet( Py_BuildValue( "ffff", c.r, c.g, c.b, 255.f ), transient );
	}

	bool getCurrentValue(Moo::Colour& c)
	{
		PyObject* pValue = property_.pyGet();
		if (!pValue) {
			PyErr_Clear();
			return false;
		}

		// Using !PyVector<Vector4>::Check( pValue ) doesn't work so we do it
		// this dodgy way
		if (std::string(pValue->ob_type->tp_name) != "Math.Vector4")
		{
			PyErr_SetString( PyExc_TypeError, "ColourView::getCurrentValue() "
				"expects a PyVector<Vector4>" );

    		Py_DECREF( pValue );
			return false;
		}

		PyVector<Vector4>* pv = static_cast<PyVector<Vector4>*>( pValue );
		Vector4 v = pv->getVector();

		c.r = v[0];
		c.g = v[1];
		c.b = v[2];
		c.a = v[3];

		Py_DECREF( pValue );
		return true;
	}

	void formatString( char* buf, Moo::Colour c )
	{
		sprintf( buf, "#%02X%02X%02X", (int) c.r, (int) c.g, (int) c.b );
	}

	bool equal( Moo::Colour c1, Moo::Colour c2 )
	{
		return ((int) c1.r == (int) c2.r &&
			(int) c1.g == (int) c2.g &&
			(int) c1.b == (int) c2.b &&
			(int) c1.a == (int) c2.a);
	}

	static ColourProperty * selectedProperty_;

	ColourProperty & property_;
	Moo::Colour oldValue_;
	bool transiented_;
	class Enroller {
		Enroller() {
			ColourProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};

ColourProperty* ColourView::selectedProperty_;
ColourView::Enroller ColourView::Enroller::s_instance;


class MultiplierFloatView : public BaseView
{
public:
	MultiplierFloatView( GenFloatProperty & property )
		: property_( property ), transiented_( false )
	{
		isMultiplier_ = property.name() == std::string("multiplier");
		if( isMultiplier_ )
			PageProperties::instance().enableMultiplier();
	}
	~MultiplierFloatView()
	{
		if( isMultiplier_ )
			PageProperties::instance().disableMultiplier();
	}

	//FloatPropertyItem* item() { return (FloatPropertyItem*)&*(propertyItems_.front()); }

	virtual void __stdcall elect()
	{
		if (!isMultiplier_)
			return;

		oldValue_ = property_.pFloat()->get();


		PageProperties::instance().addView( this );
		PageProperties::instance().setSliderMultiplier( oldValue_ );
		PageProperties::instance().setOldSliderMultiplier( oldValue_ );

		lastSeenSliderValue_ = oldValue_;



		PageProperties::instance().addView( this );
	}

	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange(bool transient)
	{
		if (!isMultiplier_)
			return;

		float f = PageProperties::instance().getSliderMultiplier();

		if (!sameMultiplier(f, oldValue_ ))
		{
			oldValue_ = f;

			PageProperties::instance().setSliderMultiplier( f );
		}

		property_.pFloat()->set( f, false );
		oldValue_ = f;
	}

	virtual void updateGUI()
	{
		if (!isMultiplier_)
			return;

		const float newValue = property_.pFloat()->get();

		const float curSlider = PageProperties::instance().getSliderMultiplier();
		const float oldSlider = PageProperties::instance().getOldSliderMultiplier();

		if (!sameMultiplier(newValue, oldValue_))
		{
			PageProperties::instance().setSliderMultiplier( newValue );
			PageProperties::instance().setOldSliderMultiplier( newValue );
			oldValue_ = newValue;
			lastSeenSliderValue_ = newValue;
			return;
		}

		if (!sameMultiplier(curSlider, lastSeenSliderValue_)&& !sameMultiplier(curSlider, oldSlider))
		{
			if (!sameMultiplier(curSlider, newValue))
			{
				transiented_ = PageProperties::instance().isTracking();
				property_.pFloat()->set( curSlider, transiented_ );
				oldValue_ = curSlider;
				lastSeenSliderValue_ = curSlider;
			}
		}
		if( transiented_ && !PageProperties::instance().isTracking() )
		{
			transiented_ = false;
			property_.pFloat()->set( curSlider, false );
		}
	}

	static GenFloatProperty::View * create( GenFloatProperty & property )
	{
		GenFloatProperty::View* view = new MultiplierFloatView( property );
		return view;
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

protected:
	bool sameMultiplier( float f1, float f2 )
	{
		return int( ( f1 + 0.05 ) * 10 ) == int( ( f2 + 0.05 ) * 10 );
	}
private:
	bool isMultiplier_;
	GenFloatProperty & property_;
	bool transiented_;
	float oldValue_;

	float lastSeenSliderValue_;

	class Enroller {
		Enroller() {
			GenFloatProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};

MultiplierFloatView::Enroller MultiplierFloatView::Enroller::s_instance;


// Python
class PythonView : public BaseView
{
public:
	PythonView( PythonProperty & property )
		: property_( property )
	{
	}

	StringPropertyItem* item() { return (StringPropertyItem*)&*(propertyItems_.front()); }

	virtual void __stdcall elect()
	{
		oldValue_ = getCurrentValue();

		StringPropertyItem* newItem = new StringPropertyItem(property_.name(), oldValue_.c_str());
		newItem->setGroup( property_.getGroup() );
		newItem->setChangeBuddy(this);
		// newItem->setFileFilter( property_.fileFilter() );
		propertyItems_.push_back(newItem);
		PageProperties::instance().addView( this );
	}

	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange(bool transient)
	{
        std::string s = item()->get();

        if (s != oldValue_)
        {
            if (this->setCurrentValue( s ))
			{
				oldValue_ = s;
			}
        }
	}

	virtual void updateGUI()
	{
        std::string s = this->getCurrentValue();

        if (s != oldValue_)
        {
			if (this->setCurrentValue( s ))
			{
				oldValue_ = s;
			}
        }
	}

	static PythonProperty::View * create( PythonProperty & property )
	{
		return new PythonView( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

private:
    std::string getCurrentValue()
    {
		PyObject* pValue = property_.pyGet();
        if (!pValue)
		{
			PyErr_Clear();
            return "";
		}

        std::string s = "";

        PyObject * pAsString = PyObject_Repr( pValue );

		if (pAsString)
		{
			s = PyString_AsString( pAsString );

			Py_DECREF( pAsString );
		}

		Py_DECREF( pValue );

        return s;
    }

    bool setCurrentValue( std::string s )
    {
		PyObject * pNew = Script::runString( s.c_str(), false );

		if (pNew)
		{
			property_.pySet( pNew );
			// This may be slightly differnt to s
			std::string newStr =  this->getCurrentValue();
			this->item()->set( newStr );
			oldValue_ = newStr;
			Py_DECREF( pNew );
		}
		else
		{
			PyErr_Clear();
			return false;
		}

		return true;
    }


	PythonProperty & property_;
	std::string oldValue_;

	class Enroller {
		Enroller() {
			PythonProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};

PythonView::Enroller PythonView::Enroller::s_instance;











// PageProperties
PageProperties * PageProperties::instance_ = NULL;


// GUITABS content ID ( declared by the IMPLEMENT_BASIC_CONTENT macro )
const std::string PageProperties::contentID = "PageProperties";



PageProperties::PageProperties()
	: CDialog(PageProperties::IDD)
	, active_( false )
	, oldSliderColour_(0, 0, 0, 0)
	, rclickItem_( NULL )
	, multiplierCount_( 0 )
	, colourCount_( 0 )
	, tracking_( false )
	, oldSliderMultiplier_( 0 )
{
	ASSERT(!instance_);
	instance_ = this;
}

PageProperties::~PageProperties()
{
	ASSERT(instance_);
	instance_ = NULL;
}

PageProperties& PageProperties::instance()
{
	ASSERT(instance_);
	return *instance_;
}


void PageProperties::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SCENE_PROPERTYLIST, propertyList_);
	DDX_Control(pDX, IDC_SCENE_COLOUR_RED, redSlider_);
	DDX_Control(pDX, IDC_SCENE_COLOUR_GREEN, greenSlider_);
	DDX_Control(pDX, IDC_SCENE_COLOUR_BLUE, blueSlider_);
	DDX_Control(pDX, IDC_SCENE_LIGHT_MULTIPLIER, multiplierSlider_);
	DDX_Control(pDX, IDC_SCENE_LIGHT_MULTIPLIER_STATIC, multiplierStatic_);
	DDX_Control(pDX, IDC_SCENE_COLOUR_RED_STATIC, mStaticColourRed);
	DDX_Control(pDX, IDC_SCENE_COLOUR_GREEN_STATIC, mStaticColourGreen);
	DDX_Control(pDX, IDC_SCENE_COLOUR_BLUE_STATIC, mStaticColourBlue);
	DDX_Control(pDX, IDC_SCENE_COLOURBOX, mStaticColourAdjust);
}



BOOL PageProperties::OnInitDialog()
{
	CDialog::OnInitDialog();

	redSlider_.SetRange(0, 255);
	greenSlider_.SetRange(0, 255);
	blueSlider_.SetRange(0, 255);
	multiplierSlider_.SetRange(0, 30);

	redSlider_.SetPageSize(0);
	greenSlider_.SetPageSize(0);
	blueSlider_.SetPageSize(0);
	multiplierSlider_.SetPageSize(0);

	redSlider_.SetPos((int)oldSliderColour_.r);
	greenSlider_.SetPos((int)oldSliderColour_.g);
	blueSlider_.SetPos((int)oldSliderColour_.b);
	setSliderMultiplier( oldSliderMultiplier_ );

	// pre allocate 1000 strings of about 16 char per string
	propertyList_.InitStorage(1000, 16);

	InitPage();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}


void PageProperties::addView(BaseView* view)
{
	viewList_.push_back(view);

	if (!active_)
		return;

	addItemsForView( view );
}

void PageProperties::addItemsForView(BaseView* view)
{
	for (BaseView::PropertyItems::iterator it = view->propertyItems().begin();
		it != view->propertyItems().end();
		it++)
	{
		propertyList_.AddPropItem(&*(*it));
	}
}

void PageProperties::addItems()
{
	propertyList_.SetRedraw(FALSE);

	propertyList_.clear();

	for (ViewList::iterator vi = viewList_.begin();
		vi != viewList_.end();
		vi++)
	{
		addItemsForView(*vi);
	}

	propertyList_.SetRedraw(TRUE);
}


void PageProperties::clear()
{
	propertyList_.clear();
	viewList_.clear();
}


void PageProperties::setSliderColour(Moo::Colour c)
{
	if (GetSafeHwnd() == NULL)
	{
		oldSliderColour_ = c;
		return;
	}

	oldSliderColour_ = getSliderColour();

	redSlider_.SetPos((int)c.r);
	greenSlider_.SetPos((int)c.g);
	blueSlider_.SetPos((int)c.b);
}

Moo::Colour PageProperties::getSliderColour()
{
	Moo::Colour c;
	c.r = (float)redSlider_.GetPos();
	c.g = (float)greenSlider_.GetPos();
	c.b = (float)blueSlider_.GetPos();
	c.a = 255.f;

	return c;
}

void PageProperties::setSliderMultiplier( float f )
{
	if (GetSafeHwnd() == NULL)
	{
		oldSliderMultiplier_ = f;
		return;
	}
	multiplierSlider_.SetPos( (int) ((f+0.001) * 10) );
	int i = multiplierSlider_.GetPos();
	f = getSliderMultiplier();
	GetTickCount();
}

float PageProperties::getSliderMultiplier()
{
	return ((float) multiplierSlider_.GetPos()) / 10.f;
}


BEGIN_MESSAGE_MAP(PageProperties, CDialog)
	ON_MESSAGE(WM_UPDATE_CONTROLS, OnUpdateControls)
	ON_MESSAGE(WM_SELECT_PROPERTYITEM, OnSelectPropertyItem)
	ON_MESSAGE(WM_CHANGE_PROPERTYITEM, OnChangePropertyItem)
	ON_MESSAGE(WM_DBLCLK_PROPERTYITEM, OnDblClkPropertyItem)
	ON_MESSAGE(WM_RCLK_PROPERTYITEM, OnRClkPropertyItem)
	ON_WM_SIZE()
	ON_WM_CLOSE()
	ON_WM_HSCROLL()
END_MESSAGE_MAP()


// PageProperties message handlers

void PageProperties::InitPage()
{
	active_ = true;

	CString name;
	GetWindowText(name);
	if (PythonAdapter::instance())
		PythonAdapter::instance()->onPageControlTabSelect("pgc", name.GetBuffer());

	// add any current properties to the list
	addItems();

	multiplierSlider_.ShowWindow( multiplierCount_ ? SW_SHOW : SW_HIDE );
	multiplierStatic_.ShowWindow( multiplierCount_ ? SW_SHOW : SW_HIDE );

	redSlider_.ShowWindow( multiplierCount_ ? SW_SHOW : SW_HIDE );
	greenSlider_.ShowWindow( multiplierCount_ ? SW_SHOW : SW_HIDE );
	blueSlider_.ShowWindow( multiplierCount_ ? SW_SHOW : SW_HIDE );

	mStaticColourRed.ShowWindow( multiplierCount_ ? SW_SHOW : SW_HIDE );
	mStaticColourGreen.ShowWindow( multiplierCount_ ? SW_SHOW : SW_HIDE );
	mStaticColourBlue.ShowWindow( multiplierCount_ ? SW_SHOW : SW_HIDE );
	mStaticColourAdjust.ShowWindow( multiplierCount_ ? SW_SHOW : SW_HIDE );
}

void PageProperties::EndPage()
{
	active_ = false;

	clear();
}

void moveWindow(CWnd* wnd, CPoint pos, CSize size = CSize(0, 0))
{
	if ( !wnd )
		return;

	CRect rect;
	wnd->GetWindowRect(rect);
	if (size.cx == 0)
		size.cx = rect.Width();
	if (size.cy == 0)
		size.cy = rect.Height();

	UINT flags = SWP_NOZORDER | SWP_NOACTIVATE;

	wnd->SetWindowPos(NULL,
		pos.x, pos.y,
		size.cx, size.cy,
		flags);
}


void PageProperties::resizePage()
{
	// resize to correspond with the size of the wnd
	CRect rectPage;
	GetWindowRect(rectPage);
	ScreenToClient(rectPage);

	int rectColourHeight = colourCount_ > 0 ? 138 : 0;
	const int staticXoffset = 15;
	const int staticYoffset = 25;
	const int sliderXoffset = 60;
	const int sliderYoffset = 22;
	const int Yincrement = 28;

	int propertyListHeight = rectPage.Height() - 10 - rectColourHeight - ( colourCount_ > 0 ? 10 : 0 );

	CPoint plPos(5, 5);
	CSize plSize(rectPage.Width() - 10, propertyListHeight);

	CPoint cbPos(5, plPos.y + plSize.cy + 10);
	CSize cbSize(rectPage.Width() - 10, rectColourHeight);

	CPoint rsPos = cbPos;
	rsPos.x += staticXoffset;
	rsPos.y += staticYoffset;

	CPoint gsPos = rsPos;
	gsPos.y += Yincrement;

	CPoint bsPos = gsPos;
	bsPos.y += Yincrement;

	CPoint isPos = bsPos;
	isPos.y += Yincrement;

	CPoint rPos = cbPos;
	rPos.x += sliderXoffset;
	rPos.y += sliderYoffset;

	CPoint gPos = rPos;
	gPos.y += Yincrement;

	CPoint bPos = gPos;
	bPos.y += Yincrement;

	CPoint iPos = bPos;
	iPos.y += Yincrement;

	int sliderWidth = rectPage.Width() - 10 - sliderXoffset - 10;

	moveWindow(GetDlgItem(IDC_SCENE_PROPERTYLIST), plPos, plSize);
	moveWindow(GetDlgItem(IDC_SCENE_COLOURBOX), cbPos, cbSize);
	moveWindow(GetDlgItem(IDC_SCENE_COLOUR_RED_STATIC), rsPos);
	moveWindow(GetDlgItem(IDC_SCENE_COLOUR_GREEN_STATIC), gsPos);
	moveWindow(GetDlgItem(IDC_SCENE_COLOUR_BLUE_STATIC), bsPos);
	moveWindow(GetDlgItem(IDC_SCENE_LIGHT_MULTIPLIER_STATIC), isPos);
	moveWindow(GetDlgItem(IDC_SCENE_COLOUR_RED), rPos, CSize(sliderWidth, 0));
	moveWindow(GetDlgItem(IDC_SCENE_COLOUR_GREEN), gPos, CSize(sliderWidth, 0));
	moveWindow(GetDlgItem(IDC_SCENE_COLOUR_BLUE), bPos, CSize(sliderWidth, 0));
	moveWindow(GetDlgItem(IDC_SCENE_LIGHT_MULTIPLIER), iPos, CSize(sliderWidth, 0));

	RedrawWindow();
}

void PageProperties::OnSize( UINT nType, int cx, int cy )
{
	CDialog::OnSize( nType, cx, cy );

	resizePage();
}

afx_msg void PageProperties::OnClose()
{
	EndPage();
	CWnd::OnClose();
}

afx_msg LRESULT PageProperties::OnUpdateControls(WPARAM wParam, LPARAM lParam)
{
	if ( !IsWindowVisible() )
		return 0;

	// loop through the items and update them
	for (ViewList::iterator it = viewList_.begin();
		it != viewList_.end();
		it++)
	{
		(*it)->updateGUI();
	}

	return 0;
}

afx_msg LRESULT PageProperties::OnSelectPropertyItem(WPARAM wParam, LPARAM lParam)
{
	GizmoManager::instance().forceGizmoSet( NULL );

	if (lParam)
	{
		BaseView* relevantView = (BaseView*)lParam;
		relevantView->onSelect();
	}

	return 0;
}

afx_msg LRESULT PageProperties::OnChangePropertyItem(WPARAM wParam, LPARAM lParam)
{
	if (lParam)
	{
		BaseView* relevantView = (BaseView*)lParam;
		relevantView->onChange(wParam != 0);
	}

	return 0;
}

void PageProperties::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	if( nSBCode == SB_THUMBTRACK )
		tracking_ = true;
	else
		tracking_ = false;

	CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
}

afx_msg LRESULT PageProperties::OnDblClkPropertyItem(WPARAM wParam, LPARAM lParam)
{
	return 0;
}

afx_msg LRESULT PageProperties::OnRClkPropertyItem(WPARAM wParam, LPARAM lParam)
{
	PropertyItem * item = (PropertyItem*)(lParam);
	if (!item)
		return 0;

	rclickItem_ = item;

	BaseView * view = (BaseView *)rclickItem_->getChangeBuddy();
	if (!view)
		return 0;

	PropertyManagerPtr propManager = view->getPropertyManger();
	if (!propManager)
		return 0;

	if (propManager->canAddItem())
	{
		CMenu menu;
		menu.LoadMenu( IDR_PROPERTIES_LIST_POPUP );
		CMenu* pPopup = menu.GetSubMenu(0);

		POINT pt;
		::GetCursorPos( &pt );
		pPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y,
			AfxGetMainWnd() );
	}
	else if (propManager->canRemoveItem())
	{
		CMenu menu;
		menu.LoadMenu( IDR_PROPERTIES_LISTITEM_POPUP );
		CMenu* pPopup = menu.GetSubMenu(0);

		POINT pt;
		::GetCursorPos( &pt );
		pPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y,
			AfxGetMainWnd() );
	}

	return 0;
}

void PageProperties::OnListAddItem()
{
	// add a new item to the selected list
	BaseView * view = (BaseView *)rclickItem_->getChangeBuddy();
	PropertyManagerPtr propManager = view->getPropertyManger();
	if (propManager)
	{
		propManager->addItem();
	}
}

void PageProperties::OnListItemRemoveItem()
{
	// remove the item from the list
	BaseView * view = (BaseView *)rclickItem_->getChangeBuddy();
	PropertyManagerPtr propManager = view->getPropertyManger();
	if (propManager)
	{
		propManager->removeItem();
	}
}

void PageProperties::enableMultiplier()
{
	++multiplierCount_;
	if( multiplierSlider_.m_hWnd )
	{
		multiplierSlider_.ShowWindow( SW_SHOW );
		multiplierStatic_.ShowWindow( SW_SHOW );
	}
}

void PageProperties::disableMultiplier()
{
	--multiplierCount_;
	if( multiplierSlider_.m_hWnd && multiplierCount_ == 0 )
	{
		multiplierSlider_.ShowWindow( SW_HIDE );
		multiplierStatic_.ShowWindow( SW_HIDE );
	}
}

void PageProperties::enableColour()
{
	++colourCount_;
	if( redSlider_.m_hWnd && colourCount_ == 1 )
	{
		resizePage();

		redSlider_.ShowWindow( SW_SHOW );
		greenSlider_.ShowWindow( SW_SHOW );
		blueSlider_.ShowWindow( SW_SHOW );

		mStaticColourRed.ShowWindow( SW_SHOW );
		mStaticColourGreen.ShowWindow( SW_SHOW );
		mStaticColourBlue.ShowWindow( SW_SHOW );
		mStaticColourAdjust.ShowWindow( SW_SHOW );
	}
}

void PageProperties::disableColour()
{
	--colourCount_;
	if( redSlider_.m_hWnd && colourCount_ == 0 )
	{
		resizePage();

		redSlider_.ShowWindow( SW_HIDE );
		greenSlider_.ShowWindow( SW_HIDE );
		blueSlider_.ShowWindow( SW_HIDE );

		mStaticColourRed.ShowWindow( SW_HIDE );
		mStaticColourGreen.ShowWindow( SW_HIDE );
		mStaticColourBlue.ShowWindow( SW_HIDE );
		mStaticColourAdjust.ShowWindow( SW_HIDE );
	}
}

BOOL PageProperties::PreTranslateMessage(MSG* pMsg)
{
    if(pMsg->message == WM_KEYDOWN)
    {
        if (pMsg->wParam == VK_TAB)
		{
			if( GetAsyncKeyState( VK_SHIFT ) )
				propertyList_.selectPrevItem();
			else
				propertyList_.selectNextItem();
			return true;
		}
        if (pMsg->wParam == VK_RETURN)
		{
			propertyList_.deselectCurrentItem();
			return true;
		}
    }

	return CDialog::PreTranslateMessage(pMsg);
}


void PageProperties::adviseSelectedId( std::string id )
{
	if (!GetSafeHwnd())     // see if this page has been created
		return;

/*	CPropertySheet* parentSheet = (CPropertySheet *)GetParent();

	if (parentSheet->GetActivePage() != this)
		return; */

	PropertyItem * hItem = propertyList_.getHighlightedItem();
	if (hItem->getType() != PropertyItem::Type_ID)
		return;

	StaticTextView * view = (StaticTextView*)(hItem->getChangeBuddy());
	view->setCurrentValue( id );
}

// help method for vector4 and matrix
#include <sstream>

template<typename EntryType>
static std::string NumVecToStr( const EntryType* vec, int size )
{
	std::ostringstream oss;
	for( int i = 0; i < size; ++i )
	{
		oss << vec[ i ];
		if( i != size - 1 )
			oss << ',';
	}
	return oss.str();
}

template<typename EntryType>
static bool StrToNumVec( std::string str, EntryType* vec, int size )
{
	bool result = false;
	std::istringstream iss( str );

	for( int i = 0; i < size; ++i )
	{
		char ch;
		iss >> vec[ i ];
		if( i != size - 1 )
		{
			iss >> ch;
			if( ch != ',' )
				break;
		}
		else
			result = iss != NULL;
	}
	return result;
}

// vector4
class Vector4View : public BaseView
{
public:
	Vector4View( Vector4Property & property )
		: property_( property )
	{
	}

	StringPropertyItem* item() { return (StringPropertyItem*)&*(propertyItems_.front()); }

	virtual void __stdcall elect()
	{
		oldValue_ = property_.pVector4()->get();

		PropertyItem* newItem = new StringPropertyItem(property_.name(), NumVecToStr( (float*)&oldValue_, 4 ).c_str() );
		newItem->setGroup( property_.getGroup() );
		newItem->setChangeBuddy(this);
		propertyItems_.push_back(newItem);
		PageProperties::instance().addView( this );
	}

	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange(bool transient)
	{
		std::string newValue = item()->get();
		Vector4 v;
		if( StrToNumVec( newValue, (float*)v, 4 ) )
		{
			property_.pVector4()->set( v, transient );
			oldValue_ = v;
		}
	}

	virtual void updateGUI()
	{
		const Vector4 newValue = property_.pVector4()->get();

		if (newValue != oldValue_)
		{
			oldValue_ = newValue;
			item()->set( NumVecToStr( (float*)&newValue, 4 ) );
		}
	}

	static Vector4Property::View * create( Vector4Property & property )
	{
		return new Vector4View( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

protected:

private:
	Vector4Property & property_;
	Vector4 oldValue_;

	class Enroller {
		Enroller() {
			Vector4Property_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};

Vector4View::Enroller Vector4View::Enroller::s_instance;


// vector2
class Vector2View : public BaseView
{
public:
	Vector2View( Vector2Property & property )
		: property_( property )
	{
	}

	StringPropertyItem* item() { return (StringPropertyItem*)&*(propertyItems_.front()); }

	virtual void __stdcall elect()
	{
		oldValue_ = property_.pVector2()->get();

		PropertyItem* newItem = new StringPropertyItem(property_.name(), NumVecToStr( (float*)&oldValue_, 2 ).c_str() );
		newItem->setGroup( property_.getGroup() );
		newItem->setChangeBuddy(this);
		propertyItems_.push_back(newItem);
		PageProperties::instance().addView( this );
	}

	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange(bool transient)
	{
		std::string newValue = item()->get();
		Vector2 v;
		if( StrToNumVec( newValue, (float*)v, 2 ) )
		{
			property_.pVector2()->set( v, transient );
			oldValue_ = v;
		}
	}

	virtual void updateGUI()
	{
		const Vector2 newValue = property_.pVector2()->get();

		if (newValue != oldValue_)
		{
			oldValue_ = newValue;
			item()->set( NumVecToStr( (float*)&newValue, 2 ) );
		}
	}

	static Vector2Property::View * create( Vector2Property & property )
	{
		return new Vector2View( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

protected:

private:
	Vector2Property & property_;
	Vector2 oldValue_;

	class Enroller {
		Enroller() {
			Vector2Property_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};

Vector2View::Enroller Vector2View::Enroller::s_instance;

// matrix
class MatrixView : public BaseView
{
public:
	MatrixView( GenMatrixProperty & property )
		: property_( property )
	{
	}

	StringPropertyItem* item() { return (StringPropertyItem*)&*(propertyItems_.front()); }

	virtual void __stdcall elect()
	{
		property_.pMatrix()->getMatrix( oldValue_, true );

		PropertyItem* newItem = new StringPropertyItem(property_.name(), NumVecToStr( (float*)&oldValue_, 16 ).c_str() );
		newItem->setGroup( property_.getGroup() );
		newItem->setChangeBuddy(this);
		propertyItems_.push_back(newItem);
		PageProperties::instance().addView( this );
	}

	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange(bool transient)
	{
		std::string newValue = item()->get();
		Matrix v;
		if( StrToNumVec( newValue, (float*)v, 16 ) )
		{
			property_.pMatrix()->setMatrix( v );
			oldValue_ = v;
		}
	}

	virtual void updateGUI()
	{
		Matrix newValue;
		property_.pMatrix()->getMatrix( newValue, true );

		if (newValue != oldValue_)
		{
			oldValue_ = newValue;
			item()->set( NumVecToStr( (float*)&newValue, 16 ) );
		}
	}

	static GenMatrixProperty::View * create( GenMatrixProperty & property )
	{
		return new MatrixView( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

protected:

private:
	GenMatrixProperty & property_;
	Matrix oldValue_;

	class Enroller {
		Enroller() {
			GenMatrixProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};

MatrixView::Enroller MatrixView::Enroller::s_instance;
