#include "Python.h"		// See http://docs.python.org/api/includes.html
#include "message_mysql.hpp"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <sys/types.h>
#include <linux/unistd.h>
#include <errno.h>
#include <sys/syscall.h>
#include "common/des.h"


DECLARE_DEBUG_COMPONENT( 0 )

const static char *g_dbinfo = "dbinfo.xml";

static MessageMysql::INT_MAP g_preparestatement;
static pthread_mutex_t g_sqlStatementLock;
static MessageMysql::INT64_MAP g_indexstatment;
static int64 g_preparesnumber = 0; 
static pthread_mutex_t g_workmutex;
static pthread_cond_t  g_workcond;
static std::string g_appdir = "";
static FILE *g_errstatementfd = NULL,
            *g_statementfd = NULL;
static MySql::ConnectionInfo g_connectionInfo;

int MessageMysql::intervalbrobe_ = 30;
int MessageMysql::maxstatemen_ = 10;

#if defined(_syscall0)
_syscall0(pid_t, gettid)
#elif defined(__NR_gettid)
static inline pid_t gettid(void)
{
	return syscall(__NR_gettid);
}
#else
#warning "use pid as tid"
static inline pid_t gettid(void)
{
	return getpid();
}
#endif
#define MAXBUFSIZE 2048
/**
 *	Constructor.
 */
MessageMysql::MessageMysql():
    dbcon_(NULL),
    useprefix_(""),
    tid_(0),
    queryBuff_(NULL),
    isconnect_(false),
    autopad_(true),
    splitprefix_(':'),
    splitdate_(""),
    updatetable_(1)
{
}


/**
 *	Destructor.
 */
MessageMysql::~MessageMysql()
{
    INFO_MSG( "Finish MessageMysql::~MessageMysql.(%d,%d)\n", 
               (unsigned int)gettid(), 
               pthread_self());

    if(isconnect_)
    {
        INFO_MSG( "clear buff.\n");

        while(g_preparestatement.size())
        {
            sleep(5);
        }

        if(dbcon_)
        {
            delete dbcon_;
        } 

        if(tid_)
            pthread_cancel(tid_);

        pthread_mutex_destroy(&g_sqlStatementLock);
        pthread_mutex_destroy(&g_workmutex);
        pthread_cond_destroy(&g_workcond);

        if(g_errstatementfd)
            fclose(g_errstatementfd);

        if(g_statementfd)
            fclose(g_statementfd);

        if(queryBuff_)
            free(queryBuff_);
    }
}

/**
 * 	read message_logger.conf file and connect mysql
 */
