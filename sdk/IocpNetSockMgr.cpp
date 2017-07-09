#include "StdAfx.h"

#include "IocpNetSockMgr.h"
#include "IocpNetFun.h"

extern int g_nIocpNetWorkThreadCnt;
extern bool g_bIocpNetGCThreadRun;

IocpNetSockMgr::IocpNetSockMgr()
{}
IocpNetSockMgr::~IocpNetSockMgr()
{}

BOOL IocpNetSockMgr::Init(HANDLE hEvent)
{
	m_memSocket.Init(GCHMAC_FREE_SOCK_COUNT/g_nIocpNetWorkThreadCnt, GCHMAC_MAX_SOCK_COUNT/g_nIocpNetWorkThreadCnt);
	m_memOverlapped.Init(GCHMAC_FREE_OVERLAPPED_COUNT/g_nIocpNetWorkThreadCnt, GCHMAC_MAX_OVERLAPPED_COUNT/g_nIocpNetWorkThreadCnt);	

	m_hEvent = hEvent;

	//创建完成端口
	m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if(m_hCompletionPort==NULL)
	{
		//GCH_ETRACE(_T("IocpNetSockMgr::Init  CreateThread CreateIoCompletionPort failed."));
		return FALSE;
	}

	//创建工作线程
	DWORD dwThreadId = 0;
	HANDLE hThread = NULL;
	hThread = CreateThread(NULL, 0, WorkDealThread, this, 0, &dwThreadId);
	if(hThread==NULL)
	{		
		//GCH_ETRACE(_T("IocpNetSockMgr::Init CreateThread WorkDealThread failed."));
		return FALSE;
	}

	InitializeCriticalSection(&m_cs);
	return TRUE;
}
void IocpNetSockMgr::DisconnectAll(void)
{
	GCSTS_Sock_Info* pinfo;
	HANDLE h;
	for(GCH_Map_PPoint_Ite pos =  m_mapSocket.begin();pos !=  m_mapSocket.end();)
	{		
		pinfo=(GCSTS_Sock_Info*)(pos->first);
		h=pinfo;
		pos++;
		DeleteSock(h);
		//GCH_KTRACE("IocpNetSockMgr::DisconnectAll   handle=%p",h);
	}
}
BOOL IocpNetSockMgr::Destroy(void)
{
	DeleteCriticalSection(&m_cs);
	return TRUE;
}
void IocpNetSockMgr::DelOverlapped(HANDLE h, GCSTS_Base_Overlapped * pOverLapped)
{
	////GCH_KTRACE("IocpNetSockMgr::DelOverlapped h=%p, pOverLapped=%p", h, pOverLapped);
	if(h == NULL)
		return;

	EnterCriticalSection(&m_cs);
	GCSTS_Sock_Info* pSockInfo = (GCSTS_Sock_Info*)h;
	if(IsValidSock(pSockInfo))
	{
		GCH_Lst_Point_Ite pos;
		GCSTS_Base_Overlapped*   pOverlapped2 = NULL;
		for(pos=pSockInfo->lstSendOverlapped.begin(); pos!=pSockInfo->lstSendOverlapped.end(); pos++)
		{
			pOverlapped2 = (GCSTS_Base_Overlapped*)*pos;
			if(pOverlapped2 == pOverLapped)
			{
				m_memOverlapped.MyDel(pOverLapped);
				////GCH_KTRACE("IocpNetSockMgr::DelOverlapped h=%p, pOverLapped=%p", h, pOverLapped);
				pSockInfo->lstSendOverlapped.erase(pos);
				break;
			}		
		}	
	}
	
	LeaveCriticalSection(&m_cs);
}
GCSTS_Base_Overlapped * IocpNetSockMgr::NewOverlapped(GCSTS_Sock_Info * pSockInfo)
{
	EnterCriticalSection(&m_cs);
	GCSTS_Base_Overlapped *pOverlapped =  m_memOverlapped.MyNew();
	////GCH_KTRACE("IocpNetSockMgr::NewOverlapped pSockInfo=%p, pOverLapped=%p", pSockInfo, pOverlapped);
	if(pSockInfo != NULL && pOverlapped != NULL)
		pSockInfo->lstSendOverlapped.push_back((INT_PTR)pOverlapped);

	if(pOverlapped != NULL)
		memset(pOverlapped, 0, sizeof(GCSTS_Base_Overlapped));
	LeaveCriticalSection(&m_cs);
	return pOverlapped;
}

