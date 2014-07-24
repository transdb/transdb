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

Storage *Storage::create(const std::string &rFileName)
{
    Storage *pStorage = new Storage(rFileName);
    if(pStorage->Init() == false)
    {
        delete pStorage;
        return NULL;
    }
    return pStorage;
}

Storage::Storage(const std::string &rFileName) : m_rDataPath(g_DataFilePath + rFileName + ".dat"),
                                                 m_rIndexPath(g_IndexFilePath + rFileName + ".idx"),
                                                 m_dataFileSize(0),
                                                 m_diskWriterCount(0),
                                                 m_pDiskWriter(new DiskWriter(*this)),
                                                 m_pDataIndexDiskWriter(new IndexBlock()),
                                                 m_memoryUsed(0),
                                                 m_sumDiskReadTime(0)
{
    
}

bool Storage::Init()
{
    //check if index file exits if not create it
    CommonFunctions::CheckFileExists(m_rIndexPath.c_str(), true);
    //check if data file exits if not create it
    CommonFunctions::CheckFileExists(m_rDataPath.c_str(), true);
    
    //open index file
    HANDLE hIndexFile = INVALID_HANDLE_VALUE;
    IOHandleGuard rIndexIOHandleGuard(hIndexFile);
    hIndexFile = IO::fopen(m_rIndexPath.c_str(), IO::IO_READ_ONLY, IO::IO_DIRECT);
    if(hIndexFile == INVALID_HANDLE_VALUE)
    {
        Log.Error(__FUNCTION__, "Cannot open index file: %s.", m_rIndexPath.c_str());
        return false;
    }
    
	//open data file
    HANDLE rDataFileHandle = INVALID_HANDLE_VALUE;
    IOHandleGuard rDataIOHandleGuard(rDataFileHandle);
    rDataFileHandle = IO::fopen(m_rDataPath.c_str(), IO::IO_RDWR, IO::IO_DIRECT);
    if(rDataFileHandle == INVALID_HANDLE_VALUE)
    {
        Log.Error(__FUNCTION__, "Cannot open data file: %s.", m_rDataPath.c_str());
        return false;
    }
    
	//resize if needed
    IO::fseek(rDataFileHandle, 0, IO::IO_SEEK_END);
	m_dataFileSize = IO::ftell(rDataFileHandle);
    IO::fseek(rDataFileHandle, 0, IO::IO_SEEK_SET);
    
    //prealloc data file
	if(m_dataFileSize == 0)
	{
        //this function will update m_dataFileSize
        m_pDiskWriter->ReallocDataFile(rDataFileHandle, g_ReallocSize, false);
	}
	Log.Notice(__FUNCTION__, "Data file: %s - loaded. Size: " SI64FMTD " bytes", m_rDataPath.c_str(), m_dataFileSize.load());
    
    //get index file size
    IO::fseek(hIndexFile, 0, IO::IO_SEEK_END);
	int64 indexFileSize = IO::ftell(hIndexFile);
    IO::fseek(hIndexFile, 0, IO::IO_SEEK_SET);
    
    //check index file alignment
    if((indexFileSize % INDEX_BLOCK_SIZE) != 0)
    {
        Log.Error(__FUNCTION__, "IndexFile corrupted: file alignment must be %u", INDEX_BLOCK_SIZE);
        return false;
    }
    
	//load indexes and freespaces
    Log.Notice(__FUNCTION__, "Loading index file: %s. Size: " SI64FMTD " bytes...", m_rIndexPath.c_str(), indexFileSize);
    //parse index file
    bool status = m_pDataIndexDiskWriter->Init(hIndexFile, &m_dataIndexes, m_pDiskWriter->m_rFreeSpace, m_dataFileSize);
    if(status == false)
    {
        //something failed -> IndexBlock::Init -> log error
        return false;
    }
    Log.Notice(__FUNCTION__, "Loading index file: %s. Size: " SI64FMTD " bytes... done", m_rIndexPath.c_str(), indexFileSize);
    
	//check all data
	if(g_StartupCrc32Check)
	{
        Crc32Check(rDataFileHandle);
	}
    
    return true;
}

