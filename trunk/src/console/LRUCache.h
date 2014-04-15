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
	explicit CacheRecord() : m_key(0), m_pPrev(NULL), m_pNext(NULL)
	{
        
	}

	uint64			m_key;
	CacheRecord		*m_pPrev;
	CacheRecord		*m_pNext;
} CRec;

class LRUCache
{
    typedef HashMap<uint64, CRec*>      LRUMap;
    
public:   
	explicit LRUCache(const char *pName, const uint64 &cacheSize);
	~LRUCache();

    bool get(uint64 *retVal);
    void put(const uint64 &x);
    bool remove(const uint64 &x);
    void recycle();

	INLINE uint64 size()
    {
        return m_pMap->size();
    }
    
    INLINE size_t cacheSize()
    {
        return m_rCRecCache.GetSize();
    }

private:
	//disable copy constructor
	LRUCache(const LRUCache& that);
    INLINE void ChangeChain(CRec *pCRecFind);

	CRec                *m_last;
	CRec                *m_first;
    LRUMap              *m_pMap;
    FixedPool<CRec>     m_rCRecCache;
};

#endif
