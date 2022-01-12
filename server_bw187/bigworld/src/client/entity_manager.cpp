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

//#pragma warning (disable: 4503)
#pragma warning (disable: 4786)

#include "cstdmf/debug.hpp"
#include "cstdmf/binaryfile.hpp"

DECLARE_DEBUG_COMPONENT2( "Entity", 0 );

#include "entity_manager.hpp"

#include "cstdmf/memory_stream.hpp"
#include "network/basictypes.hpp"

#include "filter.hpp"
#include "player.hpp"
#include "personality.hpp"
#include "update_handler.hpp"
#include "common/space_data_types.hpp"
#include "camera/annal.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_space.hpp"
#include "romp/time_of_day.hpp"

extern double getGameTotalTime();


namespace // annonymous 
{

void clearEntityDict(Entity * entity);
void clearEntitiesDict(const std::vector< EntityPtr > &entities);

} // namespace annonymous 


// -----------------------------------------------------------------------------
// Section: EntityManager
// -----------------------------------------------------------------------------


ObjectID EntityManager::nextClientID_ = (1L<<30) + 1;

/// Constructor
EntityManager::EntityManager() :
	pServer_( NULL ),
	proxyEntityID_( 0 ),
	displayIDs_ ( false ),
	playbackLastMessageTime_( 0 )
{
}

/// Destructor
EntityManager::~EntityManager()
{}

/// Instance accessor
EntityManager & EntityManager::instance()
{
	static EntityManager	ec;

	return ec;
}


/**
 *	This method tells the EntityManager class that a connection has
 *	been made and what its corresponding ServerConnection is.
 */
void EntityManager::connected( ServerConnection & server )
{
	if (pServer_ != NULL)
	{
		WARNING_MSG( "EntityManager::connected: "
			"Already got a connection!\n" );

		this->disconnected();
	}

	TRACE_MSG( "EntityManager::connected\n" );

	pServer_ = &server;
}


/**
 *	This method tells the EntityManager class that the connection
 *	has been lost. Note that calls to this method need not be balanced
 *	with calls to the 'connected' method. You can disconnect multiple
 *	times if you are switching between offline worlds.
 */
void EntityManager::disconnected()
{
	TRACE_MSG( "EntityManager::disconnected\n" );

	// forget the server
	pServer_ = NULL;

	// clear out all server spaces
	ChunkManager::instance().clearAllServerSpaces();

	// Call leaveWorld() on all entities
	for (Entities::iterator iter = enteredEntities_.begin();
		iter != enteredEntities_.end();
		iter++ )
	{
		iter->second->leaveWorld( false );
	}

	// get rid of any player things
	// NOTE: Must be done after calling leaveWorld() on player entity, 
	// otherwise leaveWorld() would be called on the wrong type.
	Player::instance().setPlayer( NULL );

	// dispose of everything in our cache
	for (Entities::iterator iter = cachedEntities_.begin();
		iter != cachedEntities_.end();
		iter++ )
	{
		clearEntityDict(iter->second);
		Py_DECREF( iter->second );
	}
	cachedEntities_.clear();

	// dispose of everything in our entered list
	for (Entities::iterator iter = enteredEntities_.begin();
		iter != enteredEntities_.end();
		iter++ )
	{
		clearEntityDict(iter->second);
		Py_DECREF( iter->second );
	}
	enteredEntities_.clear();

	// and even the unknown entities and prerequisite entity list	
	clearEntitiesDict(prereqEntities_);
	prereqEntities_.clear();

	clearEntitiesDict(passengerEntities_);
	passengerEntities_.clear();

	unknownEntities_.clear();

	// don't leave the queued messages either
	for (MessageMap::iterator iter = queuedMessages_.begin();
		iter != queuedMessages_.end();
		iter++)
	{
		for (MessageQueue::iterator qIter = iter->second.begin();
			qIter != iter->second.end();
			qIter++)
		{
			delete qIter->second;
		}
	}
	queuedMessages_.clear();

	// clear out every space
	PyGC_Collect();

	// and we are now in a pristine state
}


/**
 *	This function is called when we get a new player entity.
 *	The data on the stream contains only properties provided by the base.
 */
void EntityManager::onBasePlayerCreate( ObjectID id, EntityTypeID type,
	BinaryIStream & data, bool oldVersion )
{
	// remember that this is who the server thinks player is
	proxyEntityID_ = id;

	EntityType * pEntityType = EntityType::find( type );
	if (!pEntityType)
	{
		ERROR_MSG( "EntityManager::onBasePlayerCreate: Bad type %d. id = %d\n",
			type, id );
		return;
	}

	// create the entity
	Entity * pSister =
		!enteredEntities_.empty() ? enteredEntities_.begin()->second :
		!cachedEntities_.empty()  ? cachedEntities_.begin()->second	: NULL;

	Position3D origin(0,0,0);
	float auxZero[3] = { 0, 0, 0 };
	int enterCount = 0;

	Entity * pNewEntity = pEntityType->newEntity( id, origin, auxZero,
		enterCount, data, EntityType::BASE_PLAYER_DATA, pSister );

	// put it into cached entities for now
	cachedEntities_[ id ] = pNewEntity;

	// and make it the player entity
	Player::instance().setPlayer( pNewEntity );

	// that is all we do for now
}

/**
 *	This function is called to create the call part of the player entity.
 *	The data on the stream contains only properties provided by the cell.
 */
void EntityManager::onCellPlayerCreate( ObjectID id,
		SpaceID spaceID, ObjectID vehicleID, const Position3D & position,
		float yaw, float pitch, float roll, BinaryIStream & data,
		bool oldVersion )
{
	this->onEntityCreate( id,
		EntityTypeID( -1 ), // Dodgy indicator that it is the onCellPlayerCreate.
		spaceID, vehicleID, position, yaw, pitch, roll, data, oldVersion );

	this->onEntityEnter( id, spaceID, vehicleID );	// once for being the player
	this->onEntityEnter( id, spaceID, vehicleID );	// once for being controlled
}

/**
 *	This function is called to tell us that we may now control the given
 *	entity (or not as the case may be).
 */
void EntityManager::onEntityControl( ObjectID id, bool control )
{
	Entities::iterator found = enteredEntities_.find( id );
	if (found == enteredEntities_.end())
	{
		// TODO: handle this case
		ERROR_MSG( "EntityManager::onEntityControl: "
			"No entity id %d in the world\n", id );
		return;
	}

	// this will call through to the 'onPoseVolatile' method.
	// not entirely perfect, but not too bad.
	found->second->controlled( control );
}


/**
 *	This function is called when the server provides us with the details
 *	necessary to create an entity here. The minimum data it could send
 *	is the type of the entity, but for the moment it sends everything.
 */
