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
    explicit WriteInfo(const uint64 &key, const std::streamoff &recordPosition) : m_key(key), m_recordPosition(recordPosition)
    {
        
    }
         
    bool operator==(const WriteInfo &rWriteInfo) const
    {
        return m_key == rWriteInfo.m_key;
    }
    
    uint64          m_key;
    std::streamoff  m_recordPosition;
};

class WriteInfo_hash
{
public:
    size_t operator()(const WriteInfo &rWriteInfo) const
    {
        return rWriteInfo.m_key;
    }
};
        
class DiskWriter
{
    typedef unordered_set<WriteInfo, WriteInfo_hash>    DirtyXQueue;
    typedef std::list<WriteInfo>                        DirtyXProcess;
    
public:
    explicit DiskWriter(Storage &pStorage);
    ~DiskWriter();
    
    void Queue(RecordIndexMap::accessor &rWriteAccesor);
    void Remove(const uint64 &x);
    void Process();
    
	bool HasTasks()
	{
        std::lock_guard<tbb::mutex> rQGuard(m_queueLock);
        std::lock_guard<tbb::mutex> rPGuard(m_lock);
		return m_rQueue.size() || m_rProcess.size();
	}

    //reallocating
	void ReallocDataFile(const size_t &minSize, bool oAddFreeSpace = true);
    
private:
    DirtyXQueue         m_rQueue;
    DirtyXProcess       m_rProcess;
    tbb::mutex          m_queueLock;
    tbb::mutex			m_lock;
    Storage				&m_pStorage;
};

#endif











