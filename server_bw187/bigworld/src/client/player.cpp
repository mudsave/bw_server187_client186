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

#pragma warning(disable: 4786)	// browse info too long
#pragma warning(disable: 4503)	// class name too long

#include "player.hpp"

#include "entity_manager.hpp"
#include "entity_type.hpp"
#include "physics.hpp"
#include "filter.hpp"

#include "common/servconn.hpp"
#include "pyscript/script.hpp"

#include "duplo/pymodel.hpp"
#include "duplo/pymodelnode.hpp"
#include "particle/meta_particle_system.hpp"
#include "particle/particle_system.hpp"
#include "entity_flare_collider.hpp"

#include "cstdmf/debug.hpp"
#include "romp/enviro_minder.hpp"
#include "camera/base_camera.hpp"

#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk_terrain.hpp"
#include "romp/lens_effect_manager.hpp"
#include "romp/z_buffer_occluder.hpp"
#include "moo/terrain_block.hpp"


DECLARE_DEBUG_COMPONENT2( "App", 0 )


/// Constructor
Player::Player() :
	pEntity_( NULL ),
	shouldSendToServer_( true ),
	playerFlareCollider_( NULL )
{
}


// Destructor
Player::~Player()
{
	this->setPlayer( NULL );
}


/**
 *	Change the entity that the client controls.
 *	A NULL entity pointer is permitted to indicate no entity.
 *
 *	@return True on success, otherwise false.
 */
bool Player::setPlayer( Entity * newPlayer )
{
	if (newPlayer != NULL && newPlayer->type().pPlayerClass() == NULL)
	{
		ERROR_MSG( "Player::setPlayer: "
			"newPlayer id %d of type %s does not have an associated player class.\n",
			newPlayer->id(), newPlayer->type().name().c_str() );

		return false;
	}

	if (newPlayer == pEntity_)
	{
		return true;
	}

	// either the old or the new entity must be in a space
	/* not any more
	ChunkSpacePtr pPlayerSpace = NULL;
	if (pEntity_ != NULL)
		pPlayerSpace = pEntity_->pSpace();
	if (newPlayer != NULL && newPlayer->pSpace())
		pPlayerSpace = newPlayer->pSpace();

	if (newPlayer != NULL && !pPlayerSpace)
	{
		ERROR_MSG( "Player::setPlayer: "
			"newPlayer id %d is not in any space and nor is old player.\n",
			newPlayer->id() );
		return false;
	}
	*/

	// if there's already a player tell it its time has come
	if (pEntity_ != NULL)
	{
		pEntity_->makeNonPlayer();
		//EntityManager::instance().onEntityLeave( pEntity_->id() );

		for (uint i = 0; i < attachedPSs_.size(); i++)
		{
			attachedPSs_[i].pNode_->detach( attachedPSs_[i].pAttachment_ );
		}

		attachedPSs_.clear();

		LensEffectManager::instance().delPhotonOccluder( playerFlareCollider_ );
		playerFlareCollider_ = NULL;
	}

	// we are back to no player now
	pEntity_ = NULL;

	// set a new player if instructed
	if (newPlayer != NULL && newPlayer->type().pPlayerClass() != NULL)
	{
		// make the player script object
		pEntity_ = newPlayer;

		//EntityManager::instance().onEntityEnter(
		//	pEntity_->id(), pPlayerSpace->id(), 0 );
		pEntity_->makePlayer();

		// adorn the model with weather particle systems
		if (pEntity_->pSpace())
		{
			updateWeatherParticleSystems(
				pEntity_->pSpace()->enviro().playerAttachments() );
		}

		if (!ZBufferOccluder::isAvailable())
		{		
			playerFlareCollider_ = new EntityPhotonOccluder( *pEntity_ );
			LensEffectManager::instance().addPhotonOccluder( playerFlareCollider_ );
		}
	}

	return true;
}


/**
 *	Grabs the particle systems that want to be attached to the player,
 *	and attaches them to its model.
 */
