/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "linux_machine_guard.hpp"
#include "bwmachined.hpp"
#include <glob.h>
#include <libgen.h>

bool getProcessTimes( FILE *f, unsigned long int *utimePtr,
	unsigned long int *stimePtr, unsigned long int *vsizePtr,
	unsigned long int *starttimePtr, int *cpuPtr );

const char * machinedConfFile = "/etc/bwmachined.conf";


void sigChildHandler( int )
{
	// Clean up any child zombies.
	waitpid( -1, NULL, WNOHANG );
}

void initProcessState( bool isDaemon )
{
	if (isDaemon)
		daemon( 1, 0 );
	else
		printf( "Not running as daemon\n" );

	// Handle sigchld so we know when our children stop.
	signal( SIGCHLD, sigChildHandler );
}

void ProcessInfo::init( const ProcessMessage &pm ) {}

void cleanupProcessState()
{
}

bool updateSystemInfoP( struct SystemInfo &si )
{
	char line[ 512 ];
	FILE *af;

	// CPU updates
	int	cpu;
	// These sizes were obtained from Linux source code (fs/proc/stat.c)
	unsigned long long jiffiUser, jiffiNice, jiffiSyst, jiffiIdle;

	if ((af = fopen( "/proc/stat", "r" )) == NULL)
	{
		syslog( LOG_ERR, "Couldn't read /proc/stat: %s", strerror( errno ) );
		return false;
	}

	// Line CPU load summary line
	fgets( line, sizeof( line ), af );

	// Read each CPU load individually
	for (int i=0; i < si.nCpus; i++)
	{
		fgets( line, sizeof( line ), af );
		if (sscanf( line, "cpu%d %llu %llu %llu %llu",
				&cpu, &jiffiUser, &jiffiNice, &jiffiSyst, &jiffiIdle) != 5)
		{
			syslog( LOG_ERR, "Invalid line in /proc/stat: '%s'", line );
			break;
		}

		if (cpu != i)
		{
			syslog( LOG_CRIT, "Line %d of /proc/stat was cpu%d, not cpu%d",
				i, cpu, i );
		}

		si.cpu[i].val.update( jiffiUser + jiffiNice + jiffiSyst );
		si.cpu[i].max.update( si.cpu[i].val.cur() + jiffiIdle );
	}
	fclose(af);

	// Memory updates
	if ((af = fopen( "/proc/meminfo", "r" )) == NULL)
	{
		syslog( LOG_ERR, "Couldn't read /proc/meminfo: %s", strerror( errno ) );
		return false;
	}

	unsigned long totalmem, freemem, bufmem, cachemem, slabmem;

	for (int i=0; i < 5 && fgets( line, sizeof( line ), af) != NULL;)
	{
		if (sscanf( line, "MemTotal: %lu", &totalmem ) == 1)
		{
			i++; continue;
		}
		if (sscanf( line, "MemFree: %lu", &freemem ) == 1)
		{
			i++; continue;
		}
		if (sscanf( line, "Buffers: %lu", &bufmem ) == 1)
		{
			i++; continue;
		}
		if (sscanf( line, "Cached: %lu", &cachemem ) == 1)
		{
			i++; continue;
		}
		if (sscanf( line, "Slab: %lu", &slabmem ) == 1)
		{
			i++; continue;
		}
	}

	fclose( af );

	// Magic << 10 is to convert kilobytes -> bytes
	si.mem.max.update( uint64( totalmem ) << 10 );
	si.mem.val.update(
		uint64( totalmem - freemem - bufmem - cachemem - slabmem ) << 10 );

	// IP-level packet statistics
	if ((af = fopen( "/proc/net/snmp", "r" )) == NULL)
	{
		syslog( LOG_ERR, "Couldn't read /proc/net/snmp: %s", strerror( errno ) );
		return false;
	}

	// skip the IP header line
	fgets( line, sizeof( line ), af );
	fscanf( af,"%*s %*d %*d %*d %*d %*d %*d %*d %lld %lld %lld %lld",
		&si.packDropIn.next(), &si.packTotIn.next(),
		&si.packTotOut.next(), &si.packDropOut.next() );
	fclose( af );

	// Interface level packet and bit counts
	if ((af = fopen( "/proc/net/dev", "r" )) == NULL)
	{
		syslog( LOG_ERR, "Couldn't open /proc/net/dev: %s", strerror( errno ) );
		return false;
	}

	// Skip header lines
	fgets( line, sizeof( line ), af ); fgets( line, sizeof( line ), af );
	for (unsigned int i=0; fgets( line, sizeof( line ), af ) != NULL; )
	{
		// If we've already got a struct for this interface, re-use it,
		// otherwise make a new one
		if (i >= si.ifInfo.size())
			si.ifInfo.push_back( InterfaceInfo() );
		struct InterfaceInfo &ifInfo = si.ifInfo[i];
		char ifname[32];

		// Drop info about the loopback interface
		sscanf( line, " %[^:]", ifname );
		if (strstr( ifname, "lo" ) == NULL)
		{
			ifInfo.name = ifname;
			sscanf( line,
				" %*[^:]:%lld %lld %*d %*d %*d %*d %*d %*d "
				"%lld %lld",
				&ifInfo.bitsTotIn.next(),
				&ifInfo.packTotIn.next(),
				&ifInfo.bitsTotOut.next(),
				&ifInfo.packTotOut.next() );

			// Turn byte counts into bit counts
			ifInfo.bitsTotIn.cur() *= 8;
			ifInfo.bitsTotOut.cur() *= 8;
			i++;
		}
	}

	fclose( af );
	return true;
}

