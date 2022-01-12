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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <dirent.h>

#include "cstdmf/debug.hpp"
#include "cstdmf/concurrency.hpp"

#include "bwresource.hpp"
#include "unix_file_system.hpp"

DECLARE_DEBUG_COMPONENT2( "ResMgr", 0 )

/**
 *	This is the constructor.
 *
 *	@param basePath		Base path of the filesystem, including trailing '/'
 */
UnixFileSystem::UnixFileSystem(const std::string& basePath) :
	basePath_(basePath)
{
}

/**
 *	This is the destructor.
 */
UnixFileSystem::~UnixFileSystem()
{
}

/**
 *	This method returns the file type of a given path within the file system.
 *
 *	@param path		Path relative to the base of the filesystem.
 *  @param pFI      A pointer to the FileInfo object to fill with the file
 *                  type data.
 *
 *	@return The type of file
 */
IFileSystem::FileType UnixFileSystem::getFileType( const std::string& path,
	FileInfo * pFI )
{
	BWResource::checkAccessFromCallingThread( path,
			"UnixFileSystem::getFileType" );
	struct stat s;

	FileType ft = FT_NOT_FOUND;

	BeginThreadBlockingOperation();
	bool good = stat((basePath_ + path).c_str(), &s) == 0;
	CeaseThreadBlockingOperation();

	if (good)
	{
		if(S_ISREG(s.st_mode))
		{
			ft = FT_FILE;
		}
		if(S_ISDIR(s.st_mode))
		{
			ft = FT_DIRECTORY;
		}

		if (ft != FT_NOT_FOUND && pFI != NULL)
		{
			pFI->size = s.st_size;
			pFI->created = std::min( s.st_ctime, s.st_mtime );
			pFI->modified = s.st_mtime;
			pFI->accessed = s.st_atime;
		}
	}

	return ft;
}

/**
 *	Convert the event time into a CVS/Entries style string
 */
std::string	UnixFileSystem::eventTimeToString( uint64 eventTime )
{
	struct tm etm;
	memset( &etm, 0, sizeof(etm) );

	time_t eventUnixTime = time_t(eventTime);
	gmtime_r( &eventUnixTime, &etm );

	char buf[32];
/*	sprintf( buf, "%s %s %02d %02d:%02d:%02d %d",
		g_daysOfWeek[etm.tm_wday], g_monthsOfYear[etm.tm_mon+1], etm.tm_mday,
		etm.tm_hour, etm.tm_min, etm.tm_sec, 1900 + etm.tm_year );			*/
	asctime_r( &etm, buf );
	if (buf[24] == '\n') buf[24] = 0;
	return buf;
}


/**
 *	This method reads the contents of a directory.
 *
 *	@param path		Path relative to the base of the filesystem.
 *
 *	@return A vector containing all the filenames in the directory.
 */
IFileSystem::Directory UnixFileSystem::readDirectory(const std::string& path)
{
	BWResource::checkAccessFromCallingThread( path,
			"UnixFileSystem::readDirectory" );

	Directory dir;
	DIR* pDir;
	dirent* pEntry;

	BeginThreadBlockingOperation();

	pDir = opendir((basePath_ + path).c_str());
	if (pDir != NULL)
	{
		pEntry = readdir(pDir);

		while(pEntry)
		{
			std::string name = pEntry->d_name;

			if(name != "." && name != "..")
				dir.push_back(pEntry->d_name);

			pEntry = readdir(pDir);
		}

		closedir(pDir);
	}

	CeaseThreadBlockingOperation();

	return dir;
}

/**
 *	This method reads the contents of a file
 *
 *	@param path		Path relative to the base of the filesystem.
 *
 *	@return A BinaryBlock object containing the file data.
 */
