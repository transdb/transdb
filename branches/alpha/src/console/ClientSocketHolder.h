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
    typedef unordered_set<ClientSocket*>    ClientSocketSet;
    
public:
    ClientSocketHolder();
    ~ClientSocketHolder();
    
    void AddSocket(ClientSocket *pClientSocket);
    void RemoveSocket(ClientSocket *pClientSocket);
    uint64 GetAllSocketPacketQueueSize();
    
    void Update();
    
    
private:
    volatile ClientSocketSet    m_clientSockets;
    Mutex                       m_lock;
};

extern ClientSocketHolder g_rClientSocketHolder;

#endif
