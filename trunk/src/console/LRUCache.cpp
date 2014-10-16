//
//  LRUCache.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 9/6/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

LRUCache::LRUCache(const char *pName, uint64 cacheSize) : m_last(NULL), m_first(NULL)
{
	//prepare hashmap
    m_pMap = new LRUMap(cacheSize);
    //
    Log.Notice(__FUNCTION__, "%s LRUMap tableSizeMask: " I64FMTD, pName, m_pMap->tableSizeMask());
}

LRUCache::~LRUCache()
{
    delete m_pMap;
}

INLINE void LRUCache::ChangeChain(CRec *pCRecFind)
{
    //change chain
    if(pCRecFind == m_first)
    {
        m_first = pCRecFind->m_pNext;
        if(m_first)
            m_first->m_pPrev = NULL;
    }
    
    if(pCRecFind == m_last)
    {
        m_last = pCRecFind->m_pPrev;
    }
    
    if(pCRecFind->m_pPrev)
    {
        pCRecFind->m_pPrev->m_pNext = pCRecFind->m_pNext;
        if(pCRecFind->m_pNext)
        {
            pCRecFind->m_pNext->m_pPrev = pCRecFind->m_pPrev;
        }
    }
}

void LRUCache::put(uint64 x)
{
    CRec *pCRec;

    //remove from chain -> add to end
    //dont need to remove from map will be updated
    if(m_pMap->get(x, pCRec))
    {
        ChangeChain(pCRec);
        _FREE(pCRec);
    }

    //init 1st
    if(m_first != NULL)
    {
        pCRec = (CRec*)_MALLOC(sizeof(CRec));
    }
    else
    {
        m_first = (CRec*)_MALLOC(sizeof(CRec));
        pCRec = m_first;
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
    m_pMap->put(x, pCRec);
}

bool LRUCache::remove(uint64 x)
{
	CRec *pCRecFind;
    
    //
    if(m_pMap->get(x, pCRecFind))
	{
		//change chain
        ChangeChain(pCRecFind);

		//delete
        _FREE(pCRecFind);
		m_pMap->remove(x);
        return true;
	}
    return false;
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

