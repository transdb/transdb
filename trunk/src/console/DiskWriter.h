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
    
    explicit WriteInfo(const uint64 &key, const int64 &recordPosition) : m_key(key), m_recordPosition(recordPosition)
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
public:
    typedef HashMap<uint64, WriteInfo>                  DirtyXQueue;
    typedef Vector<WriteInfo, uint64>                   DirtyXProcess;
    typedef HashMap<uint64, RecordIndex>                RIDelQueue;
    
    explicit DiskWriter(Storage &pStorage);
    ~DiskWriter();
    
    void QueueIndexDeletetion(RecordIndexMap::accessor &rWriteAccesor);    
    void Queue(RecordIndexMap::accessor &rWriteAccesor);
    void Remove(const uint64 &x);
	void Process() _THROW1(std::runtime_error);
    void RecycleQueue();
    
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
	void ReallocDataFile(const HANDLE &hDataFile, const int64 &minSize, bool oAddFreeSpace = true);
    
private:
	//disable copy constructor and assign
	DISALLOW_COPY_AND_ASSIGN(DiskWriter);

    //private functions
    void ProcessIndexDeleteQueue();
    void WriteDataWithoutRelocateFlag(const HANDLE &hDataFile, RecordIndexMap::accessor &rWriteAccessor);
    bool WriteDataWithRelocateFlag(const HANDLE &hDataFile, RecordIndexMap::accessor &rWriteAccessor);
    
    //declarations
    DirtyXQueue         *m_pQueue;
    RIDelQueue          *m_pRIDelQueue;
    std::mutex          m_rQueueLock;
    std::mutex          m_rRIDelQueueLock;
    Storage				&m_rStorage;
    std::atomic<uint64> m_sumDiskWriteTime;
    std::atomic<uint64> m_lastNumOfItemsInProcess;
    std::atomic<uint64> m_itemsToProcessSize;
};

#endif











