/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef MYSQL_TYPEMAPPING_HPP
#define MYSQL_TYPEMAPPING_HPP

#include "Python.h"

#include "mysql_wrapper.hpp"
#include "idatabase.hpp"
#include "cstdmf/smartpointer.hpp"
#include "pyscript/pyobject_plus.hpp"
#include <vector>
#include <set>

class EntityDescription;
class MySqlEntityTypeMapping;
class StringLikeMapping;
typedef std::map<std::string, std::string> StrStrMap;
typedef std::map<std::string, EntityTypeID> StrTypeIDMap;
typedef std::set<std::string> StrSet;
typedef std::set<EntityTypeID> TypeIDSet;
typedef std::vector<MySqlEntityTypeMapping*> MySqlEntityTypeMappings;
typedef std::map< EntityTypeID, MySqlEntityTypeMapping*> MySqlEntityTypeMappingMap;

// __kyl__ (6/9/2005) Should only update CURRENT_VERSION if there are changes
// in the database that requires upgrading.
// static const uint32	DBMGR_CURRENT_VERSION	= 0;	// Pre-1.7
static const uint32	DBMGR_VERSION_1_7		= 1;		// 1.7
static const uint32	DBMGR_CURRENT_VERSION	= 2;
static const uint32	DBMGR_OLDEST_SUPPORTED_VERSION	= 1;
static const unsigned long 	SUPPORTED_MYSQL_VERSION	= 50012;

#define TABLE_NAME_PREFIX				"tbl"
#define TEMP_TABLE_NAME_PREFIX			"tmp"
#define	OLD_TABLE_NAME_PREFIX			"old"
#define	DEFAULT_SEQUENCE_COLUMN_NAME	"value"
#define DEFAULT_SEQUENCE_TABLE_NAME		"values"
#define ID_COLUMN_NAME					"id"
#define	ID_COLUMN_TYPE_NAME				"BIGINT AUTO_INCREMENT PRIMARY KEY"

// -----------------------------------------------------------------------------
// Section: PyConv
// -----------------------------------------------------------------------------
namespace PyConv
{
	inline PyObject* toPyObject( int8 i )	{	return PyInt_FromLong( i );	}
	inline PyObject* toPyObject( int16 i )	{	return PyInt_FromLong( i );	}
	inline PyObject* toPyObject( int32 i )	{	return PyInt_FromLong( i );	}
	inline PyObject* toPyObject( int64 i )	{	return PyLong_FromLongLong( i );	}
	inline PyObject* toPyObject( uint8 i )	{	return PyInt_FromLong( i );	}
	inline PyObject* toPyObject( uint16 i )	{	return PyInt_FromLong( i );	}
	inline PyObject* toPyObject( uint32 i )	{	return PyLong_FromUnsignedLong( i );	}
	inline PyObject* toPyObject( uint64 i )	{	return PyLong_FromUnsignedLongLong( i );	}
	inline PyObject* toPyObject( double d )	{	return PyFloat_FromDouble( d );	}
	inline PyObject* toPyObject( float f )	{	return toPyObject( double( f ) );	}

	template <class INT>
	inline bool toIntValue( INT& val, PyObject* pyObject )
	{
		bool isOK = true;
		if (PyInt_Check( pyObject ))
		{
			long i = PyInt_AsLong( pyObject );
			val = INT(i);
		}
		else if (PyLong_Check( pyObject ))
		{
			if (std::numeric_limits<INT>::is_signed)
			{
				long long i = PyLong_AsLongLong( pyObject );
				val = INT(i);
			}
			else
			{
				unsigned long long i = PyLong_AsUnsignedLongLong( pyObject );
				val = INT(i);
			}
			if (PyErr_Occurred())
				PyErr_Clear();	// Don't care about overflow errors.
		}
		else
		{
			isOK = false;
		}

		return isOK;
	}

