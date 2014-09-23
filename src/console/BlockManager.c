//
//  BlockManager.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/24/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "tbb/scalable_allocator.h"
#include "../shared/zlib/zlib.h"
#include "../shared/clib/CCommon.h"
#include "../shared/clib/Log/CLog.h"
#include "../shared/clib/Crc32/crc32_sse.h"
#include "CfgDefines.h"
#include "Block.h"
#include "BlockManager.h"

//Node for AVL tree
typedef struct _Node
{
    uint64  m_key;
    uint16  m_blockNumber;
} blidxnode;

void *my_avl_malloc(struct libavl_allocator *self, size_t libavl_size)
{
    void *p = scalable_malloc(libavl_size);
    return p;
}

void my_avl_free(struct libavl_allocator *self, void *libavl_block)
{
    scalable_free(libavl_block);
}

//custom allocator
struct libavl_allocator my_avl_allocator =
{
    my_avl_malloc,
    my_avl_free
};

blidxnode *blidxnode_create(uint64 key, uint16 blockNumber)
{
    blidxnode *p = scalable_malloc(sizeof(blidxnode));
    p->m_key = key;
    p->m_blockNumber = blockNumber;
    return p;
}

void blidxnode_destroy(void *avl_item, void *avl_param)
{
    scalable_free(avl_item);
}

int blockindex_cmp(const void *avl_a, const void *avl_b, void *avl_param)
{
    const blidxnode *a = (blidxnode*)avl_a;
    const blidxnode *b = (blidxnode*)avl_b;
    
    if(a->m_key < b->m_key)
        return +1;
    else if(a->m_key > b->m_key)
        return -1;
    else
        return 0;
}

void blman_delete_record_by_node(blman *self, blidxnode *node)
{
    uint8 *block = blman_get_block(self, node->m_blockNumber);
    //
    E_BLS deleteStatus = Block_DeleteRecordByKey(block, node->m_key);
    if(deleteStatus != eBLS_OK)
    {
        Log_Warning(__FUNCTION__, "Trying to delete non-existant key: " I64FMTD " in block number: %u", node->m_key, node->m_blockNumber);
    }
    avl_delete(self->blockIndex, node);
    blidxnode_destroy(node, self->blockIndex->avl_param);
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
    self->blockIndex = avl_create(&blockindex_cmp, NULL, &my_avl_allocator);
}

blman *blman_create(uint8 *blocks, uint16 blockCount)
{
    blman *p = scalable_malloc(sizeof(blman));
    if(p == NULL)
        return NULL;
    
    //create AVL tree for block index
    p->blockIndex = avl_create(&blockindex_cmp, NULL, &my_avl_allocator);
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
    scalable_free(self);
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
    uint8 rCompressedRecord[BLOCK_SIZE];
    
    //PHASE 0 --> compression
    //try to compress
    if(recordsize > g_cfg.RecordSizeForCompression)
    {
        size_t outSize;
        int cStatus = CCommon_compressGzip_Buffer(g_cfg.GzipCompressionLevel, record, recordsize, rCompressedRecord, sizeof(rCompressedRecord), &outSize);
        if(cStatus == Z_OK && outSize < recordsize)
        {
            //replace record ptr and set new record size
            record = rCompressedRecord;
            recordsize = (uint16)outSize;
            g_stats.NumOfRecordCompressions++;
        }
        else
        {
            Log_Warning(__FUNCTION__, "Compression generated bigger size. Source size: %u, new size: " I64FMTD, recordsize, outSize);
        }
    }
    
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
        blidxnode_destroy(pNodeFind, self->blockIndex->avl_param);
    }
    
    //PHASE 2 --> check record limit
    //block limit - start to rewrite from begin (record key is timestamp)
    if(g_cfg.EnableRecordLimit && self->blockIndex->avl_count >= g_cfg.RecordLimit)
    {
        int removedRecords = 0;
        //limit can be lowed so delete all records
        while(self->blockIndex->avl_count >= g_cfg.RecordLimit)
        {
            struct avl_traverser rTraverser;
            avl_t_init(&rTraverser, self->blockIndex);
            //delete 1st one (the oldest)
            blidxnode *node = avl_t_first(&rTraverser, self->blockIndex);
            if(node != NULL)
            {
                //DO NOT ADD OLDER DATA
                //check if new data are not the older one
                if(recordkey < node->m_key)
                {
                    retStatus |= (eBMS_RecordCountLimit | eBMS_OldRecord);
                    return retStatus;
                }
                
                //update counter
                ++removedRecords;
                //remove record from block
                blman_delete_record_by_node(self, node);
                //set return status
                retStatus |= eBMS_RecordCountLimit;
            }
        }

        //request defragment
        if(removedRecords >= g_cfg.DefragAfterRecordDelete)
        {
            retStatus |= eBMS_NeedDefrag;
        }
    }

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

void blman_delete_record_by_key(blman *self, uint64 recordkey)
{
    blidxnode rNodeSearch = { recordkey, 0 };
    blidxnode *node = avl_find(self->blockIndex, &rNodeSearch);
    if(node != NULL)
    {
        blman_delete_record_by_node(self, node);
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
        Log_Warning(__FUNCTION__, "numOfrecords == 0");
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
    g_stats.NumOfRecordDeframentations++;
    
    //delete bytebuffer
    bbuff_destroy(records);
}

uint32 blman_get_blocks_crc32(blman *self)
{
    //C99 - VLA
#ifndef WIN32
	uint32 crc32Array[self->blockCount];
#else
	uint32 *crc32Array = alloca(sizeof(uint32) * self->blockCount);
#endif

	for (uint16 i = 0; i < self->blockCount; ++i)
	{
		uint8 *pBlock = blman_get_block(self, i);
		crc32Array[i] = crc32_compute(pBlock, BLOCK_SIZE);
	}

	uint32 crc32 = crc32_compute((BYTE*)crc32Array, self->blockCount * sizeof(uint32));
	return crc32;
}

uint8 *blman_get_block(blman *self, uint16 blocknum)
{
    return (self->blocks + (BLOCK_SIZE * blocknum));
}

uint64 blman_get_memory_usage(blman *self)
{
    uint64 memoryUsage = 0;
    //index
    memoryUsage += (self->blockIndex->avl_count * (sizeof(struct avl_node) + sizeof(blidxnode)));
    //blocks
    memoryUsage += (self->blockCount * BLOCK_SIZE);
    //self
    memoryUsage += sizeof(blman);
    
    return memoryUsage;
}


