bool updateProcessStats( ProcessInfo &pi )
{
	// try to open the file
	char pinfoFilename[ 64 ];
	bw_snprintf( pinfoFilename, sizeof( pinfoFilename ),
		"/proc/%d/stat", (int)pi.m.pid_ );

	FILE *pinfo;
	if ((pinfo = fopen( pinfoFilename, "r" )) == NULL)
	{
		if (errno != ENOENT)
		{
			syslog( LOG_ERR, "Couldn't open %s: %s",
				pinfoFilename, strerror( errno ) );
		}

		return false;
	}

	// Retrieve process stats
	unsigned long int utime, stime, vsize, starttime;
	int cpu;
	if (!getProcessTimes( pinfo, &utime, &stime, &vsize, &starttime, &cpu ))
	{
		syslog( LOG_ERR, "Failed to update process stats for '%s': %s",
				pinfoFilename, strerror( errno ) );
		fclose( pinfo );
		return false;
	}

	if ((pi.starttime) && (pi.starttime != starttime))
	{
		syslog( LOG_ERR, "updateProcessStats: Process %d starttime differs from "
				"last known starttime (old %lu, curr %lu).",
				(int)pi.m.pid_, pi.starttime, starttime );
		fclose( pinfo );
		return false;
	}

	pi.cpu.update( utime + stime );
	pi.mem.update( vsize );
	pi.affinity = cpu;
	pi.starttime = starttime;

	fclose( pinfo );
	return true;
}

void getProcessorSpeeds( std::vector<float>& speeds )
{
	FILE	*cpuf;
	float	mhz;
	char	aline[BUFSIZ],	*look;

	speeds.clear();

	if ((cpuf = fopen( "/proc/cpuinfo", "r" )) == NULL)
	{
		syslog( LOG_ERR, "Couldn't read /proc/cpuinfo: %s", strerror( errno ) );
		return;
	}

	while (fgets(aline,BUFSIZ,cpuf))
	{
		if ((look=strchr(aline,'\n'))) *look=0;
		if ((look=strchr(aline,'\r'))) *look=0;

		if (!aline[0])
		{
			//ncpus++;
		}
		else
		{
			look=strchr(aline,':');	*look++=0;

			if (!strcmp(aline,"cpu MHz\t\t"))
			{
				sscanf(look,"%f",&mhz);
				speeds.push_back( mhz );
			}
		}
	}
	fclose(cpuf);
}


