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


#ifndef SOCKETMGR_MAC_H
#define SOCKETMGR_MAC_H

#include "SocketDefines.h"
#ifdef CONFIG_USE_KQUEUE

#define THREAD_EVENT_SIZE   4

class Socket;
class SocketWorkerThread;
class ListenSocketBase;

typedef std::set<BaseSocket*>			SocketSet;

class SocketMgr : public Singleton<SocketMgr>
{
private:
    // kqueue instance handle
    int 				m_kq_fd;
    int                 m_eventSize;
    
    // fd -> pointer binding.
	volatile SocketSet	m_sockets;
    std::mutex          m_socketLock;
    
public:
    
    /// friend class of the worker thread -> it has to access our private resources
    friend class SocketWorkerThread;
    
	/// constructor > create epoll device handle
	SocketMgr()
	{
        m_eventSize     = 0;
		m_kq_fd         = kqueue();
		if(m_kq_fd == -1)
		{
			printf("Could not create kqueue fd.");
			exit(-1);
		}
	}
	
    /// destructor > destroy epoll handle
    ~SocketMgr()
    {
        // close epoll handle
        close(m_kq_fd);
    }
    
    /// add a new socket to the epoll set and to the fd mapping
    void AddSocket(BaseSocket * pSocket, bool listenSocket);
    
    /// remove a socket from epoll set/fd mapping
    void RemoveSocket(BaseSocket * pSocket);
    
    /// closes all sockets
    void CloseAll();
    
    /// spawns worker threads
    void SpawnWorkerThreads(uint32 count);
    
	//epoll fd
	int GetKqFd()           { return m_kq_fd; }
    int GetEventSize()      { return m_eventSize; }
};

class SocketWorkerThread : public ThreadContext
{
public:
    SocketWorkerThread();
    ~SocketWorkerThread();
    
    bool run();
    
private:
    struct kevent 	*m_pEvents;
};

#define sSocketMgr SocketMgr::getSingleton()


#endif

#endif
