//
//  BlockManager.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/24/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "tbb/scalable_allocator.h"
#include "Block.h"
#include "BlockManager.h"

//Node for AVL tree
typedef struct _Node
{
    uint64  m_key;
    uint16  m_blockNumber;
} blidxnode;

blidxnode *blidxnode_create(uint64 key, uint16 blockNumber)
{
    blidxnode *p = malloc(sizeof(blidxnode));
    p->m_key = key;
    p->m_blockNumber = blockNumber;
    return p;
}

void blidxnode_destroy(void *avl_item, void *avl_param)
{
    free(avl_item);
}

int blockindex_cmp(const void *avl_a, const void *avl_b, void *avl_param)
{
    const blidxnode *a = (blidxnode*)avl_a;
    const blidxnode *b = (blidxnode*)avl_b;
    
    if(a->m_key < b->m_key)
        return -1;
    else if(a->m_key > b->m_key)
        return +1;
    else
        return 0;
}

void blman_realloc_blocks(blman *self)
{
    //inc block count
    self->blockCount += 1;

    //realloc block array
    void *pNewBlocks = scalable_aligned_realloc(self->blocks, self->blockCount * BLOCK_SIZE, 512);
    assert(pNewBlocks);
    
    //set new pointer
    self->blocks = (uint8*)pNewBlocks;

    //get new block
    uint8 *pNewBlock = blman_get_block(self, self->blockCount - 1);
    memset(pNewBlock, 0, BLOCK_SIZE);

    //init new block
    Block_InitBlock(pNewBlock);
}

void blman_dealloc_blocks(blman *self)
{
    //dealloc blocks
    scalable_aligned_free(self->blocks);
    self->blocks = NULL;

    //set block count to 0
    self->blockCount = 0;

    //clear indexes
    avl_destroy(self->blockIndex, &blidxnode_destroy);
    self->blockIndex = avl_create(&blockindex_cmp, NULL, NULL);
}

blman *blman_create(uint8 *blocks, uint16 blockCount)
{
    blman *p = calloc(1, sizeof(blman));
    if(p == NULL)
        return NULL;
    
    //create AVL tree for block index
    p->blockIndex = avl_create(&blockindex_cmp, NULL, NULL);
    p->blocks = blocks;
    p->blockCount = blockCount;
    
    //build index map if there is any block else create new empty block
    if(p->blocks != NULL)
    {
        //build index map
        for(uint16 i = 0;i < p->blockCount;++i)
        {
            uint8 *block = blman_get_block(p, i);
            CIDF *pCIDF = Block_GetCIDF(block);
            uint16 position = CIDFOffset - sizeof(RDF);
            uint16 endOfRDFArea = pCIDF->m_location + pCIDF->m_amoutOfFreeSpace;
            for(;;)
            {
                //end of RDF area
                if(position < endOfRDFArea)
                    break;

                //get RDF
                RDF *pRDF = (RDF*)(block + position);
                position -= sizeof(RDF);
                //create index node and inter to block index map
                blidxnode *node = blidxnode_create(pRDF->m_key, i);
                avl_insert(p->blockIndex, node);
            }
        }
    }
    else
    {
        blman_realloc_blocks(p);
    }
    
    return p;
}

void blman_destroy(blman *self)
{
    avl_destroy(self->blockIndex, &blidxnode_destroy);
    scalable_aligned_free(self->blocks);
    free(self);
}

