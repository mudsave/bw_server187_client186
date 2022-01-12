/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef BWSOUND_HPP
#define BWSOUND_HPP

#include "xactsnd/soundmgr.hpp"


/**
 *	This class provies methods to play special global sounds.
 */
class BWSound
{

public:

	static void playThunder(const Vector3& worldPos,
							float normalisedDistFromCamera);

	static void playFootstep(const Vector3& WorldPosition,
							bool leftFoot,
							float velocity,
							const char* terrainName );

	static void playRain(float amount, float dTime);

};



#endif // BWSOUND_HPP

/*bwsound.hpp*/