	inline bool toValue( int8& val, PyObject* pyObject )
	{	return toIntValue( val, pyObject );	}
	inline bool toValue( int16& val, PyObject* pyObject )
	{	return toIntValue( val, pyObject );	}
	inline bool toValue( int32& val, PyObject* pyObject )
	{	return toIntValue( val, pyObject );	}
	inline bool toValue( int64& val, PyObject* pyObject )
	{	return toIntValue( val, pyObject );	}
	inline bool toValue( uint8& val, PyObject* pyObject )
	{	return toIntValue( val, pyObject );	}
	inline bool toValue( uint16& val, PyObject* pyObject )
	{	return toIntValue( val, pyObject );	}
	inline bool toValue( uint32& val, PyObject* pyObject )
	{	return toIntValue( val, pyObject );	}
	inline bool toValue( uint64& val, PyObject* pyObject )
	{	return toIntValue( val, pyObject );	}
	inline bool toValue( double& val, PyObject* pyObject )
	{
		bool isOK = PyFloat_Check( pyObject );
		if (isOK)
		{
			val = PyFloat_AsDouble( pyObject );
		}
		return isOK;
	}
	inline bool toValue( float& val, PyObject* pyObject )
	{
		double d;
		if (toValue( d, pyObject ))
		{
			val = float( d );
			return true;
		}
		return false;
	}
}

/**
 *	This interface is used by MySqlTypeMapping when temporary mappings are
 * 	in use.
 */
struct ITemporaryMappingsConverter
{
	// This method returns the EntityTypeID for temporary mapping corresponding
	// to the EntityTypeID of the non-temporary mapping.
	virtual EntityTypeID getTempTypeID( EntityTypeID oldTypeID ) = 0;
	virtual void setTempMapping( MySqlEntityTypeMapping& tempMapping,
		EntityTypeID oldTypeID, DatabaseID dbID,
		const MySqlEntityTypeMapping& oldMapping ) = 0;
};

void initSpecialBigWorldTables( MySqlTransaction& transaction );

/**
 *	This interface is used by PropertyMapping to tell the visitor what tables
 * 	and columns it needs.
 */
struct ITableCollector
{
	// The following enum values are stored in bigworldTableMetadata.idx.
	// If you change their numerical values, you must update upgradeDatabase().
    /* huangshanquan  2009-12-28 change begin CSOL-7399*/
	enum IndexType
	{
		IndexTypeNone 		= 0,
		IndexTypePrimary	= 1,
		IndexTypeName		= 2,
		IndexTypeParentID	= 3,
		IndexTypeUnique 	= 4,
		IndexTypeNormal 	= 5
	};
    /* huangshanquan 2009-12-28 change end*/

	struct ColumnInfo
	{
		std::string typeStr;
		IndexType	indexType;
		// Hack for TEXT and BLOB types that can't have DEFAULT value
		// specification in typeStr.
		bool		shouldSetDefault;
		std::string	defaultVal;

		ColumnInfo() : indexType( IndexTypeNone ), shouldSetDefault( false ) {}

		bool operator==( const ColumnInfo& other ) const
		{ return (typeStr == other.typeStr) &&
				(indexType == other.indexType); }
		bool operator!=( const ColumnInfo& other ) const
		{ return !this->operator==( other ); }
	};
	// Map of column name to ColumnInfo.
	typedef std::map< std::string, ColumnInfo > NameToColInfoMap;

	struct UpdatedColumnInfo : public ColumnInfo
	{
		IndexType	oldIndexType;

		UpdatedColumnInfo( const ColumnInfo& newCol, const ColumnInfo& oldCol )
			:  ColumnInfo( newCol ), oldIndexType( oldCol.indexType )
		{}
	};
	// Map of column name to UpdatedColumnInfo.
	typedef std::map< std::string, UpdatedColumnInfo > NameToUpdatedColInfoMap;


	virtual void requireTable( const std::string& tblName,
		const NameToColInfoMap& columns ) = 0;
};

/**
 *	This class is used to access tables that stores entity meta data.
 */
class BigWorldMetaData
{
	MySqlTransaction&	transaction_;
	MySqlStatement		stmtFindTables_;
	MySqlStatement		stmtFindTableColumns_;
	MySqlStatement		stmtGetEntityTypes_;
	MySqlStatement		stmtGetEntityTypeID_;

