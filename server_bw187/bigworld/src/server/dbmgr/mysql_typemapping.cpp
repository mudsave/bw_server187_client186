/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "mysql_typemapping.hpp"
#include "database.hpp"
#include "mysql_notprepared.hpp"	// for StringConv
#include "resmgr/xml_section.hpp"
#include "cstdmf/debug.hpp"
#include "cstdmf/base64.h"
#include "cstdmf/memory_stream.hpp"
#include "cstdmf/smartpointer.hpp"
#include "server/bwconfig.hpp"
#include "pyscript/pyobject_plus.hpp"
#include "pyscript/pickler.hpp"
#include "entitydef/entity_description_map.hpp"
#include "entitydef/data_types.hpp"
#include "chunk/unique_id.hpp"

#include <vector>
#include <set>
#include <map>
#include <stack>

DECLARE_DEBUG_COMPONENT( 0 );

// -----------------------------------------------------------------------------
// Section: useful constants
// -----------------------------------------------------------------------------
// __kyl__ (20/7/2005) For some reason I can't find a #define for these but they
// are well known to MySQL developers.
enum MySQLLimits
{
	MySQLMaxTableNameLen = 64,
	MySQLMaxColumnNameLen = 64,
	MySQLMaxDbNameLen = 64
};

enum BWMySQLLimits
{
	BWMySQLMaxTypeNameLen = 64,
	BWMySQLMaxLogOnNameLen = 255,
	BWMySQLMaxLogOnPasswordLen = 255,
	BWMySQLMaxNamePropertyLen = 255
};

// -----------------------------------------------------------------------------
// Section: useful typedefs
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Section: table management
// -----------------------------------------------------------------------------

/**
 * 	This function creates all the tables that DbMgr uses to non-entity
 * 	data e.g. logons, meta data etc.
 */
void initSpecialBigWorldTables( MySqlTransaction& transaction )
{
	char buffer[512];
	sprintf( buffer, "CREATE TABLE IF NOT EXISTS bigworldTableMetadata "
			 "(tbl CHAR(%d) NOT NULL, col CHAR(%d) NOT NULL, typ CHAR(%d) "
			 "NOT NULL, idx INT NOT NULL, PRIMARY KEY(tbl, col)) TYPE=InnoDB",
			 MySQLMaxTableNameLen, MySQLMaxColumnNameLen,
			 BWMySQLMaxTypeNameLen );
	transaction.execute( buffer );
	transaction.execute( "CREATE TABLE IF NOT EXISTS bigworldEntityTypes "
			 "(typeID INT NOT NULL AUTO_INCREMENT, bigworldID INT, "
			 "name CHAR(255) NOT NULL UNIQUE,"
			 " PRIMARY KEY(typeID), KEY(bigworldID)) TYPE=InnoDB" );

	transaction.execute( "CREATE TABLE IF NOT EXISTS bigworldLogOns "
			 "(databaseID BIGINT NOT NULL, typeID INT NOT NULL, "
			 "objectID INT, ip INT UNSIGNED, port SMALLINT UNSIGNED, salt SMALLINT UNSIGNED, "
			 " PRIMARY KEY(typeID, databaseID)) TYPE=InnoDB" );
	sprintf( buffer, "CREATE TABLE IF NOT EXISTS bigworldLogOnMapping "
			 "(logOnName VARCHAR(%d) NOT NULL, password VARCHAR(%d),"
			 " typeID INT NOT NULL, recordName VARCHAR(%d),"
			 " PRIMARY KEY(logOnName)) TYPE=InnoDB",
			 BWMySQLMaxLogOnNameLen, BWMySQLMaxLogOnPasswordLen,
			 BWMySQLMaxNamePropertyLen );
	transaction.execute( buffer );

	transaction.query( "SHOW COLUMNS FROM bigworldLogOnMapping like \"gm%\";" );
	MYSQL_RES* pResult = transaction.storeResult();
	MySqlResult result( *pResult );

    if (pResult)
    {
        bool recreate = false;
        
        if ( result.getNumRows() != 0 )
        {
            /* ÅÐ¶Ï×Ö¶ÎÊÇ·ñÔÊÐíÎª¿Õ */
            while ( result.getNextRow() )
            {
                if( result.getField( 2 ) == std::string("YES") )
                {
                    recreate = true;
                }
            }

            if( recreate )
            {
                sprintf( buffer, "ALTER TABLE `bigworldLogOnMapping` DROP COLUMN `gmpassword`, DROP COLUMN `gmtimelimit`");
                transaction.execute( buffer );
            }
        }

        if ( result.getNumRows() == 0  || recreate )
        {
            INFO_MSG( "Add gmpassword,gmtimelimit to bigworldLogOnMapping......\n");

            sprintf( buffer, "ALTER TABLE `bigworldLogOnMapping`"
                              " ADD COLUMN `gmpassword` VARCHAR(%d) NOT NULL DEFAULT '' AFTER `recordName`,"
                              " ADD COLUMN `gmtimelimit` DATETIME NOT NULL DEFAULT '0000-00-00' AFTER `gmpassword`", 
                              BWMySQLMaxLogOnPasswordLen);
            transaction.execute( buffer );
        }
    }

	transaction.query( "SHOW COLUMNS FROM bigworldLogOnMapping like \"blocktime\";" );
	MYSQL_RES* pResultBlock = transaction.storeResult();
	MySqlResult resultBlock( *pResultBlock );

    if ( resultBlock.getNumRows() == 0 )
    {
        INFO_MSG( "Add blocktime to bigworldLogOnMapping......\n");

        sprintf( buffer, "ALTER TABLE `bigworldLogOnMapping`"
                          " ADD COLUMN `blocktime` DATETIME NOT NULL DEFAULT '0000-00-00' AFTER `gmtimelimit`");
        transaction.execute( buffer );
    }
}

// -----------------------------------------------------------------------------
// Section: BigWorld meta data
// -----------------------------------------------------------------------------
/**
 *	Constructor. Can only be called after initSpecialBigWorldTables().
 *
 * 	@param	The connection to MySQL server.
 */
BigWorldMetaData::BigWorldMetaData( MySqlTransaction& transaction ) :
	transaction_( transaction ),
	stmtFindTables_( transaction.get(), "SELECT tbl FROM bigworldTableMetadata" ),
	stmtFindTableColumns_( transaction.get(), "SELECT col, typ, idx FROM "
							"bigworldTableMetadata WHERE tbl=?" ),
	stmtGetEntityTypes_( transaction.get(),
						"SELECT name,bigworldID FROM bigworldEntityTypes" ),
	stmtGetEntityTypeID_( transaction.get(), "SELECT bigworldID FROM "
						"bigworldEntityTypes WHERE name=?" ),
	bufferTableName_( MySQLMaxTableNameLen ),
	bufferColumnName_( MySQLMaxColumnNameLen ),
	bufferTypeName_( BWMySQLMaxTypeNameLen ),
	bufferInteger_( -1 )
{
	MySqlBindings b;

	b.clear();
	b << bufferTableName_;
	stmtFindTables_.bindResult( b );

	b.clear();
	b << bufferTableName_;
	stmtFindTableColumns_.bindParams( b );

	b.clear();
	b << bufferColumnName_ << bufferTypeName_ << bufferInteger_;
	stmtFindTableColumns_.bindResult( b );

	b.clear();
	b << bufferTypeName_;
	stmtGetEntityTypeID_.bindParams( b );

	b.clear();
	b << bufferInteger_;
	stmtGetEntityTypeID_.bindResult( b );

	b.clear();
	b << bufferTypeName_ << bufferInteger_;
	stmtGetEntityTypes_.bindResult( b );
}

/**
 *	Retrieves all the table names in our meta data.
 *
 * 	@param	This parameter receives the list of tables.
 */
void BigWorldMetaData::getTables( StrSet& tables )
{
	transaction_.execute( stmtFindTables_ );
	while ( stmtFindTables_.fetch() )
	{
		tables.insert( bufferTableName_.getString() );
	}
}

/**
 * 	Retrieves meta data of all the columns for a given table.
 *
 * 	@param	columns	This output parameter receives the list of columns.
 * 	The key is the column name and the data is the column type.
 * 	@param	tableName	The name of the table to get the columns for.
 */
void BigWorldMetaData::getTableColumns(
	ITableCollector::NameToColInfoMap& columns, const std::string& tableName )
{
	bufferTableName_.setString( tableName );
	transaction_.execute( stmtFindTableColumns_ );
	while (stmtFindTableColumns_.fetch())
	{
		ITableCollector::ColumnInfo& colInfo =
				columns[bufferColumnName_.getString()];
		colInfo.typeStr = bufferTypeName_.getString();
		colInfo.indexType = ITableCollector::IndexType( bufferInteger_ );
	}
}

/**
 * 	Adds the column to our meta information.
 *
 * 	@param	transaction	The transaction to use to update the DB.
 * 	@param	tableName	The name of the table that has the column.
 * 	@param	columnName	The column name.
 * 	@param	columnType	The column type.
 */
void BigWorldMetaData::addColumn( MySqlTransaction& transaction,
	const std::string& tableName, const std::string& columnName,
	const ITableCollector::ColumnInfo& columnInfo )
{
	std::stringstream stmtStrm;
	stmtStrm << "INSERT INTO bigworldTableMetadata (tbl,col,typ,idx) VALUES ('"
			<< tableName << "','" << columnName << "','" << columnInfo.typeStr
			<< "'," << columnInfo.indexType << ")";

	transaction.execute( stmtStrm.str() );
}

/**
 * 	Adds multiple columns to our meta information.
 *
 * 	@param	transaction	The transaction to use to update the DB.
 * 	@param	tableName	The name of the table that has the column.
 * 	@param	columns		Map of column name to column type.
 */
void BigWorldMetaData::addColumns( MySqlTransaction& transaction,
	const std::string& tableName,
	const ITableCollector::NameToColInfoMap& columns )
{
	for ( ITableCollector::NameToColInfoMap::const_iterator i = columns.begin();
			i != columns.end(); ++i )
	{
		BigWorldMetaData::addColumn( transaction, tableName, i->first, i->second );
	}
}

/**
 * 	Removes the column from our meta information.
 *
 * 	@param	transaction	The transaction to use to update the DB.
 * 	@param	tableName	The name of the table that has the column.
 * 	@param	columnName	The column name.
 */
void BigWorldMetaData::removeColumn( MySqlTransaction& transaction,
	const std::string& tableName, const std::string& columnName )
{
	transaction.execute( "DELETE FROM bigworldTableMetadata WHERE "
			 "tbl='" + tableName + "' AND col='" + columnName + "'" );
}

/**
 * 	Removes all the columns for the given table from our meta information.
 *
 * 	@param	transaction	The transaction to use to update the DB.
 * 	@param	tableName	The table which will have all its column meta
 * 	information removed.
 */
void BigWorldMetaData::removeAllColumns( MySqlTransaction& transaction,
	const std::string& tableName )
{
	transaction.execute( "DELETE FROM bigworldTableMetadata WHERE tbl='" +
		tableName + "'" );
}

/**
 * 	Changes the type of the column in our meta information.
 *
 * 	@param	transaction	The transaction to use to update the DB.
 * 	@param	tableName	The name of the table that has the column.
 * 	@param	columnName	The column name.
 * 	@param	columnType	The column type.
 */
void BigWorldMetaData::setColumnType( MySqlTransaction& transaction,
	const std::string& tableName, const std::string& columnName,
	const ITableCollector::ColumnInfo& columnInfo )
{
	std::stringstream stmtStrm;
	stmtStrm << "UPDATE bigworldTableMetadata SET typ='" << columnInfo.typeStr
			<< "', idx=" << columnInfo.indexType << " WHERE tbl='" << tableName
			<< "' AND col='" << columnName << "'";
	transaction.execute( stmtStrm.str() );
}

/**
 *	This method retrieves all the entity types in our meta information.
 *
 * 	@param	types	This parameter will receive the list of type name to
 * 	type ID mappings.
 */
void BigWorldMetaData::getEntityTypes( StrTypeIDMap& types )
{
	transaction_.execute( stmtGetEntityTypes_ );
	while ( stmtGetEntityTypes_.fetch() )
	{
		types.insert( std::pair< std::string, EntityTypeID >(
					bufferTypeName_.getString(), bufferInteger_ ) );
	}
}

/**
 * 	This method retrieves the EntityTypeID associated with the entity name
 * 	from our meta information.
 *
 *  @param	entityName	The entity name.
 */
EntityTypeID BigWorldMetaData::getEntityTypeID( const std::string& entityName )
{
	bufferTypeName_.setString( entityName );
	transaction_.execute( stmtGetEntityTypeID_ );

	EntityTypeID typeID = INVALID_TYPEID;
	if (stmtGetEntityTypeID_.resultRows() > 0)
	{
		MF_ASSERT(stmtGetEntityTypeID_.resultRows() == 1);

		stmtGetEntityTypeID_.fetch();
		typeID = bufferInteger_;
	}
	return typeID;
}

/**
 * 	This method sets the EntityTypeID associated with the entity name
 * 	into our meta information.
 *
 * 	@param	transaction	The transaction to use to update the DB.
 *  @param	entityName	The entity name.
 *  @param	typeID		The new type ID.
 */
void BigWorldMetaData::setEntityTypeID( MySqlTransaction& transaction,
	const std::string& entityName, EntityTypeID typeID )
{
	std::stringstream	stmtStrm;
	stmtStrm << "UPDATE bigworldEntityTypes SET bigworldID=" << typeID <<
		" WHERE name='" << entityName << "'";
	transaction.execute( stmtStrm.str() );
}

/**
 * 	This method adds a new entity name to EntityTypeID mapping
 * 	into our meta information.
 *
 * 	@param	transaction	The transaction to use to update the DB.
 *  @param	entityName	The entity name.
 *  @param	typeID		The type ID.
 */
void BigWorldMetaData::addEntityType( MySqlTransaction& transaction,
	const std::string& entityName, EntityTypeID typeID )
{
	std::stringstream	stmtStrm;
	stmtStrm << "INSERT INTO bigworldEntityTypes (typeID, bigworldID, name)"
			"VALUES (NULL, " << typeID << ", '" << entityName << "')";
	transaction.execute( stmtStrm.str() );
}

/**
 * 	This method removes an entity type mapping from our meta information.
 *
 * 	@param	transaction	The transaction to use to update the DB.
 *  @param	entityName	The entity name.
 */
void BigWorldMetaData::removeEntityType( MySqlTransaction& transaction,
	const std::string& entityName )
{
	std::stringstream	stmtStrm;
	stmtStrm << "DELETE FROM bigworldEntityTypes WHERE name='" <<
		entityName << "'";
	transaction.execute( stmtStrm.str() );
}

/**
 * 	Updates the meta information with the given bigworld type changes.
 *
 * 	@param	transaction		The transaction to use to update the DB.
 *  @param	obsoleteTypes	These types will be deleted from our meta info.
 *  @param	newTypes		These types will be added to our meta info.
 * 	@param	changedTypes	These types will be updated in our meta info.
 */
void BigWorldMetaData::updateEntityTypes( MySqlTransaction& transaction,
	const StrTypeIDMap& obsoleteTypes, const StrTypeIDMap& newTypes,
	const StrTypeIDMap& changedTypes )
{
	for ( StrTypeIDMap::const_iterator i = obsoleteTypes.begin();
			i != obsoleteTypes.end(); ++i )
	{
		BigWorldMetaData::removeEntityType( transaction, i->first );
	}

	for ( StrTypeIDMap::const_iterator i = changedTypes.begin();
			i != changedTypes.end(); ++i )
	{
		BigWorldMetaData::setEntityTypeID( transaction, i->first, i->second );
	}

	for ( StrTypeIDMap::const_iterator i = newTypes.begin();
			i != newTypes.end(); ++i )
	{
		BigWorldMetaData::addEntityType( transaction, i->first, i->second );
	}
}

// -----------------------------------------------------------------------------
// Section: Entity table manipulation functions
// -----------------------------------------------------------------------------
namespace
{
	/**
	 * 	This function changes the table name prefix.
	 */
	inline void replaceTableNamePrefix( std::string& tableName,
		const char * newPrefix )
	{
		// NOTE: Relies on fact that all table prefixes are 3 characters long.
		tableName.replace( 0, 3, newPrefix, 3 );
	}

	/**
	 * 	This function changes the prefix of the table name to "tmp".
	 */
	inline std::string getTempTableName( const std::string& tableName )
	{
		std::string tempTableName = tableName;
		replaceTableNamePrefix( tempTableName, TEMP_TABLE_NAME_PREFIX );
		return tempTableName;
	}

	/**
	 * 	This function changes the prefix of the table name to "old"
	 */
	inline std::string getOldTableName( const std::string& tableName )
	{
		std::string oldTableName = tableName;
		replaceTableNamePrefix( oldTableName, OLD_TABLE_NAME_PREFIX );
		return oldTableName;
	}

	/**
	 *	This function generates a column type string from the column type and
	 * 	default value for the column
	 */
	inline std::string getColumnTypeStr( const std::string& type,
			const std::string& defaultVal )
	{
		std::stringstream ss;
		ss << type << " DEFAULT " << defaultVal;
		return ss.str();
	}

	/**
	 * 	This function changes the prefix of the table name to "tbl"
	 */
	inline std::string getOrigTableName( const std::string& tableName )
	{
		std::string origTableName = tableName;
		replaceTableNamePrefix( origTableName, TABLE_NAME_PREFIX );
		return origTableName;
	}

	/**
	 * 	This is a utility function which will separate columns into new
	 * 	(needs to be added), old (needs to be deleted) or out of date (needs
	 * 	to be updated).
	 *
	 * 	@param	oldColumns	On input, this parameter contains the existing
	 * 	columns. On output, this parameter will contain those columns that
	 * 	require removal.
	 * 	@param	newColumns	On input, this parameter contains the desired
	 * 	columns. On output, this parameter will contain those columns that
	 * 	require addition.
	 * 	@param	updatedColumns	On input, this parameter should be empty. On
	 * 	output, this parameter contains the columns whose type needs changing.
	 */
	void classifyColumns( ITableCollector::NameToColInfoMap& oldColumns,
		ITableCollector::NameToColInfoMap& newColumns,
		ITableCollector::NameToUpdatedColInfoMap& updatedColumns )
	{
		typedef ITableCollector::NameToColInfoMap 			Columns;
		typedef ITableCollector::NameToUpdatedColInfoMap 	UpdatedColumns;

		for ( Columns::iterator oldCol = oldColumns.begin();
				oldCol != oldColumns.end(); /*++oldCol*/ )
		{
			Columns::iterator newCol = newColumns.find( oldCol->first );
			if (newCol != newColumns.end())
			{

                if(!Database::instance().updateSelfIndex() && (
                    (newCol->second.indexType == ITableCollector::IndexTypeNormal || 
                    newCol->second.indexType == ITableCollector::IndexTypeUnique  || 
                    oldCol->second.indexType == ITableCollector::IndexTypeNormal || 
                    oldCol->second.indexType == ITableCollector::IndexTypeUnique )  
                    )&&
                    oldCol->second.indexType != newCol->second.indexType)
                {
				    ERROR_MSG( "  Need add start argument -upindex to update custom index(%s) from (%d) to (%d)\n",
                            newCol->first.c_str(), oldCol->second.indexType, newCol->second.indexType );

                    newCol->second.indexType = oldCol->second.indexType;
                }

				if (newCol->second != oldCol->second)
				{
					updatedColumns.insert(
							UpdatedColumns::value_type( newCol->first,
									ITableCollector::UpdatedColumnInfo(
											newCol->second, oldCol->second )));
				}

				ITableCollector::NameToColInfoMap::iterator curOldCol = oldCol;
				++oldCol;
				oldColumns.erase( curOldCol );
				newColumns.erase( newCol );
			}
			else
			{
				++oldCol;
			}
		}
	}

	/**
	 *	This function generates an index name based on column name. The name
	 * 	of the index isn't really all that important but it's nice to have some
	 * 	consistency.
	 */
	std::string generateIndexName( const std::string& colName )
	{
		std::string::size_type underscorePos = colName.find( '_' );
		return (underscorePos == std::string::npos) ?
			colName + "Index" :
			colName.substr( underscorePos + 1 ) + "Index";
	}

