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

//aligment 128bit
#define MALLOC_ALIGNMENT	16

//malloc
static void *aligned_malloc(size_t size)
{
    void *pPtr;
#ifdef WIN32
    pPtr = _aligned_malloc(size, MALLOC_ALIGNMENT);    
#else
    pPtr = malloc(size);
#endif
    if(pPtr == NULL)
    {
        Log.Error(__FUNCTION__, "Cannot allocate memory on heap.");
    }
    return pPtr;
}

//free
static void aligned_free(void *memblock)
{
#ifdef WIN32
    _aligned_free(memblock);
#else
    free(memblock);
#endif
}

//realloc
static void *aligned_realloc(void *memblock, size_t newSize)
{
    void *pPtr;
#ifdef WIN32
    pPtr = _aligned_realloc(memblock, newSize, MALLOC_ALIGNMENT);
#else
    pPtr = realloc(memblock, newSize);
#endif
    if(pPtr == NULL)
    {
        Log.Error(__FUNCTION__, "Cannot reallocate memory on heap.");
    }
    return pPtr;
}

struct MemoryBlockHeader
{
	size_t	m_dataStart;				//8
	size_t	m_freeChunkPosition;		//8
	size_t	*m_pFreeChunks;				//8
    
	INLINE void push_back(size_t freeChunkNumber)
	{
		++m_freeChunkPosition;
		m_pFreeChunks[m_freeChunkPosition] = freeChunkNumber;
	}
    
	INLINE size_t back()
	{
		return m_pFreeChunks[m_freeChunkPosition];
	}
    
	INLINE void pop_back()
	{
		--m_freeChunkPosition;
	}
};

template<typename T>
struct MemoryChunk
{
	uint8	m_val[sizeof(T)];
	size_t	m_memoryBlockNumber;
};

template<typename T>
class FixedPool
{
    static const size_t m_MemoryBlockReserveSize = 16;
    
	typedef MemoryChunk<T>																			Chunk;
    typedef std::vector<uint8*>                                                                     MemoryBlocksVec;    
#ifdef WIN32
    typedef std::set<size_t, std::less<size_t>, stdext::allocators::allocator_chunklist<size_t> >   FreeMemoryBlocksSet;
#else
    typedef std::set<size_t>                                                                        FreeMemoryBlocksSet;
#endif
    
public:
    explicit FixedPool(size_t initialSize) : m_memoryBlockSize(0)
	{
        uint8 *pMemoryBlock;
        
        m_initialSize = initialSize;
        m_rMemoryBlocks.reserve(m_MemoryBlockReserveSize);
        
        //create initial memory block
        pMemoryBlock = CreateNewBlock();
        m_rMemoryBlocks.push_back(pMemoryBlock);
        //add free block
        m_rFreeMemoryBlocks.insert(0);
    }
    
    ~FixedPool()
    {
		size_t i;
		for(i = 0;i < m_rMemoryBlocks.size();++i)
        {
            aligned_free(m_rMemoryBlocks[i]);
        }
    }
    
    T* alloc()
	{
		Chunk *pRetVal;
		uint8 *pMemoryBlock;
		MemoryBlockHeader *pMemoryBlockHeader;
		size_t freeChunk;
		size_t freeMemoryBlock;
        
		//get free memoryblock
		pMemoryBlock = GetFreeMemoryBlock(&freeMemoryBlock, &freeChunk);
		pMemoryBlockHeader = (MemoryBlockHeader*)pMemoryBlock;
        
		//get chunk from memory block
		pRetVal = (Chunk*)(pMemoryBlock+pMemoryBlockHeader->m_dataStart) + freeChunk;
		pRetVal->m_memoryBlockNumber = freeMemoryBlock;
        //construct
        new(static_cast<void*>(pRetVal)) T();
        //
        return (T*)pRetVal;
    }
    
