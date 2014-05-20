//
//  StdAfx.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/20/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#ifndef TransDB_StdAfx_h
#define TransDB_StdAfx_h

//python
#if defined(_MSC_VER) && _MSC_VER >= 1800
    #define HAVE_ROUND 1
#endif
#include <Python.h>

#include "svnversion.h"
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
#include "../shared/Containers/Vector.h"
#include "../shared/Containers/HashMap.h"
#include "../shared/IO/IO.h"
#include "../shared/CommonFunctions.h"

//Intel TBB
#define TBB_PREVIEW_MEMORY_POOL 1
#include "tbb/memory_pool.h"

#include "tbb/concurrent_queue.h"
#include "tbb/concurrent_hash_map.h"
#include "tbb/concurrent_vector.h"
#include "tbb/parallel_sort.h"
#include "tbb/concurrent_unordered_set.h"

//cfg defines
#include "CfgDefines.h"

//win32 service support
#include "ServiceWin32.h"

//stat generator support
#include "StatGenerator.h"

//block
#include "Block.h"
#include "BlockManager.h"

//
#include "ConfigWatcher.h"
#include "Launcher.h"
#include "ClientSocket.h"
#include "ClientSocketWorkerTask.h"
#include "ClientSocketWorker.h"
#include "ClientSocketHolder.h"
#include "IndexBlock.h"
#include "LRUCache.h"
#include "Storage.h"
#include "DiskWriter.h"
#include "MemoryWatcher.h"

//python interface
#include "PythonInterface.h"

#endif
