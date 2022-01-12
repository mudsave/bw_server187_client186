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
#include "multi_file_system.hpp"
#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT2( "ResMgr", 0 )

// -----------------------------------------------------------------------------
// Section: MultiFileSystem
// -----------------------------------------------------------------------------


/**
 *	This is the constructor.
 */
MultiFileSystem::MultiFileSystem()
{
}

MultiFileSystem::MultiFileSystem( const MultiFileSystem & other )
{
	copy( other );
}

MultiFileSystem & MultiFileSystem::operator=( const MultiFileSystem & other )
{
	if ( &other != this )
	{
		cleanUp();
		copy( other );
	}
	return *this;
}


/**
 *	This is the destructor.
 */
MultiFileSystem::~MultiFileSystem()
{
	cleanUp();
}

void MultiFileSystem::cleanUp()
{
	FileSystemVector::iterator it;
	for (it = baseFileSystems_.begin(); it != baseFileSystems_.end(); it++)
		delete *it;
	baseFileSystems_.clear();
}

void MultiFileSystem::copy( const MultiFileSystem & other  )
{
	FileSystemVector::const_iterator it;
	for (it = other.baseFileSystems_.begin(); it != other.baseFileSystems_.end(); it++)
		baseFileSystems_.push_back( (*it)->clone() );
}

/**
 *	This method adds a base filesystem to the search path.
 *	FileSystems will be searched in the order they are added.
 *	Note that FileSystems are expected to be allocated on the
 *	heap, and are owned by this object. They will be freed by
 *	the MultiFileSystem when it is destroyed.
 *
 *	@param pFileSystem	Pointer to the FileSystem to add.
 *	@param index	Indicates where to insert the file system in to the current set.
 */
void MultiFileSystem::addBaseFileSystem( IFileSystem* pFileSystem, int index )
{
	if (uint(index) >= baseFileSystems_.size())
		baseFileSystems_.push_back( pFileSystem );
	else
		baseFileSystems_.insert( baseFileSystems_.begin()+index, pFileSystem );
}

/**
 *	This method removes a base file system specified by its index
 */
void MultiFileSystem::delBaseFileSystem( int index )
{
	if (uint(index) >= baseFileSystems_.size()) return;
	FileSystemVector::iterator it = baseFileSystems_.begin()+index;
	delete *it;
	baseFileSystems_.erase( it );
}


/**
 *	This method reads the contents of a file.
 *
 *	@param path		Path relative to the base of the filesystem.
 *	@param pFI		If not NULL, this is set to the type of this file.
 *
 *	@return A BinaryBlock object containing the file data.
 */
IFileSystem::FileType MultiFileSystem::getFileType( const std::string& path,
	FileInfo * pFI )
{
	FileSystemVector::iterator it;
	for (it = baseFileSystems_.begin(); it != baseFileSystems_.end(); it++)
	{
		FileType ft = (*it)->getFileType( path, pFI );
		if (ft != FT_NOT_FOUND) return ft;
	}
	return FT_NOT_FOUND;
}

/**
 *	This method converts a file event time to a string - only the first file system is
 *	asked.
 *
 *	@param eventTime The file event time to convert.
 */
std::string	MultiFileSystem::eventTimeToString( uint64 eventTime )
{
	if (!baseFileSystems_.empty())
		return baseFileSystems_[0]->eventTimeToString( eventTime );

	return std::string();
}

/**
 *	This method reads the contents of a directory.
 *
 *	@param path		Path relative to the base of the filesystem.
 *
 *	@return A vector containing all the filenames in the directory.
 */
IFileSystem::Directory MultiFileSystem::readDirectory(const std::string& path)
{
	FileSystemVector::iterator it;
	Directory dir;

	for (it = baseFileSystems_.begin(); it != baseFileSystems_.end(); it++)
	{
		Directory partdir = (*it)->readDirectory(path);
		dir.insert( dir.end(), partdir.begin(), partdir.end() );
	}

	return dir;
}

/**
 *	This method reads the contents of a file.
 *
 *	@param path		Path relative to the base of the filesystem.
 *
 *	@return A BinaryBlock object containing the file data.
 */
BinaryPtr MultiFileSystem::readFile(const std::string& path)
{
	FileSystemVector::iterator it;
	for (it = baseFileSystems_.begin(); it != baseFileSystems_.end(); it++)
	{
		BinaryPtr pBinary = (*it)->readFile(path);
		if (pBinary) return pBinary;
	}
	return NULL;
}

