//
//  Storage.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/20/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

INLINE static bool SortWriteInfoForCRC32Check(const WriteInfo &rWriteInfo1, const WriteInfo &rWriteInfo2)
{
    return rWriteInfo1.m_recordPosition < rWriteInfo2.m_recordPosition;
}

Storage::Storage(const std::string &rFileName) : m_rDataPath(g_DataFilePath + rFileName + ".dat"),
                                                 m_rIndexPath(g_IndexFilePath + rFileName + ".idx")
{
	m_sumDiskReadTime		    = 0;
    m_dataFileSize              = 0;
    m_diskWriterCount           = 0;
    m_memoryUsed                = 0;
    m_pLRUCache                 = new LRUCache("Storage", g_LRUCacheMemReserve / sizeof(CRec));
       
    //check if data file exits if not create it
    CommonFunctions::CheckFileExists(m_rDataPath.c_str(), true);
    
	//open data file
    HANDLE rDataFileHandle;
    IOHandleGuard rIOHandleGuard(&rDataFileHandle);
    rDataFileHandle = IO::fopen(m_rDataPath.c_str(), IO::IO_RDWR);
    if(rDataFileHandle == INVALID_HANDLE_VALUE)
    {
        Log.Error(__FUNCTION__, "Cannot open data file: %s.", m_rDataPath.c_str());
        assert(rDataFileHandle != INVALID_HANDLE_VALUE);
    }
    
    //create disk writer
    m_pDiskWriter = new DiskWriter(*this);
    
	//resize if needed
    IO::fseek(rDataFileHandle, 0, IO::IO_SEEK_END);
	m_dataFileSize = IO::ftell(rDataFileHandle);
    IO::fseek(rDataFileHandle, 0, IO::IO_SEEK_SET);
	if(m_dataFileSize == 0)
	{
        //this function will update m_dataFileSize
        m_pDiskWriter->ReallocDataFile(rDataFileHandle, g_ReallocSize, false);
	}
	Log.Notice(__FUNCTION__, "Data file: %s - loaded. Size: " SI64FMTD " bytes", m_rDataPath.c_str(), m_dataFileSize.load());
            
	//load indexes and freespaces
    //create writter
    m_pDataIndexDiskWriter = new IndexBlock();
    //load indexes to memory
    int64 indexFileSize;
    m_pDataIndexDiskWriter->Init(this, m_rIndexPath, m_dataIndexes, m_dataFileSize, &indexFileSize);
	Log.Notice(__FUNCTION__, "Index file: %s - loaded. Size: " SI64FMTD " bytes", m_rIndexPath.c_str(), indexFileSize);
    
	//check all data
	if(g_StartupCrc32Check)
	{
        Crc32Check(rDataFileHandle);
	}
}

Storage::~Storage()
{
    //do all pending disk writes + delete
	Log.Notice(__FUNCTION__, "Processing pending disk writes.");
	{
		if(m_pDiskWriter->HasTasks())
		{
			//process
			m_pDiskWriter->Process();
			delete m_pDiskWriter;
			m_pDiskWriter = NULL;
		}
	}
	Log.Notice(__FUNCTION__, "Finished processing pending disk writes.");
    
    //delete index disk writer
    delete m_pDataIndexDiskWriter;
	m_pDataIndexDiskWriter = NULL;
    
    //delete LRU cache
    delete m_pLRUCache;
	m_pLRUCache = NULL;
}

void Storage::LoadFromDisk(const HANDLE &rDataFileHandle, const uint64 &x)
{
    RecordIndexMap::accessor rWriteAccessor;
    if(m_dataIndexes.find(rWriteAccessor, x))
    {
        //update LRU
        {
            std::lock_guard<std::mutex> rGuard(m_LRULock);
            m_pLRUCache->put(x);
        }
        
        //check manager - load from disk if not in memory
        CheckBlockManager(rDataFileHandle, x, rWriteAccessor);
    }
}

