/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef WITNESS_HPP
#define WITNESS_HPP

#include "real_entity.hpp"
#include "entity_cache.hpp"

class SpaceCache;
class SpaceViewportCache;


#define PY_METHOD_ATTRIBUTE_WITNESS( METHOD_NAME )							\
	PyObject * get_##METHOD_NAME()											\
	{																		\
		return new WitnessMethod( this, &_##METHOD_NAME );					\
	}																		\


/**
 *	This class is the Python object for methods in a Witness
 */
class WitnessMethod : public PyObjectPlus
{
	Py_Header( WitnessMethod, PyObjectPlus )

public:
	typedef PyObject * (*StaticGlue)(
		PyObject * pWitness, PyObject * args, PyObject * kwargs );

	WitnessMethod( Witness * w, StaticGlue glueFn,
		PyTypePlus * pType = &s_type_ );

	PY_KEYWORD_METHOD_DECLARE( pyCall )

private:
	EntityPtr	pEntity_;
	StaticGlue	glueFn_;
};

#undef PY_METHOD_ATTRIBUTE
#define PY_METHOD_ATTRIBUTE PY_METHOD_ATTRIBUTE_WITNESS


/**
 *	This class is a witness to the movements and perceptions of a RealEntity.
 *	It is created when a client is attached to this entity. Its main activity
 *	centres around the management of an Area of Interest list.
 */
class Witness : public Updatable
{
	PY_FAKE_PYOBJECTPLUS_BASE_DECLARE()
	Py_FakeHeader( Witness, PyObjectPlus )

public:
	// Creation/Destruction
	Witness( RealEntity & owner, BinaryIStream & data,
			CreateRealInfo createRealInfo );
	virtual ~Witness();
private:
	void init();
public:
	RealEntity & real()					{ return real_; }
	const RealEntity & real() const		{ return real_; }

	Entity & entity()					{ return entity_; }
	const Entity & entity() const		{ return entity_; }


	// Ex-overrides from RealEntity
	void writeOffloadData( BinaryOStream & data,
			const Mercury::Address & dstAddr );
	void writeBackupData( BinaryOStream & data );

	bool sendToClient( int entityMessageType, MemoryOStream & stream );
	void sendToProxy( int mercuryMessageType, MemoryOStream & stream );

	void setWitnessCapacity( ObjectID id, int bps );

	void requestEntityUpdate( ObjectID id,
			EventNumber * pEventNumbers, int size );

	void setSpaceViewportAck( ObjectID id, SpaceViewportID svid );
	void setVehicleAck( ObjectID id, ObjectID vehicleID );

	void addToAoI( Entity * pEntity, SpaceViewportID svid );
	void removeFromAoI( Entity * pEntity, SpaceViewportID svid );

	void viewportAvailable( SpaceViewport * newVP );
	void viewportWithdrawn( SpaceViewport * oldVP );
	Entity * traverseGateway( SpaceID spaceID, const Vector3 & pos );

	void newPosition( const Vector3 & position );

	void updatePositionReference( const RelPosRef & pos );
	void dumpAoI();
	void debugDump();

	// Misc
	void update();

	// Python
	virtual PyObject * pyGetAttribute( const char * attr );
	virtual int pySetAttribute( const char * attr, PyObject * value );

	virtual PyObject * pyAdditionalMembers( PyObject * pSeq ) { return pSeq; }
	virtual PyObject * pyAdditionalMethods( PyObject * pSeq ) { return pSeq; }

	PY_RW_ATTRIBUTE_DECLARE( maxPacketSize_, bandwidthPerUpdate );

	PY_RW_ATTRIBUTE_DECLARE( stealthFactor_, stealthFactor )

	void unitTest();
	PY_AUTO_METHOD_DECLARE( RETVOID, unitTest, END )

	PY_AUTO_METHOD_DECLARE( RETVOID, dumpAoI, END )

	void setAoIRadius( float radius, float hyst = 5.f );
	PY_AUTO_METHOD_DECLARE( RETVOID, setAoIRadius,
		ARG( float, OPTARG( float, 5.f, END ) ) )

	PY_RW_ATTRIBUTE_DECLARE( aoiUpdates_, enableAoICallbacks )

	static void addWatchers();

private:

	typedef std::vector< EntityCache * > KnownEntityQueue;


	// Private methods
	void addToSeen( EntityCache * pCache );
	void deleteFromSeen( Mercury::Bundle & bundle,
			KnownEntityQueue::iterator iter,
			ObjectID id = 0 );

	void sendEnter( Mercury::Bundle & bundle, EntityCache * pCache );
	void sendCreate( Mercury::Bundle & bundle, EntityCache * pCache );

	void sendGameTime( );

	void onLeaveAoI( EntityCache * pCache, ObjectID id );

	Mercury::Bundle & bundle();
	void sendToClient();

	IDAlias allocateIDAlias( const Entity & entity );

	void viewportSubscribe( EntityCache *pSrcEntityCache, uint32 numGatewaySrcs,
		SpaceViewport ** candidates, uint32 numCandidates );
	void viewportUnsubscribe( SpaceViewportID svid );

	SpaceCache * findOrCreateSpaceCache( Space * pSpace );

	static float calculatePosRef( float entityPosValue, uint8 relPosRefValue );

	// variables

	RealEntity		& real_;
	Entity			& entity_;


	TimeStamp		noiseCheckTime_;
	TimeStamp		noisePropagatedTime_;
	bool			noiseMade_;

	long			maxPacketSize_;

	KnownEntityQueue	entityQueue_;
	EntityCacheMap		aoiMap_;

	float stealthFactor_;

	float aoiHyst_;
	float aoiRadius_;
	bool aoiUpdates_;

	int bandwidthDeficit_;

	IDAlias freeAliases_[256];
	int		numFreeAliases_;

	// This is used as a reference for shorthand positions sent as 3 uint8's
	// relative to this reference position.  (see also RelPosRef). It is used
	// to reduce bandwidth.
	Vector3 referencePosition_;
	// This is the sequence number of the relative position reference sent
	// from the client.
	uint8 	referenceSeqNum_;

	typedef std::map<Space*,SpaceCache*>		SpaceCaches;
	SpaceCaches				spaces_;
	uint32					allSpacesDataChangeSeq_;

	typedef std::vector<SpaceViewportCache*>	SpaceViewportCaches;
	SpaceViewportCaches		viewports_;		// some may be NULL
	//SpaceViewport			ownEyes_;
	bool					viewportsArrayChanged_;

	friend BinaryIStream & operator>>( BinaryIStream & stream,
			EntityCache & entityCache );
	friend BinaryOStream & operator<<( BinaryOStream & stream,
			const EntityCache & entityCache );
};

#undef PY_METHOD_ATTRIBUTE
#define PY_METHOD_ATTRIBUTE PY_METHOD_ATTRIBUTE_BASE


class SpaceViewport;
typedef SmartPointer<SpaceViewport> SpaceViewportPtr;
class AoITrigger;

/**
 *	This class is a record of a space subscribed to by a REWW
 */
class SpaceCache
{
public:
	Space			* pSpace_;
	int32			knownSpaceDataSeq_;
	uint32			numRefs_;
};

/**
 *	This class is a record of a space viewport subscribed to by a REWW
 */
class SpaceViewportCache
{
public:
	SpaceViewportCache( SpaceCache * pSpaceCache );
	~SpaceViewportCache();

	enum Flags
	{
		ALIVE = 1,
		KNOWN = 2
	};

	SpaceViewportPtr	pViewport_;
	SpaceCache		* pSpaceCache_;
	EntityCache		* pGatewaySrcEC_;
	EntityCache		* pGatewayDstEC_;
	uint32			flags_;
	float			distance_;
	AoITrigger		* pAoITrigger_;

	void pSpaceCache( SpaceCache * pSpaceCache );
};


#ifdef CODE_INLINE
#include "witness.ipp"
#endif

#endif // WITNESS_HPP
