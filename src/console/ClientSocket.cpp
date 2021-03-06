//
//  ClientSocket.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/20/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

typedef void (ClientSocket::*ClientSocketHandler)(bbuff*);
static const ClientSocketHandler m_ClientSocketHandlers[OP_NUM] =
{
    NULL,                                       //C_NULL_OP
	&ClientSocket::QueueWritePacket,            //C_MSG_WRITE_DATA              = 1,
	NULL,										//S_MSG_WRITE_DATA              = 2,
	&ClientSocket::QueueReadPacket,             //C_MSG_READ_DATA               = 3,
	NULL,										//S_MSG_READ_DATA               = 4,
	&ClientSocket::QueueWritePacket,            //C_MSG_DELETE_DATA             = 5,
	NULL,										//S_MSG_DELETE_DATA             = 6,
    &ClientSocket::QueueWritePacket,            //C_MSG_GET_ALL_X               = 7,
    NULL,                                       //S_MSG_GET_ALL_X               = 8,
    &ClientSocket::HandlePong,                  //C_MSG_PONG                    = 9,
    NULL,                                       //S_MSG_PING                    = 10,
    &ClientSocket::QueueWritePacket,            //C_MSG_GET_ALL_Y               = 11,
    NULL,                                       //S_MSG_GET_ALL_Y               = 12,
	&ClientSocket::QueueReadPacket,				//C_MSG_STATUS                  = 13,
	NULL,										//S_MSG_STATUS                  = 14,
    &ClientSocket::HandleGetActivityID,         //C_MSG_GET_ACTIVITY_ID         = 15,
    NULL,                                       //S_MSG_GET_ACTIVITY_ID         = 16,
    &ClientSocket::QueueWritePacket,            //C_MSG_DELETE_X                = 17,
    NULL,                                       //S_MSG_DELETE_X                = 18,
    &ClientSocket::QueueWritePacket,            //C_MSG_DEFRAMENT_DATA          = 19,
    NULL,                                       //S_MSG_DEFRAMENT_DATA          = 20,
    &ClientSocket::QueueWritePacket,            //C_MSG_GET_FREESPACE           = 21,
    NULL,                                       //S_MSG_GET_FREESPACE           = 22,
    &ClientSocket::QueueWritePacket,            //C_MSG_WRITE_DATA_NUM          = 23,
    NULL,                                       //S_MSG_WRITE_DATA_NUM          = 24,
    &ClientSocket::QueueReadPacket,             //C_MSG_READ_LOG                = 25,
    NULL,                                       //S_MSG_READ_LOG                = 26,
    &ClientSocket::QueueReadPacket,             //C_MSG_READ_CONFIG             = 27,
    NULL,                                       //S_MSG_READ_CONFIG             = 28,
    &ClientSocket::QueueReadPacket,             //C_MSG_DEFRAGMENT_FREESPACE    = 29,
    NULL,                                       //S_MSG_DEFRAGMENT_FREESPACE    = 30,
    &ClientSocket::QueueReadPacket,             //C_MSG_EXEC_PYTHON_SCRIPT      = 31,
    NULL,                                       //S_MSG_EXEC_PYTHON_SCRIPT      = 32,
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
    
    "C_MSG_DEFRAMENT_DATA",
    "S_MSG_DEFRAMENT_DATA",
    
    "C_MSG_GET_FREESPACE",
    "S_MSG_GET_FREESPACE",
    
    "C_MSG_WRITE_DATA_NUM",
    "S_MSG_WRITE_DATA_NUM",
    
    "C_MSG_READ_LOG",
    "S_MSG_READ_LOG",
    
    "C_MSG_READ_CONFIG",
    "S_MSG_READ_CONFIG",
    
    "C_MSG_DEFRAGMENT_FREESPACE",
    "S_MSG_DEFRAGMENT_FREESPACE",
    
    "C_MSG_EXEC_PYTHON_SCRIPT",
    "S_MSG_EXEC_PYTHON_SCRIPT",
};

ClientSocket::ClientSocket(SOCKET fd) : Socket(fd, g_cfg.SocketReadBufferSize, g_cfg.SocketWriteBufferSize), m_lastPong(UNIXTIME), m_lastPing(UNIXTIME)
{
    //socket ID gen
    static std::atomic<uint64> rSocketID(0);
    //init default values
    m_socketID      = (rSocketID++);
    m_latency       = 0;
    m_size          = 0;
    m_opcode        = 0;
    m_nagleEnabled  = false;
    bbuff_create(m_pReceiveBuff);
}

ClientSocket::~ClientSocket()
{
    //delete packets in queue
    LockingPtr<PacketQueue> pPacketQueue(m_packetQueue, m_packetQueueLock);
    while(pPacketQueue->size())
    {
        Packet *pPacket = pPacketQueue->front();
        pPacketQueue->pop();
        delete pPacket;
    }
    
    //destroy receive buffer
    bbuff_destroy(m_pReceiveBuff);
    
    //remove from holder
    g_rClientSocketHolder.RemoveSocket(this);
}

