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
    SendTask(ClientSocket *pSocket) : m_pSocket(pSocket)
    {
        
    }
    
	bool run()
	{
        size_t datasize = 512;
        uint8 *pData;
        uint32 token = 0;
        
//        Packet rPacket(C_MSG_READ_DATA, 1024);
//        rPacket << uint32(GetTickCount64());
//        rPacket << uint32(0);
//        rPacket << uint64(8622535796);
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
        for(uint64 i = 0;i < 10;++i)
        {
            for(uint64 x = 0;x < 100;++x)
            {
    //            random = 1;
    //            random = rand() % 1000;
                random = x;
                
                for(uint64 y = 1;y < 1000;++y)
                {
                    datasize = rand()%1024;
                    pData = (uint8*)malloc(datasize);
                    memset(pData, (rand()%255)+1, datasize);
                    
                    Packet rPacket(C_MSG_WRITE_DATA, 1024);
                    rPacket << uint32(token);
                    rPacket << uint32(0);
                    rPacket << uint64(x);
                    rPacket << uint64(y); //uint64(GetTickCount64()); //uint64(y);
                    rPacket.append(pData, datasize);

                    free(pData);
                    
                    while(m_pSocket->SendPacket(rPacket) != OUTPACKET_RESULT_SUCCESS)
                    {
                        Wait(100);
                    }

    //                std::this_thread::sleep_for(std::chrono::microseconds(10));
                    ++token;
                }
            }
        }
        printf("%lu\n", time(NULL));
        
		return true;
	}
    
private:
	ClientSocket  *m_pSocket;
};

int main(int argc, const char * argv[])
{
    assert(StartSharedLib());    
    
    
    ClientSocket *pSocket = ConnectTCPSocket<ClientSocket>("localhost", 5555);
    if(pSocket == NULL)
        return 1;
    
	SendTask *pSendTask = new SendTask(pSocket);
	ThreadPool.ExecuteTask(pSendTask);
    
    Launcher rLauncher;
    rLauncher.run();
    
    pSocket->Disconnect();
    
    assert(StopSharedLib());    
    return 0;
}


