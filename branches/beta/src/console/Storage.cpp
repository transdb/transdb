//
//  Storage.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/20/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

Storage *g_pStorage;

INLINE static bool SortWriteInfoForCRC32Check(const WriteInfo &rWriteInfo1, const WriteInfo &rWriteInfo2)
{
    return rWriteInfo1.m_recordPosition < rWriteInfo2.m_recordPosition;
}

Storage::Storage(const char *pFileName)
{    
	//create memory pools
    m_pFreeSpaceMemoryPool      = new FixedPool<FreeSpace>(g_FreeSpaceMemReserve / sizeof(FreeSpace));
    m_pRecordIndexMemPool       = new FixedPool<RecordIndex>(g_RecordIndexMemReserve / sizeof(RecordIndex));
    m_pBlockMemPool             = new FixedPool<BlockSize_T>(g_StorageBlockMemReserve / BLOCK_SIZE);

    m_dataFileSize              = 0;
    m_diskWriterCount           = 0;
    m_memoryUsed                = 0;
    m_pLRUCache                 = new LRUCache();
    
	//+5 - extension + terminating byte
	size_t pathLen = strlen(g_sStoragePath.c_str())+strlen(pFileName)+5;
	static const char *pPathTemplate = "%s%s%s";
	//alocate data paths
	m_pDataPath = new char[pathLen];
	m_pIndexPath = new char[pathLen];
    m_pFreeSpacePath = new char[pathLen];
	//fill data paths
    sprintf(m_pDataPath, pPathTemplate, g_sStoragePath.c_str(), pFileName, ".dat");
    sprintf(m_pIndexPath, pPathTemplate, g_sStoragePath.c_str(), pFileName, ".idx");
    sprintf(m_pFreeSpacePath, pPathTemplate, g_sStoragePath.c_str(), pFileName, ".fsc");
    
    //check if data file exits if not create it
    Common::CheckFileExists(m_pDataPath, true);
    
	//open data file
    g_pStorage_FilePointer = fopen(m_pDataPath, "rb");
    if(g_pStorage_FilePointer == NULL)
    {
        Log.Error(__FUNCTION__, "Cannot open data file: %s.", m_pDataPath);
        assert(g_pStorage_FilePointer);
    }
    
    //create disk writer
    m_pDiskWriter = new DiskWriter(*this);
    
	//resize if needed
    IO::fseek(g_pStorage_FilePointer, 0, SEEK_END);
	m_dataFileSize = IO::ftell(g_pStorage_FilePointer);
    IO::fseek(g_pStorage_FilePointer, 0, SEEK_SET);
	if(m_dataFileSize == 0)
	{
        //this function will update m_dataFileSize
        m_pDiskWriter->ReallocDataFile(g_ReallocSize, false);
	}
	Log.Notice(__FUNCTION__, "Data file: %s - loaded. Size: "SI64FMTD, m_pDataPath, m_dataFileSize);
            
	//load indexes and freespaces
    //create writter
    m_pDataIndexDiskWriter = new IndexBlock();
    //load indexes to memory
    m_pDataIndexDiskWriter->Init(this, m_pIndexPath, m_dataIndexes, m_dataFileSize);
	Log.Notice(__FUNCTION__, "Index file: %s - loaded.", m_pIndexPath);
    
	//check all data
	if(g_StartupCrc32Check)
	{
		Log.Notice(__FUNCTION__, "Checking integrity of data file. This can take some time.");
		ByteBuffer rRecords;
        uint64 counter = 0;
        size_t dataIndexesSize;
        std::list<WriteInfo> rInfo;
        
        //
        dataIndexesSize = m_dataIndexes.size();

        //fill list
		for(RecordIndexMap::const_iterator itr = m_dataIndexes.begin();itr != m_dataIndexes.end();++itr)
        {
            rInfo.push_back(WriteInfo(itr->first, itr->second->m_recordStart));
        }
        
        //sort by record pos
        rInfo.sort(SortWriteInfoForCRC32Check);
        
		//check crc32
		for(std::list<WriteInfo>::iterator itr = rInfo.begin();itr != rInfo.end();++itr)
		{
            ++counter;
			ReadData(itr->m_key, rRecords);
            rRecords.clear();
            
//#ifdef CUSTOM_CHECKS
//            if(g_NumRecordsInBlock && itr->second->m_pBlockManager->numOfRecords() != g_NumRecordsInBlock)
//            {
//                Log.Error(__FUNCTION__, I64FMTD" - "I64FMTD, itr->first, itr->second->m_pBlockManager->numOfRecords());
//            }
//#endif
			CheckMemory();
            
            if(!(counter % 100000))
            {
                Log.Notice(__FUNCTION__, "Checked: "I64FMTD" from: "I64FMTD, counter, dataIndexesSize);
            }
		}
		Log.Notice(__FUNCTION__, "Finished checking integrity of data file.");
	}
    
    //close file handle
    fclose(g_pStorage_FilePointer);
    g_pStorage_FilePointer = NULL;
}

