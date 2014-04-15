/*
 * Game server
 * Copyright (C) 2010 Miroslav 'Wayland' Kudrnac
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef MEMORY_ALLOCATOR_H
#define MEMORY_ALLOCATOR_H

#include "../Defines.h"
#include "../Logs/Log.h"
#include "tbb/mutex.h"

template<typename T>
class FixedPool
{
    /** Structure to store the next available memory block. */
    struct BlockList
    {
        BlockList   *m_pNext;   ///< Pointer to the next memory block
    };
    
public:
    FixedPool() : m_pBlockList(NULL), m_allocationsFromSys(0)
    {

    }
    
    ~FixedPool()
    {
        BlockList *pBlock = m_pBlockList;
        while(pBlock)
        {
            BlockList *pNext = pBlock->m_pNext;
            _dealloc_sys(pBlock);
            pBlock = pNext;
        }
    }
    
    INLINE T* allocate()
	{
        //get free mem
        void *pRetVal = _allocate();
        //construct T from memory
        T *pTobj = new(pRetVal) T();
        return pTobj;
    }
    
    template<typename A1>
    INLINE T* allocate(const A1 &a1)
    {
        //get free mem
        void *pRetVal = _allocate();
        //construct T from memory
        T *pTobj = new(pRetVal) T(a1);
        return pTobj;
    }
    
    template<typename A1, typename A2>
    INLINE T* allocate(const A1 &a1, const A2 &a2)
    {
        //get free mem
        void *pRetVal = _allocate();
        //construct T from memory
        T *pTobj = new(pRetVal) T(a1, a2);
        return pTobj;
    }
    
    template<typename A1, typename A2, typename A3>
    INLINE T* allocate(const A1 &a1, const A2 &a2, const A3 &a3)
    {
        //get free mem
        void *pRetVal = _allocate();
        //construct T from memory
        T *pTobj = new(pRetVal) T(a1, a2, a3);
        return pTobj;
    }
    
    INLINE void deallocate(T *p)
    {
        assert(p);
        
        //call destructor
        p->~T();
        
        //put block back in pool
        BlockList *pBlock = reinterpret_cast<BlockList*>(p);
        pBlock->m_pNext = m_pBlockList;
        m_pBlockList = pBlock;
    }
    
    /**
     * Recycles half of the free memory blocks in the memory pool to the
     * system.  It is called when a memory request to the system (in other
     * instances of the static memory pool) fails.
     */
    INLINE void recycle()
    {
        BlockList* block = m_pBlockList;
        while (block)
        {
            BlockList* temp = block->m_pNext;
            if(temp)
            {
                BlockList* next = temp->m_pNext;
                block->m_pNext = next;
                _dealloc_sys(temp);
                block = next;
            }
            else
            {
                break;
            }
        }
    }
    
    INLINE uint64 GetSize()
    {
        return sizeof(T) * m_allocationsFromSys;
    }
    
    INLINE uint64 allocationsFromSys()
    {
        return m_allocationsFromSys;
    }
    
private:
    INLINE void* _allocate()
    {
        if(m_pBlockList)
        {
            void* result = m_pBlockList;
            m_pBlockList = m_pBlockList->m_pNext;
            return result;
        }

        return _alloc_sys(align(sizeof(T)));
    }
    
    INLINE void* _alloc_sys(size_t size)
    {
        ++m_allocationsFromSys;
        void* result = ::malloc(size);
        if(!result)
        {
            recycle();
            result = ::malloc(size);
            Log.Warning(__FUNCTION__, "Memory pool forced recycle.");
        }
        return result;
    }
    
    INLINE void _dealloc_sys(void *ptr)
    {
        --m_allocationsFromSys;
        ::free(ptr);
    }
    
    INLINE size_t align(size_t size)
    {
        return size >= sizeof(BlockList) ? size : sizeof(BlockList);
    }
    
    //declarations
    BlockList   *m_pBlockList;
    uint64      m_allocationsFromSys;
};

template <class T>
NOINLINE static void _S_FixedPool_Recycle(const char *pName, tbb::mutex &rLock, T &rT)
{
    Log.Debug(__FUNCTION__, "%s recycling started.", pName);
    {
        std::lock_guard<tbb::mutex> rBGuard(rLock);
        rT.recycle();
    }
    Log.Debug(__FUNCTION__, "%s recycling finished.", pName);
}

#endif