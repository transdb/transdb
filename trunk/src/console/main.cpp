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
#else
    //DAEMON
    static void PrepareDaemon()
    {
        pid_t pid, sid;
        
        //fork parent process
        pid = fork();
        if(pid < 0)
            exit(EXIT_FAILURE);
        
        //we got good pid, close parent process
        if(pid > 0)
            exit(EXIT_SUCCESS);
        
        //change file mask
        umask(0);
        
        //create new signature id for our child
        sid = setsid();
        if(sid < 0)
            exit(EXIT_FAILURE);
        
        //change directory on *unix thre is allway root (/)
        if(chdir("/") < 0)
            exit(EXIT_FAILURE);
        
        //close standart file descriptors
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        
        //all done continue to main loop
    }
#endif

void StartSharedLib()
{
    //network subsystem
    Log.Notice("Network", "Starting subsystem...");
    new SocketMgr;
    new SocketGarbageCollector;
    
    //start thread pool
    ThreadPool.Startup();
    
    //start network mgr
    sSocketMgr.SpawnWorkerThreads(g_NetworkThreads);
    
	//init CRC32
    g_CRC32 = new CRC_32();
}

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
    string sLogPath;
    for(int i = 0;i < argc;++i)
    {                
        if(strcmp(argv[i], "-c") == 0)
        {
            sConfigPath = argv[i+1];
        }
        else if(strcmp(argv[i], "-l") == 0)
        {
            sLogPath = argv[i+1];
#ifndef WIN32
            //open log file
            string sFullLogPath = FormatOutputString(sLogPath.c_str(), "transdb", true);
            Log.CreateFileLog(sFullLogPath);
#endif
        }
#ifndef WIN32
        else if(strcmp(argv[i], "-d") == 0)
        {
            //start as daemon
            PrepareDaemon();
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
                if(sLogPath.empty())
                {
                    //if service then log to file
                    char buff[MAX_PATH];
                    GetModuleFileName(NULL, buff, MAX_PATH);
                    sLogPath = string(buff);
                    size_t resize = sLogPath.find_last_of("\\");
                    sLogPath.resize(resize);
                    sLogPath.append("\\logs");
                }
				//open log file
                string sFullLogPath = FormatOutputString(sLogPath.c_str(), "transdb", true);
                Log.CreateFileLog(sFullLogPath);
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
    Log.Notice(__FUNCTION__, "Starting server in: %s mode. SVN version: %s.", g_pCompiledVersion, SVNVERSION);
    Log.Notice(__FUNCTION__, "TBB version: %u", TBB_INTERFACE_VERSION);
    Log.Notice(__FUNCTION__, "sizeof(CRec) = " I64FMTD, sizeof(CRec));
    Log.Notice(__FUNCTION__, "sizeof(FreeSpace) = " I64FMTD, sizeof(FreeSpace));
    Log.Notice(__FUNCTION__, "sizeof(RecordIndex) = " I64FMTD, sizeof(RecordIndex));
    Log.Notice(__FUNCTION__, "sizeof(DiskRecordIndex) = " I64FMTD, sizeof(DiskRecordIndex));
    Log.Notice(__FUNCTION__, "BLOCK_SIZE = " I64FMTD, BLOCK_SIZE);
    Log.Notice(__FUNCTION__, "INDEX_BLOCK_SIZE = " I64FMTD, INDEX_BLOCK_SIZE);
    Log.Notice(__FUNCTION__, "MAX_RECORD_SIZE = %u", MAX_RECORD_SIZE);
    Log.Notice(__FUNCTION__, "IMAX_RECORD_POS = %u", IMAX_RECORD_POS);
    
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
    
    //init statgenerator CPU usage
    StatGenerator::Init();
    
    //load value from config
    LoadConfig();
    
    //start lib
    StartSharedLib();
       
    //init ClientSocketWorker
    g_pClientSocketWorker = new ClientSocketWorker();
    //init storage
    g_pClientSocketWorker->InitStorage();
    //init worker threads
	g_pClientSocketWorker->InitWorkerThreads();

	//create config file watcher
    g_pConfigWatcher = new ConfigWatcher(sConfigPath.c_str());
	ThreadPool.ExecuteTask(g_pConfigWatcher);

    //open listen socket
    ListenSocket<ClientSocket> *pClientSocket = new ListenSocket<ClientSocket>(g_ListenHost.c_str(), g_ListenPort);
    bool clientSocketCreated = pClientSocket->IsOpen();
#ifdef WIN32
	if(clientSocketCreated)
		ThreadPool.ExecuteTask(pClientSocket);
#endif
    if(!clientSocketCreated)
    {
		Log.Error(__FUNCTION__, "Cannot open listen socket.");
		return 1;
    }
    Log.Notice(__FUNCTION__, "Listen socket open, accepting new connections.");
    
    //start python interface thread
    Log.Notice(__FUNCTION__, "Starting python interface.");
    ThreadPool.ExecuteTask(new PythonInterface());
    
    //from now server is running
    Log.Notice(__FUNCTION__, "Server startup complete.");
    
    //run
    Launcher rLauncher;
	rLauncher.HookSignals();
    rLauncher.run();
	rLauncher.UnhookSignals();

	//close listen socket -> threadpool will delete it
    Log.Notice(__FUNCTION__, "Closing listen socket.");
	pClientSocket->Close();
    Log.Notice(__FUNCTION__, "Listen socket closed.");

    //shutdown network subsystem
	Log.Notice(__FUNCTION__, "Shutting down network subsystem.");
#ifdef WIN32
	sSocketMgr.ShutdownThreads();
#endif
	sSocketMgr.CloseAll();
	Log.Notice(__FUNCTION__, "Network subsystem shutdown complete.");

    //delete ClientSocketWorker tasks - process all waiting tasks
    Log.Notice(__FUNCTION__, "Deleting ClientSocketWorker tasks.");
	g_pClientSocketWorker->DestroyWorkerThreads();
    Log.Notice(__FUNCTION__, "Deleted ClientSocketWorker tasks.");
    
    //shutdown threadpool
	Log.Notice(__FUNCTION__, "Shutting down threadpool.");
    ThreadPool.Shutdown();
	Log.Notice(__FUNCTION__, "Threadpool shutdown complete.");

	//delete network subsystem
	Log.Notice(__FUNCTION__, "Deleting Network Subsystem.");
	delete SocketMgr::getSingletonPtr();
	delete SocketGarbageCollector::getSingletonPtr();
	Log.Notice(__FUNCTION__, "Network Subsystem deleted.");
    
	//delete storage
	Log.Notice(__FUNCTION__, "Syncing storage.");
    g_pClientSocketWorker->DestroyStorage();
	Log.Notice(__FUNCTION__, "Storage sync complete.");
    
	//delete ClientSocketWorker
	Log.Notice(__FUNCTION__, "Deleting ClientSocketWorker.");
	delete g_pClientSocketWorker;
	Log.Notice(__FUNCTION__, "Deleted ClientSocketWorker.");

	//delete CRC32
	Log.Notice(__FUNCTION__, "Deleteting CRC32 class.");
	delete g_CRC32;
	Log.Notice(__FUNCTION__, "CRC32 class deleted.");

    //
    Log.Notice(__FUNCTION__, "Shutdown complete.");
    return 0;
}