	MySqlBuffer bufferTableName_;
	MySqlBuffer bufferColumnName_;
	MySqlBuffer bufferTypeName_;
	int			bufferInteger_;
public:
	BigWorldMetaData( MySqlTransaction& transaction );

	void getTables( StrSet& tables );
	void getTableColumns( ITableCollector::NameToColInfoMap& columns,
		const std::string& tableName );

	void addColumn( const std::string& tableName,
		const std::string& columnName,
		const ITableCollector::ColumnInfo& columnInfo )
	{
		BigWorldMetaData::addColumn( transaction_, tableName, columnName, columnInfo );
	}
	void addColumns( const std::string& tableName,
		const ITableCollector::NameToColInfoMap& columns )
	{
		BigWorldMetaData::addColumns( transaction_, tableName, columns );
	}
	void removeColumn( const std::string& tableName,
		const std::string& columnName )
	{
		BigWorldMetaData::removeColumn( transaction_, tableName, columnName );
	}
	void removeAllColumns( const std::string& tableName )
	{
		BigWorldMetaData::removeAllColumns( transaction_, tableName );
	}
	void setColumnType( const std::string& tableName,
		const std::string& columnName,
		const ITableCollector::ColumnInfo& columnInfo )
	{
		BigWorldMetaData::setColumnType( transaction_, tableName, columnName,
			columnInfo );
	}

	void getEntityTypes( StrTypeIDMap& types );
	EntityTypeID getEntityTypeID( const std::string& entityName );
	void setEntityTypeID( const std::string& entityName,
		EntityTypeID typeID	)
	{
		BigWorldMetaData::setEntityTypeID( transaction_, entityName, typeID );
	}
	void addEntityType( const std::string& entityName, EntityTypeID typeID )
	{
		BigWorldMetaData::addEntityType( transaction_, entityName, typeID );
	}
	void removeEntityType( const std::string& entityName )
	{
		BigWorldMetaData::removeEntityType( transaction_, entityName );
	}
	void updateEntityTypes( const StrTypeIDMap& obsoleteTypes,
		const StrTypeIDMap& newTypes, const StrTypeIDMap& changedTypes )
	{
		BigWorldMetaData::updateEntityTypes( transaction_, obsoleteTypes,
			newTypes, changedTypes);
	}

	static void addColumn( MySqlTransaction& transaction,
		const std::string& tableName, const std::string& columnName,
		const ITableCollector::ColumnInfo& columnInfo );
	static void addColumns( MySqlTransaction& transaction,
		const std::string& tableName,
		const ITableCollector::NameToColInfoMap& columns );
	static void removeColumn( MySqlTransaction& transaction,
		const std::string& tableName, const std::string& columnName );
	static void removeAllColumns( MySqlTransaction& transaction,
		const std::string& tableName );
	static void setColumnType( MySqlTransaction& transaction,
		const std::string& tableName, const std::string& columnName,
		const ITableCollector::ColumnInfo& columnInfo );

	static void setEntityTypeID( MySqlTransaction& transaction,
		const std::string& entityName, EntityTypeID typeID	);
	static void addEntityType( MySqlTransaction& transaction,
		const std::string& entityName, EntityTypeID typeID );
	static void removeEntityType( MySqlTransaction& transaction,
		const std::string& entityName );
	static void updateEntityTypes( MySqlTransaction& transaction,
		const StrTypeIDMap& obsoleteTypes, const StrTypeIDMap& newTypes,
		const StrTypeIDMap& changedTypes );
};

struct TableMigrationData
{
	struct NewTableData
	{
		ITableCollector::NameToColInfoMap columns;
	};
	typedef std::map< std::string, NewTableData > NewTableDataMap;

	struct UpdatedTableData
	{
		ITableCollector::NameToColInfoMap			columns;
		ITableCollector::NameToColInfoMap			obsoleteColumns;
		ITableCollector::NameToColInfoMap			newColumns;
		ITableCollector::NameToUpdatedColInfoMap	updatedColumns;
	};
	typedef std::map< std::string, UpdatedTableData > UpdatedTableDataMap;

