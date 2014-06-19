//
//  BlockManager.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/24/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

BlockIndexNode *BlockIndexNode_new(uint64 key, uint16 blockNumber)
{
    BlockIndexNode *pNode = (BlockIndexNode*)malloc(sizeof(BlockIndexNode));
    pNode->m_key = key;
    pNode->m_blockNumber = blockNumber;
    return pNode;
}

void BlockIndexNode_Delete(void *avl_item, void *avl_param)
{
    free(avl_item);
}

void BlockIndexNode_Delete_RemoveFromTree(avl_table *pTree, BlockIndexNode *pNode)
{
    //delete from map + free node memory
    void *avl_item = avl_delete(pTree, pNode);
    BlockIndexNode_Delete(avl_item, pTree->avl_param);
}

int BlockIndexNode_compare(const void *avl_a, const void *avl_b, void *avl_param)
{
    const BlockIndexNode *pNode1 = (BlockIndexNode*)avl_a;
    const BlockIndexNode *pNode2 = (BlockIndexNode*)avl_b;
    
    if (pNode1->m_key < pNode2->m_key)
        return -1;
    else if (pNode1->m_key > pNode2->m_key)
        return +1;
    else
        return 0;
}

BlockManager::BlockManager(uint8 *pBlocks, const uint16 &blockCount) : m_pBlockIndex(avl_create(&BlockIndexNode_compare, NULL, NULL)),
                                                                       m_pBlocks(pBlocks),
                                                                       m_blockCount(blockCount)
{
    //build index map
    uint8 *pBlock;
    //init traverser
    Record rRecord;
    RecordTraverser rT;
    Block::RecordTraverserInit(&rT, pBlock);
    
    //build index map if there is any buffer
    if(m_pBlocks != NULL)
    {
        for(uint16 i = 0;i < m_blockCount;++i)
        {
            pBlock = GetBlock(i);
            //init RDF traverser and build index map for block
            Block::RecordTraverserInit(&rT, pBlock);
            while(Block::RecordTraverserNext(&rT, &rRecord))
            {
                avl_insert(m_pBlockIndex, BlockIndexNode_new(rRecord.m_pRDF->m_key, i));
            }
        }
    }
    else
    {
        //create initial block
        ReallocBlocks();
    }
}

