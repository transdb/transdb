//
//  DiskWriter.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/31/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

typedef void (DiskWriter::*DiskWriterTaskHandler)(uint64, ByteBuffer&);
static const DiskWriterTaskHandler m_DiskWriterTaskHandlers[edtNUM] =
{
    NULL,
    &DiskWriter::HandleFreeSpaceDump,
    &DiskWriter::HandleDefragmentFreeSpace,
};

DiskWriter::DiskWriter(Storage &rStorage) : m_rStorage(rStorage),
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

void DiskWriter::QueueTask(uint64 socketID, EDT eTaskType, ByteBuffer &rData)
{
    //lock
    std::lock_guard<std::mutex> rGuard(m_rDiskWriteTasksLock);
    
    //add to quue
    m_rDiskWriteTasks.emplace_back(socketID, eTaskType, rData);
}

INLINE static bool _S_SortWriteInfoForWrite(const WriteInfo &rWriteInfo1, const WriteInfo &rWriteInfo2)
{
    return rWriteInfo1.m_recordPosition < rWriteInfo2.m_recordPosition;
}

static bool _S_DiskWriter_ProcessQueue(DiskWriter::DirtyXQueue *pQueue,
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
    for(uint16 i = 0;i < rWriteAccessor->second.m_pBlockManager->blockCount;++i)
    {
        uint8 *pBlock = blman_get_block(rWriteAccessor->second.m_pBlockManager, i);
        CIDF *pCIDF = Block_GetCIDF(pBlock);
        if(pCIDF->m_flags & eBLF_Dirty)
        {
            //clear dirty flag
            pCIDF->m_flags &= ~eBLF_Dirty;
            //calc offset + write to disk
            int64 blockOffset = rWriteAccessor->second.m_recordStart + (i * BLOCK_SIZE);
            IO::fseek(hDataFile, blockOffset, IO::IO_SEEK_SET);
            IO::fwrite(pBlock, BLOCK_SIZE, hDataFile);
        }
    }
}

bool DiskWriter::WriteDataWithRelocateFlag(HANDLE hDataFile, RecordIndexMap::accessor &rWriteAccessor)
{
    int64 requestDiskSize = rWriteAccessor->second.m_pBlockManager->blockCount * BLOCK_SIZE;
    int64 freePos = GetFreeSpacePos(requestDiskSize);
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
    blman_clear_dirty_flags(rWriteAccessor->second.m_pBlockManager);
    
    //calc blocks size and get 1st block its address is start
    size_t blocksSize = rWriteAccessor->second.m_pBlockManager->blockCount * BLOCK_SIZE;
    void *pBlocks = rWriteAccessor->second.m_pBlockManager->blocks;
    
    //write to disk
    IO::fseek(hDataFile, freePos, IO::IO_SEEK_SET);
    IO::fwrite(pBlocks, blocksSize, hDataFile);
    
    return true;
}