	UpdatedTableDataMap		updatedTables;
	NewTableDataMap			newTables;
	StrSet 					obsoleteTables;
	StrSet 					tables;

	StrTypeIDMap 			updatedTypes;
	StrTypeIDMap 			newTypes;
	StrTypeIDMap 			obsoleteTypes;
	StrSet 					types;

	DefsSyncMode			maxRequiredSyncMode;

	TableMigrationData()
		: maxRequiredSyncMode( DefsSyncModeNoSync )
	{}

	void init()
	{
		updatedTables.clear();
		newTables.clear();
		obsoleteTables.clear();
		tables.clear();
		updatedTypes.clear();
		newTypes.clear();
		obsoleteTypes.clear();
		types.clear();
		maxRequiredSyncMode = DefsSyncModeNoSync;
	}

	size_t getNumUpdatedTables() const	{	return updatedTables.size();	}
	size_t getNumNewTables() const		{	return newTables.size();	}
	size_t getNumObsoleteTables() const	{	return obsoleteTables.size();	}
};

/**
 *	This class is used to visit all the entity properties so that it can
 * 	collect all the tables and columns needed. It updates the
 * 	provided TableMigrationData structure.
 */
class TableCollector : public ITableCollector
{
	TableMigrationData&		data_;

	BigWorldMetaData&		metaData_;
public:
	TableCollector( TableMigrationData& data, BigWorldMetaData& metaData );
	virtual ~TableCollector();

	void addType( EntityTypeID bigworldID, const std::string& name );
	virtual void requireTable( const std::string& name,
		const NameToColInfoMap& columns );
};

/**
 *	This class is used to visit all the entity properties so that it can
 * 	collect all the tables and columns needed. It simply accumulates all the
 * 	required tables.
 */
class SimpleTableCollector : public ITableCollector
{
	TableMigrationData::NewTableDataMap	tables_;

public:
	virtual ~SimpleTableCollector()	{}

	const TableMigrationData::NewTableDataMap& getTables() const
	{
		return tables_;
	}

	SimpleTableCollector& operator+=( const SimpleTableCollector& rhs );

	virtual void requireTable( const std::string& name,
			const NameToColInfoMap& columns );
};

void createEntityTables( MySqlTransaction& transaction,
	const TableMigrationData::NewTableDataMap& tables );
void removeEntityTables( MySqlTransaction& transaction,
	const TableMigrationData::NewTableDataMap& tables );
void removeEntityTables( MySqlTransaction& transaction, const StrSet& tables );
void createTempEntityTables( MySqlTransaction& transaction,
	const TableMigrationData::NewTableDataMap& tables );
void removeTempEntityTables( MySqlTransaction& transaction,
	const TableMigrationData::NewTableDataMap& tables );
void swapTempTablesWithReal( MySqlTransaction& transaction,
	const TableMigrationData::NewTableDataMap& tempTables,
	const TableMigrationData::UpdatedTableDataMap& updatedTables,
	const TableMigrationData::NewTableDataMap& newTables,
	const StrSet& obsoleteTables );
void removeSwappedOutTables( MySqlTransaction& transaction,
	const TableMigrationData::NewTableDataMap& tempTables,
	const TableMigrationData::NewTableDataMap& newTables,
	const StrSet& obsoleteTables );

/**
 *	This is the base class for classes that maps BigWorld types to MySQL tables
 * 	and columsn.
 */
class PropertyMapping : public ReferenceCount
{
public:
	PropertyMapping( int numColumns, const std::string& propName ) :
		propName_(propName), numColumns_(numColumns)
	{}
	virtual ~PropertyMapping() {}

	// this method determines columns required in tables and adds them to a
	// table collector.
	virtual void createTables( ITableCollector& ti,
			ITableCollector::NameToColInfoMap& cols ) = 0;
	// after that is called, and the initialisation is complete,
	// we can safely create SQL statements on those tables (which we need to
	// do for sequences), so we do that here
	virtual void prepareSQL( MySql& ) = 0;

