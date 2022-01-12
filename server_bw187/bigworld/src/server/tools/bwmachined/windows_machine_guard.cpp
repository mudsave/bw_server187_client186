/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "windows_machine_guard.hpp"
#include <iphlpapi.h>
#include <psapi.h>
#include <sstream>

typedef BOOL (WINAPI *LPGETSYSTEMTIMES)(LPFILETIME,LPFILETIME,LPFILETIME);
LPGETSYSTEMTIMES imported_GetSystemTimes;

// TODO: don't hardcode this!
const char * machinedConfFile = "c:\\program files\\bigworld\\bwmachined.conf";
const char * svcName;

void openlog( const char * ident, int option, int facility )
{
	svcName = ident;
}

void syslog( int priority, const char * format, ... )
{
//	va_list ap;
//	va_start( ap, format );
//	vprintf( format, ap );
//	va_end( ap );
    HANDLE h;

    h = RegisterEventSource(NULL,  // uses local computer
             svcName);    // source name
    if (h == NULL)
        return;

	va_list ap;
	va_start( ap, format );
	int msgSize = _vscprintf( format, ap ) + 1;
	char * msg = new char[msgSize];
	va_end( ap );
	va_start( ap, format );
	_vsnprintf( msg, msgSize, format, ap );
	va_end( ap );

	puts(msg);

	OutputDebugString( msg );

    if (!ReportEvent(h,           // event log handle
            EVENTLOG_ERROR_TYPE,  // event type
            0,                    // category zero
            priority,			  // event identifier
            NULL,                 // no user security identifier
            1,                    // one substitution string
            0,                    // no data
            (LPCSTR*)&msg,	      // pointer to string array
            NULL))                // pointer to data
	{
		// TODO: what to do??
		// we couldn't write an error to the event log
		// currently we silently ignore this, but it may be better
		// to abort here because something is really wrong
	}

    DeregisterEventSource(h);
	delete[] msg;
}

// print the last windows error
static void reportLastError()
{
	LPVOID lpMsgBuf;
	FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR) &lpMsgBuf,
		0,
		NULL
	);
	// Process any inserts in lpMsgBuf.
	// ...
	// Display the string.
	syslog( LOG_ERR, "Windows error: %s\n", (LPCTSTR)lpMsgBuf );
	// Free the buffer.
	LocalFree( lpMsgBuf );
}

ThingData::ThingData()
{
	hProcess = (HANDLE) 0;
	hStopEvent = (HANDLE) 0;
}

void ThingData::init( const MachineGuardMessage& mgm )
{
	// NOTE: this machine guard messages uses a short for the pid - windows really wants a DWORD,
	// so this could fail...
	if ( mgm.pid )
	{
		this->hProcess = OpenProcess( SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, mgm.pid );
		if ( !this->hProcess )
		{
			reportLastError();
			this->hStopEvent = 0;
			return;
		}
		TCHAR szEvent[32];
		wsprintf( szEvent, TEXT("machined-proc%d"), mgm.pid );
		this->hStopEvent = OpenEvent( EVENT_MODIFY_STATE, FALSE, szEvent );
	}
}

void ThingData::cleanup()
{
	if ( this->hProcess )
		CloseHandle( this->hProcess );
	if ( this->hStopEvent )
		CloseHandle( this->hStopEvent );
}

void initProcessState( int argc, char ** argv )
{
	HMODULE kernel = GetModuleHandle( "kernel32.dll" );
	if ( kernel )
	{
		imported_GetSystemTimes = (LPGETSYSTEMTIMES) GetProcAddress( kernel, "GetSystemTimes" );
	}
}

void cleanupProcessState()
{
}

void updateCurMachineStats( struct ThingData& td )
{
	static uint64 lastIdleTime, lastKernelTime, lastUserTime;

	// if this ever breaks, then we might want to use NtQuerySystemInformation instead
	if ( imported_GetSystemTimes )
	{
		FILETIME idleFileTime, kernelFileTime, userFileTime;
		imported_GetSystemTimes( &idleFileTime, &kernelFileTime, &userFileTime );

		uint64 idleTime = *(uint64*)&idleFileTime;
		uint64 kernelTime = *(uint64*)&kernelFileTime;
		uint64 userTime = *(uint64*)&userFileTime;

		// kernel time includes idle time (double counted by GetSystemTimes)

		gCurMaxCPU = (kernelTime + userTime) / gCPUCount;
		td.curCPU = gCurMaxCPU - idleTime / gCPUCount;
	}

	// memory
	MEMORYSTATUS mem;
	GlobalMemoryStatus( &mem );
	gCurMaxMem = mem.dwTotalPhys;
	td.curMem = mem.dwAvailPhys;

	// mr network
	MIB_IPSTATS ipstats;
	GetIpStatistics( &ipstats );
	gCurNetTotOut = ipstats.dwOutRequests;
	gCurNetDropOut = ipstats.dwOutDiscards;
	gCurNetTotIn = ipstats.dwInReceives;
	gCurNetDropIn = ipstats.dwInDiscards;
}

bool updateProcessStats(ThingData & td)
{
	td.oldCPU = td.curCPU;
	td.oldMem = td.curMem;

	// check to see if the process is dead
	if ( WAIT_OBJECT_0 == WaitForSingleObject( td.hProcess, 0 ) )
	{
		return false;
	}

	FILETIME createFileTime, exitFileTime, kernelFileTime, userFileTime;
	BOOL r = GetProcessTimes( td.hProcess,
		&createFileTime, &exitFileTime,
		&kernelFileTime, &userFileTime );

	if ( r )
	{
		uint64 kernelTime = *(uint64*)&kernelFileTime;
		uint64 userTime = *(uint64*)&userFileTime;

		td.curCPU = kernelTime + userTime;
	}
	else
	{
		reportLastError();
	}

	PROCESS_MEMORY_COUNTERS procMem;
	ZeroMemory( &procMem, sizeof(procMem) );
	procMem.cb = sizeof(procMem);
	GetProcessMemoryInfo( td.hProcess, &procMem, sizeof(procMem) );
	td.curMem = procMem.WorkingSetSize;

	return true;
}

