/**
 *  @brief      保存xml文件数据
 *  @file       xmldatas.cpp
 *  @version    1.0.0
 *  @date       2011-01-29
 */
#include "xmldatas.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <algorithm>

CDataNode::CDataNode():
    name_(""), 
    value_(""), 
    child_(NULL), 
    layer_(0), 
    pBelong_(NULL)
{
}

CDataNode::CDataNode(const char *buff, int layer):
    name_(""), 
    value_(""), 
    child_(NULL), 
    layer_(layer), 
    pBelong_(NULL)
{
    this->init(buff, layer);
}

CDataNode::CDataNode(const std::string &name, const std::string &value, int layer):
    name_(name), 
    value_(value), 
    child_(NULL),
    layer_(layer), 
    pBelong_(NULL)
{
}
CDataNode::~CDataNode()
{
    if(child_ != NULL)
    {
        for(unsigned int i = 0; i < child_->size(); i++)
        {
            delete child_->at(i);
        }

        delete child_;
    }
}

int CDataNode::init(const char *buff, int layer)
{
    layer_ = layer;
    const char *p = buff + 1;
    const char *begin = p;

    while(*p != '>')p++;

    name_.append(begin, p - begin);

    p++;
    begin = p;

    //如果没有子节点就取值
    if(*(p + 1) == 0x0A && *p == 0x0D) 
    {
        return (int)(p - buff + 1);
    }
    else
    {
        while(*p != '<') 
            p++;

        //去掉前面或后面的空格和TAB
        const char *bpos = begin;
        const char *epos = p;

        while(*bpos == 0x09 || *bpos == 0x20) 
            bpos++;

        while(*bpos == 0x09 || *bpos == 0x20) 
            epos--;

        value_.append(bpos, epos - bpos);
    }

    return (int)(p - buff);
}

void CDataNode::value(const std::string &v)
{
    value_ = v;
}

int CDataNode::addChildNode(CDataNode *node)
{
    if(child_ == NULL)
    {
        child_ = new VEC_CDATANODE;
    }

    child_->push_back(node);

    return (int)child_->size();
}

int CDataNode::delChildNode(unsigned int index)
{
    if(child_ == NULL)
    {
        printf("Error node no child...\n");
        return -1;
    }

    if(index < child_->size())
    {
        printf("Error CDataNode::delChildNode..");
        return -1;
    }
    
    delete child_->at(index);
    child_->erase(child_->begin() + index);

    return 0;
}

int CDataNode::writeFile(std::ofstream &outfile)
{
    std::string content = "";

    for(int i = 0; i < layer_; i++)
        content.append("	");
    content.append("<");
    content.append(name_);
    content.append(">");
    content.append(value_);

    outfile.write(content.c_str(), (int)content.size());

    if(child_ != NULL)
    {
        for(unsigned int i = 0; i < child_->size(); i++)
        {
            outfile.write("\r\n", 2);
            child_->at(i)->writeFile(outfile);
        }
    }

    content = "";
    if(child_ != NULL && child_->size() > 0)
    {
        content.append("\r\n");
        for(int i = 0; i < layer_; i++)
            content.append("	");
    }

    content.append("</");
    content.append(name_);
    content.append(">");

    outfile.write(content.c_str(), (int)content.size());

    return 0;
}

int CDataNode::makeDataSection(DataSectionPtr ds)
{
    DataSectionPtr cd = ds->newSection(name_);
    if(child_ != NULL)
    {
        for(unsigned int i = 0; i < child_->size(); i++)
        {
            child_->at(i)->makeDataSection(cd);
        }
    }
    else
    {
        //删除tab
        cd->setString(value_);
    }

    return 0;
}

/** 
 *  DataSection转变成CDataNode 
 */
int CDataNode::sectionToDataNode(DataSectionPtr ds)
{
    if(child_ == NULL)
        child_ = new VEC_CDATANODE;

    for(int i = 0; i < ds->countChildren(); i++)
    {
        DataSectionPtr cds = ds->openChild(i);
        CDataNode *node = new CDataNode(cds->sectionName(), cds->asString(), layer_ + 1);
        if(cds->countChildren() > 0)
            node->sectionToDataNode(cds);
        child_->push_back(node);
    }

    return 0;
}
