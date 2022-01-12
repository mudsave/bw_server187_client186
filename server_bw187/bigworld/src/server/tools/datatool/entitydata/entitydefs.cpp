/**
 *  @brief      解析bw的定义
 *  @file       entitydefs.cpp
 *  @version    1.0.0
 *  @date       2011-01-29
 */
#include "Python.h"
#include "entitydefs.h"
#include "baseappmgr/baseappmgr_interface.hpp"
#include "baseapp/baseapp_int_interface.hpp"
#include "updater/updater_interface.hpp"

#include "cstdmf/memory_stream.hpp"

#include "resmgr/bwresource.hpp"
#include "resmgr/dataresource.hpp"
#include "resmgr/bwresource.hpp"

#include "server/bwconfig.hpp"
#include "server/reviver_subject.hpp"
#include "server/writedb.hpp"

#include "network/watcher_glue.hpp"

#include "entitydef/entity_description_debug.hpp"

#include "pyscript/py_output_writer.hpp"


#include "resmgr/xml_section.hpp"

#include <signal.h>

DECLARE_DEBUG_COMPONENT( 0 )

// -----------------------------------------------------------------------------
// Section: Constants
// -----------------------------------------------------------------------------
static const char * DEFAULT_ENTITY_TYPE_STR = "Avatar";
static const char * DEFAULT_NAME_PROPERTY_STR = "playerName";
static const char * UNSPECIFIED_ERROR_STR = "Unspecified error";
static const std::string EMPTY_STR;



// extern int Math_token;
extern int ResMgr_token;
static int s_moduleTokens =
	/* Math_token |*/ ResMgr_token;
extern int PyPatrolPath_token;
static int s_patrolToken = PyPatrolPath_token;

// -----------------------------------------------------------------------------
// Section: EntityDefs
// -----------------------------------------------------------------------------
/**
 * 	This function initialises EntityDefs. Must be called once and only once for
 * 	each instance of EntityDefs.
 *
 * 	@param	pEntitiesSection	The entities.xml data section.
 * 	@param	defaultTypeName		The default entity type.
 * 	@param	defaultNameProperty		The default name property.
 * 	@return	True if successful.
 */
bool EntityDefs::init( DataSectionPtr pEntitiesSection,
	const std::string& defaultTypeName, const std::string& defaultNameProperty )
{
	struct LocalHelperFunctions
	{
		// Sets output to dataDesc.name() only if it is a STRING or BLOB.
		void setNameProperty( std::string& output,
				const DataDescription& dataDesc,
				const EntityDescription& entityDesc )
		{
			const DataType* pDataType = dataDesc.dataType();
			if ((strcmp( pDataType->pMetaDataType()->name(), "BLOB" ) == 0) ||
				(strcmp( pDataType->pMetaDataType()->name(), "STRING" ) == 0))
			{
				output = dataDesc.name();
			}
			else
			{
				ERROR_MSG( "EntityDefs::init: Identifier must be of "
						"STRING or BLOB type. Identifier '%s' "
						"for entity type '%s' is ignored\n",
						dataDesc.name().c_str(),
						entityDesc.name().c_str() );
			}
		}
	} helper;

	MF_ASSERT( (entityDescriptionMap_.size() == 0) && pEntitiesSection );
	if (!entityDescriptionMap_.parse( pEntitiesSection ))
	{
		ERROR_MSG( "EntityDefs::init: "
				"Could not parse 'entities/entities.xml'\n" );
		return false;
	}

	defaultNameProperty_ = defaultNameProperty;

	// Set up some entity def stuff specific to DbMgr
	nameProperties_.resize( entityDescriptionMap_.size() );
	entityPasswordBits_.resize( entityDescriptionMap_.size() );
	for ( EntityTypeID i = 0; i < entityDescriptionMap_.size(); ++i )
	{
		const EntityDescription& entityDesc =
				entityDescriptionMap_.entityDescription(i);
		// Remember whether it has a password property.
		entityPasswordBits_[i] = (entityDesc.findProperty( "password" ) != 0);

		// Find its name property.
		const DataDescription* 	pDefaultNameDataDesc = NULL;
		std::string&			nameProperty = nameProperties_[i];
		for ( unsigned int i = 0; i < entityDesc.propertyCount(); ++i)
		{
			const DataDescription* pDataDesc = entityDesc.property( i );
			if (pDataDesc->isIdentifier())
			{
				if (nameProperty.empty())
				{
					helper.setNameProperty( nameProperty, *pDataDesc,
							entityDesc );
				}
				else // We don't support having multiple to name columns.
				{
					ERROR_MSG( "EntityDefs::init: Multiple identifiers for "
							"one entity type not supported. Identifier '%s' "
							"for entity type '%s' is ignored\n",
							pDataDesc->name().c_str(),
							entityDesc.name().c_str() );
				}
			}
			else if ( pDataDesc->name() == defaultNameProperty )
			{	// For backward compatiblity, we use the default name property
				// if none of the properties of the entity is an identifier.
				pDefaultNameDataDesc = pDataDesc;
			}
		}
		// Backward compatibility.
		if (nameProperty.empty() && pDefaultNameDataDesc)
		{
			helper.setNameProperty( nameProperty, *pDefaultNameDataDesc,
					entityDesc );
		}
	}

	entityDescriptionMap_.nameToIndex( defaultTypeName, defaultTypeID_ );

	MD5 md5;
	entityDescriptionMap_.addToMD5( md5 );
	md5.getDigest( md5Digest_ );

	return true;
}

const std::string& EntityDefs::getDefaultTypeName() const
{
	return (this->isValidEntityType( this->getDefaultType() )) ?
				this->getEntityDescription( this->getDefaultType() ).name() :
				EMPTY_STR;
}

/**
 *	This function returns the type name of the given property.
 */
std::string EntityDefs::getPropertyType( EntityTypeID typeID,
	const std::string& propName ) const
{
	const EntityDescription& entityDesc = this->getEntityDescription(typeID);
	DataDescription* pDataDesc = entityDesc.findProperty( propName );
	return ( pDataDesc ) ? pDataDesc->dataType()->typeName() : std::string();
}

/**
 *	This function prints out information about the entity defs.
 */
void EntityDefs::debugDump( int detailLevel ) const
{
	EntityDescriptionDebug::dump( entityDescriptionMap_, detailLevel );
}

/**
 *	This function checks that all the entities with name properties has the
 * 	same name properties in the other entity defs. Mainly used in updater
 * 	because we currently do not support migrating the name property.
 */
bool EntityDefs::hasMatchingNameProperties( const EntityDefs& other ) const
{
	EntityTypeID ourMaxTypeID = (EntityTypeID) this->getNumEntityTypes();
	for ( EntityTypeID ourTypeID = 0; ourTypeID < ourMaxTypeID; ++ourTypeID )
	{
		const EntityDescription& ourEntityDesc =
				entityDescriptionMap_.entityDescription( ourTypeID );
		EntityTypeID theirTypeID = other.getEntityType( ourEntityDesc.name() );
		if ((theirTypeID != INVALID_TYPEID) &&
			(this->getNameProperty( ourTypeID ) !=
				other.getNameProperty( theirTypeID )))
		{
			return false;
		}
	}

	return true;
}
// database.cpp
