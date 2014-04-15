//
//  StdAfx.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/20/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

//cfg
int     g_RecordLimit               = 1000;
int     g_DiskFlushCoef             = 100;
string  g_sStoragePath              = "";
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
uint64  g_FreeSpaceMemReserve		= 4*1024*1024;
uint64  g_StorageBlockMemReserve    = 128*1024*1024;
uint64  g_RecordIndexMemReserve     = 32*1024*1024;
int     g_NetworkThreads            = 1;
int     g_MaxParallelTasks          = 4;
int     g_MaxTasksInQueue           = 100;
int     g_WebSocketPort             = 8888;
uint64  g_IndexBlockCacheSize       = 16*1024*1024;
int     g_RecordSizeForCompression  = 200;

//socket ID gen
volatile long g_SocketID            = 0;

//start time
time_t g_StartTime                  = 0;

//for statistics
volatile long g_NumOfReadsFromDisk  = 0;
volatile long g_NumOfReadsFromCache = 0;
volatile long g_NumOfWritesToDisk   = 0;
tbb::atomic<size_t> g_ReceivedBytes;
tbb::atomic<size_t> g_SendedBytes;
tbb::atomic<size_t> g_NumOfFreads;
tbb::atomic<size_t> g_NumOfFwrites;
tbb::atomic<size_t> g_NumOfFseeks;
tbb::atomic<size_t> g_NumOfRecordCompressions;
tbb::atomic<size_t> g_NumOfRecordDecompressions;

#ifdef CUSTOM_CHECKS
uint32  g_NumRecordsInBlock	   = 0;
#endif

//CRC
CRC_32 *g_CRC32;

//TLS storage file pointer
THREAD_LOCAL_STORAGE FILE *g_pStorage_FilePointer;

#ifdef USE_INTEL_SCALABLE_ALLOCATORS
//operator override
void *operator new(size_t size) throw(std::bad_alloc)
{
    void *ptr;
    ptr = scalable_malloc(size);
    if(ptr)
        return ptr;
    else
        throw std::bad_alloc();
}

void *operator new[](size_t size) throw(std::bad_alloc)
{
    return operator new(size);
}

void *operator new(size_t size, const std::nothrow_t&) throw()
{
    void *ptr;
    ptr = scalable_malloc(size);
    return ptr;
}

void *operator new[](size_t size, const std::nothrow_t&) throw()
{
    return operator new(size, std::nothrow);
}

void operator delete(void *ptr) throw()
{
    scalable_free(ptr);
}

void operator delete[](void *ptr) throw()
{
    operator delete(ptr);
}

void operator delete(void *ptr, const std::nothrow_t&) throw()
{
    scalable_free(ptr);
}

void operator delete[](void *ptr, const std::nothrow_t&) throw()
{
    operator delete(ptr, std::nothrow);
}

#endif










