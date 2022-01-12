/**
 *  @brief      解析bw的定义
 *  @file       entitydefs.h
 *  @version    1.0.0
 *  @date       2011-01-29
 */
#ifndef ENTITYDEFS_H
#define ENTITYDEFS_H
#include "cstdmf/md5.hpp"
#include "network/channel.hpp"
#include "entitydef/entity_description_map.hpp"
#include "resmgr/datasection.hpp"
#include "common/doc_watcher.hpp"
#include "server/backup_hash.hpp"
#include "idatabase.hpp"

#include <map>
#include <set>

class RelogonAttemptHandler;
class DbUpdater;
class BWResource;

typedef Mercury::ChannelOwner BaseAppMgr;

const EntityTypeID INVALID_TYPEID = std::numeric_limits<uint16>::max();

/**
 *	This utility class destroy the objects pointed to by pointers stored
 *	in a std container.
 *	@note 从worker_thread.hpp拷贝过来的
 */
template <class CONTAINER>
struct auto_container
{
	CONTAINER	container;

	auto_container()	{}
	~auto_container()	{	clearContainer( container );	}

	static void clearContainer( CONTAINER& c )
	{
		for ( typename CONTAINER::iterator i = c.begin();
			i < c.end(); ++i )
			delete *i;
		c.clear();
	}

private:
	// Not copy-able.
	auto_container( const auto_container& );
	auto_container& operator=( auto_container& );
};

/**
 *	This class represents the entity definitions in DbMgr.
 */
class EntityDefs
{
	EntityDescriptionMap	entityDescriptionMap_;
	std::vector<bool>		entityPasswordBits_;
	MD5::Digest				md5Digest_;
	EntityTypeID			defaultTypeID_;
	std::vector< std::string >	nameProperties_;
	std::string				defaultNameProperty_;

public:
	EntityDefs() :
		entityDescriptionMap_(), entityPasswordBits_(), md5Digest_(),
		defaultTypeID_( INVALID_ENTITY_TYPE_ID ), nameProperties_(),
		defaultNameProperty_()
	{}

	bool init( DataSectionPtr pEntitiesSection,
		const std::string& defaultTypeName,
		const std::string& defaultNameProperty );

	const MD5::Digest getDigest() const 		{	return md5Digest_;	}
	const std::string& getNameProperty( EntityTypeID index ) const
	{	return nameProperties_[ index ];	}
	const std::string& getDefaultNameProperty() const
	{ 	return defaultNameProperty_;	}
	const EntityTypeID getDefaultType() const	{	return defaultTypeID_; 	}
	const std::string& getDefaultTypeName() const;

	bool entityTypeHasPassword( EntityTypeID typeID ) const
	{
		return entityPasswordBits_[typeID];
	}
	bool isValidEntityType( EntityTypeID typeID ) const
	{
		return (typeID < entityDescriptionMap_.size()) &&
				!entityDescriptionMap_.entityDescription( typeID ).
					isClientOnlyType();
	}
	EntityTypeID getEntityType( const std::string& typeName ) const
	{
		EntityTypeID typeID = INVALID_TYPEID;
		entityDescriptionMap_.nameToIndex( typeName, typeID );
		return typeID;
	}
	size_t getNumEntityTypes() const
	{
		return entityDescriptionMap_.size();
	}
	const EntityDescription& getEntityDescription( EntityTypeID typeID ) const
	{
		return entityDescriptionMap_.entityDescription( typeID );
	}
	//	This function returns the type name of the given property.
	std::string getPropertyType( EntityTypeID typeID,
		const std::string& propName ) const;


	void debugDump( int detailLevel ) const;
	bool hasMatchingNameProperties( const EntityDefs& other ) const;
};

#endif
