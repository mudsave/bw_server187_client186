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
#include "editor_chunk_entity.hpp"
#include "editor_chunk_substance.ipp"
#include "editor_chunk_station.hpp"
#include "editor_chunk_link.hpp"

#include "big_bang.hpp"
#include "appmgr/options.hpp"
#include "item_editor.hpp"
#include "item_properties.hpp"
#include "entity_link_proxy.hpp"
#include "romp/super_model.hpp"
#include "romp/geometrics.hpp"
#include "chunk/chunk_model.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunks/editor_chunk.hpp"

#include "entitydef/data_description.hpp"
#include "entitydef/entity_description.hpp"
#include "entitydef/data_types.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/xml_section.hpp"
#include "resmgr/auto_config.hpp"

#include "gizmo/link_property.hpp"

#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT2( "Editor", 0 )

std::vector<EditorChunkEntity*> EditorChunkEntity::s_dirtyModelEntities_;
SimpleMutex EditorChunkEntity::s_dirtyModelMutex_;


static AutoConfigString s_notFoundModel( "system/notFoundModel" );	

static std::string s_defaultModel = "resources/models/entity.model";


// Link to the UalEntityProvider, so entities get listed in the Asset Locator
// this token is defined in ual_entity_provider.cpp
extern int UalEntityProv_token;
static int total = UalEntityProv_token;


// -----------------------------------------------------------------------------
// Section: EditorEntityType
// -----------------------------------------------------------------------------
EditorEntityType * EditorEntityType::s_instance_ = NULL;

void EditorEntityType::startup()
{
	MF_ASSERT(!s_instance_);
	s_instance_ = new EditorEntityType();
}

void EditorEntityType::shutdown()
{
	MF_ASSERT(s_instance_);
	delete s_instance_;
	s_instance_ = NULL;
}

static void initEditorPropertyTypes();

EditorEntityType::EditorEntityType()
{
	initEditorPropertyTypes();
	// load normal entities
	eMap_.parse( BWResource::openSection( "entities/entities.xml" ) );

	// load markers with properties that are pretending to be entities
	DataSectionPtr markers = BWResource::openSection( "entities/marker_categories.xml" );

	if (markers)
	{
		for (DataSectionIterator mit = markers->begin(); mit != markers->end(); mit++)
		{
			DataSectionPtr marker = *mit;
			// test to see whether an entity
			DataSectionPtr mlevel = marker->openSection( "level" );
			if (mlevel) continue;

			DataSectionPtr mprops = marker->openSection( "Properties" );
			if (!mprops) continue;

			std::string category = marker->sectionName();

			EntityDescription * pED = new EntityDescription();
			if (!pED->parse( category, marker ))
			{
				ERROR_MSG( "Could not parse marker %s as entity\n", category.c_str() );
				continue;
			}
			for (uint i = 0; i < pED->propertyCount(); i++)
				pED->property( i )->editable( true );
			mMap_[ category ] = pED;
		}
	}

	// load the editor entity scripts
	INFO_MSG( "EditorEntityType constructor - Importing editor entity scripts\n" );
	DataSectionPtr edScripts = BWResource::openSection( "entities/editor" );

	if( edScripts )
	{
		for (DataSectionIterator it = edScripts->begin();
			it != edScripts->end();
			it++)
		{
			std::string nameStr = (*it)->sectionName();
			if ( nameStr.substr( nameStr.length() - 3 ) != ".py" )
				continue;

			// class name and module name are the same
			std::string name = nameStr.substr(0, nameStr.length() - 3);
			PyObject * pModule = PyImport_ImportModule( const_cast<char *>( name.c_str() ) );
			if (PyErr_Occurred())
			{
				ERROR_MSG( "EditorEntityType - fail to import editor script %s\n", name.c_str() );
				PyErr_Print();
				continue;
			}

			MF_ASSERT(pModule);

			PyObject * pyClass = PyObject_CallMethod( pModule, const_cast<char *>( name.c_str() ), "" );
			Py_XDECREF( pModule );

			if (PyErr_Occurred())
			{
				ERROR_MSG( "EditorEntityType - fail to open editor script %s\n", name.c_str() );
				PyErr_Print();
				continue;
			}

			MF_ASSERT(pyClass);

			pyClasses_[ name ] = pyClass;
		}
	}
}

const EntityDescription * EditorEntityType::get( const std::string & name )
{
	// try it as an entity
	EntityTypeID index = 0;
	if (eMap_.nameToIndex( name, index )) return &eMap_.entityDescription( index );

	// try it as a marker
	if (mMap_[ name ] != NULL) return mMap_[ name ];

	return NULL;
}

PyObject * EditorEntityType::getPyClass( const std::string & name )
{
	StringHashMap< PyObject * >::iterator mit = pyClasses_.find( name );
	if (mit != pyClasses_.end())
	{
		return mit->second;
	}

	return NULL;
}

EditorEntityType& EditorEntityType::instance()
{
	MF_ASSERT(s_instance_);
	return *s_instance_;
}



/*
// required by entity description, for a method we never call.
#include "network/mercury.hpp"
namespace Mercury
{
	int Bundle::size() const { return 0; }
};
*/




// -----------------------------------------------------------------------------
// Section: EntityPropertyManagerOperation
// -----------------------------------------------------------------------------

class EntityPropertyManagerOperation : public UndoRedo::Operation
{
public:
	EntityPropertyManagerOperation( EditorChunkEntityPtr entity, const std::string & propName ) :
		UndoRedo::Operation( int(typeid(EntityPropertyManagerOperation).name()) ),
		entity_( entity ),
		propName_( propName )
	{
		DataSectionPtr section = entity_->pOwnSect();
		DataSectionPtr propertiesSection = section->openSection( "properties", true );
		DataSectionPtr propSection = propertiesSection->openSection( propName_ );
		if (!propSection)
		{
			oldData_ = NULL;
		}
		else
		{
			oldData_ = new XMLSection( "temp" );
			oldData_->copy( propSection );
		}
		addChunk( entity->chunk() );
	}

private:

	virtual void undo()
	{
		// first add the current state of this block to the undo/redo list
		UndoRedo::instance().add( new EntityPropertyManagerOperation(
			entity_, propName_) );

		// then return to the old data
		DataSectionPtr section = entity_->pOwnSect();
		DataSectionPtr propertiesSection = section->openSection( "properties", true );
		DataSectionPtr propSection = propertiesSection->openSection( propName_ );
		MF_ASSERT( propSection );
		if (oldData_)
		{
			propSection->delChildren();
			propSection->copy( oldData_ );
		}
		else
		{
			propertiesSection->delChild( propSection );
		}

		// mark as dirty (for saving)
		BigBang::instance().changedChunk( entity_->chunk() );

		// reload the section
		entity_->resetSelUpdate();
	}

	virtual bool iseq( const UndoRedo::Operation & oth ) const
	{
		return (oldData_ ==
			static_cast<const EntityPropertyManagerOperation&>( oth ).oldData_) &&
			(entity_ == 
			static_cast<const EntityPropertyManagerOperation&>( oth ).entity_);
	}

	EditorChunkEntityPtr entity_;
	std::string propName_;
	DataSectionPtr oldData_;
};




// -----------------------------------------------------------------------------
// Section: EntityPropertyManager
// -----------------------------------------------------------------------------


// each entity property that needs to be managed (added to or removed) gets one of these
class EntityPropertyManager : public PropertyManager
{
public:
	EntityPropertyManager( EditorChunkEntityPtr entity, 
			const std::string & propName, const std::string & defaultItemName)
		: entity_( entity )
		, propName_( propName )
		, defaultItemName_( defaultItemName )
		, listIndex_( -1 )
	{
	}

	EntityPropertyManager( EditorChunkEntityPtr entity, 
			const std::string & propName, int listIndex )
		: entity_( entity )
		, propName_( propName )
		, defaultItemName_( "" )
		, listIndex_( listIndex )
	{
	}

	virtual bool canAddItem()
	{
		return !defaultItemName_.empty();
	}

	virtual void addItem()
	{
		if (defaultItemName_.empty())
			return;

		UndoRedo::instance().add( new EntityPropertyManagerOperation( entity_, propName_ ) );
		UndoRedo::instance().barrier( "adding property to " + entity_->edDescription(), false );

		DataSectionPtr section = entity_->pOwnSect();
		DataSectionPtr propertiesSection = section->openSection( "properties", true );
		DataSectionPtr propSection = propertiesSection->openSection( propName_, true );
		DataSectionPtr actionSection = propSection->newSection( "item" );
		actionSection->setString( defaultItemName_ );

		// mark as dirty (for saving)
		BigBang::instance().changedChunk( entity_->chunk() );

		// reload the section
		entity_->resetSelUpdate();
	}

