#ifndef Apex_GameProxy_h
#define Apex_GameProxy_h

#include "apex_proxy.h"
#include "pyscript/pyobject_plus.hpp"
#include "pyscript/script.hpp"
#include <string>
#include <queue>
	
///////////////////////////////////////////////////////////////////
//
struct apex_proxy_send_buf_st
{
	signed int nRoleId;
    std::string data;
    int iLength;	

    apex_proxy_send_buf_st()
    {
        nRoleId = 0;
        iLength = 0;
        data = "";
    }
};
struct apex_proxy_kill_buf_st
{
	signed int nRoleId;
	int iAction;

    apex_proxy_kill_buf_st()
    {
        nRoleId = 0;
        iAction = 0;
    }
};

	class PyAPexProxy : public PyObjectPlus
	{
		Py_InstanceHeader(PyAPexProxy)

	public:

		PyAPexProxy ();
        ~PyAPexProxy();

        //int static  NoticeApec_UserData(const char * pBuf,int nBufLen);
        long static NetSendToGameClient(signed int nRoleId,const char * pBuffer,int nLen);
        long  static GameServerKillUser(signed int nRoleId,int Action);

		PyObject* pyGetAttribute(const char *attr);
		int pySetAttribute(const char *attr, PyObject *value);
        PyObject *SetmpfSend(PyObject *args);
        PyObject *SetFuncCallBack(PyObject *args);

		//PY_METHOD_DECLARE(py_SetmpfSend);
        PY_METHOD_DECLARE(py_InitAPexProxy);
		PY_METHOD_DECLARE(py_StartApexProxy);
		PY_METHOD_DECLARE(py_StopApexProxy);
		PY_METHOD_DECLARE(py_NoticeApexProxy);
		PY_METHOD_DECLARE(py_SendMsgBuf2Client);
		PY_METHOD_DECLARE(py_ApexKillRoleFromBuf);

        //PY_RO_ATTRIBUTE_DECLARE(mpfSend, mpfSend);
        //PY_RO_ATTRIBUTE_DECLARE(mpfRecv, mpfRecv);
		PY_FACTORY_DECLARE();

	private:		
        static PyObject *SendMsgToClientFunc;
        static PyObject *KillUserFunc;
        static _FUNC_S_REC  mpfRecv;
        
        std::queue<apex_proxy_send_buf_st> mSendMsgQueue;
        std::queue<apex_proxy_kill_buf_st> mKillRoleQueue;
	};
    typedef SmartPointer<PyAPexProxy> PyAPexProxyPtr;

    PyAPexProxy* GetUniqueAPexProxyInstance();
#endif

