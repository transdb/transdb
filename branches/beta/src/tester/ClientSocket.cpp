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
    m_lastXSize = 0;
    m_lastg_NumOfReadsFromCache = 0;
    m_lastg_NumOfReadsFromDisk = 0;
    m_lastg_NumOfWritesToDisk = 0;
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
    uint64 tickCount;
    rPacket >> tickCount;
    
    while(OutPacket(C_MSG_PONG, sizeof(tickCount), &tickCount) != OUTPACKET_RESULT_SUCCESS)
    {
        Sleep(100);
    }
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
    
    int64 xsize;
    uint64 memuse;
    uint64 lrusize;
    uint64 lrumempoolsize;
    
    uint64 g_NumOfReadsFromDisk;
    uint64 g_NumOfReadsFromCache;
    uint64 g_NumOfWritesToDisk;
    
    uint64 RecordIndexMempoolSize;
    
    uint64 socketQueue;
    int64 taskQueue;
    
    ByteBuffer rBuffOut;
    uint64 freeSpaceCount;
    uint64 freespacemempoolsize;
    uint64 freeSpaceQueueSize;
    uint64 dataFileSize;

    
    rPacket >> token >> flags;
    
    if(flags & 1)
    {
        int status;
        uint8 *pData;
        size_t len;
        pData = (uint8*)(rPacket.contents()+rPacket.rpos());
        len = rPacket.size() - rPacket.rpos();
        
        status = decompressGzip(pData, len, rBuffOut);
        
        rBuffOut >> xsize;
        rBuffOut >> memuse;
        rBuffOut >> lrusize;
        
        rBuffOut >> lrumempoolsize;
        rBuffOut >> g_NumOfReadsFromDisk;
        rBuffOut >> g_NumOfReadsFromCache;
        rBuffOut >> g_NumOfWritesToDisk;
        rBuffOut >> RecordIndexMempoolSize;
        rBuffOut >> socketQueue;
        rBuffOut >> taskQueue;
        rBuffOut >> freespacemempoolsize >> freeSpaceCount;
        rBuffOut >> freeSpaceQueueSize;
        rBuffOut >> dataFileSize;
    }
    else
    {        
        rPacket >> xsize;
        rPacket >> memuse;
        rPacket >> lrusize;
        
        rPacket >> lrumempoolsize;
        rPacket >> g_NumOfReadsFromDisk;
        rPacket >> g_NumOfReadsFromCache;
        rPacket >> g_NumOfWritesToDisk;
        rPacket >> RecordIndexMempoolSize;
        rPacket >> socketQueue;
        rPacket >> taskQueue;
        rPacket >> freespacemempoolsize >> freeSpaceCount;
        rPacket >> freeSpaceQueueSize;
        rPacket >> dataFileSize;
    }

    Log.Debug(__FUNCTION__, "=============================================");
    Log.Debug(__FUNCTION__, "======== Server stats =======================");
    Log.Debug(__FUNCTION__, "User count:                    "SI64FMTD" ("SI64FMTD" delta/s)", xsize, (xsize-m_lastXSize)/5);
    Log.Debug(__FUNCTION__, "Memory usage:                  "I64FMTD" kB", memuse/1024);
    Log.Debug(__FUNCTION__, "LRU cache size:                "I64FMTD, lrusize);
    Log.Debug(__FUNCTION__, "LRU cache mempool size:        "I64FMTD" kB", lrumempoolsize/1024);
    Log.Debug(__FUNCTION__, "Number of reads from disk:     "I64FMTD" ("I64FMTD" delta/s)", g_NumOfReadsFromDisk, (g_NumOfReadsFromDisk-m_lastg_NumOfReadsFromDisk)/5);
    Log.Debug(__FUNCTION__, "Number of reads from cache:    "I64FMTD" ("I64FMTD" delta/s)", g_NumOfReadsFromCache, (g_NumOfReadsFromCache-m_lastg_NumOfReadsFromCache)/5);
    Log.Debug(__FUNCTION__, "Number of disk writes:         "I64FMTD" ("I64FMTD" delta/s)", g_NumOfWritesToDisk, (g_NumOfWritesToDisk-m_lastg_NumOfWritesToDisk)/5);
    Log.Debug(__FUNCTION__, "Cache hit rate:                %f %%", ((float)g_NumOfReadsFromCache / (float)(g_NumOfReadsFromDisk + g_NumOfReadsFromCache)) * 100.0f);
    Log.Debug(__FUNCTION__, "FreeSpace mempool size:        "I64FMTD" kB", freespacemempoolsize/1024);
    Log.Debug(__FUNCTION__, "FreeSpace count:               "I64FMTD, freeSpaceCount);
    Log.Debug(__FUNCTION__, "FreeSpace queue count:         "I64FMTD, freeSpaceQueueSize);    
    Log.Debug(__FUNCTION__, "Data file size:                "I64FMTD" MB", dataFileSize/(1024*1024));
    Log.Debug(__FUNCTION__, "Socket packet queue size:      "I64FMTD, socketQueue);
    Log.Debug(__FUNCTION__, "Socket task queue size:        "SI64FMTD, taskQueue);    
    Log.Debug(__FUNCTION__, "=============================================");
    
    m_lastXSize = xsize;
    m_lastg_NumOfReadsFromDisk = g_NumOfReadsFromDisk;
    m_lastg_NumOfReadsFromCache = g_NumOfReadsFromCache;
    m_lastg_NumOfWritesToDisk = g_NumOfWritesToDisk;
}

void ClientSocket::HandleGetActivityID(ByteBuffer &rPacket)
{
    
}
























