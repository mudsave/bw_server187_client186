/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef SHADOW_CASTER_HPP
#define SHADOW_CASTER_HPP

#include "moo/render_target.hpp"
#include "cstdmf/smartpointer.hpp"
#include "cstdmf/vectornodest.hpp"
#include "math/boundbox.hpp"
#include "resmgr/datasection.hpp"
#include "moo/effect_material.hpp"
#include "moo/visual.hpp"
#include "moo/terrain_block.hpp"

class MaterialDrawOverride;
class ChunkItem;
typedef SmartPointer<ChunkItem> ChunkItemPtr;

/**
 *	This class contains the common properties for the shadows
 */
class ShadowCommon
{
public:
	ShadowCommon();
	~ShadowCommon();
	void init( DataSectionPtr pConfigSection );
	MaterialDrawOverride* pCasterOverride() { return pCasterOverride_.get(); }
	MaterialDrawOverride* pReceiverOverride() { return pReceiverOverride_.get(); }
	Moo::EffectMaterial*  pTerrainReceiver() { return &terrainReceiver_; }

	float shadowIntensity() { return shadowIntensity_; }
	float shadowFadeStart() { return shadowFadeStart_; };
	float shadowDistance() { return shadowDistance_; }
	uint32 shadowBufferSize() { return shadowBufferSize_; }
	int	  shadowQuality() { return shadowQuality_; }

	void  shadowQuality( int shadowQuality) { shadowQuality_ = shadowQuality; }
private:
	typedef std::auto_ptr<MaterialDrawOverride> MaterialDrawOverridePtr;
	MaterialDrawOverridePtr pCasterOverride_;
	MaterialDrawOverridePtr pReceiverOverride_;
	Moo::EffectMaterial terrainReceiver_;

	float	shadowIntensity_;

	float	shadowDistance_;
	float	shadowFadeStart_;
	uint32	shadowBufferSize_;

	int		shadowQuality_;
};

/**
 *	This class implements the ShadowCaster object, the shadow caster
 *	implements the shadow buffer and handles rendering to and rendering
 *	with the shadow buffer. The shadow buffer is implemented as a 
 *	floating point texture.
 */
class ShadowCaster : public ReferenceCount, public Aligned
{
public:
	ShadowCaster();

	bool init( ShadowCommon* pCommon, bool halfRes, Moo::RenderTarget* pDepthParent = NULL );
	void begin( const BoundingBox& worldSpaceBB, const Vector3& lightDirection );
	void end();

	void beginReceive( bool useTerrain = true );
	void endReceive();

	void setupMatrices( const BoundingBox& worldSpaceBB, const Vector3& lightDirection );

	void angleIntensity( float angleIntensity ) { angleIntensity_ = angleIntensity; }

	const BoundingBox& aaShadowVolume() const	{ return aaShadowVolume_; }

	Moo::RenderTarget* shadowTarget() { return pShadowTarget_.getObject(); }

	void setupConstants( Moo::EffectMaterial& effect );
private:
	void pickReceivers();	
	Matrix	view_;
	Matrix	projection_;
	Matrix	viewport_;
	Matrix	shadowProjection_;
	Matrix	viewProjection_;
	BoundingBox aaShadowVolume_;

	RECT	scissorRect_;

	ShadowCommon* pCommon_;

	typedef VectorNoDestructor<ChunkItemPtr> ShadowItems;
	ShadowItems shadowItems_;

	Moo::RenderTargetPtr pShadowTarget_;

	float	angleIntensity_;

	uint32 shadowBufferSize_;

	ShadowCaster( const ShadowCaster& );
	ShadowCaster& operator=( const ShadowCaster& );
};

typedef SmartPointer<ShadowCaster> ShadowCasterPtr;

#endif // SHADOW_CASTER_HPP