Storage::~Storage()
{
    //do all pending disk writes + delete
    if(m_pDiskWriter->HasTasks())
    {
        Log.Notice(__FUNCTION__, "Processing pending disk writes.");
        //process
        m_pDiskWriter->Process();
        delete m_pDiskWriter;
        m_pDiskWriter = NULL;
        //
        Log.Notice(__FUNCTION__, "Finished processing pending disk writes.");
    }
    
    //delete index disk writer
    delete m_pDataIndexDiskWriter;
	m_pDataIndexDiskWriter = NULL;
}

void Storage::LoadFromDisk(const HANDLE &rDataFileHandle, LRUCache &rLRUCache, uint64 x)
{
    RecordIndexMap::accessor rWriteAccessor;
    if(m_dataIndexes.find(rWriteAccessor, x))
    {
        //update LRU
        rLRUCache.put(x);
        
        //check manager - load from disk if not in memory
        CheckBlockManager(rDataFileHandle, x, rWriteAccessor);
    }
}

typedef Vector<WriteInfo, uint64> WriteInfoVec;

struct StorageCrc32Check
{
    StorageCrc32Check(const RecordIndexMap &rRecordIndexMap,
                      const WriteInfoVec &rWriteInfoVec,
                      const std::string &rDataFilePath) : m_rRecordIndexMap(rRecordIndexMap), m_rWriteInfoVec(rWriteInfoVec), m_rDataFilePath(rDataFilePath)
    {
        
    }
    
    const RecordIndexMap  &m_rRecordIndexMap;
    const WriteInfoVec    &m_rWriteInfoVec;
    const std::string     &m_rDataFilePath;
    
    void operator()(const tbb::blocked_range<uint64>& range) const
    {
        HANDLE rDataFileHandle = INVALID_HANDLE_VALUE;
        IOHandleGuard rIOHandleGuard(rDataFileHandle);
        rDataFileHandle = IO::fopen(m_rDataFilePath.c_str(), IO::IO_READ_ONLY, IO::IO_DIRECT);
        if(rDataFileHandle == INVALID_HANDLE_VALUE)
        {
            Log.Error(__FUNCTION__, "Cannot open data file.");
            return;
        }
        
        RecordIndexMap::accessor rWriteAccessor;
        int64 blocksSize;
        void *pBlocks;
        uint32 crc32;
        uint8 *pBlock;
        size_t crc32ArraySize;
        uint32 *pCrc32Array;
        uint16 blockCount;
        
        //log progress
        Log.Debug("Crc32 check", "Starting range from: " I64FMTD " to: " I64FMTD, range.begin(), range.end());
        
        for(uint64 i = range.begin();i != range.end();++i)
        {
            const WriteInfo &rWriteInfo = m_rWriteInfoVec[i];
            //get from map
            if(m_rRecordIndexMap.find(rWriteAccessor, rWriteInfo.m_key))
            {
                //preallocate
                blockCount = rWriteAccessor->second.m_blockCount;
                blocksSize = blockCount * BLOCK_SIZE;
                pBlocks = scalable_aligned_malloc(blocksSize, 512);
                
                //read all blocks in one IO
                IO::fseek(rDataFileHandle, rWriteAccessor->second.m_recordStart, IO::IO_SEEK_SET);
                IO::fread(pBlocks, blocksSize, rDataFileHandle);
                
                //calc crc32
                crc32ArraySize = sizeof(uint32) * blockCount;
                pCrc32Array = (uint32*)alloca(crc32ArraySize);
                
                for(uint16 i = 0;i < blockCount;++i)
                {
                    pBlock = ((uint8*)pBlocks + (BLOCK_SIZE * i));
                    pCrc32Array[i] = g_CRC32->ComputeCRC32(pBlock, BLOCK_SIZE);
                }
                
                //
                crc32 = g_CRC32->ComputeCRC32((BYTE*)pCrc32Array, crc32ArraySize);
                //check crc32
                if(rWriteAccessor->second.m_crc32 != crc32)
                {
                    Log.Error("Crc32 check", "CRC32 error - x: " I64FMTD " saved crc: %u loaded crc: %u", rWriteAccessor->first, rWriteAccessor->second.m_crc32, crc32);
                }
                
                //release memory
                scalable_aligned_free(pBlocks);
            }
        }
        
        //log progress
        Log.Debug("Crc32 check", "Finished range from: " I64FMTD " to: " I64FMTD, range.begin(), range.end());
    }
};

