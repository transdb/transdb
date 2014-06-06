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
    &ClientSocketWorkerTask::HandleReadLog,         //C_MSG_READ_LOG        = 25,
    NULL,                                           //S_MSG_READ_LOG        = 26,
    &ClientSocketWorkerTask::HandleReadConfig,      //C_MSG_READ_LOG        = 27,
    NULL,                                           //S_MSG_READ_LOG        = 28,
};

ClientSocketWorkerTask::ClientSocketWorkerTask(ClientSocketWorker &rClientSocketWorker,
                                               Storage &rStorage,
                                               bool readerTask) : m_rClientSocketWorker(rClientSocketWorker),
                                                                  m_rStorage(rStorage),
                                                                  m_pLRUCache(new LRUCache("ClientSocketWorkerTask", g_LRUCacheMemReserve / sizeof(CRec))),
                                                                  m_rDataFileHandle(INVALID_HANDLE_VALUE),
                                                                  m_readerThread(readerTask)
{

}

ClientSocketWorkerTask::~ClientSocketWorkerTask()
{
    //delete LRU cache
    delete m_pLRUCache;
    m_pLRUCache = NULL;
}

bool ClientSocketWorkerTask::run()
{
    if(m_readerThread)
        CommonFunctions::SetThreadName("Read ClientSocketWorkerTask thread");
    else
        CommonFunctions::SetThreadName("Write ClientSocketWorkerTask thread");

    //open file per thread - read only
    IOHandleGuard rIOHandleGuard(m_rDataFileHandle);
    m_rDataFileHandle = IO::fopen(m_rStorage.m_rDataPath.c_str(), IO::IO_READ_ONLY, IO::IO_NORMAL);
    if(m_rDataFileHandle == INVALID_HANDLE_VALUE)
    {
        Log.Error(__FUNCTION__, "Cannot open data file: %s", m_rStorage.m_rDataPath.c_str());
        return true;
    }
    
    //data from queue
	ClientSocketTaskData *pClientSocketTaskData;
    //LRU recycle timer
    time_t checkTime = UNIXTIME + g_MemoryPoolsRecycleTimer;
    
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
                    m_rClientSocketWorker.m_rReadTaskDataQueue.pop(pClientSocketTaskData);
                else
                    m_rClientSocketWorker.m_rTaskDataQueue.pop(pClientSocketTaskData);
            }
            catch(tbb::user_abort&) {
                Log.Notice(__FUNCTION__, "Task aborted. ReadTask: %u", (uint32)m_readerThread);
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
            
            //check memory
            m_rStorage.CheckMemory(*m_pLRUCache);
            
            //recycle memory pools timer
            if(checkTime < UNIXTIME)
            {
                m_pLRUCache->recycle();
                //update next check time
                checkTime = UNIXTIME + g_MemoryPoolsRecycleTimer;
            }
            
            //call ~ctor + dealloc task data
            pClientSocketTaskData->~ClientSocketTaskData();
			m_rClientSocketWorker.m_pFixedPool->free(pClientSocketTaskData);
        }
    }
    catch(...)
    {
        Log.Error(__FUNCTION__, "Fatal error stopping server.");
        m_rClientSocketWorker.SetException(true);
        g_stopEvent = true;
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
    
    //read data from packet
    rClientSocketTaskData >> token >> flags >> userID >> timeStamp;
    recordSize = (uint16)(rClientSocketTaskData.size() - rClientSocketTaskData.rpos());
    pRecord = (uint8*)(rClientSocketTaskData.contents() + rClientSocketTaskData.rpos());
    
    //write data with result
    writeStatus = m_rStorage.WriteData(m_rDataFileHandle, *m_pLRUCache, userID, timeStamp, pRecord, recordSize);
    
    //send back data
    uint8 buff[32];
    StackPacket rResponse(S_MSG_WRITE_DATA, buff, sizeof(buff));
    rResponse << token;
    rResponse << flags;
    rResponse << userID;
    rResponse << timeStamp;
    rResponse << writeStatus;
    g_rClientSocketHolder.SendPacket(rClientSocketTaskData.socketID(), rResponse);
}

void ClientSocketWorkerTask::HandleReadData(ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    uint64 userID;
    uint64 timeStamp;
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
        m_rStorage.ReadData(m_rDataFileHandle, *m_pLRUCache, userID, rReadData);
    }
    else
    {
        m_rStorage.ReadData(m_rDataFileHandle, *m_pLRUCache, userID, timeStamp, rReadData);
    }
    
	//try to compress
	if(rReadData.size() > (uint32)g_DataSizeForCompression)
	{
		compressionStatus = CommonFunctions::compressGzip(g_GzipCompressionLevel, rReadData.contents(), rReadData.size(), rBuffOut);
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
    
    //send back data
    Packet rResponse(S_MSG_READ_DATA, packetSize + rReadData.size());
    rResponse << token;
    rResponse << flags;
    rResponse << userID;
    rResponse << timeStamp;
    rResponse.append(rReadData);
    g_rClientSocketHolder.SendPacket(rClientSocketTaskData.socketID(), rResponse);
}

