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

#include "cvswrapper.hpp"
#include "appmgr/options.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/multi_file_system.hpp"
#include "common/string_utils.hpp"
#include <fstream>


DECLARE_DEBUG_COMPONENT2( "CVSWrapper", 2 );

bool CVSWrapper::isFile( const std::string& pathName )
{
	IFileSystem::FileType fileType = BWResource::instance().fileSystem()->getFileType( pathName );

	return fileType == IFileSystem::FT_FILE;
}

bool CVSWrapper::isDirectory( const std::string& pathName )
{
	IFileSystem::FileType fileType = BWResource::instance().fileSystem()->getFileType( pathName );

	return fileType == IFileSystem::FT_DIRECTORY;
}

bool CVSWrapper::exists( const std::string& pathName )
{
	IFileSystem::FileType fileType = BWResource::instance().fileSystem()->getFileType( pathName );

	return fileType != IFileSystem::FT_NOT_FOUND;
}

static std::string getHKCRValue( const std::string& name )
{
	LONG size;
	if( RegQueryValue( HKEY_CLASSES_ROOT, name.c_str(), NULL, &size ) == ERROR_SUCCESS )
	{
		std::string result( (std::string::size_type)size - 1, ' ' );
		if( RegQueryValue( HKEY_CLASSES_ROOT, name.c_str(), &result[ 0 ], &size ) == ERROR_SUCCESS )
			return result;
	}
	return "";
}

CVSWrapper::CVSWrapper( const std::string& workingPath )
{
	enabled_ = Options::getOptionBool( "CVS/enable", true );
	cvsPath_ = BWResource::resolveFilename( Options::getOptionString( "CVS/path",
		"resources/scripts/cvs_stub.py" ) );
	batchLimit_ = Options::getOptionInt( "CVS/batchLimit", 128 );
	if( !BWResource::fileExists( cvsPath_ ) )
		enabled_ = false;
	if( cvsPath_.rfind( '.' ) != cvsPath_.npos )
	{
		std::string ext = cvsPath_.substr( cvsPath_.rfind( '.' ) );
		std::string type = getHKCRValue( ext );
		if( !type.empty() )
		{
			std::string openCommand = getHKCRValue( type + "\\shell\\open\\command" );
			if( !openCommand.empty() )
			{
				StringUtils::replace( openCommand, "%1", cvsPath_ );
				StringUtils::replace( openCommand, "%*", "" );
				cvsPath_ = openCommand;
			}
		}
	}
	else
		cvsPath_ = '\"' + cvsPath_ + '\"';
	workingPath_ = BWResource::resolveFilename( workingPath );
	if( *workingPath_.rbegin() != '/' )
		workingPath_ += '/';
}

void CVSWrapper::refreshFolder( const std::string& relativePathName )
{
	if( !enabled_ )
		return;

	std::string cmd = cvsPath_ + " refreshfolder ";
	cmd += '\"' + relativePathName + '\"';

	int exitCode;

	if( !exec( cmd, workingPath_, exitCode, output_ ) )
		ERROR_MSG( "Couldn't exec %s\n", cmd.c_str() );
	else
		INFO_MSG( "refresh Done, cvs output:\n%s\n", output_.c_str() );
}

bool CVSWrapper::updateFiles( std::vector<std::string> filesToUpdate )
{
	if( !enabled_ )
		return true;
	bool result = true;
	while( !filesToUpdate.empty() )
	{
		unsigned int limit = batchLimit_;
		std::string cmd = cvsPath_ + " updatefile";
		while( !filesToUpdate.empty() && limit != 0 )
		{
			--limit;
			cmd += " \"" + filesToUpdate.front() + '\"';
			filesToUpdate.erase( filesToUpdate.begin() );
		}

		int exitCode;

		if( !exec( cmd, workingPath_, exitCode, output_ ) )
		{
			ERROR_MSG( "Couldn't exec %s\n", cmd.c_str() );
			result = false;
		}
		else
			INFO_MSG( "Update Done, cvs output:\n%s\n", output_.c_str() );
		if( exitCode != 0 )
			result = false;
	}

	return result;
}