void Player::updateWeatherParticleSystems( PlayerAttachments & pa )
{
	for (uint i = 0; i < attachedPSs_.size(); i++)
	{
		attachedPSs_[i].pNode_->detach( attachedPSs_[i].pAttachment_ );
	}

	attachedPSs_.clear();

	if (pa.empty()) return;
	if (pEntity_ == NULL || pEntity_->pPrimaryModel() == NULL) return;

	for (uint i = 0; i < pa.size(); i++)
	{
		PyObject * pPyNode = pEntity_->pPrimaryModel()->node( pa[i].onNode );
		if (!pPyNode || !PyModelNode::Check( pPyNode ))
		{
			PyErr_Clear();
			continue;
		}

		SmartPointer<PyModelNode> pNode( (PyModelNode*)pPyNode, true );

		if (!pNode->attach( pa[i].pSystem ))
		{
			ERROR_MSG( "Player::updateWeatherParticleSystems: Could not attach "
				"weather particle system to new Player Model\n" );

			PyErr_Clear();
			continue;
		}

		AttachedPS aps;
		aps.pNode_ = pNode;
		aps.pAttachment_ = pa[i].pSystem;
		attachedPSs_.push_back( aps );
	}
}



extern double getGameTotalTime();

const float GROUND_TOLERANCE = 0.1f;	// 10 cm 

/**
 *	Do anything special which requires the position to be set,
 *	i.e. send it on to the server. Should be called for all
 *	controlled entities.
 */
void Player::poseUpdateNotification( Entity * pEntity )
{
	bool onGround;

	// check entity
	MF_ASSERT( pEntity != NULL );

	// check entity is on server
	if (pEntity->isClientOnly())
	{
		return;
	}

	// check that it has a cell entity
	if (!pEntity->isInWorld()) return;

	// check that the server wants updates
	if (!shouldSendToServer_) return;

	// check that we are connected to a server
	ServerConnection * pServer = EntityManager::instance().pServer();
	if (pServer == NULL) return;

	// check that we aren't sending too frequently
	if (getGameTotalTime() - pServer->lastSendTime() <
			pServer->minSendInterval())
		return;

	// check that we aren't under physics correction
	if (!!pEntity->physicsCorrected())
	{
		// return if no packet sent since physics corrected
		if (pEntity->physicsCorrected() == pServer->lastSendTime()) return;
		// ok that correction is all over now
		pEntity->physicsCorrected( 0.0 );
	}

	// ok, send it then!

	// figure out if it's on the ground...
	Vector3 globalPos = pEntity->position();
	Vector3 globalDir = Vector3::zero();
	ChunkSpacePtr pSpace;
	if ((pSpace = pEntity->pSpace()))
		pEntity->pSpace()->transformCommonToSpace( globalPos, globalDir );

	ChunkSpace::Column * csCol;
	Chunk * oCh;
	ChunkTerrain * pTer;
	float terrainHeight = -1000000.f;
	if (pSpace &&
		(csCol = pSpace->column( globalPos, false )) != NULL &&
		(oCh = csCol->pOutsideChunk()) != NULL &&
		(pTer = ChunkTerrainCache::instance( *oCh ).pTerrain()) != NULL)
	{
		const Matrix & trInv = oCh->transformInverse();
		Vector3 terPos = trInv.applyPoint( globalPos );
		terrainHeight = pTer->block()->heightAt( terPos[0], terPos[2] ) -
			trInv.applyToOrigin()[1];
	}

	// ... get the pose in local coordinates ...
	double unused;
	SpaceID spaceID;
	ObjectID vehicleID;
	Vector3 localPos( 0.f, 0.f, 0.f );
	float localAuxVolatile[3] = {0.f, 0.f, 0.f};
	pEntity->filter().getLastInput( unused, spaceID, vehicleID,
		localPos, localAuxVolatile );

	onGround =	fabs(globalPos[1] - terrainHeight) < GROUND_TOLERANCE &&
				vehicleID == 0;

	// ... and then add the move
	pServer->addMove(
		pEntity->id(),
		spaceID,
		vehicleID,
		localPos,
		localAuxVolatile[0],
		localAuxVolatile[1],
		localAuxVolatile[2],
		onGround,
		globalPos );
}


/**
 *	Find the chunk that the player is in
 */
Chunk * Player::findChunk()
{
	if (pEntity_ && pEntity_->pSpace())
	{
		const Vector3 & loc = pEntity_->fallbackTransform().applyToOrigin();
		return pEntity_->pSpace()->findChunkFromPoint( loc );
	}

	return NULL;
}




