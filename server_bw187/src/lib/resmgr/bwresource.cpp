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

#include "cstdmf/bwversion.hpp"
#include "cstdmf/debug.hpp"
#include "cstdmf/memory_trace.hpp"

#include "bwresource.hpp"
#include "xml_section.hpp"
#include "data_section_cache.hpp"
#include "data_section_census.hpp"
#include "dir_section.hpp"
#include "zip_file_system.hpp"
#include "multi_file_system.hpp"

#ifdef _WIN32
#include "Shlwapi.h"
#else
#include <pwd.h>
#endif // _WIN32


DECLARE_DEBUG_COMPONENT2( "ResMgr", 0 )

#ifdef USE_MEMORY_TRACER
#define DEFAULT_CACHE_SIZE (0)
#else
#define DEFAULT_CACHE_SIZE (100 * 1024)
#endif


#define PATHS_FILE "paths.xml"
#define PACKED_EXTENSION "zip"
#define FOLDER_SEPERATOR '/'
static bool g_inited = false;

typedef std::vector<std::string> STRINGVECTOR;


class BWResourceImpl
{
public:
	BWResourceImpl() :
		basePath_( NULL ),
		rootSection_( NULL ),
		fileSystem_( NULL ),
		census_( NULL )
	{
		nativeFileSystem_ = NativeFileSystem::create( "" );
	};

	void cleanUp( bool final );

	~BWResourceImpl()
	{
		cleanUp( true );
		delete nativeFileSystem_;
	};

	const std::string*			basePath_;
	DataSectionPtr				rootSection_;
	MultiFileSystem				* fileSystem_;
	IFileSystem					* nativeFileSystem_;
	void						* census_;

	// private interface taken from the now deprecated FilenameResolver
	bool loadPathsXML( std::string customAppFolder = "" );

	STRINGVECTOR				paths_;

    std::string                 defaultDrive_;

	void						postAddPaths();

	static bool					hasDriveInPath( const std::string& path );
	static bool					pathIsRelative( const std::string& path );

	static std::string			formatSearchPath( const std::string& path, const std::string& pathXml = "" );

#ifdef _WIN32
	static const std::string	getAppDirectory();
#endif // _WIN32
};


// function to compare case-insensitive in windows, case sensitive in unix
namespace
{
	int mf_pathcmp( const char* a, const char* b )
	{
#ifdef unix
		return strcmp( a, b );
#else
		return _stricmp( a, b );
#endif // unix
	}
};


void BWResourceImpl::cleanUp( bool final )
{
	rootSection_ = NULL;

	// Be careful here. If users hang on to DataSectionPtrs after
	// destruction of the cache, this could cause a problem at
	// shutdown time. Could make the cache a reference counted
	// object and make the DataSections use smart pointers to it,
	// if this is actually an issue.

	delete fileSystem_;

	if (final)
	{
		DataSectionCache::fini();
	}
}

void BWResourceImpl::postAddPaths()
{
	if (!g_inited)
	{
		CRITICAL_MSG( "BWResourceImpl::postAddPaths: "
				"BWResource::init should have been called already.\n" );
		return;
	}

	if ( fileSystem_ )
	{
		// already called, so clean up first.
		cleanUp( false );
	}

	fileSystem_ = new MultiFileSystem();

	int pathIndex = 0;
	std::string path;

	// Currently basePath_ is used for saves. Should have a better system.
	if ( basePath_ == NULL )
		path = BWResource::getPath( pathIndex++ );
	else
		path = *basePath_;

	IFileSystem* tempFS = NativeFileSystem::create( "" );

	// add an appropriate file system for each path
	while (!path.empty())
	{
		// check to see if the path is actually a Zip file
		std::string pakPath = path;

		// also check to see if the path contains a "res.zip" Zip file
		std::string pakPath2 = path;
		if (*pakPath2.rbegin() != '/') pakPath2.append( "/" );
		pakPath2 += "res.";
		pakPath2 += PACKED_EXTENSION;

		if ( BWResource::getExtension( pakPath ) == PACKED_EXTENSION &&
			tempFS->getFileType( pakPath, NULL ) == IFileSystem::FT_FILE )
		{
			// it's a Zip file system, so add it
			INFO_MSG( "BWResource::BWResource: Path is package %s\n",
					pakPath.c_str() );
			fileSystem_->addBaseFileSystem( new ZipFileSystem( pakPath ) );
		}
		else if ( tempFS->getFileType( pakPath2, NULL ) == IFileSystem::FT_FILE )
		{
			// it's a Zip file system, so add it
			INFO_MSG( "BWResource::BWResource: Found package %s\n",
					pakPath2.c_str() );
			fileSystem_->addBaseFileSystem( new ZipFileSystem( pakPath2 ) );
		}
		else  if ( tempFS->getFileType( path, NULL ) == IFileSystem::FT_DIRECTORY )
		{
			if (*path.rbegin() != '/') path.append( "/" );

			// it's a standard path, so add a native file system for it
			fileSystem_->addBaseFileSystem( NativeFileSystem::create( path ) );
		}
		path = BWResource::getPath( pathIndex++ );
	}

	delete tempFS;

#ifdef EDITOR_ENABLED
	// add a default native file system, to handle absolute paths in the tools
	fileSystem_->addBaseFileSystem( NativeFileSystem::create( "" ) );
#endif
	char * cacheSizeStr = ::getenv( "BW_CACHE_SIZE" );
	int cacheSize = cacheSizeStr ? atoi(cacheSizeStr) : DEFAULT_CACHE_SIZE;

	DataSectionCache::instance()->setSize( cacheSize );
	DataSectionCache::instance()->clear();

	//new DataSectionCache( cacheSize );
	rootSection_ = new DirSection( "", fileSystem_ );
}