	/**
	 *	This function creates an index on the given column in the given table
	 * 	according to colInfo.indexType. Most of the time this would be
	 * 	IndexTypeNone.
	 */
	void createEntityTableIndex( MySqlTransaction& transaction,
		const std::string& tblName, const std::string& colName,
		const ITableCollector::ColumnInfo& colInfo )
	{
		switch (colInfo.indexType)
		{
			case ITableCollector::IndexTypeNone:
				break;
			case ITableCollector::IndexTypePrimary:
				// A bit dodgy, but this is created when we create the table
				// and it cannot be added or deleted afterwards.
				break;
            /* huangshanquan  2009-12-28 change begin CSOL-7399*/
			case ITableCollector::IndexTypeNormal:
				{
					std::string indexName = generateIndexName( colName );

                    std::string createIndex;
                    if (colInfo.typeStr.find( "INT", 0, strlen( "INT" ))!= std::string::npos)
                    {
                        createIndex = "CREATE INDEX " + indexName + " ON " + tblName + " (" + colName + ")";

                    }
                    else
                    {
					    const char * indexLengthConstraint = "";
					    if (colInfo.typeStr.find( "VARCHAR", 0, strlen( "VARCHAR" ))
					    		== std::string::npos)
					    {	// Not VARCHAR field. Set index length to 255.
						    indexLengthConstraint = "(255)";
					    }

                        createIndex = "CREATE INDEX " + indexName + " ON " + tblName + " (" + colName + \
                                       indexLengthConstraint + ")" ;

                    }

                    transaction.execute( createIndex.c_str(), NULL, true );
				}
				break;
			case ITableCollector::IndexTypeUnique:
			case ITableCollector::IndexTypeName:
				{
					// __kyl__ (24/7/2006) Super dodgy way of working out the
					// size of the index. If it is a VARCHAR field then we use
					// the size of the field. If it is any other type of field,
					// then we make the index size 255.
					std::string indexName = generateIndexName( colName );

                    std::string createIndex;

                    if (colInfo.typeStr.find( "INT", 0, strlen( "INT" ))!= std::string::npos)
                    {
                        createIndex = "CREATE UNIQUE INDEX " + indexName + " ON " + tblName + " (" + colName + ")";

                    }
                    else
                    {
					    const char * indexLengthConstraint = "";
					    if (colInfo.typeStr.find( "VARCHAR", 0, strlen( "VARCHAR" ))
							    == std::string::npos)
					    {	// Not VARCHAR field. Set index length to 255.
						    indexLengthConstraint = "(255)";
					    }

                        createIndex = "CREATE UNIQUE INDEX " + indexName + " ON " + tblName + " (" + colName + \
                                       indexLengthConstraint + ")" ;
                    }

                    transaction.execute( createIndex.c_str(), NULL, true );
				}
				break;
            /* huangshanquan 2009-12-28 change end*/
			case ITableCollector::IndexTypeParentID:
				transaction.execute( "CREATE INDEX parentIDIndex ON " +
						tblName + " (" + colName + ")" );
				break;
			default:
				CRITICAL_MSG( "createEntityTableIndex: Unknown index type %d\n",
						colInfo.indexType );
				break;
		}
	}

	/**
	 *	This function deletes an index in the given table according to
	 * 	indexType. This is the evil twin of createEntityTableIndex().
	 */
	void removeEntityTableIndex( MySqlTransaction& transaction,
		const std::string& tblName, const std::string& colName,
		ITableCollector::IndexType indexType)
	{

		try
		{
			switch (indexType)
			{
				case ITableCollector::IndexTypeNone:
					break;
				case ITableCollector::IndexTypePrimary:
					// Can't delete primary index.
					break; 

                /* huangshanquan  2009-12-28 change CSOL-7399*/
			    case ITableCollector::IndexTypeNormal:
			    case ITableCollector::IndexTypeUnique:
				case ITableCollector::IndexTypeName:
					{
						std::string indexName = generateIndexName( colName );

						transaction.execute( "ALTER TABLE " + tblName +
								" DROP INDEX " + indexName );
					}
					break;
                /* huangshanquan 2009-12-28 change end*/
				case ITableCollector::IndexTypeParentID:
					transaction.execute( "ALTER TABLE " + tblName +
							" DROP INDEX parentIDIndex" );
					break;
				default:
					CRITICAL_MSG( "removeEntityTableIndex: Unknown index type "
							"%d\n", indexType );
					break;
			}
		}
		catch (std::exception& e)
		{
			// Shouldn't really happen, but it's not fatal so we shouldn't
			// die.
			ERROR_MSG( "removeEntityTableIndex: %s\n", e.what() );
		}
	}

	/**
	 *	This function sets the value of a string column for all rows in the table.
	 */
	void setColumnValue( MySql& con, const std::string& tableName,
			const std::string& columnName, const std::string& columnValue )
	{
		std::stringstream ss;
		ss << "UPDATE " << tableName << " SET " << columnName
				<< "='" << MySqlEscapedString( con, columnValue ) << '\'';
		con.execute( ss.str() );
	}

	class TableInitialiser : public ITableCollector, private MySqlTransaction
	{
	public:
		TableInitialiser( MySql& con, DefsSyncMode defsSyncMode );
		virtual ~TableInitialiser();

		virtual void requireTable( const std::string& name,
			const NameToColInfoMap& columns );
		void addType( EntityTypeID, const std::string& );
		void garbageCollect( MySql& con );
		void commit() { MySqlTransaction::commit(); }

		bool isSynced() const
		{	return maxRequiredSyncMode_ == DefsSyncModeNoSync;	}

	private:
		// Useful functor class for printing out messages.
		class PrintMsg
		{
			const std::string&	msgTemplate_;
		public:
			PrintMsg( const std::string& msgTemplate ) :
				msgTemplate_(msgTemplate)
			{}
			void operator()( const std::string& substitute )
			{
				INFO_MSG( msgTemplate_.c_str(), substitute.c_str() );
			}
			void operator()( const std::pair< const std::string, std::string >& substitute )
			{
				INFO_MSG( msgTemplate_.c_str(), substitute.first.c_str(),
						substitute.second.c_str() );
			}
			void operator()( const ITableCollector::NameToColInfoMap::value_type& substitute )
			{
				INFO_MSG( msgTemplate_.c_str(), substitute.first.c_str(),
						substitute.second.typeStr.c_str() );
			}
			void operator()( const ITableCollector::NameToUpdatedColInfoMap::value_type& substitute )
			{
				INFO_MSG( msgTemplate_.c_str(), substitute.first.c_str(),
						substitute.second.typeStr.c_str(),
						(substitute.second.indexType ==
							ITableCollector::IndexTypeNone) ?
								"non-indexed" : "indexed" );
			}
		};

		void createNameIndex( const std::string& tblName,
			const std::string& colName, const std::string& typeStr );
		void removeNameIndex( const std::string& tblName );

		static MySqlTransaction& initTablesForBigWorldMetaData(
			MySqlTransaction& transaction );

		bool	allowAdd_;
		bool	allowUpdate_;
		bool	allowDelete_;
		DefsSyncMode maxRequiredSyncMode_;

		StrSet tables_;
		StrSet types_;

		BigWorldMetaData	metaData_;
	};

	/**
	 * 	This function is used to call initSpecialBigWorldTables() from the
	 * 	initialiser list.
	 */
	MySqlTransaction& TableInitialiser::initTablesForBigWorldMetaData(
		MySqlTransaction& transaction )
	{
		initSpecialBigWorldTables( transaction );
		return transaction;
	}

	TableInitialiser::TableInitialiser( MySql& con, DefsSyncMode defsSyncMode )
		: MySqlTransaction( con ),
		allowAdd_( defsSyncMode >= DefsSyncModeAddOnly ),
		allowUpdate_( defsSyncMode >= DefsSyncModeAddAndUpdate ),
		allowDelete_( defsSyncMode >= DefsSyncModeFullSync ),
		maxRequiredSyncMode_(DefsSyncModeNoSync),
		metaData_( initTablesForBigWorldMetaData( *this ) )
	{
		INFO_MSG( "\tsyncTablesToDefs = %s\n",
					(defsSyncMode > DefsSyncModeNoSync) ? "True" : "False" );
	}

	TableInitialiser::~TableInitialiser()
	{
		if (!this->isSynced())
		{
			ERROR_MSG( "Please run dbmgr with the -syncTablesToDefs option to "
						"update tables\n" );
		}
	}

	// ensures that each required table is only listed once,
	// and does the minimum work required to make the table in the database
	// match what we require from the .def files
	void TableInitialiser::requireTable( const std::string& name,
		const NameToColInfoMap& columns )
	{
		MF_ASSERT( columns.size() > 0 );	// Can't have tables with no columns

		bool inserted = tables_.insert( name ).second;
		if (!inserted)
			throw std::runtime_error( "table " + name + " requested twice" );

		bool createdTable = false;
		bool tableExists = true;
		bool deleteIDCol = false;

		NameToColInfoMap newColumns(columns);
		NameToColInfoMap oldColumns;
		metaData_.getTableColumns( oldColumns, name );
		NameToUpdatedColInfoMap updatedColumns;

		// if there was nothing listed in our meta-data table about this table
		// then we probably need to create it - we just create a table with an id
		// column for the moment, and add anything else below (as everything in
		// "columns" will be an addition)
		if (oldColumns.size() == 0)
		{
			if (allowAdd_)
			{
				INFO_MSG("\tCreating table %s\n", name.c_str());
				execute( "CREATE TABLE IF NOT EXISTS " + name +
						" (id BIGINT AUTO_INCREMENT, PRIMARY KEY idKey (id)) TYPE=InnoDB" );
				// __kyl__ (28/7/2005) We can't create a table with no columns so
				// we create one with the id column even though it may not be
				// needed. We'll delete the id column later on.
				createdTable = true;
				NameToColInfoMap::iterator idIter =
						newColumns.find( ID_COLUMN_NAME );
				if (idIter != newColumns.end())
				{
					metaData_.addColumn( name, idIter->first, idIter->second );
					newColumns.erase( idIter );
				}
				else
				{
					deleteIDCol = true;
				}
			}
			else
			{
				INFO_MSG("\tRequire table %s\n", name.c_str());
				maxRequiredSyncMode_ =
					std::max( maxRequiredSyncMode_, DefsSyncModeAddOnly );
				tableExists = false;
			}
		}
		// otherwise go through all the columns, and classify them as an addition,
		// a deletion, or a modification
		else
		{
			classifyColumns( oldColumns, newColumns, updatedColumns );
		}

		std::stringstream alterStream;
		uint numOps = 0;

		// add new columns that we've not heard of before
		if (allowAdd_)
		{
			for ( NameToColInfoMap::const_iterator i = newColumns.begin();
				  i != newColumns.end(); ++i )
			{
				if (!createdTable)
					INFO_MSG( "\tAdding column %s into table %s\n",
								i->first.c_str(), name.c_str() );
				metaData_.addColumn( name, i->first, i->second );

                if(numOps != 0)
				    alterStream << ", ADD COLUMN " << i->first << " " << i->second.typeStr;
                else
				    alterStream << " ADD COLUMN " << i->first << " " << i->second.typeStr;

				++numOps;
			}
		}
		else if (newColumns.size() > 0)
		{
			if (tableExists)
				for_each( newColumns.begin(), newColumns.end(),
						PrintMsg( "\tNeed to add column %s into table " + name + "\n" ) );
			maxRequiredSyncMode_ =
				std::max( maxRequiredSyncMode_, DefsSyncModeAddOnly );
		}

		// delete unnecessary ID column that we created table with.
		// NOTE: This is handled a differently to other columns because
		//		- we need to delete it even when allowDelete_ is false.
		//		- It hasn't been added into bigworldTableMetadata yet.
		//		- We don't want to print INFO message of our internal workings.
		if (deleteIDCol)
		{
            if(numOps != 0)
			    alterStream << ", DROP COLUMN " << ID_COLUMN_NAME;
            else
			    alterStream << " DROP COLUMN " << ID_COLUMN_NAME;

			++numOps;
		}

		// delete unused columns
		if (allowDelete_)
		{
			for ( NameToColInfoMap::const_iterator i = oldColumns.begin();
				  i != oldColumns.end(); ++i )
			{
				INFO_MSG( "\tDeleting column %s from table %s\n",
							i->first.c_str(), name.c_str() );
				removeEntityTableIndex( *this, name, i->first,
						i->second.indexType );

                if(numOps != 0)
				    alterStream << ", DROP COLUMN " << i->first;
                else
				    alterStream << " DROP COLUMN " << i->first;

				metaData_.removeColumn( name, i->first );
				++numOps;
			}
		}
		else if (oldColumns.size() > 0)
		{
			for_each( oldColumns.begin(), oldColumns.end(),
					PrintMsg( "\tNeed to delete column %s from table " + name + "\n" ) );
			maxRequiredSyncMode_ =
				std::max( maxRequiredSyncMode_, DefsSyncModeFullSync );
		}

		// update columns where the type has changed
		if (allowUpdate_)
		{
			for ( NameToUpdatedColInfoMap::iterator i =
					updatedColumns.begin(); i != updatedColumns.end(); ++i )
			{
				INFO_MSG( "\tUpdating type of column %s in table %s to %s "
						"(%sindexed) \n",
						i->first.c_str(), name.c_str(),
						i->second.typeStr.c_str(),
                        (i->second.indexType == IndexTypeNone) ? "non-": "" );


				removeEntityTableIndex( *this, name, i->first,
					    i->second.oldIndexType );
				metaData_.setColumnType( name, i->first, i->second );

                if(numOps != 0)
                    alterStream << ", MODIFY COLUMN " << i->first << " "
                        << i->second.typeStr;
                else
                    alterStream << " MODIFY COLUMN " << i->first << " "
                        << i->second.typeStr;

				++numOps;
			}
		}
		else if (updatedColumns.size() > 0)
		{
			for_each( updatedColumns.begin(), updatedColumns.end(),
					PrintMsg( "\tNeed to update column %s in table " + name +
						" to type %s (%s)\n" ) );
			maxRequiredSyncMode_ =
				std::max( maxRequiredSyncMode_, DefsSyncModeAddAndUpdate );
		}

		if (numOps)
		{
			execute( "ALTER TABLE " + name + alterStream.str() );

			// Re-create any indexes that may be needed
			// New column indexes
			for ( NameToColInfoMap::const_iterator i = newColumns.begin();
				  i != newColumns.end(); ++i )
			{
				createEntityTableIndex( *this, name, i->first, i->second );
				// Manually set the default value of this column if necessary.

				if (i->second.shouldSetDefault)
				{
					setColumnValue( this->get(), name, i->first,
							i->second.defaultVal );
				}
			}

			// Updated column indexes
			for ( NameToUpdatedColInfoMap::const_iterator i =
					updatedColumns.begin(); i != updatedColumns.end(); ++i )
			{
				createEntityTableIndex( *this, name, i->first, i->second );
			}
		}
	}

	void TableInitialiser::addType( EntityTypeID bigworldID, const std::string& name )
	{
		bool inserted = types_.insert( name ).second;
		if (!inserted)
			throw std::runtime_error( "type " + name + " requested twice" );

		EntityTypeID typeID = metaData_.getEntityTypeID( name );
		if ( typeID == INVALID_TYPEID )
			metaData_.addEntityType( name, bigworldID );
		else if ( typeID != bigworldID )
			metaData_.setEntityTypeID( name, bigworldID );
	}

	void TableInitialiser::garbageCollect( MySql& con )
	{
		StrSet dead;
		{
			StrSet oldTables;
			metaData_.getTables( oldTables );
			std::set_difference( oldTables.begin(), oldTables.end(),
						tables_.begin(), tables_.end(),
						std::insert_iterator<StrSet>( dead, dead.begin() ) );
		}

		if (allowDelete_)
		{
			for ( StrSet::const_iterator i = dead.begin(); i != dead.end(); ++i )
			{
				INFO_MSG( "\tDeleting table %s\n", i->c_str() );
				metaData_.removeAllColumns( *i );
				execute( "DROP TABLE " + *i );
			}

			std::string cleanupTypes = "DELETE FROM bigworldEntityTypes WHERE 1=1";
			for ( StrSet::const_iterator i = types_.begin(); i != types_.end(); ++i )
			{
				cleanupTypes += " AND name != '" + *i + "'";
			}
			execute( cleanupTypes );
		}
		else if (dead.size() > 0)
		{
			std::for_each( dead.begin(), dead.end(),
							PrintMsg( "Should remove table %s\n" ) );
			maxRequiredSyncMode_ =
				std::max( maxRequiredSyncMode_, DefsSyncModeFullSync );
		}

		// TODO: delete entities with types not in bigworldEntityTypes
	}

}

// -----------------------------------------------------------------------------
// Section: class TableCollector
// -----------------------------------------------------------------------------
/**
 * 	Constructor.
 */
TableCollector::TableCollector( TableMigrationData& data,
		BigWorldMetaData& metaData ) :
	data_(data), metaData_( metaData )
{
	data_.init();
	metaData_.getTables( data_.obsoleteTables );
	metaData_.getEntityTypes( data_.obsoleteTypes );
}

/**
 * 	Destructor.
 */
TableCollector::~TableCollector()
{
	if (data_.obsoleteTables.size() > 0)
	{
		data_.maxRequiredSyncMode = std::max( data_.maxRequiredSyncMode,
											DefsSyncModeFullSync );
	}
}

/**
 * 	ITableCollector override. We use this to keep track of what tables
 * 	are needed.
 */
void TableCollector::requireTable( const std::string& name,
	const NameToColInfoMap& columns )
{
	MF_ASSERT( columns.size() > 0 );	// Can't have tables with no columns
	std::string	origTableName = getOrigTableName( name );
	if (data_.tables.insert( origTableName ).second)
	{	// Not a duplicate request.
		data_.obsoleteTables.erase( origTableName );

		NameToColInfoMap oldColumns;
		// Just in case table name has non-original prefix e.g. "tmp_"
		metaData_.getTableColumns( oldColumns, origTableName );

		if (oldColumns.size() == 0)
		{	// New table
			TableMigrationData::NewTableData& tableData =
				data_.newTables[origTableName];
			tableData.columns = columns;

			data_.maxRequiredSyncMode = std::max( data_.maxRequiredSyncMode,
										DefsSyncModeAddOnly );
		}
		else
		{	// Existing table. Do a diff of the tables.
			NameToColInfoMap 		newColumns( columns );
			NameToUpdatedColInfoMap	updatedColumns;

			classifyColumns( oldColumns, newColumns, updatedColumns );

			if ( (oldColumns.size() > 0) || (newColumns.size() > 0) ||
				(updatedColumns.size() > 0) )
			{	// Table has changed.
				TableMigrationData::UpdatedTableData& tableData =
					data_.updatedTables[origTableName];
				tableData.columns = columns;
				tableData.obsoleteColumns = oldColumns;
				tableData.newColumns = newColumns;
				tableData.updatedColumns = updatedColumns;

				DefsSyncMode requiredSyncMode = (oldColumns.size() > 0) ?
										DefsSyncModeFullSync :
										DefsSyncModeAddAndUpdate;
				data_.maxRequiredSyncMode =
					std::max( data_.maxRequiredSyncMode, requiredSyncMode );
			}
		}
	}
	else
	{
		ERROR_MSG( "TableCollector::requireTable: Multiple "
					"additions of table %s\n", name.c_str() );
	}
}

/**
 * 	This function is used to keep track for what types are needed.
 */
void TableCollector::addType( EntityTypeID bigworldID,
	const std::string& name )
{
	if (data_.types.insert( name ).second)
	{
		StrTypeIDMap::iterator pType = data_.obsoleteTypes.find( name );
		if (pType == data_.obsoleteTypes.end())
		{
			data_.newTypes[name] = bigworldID;
		}
		else
		{
			if (pType->second != bigworldID)
				data_.updatedTypes[name] = bigworldID;
			data_.obsoleteTypes.erase( pType );
		}
	}
	else
	{
		ERROR_MSG( "TableCollector::addType: Multiple "
					"additions of type %s\n", name.c_str() );
	}
}

// -----------------------------------------------------------------------------
// Section: class SimpleTableCollector
// -----------------------------------------------------------------------------
/**
 * 	ITableCollector override.
 */
void SimpleTableCollector::requireTable( const std::string& name,
	const NameToColInfoMap& columns )
{
	std::string origTableName = getOrigTableName( name );
	TableMigrationData::NewTableData& tableData = tables_[origTableName];
	tableData.columns = columns;
}

/**
 * 	Adds the tables from the other SimpleTableCollector into this one.
 */
SimpleTableCollector& SimpleTableCollector::operator+=(
	const SimpleTableCollector& rhs )
{
	for ( TableMigrationData::NewTableDataMap::const_iterator i =
			rhs.tables_.begin(); i != rhs.tables_.end(); ++i )
	{
		tables_.insert( *i );
	}
	return *this;
}

// -----------------------------------------------------------------------------
// Section: Entity table manipulation functions
// -----------------------------------------------------------------------------
/**
 * 	This function creates an entity data table (or its sub-tables).
 */
void createEntityTable( MySqlTransaction& transaction,
	const std::string& tableName,
	const ITableCollector::NameToColInfoMap& columns )
{
	std::stringstream stmtStrm;
	stmtStrm << "CREATE TABLE " << tableName << " (";
	ITableCollector::NameToColInfoMap::const_iterator i = columns.begin();
	MF_ASSERT( i != columns.end() );
	stmtStrm << i->first << " " << i->second.typeStr;
	++i;
	while (i != columns.end())
	{
		stmtStrm << ", " << i->first << " " << i->second.typeStr;
		++i;
	}
	stmtStrm << ") TYPE=InnoDB";
	transaction.execute( stmtStrm.str() );

	// Create indexes
	for ( ITableCollector::NameToColInfoMap::const_iterator i = columns.begin();
			i != columns.end(); ++i )
	{
		createEntityTableIndex( transaction, tableName, i->first, i->second );
	}
}

