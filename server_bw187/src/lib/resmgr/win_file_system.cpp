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
#include "bwresource.hpp"
#include "win_file_system.hpp"

#include "cstdmf/diary.hpp"
#include <windows.h>

#include <algorithm>

#include "network/remote_stepper.hpp"
#define conformSlash(x) (x)


DECLARE_DEBUG_COMPONENT2( "ResMgr", 0 )

/**
 *	This is the constructor.
 *
 *	@param basePath		Base path of the filesystem, including trailing '/'
 */
WinFileSystem::WinFileSystem(const std::string& basePath) :
	basePath_(basePath)
{
}

/**
 *	This is the destructor.
 */
WinFileSystem::~WinFileSystem()
{
}

/**
 *	This method returns the file type of a given path within the file system.
 *
 *	@param path		Path relative to the base of the filesystem.
 *	@param pFI		If not NULL, this is set to the type of this file.
 *
 *	@return The type of file
 */
IFileSystem::FileType WinFileSystem::getFileType( const std::string & path,
	FileInfo * pFI )
{
	BWResource::checkAccessFromCallingThread( path, "WinFileSystem::getFileType" );

	if ( path.empty() )
		return FT_NOT_FOUND;

	DiaryEntry & de = Diary::instance().add( "ft" );
	WIN32_FILE_ATTRIBUTE_DATA attrData;
	BOOL ok = GetFileAttributesEx(
		conformSlash(basePath_ + path).c_str(),
		GetFileExInfoStandard,
		&attrData );
	de.stop();
	//OutputDebugString( (path + "\n").c_str() );

	if (!ok || attrData.dwFileAttributes == DWORD(-1))
		return FT_NOT_FOUND;

	if (pFI != NULL)
	{
		pFI->size = uint64(attrData.nFileSizeHigh)<<32 | attrData.nFileSizeLow;
		pFI->created = *(uint64*)&attrData.ftCreationTime;
		pFI->modified = *(uint64*)&attrData.ftLastWriteTime;
		pFI->accessed = *(uint64*)&attrData.ftLastAccessTime;
	}

	if (attrData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		return FT_DIRECTORY;

	return FT_FILE;
}

extern const char * g_daysOfWeek[];
extern const char * g_monthsOfYear[];

/**
 *	This method converts the given file event time to a string.
 *	The string is in the same format as would be stored in a CVS/Entries file.
 */
std::string	WinFileSystem::eventTimeToString( uint64 eventTime )
{
	SYSTEMTIME st;
	memset( &st, 0, sizeof(st) );
	FileTimeToSystemTime( (const FILETIME*)&eventTime, &st );
	char buf[32];
	sprintf( buf, "%s %s %02d %02d:%02d:%02d %d",
		g_daysOfWeek[st.wDayOfWeek], g_monthsOfYear[st.wMonth], st.wDay,
		st.wHour, st.wMinute, st.wSecond, st.wYear );
	return buf;
}


/**
 *	This method reads the contents of a directory.
 *
 *	@param path		Path relative to the base of the filesystem.
 *
 *	@return A vector containing all the filenames in the directory.
 */
IFileSystem::Directory WinFileSystem::readDirectory(const std::string& path)
{
	BWResource::checkAccessFromCallingThread( path, "WinFileSystem::readDirectory" );

	Directory dir;
	bool done = false;
	WIN32_FIND_DATA findData;

	std::string dirstr = conformSlash(basePath_ + path);
	if (!dirstr.empty() && *dirstr.rbegin() != '\\' && *dirstr.rbegin() != '/')
		dirstr += "\\*";
	else
		dirstr += "*";
	HANDLE findHandle = FindFirstFile(dirstr.c_str(), &findData);

	if(findHandle == INVALID_HANDLE_VALUE)
		return dir;

	while(!done)
	{
		std::string name = findData.cFileName;

		if(name != "." && name != "..")
			dir.push_back(name);

		if(!FindNextFile(findHandle, &findData))
			done = true;
	}

	FindClose(findHandle);
	return dir;
}


/**
 *	This method reads the contents of a file
 *
 *	@param path		Path relative to the base of the filesystem.
 *
 *	@return A BinaryBlock object containing the file data.
 */
BinaryPtr WinFileSystem::readFile(const std::string& path)
{
	BWResource::checkAccessFromCallingThread( path, "WinFileSystem::readFile" );

	FILE* pFile;
	char* buf;
	int len;

	DiaryEntry & de1 = Diary::instance().add( "fo" );
	pFile = this->posixFileOpen( path, "rb" );
	de1.stop();

	if(!pFile)
		return NULL;

	fseek(pFile, 0, SEEK_END);
	len = ftell(pFile);
	fseek(pFile, 0, SEEK_SET);

	if(len <= 0)
	{
		ERROR_MSG("WinFileSystem: No data in %s\n", path.c_str());
		fclose( pFile );
		return NULL;
	}

	BinaryPtr bp = new BinaryBlock(NULL, len);
	buf = (char*)bp->data();

	if(!buf)
	{
		ERROR_MSG("WinFileSystem: Failed to alloc %d bytes\n", len);
		fclose( pFile );
		return NULL;
	}

	DiaryEntry & de2 = Diary::instance().add( "fr" );
	int frr = 1;
	int extra = 0;
	for (int sofa = 0; sofa < len; sofa += extra)
	{
		extra = std::min( len - sofa, 128*1024 );	// read in 128KB chunks
		frr = fread( buf+sofa, extra, 1, pFile );
		if (!frr) break;
	}
	de2.stop();
	if (!frr)
	{
		ERROR_MSG("WinFileSystem: Error reading from %s\n", path.c_str());
		fclose( pFile );
		return NULL;
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
bool WinFileSystem::makeDirectory(const std::string& path)
{
	return CreateDirectory(
		conformSlash(basePath_ + path).c_str(), NULL ) == TRUE;
}

/**
 *	This method writes a file to disk.
 *
 *	@param path		Path relative to the base of the filesystem.
 *	@param pData	Data to write to the file
 *	@param binary	Write as a binary file (Windows only)
 *	@return	True if successful
 */
bool WinFileSystem::writeFile( const std::string& path,
		BinaryPtr pData, bool binary )
{
	FILE* pFile = this->posixFileOpen( path, binary?"wb":"w" );
	if (!pFile) return false;

	if (pData->len())
	{
		if (fwrite( pData->data(), pData->len(), 1, pFile ) != 1)
		{
			fclose( pFile );
			CRITICAL_MSG( "WinFileSystem: Error writing to %s. Disk full?\n",
				path.c_str() );
			// this->eraseFileOrDirectory( path );
			return false;
		}
	}

	fclose( pFile );
	return true;
}

/**
 *	This method opens a file using POSIX semantics.
 *
 *	@param path		Path relative to the base of the filesystem.
 *	@param mode		The mode that the file should be opened in.
 *	@return	True if successful
 */
FILE * WinFileSystem::posixFileOpen( const std::string& path,
		const char * mode )
{
	//char mode[3] = { writeable?'w':'r', binary?'b':0, 0 };

	std::string fullPath = conformSlash(basePath_ + path);
	FILE * pFile = fopen( fullPath.c_str(), mode );

	if (!pFile)
	{
		// This case will happen often with multi 
		// res paths, don't flag it as an error
		// ERROR_MSG("WinFileSystem: Failed to open %s\n", fullPath.c_str());
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
bool WinFileSystem::moveFileOrDirectory( const std::string & oldPath,
	const std::string & newPath )
{
	return !!MoveFileEx(
		conformSlash(basePath_+oldPath).c_str(),
		conformSlash(basePath_+newPath).c_str(),
		MOVEFILE_REPLACE_EXISTING );
}


/**
 *	This method erases the file or directory specified by path.
 *	Directories need not be empty.
 *
 *	Returns true on success
 */
bool WinFileSystem::eraseFileOrDirectory( const std::string & path )
{
	FileType ft = this->getFileType( path, NULL );
	if (ft == FT_FILE)
	{
		return !!DeleteFile( conformSlash(basePath_+path).c_str() );
	}
	else if (ft == FT_DIRECTORY)
	{
		Directory d = this->readDirectory( path );
		for (uint i = 0; i < d.size(); i++)
		{
			if (!this->eraseFileOrDirectory( path + "/" + d[i] ))
				return false;
		}
		return !!RemoveDirectory( conformSlash(basePath_+path).c_str() );
	}
	else
	{
		return false;
	}
}


/**
 *	This method returns a IFileSystem pointer to a copy of this object.
 *
 *	@return	a copy of this object
 */
IFileSystem* WinFileSystem::clone()
{
	return new WinFileSystem( basePath_ );
}


// win_file_system.cpp