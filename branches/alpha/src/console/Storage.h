//
//  Storage.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/20/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#ifndef TransDB_Storage_h
#define TransDB_Storage_h

class DiskWriter;

class Storage : public ThreadContext
{
    typedef std::queue<CallbackBase*>               TaskQueue;
    
    friend class DiskWriter;
    friend class MemoryChecker;
	friend class IndexBlock;
    friend class BlockManager;
	friend class BlockManagerCopy;
    
public:
	Storage(const char *pFileName);
	~Storage();
    
    void ReadData(uint64 x, uint64 y, uint8 **pRecord, uint16 *recordSize);
    void ReadData(uint64 x, uint8 **pRecords, uint32 *recordsSize);
    uint32 WriteData(uint64 x, uint64 y, uint8 *pRecord, uint16 recordSize);
    void DeleteData(uint64 x, uint64 y);
    void DeleteData(uint64 x);
    void GetAllX(uint8 **pX, uint64 *xSize);
    void GetAllY(uint64 x, uint8 **pY, uint64 *ySize);
    void GetStats(ByteBuffer &rBuff);
    
    bool run();

protected:   
	void AddFreeSpace(std::streamoff pos, std::streamsize lenght);
	std::streamoff GetFreeSpacePos(std::streamsize size, bool oCanDefrag);
	void DefragmentFreeSpace(FreeSpaceHolder &rFreeSpaceHolder);

    void CheckBlockManager(uint64 x, RecordIndex *pRecordIndex);
    void CheckMemory();

    //declarations
	FILE						*m_pDataFile;
	char						*m_pDataPath;
	char						*m_pIndexPath;
    
    //disk writer
    uint32                      m_diskWriterCount;
    DiskWriter					*m_pDiskWriter;
    
    //shared
    FixedPool<RecordIndex>      *m_pRecordIndexMemoryPool;
    IndexBlock                  *m_pDataIndexDiskWriter;
	volatile RecordIndexMap		m_dataIndexes;
    Mutex						m_dataIndexesLock;
    
    //shared
    
    FixedPool<FreeSpace>        *m_pFreeSpaceMemoryPool;
    volatile FreeSpaceMultimap  m_freeSpace;
    Mutex                       m_freeSpaceLock;
    
    //Memory limit
    FixedPool<BlockManager>     *m_pBlockManagerMemoryPool;
    LRUCache                    *m_pLRUCache;
    uint64                      m_memoryUsed;
	uint32                      m_memoryCheckerCount;
    
    //tasks
    volatile TaskQueue          m_taskQueue;
    Mutex                       m_taskQeueuLock;
    
    //block mempool
    FixedPool<BlockSize_T>      *m_pBlockMemoryPool;
};

extern Storage *g_pStorage;

#endif
