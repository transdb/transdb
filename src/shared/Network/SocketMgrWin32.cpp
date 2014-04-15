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


#include "Network.h"

#ifdef CONFIG_USE_IOCP

initialiseSingleton(SocketMgr);

SocketMgr::SocketMgr()
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2,2), &wsaData);

	m_completionPort	= CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, (ULONG_PTR)0, 0);
	m_threadcount		= 0;
}

SocketMgr::~SocketMgr()
{
	CloseHandle(m_completionPort);
}

void SocketMgr::SpawnWorkerThreads(uint32 count)
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	if(count == 0)
		m_threadcount = si.dwNumberOfProcessors;
	else
		m_threadcount = count;

	Log.Notice(__FUNCTION__, "Spawning %u worker threads.", m_threadcount);
	for(long x = 0; x < m_threadcount; ++x)
	{
		ThreadPool.ExecuteTask(new SocketWorkerThread());
	}
}

bool SocketWorkerThread::run()
{
	HANDLE cp = sSocketMgr.GetCompletionPort();
	DWORD len;
	Socket * s;
	OverlappedStruct * ov;
	LPOVERLAPPED ol_ptr;

	for(;;)
	{
		if(!GetQueuedCompletionStatus(cp, &len, (PULONG_PTR)&s, &ol_ptr, INFINITE))
			continue;

		ov = CONTAINING_RECORD(ol_ptr, OverlappedStruct, m_overlap);

		if(ov->m_event == SOCKET_IO_THREAD_SHUTDOWN)
		{
			delete ov;
			return true;
		}

		if(ov->m_event < NUM_SOCKET_IO_EVENTS)
		{
			g_ophandlers[ov->m_event](s, static_cast<size_t>(len));
		}
	}

	return true;
}

void HandleReadComplete(Socket * s, size_t len)
{
	if(!s->IsDeleted())
	{
		s->m_readEvent.Unmark();
		if(len)
		{
			s->GetReadBuffer().IncrementWritten(len);
			s->OnRead();
			s->SetupReadEvent();
		}
		else
			s->Delete();	  // Queue deletion.
	}
}

void HandleWriteComplete(Socket * s, size_t len)
{
	if(!s->IsDeleted())
	{
		s->m_writeEvent.Unmark();
		s->BurstBegin();					// Lock
		s->GetWriteBuffer().Remove(len);
		if(s->GetWriteBuffer().GetContiguiousBytes() > 0)
			s->WriteCallback();
		else
			s->DecSendLock();
		s->BurstEnd();					  // Unlock
	}
}

void SocketMgr::CloseAll()
{
	list<Socket*> tokill;

	m_socketLock.Acquire();
	for(SocketSet::iterator itr = m_sockets.begin(); itr != m_sockets.end(); ++itr)
	{
		tokill.push_back(*itr);
	}
	m_socketLock.Release();
	
	for(list<Socket*>::iterator itr = tokill.begin(); itr != tokill.end(); ++itr)
	{
		(*itr)->Disconnect();
	}

	size_t size = 0;
	do
	{
		m_socketLock.Acquire();
		size = m_sockets.size();
		m_socketLock.Release();
	}while(size);
}

void SocketMgr::ShutdownThreads()
{
	for(long i = 0; i < m_threadcount; ++i)
	{
		OverlappedStruct * ov = new OverlappedStruct(SOCKET_IO_THREAD_SHUTDOWN);
		PostQueuedCompletionStatus(m_completionPort, 0, (ULONG_PTR)0, &ov->m_overlap);
	}
}

void SocketMgr::ShowStatus()
{
	m_socketLock.Acquire();
	//sLog.outString("_sockets.size(): %d", m_sockets.size());
	m_socketLock.Release();
}

#endif