bool MessageMysql::init(const std::string &apppath)
{ 
    queryBuff_ = (char *)malloc(MAXBUFSIZE);

	if (apppath != "")
    {
        g_appdir = apppath;
    }
    else
	{
        char proc[64];
        char buf[MAXBUFSIZE];
        int count;

        sprintf(proc, "/proc/%d/exe", (unsigned int)getpid());

        count = readlink(proc, buf, MAXBUFSIZE);

        INFO_MSG( "readlink(%s:%s)\n",proc,buf);
        if (count < 0 || count >= MAXBUFSIZE)
        {
            ERROR_MSG( "Error get app path (%s:%s) .... \n",proc,buf);
            return false;
        }

        buf[count] = '\0';
		g_appdir = buf;

        size_t pos = g_appdir.rfind('/');

        if(pos == std::string::npos)
        {
            ERROR_MSG( "Error get app path (%s:%s) ....\n",proc,buf);
            return false;
        }

        g_appdir = g_appdir.substr(0, pos);

	}

    BWResource::addPath(g_appdir.c_str());

    bool fileexist = BWResource::fileExists(g_dbinfo);

    if(!fileexist)
    {
        ERROR_MSG( "MessageMysql::init: No %s file found\n", g_dbinfo);
        return false;
    }
    else
    {
        DataSectionPtr rootsec = BWResource::openSection(g_dbinfo);

        if(!rootsec)
        {
            ERROR_MSG( "MessageMysql::init: %s  read root node ....\n", g_dbinfo);
            return false;
        }
        else
        {
            //TABLES_MAP   tablesmap_;
            DataSectionPtr secdbinfo = rootsec->findChild("dbinfo");
            useprefix_  = rootsec->readString("useprefix", "");
            autopad_    = rootsec->readBool("autopad", true);
            splitprefix_  = rootsec->readString("splitprefix", ":")[0];
            splitdate_    = rootsec->readString("splitdate", "|");
            updatetable_    = rootsec->readInt("updatetable", 1);

            //if(splitprefix_ == splitdate_[0] )
            //{
            //    ERROR_MSG( "MessageMysql::init: splitprefix eq splitdate (%c) ....\n", splitdate_);
            //    return false;
            //}

            MessageMysql::intervalbrobe_ = rootsec->readInt("intervalbrobe", 30);
            MessageMysql::maxstatemen_ = rootsec->readInt("maxstatemen", 10);

            if(!secdbinfo)
            {
                ERROR_MSG( "MessageMysql::init: error read dbinfo node ....\n");
                return false;
            }

            g_connectionInfo.host = secdbinfo->readString("host");
            g_connectionInfo.database = secdbinfo->readString("database", "");
            g_connectionInfo.username = secdbinfo->readString("username", "");
            g_connectionInfo.password = secdbinfo->readString("password", "");
            //使用加密工具，解密才是正确密码 LuoCD add 2009-08-26 
            if( g_connectionInfo.password != "")
            {
                //CPwdDesTool des;
                CDES des;
                INFO_MSG("messloger before des decrypt ,password = %s ....\n",g_connectionInfo.password.c_str());
                g_connectionInfo.password = des.DecryptString(g_connectionInfo.password);
                //INFO_MSG("messloger after des decrypt ,password = %s ....\n",g_connectionInfo.password.c_str());
            }
            // LuoCD add 2009-08-26  end
            DataSectionPtr sectables = rootsec->findChild("tables");

            if(!sectables)
            {
                ERROR_MSG( "MessageMysql::init: error read tables node ....\n");
                return false;
            }

            for(int i = 0; i < sectables->countChildren(); i++)
            {
                TABLESINFOSTRUCT tabinfo;

                DataSectionPtr subsection = sectables->openChild(i);
                tabinfo.index = subsection->readString("index", "");
                tabinfo.name = subsection->readString("name", "");
                tabinfo.engine = subsection->readString("ENGINE", "");
                tabinfo.charset = subsection->readString("CHARSET", "");
                tabinfo.append = subsection->readString("APPEND", "");

                DataSectionPtr secfield = subsection->findChild("fields");

                if(secfield)
                {
                    if(secfield->countChildren() <= 0)
                    {
                        ERROR_MSG( "MessageMysql::init: error read fields not child node ....\n");
                        continue;
                    }

                    std::string insstr = "insert into `" + tabinfo.name + "` ("; 

                    for(int j = 0; j < secfield->countChildren(); j++)
                    {
                        FIELDDEF def;
                        def.name = secfield->openChild(j)->sectionName();
                        def.def = secfield->openChild(j)->asString();

                        if(def.def.find("int") != std::string::npos)
                            def.type = 1;
                        else if(def.def.find("float") != std::string::npos)
                            def.type = 2;
                        else if(def.def.find("double") != std::string::npos)
                            def.type = 3;
                        else
                            def.type = 0; 

                        tabinfo.fields.push_back(def);

                        if(j == 0)
                            insstr = insstr + "`";
                        else
                            insstr = insstr + ",`";

                        insstr = insstr + def.name;
                        insstr = insstr + "`";
                    }

                    insstr = insstr + ") values ";

                    tabinfo.inserthesad = insstr;

                    tablesmap_.insert(TABLES_MAP::value_type(tabinfo.index, tabinfo));

                    g_indexstatment.insert(INT64_MAP::value_type(tabinfo.index, -1));

                }
                else
                {
                    ERROR_MSG("MessageMysql::init: error  read fields node ....\n");
                }
            }
        }
    }

    if(g_connectionInfo.username == "")
    {
        DEBUG_MSG("in message_logger.conf not find loginname info ....\n");
        return false;
    }

    if(g_connectionInfo.database == "")
    {
        DEBUG_MSG("in message_logger.conf not find scheme info ....\n");
        return false;
    }

    if(g_connectionInfo.host == "")
    {
        DEBUG_MSG("in message_logger.conf not find ip info ....\n");
        return false;
    }

    if(useprefix_ == "")
    {
        DEBUG_MSG("in message_logger.conf not find useprefix info ....\n");
        return false;
    }

    dbcon_ = new MySql(g_connectionInfo);

    if (dbcon_->get() == NULL)
    {
        DEBUG_MSG("connect mysql error ....\n");

        delete dbcon_;

        dbcon_ = NULL;

        return false;
    }

	dbcon_->query( "SET character_set_client =  @@character_set_database" );
	dbcon_->query( "SET character_set_connection =  @@character_set_database" );
	dbcon_->query( "SET character_set_results =  @@character_set_database" );

    INFO_MSG("connect mysql ok ....\n");

    char value = 1;
    mysql_options(dbcon_->get(), MYSQL_OPT_RECONNECT, (char *)&value);

    checkTables();

    isconnect_ = true;

    pthread_mutex_init(&g_sqlStatementLock, NULL);
    pthread_mutex_init(&g_workmutex,NULL);
    pthread_cond_init(&g_workcond,NULL);

    pthread_create(&tid_, NULL, mysqlThreadEntry, dbcon_);

    pthread_detach(tid_);

    return true;
}

/**
 * write mysql record
 */
