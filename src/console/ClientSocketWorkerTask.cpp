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

ClientSocketWorkerTask::ClientSocketWorkerTask(ClientSocketWorker &rClientSocketWorker,
                                               Storage &rStorage,
                                               PythonInterface &rPythonInterface,
                                               ConfigWatcher &rConfigWatcher,
                                               bool readerTask) : m_rClientSocketWorker(rClientSocketWorker),
                                                                  m_rStorage(rStorage),
                                                                  m_rPythonInterface(rPythonInterface),
                                                                  m_rConfigWatcher(rConfigWatcher),
                                                                  m_pLRUCache(new LRUCache("ClientSocketWorkerTask", g_cfg.LRUCacheMemReserve / sizeof(CRec))),
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
    m_rDataFileHandle = IO::fopen(m_rStorage.m_rDataPath.c_str(), IO::IO_READ_ONLY, IO::IO_DIRECT);
    if(m_rDataFileHandle == INVALID_HANDLE_VALUE)
    {
        Log.Error(__FUNCTION__, "Cannot open data file: %s", m_rStorage.m_rDataPath.c_str());
        return true;
    }
    
    //data from queue
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
                Log.Warning(__FUNCTION__, "Unknown opcode (0x%.4X)", pClientSocketTaskData->opcode());
            }
            
            //check memory
            m_rStorage.CheckMemory(*m_pLRUCache);
                        
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
    
    //TODO: check overflow
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
    uint64 X;
    uint64 Y;
    static const size_t packetSize = sizeof(token)+sizeof(flags)+sizeof(X)+sizeof(Y);
    
    //read data from packet
    rClientSocketTaskData >> token >> flags >> X >> Y;
    
    //buffer for read data
    bbuff *pReadData = bbuff_create();
    
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
        bbuff *pBuffOut = bbuff_create();
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
    g_rClientSocketHolder.SendPacket(rClientSocketTaskData.socketID(), S_MSG_READ_DATA, pReadData);
    
    //clear memory
    bbuff_destroy(pReadData);
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
//    uint32 token;
//    uint32 flags;
//    uint32 sortFlags;
//    OUTPACKET_RESULT result;
//    size_t XKeysSize;
//    
//    //vector
//    XKeyVec rXKeyVec;
//    
//    //read data from packet
//    rClientSocketTaskData >> token >> flags >> sortFlags;
//    
//    //load all keys
//    m_rStorage.GetAllX(rXKeyVec, sortFlags);
//    
//    //calc size
//    XKeysSize = rXKeyVec.size() * sizeof(XKeyVec::value_type);
//    
//    //init stream send
//    uint8 buff[32];
//    StackPacket rResponse(S_MSG_GET_ALL_X, buff, sizeof(buff));
//    rResponse << token;
//    rResponse << flags;
//    result = g_rClientSocketHolder.StartStreamSend(rClientSocketTaskData.socketID(), rResponse, XKeysSize);
//    //socket disconnection or something go to next socket
//    if(result != OUTPACKET_RESULT_SUCCESS)
//    {
//        Log.Error(__FUNCTION__, "StartStreamSend - Socket ID: " I64FMTD " disconnected.", rClientSocketTaskData.socketID());
//        return;
//    }
//    
//    //send all chunks
//    size_t chunkSize = g_cfg.SocketWriteBufferSize / 2;
//    ByteBuffer rChunk(chunkSize);
//    uint32 sendCounter = 0;
//    //
//    for(size_t i = 0;i < rXKeyVec.size();++i)
//    {
//        //add data to chunk
//        rChunk << rXKeyVec[i];
//        
//        //if chunk full or we are on end, perform send
//        if(rChunk.size() >= chunkSize || (i == (rXKeyVec.size() - 1)))
//        {
//            //send chunk
//        trySendAgain:
//            result = g_rClientSocketHolder.StreamSend(rClientSocketTaskData.socketID(), rChunk.contents(), rChunk.size());
//            if(result == OUTPACKET_RESULT_SUCCESS)
//            {
//                //clear chunk
//                rChunk.resize(0);
//                //send is ok continue with sending
//                continue;
//            }
//            else if(result == OUTPACKET_RESULT_NO_ROOM_IN_BUFFER)
//            {
//                if(sendCounter > 100)
//                {
//                    Log.Error(__FUNCTION__, "StreamSend no room in send buffer.");
//                    break;
//                }
//                
//                //socket buffer is full wait
//                Wait(100);
//                ++sendCounter;
//                goto trySendAgain;
//            }
//            else
//            {
//                //error interupt sending
//                Log.Error(__FUNCTION__, "StreamSend error: %u", (uint32)result);
//                break;
//            }
//        }
//    }
}

void ClientSocketWorkerTask::HandleGetAllY(ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    uint64 X;
    static const size_t packetSize = sizeof(token)+sizeof(flags)+sizeof(X);
    
    //buffer for y keys
    ByteBuffer rY;
    
    //read data from packet
    rClientSocketTaskData >> token >> flags >> X;
    
    //buffer for all Y
    bbuff *pY = bbuff_create();
    
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
        bbuff *pBuffOut = bbuff_create();
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
    g_rClientSocketHolder.SendPacket(rClientSocketTaskData.socketID(), S_MSG_GET_ALL_Y, pY);
    
    //clear memory
    bbuff_destroy(pY);
}

