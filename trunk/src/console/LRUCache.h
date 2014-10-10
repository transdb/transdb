//
//  LRUCache.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 9/6/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#ifndef TransDB_LRUCache_h
#define TransDB_LRUCache_h

typedef struct CacheRecord
{
	uint64			m_key;
	CacheRecord		*m_pPrev;
	CacheRecord		*m_pNext;
} CRec;

class LRUCache
{
    typedef HashMap<uint64, CRec*>      LRUMap;
    
public:   
	explicit LRUCache(const char *pName, uint64 cacheSize);
	~LRUCache();

    bool get(uint64 *retVal);
    void put(uint64 x);
    bool remove(uint64 x);

	INLINE uint64 size() const
    {
        return m_pMap->size();
    }

private:
	//disable copy constructor and assign
	DISALLOW_COPY_AND_ASSIGN(LRUCache);
    
    INLINE void ChangeChain(CRec *pCRecFind);

	CRec                *m_last;
	CRec                *m_first;
    LRUMap              *m_pMap;
};

#endif