/*~ function BigWorld player
 *  This sets the player entity, or returns the current player entity if the 
 *  entity argument is not provided. The BigWorld engine assumes that there is 
 *  no more than one player entity at any time. Changing whether an entity is 
 *  the current player entity involves changing whether it is an instance of 
 *  its normal class, or its player class. This is a class whose name equals 
 *  the entity's current class name, prefixed with the word "Player". As an 
 *  example, the player class for Biped would be PlayerBiped.
 *  
 *
 *  The following occurs if a new entity is specified:
 *  
 *  The onBecomeNonPlayer function is called on the old player entity.
 *  
 *  The old player entity becomes an instance of its original class, rather 
 *  than its player class.
 *  
 *  The reference to the current player entity is set to None.
 *  
 *  A player class for the new player entity is sought out.
 *  If the player class for the new player entity is found, then the entity 
 *  becomes an instance of this class. Otherwise, the function immediately 
 *  returns None.
 *  
 *  The onBecomePlayer function is called on the new player entity.
 *  
 *  The reference to the current player entity is set the new player entity.
 *  
 *  @param entity An optional entity. If supplied, this entity becomes the 
 *  current player.
 *  @return If the entity argument is supplied then this is None, otherwise 
 *  it's the current player entity.
 */
/**
 *	Returns the player entity.
 */
static PyObject * py_player( PyObject * args )
{
	// get the player if desired
	if (PyTuple_Size( args ) == 0)
	{
		Entity * pPlayer = Player::entity();

		if (pPlayer != NULL)
		{
			Py_INCREF( pPlayer );
			return pPlayer;
		}
		else
		{
			Py_Return;
		}
	}

	// otherwise switch player control to this entity
	PyObject * pNewPlayer = PyTuple_GetItem( args, 0 );
	if (!Entity::Check( pNewPlayer ))
	{
		PyErr_SetString( PyExc_TypeError,
			"BigWorld.player: Expected an entity\n" );
		return NULL;
	}

	Player::instance().setPlayer( (Entity*)pNewPlayer );
	Py_Return;
}
PY_MODULE_FUNCTION( player, BigWorld )



/*~ class BigWorld.PlayerMProv
 *
 *	This class inherits from MatrixProvider, and can be used to access the 
 *	position of the current player entity.  Multiple PlayerMProvs can be
 *	created, but they will all have the same value.
 *
 *	There are no attributes or functions available on a PlayerMProv, but it
 *	can be used to construct a PyMatrix from which the position can be read.
 *
 *	A new PlayerMProv is created using BigWorld.PlayerMatrix function.
 */
/**
 *	Helper class to access the matrix of the current player position
 */
class PlayerMProv : public MatrixProvider
{
	Py_Header( PlayerMProv, MatrixProvider )

public:
	PlayerMProv() : MatrixProvider( &s_type_ ) {}

	virtual void matrix( Matrix & m ) const
	{
		Entity * pEntity = Player::entity();
		if (pEntity != NULL)
		{
			m = pEntity->fallbackTransform();
		}
		else
		{
			m.setIdentity();
		}
	}

	PY_DEFAULT_CONSTRUCTOR_FACTORY_DECLARE()
};


PY_TYPEOBJECT( PlayerMProv )

PY_BEGIN_METHODS( PlayerMProv )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( PlayerMProv )
PY_END_ATTRIBUTES()

/*~ function BigWorld.PlayerMatrix
 *
 *	This function returns a new PlayerMProv which is a MatrixProvider
 *	that can be used to access the player entity's position.
 */
PY_FACTORY_NAMED( PlayerMProv, "PlayerMatrix", BigWorld )


/**
 *	Get the matrix provider for the player's transform
 */
MatrixProviderPtr Player::camTarget()
{
	return MatrixProviderPtr( new PlayerMProv(), true );
}


/*~ function BigWorld.playerDead
 *
 *	This method sets whether or not the player is dead.  This information is
 *	used by the snow system to stop snow falling if the player is dead.
 *
 *	@param isDead	an integer as boolean. This is 0 if the player is alive, otherwise
 *	it is non-zero.
 */
/**
 *	Set whether or not the player is dead
 */
