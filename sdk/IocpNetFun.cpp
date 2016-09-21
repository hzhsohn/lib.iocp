#include "stdafx.h"
#include "IocpNetFun.h"

DWORD StrIp2DwIp(char* pszIp)
{
	return inet_addr(pszIp);
}
void DwIp2StrIp(DWORD dwIP, char* pszIp)
{
	in_addr in;
	in.S_un.S_addr = htonl(dwIP);

	char* pszTmpIp = inet_ntoa(in);

	if(pszTmpIp == NULL)
	{
		//GCH_ETRACE(_T("DwIp2StrIp"), _T("pszTmpIp == NULL"));	
		return;
	}

	strcpy(pszIp, pszTmpIp);
}
void GCH_GetIpPortFromAddr(SOCKADDR_IN addr, char* pszIp, WORD& wPort)
{
	char* pszTmpIp = inet_ntoa(addr.sin_addr);
	
	if(pszTmpIp == NULL)
	{
		//GCH_ETRACE(_T("GCH_GetIpPortFromAddr"), _T("pszTmpIp == NULL"));	
		return;
	}
#ifdef UNICODE
	strcpy(pszIp, s2ws(pszTmpIp).c_str());
#else
	strcpy(pszIp, pszTmpIp);
#endif
	wPort = htons(addr.sin_port);
}
	