void EntityManager::onEntityCreate( ObjectID id, EntityTypeID type,
	SpaceID spaceID, ObjectID vehicleID, const Position3D & position,
	float yaw, float pitch, float roll,
	BinaryIStream & data, bool oldVersion )
{
	Entities::iterator iter;

	Vector3 pos( position.x, position.y, position.z );
	float auxFilter[3];
	auxFilter[0] = yaw;
	auxFilter[1] = pitch;
	auxFilter[2] = roll;

	Entity * pNewEntity;

	// If we already have the entity to create, then treat this as an update.
	if ((iter = cachedEntities_.find( id )) != cachedEntities_.end())
	{
		pNewEntity = iter->second;
		cachedEntities_.erase( iter );

		// Dodgy indicator that it is creating the cell part of a player.
		if (type != EntityTypeID( -1 ))
		{
			pNewEntity->updateProperties( data, /*callSetMethod:*/false );
		}
		else
		{
			pNewEntity->readCellPlayerData( data );
		}
		pNewEntity->pos( pos, auxFilter, 3 );
		// We remove it from the cached list without checking its enter count.
		// If that count was not > 0 then it'll be added back below.
		// And if we're loading prereqs, then our unknownEntities entry will get
		// clobbered below, but that's fine as we have newer info for it anyway.
	}
	// If it's already entered then treat it as an update also
	else if ((iter=enteredEntities_.find( id )) != enteredEntities_.end())
	{
		pNewEntity = iter->second;
		MF_ASSERT( pNewEntity->type().index() == type );
		MF_ASSERT( pNewEntity->enterCount() > 0 );
		pNewEntity->updateProperties( data, /*callSetMethod:*/true );
		// position information goes through to filter
	}
	// Otherwise we actually have to create an entity (yay!)
	else
	{
		// look up its type
		EntityType * pEntityType = EntityType::find( type );
		if (!pEntityType)
		{
			ERROR_MSG( "EntityManager::createEntity(%d): "
				"No such entity type %d\n", id, type );
			return;
		}

		// use the enterCount stored in this entity's unknown record
		int enterCount = unknownEntities_[id].count;
		//unknownEntities_.erase(id);	// done by clearQueuedMessages below

		if (enterCount == 0)
		{
			ERROR_MSG( "EntityManager::createEntity(%d): "
				"enterCount from unknown is zero (not 1) - "
				"didn't 'enter' before 'create'\n", id );
			// or possibly got the create message after the leave message,
			// in which case the entity will hang around in the cache below,
			// but that's not too bad (perfect if we start purging the cache)
		}
		else if (enterCount != 1)
		{
			WARNING_MSG( "EntityManager::createEntity(%d): "
				"enterCount from unknown is %d (not 1)\n", id, enterCount );
		}

		// find a sister entity
		Entity * pSister =
			!enteredEntities_.empty() ? enteredEntities_.begin()->second :
			!cachedEntities_.empty()  ? cachedEntities_.begin()->second	: NULL;

		// and instantiate the entity
		pNewEntity = pEntityType->newEntity( id, pos, auxFilter, enterCount,
			data, EntityType::TAGGED_CELL_ENTITY_DATA, pSister );
	}

	// If there are any messages queued for this entity then they are now stale
	// (Hmmm, what about out-of-order messages if the createEntity was dropped?)
	// Be sure to uncomment 'unknownEntities_.erase' above if this line changes
	this->clearQueuedMessages( id );

	// Now add it to our list of entered entities, as long as
	// it hasn't left the world again since we asked for the update
	if (pNewEntity->enterCount() > 0)
	{
		// Could already be in world if we got > 1 createEntity messages
		// after an enter-leave-enter sequence. Unlikely but possible.
		bool wantToEnter = !pNewEntity->isInWorld();
		if (wantToEnter)
		{
			// Note: it may already be loading its prerequisites, and in
			// fact it may complete that here. (Or if not then we'll add it
			// to the prereqs list twice). The code that checks the prereqs
			// list in tick handles all these cases: being in list twice,
			// being in list when already in world, etc.
			bool prereqsSatisfied = pNewEntity->checkPrerequisites();
			if (!prereqsSatisfied)
			{
				wantToEnter = false;

				cachedEntities_[ id ] = pNewEntity;
				prereqEntities_.push_back( pNewEntity );

				UnknownEntity & ue = unknownEntities_[ id ];
				ue.spaceID = spaceID;
				ue.vehicleID = vehicleID;
			}
		}

		if (wantToEnter && vehicleID != 0)
		{
			Entity * pVehicle = EntityManager::instance().getEntity( vehicleID, true );
			if (pVehicle == NULL || !pVehicle->isInWorld() )
			{
				wantToEnter = false;

				cachedEntities_[ id ] = pNewEntity;
				passengerEntities_.push_back( pNewEntity );

				UnknownEntity & ue = unknownEntities_[ id ];
				ue.spaceID = spaceID;
				ue.vehicleID = vehicleID;
			}
		}

		if (wantToEnter)
		{
			enteredEntities_[ id ] = pNewEntity;
			pNewEntity->enterWorld( spaceID, vehicleID, false );
		}
	}
	else
	{
		MF_ASSERT( !pNewEntity->isInWorld() );
		// cannot be still in world when enterCount is <= 0
		// that is always handled in leaveEntity.
		cachedEntities_[ id ] = pNewEntity;
	}

	// get time of message (or game time if offline)
	double time = this->lastMessageTime();
	if (time < 0.f) time = getGameTotalTime();

	// and pass the position update to the (mandatory) filter
	pNewEntity->filter().reset( time - 0.001 );
	pNewEntity->filter().input( time, spaceID, vehicleID, pos, auxFilter );
}


/**
 *	This method is called to update an entity. This message is sent by the
 *	server when a LoD level changes and a number of property changes are sent in
 *	one message. (Or at the server's discretion, it may group multiple updates
 *	into one message at other times).
 */
void EntityManager::onEntityProperties( ObjectID id,
	BinaryIStream & data, bool oldVersion )
{
	bool inWorld = true;

	// find the entity
	Entities::iterator iter = enteredEntities_.find( id );
	if (iter == enteredEntities_.end())
	{
		// complain if it wasn't in the world
		// (but accept the updates anyway if we can)
		inWorld = false;
		WARNING_MSG( "EntityManager::onEntityProperties: "
			"Got properties for %d who is not entered.\n", id );

		iter = cachedEntities_.find( id );
		if (iter == cachedEntities_.end())
		{
			// abort if it wasn't outside the world either
			WARNING_MSG( "EntityManager::onEntityProperties: "
				"Got properties for %d who does not exist!\n", id );
			return;
		}
	}

	// now stream off all the property updates
	iter->second->updateProperties( data, /*shouldCallSetMethod:*/inWorld );
}


/**
 *	This function is called when the server indicates that an entity has entered
 *	the player's AoI.
 *
 *	It is complicated because it may be called out of order.
 */
void EntityManager::onEntityEnter( ObjectID id,
	SpaceID spaceID, ObjectID vehicleID )
{
	Entity * pEntity = NULL;
	Entities::iterator iter;

	// look in the cache first
	if ((iter=cachedEntities_.find( id )) != cachedEntities_.end())
	{
		Entity * pDebutant = pEntity = iter->second;

		if (pDebutant->incrementEnter() > 0)
		{
			if (pDebutant->enterCount() != 1)
			{
				WARNING_MSG( "EntityManager::enterEntity(%d): "
					"enterCount is now %d.\n", id, pDebutant->enterCount() );
			}

			// store space and vehicle in case it reentered while
			// we were still loading its prerequisites, and we finish loading
			// the prereqs before the missing leave and future create
			// messages arrive
			Unknowns::iterator uiter = unknownEntities_.find( id );
			if (uiter != unknownEntities_.end())
			{
				uiter->second.spaceID = spaceID;
				uiter->second.vehicleID = vehicleID;
			}

			// if it's a client-side entity then the server isn't going to send
			// us a create message, so we should do all the processing here
			if (pDebutant == Player::entity() || // player gets create b4 enter
				pDebutant->isClientOnly())
			{
				// This seems a reasonable assumption but may need 
				// to be changed in the future.
				MF_ASSERT( vehicleID == 0 && "Client only entities should " \
					"not start already aboard a vehicle." );

				// if it has no prerequisites or has satisfied them already,
				// then we can immediately add it to the entered list
				if (pDebutant->checkPrerequisites())
				{
					cachedEntities_.erase( iter );
					enteredEntities_[ id ] = pDebutant;

					unknownEntities_.erase( id );

					pDebutant->enterWorld( spaceID, vehicleID, false );
					this->forwardQueuedMessages( pDebutant );
				}
				// otherwise it has just started loading its prerequisites,
				// so add it to a list of entities to check on
				else
				{
					prereqEntities_.push_back( pDebutant );

					UnknownEntity & ue = unknownEntities_[ id ];
					ue.spaceID = spaceID;
					ue.vehicleID = vehicleID;
				}
			}
		}
	}
	// ok, try the entered list
	else if ((iter=enteredEntities_.find( id )) != enteredEntities_.end())
	{
		pEntity = iter->second;

		pEntity->incrementEnter();

		if (pEntity != Player::entity())
		{
			// presumably there is a missing leaveEntity on its way
			WARNING_MSG( "EntityManager::enterEntity(%d): "
				"Entity is now entered %d times\n", id, pEntity->enterCount() );
			// .. since we ask for the update below, there's no problem with
			// ignoring the new space and vehice ids
		}
	}
	// it must be an unknown entity then
	else
	{
		// increment its count, possibly creating the record for it
		if (++unknownEntities_[id].count == 0)
		{
			// entity entered and left but packet order was swapped
			unknownEntities_.erase( id );
		}
	}

	// To keep things simple, always request an update from the server when
	// we get one of these, regardless of what state we think the entity is in.
	// The server will ignore the request if it is not in our AoI (or it has
	// already sent the 'create' message) at the time it receives the request.
	// (the player gets its 'create' messages via a different mechanism)
	// Don't request update if it we're not connected to the server or 
	// if it's a client-only entity.
	if ((pServer_ != NULL) &&
		!EntityManager::isClientOnlyID( id ) &&
		// definitely request update if we don't know anything about entity
		((pEntity == NULL) || 
		// Otherwise request update for all entities except...
		// Must not request update for witness entity - server will complain.
		// Must not request update for self-controlled entity - this is mainly
		// a hack to get around the fact that we call this function from 
		// Entity::controlled() but it is a good condition anyway.
		 ((id != proxyEntityID_) && !pEntity->selfControlled())))
	{
		pServer_->requestEntityUpdate( id,
			pEntity ? pEntity->cacheStamps() : CacheStamps() );
	}
}


/**
 *	This function is called when the server indicates that an entity has left
 *	the player's AoI. It is complicated because it may be called out of order.
 */
