#include "stdafx.h"

#include <MSTcpIP.h>
#include "IocpNetSockMgr.h"
#include "IocpNetDealQueMgr.h"
#include "IocpNet.h"

IOCPNET_RECV_DATA_CALLBACK		g_pfnIocpReceiveData_cb=NULL;
IOCPNET_DISCONNECT_CALLBACK		g_pfnIocpDisconnect_cb=NULL;
IOCPNET_ACCEPT_CALLBACK			g_pfnIocpAccept_cb=NULL;

HANDLE g_hThreadHandle[1024];		//保存线程句柄，用于在程序结束时，关闭线程
int g_nIocpNETThreadHandleCnt = 0;			//线程数

IocpNetSockMgr* g_pSockMgr=NULL;
int g_nIocpNetWorkThreadCnt = 1;		//工作线程数量，一个工作线程对应一个IocpNetSockMgr，一个完成端口句柄
bool g_bIocpNetGCThreadRun;

BOOL GCEApiSRTNetDestroy();

IocpNetSockMgr*  GetSocketMgr(HANDLE handle)
{
	GCSTS_Sock_Info* p = (GCSTS_Sock_Info*)handle;
	int i;
	for(i=0; i<g_nIocpNetWorkThreadCnt; i++)
	{
		if(g_pSockMgr[i].IsExist(p))
			return &g_pSockMgr[i];
	}
	return NULL;
}

void  DeleteAllSocketMgr()
{
	int i;
	for(i=0; i<g_nIocpNetWorkThreadCnt; i++)
	{
		g_pSockMgr[i].DisconnectAll();
	}
}

void DestroySocketMgr()
{
	if(g_pSockMgr == NULL)
		return;
	int i;
	for(i=0; i<g_nIocpNetWorkThreadCnt; i++)
	{
		if(g_pSockMgr)
		g_pSockMgr[i].Destroy();
	}
	delete []g_pSockMgr;
	g_pSockMgr = NULL;
}

DLLEXPORT_API VOID WINAPI IocpNetBegin()
{
		g_nIocpNETThreadHandleCnt = 0;
		g_nIocpNetWorkThreadCnt = 1;
		g_bIocpNetGCThreadRun=true;
		IocpNetDealQueMgr::Init();
		IocpNetCore::Init();
}
DLLEXPORT_API VOID WINAPI IocpNetEnd()
{
		DeleteAllSocketMgr();
		GCEApiSRTNetDestroy();
		g_bIocpNetGCThreadRun=false;
		IocpNetDealQueMgr::Destroy();
		IocpNetCore::Destroy();
		
		//不调用DeleteAllSocketMgr();GCEApiSRTNetDestroy();
		//如果带连接加了这句在服务器关闭的时候会出错
		DestroySocketMgr();
}
/////////////////////////////////////////////////////////////////////////
//不管是服务器端，还是客户端都调用这个初始化函数
DLLEXPORT_API BOOL WINAPI IocpNetInit(IOCPNET_RECV_DATA_CALLBACK pfnRecvDataCallback,
										   IOCPNET_DISCONNECT_CALLBACK pfnDisconnectCallback, 
										   IOCPNET_ACCEPT_CALLBACK pfnAcceptCallback, 
										   WORD wPort)
{
	if(pfnRecvDataCallback == NULL || pfnDisconnectCallback==NULL || pfnAcceptCallback == NULL)
	{
		//GCH_ETRACE(_T("IocpNetInit pfnRecvDataCallback == NULL || pfnDisconnectCallback==NULL || pfnAcceptCallback == NULL."));
		return FALSE;
	}
	g_pfnIocpReceiveData_cb = pfnRecvDataCallback;
	g_pfnIocpDisconnect_cb = pfnDisconnectCallback;
	g_pfnIocpAccept_cb = pfnAcceptCallback;

	DWORD dwThreadId = 0;
	HANDLE hThread = NULL;

	if(!IocpNetCore::InitNet(wPort))
	{
		//GCH_ETRACE(_T("IocpNetInit IocpNetCore::Init failed.port=%d"), wPort);
		return FALSE;
	}

	//计算工作线程数
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	g_nIocpNetWorkThreadCnt = systemInfo.dwNumberOfProcessors * 2 + 2;
	g_pSockMgr = new IocpNetSockMgr[g_nIocpNetWorkThreadCnt];
	
	int i;
	for(i=0; i<g_nIocpNetWorkThreadCnt; i++)
	{
		g_pSockMgr[i].Init(IocpNetCore::m_hRecvEvent);
	}

	//3.创建接收处理线程
	hThread = CreateThread(NULL, 0, IocpNetCore::DealRecvDataThread, NULL, 0, &dwThreadId);
	if(hThread==NULL)
	{
		//GCH_ETRACE(_T("IocpNetInit CreateThread IocpNetCore::DealRecvDataThread failed."));
		return FALSE;
	}
	g_hThreadHandle[g_nIocpNETThreadHandleCnt++] = hThread;

	//4.创建接收了连接线程
	if(pfnAcceptCallback != NULL && wPort != 0)
	{
		hThread = CreateThread(NULL, 0, IocpNetCore::AcceptThread, &wPort, 0, &dwThreadId);
		if(hThread==NULL)
		{
			//GCH_ETRACE(_T("IocpNetInit CreateThread IocpNetCore::AcceptThread failed."));
			return FALSE;
		}
		g_hThreadHandle[g_nIocpNETThreadHandleCnt++] = hThread;
	}

	Sleep(100);

	return TRUE;
}
BOOL GCEApiSRTNetDestroy()
{
	int i;

	for(i=0; i<g_nIocpNETThreadHandleCnt; i++)
	{
		TerminateThread(g_hThreadHandle[i], -1);
		CloseHandle(g_hThreadHandle[i]);
		//GCH_MTRACE(_T("GCEApiSRTNetDestroy g_hThreadHandle[%d]=%d."), i, g_hThreadHandle[i]);
	}
	g_nIocpNETThreadHandleCnt=0;
	IocpNetCore::Destroy();
	return TRUE;
}

