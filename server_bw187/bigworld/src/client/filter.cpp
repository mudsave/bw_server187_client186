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

#include "filter.hpp"

#include "entity_type.hpp"
#include "entity.hpp"
#include "entity_manager.hpp"

#include "duplo/chunk_attachment.hpp"
#include "cstdmf/debug.hpp"

#include "world.hpp"
#include "moo/terrain_block.hpp"
#include "cstdmf/dogwatch.hpp"
#include "chunk/chunk_space.hpp"
#include "chunk/chunk_obstacle.hpp"
#include "chunk/chunk_manager.hpp"


bool Filter::isActive_ = true;

DECLARE_DEBUG_COMPONENT2( "Entity", 0 )

extern double getGameTotalTime();



// -----------------------------------------------------------------------------
// Section: Filter Implementation Declarations
// -----------------------------------------------------------------------------

/*~ class BigWorld.AvatarFilter
 *
 *	This class inherits from Filter.  It implements a filter which tracks
 *	the last 8 entity updates from the server, and linearly interpolates
 *	between them.
 *
 *	Linear interpolation is done at ( time - latency ), where time is the
 *	current engine time, and latency is how far in the past the entity
 *	currently is.
 *
 *	Latency moves from its current value in seconds to the "ideal latency"
 *	which is the time between the two most recent updates if an update
 *	has just arrived, otherwise it is 2 * minLatency.  Lantency moves at
 *	velLatency seconds per second.
 *
 *	An AvatarFilter is created using the BigWorld.AvatarFilter function.
 */
/**
 *	This is the standard avatar-type filter, which uses one auxFiltered
 *	element: yaw. It smooths the position using an actual filter :)
 */
class AvatarFilter : public Filter
{
	Py_Header( AvatarFilter, Filter )

public:
	AvatarFilter( PyTypePlus * pType = &s_type_ );
	~AvatarFilter();

	void reset( double time );

	void input( double time, SpaceID spaceID, ObjectID vehicleID,
		const Position3D & pos, float * auxFiltered );
	void output( double time );
	bool getLastInput( double & time, SpaceID & spaceID, ObjectID & vehicleID,
		Position3D & pos, float * auxFiltered );

	void callback( int whence, SmartPointer<PyObject> fn,
		float extra = 0.f, bool passMissedBy = false );

	PyObject * pyGetAttribute( const char * attr );
	int pySetAttribute( const char * attr, PyObject * value );

	PY_FACTORY_DECLARE()

	PY_RW_ATTRIBUTE_DECLARE( latency_, latency )

	PY_RW_ATTRIBUTE_DECLARE( s_latencyVelocity_, velLatency )
	PY_RW_ATTRIBUTE_DECLARE( s_latencyMinimum_, minLatency )
	PY_RW_ATTRIBUTE_DECLARE( s_latencyFrames_, latencyFrames )
	PY_RW_ATTRIBUTE_DECLARE( s_latencyCurvePower_, latencyCurvePower )

	PY_AUTO_METHOD_DECLARE( RETVOID, callback,
		ARG( int,
		NZARG( SmartPointer<PyObject>,
		OPTARG( float, 0.f,
		OPTARG( bool, false, END ) ) ) ) )

protected:
	struct Frame
	{
		double		time;
		SpaceID		spaceID;
		ObjectID	vehicleID;
		Position3D	pos;
		Angle		yaw;
		Angle		pitch;
	};


	void extract( double tex, SpaceID & spaceID, ObjectID & vehicleID,
		Position3D & pos, Vector3 & iVelocity, float dir[2] ) const;

	static void decodePosition(	Vector3 & result, 
								const Vector3 & inputPosition );
	static void blendCodedPositions(	Vector3 & result, 
										const Vector3 & inputPosA, 
										const Vector3 & inputPosB, 
										float alpha );
	static void blendInputs(	Vector3 & resultPosition,
								Vector3 & resultDirection,
								const Frame & inputA,
								const Frame & inputB, 
								float alpha,
								SpaceID targetSpace,
								ObjectID targetVehicle );


	Frame		frames_[8];
	uint		topFrame_;

	float		latency_;
	float		idealLatency_;
	double		outputTime_;
	bool		gotNewInput_;
	bool		reset_;

	struct Callback
	{
		Callback( double time, SmartPointer<PyObject> fn, bool passMB ) :
			time_( time ), function_( fn ), passMissedBy_( passMB ) { }

		bool operator <( const Callback & b ) const
			{ return b.time_ < this->time_; }

		double					time_;
		SmartPointer<PyObject>	function_;
		bool					passMissedBy_;
	};
	struct CallbackPtr
	{
		CallbackPtr( Callback * cb ) : cb_( cb ) { }
		Callback & operator *()		{ return *cb_; }
		Callback * operator ->()	{ return cb_; }
		bool operator <( const CallbackPtr & b ) const
			{ return *b.cb_ < *this->cb_; }

		Callback * cb_;
	};

	std::priority_queue< CallbackPtr >	callbacks_;

public:
	/// Indicates whether or not we are trying to debug latency issues.
	static float s_latencyVelocity_;
	static float s_latencyMinimum_;
	static float s_latencyFrames_;
	static float s_latencyCurvePower_;

};

/*~ class BigWorld.AvatarDropFilter
 *
 *	This class inherits from AvatarFilter.  It is nearly exactly the same
 *	as its parent, except that after moving it performs a findDropPoint
 *	which places the entity on the ground surface below the position
 *	specified by the server.
 *
 *	An AvatarDropFilter is created using the BigWorld.AvatarDropFilter
 *	function.
 */
/**
 *	This is the a modified AvatarFilter. The only difference
 *	is that it always does a findDropPoint after moving.
 */
class AvatarDropFilter : public AvatarFilter
{
	Py_Header( AvatarDropFilter, AvatarFilter )

public:
	AvatarDropFilter( PyTypePlus * pType = &s_type_ );

	void input( double time, SpaceID spaceID, ObjectID vehicleID,
		const Position3D & pos, float * auxFiltered );
	void output( double time );

	PY_FACTORY_DECLARE()
};



/*~ class BigWorld.PlayerAvatarFilter
 *
 *	This class inherits from Filter.  It updates the position and
 *	yaw of the entity to the most recent update received from the
 *	server.
 *
 *	A new PlayerAvatarFilters can be created using the
 *	BigWorld.PlayerAvatarFilter() function.
 */
/**
 *	This is the avatar-type player filter, which uses one auxFiltered
 *	elements: yaw. It still teleports.
 */
class PlayerAvatarFilter : public Filter
{
	Py_Header( PlayerAvatarFilter, Filter )

public:
	PlayerAvatarFilter( PyTypePlus * pType = &s_type_ );

	void input( double time, SpaceID spaceID, ObjectID vehicleID,
		const Position3D & pos, float * auxFiltered );
	void output( double time );

	bool getLastInput( double & time, SpaceID & spaceID, ObjectID & vehicleID,
		Position3D & pos, float * auxFiltered );

	PY_FACTORY_DECLARE()

private:
	double		time_;
	SpaceID		spaceID_;
	ObjectID	vehicleID_;
	Position3D	pos_;	// position in local coordinates
	float		headYaw_;
	float		headPitch_;
	float		aux2_;
};


class ChunkAttachment;

