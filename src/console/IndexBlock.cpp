//
//  IndexBlock.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 9/3/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

IndexBlock::IndexBlock() : m_blockCount(0)
{
    m_pLRUCache             = new LRUCache("IndexBlock", g_IndexBlockCacheSize / INDEX_BLOCK_SIZE);
}

IndexBlock::~IndexBlock()
{
    delete m_pLRUCache;
}

struct IndexDef
{
	explicit IndexDef(const uint64 &key, const int64 &dataPosition, const int64 &dataLen) : m_key(key), m_dataPosition(dataPosition), m_dataLen(dataLen)
	{
        
	}
    
    uint64  m_key;
    int64   m_dataPosition;
    int64   m_dataLen;
};

INLINE static bool SortFreeSpace(const IndexDef &rIndexDefStruct1, const IndexDef &rIndexDefStruc2)
{
	return (rIndexDefStruct1.m_dataPosition < rIndexDefStruc2.m_dataPosition);
}

typedef tbb::concurrent_unordered_set<uint32>   Init_FreeBlocksSet;
typedef tbb::concurrent_vector<IndexDef>        Init_IndexDefVec;

struct FillIndex
{
    uint8                   *m_pData;
    Init_FreeBlocksSet      *m_pFreeBlocksSet;
    Init_IndexDefVec        *m_pIndexDef;
    RecordIndexMap          *m_pRecordIndexMap;
    
    void operator()(const tbb::blocked_range<uint32>& range) const
    {
        uint8 *pBlock;
        ICIDF *pICIDF;
        uint16 position;
        DREC *pDREC;
        RecordIndex *pIREC;
        void *pIRECMemory;
        
        //build index map
        for(uint32 i = range.begin();i != range.end();++i)
        {
            position = 0;
            pBlock = (m_pData + (INDEX_BLOCK_SIZE * i));
            pICIDF = GetICIDF(pBlock);
            
            //read record
            for(;;)
            {
                if(position > IMAX_RECORD_POS)
                    break;
                
                //get record
                pDREC = (DREC*)(pBlock + position);
                
                //has data
                if(!IsEmptyDREC(pDREC))
                {
                    //create storage index
                    pIRECMemory                 = malloc(sizeof(RecordIndex));
                    pIREC                       = new(pIRECMemory) RecordIndex();
                    pIREC->m_recordStart        = pDREC->m_recordStart;
                    pIREC->m_blockCount         = pDREC->m_blockCount;
                    pIREC->m_crc32              = pDREC->m_crc32;
                    pIREC->m_pBlockManager      = NULL;
                    //flags
                    pIREC->m_flags              = eRIF_None;
                    //set internal index
                    pIREC->m_IB_blockNumber     = i;
                    pIREC->m_IB_recordOffset    = position;
                    m_pRecordIndexMap->insert(RecordIndexMap::value_type(pDREC->m_key, pIREC));
                    
                    //create tmp freespace for calculating freespace list
                    m_pIndexDef->push_back(IndexDef(pDREC->m_key, pDREC->m_recordStart, pDREC->m_blockCount * BLOCK_SIZE));
                }
                
                //update position
                position += sizeof(DREC);
            }
            
            //add block with free space
            if(pICIDF->m_amoutOfFreeSpace >= sizeof(DREC))
            {
                m_pFreeBlocksSet->insert(i);
            }
        }
    }
};

