//
//  Storage.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/20/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

Storage *g_pStorage;

Storage::Storage(const char *pFileName)
{
	//create memory pools
    m_pFreeSpaceMemoryPool      = new FixedPool<FreeSpace>(g_FreeSpaceMemReserve);
    m_pBlockManagerMemoryPool   = new FixedPool<BlockManager>(g_BlockManagerMemReserve);
    m_pBlockMemoryPool          = new FixedPool<BlockSize_T>(g_StorageBlockMemReserve);
    m_pRecordIndexMemoryPool    = new FixedPool<RecordIndex>(g_RecordIndexMemReserve);

	m_memoryCheckerCount		= 0;
    m_diskWriterCount           = 0;
    m_memoryUsed                = 0;
    m_pLRUCache                 = new LRUCache();
    
	//+5 - extension + terminating byte
	size_t pathLen = strlen(g_sStoragePath.c_str())+strlen(pFileName)+5;
	static const char *pPathTemplate = "%s%s%s";
	//alocate data paths
	m_pDataPath = new char[pathLen];
	m_pIndexPath = new char[pathLen];
	//fill data paths
    sprintf(m_pDataPath, pPathTemplate, g_sStoragePath.c_str(), pFileName, ".dat");
    sprintf(m_pIndexPath, pPathTemplate, g_sStoragePath.c_str(), pFileName, ".idx");

	//open data file
    Common::CheckFileExists(m_pDataPath, true);
    m_pDataFile = fopen(m_pDataPath, "rb+");
    if(m_pDataFile == NULL)
    {
        Log.Error(__FUNCTION__, "Cannot open data file: %s.", m_pDataPath);
        assert(m_pDataFile);
    }
    
	//resize if needed
    IO::fseek(m_pDataFile, 0, SEEK_END);
	streamoff dataFileSize = IO::ftell(m_pDataFile);
    IO::fseek(m_pDataFile, 0, SEEK_SET);
	if(dataFileSize == 0)
	{
        //allocate buffer
        void *pBuff;
        pBuff = aligned_malloc(g_ReallocSize);
        memset(pBuff, 0, g_ReallocSize);
        //write to disk
        IO::fwrite(pBuff, g_ReallocSize, 1, m_pDataFile);
        IO::fflush(m_pDataFile);
		//set dataFileSize when is index loaded freespace will be calculated
        dataFileSize = g_ReallocSize;
        //release memory
        aligned_free(pBuff);
	}
	Log.Debug(__FUNCTION__, "Data file: %s - loaded.", m_pDataPath);
            
	//load indexes and freespaces
    //create writter
    m_pDataIndexDiskWriter = new IndexBlock();
    //load indexes to memory
    m_pDataIndexDiskWriter->Init(this, m_pIndexPath, const_cast<RecordIndexMap*>(&m_dataIndexes), dataFileSize);
	Log.Debug(__FUNCTION__, "Index file: %s - loaded.", m_pIndexPath);
    
    //create disk writer
    m_pDiskWriter = new DiskWriter(m_pDataPath, this);
    
	//check all data
	if(g_StartupCrc32Check)
	{
		Log.Debug(__FUNCTION__, "Checking integrity of data file. This can take some time.");
		uint8 *pRecords;
		uint32 recordsSize;
		//check crc32
		LockingPtr<RecordIndexMap> pDataIndexes(m_dataIndexes, m_dataIndexesLock);
		for(RecordIndexMap::iterator itr = pDataIndexes->begin();itr != pDataIndexes->end();++itr)
		{
			ReadData(itr->first, &pRecords, &recordsSize);
			aligned_free(pRecords);
			CheckMemory();
		}
		Log.Debug(__FUNCTION__, "Finished checking integrity of data file.");
	}
}

