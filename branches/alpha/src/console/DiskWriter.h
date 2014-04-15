//
//  DiskWriter.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/31/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#ifndef TransDB_DiskWriter_h
#define TransDB_DiskWriter_h

class BlockManagerCopy
{    
public:
	void Init(FixedPool<BlockSize_T> *pBlockPool, Blocks &rBlocks, Mutex &rQueueLock)
	{
		size_t i;
		uint8 *pBlock;

		//preallocate
		m_rBlocks.reserve(rBlocks.size());

        //rQueueLock.Acquire();
		for(i = 0;i < rBlocks.size();++i)
		{
			pBlock = (uint8*)pBlockPool->alloc();
			memcpy(pBlock, rBlocks[i], BLOCK_SIZE);
			m_rBlocks.push_back(pBlock);
		}
        //rQueueLock.Release();
	}

	void Destroy(FixedPool<BlockSize_T> *pBlockPool, Mutex &rQueueLock)
	{
		size_t i;
		BlockSize_T *pBlock;

        //rQueueLock.Acquire();
		for(i = 0;i < m_rBlocks.size();++i)
		{
			pBlock = (BlockSize_T*)m_rBlocks[i];
			pBlockPool->free(pBlock);
		}
        //rQueueLock.Release();
	}

    std::streamoff          m_recordStart;          //8
	Blocks					m_rBlocks;				//24
};

class DiskWriter
{
#ifdef WIN32
    typedef std::map<uint64, BlockManagerCopy, std::less<uint64>, stdext::allocators::allocator_chunklist<std::pair<uint64, BlockManagerCopy> > >   BlockManagerCopyMap;
#else
    typedef std::map<uint64, BlockManagerCopy>                                                                                                      BlockManagerCopyMap;
#endif
    
public:
    DiskWriter(const char *pDataFile, Storage *pStorage);
    ~DiskWriter();
    
    void Queue(uint64 x, RecordIndex *pRecordIndex);
    void Remove(uint64 x);
    void Process();
        
    //reallocating
	void ReallocDataFile(size_t const & minSize);
    
    size_t GetBlockMemoryPoolSize()
    {
        size_t val;
        m_queueLock.Acquire();
        val = m_pBlockMemoryPool->GetSize();
        m_queueLock.Release();
        return val;
    }
    
private:
    FILE						*m_pDataFile;
    
    Storage						*m_pStorage;
    BlockManagerCopyMap			m_process;
    BlockManagerCopyMap			m_queue;
    
    Mutex						m_lock;
    Mutex						m_queueLock;

    //used from socket and storage thread
    //for locking is used m_queueLock
    //block mempool
    FixedPool<BlockSize_T>      *m_pBlockMemoryPool;
};

#endif