	// this method returns a comma seperated list of the names of the columns
	// this PropertyMapping defines - useful for setting up SQL queries
	// can pass a std::string suffix to be appended to each column
	virtual std::string getColumnNames( const std::string& suffix = std::string() ) = 0;
	// this method returns the number of columns that this PropertyMapping
	// will add to a table
	int numColumns() const { return numColumns_; }
	// add this property to the set of bindings for a statement
	virtual void addToBindings( MySqlBindings& ) = 0;

	// which property do we map to in a DataSection
	const std::string& propName() const { return propName_; }
	// Set our bound value from the stream. Must be consistent with
	// streaming done in DataType. e.g. DataType::createFromStream().
	virtual void streamToBound( BinaryIStream & strm ) = 0;
	// Put our bound value into the stream. Must be consistent with
	// streaming done in DataType. e.g. DataType::addToStream().
	virtual void boundToStream( BinaryOStream & strm ) const = 0;
	// Puts the default value into the stream.
	virtual void defaultToStream( BinaryOStream & strm ) const = 0;
	// Sets the bindings to NULL.
	virtual void nullifyBound() = 0;

	// This method returns true if the type or any of its children
	// stores data in additional table(s).
	virtual bool hasTable() const = 0;
	// Updates any child tables associated with the property.
	// e.g. sequences have their own tables. Bindings must be set
	// (e.g. via streamToBound()) prior to calling this method
	// otherwise crap data will be written to the database.
	virtual void updateTable( MySqlTransaction& transaction,
		DatabaseID parentID ) = 0;
	// Gets data for any child tables into bindings.
	virtual void getTableData( MySqlTransaction& transaction,
		DatabaseID parentID ) = 0;

	// Types that can be an element in a sequence must implement
	// createSequenceBuffer() which returns an ISequenceBuffer specific
	// to that type.
	struct ISequenceBuffer
	{
		virtual ~ISequenceBuffer() {};

		// Deserialise from the stream and store value in buffer.
		// If this is called multiple times without calling reset()
		// values should be accumulated instead of overwritten.
		virtual void streamToBuffer( int numElems,
										BinaryIStream& strm ) = 0;
		// Put the idx-th buffer value into the stream.
		virtual void bufferToStream( BinaryOStream& strm, int idx ) const = 0;
		// Set binding with value of element number "idx" in the buffer.
		virtual void bufferToBound( PropertyMapping& binding,
									int idx ) const = 0;
		// Add binding value into buffer.
		virtual void boundToBuffer( const PropertyMapping& binding ) = 0;
		virtual int	getNumElems() const = 0;
		// Resets buffer to "empty".
		virtual void reset() = 0;
	};
	virtual ISequenceBuffer* createSequenceBuffer() const = 0;

	// and this method performs a cascading delete on any child tables
	// no-op by default for same reasons as above
	virtual void deleteChildren( MySqlTransaction&, DatabaseID parentID ) = 0;

	// This method creates a Python representation of this property.
	virtual PyObject* createPyObject() const = 0;
	// This method sets the binding value to the value of Python object, or
	// as close as possible if type doesn't match.
	virtual void setValue( PyObject* pyObject ) = 0;

protected:
	void addColumns( int num ) { numColumns_ += num; }

private:
	std::string propName_;
	int numColumns_;
};

typedef SmartPointer<PropertyMapping> PropertyMappingPtr;
typedef std::vector<PropertyMappingPtr> PropertyMappings;
typedef std::vector<PropertyMappings> TypeMappings;

void createEntityPropMappings( TypeMappings& types,
							const EntityDefs& entityDefs,
							const std::string& tableNamePrefix );
void destroyEntityPropMappings( MySqlEntityTypeMappings& typeMappings );
void createEntityMappings( MySqlEntityTypeMappings& entityMappings,
	TypeMappings& propMappings, const EntityDefs& entityDefs,
	const std::string& tableNamePrefix, MySql& connection,
	const TypeIDSet* pTypes = NULL);
