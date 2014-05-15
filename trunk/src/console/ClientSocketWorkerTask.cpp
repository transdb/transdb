//
//  ClientSocketWorkerTask.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 12.11.12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

typedef void (ClientSocketWorkerTask::*ClientSocketWorkerTaskHandler)(const HANDLE&, ClientSocketTaskData&);
static const ClientSocketWorkerTaskHandler m_ClientSocketWorkerTaskHandlers[OP_NUM] =
{
    NULL,                                           //C_NULL_OP
	&ClientSocketWorkerTask::HandleWriteData,       //C_MSG_WRITE_DATA		= 1,
	NULL,                                           //S_MSG_WRITE_DATA		= 2,
	&ClientSocketWorkerTask::HandleReadData,        //C_MSG_READ_DATA		= 3,
	NULL,                                           //S_MSG_READ_DATA		= 4,
	&ClientSocketWorkerTask::HandleDeleteData,      //C_MSG_DELETE_DATA		= 5,
	NULL,                                           //S_MSG_DELETE_DATA		= 6,
    &ClientSocketWorkerTask::HandleGetAllX,         //C_MSG_GET_ALL_X       = 7,
    NULL,                                           //S_MSG_GET_ALL_X       = 8,
    NULL,                                           //C_MSG_PONG            = 9,
    NULL,                                           //S_MSG_PONG            = 10,
    &ClientSocketWorkerTask::HandleGetAllY,         //C_MSG_GET_ALL_Y       = 11,
    NULL,                                           //S_MSG_GET_ALL_Y       = 12,
    &ClientSocketWorkerTask::HandleStatus,          //C_MSG_STATUS          = 13,
    NULL,                                           //S_MSG_STATUS          = 14,
    NULL,                                           //C_MSG_GET_ACTIVITY_ID = 15,
    NULL,                                           //S_MSG_GET_ACTIVITY_ID = 16,
    &ClientSocketWorkerTask::HandleDeleteX,         //C_MSG_DELETE_X        = 17,
    NULL,                                           //S_MSG_DELETE_X        = 18,
    &ClientSocketWorkerTask::HandleDefragmentData,  //C_MSG_DEFRAMENT_DATA  = 19,
    NULL,                                           //S_MSG_DEFRAMENT_DATA  = 20,
    &ClientSocketWorkerTask::HandleGetFreeSpace,    //C_MSG_GET_FREESPACE   = 21,
    NULL,                                           //S_MSG_GET_FREESPACE   = 22,
    &ClientSocketWorkerTask::HandleWriteDataNum,    //C_MSG_WRITE_DATA_NUM  = 23,
    NULL,                                           //S_MSG_WRITE_DATA_NUM  = 24,
};

ClientSocketWorkerTask::ClientSocketWorkerTask(Storage &rStorage, bool readerTask) : m_rStorage(rStorage), m_readerThread(readerTask)
{

}

bool ClientSocketWorkerTask::run()
{
    if(m_readerThread)
        SetThreadName("Read ClientSocketWorkerTask thread");
    else
        SetThreadName("ClientSocketWorkerTask thread");

    //open file per thread - read only
    HANDLE rDataFileHandle;
    IOHandleGuard rIOHandleGuard(&rDataFileHandle);
    rDataFileHandle = IO::fopen(m_rStorage.m_pDataPath, IO::IO_READ_ONLY);
    assert(rDataFileHandle != INVALID_HANDLE_VALUE);
    
    //
	ClientSocketTaskData *pClientSocketTaskData;
    
    try
    {
#ifdef WIN32
        _set_se_translator(trans_func);
#endif
        
        //main thread loop
        while(m_threadRunning)
        {
            //get task data from queue - abort throws Exception
            try {
                if(m_readerThread)
                    g_pClientSocketWorker->m_rReadTaskDataQueue.pop(pClientSocketTaskData);
                else
                    g_pClientSocketWorker->m_rTaskDataQueue.pop(pClientSocketTaskData);
            }
            catch(tbb::user_abort&) {
                Log.Notice(__FUNCTION__, "Task aborted. ReadTask: %u", (uint32)m_readerThread);
                return true;
            }

            //process task
            if(pClientSocketTaskData->opcode() < OP_NUM && m_ClientSocketWorkerTaskHandlers[pClientSocketTaskData->opcode()] != NULL)
            {
                Log.Debug(__FUNCTION__, "Processing packet opcode: (0x%.4X)", pClientSocketTaskData->opcode());
                (void)(this->*m_ClientSocketWorkerTaskHandlers[pClientSocketTaskData->opcode()])(rDataFileHandle, *pClientSocketTaskData);
            }
            else
            {
                Log.Debug(__FUNCTION__, "Unknown opcode (0x%.4X)", pClientSocketTaskData->opcode());
            }
            
            //call ~ctor + dealloc task data
            pClientSocketTaskData->~ClientSocketTaskData();
			g_pClientSocketWorker->m_pFixedPool->free(pClientSocketTaskData);
        }
    }
    catch(...)
    {
        Log.Error(__FUNCTION__, "Fatal error stopping server.");
        g_pClientSocketWorker->SetException(true);
        g_stopEvent = true;
    }

    return true;
}

