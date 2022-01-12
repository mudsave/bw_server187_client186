/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CLIENT_APP_HPP
#define CLIENT_APP_HPP

#include "Python.h"

#include "common/servconn.hpp"
#include "pyscript/pyobject_plus.hpp"
#include "pyscript/script.hpp"
#include "entity.hpp"
#include "main_app.hpp"

class MovementController;

/**
 *	This class is used to handle messages from the server.
 */
class ClientApp : public PyObjectPlus,
	public ServerMessageHandler,
	public Mercury::InputNotificationHandler
{
	Py_Header( ClientApp, PyObjectPlus )

public:
	ClientApp( Mercury::Nub & mainNub, const char * tag,
				PyTypeObject * pType = &ClientApp::s_type_ );
	virtual ~ClientApp();

	// ---- Python related ----
	PyObject *	pyGetAttribute( const char * attr );
	int			pySetAttribute( const char * attr, PyObject * value );

	// ---- Overrides from InputNotificationHandler ----
	virtual int handleInputNotification( int );

	// ---- Overrides from ServerMessageHandler ----
	virtual void onBasePlayerCreate( ObjectID id, EntityTypeID type,
		BinaryIStream & data, bool oldVersion );

	virtual void onCellPlayerCreate( ObjectID id,
		SpaceID spaceID, ObjectID vehicleID, const Position3D & pos,
		float yaw, float pitch, float roll,
		BinaryIStream & data, bool oldVersion );

	virtual void onEntityEnter( ObjectID id, SpaceID spaceID, ObjectID );

	virtual void onEntityLeave( ObjectID id, const CacheStamps & stamps );

	virtual void onEntityCreate( ObjectID id, EntityTypeID type,
		SpaceID spaceID, ObjectID vehicleID, const Position3D & pos,
		float yaw, float pitch, float roll,
		BinaryIStream & data, bool oldVersion );

	virtual void onEntityProperties( ObjectID id, BinaryIStream & data,
			bool oldVersion );

	virtual void onEntityProperty( ObjectID entityID, int propertyID,
		BinaryIStream & data, bool oldVersion );

	virtual void onEntityMethod( ObjectID entityID, int methodID,
		BinaryIStream & data, bool oldVersion );

	virtual void onEntityMove(
		ObjectID entityID, SpaceID spaceID, ObjectID vehicleID,
		const Position3D & pos, float yaw, float pitch, float roll,
		bool isVolatile );

	virtual void setTime( TimeStamp gameTime,
			float initialTimeOfDay, float gameSecondsPerSecond );

	virtual void spaceData( SpaceID spaceID, SpaceEntryID entryID, uint16 key,
		const std::string & data );

	virtual void spaceGone( SpaceID spaceID );

	virtual void onProxyData( uint16 proxyDataID, BinaryIStream & data );

	virtual void onEntitiesReset( bool keepPlayerOnBase );

	// ---- To be overriden ----
	virtual void addMove( double time );

	// ---- General interface ----
	bool tick( float dTime );

	void logOff()					{ serverConnection_.disconnect(); }

	const std::string & tag() const			{ return tag_; }
	ObjectID id() const						{ return playerID_; }

	bool setMovementController( const std::string & type,
			const std::string & data );
	void moveTo( const Position3D &pos );
	void faceTowards( const Position3D &pos );
	void snapTo( const Position3D &pos ) { position_ = pos; }
	bool isMoving() const { return pDest_ != NULL; };
	void stop();

	int addTimer( float interval, PyObjectPtr callback, bool repeat );
	void delTimer( int id );

	void destroy();

	const ServerConnection * getServerConnection() const	{ return &serverConnection_; }


	typedef std::map<ObjectID, Entity *> EntityMap;
	const EntityMap & entities() const { return entities_; }

protected:
	ServerConnection serverConnection_;
	EntityMap entities_;
	EntityMap cachedEntities_;

	SpaceID			spaceID_;
	ObjectID		playerID_;
	ObjectID		vehicleID_;

	LoginHandlerPtr pLoginInProgress_;
	bool			isDestroyed_;

	Mercury::Nub &	mainNub_;
	bool			hasScripts_;
	std::string		tag_;
	float			speed_;
	Vector3			position_;
	Direction3D		direction_;

	MovementController * pMovementController_;
	bool			autoMove_;
	Vector3			*pDest_;

	// This stuff is to manage bot timers
	class TimerRec
	{
	public:
		TimerRec( float interval, PyObjectPtr &pFunc, bool repeat ) :
			id_( ID_TICKER++ ), interval_( interval ), pFunc_( pFunc ),
			repeat_( repeat )
		{
			// Go back to 0 on overflow, since negative return values from
			// addTimer() indicate failure
			if (ID_TICKER < 0)
				ID_TICKER = 0;

			startTime_ = MainApp::instance().localTime();
		}

		bool operator< ( const TimerRec &other ) const
		{
			return finTime() >= other.finTime();
		}

		bool elapsed() const
		{
			return finTime() <= MainApp::instance().localTime();
		}

		int id() const { return id_; }
		float interval() const { return interval_; }
		float finTime() const { return startTime_ + interval_; }
		bool repeat() const { return repeat_; }
		PyObject* func() const { return pFunc_.getObject(); }
		void restart() { startTime_ = MainApp::instance().localTime(); }

	private:
		static int ID_TICKER;

		int id_;
		float interval_;
		float startTime_;
		PyObjectPtr pFunc_;
		bool repeat_;
	};

	std::priority_queue< TimerRec > timerRecs_;
	std::list< int > deletedTimerRecs_;
	void processTimers();

	PY_RO_ATTRIBUTE_DECLARE( playerID_, id );
	PY_RW_ATTRIBUTE_DECLARE( tag_, tag );
	PY_RW_ATTRIBUTE_DECLARE( speed_, speed );
	PY_RW_ATTRIBUTE_DECLARE( position_, position );
	PY_RW_ATTRIBUTE_DECLARE( direction_.yaw, yaw );
	PY_RW_ATTRIBUTE_DECLARE( direction_.pitch, pitch );
	PY_RW_ATTRIBUTE_DECLARE( direction_.roll, roll );
	PY_RW_ATTRIBUTE_DECLARE( autoMove_, autoMove );

	PY_AUTO_METHOD_DECLARE( RETVOID, logOff, END );

	PY_AUTO_METHOD_DECLARE( RETOK, setMovementController,
		ARG( std::string, ARG( std::string, END ) ) );
	PY_AUTO_METHOD_DECLARE( RETVOID, moveTo,
		ARG( Vector3, END ) );
	PY_AUTO_METHOD_DECLARE( RETVOID, faceTowards,
		ARG( Vector3, END ) );
	PY_AUTO_METHOD_DECLARE( RETVOID, snapTo,
		ARG( Vector3, END ) );
	PY_AUTO_METHOD_DECLARE( RETDATA, isMoving, END );
	PY_AUTO_METHOD_DECLARE( RETVOID, stop, END );

	PY_AUTO_METHOD_DECLARE( RETDATA, addTimer,
		ARG( float, ARG( PyObjectPtr, ARG( bool, END ) ) ) );
	PY_AUTO_METHOD_DECLARE( RETVOID, delTimer,
		ARG( int, END ) );

	PyObject * pEntities_;
	PY_RO_ATTRIBUTE_DECLARE( pEntities_, entities );
};

#endif // CLIENT_APP_HPP