bool MessageMysql::putMessage(int messagePriority,
        const std::string &mesg,
        const std::string &componentName,
        const std::string &time,
        const std::string &componentIp)
{

    //INFO_MSG("putMessage rec (%s)\n", mesg.c_str());
    std::stringstream ss;
    if(mesg[mesg.size() - 1] == '\n')
        ss << mesg.substr(0, mesg.size() - 1);
    else
        ss << mesg ;

    std::string fistr, tablename, tabindex, rmesg;

    std::getline(ss, fistr, splitprefix_);

    if(fistr.size() < useprefix_.size())
    {
        //DEBUG_MSG( "prefix length not match (%s:%s)(%d:%d)\n", 
        //        fistr.c_str(), useprefix_.c_str()), 
        //    fistr.size(), useprefix_.size();
        return false;
    }

    if(fistr != useprefix_)
    {
        //DEBUG_MSG( "prefix not match (%s:%s)\n",fistr.c_str(), useprefix_.c_str());
        return false;
    }

    std::getline(ss, tabindex, splitprefix_);

    std::getline(ss, rmesg, splitprefix_);

    TABLES_MAP::iterator itor = tablesmap_.find(tabindex);


    if(itor == tablesmap_.end())
    {
        DEBUG_MSG("table name is not find :%s ..\n",tabindex.c_str());
        return false;
    }

    if(rmesg == "")
    {
        DEBUG_MSG("not message find :%s \n", mesg.c_str());
        return false;
    }

    if(rmesg.size() > MAXBUFSIZE )
    {
        DEBUG_MSG("Length than %d:%d\n%s \n", rmesg.size(), MAXBUFSIZE, rmesg.c_str());
        return false;
    }

    std::string insstrvalue = ""; 

    if(!queryBuff_)
        queryBuff_ = (char *)malloc(MAXBUFSIZE);
    else
        memset(queryBuff_, 0x00, MAXBUFSIZE);

    mysql_real_escape_string(dbcon_->get(), queryBuff_, rmesg.c_str(), rmesg.size());

    STR_VEC vecstr;
    diviString(queryBuff_,  splitdate_, vecstr);

    if(vecstr.size() != itor->second.fields.size() && !autopad_ )
    {
        DEBUG_MSG("mot match define:%s\n(%d:%d)\n", rmesg.c_str(), 
                vecstr.size(), 
                itor->second.fields.size());

        return false;
    }

    for(unsigned int j = 0; j < itor->second.fields.size(); j++)
    {
        if(j != 0)
            insstrvalue = insstrvalue + " ,";
        else
            insstrvalue = insstrvalue + " (";


        if(vecstr.size() > j)
        { 
            if(itor->second.fields[j].type == 0)
                insstrvalue = insstrvalue + "\"";

            insstrvalue = insstrvalue + vecstr[j];

            if(itor->second.fields[j].type == 0)
                insstrvalue = insstrvalue + "\"";
        }
        else
        {
            if(itor->second.fields[j].type == 0)
                insstrvalue = insstrvalue + "\"\"";
            else
                insstrvalue = insstrvalue + "0";

        }


    }

    insstrvalue = insstrvalue + ")";


    pthread_mutex_lock(&g_sqlStatementLock);
 
    INT64_MAP::iterator indexitor = g_indexstatment.find(tabindex);

    if(indexitor == g_indexstatment.end())
    {
        DEBUG_MSG("table index not find in g_indexstatment:(%s) ..\n",tabindex.c_str());
    }
    else
    {
        g_indexstatment.insert(INT64_MAP::value_type(tabindex, -1));
    }

    MessageMysql::INT_MAP::iterator intmapitor = g_preparestatement.find(indexitor->second);

    if(intmapitor != g_preparestatement.end())
    {
        intmapitor->second.sql = intmapitor->second.sql + "," + insstrvalue;
        if(intmapitor->second.numb >= maxstatemen_)
            indexitor->second = -1;
        else
            intmapitor->second.numb++;

    }
    else
    {
        SQLSMT sqlsmt;
        sqlsmt.sql = itor->second.inserthesad + insstrvalue;
        sqlsmt.index = tabindex;
        sqlsmt.numb = 1;
        g_preparestatement.insert(MessageMysql::INT_MAP::value_type(g_preparesnumber, sqlsmt));

        if(sqlsmt.numb >= maxstatemen_)
            indexitor->second = -1;
        else
            indexitor->second = g_preparesnumber;

        g_preparesnumber++;
    }

    pthread_mutex_unlock(&g_sqlStatementLock);

    pthread_cond_signal(&g_workcond);

    return true;
}

/*
 *	
 */
bool MessageMysql::isconnect()
{
    return isconnect_;
}

/*
 *	
 */
int MessageMysql::diviString(const std::string &text,
        const std::string &div,
        STR_VEC &vecstr)
{
	vecstr.clear();
	std::string::size_type begIdx = 0, endIdx;

	for(;;)
	{
		endIdx = text.find(div, begIdx );
		if(endIdx == std::string::npos)
		{
			endIdx = text.length();
		}

		std::string value(text, begIdx, endIdx - begIdx);

		vecstr.push_back(value.c_str());

		if(endIdx == text.length())
		{
			break;
		}
		else
		{
			begIdx = endIdx + div.size();
		}

	}

	return vecstr.size();

}

/**
 * if table not exist in mysql create it	
 */
int MessageMysql::checkTables()
{
    TABLES_MAP::iterator itor = tablesmap_.begin();

    for (; itor != tablesmap_.end(); itor++)
    {
        std::vector<std::string> tableNames;

        dbcon_->getTableNames(tableNames, itor->second.name.c_str());

        bool hastable = false;

        if(tableNames.size() > 0)
        {
            for(unsigned int i = 0; i < tableNames.size(); i++)
            {
                /*
                 * compare use toupper string 
                 */
                std::string strtbl1 = tableNames[i];
                std::string strtbl2 = itor->second.name;
                transform(strtbl1.begin(),strtbl1.end(),strtbl1.begin(),toupper);  
                transform(strtbl2.begin(),strtbl2.end(),strtbl2.begin(),toupper);  
                if(strtbl1 == strtbl2)
                {
                    hastable = true;
                    break;
                }
            }
        }

        if(hastable)
        {
            INFO_MSG( "exist table :%s ..\n",itor->second.name.c_str());

            if(updatetable_ != 0)
                //checkColumns(itor->second.name.c_str(), itor->first);
                checkAppendColumns(itor->second.name.c_str(),itor->first);

        }
        else
        {
            createTables(itor->first);
        }
    }

    return 0;
}

/*
 * create a new table
 * */