void ClientSocket::OnRead()
{
    try
    {
#ifdef WIN32
        _set_se_translator(trans_func);
#endif
        
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
                
                //prepare buffer
                bbuff_reserve(m_pReceiveBuff, m_size);
            }
            
            //append data to internal buffer
            size_t remainingBytes = m_size - m_pReceiveBuff->wpos;
            size_t readBytes = std::min(remainingBytes, m_readBuffer.GetContiguiousBytes());
            bbuff_append(m_pReceiveBuff, m_readBuffer.GetBufferStart(), readBytes);
            m_readBuffer.Remove(readBytes);
            
            //wait for whole packet
            if(m_pReceiveBuff->size < m_size)
                return;

            //stats
            g_stats.ReceivedBytes += (m_size + sizeof(PackerHeader));
            
            //process packet
            if(m_opcode < OP_NUM && m_ClientSocketHandlers[m_opcode] != NULL)
            {
                Log.Debug(__FUNCTION__, "Received packet opcode: (0x%.4X), name: %s, size: %u", m_opcode, g_OpcodeNames[m_opcode], m_size);
                (void)(this->*m_ClientSocketHandlers[m_opcode])(m_pReceiveBuff);
            }
            else
            {
                Log.Warning(__FUNCTION__, "Unknown opcode (0x%.4X)", m_opcode);
            }
            
            //clear variables
            m_size = m_opcode = 0;
            bbuff_clear(m_pReceiveBuff);
        }
    }
    catch(...)
    {
        Log.Error(__FUNCTION__, "Fatal error stopping server.");
        g_stopEvent = true;
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

void ClientSocket::OutPacket(uint16 opcode, size_t len, const void* data)
{
    //try to send
    OUTPACKET_RESULT result = _OutPacket(opcode, len, data);
    if(result == SocketEnums::OUTPACKET_RESULT_SUCCESS)
        return;
    
    //no room for send create copy of packet and queue
	if(result == OUTPACKET_RESULT_NO_ROOM_IN_BUFFER)
	{
		/* queue the packet */
		Packet * pPacket = new Packet(opcode, len);        
		if(data && len)
		{
			pPacket->append((const uint8*)data, len);
		}
        
        LockingPtr<PacketQueue> pPacketQueue(m_packetQueue, m_packetQueueLock);
		pPacketQueue->push(pPacket);
	}
}

OUTPACKET_RESULT ClientSocket::SendRawData(const void *dataChunk, size_t chunkSize)
{
	if(!IsConnected())
		return OUTPACKET_RESULT_NOT_CONNECTED;
    
	BurstBegin();
	if(m_writeBuffer.GetSpace() < chunkSize)
	{
		BurstEnd();
		return OUTPACKET_RESULT_NO_ROOM_IN_BUFFER;
	}
    
    //send chunk
	bool rv = BurstSend((const uint8*)dataChunk, chunkSize);
	if(rv)
	{
		BurstPush();
	}
    
	BurstEnd();
    
    return rv ? OUTPACKET_RESULT_SUCCESS : OUTPACKET_RESULT_SOCKET_ERROR;
}

uint64 ClientSocket::GetQueueSize()
{
    LockingPtr<PacketQueue> pPacketQueue(m_packetQueue, m_packetQueueLock);
    return pPacketQueue->size();
}

void ClientSocket::ProcessQueue()
{
    LockingPtr<PacketQueue> pPacketQueue(m_packetQueue, m_packetQueueLock);    
    while(pPacketQueue->size())
    {
        //get packet
        Packet *pPacket = pPacketQueue->front();
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

OUTPACKET_RESULT ClientSocket::_OutPacket(uint16 opcode, size_t len, const void* data)
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
    g_stats.SendedBytes += (len + sizeof(PackerHeader));
    
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

void ClientSocket::QueueWritePacket(bbuff *pData)
{
    g_pClientSocketWorker->QueuePacket(m_opcode, m_socketID, pData, eTQT_Write);
}

void ClientSocket::QueueReadPacket(bbuff *pData)
{
    g_pClientSocketWorker->QueuePacket(m_opcode, m_socketID, pData, eTQT_Read);
}

void ClientSocket::HandlePong(bbuff *pData)
{
    uint64 tickCount;
    
    //read data from packet
    bbuff_read(pData, &tickCount, sizeof(tickCount));
    
    //calc latency
    m_latency = static_cast<uint32>(GetTickCount64() - tickCount);
    m_lastPong = UNIXTIME;
    
	// Dynamically change nagle buffering status based on latency.
	if(m_latency >= (uint32)g_cfg.NagleLatency)
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

void ClientSocket::HandleGetActivityID(bbuff *pData)
{
    uint32 token;
    uint32 flags;
    
    //read data from packet
    bbuff_read(pData, &token, sizeof(token));
    bbuff_read(pData, &flags, sizeof(flags));
    
    //send
    uint8 buff[64];
    StackPacket rResponse(S_MSG_GET_ACTIVITY_ID, buff, sizeof(buff));
    rResponse << token;
    rResponse << flags;
    rResponse << g_cfg.ActivityID;
    OutPacket(rResponse.GetOpcode(), rResponse.GetSize(), rResponse.GetBufferPointer());
}

void ClientSocket::SendPing()
{
    uint64 tickCount;
    
    tickCount = GetTickCount64();
    m_lastPing = UNIXTIME;
    
    //send ping
    uint32 count = 0;
    while(_OutPacket(S_MSG_PING, sizeof(tickCount), &tickCount) != OUTPACKET_RESULT_SUCCESS)
    {
        if(count > 100)
        {
            Log.Error(__FUNCTION__, "Unable to send S_MSG_PING.");
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}























