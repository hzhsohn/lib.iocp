#pragma once
#include "stdafx.h"
#include <stdio.h>
#include <list>
#include <map>
#include <wtypes.h>
#include "SyncLock.h"

using namespace std;

#pragma warning(disable:4251)

template <typename Type> class CMemMgr
{
public:
	typedef list<ULONG_PTR>			LstMem;
	typedef LstMem::iterator		LstMem_Ite;
	typedef map<ULONG_PTR, int>		MapMem;
	typedef MapMem::iterator		MapMem_Ite;

#define MAX_HASH_INDEX				10000
public:
	CMemMgr(void);
	~CMemMgr(void);

	//szFreeMem:至少保留的空闲内容数量
	//szMaxMemCnt：允许分配的最大内存数量
	BOOL Init(size_t szFreeMem, size_t szMaxMemCnt);
	BOOL Destroy();

	Type* MyNew();
	BOOL MyDel(Type* p);

	size_t GetUseCount();

private:
	BOOL CreateMemBlock();

	CCriticalSection	m_cs;

	size_t m_szFreeMem;
	size_t m_szMaxMemCnt;
	size_t m_szUseMemCnt;

	LstMem			m_lstFreeMem;	
	//哈希表，检查是否有内存泄漏，或重复删除内存，最后清理内存
	MapMem			m_mapMemMgr[MAX_HASH_INDEX];			
};

template<typename Type> CMemMgr<Type>::CMemMgr(void)
{
	m_szFreeMem = 0;
	m_szMaxMemCnt = 0;
	m_szUseMemCnt = 0;
}

template<typename Type> CMemMgr<Type>::~CMemMgr(void)
{
	Destroy();
}

template<typename Type> BOOL CMemMgr<Type>::Init(size_t szFreeMem, size_t szMaxMemCnt)
{
	CAutoSynchronizer sync(&m_cs);

	m_szFreeMem = szFreeMem;
	m_szMaxMemCnt = szMaxMemCnt;

	if(szFreeMem > szMaxMemCnt)
		return FALSE;

	return CreateMemBlock();
}
template<typename Type> BOOL CMemMgr<Type>::Destroy()
{
	CAutoSynchronizer sync(&m_cs);

	Type* pType = NULL;	
	LstMem_Ite lstPos;
	for(lstPos=m_lstFreeMem.begin(); lstPos!=m_lstFreeMem.end(); lstPos++)
	{
		pType = (Type*)*lstPos;
		delete pType;
	}
	m_lstFreeMem.clear();

	MapMem_Ite mapPos;
	int i=0;
	for(i=0; i<MAX_HASH_INDEX; i++)
	{
		for(mapPos=m_mapMemMgr[i].begin(); mapPos!=m_mapMemMgr[i].begin(); mapPos++)
		{
			pType = (Type*)mapPos->first;
			delete pType;
		}
		m_mapMemMgr[i].clear();
	}	

	return TRUE;
}
template<typename Type> Type* CMemMgr<Type>::MyNew()
{
	CAutoSynchronizer sync(&m_cs);
	if(m_szUseMemCnt >= m_szMaxMemCnt)
		return NULL;

	size_t szAllocalCnt = 0;
	Type* pType = NULL;

	if(m_lstFreeMem.size() <= 0)
	{
		if(!CreateMemBlock())
			return NULL;
	}
	
	pType = (Type*)m_lstFreeMem.front();
	m_lstFreeMem.pop_front();		

	int nIndex = (ULONG)pType%MAX_HASH_INDEX;
	m_mapMemMgr[nIndex][(ULONG_PTR)pType] = 1;

	m_szUseMemCnt++;
	return pType;
}

template<typename Type> BOOL CMemMgr<Type>::MyDel(Type* p)
{
	if(p == NULL)
		return TRUE;

	CAutoSynchronizer sync(&m_cs);
	
	int nIndex = (ULONG)p%MAX_HASH_INDEX;
	MapMem_Ite pos = m_mapMemMgr[nIndex].find((ULONG)p);
	if(pos != m_mapMemMgr[nIndex].end())
	{
		m_mapMemMgr[nIndex].erase(pos);
		if(m_lstFreeMem.size() < m_szFreeMem)
			m_lstFreeMem.push_back((ULONG)p);
		else
			delete p;
		m_szUseMemCnt--;
	}	
	else
	{
		//GCH_ETRACE(_T("CMemMgr::MyDel"), _T("p=%d\n"), p);
		return FALSE;
	}

	return TRUE;
}

template<typename Type> size_t CMemMgr<Type>::GetUseCount()
{
	return m_szUseMemCnt;
}
template<typename Type> BOOL CMemMgr<Type>::CreateMemBlock()
{
	size_t szAllocalCnt = m_szFreeMem;
	size_t i;
	Type* pType = NULL;	
	if(szAllocalCnt == 0)
		szAllocalCnt = 1; 
	for(i=0; i<szAllocalCnt; i++)
	{
		pType = new Type;	
		if(pType == NULL)
			return FALSE;
		m_lstFreeMem.push_back((ULONG_PTR)pType);

		if(m_szUseMemCnt+m_lstFreeMem.size() >= m_szMaxMemCnt)
			break;
	}
	return TRUE;
}
