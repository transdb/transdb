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
	/** Virtual destructor
	 */
	virtual ~BaseSocket() {}

	/** Returns the socket's file descriptor
	 */
	INLINE int GetFd()						{ return m_fd; }

	/** Is this socket in a read state? Or a write state?
	 */
	virtual bool Writable() = 0;

	/** Virtual ReadCallback() callback
	 */
	virtual void ReadCallback(size_t len) = 0;

	/** Virtual WriteCallback() callback
	 */
	virtual void WriteCallback(size_t len) = 0;

	/** Virtual OnError() callback
	 */
	virtual void OnError(int errcode) = 0;

	/** This is a windows-only implementation
	 */
	virtual void OnAccept(void * pointer) = 0;

	/** Are we connected?
	*/
	INLINE bool IsConnected()				{ return m_connected; }
	INLINE bool IsDeleted()					{ return m_deleted; }

	/** Disconnects the socket
	 */
	virtual void Disconnect() = 0;

	/** Deletes the socket
	 */
	virtual void Delete() = 0;
	
	/** Locks the socket's write buffer so you can begin a write operation
	 */
	virtual void BurstBegin() {}
	
	/** Unlocks the socket's write buffer so others can write to it
	 */
	virtual void BurstEnd() {}
	
	// Posts a kevent with the specifed arguments.
	virtual void PostEvent(int events, bool oneshot) {}
    
	// Atomic wrapper functions for increasing read/write locks
	virtual void IncSendLock() {}
	virtual void DecSendLock() {}
	virtual bool AcquireSendLock() 	{ return false; }

protected:
	/** This socket's file descriptor
	 */
	SOCKET			m_fd;

	/** deleted/disconnected markers
	 */
	volatile bool	m_deleted;
	volatile bool	m_connected;
};

#endif