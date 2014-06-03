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
    friend class StatGenerator;
    friend class ClientSocketWorkerTask;
    friend class MemoryWatcher;
    
public:
    static Storage *create(const std::string &rFileName);
	~Storage();
    
	//handlers
    void ReadData(const HANDLE &rDataFileHandle, LRUCache &rLRUCache, const uint64 &x, const uint64 &y, ByteBuffer &rData);
    void ReadData(const HANDLE &rDataFileHandle, LRUCache &rLRUCache, const uint64 &x, ByteBuffer &rData);
    uint32 WriteData(const HANDLE &rDataFileHandle, LRUCache &rLRUCache, const uint64 &x, const uint64 &y, const uint8 *pRecord, const uint16 &recordSize);
    void DeleteData(const HANDLE &rDataFileHandle, LRUCache &rLRUCache, const uint64 &x, const uint64 &y);
    void DeleteData(LRUCache &rLRUCache, const uint64 &x);
    void GetAllX(uint8 **pX, uint64 *xSize);
    void GetAllY(const HANDLE &rDataFileHandle, LRUCache &rLRUCache, const uint64 &x, ByteBuffer &rY);
    void GetStats(ByteBuffer &rBuff);
    void DefragmentData(const HANDLE &rDataFileHandle, LRUCache &rLRUCache, const uint64 &x);
    void GetFreeSpaceDump(ByteBuffer &rBuff, const uint32 &dumpFlags);
    
	//thread
    bool run();

private:
    //private ctor
    explicit Storage(const std::string &rFileName);
    //init
    bool Init();
	//disable copy constructor and assign
	DISALLOW_COPY_AND_ASSIGN(Storage);
    
    //freespace functions
	void AddFreeSpace(const int64 &pos, const int64 &lenght);
	int64 GetFreeSpacePos(const int64 &size);
	void DefragmentFreeSpace();
    
    //used by WriteData and DefragmentData
    void DefragmentDataInternal(RecordIndexMap::accessor &rWriteAccessor);
    
    //for CRC32 check
    void LoadFromDisk(const HANDLE &rDataFileHandle, LRUCache &rLRUCache, const uint64 &x);
    void Crc32Check(const HANDLE &rDataFileHandle);
    
    //check if data for blockmanager are in memory and load it from disk
    bool CheckBlockManager(const HANDLE &rDataFileHandle, const uint64 &x, RecordIndexMap::accessor &rWriteAccessor);
    
    //checking memory usage and handling freeing memory
    void CheckMemory(LRUCache &rLRUCache);
    
    //declarations
	const std::string           m_rDataPath;
	const std::string			m_rIndexPath;
    std::atomic<int64>          m_dataFileSize;

    //disk writer
    uint32                      m_diskWriterCount;
    DiskWriter					*m_pDiskWriter;
    
    //shared
    FixedPool<RecordIndex>      m_rRecordIndexMemPool;
    std::mutex                  m_rRecordIndexMemPoolLock;
    IndexBlock                  *m_pDataIndexDiskWriter;
    std::mutex                  m_rDataIndexDiskWriterLock;
    RecordIndexMap              m_dataIndexes;
    
    //shared
    FixedPool<BlockManager>     m_rBlockManagerMemPool;
    std::mutex                  m_rBlockManagerMemPoolLock;
    
    //shared
    FreeSpaceBlockMap           m_rFreeSpace;
    std::mutex                  m_rFreeSpaceLock;
    
    //Memory limit
    std::atomic<uint64>         m_memoryUsed;
    
    //
    FixedPool<BlockSize_T>      m_rBlockMemPool;
    std::mutex                  m_rBlockMemPoolLock;
    
    //stats
    std::atomic<uint64>         m_sumDiskReadTime;
};

#endif
