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
    eBMS_OldRecord              = 32
} E_BMS;

//for Y
class BlockManager
{
    //must be ordered map
    typedef std::map<uint64, uint16>    BlocksIndex;    
    
public:
    BlockManager(Storage &pStorage);
    BlockManager(Storage &pStorage, const uint64 &x, const Blocks &rBlocks);
    ~BlockManager();
    
    uint32 WriteRecord(const uint64 &recordkey, const uint8 *pRecord, const uint16 &recordSize, int32 *writtenInBlocks);
    void ReadRecord(const uint64 &recordkey, ByteBuffer &rData);
    void ReadRecords(ByteBuffer &rData);
    void DeleteRecord(const uint64 &recordkey);
    void GetAllRecordKeys(ByteBuffer &rY);
    void ClearDirtyFlags();
    size_t GetMemUsage();
    
    INLINE uint16 numOfBlocks()                     { return static_cast<uint16>(m_rBlocks.size()); }
    INLINE uint8 *GetBlock(const uint16 &blockNum)  { return m_rBlocks.at(blockNum); }
    INLINE size_t numOfRecords()                    { return m_blockIndex.size(); }
       
private:
    void ReallocBlocks();
    void DeleteRecord(BlocksIndex::iterator &itr);
    
    //declarations
    BlocksIndex     m_blockIndex;    
    Storage         &m_pStorage;
    Blocks          m_rBlocks;
};

#endif