Storage::~Storage()
{
    //process tasks
    CallbackBase *pCallbackBase;
    LockingPtr<TaskQueue> pTaskQueue(m_taskQueue, NULL);
    while(pTaskQueue->size())
    {
        pCallbackBase = pTaskQueue->front();
        pTaskQueue->pop();
        pCallbackBase->execute();
        delete pCallbackBase;
    }
    
    //sync data file
    IO::fflush(m_pDataFile);
    IO::fclose(m_pDataFile);
    m_pDataFile = NULL;
    
    //do all pending disk writes + delete
    m_pDiskWriter->Process();
    delete m_pDiskWriter;
    
    //delete index disk writer
    m_pDataIndexDiskWriter->Sync();
    delete m_pDataIndexDiskWriter;
    
	//clear memory pools
    delete m_pFreeSpaceMemoryPool;
    delete m_pBlockManagerMemoryPool;
    delete m_pBlockMemoryPool;
    delete m_pRecordIndexMemoryPool;
    
    //delete LRU cache
    delete m_pLRUCache;
    
    //clean up
	delete [] m_pDataPath;
	delete [] m_pIndexPath;
}

void Storage::ReadData(uint64 x, uint64 y, uint8 **pRecord, uint16 *recordSize)
{
    *pRecord = NULL;
    *recordSize = 0;
    
    RecordIndex *pRecordIndex;
    LockingPtr<RecordIndexMap> pDataIndexes(m_dataIndexes, m_dataIndexesLock); 
    RecordIndexMap::iterator itr = pDataIndexes->find(x);
    if(itr != pDataIndexes->end())
    {
        //
        pRecordIndex = itr->second;
        //update LRU
        m_pLRUCache->put(x);
        //check manager - load from disk if not in memory
        CheckBlockManager(x, pRecordIndex);
        //read data
        pRecordIndex->m_pBlockManager->ReadRecord(y, pRecord, recordSize);
    }
    
    Log.Debug(__FUNCTION__, "Read data [x:"I64FMTD",y:"I64FMTD"] size: %u", x, y, *recordSize);
}

void Storage::ReadData(uint64 x, uint8 **pRecords, uint32 *recordsSize)
{
    *pRecords = NULL;
    *recordsSize = 0;
    
    RecordIndex *pRecordIndex;
    LockingPtr<RecordIndexMap> pDataIndexes(m_dataIndexes, m_dataIndexesLock);
    RecordIndexMap::iterator itr = pDataIndexes->find(x);
    if(itr != pDataIndexes->end())
    {
        //
        pRecordIndex = itr->second;
        //update LRU
        m_pLRUCache->put(x);
        //check manager - load from disk if not in memory
        CheckBlockManager(x, pRecordIndex);
        //read data
        pRecordIndex->m_pBlockManager->ReadRecords(pRecords, recordsSize);
    }
    
    Log.Debug(__FUNCTION__, "Read data [x:"I64FMTD",y:"I64FMTD"] size: %u", x, 0, *recordsSize);
}

