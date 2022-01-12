/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef PLAYER_HPP
#define PLAYER_HPP

#include "entity.hpp"
#include "physics.hpp"

class MatrixProvider;
typedef SmartPointer<MatrixProvider> MatrixProviderPtr;
class EntityPhotonOccluder;
class PyMetaParticleSystem;
typedef SmartPointer<PyMetaParticleSystem> PyMetaParticleSystemPtr;


/**
 *	This class stores a pointer to the player entity, and other
 *	player specific objects such as the current physics controller.
 */
class Player
{
public:
	Player();
	~Player();

	bool setPlayer( Entity * newPlayer );

	void poseUpdateNotification( Entity * pEntity );

	Entity *	i_entity()		{ return pEntity_; }

	void shouldSendToServer( bool value )	{ shouldSendToServer_ = value; }

	MatrixProviderPtr camTarget();

	static Player & instance();
	void updateWeatherParticleSystems( class PlayerAttachments & pa );

	static Entity *		entity()			{ return instance().i_entity(); }

	const Vector3 & position() const
		{ return pEntity_ ? pEntity_->position() : Vector3::zero(); }

	Chunk * findChunk();

private:

	struct AttachedPS
	{
		SmartPointer<PyModelNode> pNode_;
		PyMetaParticleSystemPtr pAttachment_;
	};

	std::vector< AttachedPS > attachedPSs_;

	Entity		* pEntity_;

	bool		shouldSendToServer_;

	EntityPhotonOccluder * playerFlareCollider_;
};


#include "romp/full_screen_back_buffer.hpp"

/**
 *	This class is a helper for fading out the player.  It checks whether or
 *	not the near-plane clips through the player and, if so, makes the
 *	player's model invisible.
 *	At final composite time it uses the FullScreenBackBuffer as a buffer
 *	to draw the character and then copies it back onto the real BackBuffer
 *	translucently.
 */
class PlayerFader : public FullScreenBackBuffer::User
{
public:
	PlayerFader();
	~PlayerFader();

	void update();

	bool isEnabled();
	void beginScene();
	void endScene();	
	bool doTransfer( bool alreadyTransferred )	{ return false; }
	void doPostTransferFilter();
	
private:	
	float transparency_;
	///player transparency power
	float ptp_;
	///max player transparency
	float maxPt_;			
};

#endif // PLAYER_HPP