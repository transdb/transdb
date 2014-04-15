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
        
        while(m_threadRunning)
        {
            Packet rPacket(C_MSG_STATUS, 1024);
            rPacket << uint32(token);
            rPacket << uint32(0);
            m_pSocket->SendPacket(rPacket);
        
            ++token;
            Wait(5000);
        }
        return true;
        
//        uint64 random;
//        pData = (uint8*)malloc(datasize);
//
//        printf("%lu\n", time(NULL));
//        for(uint64 x = 0;x < 1000000;++x)
//        {
//            //random = rand();
//            random = x;
//            
//            for(uint64 y = 1;y < 1000;++y)
//            {
//                memset(pData, rand(), datasize);
//                
//                Packet rPacket(C_MSG_WRITE_DATA, 1024);
//                rPacket << uint32(token);
//                rPacket << uint32(0);
//                rPacket << uint64(random);
//                rPacket << y;
//                rPacket.append(pData, datasize);
//
//                while(m_pSocket->SendPacket(rPacket) != OUTPACKET_RESULT_SUCCESS)
//                {
//                    Wait(100);
//                    //pSocket->BurstPush();
//                    
//                    //                printf("Sleep\n");
//                }
//                
//                //if(!(token % 10000))
//                //{
//                //	Wait(100);
//                //}
//                ++token;
//            }
//        }
//        printf("%lu\n", time(NULL));
//		free(pData);
//        
//		return true;
	}
    
	ClientSocket  *m_pSocket;
};

int main(int argc, const char * argv[])
{
    assert(StartSharedLib());    
    
    
    ClientSocket *pSocket = ConnectTCPSocket<ClientSocket>("192.168.10.69", 5555);
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