Storage::~Storage()
{
    //do all pending disk writes + delete
	Log.Notice(__FUNCTION__, "Processing pending disk writes.");
	{
		if(m_pDiskWriter->HasTasks())
		{
			//init file stream - we are in main thread
			g_pStorage_FilePointer = fopen(m_pDataPath, "rb+R");
			assert(g_pStorage_FilePointer);

			//process
			m_pDiskWriter->Process();
			delete m_pDiskWriter;
			m_pDiskWriter = NULL;

			//close
			IO::fclose(g_pStorage_FilePointer);
			g_pStorage_FilePointer = NULL;
		}
	}
	Log.Notice(__FUNCTION__, "Finished processing pending disk writes.");
 
    //save freespace
    Log.Notice(__FUNCTION__, "Saving FreeSpace.");
    {
        ProcessFreeSpaceQueue();
        DefragmentFreeSpace();
        SaveFreeSpace();
    }
    Log.Notice(__FUNCTION__, "FreeSpace saved.");
    
    //delete index disk writer
    m_pDataIndexDiskWriter->Sync();
    delete m_pDataIndexDiskWriter;
	m_pDataIndexDiskWriter = NULL;
    
	//clear memory pools
    delete m_pFreeSpaceMemoryPool;
	m_pFreeSpaceMemoryPool = NULL;
    delete m_pRecordIndexMemPool;
	m_pRecordIndexMemPool = NULL;
    delete m_pBlockMemPool;
	m_pBlockMemPool = NULL;
    
    //delete LRU cache
    delete m_pLRUCache;
	m_pLRUCache = NULL;

    //clean up
	delete [] m_pDataPath;
	m_pDataPath = NULL;
	delete [] m_pIndexPath;
	m_pIndexPath = NULL;
    delete [] m_pFreeSpacePath;
	m_pFreeSpacePath = NULL;
}

void Storage::ReadData(const uint64 &x, const uint64 &y, ByteBuffer &rData)
{
    RecordIndexMap::accessor rWriteAccessor;
    if(m_dataIndexes.find(rWriteAccessor, x))
    {
        //update LRU
        {
            std::lock_guard<tbb::mutex> rGuard(m_LRULock);
            m_pLRUCache->put(x);
        }
        
        //check manager - load from disk if not in memory
        CheckBlockManager(x, rWriteAccessor);
        
        //read data
        rWriteAccessor->second->m_pBlockManager->ReadRecord(y, rData);
    }
    
    Log.Debug(__FUNCTION__, "Read data [x:"I64FMTD",y:"I64FMTD"] size: "I64FMTD, x, y, rData.size());
}

void Storage::ReadData(const uint64 &x, ByteBuffer &rData)
{
    RecordIndexMap::accessor rWriteAccessor;
    if(m_dataIndexes.find(rWriteAccessor, x))
    {
        //update LRU
        {
            std::lock_guard<tbb::mutex> rGuard(m_LRULock);
            m_pLRUCache->put(x);
        }
        
        //check manager - load from disk if not in memory
        CheckBlockManager(x, rWriteAccessor);
        
        //read data
        rWriteAccessor->second->m_pBlockManager->ReadRecords(rData);
    }
    
    Log.Debug(__FUNCTION__, "Read data [x:"I64FMTD",y:"I64FMTD"] size: "I64FMTD, x, 0, rData.size());
}

