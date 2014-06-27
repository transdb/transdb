//
//  DiskWriter.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/31/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

DiskWriter::DiskWriter(Storage &rStorage) : m_rStorage(rStorage),
                                            m_pFreeSpaceDump(new FreeSpaceDumpTask(32)),
                                            m_pQueue(new DirtyXQueue(4*1024)),
                                            m_pRIDelQueue(new RIDelQueue(4*1024)),
                                            m_sumDiskWriteTime(0),
                                            m_lastNumOfItemsInProcess(0),
                                            m_itemsToProcessSize(0)
{
    
}

DiskWriter::~DiskWriter()
{
    delete m_pQueue;
    m_pQueue = NULL;
    delete m_pRIDelQueue;
    m_pRIDelQueue = NULL;
    delete m_pFreeSpaceDump;
    m_pFreeSpaceDump = NULL;
}

void DiskWriter::Queue(RecordIndexMap::accessor &rWriteAccesor)
{
	//lock
    std::lock_guard<std::mutex> rQGuard(m_rQueueLock);

	//check if item exists, if exists replace
	WriteInfo rWriteInfo(rWriteAccesor->first, rWriteAccesor->second.m_recordStart);
    
    //add to queue
    m_pQueue->put(rWriteAccesor->first, rWriteInfo);

	//set InDiskWriteQueue flag
	rWriteAccesor->second.m_flags |= eRIF_InDiskWriteQueue;
}

void DiskWriter::QueueIndexDeletetion(RecordIndexMap::accessor &rWriteAccesor)
{
    //lock
    std::lock_guard<std::mutex> rGuard(m_rRIDelQueueLock);
    
    //create copy
    RecordIndex rRecordIndex;
    rRecordIndex.m_recordStart = rWriteAccesor->second.m_recordStart;
    rRecordIndex.m_blockCount = rWriteAccesor->second.m_blockCount;
    rRecordIndex.m_IB_recordOffset = rWriteAccesor->second.m_IB_recordOffset;
    rRecordIndex.m_IB_blockNumber = rWriteAccesor->second.m_IB_blockNumber;
    //add to queue
    m_pRIDelQueue->put(rWriteAccesor->first, rRecordIndex);
}

void DiskWriter::QueueFreeSpaceDump(uint64 socketID, uint32 token, uint32 flags, uint32 dumpFlags)
{
    std::lock_guard<std::mutex> rGuard(m_rFreeSpaceDumpLock);
    m_pFreeSpaceDump->put(socketID, FreeSpaceTask(token, flags, dumpFlags));
}

