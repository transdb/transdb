//
//  ClientSocketWorkerTask.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 12.11.12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

typedef void (ClientSocketWorkerTask::*ClientSocketWorkerTaskHandler)(ClientSocketTaskData&);
static const ClientSocketWorkerTaskHandler m_ClientSocketWorkerTaskHandlers[OP_NUM] =
{
    NULL,                                       //C_NULL_OP
	&ClientSocketWorkerTask::HandleWriteData,	//C_MSG_WRITE_DATA		= 1,
	NULL,										//S_MSG_WRITE_DATA		= 2,
	&ClientSocketWorkerTask::HandleReadData,    //C_MSG_READ_DATA		= 3,
	NULL,										//S_MSG_READ_DATA		= 4,
	&ClientSocketWorkerTask::HandleDeleteData,	//C_MSG_DELETE_DATA		= 5,
	NULL,										//S_MSG_DELETE_DATA		= 6,
    &ClientSocketWorkerTask::HandleGetAllX,     //C_MSG_GET_ALL_X       = 7,
    NULL,                                       //S_MSG_GET_ALL_X       = 8,
    NULL,                                       //C_MSG_PONG            = 9,
    NULL,                                       //S_MSG_PONG            = 10,
    &ClientSocketWorkerTask::HandleGetAllY,     //C_MSG_GET_ALL_Y       = 11,
    NULL,                                       //S_MSG_GET_ALL_Y       = 12,
    NULL,                                       //C_MSG_STATUS          = 13,
    NULL,                                       //S_MSG_STATUS          = 14,
    NULL,                                       //C_MSG_GET_ACTIVITY_ID = 15,
    NULL,                                       //S_MSG_GET_ACTIVITY_ID = 16,
    &ClientSocketWorkerTask::HandleDeleteX,     //C_MSG_DELETE_X        = 17,
    NULL,                                       //S_MSG_DELETE_X        = 18,
};

ClientSocketWorkerTask::ClientSocketWorkerTask()
{

}

ClientSocketWorkerTask::~ClientSocketWorkerTask()
{
    //close file
    IO::fflush(g_pStorage_FilePointer);
    g_pStorage_FilePointer = NULL;
}

bool ClientSocketWorkerTask::run()
{
	SetThreadName("ClientSocketWorkerTask thread");

    //open file per thread - read only
    g_pStorage_FilePointer = fopen(g_pStorage->m_pDataPath, "rb");
    assert(g_pStorage_FilePointer);
    
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
                g_pClientSocketWorker->m_rTaskDataQueue.pop(pClientSocketTaskData);
            }
            catch(tbb::user_abort&) {
                Log.Notice(__FUNCTION__, "Task aborted.");
                return true;
            }

            //process task
            if(pClientSocketTaskData->opcode() < OP_NUM && m_ClientSocketWorkerTaskHandlers[pClientSocketTaskData->opcode()] != NULL)
            {
                Log.Debug(__FUNCTION__, "Processing packet opcode: (0x%.4X)", pClientSocketTaskData->opcode());
                (void)(this->*m_ClientSocketWorkerTaskHandlers[pClientSocketTaskData->opcode()])(*pClientSocketTaskData);
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
        Sync_Add(&g_stopEvent);
    }

    return true;
}

void ClientSocketWorkerTask::HandleWriteData(ClientSocketTaskData &rClientSocketTaskData)
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
    writeStatus = g_pStorage->WriteData(userID, timeStamp, pRecord, recordSize);
    
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

void ClientSocketWorkerTask::HandleReadData(ClientSocketTaskData &rClientSocketTaskData)
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
        g_pStorage->ReadData(userID, rReadData);
    }
    else
    {
        g_pStorage->ReadData(userID, timeStamp, rReadData);
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

void ClientSocketWorkerTask::HandleDeleteData(ClientSocketTaskData &rClientSocketTaskData)
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
        g_pStorage->DeleteData(userID);
    }
    else
    {
        g_pStorage->DeleteData(userID, timeStamp);
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

void ClientSocketWorkerTask::HandleGetAllX(ClientSocketTaskData &rClientSocketTaskData)
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
    
    //get socket
    pClientSocket =  g_rClientSocketHolder.GetSocket(rClientSocketTaskData.socketID());
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
            
            aligned_free(pX);
        }
        pClientSocket->SendPacket(rResponse);
    }
}

void ClientSocketWorkerTask::HandleGetAllY(ClientSocketTaskData &rClientSocketTaskData)
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
    g_pStorage->GetAllY(userID, rY);
    
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
	}
    
    //get socket
    pClientSocket =  g_rClientSocketHolder.GetSocket(rClientSocketTaskData.socketID());
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

void ClientSocketWorkerTask::HandleDeleteX(ClientSocketTaskData &rClientSocketTaskData)
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
    Log.Debug(__FUNCTION__, "usersSize = "I64FMTD, usersSize);
    
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
            Log.Debug(__FUNCTION__, "Deleting userID: "I64FMTD, userID);
            g_pStorage->DeleteData(userID);
            Log.Debug(__FUNCTION__, "Deleted userID: "I64FMTD, userID);            
        }
    }
    
    //get socket
    pClientSocket =  g_rClientSocketHolder.GetSocket(rClientSocketTaskData.socketID());
    if(pClientSocket)
    {
        uint8 buff[32];
        StackPacket rResponse(S_MSG_DELETE_X, buff, sizeof(buff));
        rResponse << token;
        rResponse << flags;
        pClientSocket->SendPacket(rResponse);
    }
}




























