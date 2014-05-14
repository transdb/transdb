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
	explicit Storage(const char *pFileName);
	~Storage();
    
	//handlers
    void ReadData(const HANDLE &rDataFileHandle, const uint64 &x, const uint64 &y, ByteBuffer &rData);
    void ReadData(const HANDLE &rDataFileHandle, const uint64 &x, ByteBuffer &rData);
    uint32 WriteData(const HANDLE &rDataFileHandle, const uint64 &x, const uint64 &y, const uint8 *pRecord, const uint16 &recordSize);
    void DeleteData(const HANDLE &rDataFileHandle, const uint64 &x, const uint64 &y);
    void DeleteData(const uint64 &x);
    void GetAllX(uint8 **pX, uint64 *xSize);
    void GetAllY(const HANDLE &rDataFileHandle, const uint64 &x, ByteBuffer &rY);
    void GetStats(ByteBuffer &rBuff);
    void DefragmentData(const HANDLE &rDataFileHandle, const uint64 &x);
    void GetFreeSpaceDump(ByteBuffer &rBuff, const uint32 &dumpFlags);
    
	//thread
    bool run();

protected:   
	void AddFreeSpace(const int64 &pos, const int64 &lenght);
	int64 GetFreeSpacePos(const int64 &size);
	void DefragmentFreeSpace();
    
private:    
    void DefragmentDataInternal(RecordIndexMap::accessor &rWriteAccessor);
    
    //for CRC32 check
    void LoadFromDisk(const HANDLE &rDataFileHandle, const uint64 &x);
    void Crc32Check(const HANDLE &rDataFileHandle);
    
protected:
    bool CheckBlockManager(const HANDLE &rDataFileHandle, const uint64 &x, RecordIndexMap::accessor &rWriteAccessor);
    void CheckMemory();
    
    //declarations
	char						*m_pDataPath;
	char						*m_pIndexPath;
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
    LRUCache                    *m_pLRUCache;
    std::mutex                  m_LRULock;
    std::atomic<uint64>         m_memoryUsed;
    
    //
    FixedPool<BlockSize_T>      m_rBlockMemPool;
    std::mutex                  m_rBlockMemPoolLock;
    
    //stats
    std::atomic<uint64>         m_sumDiskReadTime;
};

#endif