GCSTS_Sock_Info * IocpNetSockMgr::NewSocket(SOCKET s, SOCKADDR* pAddr)
{
	//保存sock信息，用于HANDLE
	GCSTS_Sock_Info * pSockInfo = NULL;
	HANDLE hCompletionPort;
	if(false==g_bIocpNetGCThreadRun)
	{
		closesocket(s);
		return NULL; 
	}

	EnterCriticalSection(&m_cs);

	//GCH_KTRACE(_T("IocpNetSockMgr::NewSocket s=%d"), s);
	pSockInfo = m_memSocket.MyNew();
	if(pSockInfo == NULL)
	{
		LeaveCriticalSection(&m_cs);
		return NULL;
	}
	//GCH_KTRACE("IocpNetSockMgr::NewSocket 1");
	pSockInfo->s = s;
	pSockInfo->bActive = TRUE;
	GCH_GetIpPortFromAddr(*(SOCKADDR_IN*)pAddr, pSockInfo->szIp, pSockInfo->wPort);

	//GCH_KTRACE("IocpNetSockMgr::NewSocket 2");
	hCompletionPort=CreateIoCompletionPort((HANDLE) s, m_hCompletionPort,(INT_PTR)(pSockInfo),0);
	if(hCompletionPort==NULL)
	{
		m_memSocket.MyDel(pSockInfo);
		closesocket(s);
		LeaveCriticalSection(&m_cs);
		return NULL;
	}

	//GCH_KTRACE("IocpNetSockMgr::NewSocket 3");
	//读取头信息
	pSockInfo->pRecvOverlapped = NewOverlapped(NULL);
	if(pSockInfo->pRecvOverlapped == NULL)
	{
		m_memSocket.MyDel(pSockInfo);
		closesocket(s);
		LeaveCriticalSection(&m_cs);
		return NULL;
	}
	//GCH_KTRACE("IocpNetSockMgr::NewSocket 4");
	ZeroMemory(pSockInfo->pRecvOverlapped, sizeof(GCSTS_Base_Overlapped));

	if(!AddSock(pSockInfo))
	{
		DeleteSock(pSockInfo);
		LeaveCriticalSection(&m_cs);
		return NULL;
	}

	//GCH_KTRACE("IocpNetSockMgr::NewSocket 5");
	if(!RecvDataPart(pSockInfo, 0,pSockInfo->pRecvOverlapped, GCE_Operate_Recv, FALSE))
	{
		DeleteSock(pSockInfo);	
		LeaveCriticalSection(&m_cs);
		return NULL;
	}

	LeaveCriticalSection(&m_cs);

	//GCH_KTRACE("IocpNetSockMgr::NewSocket 6");
	return pSockInfo;
}

BOOL IocpNetSockMgr::RecvDataPart(HANDLE handle, DWORD dwBytesTransferred, GCSTS_Base_Overlapped *pOverlapped, int nOpCode, BOOL bClose)
{	
	int nRet;
	DWORD dwFlags=0;
	GCSTS_Sock_Info* pSockInfo = (GCSTS_Sock_Info*)handle;
	
	if(!IocpNetSockMgr::IsValidSock(pSockInfo))
	{
		return FALSE;
	}

	EnterCriticalSection(&m_cs);
	pOverlapped->dwOperatCode = nOpCode;
	pOverlapped->wLeft = PACKET_LENGTH;
	DWORD dwReceiveBytes = 0;

	pOverlapped->wsabuf.buf=&pOverlapped->szBuff[0];
	pOverlapped->wsabuf.len=pOverlapped->wLeft;
	nRet=WSARecv(pSockInfo->s,&pOverlapped->wsabuf,1,&dwReceiveBytes,&dwFlags,&pOverlapped->Overlapped,NULL);
	if(SOCKET_ERROR==nRet)
	{
		DWORD dwErr = WSAGetLastError();
		if (dwErr != ERROR_IO_PENDING)
		{
			if(bClose)
				CloseSocket(pSockInfo);
			LeaveCriticalSection(&m_cs);
			return FALSE;
		}
	}
	LeaveCriticalSection(&m_cs);
	return TRUE;
}
BOOL IocpNetSockMgr::IsExist(GCSTS_Sock_Info* pSockInfo)
{
	EnterCriticalSection(&m_cs);
	BOOL bExist = FALSE;
	GCH_Map_PPoint_Ite pos = m_mapSocket.find((INT_PTR)pSockInfo);
	
	if(pos != m_mapSocket.end())
		bExist = TRUE;
	LeaveCriticalSection(&m_cs);
	return bExist;	
}
BOOL IocpNetSockMgr::IsValidSock(GCSTS_Sock_Info* pSockInfo)
{
	EnterCriticalSection(&m_cs);
	BOOL ret=FALSE;
	if(pSockInfo)
	{
		GCH_Map_PPoint_Ite pos = m_mapSocket.find((INT_PTR)pSockInfo);	
		if(pos != m_mapSocket.end())
		{
			ret= pSockInfo->bActive;
		}
	}
	LeaveCriticalSection(&m_cs);
	return ret;	
}

