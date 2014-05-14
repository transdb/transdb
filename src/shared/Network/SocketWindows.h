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


#ifndef WINDOWS_SOCKET_H
#define WINDOWS_SOCKET_H

#include "SocketDefines.h"

class Socket
{
public:
	// Constructor. If fd = 0, it will be assigned 
	Socket(SOCKET fd, uint32 sendbuffersize, uint32 recvbuffersize);
	
	// Destructor.
	virtual ~Socket();

	// Open a connection to another machine.
	bool Connect(const char * Address, uint32 Port, uint32 timeout);

	// Disconnect the socket.
	void Disconnect();

	// Accept from the already-set fd.
	void Accept(sockaddr_in * address);

/* Implementable methods */

	// Called when data is received.
	virtual void OnRead() {}

	// Called when a connection is first successfully established.
	virtual void OnConnect() {}

	// Called when the socket is disconnected from the client (either forcibly or by the connection dropping)
	virtual void OnDisconnect() {}

/* Sending Operations */

	// Locks sending mutex, adds bytes, unlocks mutex.
	bool Send(const uint8 * Bytes, size_t Size);

	// Burst system - Locks the sending mutex.
	void BurstBegin() { m_writeMutex.Acquire(); }

	// Burst system - Adds bytes to output buffer.
	bool BurstSend(const uint8 * Bytes, size_t Size);

	// Burst system - Pushes event to queue - do at the end of write events.
	void BurstPush();

	// Burst system - Unlocks the sending mutex.
	void BurstEnd() { m_writeMutex.Release(); }

/* Client Operations */

	// Get the client's ip in numerical form.
	string GetRemoteIP();
	uint32 GetRemotePort()	{ return ntohs(m_client.sin_port); }
	SOCKET GetFd()			{ return m_fd; }
	
/* Platform-specific methods */

	void SetupReadEvent();
	void ReadCallback(size_t len);
	void WriteCallback();

	bool IsDeleted() const				{ return m_deleted; }
	bool IsConnected() const			{ return m_connected; }
	sockaddr_in & GetRemoteStruct()		{ return m_client; }
	CircularBuffer& GetReadBuffer()		{ return m_readBuffer; }
	CircularBuffer& GetWriteBuffer()	{ return m_writeBuffer; }

/* Deletion */
	void Delete();

	in_addr GetRemoteAddress() const	{ return m_client.sin_addr; }

protected:

	// Called when connection is opened.
	void _OnConnect();
  
	SOCKET              m_fd;

	CircularBuffer      m_readBuffer;
	CircularBuffer      m_writeBuffer;

	Mutex               m_writeMutex;
	Mutex               m_readMutex;

	// we are connected? stop from posting events.
    std::atomic<bool>   m_connected;

    // We are deleted? Stop us from posting events.
    std::atomic<bool>   m_deleted;

	sockaddr_in         m_client;

public:
	// Set completion port that this socket will be assigned to.
	void SetCompletionPort(HANDLE cp) { m_completionPort = cp; }
	
	// Atomic wrapper functions for increasing read/write locks
	void IncSendLock() { InterlockedIncrement(&m_writeLock); }
	void DecSendLock() { InterlockedDecrement(&m_writeLock); }
	bool AcquireSendLock()
	{
		if(m_writeLock)
			return false;
		else
		{
			IncSendLock();
			return true;
		}
	}

	OverlappedStruct    m_readEvent;
	OverlappedStruct    m_writeEvent;

private:
	// Completion port socket is assigned to
	HANDLE              m_completionPort;
	
	// Write lock, stops multiple write events from being posted.
	std::atomic<long>   m_writeLock;
	
	// Assigns the socket to his completion port.
	void AssignToCompletionPort();
};

#endif