/*~ class BigWorld.BoidsFilter
 *
 *	This subclass of AvatarFilter implements a filter which implements flocking
 *	behaviour for the models that make up an entity.  This would normally
 *	be used for an entity such as a flock of birds or a school of fish which
 *	has many models driven by one entity.
 *
 *	The interpolation of the Entity position is done using the same logic
 *	as its parent AvatarFilter.  However, it also updates the positions
 *	of individual models (which are known, according to flocking conventions,
 *	as boids) that are attached to the entity, using standard
 *	flocking rules.
 *
 *	When the flock lands, the boidsLanded method is called on the entity.
 *
 *	A new BoidsFilter is created using the BigWorld.BoidsFilter function.
 */
/**
 *	This class implements the filter used by boids.
 */
class BoidsFilter : public AvatarFilter
{
	Py_Header( BoidsFilter, AvatarFilter )

private:
	class BoidData
	{
	public:
		BoidData();

		void updateModel( const Vector3 & goal,
			ChunkEmbodiment * pModel,
			const BoidsFilter & filter,
			float dTime,
			int state);

		Vector3			pos_;
		Vector3			dir_;		// Current direction
		float			yaw_, pitch_, roll_, dYaw_;
		float			speed_;
	};

	typedef std::vector< BoidData >	Boids;

public:
	BoidsFilter( PyTypePlus * pType = &s_type_ );

	// Overrides
	virtual void output( double time );
	PyObject * pyGetAttribute( const char * attr );
	int pySetAttribute( const char * attr, PyObject * value );

	PY_FACTORY_DECLARE()
	PY_RW_ATTRIBUTE_DECLARE( INFLUENCE_RADIUS, influenceRadius )
	PY_RW_ATTRIBUTE_DECLARE( COLLISION_FRACTION, collisionFraction )
	PY_RW_ATTRIBUTE_DECLARE( GOAL_APPROACH_RADIUS, approachRadius )
	PY_RW_ATTRIBUTE_DECLARE( GOAL_STOP_RADIUS, stopRadius )
	PY_RW_ATTRIBUTE_DECLARE( NORMAL_SPEED, speed )

protected:
	inline const float getInfluenceRadiusSqr() const
	{
		return INFLUENCE_RADIUS * INFLUENCE_RADIUS;
	}
	inline const float getApproachRadiusSqr() const
	{
		return GOAL_APPROACH_RADIUS * GOAL_APPROACH_RADIUS;
	}
	inline const float getStopRadiusSqr() const
	{
		return GOAL_STOP_RADIUS * GOAL_STOP_RADIUS;
	}
	float INFLUENCE_RADIUS;
	float COLLISION_FRACTION;
	float INV_COLLISION_FRACTION;
	float NORMAL_SPEED;
	float ANGLE_TWEAK;
	float PITCH_TO_SPEED_RATIO;
	float GOAL_APPROACH_RADIUS;
	float GOAL_STOP_RADIUS;

private:
	Boids	boidData_;
	double	prevTime_;

	friend BoidData;
};


// -----------------------------------------------------------------------------
// Section: Filter
// -----------------------------------------------------------------------------


PY_TYPEOBJECT( Filter )

PY_BEGIN_METHODS( Filter )
	/*~ function Filter.reset
	 *
	 *	This function resets the current time for the filter to the specified
	 *	time.
	 *
	 *	Filters use the current time to interpolate forwards the last update of
	 *	the volatile data from the server.  Setting this time to a different
	 *	value will move all entities to the place the filter thinks they should
	 *	be at that specified time.
	 *
	 *	As a general rule this shouldn't need to be called.
	 *
	 *	@param	time	a Float the time to set the filter to.
	 */
	PY_METHOD( reset )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( Filter )
PY_END_ATTRIBUTES()

PY_SCRIPT_CONVERTERS( Filter )


/**
 *	Constructor
 */
Filter::Filter( PyTypePlus * pType ) :
	PyObjectPlus( pType ),
	entity_( NULL )
{
}


/**
 *	This method resets this filter to the input time.
 */
void Filter::reset( double time )
{
}


/**
 *	This method connects this filter to the given entity.
 *
 *	Note that no references are taken or else a circular reference
 *	would result - the filter is 'owned' by the given entity.
 */
void Filter::owner( Entity * pEntity )
{
	entity_ = pEntity;
}


/**
 *	Standard get attribute method.
 */
PyObject * Filter::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();

	return PyObjectPlus::pyGetAttribute( attr );
}

/**
 *	Standard set attribute method.
 */
int Filter::pySetAttribute( const char * attr, PyObject * value )
{
	PY_SETATTR_STD();

	return PyObjectPlus::pySetAttribute( attr, value );
}


/**
 *	Static helper method to get the current game time
 */
double Filter::getTimeNow()
{
	return getGameTotalTime();
}


// -----------------------------------------------------------------------------
// Section: Filter utilities
// -----------------------------------------------------------------------------


/**
 *	This is a collision callback that finds the closest
 *	solid triangle.
 */
class SolidCollisionCallback : public CollisionCallback
{
public:
	SolidCollisionCallback() : closestDist_(-1.0f) {}

	virtual int operator()( const ChunkObstacle & obstacle,
		const WorldTriangle & triangle, float dist )
	{
		// Look in both directions if triangle is not solid.

		if(triangle.flags() & TRIANGLE_NOCOLLIDE)
		{
			return 1 | 2;
		}

		// Otherwise just find the closest.
		if(closestDist_ == -1.0f || dist < closestDist_)
			closestDist_ = dist;

		return 1;
	}

	float distance() const { return closestDist_; }

private:
	float closestDist_;
};


/**
 *	This method does a 'findDropPoint' in the way that drop filters require.
 */
static bool filterDropPoint( ChunkSpace * pSpace, const Vector3 & fall,
	Vector3 & land, float maxDrop = 100.f )
{
	Vector3 fall2 = fall;
	fall2.y += 0.1f;
	SolidCollisionCallback scc;
	land = fall2;

	if (pSpace == NULL) return false;

	pSpace->collide( fall2, Vector3(fall2.x, fall2.y-maxDrop, fall2.z), scc );

	if (scc.distance() < 0.0f) return false;

	land.y -= scc.distance();
	return true;
}


/**
 *	This inline function returns true if the entity's current coordinate system
 *	matches the given one.
 */
static inline void coordinateSystemCheck( Entity * pEntity,
	SpaceID spaceID, ObjectID vehicleID )
{
	ChunkSpace * pSpace = &*pEntity->pSpace();
	if (pSpace != NULL)
	{
		if (spaceID != pSpace->id())
		{
			pEntity->leaveWorld( true );
			pEntity->enterWorld( spaceID, vehicleID, true );
			return;
		}
	}
	else
	{
		WARNING_MSG( "coordinateSystemCheck: entity %d is not in any space\n",
			pEntity->id() );
		return;
	}

	Entity * pVehicle = pEntity->pVehicle();
	if (pVehicle == NULL )
	{
		if (vehicleID != 0)
		{
			pEntity->pVehicle(
				EntityManager::instance().getEntity( vehicleID, true ) );
		}
	}
	else
	{
		if (vehicleID != pVehicle->id())
		{
			pEntity->pVehicle(
				EntityManager::instance().getEntity( vehicleID, true ) );
		}
	}
}


/**
 *	This function transforms the given pose to a common representation.
 */
static void transformIntoCommon( SpaceID s, ObjectID v,
	Vector3 & pos, Vector3 & dir )
{
	Entity * pVehicle = EntityManager::instance().getEntity( v );
	if (pVehicle != NULL)
	{
		pVehicle->transformVehicleToCommon( pos, dir );
	}
	else
	{
		ChunkSpace * pOur = &*ChunkManager::instance().space( s, false );
		if (pOur != NULL)
		{
			pOur->transformSpaceToCommon( pos, dir );
		}
	}
}

