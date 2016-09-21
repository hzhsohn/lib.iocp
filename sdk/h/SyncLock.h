// TUDP.cpp : 定义 DLL 应用程序的入口点。
//

#include "stdafx.h"
#pragma once
///////////////////////////////////////////////////////////////////////////////
// class CCriticalSection - 线程临界区互斥类
//
// 说明:
// 1. 此类用于多线程环境下临界区互斥，基本操作有 Lock、Unlock 和 TryLock；
// 2. 线程内允许嵌套调用 Lock，嵌套调用后必须调用相同次数的 Unlock 才可解锁；

class CCriticalSection
{
private:
	CRITICAL_SECTION m_Lock;

protected:
	virtual void InvokeInitialize() 
	{ 
		Lock(); 
	}
	virtual void InvokeFinalize() 
	{ 
		Unlock(); 
	}

public:
	CCriticalSection()
	{
		InitializeCriticalSection(&m_Lock);
	}

	~CCriticalSection()
	{
		DeleteCriticalSection(&m_Lock);
	}

	// 加锁
	void Lock()
	{
		EnterCriticalSection(&m_Lock);
	}

	//-----------------------------------------------------------------------------
	// 描述: 解锁
	//-----------------------------------------------------------------------------
	void Unlock()
	{
		LeaveCriticalSection(&m_Lock);
	}
};

class CAutoSynchronizer
{
private:
	CCriticalSection *m_pObject;
public:
	CAutoSynchronizer(CCriticalSection* pObject)
	{ 
		m_pObject = pObject; 
		m_pObject->Lock(); 
	}

	~CAutoSynchronizer()
	{ 
		m_pObject->Unlock(); 
	}
};
