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
    NULL,                                               //C_NULL_OP
	&ClientSocketWorkerTask::HandleWriteData,           //C_MSG_WRITE_DATA              = 1,
	NULL,                                               //S_MSG_WRITE_DATA              = 2,
	&ClientSocketWorkerTask::HandleReadData,            //C_MSG_READ_DATA               = 3,
	NULL,                                               //S_MSG_READ_DATA               = 4,
	&ClientSocketWorkerTask::HandleDeleteData,          //C_MSG_DELETE_DATA             = 5,
	NULL,                                               //S_MSG_DELETE_DATA             = 6,
    &ClientSocketWorkerTask::HandleGetAllX,             //C_MSG_GET_ALL_X               = 7,
    NULL,                                               //S_MSG_GET_ALL_X               = 8,
    NULL,                                               //C_MSG_PONG                    = 9,
    NULL,                                               //S_MSG_PONG                    = 10,
    &ClientSocketWorkerTask::HandleGetAllY,             //C_MSG_GET_ALL_Y               = 11,
    NULL,                                               //S_MSG_GET_ALL_Y               = 12,
    &ClientSocketWorkerTask::HandleStatus,              //C_MSG_STATUS                  = 13,
    NULL,                                               //S_MSG_STATUS                  = 14,
    NULL,                                               //C_MSG_GET_ACTIVITY_ID         = 15,
    NULL,                                               //S_MSG_GET_ACTIVITY_ID         = 16,
    &ClientSocketWorkerTask::HandleDeleteX,             //C_MSG_DELETE_X                = 17,
    NULL,                                               //S_MSG_DELETE_X                = 18,
    &ClientSocketWorkerTask::HandleDefragmentData,      //C_MSG_DEFRAMENT_DATA          = 19,
    NULL,                                               //S_MSG_DEFRAMENT_DATA          = 20,
    &ClientSocketWorkerTask::HandleGetFreeSpace,        //C_MSG_GET_FREESPACE           = 21,
    NULL,                                               //S_MSG_GET_FREESPACE           = 22,
    &ClientSocketWorkerTask::HandleWriteDataNum,        //C_MSG_WRITE_DATA_NUM          = 23,
    NULL,                                               //S_MSG_WRITE_DATA_NUM          = 24,
    &ClientSocketWorkerTask::HandleReadLog,             //C_MSG_READ_LOG                = 25,
    NULL,                                               //S_MSG_READ_LOG                = 26,
    &ClientSocketWorkerTask::HandleReadConfig,          //C_MSG_READ_CONFIG             = 27,
    NULL,                                               //S_MSG_READ_CONFIG             = 28,
    &ClientSocketWorkerTask::HandleDefragmentFreeSpace, //C_MSG_DEFRAGMENT_FREESPACE    = 29,
    NULL,                                               //S_MSG_DEFRAGMENT_FREESPACE    = 30,
    &ClientSocketWorkerTask::HandleExecutePythonScript, //C_MSG_EXEC_PYTHON_SCRIPT      = 31,
    NULL,                                               //C_MSG_EXEC_PYTHON_SCRIPT      = 32,
};