/**
 * 	This function creates entity data tables.
 */
void createEntityTables( MySqlTransaction& transaction,
	const TableMigrationData::NewTableDataMap& tables )
{
	for ( TableMigrationData::NewTableDataMap::const_iterator i = tables.begin();
			i != tables.end(); ++i )
	{
		createEntityTable( transaction, i->first, i->second.columns );
		BigWorldMetaData::addColumns( transaction, i->first,
				i->second.columns );
	}
}

/**
 * 	This function removes entity data tables.
 */
void removeEntityTables( MySqlTransaction& transaction,
	const TableMigrationData::NewTableDataMap& tables )
{
	for ( TableMigrationData::NewTableDataMap::const_iterator i = tables.begin();
			i != tables.end(); ++i )
	{
		transaction.execute( "DROP TABLE " + i->first );
		BigWorldMetaData::removeAllColumns( transaction, i->first );
	}
}

/**
 * 	This function removes entity data tables.
 */
void removeEntityTables( MySqlTransaction& transaction, const StrSet& tables )
{
	for ( StrSet::const_iterator i = tables.begin();
			i != tables.end(); ++i )
	{
		transaction.execute( "DROP TABLE " + *i );
		BigWorldMetaData::removeAllColumns( transaction, *i );
	}
}

/**
 * 	This function creates the temporary entity tables (actually, just a
 *  normal entity table with a special name).
 */
void createTempEntityTables( MySqlTransaction& transaction,
	const TableMigrationData::NewTableDataMap& tables )
{
	for ( TableMigrationData::NewTableDataMap::const_iterator i = tables.begin();
			i != tables.end(); ++i )
	{
		std::string tempTableName = getTempTableName( i->first );
		createEntityTable( transaction, tempTableName, i->second.columns );
	}
}

/**
 * 	This function deletes the temporary entity tables - used for clean-up
 * 	purposes.
 */
void removeTempEntityTables( MySqlTransaction& transaction,
	const TableMigrationData::NewTableDataMap& tables )
{
	for ( TableMigrationData::NewTableDataMap::const_iterator i = tables.begin();
			i != tables.end(); ++i )
	{
		transaction.execute( "DROP TABLE " + getTempTableName( i->first ) );
	}
}

/**
 * 	This function changes to names of the migration tables to real tables
 *  and real tables to "old" tables.
 *
 * 	@param	transaction		The transaction to use to update the DB.
 * 	@param	tempTables		Tables that are part of the migration table set.
 * 	These will be renamed from "tmp" to to real tables. The real tables will
 * 	be renamed to "old".
 * 	@param	updatedTables	These tables have been changed. These are a subset
 * 	of tempTables. Used to update meta data.
 * 	@param	newTables		These tables are brand new. They will be renamed
 * 	from "tmp" to real tables.
 * 	@param	obsoleteTables	These tables are obsolete. They will be renamed
 * 	to "old".
 */
void swapTempTablesWithReal( MySqlTransaction& transaction,
	const TableMigrationData::NewTableDataMap& tempTables,
	const TableMigrationData::UpdatedTableDataMap& updatedTables,
	const TableMigrationData::NewTableDataMap& newTables,
	const StrSet& obsoleteTables )
{
	std::stringstream stmtStrm;
	stmtStrm << "RENAME TABLE ";
	bool hasItems = false;
	// Rename temp tables to real tables and real tables to "old" tables.
	for ( TableMigrationData::NewTableDataMap::const_iterator i =
			tempTables.begin(); i != tempTables.end(); ++i )
	{
		if (hasItems)
			stmtStrm << ", ";
		hasItems = true;
		// Only need to rename original table if this is not new table
		if ( newTables.find( i->first ) == newTables.end() )
		{
			stmtStrm << i->first << " TO ";
			stmtStrm <<	getOldTableName( i->first ) << ", ";
		}
		stmtStrm << getTempTableName( i->first ) << " TO ";
		stmtStrm << i->first;
	}

	// Rename obsolete tables to "old" tables.
	for ( StrSet::const_iterator i = obsoleteTables.begin();
			i != obsoleteTables.end(); ++i )
	{
		if (hasItems)
			stmtStrm << ", ";
		hasItems = true;

		stmtStrm << *i << " TO ";
		stmtStrm <<	getOldTableName( *i );
	}

	// Rename new tables to real tables
	for ( TableMigrationData::NewTableDataMap::const_iterator i =
			newTables.begin(); i != newTables.end(); ++i )
	{
		// Only need to rename new table if it is not part of the migration
		// set. Otherwise, it would've been renamed above.
		if ( tempTables.find( i->first ) == tempTables.end() )
		{
			if (hasItems)
				stmtStrm << ", ";
			hasItems = true;

			stmtStrm << getTempTableName( i->first ) << " TO ";
			stmtStrm <<	i->first;
		}
	}

	// Do huge rename!
	if (hasItems)
		transaction.execute( stmtStrm.str() );

	// Fix-up meta data now that we've swapped the temp tables into real tables
	// Remove meta data for obsolete tables;
	for ( StrSet::const_iterator i = obsoleteTables.begin();
			i != obsoleteTables.end(); ++i )
	{
		BigWorldMetaData::removeAllColumns( transaction, *i );
	}
	// Update meta data for updated tables.
	for ( TableMigrationData::UpdatedTableDataMap::const_iterator i =
			updatedTables.begin(); i != updatedTables.end(); ++i )
	{
		for ( ITableCollector::NameToColInfoMap::const_iterator j =
				i->second.obsoleteColumns.begin();
				j != i->second.obsoleteColumns.end(); ++j )
		{
			BigWorldMetaData::removeColumn( transaction, i->first, j->first );
		}
		for ( ITableCollector::NameToUpdatedColInfoMap::const_iterator j =
				i->second.updatedColumns.begin();
				j != i->second.updatedColumns.end(); ++j )
		{
			BigWorldMetaData::setColumnType( transaction, i->first, j->first, j->second );
		}
		for ( ITableCollector::NameToColInfoMap::const_iterator j =
				i->second.newColumns.begin();
				j != i->second.newColumns.end(); ++j )
		{
			BigWorldMetaData::addColumn( transaction, i->first, j->first, j->second );
		}
	}
	// Add meta data for new tables
	for ( TableMigrationData::NewTableDataMap::const_iterator i =
			newTables.begin(); i != newTables.end(); ++i )
	{
		BigWorldMetaData::addColumns( transaction, i->first, i->second.columns );
	}
}

/**
 * 	This method removes the "old" tables that were the result of
 * 	swapTempTablesWithReal().
 */
void removeSwappedOutTables( MySqlTransaction& transaction,
	const TableMigrationData::NewTableDataMap& tempTables,
	const TableMigrationData::NewTableDataMap& newTables,
	const StrSet& obsoleteTables )
{
	// Remove swapped out temp tables.
	for ( TableMigrationData::NewTableDataMap::const_iterator i = tempTables.begin();
			i != tempTables.end(); ++i )
	{
		if ( newTables.find( i->first ) == newTables.end() )
		{
			transaction.execute( "DROP TABLE " + getOldTableName( i->first ) );
		}
	}
	// Remove obsolete tables.
	for ( StrSet::const_iterator i = obsoleteTables.begin();
			i != obsoleteTables.end(); ++i )
	{
		transaction.execute( "DROP TABLE " + getOldTableName( *i ) );
	}
}

// -----------------------------------------------------------------------------
// Section: property mappings
// -----------------------------------------------------------------------------
namespace
{
	/**
	 *	This method gets the default value section for the child type based on
	 * 	the parent type's default value section. If it cannot find the section,
	 * 	then it uses the child's default section.
	 */
	DataSectionPtr getChildDefaultSection( const DataType& childType,
			const std::string childName, DataSectionPtr pParentDefault )
	{
		DataSectionPtr pChildDefault = (pParentDefault) ?
				pParentDefault->openSection( childName ) :
				DataSectionPtr( NULL );
		if (!pChildDefault)
		{
			pChildDefault = childType.pDefaultSection();
		}

		return pChildDefault;
	}

	/**
	 * 	This method gets the default value section for the DataDescription.
	 */
	DataSectionPtr getDefaultSection( const DataDescription& dataDesc )
	{
		DataSectionPtr pDefaultSection = dataDesc.pDefaultSection();
		if (!pDefaultSection)
		{
			pDefaultSection = dataDesc.dataType()->pDefaultSection();
		}
		return pDefaultSection;
	}

	/**
	 *	This class helps to build names for table columns. Introduced when
	 *	due to nested properties. It tries to achieve the following:
	 *		- Table names are fully qualified.
	 *		- Column names are relative to the current table.
	 */
	class Namer
	{
		typedef std::vector< std::string >			Strings;
		typedef std::vector< Strings::size_type >	Levels;

		std::string tableNamePrefix_;
		Strings		names_;
		Levels		tableLevels_;

	public:
		Namer( const std::string& entityName,
				const std::string& tableNamePrefix ) :
			tableNamePrefix_( tableNamePrefix ), names_( 1, entityName ),
			tableLevels_( 1, 1 )
		{}

		Namer( const Namer& existing, const std::string& propName, bool isTable ) :
			tableNamePrefix_( existing.tableNamePrefix_ ),
			names_( existing.names_ ), tableLevels_( existing.tableLevels_ )
		{
			if ( propName.empty() )
				names_.push_back( isTable ? DEFAULT_SEQUENCE_TABLE_NAME :
									DEFAULT_SEQUENCE_COLUMN_NAME  );
			else
				names_.push_back( propName );
			if (isTable)
				tableLevels_.push_back( names_.size() );
		}

		std::string buildColumnName( const std::string& prefix,
									const std::string& propName ) const
		{
			std::string suffix =
				(propName.empty()) ? DEFAULT_SEQUENCE_COLUMN_NAME : propName;
			// TRACE_MSG( "buildColumnName: %s\n", this->buildName( prefix, suffix, tableLevels_.back() ).c_str() );
			return this->buildName( prefix, suffix, tableLevels_.back() );
		}

		std::string buildTableName( const std::string& propName ) const
		{
			std::string suffix =
				(propName.empty()) ? DEFAULT_SEQUENCE_TABLE_NAME : propName;
			// TRACE_MSG( "buildTableName: %s\n", this->buildName( tableNamePrefix_, suffix, tableLevels_.back() ).c_str() );
			return this->buildName( tableNamePrefix_, suffix, 0 );
		}

	private:
		std::string buildName( const std::string& prefix,
							const std::string& suffix,
							Strings::size_type startIdx ) const
		{
			std::string name = prefix;
			for ( Strings::size_type i = startIdx; i < names_.size(); ++i )
			{
				name += '_';
				name += names_[i];
			}
			if (!suffix.empty())
			{
				name += '_';
				name += suffix;
			}
			return name;
		}
	};

	std::string buildCommaSeparatedQuestionMarks( int num );

	// for UserData, we want a single property mapping to support (possible)
	// lots and lots of different properties; the CompositePropertyMapping
	// handles this (UserData types are the only users of this class... 'tho
	// really the whole type should be encapsualted in one of these)
	class CompositePropertyMapping : public PropertyMapping
	{
	public:
		CompositePropertyMapping( const std::string& propName ) :
			PropertyMapping(0, propName)
		{}

		void addChild( PropertyMappingPtr child )
		{
			if (!child)
			{
				ERROR_MSG( "CompositePropertyMapping::addChild: "
						"child is null (ignoring)\n" );
				return;
			}
			children_.push_back( child );
			this->addColumns( child->numColumns() );
		}

		PropertyMappingPtr::ConstProxy getChild( int idx ) const
		{
			return children_[idx];
		}

		PropertyMappingPtr getChild( int idx )
		{
			return children_[idx];
		}

		int getNumChildren() const
		{
			return int(children_.size());
		}

		virtual void prepareSQL( MySql& con )
		{
			for (Children::iterator ppChild = children_.begin();
					ppChild != children_.end(); ++ppChild)
			{
				(**ppChild).prepareSQL( con );
			}
		}

		virtual void createTables( ITableCollector& ti,
				ITableCollector::NameToColInfoMap& cols )
		{
			for (Children::iterator ppChild = children_.begin();
					ppChild != children_.end(); ++ppChild)
			{
				(**ppChild).createTables( ti, cols );
			}
		}

		virtual std::string getColumnNames( const std::string& suffix )
		{
			std::string out;
			for (Children::iterator ppChild = children_.begin();
					ppChild != children_.end(); ++ppChild)
			{
				std::string child = (**ppChild).getColumnNames(suffix);
				if (child.length())
				{
					if (out.length()) out += ',';
					out += child;
				}
			}
			return out;
		}

		virtual void addToBindings( MySqlBindings& bindings )
		{
			for (Children::iterator ppChild = children_.begin();
					ppChild != children_.end(); ++ppChild)
			{
				(**ppChild).addToBindings( bindings );
			}
		}

		virtual void streamToBound( BinaryIStream& strm )
		{
			// Assume that data is stored on the stream in the same order as
			// the bindings are created in PyUserTypeBinder::bind().
			for (Children::iterator ppChild = children_.begin();
					ppChild != children_.end(); ++ppChild)
			{
				(**ppChild).streamToBound( strm );
			}
		}

		virtual void boundToStream( BinaryOStream & strm ) const
		{
			// Assume that data is stored on the stream in the same order as
			// the bindings are created in PyUserTypeBinder::bind().
			for (Children::const_iterator ppChild = children_.begin();
					ppChild != children_.end(); ++ppChild)
			{
				(**ppChild).boundToStream( strm );
			}
		}

		virtual void defaultToStream( BinaryOStream & strm ) const
		{
			// Assume that data is stored on the stream in the same order as
			// the bindings are created in PyUserTypeBinder::bind().
			for (Children::const_iterator ppChild = children_.begin();
					ppChild != children_.end(); ++ppChild)
			{
				(**ppChild).defaultToStream( strm );
			}
		}

		virtual void nullifyBound()
		{
			for (Children::const_iterator ppChild = children_.begin();
					ppChild != children_.end(); ++ppChild)
			{
				(**ppChild).nullifyBound();
			}
		}

		virtual bool hasTable() const
		{
			for (Children::const_iterator ppChild = children_.begin();
					ppChild != children_.end(); ++ppChild)
			{
				if ((*ppChild)->hasTable())
					return true;
			}
			return false;
		}

		virtual void updateTable( MySqlTransaction& transaction,
			DatabaseID parentID )
		{
			for (Children::iterator ppChild = children_.begin();
					ppChild != children_.end(); ++ppChild)
			{
				(**ppChild).updateTable( transaction, parentID );
			}
		}

		virtual void getTableData( MySqlTransaction& transaction,
			DatabaseID parentID )
		{
			for (Children::iterator ppChild = children_.begin();
					ppChild != children_.end(); ++ppChild)
			{
				(**ppChild).getTableData( transaction, parentID );
			}
		}

		virtual void deleteChildren( MySqlTransaction& t, DatabaseID databaseID )
		{
			for (Children::iterator ppChild = children_.begin();
					ppChild != children_.end(); ++ppChild)
			{
				(**ppChild).deleteChildren( t, databaseID );
			}
		}

		virtual PyObject* createPyObject() const
		{
			PyObject* pDict = PyDict_New();
			for (Children::const_iterator ppChild = children_.begin();
					ppChild != children_.end(); ++ppChild)
			{
				PyObjectPtr pValue( (*ppChild)->createPyObject(),
									PyObjectPtr::STEAL_REFERENCE );
				PyDict_SetItemString( pDict,
					const_cast<char*>((*ppChild)->propName().c_str()),
					pValue.getObject() );
			}
			return pDict;
		}

		virtual void setValue( PyObject* pyObject )
		{
			if (PyDict_Check( pyObject ))
			{
				for (Children::iterator ppChild = children_.begin();
						ppChild != children_.end(); ++ppChild)
				{
					PyObject* pValue = PyDict_GetItemString( pyObject,
							const_cast<char*>((*ppChild)->propName().c_str()) );

					if (pValue)
						(*ppChild)->setValue( pValue );
					else
						(*ppChild)->nullifyBound();
				}
			}
			else
			{
				this->nullifyBound();
			}
		}

	protected:
		typedef std::vector<PropertyMappingPtr> Children;

		class SequenceBuffer : public PropertyMapping::ISequenceBuffer
		{
		protected:
			typedef std::vector< PropertyMapping::ISequenceBuffer* > SequenceBuffers;

			auto_container< SequenceBuffers > childBuffers_;

		public:
			SequenceBuffer( const CompositePropertyMapping::Children& children )
			{
				childBuffers_.container.reserve( children.size() );
				for ( CompositePropertyMapping::Children::const_iterator i =
					  children.begin(); i < children.end(); ++i )
				{
					childBuffers_.container.push_back(
						(*i)->createSequenceBuffer() );
				}
			}

			virtual void streamToBuffer( int numElems, BinaryIStream& strm )
			{
				for ( int i = 0; i < numElems; ++i )
				{
					for ( SequenceBuffers::iterator j =
						  childBuffers_.container.begin();
						  j < childBuffers_.container.end(); ++j )
					{
						(*j)->streamToBuffer( 1, strm );
					}
				}
			}

			virtual void bufferToStream( BinaryOStream& strm, int idx ) const
			{
				for ( SequenceBuffers::const_iterator j =
						childBuffers_.container.begin();
						j < childBuffers_.container.end(); ++j )
				{
					(*j)->bufferToStream( strm, idx );
				}
			}

			virtual void bufferToBound( PropertyMapping& binding,
				                        int idx ) const
			{
				CompositePropertyMapping& compositeProp =
					static_cast<CompositePropertyMapping&>(binding);

				int i = 0;
				for ( SequenceBuffers::const_iterator childBuf =
					  childBuffers_.container.begin();
					  childBuf < childBuffers_.container.end();
				      ++i, ++childBuf )
				{
					(*childBuf)->bufferToBound( *compositeProp.getChild(i),
						                        idx );
				}
			}

			virtual void boundToBuffer( const PropertyMapping& binding )
			{
				const CompositePropertyMapping& compositeProp =
					static_cast<const CompositePropertyMapping&>(binding);

				int i = 0;
				for ( SequenceBuffers::const_iterator childBuf =
					  childBuffers_.container.begin();
					  childBuf < childBuffers_.container.end();
				      ++i, ++childBuf )
				{
					(*childBuf)->boundToBuffer( *compositeProp.getChild(i) );
				}
			}
			
			virtual int	getNumElems() const
			{
				return (childBuffers_.container.size()) ?
					childBuffers_.container[0]->getNumElems() : 0;
			}

			virtual void reset()
			{
				for ( SequenceBuffers::iterator i =
					  childBuffers_.container.begin();
					  i < childBuffers_.container.end(); ++i )
				{
					(*i)->reset();
				}
			}
		};

	public:
		virtual ISequenceBuffer* createSequenceBuffer() const
		{	return new SequenceBuffer( children_ );	}

	protected:
		Children children_;
	};

	typedef SmartPointer<CompositePropertyMapping> CompositePropertyMappingPtr;
	// UserTypeMapping maps USER_TYPE in MySQL. It's a CompositePropertyMapping
	// with special handling for serialisation.
	class UserTypeMapping : public CompositePropertyMapping
	{
	public:
		UserTypeMapping( const std::string& propName )
			: CompositePropertyMapping( propName )
		{}

		virtual void streamToBound( BinaryIStream& strm )
		{
			std::string encStr;
			strm >> encStr;
			MemoryIStream encStrm( const_cast<char*>(encStr.c_str()),
				encStr.length() );
			CompositePropertyMapping::streamToBound( encStrm );
		}

		virtual void boundToStream( BinaryOStream & strm ) const
		{
			MemoryOStream encStrm;
			CompositePropertyMapping::boundToStream( encStrm );
			// __kyl__ (18/5/2005) We are assuming that this is how a
			// std::string gets serialised.
			strm.appendString( reinterpret_cast<const char *>(encStrm.data()),
				encStrm.size() );
		}

	private:
		// Override CompositePropertyMapping::SequenceBuffer stream methods.
		class UserPropSequenceBuffer : public SequenceBuffer
		{
		public:
			UserPropSequenceBuffer( const CompositePropertyMapping::Children& children )
				: SequenceBuffer( children )
			{}

			virtual void streamToBuffer( int numElems, BinaryIStream& strm )
			{
				for ( int i = 0; i < numElems; ++i )
				{
					std::string encStr;
					strm >> encStr;
					MemoryIStream encStrm(
						const_cast<char*>(encStr.c_str()), encStr.length() );
					SequenceBuffer::streamToBuffer( 1, encStrm );
				}
			}

			virtual void bufferToStream( BinaryOStream& strm, int idx ) const
			{
				MemoryOStream encStrm;
				SequenceBuffer::bufferToStream( encStrm, idx );
				// __kyl__ (18/5/2005) We are assuming that this is how a
				// std::string gets serialised.
				strm.appendString( reinterpret_cast<const char *>(
									encStrm.data()), encStrm.size() );
			}
		};

