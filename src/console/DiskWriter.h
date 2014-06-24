//
//  DiskWriter.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/31/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#ifndef TransDB_DiskWriter_h
#define TransDB_DiskWriter_h

struct WriteInfo
{
    explicit WriteInfo() : m_key(0), m_recordPosition(0)
    {
        
    }
    
    explicit WriteInfo(uint64 key, int64 recordPosition) : m_key(key), m_recordPosition(recordPosition)
    {
        
    }
    
    INLINE WriteInfo &operator=(const WriteInfo &rWriteInfo)
    {
        m_key               = rWriteInfo.m_key;
        m_recordPosition    = rWriteInfo.m_recordPosition;
        return *this;
    }
         
    INLINE bool operator==(const WriteInfo &rWriteInfo) const
    {
        return m_key == rWriteInfo.m_key;
    }
    
    uint64  m_key;
    int64   m_recordPosition;
};

class DiskWriter
{
    friend class Storage;
    friend class IndexBlock;
    
public:
    typedef HashMap<uint64, WriteInfo, ScalableHashMapNodeAllocator<uint64, WriteInfo> >    DirtyXQueue;
    typedef Vector<WriteInfo, uint64>                                                       DirtyXProcess;
    typedef HashMap<uint64, RecordIndex>                                                    RIDelQueue;
    //freespace
    typedef Vector<int64>                                                                   FreeSpaceOffsets;
    typedef std::map<int64, FreeSpaceOffsets>                                               FreeSpaceBlockMap;
    
    //destructor
    ~DiskWriter();
    
    void QueueIndexDeletetion(RecordIndexMap::accessor &rWriteAccesor);    
    void Queue(RecordIndexMap::accessor &rWriteAccesor);
    void Remove(uint64 x);
	void Process();
    
	bool HasTasks()
	{
        std::lock_guard<std::mutex> rQGuard(m_rQueueLock);
        std::lock_guard<std::mutex> rRIDel_Guard(m_rRIDelQueueLock);
		return m_pQueue->size() || m_pRIDelQueue->size();
	}
    
    size_t GetQueueSize()
    {
        std::lock_guard<std::mutex> rQGuard(m_rQueueLock);
        return m_pQueue->size();
    }
    
    uint64 GetLastNumOfItemsInProcess() const
    {
        return m_lastNumOfItemsInProcess;
    }
    
    uint64 GetItemsToProcessSize() const
    {
        return m_itemsToProcessSize;
    }

    //reallocating
	void ReallocDataFile(HANDLE hDataFile, int64 minSize, bool oAddFreeSpace = true);
    
private:
    //private ctor only created from Storage
    explicit DiskWriter(Storage &rStorage);
	//disable copy constructor and assign
	DISALLOW_COPY_AND_ASSIGN(DiskWriter);

    //private functions
    void ProcessIndexDeleteQueue();
    void WriteDataWithoutRelocateFlag(HANDLE hDataFile, RecordIndexMap::accessor &rWriteAccessor);
    bool WriteDataWithRelocateFlag(HANDLE hDataFile, RecordIndexMap::accessor &rWriteAccessor);
    
    //freespace functions
	void AddFreeSpace(int64 pos, int64 lenght);
	int64 GetFreeSpacePos(int64 size);
	void DefragmentFreeSpace();
    
    //declarations
    Storage				&m_rStorage;
    
    //freespace map
    FreeSpaceBlockMap   m_rFreeSpace;
    
    //queue for changes in data file
    DirtyXQueue         *m_pQueue;
    std::mutex          m_rQueueLock;
    
    //queue for delete in index file
    RIDelQueue          *m_pRIDelQueue;
    std::mutex          m_rRIDelQueueLock;
    
    //stats
    std::atomic<uint64> m_sumDiskWriteTime;
    std::atomic<uint64> m_lastNumOfItemsInProcess;
    std::atomic<uint64> m_itemsToProcessSize;
};

#endif