// -----------------------------------------------------------------------------
// Section: BWResource
// -----------------------------------------------------------------------------

/**
 *	Constructor
 */
BWResource::BWResource( const std::string * extraPath )
{
	pimpl_ = new BWResourceImpl();
	pimpl_->basePath_ = extraPath;
}

/**
 *	Destructor
 */
BWResource::~BWResource()
{
	delete pimpl_;
}

void BWResource::refreshPaths()
{
	pimpl_->postAddPaths();
}

DataSectionPtr BWResource::rootSection()
{
	return pimpl_->rootSection_;
}

MultiFileSystem* BWResource::fileSystem()
{
	return pimpl_->fileSystem_;
}

/**
 *	This method purges an item from the cache, given its relative path from
 *	the root section.
 *
 *	@param path			The path to the item to purge.
 *	@param recursive	Set this to true to perform a full recursive purge.
 *	@param spSection	If not NULL, this should be a pointer to the data
 *						section at 'path'. This is used to improve efficiency
 *						and avoid a problem with duplicate child names.
 */
void BWResource::purge( const std::string & path, bool recursive,
		DataSectionPtr spSection )
{
	if (recursive)
	{
		if (!spSection)
			spSection = pimpl_->rootSection_->openSection( path );

		if (spSection)
		{
			DataSectionIterator it = spSection->begin();
			DataSectionIterator end = spSection->end();

			while (it != end)
			{
				this->purge( path + "/" + (*it)->sectionName(), true, *it );
				it++;
			}
		}
	}

	// remove it from the cache
	DataSectionCache::instance()->remove( path );

	// remove it from the census too
	DataSectionPtr pSection = DataSectionCensus::find( path );
	if (pSection) DataSectionCensus::del( &*pSection );
}

/**
 *	This method purges everything from the cache and census.
 */
void BWResource::purgeAll()
{
	DataSectionCache::instance()->clear();
	DataSectionCensus::clear();
}


/**
 *  This method saves the section with the given pathname.
 *
 *  @param resourceID     The relative path of the section.
 */
bool BWResource::save( const std::string & resourceID )
{
	DataSectionPtr ptr = DataSectionCensus::find( resourceID );

	if (!ptr)
	{
		WARNING_MSG( "BWResource::saveSection: %s not in census\n",
			resourceID.c_str() );
		return false;
	}

	return ptr->save();
}


/**
 *	Standard open section method.
 *	Always looks in the census first.
 */
DataSectionPtr BWResource::openSection( const std::string & resourceID,
	bool makeNewSection )
{
	DataSectionPtr pExisting = DataSectionCensus::find( resourceID );
	if (pExisting) return pExisting;

	return instance().rootSection()->openSection( resourceID, makeNewSection );
}


/// static BWResource instance
static THREADLOCAL(BWResource*) s_instance(NULL);

/**
 *	This method returns the BWResource as a singleton object.
 */
BWResource & BWResource::instance()
{
	if (!g_inited)
	{
		CRITICAL_MSG(
				"BWResource::BWResource: "
				"BWResource::init should have been called already.\n" );
		init( 0, NULL );
	}

	if (s_instance == NULL)
	{
		static BWResource defaultInstance;
		s_instance = &defaultInstance;
	}

	return *s_instance;
}

/**
 *	This method sets the BWResource instance for the current thread.
 *	A NULL parameter returns it to the default shared instance.
 */
void BWResource::instance( BWResource * pNewInstance )
{
	s_instance = pNewInstance;
	s_instance->pimpl_->postAddPaths();
	DataSectionCensus::store( pNewInstance ? &pNewInstance->pimpl_->census_ : NULL );
}


// Needs to be constucted like this for broken Linux thread local storage.
THREADLOCAL( bool ) g_doneLoading( false );

/**
 *	Causes the file system to issue warnings if
 *	files are loaded from the calling thread.
 *
 *	@param	watch	true if calling thread should be watched. False otherwise.
 */
void BWResource::watchAccessFromCallingThread(bool watch)
{
	g_doneLoading = watch;
}

/**
 *	Displays the proper warnings if
 *	files are loaded from the calling thread.
 */
void BWResource::checkAccessFromCallingThread(
	const std::string& path, const std::string& msg )
{
	if ( g_doneLoading )
	{
		WARNING_MSG( "****** Accessing %s from watched thread in %s\n",
			path.c_str(), msg.c_str() );
	}
}

//
//
//	Static methods (taken from the now deprecated BWResource)
//
//

static std::string appDrive_;

#ifdef unix
// difference in .1 microsecond steps between FILETIME and time_t
const uint64 unixTimeBaseOffset = 116444736000000000ull;
const uint64 unixTimeBaseMultiplier = 10000000;

inline uint64 unixTimeToFileTime( time_t t )
{
	return uint64(t) * unixTimeBaseMultiplier + unixTimeBaseOffset;
}
#endif

// Separator character for BW_RES_PATH
#ifdef unix
#define PATH_SEPARATOR ":"
#else
#define PATH_SEPARATOR ";"
#endif

