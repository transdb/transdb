//
//  IndexBlock.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 9/3/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

IndexBlock::IndexBlock()
{
    m_pIndexFile            = NULL;
    m_blockCount            = 0;
    m_pIndexBlockMemPool    = new FixedPool<IndexBlock_T>(g_IndexBlockCacheSize / INDEX_BLOCK_SIZE);
    m_pLRUCache             = new LRUCache();
}

IndexBlock::~IndexBlock()
{
    delete m_pIndexBlockMemPool;
    delete m_pLRUCache;
    
    IO::fclose(m_pIndexFile);
    m_pIndexFile = NULL;
}

void IndexBlock::Sync()
{
    IO::fflush(m_pIndexFile);
}

struct IndexDef
{
	explicit IndexDef() : m_key(0), m_dataPosition(0), m_dataLen(0)
	{
        
	}

    uint64              m_key;
    std::streamoff      m_dataPosition;
    std::streamsize     m_dataLen;
};

INLINE static bool SortFreeSpace(const IndexDef *pIndexDefStruct1, const IndexDef *pIndexDefStruc2)
{
	return (pIndexDefStruct1->m_dataPosition < pIndexDefStruc2->m_dataPosition);
}

void IndexBlock::Init(Storage *pStorage,
					  const char *pIndexFilePath, 
				      RecordIndexMap &rRecordIndexMap,
					  std::streamoff dataFileSize)
{
	uint8 *pData;
    uint8 *pBlock;
    ICIDF *pICIDF;
    uint16 position;
    DREC *pDREC;
    RecordIndex *pIREC;
    std::streamoff dataFileSizeTmp;
    IndexDef *pIndexDef;
    std::streamsize freeSpaceLen;
    uint64 counter;
    size_t freeSpaceStart;
    bool oFreeSpaceLoadStatus;
    IndexDef *pIndexDefStruct;
    auto_ptr<FixedPool<IndexDef> > pIndexDefPool = auto_ptr<FixedPool<IndexDef> >(new FixedPool<IndexDef>(100000));

    typedef std::vector<IndexDef*> IndexDefList;
    IndexDefList::iterator itr, prevItr;
    IndexDefList rIndexDef;
    
	//save dataFileSize for later use
	dataFileSizeTmp = dataFileSize;

    //check file exists
    Common::CheckFileExists(pIndexFilePath, true);

    //open index file
    m_pIndexFile = fopen(pIndexFilePath, "rb+R");
    assert(m_pIndexFile);
    
	//get index file size
    IO::fseek(m_pIndexFile, 0, SEEK_END);
    size_t fileSize = IO::ftell(m_pIndexFile);
    
    if(fileSize != 0)
    {
        pData = (uint8*)aligned_malloc(fileSize);
		m_blockCount = static_cast<uint32>(fileSize / INDEX_BLOCK_SIZE);
        
        //read
        IO::fseek(m_pIndexFile, 0, SEEK_SET);
        IO::fread(pData, fileSize, 1, m_pIndexFile);

        //build index map
        for(uint32 i = 0;i < m_blockCount;++i)
        {
            position = 0;
            pBlock = pData+(INDEX_BLOCK_SIZE*i);
            pICIDF = GetICIDF(pBlock);

            //read record
            for(;;)
            {
                if(position >= pICIDF->m_location)
                    break;
                
                //get record
                pDREC = (DREC*)(pBlock+position);
                
                //check recordstart
                if(pDREC->m_recordStart == -1)
                {
                    Log.Error(__FUNCTION__, "INDEX CORRUPTION X: "I64FMTD" recordStart == -1 skipping", pDREC->m_key);
                }
                else
                {
                    //create storage index
                    pIREC                       = pStorage->m_pRecordIndexMemPool->alloc();
                    pIREC->m_recordStart        = pDREC->m_recordStart;
                    pIREC->m_blockCount         = pDREC->m_blockCount;
                    pIREC->m_pBlockManager      = NULL;
                    //flags
                    pIREC->m_flags              = eRIF_None;
                    //set internal index
                    pIREC->m_IB_blockNumber     = i;
                    pIREC->m_IB_recordOffset    = position;
                    rRecordIndexMap.insert(RecordIndexMap::value_type(pDREC->m_key, pIREC));
                    
                    //create tmp freespace for calculating freespace list
                    pIndexDefStruct					= pIndexDefPool->alloc();
                    pIndexDefStruct->m_key			= pDREC->m_key;
                    pIndexDefStruct->m_dataPosition = pDREC->m_recordStart;
                    pIndexDefStruct->m_dataLen		= pIREC->m_blockCount * BLOCK_SIZE;
                    rIndexDef.push_back(pIndexDefStruct);
                }
                //update position
                position += sizeof(DREC);
            }
            //add block with free space
            if(pICIDF->m_amoutOfFreeSpace >= sizeof(DREC))
            {
                m_freeBlocks.insert(i);
            }
        }
        
        //release memory
        aligned_free(pData);
	}
   
    //try to load freespace from file
    oFreeSpaceLoadStatus = pStorage->LoadFreeSpace();
    
    //load free space from index if load freespace from file failed
    if(!oFreeSpaceLoadStatus)
    {
        if(rIndexDef.size())
        {
            //sort
            std::sort(rIndexDef.begin(), rIndexDef.end(), SortFreeSpace);
            
            //init variables
            counter = 0;
            freeSpaceStart = 0;
            
            //
            for(itr = rIndexDef.begin();itr != rIndexDef.end();++itr)
            {
                pIndexDef = (*itr);

                //
                freeSpaceLen = pIndexDef->m_dataPosition - freeSpaceStart;
                if(freeSpaceLen > 0)
                {
                    pStorage->AddFreeSpace(freeSpaceStart, freeSpaceLen);
                }
                else if(freeSpaceLen < 0)
                {
                    Log.Error(__FUNCTION__, "Data curruption - "I64FMTD" - X: "I64FMTD" freeSpaceLen is negative", counter, pIndexDef->m_key);
                }
                
                freeSpaceStart += (pIndexDef->m_dataLen + freeSpaceLen);
                dataFileSizeTmp -= (pIndexDef->m_dataLen + freeSpaceLen);
                ++counter;
                
                //free memory
                pIndexDefPool->free(pIndexDef);
            }
        }
        
        //add last piece of freespace
        if(dataFileSizeTmp != 0)
        {
            pStorage->AddFreeSpace(dataFileSize - dataFileSizeTmp, dataFileSizeTmp);
        }
    }
}

