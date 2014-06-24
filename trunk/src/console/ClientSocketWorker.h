//
//  ClientSocketWorker.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 12.11.12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#ifndef __TransDB__ClientSocketWorker__
#define __TransDB__ClientSocketWorker__

class ClientSocketTaskData
{
public:
    explicit ClientSocketTaskData(uint16 opcode,
                                  uint64 socketID,
                                  ClientSocketBuffer &rPacket) : m_socketID(socketID),
                                                                 m_opcode(opcode),
                                                                 m_rpos(0),
                                                                 m_rPacketData(std::move(rPacket))
    {

    }
    
	INLINE ClientSocketTaskData &operator>>(uint32 &value)
	{
        value = read<uint32>();
		return *this;
	}
    
	INLINE ClientSocketTaskData &operator>>(uint64 &value)
	{
        value = read<uint64>();
		return *this;
	}
    
    INLINE const uint8 *contents() const            { return &m_rPacketData[0]; }
    INLINE const size_t rpos() const                { return m_rpos; }
    INLINE const size_t size() const                { return m_rPacketData.size(); }
    INLINE const uint16 opcode() const              { return m_opcode; }
    INLINE const uint64 socketID() const            { return m_socketID; }
        
private:
	//disable copy constructor and assign
	DISALLOW_COPY_AND_ASSIGN(ClientSocketTaskData);
    
	template <typename T>
    T read()
	{
		T r = read<T>(m_rpos);
		m_rpos += sizeof(T);
		return r;
	}
    
	template <typename T>
    T read(const size_t &pos)
	{
		if(pos + sizeof(T) > size())
			return (T)0;
		else
			return *((T*)&m_rPacketData[pos]);
	}
    
    //declarations
	uint64              m_socketID;
    uint16              m_opcode;
    size_t              m_rpos;
    ClientSocketBuffer  m_rPacketData;
};

class ClientSocketWorker
{
	typedef tbb::concurrent_bounded_queue<ClientSocketTaskData*>        TaskDataQueue;

	friend class ClientSocketWorkerTask;

public:
    static ClientSocketWorker *create();
	~ClientSocketWorker();
    
	//wait for queue empty and than shutdown worker threads
	void DestroyWorkerThreads();
    //write pending write to disk and destroy storage
    void DestroyStorage();
    //queue item
    void QueuePacket(uint16 opcode, uint64 socketID, ClientSocketBuffer &rPacket);
    //queuen items from opcode C_MSG_READ_DATA
    void QueueReadPacket(uint16 opcode, uint64 socketID, ClientSocketBuffer &rPacket);
	//get queue size
	size_t GetQueueSize();
    //get read queue size
    size_t GetReadQueueSize();
    //set exception
    INLINE void SetException(bool oValue)      { m_exception = oValue; }

private:
    //private ctor
    explicit ClientSocketWorker();
    //init storage, load index and data file
    bool InitStorage();
	//starts worker threads
	bool InitWorkerThreads();
	//disable copy constructor and assign
	DISALLOW_COPY_AND_ASSIGN(ClientSocketWorker);
    
    Storage             *m_pStorage;
	TaskDataQueue       m_rTaskDataQueue;
    TaskDataQueue       m_rReadTaskDataQueue;
    std::atomic<bool>   m_exception;
	tbb::fixed_pool		*m_pFixedPool;
	void				*m_pFixedPoolMem;
};

extern ClientSocketWorker *g_pClientSocketWorker;

#endif /* defined(__TransDB__ClientSocketWorker__) */
