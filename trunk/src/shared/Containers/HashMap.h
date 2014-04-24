//
//  HashMap.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 16.01.14.
//  Copyright (c) 2014 Miroslav Kudrnac. All rights reserved.
//

#ifndef TransDB_HashMap_h
#define TransDB_HashMap_h

// Hash node class template
template <class K, class V>
class HashNode
{
public:
    explicit HashNode(const K &key, const V &value) : m_key(key), m_value(value), m_pNext(NULL)
    {
        
    }
    
    INLINE K getKey() const                 { return m_key; }
    INLINE V getValue() const               { return m_value; }
    INLINE void setValue(V value)           { m_value = value; }
    INLINE HashNode *getNext() const        { return m_pNext; }
    INLINE void setNext(HashNode *pNext)    { m_pNext = pNext; }
    
private:
    //key-value pair
    K           m_key;
    V           m_value;
    //next bucket with same hash
    HashNode    *m_pNext;
};

// Hash map class template
template <class K, class V>
class HashMap
{
    typedef HashNode<K, V>              HashNodeT;
    typedef HashNodeT*                  HashNodeTable;
    typedef FixedPool<HashNodeT>        HashNodeMemPool;
    
public:
    explicit HashMap(const uint64 &tableSize) : m_recordsCount(0)
    {
        m_tableSize = nextPow2(tableSize);
        m_tableSizeMask = m_tableSize - 1;
        
        m_pTable = new HashNodeTable[m_tableSize];
        memset(m_pTable, 0, sizeof(HashNodeTable) * m_tableSize);
    }
    
    ~HashMap()
    {
        delete [] m_pTable;
        m_pTable = NULL;
    }
    
    INLINE void recycle()
    {
        m_rHashNodeMemPool.recycle();
    }
    
    INLINE uint64 size() const
    {
        return m_recordsCount;
    }
    
    INLINE uint64 tableSizeMask()
    {
        return m_tableSizeMask;
    }
    
    INLINE void clear()
    {
        HashNodeT *pEntry;
        Vector<HashNodeT*, uint64> rNodes;
        getAllNodes(rNodes);
        //deallocate
        for(uint64 i = 0;i < rNodes.size();++i)
        {
            pEntry = rNodes[i];
            m_rHashNodeMemPool.deallocate(pEntry);
        }
        memset(m_pTable, 0, sizeof(HashNodeTable) * m_tableSize);
        m_recordsCount = 0;
    }
    
    INLINE void getAllValues(Vector<V, uint64> &rValues)
    {
        if(m_recordsCount != 0)
        {
            HashNodeT *pEntry;
            for(uint64 i = 0;i < m_tableSize;++i)
            {
                pEntry = m_pTable[i];
                while(pEntry != NULL)
                {
                    rValues.push_back(pEntry->getValue());
                    pEntry = pEntry->getNext();
                }
            }
        }
    }
    
    INLINE bool containsKey(const K &key)
    {
        uint64 hashValue = static_cast<uint64>(key) & m_tableSizeMask;
        HashNodeT *pEntry = m_pTable[hashValue];
        
        while(pEntry != NULL)
        {
            if(pEntry->getKey() == key)
                return true;
            
            pEntry = pEntry->getNext();
        }
        return false;
    }
    
    INLINE bool get(const K &key, V &value)
    {
        uint64 hashValue = static_cast<uint64>(key) & m_tableSizeMask;
        HashNodeT *pEntry = m_pTable[hashValue];
        
        while(pEntry != NULL)
        {
            if(pEntry->getKey() == key)
            {
                value = pEntry->getValue();
                return true;
            }
            pEntry = pEntry->getNext();
        }
        return false;
    }
    
    INLINE void put(const K &key, const V &value)
    {
        uint64 hashValue = static_cast<uint64>(key) & m_tableSizeMask;
        HashNodeT *pPrev = NULL;
        HashNodeT *pEntry = m_pTable[hashValue];
        
        while(pEntry != NULL && pEntry->getKey() != key)
        {
            pPrev = pEntry;
            pEntry = pEntry->getNext();
        }
        
        if(pEntry == NULL)
        {
            pEntry = m_rHashNodeMemPool.allocate(key, value);
            if(pPrev == NULL)
            {
                m_pTable[hashValue] = pEntry;
            }
            else
            {
                pPrev->setNext(pEntry);
            }
            ++m_recordsCount;
        }
        else
        {
            pEntry->setValue(value);
        }
    }
    
    INLINE void remove(const K &key)
    {
        uint64 hashValue = static_cast<uint64>(key) & m_tableSizeMask;
        HashNodeT *pPrev = NULL;
        HashNodeT *pEntry = m_pTable[hashValue];
        
        while(pEntry != NULL && pEntry->getKey() != key)
        {
            pPrev = pEntry;
            pEntry = pEntry->getNext();
        }
        
        if(pEntry == NULL)
        {
            //not found
            return;
        }
        else
        {
            if(pPrev == NULL)
            {
                // remove first bucket of the list
                m_pTable[hashValue] = pEntry->getNext();
            }
            else
            {
                pPrev->setNext(pEntry->getNext());
            }
            m_rHashNodeMemPool.deallocate(pEntry);
            --m_recordsCount;
        }
    }
    
private:
    //disable copy constructor
	HashMap(const HashMap& that);
    
    //
    INLINE void getAllNodes(Vector<HashNodeT*, uint64> &rNodes)
    {
        if(m_recordsCount != 0)
        {
            HashNodeT *pEntry;
            for(uint64 i = 0;i < m_tableSize;++i)
            {
                pEntry = m_pTable[i];
                while(pEntry != NULL)
                {
                    rNodes.push_back(pEntry);
                    pEntry = pEntry->getNext();
                }
            }
        }
    }
    
    //
    INLINE uint64 nextPow2(uint64 x)
    {
        --x;
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;
        return x+1;
    }
    
    //
    HashNodeTable       *m_pTable;
    HashNodeMemPool     m_rHashNodeMemPool;
    uint64              m_tableSize;
    uint64              m_tableSizeMask;
    uint64              m_recordsCount;
};

#endif
