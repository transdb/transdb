//
//  CfgDefines.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 20.01.14.
//  Copyright (c) 2014 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

//config holder
ConfigDefines   g_cfg;
//stats holder
Statistics      g_stats;

/** Fill ConfigDefines structure with default values
 */
void init_config_default_values(ConfigDefines *self)
{
    //ID
    strncpy(self->ActivityID, "pool-3", sizeof(self->ActivityID));
    
    //Storage
    strncpy(self->DataFilePath, "", sizeof(self->DataFilePath));
    strncpy(self->IndexFilePath, "", sizeof(self->IndexFilePath));
    strncpy(self->DataFileName, "data", sizeof(self->DataFileName));
    self->EnableRecordLimit         = true;
    self->RecordLimit               = 1000;
    self->DiskFlushCoef             = 100;
    self->ReallocSize               = 128;
    self->StartupCrc32Check         = true;
    self->DefragAfterRecordDelete   = 5;
    self->FreeSpaceDefrag           = 24*60*60;
    
    //Memory
    self->MemoryLimit               = 64*1024*1024;
    self->LRUCacheMemReserve        = 1*1024*1024;
    self->IndexBlockCacheSize       = 1*1024*1024;
    
    //PublicSocket
    strncpy(self->ListenHost, "0.0.0.0", sizeof(self->ListenHost));
    self->ListenPort                = 5555;
    self->WebSocketPort             = 8888;
    self->SocketReadBufferSize      = 32;
    self->SocketWriteBufferSize     = 32;
    self->PingTimeout               = 60;
    self->PingSendInterval          = 15;
    self->NagleLatency              = 250;
    self->MaxParallelTasks          = 4;
    self->MaxParallelReadTasks      = 4;
    self->MaxTasksInQueue           = 64;
    self->MaxReadTasksInQueue       = 64;
    
    //python
    self->PythonEnable              = true;    
    strncpy(self->PythonHome, "", sizeof(self->PythonHome));
    strncpy(self->PythonScriptsFolderPath, "/Users/miroslavkudrnac/SVN/TransStorageServer/trunk/Python/", sizeof(self->PythonScriptsFolderPath));
    strncpy(self->PythonModuleName, "main", sizeof(self->PythonModuleName));
    strncpy(self->PythonClassName, "TestClass", sizeof(self->PythonClassName));
    strncpy(self->PythonRunableMethod, "run", sizeof(self->PythonRunableMethod));
    strncpy(self->PythonShutdownMethod, "OnShutdown", sizeof(self->PythonShutdownMethod));
    strncpy(self->PythonScriptVersion, "0", sizeof(self->PythonScriptVersion));
    
    //zlib
    self->GzipCompressionLevel      = 5;
    self->ZlibBufferSize            = 512*1024;
    self->DataSizeForCompression    = 1024;
    self->RecordSizeForCompression  = 256;
}

/** Init stats structure
 */
void init_stats_default_values(Statistics *self)
{
    //start time
    self->StartTime                     = UNIXTIME;
    
    //for statistics
    self->NumOfReadsFromDisk            = 0;
    self->NumOfReadsFromCache           = 0;
    self->NumOfWritesToDisk             = 1;
    self->ReceivedBytes                 = 0;
    self->SendedBytes                   = 0;
    self->NumOfRecordCompressions       = 0;
    self->NumOfRecordDecompressions     = 0;
    self->NumOfRecordDeframentations    = 0;
    self->AvgDiskReadTime               = 0;
    self->AvgDiskWriteTime              = 0;
}

//version
#ifdef DEBUG
    const char *g_pCompiledVersion = "DEBUG";
#else
    const char *g_pCompiledVersion = "RELEASE";
#endif

//malloc alignment for disk IO
int g_DataFileMallocAlignment			 = 512;
int g_IndexFileMallocAlignment			 = 512;

//force startrup
E_FSA g_ForceStartup                     = eFSA_Abort;

void GET_CONFIG_BOOL_VALUE(int *outStatus, const char *section, const char *key, bool *outVariable)
{
    bool internalStatus = g_rConfig.MainConfig.GetBool(section, key, outVariable);
    *outStatus |= !internalStatus;
    if(internalStatus == false)
    {
        Log.Error(__FUNCTION__, "Missing config value in section: %s key: %s, using default value: %s.", section, key, *outVariable ? "yes" : "no");
    }
}