#define RES_PATH_HELP												\
	"\n\nFor more information on how to correctly setup and run "	\
	"BigWorld client, please refer to the Client Installation "		\
	"Guide, in mf/bigworld/doc directory.\n"

#if !defined( _WIN32 ) || defined( _XBOX )
// these are already defined in Win32
int __argc = 0;
char ** __argv = NULL;
#endif

#ifdef _WIN32
/*static*/ const std::string BWResourceImpl::getAppDirectory()
{
	std::string appDirectory( "." );

	// get applications directory
	char buffer[MAX_PATH];
	GetModuleFileName( NULL, buffer, sizeof( buffer ) );
	std::string path( buffer );
	appDirectory = BWResource::getFilePath( path );

#ifdef EDITOR_ENABLED
	appDrive_ = buffer[0];
	appDrive_ += ":";
#endif

	std::replace( appDirectory.begin(), appDirectory.end(), '\\', '/' );

	return appDirectory;
}
#endif // _WIN32

/**
 *	Load the paths.xml file, or avoid loading it if possible.
 */
bool BWResourceImpl::loadPathsXML( std::string customAppFolder )
{
	if (paths_.empty())
	{
		char * basePath;

		// The use of the BW_RES_PATH environment variable is deprecated
		if ((basePath = ::getenv( "BW_RES_PATH" )) != NULL)
		{
			WARNING_MSG("The use of the BW_RES_PATH environment variable is deprecated post 1.8.1");
			BWResource::addPaths( basePath, "from BW_RES_PATH env variable" );
		}
	}

	if (paths_.empty())
	{
#ifdef _WIN32

		std::string appDirectory;
		do
		{
			// get the applications path if none was passed in
			// and load the config file from it
			if (customAppFolder == "")
				appDirectory = BWResourceImpl::getAppDirectory();
			else
				appDirectory = customAppFolder;

			// This insures that if appDir was passed in, the app directory will be used
			// in the next iteration.
			customAppFolder = "";

#ifdef LOGGING
			dprintf( "BWResource: app directory is %s\n",
				appDirectory.c_str() );
			dprintf( "BWResource: using %spaths.xml\n",
				appDirectory.c_str() );
			std::ofstream os( "filename.log", std::ios::trunc );
			os << "app dir: " << appDirectory << "\n";
#endif

			// load the search paths
			std::string fullPathsFileName = appDirectory + PATHS_FILE;
			if ( nativeFileSystem_->getFileType( fullPathsFileName ) != IFileSystem::FT_NOT_FOUND )
			{
				DEBUG_MSG( "Loading.. %s\n", fullPathsFileName.c_str() );
				BinaryPtr binBlock = nativeFileSystem_->readFile( fullPathsFileName );
				DataSectionPtr spRoot = XMLSection::createFromBinary( "root", binBlock );
				if ( spRoot )
				{
					// load the paths
					std::string path = "Paths";
					DataSectionPtr spSection = spRoot->openSection( path );

					STRINGVECTOR paths;
					if ( spSection )
						spSection->readStrings( "Path", paths );

					// Format the paths
					for (STRINGVECTOR::iterator i = paths.begin(); i != paths.end(); ++i)
					{
						BWResource::addPaths( *i, "from paths.xml", appDirectory );
					}
				}
				else
				{
					DEBUG_MSG( "Failed to open paths.xml" );
				}
			}
		} while (appDirectory != BWResourceImpl::getAppDirectory());
#else
		// Try reading ~/.bwmachined.conf
		char * pHomeDir = ::getenv( "HOME" );

		if (pHomeDir == NULL)
		{
			struct passwd * pPWEnt = ::getpwuid( ::getuid() );
			if (pPWEnt)
			{
				pHomeDir = pPWEnt->pw_dir;
			}
		}

		if (pHomeDir)
		{
			char buffer[1024];
			snprintf( buffer, sizeof(buffer), "%s/.bwmachined.conf", pHomeDir );
			FILE * pFile = fopen( buffer, "r" );

			if (pFile)
			{
				while (paths_.empty() &&
						fgets( buffer, sizeof( buffer ), pFile ) != NULL)
				{
					// Find first non-whitespace character in buffer
					char *start = buffer;
					while (*start && isspace( *start ))
						start++;

					if (*start != '#' && *start != '\0')
					{
						if (sscanf( start, "%*[^;];%s", start ) == 1)
						{
							BWResource::addPaths( start,
									"from ~/.bwmachined.conf" );
						}
						else
						{
							WARNING_MSG( "BWResource::BWResource: "
								"Invalid line in ~/.bwmachined.conf - \n"
								"\t'%s'\n",
								buffer );
						}
					}
				}
				fclose( pFile );
			}
			else
			{
				WARNING_MSG( "BWResource::BWResource: Unable to open %s\n",
						buffer );
			}
		}

		if (paths_.empty())
		{
			ERROR_MSG( "BWResource::BWResource: No res path set.\n"
				"\tIs ~/.bwmachined.conf correct or 'BW_RES_PATH' environment "
					"variable set?\n" );
		}
#endif
	}

#if _WIN32
#ifndef _RELEASE
	if (paths_.empty())
	{
		CRITICAL_MSG(
			"BWResource::BWResource: "
			"No paths found in BW_RES_PATH environment variable "
			"and no paths.xml file found in the working directory. "
			RES_PATH_HELP );
	}
#endif // not _RELEASE
#endif // _WIN32

#ifdef MF_SERVER
#ifdef _WIN32
	COORD c = { 80, 2000 };
	SetConsoleScreenBufferSize( GetStdHandle( STD_OUTPUT_HANDLE ), c );
#endif
#endif

	return !paths_.empty();
}