	virtual bool canRemoveItem()
	{
		return listIndex_ != -1;
	}

	virtual void removeItem()
	{
		if (listIndex_ == -1)
			return;

		UndoRedo::instance().add( new EntityPropertyManagerOperation( entity_, propName_ ) );
		UndoRedo::instance().barrier( "removing property from " + entity_->edDescription(), false );

		DataSectionPtr section = entity_->pOwnSect();
		DataSectionPtr propertiesSection = section->openSection( "properties", true );
		DataSectionPtr propSection = propertiesSection->openSection( propName_ );
		MF_ASSERT( propSection );
		DataSectionPtr childToDelete = propSection->openChild( listIndex_ );
		MF_ASSERT( childToDelete );
		propSection->delChild( childToDelete );

		// mark as dirty (for saving)
		BigBang::instance().changedChunk( entity_->chunk() );

		// reload the section
		entity_->resetSelUpdate();
	}

private:
	EditorChunkEntityPtr entity_;
	std::string propName_;
	int listIndex_;
	std::string defaultItemName_;
};




// -----------------------------------------------------------------------------
// Section: Property Proxies
// -----------------------------------------------------------------------------


/**
 *	A helper class to access the entity specific properties
 */
class EntityStringProxy : public UndoableDataProxy<StringProxy>
{
public:
	EntityStringProxy( EditorChunkEntityPtr pItem, int index, bool isProp = false ) :
		pItem_( pItem ),
		index_( index ),
		isProp_( isProp )
	{
	}

	virtual std::string EDCALL get() const
	{
		return pItem_->propGetString( index_ );
	}

	virtual void EDCALL setTransient( std::string v )
	{
		// we do absolutely nothing here
	}

	virtual bool EDCALL setPermanent( std::string v )
	{
		// set it
		bool ok = pItem_->propSetString( index_, v );
		if (!ok) return false;

		// flag the chunk as having changed
		BigBang::instance().changedChunk( pItem_->chunk() );

		// update its data section
		pItem_->edSave( pItem_->pOwnSect() );

		return true;
	}

	virtual std::string EDCALL opName()
	{
		return "Set " + pItem_->edDescription() + " " +
			pItem_->propName( index_ );
	}

private:
	EditorChunkEntityPtr	pItem_;
	int						index_;
	bool					isProp_;
};


/**
 *	A helper class to access the entity specific properties
 */
class EntityPythonProxy : public UndoableDataProxy<PythonProxy>
{
public:
	EntityPythonProxy( EditorChunkEntityPtr pItem, int index, bool isProp = false ) :
		pItem_( pItem ),
		index_( index )
	{
	}

	virtual PyObjectPtr EDCALL get() const
	{
		return pItem_->propGetPy( index_ );
	}

	virtual void EDCALL setTransient( PyObjectPtr v )
	{
		// we do absolutely nothing here
	}

	virtual bool EDCALL setPermanent( PyObjectPtr v )
	{
		if( pItem_->propSetPy( index_, v.getObject() ) )
		{
			BigBang::instance().changedChunk( pItem_->chunk() );

			// update its data section
			pItem_->edSave( pItem_->pOwnSect() );
		}
		return true;
	}

	virtual std::string EDCALL opName()
	{
		return "Set " + pItem_->edDescription() + " " +
			pItem_->propName( index_ );
	}

private:
	EditorChunkEntityPtr	pItem_;
	int						index_;
};


/**
 *	A helper class to access the entity specific properties
 */
class EntityFloatProxy : public UndoableDataProxy<FloatProxy>
{
public:
	EntityFloatProxy( EditorChunkEntityPtr pItem, int index, bool isProp = false ) :
		pItem_( pItem ),
		index_( index ),
		useTransient_( false ),
		isProp_( isProp )
	{
	}

	virtual float EDCALL get() const
	{
		if (useTransient_)
			return transientValue_;
		else
			return pItem_->propGetFloat( index_ );
	}

	virtual void EDCALL setTransient( float f )
	{
		transientValue_ = f;
		useTransient_ = true;
	}

	virtual bool EDCALL setPermanent( float f )
	{
		useTransient_ = false;

		// set it
		bool ok = pItem_->propSetFloat( index_, f );
		if (!ok) return false;

		// flag the chunk as having changed
		BigBang::instance().changedChunk( pItem_->chunk() );

		// update its data section
		pItem_->edSave( pItem_->pOwnSect() );

		return true;
	}

	virtual std::string EDCALL opName()
	{
		return "Set " + pItem_->edDescription() + " " +
			pItem_->propName( index_ );
	}

private:
	EditorChunkEntityPtr	pItem_;
	int						index_;
	float					transientValue_;
	bool					useTransient_;
	bool					isProp_;
};


/**
 *	A helper class to access the entity specific properties
 */
class EntityIntProxy : public UndoableDataProxy<IntProxy>
{
public:
	enum IntType
	{
		SINT8 = 0,
		UINT8,
		OTHER
	};
	EntityIntProxy( EditorChunkEntityPtr pItem, int index, IntType intType = OTHER, bool isProp = false ) :
		pItem_( pItem ),
		index_( index ),
		useTransient_( false ),
		isProp_( isProp ),
		intType_( intType )
	{
	}

	virtual bool getRange( int& min, int& max ) const
	{
		switch( intType_ )
		{
		case SINT8:
			min = -128;
			max = 127;
			return true;
		case UINT8:
			min = 0;
			max = 255;
			return true;
		default:
			return false;
		}
		return false;
	}

	virtual uint32 EDCALL get() const
	{
		if (useTransient_)
			return transientValue_;
		else
			return pItem_->propGetInt( index_ );
	}

	virtual void EDCALL setTransient( uint32 i )
	{
		transientValue_ = i;
		useTransient_ = true;
	}

	virtual bool EDCALL setPermanent( uint32 i )
	{
		useTransient_ = false;

		// set it
		bool ok = pItem_->propSetInt( index_, i );
		if (!ok) return false;

		// flag the chunk as having changed
		BigBang::instance().changedChunk( pItem_->chunk() );

		// update its data section
		pItem_->edSave( pItem_->pOwnSect() );

		return true;
	}

	virtual std::string EDCALL opName()
	{
		return "Set " + pItem_->edDescription() + " " +
			pItem_->propName( index_ );
	}

private:
	IntType					intType_;
	int						min_;
	int						max_;
	bool					ranged_;
	EditorChunkEntityPtr	pItem_;
	int						index_;
	uint32					transientValue_;
	bool					useTransient_;
	bool					isProp_;
};

/**
 *	A helper class to access the entity specific properties
 */
class EntityStringEnumProxy : public UndoableDataProxy<IntProxy>
{
public:
	EntityStringEnumProxy( EditorChunkEntityPtr pItem, int index, std::map<std::string,int> enumMap, bool isProp = false ) :
		pItem_( pItem ),
		index_( index ),
		useTransient_( false ),
		isProp_( isProp ),
		enumMapString_( enumMap )
	{
		for( std::map<std::string,int>::iterator iter = enumMapString_.begin(); iter != enumMapString_.end(); ++iter )
			enumMapInt_[ iter->second ] = iter->first;
	}

	virtual uint32 EDCALL get() const
	{
		if (useTransient_)
			return transientValue_;
		else
			return enumMapString_.find( pItem_->propGetString( index_ ) )->second;
	}

	virtual void EDCALL setTransient( uint32 i )
	{
		transientValue_ = i;
		useTransient_ = true;
	}

	virtual bool EDCALL setPermanent( uint32 i )
	{
		useTransient_ = false;

		// set it
		bool ok = pItem_->propSetString( index_, enumMapInt_[ i ] );
		if (!ok) return false;

		// flag the chunk as having changed
		BigBang::instance().changedChunk( pItem_->chunk() );

		// update its data section
		pItem_->edSave( pItem_->pOwnSect() );

		return true;
	}