void Storage::Crc32Check(const HANDLE &rDataFileHandle)
{
    Log.Notice(__FUNCTION__, "Checking integrity of data file. This can take some time.");
    uint64 counter = 0;
    size_t dataIndexesSize;
    typedef Vector<WriteInfo> WriteInfoVec;
    WriteInfoVec rInfo;
    
    //
    dataIndexesSize = m_dataIndexes.size();
    
    //fill list
    rInfo.reserve(dataIndexesSize);
    for(RecordIndexMap::const_iterator itr = m_dataIndexes.begin();itr != m_dataIndexes.end();++itr)
    {
        rInfo.push_back(WriteInfo(itr->first, itr->second->m_recordStart));
    }
    
    //sort by record pos
    tbb::parallel_sort(rInfo.begin(), rInfo.end(), &SortWriteInfoForCRC32Check);
    
    //check crc32
    for(WriteInfoVec::iterator itr = rInfo.begin();itr != rInfo.end();++itr)
    {
        ++counter;
        LoadFromDisk(rDataFileHandle, itr->m_key);
        //
        CheckMemory();
        
        if(!(counter % 100000))
        {
            Log.Notice(__FUNCTION__, "Checked: " I64FMTD " from: " I64FMTD, counter, dataIndexesSize);
        }
    }
    Log.Notice(__FUNCTION__, "Finished checking integrity of data file.");
}

void Storage::ReadData(const HANDLE &rDataFileHandle, const uint64 &x, const uint64 &y, ByteBuffer &rData)
{
    RecordIndexMap::accessor rWriteAccessor;
    if(m_dataIndexes.find(rWriteAccessor, x))
    {
        //update LRU
        {
            std::lock_guard<std::mutex> rGuard(m_LRULock);
            m_pLRUCache->put(x);
        }
        
        //check manager - load from disk if not in memory
        CheckBlockManager(rDataFileHandle, x, rWriteAccessor);
        
        //read data
        rWriteAccessor->second->m_pBlockManager->ReadRecord(y, rData);
    }
    
    Log.Debug(__FUNCTION__, "Read data [x:" I64FMTD ",y:" I64FMTD "] size: " I64FMTD, x, y, rData.size());
}

void Storage::ReadData(const HANDLE &rDataFileHandle, const uint64 &x, ByteBuffer &rData)
{
    RecordIndexMap::accessor rWriteAccessor;
    if(m_dataIndexes.find(rWriteAccessor, x))
    {
        //update LRU
        {
            std::lock_guard<std::mutex> rGuard(m_LRULock);
            m_pLRUCache->put(x);
        }
        
        //check manager - load from disk if not in memory
        CheckBlockManager(rDataFileHandle, x, rWriteAccessor);
        
        //read data
        rWriteAccessor->second->m_pBlockManager->ReadRecords(rData);
    }
    
    Log.Debug(__FUNCTION__, "Read data [x:" I64FMTD ",y:" I64FMTD "] size: " I64FMTD, x, 0, rData.size());
}

