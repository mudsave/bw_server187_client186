/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef ENTTIYDB_HPP
#define ENTTIYDB_HPP
#include <Python.h>
#include "cstdmf/debug.hpp"
#include "entitydefs.h"
#include "pyscript/script.hpp"
#include "pyscript/py_data_section.hpp"
#include "network/logger_message_forwarder.hpp"
#include "server/bwconfig.hpp"
#include "resmgr/xml_section.hpp"

/** 通过名字取得实体定义编号 */
EntityTypeID getEntityTypeID(const std::string &entName);

/** 取得实体数据 */
DataSectionPtr openEntity2Section(const std::string &entName, int dbid);

/** 保存实体数据 */
int saveDataSection(DataSectionPtr section, int dbid);

/** 初始化bw和数据 */
int initEntitiesDB(const std::string &dbPath);


void ptest();

#endif // IDATABASE_HPP