INLINE static bool _S_SortWriteInfoForWrite(const WriteInfo &rWriteInfo1, const WriteInfo &rWriteInfo2)
{
    return rWriteInfo1.m_recordPosition < rWriteInfo2.m_recordPosition;
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

void DiskWriter::WriteDataWithoutRelocateFlag(HANDLE hDataFile, RecordIndexMap::accessor &rWriteAccessor)
{
    uint8 *pBlock;
    CIDF *pCIDF;
    int64 blockOffset;
    
    for(uint16 i = 0;i < rWriteAccessor->second.m_pBlockManager->numOfBlocks();++i)
    {
        pBlock = rWriteAccessor->second.m_pBlockManager->GetBlock(i);
        pCIDF = Block::GetCIDF(pBlock);
        if(pCIDF->m_flags & eBLF_Dirty)
        {
            //clear dirty flag
            pCIDF->m_flags &= ~eBLF_Dirty;
            //calc offset + write to disk
            blockOffset = rWriteAccessor->second.m_recordStart + (i * BLOCK_SIZE);
            IO::fseek(hDataFile, blockOffset, IO::IO_SEEK_SET);
            IO::fwrite(pBlock, BLOCK_SIZE, hDataFile);
        }
    }
}

bool DiskWriter::WriteDataWithRelocateFlag(HANDLE hDataFile, RecordIndexMap::accessor &rWriteAccessor)
{
    int64 requestDiskSize;
    int64 freePos;
    void *pBlocks;
    size_t blocksSize;
    
    requestDiskSize = rWriteAccessor->second.m_pBlockManager->numOfBlocks() * BLOCK_SIZE;
    freePos = GetFreeSpacePos(requestDiskSize);
    if(freePos == -1)
    {
        ReallocDataFile(hDataFile, requestDiskSize);
        freePos = GetFreeSpacePos(requestDiskSize);
        if(freePos == -1)
        {
            Log.Error(__FUNCTION__, "There is no freespace on disk drive. Record: " I64FMTD " is not written.", rWriteAccessor->first);
            return false;
        }
    }
    
    //set new record position
    rWriteAccessor->second.m_recordStart = freePos;
    
    //clear dirty flag
    rWriteAccessor->second.m_pBlockManager->ClearDirtyFlags();
    
    //calc blocks size and get 1st block its address is start
    blocksSize = rWriteAccessor->second.m_pBlockManager->numOfBlocks() * BLOCK_SIZE;
    pBlocks = rWriteAccessor->second.m_pBlockManager->GetBlock(0);
    
    //write to disk
    IO::fseek(hDataFile, freePos, IO::IO_SEEK_SET);
    IO::fwrite(pBlocks, blocksSize, hDataFile);
    
    return true;
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
    
    //process freespace dump task -> Send dump over socket
    {
        ProcessFreeSpaceDump();
    }
    
    //check if we have items to process
    {
        //lock
        std::lock_guard<std::mutex> rQGuard(m_rQueueLock);
        if(m_pQueue->size() == 0)
            return;
    }
    
    //open data file for write
    HANDLE hDataFile = INVALID_HANDLE_VALUE;
    IOHandleGuard rIOHandleDataGuard(hDataFile);
    hDataFile = IO::fopen(m_rStorage.m_rDataPath.c_str(), IO::IO_WRITE_ONLY, IO::IO_DIRECT);
    if(hDataFile == INVALID_HANDLE_VALUE)
    {
        Log.Error(__FUNCTION__, "Open datafile: %s failed. Error number: %d", m_rStorage.m_rDataPath.c_str(), errno);
        return;
    }
    
    //open index file for rw
    HANDLE hIndexFile = INVALID_HANDLE_VALUE;
    IOHandleGuard rIOHandleIndexGuard(hIndexFile);
    hIndexFile = IO::fopen(m_rStorage.m_rIndexPath.c_str(), IO::IO_RDWR, IO::IO_DIRECT);
    if(hIndexFile == INVALID_HANDLE_VALUE)
    {
        Log.Error(__FUNCTION__, "Open indexfile: %s failed. Error number: %d", m_rStorage.m_rIndexPath.c_str(), errno);
        return;
    }
    
	//start writing to disk
    //get all items from queue to process
    if(_S_DiskWriter_ProcessQueue(m_pQueue, m_rQueueLock, rAllItemsToProcess))
    {
        //write accessor
        RecordIndexMap::accessor rWriteAccessor;
        Vector<FreeSpace> rFreeSpaceToAdd(1024);
        
        //stats
        startTime = GetTickCount64();
        m_lastNumOfItemsInProcess = rAllItemsToProcess.size();
        m_itemsToProcessSize = rAllItemsToProcess.size();
        
        //sort by recordPosition - asc
        tbb::parallel_sort(rAllItemsToProcess.begin(), rAllItemsToProcess.end(), &_S_SortWriteInfoForWrite);
        
        //itreate items and write to disk
        for(DirtyXProcess::iterator itr = rAllItemsToProcess.begin();itr != rAllItemsToProcess.end();++itr)
        {
            //check if items exits and load all info about it
            if(m_rStorage.m_dataIndexes.find(rWriteAccessor, itr->m_key))
            {
                if(!(rWriteAccessor->second.m_flags & eRIF_RelocateBlocks))
                {
                    WriteDataWithoutRelocateFlag(hDataFile, rWriteAccessor);
                }
                else
                {
                    //queue adding freespace
                    if(rWriteAccessor->second.m_recordStart != -1)
                    {
                        rFreeSpaceToAdd.push_back(FreeSpace(rWriteAccessor->second.m_recordStart,
                                                            rWriteAccessor->second.m_blockCount * BLOCK_SIZE));
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
                rWriteAccessor->second.m_blockCount = rWriteAccessor->second.m_pBlockManager->numOfBlocks();
                
                //update crc32
                crc32 = rWriteAccessor->second.m_pBlockManager->GetBlocksCrc32();
                if(rWriteAccessor->second.m_crc32 != crc32)
                {
                    rWriteAccessor->second.m_crc32 = crc32;
                    rWriteAccessor->second.m_flags |= eRIF_Dirty;
                }
                
                //clear flag eRIF_RelocateBlocks
                rWriteAccessor->second.m_flags &= ~eRIF_RelocateBlocks;
                
                //update index on disk if changed
                if(rWriteAccessor->second.m_flags & eRIF_Dirty)
                {
                    m_rStorage.m_pDataIndexDiskWriter->WriteRecordIndexToDisk(hIndexFile, rWriteAccessor);
                    rWriteAccessor->second.m_flags &= ~eRIF_Dirty;
                }
            }
            else
            {
                Log.Warning(__FUNCTION__, "Data deleted before written to disk. X: " I64FMTD, itr->m_key);
                continue;
            }
            
            //!!!
            //at this point data and index file should be flushed to disk
            //!!!
            //!!!
            //Commit transaction before clear flag that record is NOT in disk write queue
            //else read thread will read old data, because it can be delete from memory
            //and loaded from disk again
            //!!!
            
            //clear flag eRIF_InDiskWriteQueue
            //cannot delete this record by MemoryWatcher if is wating in disk queue
            {
                std::lock_guard<std::mutex> rQGuard(m_rQueueLock);
                if(m_pQueue->containsKey(rWriteAccessor->first) == false)
                {
                    //unset InDiskWriteQueue flag
                    rWriteAccessor->second.m_flags &= ~eRIF_InDiskWriteQueue;
                }
            }
            
            //release accessor
            rWriteAccessor.release();
            
            //stats
            --m_itemsToProcessSize;
            
        } /* end for */
        
        //add frespace from queue to Storage after transaction commit
        if(rFreeSpaceToAdd.size())
        {
            for(Vector<FreeSpace>::iterator itr = rFreeSpaceToAdd.begin();itr != rFreeSpaceToAdd.end();++itr)
            {
                AddFreeSpace(itr->m_pos, itr->m_lenght);
            }
        }
        
        //calc avg write time
        endTime = GetTickCount64() - startTime;
        m_sumDiskWriteTime += endTime;
        g_AvgDiskWriteTime = m_sumDiskWriteTime.load() / g_NumOfWritesToDisk.load();
        
    } /* end if */
}

void DiskWriter::Remove(uint64 x)
{
    std::lock_guard<std::mutex> rQGuard(m_rQueueLock);
    m_pQueue->remove(x);
}

void DiskWriter::ReallocDataFile(HANDLE hDataFile, int64 minSize, bool oAddFreeSpace /*= true*/)
{
	int64 reallocSize;
    int64 startFreeSpace;
    
    //calc size
    reallocSize = std::max(g_ReallocSize, minSize);
    
    //seek end + get new freespace start position
    IO::fseek(hDataFile, 0, IO::IO_SEEK_END);
    startFreeSpace = IO::ftell(hDataFile);
    
	//set end of file
	IO::fresize(hDataFile, startFreeSpace + reallocSize);
    
	//add free space
    if(oAddFreeSpace)
    {
        AddFreeSpace(startFreeSpace, reallocSize);
        DefragmentFreeSpace();
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
    //check if we have items to process
    {
        std::lock_guard<std::mutex> rRIDel_Guard(m_rRIDelQueueLock);
        if(m_pRIDelQueue->size() == 0)
            return;
    }
    
    //open index file for rw
    HANDLE hIndexFile = INVALID_HANDLE_VALUE;
    IOHandleGuard rIOHandleIndexGuard(hIndexFile);
    hIndexFile = IO::fopen(m_rStorage.m_rIndexPath.c_str(), IO::IO_RDWR, IO::IO_DIRECT);
    if(hIndexFile == INVALID_HANDLE_VALUE)
    {
        Log.Error(__FUNCTION__, "Open indexfile: %s failed. Error number: %d", m_rStorage.m_rIndexPath.c_str(), errno);
        return;
    }
    
    //copy data to vector, clear queue
    Vector<RecordIndex, uint64> rDeleteQueue;
    _S_DiskWriter_GetItemsFromIndexDelQueue(m_pRIDelQueue, m_rRIDelQueueLock, rDeleteQueue);
    
    //delete all records from index
    for(uint64 i = 0;i < rDeleteQueue.size();++i)
    {
        //get record
        const RecordIndex &rRecordIndex = rDeleteQueue[i];
        
        //add free space
        if(rRecordIndex.m_recordStart != -1)
        {
            AddFreeSpace(rRecordIndex.m_recordStart, rRecordIndex.m_blockCount * BLOCK_SIZE);
        }
        
        //delete record index from disk
        m_rStorage.m_pDataIndexDiskWriter->EraseRecord(hIndexFile, rRecordIndex);
    }
}

void DiskWriter::AddFreeSpace(int64 pos, int64 lenght)
{
    if(pos < 0 || lenght <= 0)
    {
        Log.Warning(__FUNCTION__, "pos < 0 || lenght <= 0");
        return;
    }
    
    FreeSpaceBlockMap::iterator itr = m_rFreeSpace.find(lenght);
    if(itr != m_rFreeSpace.end())
    {
        itr->second.push_back(pos);
    }
    else
    {
        FreeSpaceOffsets rFreeSpaceOffsets;
        rFreeSpaceOffsets.push_back(pos);
        m_rFreeSpace[lenght] = std::move(rFreeSpaceOffsets);
    }
}

struct PredGreater
{
    explicit PredGreater(int64 x) : m_x(x) {}
    INLINE bool operator()(const DiskWriter::FreeSpaceBlockMap::value_type & p) { return (m_x < p.first); }
private:
    int64 m_x;
};

int64 DiskWriter::GetFreeSpacePos(int64 lenght)
{
    int64 returnPos = -1;
    int64 newSize;
    int64 newPos;
    
    FreeSpaceBlockMap::iterator itr = m_rFreeSpace.find(lenght);
    if(itr != m_rFreeSpace.end())
    {
        returnPos = itr->second.back();
        itr->second.pop_back();
        //erase from map if empty
        if(itr->second.empty())
        {
            m_rFreeSpace.erase(itr);
        }
    }
    else
    {
        FreeSpaceBlockMap::iterator itr2 = std::find_if(m_rFreeSpace.begin(), m_rFreeSpace.end(), PredGreater(lenght));
        if(itr2 != m_rFreeSpace.end())
        {
            //get 1st position and remove from list
            returnPos = itr2->second.back();
            itr2->second.pop_back();
            //
            newSize = itr2->first - lenght;
            newPos = returnPos + lenght;
            
            //update or add new position
            itr = m_rFreeSpace.find(newSize);
            if(itr != m_rFreeSpace.end())
            {
                itr->second.push_back(newPos);
            }
            else
            {
                FreeSpaceOffsets rFreeSpaceOffsets;
                rFreeSpaceOffsets.push_back(newPos);
                m_rFreeSpace[newSize] = std::move(rFreeSpaceOffsets);
            }
            
            //erase from map if empty
            if(itr2->second.empty())
            {
                m_rFreeSpace.erase(itr2);
            }
        }
    }
    
    return returnPos;
}

void DiskWriter::DefragmentFreeSpace()
{
    //TODO: DefragmentFreeSpace
}

NOINLINE static void _S_DiskWriter_GetItemsFromFreeSpaceDump(DiskWriter::FreeSpaceDumpTask *pFreeSpaceDump,
                                                             std::mutex &rFreeSpaceDumpLock,
                                                             Vector<std::pair<uint64, FreeSpaceTask> > &rKeyPair)
{
    std::lock_guard<std::mutex> rGuard(rFreeSpaceDumpLock);
    if(pFreeSpaceDump->size())
    {
        rKeyPair.reserve(pFreeSpaceDump->size());
        pFreeSpaceDump->getKeyValuePairs(rKeyPair);
        pFreeSpaceDump->clear();
    }
}

static uint32 _S_DiskWriter_GetFreeSpaceDump(ByteBuffer &rBuff,
                                           DiskWriter::FreeSpaceBlockMap &rFreeSpaceBlockMap,
                                           int64 dataFileSize,
                                           bool fullDump)
{
    //prealloc buffer
    if(fullDump == true)
    {
        size_t reserveSize = 0;
        for(DiskWriter::FreeSpaceBlockMap::iterator itr = rFreeSpaceBlockMap.begin();itr != rFreeSpaceBlockMap.end();++itr)
        {
            reserveSize += sizeof(int64);
            reserveSize += itr->second.size() * sizeof(int64);
        }
        rBuff.reserve(reserveSize + sizeof(uint64));
    }
    
    //data file size
    rBuff << uint64(dataFileSize);
    //freeSpace size
    rBuff << uint64(rFreeSpaceBlockMap.size());
    //
    for(DiskWriter::FreeSpaceBlockMap::iterator itr = rFreeSpaceBlockMap.begin();itr != rFreeSpaceBlockMap.end();++itr)
    {
        rBuff << uint64(itr->first);
        rBuff << uint64(itr->second.size());
        if(fullDump == true)
        {
            for(DiskWriter::FreeSpaceOffsets::iterator itr2 = itr->second.begin();itr2 != itr->second.end();++itr2)
            {
                rBuff << uint64(*itr2);
            }
        }
    }
    
	//try to compress
	if(rBuff.size() > (size_t)g_DataSizeForCompression)
	{
        ByteBuffer rBuffOut;
		int compressionStatus = CommonFunctions::compressGzip(g_GzipCompressionLevel, rBuff.contents(), rBuff.size(), rBuffOut);
		if(compressionStatus == Z_OK)
		{
			Log.Debug(__FUNCTION__, "Data compressed. Original size: %u, new size: %u", rBuff.size(), rBuffOut.size());
            //add compressed data
            rBuff.clear();
            rBuff.append(rBuffOut);
            return ePF_COMPRESSED;
		}
        else
        {
            Log.Error(__FUNCTION__, "Data compression failed.");
        }
	}
    
    return ePF_NULL;
}

void DiskWriter::ProcessFreeSpaceDump()
{
    {
        std::lock_guard<std::mutex> rGuard(m_rFreeSpaceDumpLock);
        if(m_pFreeSpaceDump->size() == 0)
            return;
    }
    
    //copy to vector clean data from map
    Vector<std::pair<uint64, FreeSpaceTask> > rKeyPair;
    _S_DiskWriter_GetItemsFromFreeSpaceDump(m_pFreeSpaceDump, m_rFreeSpaceDumpLock, rKeyPair);
    
    //
    ByteBuffer rDump;
    ByteBuffer rFullDump;
    uint32 dumpFlags = 0;
    uint32 fullDumpFlags = 0;
    OUTPACKET_RESULT result;
    
    //fill buffers
    for(size_t i = 0;i < rKeyPair.size();++i)
    {
        const FreeSpaceTask &rFreeSpaceTask = rKeyPair[i].second;
        if(rFreeSpaceTask.m_dumpFlags == eSFSDF_FULL && rFullDump.size() == 0)
        {
            fullDumpFlags = _S_DiskWriter_GetFreeSpaceDump(rFullDump, m_rFreeSpace, m_rStorage.m_dataFileSize, true);
        }
        else if(rDump.size() == 0)
        {
            dumpFlags = _S_DiskWriter_GetFreeSpaceDump(rDump, m_rFreeSpace, m_rStorage.m_dataFileSize, false);
        }
    }

    //distribute to clients
    for(size_t i = 0;i < rKeyPair.size();++i)
    {
        const std::pair<uint64, FreeSpaceTask> &rPair = rKeyPair[i];
        
        //set variables
        ByteBuffer &rBuff = rPair.second.m_dumpFlags == eSFSDF_FULL ? rFullDump : rDump;
        uint32 flags = rPair.second.m_dumpFlags == eSFSDF_FULL ? fullDumpFlags : dumpFlags;
        size_t dataSize = rPair.second.m_dumpFlags == eSFSDF_FULL ? rFullDump.size() : rDump.size();
        
        //init stream send
        Packet rResponse(S_MSG_GET_FREESPACE, 32);
        rResponse << rPair.second.m_token;
        rResponse << flags;
        result = g_rClientSocketHolder.StartStreamSend(rPair.first, rResponse, dataSize);
        //socket disconnection or something go to next socket
        if(result != OUTPACKET_RESULT_SUCCESS)
            continue;
        
        //send all chunks
        uint8 arChunk[4096];
        size_t remainingData;
        size_t readDataSize;
        while(rBuff.rpos() < rBuff.size())
        {
            remainingData = rBuff.size() - rBuff.rpos();
            readDataSize = std::min(remainingData, sizeof(arChunk));
            rBuff.read((uint8*)&arChunk, readDataSize);
            //send chunk
        trySendAgain:
            result = g_rClientSocketHolder.StreamSend(rPair.first, &arChunk, readDataSize);
            if(result == OUTPACKET_RESULT_SUCCESS)
            {
                //send is ok continue with sending
                continue;
            }
            else if(result == OUTPACKET_RESULT_NO_ROOM_IN_BUFFER)
            {
                //socket buffer is full wait for
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                goto trySendAgain;
            }
            else
            {
                //error interupt sending
                Log.Error(__FUNCTION__, "StreamSend error: %u", (uint32)result);
                break;
            }
        }
    }
}