bool CVSWrapper::updateFolder( const std::string& relativePathName )
{
	if( !enabled_ )
		return true;

	std::string cmd = cvsPath_ + " updatefolder \"" + relativePathName + '\"';

	int exitCode;

	if( !exec( cmd, workingPath_, exitCode, output_ ) )
	{
		ERROR_MSG( "Couldn't exec %s\n", cmd.c_str() );
		return false;
	}
	else
		INFO_MSG( "Update Done, cvs output:\n%s\n", output_.c_str() );

	return exitCode == 0;
}

bool CVSWrapper::commitFiles( std::vector<std::string> filesToCommit, const std::string& commitMsg )
{
	if( !enabled_ )
		return true;

	bool result = true;
	while( !filesToCommit.empty() )
	{
		unsigned int limit = batchLimit_;
		int exitCode;
		std::string cmd = cvsPath_ + " commitfile \"" + commitMsg + "\"";

		while( !filesToCommit.empty() && limit != 0 )
		{
			--limit;
			cmd += " \"" + filesToCommit.front() + '\"';
			filesToCommit.erase( filesToCommit.begin() );
		}

		if ( !exec( cmd, workingPath_, exitCode, output_ ) )
		{
			ERROR_MSG( "Couldn't exec %s\n", cmd.c_str() );
			result = false;
		}
		else
			INFO_MSG( "Commit Done, cvs output:\n%s\n", output_.c_str() );
		if( exitCode != 0 )
			result = false;
	}
	return result;
}

bool CVSWrapper::isInCVS( const std::string& relativePathName )
{
	if( !enabled_ )
		return false;

	int exitCode;
	std::string cmd = cvsPath_ + " managed \"" + relativePathName + '\"';
	if ( !exec( cmd, workingPath_, exitCode, output_ ) )
	{
		ERROR_MSG( "Couldn't exec %s\n", cmd.c_str() );
		return false;
	}
	return exitCode == 0;
}

void CVSWrapper::removeFile( const std::string& relativePathName )
{
	if( !enabled_ )
		return;

	int exitCode;
	std::string cmd = cvsPath_ + " removefile \"" + relativePathName + '\"';
	if ( !exec( cmd, workingPath_, exitCode, output_ ) )
		ERROR_MSG( "Couldn't exec %s\n", cmd.c_str() );
}

/**
 * Provided resname isn't in cvs, open CVS/Entries and add the line:
 * 
 * /test.txt/0/dummy timestamp//
 * 
 * or 
 * 
 * /test.txt/0/dummy timestamp/-kb/
 */
bool CVSWrapper::addFolder( std::string relativePathName, const std::string& commitMsg )
{
	if( !enabled_ )
		return true;

	if( !isDirectory( workingPath_ + relativePathName ) )
		return false;

	int exitCode = 0;

	if( !relativePathName.empty() && relativePathName[0] == '/' )
		relativePathName.erase( relativePathName.begin() );
	std::string root = workingPath_;
	while( !relativePathName.empty() )
	{
		if( relativePathName.find( '/' ) != relativePathName.npos )
		{
			std::string prefix = relativePathName.substr( 0, relativePathName.find( '/' ) );
			relativePathName = relativePathName.substr( relativePathName.find( '/' ) + 1 );
			CVSWrapper cvs( root );
			if( !cvs.isInCVS( prefix ) && !cvs.addFolder( prefix, commitMsg ) )
				return false;
			root += prefix + '/';
		}
		else
		{
			CVSWrapper cvs( root );
			if( !cvs.isInCVS( relativePathName ) )
			{
				std::string cmd = cvsPath_ + " addfolder \"" + commitMsg + "\" ";
				cmd += '\"' + relativePathName + '\"';

				if( !exec( cmd, root, exitCode, output_ ) || exitCode != 0 )
				{
					ERROR_MSG( "Couldn't exec %s:\n%s\n", cmd.c_str(), output_.c_str() );
					return false;
				}
			}
			break;
		}
	}
	root += relativePathName + '/';
	WIN32_FIND_DATA findData;
	HANDLE find = FindFirstFile( ( root + "*.*" ).c_str(), &findData );
	if( find != INVALID_HANDLE_VALUE )
	{
		do
		{
			if( !( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) ||
				strcmp( findData.cFileName, "." ) == 0 ||
				strcmp( findData.cFileName, ".." ) == 0 )
				continue;
			if( !CVSWrapper( root ).addFolder( findData.cFileName, commitMsg ) )
				return false;
		}
		while( FindNextFile( find, &findData ) );
		FindClose( find );
	}
	return exitCode == 0;
}

