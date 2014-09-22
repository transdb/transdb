//
//  ClientSocketHolder.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 9/3/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#ifndef TransDB_ClientSocketHolder_h
#define TransDB_ClientSocketHolder_h

class ClientSocketHolder
{
    typedef std::map<uint64, ClientSocket*>    ClientSocketMap;
    
public:
    explicit ClientSocketHolder();
    ~ClientSocketHolder();
    
    //add remove socket
    void AddSocket(ClientSocket *pClientSocket);
    void RemoveSocket(ClientSocket *pClientSocket);
    
    //send packet
    void SendPacket(uint64 socketID, const StackPacket &rPacket);
    
    //send packet for CByteBuffer
    void SendPacket(uint64 socketID, uint16 opcode, bbuff *pData);
       
    //for stats
    uint64 GetAllSocketPacketQueueSize();
    
    //update for inactive socket disconnection
    void Update();
    
private:
	//disable copy constructor and assign
	DISALLOW_COPY_AND_ASSIGN(ClientSocketHolder);
    
    ClientSocketMap         m_clientSockets;
    std::recursive_mutex    m_lock;
};

extern ClientSocketHolder g_rClientSocketHolder;

#endif
