#ifndef MESSAGE_MYSQL_HPP
#define MESSAGE_MYSQL_HPP
#include "resmgr/bwresource.hpp"
#include <string>
#include <map>
#include <vector>
#include "../../dbmgr/mysql_wrapper.hpp"
#include "../../dbmgr/mysql_prepared.hpp"

extern "C" void * mysqlThreadEntry(void *arg);

extern "C" std::string getTimeString();

extern "C" void * recoverToMysqlThread(void *arg);

extern "C" void string_replace(std::string & strBig, const std::string & strsrc, const std::string &strdst);

/*
 * write log to mysql
 */
class MessageMysql
{

public:
	MessageMysql();

	~MessageMysql();

	struct ConnectionInfo
	{
		std::string	host;
	    std::string	username;
	    std::string	password;
	    std::string	database;
	};
    
    typedef struct sqlsmt 
    {
        std::string sql;
        std::string index;
        int numb;
    }SQLSMT;

    typedef struct fielddef
    {
        std::string name;
        std::string def;
        int type;
    }FIELDDEF;


    typedef  std::map<std::string, std::string>  STRING_MAP;

    typedef  std::map<int, SQLSMT>  INT_MAP;
    typedef  std::vector<std::string>  STR_VEC;
    typedef  std::vector<int>  INT_VEC;

    typedef  std::map<std::string, STR_VEC>  VECSTR_MAP;

    typedef  std::map<std::string, int64>  INT64_MAP;

    typedef  std::vector<FIELDDEF>  FIELDDEF_VEC;

    typedef struct tablesinfostruct
    {
        std::string index;
        std::string name;
        std::string engine;
        std::string charset;
        std::string append;
        std::string inserthesad;
        FIELDDEF_VEC fields;
    }TABLESINFOSTRUCT;

    typedef  std::map<std::string,TABLESINFOSTRUCT>  TABLES_MAP;

	bool init(const std::string &apppath);

    bool putMessage(int messagePriority,
                    const std::string &mesg,
                    const std::string &componentName,
                    const std::string &time,
                    const std::string &componentIp);

    bool isconnect();

    int diviString(const std::string &text, const std::string &div, STR_VEC &vecstr);

    int checkTables();

    int checkColumns(const std::string &tablename, const std::string &index);
    
    int checkAppendColumns(const std::string &tablename, const std::string &index);

    void addAppendInfo(std::string &smt,int &alternumb,STRING_MAP &oldAppend,STRING_MAP &newAppend);
    
    bool createTables(const std::string &index, std::string tablename = "");
    
    static int intervalbrobe_;

    static int maxstatemen_;

    const FIELDDEF *getColumnDefine(const std::string &index, const std::string &colname);

private:

    MySql *dbcon_;

    std::string useprefix_;

    TABLES_MAP   tablesmap_;

	pthread_t	tid_;

    char        *queryBuff_;

    bool        isconnect_;

    bool        autopad_;

    char splitprefix_;

    std::string splitdate_;

    /*
     * 0:not check 1:only check 2:check and update
     */
    int         updatetable_;      

};
#endif // MESSAGE_MYSQL_HPP
