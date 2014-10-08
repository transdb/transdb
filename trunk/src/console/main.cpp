//
//  main.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 8/20/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

#ifdef WIN32
	std::string g_serviceLongName = "TransDB";
	std::string g_serviceName = "TransDB";
	std::string g_serviceDescription = "Transaction Database Server";
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
    crc32_init();
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
	//init stats
	init_stats_default_values(&g_stats);

	//parse command args
	std::string sConfigPath;
	std::string sLogPath;
#ifndef WIN32
	std::string sPidFilePath;
	bool startAsDaemon = false;
#endif
	for (int i = 0; i < argc; ++i)
	{
		if (strcmp(argv[i], "-c") == 0)
		{
			sConfigPath = argv[i + 1];
		}
		else if (strcmp(argv[i], "-l") == 0)
		{
			sLogPath = argv[i + 1];
		}
		else if (strcmp(argv[i], "-f") == 0)
		{
			g_ForceStartup = (E_FSA)atoi(argv[i + 1]);
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
		if (strcmp(argv[i], "-s") == 0)
		{
			if (i + 1 >= argc)
			{
				Log.Error(__FUNCTION__, "Missing install/uninstall/run command after -s");
				return EXIT_FAILURE;
			}

			if (strcmp(argv[i + 1], "install") == 0)
			{
				if (i + 2 < argc)
				{
					g_serviceLongName = g_serviceName = argv[i + 2];
					Log.Notice(__FUNCTION__, "Using %s as service name.", g_serviceLongName.c_str());
				}

				if (WinServiceInstall())
					Log.Success(__FUNCTION__, "Service %s installed", g_serviceLongName.c_str());
				else
					Log.Error(__FUNCTION__, "Service %s instalation failed", g_serviceLongName.c_str());
				return EXIT_FAILURE;
			}
			else if (strcmp(argv[i + 1], "uninstall") == 0)
			{
				if (i + 2 < argc)
				{
					g_serviceLongName = g_serviceName = argv[i + 2];
					Log.Notice(__FUNCTION__, "Using %s as service name.", g_serviceLongName.c_str());
				}

				if (WinServiceUninstall())
					Log.Success(__FUNCTION__, "Service %s uninstaled", g_serviceLongName.c_str());
				else
					Log.Error(__FUNCTION__, "Service %s uninstalation failed", g_serviceLongName.c_str());
				return EXIT_FAILURE;
			}
			else if (strcmp(argv[i + 1], "run") == 0)
			{
				if (sLogPath.empty())
				{
					//if service then log to file
					char buff[MAX_PATH];
					GetModuleFileName(NULL, buff, MAX_PATH);
					sLogPath = std::string(buff);
					size_t resize = sLogPath.find_last_of("\\");
					sLogPath.resize(resize);
					sLogPath.append("\\logs");
				}
				//open log file
				std::string sFullLogPath = CommonFunctions::FormatOutputString(sLogPath.c_str(), "transdb", true);
				Log.CreateFileLog(sFullLogPath);
				//
				if (WinServiceRun())
					return EXIT_SUCCESS;
				else
					return EXIT_FAILURE;
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
		HANDLE hFile = IO::fopen(sPidFilePath.c_str(), IO::IO_WRITE_ONLY, IO::IO_NORMAL);
		if(hFile == INVALID_HANDLE_VALUE)
		{
			Log.Error(__FUNCTION__, "Cannot create pid file: %s", sPidFilePath.c_str());
			return EXIT_FAILURE;
		}

		//write pid
		snprintf(buff, buff_size, "%ld\n", pid);
		IO::fwrite(buff, strlen(buff), hFile);
		IO::fclose(hFile);
	}
#endif

	//set default
	if (sConfigPath.empty())
	{
		sConfigPath = "transdb.conf";
	}

	//set config
	if (!g_rConfig.MainConfig.SetSource(sConfigPath.c_str()))
	{
		Log.Error(__FUNCTION__, "Config: %s not found.", sConfigPath.c_str());
		return EXIT_FAILURE;
	}

	//init statgenerator CPU usage
	StatGenerator::Init();

	//load value from config
	LoadConfig();

	//alignment for DMA - http://msdn.microsoft.com/en-us/library/windows/desktop/cc644950(v=vs.85).aspx
	/** File access buffer addresses for read and write operations should be physical
	*	sector-aligned, which means aligned on addresses in memory that are integer multiples of the volume's physical sector size.
	*	Depending on the disk, this requirement may not be enforced.
	*/
#ifdef WIN32
	char cDataRootPath[4] = { 0 };
	char cIndexRootPath[4] = { 0 };
	DWORD lpSectorsPerCluster;
	DWORD lpBytesPerSector;
	DWORD lpNumberOfFreeClusters;
	DWORD lpTotalNumberOfClusters;
	BOOL oGetDiskFreeSpaceStatus;

	if (strlen(g_cfg.DataFilePath) < 3)
	{
		Log.Error(__FUNCTION__, "Wrong datafile path: %s", g_cfg.DataFilePath);
		return EXIT_FAILURE;
	}
	if (strlen(g_cfg.IndexFilePath) < 3)
	{
		Log.Error(__FUNCTION__, "Wrong indexfile path: %s", g_cfg.IndexFilePath);
		return EXIT_FAILURE;
	}

	//get root disk (C:\, D:\, etc.)
	memcpy(cDataRootPath, g_cfg.DataFilePath, 3);
	memcpy(cIndexRootPath, g_cfg.IndexFilePath, 3);

	//get bytespersector for datafile
	oGetDiskFreeSpaceStatus = GetDiskFreeSpace(cDataRootPath, &lpSectorsPerCluster, &lpBytesPerSector, &lpNumberOfFreeClusters, &lpTotalNumberOfClusters);
	if (oGetDiskFreeSpaceStatus == FALSE)
	{
		Log.Error(__FUNCTION__, "GetDiskFreeSpace failed for disk drive: %s", cDataRootPath);
		return EXIT_FAILURE;
	}
	//save datafile malloc alignment
	g_DataFileMallocAlignment = lpBytesPerSector;

	//get bytespersector for indexfile
	oGetDiskFreeSpaceStatus = GetDiskFreeSpace(cIndexRootPath, &lpSectorsPerCluster, &lpBytesPerSector, &lpNumberOfFreeClusters, &lpTotalNumberOfClusters);
	if (oGetDiskFreeSpaceStatus == FALSE)
	{
		Log.Error(__FUNCTION__, "GetDiskFreeSpace failed for disk drive: %s", cIndexRootPath);
		return EXIT_FAILURE;
	}
	//save indexfile malloc alignment
	g_IndexFileMallocAlignment = lpBytesPerSector;
#endif
	//log
	Log.Notice(__FUNCTION__, "DataFileMallocAlignment = %d", g_DataFileMallocAlignment);
	Log.Notice(__FUNCTION__, "IndexFileMallocAlignment = %d", g_IndexFileMallocAlignment);
    
    //init intel tbb memory allocator soft limit
    intptr_t softHeapLimit = static_cast<intptr_t>((static_cast<float>(g_cfg.MemoryLimit) * 1.10f));
    int ret = scalable_allocation_mode(TBBMALLOC_SET_SOFT_HEAP_LIMIT, softHeapLimit);
    if(ret != TBBMALLOC_OK)
    {
        Log.Warning(__FUNCTION__, "TBBMALLOC_SET_SOFT_HEAP_LIMIT failed with return value: %d", ret);
    }
    
    //set huge pages if OS suports it
    ret = scalable_allocation_mode(TBBMALLOC_USE_HUGE_PAGES, 1);
    if(ret != TBBMALLOC_OK)
    {
        Log.Warning(__FUNCTION__, "TBBMALLOC_USE_HUGE_PAGES failed with return value: %d", ret);
    }
    
    //start lib
    StartSharedLib();
       
    //init ClientSocketWorker
    g_pClientSocketWorker = ClientSocketWorker::create();
    if(g_pClientSocketWorker == NULL)
        return EXIT_FAILURE;
    
    //open listen socket
    ListenSocket<ClientSocket> *pClientSocket = new ListenSocket<ClientSocket>(g_cfg.ListenHost, g_cfg.ListenPort);
    bool clientSocketCreated = pClientSocket->IsOpen();
#ifdef WIN32
	if(clientSocketCreated)
		ThreadPool.ExecuteTask(pClientSocket);
#endif
    if(!clientSocketCreated)
    {
		Log.Error(__FUNCTION__, "Cannot open listen socket.");
		return EXIT_FAILURE;
    }
    Log.Notice(__FUNCTION__, "Listen socket open, accepting new connections.");
    
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

    //
    Log.Notice(__FUNCTION__, "Shutdown complete.");
    return EXIT_SUCCESS;
}