void EntityManager::onEntityLeave( ObjectID id, const CacheStamps & cacheStamps )
{
	Entities::iterator iter;

	// look in the entered list first
	if ((iter = enteredEntities_.find( id )) != enteredEntities_.end() )
	{
		// should not be in the entered list if its enterCount is <= 0
		MF_ASSERT( iter->second->enterCount() > 0 );

		// decrement its enterCount and see if that means it should go
		if (iter->second->decrementEnter() == 0)
		{
			Entity * pGeriatric = iter->second;

			pGeriatric->leaveWorld( false );

			enteredEntities_.erase( iter );
			/*
			cachedEntities_[ id ] = pGeriatric;
			pGeriatric->cacheStamps( cacheStamps );
			*/
			clearEntityDict( pGeriatric );
			Py_DECREF( pGeriatric );	// no caching for now
		}
		else
		{
			INFO_MSG( "EntityManager::leaveEntity(%d): "
					"enterCount is still %d.\n",
				id, iter->second->enterCount() );
		}
	}
	// ok, try the cached list
	else if ((iter = cachedEntities_.find( id )) != cachedEntities_.end())
	{
		// the only reason it will be in cachedEntities currently
		// (with the code above commented out) is if it was still loading
		// its prereqs when we got the leave, so the message below will
		// never appear
		if (iter->second->enterCount() <= 0)
		{
			WARNING_MSG( "EntityManager::leaveEntity(%d): "
				"Got leave for non-entered entity\n", id );
		}

		if (iter->second->decrementEnter() == 0)
		{
			// if we just made its count be zero, then dump this entity
			Entity * pGeriatric = iter->second;

			cachedEntities_.erase( iter );
			clearEntityDict( pGeriatric );
			Py_DECREF( pGeriatric );

			// it was loading its prereqs, so erase it from unknowns
			this->clearQueuedMessages( id );
			// the entity will be kept alive by the reference to it in
			// prereqEntities, which will go (and destruct the entity)
			// when its prerequisites have finised loading - we can't
			// touch it before then
		}
	}
	// ok it's an unknown entity then: an ordinary unknown entity that
	// hasn't received the onEntityCreate yet; not a 'loading-prereqs' one
	else
	{
		UnknownEntity & unknown = unknownEntities_[id];
		if (--unknown.count == 0)
		{
			// the count has got back to zero, so erase it from unknowns
			this->clearQueuedMessages( id );
		}
		else if (unknown.count < 0)
		{
			WARNING_MSG( "EntityManager::leaveEntity(%d): "
				"Got leave for unheard-of entity\n", id );
			// we intentionally made an unknowns entry for it above so when we
			// get the enter that has not yet arrived it will cancel it out
		}
	}
}

#include "cstdmf/profile.hpp"

/**
 *	This method is called when we receive a property update message
 *	for one of our client-side entities from the server.
 */
void EntityManager::onEntityProperty( ObjectID id, int messageID,
	BinaryIStream & data, bool oldVersion )
{
	const int PROPERTY_FLAG = 0x40;
	// Figure out what time it happened at
	double packetTime = this->lastMessageTime();
	if (packetTime < 0.f) packetTime = getGameTotalTime();

	// And pass it on to the appropriate entity
	Entities::iterator iter = enteredEntities_.find( id );
	if (iter != enteredEntities_.end())
	{
		iter->second->handleProperty( messageID, data );
	}
	else
	{
		// or save it for later if it's not around
		int length = data.remainingLength();
		MemoryOStream	*pMos = new MemoryOStream( length );
		pMos->transfer( data, length );

		queuedMessages_[ id ].push_back(
			std::make_pair( messageID | PROPERTY_FLAG, pMos ) );

		// TODO: Make sure this doesn't get too big. Options include:
		//	- delivering (some) messages to cached entities
		//	- cleaning up really old messages
		//	- definitely when an entity is cleared from the cache, get
		//		rid of its queue, 'coz you'll know there isn't an enter
		//		or create message for it in an out-of-order/dropped packet
	}
}

/**
 *	This method is called when we receive a script method call message
 *	for one of our client-side entities from the server.
 */
void EntityManager::onEntityMethod( ObjectID id, int messageID,
	BinaryIStream & data, bool oldVersion )
{
	// Figure out what time it happened at
	double packetTime = this->lastMessageTime();
	if (packetTime < 0.f) packetTime = getGameTotalTime();

	// And pass it on to the appropriate entity
	Entity * pEntity = NULL;
	Entities::iterator iter = enteredEntities_.find( id );
	if (iter != enteredEntities_.end())
		pEntity = iter->second;
	else if (Player::entity() != NULL && Player::entity()->id() == id)
		pEntity = Player::entity();

	if (pEntity != NULL)
	{
		pEntity->handleMethod( messageID, data );
	}
	else
	{
		// or save it for later if it's not around
		int length = data.remainingLength();
		MemoryOStream	*pMos = new MemoryOStream( length );
		pMos->transfer( data, length );

		queuedMessages_[ id ].push_back( std::make_pair( messageID, pMos ) );

		// TODO: Make sure this doesn't get too big. Options include:
		//	- delivering (some) messages to cached entities
		//	- cleaning up really old messages
		//	- definitely when an entity is cleared from the cache, get
		//		rid of its queue, 'coz you'll know there isn't an enter
		//		or create message for it in an out-of-order/dropped packet
	}
}

/**
 *	This method is called when we receive a fast-track avatar update
 *	from the server
 */
void EntityManager::onEntityMove( ObjectID id,
	SpaceID spaceID, ObjectID vehicleID, const Position3D & pos,
	float yaw, float pitch, float roll, bool isVolatile )
{
	// Figure out what time it happened at
	double packetTime = this->lastMessageTime();
	if (packetTime < 0.f) packetTime = getGameTotalTime();

	// And tell the entity's filter, if it has one
	Entities::iterator	iter = enteredEntities_.find( id );
	if (iter != enteredEntities_.end())
	{
		Entity * pE = iter->second;

		PyObject * poseVolatileNotifier = NULL;
		if (pE->isPoseVolatile() != isVolatile)
			poseVolatileNotifier = pE->isPoseVolatile( isVolatile );

		if (pE->selfControlled()) // this is a forced position / physics correction
		{
			pE->filter().reset( packetTime );
		}
		float	auxVolatile[3] = { yaw, pitch, roll };
		pE->filter().input( packetTime, spaceID, vehicleID, pos, auxVolatile );

		// make it take effect immediately
		if (pE->selfControlled())
		{
			pE->filter().output( packetTime );

			// send back ACK
			if (!pE->isClientOnly())
			{
				pServer_->addMove( id, spaceID, vehicleID, pos,
					yaw, pitch, roll,/*onGround:*/false,
					/*globalPosition:*/pE->position() );

				pE->physicsCorrected( pServer_->lastSendTime() );
			}
		}

		if (poseVolatileNotifier != NULL)
			Script::call( poseVolatileNotifier,
				Py_BuildValue( "(i)", int(isVolatile) ),
				"Entity::isPoseVolatile: ", false );
	}
	else
	{
		iter = cachedEntities_.find( id );
		bool storeInUnknowns = true;

		if (iter != cachedEntities_.end())
		{
			Entity * pEntity = iter->second;

			MF_ASSERT( pEntity != NULL );
			if (pEntity->enterCount() <= 0)
			{
				storeInUnknowns = false;
				WARNING_MSG( "EntityManager::avatarUpdate: "
					"Got update for %d who is cached\n", id );
			}
			// else store it in unknowns for when we put it in the world
		}

		if (storeInUnknowns)
		{
			// record it in unknowns.
			UnknownEntity & sleeper = unknownEntities_[ id ];
			sleeper.time = packetTime;
			sleeper.pos = pos;
			sleeper.spaceID = spaceID;
			sleeper.vehicleID = vehicleID;
			sleeper.auxFilter[0] = yaw;
			sleeper.auxFilter[1] = pitch;
			sleeper.auxFilter[2] = roll;

			/*
			WARNING_MSG( "EntityManager::avatarUpdate: "
				"Received update for %d who we do not know about.\n", id );
			*/
		}
	}
}


/**
 *	This method is called when the server tells us that the proxy has changed.
 */
void EntityManager::onEntitiesReset( bool keepPlayerAround )
{
	TRACE_MSG( "EntityManager::onEntitiesReset: keepPlayerAround = %d\n",
		keepPlayerAround );
	this->clearAllEntities( keepPlayerAround, /*keepClientOnly:*/ true );
}


/**
 *	This method removes all entities from the world 
 *	expect, if requested, the player.
 *
 *	@param keepPlayerAround	true if the player should be kept around.
 */