bool MessageMysql::createTables(const std::string &index, std::string tablename)
{
    TABLES_MAP::iterator itor = tablesmap_.find(index);
    if(itor == tablesmap_.end())
    {
        DEBUG_MSG("table name is not find :%s ..\n",index.c_str());
        return -1;
    }

    std::string tblname = tablename;

    if(tblname == "")
        tblname = itor->second.name;

    std::string crtablestr = "CREATE TABLE `" + tblname + \
                              "` ( `id` bigint(20) NOT NULL auto_increment,";

    for(unsigned int i = 0; i < itor->second.fields.size(); i++)
    {
        crtablestr = crtablestr + " `";
        crtablestr = crtablestr + itor->second.fields[i].name;
        crtablestr = crtablestr + "` ";
        crtablestr = crtablestr + itor->second.fields[i].def;
        crtablestr = crtablestr + ",";
    }

    if(itor->second.append != "")
        crtablestr = crtablestr + itor->second.append + ","; 

    crtablestr = crtablestr + "PRIMARY KEY  (`id`)) ENGINE="; 
    crtablestr = crtablestr + itor->second.engine;
    if(itor->second.charset != "")
    {
        crtablestr = crtablestr + " DEFAULT CHARSET=";
        crtablestr = crtablestr + itor->second.charset;
    }

    crtablestr = crtablestr + ";";

    dbcon_->execute(crtablestr.c_str());

    if(dbcon_->getLastErrorNum())
    {
        INFO_MSG( "exec sql error:\n%s\n%s\n%s\n",
                crtablestr.c_str(), 
                dbcon_->getLastErrorNum(),
                dbcon_->getLastError());
    }
    else if(tablename == "")
    {
        INFO_MSG( "ok create table:%s \n", tblname.c_str());
    }
    return true;
}

const MessageMysql::FIELDDEF *MessageMysql::getColumnDefine(const std::string &index, const std::string &colname)
{
    //INFO_MSG("Enter MessageMysql::getColumnDefine(%s)(%s)\n", index.c_str(), colname.c_str());
    TABLES_MAP::iterator itor = tablesmap_.find(index);
    if(itor == tablesmap_.end())
    {
        DEBUG_MSG("In getColumnDefine index not find :%s ..\n",index.c_str());
        return NULL;
    }

    for(unsigned int i = 0; i < itor->second.fields.size(); i++)
    {
        std::string name = itor->second.fields[i].name;
        std::string incol = colname;
        transform(name.begin(), name.end(), name.begin(), toupper);  
        transform(incol.begin(), incol.end(), incol.begin(), toupper);  
        if(name == incol)
        {
            return &itor->second.fields[i];
        }
    }

    return NULL;
}
/*
 * check new define tables  
 * */
int MessageMysql::checkColumns(const std::string &tablename, const std::string &index)
{
    INFO_MSG("Enter MessageMysql::checkColumns(%s):(%s) \n", tablename.c_str(), index.c_str());
    std::string dbstatement = "show columns from " + tablename;
    STRING_MAP oldcolvec, newcolvec;
    int alternumb = 0;

    /*
     * get mysql define
     */
    if (mysql_real_query(dbcon_->get(), dbstatement.c_str(), dbstatement.length()))
    {
        INFO_MSG("checkColumns query (%s) error:(%s) \n", dbstatement.c_str(), dbcon_->getLastError());
        return -1;
    }

    MYSQL_RES * result = mysql_store_result(dbcon_->get());
    if (result)
    {
        MYSQL_ROW arow;
        while ((arow = mysql_fetch_row(result)) != NULL)
        {
            std::string name = arow[0] == NULL ? "NULL" : arow[0];
            //transform(name.begin(), name.end(), name.begin(), toupper);  

            std::string Type = arow[1] == NULL ? "NULL" : arow[1];
            std::string Default = arow[3] == NULL ? "NULL" : arow[3];
            std::string coldefine = Type + Default;
            transform(coldefine.begin(), coldefine.end(), coldefine.begin(), toupper);  

            oldcolvec.insert(STRING_MAP::value_type(name, coldefine));

            //INFO_MSG("----(%s):(%s):(%s) \n", tablename.c_str(), name.c_str(), coldefine.c_str());
        }

        mysql_free_result(result);
    }

    /*
     * create temporary table 
     */
    std::string tmptable = "temp_" + tablename;
    std::string dtable = "DROP TABLE IF EXISTS `" + tmptable + "`";
    if (mysql_real_query(dbcon_->get(), dtable.c_str(), dtable.length()))
    {
        INFO_MSG("checkColumns drop temp table:%s error:(%s) \n", tablename.c_str(), dbcon_->getLastError());
        return -1;
    }

    createTables(index, tmptable);

    std::string tmpstatebment = "show columns from " + tmptable;
    if (mysql_real_query( dbcon_->get(), tmpstatebment.c_str(), tmpstatebment.length()))
    {
        INFO_MSG("checkColumns query (%s) error:(%s) \n", tmpstatebment.c_str(), dbcon_->getLastError());
        return -1;
    }

    MYSQL_RES * tmpresult = mysql_store_result(dbcon_->get());
    if (tmpresult)
    {
        MYSQL_ROW arow;
        while ((arow = mysql_fetch_row(tmpresult)) != NULL)
        {
            std::string name = arow[0] == NULL ? "NULL" : arow[0];
            //transform(name.begin(), name.end(), name.begin(), toupper);  

            std::string Type = arow[1] == NULL ? "NULL" : arow[1];
            std::string Default = arow[3] == NULL ? "NULL" : arow[3];
            std::string coldefine = Type + Default;
            transform(coldefine.begin(), coldefine.end(), coldefine.begin(), toupper);  

            newcolvec.insert(STRING_MAP::value_type(name, coldefine));
            //INFO_MSG("====(%s):(%s):(%s) \n", tmptable.c_str(), name.c_str(), coldefine.c_str());
        }

        mysql_free_result(tmpresult);
    }

    if (mysql_real_query(dbcon_->get(), dtable.c_str(), dtable.length()))
    {
        INFO_MSG("checkColumns drop temp table:%s error:(%s) \n", tablename.c_str(), dbcon_->getLastError());
        return -1;
    }

    /*
     * compare define 
     */
    std::string smt = "alter table " + tablename + " " ;
    STRING_MAP::iterator itor = newcolvec.begin();
    for(; itor != newcolvec.end(); ++itor)
    {
        STRING_MAP::iterator fitor = oldcolvec.find(itor->first);
        if(fitor == oldcolvec.end())
        {
            const FIELDDEF *coldef = getColumnDefine(index, itor->first);

            if(coldef == NULL)
                continue;

            //need add
            if(alternumb == 0)
            {
                smt = smt + " add ";
                smt = smt + coldef->name + " " + coldef->def;
            }
            else
            {
                smt = smt + ", add ";
                smt = smt + coldef->name + " " + coldef->def;
            }

            alternumb++;
        }
        else
        {
            if(fitor->second != itor->second)
            {
                const FIELDDEF *coldef = getColumnDefine(index, itor->first);

                if(coldef == NULL)
                    continue;

                //change
                if(alternumb == 0)
                {
                    smt = smt + " change " + coldef->name + " ";
                    smt = smt + coldef->name + " " + coldef->def;
                }
                else
                {
                    smt = smt + ", change " + coldef->name + " ";
                    smt = smt + coldef->name + " " + coldef->def;
                }
                alternumb++;
            }

            oldcolvec.erase(itor->first);
        }

    }

    STRING_MAP::iterator ditor = oldcolvec.begin();

    for(; ditor != oldcolvec.end(); ++ditor)
    {
        //delete drop column 
        if(alternumb == 0)
        {
            smt = smt + " drop column " + ditor->first + " ";
        }
        else
        {
            smt = smt + ", drop column " + ditor->first + " ";
        }

        alternumb++;
    }

    if(alternumb != 0)
    {
        //if(updatetable_ == 1)
        DEBUG_MSG("need table update :%s\n", smt.c_str());

        if(updatetable_ == 2)
        {
            if (mysql_real_query( dbcon_->get(), smt.c_str(), smt.length()))
            {
                INFO_MSG("alter query (%s) error:(%s) \n", smt.c_str(), dbcon_->getLastError());
                return -1;
            }
            DEBUG_MSG("update table ok\n");
        }
    }

    return 0;
}

