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
	&ClientSocket::HandleWriteData,				//C_MSG_WRITE_DATA		= 1,
	NULL,										//S_MSG_WRITE_DATA		= 2,
	&ClientSocket::HandleReadData,              //C_MSG_READ_DATA		= 3,
	NULL,										//S_MSG_READ_DATA		= 4,
	&ClientSocket::HandleDeleteData,			//C_MSG_DELETE_DATA		= 5,
	NULL,										//S_MSG_DELETE_DATA		= 6,
    &ClientSocket::HandleGetAllX,               //C_MSG_GET_ALL_X       = 7,
    NULL,                                       //S_MSG_GET_ALL_X       = 8,
    &ClientSocket::HandlePong,                  //C_MSG_PONG            = 9,
    NULL,                                       //S_MSG_PING            = 10,
    &ClientSocket::HandleGetAllY,               //C_MSG_GET_ALL_Y       = 11,
    NULL,                                       //S_MSG_GET_ALL_Y       = 12,
	&ClientSocket::HandleStatus,				//C_MSG_STATUS			= 13,
	NULL,										//S_MSG_STATUS			= 14,
    &ClientSocket::HandleGetActivityID,         //C_MSG_GET_ACTIVITY_ID = 15,
    NULL,                                       //S_MSG_GET_ACTIVITY_ID = 16,
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
};

