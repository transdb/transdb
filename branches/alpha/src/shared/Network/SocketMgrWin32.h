/*
 * Game server
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

#ifndef SOCKETMGR_H_WIN32
#define SOCKETMGR_H_WIN32

#ifdef CONFIG_USE_IOCP

class Socket;

typedef unordered_set<Socket*> SocketSet;

class SocketMgr : public Singleton<SocketMgr>
{
public:
	SocketMgr();
	~SocketMgr();

	HANDLE GetCompletionPort() { return m_completionPort; }
	void SpawnWorkerThreads(uint32 count);
	void CloseAll();
	void ShowStatus();
	void AddSocket(Socket * s)
	{
		m_socketLock.Acquire();
		m_sockets.insert(s);
		m_socketLock.Release();
	}

	void RemoveSocket(Socket * s)
	{
		m_socketLock.Acquire();
		m_sockets.erase(s);
		m_socketLock.Release();
	}

	void ShutdownThreads();

private:
	HANDLE			m_completionPort;
	SocketSet		m_sockets;
	Mutex			m_socketLock;
	volatile long	m_threadcount;
};

#define sSocketMgr SocketMgr::getSingleton()

typedef void(*OperationHandler)(Socket * s, size_t len);

class SocketWorkerThread : public ThreadContext
{
public:
	bool run();
};

void HandleReadComplete(Socket * s, size_t len);
void HandleWriteComplete(Socket * s, size_t len);

static const OperationHandler g_ophandlers[NUM_SOCKET_IO_EVENTS] =
{
	&HandleReadComplete,
	&HandleWriteComplete,
	nullptr,
};

#endif
#endif