uint32 Storage::WriteData(const uint64 &x, const uint64 &y, const uint8 *pRecord, const uint16 &recordSize)
{
    //ret value
    uint32 status;
    
    //Index - 0 write, 1 delete
    int32 writtenInBlocks[2];
    writtenInBlocks[0] = -1;
    writtenInBlocks[1] = -1;
    
    //write accesor
	RecordIndexMap::accessor rWriteAccesor;
    
    //update LRU
    {
        std::lock_guard<tbb::mutex> rGuard(m_LRULock);
        m_pLRUCache->put(x);
    }
    
    //try insert - returns false if record is existing
    if(m_dataIndexes.insert(rWriteAccesor, x))
    {
        //allocate recordIndex struct
        {
            std::lock_guard<tbb::mutex> rRI_Guard(m_rRecordIndexMemPoolLock);
            rWriteAccesor->second = m_pRecordIndexMemPool->alloc();
        }
        
        //create block manager + add new block for write + update num of blocks
        rWriteAccesor->second->m_pBlockManager = new BlockManager(*this);
        rWriteAccesor->second->m_blockCount = rWriteAccesor->second->m_pBlockManager->numOfBlocks();
        //init index block
        rWriteAccesor->second->m_IB_blockNumber = 0;
        rWriteAccesor->second->m_IB_recordOffset = -1;
        //set flags to eRIF_RealocateBlocks
        rWriteAccesor->second->m_flags = eRIF_RelocateBlocks | eRIF_Dirty;
        //write data to block
        status = rWriteAccesor->second->m_pBlockManager->WriteRecord(y, pRecord, recordSize, writtenInBlocks);
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
        CheckBlockManager(x, rWriteAccesor);
        //
        status = rWriteAccesor->second->m_pBlockManager->WriteRecord(y, pRecord, recordSize, writtenInBlocks);
        if(status == eBMS_FailRecordTooBig)
        {
            Log.Warning(__FUNCTION__, "Record [x:"I64FMTD", y:"I64FMTD"] size: %u is too big, max record size is: %u.", x, y, recordSize, MAX_RECORD_SIZE);
        }
        else if(status & eBMS_OldRecord)
        {
            Log.Warning(__FUNCTION__, "Record [x:"I64FMTD", y:"I64FMTD"] size: %u attempt to save old y.", x, y, recordSize);
        }
        else if(status & eBMS_ReallocatedBlocks)
        {
            if(!(rWriteAccesor->second->m_flags & eRIF_RelocateBlocks))
            {
                //disk thread can block freespace storage for a long time by GetFreeSpacePos
                QueueFreeSpace(rWriteAccesor->second->m_recordStart, rWriteAccesor->second->m_blockCount * BLOCK_SIZE);
                //set flag reallocate blocks so diskwrite must find new place fro data
                rWriteAccesor->second->m_flags |= eRIF_RelocateBlocks;
            }
            
            //update block count
            rWriteAccesor->second->m_blockCount = rWriteAccesor->second->m_pBlockManager->numOfBlocks();
            
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
    Log.Debug(__FUNCTION__, "Written data [x:"I64FMTD", y:"I64FMTD"] size: %u to blocks: %d,%d with status: %u", x, y, recordSize, writtenInBlocks[0], writtenInBlocks[1], status);

    return status;
}

void Storage::DeleteData(const uint64 &x)
{
	std::streamoff recordStart;
	std::streamsize recordSize;

    RecordIndexMap::accessor rWriteAccessor;
    if(m_dataIndexes.find(rWriteAccessor, x))
    {
		//save values
		recordStart = rWriteAccessor->second->m_recordStart;
		recordSize = rWriteAccessor->second->m_blockCount * BLOCK_SIZE;

        //delete from LRU
        {
            std::lock_guard<tbb::mutex> rLRU_Guard(m_LRULock);
            m_pLRUCache->remove(x);
        }
        
        //update memory counter
        m_memoryUsed -= recordSize;
        if(rWriteAccessor->second->m_pBlockManager)
        {
            m_memoryUsed -= sizeof(BlockManager);            
        }
        
        //add free space
        if(recordStart != -1)
        {
            QueueFreeSpace(recordStart, recordSize);
        }

        //delete record index from disk
        {
            std::lock_guard<tbb::mutex> rDI_Guard(m_rDataIndexDiskWriterLock);
            m_pDataIndexDiskWriter->EraseRecord(rWriteAccessor, m_dataIndexes);
        }
        
        //release memory
        if(rWriteAccessor->second->m_pBlockManager)
        {
            delete rWriteAccessor->second->m_pBlockManager;
            rWriteAccessor->second->m_pBlockManager = NULL;
        }
        
        //deallocate recordIndex struct
        {
            std::lock_guard<tbb::mutex> rRI_Guard(m_rRecordIndexMemPoolLock);
            m_pRecordIndexMemPool->free(rWriteAccessor->second);
            rWriteAccessor->second = NULL;
        }
        
        //
        m_dataIndexes.erase(rWriteAccessor);

		//debug log
		Log.Debug(__FUNCTION__, "Deleted data x:"I64FMTD, x);
    }

	//erase from disk queue
	m_pDiskWriter->Remove(x);
}

void Storage::DeleteData(const uint64 &x, const uint64 &y)
{
    RecordIndexMap::accessor rWriteAccessor;
    if(m_dataIndexes.find(rWriteAccessor, x))
    {
        //update LRU
        {
            std::lock_guard<tbb::mutex> rGuard(m_LRULock);
            m_pLRUCache->put(x);
        }
        
        //check manager - load from disk if not in memory
        CheckBlockManager(x, rWriteAccessor);
        
        //delete
        rWriteAccessor->second->m_pBlockManager->DeleteRecord(y);
        
		//debug log
		Log.Debug(__FUNCTION__, "Deleted data [x:"I64FMTD", y:"I64FMTD"]", x, y);
    }
}

void Storage::GetAllX(uint8 **pX, uint64 *xSize)
{
    //CANNOT BE IMPLEMENTED
    //iterators are not permited
    
    *pX = NULL;
    *xSize = 0;
}

void Storage::GetAllY(const uint64 &x, ByteBuffer &rY)
{
    RecordIndexMap::accessor rWriteAccessor;
    if(m_dataIndexes.find(rWriteAccessor, x))
    {
        //update LRU
        {
            std::lock_guard<tbb::mutex> rGuard(m_LRULock);
            m_pLRUCache->put(x);
        }
        
        //check manager - load from disk if not in memory
        CheckBlockManager(x, rWriteAccessor);
        
        //read data
        rWriteAccessor->second->m_pBlockManager->GetAllRecordKeys(rY);
    }
    
    Log.Debug(__FUNCTION__, "Read data [x:"I64FMTD",y:"I64FMTD"] size: "I64FMTD, x, 0, rY.size());
}

void Storage::CheckMemory()
{
    uint64 xToDelete;
    RecordIndexMap::accessor rWriteAccessor;
    
    while(m_memoryUsed.load() > g_MemoryLimit)
    {
        {
            std::lock_guard<tbb::mutex> rGetGuard(m_LRULock);
            if(!m_pLRUCache->get(&xToDelete))
                break;
        }
        
        //get record index
        if(m_dataIndexes.find(rWriteAccessor, xToDelete))
        {
            //check if record does not wait in disk thread
			if(rWriteAccessor->second->m_flags & eRIF_InDiskWriteQueue)
                return;
            
            //log
            Log.Debug(__FUNCTION__, "Memory usage: "I64FMTD", memory limit: "I64FMTD", removing x: "I64FMTD" from memory.", m_memoryUsed.load(), g_MemoryLimit, xToDelete);
            
            //update memory counter
            m_memoryUsed -= ((rWriteAccessor->second->m_pBlockManager->numOfBlocks() * BLOCK_SIZE) + sizeof(BlockManager));
            
            //clean memory
            delete rWriteAccessor->second->m_pBlockManager;
            rWriteAccessor->second->m_pBlockManager = NULL;
        }
        
        //delete from LRU
        {
            std::lock_guard<tbb::mutex> rRemoveGuard(m_LRULock);
            m_pLRUCache->remove(xToDelete);
        }
    }
}

bool Storage::CheckBlockManager(const uint64 &x, RecordIndexMap::accessor &rWriteAccessor)
{
    size_t diskOffset;
    size_t i;
    Blocks rBlocks;
    uint8 *pBlock;  
    
    if(rWriteAccessor->second->m_pBlockManager == NULL)
    {
        if(rWriteAccessor->second->m_flags & eRIF_RelocateBlocks)
        {
            Log.Error(__FUNCTION__, "pRecordIndex->m_flags & eRIF_RelocateBlocks");
			return false;
        }
        
		//preallocate
        rBlocks.resize(rWriteAccessor->second->m_blockCount);
        
        //fill blocks
        for(i = 0;i < rWriteAccessor->second->m_blockCount;++i)
        {
            //alloc new block
            {
                std::lock_guard<tbb::mutex> rBGuard(m_rBlockMemPoolLock);
                pBlock = (uint8*)m_pBlockMemPool->alloc();
            }
            
            //calc disk offset
            diskOffset = rWriteAccessor->second->m_recordStart + (i * BLOCK_SIZE);
            //read data from disk
            IO::fseek(g_pStorage_FilePointer, diskOffset, SEEK_SET);
            IO::fread(pBlock, BLOCK_SIZE, 1, g_pStorage_FilePointer);
            //assign
            rBlocks[i] = pBlock;
        }
        
		//create new blockmanager
		rWriteAccessor->second->m_pBlockManager = new BlockManager(*this, x, rBlocks);
        
        //inc memory use
        m_memoryUsed += ((rWriteAccessor->second->m_pBlockManager->numOfBlocks() * BLOCK_SIZE) + sizeof(BlockManager));
        
        //stats
        Sync_Add(&g_NumOfReadsFromDisk);
        return true;
    }
    else
    {
        //stats
        Sync_Add(&g_NumOfReadsFromCache);
        return false;
    }
}

bool Storage::run()
{
    SetThreadName("Storage thread");
    
    //init writer
    g_pStorage_FilePointer = fopen(m_pDataPath, "rb+");
    assert(g_pStorage_FilePointer);
    
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

            Wait(100);        
        }
    }
    catch(...)
    {
        Log.Error(__FUNCTION__, "Fatal error stopping server.");
        Sync_Add(&g_stopEvent);
    }
    
    IO::fclose(g_pStorage_FilePointer);
    g_pStorage_FilePointer = NULL;
    
    return false;
}