void ClientSocketWorkerTask::HandleWriteData(const HANDLE &rDataFileHandle, ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    uint64 userID;
    uint64 timeStamp;
    uint8 *pRecord;
    uint16 recordSize;
    uint32 writeStatus;
    ClientSocket *pClientSocket;
    
    //read data from packet
    rClientSocketTaskData >> token >> flags >> userID >> timeStamp;
    recordSize = (uint16)(rClientSocketTaskData.size() - rClientSocketTaskData.rpos());
    pRecord = (uint8*)(rClientSocketTaskData.contents() + rClientSocketTaskData.rpos());
    //
    writeStatus = m_rStorage.WriteData(rDataFileHandle, userID, timeStamp, pRecord, recordSize);
    
    //get socket
    pClientSocket = g_rClientSocketHolder.GetSocket(rClientSocketTaskData.socketID());
    if(pClientSocket)
    {
        //send back data
        uint8 buff[32];
        StackPacket rResponse(S_MSG_WRITE_DATA, buff, sizeof(buff));
        rResponse << token;
        rResponse << flags;
        rResponse << userID;
        rResponse << timeStamp;
        rResponse << writeStatus;
        pClientSocket->SendPacket(rResponse);
    }
}

void ClientSocketWorkerTask::HandleReadData(const HANDLE &rDataFileHandle, ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    uint64 userID;
    uint64 timeStamp;
    ClientSocket *pClientSocket;
    static const size_t packetSize = sizeof(token)+sizeof(flags)+sizeof(userID)+sizeof(timeStamp);
    
    //buffer for read data
    ByteBuffer rReadData;
    
	//for compresion
	int compressionStatus = Z_ERRNO;
	ByteBuffer rBuffOut;
    
    //read data from packet
    rClientSocketTaskData >> token >> flags >> userID >> timeStamp;
    
    //read data - if timeStamp == 0 then read all datas under userID
    if(timeStamp == 0)
    {
        m_rStorage.ReadData(rDataFileHandle, userID, rReadData);
    }
    else
    {
        m_rStorage.ReadData(rDataFileHandle, userID, timeStamp, rReadData);
    }
    
	//try to compress
	if(rReadData.size() > (uint32)g_DataSizeForCompression)
	{
		compressionStatus = Common::compressGzip(rReadData.contents(), rReadData.size(), rBuffOut);
		if(compressionStatus == Z_OK)
		{
			Log.Debug(__FUNCTION__, "Data compressed. Original size: %u, new size: %u", rReadData.size(), rBuffOut.size());
			flags = ePF_COMPRESSED;
            //add compressed data
            rReadData.clear();
            rReadData.append(rBuffOut);
		}
        else
        {
            Log.Error(__FUNCTION__, "Data compression failed. X: " I64FMTD ", Y: " I64FMTD, userID, timeStamp);
        }
	}
    
    //get socket
    pClientSocket = g_rClientSocketHolder.GetSocket(rClientSocketTaskData.socketID());
    if(pClientSocket)
    {
        //send back data
        Packet rResponse(S_MSG_READ_DATA, packetSize + rReadData.size());
        rResponse << token;
        rResponse << flags;
        rResponse << userID;
        rResponse << timeStamp;
        rResponse.append(rReadData);
        pClientSocket->SendPacket(rResponse);
    }
}

