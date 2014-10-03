//
//  ClientSocketWorker.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 12.11.12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

ClientSocketWorker	*g_pClientSocketWorker;

ClientSocketWorker *ClientSocketWorker::create()
{
    ClientSocketWorker *pClientSocketWorker = new ClientSocketWorker();
    //init storage
    if(pClientSocketWorker->InitStorage() == false)
    {
        delete pClientSocketWorker;
        return NULL;
    }
    
	//create config file watcher
    if(pClientSocketWorker->InitConfigWatcher() == false)
    {
        delete pClientSocketWorker;
        return NULL;
    }
    
    //init python interface
    if(pClientSocketWorker->InitPythonInterface() == false)
    {
        delete pClientSocketWorker;
        return NULL;
    }
    
    //init worker threads
	if(pClientSocketWorker->InitWorkerThreads() == false)
    {
        delete pClientSocketWorker;
        return NULL;
    }
    return pClientSocketWorker;
}

ClientSocketWorker::ClientSocketWorker() : m_pStorage(NULL), m_pPythonInterface(NULL), m_pConfigWatcher(NULL), m_exception(false)
{
    //set write queue limit
    m_pTaskDataQueue[eTQT_Write] = new TaskDataQueue(g_cfg.MaxTasksInQueue);
    //set read queue limit
    m_pTaskDataQueue[eTQT_Read] = new TaskDataQueue(g_cfg.MaxReadTasksInQueue);
}

ClientSocketWorker::~ClientSocketWorker()
{
    delete m_pTaskDataQueue[eTQT_Write];
    delete m_pTaskDataQueue[eTQT_Read];
}

//init storage, load index and data file
bool ClientSocketWorker::InitStorage()
{
    //alocate storage
	Log.Notice(__FUNCTION__, "Loading storage...");
    m_pStorage = Storage::create(g_cfg.DataFileName);
    if(m_pStorage == NULL)
    {
        Log.Error(__FUNCTION__, "Loading storage... failed");
        return false;
    }
    ThreadPool.ExecuteTask(m_pStorage);
    Log.Notice(__FUNCTION__, "Loading storage... done");
    return true;
}

//starts config watcher worker thread
bool ClientSocketWorker::InitConfigWatcher()
{
    Log.Notice(__FUNCTION__, "Starting config watcher...");
    m_pConfigWatcher = new ConfigWatcher();
    ThreadPool.ExecuteTask(m_pConfigWatcher);
    Log.Notice(__FUNCTION__, "Starting config watcher... done");
    return true;
}

//starts python interface if enebled
bool ClientSocketWorker::InitPythonInterface()
{
    m_pPythonInterface = new PythonInterface(*m_pConfigWatcher);
    if(g_cfg.PythonEnable)
    {
        Log.Notice(__FUNCTION__, "Starting python interface...");
        ThreadPool.ExecuteTask(m_pPythonInterface);
        Log.Notice(__FUNCTION__, "Starting python interface... done");
    }
    return true;
}

//starts worker threads
bool ClientSocketWorker::InitWorkerThreads()
{
    //start worker threads
    Log.Notice(__FUNCTION__, "Spawning writer threads...");
	for(int i = 0;i < g_cfg.MaxParallelTasks;++i)
	{
		ThreadPool.ExecuteTask(new ClientSocketWorkerTask(*m_pTaskDataQueue[eTQT_Write], *this, *m_pStorage, *m_pPythonInterface, *m_pConfigWatcher));
	}
    Log.Notice(__FUNCTION__, "Spawning writer threads... done");
    
    //start reader worker threads
    Log.Notice(__FUNCTION__, "Spawning reader threads...");
    for(int i = 0;i < g_cfg.MaxParallelReadTasks;++i)
    {
        ThreadPool.ExecuteTask(new ClientSocketWorkerTask(*m_pTaskDataQueue[eTQT_Read], *this, *m_pStorage, *m_pPythonInterface, *m_pConfigWatcher));
    }
    Log.Notice(__FUNCTION__, "Spawning reader threads... done");
    return true;
}

void ClientSocketWorker::DestroyWorkerThreads()
{
//    TaskDataQueue::size_type waitSize;
//    
//    //wait for task(s) finish only if there is no exception
//    if(!m_exception)
//    {
//        //! Return number of pushes minus number of pops.
//        /** Note that the result can be negative if there are pops waiting for the
//         corresponding pushes.  The result can also exceed capacity() if there
//         are push operations in flight. */
//        waitSize = g_cfg.MaxParallelTasks * (-1);
//        
//        //process all tasks before shutdown
//        while(m_rTaskDataQueue[eTQT_Write].size() != waitSize)
//        {
//            Log.Notice(__FUNCTION__, "Waiting for: %d write tasks to finish.", (int32)m_rTaskDataQueue[eTQT_Write].size());
//            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
//        }
//        
//        //read tasks
//        waitSize = g_cfg.MaxParallelReadTasks * (-1);
//        while(m_rTaskDataQueue[eTQT_Read].size() !=  waitSize)
//        {
//            Log.Notice(__FUNCTION__, "Waiting for: %d read tasks to finish.", (int32)m_rTaskDataQueue[eTQT_Read].size());
//            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
//        }
//        
//        Log.Notice(__FUNCTION__, "Tasks finished. Calling abort.");
//    }
//
	//wake up threads
	m_pTaskDataQueue[eTQT_Read]->abort();
	m_pTaskDataQueue[eTQT_Write]->abort();
}

//write pending write to disk and destroy storage
void ClientSocketWorker::DestroyStorage()
{
	delete m_pStorage;
    m_pStorage = NULL;
}

void ClientSocketWorker::QueuePacket(uint16 opcode, uint64 socketID, bbuff *pData, E_TQT eQueueType)
{
    //create struct with task data
    ClientSocketTaskData rTaskData;
    rTaskData.m_opcode = opcode;
    rTaskData.m_socketID = socketID;
    bbuff_create(rTaskData.m_pData);
    bbuff_append(rTaskData.m_pData, pData->storage, pData->size);
    
    //split by type of task
    m_pTaskDataQueue[eQueueType]->put(rTaskData);
}

size_t ClientSocketWorker::GetQueueSize()
{
    return m_pTaskDataQueue[eTQT_Write]->qsize();
}

size_t ClientSocketWorker::GetReadQueueSize()
{
    return m_pTaskDataQueue[eTQT_Read]->qsize();
}
