void EntityManager::clearAllEntities( bool keepPlayerAround, bool keepClientOnly )
{
	class DeleteCondition
	{
	public:
		DeleteCondition( bool keepPlayerAround, bool keepClientOnly ) :
			keepPlayerAround_( keepPlayerAround ),
			keepClientOnly_( keepClientOnly )
		{
		}

		bool operator()( const Entity * pEntity ) const
		{
			return (!keepPlayerAround_ || 
				(pEntity != Player::instance().entity())) &&
			(!keepClientOnly_ || !pEntity->isClientOnly());
		}

	private:
		bool keepPlayerAround_;
		bool keepClientOnly_;
	};
	DeleteCondition shouldDelete( keepPlayerAround, keepClientOnly );

	// Call leaveWorld() on all entities
	Entities::iterator iter = enteredEntities_.begin();
	while (iter != enteredEntities_.end())
	{
		/*
		if ((!keepPlayerAround ||
				(iter->second != Player::instance().entity())) &&
			(!keepClientOnly || !iter->second->isClientOnly()))
		*/
		if (shouldDelete( iter->second))
		{
			iter->second->leaveWorld( false );
		}

		iter++;
	}

	// get rid of any player things
	if (!keepPlayerAround)
	{
		// Clear the players space
		if (Player::instance().entity() &&
			Player::instance().entity()->pSpace())
		{
			Player::entity()->pSpace()->clear();
		}
		// NOTE: Must be done after calling leaveWorld() on player entity,
		// otherwise leaveWorld() would be called on the wrong type.
		Player::instance().setPlayer( NULL );
		proxyEntityID_ = 0;

		if (!keepClientOnly)
		{
			// reset the next client ID
			nextClientID_ = FIRST_CLIENT_ID;
		}
	}

	// dispose of everything in our cache
	iter = cachedEntities_.begin();
	Entities::iterator iend = cachedEntities_.end();
	while (iter != iend)
	{
		/*
		if ((!keepPlayerAround ||
				(iter->second != Player::instance().entity())) &&
			(!keepClientOnly || !iter->second->isClientOnly()))
		*/
		if (shouldDelete( iter->second ))
		{
			clearEntityDict( iter->second );
			Py_DECREF( iter->second );
			iter = cachedEntities_.erase(iter);
		}
		else
		{
			++iter;
		}
	}

	// dispose of everything in our entered list
	iter = enteredEntities_.begin();
	iend = enteredEntities_.end();
	while (iter != iend)
	{
		/*
		if ((!keepPlayerAround ||
				(iter->second != Player::instance().entity())) &&
			(!keepClientOnly || !iter->second->isClientOnly()))
		*/
		if (shouldDelete( iter->second ))
		{
			clearEntityDict( iter->second );
			Py_DECREF( iter->second );
			iter = enteredEntities_.erase(iter);
		}
		else
		{
			++iter;
		}
	}

	// and even the unknown entities and prerequisite entity list
	clearEntitiesDict(prereqEntities_);
	prereqEntities_.clear();

	clearEntitiesDict(passengerEntities_);
	passengerEntities_.clear();

	unknownEntities_.clear();

	// don't leave the queued messages either
	for (MessageMap::iterator iter = queuedMessages_.begin();
		 iter != queuedMessages_.end();
		 iter++)
	{
		for (MessageQueue::iterator qIter = iter->second.begin();
			 qIter != iter->second.end();
			 qIter++)
		{
			delete qIter->second;
		}
	}
	queuedMessages_.clear();

	PyGC_Collect();
}


/**
 *	This function is called to call the tick functions of all the
 *	entities. Note: These functions do not do things like animation -
 *	that is left to Models that are actually in the scene graph.
 */
void EntityManager::tick( double timeNow, double timeLast )
{
	// see if any of the entities waiting on prerequisites are now ready
	for (uint32 i = 0; i < prereqEntities_.size(); i++)
	{
		Entity * pEntity = &*prereqEntities_[i];
		if (pEntity->checkPrerequisites())
		{
			if (pEntity->enterCount() > 0 && !pEntity->isInWorld())
			{
				// can be already in world if in prereqs list twice
				ObjectID id = pEntity->id();

				// get space and vehicle ID out of unknowns
				Unknowns::iterator uit = unknownEntities_.find( id );
				MF_ASSERT( uit != unknownEntities_.end() );

				Entity * pVehicle = NULL;

				if( uit->second.vehicleID == 0 || 
					( ( pVehicle = EntityManager::instance().getEntity( 
											uit->second.vehicleID, true ) ) &&
					 pVehicle->isInWorld() ) )
				{
					cachedEntities_.erase( id );
					enteredEntities_[ id ] = pEntity;


					UnknownEntity ue = uit->second;
					unknownEntities_.erase( uit );

					// enter the entity
					pEntity->enterWorld( ue.spaceID, ue.vehicleID, false );

					// now send it any queued messages
					this->forwardQueuedMessages( pEntity );
				}
				else
				{
					passengerEntities_.push_back( prereqEntities_[i] );
				}
			}

			std::swap<>( prereqEntities_[i], prereqEntities_.back() );
			prereqEntities_.pop_back();

			i--;
		}
	}


	for (uint32 i = 0; i < passengerEntities_.size(); i++)
	{
		Entity * pEntity = &*passengerEntities_[i];

		if (pEntity->enterCount() > 0 && !pEntity->isInWorld())
		{
			Unknowns::iterator iUnknown = unknownEntities_.find( pEntity->id() );

			if( iUnknown != unknownEntities_.end() )
			{
				Entity * pVehicle = EntityManager::instance().getEntity(
					iUnknown->second.vehicleID, true );

				if ( pVehicle != NULL && pVehicle->isInWorld() )
				{
					cachedEntities_.erase( pEntity->id() );
					enteredEntities_[ pEntity->id() ] = pEntity;

					// enter the entity
					pEntity->enterWorld(	iUnknown->second.spaceID,
											iUnknown->second.vehicleID,
											false );

					// now send it any queued messages
					this->forwardQueuedMessages( pEntity );

					unknownEntities_.erase( iUnknown );
				}
				else
				{
					continue;
				}
			}
		}

		std::swap<>( passengerEntities_[i], passengerEntities_.back() );
		passengerEntities_.pop_back();

		i--;
	}


	// now tick all the entities that are in the world
	for (Entities::iterator iter = enteredEntities_.begin();
		iter != enteredEntities_.end();
		iter++)
	{
		iter->second->tick( timeNow, timeLast );
	}
}


/**
 *	This method gets the given entity ID if it exists in the world.
 *	Note that you should think twice before using this function,
 *		because the entity manager may be the best place to do
 *		whatever you were going to do with the Entity.
 *	This function does not increment the reference count of the
 *		entity it returns. Returns NULL if it can't be found.
 */
Entity * EntityManager::getEntity( ObjectID id, bool lookInCache )
{
	Entities::iterator iter = enteredEntities_.find( id );
	if (iter != enteredEntities_.end()) return iter->second;

	if (lookInCache)
	{
		iter = cachedEntities_.find( id );
		if (iter != cachedEntities_.end()) return iter->second;
	}

	return NULL;
}


/**
 *	This is an internal method used to forward any messages queued
 *	for an entity to that entity, erasing them in the process.
 */
bool EntityManager::forwardQueuedMessages( Entity * pEntity )
{
	const int PROPERTY_FLAG = 0x40;
	bool hasForwardedMessages = false;
	MessageMap::iterator iter = queuedMessages_.find( pEntity->id() );

	if (iter != queuedMessages_.end())
	{
		hasForwardedMessages = true;

		for (MessageQueue::iterator qIter = iter->second.begin();
			qIter != iter->second.end();
			qIter++)
		{
			int msgID = qIter->first;
			if (msgID & PROPERTY_FLAG)
			{
				pEntity->handleProperty( msgID & ~PROPERTY_FLAG ,
					*qIter->second );
			}
			else
			{
				pEntity->handleMethod( msgID, *qIter->second );
			}
			delete qIter->second;
		}

		queuedMessages_.erase( iter );
	}

	return hasForwardedMessages;
}

/**
 *	This is an internal method used to clear any messages queued for an entity.
 */
void EntityManager::clearQueuedMessages( ObjectID id )
{
	// clear messages
	MessageMap::iterator mit = queuedMessages_.find( id );
	if (mit != queuedMessages_.end())
	{
		for (MessageQueue::iterator qIter = mit->second.begin();
			qIter != mit->second.end();
			qIter++)
		{
			delete qIter->second;
		}
		queuedMessages_.erase( mit );
	}

	// and erase any unknown entity record
	unknownEntities_.erase( id );
}


/**
 *	This method sets the function which determines when the time
 *	of day from game time.
 */
void EntityManager::setTimeInt( ChunkSpacePtr pSpace, TimeStamp gameTime,
	float initialTimeOfDay, float gameSecondsPerSecond )
{
	TimeOfDay & timeOfDay = *pSpace->enviro().timeOfDay();
	timeOfDay.updateNotifiersOn( false );

	TRACE_MSG( "EntityManager::setTimeInt: gameTime %d initial %f gsec/sec %f\n",
		gameTime, initialTimeOfDay, gameSecondsPerSecond );

	timeOfDay.secondsPerGameHour((1.0f / gameSecondsPerSecond) * 3600.0f);

	DEBUG_MSG( "\tsec/ghour = %f\n", (1.0f / gameSecondsPerSecond) * 3600.0f );

	// This gives us the time of day as in game seconds, since the start of time.
	float tod = initialTimeOfDay + ((float)gameTime /
		(float)ServerConnection::updateFrequency() *
		(float)gameSecondsPerSecond);

	DEBUG_MSG( "\ttherefore tod in seconds = %f\n", tod );

	// Set the time of day in hours.
	timeOfDay.gameTime(tod / 3600.0f);
	timeOfDay.tick(0.0f);

	timeOfDay.updateNotifiersOn( true );
}


