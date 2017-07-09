// TestGCSRTNet.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include "time.h"
#include "IocpNet.h"
#include <map>
using namespace std;

//���տͻ��˵���������Ļص�����
void WINAPI AcceptCallBack(HANDLE handle, TCHAR *pszIP, WORD wPort);
//�Ͽ��ͻ������ӵĻص�����
void WINAPI DissconnectCallBack(HANDLE handle);
//���յ����ݰ�ʱ�Ļص�����
void WINAPI RecvDataCallBack(HANDLE handle, int nLen, char* pData);

//
map<HANDLE,time_t> gg;

//���տͻ��˵���������Ļص�����
void WINAPI AcceptCallBack(HANDLE handle, TCHAR *pszIP, WORD wPort)
{
	_tprintf(_T("AcceptCallBack-->handle=%p, pszIP=%s, wPort=%04d \n"), handle, pszIP, wPort);	
	gg[handle]=time(NULL);
}
//�Ͽ��ͻ������ӵĻص�����
void WINAPI DissconnectCallBack(HANDLE handle)
{	
	//ֱ���˳�,�����������
	_tprintf(_T("DissconnectCallBack-->handle=%p\n"), handle);
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

	//10���Ͽ�����
	if(time(NULL)- gg[handle] >10)
	{
		IocpNetDisconnect(handle);
	}
}

int _tmain(int argc, _TCHAR* argv[])
{
	IocpNetBegin();
	_tprintf(_T("The application is server.\n"));
	if(!IocpNetInit(RecvDataCallBack,DissconnectCallBack,AcceptCallBack, 2323))
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


	return 0;
}

