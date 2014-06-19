//
//  BlockManager.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/24/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#ifndef TransDB_BlockManager_h
#define TransDB_BlockManager_h

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

typedef struct _BlockIndexNode
{
    uint64  m_key;
    uint16  m_blockNumber;
} BlockIndexNode;

//for Y
class BlockManager
{
public:
    explicit BlockManager(uint8 *pBlocks, const uint16 &blockCount);
    ~BlockManager();
    
    E_BMS WriteRecord(const uint64 &recordKey, const uint8 *pRecord, uint16 recordSize);
    void ReadRecord(const uint64 &recordKey, ByteBuffer &rData);
    void ReadRecords(ByteBuffer &rData);
    void DeleteRecordByKey(const uint64 &recordKey);
    void GetAllRecordKeys(ByteBuffer &rY);
    void ClearDirtyFlags();
    void DefragmentData();
    NOINLINE uint32 GetBlocksCrc32();
    
    INLINE uint16 numOfBlocks() const               { return m_blockCount; }
    INLINE uint8 *GetBlock(const uint16 &blockNum)  { return m_pBlocks + (BLOCK_SIZE * blockNum); }
    INLINE size_t numOfRecords() const              { return m_pBlockIndex->avl_count; }
    
private:
	//disable copy constructor and assign
	DISALLOW_COPY_AND_ASSIGN(BlockManager);

    void ReallocBlocks();
    void DeleteRecord(BlockIndexNode *pBlockIndexNode);
    
    //declarations
    avl_table       *m_pBlockIndex;
    uint8           *m_pBlocks;
    uint16          m_blockCount;
};

#endif
