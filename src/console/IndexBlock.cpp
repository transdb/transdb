//
//  IndexBlock.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 9/3/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

static INLINE bool _S_SortIndexDef(const IndexDef &rIndexDefStruct1, const IndexDef &rIndexDefStruc2)
{
	return (rIndexDefStruct1.m_dataPosition < rIndexDefStruc2.m_dataPosition);
}

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
        
        //build index map
        for(uint32 i = range.begin();i != range.end();++i)
        {
            position = 0;
            pBlock = (m_pData + (INDEX_BLOCK_SIZE * i));
            pICIDF = IndexBlock::GetICIDF(pBlock);
            
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
                    //can be NULL when used for freespace defragmentation
                    if(m_pRecordIndexMap != NULL)
                    {
                        //create storage index
                        RecordIndex rRecordIndex;
                        rRecordIndex.m_recordStart        = pDREC->m_recordStart;
                        rRecordIndex.m_blockCount         = pDREC->m_blockCount;
                        rRecordIndex.m_crc32              = pDREC->m_crc32;
                        rRecordIndex.m_pBlockManager      = NULL;
                        //flags
                        rRecordIndex.m_flags              = eRIF_None;
                        //set internal index
                        rRecordIndex.m_IB_blockNumber     = i;
                        rRecordIndex.m_IB_recordOffset    = position;
                        m_pRecordIndexMap->insert(RecordIndexMap::value_type(pDREC->m_key, rRecordIndex));
                    }
                    
                    //create tmp freespace for calculating freespace list
                    m_pIndexDef->push_back(IndexDef(pDREC->m_key, pDREC->m_recordStart, pDREC->m_blockCount * BLOCK_SIZE));
                }
                
                //update position
                position += sizeof(DREC);
            }
            
            //add block with free space
            if(m_pRecordIndexMap != NULL)
            {
                if(pICIDF->m_amountOfFreeSpace >= sizeof(DREC))
                {
                    m_pFreeBlocksSet->insert(i);
                }
            }
        }
    }
};

IndexBlock::IndexBlock() : m_blockCount(0),
                           m_pLRUCache(new LRUCache("IndexBlock", g_IndexBlockCacheSize / INDEX_BLOCK_SIZE))
{

}

IndexBlock::~IndexBlock()
{
    delete m_pLRUCache;
}

E_IIS IndexBlock::Init(HANDLE hFile,
                       RecordIndexMap *pRecordIndexMap,
                       FreeSpaceBlockMap &rFreeSpace,
                       int64 dataFileSize)
{
    //for concurrent processing
    std::unique_ptr<Init_IndexDefVec> pIndexDef = std::unique_ptr<Init_IndexDefVec>(new Init_IndexDefVec());
    std::unique_ptr<Init_FreeBlocksSet> pFreeBlocksSet = std::unique_ptr<Init_FreeBlocksSet>(new Init_FreeBlocksSet());
    //prealloc
    pIndexDef->reserve(4*1024*1024);
    
	//get index file size
    IO::fseek(hFile, 0, IO::IO_SEEK_END);
	int64 fileSize = IO::ftell(hFile);
    
    if(fileSize != 0)
    {
        void *pData = scalable_aligned_malloc(fileSize, g_IndexFileMallocAlignment);
        if(pData == NULL)
        {
            Log.Error(__FUNCTION__, "Cannot allocate memory for index block file.");
            return eIIS_NoMemory;
        }
        
        //calc block count
		m_blockCount = static_cast<uint32>(fileSize / INDEX_BLOCK_SIZE);
        
        //read
        IO::fseek(hFile, 0, IO::IO_SEEK_SET);
        IO::fread(pData, fileSize, hFile);
        
        //create struct - save pointers to data and containers
        FillIndex rFillIndex;
        rFillIndex.m_pData = (uint8*)pData;
        rFillIndex.m_pFreeBlocksSet = pFreeBlocksSet.get();
        rFillIndex.m_pIndexDef = pIndexDef.get();
        rFillIndex.m_pRecordIndexMap = pRecordIndexMap;
        
        //iterate and fill containers
        tbb::parallel_for(tbb::blocked_range<uint32>(0, m_blockCount), rFillIndex);
        
        //defragment
        pIndexDef->shrink_to_fit();
        
        //add data to m_freeBlocks
        if(pRecordIndexMap != NULL)
        {
            for(Init_FreeBlocksSet::iterator itr = pFreeBlocksSet->begin();itr != pFreeBlocksSet->end();++itr)
            {
                m_freeBlocks.insert(*itr);
            }
        }
        
        //release memory
        scalable_aligned_free(pData);
	}
    
    //save to temporary variable when index file is corrpted we will not loose old data from memory
    FreeSpaceBlockMap rFreeSpaceBlockMap;
    
    //load free space from index def vector
    bool status = LoadFreeSpaceFromIndexDef(rFreeSpaceBlockMap, dataFileSize, *pIndexDef);
    if(!status)
    {
        Log.Error(__FUNCTION__, "IndexFile corrupted: cannot parse freespace blocks.");
        return eIIS_FreeSpaceCorrupted;
    }
    
    //assign to freespace map
    rFreeSpace = std::move(rFreeSpaceBlockMap);
    
    return eIIS_OK;
}

