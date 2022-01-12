/**
 *  @brief      数据操作
 *  @file       xmldatabase.cpp
 *  @version    1.0.0
 *  @date       2011-01-29
 */
#include "xmldatabase.h"

static const char * DEFAULT_ENTITY_TYPE_STR = "Avatar";
static const char * DEFAULT_NAME_PROPERTY_STR = "playerName";

CXmlDatabase::CXmlDatabase():
    pEntityDefs_(NULL)
{
}

CXmlDatabase::~CXmlDatabase()
{
    MAP_CXMLTABLE::iterator itor = databases_.begin();
    for(; itor != databases_.end(); ++itor)
        delete itor->second;

    if(pEntityDefs_ != NULL)
        delete pEntityDefs_;
}
/** 
 *  初始化 
 */
int CXmlDatabase::init(DataSectionPtr section)
{
    pEntityDefs_ = new EntityDefs();

    std::string defaultTypeName = DEFAULT_ENTITY_TYPE_STR;
    std::string nameProperty;

    if(!pEntityDefs_->init(section, defaultTypeName, nameProperty))
    {
        return -1;
    }

    std::string xmldef = BWResource::getDefaultPath() + "/entities/xmldef";
    for ( EntityTypeID ent = 0; ent < pEntityDefs_->getNumEntityTypes(); ++ent )
    {
        const EntityDescription& entDes = pEntityDefs_->getEntityDescription(ent);

        std::string nameProperty = "";

        CXmlTable *ntab = new CXmlTable(entDes.name());

        VEC_CXMLDEFINE &child = ntab->proprties();
        for ( unsigned int i = 0; i < entDes.propertyCount(); ++i )
        {
            DataDescription * dataDes = entDes.property( i );
            CXmlDefine *nDef = new CXmlDefine();
            DataType * dataType = dataDes->dataType();

            propertyMapping( dataDes->name(), *dataType, nDef);
            child.push_back(nDef);
        }

        //判断定义是否改变
        if(ntab->checkDefine())
        {
            printf("define change[%s]...\n", ntab->tableName().c_str());
        }
        else
        {
            //加载数据索引
            if(ntab->loadDataIndex() == -1)
                printf("Error load data index....\n");
        }
        
        databases_.insert(MAP_CXMLTABLE::value_type(entDes.name(), ntab));
    }

    return 0;
}
void propertyMapping( const std::string& propName, const DataType& type, CXmlDefine *def)
{
    const SequenceDataType* pSeqType = NULL;
    const ClassDataType* pClassType = NULL;
    const UserDataType* pUserType = NULL;
    const FixedDictDataType* pFixedDictType = NULL;

    if(def->name() == "")
        def->name(propName);

    if ((pSeqType = dynamic_cast<const SequenceDataType*>(&type)))
    {
        if(def->type() != "SEQUENC")
            def->type("SEQUENC");
        else
            def->defvalue("SEQUENC");

        propertyMapping("", pSeqType->getElemType(), def);

    }
    else if ((pFixedDictType = dynamic_cast<const FixedDictDataType*>(&type)))
    {
        if(def->type() != "SEQUENC")
            def->type("FIXEDDICT");
        else
            def->defvalue("FIXEDDICT");
        classTypeMapping(propName, *pFixedDictType, def);
    }
    else
    {
        const MetaDataType * pMetaType = type.pMetaDataType();
        MF_ASSERT(pMetaType);
        const char *metaName = pMetaType->name();
        if(def->type() != "SEQUENC")
            def->type(metaName);
        else
            def->defvalue(metaName);

    }

}

template <class DATATYPE>
void classTypeMapping( const std::string& propName, const DATATYPE& type, CXmlDefine *def)
{
    const ClassDataType::Fields& fields = type.getFields();

    VEC_CXMLDEFINE &child = def->child();
    for ( ClassDataType::Fields::const_iterator i = fields.begin();
            i < fields.end(); ++i )
    {
        CXmlDefine *nDef = new CXmlDefine();
        DataSectionPtr pPropDefault;
        propertyMapping(i->name_, *(i->type_), nDef);
        child.push_back(nDef);
    }
}

/** 
 *  读取数据 
 */
DataSectionPtr CXmlDatabase::openEntity(const std::string &name, int seq)
{
    MAP_CXMLTABLE::iterator itor = databases_.find(name);
    if(itor == databases_.end())
        return NULL;

    return itor->second->makeDataSection(seq);
}
/** 
 *  保存Section到内存记录 
 */
int CXmlDatabase::saveEntity(DataSectionPtr section, int seq)
{
    MAP_CXMLTABLE::iterator itor = databases_.find(section->sectionName());
    if(itor == databases_.end())
        return NULL;

    return itor->second->sectionToDataNode(section, seq);
    return seq;
}

/** 
 *  保存数据到文件 
 */
void CXmlDatabase::save(const std::string &name)
{
    MAP_CXMLTABLE::iterator itor = databases_.find(name);
    if(itor != databases_.end())
        itor->second->saveDatas();
}

/** 
 *  更新实体定义的数据到最新的结构 
 */
int CXmlDatabase::updateDefinData(const std::string &name)
{
    MAP_CXMLTABLE::iterator itor = databases_.find(name);
    if(itor != databases_.end())
        itor->second->alterDataStruct();
    return 0;
}

/** 
 *  删除一条数据 
 */
int CXmlDatabase::deleteEntity(const std::string &name, int seq)
{
    MAP_CXMLTABLE::iterator itor = databases_.find(name);
    if(itor != databases_.end())
        itor->second->deleteEntity(seq);
    return 0;
}