/**
 *	This function transforms the given pose to its local representation.
 */
static void transformFromCommon( SpaceID s, ObjectID v,
	Vector3 & pos, Vector3 & dir )
{
	Entity * pVehicle = EntityManager::instance().getEntity( v );
	if (pVehicle != NULL)
	{
		pVehicle->transformCommonToVehicle( pos, dir );
	}
	else
	{
		ChunkSpace * pOur = &*ChunkManager::instance().space( s, false );
		if (pOur != NULL)
		{
			pOur->transformCommonToSpace( pos, dir );
		}
	}
}



// -----------------------------------------------------------------------------
// Section: DumbFilter
// -----------------------------------------------------------------------------


PY_TYPEOBJECT( DumbFilter )

PY_BEGIN_METHODS( DumbFilter )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( DumbFilter )
PY_END_ATTRIBUTES()

/*~ function BigWorld.DumbFilter
 *
 *	This function creates a new DumbFilter, which is the simplest filter,
 *	which just sets the position of the owning entity to the most recently received
 *	position from the server.
 *
 *	@return	A new DumbFilter object
 */
PY_FACTORY( DumbFilter, BigWorld )


/**
 *	This constructor sets up a DumbFilter for the input entity.
 */
DumbFilter::DumbFilter( PyTypePlus * pType ) :
	Filter( pType ),
	time_( -1000.0 ),
	spaceID_( 0 ),
	vehicleID_( 0 ),
	pos_( 0, 0, 0 ),
	yaw_( 0.f )
{

}


/**
 *	Reset this filter
 */
void DumbFilter::reset( double time )
{
	time_ = -1000.0;
}


/**
 *	This method adds a filter position to this filter.
 */
void DumbFilter::input( double time, SpaceID spaceID, ObjectID vehicleID,
	const Position3D & pos, float * auxFiltered )
{
	if ( time > time_ )
	{
		time_ = time;

		spaceID_ = spaceID;
		vehicleID_ = vehicleID;
		pos_ = pos;

		if (auxFiltered != NULL)
		{
			yaw_ = auxFiltered[0];
		}
	}
}


/**
 *	This method outputs the filter position at the given time
 */
void DumbFilter::output( double time )
{
	static DogWatch dwDumbFilterOutput("DumbFilter");
	dwDumbFilterOutput.start();

	coordinateSystemCheck( entity_, spaceID_, vehicleID_ );

	float yaw = yaw_;

	// make sure it's above the ground if it's a bot
	if (pos_[1] < -12000.f)
	{
		pos_[1] = (entity_->position()[1] < -12000.f) ?
			90.f :
			entity_->position()[1] + 1.f;

		Vector3 rpos_;
		if (filterDropPoint( &*entity_->pSpace(), Vector3( pos_ ), rpos_ ))
		{
			pos_[1] = rpos_[1];
		}
		else
		{
			float terrainHeight =
				Moo::TerrainBlock::getHeight( pos_[0], pos_[2] );

			if (terrainHeight != Moo::TerrainBlock::NO_TERRAIN)
			{
				pos_[1] = terrainHeight;
			}
			else
			{
				WARNING_MSG( "DumbFilter::output: Could not get terrain\n" );
			}
		}
	}

	entity_->pos( pos_, &yaw, 1 );

	dwDumbFilterOutput.stop();
}


/**
 *	This method gets the last input.
 *	Its arguments are left untouched if it has no last input.
 */
bool DumbFilter::getLastInput( double & time, SpaceID & spaceID,
	ObjectID & vehicleID, Position3D & pos, float * auxFiltered )
{
	if (time_ != -1000.0)
	{
		time = time_;
		spaceID = spaceID_;
		vehicleID = vehicleID_;
		pos = pos_;
		if (auxFiltered != NULL)
		{
			auxFiltered[0] = yaw_;
			auxFiltered[1] = 0.f;
			auxFiltered[2] = 0.f;
		}
		return true;
	}
	return false;
}


/**
 *	Python factory method
 */
PyObject * DumbFilter::pyNew( PyObject * args )
{
	if (PyTuple_Size( args ) != 0)
	{
		PyErr_SetString( PyExc_TypeError,
			"BigWorld.DumbFilter() expects no arguments" );
		return NULL;
	}

	return new DumbFilter();
}


// -----------------------------------------------------------------------------
// Section: AvatarFilter
// -----------------------------------------------------------------------------

PY_TYPEOBJECT( AvatarFilter )

PY_BEGIN_METHODS( AvatarFilter )
	PY_METHOD( callback )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( AvatarFilter )
	/*~ attribute AvatarFilter.latency
	 *
	 *	This attribute is the current latency applied to the entity.  That is,
	 *	how far in the past that entity is, relative to the current game time.
	 *
	 *	Latency always moves towards the "ideal" latency with a velocity of
	 *	velLatency, measured in seconds per second.
	 *
	 *	If an update has just come in from the server, then the ideal latency is
	 *	the time between the two most recent updates, otherwise it is
	 *	two times minLatency.
	 *
	 *	The position and yaw of the entity are linearly interpolated between
	 *	the positions and yaws in the last 8 updates using (time - latency).
	 *
	 *	@type float
	 */
	PY_ATTRIBUTE( latency )
	/*~ attribute AvatarFilter.velLatency
	 *
	 *	This attribute is the speed, in seconds per second, that the latency
	 *	attribute can change at, as it moves towards its ideal latency.
	 *	If an update has just come in from the server, then the ideal latency is
	 *	the time between the two most recent updates, otherwise it is
	 *	two times minLatency.
	 *
	 *	@type float
	 */
	PY_ATTRIBUTE( velLatency )
	/*~ attribute AvatarFilter.minLatency
	 *
	 *	This attribute is the minimum bound for latency.
	 *
	 *	@type	float
	 */
	PY_ATTRIBUTE( minLatency )
	/*~ attribute AvatarFilter.latencyFrames
	 *
	 *	
	 *
	 *	@type	float
	 */
	PY_ATTRIBUTE( latencyFrames )
	/*~ attribute AvatarFilter.latencyCurvePower
	 *
	 *	The power used to scale the latency velocity |latency - idealLatency|^power
	 *
	 *	@type	float
	 */
	PY_ATTRIBUTE( latencyCurvePower )



PY_END_ATTRIBUTES()

/*~ function BigWorld.AvatarFilter
 *
 *	This function creates a new AvatarFilter, which is used to
 *	interpolate between the position and yaw updates from the server
 *	for its owning entity.
 *
 *	@return a new AvatarFilter
 */
PY_FACTORY( AvatarFilter, BigWorld )


float AvatarFilter::s_latencyVelocity_ = 1.00f;			// In seconds per second (note: scaled by (latency-desiredLatency)^2 before application)
float AvatarFilter::s_latencyMinimum_ = 0.10f;			// The minimum ideal latency in seconds
float AvatarFilter::s_latencyFrames_ = 2.0f;			// In input packets
float AvatarFilter::s_latencyCurvePower_ = 2.0f;	// The power used to scale the latency velocity |latency - idealLatency|^power