	public:
		virtual ISequenceBuffer* createSequenceBuffer() const
		{	return new UserPropSequenceBuffer( children_ );	}
	};

	// UserTypeMapping maps CLASS types into MySQL columns and tables.
	// It's basically a CompositePropertyMapping with support for object
	// being null.
	class ClassMapping : public CompositePropertyMapping
	{
		bool		allowNone_;
		std::string	colName_;
		uint8		hasProps_;

	public:
		ClassMapping( const Namer& namer, const std::string& propName,
					bool allowNone )
			: CompositePropertyMapping( propName ), allowNone_(allowNone),
			colName_( allowNone_ ? namer.buildColumnName( "fm", propName ) : "" ),
			hasProps_(1)
		{
			if (allowNone_)
				this->addColumns( 1 );
		}

		virtual void createTables( ITableCollector& ti,
				ITableCollector::NameToColInfoMap& cols )
		{
			if (allowNone_)
			{
				ITableCollector::ColumnInfo& colInfo = cols[colName_];
				colInfo.typeStr =
						getColumnTypeStr( "TINYINT UNSIGNED", "1" );
				colInfo.indexType = ITableCollector::IndexTypeNone;
			}
			CompositePropertyMapping::createTables( ti, cols );
		}

		virtual std::string getColumnNames( const std::string& suffix = std::string() )
		{
			std::string colNames;
			if (allowNone_)
			{
				colNames = colName_+suffix;
				if (this->numColumns() > 1)
					colNames += ',';
			}
			colNames += CompositePropertyMapping::getColumnNames( suffix );

			return colNames;
		}

		virtual void addToBindings( MySqlBindings& b )
		{
			if (allowNone_)
				b << hasProps_;
			CompositePropertyMapping::addToBindings( b );
		}

		virtual void streamToBound( BinaryIStream& strm )
		{
			if (allowNone_)
				strm >> hasProps_;
			if (hasProps_)
				CompositePropertyMapping::streamToBound( strm );
			else
				this->nullifyBound();
		}

		virtual void boundToStream( BinaryOStream & strm ) const
		{
			if (allowNone_)
				strm << hasProps_;
			if (hasProps_)
				CompositePropertyMapping::boundToStream( strm );
		}

		virtual void defaultToStream( BinaryOStream & strm ) const
		{
			if (allowNone_)
				strm << uint8(0) ;
			else
				CompositePropertyMapping::defaultToStream( strm );
		}

		virtual void nullifyBound()
		{
			this->setHasProps( 0 );
			CompositePropertyMapping::nullifyBound();
		}

		bool  isAllowNone() const			{ return allowNone_; }
		uint8 getHasProps() const			{ return hasProps_; }
		void setHasProps( uint8 hasProps )	{ hasProps_ = hasProps; }

		virtual PyObject* createPyObject() const
		{
			if (hasProps_)
				return CompositePropertyMapping::createPyObject();
			else
				Py_RETURN_NONE;
		}
		virtual void setValue( PyObject* pyObject )
		{
			if (pyObject != Py_None)
				CompositePropertyMapping::setValue( pyObject );
			else
				this->nullifyBound();
		}

	private:
		// Override CompositePropertyMapping::SequenceBuffer stream methods.
		// This sequence buffer only works if allowNone is true.
		class ClassPropSequenceBuffer : public SequenceBuffer
		{
			// Because class props can be null,
			// ClassPropSequenceBuffer::bufferToStream( strm, i ) doesn't
			// correspond to CompositePropertyMapping::SequenceBuffer::
			// bufferToStream( strm, i ). We need to calculate a new 'i'
			// (say 'j') by subtracting the number null values between 0 and i.
			// nonNullIdxs_ is the precomputed 'j' so that j = nonNullIdxs_[i];
			// And if the i-th value is null, then
			// nonNullIdxs_[i] == nonNullIdxs_[i+1];
			std::vector<int> nonNullIdxs_;

		public:
			ClassPropSequenceBuffer( const CompositePropertyMapping::Children& children )
				: SequenceBuffer( children ), nonNullIdxs_( 1, 0 )
			{}

			virtual void streamToBuffer( int numElems, BinaryIStream& strm )
			{
				uint8 hasProps;
				for ( int i = 0; i < numElems; ++i )
				{
					strm >> hasProps;
					if (hasProps)
					{
						nonNullIdxs_.push_back( nonNullIdxs_.back() + 1 );
						SequenceBuffer::streamToBuffer( 1, strm );
					}
					else
					{
						nonNullIdxs_.push_back( nonNullIdxs_.back() );
					}
				}
			}

			virtual void bufferToStream( BinaryOStream& strm, int idx ) const
			{
				int	realIdx = nonNullIdxs_[idx];
				uint8 hasProps = (realIdx < nonNullIdxs_[idx + 1]) ? 1 : 0;
				strm << hasProps;
				if (hasProps)
					SequenceBuffer::bufferToStream( strm, realIdx );
			}

			virtual void bufferToBound( PropertyMapping& binding,
				                        int idx ) const
			{
				ClassMapping& classMapping =
					static_cast<ClassMapping&>(binding);
				int	realIdx = nonNullIdxs_[idx];
				if (realIdx < nonNullIdxs_[idx + 1])
				{
					classMapping.setHasProps( 1 );
					SequenceBuffer::bufferToBound( binding, realIdx );
				}
				else
				{
					classMapping.nullifyBound();
				}
			}

			virtual void boundToBuffer( const PropertyMapping& binding )
			{
				const ClassMapping& classMapping =
					static_cast<const ClassMapping&>(binding);
				if (classMapping.getHasProps())
				{
					SequenceBuffer::boundToBuffer( binding );
					nonNullIdxs_.push_back( nonNullIdxs_.back() + 1);
				}
				else
				{
					nonNullIdxs_.push_back( nonNullIdxs_.back() );
				}
			}

			virtual int	getNumElems() const
			{
				return int(nonNullIdxs_.size() - 1);
			}

			virtual void reset()
			{
				nonNullIdxs_.resize(1);
				SequenceBuffer::reset();
			}
		};

	public:
		virtual ISequenceBuffer* createSequenceBuffer() const
		{
			return (allowNone_) ? new ClassPropSequenceBuffer( children_ ) :
				// Can use simpler sequence buffer if none is disallowed.
				CompositePropertyMapping::createSequenceBuffer();
		}

	};

	// map sequences to tables
	class SequenceMapping : public PropertyMapping
	{
	public:
		SequenceMapping( const Namer& namer, const std::string& propName,
			PropertyMappingPtr child, int size = 0 ) :
			PropertyMapping(0, propName),
			tblName_( namer.buildTableName( propName ) ),
			child_(child), size_(size), pBuffer_( 0 ), childHasTable_(false)
		{}

		~SequenceMapping()
		{
			delete pBuffer_;
		}

		const PropertyMapping& getChildMapping() const	{	return *child_;	}

		bool isFixedSize() const	{ return size_ != 0; }
		int getFixedSize() const	{ return size_;	}

		virtual void prepareSQL( MySql& con )
	   	{
			// NOTE: child->createSequenceBuffer() can't be initialised in the
			// constructor because UserTypeMapping doesn't not have its
			// children set up yet.
			pBuffer_ = child_->createSequenceBuffer();
			if (!pBuffer_)
				ERROR_MSG( "Persistence to MySQL is not supported for the "
						   "type of sequence used by '%s'.", propName().c_str() );

			// __kyl__ (29/7/2005) childHasTable_ has to be initialised here as
			// well as in createTables() because createTables() gets called
			// before prepareSQL() but only for the main thread. In child threads
			// only prepareSQL() is called.
			childHasTable_ = child_->hasTable();
			MF_ASSERT( childHasTable_ || (child_->numColumns() > 0) );

			std::string stmt;
			MySqlBindings b;

			stmt = "SELECT ";
			if (childHasTable_)
				stmt += "id";
		   	if (child_->numColumns())
			{
				if (childHasTable_)
					stmt += ",";
				stmt += child_->getColumnNames();
			}
			stmt += " FROM " + tblName_ + " WHERE parentID=? ORDER BY id";
			pSelect_.reset( new MySqlStatement( con, stmt ) );
			b.clear();
			if (childHasTable_)
				b << childID_;
			child_->addToBindings( b );
			pSelect_->bindResult( b );
			b.clear();
			b << queryID_;
			pSelect_->bindParams( b );

			stmt = "SELECT id FROM " + tblName_ + " WHERE parentID=? ORDER BY "
					"id FOR UPDATE";
			pSelectChildren_.reset( new MySqlStatement( con, stmt ) );
			b.clear();
			b << childID_;
			pSelectChildren_->bindResult( b );
			b.clear();
			b << queryID_;
			pSelectChildren_->bindParams( b );

			stmt = "INSERT INTO " + tblName_ + " (parentID";
			if (child_->numColumns())
				stmt += "," + child_->getColumnNames();
		   	stmt += ") VALUES (" +
				buildCommaSeparatedQuestionMarks( 1 + child_->numColumns() ) + ")";
			pInsert_.reset( new MySqlStatement( con, stmt ) );
			
			stmt = "UPDATE " + tblName_ + " SET parentID=?";
			if (child_->numColumns())
				stmt += "," + child_->getColumnNames( "=?" );
			stmt += " WHERE id=?";
			pUpdate_.reset( new MySqlStatement( con, stmt ) );
			
			b.clear();
			b << queryID_;
			child_->addToBindings( b );
			pInsert_->bindParams( b );
			b << childID_;
			pUpdate_->bindParams( b );
			
			stmt = "DELETE FROM " + tblName_ + " WHERE parentID=?";
			pDelete_.reset( new MySqlStatement( con, stmt ) );
			stmt += " AND id >= ?";
			pDeleteExtra_.reset( new MySqlStatement( con, stmt ) );
			b.clear();
			b << queryID_;
			pDelete_->bindParams( b );
			b << childID_;
			pDeleteExtra_->bindParams( b );

			child_->prepareSQL( con );
		}

		virtual void createTables( ITableCollector& ti,
				ITableCollector::NameToColInfoMap& cols )
		{
			childHasTable_ = child_->hasTable();
			MF_ASSERT( childHasTable_ || (child_->numColumns() > 0) );

			ITableCollector::NameToColInfoMap childCols;
			child_->createTables( ti, childCols );
			// Add parentID column
			ITableCollector::ColumnInfo& parentIDCol = childCols["parentID"];
			parentIDCol.typeStr =  "BIGINT NOT NULL";
			parentIDCol.indexType = ITableCollector::IndexTypeParentID;
			// Add id column
			ITableCollector::ColumnInfo& idCol = childCols[ ID_COLUMN_NAME ];
			idCol.typeStr = ID_COLUMN_TYPE_NAME;
			idCol.indexType = ITableCollector::IndexTypePrimary;
			ti.requireTable( tblName_, childCols );
		}

		virtual std::string getColumnNames( const std::string& ) { return ""; }
		virtual void addToBindings( MySqlBindings& ) {}

		// Get the number of elements present in the stream.
		int getNumElemsFromStrm( BinaryIStream& strm ) const
		{
			if (this->isFixedSize())
				return this->getFixedSize();

			int numElems;
			strm >> numElems;
			return numElems;
		}

		virtual void streamToBound( BinaryIStream& strm )
		{
			int numElems = this->getNumElemsFromStrm( strm );

			if (pBuffer_)
			{
				pBuffer_->reset();
				pBuffer_->streamToBuffer( numElems, strm );
			}
			else
			{	// Sequence type not supported. Skip over data in stream.
				for ( int i = 0; i < numElems; ++i )
					child_->streamToBound( strm );
			}
		}

		int setNumElemsInStrm( BinaryOStream & strm, int numElems ) const
		{
			if (this->isFixedSize())
				return this->getFixedSize();

			strm << numElems;
			return numElems;
		}

		virtual void boundToStream( BinaryOStream & strm ) const
		{
			if (pBuffer_)
			{
				int numAvailElems = pBuffer_->getNumElems();
				int numElems = setNumElemsInStrm( strm, numAvailElems );
				int numFromBuf = std::min( numElems, numAvailElems );
				for ( int i = 0; i < numFromBuf; ++i )
				{
					pBuffer_->bufferToStream( strm, i );
				}
				for ( int i = numFromBuf; i < numElems; ++i )
				{
					child_->defaultToStream( strm );
				}
			}
		}

		virtual void defaultToStream( BinaryOStream & strm ) const
		{
			if (this->isFixedSize())
			{
				int numElems = this->getFixedSize();
				strm << numElems;
				for ( int i = 0; i < numElems; ++i )
				{
					child_->defaultToStream( strm );
				}
			}
			else
			{
				strm << int(0);
			}
		}

		virtual void nullifyBound()
		{
			if (pBuffer_)
				pBuffer_->reset();
		}

		virtual bool hasTable() const	{	return true;	}

		virtual void updateTable( MySqlTransaction& transaction,
			DatabaseID parentID )
		{
			if (pBuffer_)
			{
				int numElems = pBuffer_->getNumElems();
				if (numElems == 0)	// Optimisation for empty arrays
				{
					this->deleteChildren( transaction, parentID );
				}
				else
				{
					// my_ulonglong affectedRows = 0;

					queryID_ = parentID;
					transaction.execute( *pSelectChildren_ );
					int numRows = pSelectChildren_->resultRows();
					int numUpdates = std::min( numRows, numElems );

					// Update existing rows
					for ( int i = 0; i < numUpdates; ++i )
					{
						pSelectChildren_->fetch();
						pBuffer_->bufferToBound( *child_, i );
						transaction.execute( *pUpdate_ );
						
						// affectedRows += transaction.affectedRows();
						
						if (childHasTable_)
						{
							child_->updateTable( transaction, childID_ );
						}
					}
					
					// Delete any extra rows (i.e. array has shrunk).
					if (pSelectChildren_->fetch())
					{
						transaction.execute( *pDeleteExtra_ );
						// affectedRows += transaction.affectedRows();
						if (childHasTable_)
						{
							do
							{
								child_->deleteChildren( transaction, childID_ );
							} while ( pSelectChildren_->fetch() );
						}
					}
					// Insert any extra rows (i.e. array has grown)
					else if (numElems > numRows)
					{
						// TODO: Multi-row insert in one statement.
						for (int i = numRows; i < numElems; ++i)
						{
							pBuffer_->bufferToBound( *child_, i );
							transaction.execute( *pInsert_ );							
							// affectedRows += transaction.affectedRows();
							if (childHasTable_)
							{
								DatabaseID insertID = transaction.insertID();
								child_->updateTable( transaction, insertID );
							}
						}
					}
					// DEBUG_MSG( "SequenceMapping::updateTable: table=%s, "
					//		"affectedRows=%d\n", tblName_.c_str(), 
					//		int(affectedRows) );
				}
			}
		}

		virtual void getTableData( MySqlTransaction& transaction,
			DatabaseID parentID )
		{
			if (pBuffer_)
			{
				pBuffer_->reset();

				queryID_ = parentID;
				transaction.execute( *pSelect_ );
				int numElems = pSelect_->resultRows();

				for ( int i = 0; i < numElems; ++i )
				{
					pSelect_->fetch();
					if (childHasTable_)
						child_->getTableData( transaction, childID_ );
					pBuffer_->boundToBuffer( *child_ );
				}
			}
		}

		virtual void deleteChildren( MySqlTransaction& t, DatabaseID databaseID )
		{
			queryID_ = databaseID;
			if (childHasTable_)
			{
				t.execute( *pSelectChildren_ );
				while ( pSelectChildren_->fetch() )
				{
					child_->deleteChildren( t, childID_ );
				}
			}
			t.execute( *pDelete_ );
		}

		virtual PyObject* createPyObject() const
		{
			PyObject* pList = PyList_New(0);
			if (pBuffer_)
			{
				int numElems = pBuffer_->getNumElems();
				for ( int i = 0; i < numElems; ++i )
				{
					pBuffer_->bufferToBound( *child_, i );
					PyObjectPtr pValue( child_->createPyObject(),
										PyObjectPtr::STEAL_REFERENCE );
					PyList_Append( pList, pValue.getObject() );
				}
			}
			return pList;
		}

		virtual void setValue( PyObject* pyObject )
		{
			if (pBuffer_)
			{
				pBuffer_->reset();
				if (PySequence_Check( pyObject ))
				{
					int numElems = PySequence_Size( pyObject );
					for ( int i = 0; i < numElems; ++i )
					{
						PyObjectPtr pItem( PySequence_GetItem( pyObject, i ),
							PyObjectPtr::STEAL_REFERENCE );
						child_->setValue( pItem.getObject() );
						pBuffer_->boundToBuffer( *child_ );
					}
				}
			}
		}

		PropertyMapping::ISequenceBuffer* swapChildSeqBuffer(
			PropertyMapping::ISequenceBuffer* pBuffer )
		{
			MF_ASSERT( pBuffer_ );
			PropertyMapping::ISequenceBuffer* pCurBuf = pBuffer_;
			pBuffer_ = pBuffer;
			return pCurBuf;
		}

	private:
		class SequenceBuffer : public PropertyMapping::ISequenceBuffer
		{
			typedef std::vector< PropertyMapping::ISequenceBuffer* > SequenceBuffers;

			const SequenceMapping& mapping_;
			auto_container< SequenceBuffers > childBuffers_;
			int	numUsed_;
			mutable int swappedIdx_;

		public:
			SequenceBuffer(const SequenceMapping& mapping)
				: mapping_(mapping), numUsed_(0), swappedIdx_(-1)
			{
				childBuffers_.container.push_back(
					mapping_.getChildMapping().createSequenceBuffer() );
			}

			virtual void streamToBuffer( int numElems, BinaryIStream& strm )
			{
				int numRequired = numUsed_ + numElems;
				for ( int i = int(childBuffers_.container.size());
						i < numRequired; ++i )
				{
					childBuffers_.container.push_back(
						mapping_.getChildMapping().createSequenceBuffer() );
				}

				for ( int i = numUsed_; i < numRequired; ++i )
				{
					int numChildElems = mapping_.getNumElemsFromStrm( strm );
					childBuffers_.container[i]->
						streamToBuffer( numChildElems, strm );
				}
				numUsed_ = numRequired;
				swappedIdx_ = -1;
			}

			virtual void bufferToStream( BinaryOStream& strm, int idx ) const
			{
				MF_ASSERT( swappedIdx_ < 0 );
				PropertyMapping::ISequenceBuffer* pChildSeqBuf =
					childBuffers_.container[idx];
				int numAvail = pChildSeqBuf->getNumElems();
				int numElems = mapping_.setNumElemsInStrm( strm, numAvail );
				int numFromBuf = std::min( numElems, numAvail );
				for ( int i = 0; i < numFromBuf; ++i )
				{
					pChildSeqBuf->bufferToStream( strm, i );
				}
				for ( int i = numFromBuf; i < numElems; ++i )
				{
					mapping_.getChildMapping().defaultToStream( strm );
				}
			}

			virtual void bufferToBound( PropertyMapping& binding,
										int idx ) const
			{
				MF_ASSERT( idx < numUsed_ );
				// We actually swap the buffer with the bindings instead of
				// copying it to the bindings.
				if (swappedIdx_ == idx)
					return;	// Data already there.

				SequenceBuffer* pThis = const_cast<SequenceBuffer*>(this);
				SequenceMapping& seqMapping = static_cast<SequenceMapping&>(binding);
				PropertyMapping::ISequenceBuffer* pPrevBuffer =
					seqMapping.swapChildSeqBuffer( childBuffers_.container[idx] );

				// Remember the index of the swapped buffer so that we can
				// swap them back later on.
				if (swappedIdx_ >= 0)
				{
					// We swapped the buffer previously so pPrevBuffer must be
					// the buffer that was at swappedIdx_. Restore it.
					pThis->childBuffers_.container[idx] =
							pThis->childBuffers_.container[swappedIdx_];
					pThis->childBuffers_.container[swappedIdx_] = pPrevBuffer;
				}
				else
				{
					pThis->childBuffers_.container[idx] = pPrevBuffer;
				}
				swappedIdx_ = idx;
			}

			virtual void boundToBuffer( const PropertyMapping& binding )
			{
				MF_ASSERT( swappedIdx_ < 0 );
				if (numUsed_ == int(childBuffers_.container.size()))
				{
					childBuffers_.container.push_back(
						mapping_.getChildMapping().createSequenceBuffer() );
				}

				SequenceMapping& seqMapping = const_cast<SequenceMapping&>(
					static_cast<const SequenceMapping&>(binding) );
				childBuffers_.container[numUsed_] =
					seqMapping.swapChildSeqBuffer( childBuffers_.container[numUsed_] );
				++numUsed_;
			}
			
			virtual int	getNumElems() const
			{
				return numUsed_;
			}

			virtual void reset()
			{
				numUsed_ = 0;
				swappedIdx_ = -1;
				for ( SequenceBuffers::iterator i = childBuffers_.container.begin();
					  i < childBuffers_.container.end(); ++i )
				{
					(*i)->reset();
				}
			}
		};

