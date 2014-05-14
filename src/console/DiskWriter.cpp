//
//  DiskWriter.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/31/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

DiskWriter::DiskWriter(Storage &pStorage) : m_rStorage(pStorage)
{
    //TODO: find better way how to get this value
    m_pQueue                    = new DirtyXQueue(100000);
    m_pRIDelQueue               = new RIDelQueue(100000);
    m_sumDiskWriteTime          = 0;
    m_lastNumOfItemsInProcess   = 0;
    m_itemsToProcessSize        = 0;
}

DiskWriter::~DiskWriter()
{
    delete m_pQueue;
    m_pQueue = NULL;
    delete m_pRIDelQueue;
    m_pRIDelQueue = NULL;
}

void DiskWriter::Queue(RecordIndexMap::accessor &rWriteAccesor)
{
	//lock
    std::lock_guard<std::mutex> rQGuard(m_rQueueLock);

	//check if item exists, if exists replace
	WriteInfo rWriteInfo(rWriteAccesor->first, rWriteAccesor->second->m_recordStart);
    
    //add to queue
    m_pQueue->put(rWriteAccesor->first, rWriteInfo);

	//set InDiskWriteQueue flag
	rWriteAccesor->second->m_flags |= eRIF_InDiskWriteQueue;
}

void DiskWriter::QueueIndexDeletetion(RecordIndexMap::accessor &rWriteAccesor)
{
    //lock
    std::lock_guard<std::mutex> rGuard(m_rRIDelQueueLock);
    
    //create copy
    RecordIndex rRecordIndex;
    rRecordIndex.m_recordStart = rWriteAccesor->second->m_recordStart;
    rRecordIndex.m_blockCount = rWriteAccesor->second->m_blockCount;
    rRecordIndex.m_IB_recordOffset = rWriteAccesor->second->m_IB_recordOffset;
    rRecordIndex.m_IB_blockNumber = rWriteAccesor->second->m_IB_blockNumber;
    //add to queue
    m_pRIDelQueue->put(rWriteAccesor->first, rRecordIndex);
}

INLINE static bool _S_SortWriteInfoForWrite(const WriteInfo &rWriteInfo1, const WriteInfo &rWriteInfo2)
{
    return rWriteInfo1.m_recordPosition > rWriteInfo2.m_recordPosition;
}

NOINLINE static bool _S_DiskWriter_ProcessQueue(DiskWriter::DirtyXQueue *pQueue,
                                                std::mutex &rQueueLock,
                                                DiskWriter::DirtyXProcess &rAllItemsToProcess)
{
    //lock
    std::lock_guard<std::mutex> rQGuard(rQueueLock);
    
    //fill items to process
    if(pQueue->size())
    {
        rAllItemsToProcess.reserve(pQueue->size());
        pQueue->getAllValues(rAllItemsToProcess);
        pQueue->clear();
        return true;
    }
    return false;
}

NOINLINE static bool _S_DiskWriter_GetItemsToProcess(DiskWriter::DirtyXProcess &rAllItemsToProcess,
                                                     DiskWriter::DirtyXProcess &rItemsToProcess)
{
    //warning fix
    DiskWriter::DirtyXProcess::size_type tmpSizeType = static_cast<DiskWriter::DirtyXProcess::size_type>(g_TransactionsPerCommit);
    
    //set size to 0 (preserve internal buffer) + reserve space (if needed)
    rItemsToProcess.resize(0);
    rItemsToProcess.reserve(g_TransactionsPerCommit);
    while(rAllItemsToProcess.size())
    {
        //from front
        rItemsToProcess.push_back(rAllItemsToProcess.back());
        rAllItemsToProcess.pop_back();
        //
        if(rItemsToProcess.size() == tmpSizeType)
            break;
    }
    return rItemsToProcess.size() ? true : false;
}