void ClientSocketWorkerTask::HandleStatus(ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    static const size_t packetSize = sizeof(token)+sizeof(flags);
    
    //read data from packet
    rClientSocketTaskData >> token >> flags;
    
    //buffer for stats
    bbuff *pBuff = bbuff_create();
    
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
        bbuff *pBuffOut = bbuff_create();
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
    g_rClientSocketHolder.SendPacket(rClientSocketTaskData.socketID(), S_MSG_STATUS, pBuff);
    
    //clear memory
    bbuff_destroy(pBuff);
}

void ClientSocketWorkerTask::HandleDeleteX(ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    uint8 *pXs;
    size_t XSize;

	//for decompresion
	ByteBuffer rBuffOut;
    
    //TODO: check overflow
    //read data from packet
    rClientSocketTaskData >> token >> flags;
    XSize = rClientSocketTaskData.size() - rClientSocketTaskData.rpos();
    pXs = (uint8*)(rClientSocketTaskData.contents() + rClientSocketTaskData.rpos());
    
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
    g_rClientSocketHolder.SendPacket(rClientSocketTaskData.socketID(), rResponse);
}

void ClientSocketWorkerTask::HandleDefragmentData(ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    uint8 *pXs;
    size_t XSize;
    
	//for compresion
	ByteBuffer rBuffOut;
    
    //TODO: check overflow
    //read data from packet
    rClientSocketTaskData >> token >> flags;
    XSize = rClientSocketTaskData.size() - rClientSocketTaskData.rpos();
    pXs = (uint8*)(rClientSocketTaskData.contents() + rClientSocketTaskData.rpos());
    
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
    g_rClientSocketHolder.SendPacket(rClientSocketTaskData.socketID(), rResponse);
}

void ClientSocketWorkerTask::HandleGetFreeSpace(ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    uint32 dumpFlags; //0 - full dump, 1 - only counts
    
    //read data from packet
    rClientSocketTaskData >> token >> flags >> dumpFlags;
    
    //get data
    m_rStorage.GetFreeSpaceDump(rClientSocketTaskData.socketID(), token, flags, dumpFlags);
    
    //Response will be send from DiskWriter
}

void ClientSocketWorkerTask::HandleWriteDataNum(ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    uint64 X;
    size_t dataSize;
    uint8 *pData;
    ByteBuffer rData;
    uint32 writeStatus = 0;
    
	//for decompresion
	ByteBuffer rBuffOut;
    
    //read data from packet
    rClientSocketTaskData >> token >> flags >> X;
    
    //TODO: check overflow
    //get data size
    dataSize = rClientSocketTaskData.size() - rClientSocketTaskData.rpos();
    pData = (uint8*)(rClientSocketTaskData.contents() + rClientSocketTaskData.rpos());
    
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
    g_rClientSocketHolder.SendPacket(rClientSocketTaskData.socketID(), rResponse);
}

void ClientSocketWorkerTask::HandleReadLog(ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    static const size_t packetSize = sizeof(token)+sizeof(flags);
    
    //read data from packet
    rClientSocketTaskData >> token >> flags;
    
    //buffer for log
    bbuff *pBuff = bbuff_create();
    
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
        bbuff *pBuffOut = bbuff_create();
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
    g_rClientSocketHolder.SendPacket(rClientSocketTaskData.socketID(), S_MSG_READ_LOG, pBuff);
    
    //clear memory
    bbuff_destroy(pBuff);
}

void ClientSocketWorkerTask::HandleReadConfig(ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    static const size_t packetSize = sizeof(token)+sizeof(flags);
    
    //read data from packet
    rClientSocketTaskData >> token >> flags;
    
    //buffer for config
    bbuff *pBuff = bbuff_create();
    
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
        bbuff *pBuffOut = bbuff_create();
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
    g_rClientSocketHolder.SendPacket(rClientSocketTaskData.socketID(), S_MSG_READ_CONFIG, pBuff);
    
    //clear memory
    bbuff_destroy(pBuff);
}

void ClientSocketWorkerTask::HandleDefragmentFreeSpace(ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    
    //read data from packet
    rClientSocketTaskData >> token >> flags;
    
    //process
    m_rStorage.DefragmentFreeSpace(rClientSocketTaskData.socketID(), token, flags);
    
    //Response will be send from DiskWriter
}

void ClientSocketWorkerTask::HandleExecutePythonScript(ClientSocketTaskData &rClientSocketTaskData)
{
    uint32 token;
    uint32 flags;
    size_t dataSize;
    uint8 *pData;
    static const size_t packetSize = sizeof(token)+sizeof(flags);
    
    //read data from packet
    rClientSocketTaskData >> token >> flags;

    //TODO: check overflow
    //get data size
    dataSize = rClientSocketTaskData.size() - rClientSocketTaskData.rpos();
    pData = (uint8*)(rClientSocketTaskData.contents() + rClientSocketTaskData.rpos());

    //buffer for python script result
    bbuff *pBuff = bbuff_create();
    
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
        bbuff *pBuffOut = bbuff_create();
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
    g_rClientSocketHolder.SendPacket(rClientSocketTaskData.socketID(), S_MSG_EXEC_PYTHON_SCRIPT, pBuff);
    
    //clear memory
    bbuff_destroy(pBuff);
}


