void Storage::AddFreeSpace(const std::streamoff &pos, const std::streamsize &lenght)
{
    std::lock_guard<tbb::mutex> rGuard(m_freeSpaceLock);
    m_freeSpace.insert(FreeSpaceMultimap::value_type(lenght, pos));
}

void Storage::QueueFreeSpace(const std::streamoff &pos, const std::streamsize &lenght)
{
    std::lock_guard<tbb::spin_mutex> rGuard(m_freeSpaceQueueLock);
    m_freeSpaceQueue.insert(FreeSpaceMultimap::value_type(lenght, pos));
}

void Storage::ProcessFreeSpaceQueue()
{
    std::lock_guard<tbb::mutex> rGuard(m_freeSpaceLock);
    std::lock_guard<tbb::spin_mutex> rQGuard(m_freeSpaceQueueLock);
    
    //copy
    m_freeSpace.insert(m_freeSpaceQueue.begin(), m_freeSpaceQueue.end());
    m_freeSpaceQueue.clear();
}

std::streamoff Storage::GetFreeSpacePos(const std::streamsize &size)
{
    std::streamoff returnPos = -1;
    std::streamsize newSize;
    std::streamoff newPos;
    
    //lock
    std::lock_guard<tbb::mutex> rGuard(m_freeSpaceLock);
    //
    FreeSpaceMultimap::iterator itr = m_freeSpace.find(size);
    if(itr != m_freeSpace.end())
    {
        returnPos = itr->second;
        m_freeSpace.erase(itr);
    }
    else
    {       
        //iterate over freespace and try to find suitable freespace
        for(itr = m_freeSpace.begin();itr != m_freeSpace.end();++itr)
        {
            if(itr->first > size)
            {
                returnPos = itr->second;
                newSize = itr->first - size;
                newPos = itr->second + size;
                
                m_freeSpace.erase(itr);
                m_freeSpace.insert(FreeSpaceMultimap::value_type(newSize, newPos));
                break;
            }
        }
    }
    
    return returnPos;
}

