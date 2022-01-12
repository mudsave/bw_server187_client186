/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// entity_type.ipp

#ifdef CODE_INLINE
#define INLINE	inline
#else
#define INLINE
#endif

// -----------------------------------------------------------------------------
// Section: EntityType
// -----------------------------------------------------------------------------

/**
 *	This method returns the volatile info associated with this entity type. The
 *	volatile info indicates what data associated with entities of this type
 *	changes regularly and so should be sent regularly (such as position and
 *	direction).
 */
INLINE
const VolatileInfo & EntityType::volatileInfo() const
{
	return entityDescription_.volatileInfo();
}


/**
 *	This method returns an object that describes this entity type. The
 *	description includes things such as the properties and method supported by
 *	entities of this type.
 */
INLINE
const EntityDescription & EntityType::description() const
{
	return entityDescription_;
}


#if 0
/**
 *	This method returns the Python class associated with real entities of this
 *	type.
 */
INLINE
const PyObject * EntityType::pyClass() const
{
	return pyClass_;
}


/**
 *	This method returns the Python class that is associated with ghost entities
 *	of this type. This class is generated from the real Python class. This class
 *	changes functionality to forward calls to the real entity, where necessary.
 */
INLINE
const PyObject * EntityType::pyGhostClass() const
{
	return pyGhostClass_;
}


/*
 *
 */
INLINE
PyObject * EntityType::pyClass()
{
	return pyClass_;
}


/*
 *
 */
INLINE
PyObject * EntityType::pyGhostClass()
{
	return pyGhostClass_;
}
#endif


/**
 *	This method returns the name associated with the entity type.
 */
INLINE
const char * EntityType::name() const
{
	return entityDescription_.name().c_str();
}


/**
 *	This method returns the ID of the client entity type associated with this
 *	server entity type. Often, this is the same as this object's ID but may not
 *	be so. For example, an entity of type NPC on the server may be of type
 *	Avatar on the client.
 */
INLINE
EntityTypeID EntityType::clientTypeID() const
{
	return entityDescription_.clientIndex();
}

// entity_type.ipp
