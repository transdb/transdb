//
//  ServiceWin32.cpp
//  TransDB
//
//  Created by Miroslav Kudrnac on 9/10/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#include "StdAfx.h"

#ifdef WIN32

#if !defined(WINADVAPI)
#if !defined(_ADVAPI32_)
#define WINADVAPI DECLSPEC_IMPORT
#else
#define WINADVAPI
#endif
#endif

extern int main(int argc, char** argv);
extern string g_serviceLongName;
extern string g_serviceName;
extern string g_serviceDescription;

/*
* -1 - not in service mode
*  0 - stopped
*  1 - running
*  2 - paused
*/
std::atomic<long> g_ServiceStatus(-1);

SERVICE_STATUS serviceStatus;

SERVICE_STATUS_HANDLE serviceStatusHandle = 0;

typedef WINADVAPI BOOL (WINAPI* CSD_T)(SC_HANDLE, DWORD, LPCVOID);

bool WinServiceInstall()
{
    CSD_T ChangeService_Config2;
    HMODULE advapi32;
    SC_HANDLE serviceControlManager = OpenSCManager(0, 0, SC_MANAGER_CREATE_SERVICE);
    
    if (!serviceControlManager)
    {
        Log.Error(__FUNCTION__, "SERVICE: No access to service control manager.");
        return false;
    }
    
    char path[_MAX_PATH + 10];
    if (!GetModuleFileName(0, path, sizeof(path) / sizeof(path[0])))
    {
        CloseServiceHandle(serviceControlManager);
        Log.Error(__FUNCTION__, "SERVICE: Can't get service binary filename.");
        return false;
    }
    
    std::strcat(path, " -s run");
    
    SC_HANDLE service = CreateService(serviceControlManager,
                                      g_serviceName.c_str(),          // name of service
                                      g_serviceLongName.c_str(),      // service name to display
                                      SERVICE_ALL_ACCESS,			// desired access
                                      // service type
                                      SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS,
                                      SERVICE_AUTO_START,   // start type
                                      SERVICE_ERROR_IGNORE, // error control type
                                      path,                 // service's binary
                                      0,                    // no load ordering group
                                      0,                    // no tag identifier
                                      0,                    // no dependencies
                                      0,                    // LocalSystem account
                                      0);                   // no password
    
    if (!service)
    {
        CloseServiceHandle(serviceControlManager);
        Log.Error(__FUNCTION__, "SERVICE: Can't register service for: %s", path);
        return false;
    }
    
    advapi32 = GetModuleHandle("ADVAPI32.DLL");
    if (!advapi32)
    {
        Log.Error(__FUNCTION__, "SERVICE: Can't access ADVAPI32.DLL");
        CloseServiceHandle(service);
        CloseServiceHandle(serviceControlManager);
        return false;
    }
    
    ChangeService_Config2 = (CSD_T) GetProcAddress(advapi32, "ChangeServiceConfig2A");
    if (!ChangeService_Config2)
    {
        Log.Error(__FUNCTION__, "SERVICE: Can't access ChangeServiceConfig2A from ADVAPI32.DLL");
        CloseServiceHandle(service);
        CloseServiceHandle(serviceControlManager);
        return false;
    }
    
    SERVICE_DESCRIPTION sdBuf;
    sdBuf.lpDescription = (LPSTR)g_serviceDescription.c_str();
    ChangeService_Config2(
                          service,                                // handle to service
                          SERVICE_CONFIG_DESCRIPTION,             // change: description
                          &sdBuf);                                // new data
    
    SC_ACTION _action[1];
    _action[0].Type = SC_ACTION_RESTART;
    _action[0].Delay = 10000;
    SERVICE_FAILURE_ACTIONS sfa;
    ZeroMemory(&sfa, sizeof(SERVICE_FAILURE_ACTIONS));
    sfa.lpsaActions = _action;
    sfa.cActions = 1;
    sfa.dwResetPeriod = INFINITE;
    ChangeService_Config2(
                          service,                                // handle to service
                          SERVICE_CONFIG_FAILURE_ACTIONS,         // information level
                          &sfa);                                  // new data
    
    CloseServiceHandle(service);
    CloseServiceHandle(serviceControlManager);
    return true;
}