BlockManager::~BlockManager()
{
    //delete blocks
    scalable_aligned_free(m_pBlocks);
    m_pBlocks = NULL;
    
    //clear indexes
    avl_destroy(m_pBlockIndex, &BlockIndexNode_Delete);
    m_pBlockIndex = NULL;
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

E_BMS BlockManager::WriteRecord(const uint64 &recordKey, const uint8 *pRecord, uint16 recordSize)
{
    //size check
    if(recordSize > MAX_RECORD_SIZE)
    {
        return eBMS_FailRecordTooBig;
    }
    
    //declarations
    E_BMS writeStatus = eBMS_Ok;
    E_BLS blockStatus;
    uint8 *pBlock;
    ByteBuffer rOut;
    
    //PHASE 0 --> compression
    //try to compress
    if(recordSize > g_RecordSizeForCompression)
    {
        int cStatus = CommonFunctions::compressGzip(g_GzipCompressionLevel, pRecord, recordSize, rOut);
        if(cStatus == Z_OK)
        {
            if(rOut.size() < recordSize)
            {
                pRecord = (uint8*)rOut.contents();
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
    BlockIndexNode rNodeToFind = { recordKey };
    BlockIndexNode *pFindNode = (BlockIndexNode*)avl_find(m_pBlockIndex, &rNodeToFind);
    if(pFindNode != NULL)
    {
        //get block
        pBlock = GetBlock(pFindNode->m_blockNumber);
        //update
        blockStatus = Block::UpdateRecord(pBlock, recordKey, pRecord, recordSize);
        if(blockStatus == eBLS_OK)
        {
            return eBMS_Ok;
        }
        
        //delete key will be on another block
        BlockIndexNode_Delete_RemoveFromTree(m_pBlockIndex, pFindNode);
    }
    
    //PHASE 2 --> check record limit
    //block limit - start to rewrite from begin (record key is timestamp)
    size_t recordLimit = static_cast<size_t>(g_RecordLimit);
    if(g_EnableRecordLimit && m_pBlockIndex->avl_count >= recordLimit)
    {
        int removedRecords = 0;
        //limit can be lowed so delete all records
        while(m_pBlockIndex->avl_count >= recordLimit)
        {
            //init traverser
            struct avl_traverser rAvl_traverser;
            avl_t_init(&rAvl_traverser, m_pBlockIndex);
            
            //delete 1st one (the oldest)
            BlockIndexNode *pFirstNode = (BlockIndexNode*)avl_t_first(&rAvl_traverser, m_pBlockIndex);
            if(pFirstNode == NULL)
                break;
            
            //DO NOT ADD OLDER DATA
            //check if new data are not the older one
            if(recordKey < pFirstNode->m_key)
            {
                writeStatus = (E_BMS)(writeStatus | (eBMS_RecordCountLimit | eBMS_OldRecord));
                return writeStatus;
            }
            
            //update counter
            ++removedRecords;
            
            //delete record from block + index map
            DeleteRecord(pFirstNode);
            
            //set return status
            writeStatus = (E_BMS)(writeStatus | eBMS_RecordCountLimit);
        }
        
        //request defragment
        if(removedRecords >= g_DefragAfterRecordDelete)
        {
            writeStatus = (E_BMS)(writeStatus | eBMS_NeedDefrag);
        }
    }
    
    //PHASE 3 --> try to write
    //find empty space in blocks
    for(uint16 i = 0;i < m_blockCount;++i)
    {
        pBlock = GetBlock(i);
        blockStatus = Block::WriteRecord(pBlock, recordKey, pRecord, recordSize);
        if(blockStatus == eBLS_OK)
        {
            avl_insert(m_pBlockIndex, BlockIndexNode_new(recordKey, i));
            break;
        }
        else if(blockStatus == eBLS_NO_SPACE_FOR_NEW_DATA && (i == (m_blockCount - 1)))
        {
            //relloc block + 1
            ReallocBlocks();
            //set status
            writeStatus = (E_BMS)(writeStatus | eBMS_ReallocatedBlocks);
        }
    }
    return writeStatus;
}

void BlockManager::ReadRecord(const uint64 &recordKey, ByteBuffer &rData)
{
    //find block number where is block
    BlockIndexNode rNodeToFind = { recordKey };
    BlockIndexNode *pNode = (BlockIndexNode*)avl_find(m_pBlockIndex, &rNodeToFind);
    if(pNode != NULL)
    {
        uint8 *pBlock = GetBlock(pNode->m_blockNumber);
        Block::GetRecord(pBlock, recordKey, rData);
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

void BlockManager::DeleteRecordByKey(const uint64 &recordKey)
{
    BlockIndexNode rNodeToFind = { recordKey };
    BlockIndexNode *pNode = (BlockIndexNode*)avl_find(m_pBlockIndex, &rNodeToFind);
    if(pNode != NULL)
    {
        DeleteRecord(pNode);
    }
}

void BlockManager::DeleteRecord(BlockIndexNode *pBlockIndexNode)
{    
    uint8 *pBlock;
    E_BLS deleteStatus;
    
    //get block + delete record from block
    pBlock = GetBlock(pBlockIndexNode->m_blockNumber);
    deleteStatus = Block::DeleteRecordByKey(pBlock, pBlockIndexNode->m_key);
    if(deleteStatus != eBLS_OK)
    {
        Log.Warning(__FUNCTION__, "Trying to delete non-existant key: " I64FMTD " in block number: %u", pBlockIndexNode->m_key, pBlockIndexNode->m_blockNumber);
    }
    //delete from tree + delete Node from memory
    BlockIndexNode_Delete_RemoveFromTree(m_pBlockIndex, pBlockIndexNode);
}

void BlockManager::GetAllRecordKeys(ByteBuffer &rY)
{
    if(m_pBlockIndex->avl_count != 0)
    {
        //prealloc
        rY.reserve(m_pBlockIndex->avl_count * sizeof(uint64));
        
        //init traverser
        struct avl_traverser rAvl_traverser;
        avl_t_init(&rAvl_traverser, m_pBlockIndex);
        
        for(;;)
        {
            BlockIndexNode *pNode = (BlockIndexNode*)avl_t_next(&rAvl_traverser);
            if(pNode == NULL)
                break;
            
            //add to buffer
            rY << uint64(pNode->m_key);
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
    
    //delete blocks
    scalable_aligned_free(m_pBlocks);
    m_pBlocks = NULL;
    
    //set block count to 0 !!!
    m_blockCount = 0;
    
    //clear indexes + create empty
    avl_destroy(m_pBlockIndex, &BlockIndexNode_Delete);
    m_pBlockIndex = avl_create(&BlockIndexNode_compare, NULL, NULL);
    
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
    ++g_NumOfRecordDeframentations;
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





































