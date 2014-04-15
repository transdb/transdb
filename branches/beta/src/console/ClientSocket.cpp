//
//  ClientSocket.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/20/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

typedef void (ClientSocket::*ClientSocketHandler)(ByteBuffer&);
static const ClientSocketHandler m_ClientSocketHandlers[OP_NUM] =
{
    NULL,                                       //C_NULL_OP
	&ClientSocket::QueuePacket,                 //C_MSG_WRITE_DATA		= 1,
	NULL,										//S_MSG_WRITE_DATA		= 2,
	&ClientSocket::QueuePacket,                 //C_MSG_READ_DATA		= 3,
	NULL,										//S_MSG_READ_DATA		= 4,
	&ClientSocket::QueuePacket,                 //C_MSG_DELETE_DATA		= 5,
	NULL,										//S_MSG_DELETE_DATA		= 6,
    &ClientSocket::QueuePacket,                 //C_MSG_GET_ALL_X       = 7,
    NULL,                                       //S_MSG_GET_ALL_X       = 8,
    &ClientSocket::HandlePong,                  //C_MSG_PONG            = 9,
    NULL,                                       //S_MSG_PING            = 10,
    &ClientSocket::QueuePacket,                 //C_MSG_GET_ALL_Y       = 11,
    NULL,                                       //S_MSG_GET_ALL_Y       = 12,
	&ClientSocket::HandleStatus,				//C_MSG_STATUS			= 13,
	NULL,										//S_MSG_STATUS			= 14,
    &ClientSocket::HandleGetActivityID,         //C_MSG_GET_ACTIVITY_ID = 15,
    NULL,                                       //S_MSG_GET_ACTIVITY_ID = 16,
    &ClientSocket::QueuePacket,                 //C_MSG_DELETE_X        = 17,
    NULL,                                       //S_MSG_DELETE_X        = 18,
};

static const char *g_OpcodeNames[OP_NUM] =
{
    "OP_NULL",
    
    "C_MSG_WRITE_DATA",
    "S_MSG_WRITE_DATA",
    
    "C_MSG_READ_DATA",
    "S_MSG_READ_DATA",
    
    "C_MSG_DELETE_DATA",
    "S_MSG_DELETE_DATA",
    
    "C_MSG_GET_ALL_X",
    "S_MSG_GET_ALL_X",
    
    "C_MSG_PONG",
    "S_MSG_PING",
    
    "C_MSG_GET_ALL_Y",
    "S_MSG_GET_ALL_Y",

	"C_MSG_STATUS",
	"S_MSG_STATUS",
    
    "C_MSG_GET_ACTIVITY_ID",
    "S_MSG_GET_ACTIVITY_ID",
    
    "C_MSG_DELETE_X",
    "S_MSG_DELETE_X",
};

ClientSocket::ClientSocket(SOCKET fd) : Socket(fd, g_SocketReadBufferSize, g_SocketWriteBufferSize)
{
    m_socketID      = Sync_Add(&g_SocketID);
    m_size          = 0;
    m_opcode        = 0;
    m_lastPong      = UNIXTIME;
    m_lastPing      = UNIXTIME;
    m_latency       = 0;
    m_nagleEnabled  = false;
}

ClientSocket::~ClientSocket()
{
    Packet * pPacket;
    //delete packets in queue
    LockingPtr<PacketQueue> pPacketQueue(m_packetQueue, m_packetQueueLock);
    while(pPacketQueue->size())
    {
        pPacket = pPacketQueue->front();
        pPacketQueue->pop();
        delete pPacket;
    }
    
    //remove from holder
    g_rClientSocketHolder.RemoveSocket(this);
}

