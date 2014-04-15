//
//  IndexBlock.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 9/3/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#ifndef TransDB_IndexBlock_h
#define TransDB_IndexBlock_h

class Storage;
class LRUCache;

//disable alignment
#pragma pack(push, 1)
//Control interval
typedef struct IndexControlIntervalDefinitionField
{
    uint16              m_amoutOfFreeSpace;         //2
    uint16              m_location;                 //2
} ICIDF;

//X - on disk
typedef struct DiskRecordIndex
{
    uint64              m_key;						//8
    std::streamoff      m_recordStart;				//8
    uint16              m_blockCount;				//2
} DREC;
#pragma pack(pop)

//FreeSpace
struct FreeSpace
{
	FreeSpace() : m_pos(0), m_lenght(0)	{}
	FreeSpace(const std::streamoff &pos, const std::streamsize &lenght) : m_pos(pos), m_lenght(lenght) {}
    
	std::streamoff	m_pos;
	std::streamsize	m_lenght;
};

//X - in memory
struct RecordIndex
{
	explicit RecordIndex() : m_recordStart(0), m_pBlockManager(NULL), m_blockCount(0), m_IB_recordOffset(0), m_IB_blockNumber(0), m_flags(0)
	{
        
	}

    std::streamoff      m_recordStart;				//8
    BlockManager        *m_pBlockManager;			//8
    uint16              m_blockCount;				//2
    
    //memory disk index
    int16               m_IB_recordOffset;          //2 - record offset in index block
    uint32              m_IB_blockNumber;           //4 - record block number
    
	//flags
	uint8				m_flags;					//1
};

//record index flags
typedef enum E_RECORD_INDEX_FLAGS
{
	eRIF_None               = 0,
	eRIF_RelocateBlocks     = 1,
    eRIF_Dirty              = 2,
	eRIF_InDiskWriteQueue	= 4
} E_RIF;

//block size
static const uint16 INDEX_BLOCK_SIZE    = 4096;        //4kB
//CIDF offset in block
static const uint16 ICIDFOffset         = INDEX_BLOCK_SIZE - sizeof(ICIDF);

//macro for CIDF
static INLINE ICIDF *GetICIDF(const uint8 *pBlock)
{
    return (ICIDF*)(pBlock + ICIDFOffset);
}

//for memory pool
struct IndexBlock_T
{
	IndexBlock_T()
	{
	}

    uint8 m_val[INDEX_BLOCK_SIZE];
};

//
typedef std::list<FreeSpace*>                               FreeSpaceHolder;
typedef std::multimap<std::streamsize, std::streamoff>      FreeSpaceMultimap;
typedef tbb::concurrent_hash_map<uint64, RecordIndex*>      RecordIndexMap;

class IndexBlock
{
    typedef std::set<uint32>                                                                        FreeBlocksList;
    typedef std::map<uint32, uint8*>                                                                IndexBlockCache;
    
public:
    explicit IndexBlock();
    ~IndexBlock();
    
    void Init(Storage *pStorage, const char *pIndexFilePath, RecordIndexMap &rRecordIndexMap, std::streamoff dataFileSize);
    void WriteRecordIndexToDisk(const RecordIndexMap::accessor &rAccesor);
    void EraseRecord(RecordIndexMap::accessor &rAccesor, RecordIndexMap &rRecordIndexMap);
    void Sync();
    
private:
    uint8 *GetCachedDiskBlock(const size_t &blockDiskPosition, const uint32 &blockNumber);
    
    //declarations
    uint32                      m_blockCount;
    FreeBlocksList              m_freeBlocks;
    
    //preallocated disk block
    IndexBlockCache             m_rDiskBlockCache;
    FixedPool<IndexBlock_T>     *m_pIndexBlockMemPool;
    LRUCache                    *m_pLRUCache;
    
    //IO
    FILE                        *m_pIndexFile;
};

#endif
