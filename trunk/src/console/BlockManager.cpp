//
//  BlockManager.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/24/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

BlockManager::BlockManager() : m_pBlocks(NULL), m_blockCount(0)
{
    //create initial block
    ReallocBlocks();
}

BlockManager::BlockManager(uint8 *pBlocks, uint16 blockCount) : m_pBlocks(pBlocks), m_blockCount(blockCount)
{
    //build index map
    for(uint16 i = 0;i < m_blockCount;++i)
    {
        uint8 *pBlock = GetBlock(i);
        CIDF *pCIDF = Block_GetCIDF(pBlock);
        uint16 position = CIDFOffset - sizeof(RDF);
        uint16 endOfRDFArea = pCIDF->m_location + pCIDF->m_amoutOfFreeSpace;
        for(;;)
        {
            //end of RDF area
            if(position < endOfRDFArea)
                break;
            
            RDF *pRDF = (RDF*)(pBlock + position);
            m_rBlockIndex.insert(BlocksIndex::value_type(pRDF->m_key, i));
            position -= sizeof(RDF);
        }
    }
}

BlockManager::~BlockManager()
{
    DeallocBlocks();
}

void BlockManager::DeallocBlocks()
{
    //dealloc blocks
    scalable_aligned_free((void*)m_pBlocks);
    m_pBlocks = NULL;
    
    //set block count to 0
    m_blockCount = 0;
    
    //clear indexes
    m_rBlockIndex.clear();
}

void BlockManager::ReallocBlocks()
{
    //inc block count
    m_blockCount += 1;
    
    //realloc block array
    void *pNewBlocks = scalable_aligned_realloc(m_pBlocks, m_blockCount * BLOCK_SIZE, g_DataFileMallocAlignment);
    if(pNewBlocks == NULL)
    {
        throw std::bad_alloc();
    }
    //set new pointer
    m_pBlocks = (uint8*)pNewBlocks;
    
    //get new block
    uint8 *pNewBlock = GetBlock(m_blockCount - 1);
    memset(pNewBlock, 0, BLOCK_SIZE);
    
    //init new block
    Block_InitBlock(pNewBlock);
}

uint32 BlockManager::WriteRecord(uint64 recordkey, const uint8 *pRecord, uint16 recordSize)
{
    //size check
    if(recordSize > MAX_RECORD_SIZE)
        return eBMS_FailRecordTooBig;
    
    //variables
    uint32 retStatus = eBMS_Ok;
    ByteBuffer rOut;
    
    //PHASE 0 --> compression   
    //try to compress
    if(recordSize > g_RecordSizeForCompression)
    {
        int cStatus = CommonFunctions::compressGzip(g_GzipCompressionLevel, pRecord, recordSize, rOut, g_ZlibBufferSize);
        if(cStatus == Z_OK)
        {
            if(rOut.size() < recordSize)
            {
                pRecord = rOut.contents();
                recordSize = (uint16)rOut.size();
                ++g_NumOfRecordCompressions;
            }
            else
            {
                Log.Warning(__FUNCTION__, "Compression generated bigger size. Source size: %u, new size: " I64FMTD, recordSize, rOut.size());
            }
        }
    }
    
    //PHASE 1 --> try to update
    //try to update record first
    BlocksIndex::iterator itr = m_rBlockIndex.find(recordkey);
    if(itr != m_rBlockIndex.end())
    {
        //get block
        uint8 *pBlock = GetBlock(itr->second);
        //update
        uint16 RDFPosition;
        uint16 recordPosition;
        E_BLS updateStatus = Block_UpdateRecord(pBlock, recordkey, pRecord, recordSize, NULL, &RDFPosition, &recordPosition);
        if(updateStatus == eBLS_OK)
        {
            return retStatus;
        }
        
        //delete key will be on another block
        m_rBlockIndex.erase(itr);
    }
    
    //PHASE 2 --> check record limit
    //block limit - start to rewrite from begin (record key is timestamp)
    BlocksIndex::size_type recordLimit = static_cast<BlocksIndex::size_type>(g_RecordLimit);
    if(g_EnableRecordLimit && m_rBlockIndex.size() >= recordLimit)
    {
        int removedRecords = 0;
        //limit can be lowed so delete all records
        while(m_rBlockIndex.size() >= recordLimit)
        {
            //delete 1st one (the oldest)
            itr = m_rBlockIndex.begin();
            
            //DO NOT ADD OLDER DATA
            //check if new data are not the older one
            uint64 recordKeyTmp = itr->first;
            if(recordkey < recordKeyTmp)
            {
                retStatus |= (eBMS_RecordCountLimit | eBMS_OldRecord);
                return retStatus;
            }
            
            //update counter
            ++removedRecords;
            //this invalidate iterator
            DeleteRecord(itr);
            //set return status
            retStatus |= eBMS_RecordCountLimit;
        }
        
        //request defragment
        if(removedRecords >= g_DefragAfterRecordDelete)
        {
            retStatus |= eBMS_NeedDefrag;
        }
    }
    
    //PHASE 3 --> try to write
    //find empty space in blocks
    //search from end to begin -> mostly there appends to the end
    uint16 blockNum = m_blockCount;
    for(;;)
    {
        --blockNum;
        //get block and try to write
        uint8 *pBlock = GetBlock(blockNum);
        E_BLS writeStatus = Block_WriteRecord(pBlock, recordkey, pRecord, recordSize);
        if(writeStatus == eBLS_OK)
        {
            m_rBlockIndex.insert(BlocksIndex::value_type(recordkey, blockNum));
            break;
        }
        else if(writeStatus == eBLS_NO_SPACE_FOR_NEW_DATA && blockNum == 0)
        {
            //realloc blocks + 1 -> modify m_blockCount
            ReallocBlocks();
            //set status
            retStatus |= eBMS_ReallocatedBlocks;
            //reset control variable
            blockNum = m_blockCount;
        }
    }
    
    return retStatus;
}