    void free(T* p)
	{
		MemoryBlockHeader *pMemoryBlockHeader;
		size_t chunkNumber;
		Chunk *pChunk;
		uint8 *pMemoryBlock;
		
        //call destructor
        p->~T();
		//
		pChunk = (Chunk*)(p);
        
		pMemoryBlock = (uint8*)(m_rMemoryBlocks[pChunk->m_memoryBlockNumber]);
		pMemoryBlockHeader = (MemoryBlockHeader*)pMemoryBlock;
		chunkNumber = pChunk - (Chunk*)(pMemoryBlock+pMemoryBlockHeader->m_dataStart);
        
        //this block has free space now
        m_rFreeMemoryBlocks.insert(pChunk->m_memoryBlockNumber);
        
		//add free chunk
		pMemoryBlockHeader->push_back(chunkNumber);
    }
    
    INLINE size_t GetSize()
    {
        return m_rMemoryBlocks.size() * m_memoryBlockSize;
    }
    
private:
	uint8 *GetFreeMemoryBlock(size_t *pFreeMemoryBlock, size_t *pFreeChunk)
	{
        FreeMemoryBlocksSet::iterator itr;
        uint8* pMemoryBlock;
        size_t freeMemoryBlock;
        size_t freeChunk;
        MemoryBlockHeader *pMemoryBlockHeader;
        
        if(m_rFreeMemoryBlocks.size())
        {
            //we have free block
            itr = m_rFreeMemoryBlocks.begin();
            freeMemoryBlock = (*itr);
            pMemoryBlock = m_rMemoryBlocks[freeMemoryBlock];
            pMemoryBlockHeader = (MemoryBlockHeader*)pMemoryBlock;
            
            //get free chunk
            freeChunk = pMemoryBlockHeader->back();
            pMemoryBlockHeader->pop_back();
            
            //no free chunk remove
            if(pMemoryBlockHeader->m_freeChunkPosition == -1)
            {
                m_rFreeMemoryBlocks.erase(itr);
            }
        }
        else
        {
            //we dont have free block alocate new one
            pMemoryBlock = CreateNewBlock();
            m_rMemoryBlocks.push_back(pMemoryBlock);
            
            //get free chunk
            pMemoryBlockHeader = (MemoryBlockHeader*)pMemoryBlock;
            freeChunk = pMemoryBlockHeader->back();
            pMemoryBlockHeader->pop_back();
            
            //add free block
            freeMemoryBlock = m_rMemoryBlocks.size() - 1;
            m_rFreeMemoryBlocks.insert(freeMemoryBlock);
        }
        
        //return values
        *pFreeMemoryBlock = freeMemoryBlock;
        *pFreeChunk = freeChunk;
        
		return pMemoryBlock;
	}
    
	uint8 *CreateNewBlock()
	{
		uint8 *pNewBlock;
        
		//
        m_memoryBlockSize = (sizeof(Chunk) * m_initialSize) + sizeof(MemoryBlockHeader) + (m_initialSize*sizeof(size_t));
		pNewBlock = (uint8*)aligned_malloc(m_memoryBlockSize);
		memset(pNewBlock, 0, m_memoryBlockSize);
        
		//
		MemoryBlockHeader *pMemoryBlockHeader		= (MemoryBlockHeader*)pNewBlock;
		pMemoryBlockHeader->m_freeChunkPosition     = -1;
		pMemoryBlockHeader->m_dataStart             = sizeof(MemoryBlockHeader) + m_initialSize*sizeof(size_t);
		pMemoryBlockHeader->m_pFreeChunks			= (size_t*)(pNewBlock + sizeof(MemoryBlockHeader));
		for(size_t i = 0;i < m_initialSize;++i)
		{
			pMemoryBlockHeader->push_back(i);
		}
		return pNewBlock;
	}
    
private:
	//
    FreeMemoryBlocksSet     m_rFreeMemoryBlocks;
    MemoryBlocksVec         m_rMemoryBlocks;
    size_t                  m_initialSize;
    size_t                  m_memoryBlockSize;
};

#endif