void ClientSocketWorkerTask::HandleDeleteData(ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    uint64 userID;
    uint64 timeStamp;
    
    //read data from packet
    rClientSocketTaskData >> token >> flags >> userID >> timeStamp;
    
    //read data - if timeStamp == 0 then delete all datas under userID
    if(timeStamp == 0)
    {
        m_rStorage.DeleteData(*m_pLRUCache, userID);
    }
    else
    {
        m_rStorage.DeleteData(m_rDataFileHandle, *m_pLRUCache, userID, timeStamp);
    }
    
    //send back data
    uint8 buff[32];
    StackPacket rResponse(S_MSG_DELETE_DATA, buff, sizeof(buff));
    rResponse << token;
    rResponse << flags;
    rResponse << userID;
    rResponse << timeStamp;
    g_rClientSocketHolder.SendPacket(rClientSocketTaskData.socketID(), rResponse);
}

void ClientSocketWorkerTask::HandleGetAllX(ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    static const size_t packetSize = sizeof(token)+sizeof(flags);
    
    //read data from packet
    rClientSocketTaskData >> token >> flags;
    
    //not implemented
    
    //send back data
    Packet rResponse(S_MSG_GET_ALL_X, packetSize);
    rResponse << token;
    rResponse << flags;
    g_rClientSocketHolder.SendPacket(rClientSocketTaskData.socketID(), rResponse);
}

void ClientSocketWorkerTask::HandleGetAllY(ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    uint64 userID;
    static const size_t packetSize = sizeof(token)+sizeof(flags)+sizeof(userID);
    
    //buffer for y keys
    ByteBuffer rY;
    
	//for compresion
	int compressionStatus = Z_ERRNO;
	ByteBuffer rBuffOut;
    
    //read data from packet
    rClientSocketTaskData >> token >> flags >> userID;
    
    //get all Y
    m_rStorage.GetAllY(m_rDataFileHandle, *m_pLRUCache, userID, rY);
    
	//try to compress
	if(rY.size() > (size_t)g_DataSizeForCompression)
	{
		compressionStatus = CommonFunctions::compressGzip(g_GzipCompressionLevel, rY.contents(), rY.size(), rBuffOut);
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
    
    //send data back
    Packet rResponse(S_MSG_GET_ALL_Y, rY.size() + packetSize);
    rResponse << token;
    rResponse << flags;
    rResponse << userID;
    rResponse.append(rY);
    g_rClientSocketHolder.SendPacket(rClientSocketTaskData.socketID(), rResponse);
}

void ClientSocketWorkerTask::HandleStatus(ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
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
		compressionStatus = CommonFunctions::compressGzip(g_GzipCompressionLevel, rBuff.contents(), rBuff.size(), rBuffOut);
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
    
    //send data back
    Packet rResponse(S_MSG_STATUS, 8 + rBuff.size());
    rResponse << token;
    rResponse << flags;
    rResponse.append(rBuff);
    g_rClientSocketHolder.SendPacket(rClientSocketTaskData.socketID(), rResponse);
}

void ClientSocketWorkerTask::HandleDeleteX(ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    uint8 *pUserIDs;
    size_t usersSize;
    uint64 userID;
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
        decompressionStatus = CommonFunctions::decompressGzip(pUserIDs, usersSize, rBuffOut);
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
            m_rStorage.DeleteData(*m_pLRUCache, userID);
            Log.Debug(__FUNCTION__, "Deleted userID: " I64FMTD, userID);
        }
    }
    
    //send data back
    uint8 buff[32];
    StackPacket rResponse(S_MSG_DELETE_X, buff, sizeof(buff));
    rResponse << token;
    rResponse << flags;
    g_rClientSocketHolder.SendPacket(rClientSocketTaskData.socketID(), rResponse);
}

