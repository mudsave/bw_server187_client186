/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef ENTITY_RECOVERER_HPP
#define ENTITY_RECOVERER_HPP

#include "network/mercury.hpp"

/**
 *	This class is used for loading entities from the database over a period of
 *	time.
 */
class EntityRecoverer
{
public:
	EntityRecoverer( int numExpected = 0 );

	void start();

	void addEntity( EntityTypeID entityTypeID, DatabaseID dbID );

	void onRecoverEntityComplete();

private:
	void checkFinished();
	bool sendNext();

	bool allSent() const	{ return numSent_ >= int(entities_.size()); }

	typedef std::vector< std::pair< EntityTypeID, DatabaseID > > Entities;
	Entities entities_;
	int numOutstanding_;
	int numSent_;
};

#endif // ENTITY_RECOVERER_HPP
