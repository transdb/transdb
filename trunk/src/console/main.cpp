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
    static int _null_open(int f, int fd)
    {
        int fd2;
        
        if ((fd2 = open("/dev/null", f)) < 0)
            return -1;
        
        if (fd2 == fd)
            return fd;
        
        if (dup2(fd2, fd) < 0)
            return -1;
        
        close(fd2);
        return fd;
    }

    static void PrepareDaemon()
    {
        pid_t pid, sid;
        struct rlimit rl;
        int maxfd;
        int fd;
        
        //fork parent process
        pid = fork();
        if(pid < 0)
            _exit(EXIT_FAILURE);
        
        //we got good pid, close parent process
        if(pid > 0)
            _exit(EXIT_SUCCESS);
        
        //redirect standard descriptors to /dev/null
        if(_null_open(O_RDONLY, STDIN_FILENO) < 0)
            _exit(EXIT_FAILURE);
        
        if(_null_open(O_WRONLY, STDOUT_FILENO) < 0)
            _exit(EXIT_FAILURE);
        
        if(_null_open(O_WRONLY, STDERR_FILENO) < 0)
            _exit(EXIT_FAILURE);
        
        //create new signature id for our child
        sid = setsid();
        if(sid < 0)
            _exit(EXIT_FAILURE);
        
        // A second fork ensures the process cannot acquire a controlling terminal.
        pid = fork();
        if(pid < 0)
            _exit(EXIT_FAILURE);
        
        //we got good pid, close parent process
        if(pid > 0)
            _exit(EXIT_SUCCESS);
        
        //change directory on *unix thre is allway root (/)
        if(chdir("/") < 0)
            _exit(EXIT_FAILURE);
        
        //change file mask
        umask(0);
        
        //close all decriptors
        if (getrlimit(RLIMIT_NOFILE, &rl) > 0)
            maxfd = (int)rl.rlim_max;
        else
            maxfd = (int)sysconf(_SC_OPEN_MAX);
        
        for(fd = 3;fd < maxfd;++fd)
        {
            close(fd);
        }
        
        //all done continue to main loop
    }
#endif

void StartSharedLib()
{
    //start thread pool
    Log.Notice("ThreadPool", "Starting subsystem...");
    ThreadPool.Startup();
    Log.Notice("ThreadPool", "Starting subsystem... done");
 
    //network subsystem
    Log.Notice("Network", "Starting subsystem...");
    new SocketMgr;
    new SocketGarbageCollector;
    //start network mgr
    sSocketMgr.SpawnWorkerThreads();
    Log.Notice("Network", "Starting subsystem... done");
    
	//init CRC32
    g_CRC32 = new CRC_32();
}

int main(int argc, const char * argv[])
{
	CommonFunctions::SetThreadName("Main Thread");
   
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
	std::string sConfigPath;
    std::string sLogPath;
    std::string sPidFilePath;
    bool startAsDaemon = false;
    for(int i = 0;i < argc;++i)
    {                
        if(strcmp(argv[i], "-c") == 0)
        {
            sConfigPath = argv[i+1];
        }
        else if(strcmp(argv[i], "-l") == 0)
        {
            sLogPath = argv[i+1];
        }
#ifndef WIN32
        else if(strcmp(argv[i], "-d") == 0)
        {
            startAsDaemon = true;
        }
        else if(strcmp(argv[i], "-p") == 0)
        {
            sPidFilePath = argv[i+1];
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
				string sFullLogPath = CommonFunctions::FormatOutputString(sLogPath.c_str(), "transdb", true);
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
    
#ifndef WIN32
    //start as daemon
    if(startAsDaemon)
    {
        PrepareDaemon();
    }
    
    //open log file
    if(!sLogPath.empty())
    {
        std::string sFullLogPath = CommonFunctions::FormatOutputString(sLogPath.c_str(), "transdb", true);
        Log.CreateFileLog(sFullLogPath);
    }
#endif
    
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
    
#ifndef WIN32
    //create pid file
    if(!sPidFilePath.empty())
    {
        static const uint32 buff_size = 256;
        char buff[buff_size];
        long pid = static_cast<long>(getpid());
        //create file
        CommonFunctions::CheckFileExists(sPidFilePath.c_str(), true);
        //open file
        HANDLE hFile = IO::fopen(sPidFilePath.c_str(), IO::IO_WRITE_ONLY);
        if(hFile == INVALID_HANDLE_VALUE)
            return 1;
        
        //write pid
        snprintf(buff, buff_size, "%ld\n", pid);
        IO::fwrite(buff, strlen(buff), hFile);
        IO::fclose(hFile);
    }
#endif
    
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
    if(g_PythonEnable)
    {
        Log.Notice(__FUNCTION__, "Starting python interface...");
        ThreadPool.ExecuteTask(new PythonInterface());
        Log.Notice(__FUNCTION__, "Starting python interface... done");
    }
    
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

