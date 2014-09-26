//
//  ClientSocket.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/20/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#ifndef TransDB_ClientSocket_h
#define TransDB_ClientSocket_h

using namespace SocketEnums;

#pragma pack(push, 1)
struct PackerHeader
{
    uint32  m_size;
    uint16  m_opcode;
};
#pragma pack(pop)

class ClientSocket : public Socket
{
    typedef std::queue<Packet*>     PacketQueue;
    
public:
	explicit ClientSocket(SOCKET fd);
	~ClientSocket();
    
	void OnRead();
	void OnConnect();
	void OnDisconnect();
    
    //handlers
    void QueueWritePacket(bbuff*);
    void QueueReadPacket(bbuff*);
    void HandlePong(bbuff*);
    void HandleGetActivityID(bbuff*);
    
    //queue
    void ProcessQueue();
    uint64 GetQueueSize();
    
    //ID
    INLINE const uint64 GetSocketID() const    { return m_socketID; }
    
    //sender
    void OutPacket(uint16 opcode, size_t len, const void* data);
    
    //stream support
    OUTPACKET_RESULT SendRawData(const void *dataChunk, size_t chunkSize);
    
    //ping
    void SendPing();
    std::atomic<time_t> m_lastPong;
    std::atomic<time_t> m_lastPing;
    
private:
	//disable copy constructor and assign
	DISALLOW_COPY_AND_ASSIGN(ClientSocket);
    
    OUTPACKET_RESULT _OutPacket(uint16 opcode, size_t len, const void* data);
    
    uint64                  m_socketID;
    uint32                  m_latency;
    uint32                  m_size;
    uint16                  m_opcode;
    bool                    m_nagleEnabled;
    volatile PacketQueue    m_packetQueue;
    std::mutex              m_packetQueueLock;
    bbuff*                  m_pReceiveBuff;
};

#endif
