/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef FLORA_LIGHT_MAP_HPP
#define FLORA_LIGHT_MAP_HPP

#include "light_map.hpp"
#include "moo/terrain_block.hpp"
#include "moo/device_callback.hpp"

/**
 *	This class creates a light map
 *	for the flora shader.
 */
class FloraLightMap : public EffectLightMap, public Moo::DeviceCallback
{
public:
	FloraLightMap( class Flora* flora );
	~FloraLightMap();	

	void createUnmanagedObjects();
	void deleteUnmanagedObjects();

	void update( float gametime );
	void setTransform( int constBase, const Matrix& vertexToWorld );
private:
	void setProjection(Moo::TerrainBlockPtr pValidBlock);		
	virtual void createTransformSetter();
	
	bool					inited_;
	bool					needsUpdate_;
	bool					delayedUpdate_;
	Moo::TerrainBlockPtr	blocks_[4];
	Matrix					worldToLight_;		

	Flora*					flora_;
};

#endif