uint32 Storage::WriteData(const HANDLE &rDataFileHandle, const uint64 &x, const uint64 &y, const uint8 *pRecord, const uint16 &recordSize)
{
    //ret value
    uint32 status;
    
    //write accesor
	RecordIndexMap::accessor rWriteAccesor;
    
    //update LRU
    {
        std::lock_guard<std::mutex> rGuard(m_LRULock);
        m_pLRUCache->put(x);
    }
    
    //try insert - returns false if record is existing
    if(m_dataIndexes.insert(rWriteAccesor, x))
    {
        //allocate recordIndex struct
        {
            std::lock_guard<std::mutex> rRI_Guard(m_rRecordIndexMemPoolLock);
            rWriteAccesor->second = m_rRecordIndexMemPool.allocate();
        }
        
        //allocate blockmanager
        //create block manager + add new block for write + update num of blocks
        {
            std::lock_guard<std::mutex> rBM_Guard(m_rBlockManagerMemPoolLock);
            rWriteAccesor->second->m_pBlockManager = m_rBlockManagerMemPool.allocate(*this);
            rWriteAccesor->second->m_blockCount = rWriteAccesor->second->m_pBlockManager->numOfBlocks();
        }
        
        //init index block
        rWriteAccesor->second->m_IB_blockNumber = 0;
        rWriteAccesor->second->m_IB_recordOffset = -1;
        //set flags to eRIF_RealocateBlocks
        rWriteAccesor->second->m_flags = eRIF_RelocateBlocks | eRIF_Dirty;
        //write data to block
        status = rWriteAccesor->second->m_pBlockManager->WriteRecord(y, pRecord, recordSize);
        //set record start to -1 (data are not written on disk)
        rWriteAccesor->second->m_recordStart = -1;
            
        //inc memory use
        m_memoryUsed += ((rWriteAccesor->second->m_pBlockManager->numOfBlocks() * BLOCK_SIZE) + sizeof(BlockManager));
               
        //queue write to disk
        m_pDiskWriter->Queue(rWriteAccesor);
    }
	else
	{
        //now we have locked write access, just update data
        //check manager - load from disk if not in memory
        CheckBlockManager(rDataFileHandle, x, rWriteAccesor);
        //
        status = rWriteAccesor->second->m_pBlockManager->WriteRecord(y, pRecord, recordSize);
        if(status == eBMS_FailRecordTooBig)
        {
            Log.Warning(__FUNCTION__, "Record [x:" I64FMTD ", y:" I64FMTD "] size: %u is too big, max record size is: %u.", x, y, recordSize, MAX_RECORD_SIZE);
        }
        else if(status & eBMS_OldRecord)
        {
            Log.Warning(__FUNCTION__, "Record [x:" I64FMTD ", y:" I64FMTD "] size: %u attempt to save old y.", x, y, recordSize);
        }
        else if(status & eBMS_NeedDefrag)
        {
            //if data needs defrag
            //this function takes care about freespace, memory usage, disk writer, sets correct flags
            DefragmentDataInternal(rWriteAccesor);
        }
        else if(status & eBMS_ReallocatedBlocks)
        {
            if(!(rWriteAccesor->second->m_flags & eRIF_RelocateBlocks))
            {
                //set flag reallocate blocks so diskwrite must find new place for data
                rWriteAccesor->second->m_flags |= eRIF_RelocateBlocks;
            }
            
            //set dirty
            rWriteAccesor->second->m_flags |= eRIF_Dirty;
            
            //inc memory use
            m_memoryUsed += BLOCK_SIZE;
            
            //queue write to disk
            m_pDiskWriter->Queue(rWriteAccesor);
        }
        else
        {
            //queue write to disk
            m_pDiskWriter->Queue(rWriteAccesor);
        }
	}
    
    //log
    Log.Debug(__FUNCTION__, "Written data [x:" I64FMTD ", y:" I64FMTD "] size: %u with status: %u", x, y, recordSize, status);

    return status;
}

void Storage::DeleteData(const uint64 &x)
{
	std::streamsize recordSize;

    RecordIndexMap::accessor rWriteAccessor;
    if(m_dataIndexes.find(rWriteAccessor, x))
    {
		//save values
		recordSize = rWriteAccessor->second->m_blockCount * BLOCK_SIZE;

        //delete from LRU
        {
            std::lock_guard<std::mutex> rLRU_Guard(m_LRULock);
            m_pLRUCache->remove(x);
        }
        
        //queue delete from disk
        m_pDiskWriter->QueueIndexDeletetion(rWriteAccessor);
        
        //release memory
        if(rWriteAccessor->second->m_pBlockManager)
        {
            std::lock_guard<std::mutex> rBM_Guard(m_rBlockManagerMemPoolLock);
            m_rBlockManagerMemPool.deallocate(rWriteAccessor->second->m_pBlockManager);
            rWriteAccessor->second->m_pBlockManager = NULL;
            //update memory counter
            m_memoryUsed -= (recordSize + sizeof(BlockManager));
        }
        
        //deallocate recordIndex struct
        {
            std::lock_guard<std::mutex> rRI_Guard(m_rRecordIndexMemPoolLock);
            m_rRecordIndexMemPool.deallocate(rWriteAccessor->second);
            rWriteAccessor->second = NULL;
        }
        
        //
        m_dataIndexes.erase(rWriteAccessor);

		//debug log
		Log.Debug(__FUNCTION__, "Deleted data x:" I64FMTD, x);
    }

	//erase from disk queue
	m_pDiskWriter->Remove(x);
}

