/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef SHADER_SET_HPP
#define SHADER_SET_HPP

#include <iostream>
#include "resmgr/datasection.hpp"
#include "cstdmf/smartpointer.hpp"
#include "device_callback.hpp"

typedef SmartPointer< class ShaderSet > ShaderSetPtr;

class ShaderSet : public Moo::DeviceCallback, public ReferenceCount
{
public:
	typedef uint32 ShaderID;
	typedef IDirect3DVertexShader9* ShaderHandle;

	typedef std::map< ShaderID, ShaderHandle > ShaderMap;

	ShaderSet( DataSectionPtr shaderSetSection, const std::string & vertexFormat, 
		const std::string & shaderType );
	~ShaderSet();

	static ShaderID		shaderID( char nDirectionalLights, char nPointLights, char nSpotLights );

	/// return a shaderhandle that can be used by d3d
	ShaderHandle		shader( char nDirectionalLights, char nPointLights, char nSpotLights, bool hardwareVP = false );
	ShaderHandle		shader( ShaderID shaderID, bool hardwareVP = false );

	/// destruct all shaders, but keep the shaderset.
	void				deleteUnmanagedObjects( );

	bool				isSameSet( const std::string & vertexFormat, const std::string & shaderType )
		{ return vertexFormat == vertexFormat_ && shaderType == shaderType_; }

	void				preloadAll();

private:

	ShaderMap		shaders_;
	ShaderMap		hwShaders_;

	DataSectionPtr	shaderSetSection_;

	std::string		vertexFormat_;
	std::string		shaderType_;

	bool			preloading_;

	ShaderSet(const ShaderSet&);
	ShaderSet& operator=(const ShaderSet&);
};

#ifdef CODE_INLINE
#include "shader_set.ipp"
#endif




#endif
/*shader_set.hpp*/