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
#include "app.hpp"
#include "profiler_app.hpp"

#include "cstdmf/profiler.hpp"

static const float DAMPING_RATIO = 0.97f;

Matrix * g_profilerAppMatrix;

// -----------------------------------------------------------------------------
// Section ProfilerApp
// -----------------------------------------------------------------------------

ProfilerApp ProfilerApp::instance;
int ProfilerApp_token;


ProfilerApp::ProfilerApp() :
	cpuStall_( 0.f ),
	dTime_( 0.f ),
	filteredDTime_( 0.f ),
	fps_( 0.f ),
	filteredFps_( 0.f )
{
	MainLoopTasks::root().add( this, "Profiler/App", NULL );
}


ProfilerApp::~ProfilerApp()
{
	/*MainLoopTasks::root().del( this, "Profiler/App" );*/
}


bool ProfilerApp::init()
{
	// Set up watches
	MF_WATCH( "Render/Performance/FPS(filtered)",
		filteredFps_,
		Watcher::WT_READ_ONLY,
		"Filtered FPS." );
	MF_WATCH( "Render/Performance/Frame Time (filtered)",
		filteredDTime_,
		Watcher::WT_READ_ONLY,
		"Filtered frame time." );
	MF_WATCH( "Render/Performance/CPU Stall",
		cpuStall_,
		Watcher::WT_READ_WRITE,
		"Stall CPU by this much each frame to see if we are GPU bound." );

	return true;
}


void ProfilerApp::fini()
{
}


void ProfilerApp::tick( float dTime )
{
	// Gather stats
	dTime_ = dTime;
	filteredDTime_ = ( DAMPING_RATIO * filteredDTime_ ) + ( ( 1.f - DAMPING_RATIO ) * dTime );

	double cpuFreq = stampsPerSecondD() / 1000.0;
	if (cpuStall_ > 0.f )
	{
		uint64 cur = timestamp();
		uint64 end = cur + ( uint64 )( cpuStall_ * cpuFreq );
		do
		{
			cur = timestamp();
		} while ( cur < end );
	}

	if ( dTime > 0.f )
	{
		fps_ = 1.f / dTime;
		filteredFps_ = ( DAMPING_RATIO * filteredFps_ ) + ( ( 1.f - DAMPING_RATIO ) * 1.f / dTime );
	}

	// Update profiler and camera
	updateProfiler( dTime );
	updateCamera( dTime );
}


void ProfilerApp::draw()
{
}


// -----------------------------------------------------------------------------
// Section Profile
// -----------------------------------------------------------------------------

static bool s_resetFlyThrough = false;
static double s_soakTime;
static char s_csvFilename[256];
static FILE * s_csvFile = NULL;

static std::vector<float> s_frameTimes;
static float s_frameTimesTotal = 0.0f;

static bool s_fixedFrameTime = false;
static bool s_soakTest = false;
static bool s_loop = false;
static float s_speed = 20.f;
static bool s_queueProfile = false;
static int s_state = -1;

static const float FRAME_STEP = 0.1f;
static const float CAMERA_HEIGHT = 0.0f;
static const double SOAK_SETTLE_TIME = 60.0 * 20.0;
static const double VALIDATE_TIME = 5.0;

static int  s_profilerNumRuns = 2;
static char s_profilerOutputRoot[MAX_PATH];
static bool s_profilerExitOnComplete = true;

static Vector3 s_cameraPos [] = 
{
	Vector3( -82.734970f, 47.802773f, 71.479538f),
	Vector3( -35.230576f, 53.896412f, 84.019836f),
	Vector3( -1.356117f, 38.819645f, 34.065331f),
	Vector3( 42.706276f, 31.154270f, -132.307663f),
	Vector3( 89.779572f, 10.867001f, -284.702942f),
	Vector3( -60.384830f, 50.473885f, -255.011780f),
	Vector3( -111.513870f, 10.652634f, -191.930984f),
	Vector3( -234.993546f, 12.647499f, -184.414856f),
	Vector3( -241.871704f, 7.234774f, -33.920364f),
	Vector3( -249.873505f, 11.690100f, 20.385220f),
	Vector3( -233.471161f, 64.460892f, 34.556984f)
};

