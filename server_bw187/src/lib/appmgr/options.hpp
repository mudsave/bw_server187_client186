/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include "resmgr/dataresource.hpp"
#include "pyscript/script.hpp"

/**
 *	This class handles the options associated with this program.
 */
class Options
{
public:
	static DataSectionPtr pRoot();
	static bool init( int argc, char * argv[], bool useUserTag = true );
	static bool init( const std::string& optionsFilename, bool useUserTag = true );
	static bool save( const char * path = NULL );

	static bool optionExists( const std::string& name );
	static void setOptionString( const std::string& name, const std::string& value );
	static std::string getOptionString( const std::string& name );
	static std::string getOptionString( const std::string& name, const std::string& defaultVal );
	static void setOptionInt( const std::string& name, int value );
	static int getOptionInt( const std::string& name );
	static int getOptionInt( const std::string& name, int defaultVal );
	static void setOptionBool( const std::string& name, bool value );
	static bool getOptionBool( const std::string& name );
	static bool getOptionBool( const std::string& name, bool defaultVal );
	static void setOptionFloat( const std::string& name, float value );
	static float getOptionFloat( const std::string& name );
	static float getOptionFloat( const std::string& name, float defaultVal );
	static void setOptionVector2( const std::string& name, const Vector2& value );
	static Vector2 getOptionVector2( const std::string& name );
	static Vector2 getOptionVector2( const std::string& name, const Vector2& defaultVal );
	static void setOptionVector3( const std::string& name, const Vector3& value );
	static Vector3 getOptionVector3( const std::string& name );
	static Vector3 getOptionVector3( const std::string& name, const Vector3& defaultVal );
	static void setOptionVector4( const std::string& name, const Vector4& value );
	static Vector4 getOptionVector4( const std::string& name );
	static Vector4 getOptionVector4( const std::string& name, const Vector4& defaultVal );
private:
	Options();

	// These are not implemented.
	Options( const Options & );
	Options & operator=( const Options & );

	// Data Section / XML Resource
	DataResource options_;
	DataSectionPtr pRootSection_;

	static Options instance_;

	typedef Options This;
	PY_MODULE_STATIC_METHOD_DECLARE( py_setOptionString )
	PY_MODULE_STATIC_METHOD_DECLARE( py_getOptionString )
	PY_MODULE_STATIC_METHOD_DECLARE( py_setOptionInt )
	PY_MODULE_STATIC_METHOD_DECLARE( py_getOptionInt )
	PY_MODULE_STATIC_METHOD_DECLARE( py_setOptionFloat )
	PY_MODULE_STATIC_METHOD_DECLARE( py_getOptionFloat )
	PY_MODULE_STATIC_METHOD_DECLARE( py_setOptionVector2 )
	PY_MODULE_STATIC_METHOD_DECLARE( py_getOptionVector2 )
	PY_MODULE_STATIC_METHOD_DECLARE( py_setOptionVector3 )
	PY_MODULE_STATIC_METHOD_DECLARE( py_getOptionVector3 )
	PY_MODULE_STATIC_METHOD_DECLARE( py_setOptionVector4 )
	PY_MODULE_STATIC_METHOD_DECLARE( py_getOptionVector4 )
	PY_MODULE_STATIC_METHOD_DECLARE( py_setOption )

	std::map<std::string, std::string> cache_;
};

#ifdef CODE_INLINE
#include "options.ipp"
#endif

#endif
/*options.hpp*/
