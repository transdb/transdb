//
//  CfgDefines.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 20.01.14.
//  Copyright (c) 2014 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

//cfg
bool    g_EnableRecordLimit         = true;
int     g_RecordLimit               = 1000;
int     g_TransactionsPerCommit     = 1000;
int     g_DiskFlushCoef             = 100;
string  g_DataFilePath              = "";
string  g_IndexFilePath             = "";
uint64  g_MemoryLimit               = 64*1024*1024;
int     g_MemoryFlushCoef		    = 1;
uint64  g_ReallocSize               = 128;
string  g_ListenHost                = "0.0.0.0";
int     g_ListenPort                = 5555;
uint32  g_SocketReadBufferSize      = 32;
uint32  g_SocketWriteBufferSize     = 32;
bool	g_StartupCrc32Check 		= true;
int		g_GzipCompressionLevel		= 4;
int		g_ZlibBufferSize			= 128*1024;
int		g_DataSizeForCompression	= 1024;
string  g_DataFileName              = "data";
string  g_ActivityID                = "pool-3";
int     g_PingTimeout               = 60;
int     g_PingSendInterval          = 15;
uint64	g_LRUCacheMemReserve		= 4*1024*1024;
int     g_MemoryPoolsRecycleTimer   = 60*60;
int     g_NetworkThreads            = 1;
int     g_MaxParallelTasks          = 4;
int     g_MaxParallelReadTasks      = 4;
int     g_MaxTasksInQueue           = 100;
int     g_MaxReadTasksInQueue       = 100;
int     g_WebSocketPort             = 8888;
uint64  g_IndexBlockCacheSize       = 16*1024*1024;
int     g_RecordSizeForCompression  = 200;
int     g_DefragAfterRecordDelete   = 5;

//socket ID gen
std::atomic<uint64> g_SocketID;

//start time
time_t g_StartTime                  = 0;

//for statistics
std::atomic<uint64> g_NumOfReadsFromDisk;
std::atomic<uint64> g_NumOfReadsFromCache;
std::atomic<uint64> g_NumOfWritesToDisk;
std::atomic<size_t> g_ReceivedBytes;
std::atomic<size_t> g_SendedBytes;
std::atomic<size_t> g_NumOfRecordCompressions;
std::atomic<size_t> g_NumOfRecordDecompressions;
std::atomic<size_t> g_NumOfRecordDeframentations;
std::atomic<uint64> g_AvgDiskReadTime;
std::atomic<uint64> g_AvgDiskWriteTime;

//CRC
CRC_32 *g_CRC32;

//version
#ifdef DEBUG
const char *g_pCompiledVersion = "DEBUG";
#else
const char *g_pCompiledVersion = "RELEASE";
#endif

//python
string g_PythonHome                 = "";
string g_PythonScriptsFolderPath    = "/Users/miroslavkudrnac/SVN/TransStorageServer/trunk/Python/";
string g_PythonModuleName           = "main";
string g_PythonClassName            = "TestClass";
string g_PythonRunableMethod        = "run";
string g_PythonShutdownMethod       = "OnShutdown";
string g_PythonScriptVersion        = "0";