BOOL IocpNetSockMgr::AddSock(GCSTS_Sock_Info* pSockInfo)
{
	//GCH_KTRACE(_T("IocpNetSockMgr::AddSock  1  handle=%p"), pSockInfo);
	EnterCriticalSection(&m_cs);

	m_mapSocket[(INT_PTR)pSockInfo] = (INT_PTR)pSockInfo;
	//GCH_KTRACE(_T("IocpNetSockMgr::AddSock pSockInfo=%p, pSockInfo->s=%d, SocketCount=%d, memSocketCount=%d"), pSockInfo, pSockInfo->s, m_mapSocket.size(), m_memSocket.GetUseCount());
	LeaveCriticalSection(&m_cs);
	
	return TRUE;
}
void IocpNetSockMgr::CloseSocket(HANDLE handle)
{
	//GCH_KTRACE(_T("IocpNetSockMgr::CloseSocket 1 handle=%p"), handle);
	
	GCSTS_Sock_Info* pSockInfo = (GCSTS_Sock_Info*)handle;
	//GCH_KTRACE(_T("IocpNetSockMgr::CloseSocket 2 handle=%p"), handle);
	if(IsValidSock(pSockInfo))
	{
		
		EnterCriticalSection(&m_cs);
		//GCH_KTRACE(_T("IocpNetSockMgr::CloseSocket pSockInfo=%p, pSockInfo->s=%d"), pSockInfo, pSockInfo->s);

		if(pSockInfo->pRecvOverlapped != NULL)
		{
			m_memOverlapped.MyDel(pSockInfo->pRecvOverlapped);
			pSockInfo->pRecvOverlapped = NULL;
		}
		GCH_Lst_Point_Ite pos;
		GCSTS_Base_Overlapped*   pOverlapped = NULL;
		for(pos=pSockInfo->lstSendOverlapped.begin(); pos!=pSockInfo->lstSendOverlapped.end(); pos++)
		{
			pOverlapped = (GCSTS_Base_Overlapped*)*pos;
			m_memOverlapped.MyDel(pOverlapped);
		}
		pSockInfo->lstSendOverlapped.clear();
		shutdown(pSockInfo->s, 0);
		closesocket(pSockInfo->s);
		pSockInfo->bActive = FALSE;

		IocpNetDealQueMgr::InsertOtherData(pSockInfo, GCE_CLOSE_SOCKET);
		SetEvent(m_hEvent);
		LeaveCriticalSection(&m_cs);
	}
	//GCH_KTRACE(_T("IocpNetSockMgr::CloseSocket 3 handle=%p"), handle);
	
}
BOOL IocpNetSockMgr::DeleteSock(HANDLE handle)
{
	if(handle == NULL)
		return FALSE;

	//GCH_KTRACE(_T("IocpNetSockMgr::DeleteSock 1"));
	EnterCriticalSection(&m_cs);
	GCH_Map_PPoint_Ite pos =  m_mapSocket.find((INT_PTR)handle);
	if(pos != m_mapSocket.end())
	{		
		m_mapSocket.erase((INT_PTR)handle);		
		GCSTS_Sock_Info* pSockInfo = (GCSTS_Sock_Info*)handle;
		//GCH_XTRACE(_T("IocpNetSockMgr::DeleteSock ,pSockInfo=%p ,pSockInfo->s=%d, SocketCount=%d, memSocketCount=%d"), pSockInfo, pSockInfo->s, m_mapSocket.size(), m_memSocket.GetUseCount());		
		
		if(pSockInfo->pRecvOverlapped != NULL)
		{
			m_memOverlapped.MyDel(pSockInfo->pRecvOverlapped);
			pSockInfo->pRecvOverlapped = NULL;
		}
		//GCH_KTRACE(_T("IocpNetSockMgr::DeleteSock 2"));

		GCH_Lst_Point_Ite pos;
		GCSTS_Base_Overlapped*   pOverlapped = NULL;
		for(pos=pSockInfo->lstSendOverlapped.begin(); pos!=pSockInfo->lstSendOverlapped.end(); pos++)
		{
			pOverlapped = (GCSTS_Base_Overlapped*)*pos;
			m_memOverlapped.MyDel(pOverlapped);
		}
		pSockInfo->lstSendOverlapped.clear();
		//GCH_KTRACE(_T("IocpNetSockMgr::DeleteSock 3"));

		if(pSockInfo->bActive)
		{
			shutdown(pSockInfo->s, 0);
			closesocket(pSockInfo->s);
			pSockInfo->bActive = FALSE;
			pSockInfo->s = 0;
		}
		//GCH_KTRACE(_T("IocpNetSockMgr::DeleteSock 4"));
		m_memSocket.MyDel(pSockInfo);
		//GCH_KTRACE(_T("IocpNetSockMgr::DeleteSock 5"));
	}
	LeaveCriticalSection(&m_cs);
	//GCH_KTRACE(_T("IocpNetSockMgr::DeleteSock 6"));
	return TRUE;
}