bool getProcessTimes( FILE *f, unsigned long int *utimePtr,
	unsigned long int *stimePtr, unsigned long int *vsizePtr,
	unsigned long int *starttimePtr, int *cpuPtr )
{
	if (fseek( f, SEEK_SET, 0 ) != 0)
	{
		return false;
	}

	// This hideous list of variables and the associated format string
	// originates from  'man 5 proc' under the section '/proc/[number]/stat'
	int	pid;
	char name[ 255 ];
	char state;
	int	ppid, pgrp, session, tty, tpgid;
	unsigned long int flags, minflt, cminflt, majflt, cmajflt, utime, stime;
	long int cutime, cstime, priority, nice, num_threads, itrealvalue;
	unsigned long int starttime, vsize;
	long int rss;
	unsigned long int rlim, startcode, endcode, startstack, kstkesp, kstkeip;
	unsigned long int signal, blocked, sigignore, sigcatch;
	unsigned long int wchan, nswap, cnswap;
	int exit_signal, processor;
	unsigned long rt_priority = 0, policy = 0;

	int returnVal = fscanf( f, "%d %s %c %d %d %d %d %d " // pid, ...
		"%lu %lu %lu %lu %lu %lu %lu " // flags, ....
		"%ld %ld %ld %ld %ld %ld " // cutime, ...
		"%lu %lu %ld %lu %lu %lu %lu %lu %lu " // starttime, ...
		"%lu %lu %lu %lu %lu %lu %lu " // signal, ...
		"%d %d %lu %lu", // exit_signal, ...

		&pid, name, &state,
		&ppid, &pgrp, &session, &tty, &tpgid,
		&flags, &minflt, &cminflt, &majflt, &cmajflt, &utime, &stime,
		&cutime, &cstime, &priority, &nice, &num_threads, &itrealvalue,
		&starttime, &vsize, &rss,
		&rlim, &startcode, &endcode, &startstack, &kstkesp, &kstkeip,
		&signal, &blocked, &sigignore, &sigcatch, &wchan, &nswap, &cnswap,
		&exit_signal, &processor, &rt_priority, &policy );

	static const int minResultsExpected = 39; // 2.6 added rt_priority...
	if ((returnVal < minResultsExpected) || (ferror( f )))
	{
		return false;
	}

	if ( utimePtr ) *utimePtr = utime;
	if ( stimePtr ) *stimePtr = stime;
	if ( vsizePtr ) *vsizePtr = vsize;
	if ( starttimePtr ) *starttimePtr = starttime;
	if ( cpuPtr ) *cpuPtr = processor;

	return true;
}


char* putEnvAlloc(const char* name, const char* value)
{
	int len = strlen(name) + strlen(value) + 2;
	char* str = new char[len];
	strcpy(str, name);
	strcat(str, "=");
	strcat(str, value);
	putenv(str);
	return str;
}


/**
 *  Start a new process on this machine, using the provided configuration.  The
 *  machined object needs to be passed in so that we can close down all its
 *  sockets so the child processes don't hang on to them.
 */
uint16 startProcess( const char * mfRoot,
	const char * bwResPath,
	const char * config,
	const char * type,
	uint16 uid,
	uint16 gid,
	int argc,
	char ** argv,
	BWMachined &machined )
{
	uint16 childpid;

	if ((childpid = fork()) == 0)
	{
		if (setgid( gid ) == -1)
		{
			syslog( LOG_ERR,
				"Failed to setgid() to %d for user %d, group will be root\n",
				gid, uid );
		}

		if (setuid( uid ) == -1)
		{
			syslog( LOG_ERR, "Failed to setuid to %d, aborting exec for %s\n",
				uid, type );
			exit( 1 );
		}

		// figure out the right bin dir
		char path[ 512 ];
		strcpy( path, mfRoot );
		strcat( path, "/bigworld/bin/" );
		strcat( path, config );
		strcat( path, "/" );

		// change to it
		chdir( path );

		// now add the exe name
		strncat( path, type, 32 );
		argv[0] = path;

		// Assemble the --res commandline switch
		argv[ argc++ ] = "--res";
		argv[ argc++ ] = strdup( bwResPath );

		// Close parent sockets
		machined.closeEndpoints();

		// Insert env variable for timing settings
		putEnvAlloc( "BW_TIMING_METHOD", machined.timingMethod() );

		syslog( LOG_INFO, "UID %d execing %s", uid, path );

		argv[ argc ] = NULL;
		int result = execv( path, argv );

		if (result == -1)
		{
			syslog(LOG_ERR, "Failed to exec %s\n", path);
		}

		exit(1);
		return 0;
	}
	else
		return childpid;
}


