#ifndef DB_PWD_CDES_HEADER_FILE
#define DB_PWD_CDES_HEADER_FILE

#include <string>
#include <cmath>
//#include <stdio.h>
//using namespace std;

class CPwdDesTool
{
public:
	//外面使用的接口，这个是加密
	std::string EncryptString(std::string srcEnc);
	//外面使用的接口，这个是解密
	std::string DecryptString(std::string srcDec);

    //用16个字符来初始化key，字符要由0--9,a--f,A--F组成
	CPwdDesTool(std::string key="",std::string iv = "");
	~CPwdDesTool();
    
    //int CPwdDesTool::testSSLdes( );
private:
	std::string m_strKey;
	std::string m_strIV;
	
	//把字节"ABC123",(0xABC123),转换成字符串"ABC123",(0x414243313233)默认是ABC大写输出，
	std::string ConverByesToChars( std::string src, bool toupper = true);
	
	//把字符串"ABC123",(0x414243313233),转换成字节"ABC123",(0xABC123),src原串，要有偶数个字符，否则否返回空串，表示转换失败
	std::string ConverCharsToByes( std::string src);
	
	//查src里面的字符，是不是全都由0--9,a--f,A--F组成。是就返回1，不是就返回0。由ConverCharsToByes调用
	int CheckChars( std::string src );
};

//---------------------------------------------------------------------------
#endif