static void playerDead( bool isDead )
{
	Entity * pE = Player::entity();
	if (pE == NULL || !pE->pSpace()) return;
	pE->pSpace()->enviro().playerDead( isDead );
}
PY_AUTO_MODULE_FUNCTION( RETVOID, playerDead, ARG( bool, END ), BigWorld )


/// Static instance accessor
Player & Player::instance()
{
	static Player	instance;

	return instance;
}

static bool g_checkPlayerClip = true;


#if ENABLE_WATCHER

static class WatchIniter
{
public:
	WatchIniter()
	{
		MF_WATCH_REF( "position", Player::instance(), &Player::position );
		MF_WATCH( "Client Settings/playerClip", g_checkPlayerClip );
	}
} s_initer_;

#endif // ENABLE_WATCHER


// -----------------------------------------------------------------------------
// Section: PlayerFader
// -----------------------------------------------------------------------------

#include "chunk/chunk_light.hpp"
#include "moo/render_context.hpp"
#include "moo/visual_channels.hpp"
#include "romp/geometrics.hpp"

/**
 *	This helper function is used by PlayerFader.
 *
 *	This function returns the rectangle that is the intersection of the
 *	near-plane with the view frustum.
 *
 *	@param rc			The render context (used to calculate the near-plane).
 *	@param corner		Is set to the position of the bottom-left corner of the
 *							rectangle.
 *	@param xAxis		Is set to the length of the horizontal edges.
 *	@param yAxis		Is set to the length of the vertical edges.
 *
 *	@note	The invView matrix must be correct before this method is called. You
 *			may need to call updateViewTransforms.
 */
void getNearPlaneRect( const Moo::RenderContext & rc, Vector3 & corner,
		Vector3 & xAxis, Vector3 & yAxis )
{
	// TODO: May want to make this function available to others.

	const Matrix & matrix = rc.invView();
	const Moo::Camera & camera = rc.camera();

	// zAxis is the vector from the camera position to the centre of the
	// near plane of the camera.
	Vector3 zAxis = matrix.applyToUnitAxisVector( Z_AXIS );
	zAxis.normalise();

	// xAxis is the vector from the centre of the near plane to its right edge.
	xAxis = matrix.applyToUnitAxisVector( X_AXIS );
	xAxis.normalise();

	// yAxis is the vector from the centre of the near plane to its top edge.
	yAxis = matrix.applyToUnitAxisVector( Y_AXIS );
	yAxis.normalise();

	const float fov = camera.fov();
	const float nearPlane = camera.nearPlane();
	const float aspectRatio = camera.aspectRatio();

	const float yLength = nearPlane * tanf( fov / 2.0f );
	const float xLength = yLength * aspectRatio;

	xAxis *= xLength;
	yAxis *= yLength;
	zAxis *= nearPlane;

	Vector3 nearPlaneCentre( matrix.applyToOrigin() + zAxis );
	corner = nearPlaneCentre - xAxis - yAxis;
	xAxis *= 2.f;
	yAxis *= 2.f;
}


/**
 *	Constructor. In the constructor, this object checks whether the near-plane
 *	clips the player. If so, it adjusts the visibility of the player's model.
 */
PlayerFader::PlayerFader() :	
	transparency_( 0.f ),
	ptp_( 2.f ),
	maxPt_( 0.85f )
{
	FullScreenBackBuffer::addUser(this);

	MF_WATCH( "Client Settings/fx/Player Fader/transparency power",
		ptp_,
		Watcher::WT_READ_WRITE,
		"Mathematical power for the player transparency effect (when "
		"the player models fades as it nears the camera)" );

	MF_WATCH( "Client Settings/fx/Player Fader/maximum transparency",
		maxPt_,
		Watcher::WT_READ_WRITE,
		"The maximum value of player transparency is clamped to this value." );
}


/**
 *	Destructor. In the destructor, the visibility of the player's model is
 *	restored.
 */
PlayerFader::~PlayerFader()
{
	FullScreenBackBuffer::removeUser(this);
}