void Storage::DeleteData(const HANDLE &rDataFileHandle, const uint64 &x, const uint64 &y)
{
    RecordIndexMap::accessor rWriteAccessor;
    if(m_dataIndexes.find(rWriteAccessor, x))
    {
        //update LRU
        {
            std::lock_guard<std::mutex> rGuard(m_LRULock);
            m_pLRUCache->put(x);
        }
        
        //check manager - load from disk if not in memory
        CheckBlockManager(rDataFileHandle, x, rWriteAccessor);
        
        //delete
        rWriteAccessor->second->m_pBlockManager->DeleteRecord(y);
        
        //queue write to disk
        m_pDiskWriter->Queue(rWriteAccessor);
        
		//debug log
		Log.Debug(__FUNCTION__, "Deleted data [x:" I64FMTD ", y:" I64FMTD "]", x, y);
    }
}

void Storage::GetAllX(uint8 **pX, uint64 *xSize)
{
    //CANNOT BE IMPLEMENTED
    //iterators are not permited
    
    *pX = NULL;
    *xSize = 0;
}

void Storage::GetAllY(const HANDLE &rDataFileHandle, const uint64 &x, ByteBuffer &rY)
{
    RecordIndexMap::accessor rWriteAccessor;
    if(m_dataIndexes.find(rWriteAccessor, x))
    {
        //update LRU
        {
            std::lock_guard<std::mutex> rGuard(m_LRULock);
            m_pLRUCache->put(x);
        }
        
        //check manager - load from disk if not in memory
        CheckBlockManager(rDataFileHandle, x, rWriteAccessor);
        
        //read data
        rWriteAccessor->second->m_pBlockManager->GetAllRecordKeys(rY);
    }
    
    Log.Debug(__FUNCTION__, "Read data [x:" I64FMTD ",y:" I64FMTD "] size: " I64FMTD, x, 0, rY.size());
}

void Storage::DefragmentData(const HANDLE &rDataFileHandle, const uint64 &x)
{
    RecordIndexMap::accessor rWriteAccessor;
    if(m_dataIndexes.find(rWriteAccessor, x))
    {
        //update LRU
        {
            std::lock_guard<std::mutex> rGuard(m_LRULock);
            m_pLRUCache->put(x);
        }
        
        //check manager - load from disk if not in memory
        CheckBlockManager(rDataFileHandle, x, rWriteAccessor);
     
        //defragment
        DefragmentDataInternal(rWriteAccessor);
    }
}

void Storage::DefragmentDataInternal(RecordIndexMap::accessor &rWriteAccessor)
{
    int64 memoryUsageBeforeDefrag;
    int64 memoryUsageAfterDefrag;
    
    //save memory usage
    memoryUsageBeforeDefrag = ((rWriteAccessor->second->m_pBlockManager->numOfBlocks() * BLOCK_SIZE) + sizeof(BlockManager));
    
    //set flag reallocate blocks so diskwrite must find new place fro data
    rWriteAccessor->second->m_flags |= eRIF_RelocateBlocks;
    
    //DEFRAGMENT
    {
        rWriteAccessor->second->m_pBlockManager->DefragmentData();
    }
    
    //calc new memory usage
    memoryUsageAfterDefrag = ((rWriteAccessor->second->m_pBlockManager->numOfBlocks() * BLOCK_SIZE) + sizeof(BlockManager));
    
    //set dirty
    rWriteAccessor->second->m_flags |= eRIF_Dirty;
    
    //update memory use
    if(memoryUsageBeforeDefrag > memoryUsageAfterDefrag)
        m_memoryUsed += (memoryUsageAfterDefrag - memoryUsageBeforeDefrag);
    else
        m_memoryUsed += (memoryUsageBeforeDefrag - memoryUsageAfterDefrag);
    
    //queue write to disk
    m_pDiskWriter->Queue(rWriteAccessor);
    
    //log
    Log.Debug(__FUNCTION__, "Defragmented data x:" I64FMTD, rWriteAccessor->first);
}

