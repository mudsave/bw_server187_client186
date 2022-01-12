/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef SKY_LIGHT_MAP_HPP
#define SKY_LIGHT_MAP_HPP

#include "light_map.hpp"
#include "moo/device_callback.hpp"
#include "moo/managed_effect.hpp"

class EnviroMinder;

namespace Moo
{
	class EffectConstantValue;
};

/**
 *	This class creates a light map
 *	for casting clouds on the environment.
 */
class SkyLightMap : public EffectLightMap, public Moo::DeviceCallback
{
public:
	SkyLightMap();
	~SkyLightMap();

	void activate( const EnviroMinder&, DataSectionPtr pSpaceSettings );
	void deactivate( const EnviroMinder& );

	void createUnmanagedObjects();
	void deleteUnmanagedObjects();	

	void update( float sunAngle, const Vector2& strataMovement );

	//Interface for those that contribute to the light map.
	class IContributor
	{
	public:
		virtual bool needsUpdate() = 0;		
		virtual void render(
			SkyLightMap* /*this*/,
			Moo::EffectMaterial* /*m*/,
			float sunAngle ) = 0;
	};

	void addContributor( IContributor& c );
	void delContributor( IContributor& c );

protected:
	bool init(const DataSectionPtr pSection);
	void initInternal();
private:	
	//light map rendering fns
	bool beginRender();
	void lighten(float xOffset);	
	void endRender();

	//overriden from LightMap base class
	virtual void createLightMapSetter( const std::string& textureFeedName );
	virtual void createTransformSetter();

	void texCoordOffset( const Vector2& value );
	const Vector2& texCoordOffset() const;

	void sunAngleOffset( float value );
	float sunAngleOffset() const	{ return sunAngleOffset_; }

	bool inited_;
	bool needsUpdate_;	
	Vector2 texCoordOffset_;
	float sunAngleOffset_;

	typedef std::vector<SkyLightMap::IContributor*> Contributors;
	Contributors contributors_;


	static Moo::EffectMacroSetting * s_skyLightMapSetting;
	static void configureKeywordSetting(Moo::EffectMacroSetting & setting);
};

#endif