void ClientSocketWorkerTask::HandleDeleteData(const HANDLE &rDataFileHandle, ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    uint64 userID;
    uint64 timeStamp;
    ClientSocket *pClientSocket;
    
    //read data from packet
    rClientSocketTaskData >> token >> flags >> userID >> timeStamp;
    
    //read data - if timeStamp == 0 then delete all datas under userID
    if(timeStamp == 0)
    {
        m_rStorage.DeleteData(userID);
    }
    else
    {
        m_rStorage.DeleteData(rDataFileHandle, userID, timeStamp);
    }
    
    //get socket
    pClientSocket = g_rClientSocketHolder.GetSocket(rClientSocketTaskData.socketID());
    if(pClientSocket)
    {
        //send back data
        uint8 buff[32];
        StackPacket rResponse(S_MSG_DELETE_DATA, buff, sizeof(buff));
        rResponse << token;
        rResponse << flags;
        rResponse << userID;
        rResponse << timeStamp;
        pClientSocket->SendPacket(rResponse);
    }
}

void ClientSocketWorkerTask::HandleGetAllX(const HANDLE &rDataFileHandle, ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    uint8 *pX;
    uint64 xSize;
    ClientSocket *pClientSocket;
    static const size_t packetSize = sizeof(token)+sizeof(flags);
    
	//for compresion
	int compressionStatus = Z_ERRNO;
	ByteBuffer rBuffOut;
    
    //read data from packet
    rClientSocketTaskData >> token >> flags;
    
    //get all X
    m_rStorage.GetAllX(&pX, &xSize);
    
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
    
    //get socket
    pClientSocket = g_rClientSocketHolder.GetSocket(rClientSocketTaskData.socketID());
    if(pClientSocket)
    {
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
            
            free(pX);
        }
        pClientSocket->SendPacket(rResponse);
    }
}

void ClientSocketWorkerTask::HandleGetAllY(const HANDLE &rDataFileHandle, ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    uint64 userID;
    ClientSocket *pClientSocket;
    static const size_t packetSize = sizeof(token)+sizeof(flags)+sizeof(userID);
    
    //buffer for y keys
    ByteBuffer rY;
    
	//for compresion
	int compressionStatus = Z_ERRNO;
	ByteBuffer rBuffOut;
    
    //read data from packet
    rClientSocketTaskData >> token >> flags >> userID;
    
    //get all Y
    m_rStorage.GetAllY(rDataFileHandle, userID, rY);
    
	//try to compress
	if(rY.size() > (size_t)g_DataSizeForCompression)
	{
		compressionStatus = Common::compressGzip(rY.contents(), rY.size(), rBuffOut);
		if(compressionStatus == Z_OK)
		{
			Log.Debug(__FUNCTION__, "Data compressed. Original size: %u, new size: %u", rY.size(), rBuffOut.size());
			flags = ePF_COMPRESSED;
            //add compressed data
            rY.clear();
            rY.append(rBuffOut);
		}
        else
        {
            Log.Error(__FUNCTION__, "Data compression failed. X: " I64FMTD, userID);
        }
	}
    
    //get socket
    pClientSocket = g_rClientSocketHolder.GetSocket(rClientSocketTaskData.socketID());
    if(pClientSocket)
    {
        //send data back
        Packet rResponse(S_MSG_GET_ALL_Y, rY.size() + packetSize);
        rResponse << token;
        rResponse << flags;
        rResponse << userID;
        rResponse.append(rY);
        pClientSocket->SendPacket(rResponse);
    }
}

void ClientSocketWorkerTask::HandleStatus(const HANDLE &rDataFileHandle, ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    ClientSocket *pClientSocket;
    ByteBuffer rBuff;
    
    //read data from packet
    rClientSocketTaskData >> token >> flags;
    
    //get stats
    m_rStorage.GetStats(rBuff);
    
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
        else
        {
            Log.Error(__FUNCTION__, "Data compression failed.");
        }
	}
    
    //get socket
    pClientSocket = g_rClientSocketHolder.GetSocket(rClientSocketTaskData.socketID());
    if(pClientSocket)
    {
        //send data back
        Packet rResponse(S_MSG_STATUS, 8 + rBuff.size());
        rResponse << token;
        rResponse << flags;
        rResponse.append(rBuff);
        pClientSocket->SendPacket(rResponse);
    }
}

