/**
 *  @brief      单个定义和数据
 *  @file       xmltable.h
 *  @version    1.0.0
 *  @date       2011-01-29
 */
#ifndef XMLTABLE_H
#define XMLTABLE_H
#include "xmldatas.h"
#include "xmldefine.h"
class CXmlTable
{
    public:

        CXmlTable(const std::string &name);
        ~CXmlTable();

        bool change() { return dataChange_; };

        void change(bool value) { dataChange_ = value; };

        VEC_CXMLDEFINE &proprties() { return proprties_; };

        /** 比较和保存的定义是否改变 */
        bool checkDefine();

        /** 加载数据，在更改数据结构使用 */
        VEC_CDATANODE *loadData();


        /** 加载数据记录的文件偏移量，以便快速访问 */
        int loadDataIndex();

        /** 读取记录 */
        DataSectionPtr makeDataSection(int seq);

        /** DataSection转变成CDataNode */
        int sectionToDataNode(DataSectionPtr ds, int seq);

        /** 根据新定义的数据结构整理数据 */
        int alterDataStruct();

        /** 单个数据的整理 */
        CDataNode *verifyData(CDataNode *row);

        /** 子数据的整理 */
        int seqVerifyData(CXmlDefine *def, CDataNode *oRow, CDataNode *pRow);
        int dictVerifyData(CXmlDefine *def, CDataNode *oRow, CDataNode *pRow);

        /** 保存定义 */
        int saveDefine();

        /** 保存数据 */
        int saveDatas(VEC_CDATANODE *pNode = NULL);

        /** 删除一条数据 */
        int deleteEntity(int seq);

        const std::string &tableName() { return tableName_; }

    private:

        /** 加载单个数据 */
        CDataNode *rowData(CDataNode *row, int posBegin, int posEnd);

        /** 实体名称 */
        std::string tableName_;

        /** 定义否改变过 */
        bool defineChange_;

        /** 数据否改变过 */
        bool dataChange_;

        /** 数据 */
        VEC_CDATANODE *datas_;

        /** 数据文件路径 */
        std::string dataFile_;

        /** 定义文件路径 */
        std::string defineFile_;

        /** 定义 */
        VEC_CXMLDEFINE proprties_;

        /** 打开的数据文件 */
        std::ifstream dataFtream_;

        /** 数据文件长度 */
        int fileLength_;
};
#endif
