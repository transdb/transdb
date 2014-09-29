//
//  ClientSocketWorkerTask.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 12.11.12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#ifndef __TransDB__ClientSocketWorkerTask__
#define __TransDB__ClientSocketWorkerTask__

class LRUCache;
class ClientSocketTaskData;
class ClientSocketWorker;
class PythonInterface;

/** Data per 1task to worker thread
 */
struct ClientSocketTaskData
{
    uint16  m_opcode;
    uint64  m_socketID;
    bbuff*  m_pData;
};

/** Queue for task
 */
typedef tbb::concurrent_bounded_queue<ClientSocketTaskData> TaskDataQueue;

/** To avoid if
 */
typedef enum E_TASK_QUEUE_TYPE
{
    eTQT_Read       = 0,
    eTQT_Write      = 1,
    eTQT_Num
} E_TQT;

class ClientSocketWorkerTask : public ThreadContext
{    
    friend class Storage;
    
public:
    explicit ClientSocketWorkerTask(TaskDataQueue &rTaskDataQueue,
                                    ClientSocketWorker &rClientSocketWorker,
                                    Storage &rStorage,
                                    PythonInterface &rPythonInterface,
                                    ConfigWatcher &rConfigWatcher);
    ~ClientSocketWorkerTask();
    
    //thread
    bool run();
    
    //handlers
    void HandleWriteData(ClientSocketTaskData&);
    void HandleReadData(ClientSocketTaskData&);
    void HandleDeleteData(ClientSocketTaskData&);
    void HandleGetAllX(ClientSocketTaskData&);
    void HandleGetAllY(ClientSocketTaskData&);
  	void HandleStatus(ClientSocketTaskData&);
    void HandleDeleteX(ClientSocketTaskData&);
    void HandleDefragmentData(ClientSocketTaskData&);
    void HandleGetFreeSpace(ClientSocketTaskData&);
    void HandleWriteDataNum(ClientSocketTaskData&);
    void HandleReadLog(ClientSocketTaskData&);
    void HandleReadConfig(ClientSocketTaskData&);
    void HandleDefragmentFreeSpace(ClientSocketTaskData&);
    void HandleExecutePythonScript(ClientSocketTaskData&);
    
private:
	//disable copy constructor and assign
	DISALLOW_COPY_AND_ASSIGN(ClientSocketWorkerTask);
    
    //declarations
    TaskDataQueue               &m_rTaskDataQueue;
    ClientSocketWorker          &m_rClientSocketWorker;
    Storage                     &m_rStorage;
    PythonInterface             &m_rPythonInterface;
    ConfigWatcher               &m_rConfigWatcher;
    LRUCache                    *m_pLRUCache;
    HANDLE                      m_rDataFileHandle;
};

#endif /* defined(__TransDB__ClientSocketWorkerTask__) */
