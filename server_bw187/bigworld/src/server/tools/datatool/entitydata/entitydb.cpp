#include "entitydb.hpp"
#ifdef _WIN32
#include <direct.h> 
#endif
#include "xmldatabase.h"

/** 数据定义全局对象指针 */
CXmlDatabase *g_pDatabases = NULL;

/** 
 * 初始化内置脚本模块,script.cpp里面实现 
 * 在进程里面增加Math，Resgr等模块和函数
 */
extern void runInitTimeJobs();


/**
 * 获取实体定义在数据库中的数据
 * @param entName 实体名称
 * @param dbid 数据库的数据编号,当编号给-1时将使用默认值创建新的数据
 * @return  成功返回取到的数据数据或者返回NULL
 */
DataSectionPtr openEntity2Section(const std::string &entName, int dbid)
{
    return g_pDatabases->openEntity(entName, dbid);
}

/**
 *  保存实体数据到数据库
 *  @prarm section 实体数据对象
 *  @prarm dbid 保存的数据库编号
 *  @return 如果给定编号是-1返回新数据编号, 返回负数表示有错误.
 */
int saveDataSection(DataSectionPtr section, int dbid)
{
    return g_pDatabases->saveEntity(section, dbid);
}

/**
 *  ReaMgr.saveEntity 脚本接口初始化实体定义和连接数据库 
 *  @prarm pObject 由ResMgr.openEntity 打开的实体数据
 *  @return 返回数据编号
 */
static PyObject  *saveEntity( PyObjectPtr pObject, int dbid = -1 )
{
	PyDataSection *pOwner = reinterpret_cast<PyDataSection*>( pObject.getObject());
    return  Script::getData(saveDataSection(pOwner->pSection(), dbid));
}
PY_AUTO_MODULE_FUNCTION( RETDATA, saveEntity,
        ARG( PyObjectPtr, ARG( int, END ) ), ResMgr )

/** 
 *  ResMgr.openEntity 获取数据脚本接口 
 *  @prarm args (定义名称,数据库id)
 *  @return 返回PyDataSection类型
 */
static PyObject  *openEntity( const std::string &entName, int dbid = -1 )
{
    return new PyDataSection(openEntity2Section(entName, dbid) ,
            &PyDataSection::s_type_);
}
PY_AUTO_MODULE_FUNCTION( RETDATA, openEntity,
        ARG( std::string, ARG( int, END ) ), ResMgr )


/**
 *  ReaMgr.deleteEntity 删除数据
 */
static PyObject *deleteEntity(const std::string &name, int dbid)
{
    g_pDatabases->deleteEntity(name, dbid);
    Py_Return;
}
PY_AUTO_MODULE_FUNCTION( RETDATA, deleteEntity,
        ARG( std::string, ARG( int, END ) ), ResMgr )


/**
 *  ReaMgr.getData 获取数
 *  @return 成功返回字典类型数据
 */
static PyObject *getData( const std::string &name, int seq )
{
    //判断结构是否改动
    Py_Return;
}
PY_AUTO_MODULE_FUNCTION( RETDATA, getData,
        ARG( std::string, ARG( int, END ) ), ResMgr )

/**
 *  ReaMgr.updateData 更新xml数据文件。使数据更新到最新的定义结构。
 */
static PyObject *updateData( const std::string &name )
{
    g_pDatabases->updateDefinData(name);
    Py_Return;
}
PY_AUTO_MODULE_FUNCTION( RETDATA, updateData,
        ARG( std::string, END), ResMgr )

/**
 *  ReaMgr.saveData 保存xml数据文件
 */
static PyObject *saveData( const std::string &name )
{
    g_pDatabases->save(name);
    Py_Return;
}
PY_AUTO_MODULE_FUNCTION( RETDATA, saveData,
        ARG( std::string, END), ResMgr )

/**
 *  ReaMgr.init 脚本接口初始化实体定义和连接数据库 
 *  @prarm args (存放paths.xml文件的路径)
 *  @return 返回加载成功或失败
 *  - 0 成功
 *  - -1 解析参数错误
 *  - -2 初始化BW资源失败
 */
static PyObject *loadEntityDB(PyObject *self, PyObject *args)
{
    const char *path;

    if (!PyArg_ParseTuple(args, "s", &path))
    {
        return  Script::getData(-1);
    }

    //因为BWResource::init会使用 路径 + paths.xml 所以要检查路径是否有"/"
    std::string basePath = path;
    std::replace(basePath.begin(), basePath.end(), '\\', '/');

    if(basePath[basePath.size() - 1] != '/')
    {
        basePath = basePath + "/";
    }

    //初始化资源信息把paths.xml配置的路径设置成资源路径，
    //用openSection只能用相对路径打开
    char *sourcePath[2];
    sourcePath[0] = "-r";
    sourcePath[1] = (char *)basePath.c_str();
    if(!BWResource::init(2, sourcePath))
    {
        return  Script::getData(-2);
    }

    //增加python模块
    runInitTimeJobs();

    DataSectionPtr section = BWResource::openSection( "entities/entities.xml");
    if(!section)
    {
        INFO_MSG( "ERROR openSection entities.xml....\n" );
        return  Script::getData(-3);
    }

    g_pDatabases = new CXmlDatabase();

    g_pDatabases->init(section);

    return  Script::getData(0);
}

/**
 *  ReaMgr.finish 卸载模块
 */
static PyObject *finishEntityDB(PyObject *self, PyObject *args)
{
    delete g_pDatabases;
    Script::fini(0);
    BWResource::instance().purgeAll();
    Py_Return;
}

static PyMethodDef ResMgrMethods[] = 
{
    {"init", loadEntityDB, METH_VARARGS, "加载模块数据资源,一个参数用来指定资源存放的路径,\n\
        entities/entities.xml必须存在指定的目录"},
    {"finish", finishEntityDB, METH_NOARGS, "卸载模块数据资源"},
    { NULL, NULL}
};

PyMODINIT_FUNC initResMgr(void) 
{
    PyObject* m;

    m = Py_InitModule3("ResMgr", ResMgrMethods,
            "资源相关.");

}
void ptest()
{
    PyObject* m =  Py_BuildValue("(s)","/home/hsq/love3/trunk/bigworld/src/server/tools/datatool/bin/res");

    loadEntityDB(NULL,m);
}
