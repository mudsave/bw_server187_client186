/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef TIME_QUEUE_HPP
#define TIME_QUEUE_HPP

#include <algorithm>

template<typename TIMETYPE, typename TYPE = float>
class TimeQueue
{
public:
	typedef TIMETYPE	TimeType;
	typedef TYPE		Type;

	TimeQueue();

	void add( const Type &value );
	void remove( const TimeType &maxTime, bool keepOne );
	const Type find( const TimeType &time, const Type &defaultVal ) const;
	void updateTimes( const TimeType &dTime );

private:
	struct Item
	{
		TIMETYPE	time_;
		TYPE		value_;
		// The array is stored backwards with descending time.
		bool operator<(const Item& other) const { return time_ > other.time_; }
	};

	std::vector<Item> items_;
};

template<typename TIMETYPE, typename TYPE>
INLINE TimeQueue<TIMETYPE, TYPE>::TimeQueue() 
{
}


template<typename TIMETYPE, typename TYPE>
INLINE void TimeQueue<TIMETYPE, TYPE>::add( const Type &value )
{
	Item newItem;
	newItem.time_ = 0;
	newItem.value_ = value;

	items_.push_back( newItem );
}

template<typename TIMETYPE, typename TYPE>
INLINE void TimeQueue<TIMETYPE, TYPE>::remove( const TimeType &maxTime, bool keepOne )
{
	if ( items_.size() == 0 )
		return;
		
	if ( items_[0].time_ < maxTime )
		return;

	std::vector<Item>::iterator endIt = items_.begin();
	for (; endIt != items_.end() && endIt->time_ >= maxTime; ++endIt)
		;

	if (keepOne && endIt == items_.end())
		--endIt;

	items_.erase( items_.begin(), endIt );
}


template<typename TIMETYPE, typename TYPE>
INLINE const typename TimeQueue<TIMETYPE, TYPE>::Type TimeQueue<TIMETYPE, TYPE>::find( const TimeType &time, const Type &defaultVal ) const
{
	if ( items_.size() == 1 )
		return items_[0].value_;	
	
	Item testItem;
	testItem.time_ = time;
	testItem.value_ = defaultVal;

	// Binary Search Implementation
	std::vector<Item>::const_iterator foundItem = std::upper_bound( items_.begin(), items_.end(), testItem);
	
	if (foundItem != items_.end())
	{
		return foundItem->value_;
	}
	return testItem.value_;
}

template<typename TIMETYPE, typename TYPE>
INLINE void TimeQueue<TIMETYPE, TYPE>::updateTimes( const TimeType &dTime ) 
{
	for( std::vector<Item>::iterator it = items_.begin(); it != items_.end(); ++it )
			(*it).time_ += dTime;
}

#endif // TIME_QUEUE_HPP
