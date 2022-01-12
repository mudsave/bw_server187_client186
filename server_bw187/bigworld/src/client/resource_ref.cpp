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
#include "resource_ref.hpp"

#include "romp/super_model.hpp"
#include "romp/font.hpp"
#include "moo/texture_manager.hpp"
#include "moo/visual_manager.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/data_section_census.hpp"
#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT2( "Entity", 0 )


/**
 *	Functions for data sections
 */
static ResourceRef getIfLoaded_dsect( const std::string & id )
{
	DataSectionPtr pDS = DataSectionCensus::find( id );
	return ResourceRef( &*pDS, ResourceRef::stdModRef<DataSection> );
}
static ResourceRef getOrLoad_dsect( const std::string & id )
{
	DataSectionPtr pDS = BWResource::openSection( id );
	return ResourceRef( &*pDS, ResourceRef::stdModRef<DataSection> );
}
static ResourceRef::ResourceFns s_RFns_dsect =
	{ &getIfLoaded_dsect, &getOrLoad_dsect };

/**
 *	Functions for models
 */
static ResourceRef getIfLoaded_model( const std::string & id )
{
	ModelPtr pModel = Model::get( id, false );
	return ResourceRef( &*pModel, ResourceRef::stdModRef<Model> );
}
static ResourceRef getOrLoad_model( const std::string & id )
{
	ModelPtr pModel = Model::get( id );
	return ResourceRef( &*pModel, ResourceRef::stdModRef<Model> );
}

static ResourceRef::ResourceFns s_RFns_model =
	{ &getIfLoaded_model, &getOrLoad_model };

/**
 *	Functions for visuals
 */
static ResourceRef getIfLoaded_visual( const std::string & id )
{
	Moo::VisualPtr pVisual =
		Moo::VisualManager::instance()->get( id, false );
	return ResourceRef( &*pVisual, ResourceRef::stdModRef<Moo::Visual> );
}
static ResourceRef getOrLoad_visual( const std::string & id )
{
	Moo::VisualPtr pVisual =
		Moo::VisualManager::instance()->get( id );
	return ResourceRef( &*pVisual, ResourceRef::stdModRef<Moo::Visual> );
}

static ResourceRef::ResourceFns s_RFns_visual =
	{ &getIfLoaded_visual, &getOrLoad_visual };

/**
 *	Functions for textures
 */
static ResourceRef getIfLoaded_texture( const std::string & id )
{
	Moo::BaseTexturePtr pTex =	// allowAnim, !mustExist, !loadIfMissing
		Moo::TextureManager::instance()->get( id, true, false, false );
	return ResourceRef( &*pTex, ResourceRef::stdModRef<Moo::BaseTexture> );
}
static ResourceRef getOrLoad_texture( const std::string & id )
{
	Moo::BaseTexturePtr pTex =
		Moo::TextureManager::instance()->get( id );
	return ResourceRef( &*pTex, ResourceRef::stdModRef<Moo::BaseTexture> );
}

static ResourceRef::ResourceFns s_RFns_texture =
	{ &getIfLoaded_texture, &getOrLoad_texture };

/**
 *	Functions for fonts
 */
static ResourceRef getIfLoaded_font( const std::string & id )
{
	CRITICAL_MSG( "Bad\n" );
	FontPtr pFont = FontManager::instance().get( id );
	return ResourceRef( &*pFont, ResourceRef::stdModRef<Font> );
}
static ResourceRef getOrLoad_font( const std::string & id )
{
	CRITICAL_MSG( "Bad\n" );
	FontPtr pFont = FontManager::instance().get( id );
	return ResourceRef( &*pFont, ResourceRef::stdModRef<Font> );
}

static ResourceRef::ResourceFns s_RFns_font = 
	{ &getIfLoaded_font, &getOrLoad_font };


/**
 *	The static method gets the named resource if it is loaded.
 *	Otherwise it returns an empty ResourceRef.
 */
const ResourceRef::ResourceFns & ResourceRef::ResourceFns::getForString(
	const std::string & id )
{
	uint period = id.find_last_of( '.' );
	if (period >= id.size())
		return s_RFns_dsect;
	std::string ext = id.substr( period+1 );

	if (ext == "model")
	{
		return s_RFns_model;
	}

	if (ext == "visual")
	{
		return s_RFns_visual;
	}
	
	if (ext == "bmp" ||
		ext == "tga" ||
		ext == "dds")
	{
		return s_RFns_texture;
	}

	if (ext == "font")
	{
		return s_RFns_font;
	}

	// assume it is a datasection then
	return s_RFns_dsect;
}

/**
 *	The static method gets the named resource if it is loaded.
 *	Otherwise it returns an empty ResourceRef.
 */
ResourceRef ResourceRef::getIfLoaded( const std::string & id )
{
	const ResourceFns & rf = ResourceFns::getForString( id );
	return (*rf.getIfLoaded_)( id );
}

/**
 *	The static method gets or loads the named resource.
 *	If loading fails it returns an empty ResourceRef.
 */
ResourceRef ResourceRef::getOrLoad( const std::string & id )
{
	const ResourceFns & rf = ResourceFns::getForString( id );
	return (*rf.getOrLoad_)( id );
}

// resource_ref.cpp