uint32 Storage::WriteData(uint64 x, uint64 y, uint8 *pRecord, uint16 recordSize)
{
    uint32 status;
    RecordIndex *pRecordIndex;
    streamoff blockPos;
    uint16 blockOffSet;
    uint8 *pBlock;
    //Index - 0 write, 1 delete
    int32 writtenInBlocks[2];
    writtenInBlocks[0] = -1;
    writtenInBlocks[1] = -1;
    
    //
	m_dataIndexesLock.Acquire();
    LockingPtr<RecordIndexMap> pDataIndexes(m_dataIndexes, NULL);
    RecordIndexMap::iterator itr = pDataIndexes->find(x);
    if(itr != pDataIndexes->end())
    {
        pRecordIndex = itr->second;
        //check manager - load from disk if not in memory
        CheckBlockManager(x, pRecordIndex);
        //
        status = pRecordIndex->m_pBlockManager->WriteRecord(y, pRecord, recordSize, writtenInBlocks);
		if(status == eBMS_FailRecordTooBig)
		{
			Log.Warning(__FUNCTION__, "Record [x:"I64FMTD", y:"I64FMTD"] size: %u is too big, max record size is: %u.", x, y, recordSize, BLOCK_SIZE - CIDFOffset - sizeof(RDF));
		}
        else if(status & eBMS_OldRecord)
        {
            Log.Warning(__FUNCTION__, "Record [x:"I64FMTD", y:"I64FMTD"] size: %u attempt to save old y.", x, y, recordSize);
        }
        else if(status & eBMS_ReallocatedBlocks)
        {
            if(pRecordIndex->m_recordStart != -1)
            {
                AddFreeSpace(pRecordIndex->m_recordStart, pRecordIndex->m_blockCount*BLOCK_SIZE);
                pRecordIndex->m_recordStart = -1;
            }
            
            //update num of blocks
            pRecordIndex->m_blockCount = pRecordIndex->m_pBlockManager->numOfBlocks();

            //queue write to disk
            m_pDiskWriter->Queue(x, pRecordIndex);
            
            //inc memory use
            m_memoryUsed += BLOCK_SIZE;
        }
        else
        {
            if(pRecordIndex->m_recordStart != -1)
            {
                for(uint32 i = 0;i < 2;++i)
                {
					if(writtenInBlocks[i] == -1)
						continue;

                    //write to disk
                    blockOffSet = BLOCK_SIZE * writtenInBlocks[i];
                    blockPos = pRecordIndex->m_recordStart+blockOffSet;
                    pBlock = pRecordIndex->m_pBlockManager->DataPointer().at(writtenInBlocks[i]);
                    //
                    IO::fseek(m_pDataFile, blockPos, SEEK_SET);
                    IO::fwrite(pBlock, BLOCK_SIZE, 1, m_pDataFile);
                    
                    //avoid to write it twice to disk
                    if(writtenInBlocks[0] == writtenInBlocks[1])
                        break;
                }
                IO::fflush(m_pDataFile);
            }
            else
            {
                //queue write to disk
                m_pDiskWriter->Queue(x, pRecordIndex);
            }
        }
    }
    else
    {
        //create index
        pRecordIndex = m_pRecordIndexMemoryPool->alloc();
        //create block manager + add new block for write + update num of blocks
        pRecordIndex->m_pBlockManager = m_pBlockManagerMemoryPool->alloc();
        pRecordIndex->m_pBlockManager->Init(this);
        pRecordIndex->m_pBlockManager->ReallocBlocks();
        pRecordIndex->m_blockCount = pRecordIndex->m_pBlockManager->numOfBlocks();
        //init index block
        pRecordIndex->m_IB_blockNumber = 0;
        pRecordIndex->m_IB_recordOffset = -1;
        //write data to block
        status = pRecordIndex->m_pBlockManager->WriteRecord(y, pRecord, recordSize, writtenInBlocks);
        //set record start to -1 (data are not written on disk)
        pRecordIndex->m_recordStart = -1;
        //insert index into map
        pDataIndexes->insert(RecordIndexMap::value_type(x, pRecordIndex));

        //queue write to disk
        m_pDiskWriter->Queue(x, pRecordIndex);
        
        //inc memory use
        m_memoryUsed += ((pRecordIndex->m_pBlockManager->numOfBlocks()*BLOCK_SIZE)+sizeof(BlockManager));
    }
    
    //update LRU
    m_pLRUCache->put(x);
    
    Log.Debug(__FUNCTION__, "Written data [x:"I64FMTD", y:"I64FMTD"] size: %u to blocks: %d,%d with status: %u", x, y, recordSize, writtenInBlocks[0], writtenInBlocks[1], status);
    
    //hard limit - force flush data to disk and clear memory
    if(m_memoryUsed > g_HardMemoryLimit)
    {
		//we must release lock, else we will get deadlock
		m_dataIndexesLock.Release();
		//flush data to disk
        m_pDiskWriter->Process();
    }
	else
	{
		m_dataIndexesLock.Release();
	}

    return status;
}

