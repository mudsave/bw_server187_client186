/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/



#include "packers.hpp"
#include "packer_helper.hpp"
#include "resmgr/bwresource.hpp"
#include "fx_packer.hpp"


IMPLEMENT_PACKER( FxPacker )

int FxPacker_token = 0;


bool FxPacker::prepare( const std::string& src, const std::string& dst )
{
	std::string ext = BWResource::getExtension( src );
	if ( ext == "fx" )
		type_ = FX;
	else if ( ext == "fxh" )
		type_ = FXH;
	else if ( ext == "fxo" )
		type_ = FXO;
	else
		return false;

	src_ = src;
	dst_ = dst;

	return true;
}

bool FxPacker::print()
{
	if ( src_.empty() )
	{
		printf( "Error: FxPacker not initialised properly\n" );
		return false;
	}

	if ( type_ == FXO )
		printf( "FxCompiledFile: %s\n", src_.c_str() );
	else if ( type_ == FXH )
		printf( "FxHeaderFile: %s\n", src_.c_str() );
	else
		printf( "FxSourceFile: %s\n", src_.c_str() );

	return true;
}

bool FxPacker::pack()
{
#ifndef MF_SERVER
	if ( src_.empty() || dst_.empty() )
	{
		printf( "Error: FxPacker not initialised properly\n" );
		return false;
	}

	// We could compile shaders here, but shaders need to be recompiled each
	// time the graphics settings are changed, so just copying shader source.

	if ( type_ == FXO )
		return true; // skip .fxo files
	
	// For fx and fxh files, just copy them.
	if ( !PackerHelper::copyFile( src_, dst_ ) )
		return false;

#else // MF_SERVER

	// just skip fx,fx0 and fxh files for the server

#endif // MF_SERVER
	
	return true;
}