ClientSocketWorkerTask::ClientSocketWorkerTask(TaskDataQueue &rTaskDataQueue,
                                               ClientSocketWorker &rClientSocketWorker,
                                               Storage &rStorage,
                                               PythonInterface &rPythonInterface,
                                               ConfigWatcher &rConfigWatcher) : m_rTaskDataQueue(rTaskDataQueue),
                                                                                m_rClientSocketWorker(rClientSocketWorker),
                                                                                m_rStorage(rStorage),
                                                                                m_rPythonInterface(rPythonInterface),
                                                                                m_rConfigWatcher(rConfigWatcher),
                                                                                m_pLRUCache(new LRUCache("ClientSocketWorkerTask", g_cfg.LRUCacheMemReserve / sizeof(CRec))),
                                                                                m_rDataFileHandle(INVALID_HANDLE_VALUE)
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
    CommonFunctions::SetThreadName("ClientSocketWorkerTask thread");

    //open file per thread - read only
    IOHandleGuard rIOHandleGuard(m_rDataFileHandle);
    m_rDataFileHandle = IO::fopen(m_rStorage.m_rDataPath.c_str(), IO::IO_READ_ONLY, IO::IO_DIRECT);
    if(m_rDataFileHandle == INVALID_HANDLE_VALUE)
    {
        Log.Error(__FUNCTION__, "Cannot open data file: %s", m_rStorage.m_rDataPath.c_str());
        return true;
    }
    
    try
    {
#ifdef WIN32
        _set_se_translator(trans_func);
#endif
        
        //main thread loop
        while(m_threadRunning)
        {
            ClientSocketTaskData rTaskData;
            //get task data from queue
            if(m_rTaskDataQueue.get(rTaskData, g_cfg.TaskQueueTimeout))
            {
                //process task
                if(rTaskData.m_opcode < OP_NUM && m_ClientSocketWorkerTaskHandlers[rTaskData.m_opcode] != NULL)
                {
                    Log.Debug(__FUNCTION__, "Processing packet opcode: (0x%.4X)", rTaskData.m_opcode);
                    (void)(this->*m_ClientSocketWorkerTaskHandlers[rTaskData.m_opcode])(rTaskData);
                }
                else
                {
                    Log.Warning(__FUNCTION__, "Unknown opcode (0x%.4X)", rTaskData.m_opcode);
                }
                
                //dealloc task data
                bbuff_destroy(rTaskData.m_pData);
            }
            
            //check memory
            m_rStorage.CheckMemory(*m_pLRUCache);
        }
    }
    catch(...)
    {
        Log.Error(__FUNCTION__, "Fatal error stopping server.");
        g_stopEvent = true;
    }

    return true;
}

void ClientSocketWorkerTask::HandleWriteData(ClientSocketTaskData &rTaskData)
{
    uint32 token;
    uint32 flags;
    uint64 X;
    uint64 Y;
    uint8 *pRecord;
    uint16 recordSize;
    uint32 writeStatus;
    
    //read data from packet
    bbuff_read(rTaskData.m_pData, &token, sizeof(token));
    bbuff_read(rTaskData.m_pData, &flags, sizeof(flags));
    bbuff_read(rTaskData.m_pData, &X, sizeof(X));
    bbuff_read(rTaskData.m_pData, &Y, sizeof(Y));
    
    //TODO: check overflow
    recordSize = (uint16)(rTaskData.m_pData->size - rTaskData.m_pData->rpos);
    pRecord = (uint8*)(rTaskData.m_pData->storage + rTaskData.m_pData->rpos);
    
    //write data with result
    writeStatus = m_rStorage.WriteData(m_rDataFileHandle, *m_pLRUCache, X, Y, pRecord, recordSize);
    
    //send back data
    uint8 buff[32];
    StackPacket rResponse(S_MSG_WRITE_DATA, buff, sizeof(buff));
    rResponse << token;
    rResponse << flags;
    rResponse << X;
    rResponse << Y;
    rResponse << writeStatus;
    g_rClientSocketHolder.SendPacket(rTaskData.m_socketID, rResponse);
}

void ClientSocketWorkerTask::HandleReadData(ClientSocketTaskData &rTaskData)
{
    uint32 token;
    uint32 flags;
    uint64 X;
    uint64 Y;
    static const size_t packetSize = sizeof(token)+sizeof(flags)+sizeof(X)+sizeof(Y);
    
    //read data from packet
    bbuff_read(rTaskData.m_pData, &token, sizeof(token));
    bbuff_read(rTaskData.m_pData, &flags, sizeof(flags));
    bbuff_read(rTaskData.m_pData, &X, sizeof(X));
    bbuff_read(rTaskData.m_pData, &Y, sizeof(Y));
    
    //buffer for read data
    bbuff *pReadData;
    bbuff_create(pReadData);
    
    //prepare packet
    bbuff_reserve(pReadData, packetSize);
    bbuff_append(pReadData, &token, sizeof(token));
    bbuff_append(pReadData, &flags, sizeof(flags));
    bbuff_append(pReadData, &X, sizeof(X));
    bbuff_append(pReadData, &Y, sizeof(Y));
    
    //read data - if Y == 0 then read all data under X - append to send bytebuffer
    if(Y == 0)
    {
        m_rStorage.ReadData(m_rDataFileHandle, *m_pLRUCache, X, pReadData);
    }
    else
    {
        m_rStorage.ReadData(m_rDataFileHandle, *m_pLRUCache, X, Y, pReadData);
    }
    
    //calc where data start and what size it has
    uint8 *pReadDataBegin = pReadData->storage + packetSize;
    size_t readDataSize = pReadData->size - packetSize;
    
    //log
    Log.Debug(__FUNCTION__, "Read data [x:" I64FMTD ",y:" I64FMTD "] size: " I64FMTD, X, Y, readDataSize);
    
	//try to compress
	if(readDataSize > (uint32)g_cfg.DataSizeForCompression)
	{
        bbuff *pBuffOut;
        bbuff_create(pBuffOut);
        int compressionStatus = CCommon_compressGzip(g_cfg.GzipCompressionLevel, pReadDataBegin, readDataSize, pBuffOut, g_cfg.ZlibBufferSize);
        if(compressionStatus == Z_OK)
        {
            flags = ePF_COMPRESSED;
            //modify flags
            bbuff_put(pReadData, sizeof(token), &flags, sizeof(flags));
            //prepare space and rewrite noncompressed data
            bbuff_resize(pReadData, pBuffOut->size + packetSize);
            bbuff_put(pReadData, packetSize, pBuffOut->storage, pBuffOut->size);
        }
        else
        {
            Log.Error(__FUNCTION__, "Data compression failed. X: " I64FMTD ", Y: " I64FMTD, X, Y);
        }
        bbuff_destroy(pBuffOut);
	}
    
    //send back
    g_rClientSocketHolder.SendPacket(rTaskData.m_socketID, S_MSG_READ_DATA, pReadData);
    
    //clear memory
    bbuff_destroy(pReadData);
}

