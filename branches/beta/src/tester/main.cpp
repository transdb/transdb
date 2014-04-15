//
//  main.cpp
//  TransDBTester
//
//  Created by Miroslav Kudrnac on 8/30/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "main.h"

class SendTask : public ThreadContext
{
public:
	bool run()
	{
        size_t datasize = 512;
        uint8 *pData;
        uint32 token = 0;
        
//        Packet rPacket(C_MSG_DELETE_X, 1024);
//        rPacket << uint32(GetTickCount64());
//        rPacket << uint32(0);
////        rPacket << uint64(1);
////        rPacket << uint64(0);
//        m_pSocket->SendPacket(rPacket);
//        return true;
        
//        while(m_threadRunning)
//        {
//            Packet rPacket(C_MSG_STATUS, 1024);
//            rPacket << uint32(token);
//            rPacket << uint32(0);
//            m_pSocket->SendPacket(rPacket);
//        
//            ++token;
//            Wait(5000);
//        }
//        return true;
        
        uint64 random;

        printf("%lu\n", time(NULL));
        for(uint64 x = 0;x < 10000;++x)
        {
//            random = 1;
            random = rand() % 1000;
//            random = x;
            
            for(uint64 y = 1;y < 500;++y)
            {
                datasize = 1024;
                pData = (uint8*)malloc(datasize);
                memset(pData, (rand()%255)+1, datasize);
                
                Packet rPacket(C_MSG_WRITE_DATA, 1024);
                rPacket << uint32(token);
                rPacket << uint32(0);
                rPacket << uint64(random);
                rPacket << uint64(y);
                rPacket.append(pData, datasize);

                free(pData);
                
                while(m_pSocket->SendPacket(rPacket) != OUTPACKET_RESULT_SUCCESS)
                {
                    Wait(100);
                }

                ++token;
            }
        }
        printf("%lu\n", time(NULL));
        
		return true;
	}
    
	ClientSocket  *m_pSocket;
};

int main(int argc, const char * argv[])
{
    assert(StartSharedLib());    
    
    
    ClientSocket *pSocket = ConnectTCPSocket<ClientSocket>("localhost", 5555);
    if(pSocket == NULL)
        return 1;
    
	SendTask *pSendTask = new SendTask;
	pSendTask->m_pSocket = pSocket;
	ThreadPool.ExecuteTask(pSendTask);
    
    Launcher rLauncher;
    rLauncher.run();
    
    pSocket->Disconnect();
    
    assert(StopSharedLib());    
    return 0;
}