	virtual std::string EDCALL opName()
	{
		return "Set " + pItem_->edDescription() + " " +
			pItem_->propName( index_ );
	}

private:
	EditorChunkEntityPtr	pItem_;
	int						index_;
	uint32					transientValue_;
	bool					useTransient_;
	bool					isProp_;
	std::map<std::string,int> enumMapString_;
	std::map<int,std::string> enumMapInt_;
};

class EntityFloatEnumProxy : public UndoableDataProxy<IntProxy>
{
public:
	EntityFloatEnumProxy( EditorChunkEntityPtr pItem, int index, std::map<float,int> enumMap, bool isProp = false ) :
		pItem_( pItem ),
		index_( index ),
		useTransient_( false ),
		isProp_( isProp ),
		enumMapString_( enumMap )
	{
		for( std::map<float,int>::iterator iter = enumMapString_.begin(); iter != enumMapString_.end(); ++iter )
			enumMapInt_[ iter->second ] = iter->first;
	}

	virtual uint32 EDCALL get() const
	{
		if (useTransient_)
			return transientValue_;
		else
			return enumMapString_.find( pItem_->propGetFloat( index_ ) )->second;
	}

	virtual void EDCALL setTransient( uint32 i )
	{
		transientValue_ = i;
		useTransient_ = true;
	}

	virtual bool EDCALL setPermanent( uint32 i )
	{
		useTransient_ = false;

		// set it
		bool ok = pItem_->propSetFloat( index_, enumMapInt_[ i ] );
		if (!ok) return false;

		// flag the chunk as having changed
		BigBang::instance().changedChunk( pItem_->chunk() );

		// update its data section
		pItem_->edSave( pItem_->pOwnSect() );

		return true;
	}

	virtual std::string EDCALL opName()
	{
		return "Set " + pItem_->edDescription() + " " +
			pItem_->propName( index_ );
	}

private:
	EditorChunkEntityPtr	pItem_;
	int						index_;
	uint32					transientValue_;
	bool					useTransient_;
	bool					isProp_;
	std::map<float,int> enumMapString_;
	std::map<int,float> enumMapInt_;
};


/**
 *	A helper class to access the items in a list of user type items
 */
class EntityUserTypeListProxy : public UndoableDataProxy<StringProxy>
{
public:
	EntityUserTypeListProxy( EditorChunkEntityPtr pItem, PyObject * obj, 
			const std::string & argName, bool isProp = false ) :
		pItem_( pItem ),
		action_( obj ),
		argName_( argName ),
		isProp_( isProp )
	{
	}

	virtual std::string EDCALL get() const
	{
		PyObject * val = PyObject_GetAttrString( action_, const_cast<char *>(argName_.c_str()) );
		if (val)
		{
			std::string result( PyString_AsString( val ) );
			Py_DECREF( val );
			return result;
		}
		return "";
	}

	virtual void EDCALL setTransient( std::string v )
	{
		// do absolutely nothing here
	}

	virtual bool EDCALL setPermanent( std::string v )
	{
		PyObject * newVal = PyString_FromString( v.c_str() );
		if (newVal == NULL)
		{
			MF_ASSERT(0);
			return false;
		}

		PyObject_DelAttrString( action_, const_cast<char *>(argName_.c_str()) );
		PyObject_SetAttrString( action_, const_cast<char *>(argName_.c_str()), newVal );
		Py_DECREF( newVal );
		if (PyErr_Occurred())
		{
			PyErr_Print();
			MF_ASSERT(0);
			return false;
		}

		// flag the chunk as having changed
		BigBang::instance().changedChunk( pItem_->chunk() );

		// update its data section
		pItem_->edSave( pItem_->pOwnSect() );

		return true;
	}

	virtual std::string EDCALL opName()
	{
		return "Set " + pItem_->edDescription() + " " +
			argName_.c_str();
	}

private:
	EditorChunkEntityPtr	pItem_;
	bool					isProp_;
	int						listIndex_;
	std::string				argName_;
	PyObject *				action_;
};



/**
 *	A helper class to access a user type properties
 */
class EntityUserTypeProxy : public UndoableDataProxy<StringProxy>
{
public:
	EntityUserTypeProxy( EditorChunkEntityPtr pItem, PyObject * obj, std::string propName, int listIndex ) :
		pItem_( pItem ),
		action_( obj ),
		propName_( propName ),
		listIndex_( listIndex )
	{
	}

	virtual std::string EDCALL get() const
	{
		PyInstanceObject * pInst = (PyInstanceObject *)(action_);
		PyClassObject * pClass = (PyClassObject *)(pInst->in_class);
		std::string userClassName = PyString_AsString( pClass->cl_name );
		return userClassName;
	}

	virtual void EDCALL setTransient( std::string v )
	{
		// do absolutely nothing here
	}

	virtual bool EDCALL setPermanent( std::string v )
	{
		// ok, what we do is play with the data section - remove the current prop section
		// and add a new one for the selection change
		// (since we play with the data section directly, don't need to call edSave)
		DataSectionPtr propSect = pItem_->pOwnSect()->openSection( "properties" );
		MF_ASSERT( propSect );
		DataSectionPtr listSect = propSect->openSection( propName_ );
		MF_ASSERT( listSect );

		// rename
		DataSectionPtr sect = listSect->openChild( listIndex_ );
		MF_ASSERT( sect );
		sect->delChildren();
		sect->setString( v );

		// reselect the edEntity_ to update the changes to the property pane
		pItem_->resetSelUpdate();

		// flag the chunk as having changed
		BigBang::instance().changedChunk( pItem_->chunk() );

		return true;
	}

	virtual std::string EDCALL opName()
	{
		return "Set " + pItem_->edDescription() + " " +
			propName_.c_str();
	}

private:
	EditorChunkEntityPtr	pItem_;
	PyObject *				action_;
	int						listIndex_;
	std::string				propName_;
};




// -----------------------------------------------------------------------------
// Section: SetupEditorPropertyTypes
// -----------------------------------------------------------------------------

class SetupEditorPropertyTypes
{
public:
	SetupEditorPropertyTypes() :
		patrolPathDataType( "PATROL_PATH" )
	{
	}

	// We extend string rather that PatrolPathDataType, as we only want to
	// manipulate the resource it points to, not the data
	class EditorPatrolPathDataType : public StringDataType
	{
	public:
		EditorPatrolPathDataType( MetaDataType * pMetaType ) : StringDataType( pMetaType ) { }

		GeneralProperty * createEditorProperty( const std::string& name,
			EditorChunkEntity* editorChunkEntity, int editorEntityPropertyId )
		{
            return
                new LinkProperty
                (
                    name,
                    new EntityLinkProxy(editorChunkEntity),
                    NULL // use the selection's matrix
                );
		}
	};

private:
	SimpleMetaDataType< EditorPatrolPathDataType > patrolPathDataType;

};

static void initEditorPropertyTypes()
{
	// Used to ensure we call the once, after the normal static vars have been
	// inited
	static SetupEditorPropertyTypes setup;
}




// -----------------------------------------------------------------------------
// Section: EditorChunkEntity
// -----------------------------------------------------------------------------

static void initEditorPropertyTypes();

/**
 *	Constructor.
 */
EditorChunkEntity::EditorChunkEntity() :
	pType_( NULL ),
	transform_( Matrix::identity ),
	pDict_( NULL ),
	pyClass_( NULL ),
	surrogateMarker_( false ),
	model_( NULL )
{
	initEditorPropertyTypes();
}

/**
 *	Destructor.
 */
EditorChunkEntity::~EditorChunkEntity()
{
	removeFromDirtyList();
	Py_XDECREF( pDict_ );
}


bool EditorChunkEntity::initType( const std::string type, std::string* errorString )
{
	if ( pType_ != NULL )
		return true;

	pType_ = EditorEntityType::instance().get( type );
	
	if (!pType_)
	{
		if (surrogateMarker_)
			return true;

		std::string err = "No definition for entity type '" + type + "'";
		if ( errorString )
		{
			*errorString = err;
		}
		else
		{
			ERROR_MSG( "EditorChunkEntity::load - %s\n", err.c_str() );
		}
		return false;
	}

	return true;
}


/**
 *	Our load method. We can't call (or reference) the base class's method
 *	because it would not compile (chunk item has no load method)
 */