void getProcessorSpeeds( std::vector<float>& speeds )
{
	HKEY hkey = NULL;
	if ( ERROR_SUCCESS == RegOpenKeyEx( HKEY_LOCAL_MACHINE, TEXT("HARDWARE\\DESCRIPTION\\System\\CentralProcessor"), NULL, KEY_READ, &hkey ) &&
		hkey )
	{
		DWORD ndx = 0;
		TCHAR szCPU[8];
		DWORD lenCPU = sizeof(szCPU) / sizeof(szCPU[0]);
		FILETIME ft;

		while( ERROR_SUCCESS == RegEnumKeyEx( hkey, ndx, szCPU, &lenCPU, NULL, NULL, NULL, &ft ) )
		{
			HKEY hkeyCPU = NULL;
			if ( ERROR_SUCCESS == RegOpenKeyEx( hkey, szCPU, NULL, KEY_QUERY_VALUE, &hkeyCPU ) &&
				hkeyCPU )
			{
				DWORD dwMHz = 0;
				DWORD cbMHz = sizeof(dwMHz);
				DWORD dwType = 0;
				if ( ERROR_SUCCESS == RegQueryValueEx( hkeyCPU, TEXT("~MHz"), NULL, &dwType, (LPBYTE)&dwMHz, &cbMHz ) )
				{
					speeds.push_back( (float)dwMHz );
				}

				RegCloseKey( hkeyCPU );
				ndx++;
			}
		}
		RegCloseKey( hkey );
	}
}

typedef std::map<std::string,std::string> EnvironMap;
// read the current environment into a map
static void readEnviron( EnvironMap& m )
{
	for ( int i=0; _environ[i]; i++ )
	{
		std::string var = _environ[i];
		std::string::size_type eqPos = var.find('=');
		std::string varName = var.substr( 0, eqPos );
		std::string varValue = var.substr( eqPos+1 );
		m[varName] = varValue;
	}
}

// turn an EnvironMap into a character array suitable for CreateProcess
// must be later delete[]'d
static char * makeEnvironStr( const EnvironMap& m )
{
	// calculate the length of the array required to store everything
	unsigned length = 0;
	for ( EnvironMap::const_iterator i = m.begin(); i != m.end(); ++i )
	{
		// variable name
		length += i->first.length();
		// equals sign
		length ++;
		// variable value
		length += i->second.length();
		// null terminator
		length ++;
	}
	// final null terminator
	length ++;

	char * result = new char[length];

	char * resultPos = result;
	for ( EnvironMap::const_iterator i = m.begin(); i != m.end(); ++i )
	{
		memcpy( resultPos, i->first.c_str(), i->first.length() );
		resultPos += i->first.length();
		*resultPos++ = '=';
		memcpy( resultPos, i->second.c_str(), i->second.length() );
		resultPos += i->second.length();
		*resultPos++ = 0;
	}
	*resultPos++ = 0;
	MF_ASSERT(resultPos == result+length);

	return result;
}

void startProcess( const char * mfRoot,
				   const char * bwResPath,
				   const char * config,
				   const char * type,
				   int argc,
				   char ** argv,
				   const MachineGuardMessage& mgm
				   )
{
	char path[512];

	strcpy( path, mfRoot );
	strcat( path, "\\bigworld\\bin\\server_" );
	strcat( path, config );
	strcat( path, "\\" );
	strncat( path, type, 32 );
	strcat( path, ".exe" );

	TCHAR szArgs[MAX_PATH] = TEXT("");
	int len = 0;
	for ( int ndx = 1; ndx < argc; ndx++ )
	{
		len += wsprintf( szArgs+len, TEXT( " \"%s\"" ), argv[ndx] );
	}

	// this whole copying of the environment variables thing is required because
	// we need to set some variables per-userid, and because the WinSock API
	// requires some environment variables to be set... still not sure which ones
	// despite quite a bit of googling

	// copy the whole environment into a map
	EnvironMap envMap;
	readEnviron(envMap);
	// patch it with whatever we need to pass down
	envMap["MF_ROOT"] = mfRoot;
	envMap["BW_RES_PATH"] = bwResPath;
	std::ostringstream uidToString;
	uidToString << mgm.uid;
	envMap["UID"] = uidToString.str();
	// and create the null delimited, null terminated environment string
	// from that
	char * envStr = makeEnvironStr(envMap);

	TCHAR szApp[MAX_PATH];
	lstrcpy( szApp, path );

	strcpy(path, mfRoot);
	strcat(path, "\\bigworld\\bin\\server_" );
	strcat(path, config );

	STARTUPINFO si;
	ZeroMemory( &si, sizeof(si) );
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi;
	syslog( LOG_INFO, "starting: '%s'\nwith args: '%s'\nand working path: '%s'\n", szApp, szArgs, path );
	if ( CreateProcess( szApp, szArgs, NULL, NULL, TRUE,
		NORMAL_PRIORITY_CLASS |	CREATE_NO_WINDOW, envStr, path, &si, &pi ) )
	{
		CloseHandle( pi.hThread );
		CloseHandle( pi.hProcess );
	}
	else
	{
		reportLastError();
	}

	delete[] envStr;
}
