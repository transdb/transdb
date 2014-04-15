//
//  UniqueVector.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 28.11.12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#ifndef TransDB_UniqueVector_h
#define TransDB_UniqueVector_h

#include <vector>

template<class T>
class UniqueVector
{
public:
    typedef std::vector<T>                      WriteInfoVec;
    typedef typename WriteInfoVec::iterator     iterator;
    
    void insert(const T &rWriteInfo)
    {   
        //binary search        
        iterator itr = std::lower_bound(m_rWriteInfo.begin(), m_rWriteInfo.end(), rWriteInfo);
        if(itr != m_rWriteInfo.end() && !(rWriteInfo < *itr))
        {
            (*itr) = rWriteInfo;
        }
        else
        {
            m_rWriteInfo.push_back(rWriteInfo);
            std::sort(m_rWriteInfo.begin(), m_rWriteInfo.end());
        }
    }
    
    INLINE bool find(const T &rWriteInfo)
    {
        iterator itr = std::lower_bound(m_rWriteInfo.begin(), m_rWriteInfo.end(), rWriteInfo);
        return (itr != m_rWriteInfo.end() && !(rWriteInfo < *itr));
    }
    
    INLINE iterator begin()
    {
        return m_rWriteInfo.begin();
    }
    
    INLINE iterator end()
    {
        return m_rWriteInfo.end();
    }
    
    INLINE size_t size() const
    {
        return m_rWriteInfo.size();
    }
    
    INLINE void erase(const T &rWriteInfo)
    {
        iterator itr;
        for(itr = m_rWriteInfo.begin();itr != m_rWriteInfo.end();++itr)
        {
            if(*itr == rWriteInfo)
            {
                m_rWriteInfo.erase(itr);
                return;
            }
        }
    }
    
    INLINE void clear()
    {
        m_rWriteInfo.clear();
    }
    
    UniqueVector &operator=(UniqueVector &rUniqueVector)
    {
        m_rWriteInfo = rUniqueVector.m_rWriteInfo;
        return *this;
    }
    
private:
    WriteInfoVec m_rWriteInfo;
};

#endif