void DiskWriter::Process()
{    
    //process index delete queue (delete from disk)
    {
        ProcessIndexDeleteQueue();
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
        Log.Error(__FUNCTION__, "Open datafile: %s failed. Error number: %d", m_rStorage.m_rDataPath.c_str(), IO::ferror());
        return;
    }
    
    //open index file for rw
    HANDLE hIndexFile = INVALID_HANDLE_VALUE;
    IOHandleGuard rIOHandleIndexGuard(hIndexFile);
    hIndexFile = IO::fopen(m_rStorage.m_rIndexPath.c_str(), IO::IO_RDWR, IO::IO_DIRECT);
    if(hIndexFile == INVALID_HANDLE_VALUE)
    {
        Log.Error(__FUNCTION__, "Open indexfile: %s failed. Error number: %d", m_rStorage.m_rIndexPath.c_str(), IO::ferror());
        return;
    }
    
	//start writing to disk
	DirtyXProcess rAllItemsToProcess;
	DirtyXProcess rItemsToProcess;

    //get all items from queue to process
    if(_S_DiskWriter_ProcessQueue(m_pQueue, m_rQueueLock, rAllItemsToProcess))
    {
        //write accessor
        RecordIndexMap::accessor rWriteAccessor;
        Vector<FreeSpace> rFreeSpaceToAdd(1024);
        
        //stats
        uint64 startTime = GetTickCount64();
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
                    //save recordstart to add freespace if write to disk is successfull
                    int64 recordStartTmp = rWriteAccessor->second.m_recordStart;
                    
                    //this funtion will set new rWriteAccessor->second.m_recordStart
                    //this function will fail if there is no disk space
                    bool status = WriteDataWithRelocateFlag(hDataFile, rWriteAccessor);
                    if(status == false)
                    {
                        continue;
                    }
                    
                    //queue adding freespace
                    if(recordStartTmp != -1)
                    {
                        rFreeSpaceToAdd.push_back(FreeSpace(recordStartTmp, rWriteAccessor->second.m_blockCount * BLOCK_SIZE));
                    }
                }
                
                //stats
                g_stats.NumOfWritesToDisk++;
                
                //update block count
                rWriteAccessor->second.m_blockCount = rWriteAccessor->second.m_pBlockManager->blockCount;
                
                //update crc32
                uint32 crc32 = blman_get_blocks_crc32(rWriteAccessor->second.m_pBlockManager);
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
                DiskWriter::AddFreeSpace(m_rFreeSpace, itr->m_pos, itr->m_lenght);
            }
        }
        
        //calc avg write time
        uint64 endTime = GetTickCount64() - startTime;
        m_sumDiskWriteTime += endTime;
        g_stats.AvgDiskWriteTime = m_sumDiskWriteTime / g_stats.NumOfWritesToDisk;
        
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
    reallocSize = std::max(g_cfg.ReallocSize, minSize);
    
    //seek end + get new freespace start position
    IO::fseek(hDataFile, 0, IO::IO_SEEK_END);
    startFreeSpace = IO::ftell(hDataFile);
    
	//set end of file
	IO::fresize(hDataFile, startFreeSpace + reallocSize);
    
	//add free space
    if(oAddFreeSpace)
    {
        DiskWriter::AddFreeSpace(m_rFreeSpace, startFreeSpace, reallocSize);
    }
    
    //update dataFileSize
    m_rStorage.m_dataFileSize += reallocSize;
}


static void _S_DiskWriter_GetItemsFromIndexDelQueue(DiskWriter::RIDelQueue *pRIDelQueue,
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
        Log.Error(__FUNCTION__, "Open indexfile: %s failed. Error number: %d", m_rStorage.m_rIndexPath.c_str(), IO::ferror());
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
            DiskWriter::AddFreeSpace(m_rFreeSpace, rRecordIndex.m_recordStart, rRecordIndex.m_blockCount * BLOCK_SIZE);
        }
        
        //delete record index from disk
        m_rStorage.m_pDataIndexDiskWriter->EraseRecord(hIndexFile, rRecordIndex);
    }
}

void DiskWriter::AddFreeSpace(FreeSpaceBlockMap &rFreeSpace, int64 pos, int64 lenght)
{
    if(pos < 0 || lenght <= 0)
    {
        Log.Warning(__FUNCTION__, "pos < 0 || lenght <= 0");
        return;
    }
    
    FreeSpaceBlockMap::iterator itr = rFreeSpace.find(lenght);
    if(itr != rFreeSpace.end())
    {
        itr->second.push_back(pos);
    }
    else
    {
        FreeSpaceOffsets rFreeSpaceOffsets;
        rFreeSpaceOffsets.push_back(pos);
        rFreeSpace[lenght] = std::move(rFreeSpaceOffsets);
    }
}

struct PredGreater
{
    explicit PredGreater(int64 lenght) : m_lenght(lenght) {}
    INLINE bool operator()(const FreeSpaceBlockMap::value_type & p) { return (m_lenght < p.first); }
private:
    int64 m_lenght;
};

