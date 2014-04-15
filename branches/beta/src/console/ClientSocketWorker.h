//
//  ClientSocketWorker.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 12.11.12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#ifndef __TransDB__ClientSocketWorker__
#define __TransDB__ClientSocketWorker__

struct ClientSocketTaskData
{
    explicit ClientSocketTaskData(const uint16 &opcode, const long &socketID, const ByteBuffer &rPacket) : m_opcode(opcode), m_socketID(socketID), m_rpos(0), m_size((uint16)rPacket.size())
    {
		if(m_size > BLOCK_SIZE)
		{
			Log.Error(__FUNCTION__, "Packet data bigger than BLOCK_SIZE, processing only bytes from 0 to %u", BLOCK_SIZE);
            m_size = BLOCK_SIZE;
		}
        
        memcpy(&m_rPacketData, rPacket.contents(), m_size);
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
    
    INLINE const uint8 *contents()    { return &m_rPacketData[0]; }
    INLINE const uint16 &rpos()       { return m_rpos; }
    INLINE const uint16 &size()       { return m_size; }
    INLINE const uint16 &opcode()     { return m_opcode; }
    INLINE const long &socketID()     { return m_socketID; }
        
private:
	template <typename T> T read()
	{
		T r = read<T>(m_rpos);
		m_rpos += sizeof(T);
		return r;
	}
    
	template <typename T> T read(const uint16 &pos)
	{
		if(pos + sizeof(T) > size())
			return (T)0;
		else
			return *((T*)&m_rPacketData[pos]);
	}
    
    //declarations
	uint16		m_opcode;
	long        m_socketID;
    uint8       m_rPacketData[BLOCK_SIZE];
    uint16      m_rpos;
    uint16      m_size;
};

class ClientSocketWorker
{
	typedef tbb::concurrent_bounded_queue<ClientSocketTaskData*>        TaskDataQueue;

	friend class ClientSocketWorkerTask;

public:
    ClientSocketWorker();
	~ClientSocketWorker();
    
	//starts worker threads
	void Init();
	//wait for queue empty and than shutdown worker threads
	void Destroy();
    //queue item
    void QueuePacket(const uint16 &opcode, const long &socketID, ByteBuffer &rPacket);
	//get queue size
	size_t GetQueueSize();
    //set exception
    INLINE void SetException(bool oValue)      { m_exception = oValue; }

protected:
	TaskDataQueue       m_rTaskDataQueue;
    tbb::atomic<bool>   m_exception;
	tbb::fixed_pool		*m_pFixedPool;
	void				*m_pFixedPoolMem;
};

extern ClientSocketWorker *g_pClientSocketWorker;

#endif /* defined(__TransDB__ClientSocketWorker__) */
