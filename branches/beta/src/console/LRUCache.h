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
	typedef std::map<uint64, CRec*>     LRUMap;
    
public:   
	LRUCache();
	~LRUCache();

    void put(const uint64 &x);
    bool get(uint64 *retVal);
    void remove(const uint64 &x);

	INLINE size_t size()
    {
        return m_map.size();
    }
    
    INLINE size_t cacheSize()
    {
        return m_pCRecCache->GetSize();
    }

private:
	CRec                *m_last;
	CRec                *m_first;
	LRUMap              m_map;
    FixedPool<CRec>     *m_pCRecCache;
};

#endif
