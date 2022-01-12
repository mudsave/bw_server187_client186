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



#pragma warning(disable:4786)	// remove "truncated identifier" warnings from STL

#include "bwsound.hpp"
#include "cstdmf/debug.hpp"

//#include "cstdmf/stdmf.hpp"
//#include "cstdmf/watcher.hpp"
//#include "resmgr/bwresource.hpp"


DECLARE_DEBUG_COMPONENT2( "Sound", -1 );	// debugLevel for this file



/**
 *	This method plays a footfall sound.
 *
 *	It selects from a set of sounds based on the parameters passed in
 */
void BWSound::playFootstep(	const Vector3& worldPosition,
							bool leftFoot,
							float velocity,
							const char* terrainName)
{
	char tag[128];

	sprintf(tag, "footstep_%s_%c%c",
		(*terrainName) ? terrainName : "gravel",
		velocity < 4.0 ? 'W' : (velocity < 8.0 ? 'R' : 'D'),
		leftFoot ? 'L' : 'R' );

	TRACE_MSG("BWSound::playFootstep: '%s'\n", tag);

	::soundMgr().playFx(tag, worldPosition);
}


/**
 *	This method plays a thunder sound.
 *
 *	@param worldPos	Locate the sound in space
 *	@param normalisedDistFromCamera	This is the distance from camera (normalised to
 *	0.0-1.0f).  Its used to choose which thunder sound to play (from closeup to distant)
 */
void BWSound::playThunder(const Vector3& worldPos, float distFromCamera)
{
	TRACE_MSG("BWSound::playThunder: (%1.0f,%1.0f,%1.0f), %1.3f:",
		worldPos.x, worldPos.y, worldPos.z, distFromCamera);
	MF_ASSERT(distFromCamera >= 0 && distFromCamera <= 1.f);

	const float MAX_THUNDER_ATTEN = -20;

	char tag[128];

	// chuck some of the distant ones
//	if (normalisedDistFromCamera > 0.3 && (rand() % 10) >= 5)
//		return;

	sprintf(tag, "thunder_%d", (int)(distFromCamera * 4.0) + 1);
	TRACE_MSG(" '%s'", tag);

	BaseSound* snd = ::soundMgr().findSound(tag);
	if (snd) {
		TRACE_MSG(" atten=%1.0f\n", distFromCamera * MAX_THUNDER_ATTEN);
		snd->play(distFromCamera * MAX_THUNDER_ATTEN);
	}
}


/**
 *	This method should be called whenever the amount of rain falling changes,
 *	to update the corresponding rain sound effect.
 *
 *	@param amount	The density of the rain (normalised to 0-1)
 *	@param dTime	Delta time for the last frame (in seconds)
 */
void BWSound::playRain(float amount, float dTime)
{
	MF_ASSERT(amount >= 0 && amount <= 1.f);

	const float QUIET_RAIN_ATTEN = -20;		// quietest normal rain
	const float MAX_RAIN_ATTEN = -40;		// completely silent (fade to this)
	static float curAtten = MAX_RAIN_ATTEN;

	static BaseSound* snd = NULL;	// keep track of the loaded sound for efficiency

	if (!snd)
		snd = ::soundMgr().findSound("rain_heavy");

	float desiredAtten = amount ? (1 - amount) * QUIET_RAIN_ATTEN : MAX_RAIN_ATTEN;
	// Note the brake to slow down the change of volume 
	curAtten = curAtten + Math::clamp(2.0f * dTime, desiredAtten - curAtten);

	if (curAtten != desiredAtten) {
		DEBUG_MSG("BWSound::playRain: desired=%0.1f cur=%0.1f\n", desiredAtten, curAtten);
	}

	if (snd) {
		if (curAtten <= MAX_RAIN_ATTEN) {
			if (snd->isPlaying())
				snd->stop(0);
		}
		else {
			if (!snd->isPlaying())
				snd->play();
			snd->attenuation(curAtten);
		}
	}
}



/*bwsound.cpp*/