INLINE static bool SortFreeSpace(FreeSpace *pFreeSpace1, FreeSpace *pFreeSpace2)
{
	return (pFreeSpace1->m_pos < pFreeSpace2->m_pos);
}

void Storage::DefragmentFreeSpace()
{    
    FreeSpaceHolder::iterator Ditr, Ditr2;
    FreeSpaceMultimap::iterator itr;
    FreeSpace *pFreeSpaceStruct;
    FreeSpace *pNextFreeSpaceStruct;
    FreeSpaceHolder rFreeSpaceList;
    size_t nextPosition;
    
    //lock
    std::lock_guard<tbb::mutex> rGuard(m_freeSpaceLock);
    
    //fill the list
    for(itr = m_freeSpace.begin();itr != m_freeSpace.end();++itr)
    {
        pFreeSpaceStruct = m_pFreeSpaceMemoryPool->alloc();
        pFreeSpaceStruct->m_pos = itr->second;
        pFreeSpaceStruct->m_lenght = itr->first;
        rFreeSpaceList.push_back(pFreeSpaceStruct);
    }
    
    //need more than one element in list
    if(rFreeSpaceList.size() < 2)
        return;
    
    //sort by position
    rFreeSpaceList.sort(SortFreeSpace);
    
    //defragment
    Ditr = rFreeSpaceList.begin();
    pFreeSpaceStruct = (*Ditr);
	++Ditr;
    for(;;)
    {
        //increment itr + get next
        if(Ditr == rFreeSpaceList.end())
            break;
        
		Ditr2 = Ditr++;
        pNextFreeSpaceStruct = (*Ditr2);
        
        //calc next position
        nextPosition = pFreeSpaceStruct->m_pos + pFreeSpaceStruct->m_lenght;
        if(nextPosition == pNextFreeSpaceStruct->m_pos)
        {
            pFreeSpaceStruct->m_lenght += pNextFreeSpaceStruct->m_lenght;
            m_pFreeSpaceMemoryPool->free(pNextFreeSpaceStruct);
            rFreeSpaceList.erase(Ditr2);
        }
        else
        {
            pFreeSpaceStruct = pNextFreeSpaceStruct;
        }
    }
    
    //clear + add back to map
    m_freeSpace.clear();
    for(Ditr = rFreeSpaceList.begin();Ditr != rFreeSpaceList.end();++Ditr)
    {
        pFreeSpaceStruct = (*Ditr);
        m_freeSpace.insert(FreeSpaceMultimap::value_type(pFreeSpaceStruct->m_lenght, pFreeSpaceStruct->m_pos));
        m_pFreeSpaceMemoryPool->free(pFreeSpaceStruct);
    }
}