//load config
void LoadConfig()
{
    int status;
    int memmoryLimitMB          = (int)g_MemoryLimit;
    int reallocSizeMB           = (int)g_ReallocSize;
    int readBufferSize          = (int)g_SocketReadBufferSize;
    int writeBufferSize         = (int)g_SocketWriteBufferSize;
    int logLevel                = 3;
    int indexBlockCacheSize     = (int)g_IndexBlockCacheSize;
    int LRUCacheMemReserve      = (int)g_LRUCacheMemReserve;
    
    //if status will 1 there is missing config
    status = 0;
    
#define GET_CONFIG_VALUE(Status, Function, Section, Key, Variable)                                          \
        do                                                                                                  \
        {                                                                                                   \
            bool internalStatus = g_rConfig.MainConfig.Function(Section, Key, Variable);                    \
            Status |= !internalStatus;                                                                      \
            if(internalStatus == false)                                                                     \
            {                                                                                               \
                std::stringstream ss;                                                                       \
                ss << *Variable;                                                                            \
                Log.Error(__FUNCTION__, "Missing config value in section: %s key: %s, using default value: %s.", Section, Key, ss.str().c_str());       \
            }                                                                                               \
        }while(0)                                                                                           \

    //load values from config
    GET_CONFIG_VALUE(status, GetString, "Activity", "ID", &g_ActivityID);
    GET_CONFIG_VALUE(status, GetString, "Storage", "DataFilePath", &g_DataFilePath);
    GET_CONFIG_VALUE(status, GetString, "Storage", "IndexFilePath", &g_IndexFilePath);
    GET_CONFIG_VALUE(status, GetBool, "Storage", "EnableRecordLimit", &g_EnableRecordLimit);
    GET_CONFIG_VALUE(status, GetInt, "Storage", "RecordLimit", &g_RecordLimit);
    GET_CONFIG_VALUE(status, GetInt, "Storage", "DiskFlushCoef", &g_DiskFlushCoef);
    GET_CONFIG_VALUE(status, GetInt, "Storage", "ReallocSize", &reallocSizeMB);
	GET_CONFIG_VALUE(status, GetBool, "Storage", "StartupCrc32Check", &g_StartupCrc32Check);
	GET_CONFIG_VALUE(status, GetString, "Storage", "DataFileName", &g_DataFileName);
	GET_CONFIG_VALUE(status, GetInt, "Storage", "DefragAfterRecordDelete", &g_DefragAfterRecordDelete);
    GET_CONFIG_VALUE(status, GetInt, "Storage", "TransactionsPerCommit", &g_TransactionsPerCommit);
    GET_CONFIG_VALUE(status, GetInt, "Memory", "MemoryLimit", &memmoryLimitMB);
    GET_CONFIG_VALUE(status, GetInt, "Memory", "MemoryFlushCoef", &g_MemoryFlushCoef);
    GET_CONFIG_VALUE(status, GetInt, "Memory", "LRUCacheMemReserve", &LRUCacheMemReserve);
    GET_CONFIG_VALUE(status, GetInt, "Memory", "IndexBlockCache", &indexBlockCacheSize);
    GET_CONFIG_VALUE(status, GetInt, "Memory", "MemoryPoolsRecycleTimer", &g_MemoryPoolsRecycleTimer);
    GET_CONFIG_VALUE(status, GetString, "PublicSocket", "Host", &g_ListenHost);
    GET_CONFIG_VALUE(status, GetInt, "PublicSocket", "Port", &g_ListenPort);
    GET_CONFIG_VALUE(status, GetInt, "PublicSocket", "ReadBufferSize", &readBufferSize);
    GET_CONFIG_VALUE(status, GetInt, "PublicSocket", "WriteBufferSize", &writeBufferSize);
    GET_CONFIG_VALUE(status, GetInt, "PublicSocket", "PingTimeout", &g_PingTimeout);
    GET_CONFIG_VALUE(status, GetInt, "PublicSocket", "PingSendInterval", &g_PingSendInterval);
    GET_CONFIG_VALUE(status, GetInt, "PublicSocket", "NetworkThreads", &g_NetworkThreads);
    GET_CONFIG_VALUE(status, GetInt, "PublicSocket", "MaxParallelTasks", &g_MaxParallelTasks);
    GET_CONFIG_VALUE(status, GetInt, "PublicSocket", "MaxParallelReadTasks", &g_MaxParallelReadTasks);
    GET_CONFIG_VALUE(status, GetInt, "PublicSocket", "MaxTasksInQueue", &g_MaxTasksInQueue);
    GET_CONFIG_VALUE(status, GetInt, "PublicSocket", "MaxReadTasksInQueue", &g_MaxReadTasksInQueue);
    GET_CONFIG_VALUE(status, GetInt, "PublicSocket", "WebSocketPort", &g_WebSocketPort);
    GET_CONFIG_VALUE(status, GetInt, "Log", "Level", &logLevel);
	GET_CONFIG_VALUE(status, GetInt, "Compression", "Level", &g_GzipCompressionLevel);
	GET_CONFIG_VALUE(status, GetInt, "Compression", "BufferSize", &g_ZlibBufferSize);
	GET_CONFIG_VALUE(status, GetInt, "Compression", "DataSizeForCompression", &g_DataSizeForCompression);
    GET_CONFIG_VALUE(status, GetInt, "Compression", "RecordSizeForCompression", &g_RecordSizeForCompression);
    GET_CONFIG_VALUE(status, GetString, "Python", "PythonHome", &g_PythonHome);
    GET_CONFIG_VALUE(status, GetString, "Python", "PythonScriptsFolderPath", &g_PythonScriptsFolderPath);
    GET_CONFIG_VALUE(status, GetString, "Python", "PythonModuleName", &g_PythonModuleName);
    GET_CONFIG_VALUE(status, GetString, "Python", "PythonClassName", &g_PythonClassName);
    GET_CONFIG_VALUE(status, GetString, "Python", "PythonRunableMethod", &g_PythonRunableMethod);
    GET_CONFIG_VALUE(status, GetString, "Python", "PythonShutdownMethod", &g_PythonShutdownMethod);
    GET_CONFIG_VALUE(status, GetString, "Python", "PythonScriptVersion", &g_PythonScriptVersion);
    
#undef GET_CONFIG_VALUE
    
    //adjust to bytes from MB
    g_ReallocSize = (uint64)reallocSizeMB*1024*1024;
    g_MemoryLimit = (uint64)memmoryLimitMB*1024*1024;
    g_SocketReadBufferSize = (uint32)readBufferSize*1024;
    g_SocketWriteBufferSize = (uint32)writeBufferSize*1024;
    g_IndexBlockCacheSize = (uint64)indexBlockCacheSize*1024*1024;
    g_LRUCacheMemReserve = (uint64)LRUCacheMemReserve*1024*1024;
    
    //Set log level
    Log.SetLogLevel(logLevel);
    
    //
    Log.Notice(__FUNCTION__, "Config loaded with status: %s", status == 0 ? "Ok" : "Error");
}
