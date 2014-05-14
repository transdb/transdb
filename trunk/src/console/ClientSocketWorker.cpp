//
//  ClientSocketWorker.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 12.11.12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

ClientSocketWorker	*g_pClientSocketWorker;

ClientSocketWorker::ClientSocketWorker() : m_pStorage(NULL), m_pFixedPool(NULL), m_pFixedPoolMem(NULL)
{
    m_exception = false;
}

ClientSocketWorker::~ClientSocketWorker()
{
	//destroy pool
	delete m_pFixedPool;
	m_pFixedPool = NULL;
	::free(m_pFixedPoolMem);
	m_pFixedPoolMem = NULL;
}

//init storage, load index and data file
void ClientSocketWorker::InitStorage()
{
    //alocate storage
	Log.Notice(__FUNCTION__, "Loading storage.");
    m_pStorage = new Storage(g_DataFileName.c_str());
    ThreadPool.ExecuteTask(m_pStorage);
    Log.Notice(__FUNCTION__, "Storage loaded.");
    
    //start memory watcher
    Log.Notice(__FUNCTION__, "Starting MemoryWatcher.");
    ThreadPool.ExecuteTask(new MemoryWatcher(*m_pStorage));
    Log.Notice(__FUNCTION__, "MemoryWatcher started.");
}

//starts worker threads
void ClientSocketWorker::InitWorkerThreads()
{
	size_t poolSize;

	//allocate pool 2 times bigger than needed, for mem pool overhead, min size 8MB
	poolSize = ((g_MaxTasksInQueue + g_MaxReadTasksInQueue) * sizeof(ClientSocketTaskData)) * 2;
    poolSize = std::max<size_t>(poolSize, 8*1024*1024);
    Log.Notice(__FUNCTION__, "Creating ClientSocketWorker memory pool size: " I64FMTD, poolSize);
    
	m_pFixedPoolMem = ::malloc(poolSize);
	m_pFixedPool = new tbb::fixed_pool(m_pFixedPoolMem, poolSize);

	//set queue limit
	m_rTaskDataQueue.set_capacity(g_MaxTasksInQueue);
    //set read queue limit
    m_rReadTaskDataQueue.set_capacity(g_MaxReadTasksInQueue);

	//start worker threads
    Log.Notice(__FUNCTION__, "Spawning writer threads...");
	for(int i = 0;i < g_MaxParallelTasks;++i)
	{
		ThreadPool.ExecuteTask(new ClientSocketWorkerTask(*m_pStorage, false));
	}
    Log.Notice(__FUNCTION__, "Spawning writer threads... done");
    
    //start reader worker threads
    Log.Notice(__FUNCTION__, "Spawning reader threads...");
    for(int i = 0;i < g_MaxParallelReadTasks;++i)
    {
        ThreadPool.ExecuteTask(new ClientSocketWorkerTask(*m_pStorage, true));
    }
    Log.Notice(__FUNCTION__, "Spawning reader threads... done");
}

void ClientSocketWorker::DestroyWorkerThreads()
{
    TaskDataQueue::size_type waitSize;
    
    //wait for task(s) finish only if there is no exception
    if(!m_exception)
    {
        //! Return number of pushes minus number of pops.
        /** Note that the result can be negative if there are pops waiting for the
         corresponding pushes.  The result can also exceed capacity() if there
         are push operations in flight. */
        waitSize = g_MaxParallelTasks * (-1);
        
        //process all tasks before shutdown
        while(m_rTaskDataQueue.size() != waitSize)
        {
            Log.Notice(__FUNCTION__, "Waiting for: %d tasks to finish.", (int32)m_rTaskDataQueue.size());
            Sleep(5000);
        }
        
        //read tasks
        waitSize = g_MaxParallelReadTasks * (-1);
        while(m_rReadTaskDataQueue.size() !=  waitSize)
        {
            Log.Notice(__FUNCTION__, "Waiting for: %d read tasks to finish.", (int32)m_rReadTaskDataQueue.size());
            Sleep(5000);
        }
        
        Log.Notice(__FUNCTION__, "Tasks finished. Calling abort.");
    }

	//wake up threads
	m_rTaskDataQueue.abort();
    m_rReadTaskDataQueue.abort();
}

//write pending write to disk and destroy storage
void ClientSocketWorker::DestroyStorage()
{
	delete m_pStorage;
    m_pStorage = NULL;
}

void ClientSocketWorker::QueuePacket(const uint16 &opcode, const uint64 &socketID, ClientSocketBuffer &rPacket)
{
	void *pClientSocketTaskDataMem;
    ClientSocketTaskData *pClientSocketTaskData;    
    
    //alloc memory + create struct with task data
	pClientSocketTaskDataMem = m_pFixedPool->malloc(sizeof(ClientSocketTaskData));
	if(pClientSocketTaskDataMem == NULL)
	{
		Log.Error(__FUNCTION__, "pClientSocketTaskDataMem == NULL");
		throw std::bad_alloc();
	}

    pClientSocketTaskData = new(pClientSocketTaskDataMem) ClientSocketTaskData(opcode, socketID, rPacket);
	m_rTaskDataQueue.push(pClientSocketTaskData);
}

void ClientSocketWorker::QueueReadPacket(const uint16 &opcode, const uint64 &socketID, ClientSocketBuffer &rPacket)
{
	void *pClientSocketTaskDataMem;
    ClientSocketTaskData *pClientSocketTaskData;
    
    //alloc memory + create struct with task data
	pClientSocketTaskDataMem = m_pFixedPool->malloc(sizeof(ClientSocketTaskData));
	if(pClientSocketTaskDataMem == NULL)
	{
		Log.Error(__FUNCTION__, "pClientSocketTaskDataMem == NULL");
		throw std::bad_alloc();
	}
    
    pClientSocketTaskData = new(pClientSocketTaskDataMem) ClientSocketTaskData(opcode, socketID, rPacket);
	m_rReadTaskDataQueue.push(pClientSocketTaskData);
}

size_t ClientSocketWorker::GetQueueSize()
{
	return m_rTaskDataQueue.size();
}

size_t ClientSocketWorker::GetReadQueueSize()
{
    return m_rReadTaskDataQueue.size();
}


































