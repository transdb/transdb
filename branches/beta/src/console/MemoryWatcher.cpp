//
//  MemoryWatcher.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 14.01.13.
//  Copyright (c) 2013 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

MemoryWatcher::MemoryWatcher(Storage &rStorage) : m_memoryCheckerCount(0), m_rStorage(rStorage)
{
    
}

bool MemoryWatcher::run()
{
    SetThreadName("MemoryWatcher thread");
    
    try
    {
#ifdef WIN32
        _set_se_translator(trans_func);
#endif
    
        while(m_threadRunning)
        {
            //check memory
            if(!(++m_memoryCheckerCount % g_MemoryFlushCoef))
            {
                m_rStorage.CheckMemory();
            }
            
            Wait(100);
        }
    }
    catch(...)
    {
        Log.Error(__FUNCTION__, "Fatal error stopping server.");
        g_pClientSocketWorker->SetException(true);
        Sync_Add(&g_stopEvent);
    }
    
    return true;
}