bool EditorChunkEntity::load( DataSectionPtr pSection, Chunk * pChunk, std::string* errorString )
{
	edCommonLoad( pSection );

	// read known properties
	model_ = Model::get( s_defaultModel );

	pOwnSect_ = pSection;
	
	std::string typeName = pOwnSect_->readString( "type" );

	if ( !initType( typeName, errorString ) )
	{
		ModelPtr badModel = Model::get( s_notFoundModel.value() );
		MF_ASSERT( badModel != NULL );
		if ( badModel != NULL )
			model_ = badModel;
		BigBang::instance().addError(
			pChunk, this, "Couldn't load entity: %s", typeName.c_str() );
		pOriginalSect_ = pSection;
	}

	if ( Moo::g_renderThread )
	{
		// If loading from the main thread, load straight away
		edLoad( pOwnSect_ );
	}
	return true;
}

void EditorChunkEntity::edMainThreadLoad()
{
	// have to load this in the main thread to avoid multi-thread issues with
	// some python calls/objects in edLoad
	edLoad( pOwnSect_ );
}

bool EditorChunkEntity::edLoad( const std::string type, DataSectionPtr pSection, std::string* errorString )
{
	if ( pDict_ != NULL )
		return true; // already initialised

	if ( !initType( type, errorString ) )
		return false;

	return edLoad( pSection );
}

bool EditorChunkEntity::edLoad( DataSectionPtr pSection )
{
	// get rid of any current state
	Py_XDECREF( pDict_ );
	pDict_ = NULL;
	usingDefault_.clear();

	if (!surrogateMarker_)
	{
		transform_ = pSection->readMatrix34( "transform", Matrix::identity );

		clientOnly_ = pSection->readBool( "clientOnly", false );
	}
	if ( pSection->findChild( "patrolPathNode" ) != NULL )
		patrolListNode_ = pSection->readString("patrolPathNode");
	else
		patrolListNode_ = pSection->readString("properties/patrolPathNode");

	if (!pType_ )
	{
		if (surrogateMarker_)	
			return true; // early return if it's a marker

		markModelDirty();
		return false;
	}

	// read item properties (also from parents)
	pDict_ = PyDict_New();
	DataSectionPtr propertiesSection = pSection->openSection( "properties" );
	usingDefault_.resize( pType_->propertyCount(), true );
	for (uint i=0; i < pType_->propertyCount(); i++)
	{
		DataDescription * pDD = pType_->property(i);

		if (!pDD->editable())
			continue;

		DataSectionPtr	pSubSection;

		PyObjectPtr pValue = NULL;

		// Can we get it from the DataSection?
		if (propertiesSection && (pSubSection = propertiesSection->openSection( pDD->name() )))
		{
			// TODO: support for UserDataType
			pValue = pDD->createFromSection( pSubSection );
			PyErr_Clear();
		}

		// ok, resort to the default then
		usingDefault_[i] = ( !pValue );
		if (!pValue)
		{
			pValue = pDD->pInitialValue();
		}

		PyDict_SetItemString( pDict_,
			const_cast<char*>( pDD->name().c_str() ), &*pValue );
		Py_INCREF( &*pValue );
	}

	// record the links to other models
	recordBindingProps();

	// find the correct model
	markModelDirty();

	// find the reference to the editor python class
	std::string className = pType_->name();
	pyClass_ = EditorEntityType::instance().getPyClass( className );

	return true;
}

void EditorChunkEntity::clearProperties()
{
	pType_ = NULL;
}

void EditorChunkEntity::clearEditProps()
{
	allowEdit_.clear();
}

void EditorChunkEntity::setEditProps( const std::list< std::string > & names )
{
	clearEditProps();

	if (pType_ == NULL)
		return;

	allowEdit_.resize( pType_->propertyCount() );

	for (uint i=0; i < pType_->propertyCount(); i++)
	{
		DataDescription * pDD = pType_->property(i);

		allowEdit_[ i ] = std::find( names.begin(), names.end(), pDD->name() ) 
								!= names.end();
	}
}


/**
 *	Save any property changes to this data section
 */
void EditorChunkEntity::clearPropertySection()
{
	pType_ = NULL;

	if (!pOwnSect())
		return;

	DataSectionPtr propertiesSection = pOwnSect()->openSection( "properties" );
	if (propertiesSection)
		propertiesSection->delChildren();
}


class UserDataType;

bool EditorChunkEntity::edSave( DataSectionPtr pSection )
{
	if (!edCommonSave( pSection ))
		return false;

	if (pType_ == NULL)
	{
		if (surrogateMarker_)
		{
			return true;
		}
		else if ( pOriginalSect_ )
		{
			// there was an error loading the entity type, so save the original
			// datasection but modify the transform.
			pSection->copy( pOriginalSect_ );
			pSection->writeMatrix34( "transform", transform_ );
			return true;
		}
		else
		{
			return false;
		}
	}

	// write the easy ones
	if (!surrogateMarker_)
	{
		// write the easy ones
		pSection->delChild( "id" );

		pSection->writeString( "type", pType_->name() );

		pSection->writeMatrix34( "transform", transform_ );

		pSection->writeBool( "clientOnly", clientOnly_ );

		pSection->delChild( "patrolPathNode" );
	}

	DataSectionPtr propertiesSection = pSection->openSection( "properties", true );
	propertiesSection->delChildren();

	// now write the properties from the dictionary
	for (uint i=0; i < pType_->propertyCount(); i++)
	{
		DataDescription * pDD = pType_->property(i);

		if (!pDD->editable())
			continue;

		PyObject * pValue = PyDict_GetItemString( pDict_,
			const_cast<char*>( pDD->name().c_str() ) );
		if (!pValue)
		{
			PyErr_Print();
			ERROR_MSG( "EditorChunkEntity::edSave: Failed to get prop %s\n",
				pDD->name().c_str() );
			continue;
		}

		DataSectionPtr pDS = propertiesSection->openSection( pDD->name(), true );
		pDD->dataType()->addToSection( pValue, pDS );
	}

	propertiesSection->writeString("patrolPathNode", patrolListNode_);
	
	return true;
}


/**
 *  This is called when the entity is removed from, or added to a Chunk.  Here
 *  we delete the link, it gets recreated if necessary later on, and call the
 *  base class.
 */
/*virtual*/ void EditorChunkEntity::toss( Chunk * pChunk )
{
    if (link_ != NULL)
    {
        link_->startItem(NULL);
        link_->endItem(NULL);
        link_->toss(NULL);
        link_ = NULL;
    }

    EditorChunkSubstance<ChunkItem>::toss( pChunk );
}


/**
 *	Change our transform, temporarily or permanently
 */
bool EditorChunkEntity::edTransform( const Matrix & m, bool transient )
{
	// it's permanent, so find out where we belong now
	Chunk * pOldChunk = pChunk_;
	Chunk * pNewChunk = this->edDropChunk( m.applyToOrigin() );
	if (pNewChunk == NULL) return false;

	// if this is only a temporary change, keep it in the same chunk
	if (transient)
	{
		transform_ = m;
		return true;
	}

	// make sure the chunks aren't readonly
	if (!EditorChunkCache::instance( *pOldChunk ).edIsWriteable() 
		|| !EditorChunkCache::instance( *pNewChunk ).edIsWriteable())
		return false;

	// ok, accept the transform change then
	transform_.multiply( m, pOldChunk->transform() );
	transform_.postMultiply( pNewChunk->transformInverse() );

	// note that both affected chunks have seen changes
	BigBang::instance().changedChunk( pOldChunk );
	BigBang::instance().changedChunk( pNewChunk );

	// and move ourselves into the right chunk. we have to do this
	// even if it's the same chunk so the col scene gets recreated
	pOldChunk->delStaticItem( this );
	pNewChunk->addStaticItem( this );

	return true;
}


/**
 *	Get a description of this item
 */
std::string EditorChunkEntity::edDescription()
{
	std::string name = "<unknown>";
	if ( pType_ )
		name = pType_->name();
	else if ( pOriginalSect_ )
		name = pOriginalSect_->readString( "type" ) + " " + name; 

	return name + " entity item";
}


void EditorChunkEntity::resetSelUpdate()
{
	edLoad( pOwnSect() );
	PyObject* pModule = PyImport_ImportModule( "BigBangDirector" );
	if (pModule != NULL)
	{
		PyObject* pScriptObject = PyObject_GetAttr( pModule, Py_BuildValue( "s", "bd" ) );

		if (pScriptObject != NULL)
		{
			Script::call(
				PyObject_GetAttrString( pScriptObject, "resetSelUpdate" ),
				PyTuple_New(0),
				"Reloading item");
			Py_DECREF( pScriptObject );
		}
		Py_DECREF( pModule );
	}
}