BinaryPtr UnixFileSystem::readFile(const std::string& path)
{
	BWResource::checkAccessFromCallingThread( path,
			"UnixFileSystem::readFile" );

	FILE* pFile;
	char* buf;
	int len;

	BeginThreadBlockingOperation();
	pFile = fopen((basePath_ + path).c_str(), "rb");
	CeaseThreadBlockingOperation();

	if(!pFile)
		return static_cast<BinaryBlock *>( NULL );

	fseek(pFile, 0, SEEK_END);
	len = ftell(pFile);
	fseek(pFile, 0, SEEK_SET);

	if(len <= 0)
	{
		ERROR_MSG("UnixFileSystem: No data in %s\n", path.c_str());
		fclose( pFile );
		return static_cast<BinaryBlock *>( NULL );
	}

	BinaryPtr bp = new BinaryBlock( NULL, len );
	buf = (char *) bp->data();

	BeginThreadBlockingOperation();
	bool bad = !fread( buf, len, 1, pFile );
	CeaseThreadBlockingOperation();

	if (bad)
	{
		ERROR_MSG( "UnixFileSystem: Error reading from %s\n", path.c_str() );
		fclose( pFile );
		return static_cast<BinaryBlock *>( NULL );
	}

	fclose( pFile );
	return bp;
}

/**
 *	This method creates a new directory.
 *
 *	@param path		Path relative to the base of the filesystem.
 *
 *	@return True if successful
 */
bool UnixFileSystem::makeDirectory(const std::string& path)
{
	return mkdir((basePath_ + path).c_str(), 0770) == 0;
}

/**
 *	This method writes a file to disk.
 *
 *	@param path		Path relative to the base of the filesystem.
 *	@param pData	Data to write to the file
 *	@param binary	Write as a binary file (Windows only)
 *	@return	True if successful
 */
bool UnixFileSystem::writeFile(const std::string& path,
		BinaryPtr pData, bool /*binary*/)
{
	BeginThreadBlockingOperation();
	FILE* pFile = fopen((basePath_ + path).c_str(), "w");
	CeaseThreadBlockingOperation();

	if(!pFile)
	{
		ERROR_MSG("UnixFileSystem: Failed to open %s\n", path.c_str());
		return false;
	}

	if(pData->len())
	{
		BeginThreadBlockingOperation();
		bool bad = fwrite(pData->data(), pData->len(), 1, pFile) != 1;
		CeaseThreadBlockingOperation();

		if (bad)
		{
			ERROR_MSG("UnixFileSystem: Error writing to %s\n", path.c_str());
			return false;
		}
	}

	fclose(pFile);
	return true;
}


/**
 *	This method opens a file using POSIX semantics.
 *
 *	@param path		Path relative to the base of the filesystem.
 *	@param mode     Mode to open the file as (see fopen for valid options).
 *
 *	@return	True if successful
 */
FILE * UnixFileSystem::posixFileOpen( const std::string& path,
	const char * mode )
{
	BeginThreadBlockingOperation();
	//char mode[3] = { writeable?'w':'r', binary?'b':0, 0 };
	FILE * pFile = fopen( (basePath_ + path).c_str(), mode );
	CeaseThreadBlockingOperation();

	if (!pFile)
	{
		// This case will happen often with multi res paths, don't flag it as
		// an error
		//ERROR_MSG("WinFileSystem: Failed to open %s\n", path.c_str());
		return NULL;
	}

	return pFile;
}


/**
 *	This method renames the file or directory specified by oldPath
 *	to be specified by newPath. oldPath and newPath need not be in common.
 *
 *	Returns true on success
 */
bool UnixFileSystem::moveFileOrDirectory( const std::string & oldPath,
	const std::string & newPath )
{
	return rename(
		(basePath_+oldPath).c_str(), (basePath_+newPath).c_str() ) == 0;
}


/**
 *	This method erases the given file or directory.
 *	Directories need not be empty.
 *
 *	Returns true on success
 */
bool UnixFileSystem::eraseFileOrDirectory( const std::string & path )
{
	FileType ft = this->getFileType( path, NULL );
	if (ft == FT_FILE)
	{
		return unlink( (basePath_+path).c_str() ) == 0;
	}
	else if (ft == FT_DIRECTORY)
	{
		Directory d = this->readDirectory( path );
		for (uint i = 0; i < d.size(); i++)
		{
			if (!this->eraseFileOrDirectory( path + "/" + d[i] ))
				return false;
		}
		return rmdir( (basePath_+path).c_str() ) == 0;
	}
	else
	{
		return false;
	}
}

/**
 *	This is a virtual copy constructor.
 *
 *	Returns a copy of this object.
 */

IFileSystem* UnixFileSystem::clone()
{
	return new UnixFileSystem( basePath_ );
}

// unix_file_system.cpp
