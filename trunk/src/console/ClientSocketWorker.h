//
//  ClientSocketWorker.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 12.11.12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#ifndef __TransDB__ClientSocketWorker__
#define __TransDB__ClientSocketWorker__

class ClientSocketWorker
{
public:
    static ClientSocketWorker *create();
	~ClientSocketWorker();
    
	//wait for queue empty and than shutdown worker threads
	void DestroyWorkerThreads();
    //write pending write to disk and destroy storage
    void DestroyStorage();
    //queue item
    void QueuePacket(uint16 opcode, uint64 socketID, bbuff *pData, E_TQT eQueueType);
	//get queue size
	size_t GetQueueSize();
    //get read queue size
    size_t GetReadQueueSize();
    //set exception
    INLINE void SetException(bool oValue)      { m_exception = oValue; }

private:
    //private ctor
    explicit ClientSocketWorker();
    //init storage, load index and data file
    bool InitStorage();
	//starts worker threads
	bool InitWorkerThreads();
    //starts config watcher worker thread
    bool InitConfigWatcher();
    //starts python interface if enabled
    bool InitPythonInterface();
	//disable copy constructor and assign
	DISALLOW_COPY_AND_ASSIGN(ClientSocketWorker);
    
    Storage*            m_pStorage;
    PythonInterface*    m_pPythonInterface;
    ConfigWatcher*      m_pConfigWatcher;
    TaskDataQueue*      m_pTaskDataQueue[eTQT_Num];
    std::atomic<bool>   m_exception;
};

extern ClientSocketWorker *g_pClientSocketWorker;

#endif /* defined(__TransDB__ClientSocketWorker__) */