void Storage::GetFreeSpaceDump(ByteBuffer &rBuff, const uint32 &dumpFlags)
{
    //lock
    std::lock_guard<std::mutex> rGuard(m_rFreeSpaceLock);
    
    //prealloc buffer
    if(dumpFlags == eSFSDF_FULL)
    {
        size_t reserveSize = 0;
        for(FreeSpaceBlockMap::iterator itr = m_rFreeSpace.begin();itr != m_rFreeSpace.end();++itr)
        {
            reserveSize += sizeof(int64);
            reserveSize += itr->second.size() * sizeof(int64);
        }
        rBuff.reserve(reserveSize + sizeof(uint64));
    }
    
    //data file size
    rBuff << uint64(m_dataFileSize);
    //freeSpace size
    rBuff << uint64(m_rFreeSpace.size());
    //
    for(FreeSpaceBlockMap::iterator itr = m_rFreeSpace.begin();itr != m_rFreeSpace.end();++itr)
    {
        rBuff << uint64(itr->first);
        rBuff << uint64(itr->second.size());
        if(dumpFlags == eSFSDF_FULL)
        {
            for(FreeSpaceOffsets::iterator itr2 = itr->second.begin();itr2 != itr->second.end();++itr2)
            {
                rBuff << uint64(*itr2);
            }
        }
    }
}

void Storage::CheckMemory()
{
    uint64 xToDelete;
    RecordIndexMap::accessor rWriteAccessor;
    
    while(m_memoryUsed.load() > g_MemoryLimit)
    {
        {
            std::lock_guard<std::mutex> rGetGuard(m_LRULock);
            if(!m_pLRUCache->get(&xToDelete))
                break;
        }
        
        //get record index
        if(m_dataIndexes.find(rWriteAccessor, xToDelete))
        {
            //check if record does not wait in disk thread
			if(rWriteAccessor->second->m_flags & eRIF_InDiskWriteQueue)
                return;
            
            if(rWriteAccessor->second->m_pBlockManager)
            {
                //log
                Log.Debug(__FUNCTION__, "Memory usage: " I64FMTD ", memory limit: " I64FMTD ", removing x: " I64FMTD " from memory.", m_memoryUsed.load(), g_MemoryLimit, xToDelete);
                
                //update memory counter
                m_memoryUsed -= ((rWriteAccessor->second->m_pBlockManager->numOfBlocks() * BLOCK_SIZE) + sizeof(BlockManager));
                
                //clean memory
                std::lock_guard<std::mutex> rBM_Guard(m_rBlockManagerMemPoolLock);
                m_rBlockManagerMemPool.deallocate(rWriteAccessor->second->m_pBlockManager);
                rWriteAccessor->second->m_pBlockManager = NULL;
            }
            else
            {
                Log.Warning(__FUNCTION__, "rWriteAccessor->second->m_pBlockManager == NULL");
            }
        }
        
        //delete from LRU
        {
            std::lock_guard<std::mutex> rRemoveGuard(m_LRULock);
            if(m_pLRUCache->remove(xToDelete) == false)
            {
                Log.Warning(__FUNCTION__, "m_pLRUCache->remove(" I64FMTD ") == false", xToDelete);
            }
        }
    }
}