int64 DiskWriter::GetFreeSpacePos(int64 lenght)
{
    int64 returnPos = -1;
    
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
            int64 newSize = itr2->first - lenght;
            int64 newPos = returnPos + lenght;
            
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
    //open index file for read only
    HANDLE hIndexFile = INVALID_HANDLE_VALUE;
    IOHandleGuard rIOHandleIndexGuard(hIndexFile);
    hIndexFile = IO::fopen(m_rStorage.m_rIndexPath.c_str(), IO::IO_READ_ONLY, IO::IO_DIRECT);
    if(hIndexFile == INVALID_HANDLE_VALUE)
    {
        Log.Error(__FUNCTION__, "Open indexfile: %s failed. Error number: %d", m_rStorage.m_rIndexPath.c_str(), IO::ferror());
        return;
    }
    
    //reload freespace from index file
    E_IIS status = m_rStorage.m_pDataIndexDiskWriter->Init(hIndexFile, NULL, m_rFreeSpace, m_rStorage.m_dataFileSize);
    if(status != eIIS_OK)
    {
        Log.Error(__FUNCTION__, "DefragmentFreeSpace failed.");
    }
}

static void _S_DiskWriter_GetItemsFromDiskWriteTasks(DiskWriter::DiskWriteTasksVec &rDiskWriteTasks,
                                                     std::mutex &rDiskWriteTasksLock,
                                                     DiskWriter::DiskWriteTasksVec &rDiskWriteTasksOut)
{
    std::lock_guard<std::mutex> rGuard(rDiskWriteTasksLock);
    if(rDiskWriteTasks.size())
    {
        rDiskWriteTasksOut = std::move(rDiskWriteTasks);
    }
}

void DiskWriter::ProcessTasks()
{
    //check if we have items to process
    {
        std::lock_guard<std::mutex> rGuard(m_rDiskWriteTasksLock);
        if(m_rDiskWriteTasks.empty())
            return;
    }
    
    //move data to local vector, we dont want to hold lock when processing data
    DiskWriteTasksVec rDiskWriteTasksProcess;
    _S_DiskWriter_GetItemsFromDiskWriteTasks(m_rDiskWriteTasks, m_rDiskWriteTasksLock, rDiskWriteTasksProcess);
    
    //process
    for(size_t i = 0;i < rDiskWriteTasksProcess.size();++i)
    {
        //get task
        DiskWriterTask &rDiskWriterTask = rDiskWriteTasksProcess[i];
        
        //process handler
        if(rDiskWriterTask.m_eTaskType < edtNUM && m_DiskWriterTaskHandlers[rDiskWriterTask.m_eTaskType] != NULL)
        {
            (void)(this->*m_DiskWriterTaskHandlers[rDiskWriterTask.m_eTaskType])(rDiskWriterTask.m_socketID, rDiskWriterTask.m_rData);
        }
    }
}

static void _S_DiskWriter_GetFreeSpaceDump(bbuff *pDump,
                                           FreeSpaceBlockMap &rFreeSpaceBlockMap,
                                           int64 dataFileSize,
                                           bool fullDump)
{
    //prealloc buffer
    if(fullDump == true)
    {
        size_t reserveSize = 0;
        for(FreeSpaceBlockMap::iterator itr = rFreeSpaceBlockMap.begin();itr != rFreeSpaceBlockMap.end();++itr)
        {
            reserveSize += sizeof(int64);
            reserveSize += itr->second.size() * sizeof(int64);
        }
        bbuff_reserve(pDump, pDump->wpos + reserveSize + sizeof(uint64));
    }
    else
    {
        bbuff_reserve(pDump, pDump->wpos + rFreeSpaceBlockMap.size() * sizeof(uint64) * 2 + 16);
    }
    
    //data file size
    bbuff_append(pDump, &dataFileSize, sizeof(dataFileSize));
    //freeSpace size
    uint64 freeSpaceSize = rFreeSpaceBlockMap.size();
    bbuff_append(pDump, &freeSpaceSize, sizeof(freeSpaceSize));
    //
    for(FreeSpaceBlockMap::iterator itr = rFreeSpaceBlockMap.begin();itr != rFreeSpaceBlockMap.end();++itr)
    {
        //append block size
        bbuff_append(pDump, &itr->first, sizeof(itr->first));
        //append block count
        uint64 blockCount = itr->second.size();
        bbuff_append(pDump, &blockCount, sizeof(blockCount));
        if(fullDump == true)
        {
            bbuff_append(pDump, itr->second.data(), itr->second.size() * sizeof(uint64));
        }
    }
}