void EditorChunkEntity::patrolListNode(std::string const &id)
{
    patrolListNode_ = id;
}


std::string EditorChunkEntity::patrolListNode() const
{
    return patrolListNode_;
}


std::string EditorChunkEntity::patrolListGraphId() const
{
    int idx = patrolListPropIdx();
    if (idx == -1)
        return std::string();
    else
        return propGetString(idx);
}


int EditorChunkEntity::patrolListPropIdx() const
{
	if (pType_ == NULL)
		return -1;

	for (uint i=0; i < pType_->propertyCount(); i++)
	{
		DataDescription * pDD = pType_->property(i);
		if (pDD->editable() && pDD->dataType()->typeName() == "PATROL_PATH" )
		{
			return i;
		}
	}

	return -1;
}


void EditorChunkEntity::disconnectFromPatrolList()
{
    patrolListNode_.clear();
    int idx = patrolListPropIdx();
    if (idx != -1)
    {
        propSetString(idx, std::string());
    }
    if (link_ != NULL)
    {
        link_->toss(NULL);
        link_->startItem(NULL);
        link_->endItem(NULL);
        link_ = NULL;
    }
}


/**
 *	Relinks an entity to the node 'newNode' in the patrol path 'newGraph'
 *
 *  @param newGraph       New patrol path the entity should be linked to
 *  @param newNode        New patrol path node the entity should be linked to
 *
 *  @return               true if the entity has a patrol list property
 */
bool EditorChunkEntity::patrolListRelink( const std::string& newGraph, const std::string& newNode )
{
    int idx = patrolListPropIdx();
    if (idx != -1)
    {
        propSetString( idx, newGraph );
        patrolListNode( newNode );
        edSave( pOwnSect_ );
        if ( pChunk_ != NULL )
            BigBang::instance().changedChunk( pChunk_ );
		return true;
    }   
	return false;
}



/**
 *	Add the properties of this chunk entity to the given editor
 */
bool EditorChunkEntity::edEdit( class ChunkItemEditor & editor )
{
	editor.addProperty( new StaticTextProperty( "type",
		new AccessorDataProxy< EditorChunkEntity, StringProxy >(
			this, "type", 
			&EditorChunkEntity::typeGet, 
			&EditorChunkEntity::typeSet ) ) );

	MatrixProxy * pMP = new ChunkItemMatrix( this );
	editor.addProperty( new GenPositionProperty( "position", pMP ) );
	editor.addProperty( new GenRotationProperty( "direction", pMP ) );

	editor.addProperty( new GenBoolProperty( "client only",
		new AccessorDataProxy< EditorChunkEntity, BoolProxy >(
			this, "clientOnly", 
			&EditorChunkEntity::clientOnlyGet, 
			&EditorChunkEntity::clientOnlySet ) ) );

	editor.addProperty( new StaticTextProperty( "last script error",
		new AccessorDataProxy<EditorChunkEntity,StringProxy>(
			this, "READONLY", 
			&EditorChunkEntity::lseGet, 
			&EditorChunkEntity::lseSet ) ) );

    editor.addProperty( new StaticTextProperty( "graph",
        new AccessorDataProxy<EditorChunkEntity,StringProxy>(
            this, "READONLY",
            &EditorChunkEntity::graphId,
            &EditorChunkEntity::graphId ) ) );

    editor.addProperty( new StaticTextProperty( "node",
        new AccessorDataProxy<EditorChunkEntity,StringProxy>(
            this, "READONLY",
            &EditorChunkEntity::nodeId,
            &EditorChunkEntity::nodeId ) ) );

	return edEditProperties( editor, pMP );
}

GeneralProperty* EditorChunkEntity::parseWidgetInt(
	int i, PyObject* pyEnum, DataDescription* pDD, MatrixProxy * pMP )
{
	if ( !pyEnum && !pDD->widget() )
		return 0;

	DataSectionPtr choices( new XMLSection( "ENUM choices" ) );

	if ( !!pyEnum )
	{
		bool failed = false;

		if( PySequence_Check( pyEnum ) )
		{
			int nItems = PySequence_Size( pyEnum );
			for (int j = 0; j < nItems; j++)
			{
				PyObject* item = PySequence_GetItem( pyEnum, j );
				if( PyTuple_Check( item ) )
				{
					PyObject* intObj = PyTuple_GetItem( item, 0 );
					PyObject* strObj = PyTuple_GetItem( item, 1 );
					if( intObj && strObj && PyInt_Check( intObj ) && PyString_Check( strObj ) )
					{
						int index = PyInt_AsLong( intObj );
						std::string name = PyString_AsString( strObj );
						choices->newSection( name )->setInt( index );
					}
					else
					{
						failed = true;
						Py_DECREF( item );
						break;
					}
				}
				else
					failed = true;
				Py_DECREF( item );
			}
		}
		else
			failed = true;

		if( failed )
		{
			BigBang::instance().addError( NULL, NULL,
				"Bad enumeration for %s.%s ", pType_->name().c_str(), pDD->name().c_str() );
			PyErr_Print();
			return 0;
		}
		else
		{
			return new ChoiceProperty( pDD->name(),
				new EntityIntProxy( this, i, EntityIntProxy::OTHER, true ),
				choices );
		}
	}
	else
	{
		DataSectionPtr widget = pDD->widget();

		if ( !widget )
			return 0;

		if ( widget->asString() == "ENUM" )
		{
			std::vector<DataSectionPtr> sections;
			widget->openSections( "enum", sections );
			if ( sections.empty() )
				return 0;

			for( std::vector<DataSectionPtr>::iterator it = sections.begin();
				it != sections.end(); ++it)
			{
				choices->newSection( (*it)->readString( "display" ) )->setInt(
					(*it)->readInt( "value" ) );
			}

			return new ChoiceProperty( pDD->name(),
				new EntityIntProxy( this, i, EntityIntProxy::OTHER, true ),
				choices );
		}
	}

	return 0;
}

GeneralProperty* EditorChunkEntity::parseWidgetFloat(
	int i, PyObject* pyEnum, DataDescription* pDD, MatrixProxy * pMP )
{
	if ( !pyEnum && !pDD->widget() )
		return 0;

	DataSectionPtr choices( new XMLSection( "ENUM choices" ) );

	if ( !!pyEnum )
	{
		bool failed = false;

		std::map<float,int> enumMap;
		if( PySequence_Check( pyEnum ) )
		{
			int nItems = PySequence_Size( pyEnum );
			for (int j = 0; j < nItems; j++)
			{
				PyObject* item = PySequence_GetItem( pyEnum, j );
				if( PyTuple_Check( item ) )
				{
					PyObject* floatObj = PyTuple_GetItem( item, 0 );
					PyObject* strObj = PyTuple_GetItem( item, 1 );
					if( floatObj && strObj && PyFloat_Check( floatObj ) && PyString_Check( strObj ) )
					{
						float value = (float)PyFloat_AsDouble( floatObj );
						std::string name = PyString_AsString( strObj );
						enumMap[ value ] = j;
						choices->newSection( name )->setInt( j );
					}
					else
					{
						failed = true;
						Py_DECREF( item );
						break;
					}
				}
				else
					failed = true;
				Py_DECREF( item );
			}
		}
		else
			failed = true;

		if( failed )
		{
			BigBang::instance().addError( NULL, NULL,
				"Bad enumeration for %s.%s ", pType_->name().c_str(), pDD->name().c_str() );
			PyErr_Print();
			return 0;
		}
		else
		{
			return new ChoiceProperty( pDD->name(),
				new EntityFloatEnumProxy( this, i, enumMap, true ), choices );
		}
	}
	else
	{
		DataSectionPtr widget = pDD->widget();

		if ( !widget )
			return 0;

		if( widget->asString() == "RADIUS" )
		{
			// default colour is red with 255 with 192 of alpha
			Vector4 colour = widget->readVector4( "colour", Vector4( 255, 0, 0, 192 ) );
			uint32 radiusColour =
				(((uint8)colour[0]) << 16) |
				(((uint8)colour[1]) << 8) |
				(((uint8)colour[2])) |
				(((uint8)colour[3]) << 24);
			float radius = widget->readFloat( "gizmoRadius", 2.0f );
			return new GenRadiusProperty( pDD->name(),
				new EntityFloatProxy( this, i, true ), pMP,
				radiusColour, radius );
		}
	}

	return 0;
}