// TODO:PJ This is another dirty hack to initialise the Watcher.
class CAvatarFilterWatcherIniter
{
public:
	CAvatarFilterWatcherIniter()
	{
		MF_WATCH( "Comms/Latency Velocity",	AvatarFilter::s_latencyVelocity_, Watcher::WT_READ_WRITE, "The speed at which the latency changes in seconds per second" );
		MF_WATCH( "Comms/Minimum Latency",	AvatarFilter::s_latencyMinimum_, Watcher::WT_READ_WRITE, "The minimum ideal latency in seconds" );
		MF_WATCH( "Comms/Ideal Latency Frames", AvatarFilter::s_latencyFrames_, Watcher::WT_READ_WRITE, "The ideal latency in update frequency (0.0-4.0)" );
		MF_WATCH( "Comms/Latency Speed Curve Power_", AvatarFilter::s_latencyCurvePower_, Watcher::WT_READ_WRITE, "The power used to scale the latency velocity |latency - idealLatency|^power" );
	}
};

static CAvatarFilterWatcherIniter s_avf_initer;


/**
 *	Constructor
 */
AvatarFilter::AvatarFilter( PyTypePlus * pType ) :
	Filter( pType ),
	latency_( 0 ),
	idealLatency_( 0 ),
	outputTime_( 0 ),
	gotNewInput_( false ),
	reset_( true )
{
	for (int i=0; i<8; i++)
	{
		frames_[i].time = -2000 + i;
		frames_[i].pos = Position3D( 0.f, 0.f, 0.f );
		frames_[i].yaw = 0.f;
		frames_[i].pitch = 0.f;
	}

	topFrame_ = 7;
}

/**
 *	Destructor
 */
AvatarFilter::~AvatarFilter()
{
	while (!callbacks_.empty())
	{
		delete &*callbacks_.top();
		callbacks_.pop();
	}
}


/**
 *	Reset method
 */
void AvatarFilter::reset( double time )
{
	reset_ = true;
}

/**
 *	Input method
 */
void AvatarFilter::input( double time, SpaceID spaceID, ObjectID vehicleID,
	const Position3D & pos, float * auxFiltered )
{
	if ( reset_ )
	{
		frames_[7].time = time;
		frames_[7].spaceID = spaceID;
		frames_[7].vehicleID = vehicleID;
		frames_[7].pos = pos;
		frames_[7].yaw = (auxFiltered == NULL) ? 0 : auxFiltered[0];
		frames_[7].pitch = (auxFiltered == NULL) ? 0 : auxFiltered[1];
		
		for( uint i=7; i>0; i-- )
		{
			frames_[i-1] = frames_[i];
			frames_[i-1].time -= 1.0;
		}

		this->latency_ = 7.5f;
		
		topFrame_ = 7;
		gotNewInput_ = true;
		reset_ = false;
	}
	else
	{
		if ( time >= frames_[topFrame_].time )
		{
			if ( time != frames_[topFrame_].time )
			{
				// Prevent overrunning the back of the input history
				if( this->outputTime_ - this->latency_ < frames_[(topFrame_+2)&0x7].time )
				{
					frames_[(topFrame_+3)&0x7] = frames_[(topFrame_+2)&0x7];
					frames_[(topFrame_+2)&0x7] = frames_[(topFrame_+1)&0x7];
				}
				
				topFrame_ = (topFrame_+1) & 0x7;
			}

			const Vector3 & oldPos = frames_[topFrame_].pos;

			Frame & topF = frames_[topFrame_];
			topF.time = time;
			topF.spaceID = spaceID;
			topF.vehicleID = vehicleID;
			topF.pos = (pos.x <= -12000.f) ? oldPos : pos;
			topF.yaw = (auxFiltered == NULL) ? 0 : auxFiltered[0];
			topF.pitch = (auxFiltered == NULL) ? 0 : auxFiltered[1];

			gotNewInput_ = true;
		}
		else
		{
			//WARNING_MSG( "Received avatarUpdate at %g before %g\n",
			//	time, frames_[topFrame_].time );
		}
	}
}


/**
 *	Output method
 */
void AvatarFilter::output( double time )
{
	static DogWatch dwAvatarFilterOutput("AvatarFilter");
	dwAvatarFilterOutput.start();

	// adjust ideal latency if we got something new
	if (gotNewInput_)
	{
		gotNewInput_ = false;

		// ideal latency is this

		const double newestTime = frames_[topFrame_].time;
		const double olderTime = frames_[(topFrame_+4)&0x7].time;

		AvatarFilter::s_latencyFrames_ = std::max( 0.0f, std::min( AvatarFilter::s_latencyFrames_, 4.0f ) );

		double ratio = ( 4.0 - AvatarFilter::s_latencyFrames_ ) / 4.0;

		idealLatency_ = static_cast<float>( time - ( ( newestTime * ratio ) + ( olderTime * ( 1 - ratio ) ) ) );

		idealLatency_ = std::max( idealLatency_, s_latencyMinimum_ );

	}

	// move our latency towards the ideal...
	float dTime = float(time - outputTime_);
	if (idealLatency_ > latency_)
	{
		latency_ += ( s_latencyVelocity_ * dTime ) * std::min( 1.0f, powf( fabsf( idealLatency_ - latency_ ), AvatarFilter::s_latencyCurvePower_ ) );
		latency_ = min( latency_, idealLatency_ );
	}
	else
	{
		latency_ -= ( s_latencyVelocity_ * dTime ) * std::min( 1.0f, powf( fabsf( idealLatency_ - latency_ ), AvatarFilter::s_latencyCurvePower_ ) );
		latency_ = max( latency_, idealLatency_ );
	}


	// record this so we can move latency at a velocity independent
	//  of the number of times we're called.
	outputTime_ = time;

	// find the position at 'time - latency'
	double tout = time - latency_;

	SpaceID		iSID;
	ObjectID	iVID;
	Position3D	iPos;
	Vector3		iVelocity;
	float		iDir[2];

	this->extract( tout, iSID, iVID, iPos, iVelocity, iDir );

	double botFrameTime = frames_[(topFrame_+1) & 0x7].time;
	if (tout < botFrameTime && entity_->isPoseVolatile())
	{
		latency_ = float(time - botFrameTime);
	}

	// make sure it's in the right coordinate system
	coordinateSystemCheck( entity_, iSID, iVID );

	// make sure it's above the ground if it's a bot
	if (iPos[1] < -12000.f)
	{
		iPos[1] = (entity_->position()[1] < -12000.f) ?
			90.f :
			entity_->position()[1] + 1.f;

		// if an avatar is in a shell i.e. not on the ground, server should
		// send over full position including y value. If it is on the ground
		// server will not send y position i.e. y = -13000, then make it on the ground
		// no need to drop! TODO: should review this hard coded default y value -13000.
		float terrainHeight = Moo::TerrainBlock::getHeight( iPos[0], iPos[2] );
		if (terrainHeight != Moo::TerrainBlock::NO_TERRAIN)
		{
			iPos[1] = terrainHeight;
		}
	}

	// and set the entity's position to this
	entity_->pos( iPos, (float*)&iDir, 2, iVelocity );

	dwAvatarFilterOutput.stop();

	// also call any timers that have gone off
	while (!callbacks_.empty() && tout >= callbacks_.top()->time_)
	{
		Callback * cb = &*callbacks_.top();
		callbacks_.pop();

		Py_XINCREF( &*cb->function_ );
		PyObject * res = Script::ask(
			&*cb->function_,
			cb->passMissedBy_ ?
				Py_BuildValue( "(f)", float( tout - cb->time_ ) ) :
				PyTuple_New(0),
			"AvatarFilter::output callback: " );
		// see if it wants to snap to the position at its time
		if (res != NULL)
		{
			if (PyInt_Check( res ) && PyInt_AsLong( res ) == 1)
			{
				this->extract( cb->time_, iSID, iVID, iPos, iVelocity, iDir );
				coordinateSystemCheck( entity_, iSID, iVID );
				entity_->pos( iPos, (float*)&iDir, 2 );
			}
			Py_DECREF( res );
		}

		delete cb;
	}
}