uint32 blman_write_record(blman *self, uint64 recordkey, const uint8 *record, uint16 recordsize)
{
    //size check
    if(recordsize > MAX_RECORD_SIZE)
    {
        return eBMS_FailRecordTooBig;
    }

    //variables
    uint32 retStatus = eBMS_Ok;
//    ByteBuffer rOut;
//
//    //PHASE 0 --> compression
//    //try to compress
//    if(recordSize > g_RecordSizeForCompression)
//    {
//        int cStatus = CommonFunctions::compressGzip(g_GzipCompressionLevel, pRecord, recordSize, rOut, g_ZlibBufferSize);
//        if(cStatus == Z_OK)
//        {
//            if(rOut.size() < recordSize)
//            {
//                pRecord = rOut.contents();
//                recordSize = (uint16)rOut.size();
//                ++g_NumOfRecordCompressions;
//            }
//            else
//            {
//                Log.Warning(__FUNCTION__, "Compression generated bigger size. Source size: %u, new size: " I64FMTD, recordSize, rOut.size());
//            }
//        }
//    }

    
    //PHASE 1 --> try to update
    //try to update record first
    blidxnode rNodeSearch = { recordkey, 0 };
    blidxnode *pNodeFind = avl_find(self->blockIndex, &rNodeSearch);
    if(pNodeFind != NULL)
    {
        uint8 *block = blman_get_block(self, pNodeFind->m_blockNumber);
        //update
        E_BLS updateStatus = Block_UpdateRecord(block, recordkey, record, recordsize);
        if(updateStatus == eBLS_OK)
        {
            return retStatus;
        }
        
        //delete key, record will be on another block
        avl_delete(self->blockIndex, pNodeFind);
        blidxnode_destroy(pNodeFind, NULL);
    }
    
//    //PHASE 2 --> check record limit
//    //block limit - start to rewrite from begin (record key is timestamp)
//    BlocksIndex::size_type recordLimit = static_cast<BlocksIndex::size_type>(g_RecordLimit);
//    if(g_EnableRecordLimit && m_rBlockIndex.size() >= recordLimit)
//    {
//        int removedRecords = 0;
//        //limit can be lowed so delete all records
//        while(m_rBlockIndex.size() >= recordLimit)
//        {
//            //delete 1st one (the oldest)
//            itr = m_rBlockIndex.begin();
//
//            //DO NOT ADD OLDER DATA
//            //check if new data are not the older one
//            uint64 recordKeyTmp = itr->first;
//            if(recordkey < recordKeyTmp)
//            {
//                retStatus |= (eBMS_RecordCountLimit | eBMS_OldRecord);
//                return retStatus;
//            }
//
//            //update counter
//            ++removedRecords;
//            //this invalidate iterator
//            DeleteRecord(itr);
//            //set return status
//            retStatus |= eBMS_RecordCountLimit;
//        }
//
//        //request defragment
//        if(removedRecords >= g_DefragAfterRecordDelete)
//        {
//            retStatus |= eBMS_NeedDefrag;
//        }
//    }

    //PHASE 3 --> try to write
    //find empty space in blocks
    //search from end to begin -> mostly there appends to the end
    uint16 blockNum = self->blockCount;
    for(;;)
    {
        --blockNum;
        //get block and try to write
        uint8 *block = blman_get_block(self, blockNum);
        //
        E_BLS writeStatus = Block_WriteRecord(block, recordkey, record, recordsize);
        if(writeStatus == eBLS_OK)
        {
            blidxnode *node = blidxnode_create(recordkey, blockNum);
            avl_insert(self->blockIndex, node);
            break;
        }
        else if(blockNum == 0 && writeStatus == eBLS_NO_SPACE_FOR_NEW_DATA)
        {
            //realloc blocks + 1 -> modify m_blockCount
            blman_realloc_blocks(self);
            //set status
            retStatus |= eBMS_ReallocatedBlocks;
            //reset control variable
            blockNum = self->blockCount;
        }
    }
    
    return retStatus;
}

void blman_read_record(blman *self, uint64 recordkey, bbuff *record)
{
    blidxnode rNodeSearch = { recordkey, 0 };
    blidxnode *node = avl_find(self->blockIndex, &rNodeSearch);
    if(node != NULL)
    {
        uint8 *block = blman_get_block(self, node->m_blockNumber);
        Block_GetRecord(block, recordkey, record);
    }
}

void blman_read_records(blman *self, bbuff *records)
{
    //pre-realloc buffer
    //key|recordSize|record|....Nx
    bbuff_reserve(records, BLOCK_SIZE * self->blockCount);

    for(uint16 i = 0;i < self->blockCount;++i)
    {
        uint8 *block = blman_get_block(self, i);
        Block_GetRecords(block, records);
    }
}

void blman_delete_record(blman *self, uint64 recordkey)
{
    blidxnode rNodeSearch = { recordkey, 0 };
    blidxnode *node = avl_find(self->blockIndex, &rNodeSearch);
    if(node != NULL)
    {
        uint8 *block = blman_get_block(self, node->m_blockNumber);
        //
        E_BLS deleteStatus = Block_DeleteRecordByKey(block, recordkey);
        if(deleteStatus != eBLS_OK)
        {
            //TODO: CLog
            //Log.Warning(__FUNCTION__, "Trying to delete non-existant key: " I64FMTD " in block number: %u", itr->first, itr->second);
        }
        avl_delete(self->blockIndex, node);
    }
}

