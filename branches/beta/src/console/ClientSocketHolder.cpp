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
    LockingPtr<ClientSocketMap> pClientSockets(m_clientSockets, m_lock);
    if(pClientSockets->size())
    {
        ClientSocketMap::iterator itr, itr2;
        for(itr = pClientSockets->begin();itr != pClientSockets->end();)
        {
            itr2 = itr++;
            count += itr2->second->GetQueueSize();
        }
    }
    return count;
}

void ClientSocketHolder::AddSocket(ClientSocket *pClientSocket)
{
    LockingPtr<ClientSocketMap> pClientSockets(m_clientSockets, m_lock);
    pClientSockets->insert(ClientSocketMap::value_type(pClientSocket->GetSocketID(), pClientSocket));
}

void ClientSocketHolder::RemoveSocket(ClientSocket *pClientSocket)
{
    LockingPtr<ClientSocketMap> pClientSockets(m_clientSockets, m_lock);
    pClientSockets->erase(pClientSocket->GetSocketID());
}

ClientSocket *ClientSocketHolder::GetSocket(const long &socketID)
{
    LockingPtr<ClientSocketMap> pClientSockets(m_clientSockets, m_lock);
    ClientSocketMap::iterator itr;
    
    itr = pClientSockets->find(socketID);
    if(itr != pClientSockets->end())
        return itr->second;
    else
        return NULL;
}

void ClientSocketHolder::Update()
{
    //called every 50ms
    LockingPtr<ClientSocketMap> pClientSockets(m_clientSockets, m_lock);
    if(pClientSockets->size())
    {
        time_t t = UNIXTIME;
        ClientSocket *pSocket;
        ClientSocketMap::iterator itr, itr2;
        
        //ping + process queue
        for(itr = pClientSockets->begin();itr != pClientSockets->end();)
        {
            itr2 = itr++;
            pSocket = itr2->second;
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