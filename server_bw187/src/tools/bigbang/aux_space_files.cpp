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

#include "aux_space_files.hpp"
#include "big_bang.hpp"

#include "chunk/chunk_space.hpp"
#include "resmgr/bwresource.hpp"
#include "pyscript/py_data_section.hpp"

// -----------------------------------------------------------------------------
// Section: AuxSpaceFiles
// -----------------------------------------------------------------------------


/**
 *	Constructor
 */
AuxSpaceFiles::AuxSpaceFiles()
{
	mappingPath_ = BigBang::instance().chunkDirMapping()->path();
	mappingPath_.erase( mappingPath_.end()-1 );	// remove trailing slash

	// record all aux files
	DataSectionPtr pSpaceSect = BWResource::openSection( mappingPath_ );
	for (int i = 0; i < pSpaceSect->countChildren(); i++)
	{
		std::string aname = pSpaceSect->childSectionName( i );
		if (this->hasEligibleExtension( aname ))
		{
			FileInfo fi;
			fi.name_ = aname;
			fi.alive_ = true;
			fi.backed_ = true;
			fi.data_ = NULL;
			info_.insert( fi );
		}
	}
}

/**
 *	This method adds a file to the list of auxilliary files maintained
 *	by BigBang in the space
 */
bool AuxSpaceFiles::add( const std::string & name )
{
	if (!this->hasEligibleExtension( name )) return false;

	FileInfoSet::iterator found = info_.find( (FileInfo&)name );
	if (found != info_.end())
	{
		if (!found->alive_)
		{
			found->alive_ = true;
			found->backed_ = false;	// so new section made to replace it
			found->data_ = NULL;
		}
		return true;
	}
	else
	{
		FileInfo fi;
		fi.name_ = name;
		fi.alive_ = true;
		fi.backed_ = false;
		fi.data_ = NULL;
		info_.insert( fi );
	}
	return true;
}

/**
 *	This method adds a file to the list of auxilliary files maintained
 *	by BigBang in the space
 */
static bool addSpaceFile( const std::string & name )
{
	bool ok = AuxSpaceFiles::instance().add( name );
	if (!ok)
	{
		PyErr_SetString( PyExc_ValueError, "BigBang.addSpaceFile() "
			"File has an ineligible extension" );
	}
	return ok;
}
PY_AUTO_MODULE_FUNCTION( RETOK, addSpaceFile, ARG( std::string, END ), BigBang )


/**
 *	This method gets a file from the list of auxilliary files maintained
 *	by BigBang in the space
 */
DataSectionPtr AuxSpaceFiles::get( const std::string & name )
{
	FileInfoSet::iterator found = info_.find( (FileInfo&)name );
	if (found == info_.end()) return NULL;

	FileInfo & fi = *found;
	if (!fi.alive_) return NULL;

	// see if we haven't already accessed it
	if (!fi.data_)
	{
		DataSectionPtr pSpaceSect = BWResource::openSection( mappingPath_ );

		if (!fi.backed_)
		{		// either make a blank one
			fi.data_ = pSpaceSect->newSection( name );
			MF_ASSERT( fi.data_ );
		}
		else
		{		// or read in an existing one
			fi.data_ = pSpaceSect->openSection( name );
		}
	}

	return fi.data_;
}

/**
 *	This method gets a file from the list of auxilliary files maintained
 *	by BigBang in the space
 */
static PyObject * getSpaceFile( const std::string & name )
{
	DataSectionPtr pDS = AuxSpaceFiles::instance().get( name );
	if (!pDS)
	{
		PyErr_SetString( PyExc_ValueError, "BigBang.getSpaceFile() "
			"File not found" );
		return NULL;
	}

	return new PyDataSection( pDS );
}
PY_AUTO_MODULE_FUNCTION(
	RETOWN, getSpaceFile, ARG( std::string, END ), BigBang )


/**
 *	This method deletes a file from the list of auxilliary files maintained
 *	by BigBang in the space
 */
bool AuxSpaceFiles::del( const std::string & name )
{
	FileInfoSet::iterator found = info_.find( (FileInfo&)name );
	if (found == info_.end()) return false;

	FileInfo & fi = *found;
	if (!fi.alive_) return false;

	fi.alive_ = false;
	return true;
}

/**
 *	This method deletes a file from the list of auxilliary files maintained
 *	by BigBang in the space
 */
static bool delSpaceFile( const std::string & name )
{
	bool ok = AuxSpaceFiles::instance().del( name );
	if (!ok)
	{
		PyErr_SetString( PyExc_ValueError, "BigBang.delSpaceFile() "
			"File not found" );
	}
	return ok;
}
PY_AUTO_MODULE_FUNCTION( RETOK, delSpaceFile, ARG( std::string, END ), BigBang )


/**
 *	This method returns whether or not the given file has an extension eligible
 *	for being an aux space file
 */
bool AuxSpaceFiles::hasEligibleExtension( const std::string & name )
{
	std::string ext = BWResource::getExtension( name );
	return (!ext.empty() && ext != ".chunk" && ext != ".cdata" &&
		ext != ".terrain" && ext != ".settings" && ext != ".localsettings");
}

/**
 *	Sync the disk with the in-memory representation
 */
void AuxSpaceFiles::sync()
{
	FileInfoSet::iterator nx = info_.begin();
	for (FileInfoSet::iterator it = nx++; it != info_.end(); it = nx++)
	{
		FileInfo & fi = *it;

		std::string resName = mappingPath_ + "/" + fi.name_;

		// delete it if newly deleted
		if (!fi.alive_)
		{
			// erase it and remove it from cvs
			BigBang::instance().eraseAndRemoveFile( resName );
			// need to do this even if backed_ is false to handle
			// the 'del-add-del' case
			info_.erase( it );
			continue;
		}

		// deal with it if it hasn't been accessed
		if (!fi.data_)
		{
			if (fi.backed_)	// then do nothing
				continue;
			else			// unless newly added
				this->get( fi.name_ );
			MF_ASSERT( fi.data_ );
		}

		// save and out and take care of cvs
		BigBang::instance().saveAndAddChunk(
			resName, fi.data_, !fi.backed_, false );

		// and it is now backed
		fi.backed_ = true;

		// and clear our ptr so we don't save again next sync
		fi.data_ = NULL;
	}
}

/// Instance accessor
AuxSpaceFiles & AuxSpaceFiles::instance()
{
	if (s_instance_ == NULL) s_instance_ = new AuxSpaceFiles();
	return *s_instance_;
}

/// Sync only if instance constructed
void AuxSpaceFiles::syncIfConstructed()
{
	if (s_instance_ != NULL) s_instance_->sync();
}

/// Static initialiser
AuxSpaceFiles * AuxSpaceFiles::s_instance_ = NULL;


// aux_space_files.cpp
