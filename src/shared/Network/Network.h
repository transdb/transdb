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


#ifndef NETWORK_H
#define NETWORK_H

#include "../Logs/Log.h"
#include "CircularBuffer.h"
#include "SocketDefines.h"
#include "SocketOps.h"

#ifdef CONFIG_USE_IOCP
	#include "SocketWindows.h"
	#include "SocketMgrWin32.h"
	#include "ListenSocketWin32.h"
#endif

#ifdef CONFIG_USE_EPOLL
	#include "BaseSocket.h"
	#include "SocketLinux.h"
	#include "SocketMgrLinux.h"
	#include "ListenSocketLinux.h"
#endif

#ifdef CONFIG_USE_KQUEUE
    #include "BaseSocket.h"
    #include "SocketMac.h"
    #include "SocketMgrMac.h"
    #include "ListenSocketLinux.h"
#endif

#include "SocketGarbageCollector.h"

/** Connect to a server.
* @param hostname Hostname or IP address to connect to
* @param port Port to connect to
* @return templated type if successful, otherwise null
*/
template<class T>
T* ConnectTCPSocket(const char * hostname, u_short port, uint32 timeout = 3)
{
	T * s = new T(0);
	if(!s->Connect(hostname, port, timeout))
	{
		s->Delete();
		return NULL;
	}
	return s;	
}

#endif