/**
 *	Get accessor for the time that the last message occurred
 */
double EntityManager::lastMessageTime() const
{
	if (!AnnalVault::isPlaying())
	{		// common-case first
		return pServer_ != NULL ? pServer_->lastMessageTime() : -1.0;
	}

	return playbackLastMessageTime_;
}


/**
 *	Set accessor for the time that the last message occurred
 *	 (only used during server message playback)
 */
void EntityManager::lastMessageTime( double t )
{
	playbackLastMessageTime_ = t;
}


/*~ callback Entity.onGeometryMapped
 *
 *	This callback method tells the player entity about changes to
 *	the geometry in its current space.  It is called when geometry
 *	is mapped into the player's current space.  The name of the
 *	spaces geometry is passed in as a parameter
 *
 *	@param spaceName	name describing the space's geometry
 */
/**
 *	We got some space data.
 */
void EntityManager::spaceData( SpaceID spaceID, SpaceEntryID entryID,
	uint16 key, const std::string & data )
{
	// Not at all sure that parsing of this data should be left up to the
	// space, but I certainly don't want to have a parallel set of classes
	// manging the data for each space - so the space can at least store it.
	ChunkSpacePtr pSpace = ChunkManager::instance().space( spaceID );
	seenSpaces_.insert( pSpace );
	uint16 okey = pSpace->dataEntry( entryID, key, data );

	// Acutally, for client friendliness, we'll parse the data ourselves here.
	if (okey == SPACE_DATA_TOD_KEY)
	{
		const std::string * pData =
			pSpace->dataRetrieveFirst( SPACE_DATA_TOD_KEY );
		if (pData != NULL && pData->size() >= sizeof(SpaceData_ToDData))
		{
			SpaceData_ToDData & todData = *(SpaceData_ToDData*)pData->data();
			this->setTimeInt( pSpace, pServer_->lastGameTime(),
				todData.initialTimeOfDay, todData.gameSecondsPerSecond );
		}
	}
	else if (okey == SPACE_DATA_MAPPING_KEY_CLIENT_SERVER ||
			okey == SPACE_DATA_MAPPING_KEY_CLIENT_ONLY)
	{
		// see if this mapping is being added
		if (key == okey)
		{
			MF_ASSERT( data.size() >= sizeof(SpaceData_MappingData) );
			SpaceData_MappingData & mappingData =
				*(SpaceData_MappingData*)data.data();
			std::string path( (char*)(&mappingData+1),
				data.length() - sizeof(SpaceData_MappingData) );
			pSpace->addMapping( entryID, &mappingData.matrix[0][0], path );

			// tell the player about this if it is relevant to it
			// this system probably wants to be expanded in future
			Entity * pPlayer = Player::entity();
			if (pPlayer != NULL && pPlayer->pSpace() == pSpace)
			{
				Script::call(
					PyObject_GetAttrString( pPlayer, "onGeometryMapped" ),
					Py_BuildValue( "(s)", path.c_str() ),
					"EntityManager::spaceData geometry notifier: ",
					true );
				Player::instance().updateWeatherParticleSystems(
					pSpace->enviro().playerAttachments() );
			}
		}
		// ok it's being removed then
		else
		{
			MF_ASSERT( key == uint16(-1) );
			pSpace->delMapping( entryID );
		}
	}
}

/**
 *	This space is no longer with us.
 */
void EntityManager::spaceGone( SpaceID spaceID )
{
	ChunkSpacePtr pSpace = ChunkManager::instance().space( spaceID );
	pSpace->clear();
	seenSpaces_.erase( pSpace );
}


void EntityManager::fini()
{
	this->disconnected();
	seenSpaces_.clear();
}


/*~ callback Entity.onRestore
 *
 *	This callback method informs the player entity to restore
 *	itself to a previous state given the description provided by the server.

 *	@param position		position
 *	@param direction	direction
 *	@param spaceID		space
 *	@param vehicleID	vehicle
 *	@param dict			dictonary of attributes
 */
/**
 *	This method is called when the server provides us with the details
 *	necessary to restore the client to a previous state.
 */
void EntityManager::onRestoreClient( ObjectID id,
		SpaceID spaceID, ObjectID vehicleID,
		const Position3D & pos, const Direction3D & dir,
		BinaryIStream & data, bool oldVersion )
{
	// TODO: Do something with spaceID and vehicleID
	TRACE_MSG( "EntityManager::onRestoreClient: "
			"id = %d. vehicleID = %d.\n",
		id, vehicleID );
	Entity * pPlayer = Player::entity();

	if (pPlayer && id == pPlayer->id())
	{
		const EntityDescription & entityDesc = pPlayer->type().description();
		PyObjectPtr pDict( PyDict_New(), PyObjectPtr::STEAL_REFERENCE );

		entityDesc.readStreamToDict( data,
			EntityDescription::CLIENT_DATA, pDict.getObject() );

		PyObjectPtr pPos = Script::getData( pos );
		PyObjectPtr pDir = Script::getData( *(Vector3 *)&dir );

		Script::call( PyObject_GetAttrString( pPlayer, "onRestore" ),
				Py_BuildValue( "OOiiO",
					pPos.getObject(),
					pDir.getObject(),
					spaceID,
					vehicleID,
					pDict.getObject() ),
				"", true );
#if 0
		PyObjectPtr pCurrDict( PyObject_GetAttrString( pPlayer, "__dict__" ),
			PyObjectPtr::STEAL_REFERENCE );

		Physics * pPhysics = pPlayer->pPhysics();

		if (pPhysics)
		{
			pPhysics->teleport( pos, dir );
		}

		MF_ASSERT( pCurrDict );
		if (pCurrDict)
		{
			PyDict_Update( pCurrDict.getObject(), pDict.getObject() );
			Script::call( PyObject_GetAttrString( pPlayer, "onRestore" ),
				Py_BuildValue( "O", pDict.getObject() ), "", true );
		}
#endif

		typedef std::vector< EntityID > IDs;
		IDs ids;
		{
			Entities::iterator iter = enteredEntities_.begin();

			while (iter != enteredEntities_.end())
			{
				if (!iter->second->isClientOnly())
				{
					ids.push_back( iter->first );
				}

				++iter;
			}
		}

		{
			IDs::iterator iter = ids.begin();

			while (iter != ids.end())
			{
				if (*iter != pPlayer->id())
				{
					this->onEntityLeave( *iter );
				}

				++iter;
			}
		}

		INFO_MSG( "EntityManager::onRestoreClient: enteredEntities_.size() = %d\n",
			enteredEntities_.size() );
	}
	else
	{
		CRITICAL_MSG( "EntityManager::onRestoreClient: "
				"The entity to restore (%d) is not the player (%d)\n",
			id, (pPlayer ? pPlayer->id() : 0) );
	}
}


/**
 *	This method is called when a resource update is on its way.
 */
void EntityManager::onImpendingVersion( ImpendingVersionProximity proximity )
{
	ResUpdateHandler::instance().impendingVersion( proximity );
}


// -----------------------------------------------------------------------------
// Section: Out-of-place handlers
// -----------------------------------------------------------------------------

/**
 *	This method handles voice data sent by another client.
 */
void EntityManager::onVoiceData( const Mercury::Address & srcAddr,
							  BinaryIStream & data )
{
	int length = data.remainingLength();
	data.retrieve( length );
}

/**
 *	This method handles proxy data sent from the baseapp and
 *	invoke onProxyDataDownloadComplete callback function on
 *	the personality script
 */
void EntityManager::onProxyData( uint16 proxyDataID, 
								BinaryIStream & data )
{
	int length = data.remainingLength();

	if (length <= 0)
	{
		ERROR_MSG( "EntityManager::onProxyData: received zero length"
			" of data\n" );
		return;
	}

	//DEBUG_MSG( "EntityManager::onProxyData: receive %d bytes\n", length );

	PyObject * pProxyData = PyString_FromStringAndSize(
					(char *)data.retrieve( length ), length );

	if (pProxyData == NULL)
	{
		ERROR_MSG( "EntityManager::onProxyData: proxy data is an invalid"
			" string\n" );
		return;
	}
	Script::call( PyObject_GetAttrString( Personality::instance(),
						"onProxyDataDownloadComplete" ),
						Py_BuildValue( "(iO)", proxyDataID, pProxyData ),
						"App::onProxyDataDownloadComplete",
						/*okIfFunctionNull*/ true );
	Py_DECREF( pProxyData );
}


// -----------------------------------------------------------------------------
// Section: Python module functions
// -----------------------------------------------------------------------------

