/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef GUI_FUNCTOR_CPP_HPP__
#define GUI_FUNCTOR_CPP_HPP__

#include "gui_functor.hpp"

BEGIN_GUI_NAMESPACE

struct Textor
{
	virtual std::string text( ItemPtr item ) = 0;
};

struct Updater
{
	virtual unsigned int update( ItemPtr item ) = 0;
};

struct Importer
{
	virtual DataSectionPtr import( ItemPtr item ) = 0;
};

struct Action
{
	virtual bool act( ItemPtr item ) = 0;
};

class CppFunctor : public Functor
{
	std::map< std::string, Textor* > textors_;
	std::map< std::string, Updater* > updaters_;
	std::map< std::string, Importer* > importers_;
	std::map< std::string, Action* > actions_;
public:
	void set( const std::string& name, Textor* textor );
	void set( const std::string& name, Updater* updater );
	void set( const std::string& name, Importer* importer );
	void set( const std::string& name, Action* action );
	void remove( Textor* textor );
	void remove( Updater* updater );
	void remove( Importer* importer );
	void remove( Action* action );

	virtual const std::string& name() const;
	virtual bool text( const std::string& textor, ItemPtr item, std::string& result );
	virtual bool update( const std::string& updater, ItemPtr item, unsigned int& result );
	virtual DataSectionPtr import( const std::string& importer, ItemPtr item );
	virtual bool act( const std::string& action, ItemPtr item, bool& result );
	static CppFunctor* instance();
};

template<typename T, int index = 0>
struct TextorMaker: public Textor
{
	std::string (T::*func_)( ItemPtr item );
	TextorMaker( const std::string& name, std::string (T::*func)( ItemPtr item ) )
		: func_( func )
	{
		CppFunctor::instance()->set( name, this );
	}
	~TextorMaker()
	{
		CppFunctor::instance()->remove( this );
	}
	virtual std::string text( ItemPtr item )
	{
		return ( ( ( T* )this ) ->* func_ )( item );
	}
};

template<typename T, int index = 0>
struct UpdaterMaker: public Updater
{
	unsigned int (T::*func_)( ItemPtr item );
	UpdaterMaker( const std::string& name, unsigned int (T::*func)( ItemPtr item ) )
		: func_( func )
	{
		CppFunctor::instance()->set( name, this );
	}
	~UpdaterMaker()
	{
		CppFunctor::instance()->remove( this );
	}
	virtual unsigned int update( ItemPtr item )
	{
		return ( ( ( T* )this ) ->* func_ )( item );
	}
};

template<typename T, int index = 0>
struct ActionMaker: public Action
{
	bool (T::*func_)( ItemPtr item);
	ActionMaker( const std::string& name, bool (T::*func)( ItemPtr item) )
		: func_( func )
	{
		CppFunctor::instance()->set( name, this );
	}
	~ActionMaker()
	{
		CppFunctor::instance()->remove( this );
	}
	virtual bool act( ItemPtr item )
	{
		return ( ( ( T* )this ) ->* func_ )( item );
	}
};

END_GUI_NAMESPACE

#endif//GUI_FUNCTOR_CPP_HPP__