/**
 *	This method extracts the contents of the filter at a given point in time
 */
void AvatarFilter::extract( double time, SpaceID & iSID, ObjectID & iVID,
	Position3D & iPos, Vector3 & iVelocity, float iDir[2] ) const
{
	const size_t NUM_INPUTS = 8;

	// make an ordered table of the input history for improved readability
	// 0 is most recent
	// 7 is oldest
	const Frame * const inputs[] = {	&this->frames_[(topFrame_+0)&0x7], 
										&this->frames_[(topFrame_+7)&0x7], 
										&this->frames_[(topFrame_+6)&0x7], 
										&this->frames_[(topFrame_+5)&0x7], 
										&this->frames_[(topFrame_+4)&0x7], 
										&this->frames_[(topFrame_+3)&0x7], 
										&this->frames_[(topFrame_+2)&0x7], 
										&this->frames_[(topFrame_+1)&0x7] };

	float&		iYaw = iDir[0];
	float&		iPitch = iDir[1];

	if (time < inputs[7]->time)
	{
		// before the start
		iSID = inputs[7]->spaceID;
		iVID = inputs[7]->vehicleID;
		iPos = inputs[7]->pos;
		iYaw = inputs[7]->yaw;
		iPitch = inputs[7]->pitch;

		Vector3 commonSpacePos = inputs[0]->pos;
		if( inputs[0]->vehicleID != inputs[7]->vehicleID || inputs[0]->spaceID != inputs[7]->spaceID )
		{
			Vector3 dummyDir( 0, 0, 0 );
            transformIntoCommon( inputs[0]->spaceID, inputs[0]->vehicleID, commonSpacePos, dummyDir );
			transformFromCommon( inputs[7]->spaceID, inputs[7]->vehicleID, commonSpacePos, dummyDir );
		}

		iVelocity =  commonSpacePos - inputs[7]->pos;
		iVelocity /=  static_cast<float>( inputs[0]->time - inputs[7]->time );
	}
	else if(time > frames_[topFrame_].time)
	{		
		// after the end

		// Support speculative movement for up to 2.0 seconds
		float weight = static_cast<float>( 
			std::max( -2.0, inputs[0]->time - time ) / ( inputs[0]->time - inputs[2]->time ) );

		Vector3 resultDirection;
		this->blendInputs(	iPos, 
							resultDirection, 
							*inputs[0], 
							*inputs[2], 
							weight, 
							inputs[0]->spaceID, 
							inputs[0]->vehicleID );

		iYaw	= inputs[0]->yaw;
		iPitch	= inputs[0]->pitch;
		iSID	= inputs[0]->spaceID;
		iVID	= inputs[0]->vehicleID;

		// set velocity, we should never have equal time 
		iVelocity = inputs[0]->pos - inputs[1]->pos;
		iVelocity /= static_cast<float>( inputs[0]->time - inputs[1]->time );
	}
	else
	{
		// in between - yay!
		for( uint i=1; i<NUM_INPUTS; i++ )
		{
			if( inputs[i]->time < time )
			{
				float weight = static_cast<float>( 
					( time - inputs[i-1]->time ) / 
					( inputs[i]->time - inputs[i-1]->time ) );

				iSID = inputs[i-1]->spaceID;
				iVID = inputs[i-1]->vehicleID;

				Vector3 resultDirection;
				this->blendInputs(	iPos,
									resultDirection,
									*inputs[i-1],
									*inputs[i],
									weight,
									iSID,
									iVID );

				iYaw = resultDirection.x;
				iPitch = resultDirection.y;


				// calculate velocity
				const size_t VELOCITY_HISTORY_SIZE = 3;

				const uint endFrame = std::min( NUM_INPUTS - 1, 
												i + VELOCITY_HISTORY_SIZE );

				Vector3 velocityTempPosition;
				Vector3 dumyDirection;
				this->blendInputs(	velocityTempPosition,
									dumyDirection,
									*inputs[endFrame-1],
									*inputs[endFrame],
									weight,
									iSID,
									iVID );

				iVelocity =	iPos - velocityTempPosition;
				iVelocity /= static_cast<float>( time - Math::lerp<double>( 
					weight, inputs[endFrame-1]->time, inputs[endFrame]->time));

				break;
			}
		}
	}

	if (!Filter::isActive())
	{
		iSID = frames_[topFrame_].spaceID;
		iVID = frames_[topFrame_].vehicleID;
		iPos = frames_[topFrame_].pos;
		iYaw = frames_[topFrame_].yaw;
		iPitch = frames_[topFrame_].pitch;
	}
}


/**
 *	Translate input position vector into a position in local space.
 */
void AvatarFilter::decodePosition(	Vector3 & result, 
									const Vector3 & inputPosition )
{
	result = inputPosition;

	if( result.y < -12000.0f )
	{
		float terrainHeight = Moo::TerrainBlock::getHeight( result[0], 
															result[2] );
		if (terrainHeight != Moo::TerrainBlock::NO_TERRAIN)
		{
			result.y = terrainHeight;
		}
	}
}


/**
 *	
 */
void AvatarFilter::blendCodedPositions( Vector3 & result, 
										const Vector3 & inputPosA, 
										const Vector3 & inputPosB, 
										float alpha )
{
	Vector3 decodedInputPosA;
	Vector3 decodedInputPosB;

	decodePosition( decodedInputPosA, inputPosA );
	decodePosition( decodedInputPosB, inputPosB );

	result.lerp( inputPosA, inputPosB, alpha );
}


void AvatarFilter::blendInputs(	Vector3 & resultPosition,
								Vector3 & resultDirection,
								const Frame & inputA,
								const Frame & inputB, 
								float alpha,
								SpaceID targetSpace,
								ObjectID targetVehicle )
{
	if( inputA.spaceID == targetSpace && inputA.vehicleID == targetVehicle &&
		inputB.spaceID == targetSpace && inputB.vehicleID == targetVehicle )
	{
		Vector3 decodedInputPosA;
		Vector3 decodedInputPosB;

		decodePosition( decodedInputPosA, inputA.pos );
		decodePosition( decodedInputPosB, inputB.pos );

		resultPosition.lerp( decodedInputPosA, decodedInputPosB, alpha );

		resultDirection.set(Angle( ( Angle::sameSignAngle( inputA.yaw, inputB.yaw ) - inputA.yaw ) * alpha + inputA.yaw ),
							Angle( ( Angle::sameSignAngle( inputA.pitch, inputB.pitch ) - inputA.pitch ) * alpha + inputA.pitch ),
							0.0f );	
	}
	else
	{
		Vector3 positionA;
		Vector3 positionB;

		decodePosition( positionA, inputA.pos );
		decodePosition( positionB, inputB.pos );

		Vector3 directionA( inputA.yaw, inputA.pitch, 0.0f );
		Vector3 directionB( inputB.yaw, inputB.pitch, 0.0f );

		transformIntoCommon(	inputA.spaceID,
								inputA.vehicleID,
								positionA,
								directionA );

		transformIntoCommon(	inputB.spaceID,
								inputB.vehicleID,
								positionB,
								directionB );

		resultPosition.lerp( positionA, positionB, alpha );

		resultDirection.set(Angle( ( Angle::sameSignAngle( directionA.x, directionB.x ) - directionA.x ) * alpha + directionA.x ),
							Angle( ( Angle::sameSignAngle( directionA.y, directionB.y ) - directionA.y ) * alpha + directionA.y ),
							0.0f );

		transformFromCommon(  targetSpace,  targetVehicle, resultPosition, resultDirection );
	}
}


