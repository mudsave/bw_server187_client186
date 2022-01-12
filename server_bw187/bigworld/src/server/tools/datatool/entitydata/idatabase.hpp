/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef IDATABASE_HPP
#define IDATABASE_HPP

#include "network/basictypes.hpp"
#include "common/login_interface.hpp"
#include "server/backup_hash.hpp"

#include <string>
#include <limits>

class EntityDescriptionMap;
class EntityDefs;

typedef LogOnStatus DatabaseLoginStatus;
typedef std::vector<EntityTypeID> TypeIDVec;
typedef std::map< EntityTypeID, EntityTypeID > TypeIDTypeIDMap;

const uint32 INVALID_RESOURCE_VERSION = std::numeric_limits<uint32>::max();

/**
 *	This enum defines the level of modifications we can make when trying to
 * 	sync the defs with the database tables.
 */
enum DefsSyncMode
{
	DefsSyncModeInvalid = -1,
	DefsSyncModeNoSync = 0,
	DefsSyncModeAddOnly = 1,
	DefsSyncModeAddAndUpdate = 2,
	DefsSyncModeFullSync = 3,
	DefsSyncModeMax
};

/**
 *	This struct is a key to an entity record in the database
 */
struct EntityDBKey
{
	EntityDBKey( EntityTypeID _typeID, DatabaseID _dbID,
			const std::string & s = std::string() ) :
		typeID( _typeID ), dbID( _dbID ), name( s ) {}
	EntityTypeID	typeID;	///< always required
	DatabaseID		dbID;	///< preferred but optional
	std::string		name;	///< used if dbID is zero
};

#endif // IDATABASE_HPP
