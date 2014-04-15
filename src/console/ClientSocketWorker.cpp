//
//  ClientSocketWorker.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 12.11.12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

ClientSocketWorker	*g_pClientSocketWorker;

ClientSocketWorker::ClientSocketWorker()
{
    m_exception = false;
}

ClientSocketWorker::~ClientSocketWorker()
{
	//destroy pool
	delete m_pFixedPool;
	m_pFixedPool = NULL;
	aligned_free(m_pFixedPoolMem);
	m_pFixedPoolMem = NULL;
}

//starts worker threads
void ClientSocketWorker::Init()
{
	size_t poolSize;

	//allocate pool 2bigger than needed, for mem pool overhead
	poolSize = (g_MaxTasksInQueue * sizeof(ClientSocketTaskData)) * 2;
	m_pFixedPoolMem = aligned_malloc(poolSize);
	m_pFixedPool = new tbb::fixed_pool(m_pFixedPoolMem, poolSize);

	//set queue limit
	m_rTaskDataQueue.set_capacity(g_MaxTasksInQueue);

	//start worker threads
	for(int i = 0;i < g_MaxParallelTasks;++i)
	{
		ThreadPool.ExecuteTask(new ClientSocketWorkerTask());
	}  
}

void ClientSocketWorker::Destroy()
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
            Log.Notice(__FUNCTION__, "Waiting for: %u tasks to finish.", (uint32)m_rTaskDataQueue.size());
            Sleep(5000);
        }
        Log.Notice(__FUNCTION__, "Tasks finished. Calling abort.");
    }

	//wake up threads
	m_rTaskDataQueue.abort();
}

void ClientSocketWorker::QueuePacket(const uint16 &opcode, const long &socketID, ByteBuffer &rPacket)
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

size_t ClientSocketWorker::GetQueueSize()
{
	return m_rTaskDataQueue.size();
}


