void PlayerFader::update()
{
	if (!g_checkPlayerClip)
		return;	

	transparency_ = 0.f;

	Entity * pPlayer = Player::entity();
	if (pPlayer == NULL)
		return;	

	PyModel * pModel = pPlayer->pPrimaryModel();
	if (pModel == NULL)
		return;

	if (!BaseCamera::checkCameraTooClose())
		return;

	BaseCamera::checkCameraTooClose( false );	

	//allow the player to fade out smoothly before completely disappearing
	//the transparency_ member is set with a value between 0 and 1.
	//For any value > 0, the player is fading or faded out.
	//
	//For a smooth transition, the near plane check no longer uses the
	//bounding box because it has sharp corners (and would thus create
	//discontinuities in the transparency value when the camera circles
	//around the player).
	//Instead, an ellispoid is fitted around the bounding box and the distance
	//from the camera to it is calculated.
	if (pPlayer->pPrimaryModel()->visible())
	{
		//The distance is calculated by transforming the camera position into
		//unit-bounding-box space.  A sphere is fitted around the unit bounding
		//box and the distance is calculated.
		BoundingBox bb;
		pPlayer->pPrimaryModel()->boundingBoxAcc( bb, true );
		//1.414 is so the sphere fits around the unit cube, instead of inside.
		const Vector3 s = (bb.maxBounds() - bb.minBounds())*1.414f;
		Matrix m( pPlayer->fallbackTransform() );
		m.invert();
		Vector3 origin( Moo::rc().invView().applyToOrigin() );
		m.applyPoint( origin, origin );
		origin -= bb.centre();
		origin = origin * Vector3(1.f/s.x,1.f/s.y,1.f/s.z);

		//1 is subtracted so if camera is inside unit sphere then we are fully
		//faded out.
		float d = origin.length() - 1.f;
		transparency_ = 1.f - min( max( d,0.f ),1.f );

		//Check the player itself for transparency	
		transparency_ = max( pPlayer->transparency(), transparency_ );

		//And adjust the values for final output.
		transparency_ = powf(transparency_,ptp_);
		transparency_ = min( transparency_, maxPt_ );
	}
}


bool PlayerFader::isEnabled()
{
	return (transparency_ > 0.f);
}


void PlayerFader::beginScene()
{	
	Entity * pPlayer = Player::entity();
	pPlayer->pPrimaryModel()->visible( false );
}


void PlayerFader::endScene()
{
	Entity * pPlayer = Player::entity();
	pPlayer->pPrimaryModel()->visible( true );
}


void PlayerFader::doPostTransferFilter()
{
	//now, draw the player if the near plane clipper removed the player for fading
	if ( 0.f < transparency_ && transparency_ < 1.f )
	{
		Entity * pPlayer = Player::entity();
		if (pPlayer && pPlayer->pPrimaryModel())
		{
			Moo::LightContainerPtr pLC = Moo::rc().lightContainer();
			Moo::LightContainerPtr pSC = Moo::rc().specularLightContainer();

				Chunk * pChunk = Player::instance().findChunk();
				if ( pChunk )
				{
					DX::Surface* oldDepth = NULL;
					if (!FullScreenBackBuffer::reuseZBuffer())
					{
						DX::Surface* depthBuffer = FullScreenBackBuffer::renderTarget().depthBuffer();
						Moo::rc().device()->GetDepthStencilSurface( &oldDepth );
						Moo::rc().device()->SetRenderTarget( NULL, depthBuffer );
					}
					ChunkLightCache & clc = ChunkLightCache::instance( *pChunk );
					clc.draw();
					pPlayer->pPrimaryModel()->draw( pPlayer->fallbackTransform(), 1.f );
					Moo::SortedChannel::draw();
					Moo::rc().setTexture( 0, FullScreenBackBuffer::renderTarget().pTexture() );
					Moo::rc().device()->SetPixelShader( NULL );

					Geometrics::texturedRect( Vector2(0.f,0.f),
						Vector2(Moo::rc().screenWidth(),Moo::rc().screenHeight()),
						Vector2(0,0),
						Vector2(FullScreenBackBuffer::uSize(),FullScreenBackBuffer::vSize()),
						Moo::Colour( 1.f, 1.f, 1.f,
						transparency_ ), true );
					if (oldDepth)
					{
						Moo::rc().device()->SetRenderTarget( NULL, oldDepth );
						oldDepth->Release();
						oldDepth = NULL;
					}
				}

			Moo::rc().lightContainer( pLC );
			Moo::rc().specularLightContainer( pSC );
		}
	}
}


// player.cpp
