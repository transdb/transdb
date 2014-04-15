//
//  SortedVector.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 21.01.14.
//  Copyright (c) 2014 Miroslav Kudrnac. All rights reserved.
//

#ifndef TransDB_SortedVector_h
#define TransDB_SortedVector_h

#include "Vector.h"

template <class K, class V>
class SortedVectorNode
{
public:
    explicit SortedVectorNode() : m_key(0), m_value(0)
    {
        
    }
    
    explicit SortedVectorNode(const K &key, const V &value) : m_key(key), m_value(value)
    {
        
    }
    
    INLINE bool operator<(const SortedVectorNode<K, V> &rSortedVectorNode)
    {
        return m_key < rSortedVectorNode.m_key;
    }
    
    K       m_key;
    V       m_value;
};


template <class K, class V>
class SortedVector
{
public:
    typedef SortedVectorNode<K, V>      value_type;
    
    INLINE value_type &front()
    {
        return m_rVectorTable[0];
    }
    
    INLINE value_type &operator[](const size_t &index)
    {
        return m_rVectorTable[index];
    }
    
    INLINE bool find(const K &key, V &value, size_t &indexInVector)
    {
        int64 iMid;
        int64 iMin = 0;
        int64 iMax = m_rVectorTable.size() - 1;
        
        if(iMax != -1)
        {
            while(iMin < iMax)
            {
                iMid = (iMin + iMax) / 2;
                assert(iMin < iMax);

                if(m_rVectorTable[iMid].m_key < key)
                    iMin = iMid + 1;
                else
                    iMax = iMid;
            }
            
            if(iMax == iMin && m_rVectorTable[iMin].m_key == key)
            {
                indexInVector = iMin;
                value =  m_rVectorTable[iMin].m_value;
                return true;
            }
        }

        return false;
    }
    
    INLINE void insert(const value_type &value)
    {
        m_rVectorTable.push_back(value);
    }
    
    INLINE void erase(const size_t &index)
    {
        m_rVectorTable.RemoveIndex(index);
    }
    
    INLINE void sort()
    {
        std::sort(m_rVectorTable.begin(), m_rVectorTable.end(), SortedVectorNode_Sort);
    }
    
    INLINE size_t size()
    {
        return m_rVectorTable.size();
    }
    
    INLINE void clear()
    {
        m_rVectorTable.clear();
    }
    
private:
    static INLINE bool SortedVectorNode_Sort(const value_type &rSortedVectorNode1, const value_type &rSortedVectorNode2)
    {
        return rSortedVectorNode1.m_key < rSortedVectorNode2.m_key;
    }
    
    
    Vector<value_type, uint16>     m_rVectorTable;
};


#endif
