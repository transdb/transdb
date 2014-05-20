//
//  main.h
//  TransDBTester
//
//  Created by Miroslav Kudrnac on 8/30/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#ifndef __TransDBTester__main__
#define __TransDBTester__main__

#include "../shared/Defines.h"
#include "../shared/Enums.h"
#include "../shared/Threading/Threading.h"
#include "../shared/Network/Network.h"
#include "../shared/CallBack.h"
#include "../shared/Packets/Packets.h"
#include "../shared/zlib/zlib.h"

#include "ClientSocket.h"
#include "Launcher.h"
#include "Testing.h"

//extern bool g_HasResponse;

static bool StartSharedLib()
{
    UNIXTIME = time(NULL);
    localtime(&UNIXTIME, &g_localTime);
    
    //network subsystem
    Log.Notice("Network", "Starting subsystem...");
    new SocketMgr;
    new SocketGarbageCollector;
    
    //start thread pool
    ThreadPool.Startup();
    
    //start network mgr
    sSocketMgr.SpawnWorkerThreads();
    
    return true;
}

static bool StopSharedLib()
{
    //shutdown network sub system
    sSocketMgr.CloseAll();
    
    //shutdown threadpool
    ThreadPool.Shutdown();
    
    return true;
}

#endif /* defined(__TransDBTester__main__) */