void GET_CONFIG_INT_VALUE(int *outStatus, const char *section, const char *key, int *outVariable)
{
    bool internalStatus = g_rConfig.MainConfig.GetInt(section, key, outVariable);
    *outStatus |= !internalStatus;
    if(internalStatus == false)
    {
        Log.Error(__FUNCTION__, "Missing config value in section: %s key: %s, using default value: %d.", section, key, *outVariable);
    }
}

void GET_CONFIG_STRING_VALUE(int *outStatus, const char *section, const char *key, char *buffer, size_t len)
{
    std::string val;
    bool internalStatus = g_rConfig.MainConfig.GetString(section, key, &val);
    if(internalStatus == false)
    {
        Log.Error(__FUNCTION__, "Missing config value in section: %s key: %s, using default value: %s.", section, key, buffer);
        *outStatus |= !internalStatus;
    }
    else
    {
        size_t maxVariableSize = len - 1;
        if(val.length() > maxVariableSize)
        {
            Log.Error(__FUNCTION__,
                      "Value in section: %s key: %s, has length: %u max length is: %u, using default value: %s.",
                      section, key, (uint32)val.length(), maxVariableSize, buffer);
            *outStatus |= !false;
        }
        else
        {
            strncpy(buffer, val.c_str(), len);
            *outStatus |= !internalStatus;
        }
    }
}