void DiskWriter::WriteDataWithoutRelocateFlag(const HANDLE &hDataFile, RecordIndexMap::accessor &rWriteAccessor)
{
    uint8 *pBlock;
    CIDF *pCIDF;
    int64 blockOffset;
    
    for(uint16 i = 0;i < rWriteAccessor->second->m_pBlockManager->numOfBlocks();++i)
    {
        pBlock = rWriteAccessor->second->m_pBlockManager->GetBlock(i);
        pCIDF = GetCIDF(pBlock);
        if(pCIDF->m_flags & eBLF_Dirty)
        {
            //clear dirty flag
            pCIDF->m_flags &= ~eBLF_Dirty;
            //calc offset + write to disk
            blockOffset = rWriteAccessor->second->m_recordStart + (i * BLOCK_SIZE);
            IO::fseek(hDataFile, blockOffset, SEEK_SET);
            IO::fwrite(pBlock, BLOCK_SIZE, hDataFile);
        }
    }
}

bool DiskWriter::WriteDataWithRelocateFlag(const HANDLE &hDataFile, RecordIndexMap::accessor &rWriteAccessor)
{
    int64 requestDiskSize;
    int64 freePos;
    int64 blockOffset;
    uint8 *pBlock;
    
    requestDiskSize = rWriteAccessor->second->m_pBlockManager->numOfBlocks() * BLOCK_SIZE;
    freePos = m_rStorage.GetFreeSpacePos(requestDiskSize);
    if(freePos == -1)
    {
        ReallocDataFile(hDataFile, requestDiskSize);
        freePos = m_rStorage.GetFreeSpacePos(requestDiskSize);
        if(freePos == -1)
        {
            Log.Error(__FUNCTION__, "There is not freespace on disk drive. Record: " I64FMTD " is not written.", rWriteAccessor->first);
            return false;
        }
    }
    
    //set new record position
    rWriteAccessor->second->m_recordStart = freePos;
    
    //clear dirty flag
    rWriteAccessor->second->m_pBlockManager->ClearDirtyFlags();
    
    //write to disk
    for(uint16 i = 0;i < rWriteAccessor->second->m_pBlockManager->numOfBlocks();++i)
    {
        pBlock = rWriteAccessor->second->m_pBlockManager->GetBlock(i);
        //calc offset + write to disk
        blockOffset = freePos + (i * BLOCK_SIZE);
        IO::fseek(hDataFile, blockOffset, SEEK_SET);
        IO::fwrite(pBlock, BLOCK_SIZE, hDataFile);
    }
    return true;
}

void DiskWriter::ClearInDiskQueueFlag(DiskWriter::DirtyXProcess &rProcessedItems, RecordIndexMap::accessor &rWriteAccessor)
{
    //clear flags that records are InDiskWriteQueue
    for(DirtyXProcess::iterator itr = rProcessedItems.begin();itr != rProcessedItems.end();++itr)
    {
        if(m_rStorage.m_dataIndexes.find(rWriteAccessor, itr->m_key))
        {
            //cannot delete this record by MemoryWatcher if is wating in disk queue
            {
                std::lock_guard<std::mutex> rQGuard(m_rQueueLock);
                if(m_pQueue->containsKey(rWriteAccessor->first) == false)
                {
                    //unset InDiskWriteQueue flag
                    rWriteAccessor->second->m_flags &= ~eRIF_InDiskWriteQueue;
                }
            }
        }
    }
}

