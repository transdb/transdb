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
    //if data to send are bigger than sicket buffer split data to chunk and send chunk by chunk
    if(pData->size > g_cfg.SocketWriteBufferSize)
    {
        //stream send
        std::lock_guard<std::recursive_mutex> rGuard(m_lock);
        ClientSocketMap::iterator itr = m_clientSockets.find(socketID);
        if(itr != m_clientSockets.end())
        {
            //create packet header
            PackerHeader rHeader;
            rHeader.m_opcode = opcode;
            rHeader.m_size = (uint32)pData->size;
            OUTPACKET_RESULT result = itr->second->SendRawData(&rHeader, sizeof(rHeader));
            if(result != OUTPACKET_RESULT_SUCCESS)
            {
                Log.Error(__FUNCTION__, "SendRawData failed. Opcode: %u, data size: %u, status: %u", opcode, (uint32)pData->size, (uint32)result);
                return;
            }
            
            //calc chunk size
            size_t chunkSize = g_cfg.SocketWriteBufferSize / 2;
            size_t rpos = 0;
            uint32 sendCounter = 0;
            //start sending chunks
            while(rpos < pData->size)
            {
                size_t remainingData = pData->size - rpos;
                size_t sendDataSize = std::min(remainingData, chunkSize);
                //perform send
            trySendAgain:
                result = itr->second->SendRawData(pData->storage + rpos, sendDataSize);
                if(result == OUTPACKET_RESULT_SUCCESS)
                {
                    //increment rpos
                    rpos += sendDataSize;
                    //send is ok continue with sending
                    continue;
                }
                else if(result == OUTPACKET_RESULT_NO_ROOM_IN_BUFFER)
                {
                    if(sendCounter > 100)
                    {
                        Log.Error(__FUNCTION__, "SendRawData failed: no room in send buffer. Opcode: %u, data size: %u, status: %u", opcode, (uint32)pData->size, (uint32)result);
                        break;
                    }

                    //socket buffer is full wait
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    ++sendCounter;
                    goto trySendAgain;
                }
                else
                {
                    //error interupt sending
                    Log.Error(__FUNCTION__, "SendRawData failed. Opcode: %u, data size: %u, status: %u", opcode, (uint32)pData->size, (uint32)result);
                    break;
                }
            }
        }
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

