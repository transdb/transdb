//
//  ClientSocketWorkerTask.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 12.11.12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#ifndef __TransDB__ClientSocketWorkerTask__
#define __TransDB__ClientSocketWorkerTask__

class ClientSocketTaskData;

class ClientSocketWorkerTask : public ThreadContext
{
public:
    explicit ClientSocketWorkerTask(Storage &rStorage, bool readerTask);
    
    //thread
    bool run();
    
    //handlers
    void HandleWriteData(const HANDLE&, ClientSocketTaskData&);
    void HandleReadData(const HANDLE&, ClientSocketTaskData&);
    void HandleDeleteData(const HANDLE&, ClientSocketTaskData&);
    void HandleGetAllX(const HANDLE&, ClientSocketTaskData&);
    void HandleGetAllY(const HANDLE&, ClientSocketTaskData&);
  	void HandleStatus(const HANDLE&, ClientSocketTaskData&);
    void HandleDeleteX(const HANDLE&, ClientSocketTaskData&);
    void HandleDefragmentData(const HANDLE&, ClientSocketTaskData&);
    void HandleGetFreeSpace(const HANDLE&, ClientSocketTaskData&);
    void HandleWriteDataNum(const HANDLE&, ClientSocketTaskData&);
    void HandleReadLog(const HANDLE&, ClientSocketTaskData&);
    void HandleReadConfig(const HANDLE&, ClientSocketTaskData&);
    
private:
	//disable copy constructor and assign
	DISALLOW_COPY_AND_ASSIGN(ClientSocketWorkerTask);
    
    Storage     &m_rStorage;
    bool        m_readerThread;
};

#endif /* defined(__TransDB__ClientSocketWorkerTask__) */
