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
#include "res_mgr_script.hpp"

#include "py_data_section.hpp"

#include "resmgr/bwresource.hpp"

#ifndef CODE_INLINE
#include "res_mgr_script.ipp"
#endif


/*~ module ResMgr
 *	@components{ client, cell, base }
 */

// -----------------------------------------------------------------------------
// Section: Method definitions
// -----------------------------------------------------------------------------

/*~ function ResMgr.openSection
 *	@components{ client, cell, base }
 *
 *	This function opens the specified resource as a DataSection.  If the
 *	resource is not found, then it returns None. A new section can optionally
 *	be created by specifying true in the optional second argument.
 *
 *	Resources live in a res tree and include directories, xml files, xml nodes,
 *	normal data files, binary section data file nodes, etc.
 *
 *	@param	resourceID	the id of the resource to open.
 *	@param	newSection	Boolean value indicating whether to create this
 *						as a new section, default is False.
 *
 *	@return				the DataSection that was loaded, or None if the
 *						id was not found.
 */
/**
 *	This function implements a script function. It converts a data section into
 *	a hierarchy of Python maps (aka a DataSection)
 */
static PyObject * openSection( const std::string & resourceID,
	bool makeNewSection = false )
{
	DataSectionPtr pSection = BWResource::openSection(
		resourceID, makeNewSection );

	if (!pSection)
	{
		if (!makeNewSection)
		{
			Py_Return;	// just return None if there is no section
		}
		else
		{
			PyErr_Format( PyExc_ValueError, "ResMgr.openSection(): "
				"Could not make new section '%s'", resourceID.c_str() );
			return NULL;
		}
	}

	return new PyDataSection( pSection );
}
PY_AUTO_MODULE_FUNCTION( RETOWN, openSection,
	ARG( std::string, OPTARG( bool, false, END ) ), ResMgr )

/*~ function ResMgr.save
 *	@components{ client, cell, base }
 *
 *	This function saves the previously loaded section with the specified
 *	path.  If no section with that id is still in memory, then an IO error
 *	occurs, otherwise, the section is saved.
 *
 *	@param	resourceID	the filepath of the DataSection to save.
 */
/**
 *	This function implements a script function. It saves this section at the
 *	input path.
 */
static bool save( const std::string & resourceID )
{
	if (BWResource::instance().save( resourceID )) return true;

	PyErr_Format( PyExc_IOError, "Save of %s failed", resourceID.c_str() );
	return false;
}
PY_AUTO_MODULE_FUNCTION( RETOK, save, ARG( std::string, END ), ResMgr )

/*~ function ResMgr.purge
 *	@components{ client, cell, base }
 *
 *	This function purges the previously loaded section with the specified
 *	path from the cache and census. Optionally, all child sections can also
 *	be purged (only useful if the resource is a DirSection), by specifying
 *	true in the optional second argument.
 *
 *	@param	resourceID	the id of the resource to purge.
 *	@param	recurse		Boolean value indicating whether to recursively
 *						purge any subsections. default is False.
 */
/**
 *	This function implements a script function. It purges the given section
 *	from the cache, enabling the copy on disk to be read in.
 */
static void purge( const std::string & resourceID, bool recurse = false )
{
	BWResource::instance().purge( resourceID, recurse );
}
PY_AUTO_MODULE_FUNCTION( RETVOID, purge,
	ARG( std::string, OPTARG( bool, false, END ) ), ResMgr )


// -----------------------------------------------------------------------------
// Section: Initialisation
// -----------------------------------------------------------------------------

extern int PyDataSection_token;
extern int ResourceTable_token;
int ResMgr_token = PyDataSection_token | ResourceTable_token;

/*~ attribute ResMgr.root
 *	@components{ client, cell, base }
 *
 *	This is the root data section of the resource tree.
 *
 *	@type	DataSection
 */
/**
 *	This init time job adds the root attribute to the ResMgr module
 */
PY_MODULE_ATTRIBUTE( ResMgr, root,
	new PyDataSection( BWResource::instance().rootSection() ) );

// res_mgr_script.cpp
