//
//  ClientSocketWorkerTask.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 12.11.12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#ifndef __TransDB__ClientSocketWorkerTask__
#define __TransDB__ClientSocketWorkerTask__

struct ClientSocketTaskData;

class ClientSocketWorkerTask : public ThreadContext
{
public:
    ClientSocketWorkerTask();
    ~ClientSocketWorkerTask();
    
    //thread
    bool run();
    
    //handlers
    void HandleWriteData(ClientSocketTaskData&);
    void HandleReadData(ClientSocketTaskData&);
    void HandleDeleteData(ClientSocketTaskData&);
    void HandleGetAllX(ClientSocketTaskData&);
    void HandleGetAllY(ClientSocketTaskData&);
    void HandleDeleteX(ClientSocketTaskData&);
};

#endif /* defined(__TransDB__ClientSocketWorkerTask__) */