/**
 *	This method initialises the BWResource from command line arguments.
 */
bool BWResource::init( int argc, char * argv[] )
{
	// This should be called before the BWResource is constructed.
	MF_ASSERT( !g_inited );
	g_inited = true;

	for (int i = 0; i < argc - 1; i++)
	{
		if (strcmp( argv[i], "--res" ) == 0 ||
			strcmp( argv[i], "-r" ) == 0)
		{
			addPaths( argv[i + 1], "from command line" );
		}
		if (strcmp( argv[i], "-UID" ) == 0 ||
			strcmp( argv[i], "-uid" ) == 0)
		{
			INFO_MSG( "UID set to %s (from command line).\n", argv[ i+1 ] );
#ifndef _WIN32
			::setenv( "UID", argv[ i+1 ], true );
#else
			std::string addStr = "UID=";
			addStr += argv[ i+1 ];
			::_putenv( addStr.c_str() );
#endif
		}
	}

	bool res = instance().pimpl_->loadPathsXML();
	instance().pimpl_->postAddPaths();

	if (res)
	{
		BWResource::initCommon();
	}

	return res;
}

/**
 * Initialise BWResource to use the given path directly
 */
bool BWResource::init( const std::string& fullPath, bool addAsPath )
{
	// This should be called before the BWResource is constructed.
	MF_ASSERT( !g_inited );
	g_inited = true;

	bool res = instance().pimpl_->loadPathsXML(fullPath);
	instance().pimpl_->postAddPaths();

	if (addAsPath)
		addPath( fullPath );

	if (res)
	{
		BWResource::initCommon();
	}

	return res;
}


/**
 * This method is called by both init methods.
 */
void BWResource::initCommon()
{
	DataSectionPtr pVersion = BWResource::openSection( "version.xml" );

	if (pVersion)
	{
		BWVersion::majorNumber(
			pVersion->readInt( "major", BWVersion::majorNumber() ) );
		BWVersion::minorNumber(
			pVersion->readInt( "minor", BWVersion::minorNumber() ) );
		BWVersion::patchNumber(
			pVersion->readInt( "patch", BWVersion::patchNumber() ) );
		BWVersion::reservedNumber(
			pVersion->readInt( "reserved", BWVersion::reservedNumber() ) );
	}
}


/**
 *	This method searches for a file in the current search path, recursing into
 *	sub-directories.
 *
 *	@param file the file to find
 *
 *	@return true if the file is found
 */
bool BWResource::searchPathsForFile( std::string& file )
{
	std::string filename = getFilename( file );
	STRINGVECTOR::iterator it  = instance().pimpl_->paths_.begin( );
	STRINGVECTOR::iterator end = instance().pimpl_->paths_.end( );
	bool found = false;

	// search for the file in the supplied paths
	while( !found && it != end )
	{
		found = searchForFile( formatPath((*it)), filename );
		it++;
	}

	if ( found )
		file = filename;

	return found;
}

/**
 *	Searches for a file given a start directory, recursing into sub-directories
 *
 *	@param directory start directory of search
 *	@param file the file to find
 *
 *	@return true if the file is found
 *
 */
bool BWResource::searchForFile( const std::string& directory,
		std::string& file )
{
	bool result = false;

	std::string searchFor = formatPath( directory ) + file;

	// search for file in the supplied directory
	if ( instance().fileSystem()->getFileType( searchFor, NULL ) ==
		IFileSystem::FT_FILE )
    {
		result = true;
		file = searchFor;
	}
	else
	{
		// if not found, get all directories and start to search them
		searchFor  = formatPath( directory );
		IFileSystem::Directory dirList =
			instance().fileSystem()->readDirectory( searchFor );

		for( IFileSystem::Directory::iterator d = dirList.begin();
			d != dirList.end() && !result; ++d )
		{
			searchFor  = formatPath( directory ) + (*d);

       		// check if it is a directory
			if ( instance().fileSystem()->getFileType( searchFor, NULL ) ==
				IFileSystem::FT_DIRECTORY )
			{
            	// recurse into the directory, and start it all again
				printf( "%s\n", searchFor.c_str( ) );

				result = searchForFile( searchFor, file );
			}
		}
	}

	return result;
}

#include "network/remote_stepper.hpp"

#ifdef EDITOR_ENABLED

/**
 *	Finds a file by looking in the search paths
 *
 *	Path Search rules.
 *		If the files path has a drive letter in it, then only the filename is
 *		used. The files path must be relative and NOT start with a / or \ to use
 *		the search path.
 *
 *		If either of the top 2 conditions are true or the file is not found with
 *		its own path appended to the search paths then the file is searched for
 *		using just the filename.
 *
 *	@param file the files name
 *
 *	@return files full path and filename
 *
 */
const std::string BWResource::findFile( const std::string& file )
{
	return BWResolver::findFile( file );
}

/**
 *	Resolves a file into an absolute filename based on the search paths
 *	@see findFile()
 *
 *	@param file the files name
 *
 *	@return the resolved filename
 *
 */
const std::string BWResource::resolveFilename( const std::string& file )
{
	return BWResolver::resolveFilename( file );
}

