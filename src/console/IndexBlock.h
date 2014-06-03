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
    uint16              m_location;                 //2 - not used any more
} ICIDF;

//X - on disk
typedef struct DiskRecordIndex
{
    uint64              m_key;						//8
    int64				m_recordStart;				//8
    uint32              m_crc32;                    //4
    uint16              m_blockCount;				//2
} DREC;

//FreeSpace
struct FreeSpace
{
	FreeSpace() : m_pos(0), m_lenght(0)	{}
	FreeSpace(const int64 &pos, const int64 &lenght) : m_pos(pos), m_lenght(lenght) {}
    
	int64 m_pos;
	int64 m_lenght;
};

//X - in memory
struct RecordIndex
{
	explicit RecordIndex() : m_recordStart(0),
                             m_pBlockManager(NULL),
                             m_blockCount(0),
                             m_IB_recordOffset(-1),
                             m_IB_blockNumber(0),
                             m_crc32(0),
                             m_flags(0)
	{
		memset(&m_padding, 0, sizeof(m_padding));    
	}

    int64               m_recordStart;				//8
    BlockManager        *m_pBlockManager;			//8
    uint16              m_blockCount;				//2
    
    //memory disk index
    int16               m_IB_recordOffset;          //2 - record offset in index block
    uint32              m_IB_blockNumber;           //4 - record block number
    
	//flags
    uint32              m_crc32;                    //4
	uint8				m_flags;					//1
    uint8               m_padding[3];               //3 - free bytes
};

#pragma pack(pop)

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
static const uint16 ICIDFOffset         = (INDEX_BLOCK_SIZE - sizeof(ICIDF));
//max record position
static const uint16 IMAX_RECORD_POS     = (ICIDFOffset - sizeof(DREC));

//macro for CIDF
static INLINE ICIDF *GetICIDF(const uint8 *pBlock)
{
    return (ICIDF*)(pBlock + ICIDFOffset);
}

//
static INLINE bool IsEmptyDREC(const DREC *pDREC)
{
    return (pDREC->m_key == 0 && pDREC->m_recordStart == 0 && pDREC->m_blockCount == 0 && pDREC->m_crc32 == 0);
}

//for memory pool
struct IndexBlock_T
{
	explicit IndexBlock_T()
	{
		memset(&m_val, 0, INDEX_BLOCK_SIZE);
	}

    uint8 m_val[INDEX_BLOCK_SIZE];
};

template<typename Key>
class HashCompare
{
public:
    INLINE bool equal(const Key& j, const Key& k) const
    {
        return j == k;
    }
    
    INLINE size_t hash(const Key& k) const
    {
        return static_cast<size_t>(k);
    }
};

//main map for indexes
typedef tbb::concurrent_hash_map<uint64, RecordIndex*, HashCompare<uint64> >        RecordIndexMap;

//freespace
typedef Vector<int64>                                                               FreeSpaceOffsets;
typedef std::map<int64, FreeSpaceOffsets>                                           FreeSpaceBlockMap;

class IndexBlock
{
    friend class Storage;
    
    typedef std::set<uint32>                                                        FreeBlocksList;
    typedef std::map<uint32, uint8*>                                                IndexBlockCache;
    
public:
    ~IndexBlock();
    
    void WriteRecordIndexToDisk(const HANDLE &hFile, RecordIndexMap::accessor &rWriteAccesor);
    void EraseRecord(const HANDLE &hFile, const RecordIndex &rRecordIndex);
    
private:
    //private ctor only created from Storage
    explicit IndexBlock(Storage &pStorage);
    //called from Storage
    bool Init(const std::string &rIndexFilePath, int64 dataFileSize, int64 *indexFileSize);
	//disable copy constructor and assign
	DISALLOW_COPY_AND_ASSIGN(IndexBlock);
    
    uint8 *GetCachedDiskBlock(const HANDLE &hFile, const size_t &blockDiskPosition, const uint32 &blockNumber);
    DREC *GetEmptyDREC(const uint8 *pDiskBlock, int16 *newRecordOffset);

    //declarations
    Storage                     &m_rStorage;
    uint32                      m_blockCount;
    FreeBlocksList              m_freeBlocks;
    
    //preallocated disk block
    IndexBlockCache             m_rDiskBlockCache;
    FixedPool<IndexBlock_T>     m_rIndexBlockMemPool;
    LRUCache                    *m_pLRUCache;
};

#endif