/*~ function BigWorld entity
 *  Returns the entity with the given id, or None if not found.
 *  This function can search all entities known to the client, or only entities
 *  which are not cached. An entity only becomes cached if in an online game
 *  the server indicates to the client that the entity has left the player's
 *  area of interest.
 *  @param id An integer representing the id of the entity to return.
 *  @param lookInCache An optional boolean which instructs the function to search
 *  for the entity amongst the entities which are cached. Otherwise, this
 *  function will only search amongst non-chached entities. This argument
 *  defaults to false.
 *  @return The entity corresponding to the id given, or None if no such
 *  entity is found.
 */
/**
 *	Returns the entity with the given id, or None if not found.
 *	The entity must be in the world (i.e. not cached).
 *	May want to rethink the decision stated in the line above.
 */
static PyObject * py_entity( PyObject * args )
{
	ObjectID	id;
	int			lookInCache = 0;
	if (!PyArg_ParseTuple( args, "i|i", &id, &lookInCache ))
	{
		PyErr_SetString( PyExc_TypeError,
			"py_entity: Argument parsing error." );
		return NULL;
	}

	PyObject * pEntity = EntityManager::instance().getEntity(
		id, lookInCache != 0 );

	if (pEntity != NULL)
	{
		Py_INCREF( pEntity );
		return pEntity;
	}

	Py_Return;
}
PY_MODULE_FUNCTION( entity, BigWorld )

/*~ function BigWorld createEntity
 *  Creates a new entity on the client and places it in the world. The 
 *	resulting entity will have no base or cell part.
 *
 *  @param className	The string name of the entity to instantiate.
 *	@param spaceID		The id of the space in which to place the entity.
 *	@param vehicleID	The id of the vehicle on which to place the entity
 *						(0 for no vehicle).
 *  @param position		A Vector3 containing the position at which the new
 *						entity is to be spawned.
 *  @param direction	A Vector3 containing the initial orientation of the new
 *						entity (roll, pitch, yaw).
 *  @param state		A dictionary describing the initial states of the
 *						entity's properties (as described in the entity's .def
 *						file). A property will take on it's default value if it
 *						is not listed here.
 *  @return				The ID of the new entity, as an integer.
 *
 *  Example:
 *  @{
 *  # create an arrow style Info entity at the same position as the player
 *  p = BigWorld.player()
 *  direction = ( 0, 0, p.yaw )
 *  properties = { 'modelType':2, 'text':'Created Info Entity'}
 *  BigWorld.createEntity( 'Info', p.spaceID, 0, p.position, direction, properties )
 *  @}
 */
/**
 *	Creates a new client-side entity
 */
static PyObject * py_createEntity( PyObject * args )
{
	SpaceID spaceID;
	ObjectID vehicleID;
	float x, y, z, pitch, roll, yaw;
	PyObject* pDict;
	char * className;

	if (!PyArg_ParseTuple(args, "sii(fff)(fff)O!", &className,
			&spaceID, &vehicleID, &x, &y, &z,
			&roll, &pitch, &yaw, &PyDict_Type, &pDict))
	{
		return NULL;
	}

	// Now find the index of this class
	EntityType * pType = EntityType::find( className );
	if (pType == NULL)
	{
		PyErr_Format( PyExc_ValueError, "BigWorld.createEntity: "
			"Class %s is not an entity class", className );
		return NULL;
	}


	// choose an ID for our entity
	ObjectID id = EntityManager::nextClientID();

	// make an player entity if we have none
	bool makingPlayer = false;
	if (Player::entity() == NULL)
	{
		makingPlayer = true;

		// fake up a creation stream of base data (not so nice I know)
		MemoryOStream	stream( 4096 );
		pType->description().addDictionaryToStream( pDict, stream,
			EntityDescription::BASE_DATA |
			EntityDescription::CLIENT_DATA |
			EntityDescription::EXACT_MATCH );

		EntityManager::instance().onBasePlayerCreate( id, pType->index(),
			stream, /*oldVersion:*/false );
	}

	// fake up a creation stream of cell data (really sucks!)
	MemoryOStream	stream( 4096 );
	uint8 * pNumProps = (uint8*)stream.reserve( sizeof(uint8) );
	*pNumProps = 0;

	uint8 nprops = (uint8)pType->description().clientServerPropertyCount();
	for (uint8 i = 0; i < nprops; i++)
	{
		DataDescription * pDD = pType->description().clientServerProperty( i );
		if (!makingPlayer && !pDD->isOtherClientData()) continue;

		PyObjectPtr pVal = NULL;
		if (pDict != NULL)
			pVal = PyDict_GetItemString( pDict, (char*)pDD->name().c_str() );
		if (!pVal) pVal = pDD->pInitialValue();
		PyErr_Clear();

		stream << i;
		pDD->addToStream( &*pVal, stream, false );

		(*pNumProps)++;
	}

	// create it / put in the cell data
	EntityManager::instance().onEntityCreate( id, pType->index(),
		spaceID, vehicleID, Position3D( x, y, z ), yaw, pitch, roll,
		stream, /*oldVersion:*/false );

	// and enter it unto the world
	EntityManager::instance().onEntityEnter( id, spaceID, vehicleID );

	return Script::getData( id );
}
PY_MODULE_FUNCTION( createEntity, BigWorld )



/*~ function BigWorld destroyEntity
 *  Destroys an exiting client-side entity.
 *  @param id The id of the entity to destroy.
 *
 *  Example:
 *  @{
 *  id = BigWorld.target().id # get current target ID
 *  BigWorld.destroyEntity( id )
 *  @}
 */
/**
 *	Destroys an existing client-side entity
 */
static PyObject * py_destroyEntity( PyObject * args )
{
	int id;

	if ( !PyArg_ParseTuple( args, "i", &id ) )
	{
		return NULL;
	}

	if (!EntityManager::isClientOnlyID( id ))
	{
		PyErr_Format( PyExc_ValueError, "BigWorld.destroyEntity: "
										"id is not a client only entity id" );
		return NULL;
	}

	EntityManager::instance().onEntityLeave( id );

	Py_Return;
}
PY_MODULE_FUNCTION( destroyEntity, BigWorld )



// -----------------------------------------------------------------------------
// Section: Recording and playback
// -----------------------------------------------------------------------------


struct SavedEntityState
{
	int							enterCount;
	int							type;
	SpaceID						spaceID;
	ObjectID					vehicleID;
	Vector3						pos;
	float						auxVolatile[3];
	std::vector<MemoryOStream*>	properties;
};

typedef std::map<ObjectID,SavedEntityState> SavedEntityStates;
static SavedEntityStates savedStates_;

static double s_savedStatesAt_ = 0.0;
static double s_restoredStatesAt_ = 0.0;



BinaryFile & operator<<( BinaryFile & bf, const SavedEntityState & ses )
{
	bf << ses.enterCount;
	bf << ses.type;
	bf << ses.spaceID;
	bf << ses.vehicleID;
	bf << ses.pos;
	bf << ses.auxVolatile[0] << ses.auxVolatile[1] << ses.auxVolatile[2];

	int num = EntityType::find( ses.type )->description().clientServerPropertyCount();
	for (int i = 0; i < num; i++)
	{
		ses.properties[i]->rewind();
		int sz = ses.properties[i]->size();
		bf << sz;
		bf.write( ses.properties[i]->retrieve( sz ), sz );
	}

	return bf;
}

const BinaryFile & operator>>( const BinaryFile & bf, SavedEntityState & ses )
{
	bf >> ses.enterCount;
	bf >> ses.type;
	bf >> ses.spaceID;
	bf >> ses.vehicleID;
	bf >> ses.pos;
	bf >> ses.auxVolatile[0] >> ses.auxVolatile[1] >> ses.auxVolatile[2];

	ses.properties.clear();

	int num = EntityType::find( ses.type )->description().clientServerPropertyCount();
	for (int i = 0; i < num; i++)
	{
		int sz;
		bf >> sz;
		MemoryOStream * pMos = new MemoryOStream( sz );
		bf.read( pMos->reserve( sz ), sz );
		ses.properties.push_back( pMos );
	}

	return bf;
}



static class EntityManagerEmptyAnnal : public AnnalBase
{
private:
	void clear()
	{
		EntityManager::instance().record();
	}

	void setup()
	{
		EntityManager::instance().playback();
	}

	void save( BinaryFile & bf ) const
	{
		bf << s_savedStatesAt_;
		bf.writeMap( savedStates_ );
	}

	void load( const BinaryFile & bf )
	{
		// clear it...
		for (SavedEntityStates::iterator it = savedStates_.begin();
			it != savedStates_.end();
			it++)
		{
			SavedEntityState	& ses = it->second;

			for (uint i = 0; i < ses.properties.size(); i++)
			{
				delete ses.properties[i];
			}
		}
		savedStates_.clear();

		// now load
		bf >> s_savedStatesAt_;
		bf.readMap( savedStates_ );
	}
} s_emea;