ClientSocket::ClientSocket(SOCKET fd) : Socket(fd, g_SocketReadBufferSize, g_SocketWriteBufferSize)
{
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

void ClientSocket::SendPacket(Packet &rPacket)
{
	return OutPacket(rPacket.GetOpcode(), rPacket.size(), (rPacket.size() ? (const void*)rPacket.contents() : NULL));
}

void ClientSocket::SendPacket(StackPacket &rPacket)
{
	return OutPacket(rPacket.GetOpcode(), rPacket.GetSize(), (rPacket.GetSize() ? (const void*)rPacket.GetBufferPointer() : NULL));
}

void ClientSocket::OutPacket(uint16 opcode, size_t len, const void* data)
{
    OUTPACKET_RESULT result;
    //try to send
    result = _OutPacket(opcode, len, data);
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

void ClientSocket::HandleWriteData(ByteBuffer &rPacket)
{
    uint32 token;
    uint32 flags;
    uint64 userID;
    uint64 timeStamp;
    uint8 *pRecord;
    uint16 recordSize;
    uint32 writeStatus;
    
    //read data from packet
    rPacket >> token >> flags >> userID >> timeStamp;
    recordSize = (uint16)(rPacket.size() - rPacket.rpos());
    pRecord = (uint8*)rPacket.contents() + rPacket.rpos();
    
    writeStatus = g_pStorage->WriteData(userID, timeStamp, pRecord, recordSize);
    //
    //send back data
    uint8 buff[32];
    StackPacket rResponse(S_MSG_WRITE_DATA, buff, sizeof(buff));
    rResponse << token;
    rResponse << flags;
    rResponse << userID;
    rResponse << timeStamp;
    rResponse << writeStatus;
    SendPacket(rResponse);
}

void ClientSocket::HandleReadData(ByteBuffer &rPacket)
{
    uint32 token;    
    uint32 flags;
    uint64 userID;
    uint64 timeStamp;
    uint8 *pRecord;
    uint32 recordSize;
    size_t packetSize = sizeof(token)+sizeof(flags)+sizeof(userID)+sizeof(timeStamp);

	//for compresion
	int compressionStatus = Z_ERRNO;
	ByteBuffer rBuffOut;

    //read data from packet
    rPacket >> token >> flags >> userID >> timeStamp;
    
    //read data - if timeStamp == 0 then read all datas under userID
    if(timeStamp == 0)
    {
        g_pStorage->ReadData(userID, &pRecord, &recordSize);
    }
    else
    {
        uint16 recordSize16;
        g_pStorage->ReadData(userID, timeStamp, &pRecord, &recordSize16);
        recordSize = recordSize16;
    }
    
	//try to compress
	if(recordSize > (uint32)g_DataSizeForCompression)
	{
		compressionStatus = Common::compressGzip(pRecord, recordSize, rBuffOut);
		if(compressionStatus == Z_OK)
		{
			Log.Debug(__FUNCTION__, "Data compressed. Original size: %u, new size: %u", recordSize, rBuffOut.size());
			flags = ePF_COMPRESSED;
			recordSize = static_cast<uint32>(rBuffOut.size());
		}
	}

    //send back data
    Packet rResponse(S_MSG_READ_DATA, packetSize+recordSize);
    rResponse << token;
    rResponse << flags;
    rResponse << userID;
    rResponse << timeStamp;
	if(recordSize != 0)
    {
		if(compressionStatus == Z_OK)
			rResponse.append(rBuffOut);
		else
			rResponse.append(pRecord, recordSize);

        aligned_free(pRecord);
    }
    SendPacket(rResponse);
}

void ClientSocket::HandleDeleteData(ByteBuffer &rPacket)
{
    uint32 token;   
    uint32 flags;
    uint64 userID;
    uint64 timeStamp;
    
    //read data from packet
    rPacket >> token >> flags >> userID >> timeStamp;
    
    //read data - if timeStamp == 0 then delete all datas under userID
    if(timeStamp == 0)
    {
        g_pStorage->DeleteData(userID);
    }
    else
    {
        g_pStorage->DeleteData(userID, timeStamp);
    }
    
    //send back data
    uint8 buff[32];
    StackPacket rResponse(S_MSG_DELETE_DATA, buff, sizeof(buff));
    rResponse << token;
    rResponse << flags;
    rResponse << userID;
    rResponse << timeStamp;
    SendPacket(rResponse);
}

void ClientSocket::HandleGetAllX(ByteBuffer &rPacket)
{
    uint32 token;
    uint32 flags;
    uint8 *pX;
    uint64 xSize;
    size_t packetSize = sizeof(token)+sizeof(flags);

	//for compresion
	int compressionStatus = Z_ERRNO;
	ByteBuffer rBuffOut;
    
    //read data from packet
    rPacket >> token >> flags;
    
    //get all X
    g_pStorage->GetAllX(&pX, &xSize);
    
	//try to compress
	if(xSize > (uint32)g_DataSizeForCompression)
	{
		compressionStatus = Common::compressGzip(pX, xSize, rBuffOut);
		if(compressionStatus == Z_OK)
		{
			Log.Debug(__FUNCTION__, "Data compressed. Original size: %u, new size: %u", xSize, rBuffOut.size());
			flags = ePF_COMPRESSED;
			xSize = static_cast<uint32>(rBuffOut.size());
		}
	}

    //send back data
    Packet rResponse(S_MSG_GET_ALL_X, xSize+packetSize);
    rResponse << token;
    rResponse << flags;
    if(xSize)
    {
		if(compressionStatus == Z_OK)
			rResponse.append(rBuffOut);
		else
			rResponse.append(pX, xSize);

        aligned_free(pX);
    }
    SendPacket(rResponse);
}

void ClientSocket::HandleGetAllY(ByteBuffer &rPacket)
{
    uint32 token;
    uint32 flags;
    uint64 userID;
    uint8 *pY;
    uint64 ySize;
    size_t packetSize = sizeof(token)+sizeof(flags)+sizeof(ySize);
    
	//for compresion
	int compressionStatus = Z_ERRNO;
	ByteBuffer rBuffOut;
    
    //read data from packet
    rPacket >> token >> flags >> userID;
    
    //get all Y
    g_pStorage->GetAllY(userID, &pY, &ySize);
    
	//try to compress
	if(ySize > (uint32)g_DataSizeForCompression)
	{
		compressionStatus = Common::compressGzip(pY, ySize, rBuffOut);
		if(compressionStatus == Z_OK)
		{
			Log.Debug(__FUNCTION__, "Data compressed. Original size: %u, new size: %u", ySize, rBuffOut.size());
			flags = ePF_COMPRESSED;
			ySize = static_cast<uint64>(rBuffOut.size());
		}
	}
    
    //send back data
    Packet rResponse(S_MSG_GET_ALL_Y, ySize+packetSize);
    rResponse << token;
    rResponse << flags;
    rResponse << userID;
    if(ySize)
    {
		if(compressionStatus == Z_OK)
			rResponse.append(rBuffOut);
		else
			rResponse.append(pY, ySize);
        
        aligned_free(pY);
    }
    SendPacket(rResponse);
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
		compressionStatus = Common::compressGzip((uint8*)rBuff.contents(), rBuff.size(), rBuffOut);
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