void ClientSocketWorkerTask::HandleDeleteData(ClientSocketTaskData &rTaskData)
{
    uint32 token;
    uint32 flags;
    uint64 X;
    uint64 Y;
    
    //read data from packet
    bbuff_read(rTaskData.m_pData, &token, sizeof(token));
    bbuff_read(rTaskData.m_pData, &flags, sizeof(flags));
    bbuff_read(rTaskData.m_pData, &X, sizeof(X));
    bbuff_read(rTaskData.m_pData, &Y, sizeof(Y));
    
    //read data - if timeStamp == 0 then delete all datas under userID
    if(Y == 0)
    {
        m_rStorage.DeleteData(*m_pLRUCache, X);
    }
    else
    {
        m_rStorage.DeleteData(m_rDataFileHandle, *m_pLRUCache, X, Y);
    }
    
    //send back data
    uint8 buff[32];
    StackPacket rResponse(S_MSG_DELETE_DATA, buff, sizeof(buff));
    rResponse << token;
    rResponse << flags;
    rResponse << X;
    rResponse << Y;
    g_rClientSocketHolder.SendPacket(rTaskData.m_socketID, rResponse);
}

void ClientSocketWorkerTask::HandleGetAllX(ClientSocketTaskData &rTaskData)
{
    //TODO: implement
}

void ClientSocketWorkerTask::HandleGetAllY(ClientSocketTaskData &rTaskData)
{
    uint32 token;
    uint32 flags;
    uint64 X;
    static const size_t packetSize = sizeof(token)+sizeof(flags)+sizeof(X);
    
    //read data from packet
    bbuff_read(rTaskData.m_pData, &token, sizeof(token));
    bbuff_read(rTaskData.m_pData, &flags, sizeof(flags));
    bbuff_read(rTaskData.m_pData, &X, sizeof(X));
    
    //buffer for all Y
    bbuff *pY;
    bbuff_create(pY);
    
    //prepare packet
    bbuff_reserve(pY, packetSize);
    bbuff_append(pY, &token, sizeof(token));
    bbuff_append(pY, &flags, sizeof(flags));
    bbuff_append(pY, &X, sizeof(X));
    
    //get all Y
    m_rStorage.GetAllY(m_rDataFileHandle, *m_pLRUCache, X, pY);
    
    //calc where data start and what size it has
    uint8 *pYDataBegin = pY->storage + packetSize;
    size_t YSize = pY->size - packetSize;
    
    //debug log
    Log.Debug(__FUNCTION__, "Read data [x:" I64FMTD ",y:" I64FMTD "] size: " I64FMTD, X, 0, YSize);
    
	//try to compress
	if(YSize > (size_t)g_cfg.DataSizeForCompression)
	{
        bbuff *pBuffOut;
        bbuff_create(pBuffOut);
        int compressionStatus = CCommon_compressGzip(g_cfg.GzipCompressionLevel, pYDataBegin, YSize, pBuffOut, g_cfg.ZlibBufferSize);
        if(compressionStatus == Z_OK)
        {
            flags = ePF_COMPRESSED;
            //modify flags
            bbuff_put(pY, sizeof(token), &flags, sizeof(flags));
            //prepare space and rewrite noncompressed data
            bbuff_resize(pY, pBuffOut->size + packetSize);
            bbuff_put(pY, packetSize, pBuffOut->storage, pBuffOut->size);
        }
        else
        {
            Log.Error(__FUNCTION__, "Data compression failed. X: " I64FMTD, X);
        }
        bbuff_destroy(pBuffOut);
	}
    
    //send back
    g_rClientSocketHolder.SendPacket(rTaskData.m_socketID, S_MSG_GET_ALL_Y, pY);
    
    //clear memory
    bbuff_destroy(pY);
}

