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
    friend class DiskWriter;
    friend class MemoryChecker;
	friend class IndexBlock;
    friend class BlockManager;
	friend class BlockManagerCopy;
    friend class WebService;
    friend class ClientSocketWorkerTask;
    friend class MemoryWatcher;
    
public:
	explicit Storage(const char *pFileName);
	~Storage();
    
	//handlers
    void ReadData(const uint64 &x, const uint64 &y, ByteBuffer &rData);
    void ReadData(const uint64 &x, ByteBuffer &rData);
    uint32 WriteData(const uint64 &x, const uint64 &y, const uint8 *pRecord, const uint16 &recordSize);
    void DeleteData(const uint64 &x, const uint64 &y);
    void DeleteData(const uint64 &x);
    void GetAllX(uint8 **pX, uint64 *xSize);
    void GetAllY(const uint64 &x, ByteBuffer &rY);
    void GetStats(ByteBuffer &rBuff);
    
	//thread
    bool run();

protected:   
	void AddFreeSpace(const std::streamoff &pos, const std::streamsize &lenght);
	std::streamoff GetFreeSpacePos(const std::streamsize &size);
    void QueueFreeSpace(const std::streamoff &pos, const std::streamsize &lenght);
    void ProcessFreeSpaceQueue();
	void DefragmentFreeSpace();
    
private:
    void SaveFreeSpace();
    bool LoadFreeSpace();
    
protected:
    bool CheckBlockManager(const uint64 &x, RecordIndexMap::accessor &rWriteAccessor);
    void CheckMemory();
    
    //declarations
	char						*m_pDataPath;
	char						*m_pIndexPath;
    char                        *m_pFreeSpacePath;
    std::streamoff              m_dataFileSize;
    
    //disk writer
    uint32                      m_diskWriterCount;
    DiskWriter					*m_pDiskWriter;
    
    //shared
    FixedPool<RecordIndex>      *m_pRecordIndexMemPool;
    tbb::mutex                  m_rRecordIndexMemPoolLock;
    IndexBlock                  *m_pDataIndexDiskWriter;
    tbb::mutex                  m_rDataIndexDiskWriterLock;
    RecordIndexMap              m_dataIndexes;
    
    //shared
    FixedPool<FreeSpace>        *m_pFreeSpaceMemoryPool;
    FreeSpaceMultimap           m_freeSpace;
    tbb::mutex                  m_freeSpaceLock;
    FreeSpaceMultimap           m_freeSpaceQueue;
    tbb::spin_mutex             m_freeSpaceQueueLock;
    
    //Memory limit
    LRUCache                    *m_pLRUCache;
    tbb::mutex                  m_LRULock;
    tbb::atomic<uint64>         m_memoryUsed;
    
    //
    FixedPool<BlockSize_T>      *m_pBlockMemPool;
    tbb::mutex                  m_rBlockMemPoolLock;
};

extern Storage *g_pStorage;

#endif