static Vector3 s_cameraRot [] = 
{
	Vector3( 1.962280f, 1.050426f, 0.343918f),
	Vector3( -2.601650f, 0.657083f, 0.350936f),
	Vector3( -2.919692f, 0.570586f, 0.111908f),
	Vector3( 2.915798f, 0.090967f, 0.039708f),
	Vector3( -1.483856f, -0.266379f, 0.131482f),
	Vector3( 3.106723f, 0.487893f, 0.006632f),
	Vector3( -1.576473f, 0.006836f, -0.012079f),
	Vector3( -0.006018f, 0.078081f, -0.153775f),
	Vector3( 0.000557f, -0.035240f, -0.031621f),
	Vector3( 1.072926f, -0.192336f, 0.271930f),
	Vector3( 0.840562f, 0.169282f, 0.109709f)
};


/**
 *
 */
static bool setupFlyThrough( bool fixedFrameTime, bool soakTest, bool loop )
{
	s_fixedFrameTime = fixedFrameTime;
	s_soakTest = soakTest;
	s_loop = loop;
	return true;
}

/**
 *
 */
static void runFlyThrough( bool loop )
{
	if (setupFlyThrough( false, false, loop ))
	{
		s_resetFlyThrough = true;
	}
}


/**
 *
 */
static void queueProfile( bool fixedFrameTime, bool soakTest )
{
	if (setupFlyThrough( fixedFrameTime, soakTest, false ))
	{
		s_queueProfile = true;
	}
}

static void calculatePerformance( float & frameTimesAvg, float & frameTimesStdDev )
{
	frameTimesAvg = s_frameTimesTotal / float( s_frameTimes.size() );
	frameTimesStdDev = 0.0f;

	for ( uint i = 0; i < s_frameTimes.size(); i++ )
	{
		float dev = s_frameTimes[i] - frameTimesAvg;
		frameTimesStdDev += dev * dev;
	}

	frameTimesStdDev = sqrt( frameTimesStdDev / float( s_frameTimes.size() ) );
}

static void reportCSVStats(FILE * file, double elapsedTime)
{
	// Calculate performance stats
	float frameTimesAvg;
	float frameTimesStdDev;

	calculatePerformance( frameTimesAvg, frameTimesStdDev );

	// Print stats for that block
	fprintf( file, "%g, %d, %f, %f, %f, %f\n", elapsedTime,
		s_frameTimes.size(), s_frameTimesTotal * 1000.0f,
		frameTimesAvg * 1000.0f, frameTimesStdDev * 1000.0f, 1.0f / frameTimesAvg );
}


/**
 *
 */
void ProfilerApp::updateProfiler( float dTime )
{
	static time_t startTime;
	static double prevValidateTime;
	static bool settled = false;
	static int numSoakFrames;

	if ( s_queueProfile )
	{
		s_state = 0;
		time( &startTime );
		numSoakFrames = 0;
		
		if(s_soakTest)
		{
			s_csvFile = fopen(s_csvFilename, "w");
			if(s_csvFile)
			{
				fprintf( s_csvFile,
					"Time Stamp, Num Frames, Time, Frame Time Avg, Frame Time Std Dev, FPS Avg\n" );
				DEBUG_MSG("created csv file: %s.\n", s_csvFilename);
			}
			else
				DEBUG_MSG("could not create csv file: %s.\n", s_csvFilename);
		}
		s_queueProfile = false;
	}

	if ( s_state >= 0 )
	{
		time_t curTime;
		time( &curTime );
		double elapsedTime = difftime( curTime, startTime );
		numSoakFrames++;

		// Store frame time
		s_frameTimes.push_back( dTime );
		s_frameTimesTotal += dTime;

		if ( s_soakTest )
		{
			// Process soak test update
			if ( ( elapsedTime - prevValidateTime ) > VALIDATE_TIME )
			{
				// report stats to csv file
				if(s_csvFile)
					reportCSVStats( s_csvFile, elapsedTime );

				// Reset frame times
				s_frameTimes.clear();
				s_frameTimesTotal = 0.0f;

				prevValidateTime = elapsedTime;

				if ( elapsedTime >= s_soakTime )
				{
					if (s_csvFile)
					{
						fclose(s_csvFile);
						s_csvFile = NULL;
					}

					float secondsPerFrame = ( float )elapsedTime / ( float )numSoakFrames;
					float framesPerSecond = 1.0f / secondsPerFrame;

					DEBUG_MSG( "Soak Test Done\n" );
					DEBUG_MSG( "%d frames, averaging %f ms per frame (%f frames per second)\n",
						numSoakFrames, 1000.0f * secondsPerFrame, framesPerSecond );

					App::instance().quit();
					return;
				}
			}

			if ( !g_profilerAppMatrix )
			{
				s_resetFlyThrough = true;
				prevValidateTime = elapsedTime;
			}
		}
		else
		{
			// Process profiler update
			if ( !g_profilerAppMatrix )
			{
				// If we've done a run, report
				if ( s_state > 0 )
				{
					float frameTimesAvg;
					float frameTimesStdDev;

					calculatePerformance( frameTimesAvg, frameTimesStdDev );

					DEBUG_MSG( "Profile run %d done\n", s_state );
					DEBUG_MSG( "%d frames, averaging %f ms per frame (%f frames per second)\n",
						s_frameTimes.size(), 1000.0f * frameTimesAvg, 1.0f / frameTimesAvg );
					DEBUG_MSG( "Standard Deviation: %f\n", 1000.0f * frameTimesStdDev );
				}

				// If we've done the last run quit out
				if ( s_state == s_profilerNumRuns )
				{
					s_state = -1;
					Profiler::instance().flushHistory();
					if (s_profilerExitOnComplete)
					{
						App::instance().quit();
					}
					return;
				}

				// Advance to the next run
				char historyFileName[MAX_PATH];
				sprintf( historyFileName, "%s_%d.csv", s_profilerOutputRoot, s_state );
				Profiler::instance().setNewHistory( historyFileName );
				s_resetFlyThrough = true;
				time( &startTime );
				numSoakFrames = 0;
				s_frameTimes.clear();
				s_frameTimesTotal = 0.0f;
				s_state++;
			}
		}
	}
}