class DumpFreeSpaceThread : public ThreadContext
{
public:
    explicit DumpFreeSpaceThread(uint64 socketID,
                                 int64 dataFileSize,
                                 ByteBuffer &rData,
                                 const FreeSpaceBlockMap &rFreeSpace) : m_socketID(socketID), m_dataFileSize(dataFileSize), m_rData(std::move(rData)), m_rFreeSpace(rFreeSpace)
    {
        
    }
    
    //ThreadContext
    bool run()
    {
        CommonFunctions::SetThreadName("DumpFreeSpaceThread thread - socketID: " I64FMTD, m_socketID);
        
        uint32 token;
        uint32 flags;
        uint32 dumpFlags;
        static const size_t packetSize = sizeof(token)+sizeof(flags);
        
        //unpack data
        m_rData >> token >> flags >> dumpFlags;
        
        //create buffer
        bbuff *pDump;
        bbuff_create(pDump);
        
        //prepare packet
        bbuff_reserve(pDump, packetSize);
        bbuff_append(pDump, &token, sizeof(token));
        bbuff_append(pDump, &flags, sizeof(flags));
        
        //dump
        if(dumpFlags == eSFSDF_FULL)
        {
            _S_DiskWriter_GetFreeSpaceDump(pDump, m_rFreeSpace, m_dataFileSize, true);
        }
        else
        {
            _S_DiskWriter_GetFreeSpaceDump(pDump, m_rFreeSpace, m_dataFileSize, false);
        }
        
        //calc where data start and what size it has
        uint8 *pDumpDataBegin = pDump->storage + packetSize;
        size_t dumpSize = pDump->size - packetSize;
        
        //try to compress
        if(dumpSize > (size_t)g_cfg.DataSizeForCompression)
        {
            bbuff *pBuffOut;
            bbuff_create(pBuffOut);
            int compressionStatus = CCommon_compressGzip(g_cfg.GzipCompressionLevel, pDumpDataBegin, dumpSize, pBuffOut, g_cfg.ZlibBufferSize);
            if(compressionStatus == Z_OK)
            {
                flags = ePF_COMPRESSED;
                //modify flags
                bbuff_put(pDump, sizeof(token), &flags, sizeof(flags));
                //prepare space and rewrite noncompressed data
                bbuff_resize(pDump, pBuffOut->size + packetSize);
                bbuff_put(pDump, packetSize, pBuffOut->storage, pBuffOut->size);
            }
            else
            {
                Log.Error(__FUNCTION__, "Data compression failed.");
            }
            bbuff_destroy(pBuffOut);
        }
        
        //send data
        g_rClientSocketHolder.SendPacket(m_socketID, S_MSG_GET_FREESPACE, pDump);
        
        //clear memory
        bbuff_destroy(pDump);
        return true;
    }
    
private:
    uint64              m_socketID;
    int64               m_dataFileSize;
    ByteBuffer          m_rData;
    FreeSpaceBlockMap   m_rFreeSpace;
};

void DiskWriter::HandleFreeSpaceDump(uint64 socketID, ByteBuffer &rData)
{
    //copy freespace map and run in separate thread
    DumpFreeSpaceThread *pDumpFreeSpaceThread = new DumpFreeSpaceThread(socketID, m_rStorage.m_dataFileSize, rData, m_rFreeSpace);
    ThreadPool.ExecuteTask(pDumpFreeSpaceThread);
}

void DiskWriter::HandleDefragmentFreeSpace(uint64 socketID, ByteBuffer &rData)
{
    uint32 token;
    uint32 flags;
    
    //unpack data
    rData >> token >> flags;
    
    //defragment
    {
        DefragmentFreeSpace();
    }
    
    //send finished packet
    uint8 buff[32];
    StackPacket rResponse(S_MSG_DEFRAGMENT_FREESPACE, buff, sizeof(buff));
    rResponse << token;
    rResponse << flags;
    g_rClientSocketHolder.SendPacket(socketID, rResponse);
}





