void ClientSocketWorkerTask::HandleDefragmentData(ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    uint8 *pUserIDs;
    size_t usersSize;
    uint64 userID;
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
        compressionStatus = CommonFunctions::decompressGzip(pUserIDs, usersSize, rBuffOut);
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
            m_rStorage.DefragmentData(m_rDataFileHandle, *m_pLRUCache, userID);
            Log.Debug(__FUNCTION__, "Defragmented userID: " I64FMTD, userID);
        }
    }
    
    //send data back
    uint8 buff[32];
    StackPacket rResponse(S_MSG_DEFRAMENT_DATA, buff, sizeof(buff));
    rResponse << token;
    rResponse << flags;
    g_rClientSocketHolder.SendPacket(rClientSocketTaskData.socketID(), rResponse);
}

void ClientSocketWorkerTask::HandleGetFreeSpace(ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    uint32 dumpFlags; //0 - full dump, 1 - only counts
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
        compressionStatus = CommonFunctions::compressGzip(g_GzipCompressionLevel, rBuff.contents(), rBuff.size(), rBuffOut);
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
    
    //send back data
    Packet rResponse(S_MSG_GET_FREESPACE, sizeof(token)+sizeof(flags)+rBuff.size());
    rResponse << token;
    rResponse << flags;
    
    if(compressionStatus == Z_OK)
        rResponse.append(rBuffOut);
    else
        rResponse.append(rBuff);
    
    g_rClientSocketHolder.SendPacket(rClientSocketTaskData.socketID(), rResponse);
}

void ClientSocketWorkerTask::HandleWriteDataNum(ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    uint64 userID;
    size_t dataSize;
    uint8 *pData;
    ByteBuffer rData;
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
        decompressionStatus = CommonFunctions::decompressGzip(pData, dataSize, rBuffOut);
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
            writeStatus |= m_rStorage.WriteData(m_rDataFileHandle, *m_pLRUCache, userID, key, rRecord.contents(), recordSize);
        }
    }
    
    //send back data
    uint8 buff[32];
    StackPacket rResponse(S_MSG_WRITE_DATA_NUM, buff, sizeof(buff));
    rResponse << token;
    rResponse << flags;
    rResponse << userID;
    rResponse << writeStatus;
    g_rClientSocketHolder.SendPacket(rClientSocketTaskData.socketID(), rResponse);
}

void ClientSocketWorkerTask::HandleReadLog(ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    ByteBuffer rBuff;
    
	//for compresion
	int compressionStatus = Z_ERRNO;
	ByteBuffer rBuffOut;
    
    //read data from packet
    rClientSocketTaskData >> token >> flags;
    
    //read log file from disk
    Log.GetFileLogContent(rBuff);
    
    if(rBuff.size() > (uint32)g_DataSizeForCompression)
    {
        compressionStatus = CommonFunctions::compressGzip(g_GzipCompressionLevel, rBuff.contents(), rBuff.size(), rBuffOut);
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
    
    //send back data
    Packet rResponse(S_MSG_READ_LOG, sizeof(token)+sizeof(flags)+rBuff.size());
    rResponse << token;
    rResponse << flags;
    
    if(compressionStatus == Z_OK)
        rResponse.append(rBuffOut);
    else
        rResponse.append(rBuff);
    
    g_rClientSocketHolder.SendPacket(rClientSocketTaskData.socketID(), rResponse);
}

void ClientSocketWorkerTask::HandleReadConfig(ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    ByteBuffer rBuff;
    
	//for compresion
	int compressionStatus = Z_ERRNO;
	ByteBuffer rBuffOut;
    
    //read data from packet
    rClientSocketTaskData >> token >> flags;
    
    //get config data
    const char *pConfigPath = g_pConfigWatcher->GetConfigPath();
    HANDLE hFile = IO::fopen(pConfigPath, IO::IO_READ_ONLY, IO::IO_NORMAL);
    if(hFile != INVALID_HANDLE_VALUE)
    {
        IO::fseek(hFile, 0, IO::IO_SEEK_END);
        int64 fileSize = IO::ftell(hFile);
        IO::fseek(hFile, 0, IO::IO_SEEK_SET);
        //read to buffer
        rBuff.resize(fileSize);
        IO::fread((void*)rBuff.contents(), rBuff.size(), hFile);
        IO::fclose(hFile);
    }
    
    if(rBuff.size() > (uint32)g_DataSizeForCompression)
    {
        compressionStatus = CommonFunctions::compressGzip(g_GzipCompressionLevel, rBuff.contents(), rBuff.size(), rBuffOut);
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
    
    //send back data
    Packet rResponse(S_MSG_READ_CONFIG, sizeof(token)+sizeof(flags)+rBuff.size());
    rResponse << token;
    rResponse << flags;
    
    if(compressionStatus == Z_OK)
        rResponse.append(rBuffOut);
    else
        rResponse.append(rBuff);
    
    g_rClientSocketHolder.SendPacket(rClientSocketTaskData.socketID(), rResponse);
}






















