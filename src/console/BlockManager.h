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
    explicit BlockManager(const Storage &pStorage);
    explicit BlockManager(const Storage &pStorage, const uint64 &x, const Blocks &rBlocks);
    ~BlockManager();
    
    uint32 WriteRecord(const uint64 &recordkey, const uint8 *pRecord, const uint16 &recordSize);
    void ReadRecord(const uint64 &recordkey, ByteBuffer &rData);
    void ReadRecords(ByteBuffer &rData);
    void DeleteRecord(const uint64 &recordkey);
    void GetAllRecordKeys(ByteBuffer &rY);
    void ClearDirtyFlags();
    void DefragmentData();
    NOINLINE uint32 GetBlocksCrc32();
    
    INLINE uint16 numOfBlocks()                     { return m_rBlocks.size(); }
    INLINE uint8 *GetBlock(const uint16 &blockNum)  { return m_rBlocks[blockNum]; }
    INLINE size_t numOfRecords()                    { return m_rBlockIndex.size(); }
    
private:
	//disable copy constructor
	BlockManager(const BlockManager& that);

    void DeallocBlocks();
    void ReallocBlocks();
    void DeleteRecord(BlocksIndex::iterator &itr);
    
    //declarations
    BlocksIndex     m_rBlockIndex;
    Storage         &m_rStorage;
    Blocks          m_rBlocks;
};

#endif
