/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef STREAMED_DATA_CACHE_HPP
#define STREAMED_DATA_CACHE_HPP

#include "cstdmf/concurrency.hpp"

namespace Moo
{

/**
 *	This class is a whole bunch of data blocks
 */
class StreamedDataCache
{
public:
	StreamedDataCache( const std::string & resourceID, bool createIfAbsent );
	~StreamedDataCache();

	/**
	 * This is the information for each entry in the cache.
	 */
	struct EntryInfo
	{
		uint32	psize_;
		uint32	ssize_;
		uint32	offset_;
		uint32	version_;
		uint64	modified_;
	};

	/**
	 * This class holds the state of a load request.
	 */
	class Tracker
	{
	public:
		Tracker( int offset ) :
			offset_( offset ),
			finished_( 0 )
		{};
		Tracker( int offset, int finished ) :
			offset_( offset ),
			finished_( finished )
		{};
		int offset_;
		int finished_;// 0 = not read, 1 = read ok, -1 = read error
	};

	bool fileAccessDone( Tracker* tracker );

	const EntryInfo * findEntryData( const std::string & name,
		uint32 minVersion, uint64 minModified );
	uint32 addEntryData( const std::string & name,
		uint32 version, uint64 modified );
	void endEntryData( uint32 psize, uint32 ssize );

	bool preload( uint32 offset, uint32 size, void * data );
	bool immsave( uint32 offset, uint32 size, const void * data );

	// anim data looks like:
	// uint32   preload data size (including streamed data sizes)
	// ......   preload data (standard animation)
	// uint32*	streamed data size for each block in the anim
	// ------	(end of preload data)
	// ......	streamed data

	Tracker* load( uint32 offset, uint32 size, void * data );
	Tracker* save( uint32 offset, uint32 size, const void * data );

	bool good();
	uint numEntries()				{ return directory_.size(); }
	void deleteOnClose( bool doc )	{ deleteOnClose_ = doc; }

	void loadSelf();
	void saveSelf();

private:
	bool validateEntryData( uint32 headerStart );

	std::string		fileName_;
	FILE*			file_;

	typedef std::map< std::string, EntryInfo > EntryInfos;
	EntryInfos		directory_;

	EntryInfo *		lastEntry_;
	bool			deleteOnClose_;

};


}	// namespace Moo


#endif // STREAMED_DATA_CACHE_HPP