/**
 *	This method gets the last input.
 *	Its arguments are left untouched if it has no last input.
 */
bool AvatarFilter::getLastInput( double & time, SpaceID & spaceID,
	ObjectID & vehicleID, Position3D & pos, float * auxFiltered )
{
	if (!reset_)
	{
		Frame & f = frames_[topFrame_];
		time = f.time;
		spaceID = f.spaceID;
		vehicleID = f.vehicleID;
		decodePosition( pos, f.pos );
		if (auxFiltered != NULL)
		{
			auxFiltered[0] = f.yaw;
			auxFiltered[1] = f.pitch;
		}
		return true;
	}
	return false;
}



/**
 *	Standard get attribute method.
 */
PyObject * AvatarFilter::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();

	return Filter::pyGetAttribute( attr );
}

/**
 *	Standard set attribute method.
 */
int AvatarFilter::pySetAttribute( const char * attr, PyObject * value )
{
	PY_SETATTR_STD();

	return Filter::pySetAttribute( attr, value );
}

/*~ function AvatarFilter.callback
 *
 *	This method adds a callback function to the filter.  This function will
 *	be called at a time after the event specified by the whence argument.
 *	The amount of time after the event is specified, in seconds, by the
 *	extra argument.
 *
 *	If whence is -1, then the event is when the filter's
 *	timeline reaches the penultimate update (i.e. just before the current
 *	update position starts influencing the entity).  If whence is 0, then
 *	the event is when the current update is reached.  If whence is 1
 *	then the event is when the timeline reaches the time that
 *	the callback was specified.
 *
 *	The function will get one argument, which will be zero if the
 *	passMissedBy argumenty is zero, otherwise it will be the amount
 *	of time that passed between the time specified for the callback
 *	and the time it was actually called at.
 *
 *	Note:	When the callback is made, the entity position will already have
 *			been set for the frame (so it'll be at some spot after the desired
 *			callback time), but the motors will not yet have been run on the
 *			model to move it. The positions of other entities may or may not
 *			yet have been updated for the frame.
 *
 *	Note:	If a callback function returns '1', the entity position will be
 *			snapped to the position that the entity would have had at the
 *			exact time the callback should have been called.

 *	@param	whence	an integer (-1, 0, 1).  Which event to base the callback
 *					on.
 *	@param	fn		a callable object.  The function to call.  It should take
 *					one integer argument.
 *	@param	extra	a float.  The time after the event specified by whence
 *					tha the function should be called at.
 *	@param	passMissedBy	whether or not to pass to the function the time
 *					it missed its ideal call-time by.
 *
 */
/**
 *	Call the given function when the filter's timeline reaches a given
 *	point in time, plus or minus any extra time. The point can be either
 *	-1 for the penultimate update time (i.e. just before current position
 *	update begins influencing the entity), 0 for the last update time
 *	(i.e. when at the current position update), or 1 for the game time
 *	(i.e. some time after the current position update is at its zenith).
 *
 *	@note	When the callback is made, the entity position will already have
 *			been set for the frame (so it'll be at some spot after the desired
 *			callback time), but the motors will not yet have been run on the
 *			model to move it. The positions of other entities may or may not
 *			yet have been updated for the frame.
 *	@note	If a callback function returns '1', the entity position will be
 *			snapped to the position that the entity would have had at the
 *			exact time the callback should have been called.
 */
void AvatarFilter::callback( int whence, SmartPointer<PyObject> fn,
	float extra, bool passMissedBy )
{
	double	whTime;
	switch (whence)
	{
		case -1:	whTime = frames_[(topFrame_+7)&0x7].time;	break;
		case  0:	whTime = frames_[topFrame_].time;			break;
		case  1:	whTime = Filter::getTimeNow();				break;
		default:	whTime = 0.0;								break;
	}
	callbacks_.push( new Callback( whTime + extra, fn, passMissedBy ) );
}


/**
 *	Python factory method
 */
PyObject * AvatarFilter::pyNew( PyObject * args )
{
	if (PyTuple_Size( args ) != 0)
	{
		PyErr_SetString( PyExc_TypeError,
			"BigWorld.AvatarFilter() expects no arguments" );
		return NULL;
	}

	return new AvatarFilter();
}



// -----------------------------------------------------------------------------
// Section: AvatarDropFilter
// -----------------------------------------------------------------------------


PY_TYPEOBJECT( AvatarDropFilter )

PY_BEGIN_METHODS( AvatarDropFilter )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( AvatarDropFilter )
PY_END_ATTRIBUTES()

/*~ function BigWorld.AvatarDropFilter
 *
 *	This function creates a new AvatarDropFilter, which is used to move avatars
 *	around the same as an AvatarFilter does, but also to keep them on the
 *	ground beneath their specified positions.
 *
 *	@return a new AvatarDropFilter
 */
PY_FACTORY( AvatarDropFilter, BigWorld )


/**
 *	AvatarDropFilter constructor.
 */
AvatarDropFilter::AvatarDropFilter( PyTypePlus * pType ) :
	AvatarFilter( pType )
{
}


/**
 *	Same as AvatarFilter, but with some preprocessing
 */
void AvatarDropFilter::input( double time, SpaceID spaceID, ObjectID vehicleID,
	const Position3D & pos, float * auxFiltered )
{
	Vector3 iPos( pos );
	Vector3 riPos;

	if (filterDropPoint( &*entity_->pSpace(), iPos, riPos ))
	{
		iPos[1] = riPos[1];
	}

	AvatarFilter::input( time, spaceID, vehicleID, iPos, auxFiltered );
}


/**
 *	This method does the same thing as AvatarFilter::output,
 *	but always finds the drop point after moving.
 */
void AvatarDropFilter::output( double time )
{
	Position3D iPos, riPos;

	float oy = entity_->position().y;

	// Do the standard AvatarFilter::output

	AvatarFilter::output(time);
	iPos = entity_->position();
/*
	static DogWatch dwAvDropOutput("AvDrop");
	dwAvDropOutput.start();

	// Now do a drop. Start slightly above..
	//iPos.y += 0.1f;
	iPos.y = oy + 0.5f;

	if (filterDropPoint( &*entity_.pSpace(), Vector3( iPos ), riPos, 1.0f ))
	{
		iPos[1] = riPos[1];
	}
	else
	{
		float terrainHeight = Moo::TerrainBlock::getHeight( iPos[0], iPos[2] );
		if (terrainHeight != Moo::Terrain::NO_TERRAIN)
		{
			iPos[1] = terrainHeight;
		}
	}

	entity_->pos(iPos);

	dwAvDropOutput.stop();
	*/
}


/**
 *	Python factory method
 */
PyObject * AvatarDropFilter::pyNew( PyObject * args )
{
	if (PyTuple_Size( args ) != 0)
	{
		PyErr_SetString( PyExc_TypeError,
			"BigWorld.AvatarDropFilter() expects no arguments" );
		return NULL;
	}

	return new AvatarDropFilter();
}


// -----------------------------------------------------------------------------
// Section: PlayerAvatarFilter
// -----------------------------------------------------------------------------


PY_TYPEOBJECT( PlayerAvatarFilter )