/**
 *	Dissolves a full filename (with drive and path information) to relative
 *	filename.
 *
 *	@param file the files name
 *
 *	@return the dissolved filename, "" if filename cannot be dissolved
 *
 */
const std::string BWResource::dissolveFilename( const std::string& file )
{
	return BWResolver::dissolveFilename( file );
}

/**
 *  Determines whether a dissolved filename is a valid BigWorld path or not.
 *
 *	@param file the files name
 *
 *	@return a boolean for whether the file name is valid
 */
bool BWResource::validPath( const std::string& file )
{
	if (file == "") return false;

	bool hasDriveName = (file.c_str()[1] == ':');
	bool hasNetworkName = (file.c_str()[0] == '/' && file.c_str()[1] == '/');
	hasNetworkName |= (file.c_str()[0] == '\\' && file.c_str()[1] == '\\');

	return !(hasDriveName || hasNetworkName);
}

/**
 *	Removes the drive specifier from the filename
 *
 *	@param file filename string
 *
 *	@return filename minus any drive specifier
 *
 */
const std::string BWResource::removeDrive( const std::string& file )
{
	return BWResolver::removeDrive( file );
}

void BWResource::defaultDrive( const std::string& drive )
{
	BWResolver::defaultDrive( drive );
}

#endif // EDITOR_ENABLED


/**
 *	Tests if a file exists.
 *
 *	@param file The relative path to the file - relative to the rest paths.
 *
 *	@return true if the file exists
 *
 */
bool BWResource::fileExists( const std::string& file )
{
	return BWResource::instance().fileSystem()->getFileType(
		file.c_str(), NULL ) != IFileSystem::FT_NOT_FOUND;
}

/**
 *	Tests if a file exists.
 *
 *	@param file The absolute path to the file (not including drive letter).
 *
 *	@return true if the file exists
 *
 */
bool BWResource::fileAbsolutelyExists( const std::string& file )
{
	return BWResource::instance().pimpl_->nativeFileSystem_->getFileType(
		file.c_str(), NULL ) != IFileSystem::FT_NOT_FOUND;
}

/**
 *	Retrieves the filename of a file.
 *
 *	@param file file to get filename from.
 *
 *	@return filename string.
 *
 */
const std::string BWResource::getFilename( const std::string& file )
{
	std::string::size_type pos = file.find_last_of( "\\/" );

	if (pos != std::string::npos)
		return file.substr( pos + 1, file.length() );
	else
		return file;
}

/**
 *	Retrieves the path from a file.
 *
 *	@param file file to get path of.
 *
 *	@return path string
 *
 */
const std::string BWResource::getFilePath( const std::string& file )
{
	std::string directory;
	std::string nFile = file;

    if ( file.length() >= 1 && file[file.length()-1] != '/' )
    {
    	int pos = file.find_last_not_of( " " );

        if ( 0 <= pos && pos < (int)file.length() )
            nFile = file.substr( 0, pos );
    }

    int pos = nFile.find_last_of( "\\/" );

	if (  0 <= pos && pos < (int)nFile.length( ) )
		directory = nFile.substr( 0, pos );

	return formatPath( directory );
}

/**
 *	Retrieves the extension of a filename.
 *
 *	@param file filename string
 *
 *	@return extension string
 *
 */
const std::string BWResource::getExtension( const std::string& file )
{
	std::basic_string <char>::size_type pos = file.find_last_of( "." );
	if ( pos != std::string::npos )
		return file.substr( pos + 1 );

	// no extension
	return "";
}

/**
 *	Removes the extension from the filename
 *
 *	@param file filename string
 *
 *	@return filename minus any extension
 *
 */
const std::string BWResource::removeExtension( const std::string& file )
{
	std::basic_string <char>::size_type pos = file.find_last_of( "." );
	if ( pos != std::string::npos )
		return file.substr( 0, pos );

	// no extension
	return file;
}

/**
 *	Removes the extension from the filename and replaces it with the given
 *	extension
 *
 *	@param file filename string
 *	@param newExtension the new extension to give the file, including the
 *	initial .
 *
 *	@return file with its extension changed to newExtension
 *
 */
const std::string BWResource::changeExtension( const std::string& file,
													const std::string& newExtension )
{
	return removeExtension( file ) + newExtension;
}

/**
 *	This method makes certain that the path contains a trailing folder separator.
 *
 *	@param path 	The path string to be formatted.
 *
 *	@return The path with a trailing folder separator.
 */
std::string BWResource::formatPath( const std::string& path )
{
	std::string tmpPath( path.c_str( ) );
	if ( path.empty() )
		return tmpPath;

	std::replace( tmpPath.begin(), tmpPath.end(), '\\', '/' );

	if ( tmpPath[tmpPath.size( ) - 1] != FOLDER_SEPERATOR )
		tmpPath += FOLDER_SEPERATOR;

	std::string result( tmpPath.c_str( ) );

	return result;
}

/**
 * Sets the current directory
 *
 * @param currentDirectory path to make the current directory
 *
 * @returns true if successful
 */
bool BWResource::setCurrentDirectory(
		const std::string &currentDirectory )
{
#if defined(_WIN32) && !defined(_XBOX)
	return SetCurrentDirectory( currentDirectory.c_str( ) ) ? true : false;
#else
	// TODO:PM May want to implement non-Windows versions of these.
	(void) currentDirectory;

	return false;
#endif
}