/**
 *	This method informs the entity manager that recording is
 *	about to start. It saves the states of all the entities.
 *	After careful consideration, I've decided that the best
 *	thing to save is the server properties, not the dictionary
 *	or anything else, and to call the set_ methods to set them
 *	up again in playback. Also, since we never delete an entity,
 *	all we have to do is save whether or not they're entered,
 *	and make it so when we start playback.
 *	'avatarUpdate', 'updateEntity', 'createEntity' and 'entityMessage'
 *	server messages are then saved as they are received.
 */
void EntityManager::record()
{
	// clear any existing recorded info
	for (SavedEntityStates::iterator it = savedStates_.begin();
		it != savedStates_.end();
		it++)
	{
		SavedEntityState	& ses = it->second;

		for (uint i = 0; i < ses.properties.size(); i++)
		{
			delete ses.properties[i];
		}
	}
	savedStates_.clear();

	// and gather it all up
	for (Entities::iterator it = enteredEntities_.begin();
		it != cachedEntities_.end();
		it = ((++it) == enteredEntities_.end() ? cachedEntities_.begin() : it))
	{
		SavedEntityState	ses;
		ses.enterCount = it->second->enterCount();

		ses.type = it->second->type().index();
		ses.spaceID = it->second->pSpace() ? it->second->pSpace()->id() : 0;
		ses.pos = it->second->position();
		ses.auxVolatile[0] = it->second->auxVolatile()[0];
		ses.auxVolatile[1] = it->second->auxVolatile()[1];
		ses.auxVolatile[2] = it->second->auxVolatile()[2];

		const EntityDescription & desc = it->second->type().description();

		for (uint i = 0; i < desc.clientServerPropertyCount(); i++)
		{
			const char * propName = desc.clientServerProperty( i )->name().c_str();

			PyObject * pObject = PyObject_GetAttrString(	// new ref
				it->second, const_cast<char*>( propName ) );

			MemoryOStream * pMos = new MemoryOStream( 4096 );
			desc.clientServerProperty( i )->addToStream( pObject, *pMos, false );

			Py_XDECREF( pObject );

			ses.properties.push_back( pMos );

			PyErr_Clear();	// just in case...
		}

		savedStates_[ it->first ] = ses;
	}

	// and remember this time (for offsetting server messages against...)
	s_savedStatesAt_ = getGameTotalTime();
}


/**
 *	Helper struct for playing back AoI entrances
 */
struct TEWInfo
{
	TEWInfo( ObjectID id, SpaceID spaceID, ObjectID vehicleID ) :
		id_( id ), spaceID_( spaceID ), vehicleID_( vehicleID ) { }
	ObjectID id_;
	SpaceID spaceID_;
	ObjectID vehicleID_;
};

/**
 *	This method informs the entity manager that playback is
 *	about to start. It restores the state saved by a previous
 *	'record' call.
 *
 *	To consider: Call these in the 'clear' and 'setup' methods of
 *	an annal.
 *
 *	@see record
 */
void EntityManager::playback()
{
	std::vector<ObjectID>	toLeaveWorld;
	std::vector< TEWInfo >	toEnterWorld;
	std::vector<ObjectID>	toRemove;

	// set everyone back to whatever state they were in before
	//  don't worry about saving their current state ... for now.

	for (Entities::iterator it = enteredEntities_.begin();
		it != cachedEntities_.end();
		it = ((++it) == enteredEntities_.end() ? cachedEntities_.begin() : it))
	{
		SavedEntityStates::iterator found = savedStates_.find( it->first );

		// if it's wasn't there at the start, get rid of it
		if (found == savedStates_.end())
		{
			for (int i = it->second->enterCount(); i > 0; i--)
			{
				toLeaveWorld.push_back( it->first );
			}

			toRemove.push_back( it->first );

			continue;
		}

		SavedEntityState & ses = found->second;

		// set its position and auxvolatile data
		it->second->filter().reset( getGameTotalTime() );
		it->second->filter().input( getGameTotalTime(),
			ses.spaceID, ses.vehicleID, ses.pos, ses.auxVolatile );
			// time does go up from now... (i.e. doesn't jump backwards)

		// set all its properties
		const EntityDescription & desc = it->second->type().description();

		for (uint i = 0; i < desc.clientServerPropertyCount(); i++)
		{
			MemoryOStream * pMos = ses.properties[i];
			pMos->rewind();

			PyObjectPtr pObject = desc.clientServerProperty( i )->createFromStream( *pMos, false );
			if (pObject)
			{
				it->second->setProperty(
					desc.clientServerProperty( i ), pObject );
			}
			// if it's null... then I dunno, just leave it alone I guess.
		}

		// and remember to put it where it belongs
		int cec = it->second->enterCount();
		while( cec > ses.enterCount )
		{
			toLeaveWorld.push_back( it->first );
			cec--;
		}
		while( cec < ses.enterCount )
		{
			toEnterWorld.push_back(
				TEWInfo( it->first, ses.spaceID, ses.vehicleID ) );
			cec++;
		}
	}

	// create any entities that are no longer with us
	for (SavedEntityStates::iterator it = savedStates_.begin();
		it != savedStates_.end();
		it++)
	{
		if (this->getEntity( it->first, true ) != NULL) continue;

		SavedEntityState	& ses = it->second;

		MemoryOStream	bigStream( 8192 );
		for (uint i = 0; i < ses.properties.size(); i++)
		{
			MemoryOStream * pMos = ses.properties[i];
			pMos->rewind();
			int sz = pMos->size();
			pMos->transfer( bigStream, sz );
		}

		// enter it
		int cec = 0;
		while( cec < ses.enterCount )
		{
			this->onEntityEnter( it->first, ses.spaceID, ses.vehicleID );
			cec++;
		}

		// and call the createEntity message for it... (hmmm)
		this->onEntityCreate( it->first, it->second.type,
			ses.spaceID, ses.vehicleID, ses.pos,
			ses.auxVolatile[0], ses.auxVolatile[1], ses.auxVolatile[2],
			bigStream, /*oldVersion:*/false );
	}

	// now that doing this won't break our iterators, make the world right
	for (uint i = 0; i < toEnterWorld.size(); i++)
	{
		this->onEntityEnter( toEnterWorld[i].id_,
			toEnterWorld[i].spaceID_, toEnterWorld[i].vehicleID_ );
	}

	for (uint i = 0; i < toLeaveWorld.size(); i++)
	{
		this->onEntityLeave( toLeaveWorld[i] );
	}

	// remove any that shouldn't be there
	for (uint i = 0; i < toRemove.size(); i++)
	{
		Entities::iterator found = cachedEntities_.find( toRemove[i] );

		MF_ASSERT( found != cachedEntities_.end() );

		// this will probably break a whole lot of things... but
		//  it'll eventually have to be possible
		delete found->second;
		cachedEntities_.erase( found );
	}


	// TODO: Call a method on the player to let it know playback
	//  has started. This is so it can update its inventory index, etc.

	// and remember this time (for offsetting server messages against...)
	s_restoredStatesAt_ = getGameTotalTime();

	// and we're done! woohoo!
}


/**
 *	This class interlopes between ServerConnection and EntityManager
 *	when recording, storing all the messages which it passes on.
 *	They can be later played back with the 'regurgitate' method.
 */
static class SMHInterloper : public ServerMessageHandler
{
public:

	/**
	 *	Regurgitate any messages that should have been seen by now.
	 *	Note that we don't let the EntityManager have the message
	 *	until the time it would have got it when playing the game.
	 *	It could be good to let it have the message at the exact
	 *	time it was supposed to occur - i.e. remove the network
	 *	delay! This way is probably better for repeatability
	 *	however.
	 */
	void regurgitate()
	{
		SavedMessage	saved;

		while (annal_.pop( saved ))
		{
			saved.dispatch();
		}
	}

private:

	virtual void onBasePlayerCreate( ObjectID id, EntityTypeID type,
		BinaryIStream & data,
		bool /*oldVersion*/ )
	{
		// we don't record this one for now
	}

	virtual void onCellPlayerCreate( ObjectID id,
		SpaceID spaceID, ObjectID vehicleID, const Position3D & position,
		float yaw, float pitch, float roll, BinaryIStream & data,
		bool /*oldVersion*/ )
	{
		// we don't record this one for now
	}

	virtual void onEntityCreate( ObjectID id, EntityTypeID type,
		SpaceID spaceID, ObjectID vehicleID, const Position3D & position,
		float yaw, float pitch, float roll, BinaryIStream & data,
		bool /*oldVersion*/ )
	{
		int length = data.remainingLength();
		SavedMessage	sam;
		sam.which = SavedMessage::ENTITY_CREATE;
		sam.id = id;
		sam.type = type;
		sam.pData = new MemoryOStream( length );
		sam.spaceID = spaceID;
		sam.vehicleID = vehicleID;
		sam.pos = position;
		sam.yaw = yaw;
		sam.pitch = pitch;
		sam.roll = roll;
		sam.pData->transfer( data, length );

		sam.dispatch();

		annal_.push( sam );
	}