void IndexBlock::Init(Storage *pStorage,
					  const std::string &rIndexFilePath,
				      RecordIndexMap &rRecordIndexMap,
					  int64 dataFileSize,
                      int64 *indexFileSize)
{
	HANDLE hIndexFile;
	uint8 *pData;
    int64 dataFileSizeTmp;
    int64 freeSpaceLen;
    uint64 counter;
    size_t freeSpaceStart;
    
    //for concurrent processing
    std::unique_ptr<Init_IndexDefVec> pIndexDef = std::unique_ptr<Init_IndexDefVec>(new Init_IndexDefVec());
    std::unique_ptr<Init_FreeBlocksSet> pFreeBlocksSet = std::unique_ptr<Init_FreeBlocksSet>(new Init_FreeBlocksSet());
    //prealloc
    pIndexDef->reserve(4*1024*1024);
    
	//save dataFileSize for later use
	dataFileSizeTmp = dataFileSize;
    
    //check file exists
    CommonFunctions::CheckFileExists(rIndexFilePath.c_str(), true);
    
    //open index file
    hIndexFile = IO::fopen(rIndexFilePath.c_str(), IO::IO_RDWR);
    assert(hIndexFile != INVALID_HANDLE_VALUE);
    
	//get index file size
    IO::fseek(hIndexFile, 0, IO::IO_SEEK_END);
    size_t fileSize = IO::ftell(hIndexFile);
    *indexFileSize = fileSize;
    
    if(fileSize != 0)
    {
        pData = (uint8*)malloc(fileSize);
		m_blockCount = static_cast<uint32>(fileSize / INDEX_BLOCK_SIZE);
        
        //read
        IO::fseek(hIndexFile, 0, IO::IO_SEEK_SET);
        IO::fread(pData, fileSize, hIndexFile);
        
        //create struct - save pointers to data and containers
        FillIndex rFillIndex;
        rFillIndex.m_pData = pData;
        rFillIndex.m_pFreeBlocksSet = pFreeBlocksSet.get();
        rFillIndex.m_pIndexDef = pIndexDef.get();
        rFillIndex.m_pRecordIndexMap = &rRecordIndexMap;
        
        //iterate and fill containers
        tbb::parallel_for(tbb::blocked_range<uint32>(0, m_blockCount), rFillIndex);
        
        //add data to m_freeBlocks
        for(Init_FreeBlocksSet::iterator itr = pFreeBlocksSet->begin();itr != pFreeBlocksSet->end();++itr)
        {
            m_freeBlocks.insert(*itr);
        }
        
        //release memory
        free(pData);
	}
    
	//close file handle
	IO::fclose(hIndexFile);
    
    //load free space from index
    if(pIndexDef->size())
    {
        //sort
        tbb::parallel_sort(pIndexDef->begin(), pIndexDef->end(), &SortFreeSpace);
        
        //init variables
        counter = 0;
        freeSpaceStart = 0;
        
        //
        for(Init_IndexDefVec::iterator itr = pIndexDef->begin();itr != pIndexDef->end();++itr)
        {
            //
            freeSpaceLen = itr->m_dataPosition - freeSpaceStart;
            if(freeSpaceLen > 0)
            {
                pStorage->AddFreeSpace(freeSpaceStart, freeSpaceLen);
            }
            else if(freeSpaceLen < 0)
            {
                Log.Error(__FUNCTION__, "Data curruption - " I64FMTD " - X: " I64FMTD " freeSpaceLen is negative: " SI64FMTD, counter, itr->m_key, freeSpaceLen);
            }
            
            freeSpaceStart += (itr->m_dataLen + freeSpaceLen);
            dataFileSizeTmp -= (itr->m_dataLen + freeSpaceLen);
            ++counter;
        }
    }
    
    //add last piece of freespace
    if(dataFileSizeTmp != 0)
    {
        pStorage->AddFreeSpace(dataFileSize - dataFileSizeTmp, dataFileSizeTmp);
    }
}