/**
 * A helper function to clamp rotation to the acute angles between 2 angles.
 */
void ProfilerApp::clampAngles( const Vector3& start, Vector3& end )
{
	Vector3 rot = end - start;
	if (rot[0] <= -MATH_PI) end[0] += MATH_2PI; else if (rot[0] > MATH_PI) end[0] -= MATH_2PI;
	if (rot[1] <= -MATH_PI) end[1] += MATH_2PI; else if (rot[1] > MATH_PI) end[1] -= MATH_2PI;
	if (rot[2] <= -MATH_PI) end[2] += MATH_2PI; else if (rot[2] > MATH_PI) end[2] -= MATH_2PI;
}

/**
 *
 */
void ProfilerApp::updateCamera( float dTime )
{
	static std::vector< Vector3 > cameraPos;
	static std::vector< Vector3 > cameraRot;
	static uint destId;

	static Matrix transform;
	static float t, dt;
	static Vector3 p0, p1, p2, p3;
	static Vector3 r0, r1, r2, r3;

	if ( s_resetFlyThrough )
	{
		s_resetFlyThrough = false;

		
		// Follow the links
		cameraPos.clear();
		cameraRot.clear();
		
		for( int i = 0; i < sizeof(s_cameraPos)/sizeof(Vector3); i++ ) 
		{
			cameraPos.push_back( s_cameraPos[i] );
			cameraRot.push_back( s_cameraRot[i] );
		}  

		if ( s_soakTest || s_loop )
		{
			//If we are looping then we need to copy the first 3 camera nodes to the end (required for the spline)
			for (unsigned i=0; i<3; i++)
			{
				if ( cameraPos.size() > i )
				{
					cameraPos.push_back( cameraPos[i] );
					cameraRot.push_back( cameraRot[i] );
				}
			}
		}

		// Set state to begin if we have four or more camera nodes (required for the spline)
		if ( cameraPos.size() >= 4 )
		{
			g_profilerAppMatrix = &transform;
			destId = 1;
			t = 1.f; // This will make sure the co-efficients will update on the first iteration
		}
	}

	if ( !g_profilerAppMatrix )
		return;

	float timeScale = s_fixedFrameTime ? FRAME_STEP : dTime;
	t += timeScale * dt;

	if (t >= 1.f)
	{
		
		destId++;

		if ( destId >= cameraPos.size() - 1 )
		{
			if ( s_soakTest || s_loop )
			{
				destId = 2;
			}
			else
			{
				g_profilerAppMatrix = NULL;
				return;
			}
		}

		//Get the interpolation samples
		p0 = cameraPos[ destId - 2 ];
		p1 = cameraPos[ destId - 1 ];
		p2 = cameraPos[ destId + 0 ];
		p3 = cameraPos[ destId + 1 ];

		r0 = cameraRot[ destId - 2 ];
		r1 = cameraRot[ destId - 1 ];
		r2 = cameraRot[ destId + 0 ];
		r3 = cameraRot[ destId + 1 ];

		//Make sure the rotations only result in acute turns
		clampAngles( r0, r1 );
		clampAngles( r1, r2 );
		clampAngles( r2, r3 );

		//We need to blend the time increment to avoid large speed changes
		dt = s_speed / ((p0 - p1).length() +
					2.f * (p1 - p2).length() +
					(p2 - p3).length());

		t -= 1.f;
	}

	//Interpolate the position and rotation using a Catmull-Rom spline
	float t2 = t * t;
	float t3 = t * t2;
	Vector3 pos =	0.5 *((2 * p1) +
					(-p0 + p2) * t +
					(2*p0 - 5*p1 + 4*p2 - p3) * t2 +
					(-p0 + 3*p1- 3*p2 + p3) * t3 );

	Vector3 rot =	0.5 *((2 * r1) +
					(-r0 + r2) * t +
					(2*r0 - 5*r1 + 4*r2 - r3) * t2 +
					(-r0 + 3*r1- 3*r2 + r3) * t3 );

	//Work out the direction and up vector
	Matrix m = Matrix::identity;
	m.preRotateY( rot[0] );
	m.preRotateX( rot[1] );
	m.preRotateZ( rot[2] );

	Vector3 dir = m.applyToUnitAxisVector( Z_AXIS );
	Vector3 up = m.applyToUnitAxisVector( Y_AXIS );

	//Update the transform
	transform.lookAt( pos, dir, up );
}


