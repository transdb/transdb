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

//zlib
#include "../zlib/zlib.h"

//win32 service support
#include "ServiceWin32.h"

//block
#include "Block.h"
#include "BlockManager.h"

//
#include "Common.h"
#include "ConfigWatcher.h"
#include "Launcher.h"
#include "ClientSocket.h"
#include "ClientSocketHolder.h"
#include "IndexBlock.h"
#include "LRUCache.h"
#include "Storage.h"
#include "DiskWriter.h"


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
#ifdef WIN32
        return _fseeki64(stream, offset, origin);
#else
        return std::fseek(stream, offset, origin);
#endif
    }
    
    static size_t fwrite(const void *buffer, size_t size, size_t count, FILE *stream)
    {
        return std::fwrite(buffer, size, count, stream);
    }
    
    static size_t fread(void *buffer, size_t size, size_t count, FILE *stream)
    {
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

//cfg
extern int g_RecordLimit;
extern int g_DiskFlushCoef;
extern string g_sStoragePath;
extern uint64 g_MemoryLimit;
extern uint64 g_HardMemoryLimit;
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
extern int g_LRUCacheMemReserve;
extern int g_FreeSpaceMemReserve;
extern int g_MemoryPoolReallocSize;
extern int g_BlockManagerMemReserve;
extern int g_StorageBlockMemReserve;
extern int g_DiskWriterBlockMemReserve;
extern int g_RecordIndexMemReserve;
extern int g_NetworkThreads;

//zlib
#define GZIP_ENCODING				16
extern int g_GzipCompressionLevel;
extern int g_ZlibBufferSize;
extern int g_DataSizeForCompression;

#ifdef CUSTOM_CHECKS
extern uint32 g_NumRecordsInBlock;
#endif

//CRC
extern CRC_32 *g_CRC32;

static void LoadConfig()
{
    int memmoryLimitMB;
    int hardMemoryLimitMB;
    int reallocSizeMB;
    int readBufferSize;
    int writeBufferSize;
    int logLevel;
    //load values from config
    assert(g_rConfig.MainConfig.GetString("Activity", "ID", &g_ActivityID));    
    assert(g_rConfig.MainConfig.GetString("Storage", "StoragePath", &g_sStoragePath));
    assert(g_rConfig.MainConfig.GetInt("Storage", "RecordLimit", &g_RecordLimit));
    assert(g_rConfig.MainConfig.GetInt("Storage", "DiskFlushCoef", &g_DiskFlushCoef));
    assert(g_rConfig.MainConfig.GetInt("Storage", "ReallocSize", &reallocSizeMB));
	assert(g_rConfig.MainConfig.GetBool("Storage", "StartupCrc32Check", &g_StartupCrc32Check));
	assert(g_rConfig.MainConfig.GetString("Storage", "DataFileName", &g_DataFileName));
    assert(g_rConfig.MainConfig.GetInt("Memory", "MemoryLimit", &memmoryLimitMB));
    assert(g_rConfig.MainConfig.GetInt("Memory", "HardMemoryLimit", &hardMemoryLimitMB));
    assert(g_rConfig.MainConfig.GetInt("Memory", "MemoryFlushCoef", &g_MemoryFlushCoef));
    assert(g_rConfig.MainConfig.GetInt("Memory", "LRUCacheMemReserve", &g_LRUCacheMemReserve));
    assert(g_rConfig.MainConfig.GetInt("Memory", "FreeSpaceMemReserve", &g_FreeSpaceMemReserve));
    assert(g_rConfig.MainConfig.GetInt("Memory", "BlockManagerMemReserve", &g_BlockManagerMemReserve));
    assert(g_rConfig.MainConfig.GetInt("Memory", "StorageBlockMemReserve", &g_StorageBlockMemReserve));
    assert(g_rConfig.MainConfig.GetInt("Memory", "DiskWriterBlockMemReserve", &g_DiskWriterBlockMemReserve));
    assert(g_rConfig.MainConfig.GetInt("Memory", "RecordIndexMemReserve", &g_RecordIndexMemReserve));
    assert(g_rConfig.MainConfig.GetString("PublicSocket", "Host", &g_ListenHost));
    assert(g_rConfig.MainConfig.GetInt("PublicSocket", "Port", &g_ListenPort));
    assert(g_rConfig.MainConfig.GetInt("PublicSocket", "ReadBufferSize", &readBufferSize));
    assert(g_rConfig.MainConfig.GetInt("PublicSocket", "WriteBufferSize", &writeBufferSize));
    assert(g_rConfig.MainConfig.GetInt("PublicSocket", "PingTimeout", &g_PingTimeout));
    assert(g_rConfig.MainConfig.GetInt("PublicSocket", "PingSendInterval", &g_PingSendInterval));
    assert(g_rConfig.MainConfig.GetInt("PublicSocket", "NetworkThreads", &g_NetworkThreads));
    assert(g_rConfig.MainConfig.GetInt("Log", "Level", &logLevel));
	assert(g_rConfig.MainConfig.GetInt("Compression", "Level", &g_GzipCompressionLevel));
	assert(g_rConfig.MainConfig.GetInt("Compression", "BufferSize", &g_ZlibBufferSize));
	assert(g_rConfig.MainConfig.GetInt("Compression", "DataSizeForCompression", &g_DataSizeForCompression));
    
    //adjust to bytes from MB
    g_ReallocSize = (uint64)reallocSizeMB*1024*1024;
    g_MemoryLimit = (uint64)memmoryLimitMB*1024*1024;
    g_HardMemoryLimit = (uint64)hardMemoryLimitMB*1024*1024;
    g_SocketReadBufferSize = (uint32)readBufferSize*1024*1024;
    g_SocketWriteBufferSize = (uint32)writeBufferSize*1024*1024;
    
    //Set log level
    Log.SetLogLevel(logLevel);
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

static void StopSharedLib()
{        
    //shutdown network sub system
	Log.Notice("Network", "Shutting down network subsystem.");
#ifdef WIN32
	sSocketMgr.ShutdownThreads();
#endif
	sSocketMgr.CloseAll();
    
    //shutdown threadpool
    ThreadPool.Shutdown();

	//delete
	Log.Notice("Network", "Deleting Network Subsystem...");
	delete SocketMgr::getSingletonPtr();
	delete SocketGarbageCollector::getSingletonPtr();

	//delete CRC32
	delete g_CRC32;
}


#endif
