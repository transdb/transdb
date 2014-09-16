//
//  CfgDefines.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 20.01.14.
//  Copyright (c) 2014 Miroslav Kudrnac. All rights reserved.
//

#ifndef TransDB_CfgDefines_h
#define TransDB_CfgDefines_h

typedef struct _ConfigDefines
{
    //ID
    char    ActivityID[32];
    
    //Storage
    char    DataFilePath[MAX_PATH];
    char    IndexFilePath[MAX_PATH];
    char    DataFileName[32];
    bool    EnableRecordLimit;
    int     RecordLimit;
    int     DiskFlushCoef;
    int64   ReallocSize;
    bool    StartupCrc32Check;
    int     DefragAfterRecordDelete;
    int     FreeSpaceDefrag;

    //Memory
    uint64  MemoryLimit;
    uint64  LRUCacheMemReserve;
    uint64  IndexBlockCacheSize;
    
    //PublicSocket
    char    ListenHost[32];
    int     ListenPort;
    int     WebSocketPort;
    uint32  SocketReadBufferSize;
    uint32  SocketWriteBufferSize;
    int     PingTimeout;
    int     PingSendInterval;
    int     MaxParallelTasks;
    int     MaxParallelReadTasks;
    int     MaxTasksInQueue;
    int     MaxReadTasksInQueue;

    //python
    bool    PythonEnable;
    char    PythonHome[MAX_PATH];
    char    PythonScriptsFolderPath[MAX_PATH];
    char    PythonModuleName[32];
    char    PythonClassName[32];
    char    PythonRunableMethod[32];
    char    PythonShutdownMethod[32];
    char    PythonScriptVersion[32];
    
    //zlib
    int     GzipCompressionLevel;
    int     ZlibBufferSize;
    int     DataSizeForCompression;
    int     RecordSizeForCompression;
}ConfigDefines;

typedef struct _Statistics
{
    //start time
    time_t  StartTime;
    
    //for statistics
    uint64  NumOfReadsFromDisk;
    uint64  NumOfReadsFromCache;
    uint64  NumOfWritesToDisk;
    uint64  ReceivedBytes;
    uint64  SendedBytes;
    uint64  NumOfRecordCompressions;
    uint64  NumOfRecordDecompressions;
    uint64  NumOfRecordDeframentations;
    uint64  AvgDiskReadTime;
    uint64  AvgDiskWriteTime;
}Statistics;

//config holder
extern ConfigDefines    g_cfg;
//stats holder
extern Statistics       g_stats;

/** Called on startup
 */
void init_stats_default_values(Statistics *self);

/** load and parse config
 */
void load_and_parse_config_values();

//version
extern const char *g_pCompiledVersion;

//malloc alignment for disk IO
extern int g_DataFileMallocAlignment;
extern int g_IndexFileMallocAlignment;

//force startup
/** 0 - abort server startup when error
    1 - force startup when freespace error
*/
typedef enum E_FORCE_STARTUP_ACTION
{
    eFSA_Abort                          = 0,
    eFSA_ContinueIfFreeSpaceCorrupted   = 1
} E_FSA;

extern E_FSA g_ForceStartup;

#endif
