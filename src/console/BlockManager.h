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
#ifdef WIN32
    typedef std::map<uint64, uint16, std::less<uint64>, stdext::allocators::allocator_chunklist<std::pair<uint64, uint16> > >    BlocksIndex;
#else
    typedef std::map<uint64, uint16>                                                                                             BlocksIndex;    
#endif
    
public:
    BlockManager();
    ~BlockManager();
    
    void Init(Storage *pStorage);
    void Load(uint64 x, Blocks &rBlocks);
    void ReallocBlocks();
    uint32 WriteRecord(uint64 recordkey, uint8 *pRecord, uint16 recordSize, int32 *writtenInBlocks);
    void ReadRecord(uint64 recordkey, uint8 **pRecord, uint16 *recordSize);
    void ReadRecords(uint8 **pRecords, uint32 *recordsSize);
    void DeleteRecord(uint64 recordkey);
    void GetAllRecordKeys(uint8 **pRecordKeys, uint64 *pRecordKeysSize);
    
    INLINE uint16 numOfBlocks()    { return static_cast<uint16>(m_rBlocks.size()); }
    INLINE Blocks &DataPointer()   { return m_rBlocks; }
    
private:
    void DeleteRecord(BlocksIndex::iterator itr);
    
    //declarations
    Storage         *m_pStorage;
    Blocks          m_rBlocks;
    BlocksIndex     m_blockIndex;
};

#endif
