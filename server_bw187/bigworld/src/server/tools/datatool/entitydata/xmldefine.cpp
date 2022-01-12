/**
 *  @brief      数据定义处理
 *  @file       xmldefine.cpp
 *  @version    1.0.0
 *  @date       2011-01-29
 */
#include "xmldefine.h"

CXmlDefine::CXmlDefine():
    name_(""), type_(""), default_("")
{
}

CXmlDefine::~CXmlDefine()
{
}

int CXmlDefine::writeFile(std::ofstream &outfile, int n)
{
    char *buff = new char[1024];
    for(int i = 0; i < n; i++)outfile.write("	", 1);

    memset(buff, 0x00, 1024);
    sprintf(buff, "<%s Type=\"%s\" default=\"%s\">", 
            name_.c_str(), type_.c_str(), default_.c_str());
    outfile.write(buff, strlen(buff));

    if(child_.size() > 0)
        outfile.write("\n", 1);

    for(unsigned int i = 0; i < child_.size(); i++)
    {
        child_[i]->writeFile(outfile, n+1);
    }

    if(child_.size() > 0)
        for(int i = 0; i < n; i++)outfile.write("	", 1);

    memset(buff, 0x00, 1024);
    sprintf(buff, "</%s>\n", name_.c_str());
    outfile.write(buff, strlen(buff));

    return 0;
}

static std::string getvalue(const std::string &buffer, const std::string &name)
{
    size_t found, bt , et;
    found = buffer.find(name);
    if(found == std::string::npos)
        return "";

    bt = et = found;

    while(buffer[bt] != '"')
        bt++;

    et = bt + 1;

    while(buffer[et] != '"')
        et++;

    return std::string(buffer.c_str() + bt + 1, et - bt - 1);
}

/** 
 *  检查定义
 */
bool CXmlDefine::check(std::ifstream &stream)
{
    char stringline[1024] = {};
    stream.getline(stringline, 1024);

    int bt = 0, et = 0;
    //判断名称
    while(stringline[bt] != '<')
        bt++;
    et = bt;

    while(stringline[et] != ' ' && stringline[et] != '>')
        et++;

    std::string name(stringline + bt + 1, et - bt - 1);

    if(name != name_)
    {
        printf("Difference name:(%s)[%s]\n", name.c_str(), name_.c_str());
        return true;
    }

    //判断类型
    std::string type = getvalue(stringline, "Type");
    if(type != type_)
    {
        printf("Difference type:(%s)[%s]\n", type.c_str(), type_.c_str());
        return true;
    }

    //判断默认值
    std::string def = getvalue(stringline, "default");
    if(def != default_)
    {
        printf("Difference default:(%s)[%s]\n", def.c_str(), default_.c_str());
        return true;
    }

    for(unsigned int i = 0; i < child_.size(); i++)
    {
        if(child_[i]->check(stream))
            return true;
    }

    if(child_.size() > 0)
        stream.getline(stringline, 1024);

    return false;
}

