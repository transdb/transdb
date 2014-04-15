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
    
    void AddSocket(ClientSocket *pClientSocket);
    void RemoveSocket(ClientSocket *pClientSocket);
    ClientSocket *GetSocket(const uint64 &socketID);
    uint64 GetAllSocketPacketQueueSize();
    
    void Update();
    
    
private:
    volatile ClientSocketMap                    m_clientSockets;
    Mutex                                       m_lock;
};

extern ClientSocketHolder g_rClientSocketHolder;

#endif