bool EditorChunkEntity::edEditProperties( class ChunkItemEditor & editor, MatrixProxy * pMP )
{
//	return true;

	recordBindingProps();

	if (pType_ == NULL)
	{
		if (surrogateMarker_)
		{
			return true;
		}
		else
		{
			ERROR_MSG( "EditorChunkEntity::edEdit - no properties for entity!\n" );
			return false;
		}
	}

	bool hasActions = false;

	// ok, now finally add in all the entity properties!
	for (uint i=0; i < pType_->propertyCount(); i++)
	{
		DataDescription * pDD = pType_->property(i);

		if (!pDD->editable())
			continue;

		if (surrogateMarker_ && (allowEdit_.capacity() != 0) && !allowEdit_[i])
			continue;

		GeneralProperty* prop = pDD->dataType()->createEditorProperty(
			pDD->name(), this, i );

		if (prop)
		{
			editor.addProperty( prop );
			continue;
		}

		// check for enum
		PyObject* enumResult = NULL;
		if( pyClass_ )
		{
			std::string fnName = std::string("getEnums_") + pDD->name();
			PyObject * fnPtr = PyObject_GetAttrString( pyClass_, const_cast<char *>(fnName.c_str()) );
			if( fnPtr )
			{
				enumResult = Script::ask(
						fnPtr,
						PyTuple_New( 0 ),
						"EditorChunkEntity::edEdit: " );
			}
			else
			{
				PyErr_Clear();
			}
		}

/*		if (pDD->dataType()->typeName() == "UserDataType")
		{
			PyObject * action = propGetPy(i);

			if (action != &_Py_NoneStruct )
			{
				PyInstanceObject * pInst = (PyInstanceObject *)(action);
				PyClassObject * pClass = (PyClassObject *)(pInst->in_class);
				std::string userClassName = PyString_AsString( pClass->cl_name );

				PyObject * dict = PyObject_GetAttrString( action, "ARGS" );
				MF_ASSERT( dict );
				PyObject * items = PyDict_Items( dict );
				for (int j = 0; j < PyList_Size( items ); j++)
				{
					PyObject * item = PyList_GetItem( items, j );
					std::string argName = PyString_AsString( PyTuple_GetItem( item, 0 ) );
					std::string argType = PyString_AsString( PyTuple_GetItem( item, 1 ) );
					PyObject * argVal = PyObject_GetAttrString( action, const_cast<char *>(argName.c_str()) );
					INFO_MSG( "target entity id = %s\n", PyString_AsString( argVal ) );

					// TODO: test for type
					TextProperty * tprop = new TextProperty( argName,
						new EntityUserTypeListProxy( this, action, argName, true ) );
					char tempStr[10];
					sprintf( tempStr, "%p", action );
					tprop->setGroup( pDD->name() + "/" + userClassName + " (" + tempStr + ")" );
					editor.addProperty( tprop );
				}
			}
		}
		else */
		if (pDD->dataType()->typeName().find( "ARRAY of" ) != std::string::npos && 0 )
		{
			PyObject * actions = propGetPy(i);

			// load the trigger actions script
			INFO_MSG( "Reading user data type script: ");
			std::string typeName = pDD->dataType()->typeName();
			int index = typeName.find( "USER_TYPE by " );
			MF_ASSERT(index != std::string::npos);
			MF_ASSERT(typeName.length() > (uint)(index + 12));
			int endIndex = typeName.find( '.', index );
			MF_ASSERT(endIndex != std::string::npos);
			std::string typeModuleName = typeName.substr( index + 13, endIndex - (index + 13) );
			MF_ASSERT(typeName.length() > uint(endIndex + 2));
			std::string typeInstanceName = typeName.substr( endIndex + 1 );

			INFO_MSG( "%s\n", typeModuleName.c_str() );
			PyObject * pTypeModule = PyImport_ImportModule( const_cast<char *>( typeModuleName.c_str() ) );
			if (!pTypeModule)
			{
				ERROR_MSG( "EditorChunkEntity::edEditProperties - Cannot load type module: %s\n", typeModuleName.c_str() );
				continue;
			}
			PyObject * instanceObject = PyObject_GetAttrString( pTypeModule, const_cast< char * >( typeInstanceName.c_str() ) );
			Py_DECREF( pTypeModule );

			MF_ASSERT( instanceObject );
			PyObject * pResult = PyObject_CallMethod( instanceObject, "userTypeModuleName", "" );
			Py_XDECREF( instanceObject );

			if (!pResult)
			{
				ERROR_MSG( "EditorChunkEntity::edEditProperties - Unknown user types for module: %s\n", typeModuleName.c_str() );
				PyErr_Print();
				continue;
			}

			std::vector< std::string > userTypeNames;
			std::string userTypeModuleName = PyString_AsString( pResult );
			Py_DECREF( pResult );

			MF_ASSERT( userTypeModuleName.length() > 0 );
			PyObject * pModule = PyImport_ImportModule( const_cast<char *>( userTypeModuleName.c_str() ) );
			if (PyErr_Occurred())
			{
				ERROR_MSG( "EditorChunkEntity::edEditProperties - Cannot load user types: %s\n", userTypeModuleName.c_str() );
				PyErr_Print();
				continue;
			}

			// extract the classes
			PyObject * pyDict = PyModule_GetDict( pModule );
			Py_XDECREF( pModule );

			PyObject * pyItems = PyDict_Items( pyDict );
			int nItems = PyList_Size( pyItems );
			for (int i = 0; i < nItems; i++ )
			{
				PyObject * item = PyList_GetItem( pyItems, i );
				std::string name = PyString_AsString( PyTuple_GetItem( item, 0 ) );
				PyObject * cla = PyTuple_GetItem( item, 1 );
				if (PyClass_Check( cla ) && PyObject_HasAttrString( cla, "ARGS" ))
				{
					userTypeNames.push_back( name );
				}
			}
			Py_XDECREF( pyItems );

			// now add the controls

			// add a special group control for adding of new children
			std::string eventName = pDD->name();
			std::string aTypeName = userTypeNames.front();
			GroupProperty * gprop = new GroupProperty( pDD->name() );
			gprop->setGroup( pDD->name() );
			PropertyManagerPtr manager = new EntityPropertyManager( this, eventName, aTypeName );
			gprop->setPropertyManager( manager );
			editor.addProperty( gprop );

			const int listSize = PyList_Size( actions );
			for (int listIndex = 0; listIndex < listSize; listIndex++)
			{
				PyObject * action = PyList_GetItem( actions, listIndex );

				PyInstanceObject * pInst = (PyInstanceObject *)(action);
				PyClassObject * pClass = (PyClassObject *)(pInst->in_class);
				std::string userClassName = PyString_AsString( pClass->cl_name );

				// add a special group control for removal of children
				char tempStr[10];
				sprintf( tempStr, "%p", action );
				std::string groupName = pDD->name() + "/" + userClassName + " (" + tempStr + ")";
				GroupProperty * gprop = new GroupProperty( userClassName + " (" + tempStr + ")" );
				gprop->setGroup( groupName );
				PropertyManagerPtr manager = new EntityPropertyManager( this, pDD->name(), listIndex );
				gprop->setPropertyManager( manager );
				editor.addProperty( gprop );

				// add a control for the user type
				ListTextProperty * tprop = new ListTextProperty( "type",
					new EntityUserTypeProxy( this, action, pDD->name(), listIndex ), userTypeNames );
				tprop->setGroup( groupName );
				editor.addProperty( tprop );

				// add controls for each argument
				PyObject * dict = PyObject_GetAttrString( action, "ARGS" );
				MF_ASSERT( dict );
				PyObject * items = PyDict_Items( dict );
				Py_XDECREF( dict );

				for (int j = 0; j < PyList_Size( items ); j++)
				{
					PyObject * item = PyList_GetItem( items, j );
					std::string argName = PyString_AsString( PyTuple_GetItem( item, 0 ) );
					std::string argType = PyString_AsString( PyTuple_GetItem( item, 1 ) );

					// TODO: test for type
					if (argType == "ENTITY_ID")
					{
						IDProperty * tprop = new IDProperty( argName,
							new EntityUserTypeListProxy( this, action, argName, true ) );
						tprop->setGroup( groupName );
						editor.addProperty( tprop );

						// remember the prop so can draw lines between the items
						bindingProps_.push_back( BindingProperty(action, argName) );
					}
					else
					{
						TextProperty * tprop = new TextProperty( argName,
							new EntityUserTypeListProxy( this, action, argName, true ) );
						tprop->setGroup( groupName );
						editor.addProperty( tprop );
					}
				}
				Py_XDECREF( items );
			}
		}
		else if (pDD->dataType()->typeName().find( "INT" ) != std::string::npos)
		{
			GeneralProperty* prop = parseWidgetInt( i, enumResult, pDD, pMP );
			if ( prop )
			{
				editor.addProperty( prop );				
			}
			else if (pDD->dataType()->typeName().find( "INT8" ) != std::string::npos)
			{
				if (pDD->dataType()->typeName().find( "UINT8" ) != std::string::npos)
				{
					editor.addProperty( new GenIntProperty( pDD->name(),
						new EntityIntProxy( this, i, EntityIntProxy::UINT8, true ) ) );
				}
				else
				{
					editor.addProperty( new GenIntProperty( pDD->name(),
						new EntityIntProxy( this, i, EntityIntProxy::SINT8, true ) ) );
				}
			}
			else
			{
				editor.addProperty( new GenIntProperty( pDD->name(),
					new EntityIntProxy( this, i, EntityIntProxy::OTHER, true ) ) );
			}
		}
		else if (pDD->dataType()->typeName() == "FLOAT")
		{
			GeneralProperty* prop = parseWidgetFloat( i, enumResult, pDD, pMP );
			if ( prop )
			{
				editor.addProperty( prop );
			}
			else
			{
				editor.addProperty( new GenFloatProperty( pDD->name(),
					new EntityFloatProxy( this, i, true ) ) );
			}
		}
//		else if (pDD->dataType()->typeName() == "VECTOR2")
//		{
//		}
//		else if (pDD->dataType()->typeName() == "VECTOR3")
//		{
//		}
//		else if (pDD->dataType()->typeName() == "VECTOR4")
//		{
//		}
		else if (pDD->dataType()->typeName() == "STRING")
		{
			if(enumResult != NULL)
			{
				DataSectionPtr choices( new XMLSection( "ENUM choices" ) );

				bool failed = false;

				std::map<std::string,int> enumMap;
				if( PySequence_Check( enumResult ) )
				{
					int nItems = PySequence_Size( enumResult );

					for (int j = 0; j < nItems; j++)
					{
						PyObject* item = PySequence_GetItem( enumResult, j );
						if( PyTuple_Check( item ) )
						{
							PyObject* valueObj = PyTuple_GetItem( item, 0 );
							PyObject* nameObj = PyTuple_GetItem( item, 1 );
							if( valueObj && nameObj && PyString_Check( valueObj ) && PyString_Check( nameObj ) )
							{
								std::string value = PyString_AsString( valueObj );
								std::string name = PyString_AsString( nameObj );
								enumMap[ value ] = j;
								choices->newSection( name )->setInt( j );
							}
							else
							{
								failed = true;
								Py_DECREF( item );
								break;
							}
						}
						else
							failed = true;
						Py_DECREF( item );
					}
				}
				else
					failed = true;

				if( failed )
				{
					BigBang::instance().addError( NULL, NULL,
						"Bad enumeration for %s.%s in EditorChunkEntity::edEditProperties",
						pType_->name().c_str(), pDD->name().c_str() );
					PyErr_Print();
				}
				else
					editor.addProperty( new ChoiceProperty( pDD->name(),
						new EntityStringEnumProxy( this, i, enumMap, true ),
						choices ) );
			}
			else
				editor.addProperty( new TextProperty( pDD->name(),
					new EntityStringProxy( this, i, true ) ) );
		}
		else
		{
			// TODO: Should probably make this read-only. It may not work if the
			// Python object does not have a repr that can be eval'ed.
			// treat everything else as a generic Python property
			editor.addProperty( new PythonProperty( pDD->name(),
				new EntityPythonProxy( this, i, true ) ) );

			// None specified, use the default (read only) string property
			//editor.addProperty( new StaticTextProperty( pDD->name(), 
			//	new EntityStringProxy( this, i, true ) ) );
		}
		Py_XDECREF( enumResult );
	}

	return true;
}


