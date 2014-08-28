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

class ClientSocketWorkerTask : public ThreadContext
{    
    friend class Storage;
    
public:
    explicit ClientSocketWorkerTask(ClientSocketWorker &rClientSocketWorker, Storage &rStorage, bool readerTask);
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
    void HandleSqlQuery(ClientSocketTaskData&);
    
private:
	//disable copy constructor and assign
	DISALLOW_COPY_AND_ASSIGN(ClientSocketWorkerTask);
    
    //declarations
    ClientSocketWorker          &m_rClientSocketWorker;
    Storage                     &m_rStorage;
    LRUCache                    *m_pLRUCache;
    HANDLE                      m_rDataFileHandle;
    bool                        m_readerThread;
};

#endif /* defined(__TransDB__ClientSocketWorkerTask__) */