void Storage::DeleteData(uint64 x)
{
	std::streamoff recordStart;
	std::streamsize recordSize;

    LockingPtr<RecordIndexMap> pDataIndexes(m_dataIndexes, m_dataIndexesLock);
    RecordIndexMap::iterator itr = pDataIndexes->find(x);
    if(itr != pDataIndexes->end())
    {
		//save values
		recordStart = itr->second->m_recordStart;
		recordSize = itr->second->m_pBlockManager->numOfBlocks() * BLOCK_SIZE;

        //delete from LRU
        m_pLRUCache->remove(x);
        //update memory counter
        m_memoryUsed -= (recordSize + sizeof(BlockManager));

        //if recordstart is not -1 we can add freespace
		//if recordstart in -1 - just delete and diskwriter will add freespace
        if(recordStart != -1)
        {
			//add free space
            AddFreeSpace(recordStart, recordSize);

			//erase from disk queue - async
			LockingPtr<TaskQueue> pTaskQueue(m_taskQueue, m_taskQeueuLock);
			CallbackBase *pCallBack = new CallbackP1<DiskWriter, uint64>(m_pDiskWriter, &DiskWriter::Remove, x);
			pTaskQueue->push(pCallBack);
        }

        //delete record index from disk
        m_pDataIndexDiskWriter->EraseRecord(itr, pDataIndexes);
        
        //release memory
        m_pBlockManagerMemoryPool->free(itr->second->m_pBlockManager);
        m_pRecordIndexMemoryPool->free(itr->second);
        //
        pDataIndexes->erase(itr);

		//debug log
		Log.Debug(__FUNCTION__, "Deleted data x:"I64FMTD, x);
    }
}

void Storage::DeleteData(uint64 x, uint64 y)
{
    RecordIndex *pRecordIndex;
    
    LockingPtr<RecordIndexMap> pDataIndexes(m_dataIndexes, m_dataIndexesLock); 
    RecordIndexMap::const_iterator itr = pDataIndexes->find(x);
    if(itr != pDataIndexes->end())
    {
        //
        pRecordIndex = itr->second;
        //update LRU
        m_pLRUCache->put(x);
        //check manager - load from disk if not in memory
        CheckBlockManager(x, pRecordIndex);
        //delete
        pRecordIndex->m_pBlockManager->DeleteRecord(y);
		//debug log
		Log.Debug(__FUNCTION__, "Deleted data [x:"I64FMTD", y:"I64FMTD"]", x, y);
    }
}

void Storage::GetAllX(uint8 **pX, uint64 *xSize)
{
    LockingPtr<RecordIndexMap> pDataIndexes(m_dataIndexes, m_dataIndexesLock);
    
    *pX = NULL;
    *xSize = pDataIndexes->size()*sizeof(uint64);

    if(*xSize)
    {
        uint64 position = 0;
        *pX = (uint8*)aligned_malloc(*xSize);
        for(RecordIndexMap::const_iterator itr = pDataIndexes->begin();itr != pDataIndexes->end();++itr)
        {
            memcpy(*pX+position, &itr->first, sizeof(itr->first));
            position += sizeof(uint64);
        }
    }
}

void Storage::GetAllY(uint64 x, uint8 **pY, uint64 *ySize)
{
    *pY = NULL;
    *ySize = 0;
    
    RecordIndex *pRecordIndex;
    LockingPtr<RecordIndexMap> pDataIndexes(m_dataIndexes, m_dataIndexesLock);
    RecordIndexMap::iterator itr = pDataIndexes->find(x);
    if(itr != pDataIndexes->end())
    {
        //
        pRecordIndex = itr->second;
        //update LRU
        m_pLRUCache->put(x);
        //check manager - load from disk if not in memory
        CheckBlockManager(x, pRecordIndex);
        //read data
        pRecordIndex->m_pBlockManager->GetAllRecordKeys(pY, ySize);
    }
    
    Log.Debug(__FUNCTION__, "Read data [x:"I64FMTD",y:"I64FMTD"] size: "I64FMTD, x, 0, *ySize);
}

void Storage::CheckMemory()
{
    LockingPtr<RecordIndexMap> pDataIndexes(m_dataIndexes, m_dataIndexesLock);
    
    RecordIndex *pRecordIndex;
    RecordIndexMap::iterator itr;
    uint64 xToDelete;
    while(m_memoryUsed > g_MemoryLimit)
    {
        if(!m_pLRUCache->get(&xToDelete))
            break;
        
        //get record index
        itr = pDataIndexes->find(xToDelete);
        if(itr != pDataIndexes->end())
        {
            pRecordIndex = itr->second;
            //wait until data are written to disk
            if(pRecordIndex->m_recordStart == -1)
                return;
            
            //log
            Log.Debug(__FUNCTION__, "Memory usage: "I64FMTD", memory limit: "I64FMTD", removing x: "I64FMTD" from memory.", m_memoryUsed, g_MemoryLimit, xToDelete);
            
            //update memory counter
            m_memoryUsed -= ((pRecordIndex->m_pBlockManager->numOfBlocks()*BLOCK_SIZE) + sizeof(BlockManager));
            
            //clean memory
            m_pBlockManagerMemoryPool->free(pRecordIndex->m_pBlockManager);
            pRecordIndex->m_pBlockManager = NULL;
        }
        //delete from LRU
        m_pLRUCache->remove(xToDelete);
    }
}

