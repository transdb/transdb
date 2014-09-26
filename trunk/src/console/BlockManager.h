//
//  BlockManager.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/24/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#ifndef TransDB_BlockManager_h
#define TransDB_BlockManager_h

#ifdef __cplusplus
extern "C" {
#endif

#include "../shared/clib/Buffers/CByteBuffer.h"
#include "../shared/clib/Containers/avl.h"
    
typedef enum BlockManagerStatus
{
    eBMS_Ok                     = 1,
    eBMS_ReallocatedBlocks      = 2,
    eBMS_FailRecordTooBig       = 4,
    eBMS_GeneralFail            = 8,
    eBMS_RecordCountLimit       = 16,
    eBMS_OldRecord              = 32,
    eBMS_NeedDefrag             = 64
} E_BMS;

typedef struct BlockManager
{
    struct avl_table    *blockIndex;
    uint8               *blocks;
    uint16              blockCount;
} blman;

/**
 */
blman *blman_create(uint8 *blocks, uint16 blockCount);

/**
 */
void blman_destroy(blman *self);

/**
 */
uint32 blman_write_record(blman *self, uint64 recordkey, const uint8 *record, uint16 recordsize);

/**
 */
void blman_read_record(blman *self, uint64 recordkey, bbuff *record);

/**
 */
void blman_read_records(blman *self, bbuff *records);

/**
 */
void blman_delete_record_by_key(blman *self, uint64 recordkey);
    
/**
 */
void blman_get_all_record_keys(blman *self, bbuff *recordkeys);

/**
 */
void blman_clear_dirty_flags(blman *self);

/**
 */
void blman_defragment_data(blman *self);

/**
 */
uint32 blman_get_blocks_crc32(blman *self);

/**
 */
uint8 *blman_get_block(blman *self, uint16 blocknum);

/**
 */
uint64 blman_get_memory_usage(blman *self);
    
#ifdef __cplusplus
}
#endif
    
#endif