// -----------------------------------------------------------------------------
// Section Python module functions
// -----------------------------------------------------------------------------

/**
 *
 */
PyObject* py_setProfilerHistory( PyObject* args )
{
	if ( PyTuple_Size( args ) != 1 )
	{
		Py_Return;
	}

	const char* historyFileName = PyString_AsString( PyTuple_GetItem( args, 0 ) );

	Profiler::instance().setNewHistory( historyFileName );

	Py_Return;
}
PY_MODULE_FUNCTION( setProfilerHistory, BigWorld )


/**
 *
 */
PyObject* py_closeProfilerHistory( PyObject* args )
{
	Profiler::instance().closeHistory();
	Py_Return;
}
PY_MODULE_FUNCTION( closeProfilerHistory, BigWorld )

/**
 *
 */
PyObject* py_runFlyThrough( PyObject* args )
{
	s_loop = false;
	s_speed = 20.f;
	if (!PyArg_ParseTuple( args, "s|if", &s_loop, &s_speed ))
	{
		PyErr_SetString( PyExc_TypeError, "BigWorld.runFlyThrough() "
			"expects a string and an optional bool and an optional float" );
		Py_Return;
	}

	runFlyThrough( s_loop );

	Py_Return;
}
PY_MODULE_FUNCTION( runFlyThrough, BigWorld )

/**
 *
 */
PyObject* py_cancelFlyThrough( PyObject* args )
{
	s_state = -1;
	g_profilerAppMatrix = NULL;
	Py_Return;
}
PY_MODULE_FUNCTION( cancelFlyThrough, BigWorld )

/**
 *
 */
PyObject* py_runProfiler( PyObject* args )
{
	s_profilerNumRuns = 2;
	strncpy( s_profilerOutputRoot, "profiler_run", MAX_PATH-1 );
	char * profilerOutputRoot = NULL;
	s_profilerExitOnComplete = true;

	if (!PyArg_ParseTuple( args, "|isi", &s_profilerNumRuns, &profilerOutputRoot, &s_profilerExitOnComplete ))
	{
		PyErr_SetString( PyExc_TypeError, "BigWorld.runProfiler() "
			"expects an optional int, optional string and optional bool" );
		Py_Return;
	}

	if (profilerOutputRoot)
	{
		strncpy( s_profilerOutputRoot, profilerOutputRoot, MAX_PATH-1 );
	}

	queueProfile( true, false );

	Py_Return;
}
PY_MODULE_FUNCTION( runProfiler, BigWorld )

/**
 *
 */
PyObject* py_runSoakTest( PyObject* args )
{
	char * filename = NULL;
	s_soakTime = 24.0 * 60.0;
	s_fixedFrameTime = false;

	if (!PyArg_ParseTuple( args, "|dsi", &s_soakTime, &filename, &s_fixedFrameTime ))
	{
		PyErr_SetString( PyExc_TypeError, "BigWorld.runSoakTest() "
			"expects an optional int and optional string and optional bool" );
		Py_Return;
	}

	// read filename
	memset(s_csvFilename, 0, sizeof(s_csvFilename));
	if(filename)
		strncpy(s_csvFilename, filename, sizeof(s_csvFilename));
	else
		sprintf(s_csvFilename, "soak_%d.csv", (int) time(NULL));

	s_soakTime *= 60.0; // make amount equal minutes

	queueProfile( s_fixedFrameTime, true );

	Py_Return;
}
PY_MODULE_FUNCTION( runSoakTest, BigWorld )


// profiler_app.cpp