void ClientSocket::OnRead()
{
    for(;;)
    {
        if(m_size == 0)
        {
            //wait for header
            if(m_readBuffer.GetSize() < sizeof(PackerHeader))
                return;
            
            //read header
            PackerHeader rHeader;
            m_readBuffer.Read(&rHeader, sizeof(PackerHeader));
            
            //set variables
            m_size = rHeader.m_size;
            m_opcode = rHeader.m_opcode;
        }
        
        //wait for whole packet
        if(m_readBuffer.GetSize() < m_size)
            return;

        //stats
        g_ReceivedBytes += (m_size + sizeof(PackerHeader));
        
        //prepare buffer
        m_rLocalBuffer.resize(m_size);
        
        //fill buffer with data
        if(m_size)
        {
            m_readBuffer.Read((void*)m_rLocalBuffer.contents(), m_size);
        }
        
        //process packet
        if(m_opcode < OP_NUM && m_ClientSocketHandlers[m_opcode] != NULL)
        {
            //log
            Log.Debug(__FUNCTION__, "Received packet opcode: (0x%.4X), name: %s, size: %u", m_opcode, g_OpcodeNames[m_opcode], m_size);
            //process
            (void)(this->*m_ClientSocketHandlers[m_opcode])(m_rLocalBuffer);
        }
        else
        {
            Log.Debug(__FUNCTION__, "Unknown opcode (0x%.4X)", m_opcode);
        }
        
        //clear variables
        m_size = m_opcode = 0;
    }
}

void ClientSocket::OnConnect()
{
    g_rClientSocketHolder.AddSocket(this);
}

void ClientSocket::OnDisconnect()
{
    g_rClientSocketHolder.RemoveSocket(this);
}

void ClientSocket::SendPacket(const Packet &rPacket)
{
	return OutPacket(rPacket.GetOpcode(), rPacket.size(), (rPacket.size() ? (const void*)rPacket.contents() : NULL));
}

void ClientSocket::SendPacket(const StackPacket &rPacket)
{
	return OutPacket(rPacket.GetOpcode(), rPacket.GetSize(), (rPacket.GetSize() ? (const void*)rPacket.GetBufferPointer() : NULL));
}

void ClientSocket::OutPacket(const uint16 &opcode, const size_t &len, const void* data)
{
    OUTPACKET_RESULT result;
    //try to send
    result = _OutPacket(opcode, len, data);
    Log.Debug(__FUNCTION__, "_OutPacket result: %u", (uint32)result);
    if(result == SocketEnums::OUTPACKET_RESULT_SUCCESS)
        return;
    
    //no room for send create copy of packet and queue
	if(result == OUTPACKET_RESULT_NO_ROOM_IN_BUFFER)
	{
		/* queue the packet */
		Packet * pPacket = new Packet(opcode, len);        
		if(len)
		{
			pPacket->append((const uint8*)data, len);
		}
        
        LockingPtr<PacketQueue> pPacketQueue(m_packetQueue, m_packetQueueLock);
		pPacketQueue->push(pPacket);
	}
}

uint64 ClientSocket::GetQueueSize()
{
    LockingPtr<PacketQueue> pPacketQueue(m_packetQueue, m_packetQueueLock);
    return pPacketQueue->size();
}

void ClientSocket::ProcessQueue()
{
	Packet * pPacket;
    
    LockingPtr<PacketQueue> pPacketQueue(m_packetQueue, m_packetQueueLock);    
    while(pPacketQueue->size())
    {
        //get packet
        pPacket = pPacketQueue->front();
		/* try to push out as many as you can */
		switch(_OutPacket(pPacket->GetOpcode(), pPacket->size(), pPacket->size() ? pPacket->contents() : NULL))
		{
            case OUTPACKET_RESULT_SUCCESS:
			{
   				pPacketQueue->pop();
				delete pPacket;
			}break;
            case OUTPACKET_RESULT_NO_ROOM_IN_BUFFER:
			{
				/* still connected */
				return;
			}break;
            default:
			{
                /* kill everything in the buffer */
                while(pPacketQueue->size())
                {
                    pPacket = pPacketQueue->front();
                    pPacketQueue->pop();
                    delete pPacket;
                }
			}break;
		}
    }
}