void ClientSocketWorkerTask::HandleDeleteX(const HANDLE &rDataFileHandle, ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    uint8 *pUserIDs;
    size_t usersSize;
    uint64 userID;
    ClientSocket *pClientSocket;
    ByteBuffer rUsers;

	//for decompresion
	int decompressionStatus = Z_ERRNO;
	ByteBuffer rBuffOut;
    
    //read data from packet
    rClientSocketTaskData >> token >> flags;
    usersSize = rClientSocketTaskData.size() - rClientSocketTaskData.rpos();
    pUserIDs = (uint8*)(rClientSocketTaskData.contents() + rClientSocketTaskData.rpos());
    
    Log.Debug(__FUNCTION__, "token = %u", token);
    Log.Debug(__FUNCTION__, "flags = %u", flags);
    Log.Debug(__FUNCTION__, "usersSize = " I64FMTD, usersSize);
    
    //decompress
    if(flags & ePF_COMPRESSED)
    {
        decompressionStatus = Common::decompressGzip(pUserIDs, usersSize, rBuffOut);
        if(decompressionStatus == Z_OK)
        {
            Log.Debug(__FUNCTION__, "Data decompressed. Original size: %u, new size: %u", usersSize, rBuffOut.size());
            flags = ePF_NULL;
            usersSize = rBuffOut.size();
            pUserIDs = (uint8*)rBuffOut.contents();
        }
        else
        {
            flags = ePF_DECOMPRESS_FAILED;
            Log.Error(__FUNCTION__, "Decompression failed.");
        }
    }
        
    //only if decompression is ok
    if(flags == ePF_NULL)
    {
        //fill buffer
        rUsers.append(pUserIDs, usersSize);
        
        //iterate and delete all users        
        while(rUsers.rpos() < rUsers.size())
        {
            rUsers >> userID;
            Log.Debug(__FUNCTION__, "Deleting userID: " I64FMTD, userID);
            m_rStorage.DeleteData(userID);
            Log.Debug(__FUNCTION__, "Deleted userID: " I64FMTD, userID);
        }
    }
    
    //get socket
    pClientSocket = g_rClientSocketHolder.GetSocket(rClientSocketTaskData.socketID());
    if(pClientSocket)
    {
        uint8 buff[32];
        StackPacket rResponse(S_MSG_DELETE_X, buff, sizeof(buff));
        rResponse << token;
        rResponse << flags;
        pClientSocket->SendPacket(rResponse);
    }
}

void ClientSocketWorkerTask::HandleDefragmentData(const HANDLE &rDataFileHandle, ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    uint8 *pUserIDs;
    size_t usersSize;
    uint64 userID;
    ClientSocket *pClientSocket;
    ByteBuffer rUsers;
    
	//for compresion
	int compressionStatus = Z_ERRNO;
	ByteBuffer rBuffOut;
    
    //read data from packet
    rClientSocketTaskData >> token >> flags;
    usersSize = rClientSocketTaskData.size() - rClientSocketTaskData.rpos();
    pUserIDs = (uint8*)(rClientSocketTaskData.contents() + rClientSocketTaskData.rpos());
    
    Log.Debug(__FUNCTION__, "token = %u", token);
    Log.Debug(__FUNCTION__, "flags = %u", flags);
    Log.Debug(__FUNCTION__, "usersSize = " I64FMTD, usersSize);
    
    //decompress
    if(flags & ePF_COMPRESSED)
    {
        compressionStatus = Common::decompressGzip(pUserIDs, usersSize, rBuffOut);
        if(compressionStatus == Z_OK)
        {
            Log.Debug(__FUNCTION__, "Data decompressed. Original size: %u, new size: %u", usersSize, rBuffOut.size());
            flags = ePF_NULL;
            usersSize = rBuffOut.size();
            pUserIDs = (uint8*)rBuffOut.contents();
        }
        else
        {
            flags = ePF_DECOMPRESS_FAILED;
            Log.Error(__FUNCTION__, "Decompression failed.");
        }
    }
    
    //only if decompression is ok
    if(flags == ePF_NULL)
    {
        //fill buffer
        rUsers.append(pUserIDs, usersSize);
        
        //iterate and delete all users
        while(rUsers.rpos() < rUsers.size())
        {
            rUsers >> userID;
            Log.Debug(__FUNCTION__, "Defragmenting userID: " I64FMTD, userID);
            m_rStorage.DefragmentData(rDataFileHandle, userID);
            Log.Debug(__FUNCTION__, "Defragmented userID: " I64FMTD, userID);
        }
    }
    
    //get socket
    pClientSocket = g_rClientSocketHolder.GetSocket(rClientSocketTaskData.socketID());
    if(pClientSocket)
    {
        uint8 buff[32];
        StackPacket rResponse(S_MSG_DEFRAMENT_DATA, buff, sizeof(buff));
        rResponse << token;
        rResponse << flags;
        pClientSocket->SendPacket(rResponse);
    }
}