bool WinServiceUninstall()
{
    SC_HANDLE serviceControlManager = OpenSCManager(0, 0, SC_MANAGER_CONNECT);
    
    if (!serviceControlManager)
    {
        Log.Error(__FUNCTION__, "SERVICE: No access to service control manager.");
        return false;
    }
    
    SC_HANDLE service = OpenService(serviceControlManager,
                                    g_serviceName.c_str(), SERVICE_QUERY_STATUS | DELETE);
    
    if (!service)
    {
        CloseServiceHandle(serviceControlManager);
        Log.Error(__FUNCTION__, "SERVICE: Service not found: %s", g_serviceName.c_str());
        return false;
    }
    
    SERVICE_STATUS serviceStatus2;
    if (QueryServiceStatus(service, &serviceStatus2))
    {
        if (serviceStatus2.dwCurrentState == SERVICE_STOPPED)
            DeleteService(service);
    }
    
    CloseServiceHandle(service);
    CloseServiceHandle(serviceControlManager);
    return true;
}

void WINAPI ServiceControlHandler(DWORD controlCode)
{
    switch (controlCode)
    {
        case SERVICE_CONTROL_INTERROGATE:
            break;
            
        case SERVICE_CONTROL_SHUTDOWN:
        case SERVICE_CONTROL_STOP:
            serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
            SetServiceStatus(serviceStatusHandle, &serviceStatus);
            
            g_ServiceStatus = 0;
            return;
            
        case SERVICE_CONTROL_CONTINUE:
            serviceStatus.dwCurrentState = SERVICE_RUNNING;
            SetServiceStatus(serviceStatusHandle, &serviceStatus);

            g_ServiceStatus = 1;
            break;
            
        default:
            if (controlCode >= 128 && controlCode <= 255)
                // user defined control code
                break;
            else
                // unrecognized control code
                break;
    }
    
    SetServiceStatus(serviceStatusHandle, &serviceStatus);
}

void WINAPI ServiceMain(DWORD argc, char* argv[])
{
    // initialise service status
    serviceStatus.dwServiceType = SERVICE_WIN32;
    serviceStatus.dwCurrentState = SERVICE_START_PENDING;
    serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    serviceStatus.dwWin32ExitCode = NO_ERROR;
    serviceStatus.dwServiceSpecificExitCode = NO_ERROR;
    serviceStatus.dwCheckPoint = 0;
    serviceStatus.dwWaitHint = 0;
    
    serviceStatusHandle = RegisterServiceCtrlHandler(g_serviceName.c_str(), ServiceControlHandler);
    
    if (serviceStatusHandle)
    {
        char path[_MAX_PATH + 1];
        unsigned int i, last_slash = 0;
        
        GetModuleFileName(0, path, sizeof(path) / sizeof(path[0]));
        
        for (i = 0; i < std::strlen(path); ++i)
        {
            if (path[i] == '\\') last_slash = i;
        }
        
        path[last_slash] = 0;
        
        // service is starting
        serviceStatus.dwCurrentState = SERVICE_START_PENDING;
        SetServiceStatus(serviceStatusHandle, &serviceStatus);
        
        // do initialisation here
        SetCurrentDirectory(path);
        
        // running
        serviceStatus.dwControlsAccepted |= (SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN);
        serviceStatus.dwCurrentState = SERVICE_RUNNING;
        SetServiceStatus(serviceStatusHandle, &serviceStatus);
        
        ////////////////////////
        // service main cycle //
        ////////////////////////
        
        g_ServiceStatus = 1;
        argc = 1;
        main(argc , argv);
        
        // service was stopped
        serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(serviceStatusHandle, &serviceStatus);
        
        // do cleanup here
        
        // service is now stopped
        serviceStatus.dwControlsAccepted &= ~(SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN);
        serviceStatus.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(serviceStatusHandle, &serviceStatus);
    }
}

bool WinServiceRun()
{
    SERVICE_TABLE_ENTRY serviceTable[] =
    {
        { (LPSTR)g_serviceName.c_str(), ServiceMain },
        { 0, 0 }
    };
    
    if (!StartServiceCtrlDispatcher(serviceTable))
    {
        Log.Error(__FUNCTION__, "StartService Failed. Error [%u]", ::GetLastError());
        return false;
    }
    return true;
}
#endif