void blman_get_all_record_keys(blman *self, bbuff *recordKeys)
{
    if(self->blockIndex->avl_count != 0)
    {
        //prealloc
        bbuff_reserve(recordKeys, self->blockIndex->avl_count * sizeof(uint64));
        //add to buffer
        RDF *pRDF;
        struct avl_traverser rTraverser;
        avl_t_init(&rTraverser, self->blockIndex);
        //
        while((pRDF = avl_t_next(&rTraverser)) != NULL)
        {
            bbuff_append(recordKeys, &pRDF->m_key, sizeof(pRDF->m_key));
        }
    }
}

void blman_clear_dirty_flags(blman *self)
{
    for(uint16 i = 0;i < self->blockCount;++i)
    {
        uint8 *pBlock = blman_get_block(self, i);
        CIDF *pCIDF = Block_GetCIDF(pBlock);
        pCIDF->m_flags &= ~eBLF_Dirty;
    }
}

void blman_defragment_data(blman *self)
{
    //TODO: sort for best fit

    //save record count
    size_t numOfrecords = self->blockIndex->avl_count;
    if(numOfrecords == 0)
    {
//        Log.Warning(__FUNCTION__, "numOfrecords == 0");
        return;
    }
    //create bytebuffer
    bbuff *records = bbuff_create();

    //read all data - key|recordSize|record|....Nx
    blman_read_records(self, records);

    //deallloc blocks
    blman_dealloc_blocks(self);
    //create empty one
    blman_realloc_blocks(self);

    //insert
    if(numOfrecords)
    {
        while(records->rpos < records->size)
        {
            uint64 recordkey;
            uint16 recordSize;
            uint8 arData[BLOCK_SIZE];
            //get record
            bbuff_read(records, &recordkey, sizeof(recordkey));
            bbuff_read(records, &recordSize, sizeof(recordSize));
            bbuff_read(records, &arData, recordSize);

            //insert - ignore return value
            (void)blman_write_record(self, recordkey, arData, recordSize);
        }
    }
    
    //update counter
//    g_NumOfRecordDeframentations++;
    
    //delete bytebuffer
    bbuff_destroy(records);
}

uint32 blman_get_blocks_crc32(blman *self)
{
//	size_t crc32ArraySize = sizeof(uint32) * self->blockCount;
//	uint32 *pCrc32Array = (uint32*)alloca(crc32ArraySize);
//
//	for (uint16 i = 0; i < self->blockCount; ++i)
//	{
//		uint8 *pBlock = blman_get_block(self, i);
//		pCrc32Array[i] = g_CRC32->ComputeCRC32(pBlock, BLOCK_SIZE);
//	}
//
//	uint32 crc32 = g_CRC32->ComputeCRC32((BYTE*)pCrc32Array, crc32ArraySize);
//	return crc32;
    
    return 0;
}

uint8 *blman_get_block(blman *self, uint16 blocknum)
{
    return (self->blocks + (BLOCK_SIZE * blocknum));
}

