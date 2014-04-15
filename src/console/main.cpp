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
#ifdef WIN32
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

	//create config file watcher
	ThreadPool.ExecuteTask(new ConfigWatcher(sConfigPath.c_str()));

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
        
    //run
    Launcher rLauncher;
	rLauncher.HookSignals();
    rLauncher.run();
	rLauncher.UnhookSignals();

	//close listen socket -> threadpool will delete it
	pClientSocket->Close();

	//stop lib
    StopSharedLib();

	//delete storage
	Log.Notice(__FUNCTION__, "Syncing storage.");
	delete g_pStorage;
	Log.Notice(__FUNCTION__, "Storage sync complete.");

    return 0;
}

