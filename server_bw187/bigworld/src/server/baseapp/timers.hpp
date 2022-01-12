/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef TIMERS_HPP
#define TIMERS_HPP

#include <map>

#include "cstdmf/time_queue.hpp"

class BinaryIStream;
class BinaryOStream;

/**
 *	This class stores a collection of timers that have an associated script id.
 */
class Timers
{
public:
	typedef int32 ScriptID;

	ScriptID addTimer( float initialOffset, float repeatOffset, int userArg,
			TimeQueueHandler * pHandler );
	bool delTimer( ScriptID timerID );

	void releaseTimer( TimeQueueId handle );

	void cancelAll();

	void writeToStream( BinaryOStream & stream ) const;
	void readFromStream( BinaryIStream & stream,
			uint32 numTimers, TimeQueueHandler * pHandler );

	ScriptID getIDForHandle( TimeQueueId handle ) const;

	bool isEmpty() const	{ return map_.empty(); }

private:
	typedef std::map< ScriptID, TimeQueueId > Map;

	ScriptID getNewID();
	Map::const_iterator findTimer( TimeQueueId handle ) const;
	Map::iterator findTimer( TimeQueueId handle );

	Map map_;
};


/**
 *	This namespace contains helper methods so that Timers can be a pointer
 *	instead of an instance. These functions handle the creation and destruction
 *	of the object.
 */
namespace TimersUtil
{
	Timers::ScriptID addTimer( Timers ** ppTimers,
		float initialOffset, float repeatOffset, int userArg,
		TimeQueueHandler * pHandler );
	bool delTimer( Timers * pTimers, Timers::ScriptID timerID );

	void releaseTimer( Timers ** ppTimers, TimeQueueId handle );

	void cancelAll( Timers * pTimers );

	void writeToStream( Timers * pTimers, BinaryOStream & stream );
	void readFromStream( Timers ** ppTimers, BinaryIStream & stream,
			TimeQueueHandler * pHandler );

	Timers::ScriptID getIDForHandle( Timers * pTimers, TimeQueueId handle );
}

#endif // TIMERS_HPP
