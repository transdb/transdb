//
//  StdAfx.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/20/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#ifndef TransDB_StdAfx_h
#define TransDB_StdAfx_h

//constants
//#define	CUSTOM_CHECKS

#include "../shared/Defines.h"
#include "../shared/Enums.h"
#include "../shared/Threading/Threading.h"
#include "../shared/Network/Network.h"
#include "../shared/Packets/Packets.h"
#include "../shared/CallBack.h"
#include "../shared/Config/Config.h"
#include "../shared/crc32.h"
#include "../shared/Memory/MemoryAllocator.h"
#include "../shared/Packets/GeewaPacket.h"
#include "../shared/CrashHandler.h"

//zlib
#include "../zlib/zlib.h"

//Intel TBB
#define TBB_PREVIEW_MEMORY_POOL 1
#include "tbb/memory_pool.h"

#include "tbb/concurrent_queue.h"
#include "tbb/concurrent_hash_map.h"
#include "tbb/atomic.h"
#include "tbb/mutex.h"
#include "tbb/recursive_mutex.h"

#ifdef USE_INTEL_SCALABLE_ALLOCATORS
    #include "tbb/scalable_allocator.h"
#endif

//win32 service support
#include "ServiceWin32.h"

//web service support
#include "WebService.h"

//block
#include "Block.h"
#include "BlockManager.h"

//
#include "Common.h"
#include "ConfigWatcher.h"
#include "Launcher.h"
#include "ClientSocketWorkerTask.h"
#include "ClientSocketWorker.h"
#include "ClientSocket.h"
#include "ClientSocketHolder.h"
#include "IndexBlock.h"
#include "LRUCache.h"
#include "Storage.h"
#include "DiskWriter.h"
#include "MemoryWatcher.h"
#include "StringCompressor.h"


//cfg
extern int g_RecordLimit;
extern int g_DiskFlushCoef;
extern string g_sStoragePath;
extern uint64 g_MemoryLimit;
extern int g_MemoryFlushCoef;
extern uint64 g_ReallocSize;
extern string g_ListenHost;
extern int g_ListenPort;
extern uint32 g_SocketReadBufferSize;
extern uint32 g_SocketWriteBufferSize;
extern bool g_StartupCrc32Check;
extern string g_DataFileName;
extern string g_ActivityID;
extern int g_PingTimeout;
extern int g_PingSendInterval;
extern uint64 g_LRUCacheMemReserve;
extern uint64 g_FreeSpaceMemReserve;
extern uint64 g_StorageBlockMemReserve;
extern uint64 g_RecordIndexMemReserve;
extern int g_MemoryPoolReallocSize;
extern int g_NetworkThreads;
extern int g_MaxParallelTasks;
extern int g_MaxTasksInQueue;
extern int g_WebSocketPort;
extern uint64 g_IndexBlockCacheSize;

//socket ID gen
extern volatile long g_SocketID;

//start time
extern time_t g_StartTime;

//for statistics
extern volatile long g_NumOfReadsFromDisk;
extern volatile long g_NumOfReadsFromCache;
extern volatile long g_NumOfWritesToDisk;
extern tbb::atomic<size_t>  g_ReceivedBytes;
extern tbb::atomic<size_t>  g_SendedBytes;
extern tbb::atomic<size_t>  g_NumOfFreads;
extern tbb::atomic<size_t>  g_NumOfFwrites;
extern tbb::atomic<size_t>  g_NumOfFseeks;
extern tbb::atomic<size_t>  g_NumOfRecordCompressions;
extern tbb::atomic<size_t>  g_NumOfRecordDecompressions;

//zlib
#define GZIP_ENCODING				16
#define ZLIB_CHUNK                  16384
extern int g_GzipCompressionLevel;
extern int g_ZlibBufferSize;
extern int g_DataSizeForCompression;
extern int g_RecordSizeForCompression;

#ifdef CUSTOM_CHECKS
extern uint32 g_NumRecordsInBlock;
#endif

//CRC
extern CRC_32 *g_CRC32;

//TLS storage file pointer
extern THREAD_LOCAL_STORAGE FILE *g_pStorage_FilePointer;

#ifdef WIN32
static void trans_func(unsigned int u, EXCEPTION_POINTERS* pExp)
{
	HandleCrash(pExp);
}
#endif

class IO
{
public:
    static int64 ftell(FILE *stream)
    {
#ifdef WIN32
        return _ftelli64(stream);
#else
        return std::ftell(stream);
#endif
    }
    
    static int fseek(FILE *stream, int64 offset, int origin)
    {
        ++g_NumOfFseeks;
#ifdef WIN32
        return _fseeki64(stream, offset, origin);
#else
        return std::fseek(stream, offset, origin);
#endif
    }
    
    static size_t fwrite(const void *buffer, size_t size, size_t count, FILE *stream)
    {
        ++g_NumOfFwrites;
        return std::fwrite(buffer, size, count, stream);
    }
    
    static size_t fread(void *buffer, size_t size, size_t count, FILE *stream)
    {
        ++g_NumOfFreads;
        return std::fread(buffer, size, count, stream);
    }
    
    static int fflush(FILE *stream)
    {
        return std::fflush(stream);
    }
    
    static int fclose(FILE *stream)
    {
        return std::fclose(stream);
    }
};

