//
//  ClientSocket.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/20/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "main.h"

typedef void (ClientSocket::*ClientSocketHandler)(ByteBuffer&);
static const ClientSocketHandler m_ClientSocketHandlers[OP_NUM] =
{
    NULL,                                       //C_NULL_OP
	NULL,                                       //C_MSG_WRITE_DATA		= 1,
	&ClientSocket::HandleWriteData,				//S_MSG_WRITE_DATA		= 2,
	NULL,                                       //C_MSG_READ_DATA		= 3,
	&ClientSocket::HandleReadData,				//S_MSG_READ_DATA		= 4,
	NULL,                                       //C_MSG_DELETE_DATA		= 5,
	&ClientSocket::HandleDeleteData,			//S_MSG_DELETE_DATA		= 6,
    NULL,                                       //C_MSG_GET_ALL_X       = 7,
    &ClientSocket::HandleGetAllX,               //S_MSG_GET_ALL_X       = 8,
    NULL,                                       //C_MSG_PONG            = 9,
    &ClientSocket::HandlePing,                  //S_MSG_PING            = 10,
    NULL,                                       //C_MSG_GET_ALL_Y       = 11,
    &ClientSocket::HandleGetAllY,               //S_MSG_GET_ALL_Y       = 12,
	NULL,                                       //C_MSG_STATUS			= 13,
	&ClientSocket::HandleStatus,				//S_MSG_STATUS			= 14,
    NULL,                                       //C_MSG_GET_ACTIVITY_ID = 15,
    &ClientSocket::HandleGetActivityID,         //S_MSG_GET_ACTIVITY_ID = 16,
    NULL,                                       //C_MSG_DELETE_X        = 17,
    &ClientSocket::HandleNull,                  //S_MSG_DELETE_X        = 18,
};

static int decompressGzip(uint8 *pData, size_t dataLen, ByteBuffer &rBuffOut)
{
    int ret;
    size_t processed;
    uInt buffSize = 16*1024;
    uint8 *outBuff = new uint8[buffSize];
    
    //set up stream
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree  = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = 0;
    stream.next_in = Z_NULL;
    
    //init for gzip
    ret = inflateInit2(&stream, 16+MAX_WBITS);
    if(ret != Z_OK)
    {
        delete [] outBuff;
        return ret;
    }
    
    /* decompress until deflate stream ends or end of file */
    stream.avail_in = static_cast<uInt>(dataLen);
    stream.next_in = pData;
    
    /* run inflate() on input until output buffer not full */
    do
    {
        stream.avail_out = buffSize;
        stream.next_out = outBuff;
        ret = inflate(&stream, Z_FINISH);
        switch (ret)
        {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;     /* and fall through */
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
            {
                delete [] outBuff;
                inflateEnd(&stream);
                return ret;
            }
        }
        
        processed = buffSize - stream.avail_out;
        rBuffOut.append(outBuff, processed);
        
    }while(stream.avail_out == 0);
    
    /* clean up and return */
    delete [] outBuff;
    inflateEnd(&stream);
    return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

ClientSocket::ClientSocket(SOCKET fd) : Socket(fd, 64*1024*1024, 64*1024*1024)
{
    m_size      = 0;
    m_opcode    = 0;
}

ClientSocket::~ClientSocket()
{
    
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
        
//        DEBUG_LOG(__FUNCTION__, "Received packet opcode: %u, size: %u", m_opcode, m_size);
        
        //create packet
        m_rLocalBuffer.resize(m_size);
        if(m_size)
        {
            m_readBuffer.Read((void*)m_rLocalBuffer.contents(), m_size);
        }
        
        //processpacket
        if(m_opcode < OP_NUM && m_ClientSocketHandlers[m_opcode] != NULL)
            (void)(this->*m_ClientSocketHandlers[m_opcode])(m_rLocalBuffer);
        else
            Log.Debug(__FUNCTION__, "Unknown opcode (0x%.4X)", m_opcode);
        
        //clear variables
        m_size = m_opcode = 0;
    }
}

void ClientSocket::OnConnect()
{
    
}

void ClientSocket::OnDisconnect()
{
    
}

OUTPACKET_RESULT ClientSocket::SendPacket(Packet &rPacket)
{
	return OutPacket(rPacket.GetOpcode(), rPacket.size(), (rPacket.size() ? (const void*)rPacket.contents() : NULL));
}

