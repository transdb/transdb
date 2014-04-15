//
//  ServiceWin32.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 9/10/12.
//  Copyright (c) 2012 Miroslav Kudrnac. All rights reserved.
//

#ifndef __TransDB__ServiceWin32__
#define __TransDB__ServiceWin32__

#ifdef WIN32
#ifndef _WIN32_SERVICE_
#define _WIN32_SERVICE_

bool WinServiceInstall();
bool WinServiceUninstall();
bool WinServiceRun();

extern volatile long g_ServiceStatus;

#endif                                                      // _WIN32_SERVICE_
#endif                                                      // WIN32

#endif /* defined(__TransDB__ServiceWin32__) */
