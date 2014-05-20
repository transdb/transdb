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


#ifndef SOCKETMGR_LINUX_H
#define SOCKETMGR_LINUX_H

#include "SocketDefines.h"
#ifdef CONFIG_USE_EPOLL

#define THREAD_EVENT_SIZE 4096      // This is the number of socket events each thread can receieve at once.
                                    // This default value should be more than enough.

class Socket;
class SocketWorkerThread;
class ListenSocketBase;

typedef unordered_set<BaseSocket*>			SocketSet;

class SocketMgr : public Singleton<SocketMgr>
{
private:
    /// /dev/epoll instance handle
    int 				m_epoll_fd;

    // fd -> pointer binding.
	volatile SocketSet	m_sockets;
	Mutex				m_socketLock;

public:

    /// friend class of the worker thread -> it has to access our private resources
    friend class SocketWorkerThread;

	/// constructor > create epoll device handle
	SocketMgr()
	{
		m_epoll_fd = epoll_create(THREAD_EVENT_SIZE);
		if(m_epoll_fd == -1)
		{
			printf("Could not create epoll fd (/dev/epoll).");
			exit(-1);
		}
	}
	
    /// destructor > destroy epoll handle
    ~SocketMgr()
    {
        // close epoll handle
        close(m_epoll_fd);
    }	

    /// add a new socket to the epoll set and to the fd mapping
    void AddSocket(BaseSocket * pSocket);

    /// remove a socket from epoll set/fd mapping
    void RemoveSocket(BaseSocket * pSocket);
	
	//This is called when a socket has data to write for the first time.
	void WantWrite(BaseSocket * pSocket);			
	
	//
	void WantRead(BaseSocket * pSocket);	

    /// closes all sockets
    void CloseAll();

    /// spawns worker threads
    void SpawnWorkerThreads(uint32 count);
		
	//epoll fd
	int GetEpollFd()	{ return m_epoll_fd; }
};

class SocketWorkerThread : public ThreadContext
{
private:
    /// epoll event struct
    struct epoll_event 	m_events[THREAD_EVENT_SIZE];
	
public:
    bool run();
};

#define sSocketMgr SocketMgr::getSingleton()

#endif

#endif