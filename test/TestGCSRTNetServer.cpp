// TestGCSRTNet.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include "time.h"
#include "IocpNet.h"


//���տͻ��˵���������Ļص�����
void WINAPI AcceptCallBack(HANDLE handle, TCHAR *pszIP, WORD wPort);
//�Ͽ��ͻ������ӵĻص�����
void WINAPI DissconnectCallBack(HANDLE handle);
//���յ����ݰ�ʱ�Ļص�����
void WINAPI RecvDataCallBack(HANDLE handle, int nLen, char* pData);

//���տͻ��˵���������Ļص�����
void WINAPI AcceptCallBack(HANDLE handle, TCHAR *pszIP, WORD wPort)
{
	_tprintf(_T("AcceptCallBack-->handle=%p, pszIP=%s, wPort=%04d \n"), handle, pszIP, wPort);	
	return;
}
//�Ͽ��ͻ������ӵĻص�����
void WINAPI DissconnectCallBack(HANDLE handle)
{	
	//ֱ���˳�,�����������
	_tprintf(_T("DissconnectCallBack-->handle=%p\n"), handle);
	return;
}

//���յ����ݰ�ʱ�Ļص�����
void WINAPI RecvDataCallBack(HANDLE handle, int nLen, char* pData)
{
	TCHAR ip[32];
	IocpNetGetPeerIP(handle,ip);

	pData[nLen]=0;
	_tprintf(_T("Recv Socket=%p  ip=%s , nLen=%d, %s , time=%d\n\n"),handle,ip, nLen,pData,time(NULL));

	//���ص��ͻ���
	IocpNetSend(handle, 3,"abn");
}

int _tmain(int argc, _TCHAR* argv[])
{
	IocpNetBegin();
	_tprintf(_T("The application is server.\n"));
	if(!IocpNetInit(RecvDataCallBack,DissconnectCallBack,AcceptCallBack, 8888))
	{
		_tprintf(_T("IocpNetInit failed as server.\n"));
		getchar();
		return 0;
	}

	int nExitCode;
	while(true)
	{
		scanf( "%d", &nExitCode);
		if(nExitCode == 0)
		{
			break;
		}
	}

	IocpNetEnd();

	////////////////////////////////////////////////////////
	/*
	�ͻ�������
	IocpNetBegin();
	_tprintf(_T("The application is client.\n"));

	if(!IocpNetInit(RecvDataCallBack,DissconnectCallBack,AcceptCallBack))
	{
		_tprintf(_T("GCEApiSRTNetInit failed as client.\n"));
		return 0;
	}

	HANDLE handle;
	TCHAR ip[16];
	GSRTGetIp(_T("localhost"),ip);
	handle=IocpNetConnect(ip,2323);
	if(handle == NULL)
	{
		_tprintf(_T("GCEApiSRTNetConnect to failed .\n"));
		getchar();
		return 0;
	}

	while(true)
	{		
		if(!IocpNetSend(handle, 3, "abc"))
		{ return 0; }
		Sleep(2000);
	}
	IocpNetEnd();
	*/


	return 0;
}