void createEntityMappings( MySqlEntityTypeMappings& entityMappings,
	const EntityDefs& entityDefs, const std::string& tableNamePrefix,
	MySql& connection, const TypeIDSet* pTypes = NULL );

class MySqlEntityTypeMapping : public ReferenceCount
{
public:

	MySqlEntityTypeMapping( MySql&, const std::string&, EntityTypeID type,
		const EntityDescription&, PropertyMappings&, const std::string&,
		bool isForMigration = false );
	DatabaseID getDbID( MySqlTransaction& transaction,
		const std::string& name );
	bool getName( MySqlTransaction& transaction, DatabaseID dbID,
		std::string& name );
	bool checkExists( MySqlTransaction& transaction, DatabaseID dbID );
	bool deleteWithID( MySqlTransaction& t, DatabaseID id );

	void streamToBound( BinaryIStream& strm );
	DatabaseID insertNew( MySqlTransaction& transaction );
	bool update( MySqlTransaction& transaction, DatabaseID dbID );

	bool getPropsByID( MySqlTransaction& transaction, DatabaseID dbID,
		std::string& name );
	DatabaseID getPropsByName( MySqlTransaction& transaction,
		const std::string& name );
	void boundToStream( BinaryOStream& strm, const std::string* pPasswordOverride );

	// Methods used for migration
	bool insertNewWithID( MySqlTransaction& transaction, DatabaseID dbID );
	DatabaseID getNextPropsByID( MySqlTransaction& transaction, DatabaseID dbID );

	// get the index of the entity type as mapped by the database
	int getDatabaseTypeID() const	{ return mappedType_;	}
	// Whether this entity has a name property i.e. dbMgr/nameProperty
	bool hasNameProp() const		{ return pNameProp_ != 0;	}

	PropertyMapping* getPropMapByName( const std::string& name )
	{
		std::map< std::string, PropertyMapping*>::iterator propMapIter =
			propsNameMap_.find(name);
		return ( propMapIter != propsNameMap_.end() )
					? propMapIter->second : 0;
	}

	const PropertyMapping* getPropMapByName( const std::string& name ) const
	{
		return const_cast<MySqlEntityTypeMapping*>(this)->getPropMapByName( name );
	}

	const EntityDescription& getEntityDescription() const { return description_; }
	const PropertyMappings& getPropertyMappings() const	{ return properties_; }
	PropertyMappings& getPropertyMappings() { return properties_; }

private:
	const EntityDescription& description_;
	PropertyMappings properties_;
	DatabaseID id_;
	MySqlStatement insertStmt_;
	std::auto_ptr<MySqlStatement> pUpdateStmt_;
	std::auto_ptr<MySqlStatement> pSelectNamedStmt_;
	std::auto_ptr<MySqlStatement> pSelectNamedForIDStmt_;
	std::auto_ptr<MySqlStatement> pSelectIDForNameStmt_;
	MySqlStatement selectIDForIDStmt_;
	std::auto_ptr<MySqlStatement> pSelectIDStmt_;
	MySqlStatement deleteIDStmt_;
	std::map< std::string, PropertyMapping* > propsNameMap_;
	// Statements needed to do migration.
	std::auto_ptr<MySqlStatement> pInsertWithIDStmt_;
	std::auto_ptr<MySqlStatement> pSelectNextIDStmt_;

	// Non-configurable cell properties.
	// Enums must be in the order that these properties are stored in the stream.
	enum FixedCellProp
	{
		CellPositionIdx,
		CellDirectionIdx,
		CellSpaceIDIdx,
		NumFixedCellProps
	};
	PropertyMapping* fixedCellProps_[ NumFixedCellProps ];
	StringLikeMapping* pNameProp_;

	// we cache what the EntityTypeID is in the database
	int mappedType_;

	bool getPropsImpl( MySqlTransaction& transaction, MySqlStatement& stmt );
};