/*
 * check new check Append and Columns define tables  
 * */
int MessageMysql::checkAppendColumns(const std::string &tablename, const std::string &index)
{
    INFO_MSG("Enter MessageMysql::checkAppendColumns(%s):(%s) \n", tablename.c_str(), index.c_str());
    std::string dbstatement = "show create table " + tablename;
    STRING_MAP oldcolvec, newcolvec;
    STRING_MAP oldAppend,newAppend;
    std::string createSql;
    int alternumb = 0;

    /*
     * get mysql define
     */
    if (mysql_real_query(dbcon_->get(), dbstatement.c_str(), dbstatement.length()))
    {
        INFO_MSG("checkColumns query (%s) error:(%s) \n", dbstatement.c_str(), dbcon_->getLastError());
        return -1;
    }

    MYSQL_RES * result = mysql_store_result(dbcon_->get());
    if (result)
    {
        MYSQL_ROW arow;
        if((arow = mysql_fetch_row(result)) != NULL)
        {
            createSql = arow[1]; 
            mysql_free_result(result);           
        }
        else
        {
            mysql_free_result(result);
            return -1;
        }
        
    }
    INFO_MSG("checkColumns createSql (%s)\n", createSql.c_str());
    
    std::string::size_type startPos = createSql.find_first_of("(");
    std::string::size_type endPos = createSql.find_last_of(")");
    std::string::size_type curPos = startPos+1 ;
    std::string::size_type substrStart = startPos+1 ;
    int flag = 0, collateflag = 0;
    
    while(curPos <= endPos)
    {
        if( (( createSql[curPos] == ',')&&(flag == 0)) ||(curPos == endPos))
        {
            std::string sub = createSql.substr(substrStart,curPos-substrStart);
            substrStart = curPos+1;
            std::string::size_type pos1 = sub.find_first_not_of(" \t\r\n");
            std::string::size_type pos2 = sub.find_last_not_of(" \t\r\n");
            std::string tmp = sub.substr(pos1,pos2-pos1+1);
            //INFO_MSG("tmp=====[%s]\n", tmp.c_str());
            if(tmp[0] == '`')
            {//这是列的字段
                std::string::size_type sepPos = tmp.find_first_of('`',1) ;
                std::string colName = tmp.substr(1,sepPos-1);
                std::string::size_type pos = tmp.find_first_not_of(" \t",sepPos+1);

                /* 
                 * 查找是否有字符校验语句,有的话要剔除(如：COLLATE latin1_bin) 
                 * 因为mysql数据库默认使用的校验字符集是latin1_swedish_ci，当数据库
                 * 或表指定为latin1_bin时用默认创建字符类型字段会在show create table
                 * 中显示COLLATE latin1_bin这样就导致比较配置不相同.
                 */
                std::string colDefind = tmp.substr(pos,tmp.length()-pos);
                std::string colDefindCopy = colDefind;

                transform(colDefindCopy.begin(),colDefindCopy.end(),colDefindCopy.begin(),toupper);
                std::string::size_type collaPos = colDefindCopy.find("COLLATE");

                if(collaPos != std::string::npos)
                {
                    std::string::size_type collateEndPos = colDefind.find(" ", collaPos + 1);
                    collateEndPos = colDefind.find(" ", collateEndPos + 1);
                    if(collateflag == 0)
                    {
                        WARNING_MSG("remove substring [%s]\n", colDefind.substr(collaPos, collateEndPos - collaPos + 1).c_str());
                        collateflag++;
                    }
                    colDefind.erase(collaPos, collateEndPos - collaPos + 1);
                }


                oldcolvec.insert(STRING_MAP::value_type(colName, colDefind));
            }
            else
            {//key(即index)的字段 非PRIMARY KEY的字段，可能有多条
                if((tmp.find("PRIMARY KEY"))== std::string::npos)
                {
                    std::string::size_type pos1 = tmp.find_first_of('`') ;
                    std::string::size_type pos2 = tmp.find_first_of('`',pos1+1) ;
                    std::string::size_type pos3 = tmp.find_first_of('(',pos2);
                    std::string::size_type pos4 = tmp.find_last_of(")");
                    oldAppend.insert(STRING_MAP::value_type(tmp.substr(pos1,pos2-pos1+1), tmp.substr(pos3,pos4-pos3+1)));
                }                
            }            
        }
        else if( createSql[curPos] == '(')
        {
            flag++;
        }
        else if( createSql[curPos] == ')')
        {
            flag--;
        }
        curPos++;
    }
    //oldcolvec 要删除id 信息   
    oldcolvec.erase("id");

    TABLES_MAP::iterator itor = tablesmap_.find(index);
    if(itor == tablesmap_.end())
    {
        DEBUG_MSG("table name is not find :%s ..\n",index.c_str());
        return -1;
    }
    TABLESINFOSTRUCT tblInfo = itor->second;
    unsigned int count = tblInfo.fields.size();
    for(unsigned int i = 0; i < count; i++)
    {
        std::string tmp = tblInfo.fields[i].name;
        std::string::size_type pos1 = tmp.find_first_not_of(" \t\r\n");
        std::string::size_type pos2 = tmp.find_last_not_of(" \t\r\n");
        std::string colName = tmp.substr(pos1,pos2-pos1+1);

        tmp = tblInfo.fields[i].def;
        pos1 = tmp.find_first_not_of(" \t\r\n");
        pos2 = tmp.find_last_not_of(" \t\r\n");
        std::string def = tmp.substr(pos1,pos2-pos1+1);
                
        newcolvec.insert(STRING_MAP::value_type(colName, def));
    }

    
    //append只有一条，但里面可能有多个用逗号分开的key，所以扫描一次。
    if(tblInfo.append != "")
    {
        std::string::size_type endPos = tblInfo.append.length();
        std::string::size_type curPos = 0;
        std::string::size_type substrStart = 0 ;
        int flag = 0;
        
        while(curPos <= endPos)
        {
            if( (curPos == endPos)||(( tblInfo.append[curPos] == ',')&&(flag == 0)))
            {
                std::string sub = tblInfo.append.substr(substrStart,curPos-substrStart);
                substrStart = curPos+1;
                std::string::size_type subpos1 = sub.find_first_not_of(" \t\r\n");
                std::string::size_type subpos2 = sub.find_last_not_of(" \t\r\n");
                std::string tmp = sub.substr(subpos1,subpos2-subpos1+1);
                
                std::string::size_type pos1 = tmp.find_first_of('`') ;
                std::string::size_type pos2 = tmp.find_first_of('`',pos1+1) ;
                std::string::size_type pos3 = tmp.find_first_of('(',pos2);
                std::string::size_type pos4 = tmp.find_last_of(")");
                newAppend.insert(STRING_MAP::value_type(tmp.substr(pos1,pos2-pos1+1), tmp.substr(pos3,pos4-pos3+1)));
            }
            else if( tblInfo.append[curPos] == '(')
            {
                flag++;
            }
            else if( tblInfo.append[curPos] == ')')
            {
                flag--;
            }
            curPos++;
        }
    }


    /*
     * compare define 
     */
    std::string smt = "alter table " + tablename + " " ;
    STRING_MAP::iterator colItor = newcolvec.begin();
    for(; colItor != newcolvec.end(); ++colItor)
    {
        STRING_MAP::iterator fitor = oldcolvec.find(colItor->first);
        if(fitor == oldcolvec.end())
        {
            const FIELDDEF *coldef = getColumnDefine(index, colItor->first);
            if(coldef == NULL)
                continue;
            //need add
            if(alternumb == 0)
            {
                smt = smt + " add ";
                smt = smt + coldef->name + " " + coldef->def;
            }
            else
            {
                smt = smt + ", add ";
                smt = smt + coldef->name + " " + coldef->def;
            }
            alternumb++;
        }
        else
        {
            if(strcasecmp(fitor->second.c_str(),colItor->second.c_str()) != 0)
            //fitor->second != colItor->second)
            {
                const FIELDDEF *coldef = getColumnDefine(index, colItor->first);

                if(coldef == NULL)
                    continue;
                //change
                if(alternumb == 0)
                {
                    smt = smt + " change " + coldef->name + " ";
                    smt = smt + coldef->name + " " + coldef->def;
                }
                else
                {
                    smt = smt + ", change " + coldef->name + " ";
                    smt = smt + coldef->name + " " + coldef->def;
                }
                alternumb++;
            }
            oldcolvec.erase(colItor->first);
        }
    }
    STRING_MAP::iterator ditor = oldcolvec.begin();
    for(; ditor != oldcolvec.end(); ++ditor)
    {
        //delete drop column 
        if(alternumb == 0)
        {
            smt = smt + " drop column " + ditor->first + " ";
        }
        else
        {
            smt = smt + ", drop column " + ditor->first + " ";
        }
        alternumb++;
    }
    addAppendInfo(smt,alternumb,oldAppend,newAppend);
    if(alternumb != 0)
    {
        DEBUG_MSG("need table update :%s\n", smt.c_str());
        if(updatetable_ == 2)
        {
            if (mysql_real_query( dbcon_->get(), smt.c_str(), smt.length()))
            {
                INFO_MSG("alter query (%s) error:(%s) \n", smt.c_str(), dbcon_->getLastError());
                return -1;
            }
            DEBUG_MSG("update table ok\n");
        }
    }
    return 0;
}