void DiskWriter::Process()
{
    uint64 startTime;
    uint64 endTime;
    uint32 crc32;
    DirtyXProcess rAllItemsToProcess;
    DirtyXProcess rItemsToProcess;
    
    //process index delete queue (delete from disk)
    {
        ProcessIndexDeleteQueue();
    }
    
	//start writing to disk
    //get all items from queue to process
    if(_S_DiskWriter_ProcessQueue(m_pQueue, m_rQueueLock, rAllItemsToProcess))
    {
        //stats
        startTime = GetTickCount64();
        m_lastNumOfItemsInProcess = rAllItemsToProcess.size();
        
        //sort by recordPosition - desc
        std::sort(rAllItemsToProcess.begin(), rAllItemsToProcess.end(), _S_SortWriteInfoForWrite);
        
        //get new items to process, split by coef how many trans per update
        while(_S_DiskWriter_GetItemsToProcess(rAllItemsToProcess, rItemsToProcess))
        {
            //write accessor
            RecordIndexMap::accessor rWriteAccessor;
            Vector<FreeSpace> rFreeSpaceToAdd(1024);
            //stats
            m_itemsToProcessSize = rItemsToProcess.size();
            
            //Transacted
            HANDLE hTransaction = IO::ftrans();
            HANDLE hDataFile = IO::fopentrans(m_rStorage.m_pDataPath, IO::IO_WRITE_ONLY, hTransaction);
            HANDLE hIndexFile = IO::fopentrans(m_rStorage.m_pIndexPath, IO::IO_RDWR, hTransaction);
            
            //itreate items and write to disk
            for(DirtyXProcess::iterator itr = rItemsToProcess.begin();itr != rItemsToProcess.end();++itr)
            {
                //check if items exits and load all info about it
                if(m_rStorage.m_dataIndexes.find(rWriteAccessor, itr->m_key))
                {
                    if(!(rWriteAccessor->second->m_flags & eRIF_RelocateBlocks))
                    {
                        WriteDataWithoutRelocateFlag(hDataFile, rWriteAccessor);
                    }
                    else
                    {
                        //queue adding freespace
                        if(rWriteAccessor->second->m_recordStart != -1)
                        {
                            rFreeSpaceToAdd.push_back(FreeSpace(rWriteAccessor->second->m_recordStart,
                                                                rWriteAccessor->second->m_blockCount * BLOCK_SIZE));
                        }
                        //this funtion will set new m_recordStart
                        //this function will fail if there is no disk space
                        bool status = WriteDataWithRelocateFlag(hDataFile, rWriteAccessor);
                        if(status == false)
                        {
                            continue;
                        }
                    }
                    
                    //stats
                    ++g_NumOfWritesToDisk;
                    
                    //update block count
                    rWriteAccessor->second->m_blockCount = rWriteAccessor->second->m_pBlockManager->numOfBlocks();
                    
                    //update crc32
                    crc32 = rWriteAccessor->second->m_pBlockManager->GetBlocksCrc32();
                    if(rWriteAccessor->second->m_crc32 != crc32)
                    {
                        rWriteAccessor->second->m_crc32 = crc32;
                        rWriteAccessor->second->m_flags |= eRIF_Dirty;
                    }
                    
                    //clear flag eRIF_RelocateBlocks
                    rWriteAccessor->second->m_flags &= ~eRIF_RelocateBlocks;
                    
                    //update index on disk if changed
                    if(rWriteAccessor->second->m_flags & eRIF_Dirty)
                    {
                        std::lock_guard<std::mutex> rGuard(m_rStorage.m_rDataIndexDiskWriterLock);
                        m_rStorage.m_pDataIndexDiskWriter->WriteRecordIndexToDisk(hIndexFile, rWriteAccessor);
                        rWriteAccessor->second->m_flags &= ~eRIF_Dirty;
                    }
                }
                else
                {
                    Log.Warning(__FUNCTION__, "Data deleted before written to disk. X: " I64FMTD, itr->m_key);
                }
				//release accessor
				rWriteAccessor.release();
                
                //stats
                --m_itemsToProcessSize;

            } /* end for */
            
            //!!!
            //Commit transaction before clear flag that record is NOT in disk write queue
            //else read thread will read old data, because it can be delete from memory
            //and loaded from disk again
            //!!!
            //close file handles
            IO::fclose(hDataFile);
            IO::fclose(hIndexFile);
            
            //commit and close transaction
            IO::fcommittrans(hTransaction);
            IO::fclosetrans(hTransaction);
            //!!!
            //!!!
            
            //clear flag eRIF_InDiskWriteQueue
            ClearInDiskQueueFlag(rItemsToProcess, rWriteAccessor);
			//release accessor
			rWriteAccessor.release();
            
            //add frespace from queue to Storage after transaction commit
            if(rFreeSpaceToAdd.size())
            {
                for(Vector<FreeSpace>::iterator itr = rFreeSpaceToAdd.begin();itr != rFreeSpaceToAdd.end();++itr)
                {
                    m_rStorage.AddFreeSpace(itr->m_pos, itr->m_lenght);
                }
            }
        } /* end while */
        
        //calc avg write time
        endTime = GetTickCount64() - startTime;
        m_sumDiskWriteTime += endTime;
        g_AvgDiskWriteTime = m_sumDiskWriteTime.load() / g_NumOfWritesToDisk.load();
        
    } /* end if */
}