OUTPACKET_RESULT ClientSocket::SendPacket(StackPacket &rPacket)
{
	return OutPacket(rPacket.GetOpcode(), rPacket.GetSize(), (rPacket.GetSize() ? (const void*)rPacket.GetBufferPointer() : NULL));
}

OUTPACKET_RESULT ClientSocket::OutPacket(uint16 opcode, size_t len, const void* data)
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
//	DEBUG_LOG(__FUNCTION__, "Sended packet opcode: %u (0x%.4X) size: %u", opcode, opcode, (uint32)len);
    
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
    uint64 x;
    rPacket >> token >> flags >> x;
    
//    g_HasResponse = true;
    
    if(!(token % 1000))
	{
		printf("%u\n", token);
	}
    
//    Packet rDPacket(C_MSG_DELETE_X, 1024);
//    rDPacket << uint32(token);
//    rDPacket << uint32(0);
//    rDPacket << uint64(x);
//    while(SendPacket(rDPacket) != OUTPACKET_RESULT_SUCCESS)
//    {
//        Sleep(100);
//    }
}

void ClientSocket::HandleReadData(ByteBuffer &rPacket)
{
    uint32 tokem;
    uint32 flags;
    uint64 userID;
    uint64 timeStamp;
    ByteBuffer rBuffOut;
    
    rPacket >> tokem >> flags >> userID >> timeStamp;
    
    if(flags & 1)
    {
        int status;
        uint8 *pData;
        size_t len;
        pData = (uint8*)(rPacket.contents()+rPacket.rpos());
        len = rPacket.size() - rPacket.rpos();
        
        status = decompressGzip(pData, len, rBuffOut);
    }
    
    uint64 y;
    uint16 recordSize;
    uint8 *pRecord;
    
    while(rBuffOut.rpos() < rBuffOut.size())
    {
        rBuffOut >> y;
        rBuffOut >> recordSize;
        pRecord = (uint8*)malloc(recordSize);
        rBuffOut.read(pRecord, recordSize);
        free(pRecord);
        
        printf(I64FMTD"\n", y);
    }
}

void ClientSocket::HandleDeleteData(ByteBuffer &rPacket)
{

}

void ClientSocket::HandleGetAllX(ByteBuffer &rPacket)
{
    uint32 token;
    uint32 flags;
    
    rPacket >> token >> flags;
    
    printf("%lu\n", rPacket.size());    
}

void ClientSocket::HandlePing(ByteBuffer &rPacket)
{
//    uint64 tickCount;
//    rPacket >> tickCount;
//    
//    while(OutPacket(C_MSG_PONG, sizeof(tickCount), &tickCount) != OUTPACKET_RESULT_SUCCESS)
//    {
//        std::this_thread::sleep_for(std::chrono::milliseconds(100));
//    }
}

void ClientSocket::HandleGetAllY(ByteBuffer &rPacket)
{
    uint32 tokem;
    uint32 flags;
    uint64 userID;
    ByteBuffer rBuffOut;
    
    rPacket >> tokem >> flags >> userID;
    
    if(flags & 1)
    {
        int status;
        uint8 *pData;
        size_t len;
        pData = (uint8*)(rPacket.contents()+rPacket.rpos());
        len = rPacket.size() - rPacket.rpos();
        
        status = decompressGzip(pData, len, rBuffOut);
    }
    
    uint64 y;
    while(rBuffOut.rpos() < rBuffOut.size())
    {
        rBuffOut >> y;
        printf(I64FMTD"\n", y);
    }
}

void ClientSocket::HandleStatus(ByteBuffer &rPacket)
{
    uint32 token;
    uint32 flags;
    std::string sStats;
    ByteBuffer rBuffOut;


    
    rPacket >> token >> flags;
    
    if(flags & 1)
    {
        int status;
        uint8 *pData;
        size_t len;
        pData = (uint8*)(rPacket.contents()+rPacket.rpos());
        len = rPacket.size() - rPacket.rpos();
        
        status = decompressGzip(pData, len, rBuffOut);
        
        rBuffOut >> sStats;
    }
    else
    {        
        rPacket >> sStats;
    }

    Log.Notice(__FUNCTION__, "%s", sStats.c_str());
}

void ClientSocket::HandleGetActivityID(ByteBuffer &rPacket)
{
    
}
