bool IndexBlock::LoadFreeSpaceFromIndexDef(FreeSpaceBlockMap &rFreeSpaceBlockMap, int64 dataFileSize, Init_IndexDefVec &rIndexDef)
{
    int64 dataFileSizeTmp;
    int64 freeSpaceLen;
    uint64 counter;
    int64 freeSpaceStart;
    bool status = true;
    
	//save dataFileSize for later use
	dataFileSizeTmp = dataFileSize;
    
    //load free space from index def vector
    if(rIndexDef.size())
    {
        //sort
        tbb::parallel_sort(rIndexDef.begin(), rIndexDef.end(), &_S_SortIndexDef);
        
        //init variables
        counter = 0;
        freeSpaceStart = 0;
        
        //
        for(Init_IndexDefVec::const_iterator itr = rIndexDef.begin();itr != rIndexDef.end();++itr)
        {
            //
            freeSpaceLen = itr->m_dataPosition - freeSpaceStart;
            if(freeSpaceLen > 0)
            {
                DiskWriter::AddFreeSpace(rFreeSpaceBlockMap, freeSpaceStart, freeSpaceLen);
            }
            else if(freeSpaceLen < 0)
            {
                Log.Error(__FUNCTION__, "Data curruption - " I64FMTD " - X: " I64FMTD " freeSpaceLen is negative: " SI64FMTD, counter, itr->m_key, freeSpaceLen);
                status = false;
            }
            
            freeSpaceStart += (itr->m_dataLen + freeSpaceLen);
            dataFileSizeTmp -= (itr->m_dataLen + freeSpaceLen);
            ++counter;
        }
    }
    
    //add last piece of freespace
    if(dataFileSizeTmp != 0)
    {
        DiskWriter::AddFreeSpace(rFreeSpaceBlockMap, dataFileSize - dataFileSizeTmp, dataFileSizeTmp);
    }
    
    return status;
}

void IndexBlock::WriteRecordIndexToDisk(HANDLE hFile, RecordIndexMap::accessor &rWriteAccesor)
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
    rDiskRecord.m_recordStart   = rWriteAccesor->second.m_recordStart;
    rDiskRecord.m_crc32         = rWriteAccesor->second.m_crc32;
    rDiskRecord.m_blockCount    = rWriteAccesor->second.m_blockCount;
    
    //x is existing rewrite
    if(rWriteAccesor->second.m_IB_recordOffset != -1)
    {
        //calc block position on disk
        diskPosition = (INDEX_BLOCK_SIZE * rWriteAccesor->second.m_IB_blockNumber);
        
        //get disk block
        pDiskBlock = GetCachedDiskBlock(hFile, diskPosition, rWriteAccesor->second.m_IB_blockNumber);
        
        //update record
        memcpy(pDiskBlock + rWriteAccesor->second.m_IB_recordOffset, &rDiskRecord, sizeof(DREC));
        
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
            pDiskBlock = (uint8*)scalable_aligned_malloc(INDEX_BLOCK_SIZE, g_IndexFileMallocAlignment);
            memset(pDiskBlock, 0, INDEX_BLOCK_SIZE);
            
            //add to cache
            m_rDiskBlockCache.insert(IndexBlockCache::value_type(m_blockCount, pDiskBlock));
            m_pLRUCache->put(m_blockCount);
            
            //create ICIDF
            pICIDF = GetICIDF(pDiskBlock);
            pICIDF->m_amountOfFreeSpace = INDEX_BLOCK_SIZE - sizeof(DREC) - sizeof(ICIDF);
            
            //add record
            memcpy(pDiskBlock, &rDiskRecord, sizeof(DREC));
            
            //set index
            rWriteAccesor->second.m_IB_blockNumber = m_blockCount;
            rWriteAccesor->second.m_IB_recordOffset = 0;
            
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
                Log.Error(__FUNCTION__, "m_amoutOfFreeSpace = %u", pICIDF->m_amountOfFreeSpace);
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
                rWriteAccesor->second.m_IB_blockNumber = freeBlock;
                rWriteAccesor->second.m_IB_recordOffset = recordOffset;
                
                //update ICIDF
                pICIDF->m_amountOfFreeSpace -= sizeof(DREC);
                
                //check free space
                if(pICIDF->m_amountOfFreeSpace < sizeof(DREC))
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

void IndexBlock::EraseRecord(HANDLE hFile, const RecordIndex &rRecordIndex)
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
    pICIDF->m_amountOfFreeSpace += sizeof(DREC);
    
    //block has freespace
    m_freeBlocks.insert(rRecordIndex.m_IB_blockNumber);
    
    //write to disk
    IO::fseek(hFile, diskPosition, IO::IO_SEEK_SET);
    IO::fwrite(pDiskBlock, INDEX_BLOCK_SIZE, hFile);
}

ICIDF *IndexBlock::GetICIDF(const uint8 *pBlock)
{
    return (ICIDF*)(pBlock + ICIDFOffset);
}

bool IndexBlock::IsEmptyDREC(const DREC *pDREC)
{
    return (pDREC->m_key == 0 && pDREC->m_recordStart == 0 && pDREC->m_blockCount == 0 && pDREC->m_crc32 == 0);
}

uint8 *IndexBlock::GetCachedDiskBlock(HANDLE hFile, int64 blockDiskPosition, uint32 blockNumber)
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
                scalable_aligned_free((void*)itr->second);
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
	pDiskBlock = (uint8*)scalable_aligned_malloc(INDEX_BLOCK_SIZE, g_IndexFileMallocAlignment);
    
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































