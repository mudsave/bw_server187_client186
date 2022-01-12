/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CVSWRAPPER_HPP
#define CVSWRAPPER_HPP

#include <set>

class CVSWrapper
{
	std::string output_;
	std::string cvsPath_;
	std::string workingPath_;
	unsigned int batchLimit_;
	bool enabled_;

	enum Status
	{
		UNKNOWN = 0,
		ADDED,
		REMOVED,
		UPTODATE,
		LOCALMODIFIED,
		NEEDCHECKOUT
	};

	static bool isFile( const std::string& pathName );
	static bool isDirectory( const std::string& pathName );
	static bool exists( const std::string& pathName );
	bool exec( std::string cmd, std::string workingDir, int& exitCode, std::string& output );
public:
	CVSWrapper( const std::string& workingPath );

	void refreshFolder( const std::string& relativePathName );

	bool updateFiles( std::vector<std::string> filesToUpdate );
	bool updateFolder( const std::string& relativePathName );
	bool commitFiles( std::vector<std::string> filesToCommit, const std::string& commitMsg );
	bool enabled() const	{	return enabled_;	}

	bool isInCVS( const std::string& relativePathName );

	void removeFile( const std::string& relativePathName );
	bool addFolder( std::string relativePathName, const std::string& commitMsg );
	bool addFile( std::string relativePathName, bool isBinary );

	const std::string& output() const;
};

#endif // CVSWRAPPER_HPP