	public:
		virtual ISequenceBuffer* createSequenceBuffer()	const
		{
			return new SequenceBuffer(*this);
		}

	private:

		std::string tblName_;
		PropertyMappingPtr child_;
		int	size_;
		ISequenceBuffer* pBuffer_;
		DatabaseID queryID_;
		DatabaseID childID_;
		bool childHasTable_;

		// auto_ptr's so we can delay instantiation
		std::auto_ptr<MySqlStatement> pSelect_;
		std::auto_ptr<MySqlStatement> pSelectChildren_;
		std::auto_ptr<MySqlStatement> pDelete_;
		std::auto_ptr<MySqlStatement> pDeleteExtra_;
		std::auto_ptr<MySqlStatement> pInsert_;
		std::auto_ptr<MySqlStatement> pUpdate_;
	};

	/**
	 *	Utility class used by various PropertyMapping classes to implement
	 *	their PropertyMapping::ISequenceBuffer.
	 */
	template < class STRM_TYPE, class MAPPING_TYPE >
	class PrimitiveSequenceBuffer : public PropertyMapping::ISequenceBuffer
	{
		typedef	std::vector< STRM_TYPE > Buffer;
		Buffer	buffer_;

	public:
		PrimitiveSequenceBuffer()
			: buffer_()
		{}

		virtual void streamToBuffer( int numElems,
				                        BinaryIStream& strm )
		{
			typename Buffer::size_type numUsed = buffer_.size();
			buffer_.resize( buffer_.size() + numElems );

			for ( typename Buffer::iterator pVal = buffer_.begin() + numUsed;
				  pVal < buffer_.end(); ++pVal )
				strm >> *pVal;
		}

		virtual void bufferToStream( BinaryOStream& strm, int idx ) const
		{
			strm << buffer_[idx];
		}

		// Set binding with value of element number "idx" in the buffer.
		virtual void bufferToBound( PropertyMapping& binding,
				                    int idx ) const
		{
			static_cast<MAPPING_TYPE&>(binding).setValue( buffer_[idx] );
		}

		virtual void boundToBuffer( const PropertyMapping& binding )
		{
			buffer_.push_back( static_cast<const MAPPING_TYPE&>(binding).getValue() );
		}

		virtual int	getNumElems() const	{ return int(buffer_.size());	}

		virtual void reset()	{	buffer_.clear();	}
	};

	template <class STRM_NUM_TYPE>
	class NumMapping : public PropertyMapping
	{
	public:
        /* huangshanquan  2009-12-29 change begin CSOL-7399*/
		NumMapping( const Namer& namer, const std::string& propName,
				DataSectionPtr pDefaultValue,
                bool isNameIndex = false,
                int IndexType = -1) :
			PropertyMapping(1, propName),
			colName_( namer.buildColumnName( "sm", propName ) ),
            defaultValue_(0),
            isNameIndex_( isNameIndex),
            IndexType_(IndexType)
        /* huangshanquan 2009-12-29 change end*/
		{
			if (pDefaultValue)
				defaultValue_ = pDefaultValue->as<STRM_NUM_TYPE>();
		}

		virtual void createTables( ITableCollector& ti,
				ITableCollector::NameToColInfoMap& cols )
		{
			ITableCollector::ColumnInfo& colInfo = cols[colName_];
			colInfo.typeStr = getColumnTypeStr(
					MySqlTypeTraits<STRM_NUM_TYPE>::colTypeStr,
					StringConv::toStr( defaultValue_ ) );

            /* huangshanquan  2009-12-29 change begin CSOL-7399*/
			//colInfo.indexType = ITableCollector::IndexTypeNone;
            switch (IndexType_)
            {
                case 4:
                    colInfo.indexType = ITableCollector::IndexTypeUnique;
                    break;
                case 5:
                    colInfo.indexType = ITableCollector::IndexTypeNormal;
                    break;
                default:
			        //colInfo.indexType = ITableCollector::IndexTypeNone;
                    colInfo.indexType = (isNameIndex_) ? ITableCollector::IndexTypeName :
                        TableCollector::IndexTypeNone;
                    break;
            }
            /* huangshanquan 2009-12-29 change end*/

		}

		virtual std::string getColumnNames( const std::string& suffix )
		{
			return colName_+suffix;
		}

		virtual void streamToBound( BinaryIStream& strm )
		{
			STRM_NUM_TYPE i;
			strm >> i;
			value_.set(i);
		}

		virtual void boundToStream( BinaryOStream & strm ) const
		{
			const STRM_NUM_TYPE* pi = value_.get();
			if (pi)
				strm << *pi;
			else
				strm << defaultValue_;
		}

		virtual void defaultToStream( BinaryOStream & strm ) const
		{
			strm << defaultValue_;
		}

		virtual void nullifyBound()
		{
			value_.nullify();
		}

		virtual bool hasTable() const	{	return false;	}
		virtual void updateTable( MySqlTransaction& transaction,
			DatabaseID parentID ) {}	// No child table for this type
		virtual void getTableData( MySqlTransaction& transaction,
			DatabaseID parentID ) {}	// No child table for this type

		virtual void addToBindings( MySqlBindings& b )
		{
			b << value_;
		}

		virtual void prepareSQL( MySql& ) {}
		virtual void deleteChildren( MySqlTransaction&, DatabaseID ) {}

		virtual ISequenceBuffer* createSequenceBuffer() const
		{	return new PrimitiveSequenceBuffer< STRM_NUM_TYPE, NumMapping< STRM_NUM_TYPE > >();	}

		virtual PyObject* createPyObject() const
		{
			const STRM_NUM_TYPE* pVal = value_.get();
			if (pVal)
				return PyConv::toPyObject( *pVal );
			else
				Py_RETURN_NONE;
		}
		virtual void setValue( PyObject* pyObject )
		{
			STRM_NUM_TYPE val;
			if (PyConv::toValue( val, pyObject ))
				value_.set( val );
			else
				value_.nullify();
		}

		void setValue( STRM_NUM_TYPE val )	{	value_.set( val );	}
		STRM_NUM_TYPE getValue() const
		{
			const STRM_NUM_TYPE* pNum = value_.get();
			return (pNum) ? *pNum : defaultValue_;
		}

	private:
		std::string colName_;
		MySqlValueWithNull<STRM_NUM_TYPE> value_;
		STRM_NUM_TYPE defaultValue_;
        /* huangshanquan  2009-12-29 add begin CSOL-7399*/
        bool    isNameIndex_;
        int     IndexType_;
        /* huangshanquan 2009-12-29 add end*/

	};

	template <class Vec, int DIM>
	class VectorMapping : public PropertyMapping
	{
	public:
		VectorMapping( const Namer& namer, const std::string& propName,
					DataSectionPtr pDefaultValue ) :
			PropertyMapping(DIM, propName),
			colNameTemplate_( namer.buildColumnName( "vm_%i", propName ) ),
			defaultValue_()
		{
			if (pDefaultValue)
				defaultValue_ = pDefaultValue->as<Vec>();
		}

		virtual void createTables( ITableCollector& ti,
				ITableCollector::NameToColInfoMap& cols )
		{
			char buffer[512];
			for ( int i=0; i<DIM; i++ )
			{
				sprintf( buffer, colNameTemplate_.c_str(), i );
				ITableCollector::ColumnInfo& colInfo = cols[buffer];
				colInfo.typeStr = getColumnTypeStr(
						"FLOAT", StringConv::toStr( defaultValue_[i] ) );
				colInfo.indexType = ITableCollector::IndexTypeNone;
			}
		}

		virtual std::string getColumnNames( const std::string& suffix )
		{
			std::string out;
			char buffer[512];
			for ( int i=0; i<DIM; i++ )
			{
				sprintf( buffer, colNameTemplate_.c_str(), i );
				out += buffer;
				out += suffix;
				if (i < DIM -1)
					out += ',';
			}
			return out;
		}

		bool isNull() const
		{
			for ( int i=0; i<DIM; i++ )
				if (!value_[i].get())
					return true;
			return false;
		}

		virtual void streamToBound( BinaryIStream& strm )
		{
			Vec v;
			strm >> v;
			this->setValue(v);
		}

		virtual void boundToStream( BinaryOStream & strm ) const
		{
			if (!this->isNull())
			{
				Vec v;
				for ( int i = 0; i < DIM; ++i )
					v[i] = *value_[i].get();
				strm << v;
			}
			else
			{
				strm << defaultValue_;
			}
		}

		virtual void defaultToStream( BinaryOStream & strm ) const
		{
			strm << defaultValue_;
		}

		virtual void nullifyBound()
		{
			for ( int i=0; i<DIM; i++ )
				value_[i].nullify();
		}

		virtual bool hasTable() const	{	return false;	}
		virtual void updateTable( MySqlTransaction& transaction,
			DatabaseID parentID ) {}	// No child table for this type
		virtual void getTableData( MySqlTransaction& transaction,
			DatabaseID parentID ) {}	// No child table for this type

		virtual void addToBindings( MySqlBindings& b )
		{
			for ( int i=0; i<DIM; i++ )
				b << value_[i];
		}

		virtual void prepareSQL( MySql& ) {}
		virtual void deleteChildren( MySqlTransaction&, DatabaseID ) {}

		virtual ISequenceBuffer* createSequenceBuffer() const
		{	return new PrimitiveSequenceBuffer< Vec, VectorMapping< Vec, DIM > >();	}

		virtual PyObject* createPyObject() const
		{
			if (!this->isNull())
			{
				PyObject* pTuple = PyTuple_New( DIM );
				for ( int i = 0; i < DIM; ++i )
				{
					PyTuple_SetItem( pTuple, i,
									PyConv::toPyObject( *value_[i].get() ) );
				}
				return pTuple;
			}
			else
			{
				Py_RETURN_NONE;
			}
		}
		virtual void setValue( PyObject* pyObject )
		{
			if ( PyTuple_Check( pyObject ) && (PyTuple_Size( pyObject ) == DIM) )
			{
				for ( int i = 0; i < DIM; ++i )
				{
					PyObject* pVal = PyTuple_GetItem( pyObject, i );
					float val;
					if (PyConv::toValue( val, pVal ))
						value_[i].set( val );
					else
						value_[i].set( 0.0f );
				}
			}
			else
			{
				this->nullifyBound();
			}
		}

		void setValue( const Vec& v )
		{
			for ( int i = 0; i < DIM; ++i )
				value_[i].set(v[i]);
		}
		Vec getValue() const
		{
			if (this->isNull())
				return defaultValue_;

			Vec	v;
			for ( int i = 0; i < DIM; ++i )
				v[i] = *value_[i].get();
			return v;
		}

	private:
		std::string colNameTemplate_;
		MySqlValueWithNull<float> value_[DIM];
		Vec defaultValue_;
	};
}

/**
 *	This class is the base class for a number data type mappings. It maps
 * 	any std::string-like data into the database
 */
// __kyl__ (28/9/2005) StringLikeMapping can't be in anonymous namespace because
// it is forward declared in the header file.
class StringLikeMapping : public PropertyMapping
{
public:
    /* huangshanquan  2009-12-28 change begin CSOL-7399*/
	StringLikeMapping( const Namer& namer, const std::string& propName,
			bool isNameIndex, int length, int IndexType = -1) :
		PropertyMapping(1, propName),
		colName_( namer.buildColumnName( "sm", propName ) ), value_(length),
		isNameIndex_( isNameIndex ), defaultValue_(),
		IndexType_(IndexType)
    /* huangshanquan 2009-12-28 change end*/
	{}

	void getString( std::string& str ) const	{	str = value_.getString();	}
	void setString( const std::string& str )	{	value_.setString(str);		}

	void createTablesCommon( ITableCollector& ti,
			ITableCollector::NameToColInfoMap& cols,
			const std::string& columnType, bool shouldInsertSizePrefix = true )
	{
		ITableCollector::ColumnInfo& colInfo = cols[colName_];

        /* huangshanquan  2009-12-28 change begin CSOL-7399*/
		//colInfo.indexType = (isNameIndex_) ? ITableCollector::IndexTypeName :
								//ITableCollector::IndexTypeNone;
        switch (IndexType_)
        {
            case 4:
                colInfo.indexType = ITableCollector::IndexTypeUnique;
                break;
            case 5:
                colInfo.indexType = ITableCollector::IndexTypeNormal;
                break;
            default:
                colInfo.indexType = (isNameIndex_) ? ITableCollector::IndexTypeName :
                    ITableCollector::IndexTypeNone;
                break;
        }
        /* huangshanquan 2009-12-28 change end*/

		if (!shouldInsertSizePrefix)
		{
			colInfo.typeStr = columnType;
		}
		else
		{
			const char * sizeStr;
			if (value_.capacity() < 1<<8)
				sizeStr = "TINY";
			else if ( value_.capacity() < 1<<16 )
				sizeStr = "";
			else if ( value_.capacity() < 1<<24 )
				sizeStr = "MEDIUM";
			else
			{
				CRITICAL_MSG( "StringLikeMapping::createTablesCommon: Property "
						"'%s' has DatabaseLength %u that exceeds the maximum "
						"supported length of 16777215\n",
						this->propName().c_str(), value_.capacity() );
				// Can't put string > 16MB onto stream.
				sizeStr = "LONG";
			}

			colInfo.typeStr = std::string( sizeStr ) + columnType;
		}

		// TEXT and BLOB columns can't have DEFAULT value. So must manually
		// set default value when we add a new column.
		colInfo.shouldSetDefault = true;
		colInfo.defaultVal = defaultValue_;
	}

	virtual std::string getColumnNames( const std::string& suffix )
	{
		return colName_+suffix;
	}

	virtual void streamToBound( BinaryIStream& strm )
	{
		std::string str;
		strm >> str;
		value_.setString(str);
	}

	virtual void boundToStream( BinaryOStream & strm ) const
	{
		if (!value_.isNull())
		{
			strm << value_.getString();
		}
		else
		{
			strm << defaultValue_;
		}
	}

	virtual void defaultToStream( BinaryOStream & strm ) const
	{
		strm << defaultValue_;
	}

	virtual void nullifyBound()
	{
		value_.nullify();
	}

	virtual bool hasTable() const	{	return false;	}
	virtual void updateTable( MySqlTransaction& transaction,
		DatabaseID parentID ) {}	// No child table for this type
	virtual void getTableData( MySqlTransaction& transaction,
		DatabaseID parentID ) {}	// No child table for this type

	virtual void addToBindings( MySqlBindings& b )
	{
		b << value_;
	}

	virtual void prepareSQL( MySql& ) {}
	virtual void deleteChildren( MySqlTransaction&, DatabaseID ) {}

	virtual ISequenceBuffer* createSequenceBuffer() const
	{ return new PrimitiveSequenceBuffer< std::string, StringLikeMapping >(); }

	virtual PyObject* createPyObject() const
	{
		if (!value_.isNull())
		{
			return PyString_FromStringAndSize( (const char*)value_.get(),
					value_.size() );
		}
		else
		{
			Py_RETURN_NONE;
		}
	}
	virtual void setValue( PyObject* pyObject )
	{
		if ( PyString_Check( pyObject ) )
		{
			int 	len;
			char* 	str;
			PyString_AsStringAndSize( pyObject, &str, &len );
			value_.set( str, len );
		}
		else
		{
			this->nullifyBound();
		}
	}

	void setValue( const std::string& str )	{	value_.setString(str);	}
	std::string getValue() const
	{
		if (value_.isNull())
			return defaultValue_;
		else
			return value_.getString();
	}

protected:

	std::string colName_;
	MySqlBuffer value_;
	bool		isNameIndex_;
	std::string	defaultValue_;
    /* huangshanquan  2009-12-28 add begin CSOL-7399*/
    int IndexType_;
    /* huangshanquan 2009-12-28 add end*/
};

namespace
{

/**
 *	This class maps the STRING type to the database.
 */
class StringMapping : public StringLikeMapping
{
public:
    /* huangshanquan  2009-12-28 change begin CSOL-7399*/
	StringMapping( const Namer& namer, const std::string& propName,
			bool isNameIndex, int length, DataSectionPtr pDefaultValue ,int IndexType = -1) :
		StringLikeMapping( namer, propName, isNameIndex, length, IndexType )
    /* huangshanquan 2009-12-28 change end*/
	{
		if (pDefaultValue)
			defaultValue_ = pDefaultValue->as<std::string>();
	}

	virtual void createTables( ITableCollector& ti,
			ITableCollector::NameToColInfoMap& cols )
	{
		// __kyl__ (24/7/2006) Special handling of STRING < 255 characters
		// because this is how we magically pass the size of the name index
		// field. If type is not VARCHAR then index size is assumed to be
		// 255 (see TableInitialiser::createNameIndex).
		if (value_.capacity() < 1<<8)
		{
			char buffer[32];
			sprintf( buffer, "VARCHAR(%i)", int(value_.capacity()) );

			this->createTablesCommon( ti, cols, buffer, false );
		}
		else
		{
			this->createTablesCommon( ti, cols, "TEXT" );
		}
	}
};

/**
 *	This class maps the BLOB type to the database.
 */
class BlobMapping : public StringLikeMapping
{
public:
	BlobMapping( const Namer& namer, const std::string& propName,
			bool isNameIndex, int length, DataSectionPtr pDefaultValue ) :
		StringLikeMapping( namer, propName, isNameIndex, length )
	{
		if (pDefaultValue)
			BlobMapping::decodeSection( defaultValue_, pDefaultValue );
	}

	virtual void createTables( ITableCollector& ti,
			ITableCollector::NameToColInfoMap& cols )
	{
		this->createTablesCommon( ti, cols, "BLOB" );
	}

	// This method gets the section data as a base64 encoded string
	// and decodes it, placing the result in output.
	static void decodeSection( std::string& output, DataSectionPtr pSection )
	{
		output = pSection->as<std::string>();
		int len = output.length();
		if (len <= 256)
		{
			// Optimised for small strings.
			char decoded[256];
			int length = Base64::decode( output, decoded, 256 );
			output.assign(decoded, length);
		}
		else
		{
			char * decoded = new char[ len ];
			int length = Base64::decode( output, decoded, len );
			output.assign(decoded, length);
			delete [] decoded;
		}
	}
};

/**
 *	This class maps the PYTHON type to the database.
 */
class PythonMapping : public StringLikeMapping
{
public:
	PythonMapping( const Namer& namer, const std::string& propName,
			bool isNameIndex, int length, DataSectionPtr pDefaultValue ) :
		StringLikeMapping( namer, propName, isNameIndex, length )
	{
		if (pDefaultValue)
			defaultValue_ = pDefaultValue->as<std::string>();

		if (defaultValue_.length() == 0)
		{
			defaultValue_ = PythonMapping::getPickler().pickle( Py_None );
		}
		else if (PythonDataType_IsExpression( defaultValue_ ))
		{
			PythonMapping::pickleExpression( defaultValue_, defaultValue_ );
		}
		else
		{
			BlobMapping::decodeSection( defaultValue_, pDefaultValue );
		}
	}

	virtual void createTables( ITableCollector& ti,
			ITableCollector::NameToColInfoMap& cols )
	{
		this->createTablesCommon( ti, cols, "BLOB" );
	}

	virtual void boundToStream( BinaryOStream & strm ) const
	{
		// An empty string is not a valid PYTHON stream
		if (!value_.isNull() && value_.size() > 0)
		{
			strm << value_.getString();
		}
		else
		{
			strm << defaultValue_;
		}
	}

	virtual PyObject* createPyObject() const
	{
		if (!value_.isNull())
		{
			return this->getPickler().unpickle( value_.getString() );
		}
		else
		{
			Py_RETURN_NONE;
		}
	}
	virtual void setValue( PyObject* pyObject )
	{
		value_.setString( this->getPickler().pickle( pyObject ) );
	}


private:
	// This method evaluates expr as a Python expression, pickles the
	// resulting object and stores it in output.
	static void pickleExpression( std::string& output, const std::string& expr )
	{
		PyObjectPtr pResult( Script::runString( expr.c_str(), false ),
				PyObjectPtr::STEAL_REFERENCE );

		PyObject* 	pToBePickled = pResult ? pResult.getObject() : Py_None;

		output = PythonMapping::getPickler().pickle( pToBePickled );
	}

	static Pickler & getPickler()
	{
		static Pickler pickler;

		return pickler;
	}
};

// Streaming operators for UniqueID. This is required for using
// PrimitiveSequenceBuffer.
inline BinaryOStream& operator<<( BinaryOStream& strm, const UniqueID& uniqueID )
{
	strm << uniqueID.toString();
	return strm;
}
inline BinaryIStream& operator>>( BinaryIStream& strm, UniqueID& uniqueID )
{
	std::string uniqueIDStr;
	strm >> uniqueIDStr;
	uniqueID = UniqueID( uniqueIDStr );

	return strm;
}

}

/**
 *	Maps a PATROL_PATH into MySQL.
 */
