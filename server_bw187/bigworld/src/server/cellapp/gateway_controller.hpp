/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef GATEWAY_CONTROLLER_HPP
#define GATEWAY_CONTROLLER_HPP

#include "controller.hpp"

class Cell;


/**
 *	This class is the source side of a gateway between co-ordinate systems
 *
 *	When a cell 'sees' one of these, it subscribes to the correspodning
 *	gateway dst controller for a certain range of updates.
 */
class GatewaySrcController : public Controller
{
	DECLARE_CONTROLLER_TYPE( GatewaySrcController )
public:
	GatewaySrcController();
	~GatewaySrcController();

	virtual void writeGhostToStream( BinaryOStream& stream );
	virtual bool readGhostFromStream( BinaryIStream& stream );

	//virtual void writeRealToStream( BinaryOStream& stream );
	//virtual bool readRealFromStream( BinaryIStream& stream );

	virtual void startGhost();
	virtual void stopGhost();

	//virtual void startReal( bool isInitialStart );
	//virtual void stopReal( bool isFinalStop );

	void subscribe( const Cell & cell, float range );

	void gatewayDstRelocated( const EntityMailBoxRef & embr );

	ObjectID gatewayDstID() const			{ return gatewayDstMB_.id; }

private:
	GatewaySrcController( const GatewaySrcController& );
	GatewaySrcController& operator=( const GatewaySrcController& );

	EntityMailBoxRef	gatewayDstMB_;
};


/**
 *	This class is the destination side of a gateway between co-ordinate systems
 *
 *	When a cell 'sees' one of these, it attempts to match it up to a gateway src
 *	controller that it has subscribed to. If successful, then it creates a
 *	System to represent its view through the connected gateway.
 */
class GatewayDstController : public Controller
{
	DECLARE_CONTROLLER_TYPE( GatewayDstController )
public:
	GatewayDstController();
	~GatewayDstController();

	virtual void writeGhostToStream( BinaryOStream& stream );
	virtual bool readGhostFromStream( BinaryIStream& stream );

	virtual void startGhost();
	virtual void stopGhost();

	void relocated();
	void gatewaySrc( GatewaySrcController * pGatewaySrc );

	/**
	 *	This class contains details about the subscription of a particular cell
	 */
	struct Subscription
	{
		Mercury::Address	cellHost_;
		SpaceID				spaceID_;
		float				range_;
	};

	void subscribe( const Subscription & sub );

	ObjectID				gatewaySrcID() const
		{ return gatewaySrcID_; }
	GatewaySrcController *	gatewaySrcController() const
		{ return gatewaySrcController_; }

	typedef std::vector<Subscription> Subscriptions;
	const Subscriptions &	subscriptions() const
		{ return subscriptions_; }

private:
	GatewayDstController( const GatewayDstController& );
	GatewayDstController& operator=( const GatewayDstController& );

	ObjectID					gatewaySrcID_;
	GatewaySrcController *		gatewaySrcController_;
	Subscriptions				subscriptions_;
	bool						ghostStarted_;
};



#endif // GATEWAY_CONTROLLER_HPP
