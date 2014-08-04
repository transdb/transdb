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
class DiskWriter;

//disable alignment
#pragma pack(push, 1)

//Control interval
typedef struct IndexControlIntervalDefinitionField
{
    uint16              m_amountOfFreeSpace;        //2
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

#pragma pack(pop)

//FreeSpace
struct FreeSpace
{
	explicit FreeSpace() : m_pos(0), m_lenght(0)	{}
	explicit FreeSpace(int64 pos, int64 lenght) : m_pos(pos), m_lenght(lenght) {}
    
	int64 m_pos;
	int64 m_lenght;
};

//for parsing index file
struct IndexDef
{
	explicit IndexDef(uint64 key, int64 dataPosition, int64 dataLen) : m_key(key), m_dataPosition(dataPosition), m_dataLen(dataLen) {}
    
    uint64  m_key;
    int64   m_dataPosition;
    int64   m_dataLen;
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

//record index flags
typedef enum E_RECORD_INDEX_FLAGS
{
	eRIF_None               = 0,
	eRIF_RelocateBlocks     = 1,
    eRIF_Dirty              = 2,
	eRIF_InDiskWriteQueue	= 4
} E_RIF;

//init status
typedef enum E_INDEXBLOCK_INIT_STATUS
{
    eIIS_OK                 = 0,
    eIIS_NoMemory           = 1,
    eIIS_FreeSpaceCorrupted = 2
} E_IIS;

//block size
static const uint16 INDEX_BLOCK_SIZE    = 4096;        //4kB
//CIDF offset in block
static const uint16 ICIDFOffset         = (INDEX_BLOCK_SIZE - sizeof(ICIDF));
//max record position
static const uint16 IMAX_RECORD_POS     = (ICIDFOffset - sizeof(DREC));

//for key hash
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
typedef tbb::concurrent_hash_map<uint64, RecordIndex, HashCompare<uint64> >         RecordIndexMap;
//for getting all X keys
typedef tbb::concurrent_vector<uint64>                                              XKeyVec;
//for parsing index file
typedef tbb::concurrent_unordered_set<uint32>                                       Init_FreeBlocksSet;
typedef tbb::concurrent_vector<IndexDef>                                            Init_IndexDefVec;
//freespace
typedef Vector<int64>                                                               FreeSpaceOffsets;
typedef std::map<int64, FreeSpaceOffsets>                                           FreeSpaceBlockMap;

class IndexBlock
{
    friend class Storage;
    friend class DiskWriter;
    
    typedef std::set<uint32>                                                        FreeBlocksList;
    typedef std::map<uint32, uint8*>                                                IndexBlockCache;
    
public:
    ~IndexBlock();
    
    /** Create DiskRecordIndex and write/update on disk
     */
    void WriteRecordIndexToDisk(HANDLE hFile, RecordIndexMap::accessor &rWriteAccesor);
    
    /** Mark DiskRecordIndex as empty
     */
    void EraseRecord(HANDLE hFile, const RecordIndex &rRecordIndex);
    
    /** Get IndexControlIntervalDefinitionField from block
     */
    static ICIDF *GetICIDF(const uint8 *pBlock);
    
    /** check if DREC is empty
     */
    static bool IsEmptyDREC(const DREC *pDREC);   
    
private:
    //private ctor only created from Storage
    explicit IndexBlock();
    
    /** Load data from index file
     *  fill RecordIndexMap if provided
     *  fill freespace map
     *  Called from Storage.cpp or DisrWrite.cpp
     */
    E_IIS Init(HANDLE hFile, RecordIndexMap *pRecordIndexMap, FreeSpaceBlockMap &rFreeSpace, int64 dataFileSize);
    
	//disable copy constructor and assign
	DISALLOW_COPY_AND_ASSIGN(IndexBlock);
    
    /** Fill freespace map from indexfdef vector
     */
    bool LoadFreeSpaceFromIndexDef(FreeSpaceBlockMap &rFreeSpaceBlockMap, int64 dataFileSize, Init_IndexDefVec &rIndexDef);
    
    /** return block from cache if not in cache loads from disk
     */
    uint8 *GetCachedDiskBlock(HANDLE hFile, int64 blockDiskPosition, uint32 blockNumber);
    
    /** find empty DiskRecordIndex in block
     */
    DREC *GetEmptyDREC(const uint8 *pDiskBlock, int16 *newRecordOffset);

    //declarations
    uint32                      m_blockCount;
    FreeBlocksList              m_freeBlocks;
    
    //preallocated disk block
    IndexBlockCache             m_rDiskBlockCache;
    LRUCache                    *m_pLRUCache;
};

#endif