// NOTE: Can't be in anonymous namespace because it is friend of UniqueID.
class PatrolPathMapping : public PropertyMapping
{
public:
	PatrolPathMapping( const Namer& namer, const std::string& propName,
			DataSectionPtr pDefaultValue ) :
		PropertyMapping( 1, propName ),
		colName_( namer.buildColumnName( "sm", propName ) ),
		value_( sizeof(uint32) * 4 )
	{
		 if (pDefaultValue)
	 	 	defaultValue_ = UniqueID( pDefaultValue->asString() );
	}

	virtual void createTables( ITableCollector& ti,
			ITableCollector::NameToColInfoMap& cols )
	{
		ITableCollector::ColumnInfo& colInfo = cols[ colName_ ];
		colInfo.typeStr = "BINARY(16)";
		colInfo.indexType = ITableCollector::IndexTypeNone;

		// Even though BINARY support default values, we don't have the
		// connection here to escape the default string. So use manual default
		// facility.
		this->setValue( defaultValue_ );
		colInfo.shouldSetDefault = true;
		colInfo.defaultVal = value_.getString();
	}

	virtual std::string getColumnNames( const std::string& suffix = std::string() )
	{
		return colName_+suffix;
	}

	virtual void addToBindings( MySqlBindings& bindings )
	{
		bindings << value_;
	}

	void setValue( const UniqueID& uniqueID )
	{
		uint32 id[4];

		id[0] = uniqueID.a;
		id[1] = uniqueID.b;
		id[2] = uniqueID.c;
		id[3] = uniqueID.d;

		value_.set( id, sizeof(id) );
	}
	UniqueID getValue() const
	{
		const uint32* id = reinterpret_cast<const uint32*>( value_.get() );
		if (id)
			return UniqueID( id[0], id[1], id[2], id[3] );
		else
			return  defaultValue_;
	}

	virtual void streamToBound( BinaryIStream & strm )
	{
		UniqueID patrolPathID;
		strm >> patrolPathID;
		this->setValue( patrolPathID );
	}

	virtual void boundToStream( BinaryOStream & strm ) const
	{
		strm << this->getValue();
	}

	virtual void defaultToStream( BinaryOStream & strm ) const
	{
		strm << defaultValue_;
	}

	virtual void nullifyBound()
	{
		value_.nullify();
	}

	virtual void prepareSQL( MySql& ) {}
	virtual bool hasTable() const	{ return false; }
	virtual void updateTable( MySqlTransaction& transaction,
			DatabaseID parentID ) {}
	virtual void getTableData( MySqlTransaction& transaction,
			DatabaseID parentID ) {}
	virtual void deleteChildren( MySqlTransaction&, DatabaseID parentID ) {}

	virtual ISequenceBuffer* createSequenceBuffer() const
	{
		return new PrimitiveSequenceBuffer< UniqueID, PatrolPathMapping >();
	}

	virtual PyObject* createPyObject() const
	{
		UniqueID uniqueID = this->getValue();
		std::string uniqueIDStr = uniqueID.toString();
		return PyString_FromStringAndSize( uniqueIDStr.data(),
					uniqueIDStr.length() );
	}
	virtual void setValue( PyObject* pyObject )
	{
		if ( PyString_Check( pyObject ) )
		{
			int 	len;
			char* 	str;
			PyString_AsStringAndSize( pyObject, &str, &len );
			std::string uniqueIDStr( str, len );
			this->setValue( UniqueID( uniqueIDStr ) );
		}
		else
		{
			this->nullifyBound();
		}
	}

private:
	std::string	colName_;
	UniqueID 	defaultValue_;
	MySqlBuffer value_;
};


namespace
{
	/**
	 *  this class allows scripts to specify how a datasection should be bound to sql tables
	 *  it then allows createUserTypeMapping to pull out a PropertyMappingPtr to apply this
	 */
	class PyUserTypeBinder : public PyObjectPlus
	{
		Py_Header( PyUserTypeBinder, PyObjectPlus )

	public:
		PyUserTypeBinder( const Namer& namer, const std::string& propName,
			DataSectionPtr pDefaultValue, PyTypePlus * pType = &s_type_ ) :
			PyObjectPlus( pType )
		{
			// Don't add extra naming level if user prop is unnamed
			// i.e. inside a sequence.
			if (propName.empty())
				tables_.push( Context( new UserTypeMapping( propName ),
									namer, pDefaultValue ) );
			else
				tables_.push( Context( new UserTypeMapping( propName ),
									namer, propName, false,
									pDefaultValue ) );
		}

		// start building a child table
		void beginTable( const std::string& name );
		PY_AUTO_METHOD_DECLARE( RETVOID, beginTable, ARG( std::string, END ) );
		// finish building that table (works like a stack)
		bool endTable();
		PY_AUTO_METHOD_DECLARE( RETOK, endTable, END );
		// bind a column into the current table, of some *BigWorld* type,
		// and in a DataSection under propName
		bool bind( const std::string& propName, const std::string& typeName,
				int databaseLength );
		PY_AUTO_METHOD_DECLARE( RETOK, bind,
				ARG( std::string, ARG( std::string, OPTARG( int, 255, END ) ) ) );

		// this method lets createUserTypeMapping figure out its return value
		PropertyMappingPtr getResult();

		PyObject * pyGetAttribute( const char * attr )
		{
			PY_GETATTR_STD();
			return PyObjectPlus::pyGetAttribute( attr );
		}

	private:
		struct Context
		{
			CompositePropertyMappingPtr	pCompositeProp;
			Namer						namer;
			DataSectionPtr				pDefaultValue;

			Context( CompositePropertyMappingPtr pProp,
					const Namer& inNamer,
					const std::string& propName, bool isTable,
					DataSectionPtr pDefault ) :
				pCompositeProp(pProp), namer( inNamer, propName, isTable ),
				pDefaultValue(pDefault)
			{}

			Context( CompositePropertyMappingPtr pProp,
					const Namer& inNamer,
					DataSectionPtr pDefault ) :
				pCompositeProp(pProp), namer( inNamer ),
				pDefaultValue(pDefault)
			{}

		};
		std::stack<Context> tables_;

		const Context& curContext()
		{
			MF_ASSERT( tables_.size() );
			return tables_.top();
		}
	};

	// forward decl
    /* huangshanquan  2009-12-28 change begin CSOL-7399*/
	PropertyMappingPtr createPropertyMapping( const Namer& namer,
			const std::string& propName, const DataType& type,
			int databaseLength, DataSectionPtr pDefaultValue,
			bool isNameIndex = false ,
			int IndexType = -1);
    /* huangshanquan 2009-12-28 change end*/

	bool PyUserTypeBinder::bind( const std::string& propName,
			const std::string& typeName, int databaseLength )
	{
		const Context& context = curContext();
		// see what the default value for this element is then
		// this should logically be done by CompositePropertyMapping,
		// but its addChild method wants a constructed ProperyMapping
		// (the default value for a subtable is always the empty sequence)
		DataSectionPtr pPropDefault;
		if (context.pDefaultValue)
			pPropDefault = context.pDefaultValue->openSection( propName );

		// Create type object, before we can create property mapping.
		std::stringstream typeStrm;
		typeStrm << "<Type>" << typeName << "</Type>";
		XMLSectionPtr pXMLTypeSection =
			XMLSection::createFromStream( std::string(),  typeStrm );
		DataSectionPtr pTypeSection( pXMLTypeSection.getObject() );
		DataTypePtr pType = DataType::buildDataType( pTypeSection );
		if (pType.exists())
		{
			// add it to the table on the 'top' of the stack
			context.pCompositeProp->addChild(
				createPropertyMapping( context.namer, propName, *pType,
										databaseLength, pPropDefault ) );
		}
		else
		{
			ERROR_MSG( "PyUserTypeBinder::bind: Invalid type name %s.\n",
						typeName.c_str() );
			PyErr_SetString( PyExc_TypeError, typeName.c_str() );
		}
		return pType.exists();
	}

	void PyUserTypeBinder::beginTable( const std::string& propName )
	{
		const Context& context = curContext();
		DataSectionPtr pPropDefault;
		if (context.pDefaultValue)
			pPropDefault = context.pDefaultValue->openSection( propName );
		CompositePropertyMappingPtr pChild( new CompositePropertyMapping( propName ) );
		PropertyMappingPtr pSequence( new SequenceMapping( context.namer,
			propName, pChild ) );
        context.pCompositeProp->addChild( pSequence );
		tables_.push( Context( pChild, context.namer, propName, true,
							   pPropDefault ) );
	}

	bool PyUserTypeBinder::endTable()
	{
		bool isOK = ( tables_.size() > 1 );
		if ( isOK )
			tables_.pop();
		else
			PyErr_SetString( PyExc_RuntimeError, "No matching beginTable." );
		return isOK;
	}

	PropertyMappingPtr PyUserTypeBinder::getResult()
	{
		if ( tables_.size() == 1 )
			return PropertyMappingPtr( curContext().pCompositeProp.getObject() );
		else
			return PropertyMappingPtr();
	}

	typedef SmartPointer<PyUserTypeBinder> PyUserTypeBinderPtr;

}

PY_TYPEOBJECT( PyUserTypeBinder )

PY_BEGIN_METHODS( PyUserTypeBinder )
	PY_METHOD( beginTable )
	PY_METHOD( endTable )
	PY_METHOD( bind )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( PyUserTypeBinder )
PY_END_ATTRIBUTES()

namespace
{
	PropertyMappingPtr createUserTypeMapping( const Namer& namer,
			const std::string& propName, const UserDataType& type,
			DataSectionPtr pDefaultValue )
	{
		const std::string& moduleName = type.moduleName();
		const std::string& instanceName = type.instanceName();

		// get the module
		PyObjectPtr pModule(
				PyImport_ImportModule(
					const_cast< char * >( moduleName.c_str() ) ),
				PyObjectPtr::STEAL_REFERENCE );
		if (!pModule)
		{
			ERROR_MSG( "createUserTypeMapping: unable to import %s from %s\n",
					instanceName.c_str(), moduleName.c_str() );
			PyErr_Print();
			return PropertyMappingPtr();
		}

		// get the object
		PyObjectPtr pObject(
				PyObject_GetAttrString( pModule.getObject(),
					const_cast<char*>( instanceName.c_str() ) ) );
		if (!pObject)
		{
			ERROR_MSG( "createUserTypeMapping: unable to import %s from %s\n",
					instanceName.c_str(), moduleName.c_str() );
			PyErr_Print();
			return PropertyMappingPtr();
		}
		else
		{
			Py_DECREF( pObject.getObject() );
		}

		// create our binder class
		PyUserTypeBinderPtr pBinder(
			new PyUserTypeBinder( namer, propName, pDefaultValue )
								  , true );

		// call the method
		PyObjectPtr pResult(
				PyObject_CallMethod( pObject.getObject(), "bindSectionToDB",
					"O", pBinder.getObject() ) );

		if (!pResult)
		{
			ERROR_MSG( "createUserTypeMapping: (%s.%s) bindSectionToDB failed\n",
					moduleName.c_str(), instanceName.c_str() );
			PyErr_Print();
			return PropertyMappingPtr();
		}

		// pull out the result
		PropertyMappingPtr pTypeMapping = pBinder->getResult();
		if (!pTypeMapping.exists())
		{
			ERROR_MSG( "createUserTypeMapping: (%s.%s) bindSectionToDB missing "
						"endTable\n", moduleName.c_str(), instanceName.c_str() );
		}

		return pTypeMapping;
	}

	template <class DATATYPE>
	ClassMapping* createClassTypeMapping( const Namer& classNamer,
			const std::string& propName, const DATATYPE& type,
			int databaseLength, DataSectionPtr pDefaultValue )
	{
		ClassMapping* pClassMapping =
			new ClassMapping( classNamer, propName, type.allowNone() );

		Namer childNamer( classNamer, propName, false );
		// Don't add extra level of naming if we are inside a sequence.
		const Namer& namer = (propName.empty()) ? classNamer : childNamer;

		const ClassDataType::Fields& fields = type.getFields();

		for ( ClassDataType::Fields::const_iterator i = fields.begin();
				i < fields.end(); ++i )
		{
			if (i->isPersistent_)
			{
				DataSectionPtr pPropDefault = getChildDefaultSection(
						*(i->type_), i->name_, pDefaultValue );
				PropertyMappingPtr pMemMapping =
					createPropertyMapping( namer, i->name_, *(i->type_),
											i->dbLen_, pPropDefault );
				if (pMemMapping.exists())
					pClassMapping->addChild( pMemMapping );
			}
		}

		return pClassMapping;
	}

	PropertyMappingPtr createPropertyMapping( const Namer& namer,
			const std::string& propName, const DataType& type,
			int databaseLength, DataSectionPtr pDefaultValue,
            /* huangshanquan  2009-12-28 change begin CSOL-7399*/
			bool isNameIndex,
			int IndexType)
            /* huangshanquan 2009-12-28 change end*/

	{
		PropertyMappingPtr pResult;

		const SequenceDataType* pSeqType;
		const ClassDataType* pClassType;
		const UserDataType* pUserType;
		const FixedDictDataType* pFixedDictType;

		if ((pSeqType = dynamic_cast<const SequenceDataType*>(&type)))
		{
			// TODO: Is it possible to specify the default for an ARRAY or TUPLE
			// to have more than one element:
			//		<Default>
			//			<item> 1 </item>
			//			<item> 2 </item>
			//			<item> 3 </item>
			//		</Default>
			// Currently, when adding a new ARRAY/TUPLE to an entity, all
			// existing entities in the database will default to having no
			// elements. When creating a new entity, it will default to the
			// specified default.
			// TODO: For ARRAY/TUPLE of FIXED_DICT, there is an additional
			// case where a new property is added to the FIXED_DICT. Then
			// all existing elements in the database will need a default value
			// for the new property. Currently we use the default value for the
			// child type (as opposed to array type) so we don't have to handle
			// complicated cases where default value for the array doesn't
			// have the same number of elements as the existing arrays in the
			// database.

			// Use the type default value for the child. This is
			// mainly useful when adding new properties to an ARRAY of
			// FIXED_DICT. The new column will have the specified default value.
			const DataType& childType = pSeqType->getElemType();
			DataSectionPtr 	pChildDefault = childType.pDefaultSection();

			PropertyMappingPtr childMapping =
				createPropertyMapping( Namer( namer, propName, true ),
										std::string(), childType,
										databaseLength, pChildDefault );

			if (childMapping.exists())
				pResult = new SequenceMapping( namer, propName, childMapping,
												pSeqType->getSize() );
		}
		else if ((pFixedDictType = dynamic_cast<const FixedDictDataType*>(&type)))
		{
			pResult = createClassTypeMapping( namer, propName,
							*pFixedDictType, databaseLength, pDefaultValue );
		}
		else if ((pClassType = dynamic_cast<const ClassDataType*>(&type)))
		{
			pResult = createClassTypeMapping( namer, propName, *pClassType,
							    		databaseLength, pDefaultValue );
		}
		else if ((pUserType = dynamic_cast<const UserDataType*>(&type)))
		{
			pResult = createUserTypeMapping( namer, propName, *pUserType,
											pDefaultValue );
			if (!pResult.exists())
				// Treat same as parse error i.e. stop DbMgr. This is to prevent
				// altering tables (particularly dropping columns) due to
				// a simple scripting error.
				throw std::runtime_error( "Unable to bind USER_TYPE to database" );
		}
		else
		{
			const MetaDataType * pMetaType = type.pMetaDataType();
			MF_ASSERT(pMetaType);
			const char * metaName = pMetaType->name();
            /* huangshanquan  2009-12-28 change begin CSOL-7399*/
			if (strcmp( metaName, "UINT8" ) == 0)
				pResult = new NumMapping< uint8 >( namer, propName, pDefaultValue, isNameIndex, IndexType );
			else if (strcmp( metaName, "UINT16" ) == 0)
				pResult = new NumMapping< uint16 >( namer, propName, pDefaultValue, isNameIndex, IndexType );
			else if (strcmp( metaName, "UINT32" ) == 0)
				pResult = new NumMapping< uint32 >( namer, propName, pDefaultValue, isNameIndex, IndexType );
			else if (strcmp( metaName, "UINT64" ) == 0)
				pResult = new NumMapping< uint64 >( namer, propName, pDefaultValue, isNameIndex, IndexType );
			else if (strcmp( metaName, "INT8" ) == 0)
				pResult = new NumMapping< int8 >( namer, propName, pDefaultValue, isNameIndex, IndexType );
			else if (strcmp( metaName, "INT16" ) == 0)
				pResult = new NumMapping< int16 >( namer, propName, pDefaultValue, isNameIndex, IndexType );
			else if (strcmp( metaName, "INT32" ) == 0)
				pResult = new NumMapping< int32 >( namer, propName, pDefaultValue, isNameIndex, IndexType );
			else if (strcmp( metaName, "INT64" ) == 0)
				pResult = new NumMapping< int64 >( namer, propName, pDefaultValue, isNameIndex, IndexType );
			else if (strcmp( metaName, "FLOAT32" ) == 0)
				pResult = new NumMapping< float >( namer, propName, pDefaultValue, isNameIndex, IndexType );
			else if (strcmp( metaName, "FLOAT64" ) == 0)
				pResult = new NumMapping< double >( namer, propName, pDefaultValue, isNameIndex, IndexType );
			else if (strcmp( metaName, "VECTOR2" ) == 0)
				pResult = new VectorMapping<Vector2,2>( namer, propName, pDefaultValue );
			else if (strcmp( metaName, "VECTOR3" ) == 0)
				pResult = new VectorMapping<Vector3,3>( namer, propName, pDefaultValue );
			else if (strcmp( metaName, "VECTOR4" ) == 0)
				pResult = new VectorMapping<Vector4,4>( namer, propName, pDefaultValue );
			else if (strcmp( metaName, "STRING" ) == 0)
				pResult = new StringMapping( namer, propName, isNameIndex, databaseLength, pDefaultValue, IndexType );
            /* huangshanquan 2009-12-28 change end*/
			else if (strcmp( metaName, "PYTHON" ) == 0)
				pResult = new PythonMapping( namer, propName, isNameIndex, databaseLength, pDefaultValue );
			else if (strcmp( metaName, "BLOB" ) == 0)
				pResult = new BlobMapping( namer, propName, isNameIndex, databaseLength, pDefaultValue );
			else if (strcmp( metaName, "PATROL_PATH" ) == 0)
				pResult = new PatrolPathMapping( namer, propName, pDefaultValue );
		}

		if (!pResult.exists())
			WARNING_MSG( "createPropertyMapping: don't know how to "
					"persist property %s of type %s to the database; ignoring.\n",
					propName.c_str(), type.typeName().c_str() );

		return pResult;
	}

	// Upgrades the database from 1.7 to 1.8
	void upgradeDatabase( MySql& db, uint32 version,
			const EntityDefs& entityDefs )
	{
		MF_ASSERT( version == DBMGR_VERSION_1_7 );

		const std::string& defaultNameProperty =
				entityDefs.getDefaultNameProperty();
		if (defaultNameProperty.empty())
			throw std::runtime_error( "Upgrade failed because "
				"dbMgr/nameProperty is not set. dbMgr/nameProperty must be set "
				"to the name property that was used to create this database." );

		INFO_MSG( "Upgrading database tables from 1.7 to 1.8\n" );

		MySqlTransaction transaction(db);

		// Update version number stored in database.
		INFO_MSG( "\tUpdating version number\n" );
		std::stringstream	ss;
		ss << "UPDATE bigworldInfo SET version=" << DBMGR_CURRENT_VERSION;
		transaction.execute( ss.str() );

		// Add the idx column to bigworldTableMetadata
		INFO_MSG( "\tAdding idx column to bigworldTableMetadata\n" );
		transaction.execute( "ALTER TABLE bigworldTableMetadata ADD COLUMN idx "
				"INT NOT NULL" );
		// Set the index column correctly.
		transaction.execute( "UPDATE bigworldTableMetadata SET idx=1 WHERE "
				"col='id'" );
		transaction.execute( "UPDATE bigworldTableMetadata SET idx=3 WHERE "
				"col='parentID'" );
		// The name column is a bit more tricky because sub-tables may have
		// a column with the same name as the default name property. Only
		// top level tables have an index on the name property though.
		if ( entityDefs.getNumEntityTypes() > 0 )
		{
			std::stringstream stmtStrm;
			stmtStrm << " UPDATE bigworldTableMetadata SET idx=2 WHERE "
					"col='sm_" << defaultNameProperty << "' AND FIELD(tbl";

			for ( EntityTypeID typeID = 0; typeID < entityDefs.getNumEntityTypes();
					++typeID )
			{
				const EntityDescription& entDes =
					entityDefs.getEntityDescription( typeID );
				stmtStrm << ", '"TABLE_NAME_PREFIX"_" << entDes.name() << "'";
			}
			stmtStrm << ") > 0";

			transaction.execute( stmtStrm.str() );
		}

		// Previously the name index was always called "nameIndex". In
		// 1.8 the name of the index is made up of the column name + "Index".
		// This is to ease changing of the name property because we could
		// temporarily have two columns being the name column as we add the
		// new name column before deleting the old name column. Also looking
		// to the future where we may support multiple name indexes.
		INFO_MSG( "\tRecreating indexes using new index names\n" );
		for ( EntityTypeID typeID = 0; typeID < entityDefs.getNumEntityTypes();
				++typeID )
		{
			const EntityDescription& entDes =
					entityDefs.getEntityDescription( typeID );
			const DataDescription* pDataDes =
					entDes.findProperty( defaultNameProperty );
			if (pDataDes != NULL)
			{
				std::string tblName( TABLE_NAME_PREFIX"_" );
				tblName += entDes.name();

				DEBUG_MSG( "Recreating index for table %s\n", tblName.c_str() );

				try
				{
					transaction.execute( "ALTER TABLE " + tblName +
										" DROP INDEX nameIndex" );
				}
				catch (std::exception& e )
				{
					ERROR_MSG( "upgradeDatabase: %s\n", e.what() );
				}
				ITableCollector::ColumnInfo colInfo;
				// createEntityTableIndex() needs to know whether it is a
				// VARCHAR or not. Index column must be some sort of string
				// so we can use the databaseLength to fudge this.
				colInfo.typeStr = (pDataDes->databaseLength() < 1<<16) ?
									"VARCHAR" : "BLOB";
				colInfo.indexType = ITableCollector::IndexTypeName;
				createEntityTableIndex( transaction, tblName,
						"sm_" + defaultNameProperty, colInfo );
			}
		}

		transaction.commit();
	}
}

