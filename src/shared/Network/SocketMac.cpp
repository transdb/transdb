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

#ifdef CONFIG_USE_KQUEUE

Socket::Socket(SOCKET fd, size_t readbuffersize, size_t writebuffersize) : m_readBuffer(readbuffersize), m_writeBuffer(writebuffersize)
{
	//set fd
	m_fd = fd;
    
	// Check for needed fd allocation.
	if(m_fd == 0)
	{
		m_fd = SocketOps::CreateTCPFileDescriptor();
	}
    
	/* switch the socket to nonblocking mode */
	SocketOps::Nonblocking(m_fd);
	
	/* disable nagle buffering by default */
	SocketOps::DisableBuffering(m_fd);	
    
	m_writeLock 	= 0;
	m_deleted 		= false;
	m_connected 	= false;
}

bool Socket::Connect(const char * Address, uint32 Port, uint32 timeout)
{
	struct hostent * ci = gethostbyname(Address);
	if(ci == NULL)
		return false;
    
	m_peer.sin_family = ci->h_addrtype;
	m_peer.sin_port = ntohs((u_short)Port);
    m_peer.sin_family = AF_INET;
	memcpy(&m_peer.sin_addr.s_addr, ci->h_addr_list[0], ci->h_length);

    //set non-blocking
    SocketOps::Nonblocking(m_fd);
        
    //try to connect
    int result = connect(m_fd, (const sockaddr*)&m_peer, sizeof(sockaddr_in));
    if(result == 0)
    {
        Log.Debug(__FUNCTION__, "result == 0");
        return false;        
    }
    
    //async kqueue socket connect
    bool oResult = false;
    int kq, count;
    struct kevent event;
    struct timespec rTimeout;
    rTimeout.tv_sec     = timeout;
    rTimeout.tv_nsec    = 0;    
    
    kq = kqueue();
    if(kq == -1)
    {
        Log.Debug(__FUNCTION__, "kqueue() == -1");
        return false;
    }
    
    EV_SET(&event, m_fd, EVFILT_WRITE, EV_ADD | EV_ONESHOT | EV_CLEAR, 0, 0, NULL);
    if(kevent(kq, &event, 1, NULL, 0, NULL) < 0)
    {
        Log.Debug(__FUNCTION__, "Could not post event on fd %u", m_fd);
        return false;
    }    
    
    count = kevent(kq, NULL, 0, &event, 1, &rTimeout);
    if(count == 1)
    {
        int so_error;
        socklen_t len = sizeof(so_error);
        getsockopt(m_fd, SOL_SOCKET, SO_ERROR, &so_error, &len);
        if(so_error == 0)
        {
            _OnConnect();
            oResult = true; 
        }
        else
        {
            Log.Debug(__FUNCTION__, "SO_ERROR: %d", so_error);
        }
    }
    
    close(kq);
    return oResult;
}

Socket::~Socket()
{
	
}

void Socket::Accept(const sockaddr_in * peer)
{
	memcpy(&m_peer, peer, sizeof(sockaddr));
	_OnConnect();	
}

void Socket::_OnConnect()
{
	m_connected = true;
	
	//add to socket mgr
	sSocketMgr.AddSocket(this, false);
	
	// Call virtual onconnect
	OnConnect();
}

/* This is called when the socket engine gets an event on the socket */
void Socket::ReadCallback(size_t len)
{
	//lock
	m_readMutex.Acquire();
	
	/* Any other platform, we have to call read() to actually get the data. */    
    size_t space = m_readBuffer.GetSpace();
    ssize_t bytes = recv(m_fd, m_readBuffer.GetBuffer(), space, 0);
    if(bytes <= 0)
    {
        m_readMutex.Release();
        Disconnect();
        return;
    }
    else if(bytes > 0)
    {
        m_readBuffer.IncrementWritten(bytes);
        OnRead();
    }
	
	m_readMutex.Release();
}

/* This is called when the socket engine gets an event on the socket */
void Socket::WriteCallback(size_t len)
{
	// We should already be locked at this point, so try to push everything out.    
    ssize_t bytes = send(m_fd, m_writeBuffer.GetBufferStart(), m_writeBuffer.GetContiguiousBytes(), 0);
    if(bytes < 0)
    {
        Disconnect();
        return;
    }
	
	m_writeBuffer.Remove(bytes);
}

bool Socket::BurstSend(const void * data, size_t bytes)
{
	return m_writeBuffer.Write(data, bytes);
}

void Socket::BurstPush()
{
	if(AcquireSendLock())
	{
        PostEvent(EVFILT_WRITE, true);
	}
}

void Socket::Disconnect()
{
	if(!m_connected) 
		return;
    
	m_connected = false;
    
	OnDisconnect();
	sSocketMgr.RemoveSocket(this);
	SocketOps::CloseSocket(m_fd);
    
	if(!m_deleted)
	{
		Delete();
	}
}

void Socket::Delete()
{
	if(m_deleted) 
		return;
    
	m_deleted = true;
    
	if(m_connected) 
	{
		Disconnect();	
	}
	
	sSocketGarbageCollector.QueueSocket(this);	
}

void Socket::OnError(int errcode)
{    
    Log.Debug(__FUNCTION__, "Error number: %u", errcode);
	Disconnect();
}

bool Socket::Writable()
{
	return (m_writeBuffer.GetSize() > 0) ? true : false;
}

string Socket::GetRemoteIP()
{
	char* ip = (char*)inet_ntoa(m_peer.sin_addr);
	if(ip != NULL)
		return string(ip);
	else
		return string("noip");
}

void Socket::PostEvent(int events, bool oneshot)
{
    int kq = sSocketMgr.GetKqFd();
    
    struct kevent ev;
    memset(&ev, 0, sizeof(ev));
    
    if(oneshot)
    {
        EV_SET(&ev, m_fd, events, EV_ADD | EV_ONESHOT, 0, 0, this);
    }
    else
    {
        EV_SET(&ev, m_fd, events, EV_ADD, 0, 0, this);
    }
    
    if(kevent(kq, &ev, 1, 0, 0, NULL) < 0)
    {
        Log.Warning(__FUNCTION__, "Could not modify event for fd %u", m_fd);
    }
}

#endif