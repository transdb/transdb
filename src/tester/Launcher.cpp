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

#include "main.h"

std::atomic<bool> g_stopEvent(false);

void Launcher::HookSignals()
{
	signal(SIGINT, &Launcher::OnSignal);
	signal(SIGTERM, &Launcher::OnSignal);
	signal(SIGABRT, &Launcher::OnSignal);
#ifdef WIN32
	signal(SIGBREAK, &Launcher::OnSignal);
#endif
}

void Launcher::UnhookSignals()
{
	signal(SIGINT, NULL);
	signal(SIGTERM, NULL);
	signal(SIGABRT, NULL);
#ifdef WIN32
	signal(SIGBREAK, NULL);
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
            g_stopEvent = true;
            break;
	}
    
	signal(s, OnSignal);
}

void Launcher::run()
{
	time_t curTime = 0;

	while(g_stopEvent == false)
	{
		//check every 5mins
		if(!((++m_loopCounter) % 10000))
		{
//			ThreadPool.ShowStats();
			ThreadPool.IntegrityCheck();
		}

		/* since time() is an expensive system call, we only update it once per server loop */
		curTime = time(NULL);
		if(UNIXTIME != curTime)
		{
			UNIXTIME = curTime;
			localtime(&curTime, &g_localTime);
		}

		//update socket collector
		sSocketGarbageCollector.Update();

		//wait
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}

	Log.Notice(__FUNCTION__, "Server shutdown in progress.");
}