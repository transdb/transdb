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
    void HandlePing(ByteBuffer &rPacket);
    void HandleGetAllY(ByteBuffer &rPacket);
	void HandleStatus(ByteBuffer &rPacket);
    void HandleGetActivityID(ByteBuffer &rPacket);
    void HandleNull(ByteBuffer &rPacket)
    {
        
    }
    
	OUTPACKET_RESULT SendPacket(Packet &rPacket);
	OUTPACKET_RESULT SendPacket(StackPacket &rPacket);
    
private:
    OUTPACKET_RESULT OutPacket(uint16 opcode, size_t len, const void* data);
    
    ByteBuffer  m_rLocalBuffer;
    
    uint32      m_size;
    uint16      m_opcode;
};

#endif