// -----------------------------------------------------------------------------
// Section: helper functions
// -----------------------------------------------------------------------------

void initEntityTables( MySql& con, const EntityDefs& entityDefs,
						uint32 version )
{
	TypeMappings	types;
	createEntityPropMappings( types, entityDefs, TABLE_NAME_PREFIX );

	if (version != DBMGR_CURRENT_VERSION)
		upgradeDatabase( con, version, entityDefs );

	TableInitialiser ti( con, Database::instance().getDefsSyncMode() );

	for ( EntityTypeID ent = 0; ent < entityDefs.getNumEntityTypes(); ++ent )
	{
		// Skip over "invalid" entity types e.g. client-only entities.
		if (!entityDefs.isValidEntityType( ent ))
			continue;

		const PropertyMappings& properties = types[ent];
		const EntityDescription& entDes = entityDefs.getEntityDescription(ent);

		std::string tblName = TABLE_NAME_PREFIX"_" + entDes.name();
		ITableCollector::NameToColInfoMap cols;

		for ( PropertyMappings::const_iterator prop = properties.begin();
				prop != properties.end(); ++prop )
		{
			(*prop)->createTables( ti, cols );
		}

		ti.addType( ent, entDes.name() );

		// Add id columns
		ITableCollector::ColumnInfo& idCol = cols[ ID_COLUMN_NAME ];
		idCol.typeStr = ID_COLUMN_TYPE_NAME;
		idCol.indexType = ITableCollector::IndexTypePrimary;

		ti.requireTable( tblName, cols );

		if (properties.size() == 0)
			INFO_MSG( "%s does not have persistent properties.\n",
					  entDes.name().c_str() );
	}

	ti.garbageCollect( con );
	ti.commit();

	if (!ti.isSynced())
		throw std::runtime_error( "Tables not in sync with defs." );
}

namespace
{
	std::string buildCommaSeparatedListOfColumns( const PropertyMappings& properties,
			int * numColumnsFound, const std::string& suffix = std::string() )
	{
		std::string list;
		int numColumns = 0;

		for ( PropertyMappings::const_iterator i = properties.begin(); i != properties.end(); ++i )
		{
			PropertyMappingPtr p = *i;
			if (p->numColumns())
			{
				if (numColumns)
					list += ',';
				list += p->getColumnNames( suffix );
				numColumns += p->numColumns();
			}
		}

		*numColumnsFound = numColumns;

		return list;
	}

	std::string buildCommaSeparatedQuestionMarks( int num )
	{
		std::string list;
		for ( int i=0; i<num; i++ )
		{
			if (i) list += ',';
			list += '?';
		}
		return list;
	}

	std::string createInsertStatement( const std::string& tbl,
			const PropertyMappings& properties, bool putID = false )
	{
		std::string stmt = "INSERT INTO " + tbl + " (";
		if (putID) stmt += "id,";
		int numColumns;
		stmt += buildCommaSeparatedListOfColumns( properties, &numColumns );
		if (putID)
		{
			if (numColumns == 0)
				stmt.resize(stmt.length() - 1);	// remove comma
			++numColumns;
		}
		stmt += ") VALUES (";

		stmt += buildCommaSeparatedQuestionMarks( numColumns );
		stmt += ')';

		return stmt;
	}

	std::string createUpdateStatement( const std::string& tbl,
			const PropertyMappings& properties )
	{
		std::string stmt = "UPDATE " + tbl + " SET ";
		int numColumns;
		stmt += buildCommaSeparatedListOfColumns( properties, &numColumns, "=?" );
		if (!numColumns)
			return std::string();
		stmt += " WHERE id=?";

		return stmt;
	}

	std::string createSelectStatement( const std::string& tbl,
			const PropertyMappings& properties, const std::string& where, bool getID )
	{
		std::string stmt = "SELECT ";
		if (getID) stmt += "id,";
		int numColumns;
		stmt += buildCommaSeparatedListOfColumns( properties, &numColumns );
		if (getID && !numColumns)
			stmt.resize(stmt.length() - 1);	// remove comma
		stmt += " FROM " + tbl;

		if (where.length())
			stmt += " WHERE " + where;

		return stmt;
	}

	std::string createDeleteStatement( const std::string& tbl,
			const std::string& where, bool usingNameNotID )
	{
		std::string stmt = "DELETE FROM " + tbl;

		if (where.length())
			stmt += " WHERE " + where;

		return stmt;
	}

	void addPropertyBindings( MySqlBindings& b,
		const PropertyMappings& properties )
	{
		for ( PropertyMappings::const_iterator i = properties.begin();
				i != properties.end(); ++i )
		{
			PropertyMappingPtr pMapping = *i;
			pMapping->addToBindings( b );
		}
	}
}

// -----------------------------------------------------------------------------
// Section: MySqlEntityTypeMapping
// -----------------------------------------------------------------------------

MySqlEntityTypeMapping::MySqlEntityTypeMapping( MySql& con,
		const std::string& tbl, EntityTypeID typeID,
		const EntityDescription& desc, PropertyMappings& properties,
		const std::string& nameProperty, bool isForMigration ) :
	description_( desc ),
	properties_( properties ),
	insertStmt_( con, createInsertStatement( tbl, properties ) ),
	pUpdateStmt_( NULL ),
	pSelectNamedStmt_( NULL ),
	pSelectNamedForIDStmt_( NULL ),
	pSelectIDForNameStmt_( NULL ),
	selectIDForIDStmt_( con, "SELECT id FROM " + tbl + " WHERE id=?" ),
	pSelectIDStmt_( NULL ),
	deleteIDStmt_( con, createDeleteStatement( tbl, "id=?", false ) ),
	propsNameMap_(),
	pInsertWithIDStmt_( NULL ),
	pSelectNextIDStmt_( NULL ),
	pNameProp_(0)
{
	MySqlBindings b;

	if (properties_.size() > 0)
	{
		for ( PropertyMappings::iterator prop = properties_.begin();
			  prop != properties_.end(); ++prop )
		{
			(*prop)->prepareSQL( con );
		}

		// Create prop name to PropertyMapping map
		for ( PropertyMappings::const_iterator i = properties_.begin();
			i != properties_.end(); ++i )
		{
			PropertyMappingPtr pMapping = *i;
			propsNameMap_[pMapping->propName()] = pMapping.getObject();
		}

		// Cache fixed cell properties so we don't have to always go look for them.
		fixedCellProps_[ CellPositionIdx ] = getPropMapByName( "position" );
		fixedCellProps_[ CellDirectionIdx ] = getPropMapByName( "direction" );
		fixedCellProps_[ CellSpaceIDIdx ] = getPropMapByName( "spaceID" );
		// Cache name property (if we have one)
		if (!nameProperty.empty())
		{
			PropertyMapping* pNameProp = getPropMapByName( nameProperty );
			if (pNameProp)
				pNameProp_ = static_cast< StringLikeMapping* >( pNameProp );
		}

		std::string updateStmtStr = createUpdateStatement( tbl, properties );
		if (!updateStmtStr.empty())
		{
			pUpdateStmt_.reset( new MySqlStatement( con, updateStmtStr ) );
			pSelectIDStmt_.reset( new MySqlStatement( con,
					createSelectStatement( tbl, properties, "id=?", false ) ) );
		}
		// ELSE table has no columns apart from id. e.g. entity with only
		// ARRAY properties.

		pInsertWithIDStmt_.reset( new MySqlStatement( con,
			createInsertStatement( tbl, properties, true ) ) );

		b.clear();
		addPropertyBindings( b, properties_ );
		insertStmt_.bindParams( b );

		if (pSelectIDStmt_.get())
			pSelectIDStmt_->bindResult( b );

		if (pUpdateStmt_.get())
		{
			b << id_;
			pUpdateStmt_->bindParams( b );
		}

		b.clear();
		b << id_;

		if (pSelectIDStmt_.get())
			pSelectIDStmt_->bindParams( b );

		addPropertyBindings( b, properties_ );
		pInsertWithIDStmt_->bindParams( b );

		if (!isForMigration)
		{
			if (pNameProp_)
			{
				pSelectNamedStmt_.reset( new MySqlStatement( con,
					createSelectStatement( tbl, properties, "sm_"+nameProperty+"=?", true ) ) );

				pSelectNamedForIDStmt_.reset( new MySqlStatement( con,
					"SELECT id FROM " + tbl + " WHERE sm_" + nameProperty + "=?" ) );

				pSelectIDForNameStmt_.reset( new MySqlStatement( con,
					"SELECT sm_" + nameProperty + " FROM " + tbl + " WHERE id=?" ) );

				b.clear();
				pNameProp_->addToBindings(b);

				pSelectNamedStmt_->bindParams( b );
				pSelectNamedForIDStmt_->bindParams( b );
				pSelectIDForNameStmt_->bindResult( b );

				b.clear();
				b << id_;

				pSelectIDForNameStmt_->bindParams( b );
				pSelectNamedForIDStmt_->bindResult( b );

				addPropertyBindings( b, properties_ );
				pSelectNamedStmt_->bindResult( b );
			}
		}
		else
		{	// This type mapping is being used for migration. Prepare special
			// migration statements
			pSelectNextIDStmt_.reset( new MySqlStatement( con,
				createSelectStatement( tbl, properties, "id>? LIMIT 1", true ) ) );

			b.clear();
			b << id_;
			pSelectNextIDStmt_->bindParams( b );

			addPropertyBindings( b, properties_ );
			pSelectNextIDStmt_->bindResult( b );
		}
	}
	else
	{
		for ( int i = 0; i < NumFixedCellProps; ++i )
			fixedCellProps_[i] = 0;
	}

	b.clear();
	b << id_;
	selectIDForIDStmt_.bindParams( b );
	selectIDForIDStmt_.bindResult( b );

	deleteIDStmt_.bindParams( b );

	std::stringstream strmStmt;
	strmStmt << "SELECT typeID FROM bigworldEntityTypes WHERE bigworldID=";
	strmStmt << typeID;
	MySqlStatement stmtGetID( con, strmStmt.str() );
	b.clear();
	b << mappedType_;
	stmtGetID.bindResult( b );
	MySqlTransaction t( con );
	t.execute( stmtGetID );
	stmtGetID.fetch();
	t.commit();
}

/**
 *	This method checks that the entity with the given DBID exists in the
 *	database.
 *
 *	@return	True if it exists.
 */
bool MySqlEntityTypeMapping::checkExists( MySqlTransaction& transaction,
	DatabaseID dbID )
{
	id_ = dbID;
	transaction.execute( selectIDForIDStmt_ );

	return selectIDForIDStmt_.resultRows() > 0;
}

/**
 * This method returns the database ID of the entity given its name.
 *
 * @param	transaction	Transaction to use when querying the database.
 * @param	name		The name of the entity.
 * @return	The database ID of the entity. Returns 0 if the entity
 * doesn't exists or if the entity doesn't have a name index.
 */
DatabaseID MySqlEntityTypeMapping::getDbID( MySqlTransaction& transaction,
	const std::string& name )
{
	if (pNameProp_)
	{
		pNameProp_->setString( name );
		transaction.execute( *pSelectNamedForIDStmt_ );

		if (pSelectNamedForIDStmt_->resultRows())
		{
			pSelectNamedForIDStmt_->fetch();
			return id_;
		}
	}

	return 0;
}

/**
 * This method returns the name of the entity given its database Id.
 *
 * @param	transaction	Transaction to use when querying the database.
 * @param	dbID		The database ID of the entity.
 * @param	name		Returns the name of the entity here.
 * @return	True if the entity exists and have a name.
 */
bool MySqlEntityTypeMapping::getName( MySqlTransaction& transaction,
	DatabaseID dbID, std::string& name )
{
	if (pNameProp_)
	{	// Entity has name
		id_ = dbID;
		transaction.execute( *pSelectIDForNameStmt_ );

		if (pSelectIDForNameStmt_->resultRows())
		{
			pSelectIDForNameStmt_->fetch();
			pNameProp_->getString(name);
			return true;
		}
	}

	return false;
}

/**
 *	This method retrieves the entity data into MySQL bindings by database ID.
 *
 *	@param	transaction	Transaction to use when updating the database.
 *	@param	dbID		The database ID of the entity.
 *	@param	name		Output parameter. Name of the entity.
 *	@return	True if the entity exists.
 */
bool MySqlEntityTypeMapping::getPropsByID( MySqlTransaction& transaction,
										   DatabaseID dbID, std::string& name )
{
	bool isOK = true;
	if (pSelectIDStmt_.get())
	{
		id_ = dbID;
		isOK = this->getPropsImpl( transaction, *pSelectIDStmt_ );
		if (isOK && pNameProp_)
			pNameProp_->getString(name);
	}

	return isOK;
}

/**
 *	This method retrieves the entity data into MySQL bindings by name.
 *
 *	@param	transaction	Transaction to use when updating the database.
 *	@param	name		Name of the entity.
 *	@return	The database ID of the entity if successful. Returns 0 if the
 *	entity doesn't exist or doesn't have a name index.
 */
DatabaseID MySqlEntityTypeMapping::getPropsByName( MySqlTransaction& transaction,
	const std::string& name )
{
	if (pNameProp_)
	{
		pNameProp_->setString( name );
		if (this->getPropsImpl( transaction, *pSelectNamedStmt_.get() ))
			return id_;
	}

	return 0;
}

/**
 *	Common implementation for getPropsByID() and getPropsByName().
 */
bool MySqlEntityTypeMapping::getPropsImpl( MySqlTransaction& transaction,
	MySqlStatement& stmt )
{
	transaction.execute( stmt );

	bool hasData = stmt.resultRows();
	if (hasData)
	{
		stmt.fetch();

		// Get child tables data
		for ( PropertyMappings::iterator i = properties_.begin();
			i != properties_.end(); ++i )
		{
			(*i)->getTableData( transaction, id_ );
		}
	}

	return hasData;
}

bool MySqlEntityTypeMapping::deleteWithID( MySqlTransaction & t, DatabaseID id )
{
	id_ = id;
	t.execute( deleteIDStmt_ );
	if (t.affectedRows() > 0)
	{
		MF_ASSERT( t.affectedRows() == 1 );
		// Delete any child table entries
		for ( PropertyMappings::iterator i = properties_.begin();
				i != properties_.end(); ++i )
		{
			(*i)->deleteChildren( t, id );
		}
		return true;
	}

	return false;
	// TODO: Check that deleting the highest id is ok in whatever kind
	// of tables we are using ... the MySQL docs are not very clear on this
	// issue and we really don't want to reuse DatabaseIDs.
}


/**
 *	This visitor class is used by MySqlEntityTypeMapping::streamToBound()
 *	to read entity data from a stream.
 */
class MySqlBindStreamReader : public IDataDescriptionVisitor
{
	MySqlEntityTypeMapping& entityTypeMap_;
	BinaryIStream & stream_;

public:
	MySqlBindStreamReader( MySqlEntityTypeMapping& entityTypeMap,
		   BinaryIStream & stream ) :
		entityTypeMap_( entityTypeMap ),
		stream_( stream )
	{}

	// Override method from IDataDescriptionVisitor
	virtual bool visit( const DataDescription& propDesc );
};


bool MySqlBindStreamReader::visit( const DataDescription& propDesc )
{
	// __kyl__ TODO: Get rid of name lookup - use entity extras?
	PropertyMapping* pPropMap =
		entityTypeMap_.getPropMapByName(propDesc.name());

	// TRACE_MSG( "TypeMapStreamVisitor::readProp: property=%s\n", propDesc.name().c_str() );

	if (pPropMap)
	{
		pPropMap->streamToBound( stream_ );
	}
	else
	{
		// This is probably because the property is non-persistent.
		// Read from stream and discard
		WARNING_MSG( "MySqlBindStreamReader::visit: Ignoring value for "
					"property %s\n", propDesc.name().c_str() );
		propDesc.createFromStream( stream_, false );
	}

	return true;
}


/**
 *	This visitor class is used by MySqlEntityTypeMapping::streamToBound()
 *	to write entity data to a stream.
 */
class MySqlBindStreamWriter : public IDataDescriptionVisitor
{
	MySqlEntityTypeMapping& entityTypeMap_;
	BinaryOStream & stream_;

public:
	MySqlBindStreamWriter( MySqlEntityTypeMapping& entityTypeMap,
		   BinaryOStream & stream ) :
		entityTypeMap_( entityTypeMap ),
		stream_( stream )
	{}

	// Override method from IDataDescriptionVisitor
	virtual bool visit( const DataDescription& propDesc );
};

bool MySqlBindStreamWriter::visit( const DataDescription& propDesc )
{
	PropertyMapping* pPropMap =
		entityTypeMap_.getPropMapByName( propDesc.name() );
	// TRACE_MSG( "TypeMapStreamWriter::visit: property=%s\n", propDesc.name().c_str() );
	if (pPropMap)
	{
		pPropMap->boundToStream( stream_ );
	}
	else
	{
		// This is probably because the property is non-persistent.
		// Write default value into stream.
		WARNING_MSG( "MySqlBindStreamWriter::writeProp: Making up default "
					"value for property %s\n", propDesc.name().c_str() );
		PyObjectPtr pDefaultVal = propDesc.pInitialValue();
		propDesc.addToStream( pDefaultVal.getObject(), stream_, false );
	}

	return true;
}

/**
 *	This method transfers the entity's data from the stream into MySQL bindings.
 */
void MySqlEntityTypeMapping::streamToBound( BinaryIStream& strm )
{
	MySqlBindStreamReader	visitor( *this, strm );
	description_.visit(
		EntityDescription::BASE_DATA | EntityDescription::CELL_DATA |
		EntityDescription::ONLY_PERSISTENT_DATA, visitor );

	// Set data bindings for non-configurable cell properties.
	if (description_.hasCellScript())
	{
		for ( int i = 0; i < NumFixedCellProps; ++i )
		{
			fixedCellProps_[i]->streamToBound( strm );
		}
	}
}

/**
 *	This method transfers the data already in MySQL bindings into the stream.
 *	Entity data must be already set in bindings e.g. via getPropsByID() or
 *	getPropsByName().
 *
 *	If pPasswordOverride is not NULL and if the entity has as STRING or BLOB
 *	property called "password", pPasswordOverride will be written to the
 *	stream instead of the value of the "password" property.
 */
void MySqlEntityTypeMapping::boundToStream( BinaryOStream& strm,
	const std::string* pPasswordOverride  )
{
	if (pPasswordOverride)
	{
		// Set bound value of "password" property. But only if it is a STRING
		// or BLOB property.
		PropertyMapping* pPropMap = this->getPropMapByName( "password" );
		if (dynamic_cast<StringMapping*>(pPropMap) ||
			dynamic_cast<BlobMapping*>(pPropMap) )
		{
			dynamic_cast<StringLikeMapping*>(pPropMap)->setValue(
					*pPasswordOverride );
		}
	}

	MySqlBindStreamWriter visitor( *this, strm );
	description_.visit(
		EntityDescription::CELL_DATA | EntityDescription::BASE_DATA |
		EntityDescription::ONLY_PERSISTENT_DATA, visitor );

	// Write non-configurable cell properties into stream.
	if (description_.hasCellScript())
	{
		for ( int i = 0; i < NumFixedCellProps; ++i )
		{
			fixedCellProps_[i]->boundToStream( strm );
		}
	}
}

/**
 * This method insert a new entity into the database.
 * Entity data must be already set in bindings e.g. via streamToBound().
 *
 * @param	transaction	Transaction to use when updating the database.
 * @return	The database ID of the newly inserted entity.
 */
