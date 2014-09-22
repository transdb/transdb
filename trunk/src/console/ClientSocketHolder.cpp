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

void ClientSocketHolder::SendPacket(uint64 socketID, const StackPacket &rPacket)
{
    std::lock_guard<std::recursive_mutex> rGuard(m_lock);
    ClientSocketMap::iterator itr = m_clientSockets.find(socketID);
    if(itr != m_clientSockets.end())
    {
        itr->second->OutPacket(rPacket.GetOpcode(), rPacket.GetSize(), (rPacket.GetSize() ? (const void*)rPacket.GetBufferPointer() : NULL));
    }
}

void ClientSocketHolder::SendPacket(uint64 socketID, uint16 opcode, bbuff *pData)
{
    //stream send
    if(pData->size > g_cfg.SocketWriteBufferSize)
    {
        //calc chunk size
//        size_t chunkSize = g_cfg.SocketWriteBufferSize / 2;
        
        //send packet header
        {
            std::lock_guard<std::recursive_mutex> rGuard(m_lock);
            ClientSocketMap::iterator itr = m_clientSockets.find(socketID);
            if(itr != m_clientSockets.end())
            {
                //create packet header
                PackerHeader rHeader;
                rHeader.m_opcode = opcode;
                rHeader.m_size = (uint32)pData->size;
                itr->second->SendRawData(&rHeader, sizeof(rHeader));
            }
        }
        
        //start sending chunks
        
        
    }
    else
    {
        std::lock_guard<std::recursive_mutex> rGuard(m_lock);
        ClientSocketMap::iterator itr = m_clientSockets.find(socketID);
        if(itr != m_clientSockets.end())
        {
            itr->second->OutPacket(opcode, pData->size, pData->storage);
        }
    }
}

void ClientSocketHolder::Update()
{
    //called every 500ms
    std::lock_guard<std::recursive_mutex> rGuard(m_lock);
    if(!m_clientSockets.empty())
    {
        time_t t = UNIXTIME;
        ClientSocketMap::iterator itr, itr2;
        
        //ping + process queue
        for(itr = m_clientSockets.begin();itr != m_clientSockets.end();)
        {
            itr2 = itr++;
            ClientSocket *pSocket = itr2->second;
            //
            if(pSocket->m_lastPong < t && ((uint64)(t - pSocket->m_lastPong) > (uint64)g_cfg.PingTimeout))
            {
                Log.Warning(__FUNCTION__, "Connection to client: %s - dropped due to pong timeout.", pSocket->GetRemoteIP().c_str());
                pSocket->Disconnect();
                continue;
            }
            
			if((uint64)(t - pSocket->m_lastPing) > (uint64)g_cfg.PingSendInterval)
            {
                // send a ping packet.
                pSocket->SendPing();
            }
            
            //process queue
            pSocket->ProcessQueue();
        }
    }
}

