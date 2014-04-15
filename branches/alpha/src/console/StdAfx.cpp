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
uint64  g_MemoryLimit               = 512;
uint64  g_HardMemoryLimit           = 1024;
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
int		g_LRUCacheMemReserve		= 100000;
int		g_FreeSpaceMemReserve		= 100000;
int     g_BlockManagerMemReserve    = 100000;
int     g_StorageBlockMemReserve    = 100000;
int     g_DiskWriterBlockMemReserve = 100000;
int     g_RecordIndexMemReserve     = 100000;
int     g_NetworkThreads            = 0;

#ifdef CUSTOM_CHECKS
uint32  g_NumRecordsInBlock	   = 0;
#endif

//CRC
CRC_32 *g_CRC32;
