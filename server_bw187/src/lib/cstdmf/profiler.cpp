/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "pch.hpp"

#include "profiler.hpp"

#include "debug.hpp"

DECLARE_DEBUG_COMPONENT2( "CStdMF", 0 )

// -----------------------------------------------------------------------------
// Section Profiler
// -----------------------------------------------------------------------------

Profiler* Profiler::instance_ = NULL;

static const float DAMPING_RATIO = 0.97f;

static void DestroyProfiler()
{
	if ( Profiler* profiler = &Profiler::instanceNoCreate() )
		delete profiler;
}


/**
 *
 */
Profiler::Profiler() :
	threadId_( OurThreadID() ),
	stampsPerMs_( stampsPerSecondD() / 1000.0 ),
	prevTime_( timestamp() ),
	historyFile_( NULL ),
	slotNamesWritten_( false ),
	frameCount_( 0 ),
	numSlots_( 1 ),
	curSlot_( 0 ),
	slotStackPos_( 0 )
{
	memset( slots_, 0, sizeof( slots_ ) );
	slots_[0].name_ = "Unaccounted";
	MF_WATCH( "Profiler/Unaccounted", slots_[0].curTimeMs_, Watcher::WT_READ_ONLY );

	instance_ = this;

	atexit( DestroyProfiler );

#if ENABLE_PROFILER
//	historyFile_ = fopen( "profiler.csv", "w" );
#endif

	this->begin( 0 );
}


/**
 *
 */
Profiler::~Profiler()
{
	while ( slotStackPos_ )
		this->end();

	closeHistory();

	Profiler::instance_ = NULL;
}


/**
 *
 */
void Profiler::tick()
{
	this->end();
	MF_ASSERT( slotStackPos_ == 0 );

	// Update current times for watchers
	for ( int i = 0; i < numSlots_; i++ )
	{
		uint64 slotTime = slots_[i].times_[frameCount_];
		float slotTimeF = ( float )( ( double )slotTime / stampsPerMs_ );
		slots_[i].curTimeMs_ =
			( DAMPING_RATIO * slots_[i].curTimeMs_ ) + ( ( 1.f - DAMPING_RATIO ) * slotTimeF );

		slots_[i].curCount_ = slots_[i].counts_[frameCount_];
	}

	frameCount_++;

	// Flush
	if ( frameCount_ == NUM_FRAMES )
	{
		this->flushHistory();
		frameCount_ = 0;
	}

	// Start next frame
	this->begin( 0 );
}


/**
 *
 */
int Profiler::declareSlot( const char* name )
{
#if ENABLE_PROFILER
	slotNamesWritten_ = false;
	slots_[numSlots_].name_ = name;

	std::string watcherName( std::string( "Profiler/" ) + std::string( name ) );
	MF_WATCH( watcherName.c_str(), slots_[numSlots_].curTimeMs_, Watcher::WT_READ_ONLY );

	std::string watcherName2( std::string( "Profiler Count/" ) + std::string( name ) );
	MF_WATCH( watcherName2.c_str(), slots_[numSlots_].curCount_, Watcher::WT_READ_ONLY );

	return numSlots_++;
#else
	return 0;
#endif
}


/**
 *
 */
void Profiler::setNewHistory( const char* historyFileName )
{
#if ENABLE_PROFILER
	closeHistory();

	historyFile_ = fopen( historyFileName, "w" );
	slotNamesWritten_ = false;
	frameCount_ = 0;
#endif
} 
 	
/** 
* 
*/ 
void Profiler::closeHistory() 
{ 
	if ( historyFile_ ) 
	{ 
		flushHistory(); 
		fclose( historyFile_ ); 
		historyFile_ = NULL; 
	}
}


/**
 *
 */
void Profiler::flushHistory()
{
	// Save
	if ( historyFile_ )
	{
		// Save slot names (twice - for times and counts, skipping unaccounted for counts)
		if ( !slotNamesWritten_ )
		{
			fprintf( historyFile_, "Total," );

			for ( int i = 0; i < numSlots_; i++ )
				fprintf( historyFile_, "%s,", slots_[i].name_ );

			for ( int i = 1; i < numSlots_; i++ )
				fprintf( historyFile_, "%s,", slots_[i].name_ );

			fprintf( historyFile_, "\n" );

			slotNamesWritten_ = true;
		}

		// Calculate total for each frame
		uint64 frameTotals[NUM_FRAMES];
		for ( int i = 0; i < frameCount_; i++ )
		{
			frameTotals[i] = 0;
			for ( int j = 0; j < numSlots_; j++ )
				frameTotals[i] += slots_[j].times_[i];
		}

		// Save each frame
		for ( int i = 0; i < frameCount_; i++ )
		{
			float frameTotal = ( float )( ( double )frameTotals[i] / stampsPerMs_ );
			fprintf( historyFile_, "%f,", frameTotal );

			for ( int j = 0; j < numSlots_; j++ )
			{
				uint64 slotTime = slots_[j].times_[i];
				float slotTimeF = ( float )( ( double )slotTime / stampsPerMs_ );
				fprintf( historyFile_, "%f,", slotTimeF );
			}

			for ( int j = 1; j < numSlots_; j++ )
			{
				fprintf( historyFile_, "%d,", slots_[j].counts_[i] );
			}

			fprintf( historyFile_, "\n" );
		}
	}

	// Clear
	for ( int i = 0; i < frameCount_; i++ )
	{
		for ( int j = 0; j < numSlots_; j++ )
		{
			slots_[j].counts_[i] = 0;
			slots_[j].times_[i] = 0;
		}
	}
}



// profiler.cpp
