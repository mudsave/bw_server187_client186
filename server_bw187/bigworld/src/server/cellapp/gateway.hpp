/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef GATEWAY_HPP
#define GATEWAY_HPP

#include "entity_extra.hpp"

namespace CellAppInterface
{
	struct gatewaySubscribeArgs;
};

class GatewaySrcController;
class GatewayDstController;

/**
 *	This class manages gateway controllers on entities that have them
 */
class Gateway : public EntityExtra
{
public:
	Gateway( Entity & e );
	~Gateway();

	virtual void relocated();

	void subscribe( const CellAppInterface::gatewaySubscribeArgs & args );

	void addSrc( GatewaySrcController * pController );
	void delSrc( GatewaySrcController * pController );

	void addDst( GatewayDstController * pController );
	void delDst( GatewayDstController * pController );

	void noticeGatewaySrc( GatewaySrcController * pController, bool alive );
	GatewaySrcController * findGatewaySrc( ObjectID gatewayDstID );

	uint32 numSrcs() const	{ return srcs_.size(); }

	void inCellNimbus( Cell & cell, bool isInNimbus );

	static const Instance<Gateway> instance;

private:
	Gateway( const Gateway& );
	Gateway& operator=( const Gateway& );

	typedef std::vector<GatewaySrcController*> GatewaySrcs;
	GatewaySrcs srcs_;

	typedef std::vector<GatewayDstController*> GatewayDsts;
	GatewayDsts dsts_;
};



#endif // GATEWAY_HPP