bool Storage::CheckBlockManager(const HANDLE &rDataFileHandle, const uint64 &x, RecordIndexMap::accessor &rWriteAccessor)
{
    if(rWriteAccessor->second->m_pBlockManager == NULL)
    {
        uint16 i;
        Blocks rBlocks;
        uint8 *pBlock;
        ByteBuffer rDiskBlocks;
        uint64 startTime;
        uint64 endTime;
        uint32 crc32;
        
        //this cannot happen, else it causes a crash
        if(rWriteAccessor->second->m_flags & eRIF_RelocateBlocks)
        {
            Log.Error(__FUNCTION__, "pRecordIndex->m_flags & eRIF_RelocateBlocks");
			return false;
        }
        
        //
        startTime = GetTickCount64();
        
		//preallocate
        rBlocks.resize(rWriteAccessor->second->m_blockCount);
        rDiskBlocks.resize(rWriteAccessor->second->m_blockCount * BLOCK_SIZE);
        
        //read all blocks in one IO
        IO::fseek(rDataFileHandle, rWriteAccessor->second->m_recordStart, IO::IO_SEEK_SET);
        IO::fread((void*)rDiskBlocks.contents(), rDiskBlocks.size(), rDataFileHandle);
        
        //copy to blocks allocated from memory pool
        {
            std::lock_guard<std::mutex> rBGuard(m_rBlockMemPoolLock);
            //split blocks
            for(i = 0;i < rWriteAccessor->second->m_blockCount;++i)
            {
                pBlock = (uint8*)m_rBlockMemPool.allocate();
                rDiskBlocks.read(pBlock, BLOCK_SIZE);
                rBlocks[i] = pBlock;
            }
        }
        
		//create new blockmanager
        {
            std::lock_guard<std::mutex> rBM_Guard(m_rBlockManagerMemPoolLock);
            rWriteAccessor->second->m_pBlockManager = m_rBlockManagerMemPool.allocate(*this, x, rBlocks);
        }
        
        //compute crc32
        crc32 = rWriteAccessor->second->m_pBlockManager->GetBlocksCrc32();
        //check crc32
        if(rWriteAccessor->second->m_crc32 != crc32)
        {
            Log.Error(__FUNCTION__, "CRC32 error - x: " I64FMTD " saved crc: %u loaded crc: %u", x, rWriteAccessor->second->m_crc32, crc32);
        }
        
        //inc memory use
        m_memoryUsed += ((rWriteAccessor->second->m_pBlockManager->numOfBlocks() * BLOCK_SIZE) + sizeof(BlockManager));
        
        //stats
        ++g_NumOfReadsFromDisk;
        
        //calc avg read time
        endTime = GetTickCount64() - startTime;
        m_sumDiskReadTime += endTime;
        g_AvgDiskReadTime = m_sumDiskReadTime.load() / g_NumOfReadsFromDisk.load();
        return true;
    }
    else
    {
        //stats
        ++g_NumOfReadsFromCache;
        return false;
    }
}

bool Storage::run()
{
    CommonFunctions::SetThreadName("Storage thread");
    
    try
    {
#ifdef WIN32
        _set_se_translator(trans_func);
#endif
        //set start time
        time_t checkTime = UNIXTIME + g_MemoryPoolsRecycleTimer;
        
        while(m_threadRunning)
        {                
            //flush data to disk
            if(!(++m_diskWriterCount % g_DiskFlushCoef))
            {
                //process disk operations
                m_pDiskWriter->Process();
            }
            
            //diskwriter queue recycle timer
            if(checkTime < UNIXTIME)
            {
                m_pDiskWriter->RecycleQueue();
                checkTime = UNIXTIME + g_MemoryPoolsRecycleTimer;
            }

            Wait(100);
        }
    }
    catch(...)
    {
        Log.Error(__FUNCTION__, "Fatal error stopping server.");
        g_pClientSocketWorker->SetException(true);
        g_stopEvent = true;
    }
    
    return false;
}

void Storage::AddFreeSpace(const int64 &pos, const int64 &lenght)
{
    if(pos < 0 || lenght <= 0)
    {
        Log.Warning(__FUNCTION__, "pos < 0 || lenght <= 0");
        return;
    }
    
    //lock
    std::lock_guard<std::mutex> rGuard(m_rFreeSpaceLock);
    //
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
    explicit PredGreater(const int64 &x) : m_x(x) {}
    INLINE bool operator()(const FreeSpaceBlockMap::value_type & p) { return (m_x < p.first); }
private:
    int64 m_x;
};

int64 Storage::GetFreeSpacePos(const int64 &lenght)
{
    int64 returnPos = -1;
    int64 newSize;
    int64 newPos;
    
    //lock
    std::lock_guard<std::mutex> rGuard(m_rFreeSpaceLock);
    //
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

void Storage::DefragmentFreeSpace()
{    
    //TODO: DefragmentFreeSpace
}

void Storage::GetStats(ByteBuffer &rBuff)
{
    //generate stats with decription
    StatGenerator rStatGenerator(*this);
    rStatGenerator.GenerateStats(rBuff, true);
}























