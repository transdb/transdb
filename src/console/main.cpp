//
//  main.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/20/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

#ifdef WIN32
    string g_serviceLongName = "TransDB";
    string g_serviceName = "TransDB";
    string g_serviceDescription = "Transaction Database Server";
#endif

int main(int argc, const char * argv[])
{
	SetThreadName("Main Thread");
   
#ifdef WIN32
	//start crash handler
	StartCrashHandler();
	//windows only, helps fight heap allocation on allocs lower then 16KB
	ULONG arg = 2;
#ifdef DEBUG
	HeapSetInformation(GetProcessHeap(), HeapEnableTerminationOnCorruption, &arg, sizeof(arg));
#else
	HeapSetInformation(GetProcessHeap(), HeapCompatibilityInformation, &arg, sizeof(arg));
#endif
#endif
    
	//set time
	UNIXTIME = time(NULL);
	localtime(&UNIXTIME, &g_localTime);
    g_StartTime = UNIXTIME;

	//parse command args
	string sConfigPath;
    for(int i = 0;i < argc;++i)
    {                
        if(strcmp(argv[i], "-c") == 0)
        {
            sConfigPath = argv[i+1];
        }
#ifdef CUSTOM_CHECKS
		if(strcmp(argv[i], "-nrib") == 0)
		{
			g_NumRecordsInBlock = atoi(argv[i+1]);
		}
#endif
#ifdef WIN32
		if(strcmp(argv[i], "-s") == 0)
        {
			if(i+1 >= argc)
			{
				Log.Error(__FUNCTION__, "Missing install/uninstall/run command after -s");
				return 1;
			}

            if(strcmp(argv[i+1], "install") == 0)
            {
				if(i+2 < argc)
				{
					g_serviceLongName = g_serviceName = argv[i+2];
					Log.Notice(__FUNCTION__, "Using %s as service name.", g_serviceLongName.c_str());
				}

                if(WinServiceInstall())
                    Log.Success(__FUNCTION__, "Service %s installed", g_serviceLongName.c_str());
                else
                    Log.Error(__FUNCTION__, "Service %s instalation failed", g_serviceLongName.c_str());
                return 1;
            }
            else if(strcmp(argv[i+1], "uninstall") == 0)
            {
				if(i+2 < argc)
				{
					g_serviceLongName = g_serviceName = argv[i+2];
					Log.Notice(__FUNCTION__, "Using %s as service name.", g_serviceLongName.c_str());
				}

                if(WinServiceUninstall())
                    Log.Success(__FUNCTION__, "Service %s uninstaled", g_serviceLongName.c_str());
                else
                    Log.Error(__FUNCTION__, "Service %s uninstalation failed", g_serviceLongName.c_str());
                return 1;
            }
            else if(strcmp(argv[i+1], "run") == 0)
            {
                //if service then log to file
				char buff[MAX_PATH];
				string logPath;
				GetModuleFileName(NULL, buff, MAX_PATH);
				logPath = string(buff);
				size_t resize = logPath.find_last_of("\\");
				logPath.resize(resize);
				logPath.append("\\logs");
				//open log file
                Log.CreateFileLog(FormatOutputString(logPath.c_str(), "transdb", true).c_str());
                //
                if(WinServiceRun())
                    return 0;
                else
                    return 1;
            }
        }
#endif
    }
    
    //print some struct sizes
    Log.Notice(__FUNCTION__, "sizeof(CRec) = "I64FMTD, sizeof(CRec));
    Log.Notice(__FUNCTION__, "sizeof(FreeSpace) = "I64FMTD, sizeof(FreeSpace));
    Log.Notice(__FUNCTION__, "sizeof(RecordIndex) = "I64FMTD, sizeof(RecordIndex));
    Log.Notice(__FUNCTION__, "BLOCK_SIZE = "I64FMTD, BLOCK_SIZE);
    Log.Notice(__FUNCTION__, "INDEX_BLOCK_SIZE = "I64FMTD, INDEX_BLOCK_SIZE);
    Log.Notice(__FUNCTION__, "MAX_RECORD_SIZE = %u", MAX_RECORD_SIZE);
    
    //set default
    if(sConfigPath.empty())
    {
        sConfigPath = "transdb.conf";
    }
    
    //set config
    if(!g_rConfig.MainConfig.SetSource(sConfigPath.c_str()))
    {
        Log.Error(__FUNCTION__, "Config: %s not found.", sConfigPath.c_str());
        return 1;
    }
    
    //load value from config
    LoadConfig();
    
    //start lib
    StartSharedLib();

    //alocate storage
	Log.Notice(__FUNCTION__, "Loading storage.");
    g_pStorage = new Storage(g_DataFileName.c_str());
    ThreadPool.ExecuteTask(g_pStorage);
    Log.Notice(__FUNCTION__, "Storage loaded.");
    
    //start memory watcher
    ThreadPool.ExecuteTask(new MemoryWatcher(*g_pStorage));
    
    //init ClientSocketWorker
    g_pClientSocketWorker = new ClientSocketWorker();
	g_pClientSocketWorker->Init();

	//create config file watcher
	ThreadPool.ExecuteTask(new ConfigWatcher(sConfigPath.c_str()));

    //open listen socket
    ListenSocket<ClientSocket> *pClientSocket = new ListenSocket<ClientSocket>(g_ListenHost.c_str(), g_ListenPort);
    ListenSocket<WebService> *pWebService = new ListenSocket<WebService>(g_ListenHost.c_str(), g_WebSocketPort);
    bool clientSocketCreated = pClientSocket->IsOpen();
    bool webSocketCreated = pWebService->IsOpen();
#ifdef WIN32
	if(clientSocketCreated)
		ThreadPool.ExecuteTask(pClientSocket);
    
    if(webSocketCreated)
		ThreadPool.ExecuteTask(pWebService);
#endif
    if(!clientSocketCreated)
    {
		Log.Error(__FUNCTION__, "Cannot open listen socket.");
		return 1;
    }
    if(!webSocketCreated)
    {
        Log.Error(__FUNCTION__, "Cannot open web listen socket.");
    }
    Log.Notice(__FUNCTION__, "Listen socket open, accepting new connections.");
        
    //run
    Launcher rLauncher;
	rLauncher.HookSignals();
    rLauncher.run();
	rLauncher.UnhookSignals();

	//close listen socket -> threadpool will delete it
    Log.Notice(__FUNCTION__, "Closing listen socket.");
	pClientSocket->Close();
    Log.Notice(__FUNCTION__, "Listen socket closed.");
    //close web socket
    Log.Notice(__FUNCTION__, "Closing web listen socket.");
    pWebService->Close();
    Log.Notice(__FUNCTION__, "Web listen socket closed.");

    //shutdown network subsystem
	Log.Notice(__FUNCTION__, "Shutting down network subsystem.");
#ifdef WIN32
	sSocketMgr.ShutdownThreads();
#endif
	sSocketMgr.CloseAll();
	Log.Notice(__FUNCTION__, "Network subsystem shutdown complete.");

    //delete ClientSocketWorker tasks - process all waiting tasks
    Log.Notice(__FUNCTION__, "Deleting ClientSocketWorker tasks.");
	g_pClientSocketWorker->Destroy();
    Log.Notice(__FUNCTION__, "Deleted ClientSocketWorker tasks.");
    
    //shutdown threadpool
	Log.Notice(__FUNCTION__, "Shutting down threadpool.");
    ThreadPool.Shutdown();
	Log.Notice(__FUNCTION__, "Threadpool shutdown complete.");

	//delete ClientSocketWorker
	Log.Notice(__FUNCTION__, "Deleting ClientSocketWorker.");
	delete g_pClientSocketWorker;
	Log.Notice(__FUNCTION__, "Deleted ClientSocketWorker.");

	//delete network subsystem
	Log.Notice(__FUNCTION__, "Deleting Network Subsystem.");
	delete SocketMgr::getSingletonPtr();
	delete SocketGarbageCollector::getSingletonPtr();
	Log.Notice(__FUNCTION__, "Network Subsystem deleted.");
    
	//delete storage
	Log.Notice(__FUNCTION__, "Syncing storage.");
	delete g_pStorage;
	Log.Notice(__FUNCTION__, "Storage sync complete.");

	//delete CRC32
	Log.Notice(__FUNCTION__, "Deleteting CRC32 class.");
	delete g_CRC32;
	Log.Notice(__FUNCTION__, "CRC32 class deleted.");

    //
    Log.Notice(__FUNCTION__, "Shutdown complete.");
    return 0;
}

