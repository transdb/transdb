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

class ClientSocket : public Socket
{
    typedef std::queue<Packet*>     PacketQueue;
    
public:
	ClientSocket(SOCKET fd);
	~ClientSocket();
    
	void OnRead();
	void OnConnect();
	void OnDisconnect();
    
    //handlers
    void HandleWriteData(ByteBuffer &rPacket);
    void HandleReadData(ByteBuffer &rPacket);
    void HandleDeleteData(ByteBuffer &rPacket);
    void HandleGetAllX(ByteBuffer &rPacket);
    void HandlePong(ByteBuffer &rPacket);
    void HandleGetAllY(ByteBuffer &rPacket);
	void HandleStatus(ByteBuffer &rPacket);
    void HandleGetActivityID(ByteBuffer &rPacket);
    
    //queue
    void ProcessQueue();
    uint64 GetQueueSize();
    
    //senders
	void SendPacket(Packet &rPacket);
	void SendPacket(StackPacket &rPacket);
    void OutPacket(uint16 opcode, size_t len, const void* data);
    
    //ping
    void SendPing();
    volatile time_t m_lastPong;
    volatile time_t m_lastPing;
    
private:
    OUTPACKET_RESULT _OutPacket(uint16 opcode, size_t len, const void* data);
    
    uint32                  m_latency;
    uint32                  m_size;
    uint16                  m_opcode;
    bool                    m_nagleEnabled;
    volatile PacketQueue    m_packetQueue;
    Mutex                   m_packetQueueLock;
    ByteBuffer              m_rLocalBuffer;
};

#endif