void ClientSocketWorkerTask::HandleStatus(ClientSocketTaskData &rTaskData)
{
    uint32 token;
    uint32 flags;
    static const size_t packetSize = sizeof(token)+sizeof(flags);
    
    //read data from packet
    bbuff_read(rTaskData.m_pData, &token, sizeof(token));
    bbuff_read(rTaskData.m_pData, &flags, sizeof(flags));
    
    //buffer for stats
    bbuff *pBuff;
    bbuff_create(pBuff);
    
    //prepare packet
    bbuff_reserve(pBuff, packetSize);
    bbuff_append(pBuff, &token, sizeof(token));
    bbuff_append(pBuff, &flags, sizeof(flags));
    
    //get stats
    m_rStorage.GetStats(pBuff);
    
    //calc where data start and what size it has
    uint8 *pStatsDataBegin = pBuff->storage + packetSize;
    size_t StatsSize = pBuff->size - packetSize;
    
	//try to compress
	if(StatsSize > (size_t)g_cfg.DataSizeForCompression)
	{
        bbuff *pBuffOut;
        bbuff_create(pBuffOut);
        int compressionStatus = CCommon_compressGzip(g_cfg.GzipCompressionLevel, pStatsDataBegin, StatsSize, pBuffOut, g_cfg.ZlibBufferSize);
        if(compressionStatus == Z_OK)
        {
            flags = ePF_COMPRESSED;
            //modify flags
            bbuff_put(pBuff, sizeof(token), &flags, sizeof(flags));
            //prepare space and rewrite noncompressed data
            bbuff_resize(pBuff, pBuffOut->size + packetSize);
            bbuff_put(pBuff, packetSize, pBuffOut->storage, pBuffOut->size);
        }
        else
        {
            Log.Error(__FUNCTION__, "Data compression failed.");
        }
        bbuff_destroy(pBuffOut);
	}
    
    //send back
    g_rClientSocketHolder.SendPacket(rTaskData.m_socketID, S_MSG_STATUS, pBuff);
    
    //clear memory
    bbuff_destroy(pBuff);
}

