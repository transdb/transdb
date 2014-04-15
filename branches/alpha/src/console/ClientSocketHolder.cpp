//
//  ClientSocketHolder.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 9/3/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

ClientSocketHolder g_rClientSocketHolder;

ClientSocketHolder::ClientSocketHolder()
{

}

ClientSocketHolder::~ClientSocketHolder()
{
    
}

uint64 ClientSocketHolder::GetAllSocketPacketQueueSize()
{
    uint64 count = 0;
    LockingPtr<ClientSocketSet> pClientSockets(m_clientSockets, m_lock);
    if(pClientSockets->size())
    {
        ClientSocketSet::iterator itr, itr2;
        for(itr = pClientSockets->begin();itr != pClientSockets->end();)
        {
            itr2 = itr++;
            count += (*itr2)->GetQueueSize();
        }
    }
    return count;
}

void ClientSocketHolder::AddSocket(ClientSocket *pClientSocket)
{
    LockingPtr<ClientSocketSet> pClientSockets(m_clientSockets, m_lock);
    pClientSockets->insert(pClientSocket);
}

void ClientSocketHolder::RemoveSocket(ClientSocket *pClientSocket)
{
    LockingPtr<ClientSocketSet> pClientSockets(m_clientSockets, m_lock);
    pClientSockets->erase(pClientSocket);
}

void ClientSocketHolder::Update()
{
    //called every 50ms
    LockingPtr<ClientSocketSet> pClientSockets(m_clientSockets, m_lock);
    if(pClientSockets->size())
    {
        time_t t = UNIXTIME;
        ClientSocket *pSocket;
        ClientSocketSet::iterator itr, itr2;
        
        //ping + process queue
        for(itr = pClientSockets->begin();itr != pClientSockets->end();)
        {
            itr2 = itr++;
            pSocket = (*itr2);
            //
            if(pSocket->m_lastPong < t && ((uint64)(t - pSocket->m_lastPong) > (uint64)g_PingTimeout))
            {
                Log.Warning(__FUNCTION__, "Connection to client: %s - dropped due to pong timeout.", pSocket->GetRemoteIP().c_str());
                pSocket->Disconnect();
                return;
            }
            
			if((uint64)(t - pSocket->m_lastPing) > (uint64)g_PingSendInterval)
            {
                // send a ping packet.
                pSocket->SendPing();
            }
            
            //process queue
            pSocket->ProcessQueue();
        }
    }
}