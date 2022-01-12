/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef FILTER_HPP
#define FILTER_HPP

#include <queue>

#include "network/basictypes.hpp"
#include "math/vector3.hpp"

#include "pyscript/pyobject_plus.hpp"
#include "pyscript/script.hpp"


class Entity;


/*~ class BigWorld.Filter
 *
 *	This is the abstract base class for all Filter objects.  Filters are used
 *	to interpolate volatile data of entities (the position and the orientation) 
 *	between server updates of these variables.
 *
 *	The only functionality defined in the base class is the reset function,
 *	which resets the filter to have its current time set to the specified time.
 *
 *	Every entity has a filter attribute.  Assigning a filter to that attribute
 *	sets that entity to be the owner of the specified filter.  This will make
 *	the filter process server updates for that entity.
 */
/*
 *	This class is the base class for all filters.
 */
class Filter : public PyObjectPlus
{
	Py_Header( Filter, PyObjectPlus )

public:
	Filter( PyTypePlus * pType );

	virtual void reset( double time );

	// Each class knows how much and what auxFiltered data there is.
	virtual void input( double time, SpaceID spaceID, ObjectID vehicleID,
		const Position3D & pos, float * auxFiltered ) = 0;

	virtual void output( double time ) = 0;

	virtual void owner( Entity * pEntity );

	virtual bool getLastInput( double & time, SpaceID & spaceID,
		ObjectID & vehicleID, Position3D & pos, float * auxFiltered ) = 0;

	PyObject * pyGetAttribute( const char * attr );
	int pySetAttribute( const char * attr, PyObject * value );

	PY_AUTO_METHOD_DECLARE( RETVOID, reset,
		OPTARG( double, getTimeNow(), END ) )

	static bool isActive()				{ return isActive_; }
	static void isActive( bool value )	{ isActive_ = value; }

protected:
	Entity		* entity_;

	static double getTimeNow();

private:
	static bool isActive_;
};

typedef SmartPointer<Filter> FilterPtr;

PY_SCRIPT_CONVERTERS_DECLARE( Filter )


/*~ class BigWorld.DumbFilter
 *
 *	This subclass of the Filter class simply sets the position of the entity
 *	to the most recent position specified by the server, performing no 
 *	interpolation.  It ignores position updates which are older than the
 *	current position of the entity.
 *
 *	A new DumbFilter can be created using BigWorld.DumbFilter function.
 */
/**
 *	This is a dumb filter class. It uses only the first element of auxFiltered.
 *	It simply sets the position to the last input position.
 *	It does however ensure ordering on time, and doesn't call
 *	the Entity's 'pos' function unless necessary. This could
 *	almost be the Teleport filter.
 */
class DumbFilter : public Filter
{
	Py_Header( DumbFilter, Filter )

public:
	DumbFilter( PyTypePlus * pType = &s_type_ );

	void reset( double time );

	virtual void input( double time, SpaceID spaceID, ObjectID vehicleID,
		const Position3D & pos, float * auxFiltered );
	virtual void output( double time );
	virtual bool getLastInput( double & time, SpaceID & spaceID,
		ObjectID & vehicleID, Position3D & pos, float * auxFiltered );

	PY_FACTORY_DECLARE()

private:
	double		time_;
	Position3D	pos_;

	float		yaw_;

	SpaceID		spaceID_;
	ObjectID	vehicleID_;
};




#endif // FILTER_HPP