void ClientSocketWorkerTask::HandleGetFreeSpace(const HANDLE &rDataFileHandle, ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    uint32 dumpFlags; //0 - full dump, 1 - only counts
    ClientSocket *pClientSocket;
    ByteBuffer rBuff;
    
	//for compresion
	int compressionStatus = Z_ERRNO;
	ByteBuffer rBuffOut;
    
    //read data from packet
    rClientSocketTaskData >> token >> flags >> dumpFlags;
    
    //get data
    m_rStorage.GetFreeSpaceDump(rBuff, dumpFlags);
    
    if(rBuff.size() > (uint32)g_DataSizeForCompression)
    {
        compressionStatus = Common::compressGzip(rBuff.contents(), rBuff.size(), rBuffOut);
        if(compressionStatus == Z_OK)
        {
            Log.Debug(__FUNCTION__, "Data compressed. Original size: %u, new size: %u", rBuff.size(), rBuffOut.size());
            flags = ePF_COMPRESSED;
        }
        else
        {
            Log.Error(__FUNCTION__, "Data compression failed.");
        }
    }
    
    //get socket
    pClientSocket = g_rClientSocketHolder.GetSocket(rClientSocketTaskData.socketID());
    if(pClientSocket)
    {
        //send back data
        Packet rResponse(S_MSG_GET_FREESPACE, sizeof(token)+sizeof(flags)+rBuff.size());
        rResponse << token;
        rResponse << flags;
        
        if(compressionStatus == Z_OK)
            rResponse.append(rBuffOut);
        else
            rResponse.append(rBuff);
        
        pClientSocket->SendPacket(rResponse);
    }
}

void ClientSocketWorkerTask::HandleWriteDataNum(const HANDLE &rDataFileHandle, ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    uint64 userID;
    size_t dataSize;
    uint8 *pData;
    ByteBuffer rData;
    ClientSocket *pClientSocket;
    uint32 writeStatus = 0;
    
	//for decompresion
	int decompressionStatus = Z_ERRNO;
	ByteBuffer rBuffOut;
    
    //read data from packet
    rClientSocketTaskData >> token >> flags >> userID;
    
    //get data size
    dataSize = rClientSocketTaskData.size() - rClientSocketTaskData.rpos();
    pData = (uint8*)(rClientSocketTaskData.contents() + rClientSocketTaskData.rpos());
    
    //decompress
    if(flags & ePF_COMPRESSED)
    {
        decompressionStatus = Common::decompressGzip(pData, dataSize, rBuffOut);
        if(decompressionStatus == Z_OK)
        {
            Log.Debug(__FUNCTION__, "Data decompressed. Original size: %u, new size: %u", dataSize, rBuffOut.size());
            flags = ePF_NULL;
            dataSize = rBuffOut.size();
            pData = (uint8*)rBuffOut.contents();
        }
        else
        {
            flags = ePF_DECOMPRESS_FAILED;
            Log.Error(__FUNCTION__, "Decompression failed. X: " I64FMTD, userID);
        }
    }
    
    //only if decompression is ok
    if(flags == ePF_NULL)
    {
        uint64 key;
        uint16 recordSize;
        ByteBuffer rRecord;
        
        //fill buffer
        rData.append(pData, dataSize);
        
        //iterate and write
        //key|recordSize|record|....Nx
        while(rData.rpos() < rData.size())
        {
            //get info
            rData >> key;
            rData >> recordSize;
            rRecord.resize(recordSize);
            rData.read((uint8*)rRecord.contents(), rRecord.size());
            
            //write
            writeStatus |= m_rStorage.WriteData(rDataFileHandle, userID, key, rRecord.contents(), recordSize);
        }
    }
    
    //get socket
    pClientSocket = g_rClientSocketHolder.GetSocket(rClientSocketTaskData.socketID());
    if(pClientSocket)
    {
        uint8 buff[32];
        StackPacket rResponse(S_MSG_WRITE_DATA_NUM, buff, sizeof(buff));
        rResponse << token;
        rResponse << flags;
        rResponse << userID;
        rResponse << writeStatus;
        pClientSocket->SendPacket(rResponse);
    }
}
