bool checkProcess( uint16 pid )
{
	char buf[32];
	struct stat statinfo;
	int ret;

	snprintf( buf, sizeof( buf ) - 1, "/proc/%d", pid );

	if ((ret = stat( buf, &statinfo )) == -1 && errno != ENOENT)
	{
		syslog( LOG_ERR, "checkProcess(): stat(%s) failed (%s)",
			buf, strerror( errno ) );
	}

	return ret == 0;
}


bool validateProcessInfo( const ProcessInfo &processInfo )
{
	// try to open the file
	char pinfoFilename[ 64 ];
	bw_snprintf( pinfoFilename, sizeof( pinfoFilename ),
		"/proc/%d/stat", (int)processInfo.m.pid_ );

	FILE *fp;
	if ((fp = fopen( pinfoFilename, "r" )) == NULL)
	{
		if (errno != ENOENT)
		{
			syslog( LOG_ERR, "Couldn't open %s: %s",
				pinfoFilename, strerror( errno ) );
		}

		return false;
	}

	unsigned long int starttime;

	bool status = getProcessTimes( fp, NULL, NULL, NULL, &starttime, NULL );
	fclose( fp );
	
	if ((status) &&
		(processInfo.starttime) && (starttime != processInfo.starttime))
	{
		syslog( LOG_ERR, "validateProcessInfo: Process %d starttime differs "
					"from last known starttime (old %lu, curr %lu).",
					(int)processInfo.m.pid_, processInfo.starttime, starttime );
		return false;
	}

	return status;
}


void checkCoreDumps( const char *mfroot, UserMessage &um )
{
	char path[ 512 ], cwd[ 512 ], assert[ 512 ], subdir[ 512 ];
	struct stat statinfo;
	glob_t matches;
	unsigned i;
	FILE *fp;
	char c;

	bw_snprintf( path, sizeof( path ), "%s/bigworld/bin", mfroot );
	getcwd( cwd, sizeof( cwd ) );
	chdir( path );
	um.coredumps_.clear();

	if (glob( "*/core.*", 0, NULL, &matches ) == 0)
	{
		// Limiting the reporting of core dumps to only 10 to avoid
		// exceeding the MGMPacket::MAX_SIZE.
		size_t MAX_CORES = 10;

		for (i=0; i < std::min( MAX_CORES, matches.gl_pathc ); i++)
		{
			const char *core = matches.gl_pathv[i];
			UserMessage::CoreDump cd;
			cd.filename_ = core;

			// Extract subdirectory that the coredump is in
			strcpy( subdir, core );

			// Use info from assertion if it's there.  Magic 6 is length of the
			// string '/core.'.
			bw_snprintf( assert, sizeof( assert ), "%s/assert.%s.log",
				dirname( subdir ), strstr( core, "/" ) + 6 );

			if ((fp = fopen( assert, "r" )) != NULL)
			{
				while ((c = fgetc( fp )) != EOF)
				{
					cd.assert_.push_back( c );
				}

				fclose( fp );
			}

			// Get timestamp for coredump
			if (stat( core, &statinfo ) == 0)
			{
				cd.time_ = statinfo.st_ctime;
			}
			else
			{
				syslog( LOG_ERR, "Couldn't stat() %s: %s",
					core, strerror( errno ) );
			}

			um.coredumps_.push_back( cd );
		}

		syslog( LOG_ERR, "Found %u coredumps in %s, %u reported.",
				i, path, matches.gl_pathc );
	}

	chdir( cwd );
}

// linux_machine_guard.cpp