void MessageMysql::addAppendInfo(std::string &smt,int &alternumb,STRING_MAP &oldAppend,STRING_MAP &newAppend)
{
    STRING_MAP tmpMap = newAppend;
    STRING_MAP::iterator itor = tmpMap.begin();
    for(;itor!=tmpMap.end();++itor)
    {
        STRING_MAP::iterator fitor = oldAppend.find(itor->first);

        std::string istr = itor->second.c_str();
        std::string fstr = fitor->second.c_str();

        string_replace(istr, " ", "");
        string_replace(fstr, " ", "");
        std::transform(istr.begin(), istr.end(), istr.begin(), toupper);   
        std::transform(fstr.begin(), fstr.end(), fstr.begin(), toupper);   
        
        if(fitor != oldAppend.end() && istr == fstr)
        {
            oldAppend.erase(fitor);            
            newAppend.erase(newAppend.find(itor->first));
        }
    }
   
   for( itor = newAppend.begin();itor!=newAppend.end();++itor)
   {
        if(alternumb == 0)
        {
            smt = smt + " ADD INDEX ";
            smt = smt + itor->first + " " + itor->second;
        }
        else
        {
            smt = smt + ", ADD INDEX ";
            smt = smt + itor->first + " " + itor->second;
        }
        alternumb++;
    }

   for( itor = oldAppend.begin();itor!=oldAppend.end();++itor)
   {
        if(alternumb == 0)
        {
            smt = smt + " DROP INDEX ";
            smt = smt + itor->first;
        }
        else
        {
            smt = smt + ", DROP INDEX ";
            smt = smt + itor->first;
        }
        alternumb++;
    }    
}