class MySqlTypeMapping
{
public:
	MySqlTypeMapping( MySql& con, const EntityDefs& entityDefs,
		const char * tableNamePrefix , const TypeIDSet* pTypes = NULL );
	~MySqlTypeMapping();

	void setEntityMappings( MySql& connection, const EntityDefs& entityDefs,
		const char * tableNamePrefix = TABLE_NAME_PREFIX,
		const TypeIDSet* pTypes = NULL );
	void clearMappings();
	void setTemporaryMappings( MySql& connection, const EntityDefs& entityDefs,
		const std::string& tableNamePrefix,
		ITemporaryMappingsConverter& converter );
	void clearTemporaryMappings();
	bool hasTemporaryMappings() const	{	return pTempMappings_; }

	bool hasNameProp( EntityTypeID typeID ) const;
	DatabaseID getEntityDbID( MySqlTransaction& transaction,
		EntityTypeID typeID, const std::string& name );
	bool getEntityName( MySqlTransaction& transaction,
		EntityTypeID typeID, DatabaseID dbID, std::string& name );
	bool checkEntityExists( MySqlTransaction& transaction,
		EntityTypeID typeID, DatabaseID dbID );
	bool deleteEntityWithID( MySqlTransaction&, EntityTypeID, DatabaseID );

	// Transfer data into MySQL bindings, ready for DB operation.
	void streamToBound( EntityTypeID typeID, DatabaseID dbID,
		BinaryIStream& entityDataStrm );
	void baseRefToBound( const EntityMailBoxRef& baseRef );
	void logOnMappingToBound( const std::string& logOnName,
		const std::string& password, EntityTypeID typeID,
		const std::string& recordName );
	std::string getBoundLogOnName() const {	return boundLogOnName_.getString(); }

	// DB operations that use the entity data already in MySQL bindings.
	DatabaseID newEntity( MySqlTransaction& transaction, EntityTypeID typeID );
	bool updateEntity( MySqlTransaction& transaction, EntityTypeID typeID,
		DatabaseID dbID );
	void addLogOnRecord( MySqlTransaction&, EntityTypeID, DatabaseID );
	void removeLogOnRecord( MySqlTransaction&, EntityTypeID, DatabaseID );
	void setLogOnMapping( MySqlTransaction& transaction );

	// DB operations that retrieve entity data from database into MySQL bindings.
	bool getEntityToBound( MySqlTransaction& transaction, EntityDBKey& ekey );

	// Operations that returns the entity data already in MySQL bindings.
	void boundToStream( EntityTypeID typeID, BinaryOStream& entityDataStrm,
		const std::string* pPasswordOverride );

	// Old-style operations that store stuff into bindings, execute DB query
	// and return results in one swell swoop.
	bool getLogOnRecord( MySqlTransaction&, EntityTypeID, DatabaseID,
		EntityMailBoxRef& );
	bool getLogOnMapping( MySqlTransaction&, const std::string& logOnName,
			std::string& password, EntityTypeID& typeID, std::string& recordName,
            int &isblock , int &istakeover );

private:
	MySqlEntityTypeMappings	mappings_;
	MySqlEntityTypeMappingMap* pTempMappings_;
	ITemporaryMappingsConverter* pTempConverter_;
	MySqlStatement stmtAddLogOn_;
	MySqlStatement stmtRemoveLogOn_;
	MySqlStatement stmtGetLogOn_;

	MySqlStatement stmtSetLogOnMapping_;
	MySqlStatement stmtGetLogOnMapping_;

	int boundTypeID_;
	int isblock_;
	int istakeover_;
	DatabaseID boundDatabaseID_;
	ObjectID boundObjectID_;
	MySqlValueWithNull<ObjectID> boundOptObjectID_;
	MySqlValueWithNull<uint32> boundAddress_;
	MySqlValueWithNull<uint16> boundPort_;
	MySqlValueWithNull<uint16> boundSalt_;

	MySqlBuffer boundLogOnName_;
	MySqlBuffer boundPassword_;
	MySqlBuffer boundRecordName_;
};

void initEntityTables( MySql& con, const EntityDefs& entityDefs,
	uint32 version );

#endif