void Storage::SaveFreeSpace()
{
    FILE *pFile;
    ByteBuffer rBuff;
    ByteBuffer rOut;
    int32 status = -1;
    
    //lock
    std::lock_guard<tbb::mutex> rGuard(m_freeSpaceLock);
    
    if(m_freeSpace.size())
    {
        for(FreeSpaceMultimap::const_iterator itr = m_freeSpace.begin();itr != m_freeSpace.end();++itr)
        {
            rBuff << uint64(itr->first);
            rBuff << uint64(itr->second);
        }
        
        //compress
        if(rBuff.size() > g_DataSizeForCompression)
        {
            status = Common::compressGzip(rBuff.contents(), rBuff.size(), rOut);
            if(status == Z_OK)
            {
                rBuff.clear();
                rBuff.append(rOut);
            }
        }
        
        pFile = fopen(m_pFreeSpacePath, "wb");
        if(pFile)
        {
            IO::fwrite(&status, sizeof(status), 1, pFile);
            IO::fwrite(rBuff.contents(), rBuff.size(), 1, pFile);
            IO::fclose(pFile);
        }
    }
}

bool Storage::LoadFreeSpace()
{
    FILE *pFile;
    ByteBuffer rBuff;
    ByteBuffer rOut;
    int32 status;
    size_t fileSize;
    uint64 blockSize;
    uint64 position;
    
    
    pFile = fopen(m_pFreeSpacePath, "rb");
    if(pFile)
    {
        //load file size
        IO::fseek(pFile, 0, SEEK_END);
        fileSize = IO::ftell(pFile);
        IO::fseek(pFile, 0, SEEK_SET);
        
        //read status
        IO::fread(&status, sizeof(status), 1, pFile);
        //prepare buff
        rBuff.resize(fileSize - sizeof(status));
        IO::fread((void*)rBuff.contents(), rBuff.size(), 1, pFile);
        //decompress
        if(status != -1)
        {
            status = Common::decompressGzip(rBuff.contents(), rBuff.size(), rOut);
            if(status != Z_OK)
            {
                IO::fclose(pFile);
                return false;
            }
            rBuff.clear();
            rBuff.append(rOut);
        }
        IO::fclose(pFile);
        
        //lock
        std::lock_guard<tbb::mutex> rGuard(m_freeSpaceLock);        
        
        //fill map
        while(rBuff.rpos() != rBuff.size())
        {
            rBuff >> blockSize;
            rBuff >> position;
            m_freeSpace.insert(FreeSpaceMultimap::value_type(blockSize, position));
        }
        
        //delete file - in case of server crash we can load freespace from dataindex file and we do not load old freespaces
        remove(m_pFreeSpacePath);
        return true;
    }
    return false;
}