/**
 * Checks for the existants of a drive letter
 *
 * @returns true if a drive is supplied in the path
 */
bool BWResourceImpl::hasDriveInPath( const std::string& path )
{
	return
    	strchr( path.c_str(), ':' ) != NULL ||					// has a drive colon
    	( path.c_str()[0] == '\\' || path.c_str()[0] == '/' );	// starts with a root char
}

/**
 * Check is the path is relative and not absolute.
 *
 * @returns true if the path is relative
 */
bool BWResourceImpl::pathIsRelative( const std::string& path )
{
	// has no drive letter, and no root directory
	return !hasDriveInPath( path ) && ( path.c_str()[0] != '/' && path.c_str()[0] != '\\');
}

/**
 * Returns the first path in the paths.xml file
 *
 * @returns first path in paths.xml
 */
const std::string BWResource::getDefaultPath( void )
{
	return instance().pimpl_->paths_.empty() ? std::string( "" ) : instance().pimpl_->paths_[0];
}

/**
 * Returns the path in the paths.xml file at position index
 *
 * @returns path at position index in paths.xml
 */
const std::string BWResource::getPath( int index )
{
	if (!instance().pimpl_->paths_.empty() &&
			0 <= index && index < (int)instance().pimpl_->paths_.size() )
	{
		return instance().pimpl_->paths_[index];
	}
	else
		return std::string( "" );
}

int BWResource::getPathNum()
{
	return int( instance().pimpl_->paths_.size() );
}

std::string BWResource::getPathAsCommandLine()
{
	std::string commandLine;
	int totalPathNum = getPathNum();
	// generally we should only have the game path & bigworld path.
	// so we remove the automatically appended path
	if( totalPathNum > 2 )
		--totalPathNum;
	for( int i = 0; i < totalPathNum; ++i )
	{
		if( commandLine.empty() )
			commandLine = "--res ";
		commandLine += getPath( i );
		if( i != totalPathNum - 1 )
			commandLine += ';';
	}
	return commandLine;
}

/**
 * Ensures that the path exists
 *
 */
bool BWResource::ensurePathExists( const std::string& path )
{
	std::string nPath = path;
    std::replace( nPath.begin(), nPath.end(), '/', '\\' );

    if ( nPath.empty() )
		return false; // just to be safe

    if ( nPath[nPath.size()-1] != '\\' )
		nPath = getFilePath( nPath );

    std::replace( nPath.begin(), nPath.end(), '/', '\\' );

	// skip drive letter, if using absolute paths
	uint poff = 0;
	if ( nPath.size() > 1 && nPath[1] == ':' )
		poff = 2;

	while (1)
	{
		poff = nPath.find_first_of( '\\', poff + 1 );
		if (poff > nPath.length())
			break;

		std::string subpath = nPath.substr( 0, poff );
		instance().fileSystem()->makeDirectory( subpath.c_str() );
	}
	return true;
}

bool BWResource::isValid()
{
	return instance().pimpl_->paths_.size( ) != 0;
}

std::string BWResourceImpl::formatSearchPath( const std::string& path, const std::string& pathXml )
{
	std::string tmpPath( path.c_str() );

#ifdef _WIN32
	// get the applications path and load the config file from it
	std::string appDirectory;

	if (pathXml != "")
		appDirectory = pathXml;
	else
		appDirectory = BWResourceImpl::getAppDirectory();

	// Does it contains a drive letter?
	if (tmpPath.size() > 1 && tmpPath[1] == ':')
	{
		MF_ASSERT(isalpha(tmpPath[0]));
	}
	// Is it a relative tmpPath?
	else if (tmpPath.size() > 0 && tmpPath[0] != '/')
	{
		char fullPath[MAX_PATH];
		tmpPath = appDirectory + tmpPath;
		// make sure slashes are windows-style
		std::replace( tmpPath.begin(), tmpPath.end(), '/', '\\' );
		// canonicalize (that is, remove redundant relative path info)
		if ( PathCanonicalize( fullPath, tmpPath.c_str() ) )
			tmpPath = fullPath;
	}
	else
	{
		char fullPath[MAX_PATH];
		// make sure slashes are windows-style
		std::replace( tmpPath.begin(), tmpPath.end(), '/', '\\' );
		// get absolute path
		char* filePart;
		if ( GetFullPathName( tmpPath.c_str(), sizeof( fullPath ), fullPath, &filePart ) )
			tmpPath = fullPath;
#ifndef EDITOR_ENABLED
			// not editor enabled, but win32, so remove drive introduced by
			// GetFullPathName to be consistent.
			tmpPath = BWResolver::removeDrive( tmpPath );
#endif //  EDITOR_ENABLED
	}
#endif // _WIN32

	// Forward slashes only
	std::replace( tmpPath.begin(), tmpPath.end(), '\\', '/' );

	// Trailing slashes aren't allowed
	if (tmpPath[tmpPath.size() - 1] == '/')
		tmpPath = tmpPath.substr( 0, tmpPath.size() - 1 );

	return tmpPath;
}

void BWResource::addPath( const std::string& path, int atIndex )
{
	std::string fpath = BWResourceImpl::formatSearchPath( path );

	BWResourceImpl* pimpl = instance().pimpl_;
	if (uint(atIndex) >= pimpl->paths_.size())
		atIndex = pimpl->paths_.size();
	pimpl->paths_.insert( pimpl->paths_.begin()+atIndex, fpath );

	INFO_MSG( "Added res path: \"%s\"\n", fpath.c_str() );

	if ( instance().fileSystem() )
		instance().refreshPaths(); // already created fileSystem_, so recreate them
}