void ClientSocketWorkerTask::HandleDeleteX(ClientSocketTaskData &rTaskData)
{
    uint32 token;
    uint32 flags;
    uint8 *pXs;
    size_t XSize;

    //TODO: use bbuff
	//for decompresion
	ByteBuffer rBuffOut;
    
    //read data from packet
    bbuff_read(rTaskData.m_pData, &token, sizeof(token));
    bbuff_read(rTaskData.m_pData, &flags, sizeof(flags));
    
    //TODO: check overflow
    XSize = rTaskData.m_pData->size - rTaskData.m_pData->rpos;
    pXs = (uint8*)(rTaskData.m_pData->storage + rTaskData.m_pData->rpos);
    
    //decompress
    if(flags & ePF_COMPRESSED)
    {
        int decompressionStatus = CommonFunctions::decompressGzip(pXs, XSize, rBuffOut, g_cfg.ZlibBufferSize);
        if(decompressionStatus == Z_OK)
        {
            Log.Debug(__FUNCTION__, "Data decompressed. Original size: %u, new size: %u", XSize, rBuffOut.size());
            flags = ePF_NULL;
            XSize = rBuffOut.size();
            pXs = (uint8*)rBuffOut.contents();
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
        //iterate and delete all users
        size_t rpos = 0;
        while(rpos < XSize)
        {
            uint64 X = *(uint64*)&pXs[rpos];
            rpos += sizeof(X);
            
            //delete whole data undex X key
            m_rStorage.DeleteData(*m_pLRUCache, X);
        }
    }
    
    //send data back
    uint8 buff[32];
    StackPacket rResponse(S_MSG_DELETE_X, buff, sizeof(buff));
    rResponse << token;
    rResponse << flags;
    g_rClientSocketHolder.SendPacket(rTaskData.m_socketID, rResponse);
}

void ClientSocketWorkerTask::HandleDefragmentData(ClientSocketTaskData &rTaskData)
{
    uint32 token;
    uint32 flags;
    uint8 *pXs;
    size_t XSize;
    
    //for compresion
    ByteBuffer rBuffOut;
    
    //read data from packet
    bbuff_read(rTaskData.m_pData, &token, sizeof(token));
    bbuff_read(rTaskData.m_pData, &flags, sizeof(flags));
    
    //TODO: check overflow
    XSize = rTaskData.m_pData->size - rTaskData.m_pData->rpos;
    pXs = (uint8*)(rTaskData.m_pData->storage + rTaskData.m_pData->rpos);
    
    //decompress
    if(flags & ePF_COMPRESSED)
    {
        int decompressionStatus = CommonFunctions::decompressGzip(pXs, XSize, rBuffOut, g_cfg.ZlibBufferSize);
        if(decompressionStatus == Z_OK)
        {
            Log.Debug(__FUNCTION__, "Data decompressed. Original size: %u, new size: %u", XSize, rBuffOut.size());
            flags = ePF_NULL;
            XSize = rBuffOut.size();
            pXs = (uint8*)rBuffOut.contents();
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
        //iterate and delete all users
        size_t rpos = 0;
        while(rpos < XSize)
        {
            uint64 X = *(uint64*)&pXs[rpos];
            rpos += sizeof(X);
            
            //defragment data under X key
            m_rStorage.DefragmentData(m_rDataFileHandle, *m_pLRUCache, X);
        }
    }
    
    //send data back
    uint8 buff[32];
    StackPacket rResponse(S_MSG_DEFRAMENT_DATA, buff, sizeof(buff));
    rResponse << token;
    rResponse << flags;
    g_rClientSocketHolder.SendPacket(rTaskData.m_socketID, rResponse);
}

void ClientSocketWorkerTask::HandleGetFreeSpace(ClientSocketTaskData &rTaskData)
{
    uint32 token;
    uint32 flags;
    uint32 dumpFlags; //0 - full dump, 1 - only counts
    
    //read data from packet
    bbuff_read(rTaskData.m_pData, &token, sizeof(token));
    bbuff_read(rTaskData.m_pData, &flags, sizeof(flags));
    bbuff_read(rTaskData.m_pData, &dumpFlags, sizeof(dumpFlags));
    
    //get data
    m_rStorage.GetFreeSpaceDump(rTaskData.m_socketID, token, flags, dumpFlags);
    
    //Response will be send from DiskWriter
}

void ClientSocketWorkerTask::HandleWriteDataNum(ClientSocketTaskData &rTaskData)
{
    uint32 token;
    uint32 flags;
    uint64 X;
    size_t dataSize;
    uint8 *pData;
    ByteBuffer rData;
    uint32 writeStatus = 0;
    
    //TODO: use bbuff
	//for decompresion
	ByteBuffer rBuffOut;
    
    //read data from packet
    bbuff_read(rTaskData.m_pData, &token, sizeof(token));
    bbuff_read(rTaskData.m_pData, &flags, sizeof(flags));
    bbuff_read(rTaskData.m_pData, &X, sizeof(X));
    
    //TODO: check overflow
    //get data size
    dataSize = rTaskData.m_pData->size - rTaskData.m_pData->rpos;
    pData = (uint8*)(rTaskData.m_pData->storage + rTaskData.m_pData->rpos);
    
    //decompress
    if(flags & ePF_COMPRESSED)
    {
        int decompressionStatus = CommonFunctions::decompressGzip(pData, dataSize, rBuffOut, g_cfg.ZlibBufferSize);
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
            Log.Error(__FUNCTION__, "Decompression failed. X: " I64FMTD, X);
        }
    }
    
    //only if decompression is ok
    if(flags == ePF_NULL)
    {
        //iterate and write
        //key|recordSize|record|....Nx
        size_t rpos = 0;
        while(rpos < dataSize)
        {
            //get key
            uint64 recordKey = *(uint64*)&pData[rpos];
            rpos += sizeof(recordKey);
            //get record size
            uint16 recordSize = *(uint16*)&pData[rpos];
            rpos += sizeof(recordSize);

            //now rpos is pointing to begin of data
            //so read pass ptr and record size directly to storage
            writeStatus |= m_rStorage.WriteData(m_rDataFileHandle, *m_pLRUCache, X, recordKey, &pData[rpos], recordSize);
            //uodate rpos
            rpos += recordSize;
        }
    }
    
    //send back data
    uint8 buff[32];
    StackPacket rResponse(S_MSG_WRITE_DATA_NUM, buff, sizeof(buff));
    rResponse << token;
    rResponse << flags;
    rResponse << X;
    rResponse << writeStatus;
    g_rClientSocketHolder.SendPacket(rTaskData.m_socketID, rResponse);
}

void ClientSocketWorkerTask::HandleReadLog(ClientSocketTaskData &rTaskData)
{
    uint32 token;
    uint32 flags;
    static const size_t packetSize = sizeof(token)+sizeof(flags);
    
    //read data from packet
    bbuff_read(rTaskData.m_pData, &token, sizeof(token));
    bbuff_read(rTaskData.m_pData, &flags, sizeof(flags));
    
    //buffer for log
    bbuff *pBuff;
    bbuff_create(pBuff);
    
    //prepare packet
    bbuff_reserve(pBuff, packetSize);
    bbuff_append(pBuff, &token, sizeof(token));
    bbuff_append(pBuff, &flags, sizeof(flags));
    
    //read log file from disk
    Log.GetFileLogContent(pBuff);
    
    //calc where data start and what size it has
    uint8 *pLogDataBegin = pBuff->storage + packetSize;
    size_t logSize = pBuff->size - packetSize;
    
    //try to compress
    if(logSize > (size_t)g_cfg.DataSizeForCompression)
    {
        bbuff *pBuffOut;
        bbuff_create(pBuffOut);
        int compressionStatus = CCommon_compressGzip(g_cfg.GzipCompressionLevel, pLogDataBegin, logSize, pBuffOut, g_cfg.ZlibBufferSize);
        if(compressionStatus == Z_OK)
        {
            flags = ePF_COMPRESSED;
            //modify flags
            bbuff_put(pBuff, sizeof(token), &flags, sizeof(flags));
            //prepare space and rewrite noncompressed data
            bbuff_resize(pBuff, pBuffOut->size + packetSize);
            bbuff_put(pBuff, packetSize, pBuffOut->storage, pBuffOut->size);
        }
        else
        {
            Log.Error(__FUNCTION__, "Data compression failed.");
        }
        bbuff_destroy(pBuffOut);
    }
    
    //send back
    g_rClientSocketHolder.SendPacket(rTaskData.m_socketID, S_MSG_READ_LOG, pBuff);
    
    //clear memory
    bbuff_destroy(pBuff);
}

void ClientSocketWorkerTask::HandleReadConfig(ClientSocketTaskData &rTaskData)
{
    uint32 token;
    uint32 flags;
    static const size_t packetSize = sizeof(token)+sizeof(flags);
    
    //read data from packet
    bbuff_read(rTaskData.m_pData, &token, sizeof(token));
    bbuff_read(rTaskData.m_pData, &flags, sizeof(flags));
    
    //buffer for config
    bbuff *pBuff;
    bbuff_create(pBuff);
    
    //prepare packet
    bbuff_reserve(pBuff, packetSize);
    bbuff_append(pBuff, &token, sizeof(token));
    bbuff_append(pBuff, &flags, sizeof(flags));
    
    //get config path
    std::string sConfigPath = g_rConfig.MainConfig.GetConfigFilePath();
    //fill buffer with data
    HANDLE hFile = INVALID_HANDLE_VALUE;
    IOHandleGuard rIOGuard(hFile);
    hFile = IO::fopen(sConfigPath.c_str(), IO::IO_READ_ONLY, IO::IO_NORMAL);
    if(hFile != INVALID_HANDLE_VALUE)
    {
        IO::fseek(hFile, 0, IO::IO_SEEK_END);
        int64 fileSize = IO::ftell(hFile);
        IO::fseek(hFile, 0, IO::IO_SEEK_SET);
        
        //prealloc buffer
        bbuff_reserve(pBuff, pBuff->wpos + fileSize);
        
        //read to buffer
        //append data
        uint8 buff[4096];
        size_t bytesRead;
        while((bytesRead = IO::fread(buff, sizeof(buff), hFile)) > 0)
        {
            bbuff_append(pBuff, buff, bytesRead);
        }
    }
    
    //calc where data start and what size it has
    uint8 *pCfgDataBegin = pBuff->storage + packetSize;
    size_t cfgSize = pBuff->size - packetSize;
    
    //try to compress
    if(cfgSize > (size_t)g_cfg.DataSizeForCompression)
    {
        bbuff *pBuffOut;
        bbuff_create(pBuffOut);
        int compressionStatus = CCommon_compressGzip(g_cfg.GzipCompressionLevel, pCfgDataBegin, cfgSize, pBuffOut, g_cfg.ZlibBufferSize);
        if(compressionStatus == Z_OK)
        {
            flags = ePF_COMPRESSED;
            //modify flags
            bbuff_put(pBuff, sizeof(token), &flags, sizeof(flags));
            //prepare space and rewrite noncompressed data
            bbuff_resize(pBuff, pBuffOut->size + packetSize);
            bbuff_put(pBuff, packetSize, pBuffOut->storage, pBuffOut->size);
        }
        else
        {
            Log.Error(__FUNCTION__, "Data compression failed.");
        }
        bbuff_destroy(pBuffOut);
    }
    
    //send back
    g_rClientSocketHolder.SendPacket(rTaskData.m_socketID, S_MSG_READ_CONFIG, pBuff);
    
    //clear memory
    bbuff_destroy(pBuff);
}

void ClientSocketWorkerTask::HandleDefragmentFreeSpace(ClientSocketTaskData &rTaskData)
{
    uint32 token;
    uint32 flags;
    
    //read data from packet
    bbuff_read(rTaskData.m_pData, &token, sizeof(token));
    bbuff_read(rTaskData.m_pData, &flags, sizeof(flags));
    
    //process
    m_rStorage.DefragmentFreeSpace(rTaskData.m_socketID, token, flags);
    
    //Response will be send from DiskWriter
}

void ClientSocketWorkerTask::HandleExecutePythonScript(ClientSocketTaskData &rTaskData)
{
    uint32 token;
    uint32 flags;
    size_t dataSize;
    uint8 *pData;
    static const size_t packetSize = sizeof(token)+sizeof(flags);
    
    //read data from packet
    bbuff_read(rTaskData.m_pData, &token, sizeof(token));
    bbuff_read(rTaskData.m_pData, &flags, sizeof(flags));

    //TODO: check overflow
    //get data size
    dataSize = rTaskData.m_pData->size - rTaskData.m_pData->rpos;
    pData = (uint8*)(rTaskData.m_pData->storage + rTaskData.m_pData->rpos);

    //buffer for python script result
    bbuff *pBuff;
    bbuff_create(pBuff);
    
    //prepare packet
    bbuff_reserve(pBuff, packetSize);
    bbuff_append(pBuff, &token, sizeof(token));
    bbuff_append(pBuff, &flags, sizeof(flags));
    
    //execute
    m_rPythonInterface.executePythonScript(&m_rStorage, m_pLRUCache, m_rDataFileHandle, pData, dataSize, pBuff);
    
    //calc where data start and what size it has
    uint8 *pResultDataBegin = pBuff->storage + packetSize;
    size_t resultSize = pBuff->size - packetSize;
    
    //try to compress
    if(resultSize > (size_t)g_cfg.DataSizeForCompression)
    {
        bbuff *pBuffOut;
        bbuff_create(pBuffOut);
        int compressionStatus = CCommon_compressGzip(g_cfg.GzipCompressionLevel, pResultDataBegin, resultSize, pBuffOut, g_cfg.ZlibBufferSize);
        if(compressionStatus == Z_OK)
        {
            flags = ePF_COMPRESSED;
            //modify flags
            bbuff_put(pBuff, sizeof(token), &flags, sizeof(flags));
            //prepare space and rewrite noncompressed data
            bbuff_resize(pBuff, pBuffOut->size + packetSize);
            bbuff_put(pBuff, packetSize, pBuffOut->storage, pBuffOut->size);
        }
        else
        {
            Log.Error(__FUNCTION__, "Data compression failed.");
        }
        bbuff_destroy(pBuffOut);
    }
    
    //send back
    g_rClientSocketHolder.SendPacket(rTaskData.m_socketID, S_MSG_EXEC_PYTHON_SCRIPT, pBuff);
    
    //clear memory
    bbuff_destroy(pBuff);
}


















