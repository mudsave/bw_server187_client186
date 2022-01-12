/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef ENTITY_MANAGER_HPP
#define ENTITY_MANAGER_HPP

#pragma warning (disable: 4786)

#include <string>
#include <vector>
#include <map>
#include <set>

#include "network/basictypes.hpp"
#include "common/servconn.hpp"
#include "entity.hpp"


class MemoryOStream;
class ServerConnection;
class SubSpace;
class ChunkSpace;

typedef std::map<ObjectID,Entity*>	Entities;
typedef SmartPointer<ChunkSpace> ChunkSpacePtr;



/**
 *	This class stores all the entities that exist on the client,
 *	and controls which ones should be displayed in the world.
 */
class EntityManager : public ServerMessageHandler
{
public:
	EntityManager();
	~EntityManager();

	static EntityManager & instance();

	void connected( ServerConnection & server );
	ServerConnection * pServer()			{ return pServer_; }
	void disconnected();
	void fini();

	virtual void onBasePlayerCreate( ObjectID id, EntityTypeID type,
		BinaryIStream & data, bool oldVersion );

	virtual void onCellPlayerCreate( ObjectID id,
		SpaceID spaceID, ObjectID vehicleID, const Position3D & position,
		float yaw, float pitch, float roll, BinaryIStream & data,
		bool oldVersion );

	virtual void onEntityControl( ObjectID id, bool control );

	virtual void onEntityCreate( ObjectID id, EntityTypeID type,
		SpaceID spaceID, ObjectID vehicleID, const Position3D & position,
		float yaw, float pitch, float roll, BinaryIStream & data,
		bool oldVersion );

	virtual void onEntityProperties( ObjectID id,
		BinaryIStream & data, bool oldVersion );

	virtual void onEntityEnter( ObjectID id,
		SpaceID spaceID, ObjectID vehicleID );
	virtual void onEntityLeave( ObjectID id,
		const CacheStamps & stamps = CacheStamps() );

	virtual void onEntityProperty( ObjectID id, int messageID,
		BinaryIStream & data, bool oldVersion );
	virtual void onEntityMethod( ObjectID id, int messageID,
		BinaryIStream & data, bool oldVersion );

	virtual void onEntityMove( ObjectID id,
		SpaceID spaceID, ObjectID vehicleID, const Position3D & pos,
		float yaw, float pitch, float roll, bool isVolatile );

	virtual void onEntitiesReset( bool keepPlayerAround );
	void clearAllEntities( bool keepPlayerAround, bool keepClientOnly = false );

	virtual void spaceData( SpaceID spaceID, SpaceEntryID entryID,
		uint16 key, const std::string & data );

	virtual void spaceGone( SpaceID spaceID );

	virtual void onVoiceData( const Mercury::Address & srcAddr,
		BinaryIStream & data );

	virtual void onProxyData( uint16 proxyDataID, 
								BinaryIStream & data );

	virtual void onRestoreClient( ObjectID id,
		SpaceID spaceID, ObjectID vehicleID,
		const Position3D & pos, const Direction3D & dir,
		BinaryIStream & data, bool oldVersion );

	virtual void onImpendingVersion( ImpendingVersionProximity proximity );

	void tick( double timeNow, double timeLast );

	ObjectID	proxyEntityID()				{ return proxyEntityID_; }

	bool displayIDs() const					{ return displayIDs_; }
	void displayIDs( bool data ) 			{ displayIDs_ = data; }

	Entity * getEntity( ObjectID id, bool lookInCache = false );

	Entities& entities() 					{ return enteredEntities_; }
	const Entities & cachedEntities() const { return cachedEntities_; }

	static const EntityID FIRST_CLIENT_ID = (1L << 30) + 1;
	static EntityID nextClientID()				{ return nextClientID_++; }
	static bool isClientOnlyID( EntityID id )	{ return id >= FIRST_CLIENT_ID; }

	double lastMessageTime() const;
	void lastMessageTime( double t );

	void record();
	void playback();

	void gatherInput();

private:
	bool forwardQueuedMessages( Entity * pEntity );
	void clearQueuedMessages( ObjectID id );
	void setTimeInt( ChunkSpacePtr pSpace, TimeStamp gameTime,
		float initialTimeOfDay, float gameSecondsPerSecond );


	ServerConnection	* pServer_;
	ObjectID	proxyEntityID_;

	Entities	enteredEntities_;
	Entities	cachedEntities_;

	class UnknownEntity
	{
	public:
		UnknownEntity() :
			count( 0 ),
			time(-1000),
			spaceID(0),
			vehicleID(0),
			pos(0,0,0)
		{
			auxFilter[0] = 0;
			auxFilter[1] = 0;
			auxFilter[2] = 0;
		}

		int			count;

		double		time;
		SpaceID		spaceID;
		ObjectID	vehicleID;
		Position3D	pos;
		float		auxFilter[3];

		// something for the queue of messages
	};

	typedef std::map<ObjectID,UnknownEntity>	Unknowns;
	Unknowns	unknownEntities_;

	typedef std::vector< std::pair< uint8, MemoryOStream * > > MessageQueue;
	typedef std::map< ObjectID, MessageQueue > MessageMap;
	MessageMap	queuedMessages_;

	std::vector< EntityPtr >	prereqEntities_;
	std::vector< EntityPtr >	passengerEntities_;

	bool		displayIDs_;

	static ObjectID	nextClientID_;

	double		playbackLastMessageTime_;

	std::set< ChunkSpacePtr >	seenSpaces_;

public:
	static SubSpace		* defaultEntitySubSpace_;
};








#endif // ENTITY_MANAGER_HPP