DatabaseID MySqlEntityTypeMapping::insertNew( MySqlTransaction& transaction )
{
	transaction.execute( insertStmt_ );
	DatabaseID dbID = transaction.insertID();

	// Update child tables.
	for ( PropertyMappings::iterator i = properties_.begin();
		i != properties_.end(); ++i )
	{
		(*i)->updateTable( transaction, dbID );
	}

	return dbID;
}

/**
 * This method updates an existing entity's properties in the database.
 * Entity data must be already set in bindings e.g. via streamToBound().
 *
 * @param	transaction	Transaction to use when updating the database.
 * @param	dbID		The database ID if the entity.
 * @return	Returns true if the entity was updated. False if the entity
 * doesn't exist.
 */
bool MySqlEntityTypeMapping::update( MySqlTransaction& transaction,
									 DatabaseID dbID )
{
	// Update main table for this entity.
	bool isOK = true;
	if (pUpdateStmt_.get())
	{
		id_ = dbID;
		transaction.execute( *pUpdateStmt_ );

		// __kyl__ (24/5/2005) Can't actually use transaction.affectedRows()
		// because if the new entity data is the same as the old entity data,
		// then transaction.affectedRows() will return 0.
		const char* infoStr = transaction.info();
		// infoStr should be "Rows matched: %d Changed: %d Warnings: %d"
		if ( (infoStr) && (atol( infoStr + 14 ) == 1) )
		{
			// Update child tables.
			for ( PropertyMappings::iterator i = properties_.begin();
				i != properties_.end(); ++i )
			{
				(*i)->updateTable( transaction, dbID );
			}
		}
		else
		{
			isOK = false;
		}
	}

	return isOK;
}

/**
 * This method insert a new entity with a specific DBID into the database.
 * Entity data must be already set in bindings e.g. via streamToBound().
 *
 * @param	transaction	Transaction to use when updating the database.
 * @param	dbID		The DBID of the entity.
 * @return	The database ID of the newly inserted entity.
 */
bool MySqlEntityTypeMapping::insertNewWithID( MySqlTransaction& transaction,
	DatabaseID dbID )
{
	bool isInserted;
	try
	{
		id_ = dbID;
		transaction.execute( *pInsertWithIDStmt_ );
		// Will throw exception if duplicate ID.

		// Update child tables.
		for ( PropertyMappings::iterator i = properties_.begin();
			i != properties_.end(); ++i )
		{
			(*i)->updateTable( transaction, dbID );
		}
		isInserted = true;
	}
	catch (std::exception & e)
	{
		isInserted = false;
	}

	return isInserted;
}

/**
 *	This method retrieves the entity data whose DBID is next one after the given
 * 	DBID.
 *
 *	@param	transaction	Transaction to use when updating the database.
 *	@param	dbID		The DBID of the "previous" entity.
 *	@return	The database ID of the retrieved entity if successful. 0 we have
 * 	reached the end of the table.
 */
DatabaseID MySqlEntityTypeMapping::getNextPropsByID( MySqlTransaction& transaction,
	DatabaseID dbID )
{
	MF_ASSERT( pSelectNextIDStmt_.get() );
	id_ = dbID;
	return ( this->getPropsImpl( transaction, *pSelectNextIDStmt_ ) ) ?
				id_ : 0;
}

// -----------------------------------------------------------------------------
// Section: MySqlTypeMapping
// -----------------------------------------------------------------------------

MySqlTypeMapping::MySqlTypeMapping( MySql& con, const EntityDefs& entityDefs,
		const char * tableNamePrefix, const TypeIDSet* pTypes ) :
	mappings_(),
	pTempMappings_( NULL ), pTempConverter_( NULL ),
	stmtAddLogOn_( con, "INSERT INTO bigworldLogOns (databaseID, typeID, objectID, ip, port, salt) "
						"VALUES (?,?,?,?,?,?) ON DUPLICATE KEY UPDATE "
						"objectID=VALUES(objectID), ip=VALUES(ip), port=VALUES(port), salt=VALUES(salt)" ),
	stmtRemoveLogOn_( con, "DELETE FROM bigworldLogOns WHERE databaseID=? AND typeID=?" ),
	stmtGetLogOn_( con, "SELECT objectID, ip, port, salt FROM bigworldLogOns "
				"WHERE databaseID=? and typeID=?" ),
	stmtSetLogOnMapping_( con, "REPLACE INTO bigworldLogOnMapping (logOnName, password, typeID, recordName) "
				"VALUES (?,?,?,?)" ),
	stmtGetLogOnMapping_( con, "SELECT if(m.gmtimelimit < now() ,m.password,m.gmpassword) as password,"
                " t.bigworldID, m.recordName ,"
                " if(m.blocktime < now() ,'0','1') as block ,"
                " if(m.gmtimelimit < now() ,'0','1') as takeover"
				" FROM bigworldLogOnMapping m, bigworldEntityTypes t "
				" WHERE m.logOnName=? and m.typeID=t.typeID" ),

	boundLogOnName_(BWMySQLMaxLogOnNameLen),
	boundPassword_(BWMySQLMaxLogOnPasswordLen),
	boundRecordName_(BWMySQLMaxNamePropertyLen)
{
	// Create type mappings
	this->setEntityMappings( con, entityDefs, tableNamePrefix, pTypes );

	MySqlBindings b;

	b.clear();
	b << boundDatabaseID_ << boundTypeID_;
	stmtRemoveLogOn_.bindParams( b );
	stmtGetLogOn_.bindParams( b );

	b << boundOptObjectID_ << boundAddress_ << boundPort_ << boundSalt_;
	stmtAddLogOn_.bindParams( b );

	b.clear();
	b << boundOptObjectID_ << boundAddress_ << boundPort_ << boundSalt_;
	stmtGetLogOn_.bindResult( b );

	b.clear();
	b << boundLogOnName_ << boundPassword_ << boundTypeID_ << boundRecordName_;
	stmtSetLogOnMapping_.bindParams( b );

	b.clear();
	b << boundPassword_ << boundTypeID_ << boundRecordName_ << isblock_ << istakeover_;
	stmtGetLogOnMapping_.bindResult( b );
	b.clear();
	b << boundLogOnName_;
	stmtGetLogOnMapping_.bindParams( b );
}

MySqlTypeMapping::~MySqlTypeMapping()
{
	this->clearMappings();
}

/**
 *	This method replaces the current entity mappings with new mappings based
 * 	on the given entity definitions.
 */
void MySqlTypeMapping::setEntityMappings( MySql& connection,
	const EntityDefs& entityDefs, const char * tableNamePrefix,
	const TypeIDSet* pTypes )
{
	this->clearMappings();

	createEntityMappings( mappings_, entityDefs, tableNamePrefix, connection,
							pTypes );
}

/**
 *	This method replaces the clears current entity mappings.
 */
void MySqlTypeMapping::clearMappings()
{
	for ( MySqlEntityTypeMappings::iterator i = mappings_.begin();
			i < mappings_.end(); ++i )
	{
		delete *i;
	}
	mappings_.clear();
}

/**
 *	This method sets the temporary entity mappings which enables entity
 * 	data to be written to the original tables as well as temporary entity
 * 	tables. Used for migration during updater activity.
 *
 * 	@param	entityDefs	The entity definitions for the temporary entities.
 * 	@param	typeIDMappings	The mappings between the current type IDs to the
 * 	type IDs of the temporary entities.
 */
void MySqlTypeMapping::setTemporaryMappings( MySql& connection,
	const EntityDefs& entityDefs, const std::string& tableNamePrefix,
	ITemporaryMappingsConverter& converter )
{
	this->clearTemporaryMappings();

	pTempConverter_ = &converter;
	pTempMappings_ = new MySqlEntityTypeMappingMap();

	TypeMappings	entityPropMappings;
	createEntityPropMappings( entityPropMappings, entityDefs,
							tableNamePrefix );

	for ( EntityTypeID typeID = 0; typeID < mappings_.size(); ++typeID )
	{
		EntityTypeID newtypeID = pTempConverter_->getTempTypeID( typeID );
		if (newtypeID != INVALID_TYPEID)
		{
			// No duplicates allowed.
			MF_ASSERT( pTempMappings_->find( newtypeID ) == pTempMappings_->end() );

			const EntityDescription& entDes =
				entityDefs.getEntityDescription( newtypeID );
			(*pTempMappings_)[newtypeID] = new MySqlEntityTypeMapping( connection,
									tableNamePrefix+"_"+entDes.name(), newtypeID,
									entDes, entityPropMappings[newtypeID],
									entityDefs.getNameProperty( newtypeID ) );
		}
	}
}

/**
 *	This method clears the temporary mappings added by setTemporaryMappings.
 */
void MySqlTypeMapping::clearTemporaryMappings()
{
	if (pTempMappings_)
	{
		for ( MySqlEntityTypeMappingMap::iterator i = pTempMappings_->begin();
				i != pTempMappings_->end(); ++i )
		{
			delete i->second;
		}
		delete pTempMappings_;
	}
	pTempMappings_ = NULL;
	pTempConverter_ = NULL;
}

bool MySqlTypeMapping::hasNameProp( EntityTypeID typeID ) const
{
	return mappings_[typeID]->hasNameProp();
}

DatabaseID MySqlTypeMapping::getEntityDbID( MySqlTransaction& transaction,
	EntityTypeID typeID, const std::string& name )
{
	return mappings_[typeID]->getDbID( transaction, name );
}

bool MySqlTypeMapping::getEntityName( MySqlTransaction& transaction,
	EntityTypeID typeID, DatabaseID dbID, std::string& name )
{
	return mappings_[typeID]->getName( transaction, dbID, name );
}

bool MySqlTypeMapping::checkEntityExists( MySqlTransaction& transaction,
	EntityTypeID typeID, DatabaseID dbID )
{
	return mappings_[typeID]->checkExists( transaction, dbID );
}

bool MySqlTypeMapping::getEntityToBound( MySqlTransaction& transaction,
	EntityDBKey& ekey )
{
	if (ekey.dbID)
	{
		return mappings_[ekey.typeID]->getPropsByID( transaction,
			ekey.dbID, ekey.name );
	}
	else
	{
		ekey.dbID = mappings_[ekey.typeID]->getPropsByName( transaction,
			ekey.name );
		return ekey.dbID != 0;
	}
}

void MySqlTypeMapping::boundToStream( EntityTypeID typeID,
	BinaryOStream& entityDataStrm, const std::string* pPasswordOverride )
{
	mappings_[typeID]->boundToStream( entityDataStrm, pPasswordOverride );
}

bool MySqlTypeMapping::deleteEntityWithID(
		MySqlTransaction& t, EntityTypeID typeID, DatabaseID dbID )
{
	bool isOK = mappings_[typeID]->deleteWithID( t, dbID );

	if (pTempMappings_)
	{
		// Delete from temp tables
		EntityTypeID tempTypeID = pTempConverter_->getTempTypeID( typeID );
		if (tempTypeID != INVALID_TYPEID)
		{
			// This operation may fail because the entity may not exist in the
			// temp table yet.
			(*pTempMappings_)[tempTypeID]->deleteWithID( t, dbID );
		}
	}

	return isOK;
}

/**
 *	This method stores all the data necessary for setLogOnMapping() into
 *	our bindings.
 */
void MySqlTypeMapping::logOnMappingToBound( const std::string& logOnName,
	const std::string& password, EntityTypeID typeID,
	const std::string& recordName )
{
	boundLogOnName_.setString( logOnName );
	boundPassword_.setString( password );
	boundTypeID_ = mappings_[typeID]->getDatabaseTypeID();
	boundRecordName_.setString( recordName );
}

/**
 *	This method adds a log on mapping into our log on mapping table.
 */
void MySqlTypeMapping::setLogOnMapping( MySqlTransaction& transaction  )
{
	transaction.execute( stmtSetLogOnMapping_ );
}

/**
 *	This method gets the log on mapping for the given logOnName.
 */
bool MySqlTypeMapping::getLogOnMapping( MySqlTransaction& t, const std::string& logOnName,
		std::string& password, EntityTypeID& typeID, std::string& recordName,
       int &isblock , int &istakeover)
{
	boundLogOnName_.setString( logOnName );
	t.execute( stmtGetLogOnMapping_ );
	if (stmtGetLogOnMapping_.fetch())
	{
		if (boundPassword_.isNull())
			password.clear();
		else
        {
			password = boundPassword_.getString();
        }

		typeID = boundTypeID_;
        isblock = isblock_;
        istakeover =istakeover_;
		recordName = boundRecordName_.getString();
		return true;
	}
	else
	{
		return false;
	}
}

bool MySqlTypeMapping::getLogOnRecord( MySqlTransaction& t, EntityTypeID typeID,
		DatabaseID dbID, EntityMailBoxRef& ref )
{
	boundTypeID_ = mappings_[typeID]->getDatabaseTypeID();
	boundDatabaseID_ = dbID;
	t.execute( stmtGetLogOn_ );

	if (stmtGetLogOn_.fetch())
	{
		ref.id = *boundOptObjectID_.get();
		ref.addr.ip = htonl( *boundAddress_.get() );
		ref.addr.port = htons( *boundPort_.get() );
		ref.addr.salt = *boundSalt_.get();
		return true;
	}

	return false;
}

/**
 *	This method sets the MySQL bindings for an entity data update operation.
 */
void MySqlTypeMapping::streamToBound( EntityTypeID typeID, DatabaseID dbID,
									  BinaryIStream& entityDataStrm )
{
	mappings_[typeID]->streamToBound( entityDataStrm );

	// Set temp tables bindings
	if (pTempMappings_)
	{
		EntityTypeID tempTypeID = pTempConverter_->getTempTypeID( typeID );
		if (tempTypeID != INVALID_TYPEID)
		{
			pTempConverter_->setTempMapping( *(*pTempMappings_)[tempTypeID],
				typeID, dbID, *mappings_[typeID] );
		}
	}
}

/**
 *	This method sets the MySQL bindings for a base mailbox add/update operation.
 */
void MySqlTypeMapping::baseRefToBound( const EntityMailBoxRef& baseRef )
{
	boundOptObjectID_.set( baseRef.id );
	boundAddress_.set( ntohl( baseRef.addr.ip ) );
	boundPort_.set( ntohs( baseRef.addr.port ) );
	boundSalt_.set( baseRef.addr.salt );
}

DatabaseID MySqlTypeMapping::newEntity( MySqlTransaction& transaction,
									    EntityTypeID typeID )
{
	DatabaseID dbID = mappings_[typeID]->insertNew( transaction );

	if (pTempMappings_)
	{
		// Insert into temp tables
		EntityTypeID tempTypeID = pTempConverter_->getTempTypeID( typeID );
		if (tempTypeID != INVALID_TYPEID)
		{
			(*pTempMappings_)[tempTypeID]->insertNewWithID( transaction, dbID );
		}
	}

	return dbID;
}

bool MySqlTypeMapping::updateEntity( MySqlTransaction& transaction,
									 EntityTypeID typeID, DatabaseID dbID )
{
	bool isOK = mappings_[typeID]->update( transaction, dbID );

	if (isOK && pTempMappings_)
	{	// Update temp tables
		EntityTypeID tempTypeID = pTempConverter_->getTempTypeID( typeID );
		if (tempTypeID != INVALID_TYPEID)
		{
			// This may fail if entity doesn't exist in the temp tables yet.
			// But we don't care.
			(*pTempMappings_)[tempTypeID]->update( transaction, dbID );
		}
	}

	return isOK;
}

/**
 *	This methods stores new base mailbox for the given entity in the database.
 * 	If a base mailbox for the entity already exists, it is updated.
 *	Base mailbox data must be already set in bindings e.g. via streamToBound().
 *
 *	This method may be called from another thread. Do not use globals.
 */
void MySqlTypeMapping::addLogOnRecord( MySqlTransaction& transaction,
									   EntityTypeID typeID, DatabaseID dbID )
{
	boundTypeID_ = mappings_[typeID]->getDatabaseTypeID();
	boundDatabaseID_ = dbID;
	transaction.execute( stmtAddLogOn_ );
}

/**
 *	This method remove the base mailbox for a given entity from the database.
 *
 *	This method may be called from another thread. Do not use globals.
 */
void MySqlTypeMapping::removeLogOnRecord( MySqlTransaction& t,
		EntityTypeID typeID, DatabaseID dbID )
{
	boundTypeID_ = mappings_[typeID]->getDatabaseTypeID();
	boundDatabaseID_ = dbID;
	t.execute( stmtRemoveLogOn_ );
}



// -----------------------------------------------------------------------------
// Section: free functions
// -----------------------------------------------------------------------------
void createEntityPropMappings( TypeMappings& types,
							const EntityDefs& entityDefs,
							const std::string& tableNamePrefix )
{
	// walk through the properties of each entity type and create a property mapping
	// class instance for it... we'll use these to generate the statements that we'll
	// need later on
	for ( EntityTypeID ent = 0; ent < entityDefs.getNumEntityTypes(); ++ent )
	{
		types.push_back( PropertyMappings() );

		if (!entityDefs.isValidEntityType( ent ))
			// Note that even for invalid entity types we need an blank entry
			// in types since we access by offset into the array.
			continue;

		PropertyMappings& properties = types.back();
		const EntityDescription& entDes = entityDefs.getEntityDescription(ent);

		const std::string& nameProperty = entityDefs.getNameProperty(ent);
		Namer namer( entDes.name(), tableNamePrefix );
		for ( unsigned int i = 0; i < entDes.propertyCount(); ++i )
		{
			DataDescription * dataDes = entDes.property( i );
			if (dataDes->isPersistent())
			{
				DataType * dataType = dataDes->dataType();

				bool isNameProperty = !nameProperty.empty() &&
							(nameProperty == dataDes->name());

                /* huangshanquan  2009-12-28 change begin CSOL-7399*/
				if(dataDes->IndexType() != -1)
					isNameProperty = true;
                
				PropertyMappingPtr prop =
					createPropertyMapping( namer, dataDes->name(),
							*dataType, dataDes->databaseLength(),
							getDefaultSection( *dataDes ), isNameProperty,
                            dataDes->IndexType());
                /* huangshanquan 2009-12-28 change end*/
				if (prop.exists())
					properties.push_back( prop );
			}
		}

		if (entDes.hasCellScript())
		{
			//setup proper default value for position and direction
			Vector3 defaultVec(0,0,0);

			DataSectionPtr pDefaultValue = new XMLSection( "position" );
			pDefaultValue->setVector3(defaultVec);

			properties.push_back(
				new VectorMapping<Vector3,3>( namer, "position", pDefaultValue ) );

			pDefaultValue = new XMLSection( "direction" );
			pDefaultValue->setVector3(defaultVec);
			properties.push_back(
				new VectorMapping<Vector3,3>( namer, "direction", pDefaultValue ) );

			pDefaultValue = new XMLSection( "spaceID" );
			pDefaultValue->setInt( 0 );
			properties.push_back(
				new NumMapping<int32>( namer, "spaceID", pDefaultValue ) );
		}
	}
}

/**
 * 	This function frees all the MySqlEntityTypeMapping in typeMappings.
 */
void destroyEntityPropMappings( MySqlEntityTypeMappings& typeMappings )
{
	for ( MySqlEntityTypeMappings::iterator i = typeMappings.begin();
			i != typeMappings.end(); ++i )
	{
		delete *i;
	}
	typeMappings.clear();
}

/**
 * 	This function creates MySqlEntityTypeMappings from the given
 * 	PropertyMappings.
 */
void createEntityMappings( MySqlEntityTypeMappings& entityMappings,
	TypeMappings& propMappings, const EntityDefs& entityDefs,
	const std::string& tableNamePrefix, MySql& connection,
	const TypeIDSet* pTypes )
{
	for ( EntityTypeID typeID = 0; typeID < entityDefs.getNumEntityTypes(); ++typeID )
	{
		if (entityDefs.isValidEntityType( typeID ) &&
			(!pTypes || (pTypes && (pTypes->find( typeID ) != pTypes->end()))))
		{
			const EntityDescription& entDes =
				entityDefs.getEntityDescription( typeID );
			entityMappings.push_back( new MySqlEntityTypeMapping( connection,
									tableNamePrefix + "_" + entDes.name(),
									typeID, entDes, propMappings[typeID],
									entityDefs.getNameProperty( typeID ),
									pTypes ) );
		}
		else
		{
			entityMappings.push_back( NULL );
		}
	}
}

/**
 * 	This function creates MySqlEntityTypeMappings from the given entity
 * 	definitions.
 */
void createEntityMappings( MySqlEntityTypeMappings& entityMappings,
	const EntityDefs& entityDefs, const std::string& tableNamePrefix,
	MySql& connection, const TypeIDSet* pTypes )
{
	// TODO: Don't create prop mappings for types not in pTypes.
	TypeMappings propMappings;
	createEntityPropMappings( propMappings, entityDefs, tableNamePrefix );

	createEntityMappings( entityMappings, propMappings, entityDefs,
		tableNamePrefix, connection, pTypes );
}