void Storage::Crc32Check(const HANDLE &rDataFileHandle)
{
    Log.Notice(__FUNCTION__, "Checking integrity of data file. This can take some time.");
    
    size_t dataIndexesSize;
    WriteInfoVec rInfo;
    
    //
    dataIndexesSize = m_dataIndexes.size();
    
    //fill list
    rInfo.reserve(dataIndexesSize);
    for(RecordIndexMap::const_iterator itr = m_dataIndexes.begin();itr != m_dataIndexes.end();++itr)
    {
        rInfo.push_back(WriteInfo(itr->first, itr->second.m_recordStart));
    }
    
    //sort by record pos
    tbb::parallel_sort(rInfo.begin(), rInfo.end(), &SortWriteInfoForCRC32Check);
    
    //start paraller crc32
    tbb::parallel_for(tbb::blocked_range<uint64>(0, rInfo.size()), StorageCrc32Check(m_dataIndexes, rInfo, m_rDataPath));
    
    Log.Notice(__FUNCTION__, "Finished checking integrity of data file.");
}

void Storage::ReadData(HANDLE rDataFileHandle, LRUCache &rLRUCache, uint64 x, uint64 y, ByteBuffer &rData)
{
    RecordIndexMap::accessor rWriteAccessor;
    if(m_dataIndexes.find(rWriteAccessor, x))
    {
        //update LRU
        rLRUCache.put(x);
        
        //check manager - load from disk if not in memory
        CheckBlockManager(rDataFileHandle, x, rWriteAccessor);
        
        //read data
        rWriteAccessor->second.m_pBlockManager->ReadRecord(y, rData);
    }
    
    Log.Debug(__FUNCTION__, "Read data [x:" I64FMTD ",y:" I64FMTD "] size: " I64FMTD, x, y, rData.size());
}

void Storage::ReadData(HANDLE rDataFileHandle, LRUCache &rLRUCache, uint64 x, ByteBuffer &rData)
{
    RecordIndexMap::accessor rWriteAccessor;
    if(m_dataIndexes.find(rWriteAccessor, x))
    {
        //update LRU
        rLRUCache.put(x);
        
        //check manager - load from disk if not in memory
        CheckBlockManager(rDataFileHandle, x, rWriteAccessor);
        
        //read data
        rWriteAccessor->second.m_pBlockManager->ReadRecords(rData);
    }
    
    Log.Debug(__FUNCTION__, "Read data [x:" I64FMTD ",y:" I64FMTD "] size: " I64FMTD, x, 0, rData.size());
}