PY_BEGIN_METHODS( PlayerAvatarFilter )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( PlayerAvatarFilter )
PY_END_ATTRIBUTES()

/*~ function BigWorld.PlayerAvatarFilter
 *
 *	This function creates a new PlayerAvatarFilter.  This updates the position
 *	and yaw of the entity to those specified by the most recent server update.
 *
 *	@return	a new PlayerAvatarFilter
 */
PY_FACTORY( PlayerAvatarFilter, BigWorld )


/**
 *	Constructor
 */
PlayerAvatarFilter::PlayerAvatarFilter( PyTypePlus * pType ) :
	Filter( pType ),
	time_( -1000.0 ),
	spaceID_( 0 ),
	vehicleID_( 0 ),
	pos_( 0, 0, 0 ),
	headYaw_( 0 ),
	headPitch_( 0 ),
	aux2_( 0 )
{

}


/**
 *	Input method
 */
void PlayerAvatarFilter::input( double time, SpaceID spaceID,
	ObjectID vehicleID, const Position3D & pos, float * auxFiltered )
{
	time_ = time;

	spaceID_ = spaceID;
	vehicleID_ = vehicleID;
	pos_ = pos;
	if (auxFiltered != NULL)
	{
		headYaw_ = auxFiltered[0];
		headPitch_ = auxFiltered[1];
		aux2_ = auxFiltered[2];
	}
}


/**
 *	Output method
 */
void PlayerAvatarFilter::output( double time )
{
	coordinateSystemCheck( entity_, spaceID_, vehicleID_ );

	float	aux[3];
	aux[0] = headYaw_;
	aux[1] = headPitch_;
	aux[2] = aux2_;
	entity_->pos( pos_, aux, 3 );
}


/**
 *	This method gets the last input.
 *	Its arguments are left untouched if it has no last input.
 */
bool PlayerAvatarFilter::getLastInput( double & time, SpaceID & spaceID,
	ObjectID & vehicleID, Position3D & pos, float * auxFiltered )
{
	if (time_ != -1000.0)
	{
		time = this->time_;
		spaceID = this->spaceID_;
		vehicleID = this->vehicleID_;
		pos = this->pos_;
		if (auxFiltered != NULL)
		{
			auxFiltered[0] = this->headYaw_;
			auxFiltered[1] = this->headPitch_;
			auxFiltered[2] = this->aux2_;
		}
		return true;
	}
	return false;
}


/**
 *	Python factory method
 */
PyObject * PlayerAvatarFilter::pyNew( PyObject * args )
{
	if (PyTuple_Size( args ) != 0)
	{
		PyErr_SetString( PyExc_TypeError,
			"BigWorld.PlayerAvatarFilter() expects no arguments" );
		return NULL;
	}

	return new PlayerAvatarFilter();
}


// -----------------------------------------------------------------------------
// Section: BoidsFilter
// -----------------------------------------------------------------------------


PY_TYPEOBJECT( BoidsFilter )

PY_BEGIN_METHODS( BoidsFilter )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( BoidsFilter )

	/*~ attribute BoidsFilter.influenceRadius
	 *
	 *	The influence radius simply determines how close
	 *	a boid must be to another to affect it at all.
	 *
	 *	@type	float
	 */
	PY_ATTRIBUTE( influenceRadius )

	/*~ attribute BoidsFilter.collisionFraction
	 *
	 *	The collision fraction applies to the
	 *	influence radius, and specifies the percentile
	 *	band of the influence radius which is considered
	 *	to be a collision.
	 *
	 *	@type	float
	 */
	PY_ATTRIBUTE( collisionFraction )

	/*~ attribute BoidsFilter.approachRadius
	 *
	 *	When the boids are in the process of landing, and
	 *	a boid is inside the approach radius, then it will
	 *	point directly at its goal, instead of smoothly turning
	 *	towards it.
	 *
	 *	@type	float
	 */
	PY_ATTRIBUTE( approachRadius )

	/*~ attribute BoidsFilter.stopRadius
	 *
	 *	When the boids are in the process of landing, and
	 *	a boid is inside the stop radius, it will be positioned
	 *	exactly at the goal, and its speed will be set to 0.
	 *
	 *	@type	float
	 */
	PY_ATTRIBUTE( stopRadius )

	/*~ attribute BoidsFilter.speed
	 *
	 *	The normal speed of the boids.  While boids will speed
	 *	up and slow down during flocking, their speed will always
	 *	be damped toward this speed.
	 *
	 *	@type	float
	 */
	PY_ATTRIBUTE( speed )

PY_END_ATTRIBUTES()

/*~ function BigWorld.BoidsFilter
 *
 *	This function creates a new BoidsFilter.  This is used to filter the
 *	movement of an Entity which consists of several models (boids) for which
 *	flocking behaviour is desired.
 *
 *	@return a new BoidsFilter
 */
PY_FACTORY( BoidsFilter, BigWorld )


const float INFLUENCE_RADIUS				= 10.0f;
const float INFLUENCE_RADIUS_SQUARED		= INFLUENCE_RADIUS * INFLUENCE_RADIUS;
const float COLLISION_FRACTION				= 0.5f;
const float INV_COLLISION_FRACTION			= 1.0f/(1.0f-COLLISION_FRACTION);
const float NORMAL_SPEED					= 0.5f;
const float ANGLE_TWEAK						= 0.02f;
const float PITCH_TO_SPEED_RATIO			= 0.002f;
const float GOAL_APPROACH_RADIUS_SQUARED	= (10.0f * 10.0f);
const float GOAL_STOP_RADIUS_SQUARED		= (1.0f * 1.0f);
const int	STATE_LANDING					= 1;
const float MAX_STEP 	= 0.1f;

/**
 *	Constructor.
 */
BoidsFilter::BoidsFilter( PyTypePlus * pType ) :
	AvatarFilter( pType ),
	prevTime_( -1.f ),
	INFLUENCE_RADIUS( 10.f ),
	COLLISION_FRACTION( 0.5f ),
	NORMAL_SPEED( 0.5f ),
	ANGLE_TWEAK( 0.02f ),
	PITCH_TO_SPEED_RATIO( 0.002f ),
	GOAL_APPROACH_RADIUS( 10.f ),
	GOAL_STOP_RADIUS( 1.f )
{
}

/**
 *	Standard get attribute method.
 */
PyObject * BoidsFilter::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();

	return AvatarFilter::pyGetAttribute( attr );
}

/**
 *	Standard set attribute method.
 */
int BoidsFilter::pySetAttribute( const char * attr, PyObject * value )
{
	PY_SETATTR_STD();

	return AvatarFilter::pySetAttribute( attr, value );
}

/*~ callback Entity.boidsLanded
 *
 *	This callback method is called on entities using the boids filter.
 *	It lets the entity know when the boids have landed.  A typical response
 *	to this notifier is to delete the boids models.
 *
 *	@see BigWorld.BoidsFilter
 */
/**
 *	This method produces the output of this filter.
 */