//BlockManager::BlockManager() : m_pBlocks(NULL), m_blockCount(0)
//{
//    //create initial block
//    ReallocBlocks();
//}
//
//BlockManager::BlockManager(uint8 *pBlocks, uint16 blockCount) : m_pBlocks(pBlocks), m_blockCount(blockCount)
//{
//    //build index map
//    for(uint16 i = 0;i < m_blockCount;++i)
//    {
//        uint8 *pBlock = GetBlock(i);
//        CIDF *pCIDF = Block_GetCIDF(pBlock);
//        uint16 position = CIDFOffset - sizeof(RDF);
//        uint16 endOfRDFArea = pCIDF->m_location + pCIDF->m_amoutOfFreeSpace;
//        for(;;)
//        {
//            //end of RDF area
//            if(position < endOfRDFArea)
//                break;
//            
//            RDF *pRDF = (RDF*)(pBlock + position);
//            m_rBlockIndex.insert(BlocksIndex::value_type(pRDF->m_key, i));
//            position -= sizeof(RDF);
//        }
//    }
//}
//
//BlockManager::~BlockManager()
//{
//    DeallocBlocks();
//}
//
//void BlockManager::DeallocBlocks()
//{
//    //dealloc blocks
//    scalable_aligned_free((void*)m_pBlocks);
//    m_pBlocks = NULL;
//    
//    //set block count to 0
//    m_blockCount = 0;
//    
//    //clear indexes
//    m_rBlockIndex.clear();
//}
//
//void BlockManager::ReallocBlocks()
//{
//    //inc block count
//    m_blockCount += 1;
//    
//    //realloc block array
//    void *pNewBlocks = scalable_aligned_realloc(m_pBlocks, m_blockCount * BLOCK_SIZE, g_DataFileMallocAlignment);
//    if(pNewBlocks == NULL)
//    {
//        throw std::bad_alloc();
//    }
//    //set new pointer
//    m_pBlocks = (uint8*)pNewBlocks;
//    
//    //get new block
//    uint8 *pNewBlock = GetBlock(m_blockCount - 1);
//    memset(pNewBlock, 0, BLOCK_SIZE);
//    
//    //init new block
//    Block_InitBlock(pNewBlock);
//}
//
//uint32 BlockManager::WriteRecord(uint64 recordkey, const uint8 *pRecord, uint16 recordSize)
//{
//    //size check
//    if(recordSize > MAX_RECORD_SIZE)
//        return eBMS_FailRecordTooBig;
//    
//    //variables
//    uint32 retStatus = eBMS_Ok;
//    ByteBuffer rOut;
//    
//    //PHASE 0 --> compression   
//    //try to compress
//    if(recordSize > g_RecordSizeForCompression)
//    {
//        int cStatus = CommonFunctions::compressGzip(g_GzipCompressionLevel, pRecord, recordSize, rOut, g_ZlibBufferSize);
//        if(cStatus == Z_OK)
//        {
//            if(rOut.size() < recordSize)
//            {
//                pRecord = rOut.contents();
//                recordSize = (uint16)rOut.size();
//                ++g_NumOfRecordCompressions;
//            }
//            else
//            {
//                Log.Warning(__FUNCTION__, "Compression generated bigger size. Source size: %u, new size: " I64FMTD, recordSize, rOut.size());
//            }
//        }
//    }
//    
//    //PHASE 1 --> try to update
//    //try to update record first
//    BlocksIndex::iterator itr = m_rBlockIndex.find(recordkey);
//    if(itr != m_rBlockIndex.end())
//    {
//        //get block
//        uint8 *pBlock = GetBlock(itr->second);
//        //update
//        uint16 RDFPosition;
//        uint16 recordPosition;
//        E_BLS updateStatus = Block_UpdateRecord(pBlock, recordkey, pRecord, recordSize, NULL, &RDFPosition, &recordPosition);
//        if(updateStatus == eBLS_OK)
//        {
//            return retStatus;
//        }
//        
//        //delete key will be on another block
//        m_rBlockIndex.erase(itr);
//    }
//    
//    //PHASE 2 --> check record limit
//    //block limit - start to rewrite from begin (record key is timestamp)
//    BlocksIndex::size_type recordLimit = static_cast<BlocksIndex::size_type>(g_RecordLimit);
//    if(g_EnableRecordLimit && m_rBlockIndex.size() >= recordLimit)
//    {
//        int removedRecords = 0;
//        //limit can be lowed so delete all records
//        while(m_rBlockIndex.size() >= recordLimit)
//        {
//            //delete 1st one (the oldest)
//            itr = m_rBlockIndex.begin();
//            
//            //DO NOT ADD OLDER DATA
//            //check if new data are not the older one
//            uint64 recordKeyTmp = itr->first;
//            if(recordkey < recordKeyTmp)
//            {
//                retStatus |= (eBMS_RecordCountLimit | eBMS_OldRecord);
//                return retStatus;
//            }
//            
//            //update counter
//            ++removedRecords;
//            //this invalidate iterator
//            DeleteRecord(itr);
//            //set return status
//            retStatus |= eBMS_RecordCountLimit;
//        }
//        
//        //request defragment
//        if(removedRecords >= g_DefragAfterRecordDelete)
//        {
//            retStatus |= eBMS_NeedDefrag;
//        }
//    }
//    
//    //PHASE 3 --> try to write
//    //find empty space in blocks
//    //search from end to begin -> mostly there appends to the end
//    uint16 blockNum = m_blockCount;
//    for(;;)
//    {
//        --blockNum;
//        //get block and try to write
//        uint8 *pBlock = GetBlock(blockNum);
//        E_BLS writeStatus = Block_WriteRecord(pBlock, recordkey, pRecord, recordSize);
//        if(writeStatus == eBLS_OK)
//        {
//            m_rBlockIndex.insert(BlocksIndex::value_type(recordkey, blockNum));
//            break;
//        }
//        else if(writeStatus == eBLS_NO_SPACE_FOR_NEW_DATA && blockNum == 0)
//        {
//            //realloc blocks + 1 -> modify m_blockCount
//            ReallocBlocks();
//            //set status
//            retStatus |= eBMS_ReallocatedBlocks;
//            //reset control variable
//            blockNum = m_blockCount;
//        }
//    }
//    
//    return retStatus;
//}
//
//void BlockManager::ReadRecord(uint64 recordkey, bbuff *pData)
//{
//    BlocksIndex::iterator itr = m_rBlockIndex.find(recordkey);
//    if(itr != m_rBlockIndex.end())
//    {
//        uint8 *pBlock = GetBlock(itr->second);
//        Block_GetRecord(pBlock, recordkey, pData);
//    }
//}
//
//void BlockManager::ReadRecords(bbuff *pData)
//{
//    //pre-realloc buffer
//    //key|recordSize|record|....Nx
//    bbuff_reserve(pData, BLOCK_SIZE * m_blockCount);
//    
//    for(uint16 i = 0;i < m_blockCount;++i)
//    {
//        uint8 *pBlock = GetBlock(i);
//        Block_GetRecords(pBlock, pData);
//    }
//}
//
//void BlockManager::DeleteRecord(uint64 recordkey)
//{
//    BlocksIndex::iterator itr = m_rBlockIndex.find(recordkey);
//    if(itr != m_rBlockIndex.end())
//    {
//        DeleteRecord(itr);
//    }
//}
//
//void BlockManager::DeleteRecord(BlocksIndex::iterator &itr)
//{
//    uint8 *pBlock;
//    uint16 RDFPosition;
//    uint16 recordPosition;
//    E_BLS deleteStatus;
//    
//    //delete
//    pBlock = GetBlock(itr->second);
//    deleteStatus = Block_DeleteRecord(pBlock, itr->first, NULL, &RDFPosition, &recordPosition);
//    if(deleteStatus != eBLS_OK)
//    {
//        Log.Warning(__FUNCTION__, "Trying to delete non-existant key: " I64FMTD " in block number: %u", itr->first, itr->second);
//    }
//    m_rBlockIndex.erase(itr);
//}
//
//void BlockManager::GetAllRecordKeys(bbuff *pY)
//{
//    if(!m_rBlockIndex.empty())
//    {
//        //prealloc
//        bbuff_reserve(pY, m_rBlockIndex.size() * sizeof(uint64));
//        //add to buffer
//        for(BlocksIndex::iterator itr = m_rBlockIndex.begin();itr != m_rBlockIndex.end();++itr)
//        {
//            bbuff_append(pY, &itr->first, sizeof(itr->first));
//        }
//    }
//}
//
//void BlockManager::ClearDirtyFlags()
//{
//    for(uint16 i = 0;i < m_blockCount;++i)
//    {
//        uint8 *pBlock = GetBlock(i);
//        CIDF *pCIDF = Block_GetCIDF(pBlock);
//        pCIDF->m_flags &= ~eBLF_Dirty;
//    }
//}
//
//void BlockManager::DefragmentData()
//{
//    //TODO: sort for best fit
//    
//    //save record count
//    size_t numOfrecords = numOfRecords();
//    if(numOfrecords == 0)
//    {
//        Log.Warning(__FUNCTION__, "numOfrecords == 0");
//        return;
//    }
//    //create bytebuffer
//    CByteBuffer *pData = bbuff_create();
//    
//    //read all data - key|recordSize|record|....Nx
//    ReadRecords(pData);
//    
//    //deallloc blocks
//    DeallocBlocks();
//    //create empty one
//    ReallocBlocks();
//    
//    //insert
//    if(numOfrecords)
//    {
//        while(pData->rpos < pData->size)
//        {
//            uint64 recordkey;
//            uint16 recordSize;
//            uint8 arData[BLOCK_SIZE];
//            //get record
//            bbuff_read(pData, &recordkey, sizeof(recordkey));
//            bbuff_read(pData, &recordSize, sizeof(recordSize));
//            bbuff_read(pData, &arData, recordSize);
//            
//            //insert - ignore return value
//            (void)WriteRecord(recordkey, arData, recordSize);
//        }
//    }
//    
//    //update counter
//    g_NumOfRecordDeframentations++;
//    
//    //delete bytebuffer
//    bbuff_destroy(pData);
//}
//
//uint32 BlockManager::GetBlocksCrc32() const
//{
//	size_t crc32ArraySize = sizeof(uint32) * m_blockCount;
//	uint32 *pCrc32Array = (uint32*)alloca(crc32ArraySize);
//
//	for (uint16 i = 0; i < m_blockCount; ++i)
//	{
//		uint8 *pBlock = GetBlock(i);
//		pCrc32Array[i] = g_CRC32->ComputeCRC32(pBlock, BLOCK_SIZE);
//	}
//
//	uint32 crc32 = g_CRC32->ComputeCRC32((BYTE*)pCrc32Array, crc32ArraySize);
//	return crc32;
//}
//




