int IocpNetSockMgr::SendPacket(HANDLE handle, int nLen, char* pData)
{
	int nRet;
	DWORD dwSendBufferlen;

	GCSTS_Sock_Info *pSockInfo = (GCSTS_Sock_Info*)handle;
	if(nLen < 0 || nLen > PACKET_LENGTH)
	{
		//GCH_ETRACE(_T("IocpNetSockMgr::SendPacket , nLen==%d"), nLen);
		return -1;
	}

	if(!IocpNetSockMgr::IsValidSock(pSockInfo) || pSockInfo->s == 0)
	{
		return -1;
	}
	
	EnterCriticalSection(&m_cs);
	GCSTS_Base_Overlapped *pOverlapped =  NewOverlapped(pSockInfo);
	if(pOverlapped == NULL)
	{
		CloseSocket(pSockInfo);
		LeaveCriticalSection(&m_cs);
		return -1;
	}
	pOverlapped->dwOperatCode = GCE_Operate_Send;
		

	//将包体打包到内
	memcpy(&pOverlapped->szBuff[0], pData, nLen);

	pOverlapped->wLeft = (WORD)(nLen);

	pOverlapped->wsabuf.buf=pOverlapped->szBuff;
	pOverlapped->wsabuf.len=pOverlapped->wLeft;
	nRet=WSASend(pSockInfo->s, &pOverlapped->wsabuf, 1, &dwSendBufferlen, 0, &pOverlapped->Overlapped,NULL);
	if(SOCKET_ERROR==nRet)
	{
		DWORD dwErr = WSAGetLastError();
		if (dwErr != ERROR_IO_PENDING)
		{
			//GCH_ETRACE(_T("IocpNetSockMgr::SendPacket , WriteFile failed. dwErr=%d, pSockInfo=%p, s=%d, pOverlapped=%p, wLeft=%d"),
			//	dwErr, pSockInfo, pSockInfo->s, pOverlapped, pOverlapped->wLeft);
			CloseSocket(pSockInfo);
			LeaveCriticalSection(&m_cs);
			return -1;
		}
	}

	LeaveCriticalSection(&m_cs);
	return dwSendBufferlen;
}
BOOL IocpNetSockMgr::SetOutDat(HANDLE handle, INT_PTR pData, BYTE yDataType)
{
	if(handle == NULL)
		return FALSE;

	EnterCriticalSection(&m_cs);
	GCSTS_Sock_Info *pSockInfo = (GCSTS_Sock_Info*)handle;

	GCH_Map_PPoint_Ite pos = m_mapSocket.find((INT_PTR)pSockInfo);
	if(pos != m_mapSocket.end())
	{	
		pSockInfo->yDataType = yDataType;
		pSockInfo->m_pOutDat = pData;
	}
	
	LeaveCriticalSection(&m_cs);
	return TRUE;
}
INT_PTR IocpNetSockMgr::GetOutDat(HANDLE handle, BYTE& yDataType)
{
	if(handle == NULL)
		return FALSE;
	
	INT_PTR pOutDat = NULL;
	EnterCriticalSection(&m_cs);
	GCSTS_Sock_Info *pSockInfo = (GCSTS_Sock_Info*)handle;
	GCH_Map_PPoint_Ite pos = m_mapSocket.find((INT_PTR)pSockInfo);
	if(pos != m_mapSocket.end())
	{
		yDataType = pSockInfo->yDataType;
		pOutDat = pSockInfo->m_pOutDat;
	}
	
	LeaveCriticalSection(&m_cs);
	return pOutDat;
}

