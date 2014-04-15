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
	FreeSpace()	{}
	FreeSpace(std::streamoff pos, std::streamsize lenght) : m_pos(pos), m_lenght(lenght) {}
    
	std::streamoff	m_pos;
	std::streamsize	m_lenght;
};

//X - in memory
struct RecordIndex
{
	RecordIndex()
	{
        m_recordStart       = 0;
        m_pBlockManager     = NULL;
        m_blockCount        = 0;
        m_IB_recordOffset   = 0;
        m_IB_blockNumber    = 0;
	}

    std::streamoff      m_recordStart;				//8
    BlockManager        *m_pBlockManager;			//8
    uint16              m_blockCount;				//2
    
    //memory disk index
    int16               m_IB_recordOffset;          //2 - record offset in index block
    uint32              m_IB_blockNumber;           //4 - record block number
};

//block size
static const uint16 INDEX_BLOCK_SIZE = 4096;        //4kB
//CIDF offset in block
static const uint16 ICIDFOffset = INDEX_BLOCK_SIZE - sizeof(ICIDF);

//macro for CIDF
#define GetICIDF(block)      (ICIDF*)(block+ICIDFOffset)

//
#ifdef WIN32
typedef std::map<uint64, RecordIndex*, std::less<uint64>, stdext::allocators::allocator_chunklist<std::pair<uint64, RecordIndex*> > >       RecordIndexMap;
typedef std::list<FreeSpace*, stdext::allocators::allocator_chunklist<FreeSpace*> >                                                         FreeSpaceHolder;
typedef std::multimap<std::streamsize, std::streamoff>                                                                                      FreeSpaceMultimap;
#else
typedef std::map<uint64, RecordIndex*>                                                                                                      RecordIndexMap;
typedef std::list<FreeSpace*>                                                                                                               FreeSpaceHolder;
typedef std::multimap<std::streamsize, std::streamoff>                                                                                      FreeSpaceMultimap;
#endif

class IndexBlock
{
#ifdef WIN32
    typedef std::set<uint32, std::less<uint32>, stdext::allocators::allocator_chunklist<uint32> >   FreeBlocksList;
#else
    typedef std::set<uint32>                                                                        FreeBlocksList;
#endif
    
public:
    IndexBlock();
    ~IndexBlock();
    
    void Init(Storage *pStorage, const char *pIndexFilePath, RecordIndexMap *pRecordIndexMap, std::streamoff dataFileSize);
    void WriteRecordIndexToDisk(RecordIndexMap::iterator &itr);
    void EraseRecord(RecordIndexMap::iterator &itr, LockingPtr<RecordIndexMap> &pRecordIndexMap);
    void Sync();
    
private:
    uint32          m_blockCount;
    FreeBlocksList  m_freeBlocks;
    
    //preallocated disk block
    uint8           *m_pCachedDiskBlock;
    uint32          m_cachedDiskBlockNum;
    
    //IO
    FILE            *m_pIndexFile;
};

#endif
