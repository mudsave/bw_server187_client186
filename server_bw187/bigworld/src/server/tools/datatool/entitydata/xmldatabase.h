/**
 *  @brief      xml数据库管理，提供对xml数据库的初始化，
 *              查询，更新操作功能
 *  @file       xmldatabase.h
 *  @version    1.0.0
 *  @date       2011-01-29
 */
#ifndef XMLDATABASE_H
#define XMLDATABASE_H
#include "xmltable.h"
#include "entitydefs.h"
typedef std::map< std::string, CXmlTable * > MAP_CXMLTABLE;

/**
 *  @class CXmlDatabase
 *  定义管理类全局唯一
 */
class CXmlDatabase
{
    public:

        CXmlDatabase();

        ~CXmlDatabase();

        /** 初始化 */
        int init(DataSectionPtr section);
        
        /** 读取数据 */
        DataSectionPtr openEntity(const std::string &name, int seq);

        /** 保存Section到内存记录 */
        int saveEntity(DataSectionPtr section, int seq);

        /** 取出数据成为字典 */
        PyObject *openData(const std::string &name, int seq);

        /** 保存字典到内存记录 */
        int openData(const std::string &name, int seq, PyObject *dict);

        /** 保存数据到文件 */
        void save(const std::string &name);

        /** 更新实体定义的数据到最新的结构 */
        int updateDefinData(const std::string &name);

        /** 删除一条数据 */
        int deleteEntity(const std::string &name, int seq);

    private:

        EntityDefs *pEntityDefs_;

        MAP_CXMLTABLE databases_;
};
template <class DATATYPE>
void classTypeMapping( const std::string& propName, const DATATYPE& type, CXmlDefine *def = NULL);

void propertyMapping( const std::string& propName, const DataType& type, CXmlDefine *def = NULL);
#endif