/**
 *	This method reads the contents of a file.
 *
 *	@param path		Path relative to the base of the filesystem.
 *	@param ret		Returned vector of binary blocks 
 */
void MultiFileSystem::collateFiles(const std::string& path,
								std::vector<BinaryPtr>& ret)
{
	FileSystemVector::iterator it;
	for (it = baseFileSystems_.begin(); it != baseFileSystems_.end(); it++)
	{
		BinaryPtr pBinary = (*it)->readFile(path);
		if (pBinary) ret.push_back(pBinary);		
	}	
}

/**
 *	This method creates a new directory.
 *
 *	@param path		Path relative to the base of the filesystem.
 *
 *	@return True if successful
 */
bool MultiFileSystem::makeDirectory(const std::string& path)
{
	FileSystemVector::iterator it;
	for (it = baseFileSystems_.begin(); it != baseFileSystems_.end(); it++)
	{
		if ((*it)->makeDirectory(path))
			return true;
	}

	return false;
}

/**
 *	This method writes a file to disk.
 *	Iterate through the base file systems until the given path is found.
 *
 *	@param path		Path relative to the base of the filesystem.
 *	@param pData	Data to write to the file
 *	@param binary	Write as a binary file (Windows only)
 *	@return	True if successful
 */
bool MultiFileSystem::writeFile(const std::string& path,
		BinaryPtr pData, bool binary)
{
	FileSystemVector::iterator it;
	for (it = baseFileSystems_.begin(); it != baseFileSystems_.end(); it++)
	{
		if ((*it)->writeFile(path, pData, binary))
			return true;
	}

	return false;
}

/**
 *	This method moves a file around.
 *	Iterate through the base file systems until the given path is found.
 *
 *	@param oldPath		The path to move from.
 *	@param newPath		The path to move to.
 *	@return	True if successful
 */
bool MultiFileSystem::moveFileOrDirectory( const std::string & oldPath,
	const std::string & newPath )
{
	FileSystemVector::iterator it;
	for (it = baseFileSystems_.begin(); it != baseFileSystems_.end(); it++)
	{
		if ((*it)->moveFileOrDirectory( oldPath, newPath ))
			return true;
	}

	return false;
}

/**
 *	This method erases a given file or directory from the first
 *	base file system that it is found in.
 *	Iterate through the base file systems until the given path is found.
 *
 *	@param path		Path relative to the base of the filesystem.
 *	@return	True if successful, otherwise false.
 */
bool MultiFileSystem::eraseFileOrDirectory( const std::string & path )
{
	FileSystemVector::iterator it;
	for (it = baseFileSystems_.begin(); it != baseFileSystems_.end(); it++)
	{
		if ((*it)->eraseFileOrDirectory( path ))
			return true;
	}

	return false;
}


/**
 *	This method opens the given file using posix semantics.
 *
 *	@param path		Path relative to the base of the filesystem.
 *	@param mode		The mode that the file should be opened in.
 *
 *	@return	True if successful
 */
FILE * MultiFileSystem::posixFileOpen( const std::string & path,
	const char * mode )
{
	FILE * pFile = NULL;

	FileSystemVector::iterator it;
	for (it = baseFileSystems_.begin(); it != baseFileSystems_.end(); it++)
	{
		pFile = (*it)->posixFileOpen( path, mode );
		if (pFile != NULL) 
		{
			break;
		}
	}

	/*
	 //	This is useful to debug the order in which resources are 
	 //	searched for and loaded from the res path. I've used it 
	 //	more than once to trace how python modules are loaded.

	if (pFile == NULL)
	{
		DEBUG_MSG("Could not open: %s.\n", path.c_str());
	}
	else
	{
		DEBUG_MSG("Found file: %s.\n", path.c_str());
	}
	*/

	return pFile;
}


/**
 *	This method returns a IFileSystem pointer to a copy of this object.
 *
 *	@return	a copy of this object
 */
IFileSystem* MultiFileSystem::clone()
{
	return new MultiFileSystem( *this );
}


// -----------------------------------------------------------------------------
// Section: NativeFileSystem
// -----------------------------------------------------------------------------


#ifdef _WIN32
#include "win_file_system.hpp"
#else
#include "unix_file_system.hpp"
#endif

/**
 *	Create a native file system
 */
IFileSystem * NativeFileSystem::create( const std::string & path )
{
#ifdef _WIN32
	return new WinFileSystem( path );
#else
	return new UnixFileSystem( path );
#endif
}


// don't ask...
const char * g_daysOfWeek[] =
	{ "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
const char * g_monthsOfYear[] =
	{ "Bad", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

// multi_file_system.cpp
