/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef _ZIP_FILE_SYSTEM_HEADER
#define _ZIP_FILE_SYSTEM_HEADER

#include "file_system.hpp"
#include <map>
#include <string>
#include <stdio.h>
#include "cstdmf/concurrency.hpp"

/**
 *	This class provides an implementation of IFileSystem
 *	that reads from a zip file.	
 */	
class ZipFileSystem : public IFileSystem 
{
public:
	ZipFileSystem( const std::string& zipFile );
	~ZipFileSystem();

	virtual FileType	getFileType(const std::string& path,
							FileInfo * pFI = NULL );
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

	virtual IFileSystem*	clone();

private:
	SimpleMutex mutex_; // to make sure only one thread access the zlib stream
	typedef std::map<std::string, long> FileMap;
	typedef std::map<std::string, Directory > DirMap;

	std::string			path_;

	FileMap				fileMap_;
	DirMap				dirMap_;
	FILE*				pFile_;

	bool				openZip(const std::string& path);	
	void				closeZip();

	ZipFileSystem( const ZipFileSystem & other );
	ZipFileSystem & operator=( const ZipFileSystem & other );

	virtual BinaryPtr	readFileInternal(const std::string& path);
};

#endif