void Storage::GetStats(ByteBuffer &rBuff)
{
    std::lock_guard<tbb::mutex> rGuard(m_freeSpaceLock);      
    std::lock_guard<tbb::spin_mutex> rQGuard(m_freeSpaceQueueLock);
    std::lock_guard<tbb::mutex> rLRUGuard(m_LRULock);
    
    rBuff << uint64(m_dataIndexes.size());                                      //Index size
    rBuff << uint64(m_memoryUsed.load());                                       //Memory usage
    rBuff << uint64(m_pLRUCache->size());                                       //LRU cache count
    
	rBuff << uint64(m_pLRUCache->cacheSize());									//LRU mempool cache size
    rBuff << uint64(g_NumOfReadsFromDisk);                                      //Number of reads from disk
    rBuff << uint64(g_NumOfReadsFromCache);                                     //Number of reads from cache
    rBuff << uint64(g_NumOfWritesToDisk);                                       //Number of disk writes
    rBuff << uint64(0);                                                         //
    rBuff << uint64(g_rClientSocketHolder.GetAllSocketPacketQueueSize());       //add packet queue size from all sockets
    rBuff << uint64(g_pClientSocketWorker->GetQueueSize());						//Client socket worker tasks

    //send freespace
	rBuff << uint64(m_pFreeSpaceMemoryPool->GetSize());                         //freeSpace mempool size
    rBuff << uint64(m_freeSpace.size());                                        //size of freespace
    rBuff << uint64(m_freeSpaceQueue.size());                                   //size of freespace queue
    rBuff << uint64(m_dataFileSize);                                            //size of data file
}