char* IocpNetSockMgr::GetPeerIP(HANDLE handle, char* pszIp)
{
	if(handle == NULL)
		return 0;

	GCSTS_Sock_Info *pSockInfo = (GCSTS_Sock_Info*)handle;

	if(IocpNetSockMgr::IsValidSock(pSockInfo))
	{
		strcpy(pszIp, pSockInfo->szIp);
		return pszIp;
	}
	return NULL;

}
WORD IocpNetSockMgr::GetPeerPort(HANDLE handle)
{
	WORD wPort = 0;

	if(handle == NULL)
		return 0;

	GCSTS_Sock_Info *pSockInfo = (GCSTS_Sock_Info*)handle;

	if(IocpNetSockMgr::IsValidSock(pSockInfo))
	{
		wPort = ntohs(pSockInfo->wPort);
	}
	return wPort;
}
BOOL IocpNetSockMgr::GetPeerAddress(HANDLE handle, char* pszIp, WORD& wPort)
{
	if(handle == NULL)
		return 0;

	GCSTS_Sock_Info *pSockInfo = (GCSTS_Sock_Info*)handle;

	if(IocpNetSockMgr::IsValidSock(pSockInfo))
	{
		wPort = ntohs(pSockInfo->wPort);
		strcpy(pszIp, pSockInfo->szIp);
	}
	return TRUE;
}
void IocpNetSockMgr::OnCompleteEvent(DWORD dwBytesTransferred, HANDLE h, GCSTS_Base_Overlapped *pOverlapped)
{
	DWORD dwOperatCode = 0;
	
	GCSTS_Sock_Info *pSockInfo = (GCSTS_Sock_Info*)h;
		
	if(!IocpNetSockMgr::IsValidSock(pSockInfo))
	{
		return;
	}
	
	EnterCriticalSection(&m_cs);
	if(pOverlapped != pSockInfo->pRecvOverlapped)
	{
		GCH_Lst_Point_Ite pos;
		GCSTS_Base_Overlapped*   pOverlapped2 = NULL;
		for(pos=pSockInfo->lstSendOverlapped.begin(); pos!=pSockInfo->lstSendOverlapped.end(); pos++)
		{
			pOverlapped2 = (GCSTS_Base_Overlapped*)*pos;
			if(pOverlapped2 == pOverlapped)
			{
				break;
			}
		}

		if(pos==pSockInfo->lstSendOverlapped.end())
		{
			CloseSocket(h);
			LeaveCriticalSection(&m_cs);
			return;
		}
	}

	dwOperatCode = pOverlapped->dwOperatCode;
	if(GCE_Operate_Send == dwOperatCode)
	{
		//发送完成
		SendComplete(dwBytesTransferred, h, (GCSTS_Base_Overlapped*)pOverlapped);	
	}
	else if(GCE_Operate_Recv == dwOperatCode)
	{
		//接收数据
		RecvComplete(dwOperatCode, dwBytesTransferred, h,pOverlapped, pSockInfo);
	}

	LeaveCriticalSection(&m_cs);
}
BOOL IocpNetSockMgr::SendComplete(DWORD dwBytesTransferred, HANDLE h, GCSTS_Base_Overlapped *pOverlapped)
{
	DelOverlapped(h, pOverlapped);
	return TRUE;
}
BOOL IocpNetSockMgr::RecvComplete(DWORD dwOperatCode, DWORD dwBytesTransferred, HANDLE h, GCSTS_Base_Overlapped *pOverlapped, GCSTS_Sock_Info *pSockInfo)
{
	//插入数据
	IocpNetDealQueMgr::InsertWaitDealData(h,
		GCE_OP_NONE, 
		dwBytesTransferred, 
		pOverlapped->szBuff);
	SetEvent(m_hEvent);
				
	//接收新的包头
	if(!RecvDataPart(h, dwBytesTransferred,  pOverlapped, GCE_Operate_Recv))
	{
		return FALSE;
	}
	return TRUE;
}
DWORD WINAPI IocpNetSockMgr::WorkDealThread( void *pVoid) 
{
	IocpNetSockMgr* pSockMgr = (IocpNetSockMgr*)pVoid;
	HANDLE * h = NULL;
	GCSTS_Base_Overlapped *pOverlapped=NULL; 

	while(g_bIocpNetGCThreadRun)
	{
		//GCH_KTRACE(_T("IocpNetCore::WorkDealThread 1"));
		DWORD dwBytesTransferred = 0;

		BOOL ret =GetQueuedCompletionStatus(pSockMgr->m_hCompletionPort, &dwBytesTransferred,(LPDWORD)&h,  (LPOVERLAPPED *)&pOverlapped, INFINITE);
		if(!pSockMgr->ErrDeal(ret, dwBytesTransferred, h, pOverlapped))
		{
			continue;
		}
		//GCH_KTRACE(_T("IocpNetCore::WorkDealThread 2"));
		pSockMgr->OnCompleteEvent(dwBytesTransferred, h, pOverlapped);
		//GCH_KTRACE(_T("IocpNetCore::WorkDealThread 3"));

	}
	return 0;
}
BOOL IocpNetSockMgr::ErrDeal(BOOL ret, DWORD dwBytesTransferred,  HANDLE h, GCSTS_Base_Overlapped *pOverlapped)
{
	int err = WSAGetLastError();

	//GCH_ETRACE(_T("IocpNetSockMgr::ErrDeal "), _T("ret=%d, err=%d, dwBytesTransferred=%d, h=%p, pOverlapped=%p, "), ret, err, dwBytesTransferred, h, pOverlapped);
	//	If a socket handle associated with a completion port is closed, GetQueuedCompletionStatus returns ERROR_SUCCESS, 
	//with lpNumberOfBytes equal zero.
	if(dwBytesTransferred==0/* && err == ERROR_SUCCESS*/)
	{
		CloseSocket(h);
		return FALSE;
	}

	//	If *lpOverlapped is NULL and the function does not dequeue a completion packet from the completion port, the return value is zero. 
	//The function does not store information in the variables pointed to by the lpNumberOfBytes and lpCompletionKey parameters. To get extended error
	//information, call GetLastError. If the function did not dequeue a completion packet because the wait timed out, GetLastError returns WAIT_TIMEOUT.
	if(ret == FALSE && pOverlapped == NULL && err == WAIT_TIMEOUT)
	{
		return FALSE;
	}
	//	If *lpOverlapped is not NULL and the function dequeues a completion packet for a failed I/O operation from the completion port, 
	//the return value is zero. The function stores information in the variables pointed to by lpNumberOfBytes, lpCompletionKey, 
	//and lpOverlapped. To get extended error information, call GetLa stError.
	if(ret == FALSE && pOverlapped != NULL && err != ERROR_IO_PENDING)
	{
		CloseSocket(h);
		return FALSE;
	}

	//	If the function dequeues a completion packet for a successful I/O operation from the completion port, the return value is nonzero. 
	//The function stores information in the variables pointed to by the lpNumberOfBytesTransferred, lpCompletionKey, and lpOverlapped parameters.
	if(ret == TRUE)
	{
		return TRUE;
	}

	CloseSocket(h);

	return FALSE;
}
