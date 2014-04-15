/*
 * Game Server
 * Copyright (C) 2010 Miroslav 'Wayland' Kudrnac
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "StdAfx.h"

volatile long g_stopEvent = 0;

void Launcher::HookSignals()
{
	signal(SIGINT, Launcher::OnSignal);
	signal(SIGTERM, Launcher::OnSignal);
	signal(SIGABRT, Launcher::OnSignal);
#ifdef WIN32	
	signal(SIGBREAK, Launcher::OnSignal);
#endif	
}

void Launcher::UnhookSignals()
{
	signal(SIGINT, 0);
	signal(SIGTERM, 0);
	signal(SIGABRT, 0);
#ifdef WIN32
	signal(SIGBREAK, 0);
#endif	
}

void Launcher::OnSignal(int s)
{
	switch(s)
	{
	case SIGINT:
	case SIGTERM:
	case SIGABRT:
#ifdef WIN32	
	case SIGBREAK:
#endif	
		Sync_Add(&g_stopEvent);
		break;
	}

	signal(s, OnSignal);
}

void Launcher::run()
{
	time_t curTime;
	while(g_stopEvent == 0)
	{
#ifdef WIN32
		if(g_ServiceStatus == 0)
			break;
#endif

		//check every 5mins
		if(!((++m_loopCounter) % 10000))
		{
#ifdef DEBUG
			ThreadPool.ShowStats();
#endif
			ThreadPool.IntegrityCheck();
		}

		/* since time() is an expensive system call, we only update it once per server loop */
		curTime = time(NULL);
		if(UNIXTIME != curTime)
		{
			UNIXTIME = curTime;
			localtime(&curTime, &g_localTime);
		}

        //update sockets
        g_rClientSocketHolder.Update();
        
		//update socket collector
		sSocketGarbageCollector.Update();

		//wait
#ifdef WIN32		
		WaitForSingleObject(m_Handle, 50);
#else
		Sleep(50);
#endif
	}

	Log.Notice(__FUNCTION__, "Server shutdown in progress.");
}