/*
 * exec statement 
 * */
void * mysqlThreadEntry(void *arg)
{
    INFO_MSG("Enter mysqlThreadEntry ....\n");

    MySql *dbcon = (MySql*)arg;

    struct timeval tpstart,tpend;
    float  timeuse;
    bool   ismysqlwrite = 1; 

    std::string errfilepath,bufffilepath;

    int64 insql = -1;
    gettimeofday(&tpstart,NULL);

    std::string insertsql = ""; 

    int reconnect = 0;

    for(;;)
    {
        insertsql = "";
        if(g_preparestatement.size() == 0)
        {
            reconnect = 0;
            pthread_mutex_lock(&g_workmutex);
            pthread_cond_wait(&g_workcond,&g_workmutex);
            pthread_mutex_unlock(&g_workmutex);
        }

        if(reconnect == 0)
        {
            dbcon->ping();

	        dbcon->query( "SET character_set_client =  @@character_set_database" );
	        dbcon->query( "SET character_set_connection =  @@character_set_database" );
	        dbcon->query( "SET character_set_results =  @@character_set_database" );
        }

        reconnect = 1;

        if(g_preparesnumber != 0 )
        {
            pthread_mutex_lock(&g_sqlStatementLock);

            MessageMysql::INT_MAP::iterator itor = g_preparestatement.begin();

            if(itor != g_preparestatement.end())
            {
                insertsql = insertsql + itor->second.sql;

                MessageMysql::INT64_MAP::iterator indexitor = g_indexstatment.find(itor->second.index);
                if(indexitor != g_indexstatment.end())
                {
                    if(indexitor->second == itor->first)
                    {
                        indexitor->second = -1;
                    }
                }
                else
                {
                    g_indexstatment.insert(MessageMysql::INT64_MAP::value_type(itor->second.index, -1));
                }
                insql = itor->first;
            }
            else
                insertsql = "";

            pthread_mutex_unlock(&g_sqlStatementLock);
        }

        if(!insertsql.empty())
        {
            if(ismysqlwrite)
            {
                //use mysql
                dbcon->ping();

                mysql_real_query(dbcon->get(), insertsql.c_str(), insertsql.size());

                int errnumb = dbcon->getLastErrorNum();
                if(errnumb)
                {
                    INFO_MSG( "exec sql error%d\n%s\n%s\n",
                            errnumb,
                            dbcon->getLastError(),
                            insertsql.c_str());

                    if(dbcon->ping())
                    {
                        //DEBUG_MSG( "reconnect mysql ..\n");

                        if(g_errstatementfd == NULL)
                        {
                            errfilepath = g_appdir + "/error_statement_" + getTimeString();
                            g_errstatementfd = fopen(errfilepath.c_str(), "w+");
                            if(g_errstatementfd == NULL)
                            {
                                DEBUG_MSG( "not create file thread stop:%s\n", errfilepath.c_str());
                                return NULL;
                            }
                        }

                        /* write error Statement file */
                        fputs( insertsql.c_str(), g_errstatementfd );
                        fputs( "\n", g_errstatementfd );
                        fflush( g_errstatementfd );
                    }
                    else
                    {
                        INFO_MSG( "lose connect mysql ...\n");

                        gettimeofday(&tpstart,NULL);

                        ismysqlwrite = 0;

                        if(g_statementfd == NULL)
                        {
                            bufffilepath = g_appdir + "/statement_" + getTimeString();
                            g_statementfd = fopen(bufffilepath.c_str(), "w+");
                            if(g_statementfd == NULL)
                            {
                                DEBUG_MSG( "not create file thread stop:%s\n", bufffilepath.c_str());
                                return NULL;
                            }
                        }

                        string_replace(insertsql, ";", ";\n");
                        fputs( insertsql.c_str(), g_statementfd );
                        fflush( g_statementfd );
                    }
                }
                else
                {
                    MYSQL_RES *result = mysql_store_result( dbcon->get() ); 
                    if(!result)
                        mysql_free_result( result ) ;

                }
            }
            else
            {
                if(g_statementfd == NULL)
                {
                    bufffilepath = g_appdir + "/statement_" + getTimeString();
                    g_statementfd = fopen(bufffilepath.c_str(), "w+");
                    if(g_statementfd == NULL)
                    {
                        DEBUG_MSG( "not create file thread stop:%s\n", bufffilepath.c_str());
                        return NULL;
                    }
                }

                string_replace(insertsql, ";", ";\n");
                fputs( insertsql.c_str(), g_statementfd );
                fflush( g_statementfd );

                gettimeofday(&tpend,NULL);

                timeuse = 1000000 * (tpend.tv_sec-tpstart.tv_sec)
                    + tpend.tv_usec-tpstart.tv_usec;

                timeuse /= 1000000;

                if(timeuse > MessageMysql::intervalbrobe_ && \
                        dbcon->ping() && \
                        g_statementfd != NULL)
                {
                    INFO_MSG( "reconnect mysql ........\n");

	                dbcon->query( "SET character_set_client =  @@character_set_database" );
	                dbcon->query( "SET character_set_connection =  @@character_set_database" );
	                dbcon->query( "SET character_set_results =  @@character_set_database" );

                    fclose(g_statementfd);
                    g_statementfd = NULL;
                    ismysqlwrite = 1;

                    pthread_t	tid;
                    void * parm = (void *)bufffilepath.c_str();
                    pthread_create(&tid, NULL, recoverToMysqlThread, parm);
                    pthread_detach(tid);
                }
            }

            pthread_mutex_lock(&g_sqlStatementLock);

            g_preparestatement.erase(insql);

            if(g_preparestatement.size() == 0)
                g_preparesnumber = 0;

            pthread_mutex_unlock(&g_sqlStatementLock);

        }
    }

    return NULL;
}

