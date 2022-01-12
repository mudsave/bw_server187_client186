/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef AUX_SPACE_FILES_HPP
#define AUX_SPACE_FILES_HPP

#include <set>
#include <string>

#include "cstdmf/smartpointer.hpp"

class DataSection;
typedef SmartPointer<DataSection> DataSectionPtr;

/**
 *	This class is a list of auxilliary files maintained by BigBang.
 *	They are maintained in memory and saved out and synced with cvs
 *	when the project is saved (with the 'sync' call)
 */
class AuxSpaceFiles
{
public:
	AuxSpaceFiles();

	bool add( const std::string & name );
	DataSectionPtr get( const std::string & name );
	bool del( const std::string & name );

	bool hasEligibleExtension( const std::string & name );

	void sync();	// sync to disk

	static AuxSpaceFiles & instance();
	static void syncIfConstructed();

private:
	std::string	mappingPath_;

	struct FileInfo
	{
		std::string		name_;
		bool			alive_;
		bool			backed_;
		DataSectionPtr	data_;

		bool operator <( const FileInfo & other ) const
		{
			return name_ < other.name_;
		}
	};
	typedef std::set<FileInfo> FileInfoSet;
	FileInfoSet	info_;

	static AuxSpaceFiles * s_instance_;
};


#endif // AUX_SPACE_FILES_HPP