void IndexBlock::WriteRecordIndexToDisk(const HANDLE &hFile, RecordIndexMap::accessor &rWriteAccesor)
{
    ICIDF *pICIDF;
    uint32 freeBlock;
    size_t diskPosition;
    uint8 *pDiskBlock;
    DREC *pDREC;
    int16 recordOffset;
    
    //fill disk record
    DREC rDiskRecord;
    rDiskRecord.m_key           = rWriteAccesor->first;
    rDiskRecord.m_recordStart   = rWriteAccesor->second->m_recordStart;
    rDiskRecord.m_crc32         = rWriteAccesor->second->m_crc32;
    rDiskRecord.m_blockCount    = rWriteAccesor->second->m_blockCount;
    
    //x is existing rewrite
    if(rWriteAccesor->second->m_IB_recordOffset != -1)
    {
        //calc block position on disk
        diskPosition = (INDEX_BLOCK_SIZE * rWriteAccesor->second->m_IB_blockNumber);
        
        //get disk block
        pDiskBlock = GetCachedDiskBlock(hFile, diskPosition, rWriteAccesor->second->m_IB_blockNumber);
        
        //update record
        memcpy(pDiskBlock + rWriteAccesor->second->m_IB_recordOffset, &rDiskRecord, sizeof(DREC));
        
        //update on disk
        IO::fseek(hFile, diskPosition, IO::IO_SEEK_SET);
        IO::fwrite(pDiskBlock, INDEX_BLOCK_SIZE, hFile);
    }
    else
    {
        //we have no empty block
        if(m_freeBlocks.empty())
        {
            //create new block
            pDiskBlock = (uint8*)m_rIndexBlockMemPool.allocate();
            
            //add to cache
            m_rDiskBlockCache.insert(IndexBlockCache::value_type(m_blockCount, pDiskBlock));
            m_pLRUCache->put(m_blockCount);
            
            //create ICIDF
            pICIDF = GetICIDF(pDiskBlock);
            pICIDF->m_amoutOfFreeSpace = INDEX_BLOCK_SIZE - sizeof(DREC) - sizeof(ICIDF);
            
            //add record
            memcpy(pDiskBlock, &rDiskRecord, sizeof(DREC));
            
            //set index
            rWriteAccesor->second->m_IB_blockNumber = m_blockCount;
            rWriteAccesor->second->m_IB_recordOffset = 0;
            
            //add freeblock
            m_freeBlocks.insert(m_blockCount);
            
            //update block count
            ++m_blockCount;
            
            //write to disk - to the end
            IO::fseek(hFile, 0, IO::IO_SEEK_END);
            IO::fwrite(pDiskBlock, INDEX_BLOCK_SIZE, hFile);
        }
        else
        {
            //write to block with free space
            freeBlock = (*m_freeBlocks.begin());
            diskPosition = (INDEX_BLOCK_SIZE*freeBlock);
            
            //get disk block
            //try to use cache
            pDiskBlock = GetCachedDiskBlock(hFile, diskPosition, freeBlock);
            
            //get ICIDF
            pICIDF = GetICIDF(pDiskBlock);
            
            //find empty DREC must be in block marked with freespace
            pDREC = GetEmptyDREC(pDiskBlock, &recordOffset);
            if(pDREC == NULL)
            {
                Log.Error(__FUNCTION__, "GetEmptyDREC returned NULL for block number: %u, disk position: " I64FMTD, freeBlock, diskPosition);
                Log.Error(__FUNCTION__, "=== ICIDF");
                Log.Error(__FUNCTION__, "m_amoutOfFreeSpace = %u", pICIDF->m_amoutOfFreeSpace);
                Log.Error(__FUNCTION__, "m_location = %u", pICIDF->m_location);
                //
                //dirty fix erase from free blocks and write to another block
                m_freeBlocks.erase(freeBlock);
                this->WriteRecordIndexToDisk(hFile, rWriteAccesor);
            }
            else
            {
                //add record
                memcpy(pDREC, &rDiskRecord, sizeof(DREC));
                
                //set index
                rWriteAccesor->second->m_IB_blockNumber = freeBlock;
                rWriteAccesor->second->m_IB_recordOffset = recordOffset;
                
                //update ICIDF
                pICIDF->m_amoutOfFreeSpace -= sizeof(DREC);
                
                //check free space
                if(pICIDF->m_amoutOfFreeSpace < sizeof(DREC))
                {
                    m_freeBlocks.erase(freeBlock);
                }
                
                //write to disk
                IO::fseek(hFile, diskPosition, IO::IO_SEEK_SET);
                IO::fwrite(pDiskBlock, INDEX_BLOCK_SIZE, hFile);
            }
        }
    }
}

