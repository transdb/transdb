//
//  DiskWriter.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/31/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

DiskWriter::DiskWriter(const char *pDataFile, Storage *pStorage)
{
    m_pStorage			= pStorage;
	m_pBlockMemoryPool	= new FixedPool<BlockSize_T>(g_DiskWriterBlockMemReserve);

    m_pDataFile = fopen(pDataFile, "rb+");
    assert(m_pDataFile);
}

DiskWriter::~DiskWriter()
{
    IO::fclose(m_pDataFile);
    
	//delete pool
	delete m_pBlockMemoryPool;
}

void DiskWriter::Queue(uint64 x, RecordIndex *pRecordIndex)
{
    m_queueLock.Acquire();          //<--------
    
    //check if exists
    BlockManagerCopyMap::iterator itr;
    itr = m_queue.find(x);
    if(itr != m_queue.end())
    {
		itr->second.Destroy(m_pBlockMemoryPool, m_queueLock);
        m_queue.erase(itr);
    }
    
    //create copy of block manager data
    BlockManagerCopy rBlockManagerCopy;
    rBlockManagerCopy.m_recordStart = -1;
	rBlockManagerCopy.Init(m_pBlockMemoryPool, pRecordIndex->m_pBlockManager->DataPointer(), m_queueLock);
    
    //add to queue
    m_queue.insert(BlockManagerCopyMap::value_type(x, rBlockManagerCopy));
    m_queueLock.Release();          //-------->
}

void DiskWriter::Remove(uint64 x)
{
    m_lock.Acquire();
    m_queueLock.Acquire();
    
    BlockManagerCopyMap::iterator itr;
    //queue
    itr = m_queue.find(x);
    if(itr != m_queue.end())
    {
        itr->second.Destroy(m_pBlockMemoryPool, m_queueLock);
        m_queue.erase(itr);
    }
    //process
    itr = m_process.find(x);
    if(itr != m_process.end())
    {
        itr->second.Destroy(m_pBlockMemoryPool, m_queueLock);
        m_process.erase(itr);
    }
    
    m_queueLock.Release();
    m_lock.Release();
}

void DiskWriter::Process()
{
    //process queue
    m_lock.Acquire();
    m_queueLock.Acquire();
    BlockManagerCopyMap::iterator itr, itr2, searchItr;
    if(m_queue.size())
    {
        for(itr = m_queue.begin();itr != m_queue.end();)
        {
            itr2 = itr++;
            searchItr = m_process.find(itr2->first);
            if(searchItr != m_process.end())
            {
                searchItr->second.Destroy(m_pBlockMemoryPool, m_queueLock);
                m_process.erase(searchItr);
            }
            m_process.insert(BlockManagerCopyMap::value_type(itr2->first, itr2->second));
            m_queue.erase(itr2);
        }
    }
    m_queueLock.Release();
    
    //start writing
    streamoff recordStart;
    size_t requestedDiskSize;
    RecordIndexMap::iterator DiItr;
	size_t i;
	size_t offset;
	uint8 *pData;
    bool oCanDefrag = true;
    
    if(m_process.size())
    {
        for(itr = m_process.begin();itr != m_process.end();)
        {
            itr2 = itr++;
            
            //check for newer data in queue
            m_queueLock.Acquire();
            searchItr = m_queue.find(itr2->first);
            if(searchItr != m_queue.end())
            {
                itr2->second.Destroy(m_pBlockMemoryPool, m_queueLock);
                m_process.erase(itr2);
                m_queueLock.Release();
                continue;
            }
            m_queueLock.Release();
            
            //calc requested disk size
            requestedDiskSize = itr2->second.m_rBlocks.size() * BLOCK_SIZE;
            
            //get free space
            recordStart = m_pStorage->GetFreeSpacePos(requestedDiskSize, oCanDefrag);
            oCanDefrag = false;
            
            if(recordStart == -1)
            {
                ReallocDataFile(requestedDiskSize);
                recordStart = m_pStorage->GetFreeSpacePos(requestedDiskSize, oCanDefrag);
                if(recordStart == -1)
                {
                    Log.Error(__FUNCTION__, "recordStart == -1");
                    m_lock.Release();
                    return;
                }
            }
            
            //write to disk
			for(i = 0;i < itr2->second.m_rBlocks.size();++i)
			{
				//calc offset
				offset = recordStart+(i*BLOCK_SIZE);
				pData = itr2->second.m_rBlocks[i];
				//write
				IO::fseek(m_pDataFile, offset, SEEK_SET);
				IO::fwrite(pData, BLOCK_SIZE, 1, m_pDataFile);
			}
            
            //set record start
            itr2->second.m_recordStart = recordStart;
            
            //deallocate
            m_queueLock.Acquire();
            itr2->second.Destroy(m_pBlockMemoryPool, m_queueLock);
            m_queueLock.Release();
        }

		//flush to disk
		IO::fflush(m_pDataFile);

        //update indexes
		LockingPtr<RecordIndexMap> pDataIndexes(m_pStorage->m_dataIndexes, NULL);    
        m_pStorage->m_dataIndexesLock.Acquire();
        {
            for(itr = m_process.begin();itr != m_process.end();++itr)
            {
                DiItr = pDataIndexes->find(itr->first);
                if(DiItr != pDataIndexes->end())
                {
                    DiItr->second->m_recordStart = itr->second.m_recordStart;
                    m_pStorage->m_pDataIndexDiskWriter->WriteRecordIndexToDisk(DiItr);
                }
                else
                {
                    //this means that record was deleted - add free space
                    m_pStorage->AddFreeSpace(itr->second.m_recordStart, itr->second.m_rBlocks.size()*BLOCK_SIZE);
                    //
                    Log.Warning(__FUNCTION__, "Record was deleted and then written to disk. X: "I64FMTD, itr->first);
                }
            }
            //sync dataindexes
            m_pStorage->m_pDataIndexDiskWriter->Sync();
        }
		m_pStorage->m_dataIndexesLock.Release();

		//clear
		m_process.clear();
    }
    m_lock.Release();
}


void DiskWriter::ReallocDataFile(size_t const & minSize)
{
    void *pBuff;
	size_t newSize;
    streamoff startFreeSpace;
    
    //calc size + alocate buffer
    newSize = std::max<size_t>(g_ReallocSize, minSize);
    pBuff = aligned_malloc(newSize);
    memset(pBuff, 0, newSize);
    
    IO::fseek(m_pDataFile, 0, SEEK_END);
	startFreeSpace = IO::ftell(m_pDataFile);
    
	//write to disk
    IO::fwrite(pBuff, newSize, 1, m_pDataFile);
    IO::fflush(m_pDataFile);
    
	//add free space
	m_pStorage->AddFreeSpace(startFreeSpace, newSize);
    
    //release memory
    aligned_free(pBuff);
}