void EditorChunkEntity::recordBindingProps()
{
//	return;

	bindingProps_.clear();

	if (pType_ == NULL)
		return;

	for (uint i=0; i < pType_->propertyCount(); i++)
	{
		DataDescription * pDD = pType_->property(i);

		if (!pDD->editable())
			continue;

		if (pDD->dataType()->typeName().find( "ARRAY:" ) != std::string::npos)
		{
			PyObject * actions = propGetPy(i);

			const int listSize = PyList_Size( actions );
			for (int listIndex = 0; listIndex < listSize; listIndex++)
			{
				PyObject * action = PyList_GetItem( actions, listIndex );

				PyInstanceObject * pInst = (PyInstanceObject *)(action);
				PyClassObject * pClass = (PyClassObject *)(pInst->in_class);
				std::string userClassName = PyString_AsString( pClass->cl_name );

				bool argsAdded = false;
				PyObject * dict = PyObject_GetAttrString( action, "ARGS" );
				MF_ASSERT( dict );
				PyObject * items = PyDict_Items( dict );
				Py_XDECREF( dict );

				for (int j = 0; j < PyList_Size( items ); j++)
				{
					PyObject * item = PyList_GetItem( items, j );
					std::string argName = PyString_AsString( PyTuple_GetItem( item, 0 ) );
					std::string argType = PyString_AsString( PyTuple_GetItem( item, 1 ) );

					if (argType == "ENTITY_ID")
						bindingProps_.push_back( BindingProperty(action, argName) );
				}
				Py_XDECREF( items );
			}
		}
	}
}


/**
 *  If we've got a patrolList property, and it's connected to a station graph,
 *  find the connected node.  This is either the explicitly named node, or
 *  the closest node.
 */
EditorChunkStationNode* EditorChunkEntity::connectedNode()
{
    std::string graphName = patrolListGraphId();

    if (graphName.empty())
        return NULL;

	UniqueID graphNameID(graphName);
	StationGraph* graph = StationGraph::getGraph( graphNameID );
    if (graph == NULL)
        return NULL;

    // If there is an explicitly named node, and it's in this graph
    // then return the node:
    if (!patrolListNode_.empty())
    {
        EditorChunkStationNode *node = 
            (EditorChunkStationNode *)graph->getNode(patrolListNode_);
        if (node != NULL && !node->deleted())
            return node;
    }

    // Find the nearest node:
    std::vector<ChunkStationNode*> nodes = graph->getAllNodes();
	if (nodes.empty())
		return NULL;

	EditorChunkStationNode *closestNode = NULL;

	float closestDistSq = FLT_MAX;

	Vector3 from = chunk()->transform().applyPoint( edTransform().applyToOrigin() )
		+ Vector3(0.f, 1.f, 0.f);

	std::vector<ChunkStationNode*>::iterator i = nodes.begin();
	for (; i != nodes.end(); ++i)
	{
		EditorChunkStationNode* dest = static_cast<EditorChunkStationNode*>(*i);

		if (!dest->chunk())
			continue;

		Vector3 to = dest->chunk()->transform().applyPoint( dest->edTransform().applyToOrigin() )
			+ Vector3(0.f, 0.1f, 0.f);

		float len = (to - from).lengthSquared();
		if (len < closestDistSq)
		{
			closestNode = dest;
			closestDistSq = len;
		}
	}

	return closestNode;
}


void EditorChunkEntity::draw()
{
	if (edShouldDraw())
	{
		EditorChunkSubstance<ChunkItem>::draw();

		EditorChunkStationNode* station = connectedNode();
		if (station != NULL && !BigBang::instance().drawSelection() )
		{
            if (link_ == NULL)
            {
                link_ = new EditorChunkLink();
                link_->chunk(chunk());
                link_->startItem(this);
                link_->endItem(station);
            }
			link_->draw();
		}
	}
}


/*virtual*/ void EditorChunkEntity::tick(float dtime)
{
    EditorChunkSubstance<ChunkItem>::tick(dtime);

    EditorChunkStationNode* station = connectedNode();
    if (station != NULL && link_ != NULL)
    {
        link_->startItem(this);
        link_->endItem(station);
        link_->tick(dtime);
    }
}


