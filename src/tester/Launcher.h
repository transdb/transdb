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

#ifndef LAUNCHER_H
#define LAUNCHER_H

class InterClient;

class Launcher
{
public:
	Launcher()
	{
		m_loopCounter	= 0;
#ifdef WIN32		
		m_Handle		= CreateEvent(NULL, FALSE, FALSE, NULL);
#endif		
	}
	~Launcher()
	{
#ifdef WIN32		
		CloseHandle(m_Handle);
#endif		
	}

	void run();

	void HookSignals();
	void UnhookSignals();

private:
	static void OnSignal(int s);

#ifdef WIN32
	HANDLE m_Handle;
#endif
	uint32 m_loopCounter;
};

extern std::atomic<bool> g_stopEvent;

#endif