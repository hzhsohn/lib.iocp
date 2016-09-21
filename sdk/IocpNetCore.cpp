#include "StdAfx.h"
#include "IocpNetCore.h"
#include "IocpNetSockMgr.h"
#include "IocpNet.h"
#include <MSTcpIP.h>
#include "IocpNetFun.h"

extern bool g_bIocpNetGCThreadRun;

SOCKET	IocpNetCore::m_sListen = 0;
HANDLE IocpNetCore::m_hRecvEvent = NULL;
BOOL IocpNetCore::m_bActive=FALSE;
WORD IocpNetCore::m_dwPacketNumber;

extern IocpNetSockMgr* g_pSockMgr;
extern int g_nIocpNetWorkThreadCnt;

extern IOCPNET_ACCEPT_CALLBACK			g_pfnIocpAccept_cb;
extern IOCPNET_RECV_DATA_CALLBACK		g_pfnIocpReceiveData_cb;
extern IOCPNET_DISCONNECT_CALLBACK		g_pfnIocpDisconnect_cb;

BOOL IocpNetCore::Init()
{
	return TRUE;
}

BOOL IocpNetCore::Destroy()
{
	m_bActive = FALSE;
	closesocket(m_sListen);
	return TRUE;
}

BOOL IocpNetCore::InitNet(WORD wPort)
{
	WORD wVersionRequested;
	WSADATA wsaData;

	wVersionRequested = MAKEWORD(1, 1);

	if(WSAStartup( wVersionRequested, &wsaData ) !=0 )
	{
		//GCH_ETRACE(_T("IocpNetCore::Init"), _T("WSAStartup failed."));
		return FALSE;
	}

	m_hRecvEvent =  CreateEvent(NULL,FALSE, FALSE, NULL);

	if(m_hRecvEvent==NULL)
	{
		//GCH_ETRACE(_T("IocpNetCore::Init"), _T("CreateEvent failed."));
		return FALSE;
	}	

	if(wPort != 0)
	{
		if(!InitListenSocket(wPort))
		{
			//GCH_ETRACE(_T("IocpNetCore::Init"), _T("InitListenSocket failed. port=%d"), wPort);
			return FALSE;
		}
	}

	m_bActive = TRUE;

	return TRUE;
}

BOOL IocpNetCore::InitListenSocket(WORD wPort)
{
	//¼àÌý¶Ë¿Ú
	m_sListen = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	if(m_sListen == INVALID_SOCKET)
	{
		//GCH_ETRACE(_T("IocpNetCore::Init"), _T("WSASocket failed."));
		return FALSE;
	}

	SOCKADDR_IN addr;
	ZeroMemory(&addr, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(wPort);

	if(bind(m_sListen, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR)
	{
		//GCH_ETRACE(_T("IocpNetCore::Init"), _T("bind failed. port=%d"), wPort);
		closesocket(m_sListen);
		return FALSE;
	}

	if(listen(m_sListen, 100) == SOCKET_ERROR)
	{
		//GCH_ETRACE(_T("IocpNetCore::Init"), _T("listen failed. m_sListen=%d"), m_sListen);
		closesocket(m_sListen);
		return FALSE;
	}

	return TRUE;
}

DWORD WINAPI IocpNetCore::AcceptThread(void* pVoid)
{
	SOCKADDR_IN addrAccept;
	int addrlen = sizeof(SOCKADDR_IN);
	ZeroMemory(&addrAccept, addrlen);
	HANDLE h = NULL;
	SOCKET sockAccept= INVALID_SOCKET;

	struct tcp_keepalive tcpin;
	tcpin.onoff=1;
	tcpin.keepaliveinterval=1000;
	tcpin.keepalivetime=6000;

	while(g_bIocpNetGCThreadRun)
	{
		//GCH_KTRACE(_T("IocpNetCore::AcceptThread 1"));
			if((sockAccept=accept(m_sListen, (SOCKADDR*)&addrAccept, &addrlen))==INVALID_SOCKET)
			{
				int err=WSAGetLastError();
				if(err != ERROR_IO_PENDING)
				{
					//GCH_ETRACE(_T("IocpNetCore::AcceptThread accept failed. err=%d"), GetLastError());
				}
				continue;
			}
		//GCH_KTRACE(_T("IocpNetCore::AcceptThread 2"));
			DWORD dwSize = 0;
			int err=WSAIoctl(sockAccept,SIO_KEEPALIVE_VALS, &tcpin, sizeof(tcpin),	NULL,0, &dwSize,NULL,NULL);
			if(err==SOCKET_ERROR)
			{
				//GCH_ETRACE(_T("IocpNetCore::AcceptThread WSAIoctl failed err=%d"), GetLastError());
				closesocket(sockAccept);
				continue;
			}
		//GCH_KTRACE(_T("IocpNetCore::AcceptThread 3"));
			if((h=g_pSockMgr[sockAccept%g_nIocpNetWorkThreadCnt].NewSocket(sockAccept, (SOCKADDR*)&addrAccept)) == NULL)
			{
				//GCH_ETRACE(_T("IocpNetCore::AcceptThread NewSocket = NULL"));
			}
			else
			{
				//GCH_KTRACE(_T("IocpNetCore::AcceptThread 4"));
				IocpNetDealQueMgr::InsertOtherData(h, GCE_ACCEPT_COONECT);
				SetEvent(m_hRecvEvent);
				//GCH_KTRACE(_T("IocpNetCore::AcceptThread 5"));
			}
	}
	return 0;
}

DWORD WINAPI IocpNetCore::DealRecvDataThread( void *pVoid)
{
	GCSTH_RecvDataInfo recvData;
	GCSTS_Sock_Info* pSock = NULL;
	while(m_bActive)
	{
		ZeroMemory(&recvData, sizeof(recvData));
		if(IocpNetDealQueMgr::GetDealData(&recvData))
		{
			pSock = (GCSTS_Sock_Info*)recvData.handle;
			if(recvData.enOpType == GCE_CLOSE_SOCKET)
			{
				g_pfnIocpDisconnect_cb(recvData.handle);
				if(pSock != NULL && pSock->s != 0)
					g_pSockMgr[pSock->s%g_nIocpNetWorkThreadCnt].DeleteSock(recvData.handle);				
			}
			else if(recvData.enOpType == GCE_ACCEPT_COONECT)
			{				
				char szIp[20]={0};
				memset(szIp, 0, sizeof(szIp));
				WORD wPort = 0;
				if(pSock != NULL && pSock->s != 0)
					g_pSockMgr[pSock->s%g_nIocpNetWorkThreadCnt].GetPeerAddress(recvData.handle, szIp, wPort);
				g_pfnIocpAccept_cb(recvData.handle, szIp, wPort);
			}
			else
			{
				g_pfnIocpReceiveData_cb(recvData.handle, recvData.nLen, recvData.buf);
			}
		}		
		else
		{
			WaitForSingleObject(m_hRecvEvent, INFINITE);
			//GCH_KTRACE("IocpNetCore::DealRecvDataThread 1");
		}
	}

	return 0;
}