void Storage::CheckBlockManager(uint64 x, RecordIndex *pRecordIndex)
{
    uint16 i;
    uint8 *pBlock;
    size_t offset;
    Blocks rBlocks;
    
    if(pRecordIndex->m_pBlockManager == NULL)
    {
        if(pRecordIndex->m_recordStart == -1)
        {
            Log.Error(__FUNCTION__, "pRecordIndex->m_recordStart == -1");
			return;
        }

        //prealloc
        rBlocks.reserve(pRecordIndex->m_blockCount);
        
		//read data from disk - block by block
        for(i = 0;i < pRecordIndex->m_blockCount;++i)
        {
            pBlock = (uint8*)m_pBlockMemoryPool->alloc();
            offset = pRecordIndex->m_recordStart+(i*BLOCK_SIZE);
            IO::fseek(m_pDataFile, offset, SEEK_SET);
            IO::fread((void*)pBlock, BLOCK_SIZE, 1, m_pDataFile);
            rBlocks.push_back(pBlock);
        }
        
		//create new blockmanager
		pRecordIndex->m_pBlockManager = m_pBlockManagerMemoryPool->alloc();
        pRecordIndex->m_pBlockManager->Init(this);
		pRecordIndex->m_pBlockManager->Load(x, rBlocks);

        //inc memory use
        m_memoryUsed += ((pRecordIndex->m_pBlockManager->numOfBlocks()*BLOCK_SIZE)+sizeof(BlockManager));
    }
}

bool Storage::run()
{
    SetThreadName("Storage thread");
    
    CallbackBase *pCallbackBase;
    while(m_threadRunning)
    {        
        //process tasks
        LockingPtr<TaskQueue> pTaskQueue(m_taskQueue, NULL);
        m_taskQeueuLock.Acquire();
        while(pTaskQueue->size())
        {
            pCallbackBase = pTaskQueue->front();
            pTaskQueue->pop();
            pCallbackBase->execute();
            delete pCallbackBase;
        }
        m_taskQeueuLock.Release();
        
        //flush data to disk
        if(!(++m_diskWriterCount % g_DiskFlushCoef))
        {
            //process disk operations
            m_pDiskWriter->Process();
        }
        
		//check memory
		if(!(++m_memoryCheckerCount % g_MemoryFlushCoef))
		{
			CheckMemory();
		}

        Wait(100);        
    }
    
    return false;
}

void Storage::AddFreeSpace(std::streamoff pos, std::streamsize lenght)
{
    if(pos == -1)
    {
        Log.Error(__FUNCTION__, "Attempt to add pos == -1");
        return;
    }

    LockingPtr<FreeSpaceMultimap> pFreeSpace(m_freeSpace, m_freeSpaceLock);
    pFreeSpace->insert(FreeSpaceMultimap::value_type(lenght, pos));
}