	virtual void onEntityProperties( ObjectID id, /*uint32 type,*/
		BinaryIStream & data, bool oldVersion )
	{
		SavedMessage	sam;
		sam.which = SavedMessage::ENTITY_PROPERTIES;
		sam.id = id;
//		sam.type = type;
		int length = data.remainingLength();
		sam.pData = new MemoryOStream( length );
		sam.pData->transfer( data, length );


		sam.dispatch();

		annal_.push( sam );
	}

	virtual void onEntityEnter( ObjectID id,
		SpaceID spaceID, ObjectID vehicleID )
	{
		SavedMessage	sam;
		sam.which = SavedMessage::ENTITY_ENTER;
		sam.id = id;
		sam.spaceID = spaceID;
		sam.vehicleID = vehicleID;

		sam.dispatch();

		annal_.push( sam );
	}

	virtual void onEntityLeave( ObjectID id,
		const CacheStamps & stamps )
//		EventNumber * pEventNumbers, int size )
	{
		SavedMessage	sam;
		sam.which = SavedMessage::ENTITY_LEAVE;
		sam.id = id;

		sam.dispatch();

		annal_.push( sam );
	}

	virtual void onEntityProperty( ObjectID id, int messageID,
		BinaryIStream & data, bool oldVersion )
	{
		int length = data.remainingLength();
		SavedMessage	sam;
		sam.which = SavedMessage::ENTITY_PROPERTY;
		sam.id = id;
		sam.mid = messageID;
		sam.pData = new MemoryOStream( length );
		sam.pData->transfer( data, length );

		sam.dispatch();

		annal_.push( sam );
	}

	virtual void onEntityMethod( ObjectID id, int messageID,
		BinaryIStream & data, bool oldVersion )
	{
		int length = data.remainingLength();
		SavedMessage	sam;
		sam.which = SavedMessage::ENTITY_METHOD;
		sam.id = id;
		sam.mid = messageID;
		sam.pData = new MemoryOStream( length );
		sam.pData->transfer( data, length );

		sam.dispatch();

		annal_.push( sam );
	}

	virtual void onEntityMove( ObjectID id, SpaceID spaceID,
		ObjectID vehicleID, const Position3D & pos,
		float yaw, float pitch, float roll, bool isVolatile )
	{
		SavedMessage	sam;
		sam.which = SavedMessage::ENTITY_MOVE;
		sam.id = id;
		sam.spaceID = spaceID;
		sam.vehicleID = vehicleID;
		sam.pos = pos;
		sam.yaw = yaw;
		sam.pitch = pitch;
		sam.roll = roll;
		sam.isVolatile = isVolatile;

		sam.dispatch();

		annal_.push( sam );
	}

	virtual void spaceData( SpaceID spaceID, SpaceEntryID entryID,
		uint16 key, const std::string & data )
	{
		ERROR_MSG( "TODO: Implement spaceData in SMH interloper\n" );
		EntityManager::instance().spaceData(
			spaceID, entryID, key, data );
	}

	virtual void spaceGone( SpaceID spaceID )
	{
		ERROR_MSG( "TODO: Implement spaceGone in SMH interloper\n" );
		EntityManager::instance().spaceGone( spaceID );
	}


public:
	struct SavedMessage
	{
		SavedMessage() :
			which( BAD_SAVED_MESSAGE ),
			time( EntityManager::instance().lastMessageTime() - s_savedStatesAt_ ),
			pData( NULL )
		{
		}

		SavedMessage( const SavedMessage & other ) :
			pData( NULL )
		{
			// we use operator= to save writing that code twice.
			//  operator= expects pData to be set up.
			*this = other;
		}

		SavedMessage & operator=( const SavedMessage & other )
		{
			if (&other != this)
			{
				if (pData != NULL) delete pData;

				memcpy( (void*)this, (void*)&other, sizeof( other ) );

				if (other.pData != NULL)
				{
					other.pData->rewind();
					int len = other.pData->size();
					pData = new MemoryOStream( len );
					pData->transfer( *other.pData, len );
				}
			}

			return *this;
		}

		~SavedMessage()
		{
			if (pData != NULL) delete pData;
		}

		enum
		{
			BAD_SAVED_MESSAGE = -1,
			ENTITY_ENTER,
			ENTITY_LEAVE,
			ENTITY_CREATE,
			ENTITY_PROPERTIES,
			ENTITY_PROPERTY,
			ENTITY_METHOD,
			ENTITY_MOVE
		}				which;

		double			time;

		ObjectID		id;
		EntityTypeID	type;
		MemoryOStream	*pData;
		int				mid;
		SpaceID			spaceID;
		ObjectID		vehicleID;
		Position3D		pos;
		float			yaw;
		float			pitch;
		float			roll;
		bool			isVolatile;


		void dispatch()
		{
			EntityManager & em = EntityManager::instance();

			// EntityManager will ignore this value if we're recording
			//  instead of playing back (i.e. when s_restoredStatesAt_
			//  would be meaningless ... hmmmm... or we could set
			//  s_restoredStatesAt_ to be the same as s_savedStatesAt_...)
			em.lastMessageTime( time + s_restoredStatesAt_ );

			if (pData != NULL) pData->rewind();

			switch (which)
			{
			case ENTITY_ENTER:
				em.onEntityEnter( id, spaceID, vehicleID );
				break;
			case ENTITY_LEAVE:
				em.onEntityLeave( id );
				break;

			case ENTITY_CREATE:
				em.onEntityCreate( id, type, spaceID, vehicleID,
					pos, yaw, pitch, roll, *pData, false );
				break;
			case ENTITY_PROPERTIES:
				em.onEntityProperties( id, *pData, false );
				break;

			case ENTITY_PROPERTY:
				em.onEntityProperty( id, mid, *pData, false );
				break;
			case ENTITY_METHOD:
				em.onEntityMethod( id, mid, *pData, false );
				break;
			case ENTITY_MOVE:
				em.onEntityMove( id, spaceID, vehicleID,
					pos, yaw, pitch, roll, isVolatile );
				break;
			default:
				ERROR_MSG( "SavedMessage::dispatch: "
					"Bad saved message! (which = %d)\n", which );
				break;
			}
		}

		void save( BinaryFile & bf ) const
		{
			bf << which << time << id << type << spaceID << vehicleID;

			if (pData != NULL)
			{
				pData->rewind();

				int32 sz = pData->size();
				bf << sz;
				bf.write( pData->retrieve( sz ), sz );
			}
			else
			{
				bf << int32(-1);
			}

			bf << mid << pos << yaw << pitch << roll;
		}

		void load( const BinaryFile & bf )
		{
			if (pData != NULL) delete pData;

			bf >> which >> time >> id >> type >> spaceID >> vehicleID;

			int32 sz;
			bf >> sz;
			if (sz != -1)
			{
				pData = new MemoryOStream( sz );
				bf.read( pData->reserve( sz ), sz );
			}
			else
			{
				pData = NULL;
			}

			bf >> mid >> pos >> yaw >> pitch >> roll;
		}
	};

	//friend BinaryFile & operator<<( BinaryFile & bf, const SMHInterloper::SavedMessage & m );
	//friend const BinaryFile & operator>>( const BinaryFile & bf, SMHInterloper::SavedMessage & m );
private:

	AnnalAbundant<SavedMessage>	annal_;
} s_smhInterloper;


BinaryFile & operator<<( BinaryFile & bf, const SMHInterloper::SavedMessage & m )
	{ m.save( bf ); return bf; }

const BinaryFile & operator>>( const BinaryFile & bf, SMHInterloper::SavedMessage & m )
	{ m.load( bf ); return bf; }



/**
 *	This method gathers input from the stored ServerConnection,
 *	if we are online and entities have been successfully enabled.
 */
void EntityManager::gatherInput()
{
	if (AnnalVault::isRecording())
	{
		if (pServer_ != NULL)
		{
			pServer_->setMessageHandler( &s_smhInterloper );
			pServer_->processInput();
			pServer_->setMessageHandler( this );
		}
	}
	else if (AnnalVault::isPlaying())
	{
		s_smhInterloper.regurgitate();
	}
	else	// stopped
	{
		if (pServer_ != NULL)
		{
			pServer_->processInput();
		}
	}
}

namespace // annonymous 
{

void clearEntityDict(Entity * entity)
{
	PyObject * entityDict = PyObject_GetAttrString( entity, "__dict__" );
	if (entityDict != NULL)
	{
		PyDict_Clear(entityDict);
		Py_DECREF(entityDict);
	}
}


void clearEntitiesDict(const std::vector< EntityPtr > &entities)
{
	std::vector< EntityPtr >::const_iterator it  = entities.begin();
	std::vector< EntityPtr >::const_iterator end = entities.end();
	while (it != end)
	{
		clearEntityDict(it->getObject());
		++it;
	}
}

} // namespace annonymous 

// entity_manager.cpp