void IndexBlock::WriteRecordIndexToDisk(const RecordIndexMap::accessor &rAccesor)
{    
    ICIDF *pICIDF;
    uint32 freeBlock;
    size_t diskPosition;
    uint8 *pDiskBlock;
    
    //fill disk record    
    DREC rDiskRecord;    
    rDiskRecord.m_key = rAccesor->first;
    rDiskRecord.m_recordStart = rAccesor->second->m_recordStart;
    rDiskRecord.m_blockCount = rAccesor->second->m_blockCount;
    
    //x is existing rewrite
    if(rAccesor->second->m_IB_recordOffset != -1)
    {
        //calc block position on disk
        diskPosition = (INDEX_BLOCK_SIZE * rAccesor->second->m_IB_blockNumber);
        
        //get disk block
        pDiskBlock = GetCachedDiskBlock(diskPosition, rAccesor->second->m_IB_blockNumber);
        
        //update record
        memcpy(pDiskBlock + rAccesor->second->m_IB_recordOffset, &rDiskRecord, sizeof(rDiskRecord));
        
        //update on disk
        IO::fseek(m_pIndexFile, diskPosition, SEEK_SET);
        IO::fwrite(pDiskBlock, INDEX_BLOCK_SIZE, 1, m_pIndexFile);
    }
    else
    {
        //we have no empty block
        if(m_freeBlocks.empty())
        {
            //create new block
            pDiskBlock = (uint8*)m_pIndexBlockMemPool->alloc();
            memset(pDiskBlock, 0, INDEX_BLOCK_SIZE);
            
            //add to cache
            m_rDiskBlockCache.insert(IndexBlockCache::value_type(m_blockCount, pDiskBlock));
            m_pLRUCache->put(m_blockCount);
            
            //create ICIDF
            pICIDF = GetICIDF(pDiskBlock);
            pICIDF->m_location = sizeof(rDiskRecord);
            pICIDF->m_amoutOfFreeSpace = INDEX_BLOCK_SIZE - sizeof(rDiskRecord) - sizeof(ICIDF);
            
            //add record
            memcpy(pDiskBlock, &rDiskRecord, sizeof(rDiskRecord));
            
            //set index
            rAccesor->second->m_IB_blockNumber = m_blockCount;
            rAccesor->second->m_IB_recordOffset = 0;
            
            //add freeblock
            m_freeBlocks.insert(m_blockCount);
            
            //update block count
            ++m_blockCount;
            
            //write to disk - to the end
            IO::fseek(m_pIndexFile, 0, SEEK_END);
            IO::fwrite(pDiskBlock, INDEX_BLOCK_SIZE, 1, m_pIndexFile);
        }
        else
        {
            //write to block with free space
            freeBlock = (*m_freeBlocks.begin());
            diskPosition = (INDEX_BLOCK_SIZE*freeBlock);            
            
            //get disk block            
            //try to use cache
            pDiskBlock = GetCachedDiskBlock(diskPosition, freeBlock);
                        
            //get ICIDF
            pICIDF = GetICIDF(pDiskBlock);
            
            //add record
            memcpy(pDiskBlock + pICIDF->m_location, &rDiskRecord, sizeof(rDiskRecord));
            
            //set index
            rAccesor->second->m_IB_blockNumber = freeBlock;
            rAccesor->second->m_IB_recordOffset = pICIDF->m_location;
            
            //update ICIDF
            pICIDF->m_location += sizeof(rDiskRecord);
            pICIDF->m_amoutOfFreeSpace -= sizeof(rDiskRecord);
            
            //check free space
            if(pICIDF->m_amoutOfFreeSpace < sizeof(rDiskRecord))
            {
                m_freeBlocks.erase(freeBlock);
            }
            
            //write to disk
            IO::fseek(m_pIndexFile, diskPosition, SEEK_SET);
            IO::fwrite(pDiskBlock, INDEX_BLOCK_SIZE, 1, m_pIndexFile);        
        }
    }
}

