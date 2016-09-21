#pragma once
#include "windef.h"
#include <winsock2.h>

DWORD StrIp2DwIp(char* pszIp);
void DwIp2StrIp(DWORD dwIP, char* pszIp);
void GCH_GetIpPortFromAddr(SOCKADDR_IN addr, char* pszIp, WORD& wPort);