DLLEXPORT_API char* WINAPI IocpNetGetIp(char*host,char*ip)
{
	WSADATA wsaData;
	PHOSTENT hostinfo; 
	char*ip_cstr;
	unsigned long lgIP;
	ip_cstr=NULL;
	lgIP = inet_addr(host);   
	//输入的IP字符串,这是适应WINCE
	if(lgIP != INADDR_NONE)  
	{
		strcpy(ip,host);
		return host;
	}

	if(WSAStartup(MAKEWORD(2,0), &wsaData)== 0)
	{ 
		if((hostinfo = gethostbyname(host)) != NULL)
		{
			ip_cstr = inet_ntoa (*(struct in_addr *)*hostinfo->h_addr_list); 
		} 
		WSACleanup();
	}

	if(ip_cstr)
	{
		strcpy(ip,ip_cstr);
	}
	else
	{
		strcpy(ip,"");
	}
	return ip;
}


DLLEXPORT_API HANDLE WINAPI IocpNetConnect(char* pszIP, WORD wPort, int nTimeOut)
{
	GCSTS_Sock_Info *pSockInfo = NULL;
	short nhKeyCalcul=0;

	SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
	if(s == INVALID_SOCKET)
	{
		//GCH_ETRACE(_T("IocpNetConnect s == INVALID_SOCKET"));
		return NULL;
	}

	SOCKADDR_IN addr;
	ZeroMemory(&addr, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(wPort);
	addr.sin_addr.s_addr = inet_addr(pszIP);

	if(connect(s, (SOCKADDR*)&addr, sizeof(addr))==SOCKET_ERROR)
	{
		//GCH_ETRACE(_T("IocpNetConnect connect to %s:%d faield."), pszIP, wPort);
		//GCH_ETRACE(_T("IocpNetConnect closesocket  start s=%d"), s);	
		closesocket(s);
		//GCH_ETRACE(_T("IocpNetConnect closesocket  end s=%d"), s);	
		return NULL;
	}

	//设置积极连接超时
	struct tcp_keepalive tcpin;
	tcpin.onoff=1;
	tcpin.keepaliveinterval=1000;
	tcpin.keepalivetime=6000;
	DWORD dwSize = 0;
	int err=WSAIoctl(s,SIO_KEEPALIVE_VALS, &tcpin, sizeof(tcpin), NULL,0, &dwSize,NULL,NULL);

	pSockInfo=g_pSockMgr[s%g_nIocpNetWorkThreadCnt].NewSocket(s, (SOCKADDR*)&addr);
	
	if(pSockInfo == NULL)
	{
		//GCH_ETRACE(_T("IocpNetConnect pHandle == NULL."));
	}
	if(false==g_bIocpNetGCThreadRun)return NULL; 
	return pSockInfo;
}

DLLEXPORT_API BOOL WINAPI IocpNetDisconnect(HANDLE handle)
{
	IocpNetSockMgr* pSockMgr = GetSocketMgr(handle);
	if(pSockMgr == NULL)
		return FALSE;

	pSockMgr->DeleteSock(handle);

	return TRUE;
}

DLLEXPORT_API int WINAPI IocpNetSend(HANDLE handle, int nLen, char* pData)
{
	IocpNetSockMgr* pSockMgr = GetSocketMgr(handle);
	if(pSockMgr == NULL)
		return FALSE;

	return pSockMgr->SendPacket(handle, nLen, pData);
}

DLLEXPORT_API char* WINAPI IocpNetGetPeerIP(HANDLE handle, char* pszIp)
{
	if(pszIp)
	{pszIp[0]=0;}
	IocpNetSockMgr* pSockMgr = GetSocketMgr(handle);
	if(pSockMgr == NULL)
		return NULL;

	return pSockMgr->GetPeerIP(handle, pszIp);
}

DLLEXPORT_API WORD WINAPI IocpNetGetPeerPort(HANDLE handle)
{
	IocpNetSockMgr* pSockMgr = GetSocketMgr(handle);
	if(pSockMgr == NULL)
		return 0;

	return pSockMgr->GetPeerPort(handle);
}
