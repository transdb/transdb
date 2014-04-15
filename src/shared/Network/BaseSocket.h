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


#ifndef BASE_SOCKET_H
#define BASE_SOCKET_H

#include "SocketDefines.h"

class BaseSocket
{
public:
	/** Returns the socket's file descriptor
	 */
	virtual SOCKET GetFd() = 0;
    virtual void SetFd(SOCKET fd) = 0;

	/** Is this socket in a read state? Or a write state?
	 */
	virtual bool Writable() = 0;

	/** Virtual ReadCallback() callback
	 */
	virtual void ReadCallback(size_t len) = 0;

	/** Virtual WriteCallback() callback
	 */
	virtual void WriteCallback(size_t len) = 0;
    
    /* */
    virtual void Accept(const sockaddr_in * peer) = 0;

	/** Virtual OnError() callback
	 */
	virtual void OnError(int errcode) = 0;

   	/** When we read data this is called
	 */
	virtual void OnRead() = 0;
	virtual void OnConnect() = 0;
	virtual void OnDisconnect() = 0;
    
	/** Are we connected?
	*/
	virtual bool IsConnected() = 0;
	virtual bool IsDeleted() = 0;

	/** Disconnects the socket
	 */
	virtual void Disconnect() = 0;

	/** Deletes the socket
	 */
	virtual void Delete() = 0;
	
	/** Locks the socket's write buffer so you can begin a write operation
	 */
	virtual void BurstBegin() = 0;
    
	/** Unlocks the socket's write buffer so others can write to it
	 */
	virtual void BurstEnd() = 0;
    
	/** Writes the specified data to the end of the socket's write buffer
	 */
	virtual bool BurstSend(const void * data, size_t bytes) = 0;
    
	/** Burst system - Pushes event to queue - do at the end of write events.
	 */
	virtual void BurstPush() = 0;
	
	// Posts a kevent with the specifed arguments.
	virtual void PostEvent(int events) = 0;
    
	// Atomic wrapper functions for increasing read/write locks
	virtual void IncSendLock() = 0;
	virtual void DecSendLock() = 0;
	virtual bool AcquireSendLock() = 0;
};

#endif