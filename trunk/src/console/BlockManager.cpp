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

BlockManager::BlockManager(uint8 *pBlocks, const uint16 &blockCount) : m_pBlocks(pBlocks), m_blockCount(blockCount)
{
    //build index map
    uint8 *pBlock;
    uint16 position;
    uint16 endOfRDFArea;
    CIDF *pCIDF;
    RDF *pRDF;
    
    //build index map
    for(uint16 i = 0;i < m_blockCount;++i)
    {
        pBlock = GetBlock(i);
        pCIDF = Block::GetCIDF(pBlock);
        position = CIDFOffset - sizeof(RDF);
        endOfRDFArea = pCIDF->m_location + pCIDF->m_amoutOfFreeSpace;
        for(;;)
        {
            //end of RDF area
            if(position < endOfRDFArea)
                break;
            
            pRDF = (RDF*)(pBlock + position);
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
    void *pNewBlocks = scalable_aligned_realloc(m_pBlocks, m_blockCount * BLOCK_SIZE, 512);
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
    Block::InitBlock(pNewBlock);
}

uint32 BlockManager::WriteRecord(const uint64 &recordkey, const uint8 *pRecord, const uint16 &recordSize)
{
    //size check
    if(recordSize > MAX_RECORD_SIZE)
        return eBMS_FailRecordTooBig;
    
    E_BLS status;
    uint8 *pBlock;
    uint16 RDFPosition;
    uint16 recordPosition;
    BlocksIndex::iterator itr;
    uint64 recordKeyTmp;
    
    int removedRecords = 0;
    uint32 retStatus = eBMS_Ok;
    
    uint16 recordSizeLocal;
    ByteBuffer rOut;
    int cStatus;
    
    //PHASE 0 --> compression
    //save variables
    recordSizeLocal = recordSize;
    
    //try to compress
    if(recordSizeLocal > g_RecordSizeForCompression)
    {
        cStatus = CommonFunctions::compressGzip(g_GzipCompressionLevel, pRecord, recordSizeLocal, rOut);
        if(cStatus == Z_OK)
        {
            if(rOut.size() < recordSizeLocal)
            {
                pRecord = rOut.contents();
                recordSizeLocal = (uint16)rOut.size();
                g_NumOfRecordCompressions++;
            }
            else
            {
                Log.Warning(__FUNCTION__, "Compression generated bigger size. Source size: %u, new size: " I64FMTD, recordSizeLocal, rOut.size());
            }
        }
    }
    
    //PHASE 1 --> try to update
    //try to update record first
    itr = m_rBlockIndex.find(recordkey);
    if(itr != m_rBlockIndex.end())
    {
        //get block
        pBlock = GetBlock(itr->second);
        //update
        status = Block::UpdateRecord(pBlock, recordkey, pRecord, recordSizeLocal, NULL, &RDFPosition, &recordPosition);
        if(status == eBLS_OK)
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
        //limit can be lowed so delete all records
        while(m_rBlockIndex.size() >= recordLimit)
        {
            //delete 1st one (the oldest)
            itr = m_rBlockIndex.begin();
            
            //DO NOT ADD OLDER DATA
            //check if new data are not the older one
            recordKeyTmp = itr->first;
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
    for(uint16 i = 0;i < m_blockCount;++i)
    {
        pBlock = GetBlock(i);
        status = Block::WriteRecord(pBlock, recordkey, pRecord, recordSizeLocal);
        if(status == eBLS_OK)
        {
            m_rBlockIndex.insert(BlocksIndex::value_type(recordkey, i));
            break;
        }
        else if(status == eBLS_NO_SPACE_FOR_NEW_DATA && (i == (m_blockCount - 1)))
        {
            //relloc block + 1
            ReallocBlocks();
            //set status
            retStatus |= eBMS_ReallocatedBlocks;
        }
    }
    
    return retStatus;
}

void BlockManager::ReadRecord(const uint64 &recordkey, ByteBuffer &rData)
{
    uint8 *pBlock;
    BlocksIndex::iterator itr;
    
    itr = m_rBlockIndex.find(recordkey);
    if(itr != m_rBlockIndex.end())
    {
        pBlock = GetBlock(itr->second);
        Block::GetRecord(pBlock, recordkey, rData);
    }
}

void BlockManager::ReadRecords(ByteBuffer &rData)
{
    //pre-realloc buffer
    //key|recordSize|record|....Nx
    rData.reserve(BLOCK_SIZE * m_blockCount);
    
    uint8 *pBlock;
    for(uint16 i = 0;i < m_blockCount;++i)
    {
        pBlock = GetBlock(i);
        Block::GetRecords(pBlock, rData);
    }
}

void BlockManager::DeleteRecord(const uint64 &recordkey)
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
    deleteStatus = Block::DeleteRecord(pBlock, itr->first, NULL, &RDFPosition, &recordPosition);
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
    uint8 *pBlock;
    CIDF *pCIDF;
    
    for(uint16 i = 0;i < m_blockCount;++i)
    {
        pBlock = GetBlock(i);
        pCIDF = Block::GetCIDF(pBlock);
        pCIDF->m_flags &= ~eBLF_Dirty;
    }
}

void BlockManager::DefragmentData()
{
    //TODO: sort for best fit
    
    uint64 recordkey;
    uint16 recordSize;
    size_t numOfrecords;
    uint8 arData[BLOCK_SIZE];
    ByteBuffer rData;
    
    //save record count
    numOfrecords = numOfRecords();
    if(numOfrecords == 0)
    {
        Log.Warning(__FUNCTION__, "numOfrecords == 0");
        return;
    }
    
    //read all data - key|recordSize|record|....Nx
    ReadRecords(rData);
    
    //deallloc blocks
    DeallocBlocks();
    //create empty one
    ReallocBlocks();
    
    //insert
    if(numOfrecords)
    {
        while(rData.rpos() < rData.size())
        {
            //get record
            rData >> recordkey;
            rData >> recordSize;
            rData.read(arData, recordSize);
            
            //insert - ignore return value
            (void)WriteRecord(recordkey, arData, recordSize);
        }
    }
    
    //update counter
    g_NumOfRecordDeframentations++;
}

uint32 BlockManager::GetBlocksCrc32()
{
    uint32 crc32;
    uint8 *pBlock;
    size_t crc32ArraySize = sizeof(uint32) * m_blockCount;
    uint32 *pCrc32Array = (uint32*)alloca(crc32ArraySize);
    
    for(uint16 i = 0;i < m_blockCount;++i)
    {
        pBlock = GetBlock(i);
        pCrc32Array[i] = g_CRC32->ComputeCRC32(pBlock, BLOCK_SIZE);
    }
    
    //
    crc32 = g_CRC32->ComputeCRC32((BYTE*)pCrc32Array, crc32ArraySize);
    return crc32;
}





