void IndexBlock::EraseRecord(RecordIndexMap::accessor &rWriteAccesor, RecordIndexMap &rRecordIndexMap)
{
    //not written yet
    if(rWriteAccesor->second->m_IB_recordOffset == -1)
        return;
    
    ICIDF *pICIDF;  
	DREC *pDREC;
    size_t diskPosition;
    size_t moveSize;
	uint16 position;
    RecordIndexMap::accessor rWriteAccesorReindex;
    uint8 *pDiskBlock;
    
    //calc position on disk
    diskPosition = (INDEX_BLOCK_SIZE * rWriteAccesor->second->m_IB_blockNumber);
    
    //load block from disk
    pDiskBlock = GetCachedDiskBlock(diskPosition, rWriteAccesor->second->m_IB_blockNumber);    
        
    //get ICIDF
    pICIDF = GetICIDF(pDiskBlock);
    
    //move
    moveSize = INDEX_BLOCK_SIZE - rWriteAccesor->second->m_IB_recordOffset - pICIDF->m_amoutOfFreeSpace - sizeof(ICIDF);
    memmove(pDiskBlock + rWriteAccesor->second->m_IB_recordOffset, pDiskBlock + rWriteAccesor->second->m_IB_recordOffset + sizeof(DREC), moveSize);
    
    //update free space
    pICIDF->m_amoutOfFreeSpace += sizeof(DREC);
    pICIDF->m_location -= sizeof(DREC);
    
    //clear free space
    memset(pDiskBlock + pICIDF->m_location, 0, pICIDF->m_amoutOfFreeSpace);

	//reindex 
	position = rWriteAccesor->second->m_IB_recordOffset;
    for(;;)
    {
        if(position >= pICIDF->m_location)
            break;
                
        pDREC = (DREC*)(pDiskBlock + position);

        //change internal index
        if(rRecordIndexMap.find(rWriteAccesorReindex, pDREC->m_key))
        {
            rWriteAccesorReindex->second->m_IB_recordOffset -= sizeof(DREC);
        }
                
        //update position
        position += sizeof(DREC);
    }

    //block has freespace
    m_freeBlocks.insert(rWriteAccesor->second->m_IB_blockNumber);
    
    //write to disk
    IO::fseek(m_pIndexFile, diskPosition, SEEK_SET);
    IO::fwrite(pDiskBlock, INDEX_BLOCK_SIZE, 1, m_pIndexFile);
}

uint8 *IndexBlock::GetCachedDiskBlock(const size_t &blockDiskPosition, const uint32 &blockNumber)
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
                m_pIndexBlockMemPool->free((IndexBlock_T*)itr->second);
                m_rDiskBlockCache.erase(itr);
            }
        }
    }
    
    //allocate new block
    pDiskBlock = (uint8*)m_pIndexBlockMemPool->alloc();
    
    //read from disk
    IO::fseek(m_pIndexFile, blockDiskPosition, SEEK_SET);
    IO::fread(pDiskBlock, INDEX_BLOCK_SIZE, 1, m_pIndexFile);
    
    //add to cache
    m_rDiskBlockCache.insert(IndexBlockCache::value_type(blockNumber, pDiskBlock));
    m_pLRUCache->put(blockNumber);
    
    return pDiskBlock;
}

































