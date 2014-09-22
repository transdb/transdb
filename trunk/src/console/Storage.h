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
    friend class StatGenerator;
    friend class ClientSocketWorkerTask;
    
public:
    static Storage *create(const std::string &rFileName);
	~Storage();
    
	//handlers
    void ReadData(HANDLE rDataFileHandle, LRUCache &rLRUCache, uint64 x, uint64 y, bbuff *pData);
    void ReadData(HANDLE rDataFileHandle, LRUCache &rLRUCache, uint64 x, bbuff *pData);
    uint32 WriteData(HANDLE rDataFileHandle, LRUCache &rLRUCache, uint64 x, uint64 y, const uint8 *pRecord, uint16 recordSize);
    void DeleteData(HANDLE rDataFileHandle, LRUCache &rLRUCache, uint64 x, uint64 y);
    void DeleteData(LRUCache &rLRUCache, uint64 x);
    void GetAllX(XKeyVec &rXKeyVec, uint32 sortFlags);
    void GetAllY(HANDLE rDataFileHandle, LRUCache &rLRUCache, uint64 x, bbuff *pY);
    void GetStats(bbuff *pBuff);
    void DefragmentData(HANDLE rDataFileHandle, LRUCache &rLRUCache, uint64 x);
    void GetFreeSpaceDump(uint64 socketID, uint32 token, uint32 flags, uint32 dumpFlags);
    void DefragmentFreeSpace(uint64 socketID, uint32 token, uint32 flags);
    
	//thread
    bool run();

private:
    //private ctor
    explicit Storage(const std::string &rFileName);
    //init
    bool Init();
	//disable copy constructor and assign
	DISALLOW_COPY_AND_ASSIGN(Storage);
        
    //used by WriteData and DefragmentData
    void DefragmentDataInternal(RecordIndexMap::accessor &rWriteAccessor);
    
    //for CRC32 check
    void LoadFromDisk(const HANDLE &rDataFileHandle, LRUCache &rLRUCache, uint64 x);
    void Crc32Check(const HANDLE &rDataFileHandle);
    
    //check if data for blockmanager are in memory and load it from disk
    bool CheckBlockManager(const HANDLE &rDataFileHandle, const uint64 &x, RecordIndexMap::accessor &rWriteAccessor);
    
    //checking memory usage and handling freeing memory
    void CheckMemory(LRUCache &rLRUCache);
    
    //file paths
	const std::string           m_rDataPath;
	const std::string			m_rIndexPath;
    
    //data file size
    std::atomic<int64>          m_dataFileSize;

    //disk writer
    DiskWriter					*m_pDiskWriter;
    
    //index writter - used only by DiskWriter and Storage when loading
    IndexBlock                  *m_pDataIndexDiskWriter;
    
    //main index map in memory
    RecordIndexMap              m_dataIndexes;
    
    //Memory limit
    std::atomic<uint64>         m_memoryUsed;
    
    //stats
    std::atomic<uint64>         m_sumDiskReadTime;
};

#endif
