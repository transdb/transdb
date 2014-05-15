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

Socket::Socket(SOCKET fd, uint32 sendbuffersize, uint32 recvbuffersize) : m_fd(fd), m_connected(false),	m_deleted(false), m_readBuffer(recvbuffersize), m_writeBuffer(sendbuffersize)
{
	// IOCP Member Variables
	m_writeLock = 0;
	m_completionPort = 0;

	// Check for needed fd allocation.
	if(m_fd == 0)
	{
		m_fd = SocketOps::CreateTCPFileDescriptor();
	}
}

Socket::~Socket()
{

}

bool Socket::Connect(const char * Address, uint32 Port, uint32 timeout)
{
	struct hostent * ci = gethostbyname(Address);
	if(ci == NULL)
		return false;

	m_client.sin_family = ci->h_addrtype;
	m_client.sin_port = ntohs((u_short)Port);
	memcpy(&m_client.sin_addr.s_addr, ci->h_addr_list[0], ci->h_length);

	/* switch the socket to blocking mode */
	SocketOps::Blocking(m_fd);

	/* try to connect */
	int ret = connect(m_fd, (const sockaddr*)&m_client, sizeof(m_client));
	if(ret == -1)
		return false;

	// at this point the connection was established
	m_completionPort = sSocketMgr.GetCompletionPort();

	_OnConnect();
	return true;
}

void Socket::Accept(sockaddr_in * address)
{
	memcpy(&m_client, address, sizeof(*address));
	_OnConnect();
}

void Socket::_OnConnect()
{
	// set common parameters on the file descriptor
	SocketOps::Nonblocking(m_fd);
	SocketOps::DisableBuffering(m_fd);
	m_connected = true;

	// IOCP stuff
	AssignToCompletionPort();
	SetupReadEvent();

	sSocketMgr.AddSocket(this);

	// Call virtual onconnect
	OnConnect();
}

bool Socket::Send(const uint8 * Bytes, size_t Size)
{
	bool rv;

	// This is really just a wrapper for all the burst stuff.
	BurstBegin();
	rv = BurstSend(Bytes, Size);
	if(rv)
		BurstPush();
	BurstEnd();

	return rv;
}

bool Socket::BurstSend(const uint8 * Bytes, size_t Size)
{
	return m_writeBuffer.Write(Bytes, Size);
}

string Socket::GetRemoteIP()
{
	char* ip = (char*)inet_ntoa(m_client.sin_addr);
	if(ip != NULL)
		return string(ip);
	else
		return string("noip");
}

void Socket::Disconnect()
{
	m_connected = false;

	// remove from mgr
	sSocketMgr.RemoveSocket(this);

	SocketOps::CloseSocket(m_fd);

	// Call virtual ondisconnect
	OnDisconnect();

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

void Socket::WriteCallback()
{
	if(m_deleted || !m_connected)
		return;

	// We don't want any writes going on while this is happening.
	std::lock_guard<std::mutex> rGuard(m_writeMutex);
	if(m_writeBuffer.GetContiguiousBytes())
	{
		DWORD w_length = 0;
		DWORD flags = 0;

		// attempt to push all the data out in a non-blocking fashion.
		WSABUF buf;
		buf.len = (u_long)m_writeBuffer.GetContiguiousBytes();
		buf.buf = (char*)m_writeBuffer.GetBufferStart();

		m_writeEvent.Mark();
		m_writeEvent.Reset(SOCKET_IO_EVENT_WRITE_END);

		int r = WSASend(m_fd, &buf, 1, &w_length, flags, &m_writeEvent.m_overlap, NULL);
		if(r == SOCKET_ERROR)
		{
			int errNumber = WSAGetLastError();
			if(errNumber != WSA_IO_PENDING)
			{
				m_writeEvent.Unmark();
				DecSendLock();
				Disconnect();
			}
		}
	}
	else
	{
		// Write operation is completed.
		DecSendLock();
	}
}

void Socket::SetupReadEvent()
{
	if(m_deleted || !m_connected)
		return;

	std::lock_guard<std::mutex> rGuard(m_readMutex);
	DWORD r_length = 0;
	DWORD flags = 0;
	WSABUF buf;
	buf.len = (u_long)m_readBuffer.GetSpace();
	buf.buf = (char*)m_readBuffer.GetBuffer();	

	m_readEvent.Mark();
	m_readEvent.Reset(SOCKET_IO_EVENT_READ_COMPLETE);

	int r = WSARecv(m_fd, &buf, 1, &r_length, &flags, &m_readEvent.m_overlap, NULL);
	if(r == SOCKET_ERROR)
	{
		int errNumber = WSAGetLastError();
		if(errNumber != WSA_IO_PENDING)
		{
			m_readEvent.Unmark();
			Disconnect();
		}
	}
}

void Socket::ReadCallback(size_t len)
{
	m_readBuffer.IncrementWritten(len);
	OnRead();
	SetupReadEvent();
}

void Socket::AssignToCompletionPort()
{
	//add to existing completionPort
	CreateIoCompletionPort((HANDLE)m_fd, m_completionPort, (ULONG_PTR)this, 0);
}

void Socket::BurstPush()
{
	if(AcquireSendLock())
	{
		WriteCallback();
	}
}

#endif