OUTPACKET_RESULT ClientSocket::_OutPacket(const uint16 &opcode, const size_t &len, const void* data)
{
	bool rv;
	if(!IsConnected())
		return OUTPACKET_RESULT_NOT_CONNECTED;
    
	BurstBegin();
	if(m_writeBuffer.GetSpace() < (len + sizeof(PackerHeader)))
	{
		BurstEnd();
		return OUTPACKET_RESULT_NO_ROOM_IN_BUFFER;
	}
    
	//log
    Log.Debug(__FUNCTION__, "Sended packet opcode: (0x%.4X), name: %s, size: %u", opcode, g_OpcodeNames[opcode], (uint32)len);
    
    //stats
    g_SendedBytes += (len + sizeof(PackerHeader));
    
	//create header
	PackerHeader rHeader;
	rHeader.m_opcode = opcode;
	rHeader.m_size = static_cast<uint32>(len);
    
	// Pass the header to our send buffer
	rv = BurstSend((const uint8*)&rHeader, sizeof(PackerHeader));
    
	// Pass the rest of the packet to our send buffer (if there is any)
	if(len > 0 && rv)
	{
		rv = BurstSend((const uint8*)data, len);
	}
    
	if(rv)
	{
		BurstPush();
	}
    
	BurstEnd();
    
	return rv ? OUTPACKET_RESULT_SUCCESS : OUTPACKET_RESULT_SOCKET_ERROR;
}

void ClientSocket::QueuePacket(ByteBuffer &rPacket)
{
    g_pClientSocketWorker->QueuePacket(m_opcode, m_socketID, rPacket);
}

void ClientSocket::HandleStatus(ByteBuffer &rPacket)
{
    uint32 token;
    uint32 flags;
    ByteBuffer rBuff;
    
    //read data from packet
    rPacket >> token >> flags;
    
    //get stats
    g_pStorage->GetStats(rBuff);
    
	//for compresion
	int compressionStatus = Z_ERRNO;
	ByteBuffer rBuffOut;
    
	//try to compress
	if(rBuff.size() > (uint32)g_DataSizeForCompression)
	{
		compressionStatus = Common::compressGzip(rBuff.contents(), rBuff.size(), rBuffOut);
		if(compressionStatus == Z_OK)
		{
			Log.Debug(__FUNCTION__, "Data compressed. Original size: %u, new size: %u", rBuff.size(), rBuffOut.size());
			flags = ePF_COMPRESSED;
			rBuff.clear();
            rBuff.append(rBuffOut);
		}
	}
    
    //send
    Packet rResponse(S_MSG_STATUS, 128);
    rResponse << token;
    rResponse << flags;
    rResponse.append(rBuff);
    SendPacket(rResponse);
}

void ClientSocket::HandlePong(ByteBuffer &rPacket)
{
    uint64 tickCount;
    rPacket >> tickCount;
    
    //calc latency
    m_latency = static_cast<uint32>(GetTickCount64() - tickCount);
    m_lastPong = UNIXTIME;
    
	// Dynamically change nagle buffering status based on latency.
	if(m_latency >= 250)
	{
		if(!m_nagleEnabled)
		{
			SocketOps::EnableBuffering(m_fd);
			m_nagleEnabled = true;
		}
	}
	else
	{
		if(m_nagleEnabled)
		{
			SocketOps::DisableBuffering(m_fd);
			m_nagleEnabled = false;
		}
	}
    
    Log.Debug(__FUNCTION__, "Received PONG latency: %u, Nagle buffering: %u.", m_latency, (uint32)m_nagleEnabled);
}

void ClientSocket::HandleGetActivityID(ByteBuffer &rPacket)
{
    uint32 token;
    uint32 flags;
    
    //read data from packet
    rPacket >> token >> flags;
    
    //send
    Packet rResponse(S_MSG_GET_ACTIVITY_ID, 32);
    rResponse << token;
    rResponse << flags;
    rResponse << g_ActivityID;
    SendPacket(rResponse);
}

void ClientSocket::SendPing()
{
    uint64 tickCount;
    
    tickCount = GetTickCount64();
    m_lastPing = UNIXTIME;
    
    //send ping
    OutPacket(S_MSG_PING, sizeof(tickCount), &tickCount);
}























