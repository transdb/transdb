//
//  DiskWriter.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/31/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

DiskWriter::DiskWriter(Storage &pStorage) : m_pStorage(pStorage)
{
    
}

DiskWriter::~DiskWriter()
{

}

void DiskWriter::Queue(RecordIndexMap::accessor &rWriteAccesor)
{
	//lock
    std::lock_guard<tbb::mutex> rGuard(m_queueLock);

	//check if item exists, if exists replace
	WriteInfo rWriteInfo(rWriteAccesor->first, rWriteAccesor->second->m_recordStart);
	DirtyXQueue::iterator itr = m_rQueue.find(rWriteInfo);
	if(itr != m_rQueue.end())
	{
		m_rQueue.erase(itr);
	}
    m_rQueue.insert(rWriteInfo);

	//set InDiskWriteQueue flag
	rWriteAccesor->second->m_flags |= eRIF_InDiskWriteQueue;
}

INLINE static bool SortWriteInfoForWrite(const WriteInfo &rWriteInfo1, const WriteInfo &rWriteInfo2)
{
    return rWriteInfo1.m_recordPosition < rWriteInfo2.m_recordPosition;
}

void DiskWriter::Process()
{
    //process queue
    {
        //lock
        std::lock_guard<tbb::mutex> rQGuard(m_queueLock);
        std::lock_guard<tbb::mutex> rPGuard(m_lock);
        
        //copy
        for(DirtyXQueue::iterator itr = m_rQueue.begin();itr != m_rQueue.end();++itr)
        {
            m_rProcess.push_back(*itr);
        }
        m_rQueue.clear();
    }
    
    //process freespace
    {
        m_pStorage.ProcessFreeSpaceQueue();
        m_pStorage.DefragmentFreeSpace();
    }
        
    uint8 *pBlock;
    CIDF *pCIDF;
    uint16 i;
    size_t blockOffset;
    std::streamsize requestDiskSize;
    std::streamoff freePos;

    //write accesor
    RecordIndexMap::accessor rWriteAccesor;
    
	//start writing to disk
	{
		//lock
		std::lock_guard<tbb::mutex> rPGuard(m_lock);
		if(m_rProcess.size())
		{
			//sort by recordPosition
            m_rProcess.sort(SortWriteInfoForWrite);
        
			//start writing to disk
			for(DirtyXProcess::iterator itr = m_rProcess.begin();itr != m_rProcess.end();++itr)
			{
				if(m_pStorage.m_dataIndexes.find(rWriteAccesor, itr->m_key))
				{                    
					//update blocks
					if(!(rWriteAccesor->second->m_flags & eRIF_RelocateBlocks))
					{
						for(i = 0;i < rWriteAccesor->second->m_pBlockManager->numOfBlocks();++i)
						{
							pBlock = rWriteAccesor->second->m_pBlockManager->GetBlock(i);
							pCIDF = GetCIDF(pBlock);
							if(pCIDF->m_flags & eBLF_Dirty)
							{
								//clear dirty flag
								pCIDF->m_flags &= ~eBLF_Dirty;
								//calc offset + write to disk
								blockOffset = rWriteAccesor->second->m_recordStart + (i * BLOCK_SIZE);
								IO::fseek(g_pStorage_FilePointer, blockOffset, SEEK_SET);
								IO::fwrite(pBlock, BLOCK_SIZE, 1, g_pStorage_FilePointer);
                                
								//stats
								Sync_Add(&g_NumOfWritesToDisk);
							}
						}
					}
					//write/rellocate blocks to new free position
					else
					{
						requestDiskSize = rWriteAccesor->second->m_pBlockManager->numOfBlocks() * BLOCK_SIZE;
						freePos = m_pStorage.GetFreeSpacePos(requestDiskSize);
						if(freePos == -1)
						{
							ReallocDataFile(requestDiskSize);
							freePos = m_pStorage.GetFreeSpacePos(requestDiskSize);
							if(freePos == -1)
							{
								Log.Error(__FUNCTION__, "freePos == -1");
								return;
							}
						}
                    
						//clear dirty flag
						rWriteAccesor->second->m_pBlockManager->ClearDirtyFlags();
                    
						//write to disk
						for(i = 0;i < rWriteAccesor->second->m_pBlockManager->numOfBlocks();++i)
						{
							pBlock = rWriteAccesor->second->m_pBlockManager->GetBlock(i);
							//calc offset + write to disk
							blockOffset = freePos + (i * BLOCK_SIZE);
							IO::fseek(g_pStorage_FilePointer, blockOffset, SEEK_SET);
							IO::fwrite(pBlock, BLOCK_SIZE, 1, g_pStorage_FilePointer);
                        
							//stats
							Sync_Add(&g_NumOfWritesToDisk);
						}
                    
						//set record position
						rWriteAccesor->second->m_recordStart = freePos;
					}
                
					//update index on disk if changed
                    if(rWriteAccesor->second->m_flags & eRIF_Dirty)
                    {
                        std::lock_guard<tbb::mutex> rGuard(m_pStorage.m_rDataIndexDiskWriterLock);
                        m_pStorage.m_pDataIndexDiskWriter->WriteRecordIndexToDisk(rWriteAccesor);
                        rWriteAccesor->second->m_flags &= ~eRIF_Dirty;
                    }
                
					//clear flag
					rWriteAccesor->second->m_flags &= ~eRIF_RelocateBlocks;
                    
                    //cannot delete this record if is wating in disk queue
                    {
                        std::lock_guard<tbb::mutex> rQGuard(m_queueLock);
                        if(m_rQueue.find(WriteInfo(rWriteAccesor->first, 0)) == m_rQueue.end())
                        {
                            //unset InDiskWriteQueue flag
                            rWriteAccesor->second->m_flags &= ~eRIF_InDiskWriteQueue;
                        }
                    }
				}
				else
				{
					Log.Warning(__FUNCTION__, "Data deleted before written to disk. X: "I64FMTD, itr->m_key);
				}
			}

			//clear
			m_rProcess.clear();
		}
	}
}

void DiskWriter::Remove(const uint64 &x)
{
    {
        std::lock_guard<tbb::mutex> rQGuard(m_queueLock);
        m_rQueue.erase(WriteInfo(x, 0));
    }
    
    {
        std::lock_guard<tbb::mutex> rPGuard(m_lock);
        m_rProcess.remove(WriteInfo(x, 0));
    }
}

void DiskWriter::ReallocDataFile(const size_t &minSize, bool oAddFreeSpace /*= true*/)
{
    void *pBuff;
	size_t newSize;
    streamoff startFreeSpace;
    
    //calc size
    newSize = std::max<size_t>(g_ReallocSize, minSize);
    pBuff = aligned_malloc(newSize);
    memset(pBuff, 0, newSize);
    
    //seek end + get new freespace start position
    IO::fseek(g_pStorage_FilePointer, 0, SEEK_END);
    startFreeSpace = IO::ftell(g_pStorage_FilePointer);
    
	//write to disk dummy byte for resize
    IO::fwrite(pBuff, newSize, 1, g_pStorage_FilePointer);
    
	//add free space
    if(oAddFreeSpace)
    {
        m_pStorage.AddFreeSpace(startFreeSpace, newSize);
        m_pStorage.DefragmentFreeSpace();
    }
    
    //update dataFileSize
    m_pStorage.m_dataFileSize += newSize;
    
    //release memory
    aligned_free(pBuff);
}














