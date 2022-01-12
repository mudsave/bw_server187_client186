#include "db_pwd_des.h"
#include <stdio.h>
#include <openssl/des.h>
using namespace std;

string CPwdDesTool::EncryptString( string srcEnc)
{
        int             n=0;
        DES_cblock      Key2;
        DES_key_schedule schedule;
        int size = srcEnc.length();
        char* Res = new char[size+2];
        memset(Res,0,size+2);
        /* Prepare the key for use with DES_cfb64_encrypt */
        memcpy( Key2, m_strKey.c_str(),8);
        DES_set_odd_parity( &Key2 );
        DES_set_key_checked( &Key2, &schedule );
 
        /* Encryption occurs here */
        DES_cfb64_encrypt( (const unsigned char * ) srcEnc.c_str(), ( unsigned char * ) Res,
                           size+1, &schedule, &Key2, &n, DES_ENCRYPT );
 
        string temp(Res);
        delete Res;
        Res = 0;
        return ConverByesToChars(temp);         
}
 
 
string CPwdDesTool::DecryptString( string srcDec)
{ 
        string srcByeDec = ConverCharsToByes(srcDec);
        if (srcByeDec == "")
        {
            return "";
        }
        int size = srcByeDec.length();
        int             n=0; 
        DES_cblock      Key2;
        DES_key_schedule schedule; 
        char *Res = new char[ size +2];
        memset(Res,0,size+2);
        /* Prepare the key for use with DES_cfb64_encrypt */
        memcpy( Key2, m_strKey.c_str(),8);
        DES_set_odd_parity( &Key2 );
        DES_set_key_checked( &Key2, &schedule );
 
        /* Decryption occurs here */
        DES_cfb64_encrypt( (const unsigned char * ) srcByeDec.c_str(), ( unsigned char * ) Res,
                           size+1, &schedule, &Key2, &n, DES_DECRYPT );
 
        string strResult(Res);
        delete Res;
        Res = 0;
        return strResult; 
}

CPwdDesTool::CPwdDesTool(string key,string iv)
{
    if((key == "")||(key.length() < 16)||(CheckChars(key )== 0))
    {
        //m_strKey = ConverCharsToByes("13CBA7604A952762");
        m_strKey = ConverCharsToByes("31CBA70F4A59E26D");
        m_strIV = m_strKey;
    }
    else
    {
        m_strKey = ConverCharsToByes(key);
        m_strIV  = ConverCharsToByes(iv);
    }
}

CPwdDesTool::~CPwdDesTool()
{

}

string CPwdDesTool::ConverByesToChars( string src, bool toupper)
{ 
	int len = src.length();
	string result = "";
    result.reserve(len*2);
	for(int i = 0; i<len; ++i)
	{
	    unsigned char temp = src[i];
		char buffer[4] = {0};
        if(toupper)
        {
            sprintf(buffer,"%02X",temp);
        }
        else
        {
            sprintf(buffer,"%02x",temp);
        }
		result+=buffer;
	}
	return result;
};

string CPwdDesTool::ConverCharsToByes( string src )
{    
    int len = src.length();
    //要有偶数个字符,否则不转换
    if(1 == len%2)
    {
        return "";
    }
    //src里面是不是有不能转换的字符，有就不能转换
    if(CheckChars( src ) != 1)
    {
        return "";
    }
    char  charBuf[12] = {0};//注意这里要留够的内存，给下面sscanf使用
    string strTemp ="";
    strTemp.reserve(len);
    for(int i=0; i<len/2;++i )
    {
        sscanf((const char*)&src[i*2], "%2x", charBuf );
        strTemp += charBuf[0];
    }
    return strTemp;
};

int CPwdDesTool::CheckChars( string src )
{
	int len = src.length();
	int count = 0;
	for( count = 0; count < len; ++count)
	{
	    char temp = src[count];
	    if(( temp >= '0')&&(temp <='9'))
	    {
	        continue;
	    }
	    if(( temp >= 'A')&&(temp <='F'))
	    {
	        continue;
	    }
	    if(( temp >= 'a')&&(temp <='f'))
	    {
	        continue;
	    }
	    return 0;
	}
    return 1;
}

/*
int CPwdDesTool::testSSLdes( )
{
    //明文，加密得到密文
    string mingWen = "123456";
    string strMiwen = EncryptString(mingWen);
    //std::cout<<mingWen<<std::endl;
    //std::cout<<strMiwen<<std::endl;
    printf("mingWen text\t : %s \n",mingWen.c_str());
    printf("strMiwen text\t : %s \n",strMiwen.c_str());

    
    //==========================c2fc967147387d
    //std::cout<<"=========================="<<endl;
    //密文，解密得到明文

    string md2 = DecryptString(strMiwen);
    //std::cout<<mingWen<<std::endl;
    //std::cout<<md<<std::endl;
    printf("strMiwen text\t : %s \n",strMiwen.c_str());
    printf("md2 text\t : %s \n",md2.c_str());

    return 0;
}
*/

