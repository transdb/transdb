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
    std::lock_guard<std::recursive_mutex> rGuard(m_lock);
    if(!m_clientSockets.empty())
    {
        ClientSocketMap::iterator itr, itr2;
        for(itr = m_clientSockets.begin();itr != m_clientSockets.end();)
        {
            itr2 = itr++;
            count += itr2->second->GetQueueSize();
        }
    }
    return count;
}

void ClientSocketHolder::AddSocket(ClientSocket *pClientSocket)
{
    std::lock_guard<std::recursive_mutex> rGuard(m_lock);
    m_clientSockets.insert(ClientSocketMap::value_type(pClientSocket->GetSocketID(), pClientSocket));
}

void ClientSocketHolder::RemoveSocket(ClientSocket *pClientSocket)
{
    std::lock_guard<std::recursive_mutex> rGuard(m_lock);
    m_clientSockets.erase(pClientSocket->GetSocketID());
}

void ClientSocketHolder::SendPacket(const uint64 &socketID, const Packet &rPacket)
{
    std::lock_guard<std::recursive_mutex> rGuard(m_lock);
    ClientSocketMap::iterator itr = m_clientSockets.find(socketID);
    if(itr != m_clientSockets.end())
    {
        itr->second->OutPacket(rPacket.GetOpcode(), rPacket.size(), (rPacket.size() ? (const void*)rPacket.contents() : NULL));
    }
}

void ClientSocketHolder::SendPacket(const uint64 &socketID, const StackPacket &rPacket)
{
    std::lock_guard<std::recursive_mutex> rGuard(m_lock);
    ClientSocketMap::iterator itr = m_clientSockets.find(socketID);
    if(itr != m_clientSockets.end())
    {
        itr->second->OutPacket(rPacket.GetOpcode(), rPacket.GetSize(), (rPacket.GetSize() ? (const void*)rPacket.GetBufferPointer() : NULL));
    }
}

void ClientSocketHolder::Update()
{
    //called every 500ms
    std::lock_guard<std::recursive_mutex> rGuard(m_lock);
    if(!m_clientSockets.empty())
    {
        time_t t = UNIXTIME;
        ClientSocket *pSocket;
        ClientSocketMap::iterator itr, itr2;
        
        //ping + process queue
        for(itr = m_clientSockets.begin();itr != m_clientSockets.end();)
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

