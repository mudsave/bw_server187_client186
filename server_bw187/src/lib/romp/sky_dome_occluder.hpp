/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

/**
 *	This class checks line-of-sight of the sun flare only,
 *	through the sky domes using occlusion queries.
 */
 
#ifndef SKY_DOME_OCCLUDER_HPP
#define SKY_DOME_OCCLUDER_HPP

#include "photon_occluder.hpp"
#include "occlusion_query_helper.hpp"
#include "moo/device_callback.hpp"

class LensEffect;

class SkyDomeOccluder : public PhotonOccluder, public Moo::DeviceCallback
{
public:
	SkyDomeOccluder( class EnviroMinder& enviroMinder );
	~SkyDomeOccluder();
	
	static bool isAvailable();
	
	virtual PhotonOccluder::Result collides(
			const Vector3 & photonSourcePosition,
			const Vector3 & cameraPosition,
			const LensEffect& le );
	virtual void beginOcclusionTests();
	virtual void endOcclusionTests();
	
	virtual void deleteUnmanagedObjects();
private:
	uint32 drawOccluders();

	bool	sunThisFrame_;
	UINT	lastSunResult_;
	OcclusionQueryHelper	helper_;
	class EnviroMinder& enviroMinder_;
};

#endif