void IndexBlock::EraseRecord(const HANDLE &hFile, const RecordIndex &rRecordIndex)
{
    //not written yet
    if(rRecordIndex.m_IB_recordOffset == -1)
    return;
    
    ICIDF *pICIDF;
	DREC *pDREC;
    size_t diskPosition;
    uint8 *pDiskBlock;
    
    //calc position on disk
    diskPosition = (INDEX_BLOCK_SIZE * rRecordIndex.m_IB_blockNumber);
    
    //load block from disk
    pDiskBlock = GetCachedDiskBlock(hFile, diskPosition, rRecordIndex.m_IB_blockNumber);
    
    //get ICIDF
    pICIDF = GetICIDF(pDiskBlock);
    
    /*
     
     Record size 18 bytes
     +---------+---------+---------+---------------+------+
     | record1 | record2 | record3 |  free space   | ICIDF|
     +---------+---------+---------+---------------+------+
     0         18        36         54             4092  4096
     
     */
    
    //clear DREC
    pDREC = (DREC*)(pDiskBlock + rRecordIndex.m_IB_recordOffset);
    memset(pDREC, 0, sizeof(DREC));
    
    //update ICIDF
    pICIDF->m_amoutOfFreeSpace += sizeof(DREC);
    
    //block has freespace
    m_freeBlocks.insert(rRecordIndex.m_IB_blockNumber);
    
    //write to disk
    IO::fseek(hFile, diskPosition, IO::IO_SEEK_SET);
    IO::fwrite(pDiskBlock, INDEX_BLOCK_SIZE, hFile);
}

uint8 *IndexBlock::GetCachedDiskBlock(const HANDLE &hFile, const size_t &blockDiskPosition, const uint32 &blockNumber)
{
    uint8 *pDiskBlock;
    uint64 blockToDelete;
    IndexBlockCache::iterator itr;
    
    //find block in cache
    itr = m_rDiskBlockCache.find(blockNumber);
    if(itr != m_rDiskBlockCache.end())
    {
        m_pLRUCache->put(blockNumber);
        return itr->second;
    }
    
    //check cache and read block from disk
    if((m_rDiskBlockCache.size() * INDEX_BLOCK_SIZE) > g_IndexBlockCacheSize)
    {
        if(m_pLRUCache->get(&blockToDelete))
        {
            itr = m_rDiskBlockCache.find((uint32)blockToDelete);
            if(itr != m_rDiskBlockCache.end())
            {
                m_rIndexBlockMemPool.deallocate((IndexBlock_T*)itr->second);
                m_rDiskBlockCache.erase(itr);
            }
            //remove from LRUCache
            if(m_pLRUCache->remove(blockToDelete) == false)
            {
                Log.Warning(__FUNCTION__, "m_pLRUCache->remove(" I64FMTD ") == false", blockToDelete);
            }
        }
    }
    
    //allocate new block
    pDiskBlock = (uint8*)m_rIndexBlockMemPool.allocate();
    
    //read from disk
    IO::fseek(hFile, blockDiskPosition, IO::IO_SEEK_SET);
    IO::fread(pDiskBlock, INDEX_BLOCK_SIZE, hFile);
    
    //add to cache
    m_rDiskBlockCache.insert(IndexBlockCache::value_type(blockNumber, pDiskBlock));
    m_pLRUCache->put(blockNumber);
    
    return pDiskBlock;
}

DREC *IndexBlock::GetEmptyDREC(const uint8 *pDiskBlock, int16 *newRecordOffset)
{
    DREC *pDREC;
    int16 position = 0;
    
    for(;;)
    {
        if(position > IMAX_RECORD_POS)
            break;
        
        //get DREC
        pDREC = (DREC*)(pDiskBlock + position);
        
        //check if is empty
        if(IsEmptyDREC(pDREC))
        {
            *newRecordOffset = position;
            return pDREC;
        }
        
        //increment
        position += sizeof(DREC);
    }
    
    return NULL;
}































