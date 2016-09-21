#pragma once

/*
* Copyright (c) 2004, game carrier
* All rights reserved.

* Filename: GCSMemManager.h
* Description:

* Author:bloodspider
* Time:2007-3-31   14:59
* Modify connect:

*/

#include <stdio.h>
#include <list>
#include <wtypes.h>

using namespace std;

#pragma warning(disable:4251)

/** @defgroup Util util
*  基本类
*  @author  bloodspider
*  @date    2007-7-26 17:19
*  @{
*/

/**@defgroup UtilClass class
* 内存管理模板类
* @author  bloodspider
* @date    12:7:2007   15:33
* @{
*/
template <typename Type> class IocpNetMemManager
{
public:
	IocpNetMemManager(void);
	~IocpNetMemManager(void);

	/** 初始化 \n
	*  @param[in] szSize 分配内存时的"Type"数量
	*  @param[in] szMaxBlockCnt 最大块数
	*  @param[in] szFreeBloackCnt 空闲块数
	*  @param[in] bZero 内存是否清0
	*  @return 
	*/
	BOOL Init(size_t szSize, size_t szMaxBlockCnt, size_t szFreeBloackCnt, BOOL bZero=TRUE);

	/** 销毁 \n
	*/
	VOID Destroy();
	/** 初始化 \n
	*  @param[in] bZero 内存是否清0
	*  @return 返回"Type"指针
	*/
	Type* MyNew(BOOL bZero=TRUE);

	/** 初始化 \n
	*  @param[in] p 被删除的指针
	*  @return 
	*/
	BOOL MyDel(Type* p);

	int GetCount();

private:

	/** 安全锁,保存证类是线程安全的 */
	CRITICAL_SECTION m_cs;

	/** 空闲内存链表 */
	list<INT_PTR>	m_lstFreeMem;
	/** 正在使用的内存链表 */
	list<INT_PTR>	m_lstUseMem;

	/** 分配内存时的"Type"数量 */
	size_t m_szSize;
	/** 最大块数 */
	size_t	m_szMaxBlockCnt;
	/** 空闲块数 */
	size_t	m_szFreeBloackCnt;
};

template<typename Type> IocpNetMemManager<Type>::IocpNetMemManager(void)
{
	InitializeCriticalSection(&m_cs);
}

template<typename Type> IocpNetMemManager<Type>::~IocpNetMemManager(void)
{
	Destroy();
	DeleteCriticalSection(&m_cs);
}

/*
*Function name:
*
*Description:
*
*Input:	
*
*Output:
*
*Return:HRESULT
*
* Author:bloodspider
* Time:31:3:2007   15:00
*/
template<typename Type> BOOL IocpNetMemManager<Type>::Init(size_t szSize, size_t szMaxBlockCnt, size_t szFreeBloackCnt, BOOL bZero)
{
	m_szSize = szSize;
	m_szMaxBlockCnt = szMaxBlockCnt;
	m_szFreeBloackCnt = szFreeBloackCnt;

	size_t i;
	Type* pType = NULL;
	BOOL ret = TRUE;

	for(i=0; i<m_szFreeBloackCnt; i++)
	{
		pType = new Type[m_szSize];
		if(pType != NULL)
		{
			if(bZero)
			{
				ZeroMemory(pType, sizeof(Type)*m_szSize);
			}
			m_lstFreeMem.push_back((INT_PTR)pType);	
		}
		else{
			ret = FALSE;
			Destroy();
			break;
		}
	}

	return ret;
}
template<typename Type> int IocpNetMemManager<Type>::GetCount()
{
	return m_lstUseMem.size();
}
/*
*Function name:
*
*Description:
*
*Input:	
*
*Output:
*
*Return:HRESULT
*
* Author:bloodspider
* Time:31:3:2007   15:00
*/

template<typename Type> VOID IocpNetMemManager<Type>::Destroy()
{
	EnterCriticalSection(&m_cs);

	list<INT_PTR>::iterator pos;	
	Type* pType = NULL;

	for( pos = m_lstFreeMem.begin(); pos != m_lstFreeMem.end(); pos++ )
	{
		pType = (Type*)*pos;
		delete[] pType;
	}
	m_lstFreeMem.clear();

	for( pos = m_lstUseMem.begin(); pos != m_lstUseMem.end(); pos++ )
	{
		pType = (Type*)*pos;
		delete[] pType;
	}
	m_lstUseMem.clear();

	LeaveCriticalSection(&m_cs);
}

/*
*Function name:
*
*Description:
*
*Input:	
*
*Output:
*
*Return:HRESULT
*
* Author:bloodspider
* Time:31:3:2007   15:00
*/

template<typename Type> Type* IocpNetMemManager<Type>::MyNew(BOOL bZero)
{
	Type* pType = NULL;

	if(m_lstUseMem.size() >= m_szMaxBlockCnt)
	{
		return NULL;
	}

	EnterCriticalSection(&m_cs);

	if(m_lstFreeMem.size() > 0)
	{
		pType = (Type*)m_lstFreeMem.front();
		m_lstFreeMem.pop_front();
		m_lstUseMem.push_back((INT_PTR)pType);	
	}
	else
	{
		pType = new Type[m_szSize];
		if(pType != NULL && bZero)
		{
			ZeroMemory(pType, sizeof(Type)*m_szSize);			
		}
		m_lstUseMem.push_back((INT_PTR)pType);	
	}

	LeaveCriticalSection(&m_cs);
	return pType;
}

/*
*Function name:
*
*Description:回收内存,理论上,先申请的内存,请使用完.所以这里用链表不会影响效率
*
*Input:	
*
*Output:
*
*Return:HRESULT
*
* Author:bloodspider
* Time:31:3:2007   15:00
*/

template<typename Type> BOOL IocpNetMemManager<Type>::MyDel(Type* p)
{
	if(p == NULL)
	{
		return TRUE;
	}

	Type* pType = (Type*)p;	
	list<INT_PTR>::iterator pos;	

	EnterCriticalSection(&m_cs);

	for( pos = m_lstUseMem.begin(); pos != m_lstUseMem.end(); pos++ )
	{
		pType = (Type*)*pos;
		if(pType == p)
		{
			m_lstUseMem.erase(pos);
			break;
		}		
	}

	if(m_lstFreeMem.size() < m_szFreeBloackCnt)
	{	
		//暂时没有加安全保护，地址相同的p可能会被加入多次
		m_lstFreeMem.push_back((INT_PTR)p);	

	}
	else
	{
		delete[] p;
	}

	LeaveCriticalSection(&m_cs);

	return TRUE;
}

/** @}*/ // class

/** @}*/ // util