/**
 *	Add all the PATH_SEPERATOR separated paths in path to the search path
 */
void BWResource::addPaths( const std::string& path, const char* desc, const std::string& pathXml )
{
	std::string buf = path;
	char* tok = strtok( const_cast<char*>( buf.c_str() ), PATH_SEPARATOR );

	while (tok != 0)
	{
		// Trim leading white space
		while (isspace( *tok ))
			++tok;

		// Trim trailing white space
		char* last = tok + strlen( tok ) - 1;
		while (last > tok && isspace( *last ))
			*last-- = '\0';

		// If it's not empty
		if (last > tok)
		{
			std::string path = BWResourceImpl::formatSearchPath( tok, pathXml );

			if (instance().pimpl_->nativeFileSystem_->getFileType( path, 0 ) ==
					IFileSystem::FT_NOT_FOUND)
			{
#ifdef MF_SERVER
				ERROR_MSG( "BWResource::addPaths: "
							"Resource path does not exist: %s\n",
						path.c_str() );
#else
				CRITICAL_MSG( "Search path does not exist: %s"
						RES_PATH_HELP, path.c_str() );
#endif
			}

			instance().pimpl_->paths_.push_back( path );
			INFO_MSG( "Added res path: \"%s\" (%s).\n", path.c_str(), desc );
		}

		tok = strtok( 0, PATH_SEPARATOR );
	}

#if defined( _EVALUATION )
    STRINGVECTOR::iterator it  = instance().paths_.begin( );
    STRINGVECTOR::iterator end = instance().paths_.end( );
    while ( it != end ) {
		if ( it->find( "bigworld\\res" ) != std::string::npos &&
			std::distance( it, end ) > 1 )
		{
			CRITICAL_MSG(
				"<bigworld/res> is not the last entry in the search path (%s)."
				"This is usually an error. Press 'No' to ignore and proceed "
				"(you may experience errors or other unexpected behavior). %d"
				RES_PATH_HELP, path.c_str(), std::distance( it, end ) );
		}
		++it;
	}
#endif

	if ( instance().fileSystem() )
		instance().refreshPaths(); // already created fileSystem_, so recreate them
}

/**
 *	Compares the modification dates of two files, and returns
 *	true if the first file 'oldFile' is indeed older (by at least
 *	mustBeOlderThanSecs seconds) than the second file 'newFile'.
 *	Returns false on error.
 */
bool BWResource::isFileOlder( const std::string & oldFile,
	const std::string & newFile, int mustBeOlderThanSecs )
{
	bool result = false;

	uint64 ofChanged = modifiedTime( oldFile );
	uint64 nfChanged = modifiedTime( newFile );

	if (ofChanged != 0 && nfChanged != 0)
	{
		uint64	mbotFileTime = uint64(mustBeOlderThanSecs) * uint64(10000000);
		result = (ofChanged + mbotFileTime < nfChanged);
	}

	return result;
}


/**
 *	Returns the time the given file (or resource) was modified.
 *	Returns 0 on error (e.g. the file does not exist)
 *  Only files are handled for backwards compatibility.
 */
uint64 BWResource::modifiedTime( const std::string & fileOrResource )
{
#ifdef EDITOR_ENABLED
	std::string filename = BWResourceImpl::hasDriveInPath( fileOrResource ) ?
		fileOrResource : resolveFilename( fileOrResource );
#else
	std::string filename = fileOrResource;
#endif

	IFileSystem::FileInfo fi;
	if ( instance().fileSystem()->getFileType( filename, &fi ) == IFileSystem::FT_FILE )
		return fi.modified;

	return 0;
}

#ifdef _WIN32
/*static*/ const std::string BWResource::appDirectory()
{
	return BWResourceImpl::getAppDirectory();
}
#endif // _WIN32


std::ostream& operator<<(std::ostream& o, const BWResource& /*t*/)
{
	o << "BWResource\n";
	return o;
}



// -----------------------------------------------------------------------------
// Section: BWResolver
// -----------------------------------------------------------------------------

/**
 *	Finds a file by looking in the search paths
 *
 *	Path Search rules.
 *		If the files path has a drive letter in it, then only the filename is
 *		used. The files path must be relative and NOT start with a / or \ to use
 *		the search path.
 *
 *		If either of the top 2 conditions are true or the file is not found with
 *		its own path appended to the search paths then the file is searched for
 *		using just the filename.
 *
 *	@param file the files name
 *
 *	@return files full path and filename
 *
 */
const std::string BWResolver::findFile( const std::string& file )
{
	std::string tFile;
	bool found = false;

	// is the file relative and have no drive letter
	if ( !BWResourceImpl::hasDriveInPath( file ) && BWResourceImpl::pathIsRelative( file ) )
	{
		STRINGVECTOR::iterator it  = BWResource::instance().pimpl_->paths_.begin( );
		STRINGVECTOR::iterator end = BWResource::instance().pimpl_->paths_.end( );

		// search for the file in the supplied paths
		while( !found && it != end )
		{
			tFile  = BWResource::formatPath((*it)) + file;
			found = BWResource::fileAbsolutelyExists( tFile );
			it++;
		}

		// the file doesn't exist, let's see about its directory
		if (!found)
		{
			std::string dir = BWResource::getFilePath( file );

			STRINGVECTOR::iterator it = BWResource::instance().pimpl_->paths_.begin( );
            for (; it != end; ++it)
			{
				std::string path = std::string( BWResource::formatPath((*it)) );
				found = BWResource::fileAbsolutelyExists( path + dir + '.' );
				if (found)
				{
					tFile = path + file;
					break;
				}
			}
		}
	}
	else
	{

		// the file is already resolved!
		tFile = file;
		found = true;
	}

	if ( !found )
	{
    	tFile  = BWResource::formatPath( BWResource::getDefaultPath() ) + file;
    }

	return tFile;
}