std::string EditorChunkEntity::typeGet() const
{
	if ( pType_ == NULL )
	{
		if ( pOriginalSect_ )
			return pOriginalSect_->readString( "type" );
		else
			return "";
	}
	return pType_->name();
}

bool EditorChunkEntity::clientOnlySet( const bool & b )
{
	clientOnly_ = b;
	return true;
}

std::string EditorChunkEntity::propGetString( int index ) const
{
    EditorChunkEntity *myself = const_cast<EditorChunkEntity *>(this);
	return myself->propGet(index)->asString();
}

bool EditorChunkEntity::propSetString( int index, const std::string & s )
{
	DataSectionPtr pTemp = new XMLSection( "temp" );
	pTemp->setString( s );
	return propSet( index, pTemp );
}

float EditorChunkEntity::propGetFloat( int index )
{
	return propGet(index)->asFloat();
}

bool EditorChunkEntity::propSetFloat( int index, float f )
{
	DataSectionPtr pTemp = new XMLSection( "temp" );
	pTemp->setFloat( f );
	return propSet( index, pTemp );
}

int EditorChunkEntity::propGetInt( int index )
{
	return propGet(index)->asInt();
}

bool EditorChunkEntity::propSetInt( int index, int f )
{
	if ( pType_ == NULL )
		return false;

	DataDescription * pDD = pType_->property( index );

	DataSectionPtr pTemp = new XMLSection( "temp" );
	pTemp->setInt( f );
	return propSet( index, pTemp );
}

PyObject * EditorChunkEntity::propGetPy( int index )
{
	if ( pType_ == NULL )
		return NULL;

	MF_ASSERT( index < (int) pType_->propertyCount() );
	DataDescription * pDD = pType_->property( index );

	PyObject * pValue = PyDict_GetItemString( pDict_,
		const_cast<char*>( pDD->name().c_str() ) );

	return pValue;
}

bool EditorChunkEntity::propSetPy( int index, PyObject * pObj )
{
	if ( pType_ == NULL )
		return false;

	MF_ASSERT( index < (int) pType_->propertyCount() );
	DataDescription * pDD = pType_->property( index );

	if (PyDict_SetItemString( pDict_,
		const_cast<char*>( pDD->name().c_str() ), pObj ) == -1)
	{
		ERROR_MSG( "EditorChunkEntity::propSetPy: Failed to set value\n" );
		PyErr_Print();
		return false;
	}

	return true;
}

int EditorChunkEntity::propCount() const
{
	if ( pType_ == NULL )
		return 0;

    return (int)pType_->propertyCount();
}

DataSectionPtr EditorChunkEntity::propGet( int index )
{
	if ( pType_ == NULL )
		return NULL;

	MF_ASSERT( index < (int) pType_->propertyCount() );
	DataDescription * pDD = pType_->property( index );

	PyObject * pValue = PyDict_GetItemString( pDict_,
		const_cast<char*>( pDD->name().c_str() ) );

	if (pValue == NULL)
	{
		PyErr_Clear();

		PyObject * pStr = PyObject_Str( pDict_ );
//		HACK_MSG( "pDict_ = %s\n", PyString_AsString( pStr ) );
		Py_XDECREF( pStr );

		return NULL;
	}

	DataSectionPtr pTemp = new XMLSection( "temp" );

	pDD->addToSection( pValue, pTemp );

	return pTemp;
}

bool EditorChunkEntity::propSet( int index, DataSectionPtr pTemp )
{
	if ( pType_ == NULL )
		return false;

	MF_ASSERT( index < (int) pType_->propertyCount() );
	DataDescription * pDD = pType_->property( index );

	PyObjectPtr pNewVal = pDD->createFromSection( pTemp );
	if (!pNewVal)
	{
		PyErr_Clear();
		return false;
	}

	PyDict_SetItemString( pDict_,
		const_cast<char*>( pDD->name().c_str() ), &*pNewVal );
	Py_INCREF( &*pNewVal );


	markModelDirty();

	return true;
}


std::string EditorChunkEntity::propName( int index )
{
	if ( pType_ == NULL )
		return "";

	MF_ASSERT( index < (int) pType_->propertyCount() );
	DataDescription * pDD = pType_->property( index );

	return pDD->name();
}

void EditorChunkEntity::calculateModel()
{
	ModelPtr oldModelHolder;
	if (pyClass_)
	{
		// The tuple will dec this; we don't want it to
		Py_XINCREF( pDict_ );

		PyObject * args = PyTuple_New( 1 );
		PyTuple_SetItem( args, 0, pDict_ );

		PyObject * result = Script::ask(
			PyObject_GetAttrString( pyClass_, "modelName" ),
			args,
			"EditorChunkEntity::calculateModel: ",
			/*okIfFunctionNULL:*/true );

		if (result)
		{
			std::string modelName = "";

			if ( PyString_Check( result ) )
				modelName = PyString_AsString( result );

			if ( modelName.empty() )
				modelName = s_defaultModel;

			if ( modelName.length() < 7  ||
					modelName.substr( modelName.length() - 6 ) != ".model" )
				modelName += ".model";

			// change it!
			ModelPtr m = Model::get( modelName );
			if (!m)
			{
				ERROR_MSG( "EditorChunkEntity::calculateModel - fail to find model %s\n"
							"Substituting with default model\n", modelName.c_str() );

				m = Model::get( s_defaultModel );
			}

			// the variable 'oldModelHolder' will keep the model alive until the method
			// returns, so the ChunkModelObstacle doesn't crash with an BB reference to
			// a deleted object (we should really change that bb referencing)
			oldModelHolder = model_;

			model_ = m;
			MF_ASSERT( model_ );

			if (surrogateMarker_)
				return;
		}
	}
	// Update the collision scene
	Chunk* c = chunk();

	// don't let the ref count go to 0 in the following commands
	ChunkItemPtr ourself = this;

	c->delStaticItem( this );
	c->addStaticItem( this );
}

/**
 *	Get the representative model for this entity
 */
ModelPtr EditorChunkEntity::reprModel() const
{
	return model_;
}

bool EditorChunkEntity::isDefaultModel() const
{
	if (model_)
        return model_->resourceID() == s_defaultModel;

	// else assume it is!
	return true;
}

void EditorChunkEntity::markModelDirty()
{
	SimpleMutexHolder permission( s_dirtyModelMutex_ );

	if (std::find(s_dirtyModelEntities_.begin(), s_dirtyModelEntities_.end(), this) == s_dirtyModelEntities_.end())
		s_dirtyModelEntities_.push_back(this);
}

void EditorChunkEntity::removeFromDirtyList()
{
	SimpleMutexHolder permission( s_dirtyModelMutex_ );

	std::vector<EditorChunkEntity*>::iterator it = 
		std::find(s_dirtyModelEntities_.begin(), s_dirtyModelEntities_.end(), this);

	if (it != s_dirtyModelEntities_.end())
		s_dirtyModelEntities_.erase( it );
}

void EditorChunkEntity::calculateDirtyModels()
{
	SimpleMutexHolder permission( s_dirtyModelMutex_ );

	uint i = 0;
	while (i < EditorChunkEntity::s_dirtyModelEntities_.size())
	{
		EditorChunkEntity* ent = EditorChunkEntity::s_dirtyModelEntities_[i];

		if (!ent->chunk() || !ent->chunk()->online())
		{
			++i;
		}
		else
		{
			ent->calculateModel();
 			EditorChunkEntity::s_dirtyModelEntities_.erase(EditorChunkEntity::s_dirtyModelEntities_.begin() + i);
		}
	}
}

Vector3 EditorChunkEntity::edMovementDeltaSnaps()
{
	if (pType_ && pType_->name() == std::string( "Door" ))
	{
		return Vector3( 1.f, 1.f, 1.f );
	}

	return EditorChunkItem::edMovementDeltaSnaps();
}

float EditorChunkEntity::edAngleSnaps()
{
	if (pType_ && pType_->name() == std::string( "Door" ))
	{
		return 90.f;
	}

	return EditorChunkItem::edAngleSnaps();
}



/// Write the factory statics stuff
#undef IMPLEMENT_CHUNK_ITEM_ARGS
#define IMPLEMENT_CHUNK_ITEM_ARGS (pSection, pChunk, &errorString)
IMPLEMENT_CHUNK_ITEM( EditorChunkEntity, entity, 1 )


// editor_chunk_entity.cpp