void BoidsFilter::output( double time )
{
	static DogWatch dwBoidsFilterOutput("BoidsFilter");
	dwBoidsFilterOutput.start();

	AvatarFilter::output( time );

	PyObject* pState = PyObject_GetAttrString( entity_, "state" );

	if(!pState)
	{
		WARNING_MSG("BoidsFilter: Entity has no state\n");
		dwBoidsFilterOutput.stop();
		return;
	}

	int state = PyInt_AsLong(pState);
	Py_DECREF(pState);

	Vector3 goal( entity_->position().x,
		entity_->position().y, entity_->position().z );

	if(state != STATE_LANDING)
	{
		goal.y = Moo::TerrainBlock::getHeight( goal.x, goal.z ) + 20.f;

		if (goal.y < -20.0f)
			goal.y = -20.0f;
	}

	float dTime = 1.f;

	if (prevTime_ != -1.f)
	{
		dTime = float(time - prevTime_);
	}
	prevTime_ = time;

	ChunkEmbodiments & models = entity_->auxiliaryEmbodiments();
	int size = models.size();

	int oldSize = boidData_.size();

	if (oldSize != size)
	{
		boidData_.resize( size );

		for (int i = oldSize; i < size; i++)
		{
			boidData_[i].pos_ += goal;
		}
	}

	bool boidsLanded = false;

	if (dTime < 20.f)
	{
		while (dTime > 0.f)
		{
			float thisStep = min( dTime, MAX_STEP );
			dTime -= MAX_STEP;

			for (int i = 0; i < size; i++)
			{
				ChunkEmbodimentPtr pCA = models[i];

				boidData_[i].updateModel( goal, &*pCA, *this, thisStep, state );

				if(state == STATE_LANDING && boidData_[i].pos_ == goal)
					boidsLanded = true;
			}
		}
	}

	// If any boids landed, call the script and it will delete the models.

	if(boidsLanded)
	{
		Script::call( PyObject_GetAttrString( entity_, "boidsLanded" ),
			PyTuple_New(0), "BoidsFilter::output: ", true );
	}

	dwBoidsFilterOutput.stop();
}


/**
 *	Constructor.
 */
BoidsFilter::BoidData::BoidData() :
	pos_( 20.0f*(unitRand()-unitRand()),
			2.0f * unitRand(),
			20.0f * ( unitRand() - unitRand() ) ),
	dir_( unitRand(), unitRand(), unitRand() ),
	yaw_( 0.f ),
	pitch_( 0.f ),
	roll_( 0.f ),
	dYaw_( 0.f ),
	speed_( 0.1f )
{
	// Make sure it is not the zero vector.
	dir_.x += 0.0001f;
	dir_.normalise();
}


/**
 *	This method updates the state of the model so that it is in its new
 *	position.
 */
void BoidsFilter::BoidData::updateModel( const Vector3 & goal,
		ChunkEmbodiment * pCA,
		const BoidsFilter & filter,
		float dTime,
		int state)
{
	// The settings currently assume that dTime is in 30th of a second.
	dTime *= 30.f;

	const BoidsFilter::Boids & boids = filter.boidData_;
	Vector3 deltaPos = Vector3::zero();
	Vector3 deltaDir = Vector3::zero();
	float count = 0;
	const float INFLUENCE_RADIUS_SQUARED = filter.getInfluenceRadiusSqr();
	const float INV_COLLISION_FRACTION = 1.0f/(1.0f-filter.COLLISION_FRACTION);
	const float GOAL_APPROACH_RADIUS_SQUARED = filter.getApproachRadiusSqr();
	const float GOAL_STOP_RADIUS_SQUARED = filter.getStopRadiusSqr();

	for (uint i = 0; i < boids.size(); i++)
	{
		const BoidData & otherBoid = boids[i];

		if (&otherBoid != this)
		{
			Vector3 diff = pos_ - otherBoid.pos_;
			float dist = diff.lengthSquared();
			dist = INFLUENCE_RADIUS_SQUARED - dist;

			if (dist > 0.f)
			{
				dist /= INFLUENCE_RADIUS_SQUARED;
				count += 1.f;

				diff.normalise();
				float collWeight = dist - filter.COLLISION_FRACTION;

				if (collWeight > 0.f)
				{
					collWeight *= INV_COLLISION_FRACTION;
				}
				else
				{
					collWeight = 0.f;
				}

				if (dist - (1.f - filter.COLLISION_FRACTION) > 0.f)
				{
					collWeight -= dist * (1.f - filter.COLLISION_FRACTION);
				}

				Vector3 delta = collWeight * diff;
				deltaPos += delta;
				deltaDir += otherBoid.dir_ * dist;
			}
		}
	}

	if (count != 0.f)
	{
		deltaDir /= count;
		deltaDir -= dir_;
		deltaDir *= 1.5f;
	};

	Vector3 delta = deltaDir + deltaPos;

	// Add in the influence of the global goal
	Vector3 goalDir = goal - pos_;
	goalDir.normalise();
	goalDir *= 0.5f;
	delta += goalDir;

	// First deal with pitch changes
	if (delta.y > 0.01f )
	{   // We're too low
		pitch_ += filter.ANGLE_TWEAK * dTime;

		if (pitch_ > 0.8f)
		{
			pitch_ = 0.8f;
		}
	}
	else if (delta.y < -0.01f)
	{   // We're too high
		pitch_ -= filter.ANGLE_TWEAK * dTime;
		if (pitch_ < -0.8f)
		{
			pitch_ = -0.8f;
		}
	}
	else
	{
		// Add damping
		pitch_ *= 0.98f;
	}

	// Speed up or slow down depending on angle of attack
	speed_ -= pitch_ * filter.PITCH_TO_SPEED_RATIO;

	// Damp back to normal
	speed_ = (speed_-filter.NORMAL_SPEED)*0.99f + filter.NORMAL_SPEED;

	if( speed_ < filter.NORMAL_SPEED/2 )
		speed_ = filter.NORMAL_SPEED/2;
	if( speed_ > filter.NORMAL_SPEED*5 )
		speed_ = filter.NORMAL_SPEED*5;

	// Now figure out yaw changes
	Vector3 offset	= delta;

	if(state != 1)
		offset[Y_COORD]		= 0.0f;

	delta				= dir_;
	offset.normalise( );
	float dot = offset.dotProduct( delta );

	// Speed up slightly if not turning much
	if (dot > 0.7f)
	{
		dot -= 0.7f;
		speed_ += dot * 0.05f;
	}

	Vector3 vo = offset.crossProduct( delta );
	dot = (1.0f - dot)/2.0f * 0.07f;

	if( vo.y > 0.05f )
		dYaw_ = (dYaw_*19.0f + dot) * 0.05f;
	else if( vo.y < -0.05f )
		dYaw_ = (dYaw_*19.0f - dot) * 0.05f;
	else
		dYaw_ *= 0.98f; // damp it

	yaw_ += dYaw_ * dTime;
	roll_ = -dYaw_ * 20.0f * dTime;


	Matrix m;
	m.setTranslate( pos_ );
	m.preRotateY( -yaw_ );
	m.preRotateX( -pitch_ );
	m.preRotateZ( -roll_ );

	dir_ = m.applyToUnitAxisVector( Z_AXIS );

	if(state == STATE_LANDING)
	{
		Vector3 goalDiff = pos_ - goal;
		float len = goalDiff.lengthSquared();

		if(len < GOAL_APPROACH_RADIUS_SQUARED)
			dir_ = goalDir;

		if(len > 0 && len < GOAL_STOP_RADIUS_SQUARED)
		{
			pos_ = goal;
			speed_ = 0;
		}
	}

	pos_ += dir_ * speed_ * dTime;

	((ChunkAttachment*)pCA)->setMatrix( m );
}


/**
 *	Python factory method
 */
PyObject * BoidsFilter::pyNew( PyObject * args )
{
	if (PyTuple_Size( args ) != 0)
	{
		PyErr_SetString( PyExc_TypeError,
			"BigWorld.BoidsFilter() expects no arguments" );
		return NULL;
	}

	return new BoidsFilter();
}

// filter.cpp
