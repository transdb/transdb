//
//  BlockManager.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/24/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#ifndef TransDB_BlockManager_h
#define TransDB_BlockManager_h

class Storage;
class ClientSocketWorkerTask;

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


//for Y
class BlockManager
{
    //must be ordered map
    typedef std::map<uint64, uint16>    BlocksIndex;
    
public:
    explicit BlockManager();
    explicit BlockManager(uint8 *pBlocks, uint16 blockCount);
    ~BlockManager();
    
    uint32 WriteRecord(uint64 recordkey, const uint8 *pRecord, uint16 recordSize);
    void ReadRecord(uint64 recordkey, ByteBuffer &rData);
    void ReadRecords(ByteBuffer &rData);
    void DeleteRecord(uint64 recordkey);
    void GetAllRecordKeys(ByteBuffer &rY);
    void ClearDirtyFlags();
    void DefragmentData();
    uint32 GetBlocksCrc32() const;
    
    INLINE uint16 numOfBlocks() const               { return m_blockCount; }
    INLINE uint8 *GetBlock(uint16 blockNum) const   { return m_pBlocks + (BLOCK_SIZE * blockNum); }
    INLINE size_t numOfRecords() const              { return m_rBlockIndex.size(); }
    
private:
	//disable copy constructor and assign
	DISALLOW_COPY_AND_ASSIGN(BlockManager);
    
    void DeallocBlocks();
    void ReallocBlocks();
    void DeleteRecord(BlocksIndex::iterator &itr);
    
    //declarations
    BlocksIndex     m_rBlockIndex;
    uint8           *m_pBlocks;
    uint16          m_blockCount;
};

#endif
