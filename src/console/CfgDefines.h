//
//  CfgDefines.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 20.01.14.
//  Copyright (c) 2014 Miroslav Kudrnac. All rights reserved.
//

#ifndef TransDB_CfgDefines_h
#define TransDB_CfgDefines_h

//cfg
extern bool g_EnableRecordLimit;
extern int g_RecordLimit;
extern int g_DiskFlushCoef;
extern std::string g_DataFilePath;
extern std::string g_IndexFilePath;
extern uint64 g_MemoryLimit;
extern int64 g_ReallocSize;
extern std::string g_ListenHost;
extern int g_ListenPort;
extern uint32 g_SocketReadBufferSize;
extern uint32 g_SocketWriteBufferSize;
extern bool g_StartupCrc32Check;
extern std::string g_DataFileName;
extern std::string g_ActivityID;
extern int g_PingTimeout;
extern int g_PingSendInterval;
extern uint64 g_LRUCacheMemReserve;
extern int g_MaxParallelTasks;
extern int g_MaxParallelReadTasks;
extern int g_MaxTasksInQueue;
extern int g_MaxReadTasksInQueue;
extern int g_WebSocketPort;
extern uint64 g_IndexBlockCacheSize;
extern int g_DefragAfterRecordDelete;
extern int g_FreeSpaceDefrag;

//socket ID gen
extern std::atomic<uint64> g_SocketID;

//start time
extern time_t g_StartTime;

//for statistics
extern std::atomic<uint64> g_NumOfReadsFromDisk;
extern std::atomic<uint64> g_NumOfReadsFromCache;
extern std::atomic<uint64> g_NumOfWritesToDisk;
extern std::atomic<size_t> g_ReceivedBytes;
extern std::atomic<size_t> g_SendedBytes;
extern std::atomic<size_t> g_NumOfRecordCompressions;
extern std::atomic<size_t> g_NumOfRecordDecompressions;
extern std::atomic<size_t> g_NumOfRecordDeframentations;
extern std::atomic<uint64> g_AvgDiskReadTime;
extern std::atomic<uint64> g_AvgDiskWriteTime;

//zlib
extern int g_GzipCompressionLevel;
extern int g_ZlibBufferSize;
extern int g_DataSizeForCompression;
extern int g_RecordSizeForCompression;

//CRC
extern CRC_32 *g_CRC32;

//version
extern const char *g_pCompiledVersion;

//python
extern bool g_PythonEnable;
extern std::string g_PythonHome;
extern std::string g_PythonScriptsFolderPath;
extern std::string g_PythonModuleName;
extern std::string g_PythonClassName;
extern std::string g_PythonRunableMethod;
extern std::string g_PythonShutdownMethod;
extern std::string g_PythonScriptVersion;

//malloc alignment for disk IO
extern int g_DataFileMallocAlignment;
extern int g_IndexFileMallocAlignment;

//force startrup
/** 0 - abort server startup when error
    1 - force startup when freespace error
*/

typedef enum E_FORCE_STARTUP_ACTION
{
    eFSA_Abort                          = 0,
    eFSA_ContinueIfFreeSpaceCorrupted   = 1
} E_FSA;

extern E_FSA g_ForceStartup;

//Allocator
template <class K, class V>
class ScalableHashMapNodeAllocator
{
public:
    HashNode<K, V> *allocate(const K &key, const V &value)
    {
        void *pMem = scalable_malloc(sizeof(HashNode<K, V>));
        return new(pMem) HashNode<K, V>(key, value);
    }
    
    void deallocate(HashNode<K, V> *p)
    {
        p->~HashNode<K, V>();
        scalable_free(p);
    }
};

#ifdef WIN32
static void trans_func(unsigned int u, EXCEPTION_POINTERS* pExp)
{
    PrintCrashInformation(pExp);
	HandleCrash(pExp);
}
#endif

//load config
void LoadConfig();


#endif