std::streamoff Storage::GetFreeSpacePos(std::streamsize size, bool oCanDefrag)
{
    std::streamoff returnPos = -1;
    std::streamsize newSize;
    std::streamoff newPos;
    
    LockingPtr<FreeSpaceMultimap> pFreeSpace(m_freeSpace, m_freeSpaceLock);
    FreeSpaceMultimap::iterator itr = pFreeSpace->find(size);
    if(itr != pFreeSpace->end())
    {
        returnPos = itr->second;
        pFreeSpace->erase(itr);
        return returnPos;
    }
    else
    {
//        if(oCanDefrag)
//        {
//            FreeSpace *pFreeSpaceStruct;
//            FreeSpaceHolder rFreeSpaceList;
//            
//            //fill the list
//            for(itr = pFreeSpace->begin();itr != pFreeSpace->end();++itr)
//            {
//                pFreeSpaceStruct = m_pFreeSpaceMemoryPool->alloc();
//                pFreeSpaceStruct->m_pos = itr->second;
//                pFreeSpaceStruct->m_lenght = itr->first;
//                rFreeSpaceList.push_back(pFreeSpaceStruct);
//            }
//            
//            //defrag
//            DefragmentFreeSpace(rFreeSpaceList);
//            
//            //clear + fill multimap
//            pFreeSpace->clear();
//            //
//            for(FreeSpaceHolder::iterator itr = rFreeSpaceList.begin();itr != rFreeSpaceList.end();++itr)
//            {
//                pFreeSpaceStruct = (*itr);
//                pFreeSpace->insert(FreeSpaceMultimap::value_type(pFreeSpaceStruct->m_lenght, pFreeSpaceStruct->m_pos));
//                m_pFreeSpaceMemoryPool->free(pFreeSpaceStruct);
//            }
//        }
        
        //
        for(itr = pFreeSpace->begin();itr != pFreeSpace->end();++itr)
        {
            if(itr->first > size)
            {
                returnPos = itr->second;
                newSize = itr->first - size;
                newPos = itr->second + size;
                
                pFreeSpace->erase(itr);
                pFreeSpace->insert(FreeSpaceMultimap::value_type(newSize, newPos));
                return returnPos;
            }
        }
    }
    return returnPos;
}

static bool SortFreeSpace(FreeSpace *pFreeSpace1, FreeSpace *pFreeSpace2)
{
	return (pFreeSpace1->m_pos < pFreeSpace2->m_pos);
}

void Storage::DefragmentFreeSpace(FreeSpaceHolder &rFreeSpaceList)
{
    //
	rFreeSpaceList.sort(SortFreeSpace);
    //
    FreeSpaceHolder::iterator itr;
    FreeSpace *pFreeSpace;
    FreeSpace *pNextFreeSpace;
    
    
DefragmentFreeSpace_Restart:
    
    itr = rFreeSpaceList.begin();
	for(;itr != rFreeSpaceList.end();++itr)
	{
		pFreeSpace = (*itr);
        ++itr;
        
		if(itr != rFreeSpaceList.end())
		{
			pNextFreeSpace = (*itr);
			if((pNextFreeSpace->m_pos - pFreeSpace->m_lenght) == pFreeSpace->m_pos)
			{
				pFreeSpace->m_lenght += pNextFreeSpace->m_lenght;
                m_pFreeSpaceMemoryPool->free(*itr);
				rFreeSpaceList.erase(itr);
				goto DefragmentFreeSpace_Restart;
			}
		}
		else
			break;
	}
}

void Storage::GetStats(ByteBuffer &rBuff)
{
    LockingPtr<RecordIndexMap> pDataIndexes(m_dataIndexes, m_dataIndexesLock);
    LockingPtr<FreeSpaceMultimap> pFreeSpaceHolder(m_freeSpace, m_freeSpaceLock);
    
    rBuff << uint64(pDataIndexes->size());                                      //Index size
    rBuff << uint64(m_memoryUsed);                                              //Memory usage
    rBuff << uint64(m_pLRUCache->size());                                       //LRU cache count
    
	rBuff << uint64(m_pLRUCache->cacheSize());									//LRU mempool cache size
    rBuff << uint64(m_pBlockManagerMemoryPool->GetSize());                      //Block mamager mempool size
    rBuff << uint64(m_pBlockMemoryPool->GetSize());                             //Block mempool size for Storage
    rBuff << uint64(m_pDiskWriter->GetBlockMemoryPoolSize());                   //Block mempool size for DiskWriter
    rBuff << uint64(m_pRecordIndexMemoryPool->GetSize());                       //RecordIndex mempool size
    
    rBuff << uint64(g_rClientSocketHolder.GetAllSocketPacketQueueSize());       //add packet queue size from all sockets
    
    //send freespace
	rBuff << uint64(m_pFreeSpaceMemoryPool->GetSize());                         //freeSpace mempool size    
    rBuff << uint64(pFreeSpaceHolder->size());                                  //size of freespace
//    if(pFreeSpaceHolder->size())
//    {
//        FreeSpace *pFreeSpace = pFreeSpaceHolder->back();
//        rBuff.append(pFreeSpace, sizeof(FreeSpace));
//    }
}























