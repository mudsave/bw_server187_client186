/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef GUI_MANAGER_HPP__
#define GUI_MANAGER_HPP__

#include <string>
#include <vector>
#include <map>
#include <set>
#include "cstdmf/smartpointer.hpp"
#include "resmgr/datasection.hpp"
#include "gui_forward.hpp"

BEGIN_GUI_NAMESPACE

class InputDevice
{
public:
	virtual ~InputDevice();
	virtual bool isKeyDown( const std::string& key ) = 0;
};

// the framework uses a java style view so not eveything is necessarily consted
class Item : public SafeReferenceCount
{
	std::string type_;
	std::string name_;
	std::string displayName_;
	std::string description_;
	std::string icon_;
	std::string shortcutKey_;
	std::string action_;
	std::string updater_;
	std::string imports_;
	std::map<std::string,std::string> values_;
	std::vector<Item*> parents_;// no smarter pointer here, remove cycle
	std::vector<ItemPtr> subitems_;
	unsigned short commandID_;
	static std::set<unsigned short>& unusedCommands();
	static std::map<std::string, ItemTypePtr>& types();
public:
	Item( const std::string& type, const std::string& name, const std::string& displayName,
		const std::string& description,	const std::string& icon, const std::string& shortcutKey,
		const std::string& action, const std::string& updater, const std::string& imports,
		unsigned int commandID = GUI_COMMAND_END );
	Item( DataSectionPtr section );
	virtual ~Item();
	void add( ItemPtr item );
	void add( const DataSectionPtr section );
	void insert( std::vector<ItemPtr>::size_type index, ItemPtr item );
	void remove( std::vector<ItemPtr>::size_type index );
	void remove( ItemPtr item );

	std::vector<ItemPtr>::size_type num() const;
	ItemPtr operator[]( std::vector<ItemPtr>::size_type index );
	const ItemPtr operator[]( std::vector<ItemPtr>::size_type index ) const;

	std::vector<Item*>::size_type parentNum();
	Item* parent( std::vector<Item*>::size_type index );
	void addParent( Item* parent );
	bool isAncestor( Item* item ) const;
	std::string pathTo( Item* item );

	const std::string& type() const;
	const std::string& name() const;
	std::string displayName();
	std::string description();
	const std::string& action() const;
	const std::string& updater() const;
	const std::string& shortcutKey() const;
	unsigned short commandID() const;

	bool exist( const std::string& name );
	std::string operator[]( const std::string& name );
	void set( const std::string& name, const std::string& value );

	unsigned int update();
	bool act();
	bool processInput( InputDevice* inputDevice );
	void changed();

	virtual ItemPtr operator()( const std::string& path );

	ItemPtr findByCommandID( unsigned short commandID );
	ItemPtr findByName( const std::string& name );

	static void registerType( ItemTypePtr itemType );
	static void unregisterType( ItemTypePtr itemType );

	static void init();
};

class ItemType : public SafeReferenceCount
{
public:
	virtual ~ItemType();
	virtual const std::string& name() const = 0;
	virtual bool act( ItemPtr item ) = 0;
	virtual unsigned int update( ItemPtr item ) = 0;
	virtual void shortcutPressed( ItemPtr item ) = 0;
};

class BasicItemType : public ItemType
{
	std::string name_;
public:
	BasicItemType( const std::string& typeName );
	virtual ~BasicItemType();
	virtual const std::string& name() const;
	virtual bool act( ItemPtr item );
	virtual unsigned int update( ItemPtr item );
	virtual void shortcutPressed( ItemPtr item );
};

class Subscriber : public SafeReferenceCount
{
	std::string root_;
	ItemPtr rootItem_;
public:
	Subscriber( const std::string& root );
	virtual ~Subscriber(){}
	const std::string& root() const;
	ItemPtr rootItem();
	void rootItem( ItemPtr item );
	virtual void changed( ItemPtr item ) = 0;
};

typedef SmartPointer<Subscriber> SubscriberPtr;

class Manager : public Item
{
	std::map<std::string,std::set<SubscriberPtr> > subscribers_;
public:
	Manager();
	Manager( const DataSectionPtr section );
	virtual ~Manager(){}
	using Item::add;
	void add( SubscriberPtr subscriber );
	void remove( SubscriberPtr subscriber );
	virtual ItemPtr operator()( const std::string& path );

	void act( unsigned short commandID );
	void update( unsigned short commandID );
	void update();
	void changed( ItemPtr item );

	static bool resolveFileName( std::string& fileName );
	static Manager* instance();
};

static class GUILifeManager
{
public:
	GUILifeManager();
	~GUILifeManager();
}
GUILifeManager;

END_GUI_NAMESPACE

#endif//GUI_MANAGER_HPP__