/**
 *	Resolves a file into an absolute filename based on the search paths
 *	@see findFile()
 *
 *	@param file the files name
 *
 *	@return the resolved filename
 *
 */
const std::string BWResolver::resolveFilename( const std::string& file )
{
	std::string tmpFile = file;
    std::replace( tmpFile.begin(), tmpFile.end(), '\\', '/' );
    std::string foundFile = tmpFile;

    foundFile = findFile( tmpFile );

    if ( strchr( foundFile.c_str(), ':' ) == NULL && ( foundFile[0] == '/' || foundFile[0] == '\\' ) )
    	foundFile = appDrive_ + foundFile;

#ifdef LOGGING
    if( file != foundFile && !fileExists( foundFile ) )
    {
        dprintf( "BWResource - Invalid File: %s \n\tResolved to %s\n", file.c_str(), foundFile.c_str() );
	    std::ofstream os( "filename.log", std::ios::app | std::ios::out );
    	os << "Error resolving file: " << file << "\n";
    }
#endif

	return foundFile;
}

/**
 *	Dissolves a full filename (with drive and path information) to relative
 *	filename.
 *
 *	@param file the files name
 *
 *	@return the dissolved filename, "" if filename cannot be dissolved
 *
 */
const std::string BWResolver::dissolveFilename( const std::string& file )
{
	std::string newFile = file;
    std::replace( newFile.begin(), newFile.end(), '\\', '/' );

	std::string path;
	bool found = false;

	// is the filename valid
	// we can only dissolve an absolute path (has a root), and a drive specified
	// is optional.

    if ( !BWResourceImpl::hasDriveInPath( newFile ) && BWResourceImpl::pathIsRelative( newFile ) )
        return newFile;

    STRINGVECTOR::iterator it  = BWResource::instance().pimpl_->paths_.begin( );
    STRINGVECTOR::iterator end = BWResource::instance().pimpl_->paths_.end( );

    // search for the file in the supplied paths
    while( !found && it != end )
    {
        // get the search path
        path   = std::string( BWResource::formatPath((*it)) );

        // easiest test: is the search path a substring of the file
        if ( mf_pathcmp( path.c_str(), newFile.substr( 0, path.length( ) ).c_str() ) == 0 )
        {
            newFile = newFile.substr( path.length( ), file.length( ) );
            found = true;
        }

        if ( !found )
        {
            // is the only difference the drive specifiers?

#ifdef _WIN32
			// remove drive letters if in Win32
            std::string sPath = removeDrive( path );
			std::string fDriveLess = removeDrive( newFile );
#else
            std::string sPath = path;
			std::string fDriveLess = newFile;
#endif
            // remove the filenames
            sPath = BWResource::formatPath( sPath );
			std::string fPath = BWResource::getFilePath( fDriveLess );

            if ( ( fPath.length( ) > sPath.length() ) &&
				( mf_pathcmp( sPath.c_str(), fPath.substr( 0, sPath.length() ).c_str() ) == 0 ) )
            {
                newFile = fDriveLess.substr( sPath.length(), file.length() );
                found = true;
            }
        }

        // point to next search path
        it++;
    }

    if ( !found )
    {
        // there is still more we can do!
        // we can look for similar paths in the search path and the
        // file's path
        // eg. \demo\zerodata\scenes & m:\bigworld\zerodata\scenes
        // TODO: implement this feature if required

        // the file is invalid
#ifdef LOGGING
        dprintf( "BWResource - Invalid: %s\n", file.c_str() );
	    std::ofstream os( "filename.log", std::ios::app | std::ios::out );
    	os << "Error dissolving file: " << file << "\n";
        newFile = "";
#endif
    }


	return newFile;
}

/**
 *	Removes the drive specifier from the filename
 *
 *	@param file filename string
 *
 *	@return filename minus any drive specifier
 *
 */
const std::string BWResolver::removeDrive( const std::string& file )
{
	std::string tmpFile( file );

    int firstOf = tmpFile.find_first_of( ":" );
	if ( 0 <= firstOf && firstOf < (int)tmpFile.size( ) )
		tmpFile = tmpFile.substr( firstOf + 1 );

	return tmpFile;
}

void BWResolver::defaultDrive( const std::string& drive )
{
    STRINGVECTOR::iterator it  = BWResource::instance().pimpl_->paths_.begin( );
    STRINGVECTOR::iterator end = BWResource::instance().pimpl_->paths_.end( );

	BWResource::instance().pimpl_->defaultDrive_ = drive;

    // search for the file in the supplied paths
    while( it != end )
    {
        // get the search path
        std::string path = *it;

        if ( path.c_str()[1] != ':' )
        {
            path = BWResource::instance().pimpl_->defaultDrive_ + path;
            (*it) = path;
        }

        ++it;
    }
}