void BlockManager::ReadRecord(uint64 recordkey, CByteBuffer *pData)
{
    BlocksIndex::iterator itr = m_rBlockIndex.find(recordkey);
    if(itr != m_rBlockIndex.end())
    {
        uint8 *pBlock = GetBlock(itr->second);
        Block_GetRecord(pBlock, recordkey, pData);
    }
}

void BlockManager::ReadRecords(CByteBuffer *pData)
{
    //pre-realloc buffer
    //key|recordSize|record|....Nx
    CByteBuffer_reserve(pData, BLOCK_SIZE * m_blockCount);
    
    for(uint16 i = 0;i < m_blockCount;++i)
    {
        uint8 *pBlock = GetBlock(i);
        Block_GetRecords(pBlock, pData);
    }
}

void BlockManager::DeleteRecord(uint64 recordkey)
{
    BlocksIndex::iterator itr = m_rBlockIndex.find(recordkey);
    if(itr != m_rBlockIndex.end())
    {
        DeleteRecord(itr);
    }
}

void BlockManager::DeleteRecord(BlocksIndex::iterator &itr)
{
    uint8 *pBlock;
    uint16 RDFPosition;
    uint16 recordPosition;
    E_BLS deleteStatus;
    
    //delete
    pBlock = GetBlock(itr->second);
    deleteStatus = Block_DeleteRecord(pBlock, itr->first, NULL, &RDFPosition, &recordPosition);
    if(deleteStatus != eBLS_OK)
    {
        Log.Warning(__FUNCTION__, "Trying to delete non-existant key: " I64FMTD " in block number: %u", itr->first, itr->second);
    }
    m_rBlockIndex.erase(itr);
}

void BlockManager::GetAllRecordKeys(ByteBuffer &rY)
{
    if(!m_rBlockIndex.empty())
    {
        //prealloc
        rY.reserve(m_rBlockIndex.size() * sizeof(uint64));
        //add to buffer
        for(BlocksIndex::iterator itr = m_rBlockIndex.begin();itr != m_rBlockIndex.end();++itr)
        {
            rY << uint64(itr->first);
        }
    }
}

void BlockManager::ClearDirtyFlags()
{
    for(uint16 i = 0;i < m_blockCount;++i)
    {
        uint8 *pBlock = GetBlock(i);
        CIDF *pCIDF = Block_GetCIDF(pBlock);
        pCIDF->m_flags &= ~eBLF_Dirty;
    }
}

void BlockManager::DefragmentData()
{
    //TODO: sort for best fit
    
    //save record count
    size_t numOfrecords = numOfRecords();
    if(numOfrecords == 0)
    {
        Log.Warning(__FUNCTION__, "numOfrecords == 0");
        return;
    }
    //create bytebuffer
    CByteBuffer *pData = CByteBuffer_create();
    
    //read all data - key|recordSize|record|....Nx
    ReadRecords(pData);
    
    //deallloc blocks
    DeallocBlocks();
    //create empty one
    ReallocBlocks();
    
    //insert
    if(numOfrecords)
    {
        while(pData->m_rpos < pData->m_size)
        {
            uint64 recordkey;
            uint16 recordSize;
            uint8 arData[BLOCK_SIZE];
            //get record
            CByteBuffer_read(pData, &recordkey, sizeof(recordkey));
            CByteBuffer_read(pData, &recordSize, sizeof(recordSize));
            CByteBuffer_read(pData, &arData, recordSize);
            
            //insert - ignore return value
            (void)WriteRecord(recordkey, arData, recordSize);
        }
    }
    
    //update counter
    g_NumOfRecordDeframentations++;
    
    //delete bytebuffer
    CByteBuffer_destroy(pData);
}

uint32 BlockManager::GetBlocksCrc32() const
{
	size_t crc32ArraySize = sizeof(uint32) * m_blockCount;
	uint32 *pCrc32Array = (uint32*)alloca(crc32ArraySize);

	for (uint16 i = 0; i < m_blockCount; ++i)
	{
		uint8 *pBlock = GetBlock(i);
		pCrc32Array[i] = g_CRC32->ComputeCRC32(pBlock, BLOCK_SIZE);
	}

	uint32 crc32 = g_CRC32->ComputeCRC32((BYTE*)pCrc32Array, crc32ArraySize);
	return crc32;
}





