uint32 Storage::WriteData(HANDLE rDataFileHandle, LRUCache &rLRUCache, uint64 x, uint64 y, const uint8 *pRecord, uint16 recordSize)
{
    //ret value
    uint32 status;
    
    //write accesor
	RecordIndexMap::accessor rWriteAccesor;
    
    //update LRU
    rLRUCache.put(x);
    
    //try insert - returns false if record is existing
    if(m_dataIndexes.insert(rWriteAccesor, x))
    {
        //allocate blockmanager
        //create block manager + add new block for write + update num of blocks
        void *pBlockManagerMem = scalable_malloc(sizeof(BlockManager));
        rWriteAccesor->second.m_pBlockManager = new(pBlockManagerMem) BlockManager();
        rWriteAccesor->second.m_blockCount = rWriteAccesor->second.m_pBlockManager->numOfBlocks();
        
        //init index block
        rWriteAccesor->second.m_IB_blockNumber = 0;
        rWriteAccesor->second.m_IB_recordOffset = -1;
        //set flags to eRIF_RealocateBlocks
        rWriteAccesor->second.m_flags = eRIF_RelocateBlocks | eRIF_Dirty;
        //write data to block
        status = rWriteAccesor->second.m_pBlockManager->WriteRecord(y, pRecord, recordSize);
        //set record start to -1 (data are not written on disk)
        rWriteAccesor->second.m_recordStart = -1;
            
        //inc memory use
        m_memoryUsed += ((rWriteAccesor->second.m_pBlockManager->numOfBlocks() * BLOCK_SIZE) + sizeof(BlockManager));
               
        //queue write to disk
        m_pDiskWriter->Queue(rWriteAccesor);
    }
	else
	{
        //now we have locked write access, just update data
        //check manager - load from disk if not in memory
        CheckBlockManager(rDataFileHandle, x, rWriteAccesor);
        //
        status = rWriteAccesor->second.m_pBlockManager->WriteRecord(y, pRecord, recordSize);
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
            if(!(rWriteAccesor->second.m_flags & eRIF_RelocateBlocks))
            {
                //set flag reallocate blocks so diskwrite must find new place for data
                rWriteAccesor->second.m_flags |= eRIF_RelocateBlocks;
            }
            
            //set dirty
            rWriteAccesor->second.m_flags |= eRIF_Dirty;
            
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

void Storage::DeleteData(LRUCache &rLRUCache, uint64 x)
{
	uint64 recordSize;

    RecordIndexMap::accessor rWriteAccessor;
    if(m_dataIndexes.find(rWriteAccessor, x))
    {
		//save values
		recordSize = rWriteAccessor->second.m_blockCount * BLOCK_SIZE;

        //delete from LRU
        rLRUCache.remove(x);
        
        //queue delete from disk
        m_pDiskWriter->QueueIndexDeletetion(rWriteAccessor);
        
        //release memory
        if(rWriteAccessor->second.m_pBlockManager)
        {
            rWriteAccessor->second.m_pBlockManager->~BlockManager();
            scalable_free((void*)rWriteAccessor->second.m_pBlockManager);
            rWriteAccessor->second.m_pBlockManager = NULL;
            //update memory counter
            m_memoryUsed -= (recordSize + sizeof(BlockManager));
        }
        
        //delete from main map
        m_dataIndexes.erase(rWriteAccessor);

		//debug log
		Log.Debug(__FUNCTION__, "Deleted data x:" I64FMTD, x);
    }

	//erase from disk queue
	m_pDiskWriter->Remove(x);
}

void Storage::DeleteData(HANDLE rDataFileHandle, LRUCache &rLRUCache, uint64 x, uint64 y)
{
    RecordIndexMap::accessor rWriteAccessor;
    if(m_dataIndexes.find(rWriteAccessor, x))
    {
        //update LRU
        rLRUCache.put(x);
        
        //check manager - load from disk if not in memory
        CheckBlockManager(rDataFileHandle, x, rWriteAccessor);
        
        //delete
        rWriteAccessor->second.m_pBlockManager->DeleteRecord(y);
        
        //queue write to disk
        m_pDiskWriter->Queue(rWriteAccessor);
        
		//debug log
		Log.Debug(__FUNCTION__, "Deleted data [x:" I64FMTD ", y:" I64FMTD "]", x, y);
    }
}

struct FillXKeys
{
    uint8       *m_pData;
    XKeyVec     *m_pXKeys;
    
    void operator()(const tbb::blocked_range<uint32>& range) const
    {
        uint8 *pBlock;
        uint16 position;
        DREC *pDREC;
        
        //build index map
        for(uint32 i = range.begin();i != range.end();++i)
        {
            position = 0;
            pBlock = (m_pData + (INDEX_BLOCK_SIZE * i));
            
            //read record
            for(;;)
            {
                if(position > IMAX_RECORD_POS)
                    break;
                
                //get record
                pDREC = (DREC*)(pBlock + position);
                
                //has data
                if(!IndexBlock::IsEmptyDREC(pDREC))
                {
                    //add key
                    m_pXKeys->push_back(pDREC->m_key);
                }
                
                //update position
                position += sizeof(DREC);
            }
        }
    }
};

void Storage::GetAllX(ByteBuffer &rX)
{
    //open index file for rw
    HANDLE hIndexFile = INVALID_HANDLE_VALUE;
    IOHandleGuard rIOHandleIndexGuard(hIndexFile);
    hIndexFile = IO::fopen(m_rIndexPath.c_str(), IO::IO_READ_ONLY, IO::IO_DIRECT);
    if(hIndexFile == INVALID_HANDLE_VALUE)
    {
        Log.Error(__FUNCTION__, "Open indexfile: %s failed. Error number: %d", m_rIndexPath.c_str(), IO::ferror());
        return;
    }
    
    //get index file size and blockcount
    IO::fseek(hIndexFile, 0, IO::IO_SEEK_END);
    int64 fileSize = IO::ftell(hIndexFile);
    uint32 blockCount = static_cast<uint32>(fileSize / INDEX_BLOCK_SIZE);
    
    //alloc buffer
    void *pData = scalable_aligned_malloc(fileSize, 512);
    if(pData == NULL)
    {
        Log.Error(__FUNCTION__, "Cannot allocate memory for index block file.");
        return;
    }
    
    //read from disk in 1 IO
    IO::fseek(hIndexFile, 0, IO::IO_SEEK_SET);
    IO::fread(pData, fileSize, hIndexFile);
    
    //prealloc vector
    XKeyVec rXKeyVec;
    rXKeyVec.reserve(m_dataIndexes.size());
    
    //create struct
    FillXKeys rFillXKeys;
    rFillXKeys.m_pData = (uint8*)pData;
    rFillXKeys.m_pXKeys = &rXKeyVec;
    
    //iterate and fill container with X keys
    tbb::parallel_for(tbb::blocked_range<uint32>(0, blockCount), rFillXKeys);
 
    //defragment
    rXKeyVec.shrink_to_fit();
    
    //free
    scalable_aligned_free(pData);
    
    //reserve buffer size
    rX.reserve(rXKeyVec.size() * sizeof(uint64));
    //add data to buffer
    for(XKeyVec::iterator itr = rXKeyVec.begin();itr != rXKeyVec.end();++itr)
    {
        rX << uint64(*itr);
    }
}

void Storage::GetAllY(HANDLE rDataFileHandle, LRUCache &rLRUCache, uint64 x, ByteBuffer &rY)
{
    RecordIndexMap::accessor rWriteAccessor;
    if(m_dataIndexes.find(rWriteAccessor, x))
    {
        //update LRU
        rLRUCache.put(x);
        
        //check manager - load from disk if not in memory
        CheckBlockManager(rDataFileHandle, x, rWriteAccessor);
        
        //read data
        rWriteAccessor->second.m_pBlockManager->GetAllRecordKeys(rY);
        
        //debug log
        Log.Debug(__FUNCTION__, "Read data [x:" I64FMTD ",y:" I64FMTD "] size: " I64FMTD, x, 0, rY.size());
    }
}

void Storage::DefragmentData(HANDLE rDataFileHandle, LRUCache &rLRUCache, uint64 x)
{
    RecordIndexMap::accessor rWriteAccessor;
    if(m_dataIndexes.find(rWriteAccessor, x))
    {
        //update LRU
        rLRUCache.put(x);
        
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
    memoryUsageBeforeDefrag = ((rWriteAccessor->second.m_pBlockManager->numOfBlocks() * BLOCK_SIZE) + sizeof(BlockManager));
    
    //set flag reallocate blocks so diskwrite must find new place fro data
    rWriteAccessor->second.m_flags |= eRIF_RelocateBlocks;
    
    //DEFRAGMENT
    {
        rWriteAccessor->second.m_pBlockManager->DefragmentData();
    }
    
    //calc new memory usage
    memoryUsageAfterDefrag = ((rWriteAccessor->second.m_pBlockManager->numOfBlocks() * BLOCK_SIZE) + sizeof(BlockManager));
    
    //set dirty
    rWriteAccessor->second.m_flags |= eRIF_Dirty;
    
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

void Storage::GetFreeSpaceDump(uint64 socketID, uint32 token, uint32 flags, uint32 dumpFlags)
{
    m_pDiskWriter->QueueFreeSpaceDump(socketID, token, flags, dumpFlags);
}

void Storage::CheckMemory(LRUCache &rLRUCache)
{
    uint64 xToDelete;
    RecordIndexMap::accessor rWriteAccessor;
    
    while(m_memoryUsed.load() > g_MemoryLimit)
    {
        if(!rLRUCache.get(&xToDelete))
            break;
        
        //get record index
        if(m_dataIndexes.find(rWriteAccessor, xToDelete))
        {
            //check if record does not wait in disk thread
			if(rWriteAccessor->second.m_flags & eRIF_InDiskWriteQueue)
                return;
            
            //can be NULL
            if(rWriteAccessor->second.m_pBlockManager)
            {
                //log
                Log.Debug(__FUNCTION__, "Memory usage: " I64FMTD ", memory limit: " I64FMTD ", removing x: " I64FMTD " from memory.", m_memoryUsed.load(), g_MemoryLimit, xToDelete);
                
                //update memory counter
                m_memoryUsed -= ((rWriteAccessor->second.m_pBlockManager->numOfBlocks() * BLOCK_SIZE) + sizeof(BlockManager));
                
                //clean memory
                rWriteAccessor->second.m_pBlockManager->~BlockManager();
                scalable_free((void*)rWriteAccessor->second.m_pBlockManager);
                rWriteAccessor->second.m_pBlockManager = NULL;
            }
        }
        
        //delete from LRU
        if(rLRUCache.remove(xToDelete) == false)
        {
            Log.Warning(__FUNCTION__, "m_pLRUCache->remove(" I64FMTD ") == false", xToDelete);
        }
    }
}

bool Storage::CheckBlockManager(const HANDLE &rDataFileHandle, const uint64 &x, RecordIndexMap::accessor &rWriteAccessor)
{
    if(rWriteAccessor->second.m_pBlockManager == NULL)
    {
        void *pBlocks;
        int64 blocksSize;
        uint64 startTime;
        uint64 endTime;
        uint32 crc32;
        
        //this cannot happen, else it causes a crash
        if(rWriteAccessor->second.m_flags & eRIF_RelocateBlocks)
        {
            Log.Error(__FUNCTION__, "pRecordIndex->m_flags & eRIF_RelocateBlocks");
			return false;
        }
        
        //for stats
        startTime = GetTickCount64();
        
		//preallocate
        blocksSize = rWriteAccessor->second.m_blockCount * BLOCK_SIZE;
        pBlocks = scalable_aligned_malloc(blocksSize, 512);
        
        //read all blocks in one IO
        IO::fseek(rDataFileHandle, rWriteAccessor->second.m_recordStart, IO::IO_SEEK_SET);
        IO::fread(pBlocks, blocksSize, rDataFileHandle);
        
		//create new blockmanager
        void *pBlockManagerMem = scalable_malloc(sizeof(BlockManager));
        rWriteAccessor->second.m_pBlockManager = new(pBlockManagerMem) BlockManager((uint8*)pBlocks, rWriteAccessor->second.m_blockCount);
        
        //compute crc32
        crc32 = rWriteAccessor->second.m_pBlockManager->GetBlocksCrc32();
        //check crc32
        if(rWriteAccessor->second.m_crc32 != crc32)
        {
            Log.Error(__FUNCTION__, "CRC32 error - x: " I64FMTD " saved crc: %u loaded crc: %u", x, rWriteAccessor->second.m_crc32, crc32);
        }
        
        //inc memory use
        m_memoryUsed += ((rWriteAccessor->second.m_pBlockManager->numOfBlocks() * BLOCK_SIZE) + sizeof(BlockManager));
        
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
        
        while(m_threadRunning)
        {                
            //flush data to disk
            if(!(++m_diskWriterCount % g_DiskFlushCoef))
            {
                //process disk operations
                m_pDiskWriter->Process();
            }
            
            //process freespace dump task -> Send dump over socket
            {
                m_pDiskWriter->ProcessFreeSpaceDump();
            }
            
            if(!(m_diskWriterCount % g_FreeSpaceDefrag))
            {
                m_pDiskWriter->DefragmentFreeSpace();
            }

            Wait(100);
        }
    }
    catch(std::runtime_error &rEx)
    {
        Log.Error(__FUNCTION__, "Runtime error stopping server. Description: %s", rEx.what());
        g_pClientSocketWorker->SetException(true);
        g_stopEvent = true;
    }
    catch(...)
    {
        Log.Error(__FUNCTION__, "Fatal error stopping server.");
        g_pClientSocketWorker->SetException(true);
        g_stopEvent = true;
    }
    
    return false;
}

void Storage::GetStats(ByteBuffer &rBuff)
{
    //generate stats with decription
    StatGenerator rStatGenerator(*this);
    rStatGenerator.GenerateStats(rBuff, true);
}