static void LoadConfig()
{
    int status;
    int memmoryLimitMB          = (int)g_MemoryLimit;
    int reallocSizeMB           = (int)g_ReallocSize;
    int readBufferSize          = (int)g_SocketReadBufferSize;
    int writeBufferSize         = (int)g_SocketWriteBufferSize;
    int logLevel                = 3;
    int indexBlockCacheSize     = (int)g_IndexBlockCacheSize;
    int LRUCacheMemReserve      = (int)g_LRUCacheMemReserve;
    int freeSpaceMemReserve     = (int)g_FreeSpaceMemReserve;
    int storageBlockMemReserve  = (int)g_StorageBlockMemReserve;
    int recordIndexMemReserve   = (int)g_RecordIndexMemReserve;
    
    //if status will 1 there is missing config
    status = 0;
    
    //load values from config
    status |= !g_rConfig.MainConfig.GetString("Activity", "ID", &g_ActivityID);
    status |= !g_rConfig.MainConfig.GetString("Storage", "StoragePath", &g_sStoragePath);
    status |= !g_rConfig.MainConfig.GetInt("Storage", "RecordLimit", &g_RecordLimit);
    status |= !g_rConfig.MainConfig.GetInt("Storage", "DiskFlushCoef", &g_DiskFlushCoef);
    status |= !g_rConfig.MainConfig.GetInt("Storage", "ReallocSize", &reallocSizeMB);
	status |= !g_rConfig.MainConfig.GetBool("Storage", "StartupCrc32Check", &g_StartupCrc32Check);
	status |= !g_rConfig.MainConfig.GetString("Storage", "DataFileName", &g_DataFileName);
    status |= !g_rConfig.MainConfig.GetInt("Memory", "MemoryLimit", &memmoryLimitMB);
    status |= !g_rConfig.MainConfig.GetInt("Memory", "MemoryFlushCoef", &g_MemoryFlushCoef);
    status |= !g_rConfig.MainConfig.GetInt("Memory", "LRUCacheMemReserve", &LRUCacheMemReserve);
    status |= !g_rConfig.MainConfig.GetInt("Memory", "FreeSpaceMemReserve", &freeSpaceMemReserve);
    status |= !g_rConfig.MainConfig.GetInt("Memory", "StorageBlockMemReserve", &storageBlockMemReserve);
    status |= !g_rConfig.MainConfig.GetInt("Memory", "RecordIndexMemReserve", &recordIndexMemReserve);
    status |= !g_rConfig.MainConfig.GetInt("Memory", "IndexBlockCache", &indexBlockCacheSize);
    status |= !g_rConfig.MainConfig.GetString("PublicSocket", "Host", &g_ListenHost);
    status |= !g_rConfig.MainConfig.GetInt("PublicSocket", "Port", &g_ListenPort);
    status |= !g_rConfig.MainConfig.GetInt("PublicSocket", "ReadBufferSize", &readBufferSize);
    status |= !g_rConfig.MainConfig.GetInt("PublicSocket", "WriteBufferSize", &writeBufferSize);
    status |= !g_rConfig.MainConfig.GetInt("PublicSocket", "PingTimeout", &g_PingTimeout);
    status |= !g_rConfig.MainConfig.GetInt("PublicSocket", "PingSendInterval", &g_PingSendInterval);
    status |= !g_rConfig.MainConfig.GetInt("PublicSocket", "NetworkThreads", &g_NetworkThreads);
    status |= !g_rConfig.MainConfig.GetInt("PublicSocket", "MaxParallelTasks", &g_MaxParallelTasks);
    status |= !g_rConfig.MainConfig.GetInt("PublicSocket", "MaxTasksInQueue", &g_MaxTasksInQueue);
    status |= !g_rConfig.MainConfig.GetInt("PublicSocket", "WebSocketPort", &g_WebSocketPort);
    status |= !g_rConfig.MainConfig.GetInt("Log", "Level", &logLevel);
	status |= !g_rConfig.MainConfig.GetInt("Compression", "Level", &g_GzipCompressionLevel);
	status |= !g_rConfig.MainConfig.GetInt("Compression", "BufferSize", &g_ZlibBufferSize);
	status |= !g_rConfig.MainConfig.GetInt("Compression", "DataSizeForCompression", &g_DataSizeForCompression);
    status |= !g_rConfig.MainConfig.GetInt("Compression", "RecordSizeForCompression", &g_RecordSizeForCompression);
    
    //adjust to bytes from MB
    g_ReallocSize = (uint64)reallocSizeMB*1024*1024;
    g_MemoryLimit = (uint64)memmoryLimitMB*1024*1024;
    g_SocketReadBufferSize = (uint32)readBufferSize*1024*1024;
    g_SocketWriteBufferSize = (uint32)writeBufferSize*1024*1024;
    g_IndexBlockCacheSize = (uint64)indexBlockCacheSize*1024*1024;
    g_LRUCacheMemReserve = (uint64)LRUCacheMemReserve*1024*1024;
    g_FreeSpaceMemReserve = (uint64)freeSpaceMemReserve*1024*1024;
    g_StorageBlockMemReserve = (uint64)storageBlockMemReserve*1024*1024;
    g_RecordIndexMemReserve = (uint64)recordIndexMemReserve*1024*1024;
    
    //Set log level
    Log.SetLogLevel(logLevel);
    
    //
    Log.Notice(__FUNCTION__, "Config loaded with status: %s", status == 0 ? "Ok" : "Error");
}

static void StartSharedLib()
{       
    //network subsystem
    Log.Notice("Network", "Starting subsystem...");
    new SocketMgr;
    new SocketGarbageCollector;
    
    //start thread pool
    ThreadPool.Startup();
    
    //start network mgr
    sSocketMgr.SpawnWorkerThreads(g_NetworkThreads);
    
	//init CRC32
    g_CRC32 = new CRC_32();
}

#endif
