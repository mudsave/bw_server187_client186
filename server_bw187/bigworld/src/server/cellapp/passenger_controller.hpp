/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef PASSENGER_CONTROLLER_HPP
#define PASSENGER_CONTROLLER_HPP

#include "controller.hpp"

#include "entity_population.hpp"


/**
 *	This class is used to place an entity on a vehicle.
 */
class PassengerController : public Controller, public EntityPopulation::Observer
{
	DECLARE_CONTROLLER_TYPE( PassengerController )
public:
	PassengerController( ObjectID vehicleID = 0 );
	~PassengerController();

	virtual void writeGhostToStream( BinaryOStream& stream );
	virtual bool readGhostFromStream( BinaryIStream& stream );

	virtual void startGhost();
	virtual void stopGhost();

	void onVehicleGone( Position3D * pBackupPosition = NULL );

	// Override from PopulationWatcher
	virtual void onEntityAdded( Entity & entity );

private:
	PassengerController( const PassengerController& );
	PassengerController& operator=( const PassengerController& );

	ObjectID	vehicleID_;
	Position3D	initialGlobalPosition_;
};

#endif // PASSENGER_CONTROLLER_HPP
