/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/


#include "entity_type.hpp"
#include "entity.hpp"

#include "cstdmf/debug.hpp"

#include <Python.h>
#include "pyscript/script.hpp"
#include "common/servconn.hpp"

#include "resmgr/bwresource.hpp"


DECLARE_DEBUG_COMPONENT2( "Entity", 0 )

// Static variable instantiations
EntityType::Types *			EntityType::s_types_;
EntityDescriptionMap *		EntityType::s_entityDescriptionMap_;


/**
 *	Constructor (private)
 */
EntityType::EntityType( const EntityDescription & description ):
	description_( description )
{
}


/**
 *	Destructor.
 */
EntityType::~EntityType()
{
}


/**
 *	This static method returns the EntityType instance for a given type id.
 */
EntityType * EntityType::find( uint type )
{
	if (type >= s_types_->size())
	{
		CRITICAL_MSG( "EntityType::find: "
			"No such type index %d\n", type );
		return NULL;
	}
	return (*s_types_)[ type ];
}


/**
 *	This static method returns the EntityType instance for a given type name.
 */
EntityType * EntityType::find( const std::string & name )
{
	EntityTypeID	type = 0;

	if (!s_entityDescriptionMap_->nameToIndex( name, type ))
	{
		CRITICAL_MSG( "EntityType::find: "
			"No entity type named %s\n", name.c_str() );
		return NULL;
	}

	return EntityType::find(type);
}


/**
 *	This static method initialises the entity type stuff.
 */
bool EntityType::init( const std::string &clientPath, MD5::Digest& digest )
{
	// Construct statics
	s_types_ = new Types;
	s_entityDescriptionMap_ = new EntityDescriptionMap;

	// Initialise the entity descriptions.
	DataSectionPtr	pEntitiesList =
		BWResource::openSection( clientPath );

	if (pEntitiesList && s_entityDescriptionMap_->parse( pEntitiesList ))
	{
		// can check that entity definition is up to date here
	}
	else
	{
		CRITICAL_MSG( "EntityType::init: "
				"EntityDescriptionMap::parse failed\n" );
	}

	// Create an EntityType for each EntityDescription read in.
	int numEntityTypes = s_entityDescriptionMap_->size();
	s_types_->resize( numEntityTypes );

	int numClientTypes = 0;

	if (numEntityTypes <= 0)
	{
		ERROR_MSG( "EntityType::init: There are no entity descriptions!\n" );
		return false;
	}

	for (int i = 0; i < numEntityTypes; i++)
	{
		(*s_types_)[i] = NULL;

		const EntityDescription & currDesc =
			s_entityDescriptionMap_->entityDescription( i );

		// Ignore server only types
		if (!currDesc.isClientType())
		{
			continue;
		}

		(*s_types_)[ currDesc.clientIndex() ] = new EntityType( currDesc );
		numClientTypes++;
	}

	MD5 md5;
	s_entityDescriptionMap_->addToMD5( md5 );
	md5.getDigest( digest );
	s_types_->resize( numClientTypes );

	return true;
}


/**
 *	This static method should be could on shutdown to clean up the entity type
 *	resources.
 */
bool EntityType::fini()
{
	Types::iterator iter = s_types_->begin();

	while (iter != s_types_->end())
	{
		// May be NULL.
		delete *iter;
		++iter;
	}

	delete s_types_;
	s_types_ = NULL;
	delete s_entityDescriptionMap_;
	s_entityDescriptionMap_ = NULL;

	return true;
}


/**
 *	This function returns a brand new instance of a dictionary associated with
 *	this entity type. If a data section is passed in, the values are read from
 *	it, otherwise entries get their default values.
 */
PyObject * EntityType::newDictionary( DataSectionPtr pSection ) const
{
	PyObject * pDict = PyDict_New();

	for (uint i=0; i < description_.propertyCount(); i++)
	{
		DataDescription * pDD = description_.property(i);

		if (!pDD->isClientOnlyData() && !pDD->isClientServerData())
		{
			// if we're not interested in it, don't set it.
			continue;
		}

		DataSectionPtr	pSubSection;

		PyObjectPtr pValue;

		// Can we get it from the DataSection?
		if (pSection &&
			(pSubSection = pSection->openSection( pDD->name() )))
		{
			// createFromSection returns a new reference.
			pValue = pDD->createFromSection( pSubSection );
			Py_XDECREF( pValue.getObject() );
		}
		else	// ok, resort to the default then
		{
			// pInitialValue returns a Smart pointer.
			pValue = pDD->pInitialValue();
		}

		PyDict_SetItemString( pDict,
			const_cast<char*>( pDD->name().c_str() ), pValue.getObject() );
	}

	return pDict;
}


/**
 *	This function returns a brand new instance of a dictionary associated with
 *	this entity type. It streams the properties from the input stream.
 *	This is only used for creating the player entity.
 */
PyObject * EntityType::newDictionary( BinaryIStream & stream,
	int dataDomains ) const
{
	PyObject * pDict = PyDict_New();

	description_.readStream( stream, dataDomains, pDict );

	return pDict;
}


/**
 *	This function returns a brand new instance of an entity associated with
 *	this entity type. It streams the properties from the input stream.
 */
Entity * EntityType::newEntity( ObjectID id, const Vector3 & pos,
	float yaw, float pitch, float roll,
	BinaryIStream & data, bool isBasePlayer )
{
	return new Entity( id, *this, pos, yaw, pitch, roll, data, isBasePlayer );
}

// entity_type.cpp
