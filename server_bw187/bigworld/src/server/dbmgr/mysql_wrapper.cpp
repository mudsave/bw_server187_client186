/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "mysql_wrapper.hpp"

#include "mysql_prepared.hpp"
#include "mysql_notprepared.hpp"

#include "cstdmf/binary_stream.hpp"

#include <mysql/errmsg.h>

DECLARE_DEBUG_COMPONENT( 0 );

// -----------------------------------------------------------------------------
// Section: class MySqlTypeTraits
// -----------------------------------------------------------------------------
const char * MySqlTypeTraits<int8>::colTypeStr = "TINYINT";
const char * MySqlTypeTraits<uint8>::colTypeStr = "TINYINT UNSIGNED";
const char * MySqlTypeTraits<int16>::colTypeStr = "SMALLINT";
const char * MySqlTypeTraits<uint16>::colTypeStr = "SMALLINT UNSIGNED";
const char * MySqlTypeTraits<int32>::colTypeStr = "INT";
const char * MySqlTypeTraits<uint32>::colTypeStr = "INT UNSIGNED";
const char * MySqlTypeTraits<int64>::colTypeStr = "BIGINT";
const char * MySqlTypeTraits<uint64>::colTypeStr = "BIGINT UNSIGNED";
const char * MySqlTypeTraits<int>::colTypeStr = "INT";
const char * MySqlTypeTraits<float>::colTypeStr = "FLOAT";
const char * MySqlTypeTraits<double>::colTypeStr = "DOUBLE";

// -----------------------------------------------------------------------------
// Section: class MySql
// -----------------------------------------------------------------------------

MySql::MySql( const MySql::ConnectionInfo& connectInfo ) :
// initially set all pointers to 0 so that we can see where we got to
// should an error occur
	sql_(0),
	inTransaction_(false)
{
	try
	{
		sql_ = mysql_init( 0 );
		if (!sql_)
			this->throwError( this->sql_ );
#ifdef MESSAGE_LOGGER
		if (!mysql_real_connect( sql_, connectInfo.host.c_str(),
				connectInfo.username.c_str(), connectInfo.password.c_str(),
				connectInfo.database.c_str(), 0, NULL, CLIENT_MULTI_STATEMENTS ))
#else
		if (!mysql_real_connect( sql_, connectInfo.host.c_str(),
				connectInfo.username.c_str(), connectInfo.password.c_str(),
				connectInfo.database.c_str(), 0, NULL, 0 ))
#endif
			this->throwError( this->sql_ );
	}
	catch (std::exception& e)
	{
		ERROR_MSG( "MySql::MySql: %s\n", e.what() );
		if (sql_) mysql_close( sql_ );
		throw;
	}
}

MySql::~MySql()
{
	MF_ASSERT( !inTransaction_ );
	mysql_close( sql_ );
}

namespace MySqlUtils
{
	inline unsigned int getErrno( MYSQL* connection )
	{
		return mysql_errno( connection );
	}

	inline unsigned int getErrno( MYSQL_STMT* statement )
	{
		return mysql_stmt_errno( statement );
	}
}

template <class MYSQLOBJ>
void MySql::throwError( MYSQLOBJ* failedObj )
{
	unsigned int mySqlErrno = MySqlUtils::getErrno( failedObj );
	switch (mySqlErrno)
	{
		case ER_LOCK_DEADLOCK:
		case ER_LOCK_WAIT_TIMEOUT:
			throw MySqlRetryTransactionException( failedObj );
			break;
		case CR_SERVER_GONE_ERROR:
		case CR_SERVER_LOST:
			{
				MySqlError serverGone( failedObj );
				this->onFatalError( serverGone );
				throw serverGone;
			}
			break;
		default:
			throw MySqlError( failedObj );
			break;
	}
}

void MySql::execute( const std::string& statement, BinaryOStream * resdata, bool isrCcreateIndex )
{
	//DEBUG_MSG("MySqlTransaction::execute: %s\n", statement.c_str());
	if (mysql_real_query( this->sql_, statement.c_str(), statement.length() ))
    {
        if( !isrCcreateIndex )
        {
		    this->throwError( this->sql_ );
        }
        else
        {
	        ERROR_MSG( "Create index error : %s\n", getLastError() );
            return;
        }
    }
	MYSQL_RES * result = mysql_store_result( this->sql_ );
	if (result)
	{
		if (resdata != NULL)
		{
			int nrows = mysql_num_rows( result );
			int nfields = mysql_num_fields( result );
			(*resdata) << nrows << nfields;
			MYSQL_ROW arow;
			while ((arow = mysql_fetch_row( result )) != NULL)
			{
				unsigned long *lengths = mysql_fetch_lengths(result);
				for (int i = 0; i < nfields; i++)
				{
					if (arow[i] == NULL)
						(*resdata) << "NULL";
					else
						resdata->appendString(arow[i],lengths[i]);
				}
			}
		}
		mysql_free_result( result );
	}
}

void MySql::execute( MySqlUnPrep::Statement& statement )
{
	statement.giveResult( 0 );
	std::string query = statement.getQuery( this->sql_ );
	// DEBUG_MSG("MySqlTransaction::execute: %s\n", query.c_str());
	if (mysql_real_query( this->sql_, query.c_str(), query.length() ))
		this->throwError( this->sql_ );
	MYSQL_RES * result = mysql_store_result( this->sql_ );
	statement.giveResult( result );
}

void MySql::execute( MySqlPrep::Statement& stmt )
{
	if (mysql_stmt_execute( stmt.get() ))
		this->throwError( stmt.get() );

	if (mysql_stmt_store_result( stmt.get() ))
		this->throwError( stmt.get() );
}

// This is the non-exception version of execute().
int MySql::query( const std::string & statement )
{
	int errorNum =
			mysql_real_query( sql_, statement.c_str(), statement.length() );

	if ((errorNum == CR_SERVER_GONE_ERROR) || (errorNum == CR_SERVER_LOST))
	{
		this->onFatalError( MySqlError( sql_ ) );
	}

	return errorNum;
}


/**
 * 	This function returns the list of table names that matches the specified
 * 	pattern.
 */
void MySql::getTableNames( std::vector<std::string>& tableNames,
							const char * pattern )
{
	tableNames.clear();

	MYSQL_RES * pResult = mysql_list_tables( sql_, pattern );
	if (pResult)
	{
		tableNames.reserve( mysql_num_rows( pResult ) );

		MYSQL_ROW row;
		while ((row = mysql_fetch_row( pResult )) != NULL)
		{
			unsigned long *lengths = mysql_fetch_lengths(pResult);
			tableNames.push_back( std::string( row[0], lengths[0] ) );
		}
		mysql_free_result( pResult );
	}
}

// -----------------------------------------------------------------------------
// Section: class MySqlLockedTables
// -----------------------------------------------------------------------------

MySqlLockedTables::MySqlLockedTables( MySql& sql, const std::string& tables ) :
	lock_( sql )
{
	lock_.sql.execute( "LOCK TABLES " + tables );
}

MySqlLockedTables::~MySqlLockedTables()
{
	lock_.sql.execute( "UNLOCK TABLES" );
}

// -----------------------------------------------------------------------------
// Section: class MySqlBuffer
// -----------------------------------------------------------------------------
void MySqlBuffer::printTruncateError( unsigned long size,
			unsigned long capacity )
{
	// Can't do this in the header file due to dependency on
	// DECLARE_DEBUG_COMPONENT()
	ERROR_MSG( "MySqlBuffer::set: truncating data of size %u to %u\n",
		size, capacity );
}