void DiskWriter::Remove(const uint64 &x)
{
    std::lock_guard<std::mutex> rQGuard(m_rQueueLock);
    m_pQueue->remove(x);
}

void DiskWriter::RecycleQueue()
{
    _S_FixedPool_Recycle("DiskQueue", m_rQueueLock, *m_pQueue);
    _S_FixedPool_Recycle("RIDelQueue", m_rRIDelQueueLock, *m_pRIDelQueue);
}

void DiskWriter::ReallocDataFile(const HANDLE &hDataFile, const size_t &minSize, bool oAddFreeSpace /*= true*/)
{
	int64 reallocSize;
    int64 startFreeSpace;
    
    //calc size
    reallocSize = std::max<int64>(g_ReallocSize, minSize);
    
    //seek end + get new freespace start position
    IO::fseek(hDataFile, 0, SEEK_END);
    startFreeSpace = IO::ftell(hDataFile);
    
	//set end of file
	IO::fresize(hDataFile, startFreeSpace + reallocSize);
    
	//add free space
    if(oAddFreeSpace)
    {
        m_rStorage.AddFreeSpace(startFreeSpace, reallocSize);
        m_rStorage.DefragmentFreeSpace();
    }
    
    //update dataFileSize
    m_rStorage.m_dataFileSize += reallocSize;
}


NOINLINE static void _S_DiskWriter_GetItemsFromIndexDelQueue(DiskWriter::RIDelQueue *pRIDelQueue,
                                                             std::mutex &rRIDelQueueLock,
                                                             Vector<RecordIndex, uint64> &rDeleteQueue)
{
    std::lock_guard<std::mutex> rRIDel_Guard(rRIDelQueueLock);
    if(pRIDelQueue->size())
    {
        rDeleteQueue.reserve(pRIDelQueue->size());
        pRIDelQueue->getAllValues(rDeleteQueue);
        pRIDelQueue->clear();
    }
}

void DiskWriter::ProcessIndexDeleteQueue()
{
    Vector<RecordIndex, uint64> rDeleteQueue;
    
    //copy data to vector, clear queue
    _S_DiskWriter_GetItemsFromIndexDelQueue(m_pRIDelQueue, m_rRIDelQueueLock, rDeleteQueue);
    
    //process
    if(rDeleteQueue.size())
    {
        //Trancasted
        HANDLE hTransaction = IO::ftrans();
        HANDLE hIndexFile = IO::fopentrans(m_rStorage.m_pIndexPath, IO::IO_RDWR, hTransaction);
        
        //delete all records from index
        for(uint64 i = 0;i < rDeleteQueue.size();++i)
        {
            //get record
            const RecordIndex &rRecordIndex = rDeleteQueue[i];
            
            //add free space
            if(rRecordIndex.m_recordStart != -1)
            {
                m_rStorage.AddFreeSpace(rRecordIndex.m_recordStart, rRecordIndex.m_blockCount * BLOCK_SIZE);
            }
            
            //delete record index from disk
            {
                std::lock_guard<std::mutex> rDI_Guard(m_rStorage.m_rDataIndexDiskWriterLock);
                m_rStorage.m_pDataIndexDiskWriter->EraseRecord(hIndexFile, rRecordIndex);
            }
        }
        //close file handle
        IO::fclose(hIndexFile);
        
        //commit + close transaction
        IO::fcommittrans(hTransaction);
        IO::fclosetrans(hTransaction);
    }
}












