/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef _FILE_SYSTEM_HEADER
#define _FILE_SYSTEM_HEADER

#include "binary_block.hpp"
#include <string>
#include <vector>
#include "cstdmf/stdmf.hpp"

/**
 *	This class provides an abstract interface to a filesystem.
 */	
class IFileSystem
{
public:
	/**
	 *	This typedef represents a directory of filenames.
	 */	
	typedef std::vector<std::string> Directory;

	/**
	 *	This enumeration describes a file type.
	 */	 
	enum FileType
	{
		FT_NOT_FOUND,
		FT_DIRECTORY,
		FT_FILE
	};

	/**
	 *	This structure provides extra info about a file
	 */
	struct FileInfo
	{
		uint64 size;
		uint64 created;
		uint64 modified;
		uint64 accessed;
	};

	virtual ~IFileSystem() {}

	virtual FileType	getFileType( const std::string & path,
							FileInfo * pFI = NULL ) = 0;
	virtual std::string	eventTimeToString( uint64 eventTime ) = 0;

	virtual Directory	readDirectory( const std::string & path ) = 0;
	virtual BinaryPtr	readFile( const std::string & path ) = 0;

	virtual bool		makeDirectory( const std::string & path ) = 0;
	virtual bool		writeFile( const std::string & path, 
							BinaryPtr pData, bool binary ) = 0;
	virtual bool		moveFileOrDirectory( const std::string & oldPath,
							const std::string & newPath ) = 0;
	virtual bool		eraseFileOrDirectory( const std::string & path ) = 0;

	virtual FILE *		posixFileOpen( const std::string & path,
							const char * mode ) { return NULL; }

	virtual IFileSystem*	clone() = 0;

};

/**
 *	The file system that is native to the platform being compiled for
 */
namespace NativeFileSystem
{
	IFileSystem * create( const std::string & path );
};

#endif
