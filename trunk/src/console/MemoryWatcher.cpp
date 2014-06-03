//
//  MemoryWatcher.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 14.01.13.
//  Copyright (c) 2013 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

MemoryWatcher::MemoryWatcher(Storage &rStorage) : m_rStorage(rStorage), m_memoryCheckerCount(0), m_memoryPoolCheckerCount(0)
{
    
}

bool MemoryWatcher::run()
{
    CommonFunctions::SetThreadName("MemoryWatcher thread");
    
    try
    {
#ifdef WIN32
        _set_se_translator(trans_func);
#endif
    
        while(m_threadRunning)
        {
            //recycle memory pools
            if(!(++m_memoryPoolCheckerCount % g_MemoryPoolsRecycleTimer))
            {
                _S_FixedPool_Recycle("BlockMemPool", m_rStorage.m_rBlockMemPoolLock, m_rStorage.m_rBlockMemPool);
                _S_FixedPool_Recycle("RecordIndexMemPool", m_rStorage.m_rRecordIndexMemPoolLock, m_rStorage.m_rRecordIndexMemPool);
                _S_FixedPool_Recycle("BlockManagerMemPool", m_rStorage.m_rBlockManagerMemPoolLock, m_rStorage.m_rBlockManagerMemPool);
            }
            
            Wait(1000);
        }
    }
    catch(...)
    {
        Log.Error(__FUNCTION__, "Fatal error stopping server.");
        g_pClientSocketWorker->SetException(true);
        g_stopEvent = true;
    }
    
    return true;
}