/*
 *
 */
std::string getTimeString()
{
    std::string ret;
    char daybuf[MAXBUFSIZE ];
    time_t  timep;
    struct  tm  *tp;

    time(&timep);
    tp = gmtime(&timep);

    sprintf(daybuf, "%d-%d-%d_%d:%d:%d", 
            (1900+tp->tm_year), (1+tp->tm_mon), tp->tm_mday,
            tp->tm_hour, tp->tm_min, tp->tm_sec);

    ret = daybuf;

    return ret;
}

/*
 *
 */
void * recoverToMysqlThread(void *arg)
{
    INFO_MSG("Enter recoverToMysqlThread (%s)\n", (char *)arg);

    FILE *errstatementfd;
    char *line = NULL;
    size_t len = 0;

    std::string path = (char *) arg;

    FILE *statementfd = fopen(path.c_str(), "r");

    if(statementfd == NULL)
    {
        DEBUG_MSG( "Open file error %s\n", path.c_str());
        return NULL;
    }

    MySql *dbcon = new MySql( g_connectionInfo );

    if (dbcon->get() == NULL)
    {
        INFO_MSG("connect mysql error ..\n");

        delete dbcon;

        fclose(statementfd);

        return false;
    }

    while (getline( &line, &len, statementfd ) != -1)
    {
        mysql_real_query(dbcon->get(), line, strlen(line));

        int errnumb = dbcon->getLastErrorNum();

        if(errnumb)
        {
            INFO_MSG( "recoverToMysql exec sql error%d\n%s\n%s\n",
                    errnumb, dbcon->getLastError(),
                    line);

            if(errstatementfd == NULL)
            {
                std::string errfilepath = g_appdir + "/recover_error_statement_" + getTimeString();
                errstatementfd = fopen(errfilepath.c_str(), "w+");

                if(errstatementfd == NULL)
                {
                    DEBUG_MSG( "not create file thread stop:%s\n", errfilepath.c_str());
                    break;
                }
            }

            fputs( line, errstatementfd );
            fputs( "\n", errstatementfd );
            fflush( errstatementfd );
        }
    }

    INFO_MSG("Finish write record to Mysql (%s)\n", (char *)arg);

    delete dbcon;

    if (line)
        free(line);

    fclose(statementfd);

    if(errstatementfd)
        fclose(errstatementfd);


    return NULL;
}

/*
 *
 */
void string_replace( std::string & strBig,
        const std::string & strsrc,
        const std::string &strdst)
{
    std::string::size_type pos=0;

    std::string::size_type srclen=strsrc.size();

    std::string::size_type dstlen=strdst.size();

    while( (pos=strBig.find(strsrc, pos)) != std::string::npos)
    {
        strBig.erase(pos, srclen);

        strBig.insert(pos, strdst);

        pos += dstlen;
    }
}