bool CVSWrapper::addFile( std::string relativePathName, bool isBinary )
{
	if( !enabled_ )
		return true;

	int exitCode = 0;

	std::string cmd = cvsPath_;
	if( isBinary )
		cmd += " addbinaryfile ";
	else
		cmd += " addfile ";
	cmd += '\"' + relativePathName + '\"';

	if ( !exec( cmd, workingPath_, exitCode, output_ ) || exitCode != 0 )
	{
		ERROR_MSG( "Couldn't exec %s:\n%s\n", cmd.c_str(), output_.c_str() );
		return false;
	}

	if( relativePathName.find( '*' ) != relativePathName.npos )
	{
		WIN32_FIND_DATA findData;
		HANDLE find = FindFirstFile( ( workingPath_ + "*.*" ).c_str(), &findData );
		if( find != INVALID_HANDLE_VALUE )
		{
			do
			{
				if( !( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) ||
					strcmp( findData.cFileName, "." ) == 0 ||
					strcmp( findData.cFileName, ".." ) == 0 )
					continue;
				if( !CVSWrapper( workingPath_ + findData.cFileName ).addFile( relativePathName, isBinary ) )
					return false;
			}
			while( FindNextFile( find, &findData ) );
			FindClose( find );
		}
	}

	return exitCode == 0;
}

const std::string& CVSWrapper::output() const
{
	return output_;
}

bool CVSWrapper::exec( std::string cmd, std::string workingDir, int& exitCode, std::string& output )
{
	output.clear();

	if( !enabled_ )
		return true;

	CWaitCursor wait;
	DEBUG_MSG( "execing %s in %s\n", cmd.c_str(), workingDir.c_str() );

    STARTUPINFO si;
    PROCESS_INFORMATION pi;
	SECURITY_ATTRIBUTES saAttr; 

	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
	saAttr.bInheritHandle = TRUE; 
	saAttr.lpSecurityDescriptor = NULL; 

    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    ZeroMemory( &pi, sizeof(pi) );

	si.dwFlags = STARTF_USESTDHANDLES;


	HANDLE stdErrRead, stdErrWrite;

	if (!CreatePipe( &stdErrRead, &stdErrWrite, &saAttr, 0 )) 
	{
		ERROR_MSG( "Couldn't create pipe\n" );
		return false;
	}

    si.hStdError = stdErrWrite;
	si.hStdOutput = stdErrWrite;

	if (!CreateProcess( 0, const_cast<char*>( cmd.c_str() ), 0, 0, true, CREATE_NO_WINDOW, 0, workingDir.c_str(), &si, &pi ))
	{
		ERROR_MSG( "Unable to create process %s, last error is %u\n", cmd.c_str(), GetLastError() );
		return false;
	}

	// stdErrWrite is used by the new process now, we don't need it
	CloseHandle( stdErrWrite );

	// Read all the output
	char buffer[1024];
	DWORD amntRead;

	while ( ReadFile( stdErrRead, buffer, 1023, &amntRead, 0 ) != 0 )
	{
		MF_ASSERT( amntRead < 1024 );

		buffer[amntRead] = '\0';
		output += buffer;
	}

    // Wait until child process exits
	if (WaitForSingleObject( pi.hProcess, INFINITE ) == WAIT_FAILED)
	{
		ERROR_MSG( "WaitForSingleObject failed\n" );
		return false;
	}

	// Get the exit code
	if (!GetExitCodeProcess( pi.hProcess, (LPDWORD) &exitCode ))
	{
		ERROR_MSG( "Unable to get exit code\n" );
		return false;
	}

    // Close process and thread handles. 
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );

	return true;
}
