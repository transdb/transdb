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

#ifdef CONFIG_USE_EPOLL

initialiseSingleton(SocketMgr);

void SocketMgr::AddSocket(BaseSocket * pSocket, bool listenSocket)
{
	//add socket to storage
	LockingPtr<SocketSet> pSockets(m_sockets, m_socketLock);
	if(pSockets->find(pSocket) == pSockets->end())
	{
		pSockets->insert(pSocket);

		// Add epoll event based on socket activity.
		struct epoll_event ev;
		memset(&ev, 0, sizeof(epoll_event));
		ev.data.ptr = pSocket;
		ev.events 	= (pSocket->Writable()) ? EPOLLOUT : EPOLLIN;
	
		if(epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, pSocket->GetFd(), &ev))
		{
			Log.Warning(__FUNCTION__, "Could not add event to epoll set on fd %u", pSocket->GetFd());
		}
	}
}

void SocketMgr::RemoveSocket(BaseSocket * pSocket)
{
	//remove socket from storage
	LockingPtr<SocketSet> pSockets(m_sockets, m_socketLock);	
	SocketSet::iterator itr = pSockets->find(pSocket);
	if(itr != pSockets->end())
	{
		pSockets->erase(itr);
	
		// Remove from epoll list.
		struct epoll_event ev;
		memset(&ev, 0, sizeof(epoll_event));
		ev.data.ptr = pSocket;
		ev.events 	= (pSocket->Writable()) ? EPOLLOUT : EPOLLIN;

		if(epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, pSocket->GetFd(), &ev))
		{
			Log.Warning(__FUNCTION__, "Could not remove fd %u from epoll set, errno %u", pSocket->GetFd(), errno);
		}
	}
}

void SocketMgr::CloseAll()
{
	LockingPtr<SocketSet> pSockets(m_sockets, NULL);
	std::list<BaseSocket*> tokill;
	
	m_socketLock.Acquire();
	for(SocketSet::iterator itr = pSockets->begin();itr != pSockets->end();++itr)
	{
		tokill.push_back(*itr);
	}
	m_socketLock.Release();
	
	for(std::list<BaseSocket*>::iterator itr = tokill.begin(); itr != tokill.end(); ++itr)
	{
		(*itr)->Disconnect();
	}	
	
	size_t size = 0;
	do
	{
		m_socketLock.Acquire();
		size = pSockets->size();
		m_socketLock.Release();
	}while(size);
}

void SocketMgr::SpawnWorkerThreads()
{
    ThreadPool.ExecuteTask(new SocketWorkerThread());
}

SocketWorkerThread::SocketWorkerThread()
{
    long numOfEvents = sSocketMgr.GetEventSize();
    m_pEvents = new struct epoll_event[numOfEvents];
}

SocketWorkerThread::~SocketWorkerThread()
{
    delete [] m_pEvents;
}

bool SocketWorkerThread::run()
{
	int i;
    int fd_count;
    Socket * pSocket;
	int epoll_fd = sSocketMgr.GetEpollFd();
	int maxevents = sSocketMgr.GetEventSize();
			
    while(m_threadRunning)
    {
        fd_count = epoll_wait(epoll_fd, m_pEvents, maxevents, 10000);
        for(i = 0; i < fd_count; ++i)
        {
			pSocket = static_cast<Socket*>(m_pEvents[i].data.ptr);
			if(pSocket == NULL)
			{
				Log.Error(__FUNCTION__, "epoll returned invalid fd %u", m_pEvents[i].data.fd);
				continue;
			}
			
			if((m_pEvents[i].events & EPOLLHUP) || (m_pEvents[i].events & EPOLLERR))
			{
				pSocket->OnError(errno);
			}
			else if(m_pEvents[i].events & EPOLLIN)
			{
				/* Len is unknown at this point. */
				pSocket->ReadCallback(0);
				
			}
			else if(m_pEvents[i].events & EPOLLOUT)
			{
				pSocket->BurstBegin();						//lock
				pSocket->WriteCallback(0);					//perform send
                if(pSocket->IsConnected())
                {
                    if(pSocket->Writable())
                    {
                        pSocket->PostEvent(EPOLLOUT);       // Still remaining data.
                    }
                    else
                    {
                        pSocket->DecSendLock();
                        pSocket->PostEvent(EPOLLIN);
                    }
                }
				pSocket->BurstEnd(); 						//Unlock
			}
        }       
    }
    return true;
}

#endif
