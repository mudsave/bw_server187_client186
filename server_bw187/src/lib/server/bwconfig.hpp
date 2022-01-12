/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef BWCONFIG_HPP
#define BWCONFIG_HPP

#include <string>
#include <vector>

#include "cstdmf/stdmf.hpp"
#include "resmgr/datasection.hpp"

#include "common.hpp"

// Uncomment the line below if you want to debug configuration options.
#include <typeinfo>

/**
 *	This namespace contains functions for getting values out of the
 *	configuration files.
 *
 *	The initial configuration files is server/bw.xml. This can chain other
 *	configuration files by adding a section "parentFile". If the value
 *	cannot be found in the initial configuration file, parent files are then
 *	checked.
 */
namespace BWConfig
{
	typedef std::vector< std::pair< DataSectionPtr, std::string > > Container;
	extern int shouldDebug;

	bool init( int argc, char * argv[] );
	bool hijack( Container & container, int argc, char * argv[] );

	bool isBad();

	template <class C>
		const char * typeOf( const C & c ) { return typeid(c).name(); }
	inline const char * typeOf( char * ) { return "String"; }
	inline const char * typeOf( std::string ) { return "String"; }
	inline const char * typeOf( bool ) { return "Boolean"; }
	inline const char * typeOf( int ) { return "Integer"; }
	inline const char * typeOf( float ) { return "Float"; }

	void prettyPrint( const char * path,
			const std::string & oldValue, const std::string & newValue,
			const char * typeStr, bool isHit );

	template <class C>
	void printDebug( const char * path,
			const C & oldValue, C newValue, bool isHit )
	{
		std::string oldValueStr = watcherValueToString( oldValue );
		std::string newValueStr = watcherValueToString( newValue );

		prettyPrint( path, oldValueStr, newValueStr,
				typeOf( oldValue ), isHit );
	}

	/**
	 *	This function returns the data section in the configuration files
	 *	corresponding to the input path.
	 */
	DataSectionPtr getSection( const char * path,
		DataSectionPtr defaultValue = NULL );

	/**
	 *	This function retrieves the name of all the children sections in the
	 * 	configuration files corresponding to the input path. It adds the
	 * 	children names to childrenNames.
	 */
	void getChildrenNames( std::vector< std::string >& childrenNames,
		const char * parentPath );

	/**
	 *	This function changes the input value to the value in the
	 *	configuration files corresponding to the input path. If the value is
	 *	not specified in the configuration files, the value is left
	 *	unchanged.
	 *
	 *	@return True if the value was found, otherwise false.
	 */
	template <class C>
		bool update( const char * path, C & value )
	{
		DataSectionPtr pSect = getSection( path );

		if (shouldDebug)
		{
			printDebug( path, value,
					pSect ? pSect->as( value ) : value, pSect );
		}

		if (pSect) value = pSect->as( value );

		return bool(pSect);
	}

	/**
	 *	This function returns the configuration value corresponding to the
	 *	input path. If this value is not specified in the configuration
	 *	files, the default value is returned.
	 */
	template <class C>
		C get( const char * path, const C & defaultValue )
	{
		DataSectionPtr pSect = getSection( path );

		if (shouldDebug)
		{
			printDebug( path, defaultValue,
				pSect ? pSect->as( defaultValue ) : defaultValue, pSect );
		}

		if (pSect)
		{
			return pSect->as( defaultValue );
		}

		return defaultValue;
	}


	std::string get( const char * path, const char * defaultValue );

	inline
	std::string get( const char * path )
	{
		return get( path, "" );
	}
};


/*
 *	This method is used to register the watcher nub with bwmachined. It also
 *	binds the watcher nub to an interface. This interface is specified by the
 *	CONFIG_PATH/monitoringInterface configuration option else by the root
 *	level monitoringInterface option else to the same interface as the nub.
 */
#define BW_REGISTER_WATCHER( ID, ABRV, NAME, CONFIG_PATH, NUB )				\
{																			\
	std::string interfaceName =												\
		BWConfig::get( CONFIG_PATH "/monitoringInterface",					\
				BWConfig::get( "monitoringInterface",						\
					NUB.address().ipAsString() ) );							\
																			\
	WatcherGlue::instance().registerWatcher(								\
			ID, ABRV, NAME, interfaceName.c_str() );						\
																			\
	NUB.registerFileDescriptor(												\
			WatcherGlue::instance().getSocketDescriptor(),					\
			&WatcherGlue::instance() );										\
}

/*
 *	This macro is used to select the internal interface out of the configuration
 *	file. The process specific option is checked then the general and it
 *	defaults to using BWMACHINED's interface.
 */
#define BW_INTERNAL_INTERFACE( CONFIG_PATH )								\
	BWConfig::get( #CONFIG_PATH "/internalInterface",						\
			BWConfig::get( "internalInterface",								\
				Mercury::Nub::USE_BWMACHINED ) ).c_str()

#endif // BWCONFIG_HPP
