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

#include <stdio.h>
#include <d3dx9.h>

#include "cstdmf/debug.hpp"
#include "cstdmf/memory_counter.hpp"
#include "resmgr/bwresource.hpp"

#include "shader_manager.hpp"

DECLARE_DEBUG_COMPONENT2( "Moo", 0 )

memoryCounterDefine( shaders, Base );

ShaderManager& ShaderManager::instance()
{
	static ShaderManager shaderManagerInstance;

	return shaderManagerInstance;
}

/**
 * Returns a smartpointer to a shaderset.
 * Creates the set if it hasn't been created yet.
 */
ShaderSetPtr ShaderManager::shaderSet( const std::string& vertexFormat, const std::string& shaderType, 
									  const Moo::VertexDeclaration* pDecl )
{
	// Try to find the shader set we are looking for.
	ShaderFormatMap::iterator fit = sets_.find( vertexFormat );
	if( fit != sets_.end() )
	{
		ShaderSetMap::iterator it = fit->second.find( shaderType );
		if (it != fit->second.end())
			return it->second;
	}

	return this->loadSet( vertexFormat, shaderType, pDecl );
}

/**
 *	Private method to load a shader set if it hasn't already been
 */
ShaderSetPtr ShaderManager::loadSet( const std::string& vertexFormat, const std::string& shaderType, const Moo::VertexDeclaration* pDecl )
{
	MF_ASSERT( shaderRoot_ );

	// Create the shader set since we haven't already.
	ShaderSetPtr theSet = NULL;

	// Open the root section of the shader set.
	DataSectionPtr setSection = shaderRoot_->openSection( vertexFormat + "/" + shaderType );


	if (setSection)
	{
		// Create our new shaderset.
		theSet = new ShaderSet( setSection, vertexFormat, shaderType );
	}
	else if (pDecl)
	{
		const Moo::VertexDeclaration::Aliases& aliases = pDecl->aliases();
		for (uint32 i = 0; i < aliases.size() && !theSet; i++)
		{
			theSet = this->shaderSet( aliases[i], shaderType );
		}	
	}

	if (theSet)
	{
		{
			memoryCounterSub( shaders );
			memoryClaim( sets_ );
			for (uint32 i = 0; i < sets_.size(); i++)
				memoryClaim( (sets_.begin() + i)->second );
		}
		sets_[ vertexFormat ][ shaderType ] = theSet;
		{
			memoryCounterAdd( shaders );
			memoryClaim( sets_ );
			for (uint32 i = 0; i < sets_.size(); i++)
				memoryClaim( (sets_.begin() + i)->second );
		}
	}

	return theSet;
}


ShaderManager::ShaderManager()
{
	shaderRoot_ = BWResource::instance().openSection( "shaders" );
	if( shaderRoot_ )
	{
	}
	else
	{
		ERROR_MSG( "Error opening shader root section!\n" );
	}

	memoryCounterAdd( shaders );
	memoryClaim( sets_ );
}

ShaderManager::~ShaderManager()
{
	memoryCounterSub( shaders );
	memoryClaim( sets_ );
	for (uint32 i = 0; i < sets_.size(); i++)
		memoryClaim( (sets_.begin() + i)->second );
}

std::ostream& operator<<(std::ostream& o, const ShaderManager& t)
{
	o << "ShaderManager\n";
	return o;
}

// shader_manager.cpp
