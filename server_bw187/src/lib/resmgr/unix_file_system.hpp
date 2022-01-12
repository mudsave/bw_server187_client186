/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef _UNIX_FILE_SYSTEM_HEADER
#define _UNIX_FILE_SYSTEM_HEADER

#include "file_system.hpp"

/**
 *	This class provides an implementation of IFileSystem that
 *	reads from a Unix filesystem.	
 */	
class UnixFileSystem : public IFileSystem
{
public:	
	UnixFileSystem(const std::string& basePath);
	~UnixFileSystem();

	virtual FileType	getFileType(const std::string& path, FileInfo * pFI);
	virtual std::string	eventTimeToString( uint64 eventTime );

	virtual Directory	readDirectory(const std::string& path);
	virtual BinaryPtr	readFile(const std::string& path);

	virtual bool		makeDirectory(const std::string& path);
	virtual bool		writeFile(const std::string& path, 
							BinaryPtr pData, bool binary);
	virtual bool		moveFileOrDirectory( const std::string & oldPath,
							const std::string & newPath );
	virtual bool		eraseFileOrDirectory( const std::string & path );

	virtual FILE *		posixFileOpen( const std::string & path,
							const char * mode );

	virtual IFileSystem* clone();

private:
	std::string			basePath_;
};

#endif