//load config
void LoadConfig()
{
    //load default values
    init_config_default_values(&g_cfg);
    
    int status;
    int memmoryLimitMB          = (int)g_cfg.MemoryLimit;
    int reallocSizeMB           = (int)g_cfg.ReallocSize;
    int readBufferSize          = (int)g_cfg.SocketReadBufferSize;
    int writeBufferSize         = (int)g_cfg.SocketWriteBufferSize;
    int logLevel                = 3;
    int indexBlockCacheSize     = (int)g_cfg.IndexBlockCacheSize;
    int LRUCacheMemReserve      = (int)g_cfg.LRUCacheMemReserve;
    
    //if status will 1 there is missing config
    status = 0;
    
    //load values from config
    GET_CONFIG_STRING_VALUE(&status, "Activity", "ID", g_cfg.ActivityID, sizeof(g_cfg.ActivityID));
    GET_CONFIG_STRING_VALUE(&status, "Storage", "DataFilePath", g_cfg.DataFilePath, sizeof(g_cfg.DataFilePath));
    GET_CONFIG_STRING_VALUE(&status, "Storage", "IndexFilePath", g_cfg.IndexFilePath, sizeof(g_cfg.IndexFilePath));
    GET_CONFIG_BOOL_VALUE(&status, "Storage", "EnableRecordLimit", &g_cfg.EnableRecordLimit);
    GET_CONFIG_INT_VALUE(&status, "Storage", "RecordLimit", &g_cfg.RecordLimit);
    GET_CONFIG_INT_VALUE(&status, "Storage", "DiskFlushCoef", &g_cfg.DiskFlushCoef);
    GET_CONFIG_INT_VALUE(&status, "Storage", "ReallocSize", &reallocSizeMB);
	GET_CONFIG_BOOL_VALUE(&status, "Storage", "StartupCrc32Check", &g_cfg.StartupCrc32Check);
	GET_CONFIG_STRING_VALUE(&status, "Storage", "DataFileName", g_cfg.DataFileName, sizeof(g_cfg.DataFileName));
	GET_CONFIG_INT_VALUE(&status, "Storage", "DefragAfterRecordDelete", &g_cfg.DefragAfterRecordDelete);
    GET_CONFIG_INT_VALUE(&status, "Storage", "FreeSpaceDefrag", &g_cfg.FreeSpaceDefrag);
    GET_CONFIG_INT_VALUE(&status, "Memory", "MemoryLimit", &memmoryLimitMB);
    GET_CONFIG_INT_VALUE(&status, "Memory", "LRUCacheMemReserve", &LRUCacheMemReserve);
    GET_CONFIG_INT_VALUE(&status, "Memory", "IndexBlockCache", &indexBlockCacheSize);
    GET_CONFIG_STRING_VALUE(&status, "PublicSocket", "Host", g_cfg.ListenHost, sizeof(g_cfg.ListenHost));
    GET_CONFIG_INT_VALUE(&status, "PublicSocket", "Port", &g_cfg.ListenPort);
    GET_CONFIG_INT_VALUE(&status, "PublicSocket", "ReadBufferSize", &readBufferSize);
    GET_CONFIG_INT_VALUE(&status, "PublicSocket", "WriteBufferSize", &writeBufferSize);
    GET_CONFIG_INT_VALUE(&status, "PublicSocket", "PingTimeout", &g_cfg.PingTimeout);
    GET_CONFIG_INT_VALUE(&status, "PublicSocket", "PingSendInterval", &g_cfg.PingSendInterval);
    GET_CONFIG_INT_VALUE(&status, "PublicSocket", "NagleLatency", &g_cfg.NagleLatency);
    GET_CONFIG_INT_VALUE(&status, "PublicSocket", "MaxParallelTasks", &g_cfg.MaxParallelTasks);
    GET_CONFIG_INT_VALUE(&status, "PublicSocket", "MaxParallelReadTasks", &g_cfg.MaxParallelReadTasks);
    GET_CONFIG_INT_VALUE(&status, "PublicSocket", "MaxTasksInQueue", &g_cfg.MaxTasksInQueue);
    GET_CONFIG_INT_VALUE(&status, "PublicSocket", "MaxReadTasksInQueue", &g_cfg.MaxReadTasksInQueue);
    GET_CONFIG_INT_VALUE(&status, "PublicSocket", "WebSocketPort", &g_cfg.WebSocketPort);
    GET_CONFIG_INT_VALUE(&status, "Log", "Level", &logLevel);
	GET_CONFIG_INT_VALUE(&status, "Compression", "Level", &g_cfg.GzipCompressionLevel);
	GET_CONFIG_INT_VALUE(&status, "Compression", "BufferSize", &g_cfg.ZlibBufferSize);
	GET_CONFIG_INT_VALUE(&status, "Compression", "DataSizeForCompression", &g_cfg.DataSizeForCompression);
    GET_CONFIG_INT_VALUE(&status, "Compression", "RecordSizeForCompression", &g_cfg.RecordSizeForCompression);
    GET_CONFIG_BOOL_VALUE(&status, "Python", "PythonEnable", &g_cfg.PythonEnable);
    GET_CONFIG_STRING_VALUE(&status, "Python", "PythonHome", g_cfg.PythonHome, sizeof(g_cfg.PythonHome));
    GET_CONFIG_STRING_VALUE(&status, "Python", "PythonScriptsFolderPath", g_cfg.PythonScriptsFolderPath, sizeof(g_cfg.PythonScriptsFolderPath));
    GET_CONFIG_STRING_VALUE(&status, "Python", "PythonModuleName", g_cfg.PythonModuleName, sizeof(g_cfg.PythonModuleName));
    GET_CONFIG_STRING_VALUE(&status, "Python", "PythonClassName", g_cfg.PythonClassName, sizeof(g_cfg.PythonClassName));
    GET_CONFIG_STRING_VALUE(&status, "Python", "PythonRunableMethod", g_cfg.PythonRunableMethod, sizeof(g_cfg.PythonRunableMethod));
    GET_CONFIG_STRING_VALUE(&status, "Python", "PythonShutdownMethod", g_cfg.PythonShutdownMethod, sizeof(g_cfg.PythonShutdownMethod));
    GET_CONFIG_STRING_VALUE(&status, "Python", "PythonScriptVersion", g_cfg.PythonScriptVersion, sizeof(g_cfg.PythonScriptVersion));
    
    //adjust to bytes from MB
    g_cfg.ReallocSize = (int64)reallocSizeMB*1024*1024;
    g_cfg.MemoryLimit = (uint64)memmoryLimitMB*1024*1024;
    g_cfg.SocketReadBufferSize = (uint32)readBufferSize*1024;
    g_cfg.SocketWriteBufferSize = (uint32)writeBufferSize*1024;
    g_cfg.IndexBlockCacheSize = (uint64)indexBlockCacheSize*1024*1024;
    g_cfg.LRUCacheMemReserve = (uint64)LRUCacheMemReserve*1024*1024;
    
    //Set log level
    Log.SetLogLevel(logLevel);
    
    //
    Log.Notice(__FUNCTION__, "Config loaded with status: %s", status == 0 ? "Ok" : "Error");
}
