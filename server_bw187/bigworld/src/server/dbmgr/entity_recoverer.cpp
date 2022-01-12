/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "Python.h"		// See http://docs.python.org/api/includes.html

#include "entity_recoverer.hpp"
#include "database.hpp"

DECLARE_DEBUG_COMPONENT( 0 )

// -----------------------------------------------------------------------------
// Section: RecoveringEntityHandler
// -----------------------------------------------------------------------------
/**
 *	This class handles recovering an entity.
 */
class RecoveringEntityHandler : public Mercury::ReplyMessageHandler,
								public Database::GetEntityHandler,
                                public IDatabase::IPutEntityHandler
{
	enum State
	{
		StateInit,
		StateWaitingForSetBaseToLoggingOn,
		StateWaitingForCreateBase,
		StateWaitingForSetBaseToFinal
	};

	State				state_;
	EntityDBKey			ekey_;
	EntityDBRecordOut	outRec_;
	Mercury::Bundle		createBaseBundle_;
	EntityRecoverer&	mgr_;

public:
	RecoveringEntityHandler( EntityTypeID entityTypeID, DatabaseID dbID,
		EntityRecoverer& mgr );
	virtual ~RecoveringEntityHandler()	{	mgr_.onRecoverEntityComplete();	}

	void recover();

	virtual void handleMessage( const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data, void * );
	virtual void handleException( const Mercury::NubException & ne, void * );

	// IDatabase::IGetEntityHandler/Database::GetEntityHandler overrides
	virtual EntityDBKey& key()					{	return ekey_;	}
	virtual EntityDBRecordOut& outrec()			{	return outRec_;	}
	virtual void onGetEntityCompleted( bool isOK );

	// IDatabase::IPutEntityHandler overrides
	virtual void onPutEntityComplete( bool isOK, DatabaseID dbID );

private:

};

/**
 *	Constructor.
 */
RecoveringEntityHandler::RecoveringEntityHandler( EntityTypeID typeID,
	DatabaseID dbID, EntityRecoverer& mgr )
	: state_(StateInit), ekey_( typeID, dbID ), outRec_(),
	createBaseBundle_(), mgr_(mgr)
{}

/**
 *	Start recovering the entity.
 */
void RecoveringEntityHandler::recover()
{
	// Start create new base message even though we're not sure entity exists.
	// This is to take advantage of getEntity() streaming properties into the
	// bundle directly.
	Database::prepareCreateEntityBundle( ekey_.typeID, ekey_.dbID,
		Mercury::Address( 0, 0 ), this, createBaseBundle_ );

	// Get entity data into bundle
	outRec_.provideStrm( createBaseBundle_ );
	Database::instance().getEntity( *this );
	// When getEntity() completes onGetEntityCompleted() is called.
}

/**
 *	Handles reponse from BaseAppMgr that base was created successfully.
 */
void RecoveringEntityHandler::handleMessage( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data, void * )
{
	Mercury::Address proxyAddr;
	data >> proxyAddr;
	EntityMailBoxRef baseRef;
	data >> baseRef;
	// Still may contain a sessionKey if it is a proxy and contains
	// latestVersion and impendingVersion from the BaseAppMgr.
	data.finish();

	state_ = StateWaitingForSetBaseToFinal;
	EntityMailBoxRef*	pBaseRef = &baseRef;
	EntityDBRecordIn	erec;
	erec.provideBaseMB( pBaseRef );
	Database::instance().putEntity( ekey_, erec, *this );
	// When putEntity() completes, onPutEntityComplete() is called.
}

/**
 *	Handles reponse from BaseAppMgr that base creation has failed.
 */
void RecoveringEntityHandler::handleException( const Mercury::NubException & ne, void * )
{
	delete this;
}

void RecoveringEntityHandler::onGetEntityCompleted( bool isOK )
{
	if (isOK)
	{
		state_ = StateWaitingForSetBaseToLoggingOn;
		EntityMailBoxRef	baseRef;
		Database::setBaseRefToLoggingOn( baseRef, ekey_.typeID );
		EntityMailBoxRef*	pBaseRef = &baseRef;
		EntityDBRecordIn erec;
		erec.provideBaseMB( pBaseRef );
		Database::instance().getIDatabase().putEntity( ekey_, erec, *this );
		// When putEntity() completes, onPutEntityComplete() is called.
	}
	else
	{
		delete this;
	}
}

void RecoveringEntityHandler::onPutEntityComplete( bool isOK, DatabaseID dbID )
{
	MF_ASSERT(isOK);

	if (state_ == StateWaitingForSetBaseToLoggingOn)
	{
		state_ = StateWaitingForCreateBase;
		Database::instance().baseAppMgr().send( &createBaseBundle_ );
	}
	else
	{
		MF_ASSERT(state_ == StateWaitingForSetBaseToFinal);
		delete this;
	}
}



// -----------------------------------------------------------------------------
// Section: EntityRecoverer
// -----------------------------------------------------------------------------
/**
 *	Constructor.
 */
EntityRecoverer::EntityRecoverer( int numExpected ) :
	numOutstanding_( 0 ),
	numSent_( 0 )
{
	entities_.reserve( numExpected );
}


/**
 *	This method starts loading the entities into the system.
 */
void EntityRecoverer::start()
{
	// TODO: Make this configurable.
	const int maxOutstanding = 5;

	while ((numOutstanding_ < maxOutstanding) && this->sendNext())
	{
		// Do nothing.
	}
}


/**
 *	This method adds a database entry that will later be loaded.
 */
void EntityRecoverer::addEntity( EntityTypeID entityTypeID, DatabaseID dbID )
{
	entities_.push_back( std::make_pair( entityTypeID, dbID ) );
}

/**
 *	This method loads the next pending entity.
 */
bool EntityRecoverer::sendNext()
{
	bool done = this->allSent();
	if (!done)
	{
		RecoveringEntityHandler* pEntityRecoverer =
			new RecoveringEntityHandler( entities_[numSent_].first,
				entities_[numSent_].second, *this );
		pEntityRecoverer->recover();

		++numSent_;

		// TRACE_MSG( "EntityRecoverer::sendNext: numSent = %d\n", numSent_ );

		++numOutstanding_;
	}

	this->checkFinished();

	return !done;
}

/**
 *	RecoveringEntityHandler calls this method when the process of recovering
 *	the entity has completed - regardless of success or failure.
 */
void EntityRecoverer::onRecoverEntityComplete()
{
	--numOutstanding_;
	this->sendNext();
}

/**
 *	This method checks whether or not this object has finished its job. If it
 *	has, this object deletes itself.
 */
void EntityRecoverer::checkFinished()
{
	// TODO: Inform the database.
	if ((numOutstanding_ == 0) && this->allSent())
	{
		Database::instance().startServerEnd();
		delete this;
	}
}

// entity_recoverer.cpp
