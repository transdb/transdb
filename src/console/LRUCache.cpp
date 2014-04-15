//
//  LRUCache.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 9/6/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

LRUCache::LRUCache()
{
	m_first = NULL;
	m_last	= NULL;

	//prepare cache
    m_pCRecCache = new FixedPool<CRec>(g_LRUCacheMemReserve);
}

LRUCache::~LRUCache()
{
    delete m_pCRecCache;
}

void LRUCache::put(uint64 x)
{
	CRec *pCRec;

	//remove from chain -> add to end
	remove(x);

	//init 1st
	if(m_first == NULL)
	{
		m_first = m_pCRecCache->alloc();
		pCRec = m_first;
	}
	else
    {
		pCRec = m_pCRecCache->alloc();
    }

	//init
	pCRec->m_key = x;
	pCRec->m_pPrev = m_last;
	pCRec->m_pNext = NULL;

	//set last
	if(m_last)
		m_last->m_pNext = pCRec;

	m_last = pCRec;

	//insert to map
	m_map.insert(LRUMap::value_type(x, pCRec));
}

void LRUCache::remove(uint64 x)
{
	LRUMap::iterator itr;
	CRec *pCRecFind;

	itr = m_map.find(x);
	if(itr != m_map.end())
	{
		pCRecFind = itr->second;

		//change chain
		if(pCRecFind == m_last)
		{
            m_last = pCRecFind->m_pPrev;
		}

		if(pCRecFind == m_first)
		{
			m_first = pCRecFind->m_pNext;
			if(m_first)
				m_first->m_pPrev = NULL;
		}

		if(pCRecFind->m_pPrev)
		{
			pCRecFind->m_pPrev->m_pNext = pCRecFind->m_pNext;
			if(pCRecFind->m_pNext)
				pCRecFind->m_pNext->m_pPrev = pCRecFind->m_pPrev;
		}

		//delete
		m_pCRecCache->free(pCRecFind);
		m_map.erase(itr);
	}
}

bool LRUCache::get(uint64 *retVal)
{
	if(m_first)
	{
		*retVal = m_first->m_key;
		return true;
	}
        
    return false;
}


