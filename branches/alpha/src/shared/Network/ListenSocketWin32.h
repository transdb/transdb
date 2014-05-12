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


#ifndef LISTEN_SOCKET_WIN32_H
#define LISTEN_SOCKET_WIN32_H

#ifdef CONFIG_USE_IOCP

#include "../Threading/ThreadPool.h"

template<class T>
class ListenSocket : public ThreadContext
{
public:
	ListenSocket(const char * ListenAddress, uint32 Port)
	{
		m_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

		//socket settings
		SocketOps::ReuseAddr(m_socket);
		SocketOps::Blocking(m_socket);
		SocketOps::SetTimeout(m_socket, 60);

		m_address.sin_family		= AF_INET;
		m_address.sin_port			= ntohs((u_short)Port);
		m_address.sin_addr.s_addr	= htonl(INADDR_ANY);
		m_opened					= false;

		if(strcmp(ListenAddress, "0.0.0.0") != 0)
		{
			struct hostent * hostname = gethostbyname(ListenAddress);
			if(hostname != 0)
				memcpy(&m_address.sin_addr.s_addr, hostname->h_addr_list[0], hostname->h_length);
		}

		// bind.. well attempt to.
		int ret = bind(m_socket, (const sockaddr*)&m_address, sizeof(m_address));
		if(ret != 0)
		{
			printf("Bind unsuccessful on port %u.", Port);
			return;
		}

		ret = listen(m_socket, SOMAXCONN);
		if(ret != 0) 
		{
			printf("Unable to listen on port %u.", Port);
			return;
		}

		m_opened	= true;
		m_len		= sizeof(sockaddr_in);
		m_cp		= sSocketMgr.GetCompletionPort();
	}

	~ListenSocket()
	{
		Close();	
	}

	bool run()
	{
		while(m_opened)
		{
			aSocket = WSAAccept(m_socket, (sockaddr*)&m_tempAddress, (socklen_t*)&m_len, NULL, NULL);
			if(aSocket == INVALID_SOCKET)
				continue;

			socket = new T(aSocket);
			socket->SetCompletionPort(m_cp);
			socket->Accept(&m_tempAddress);
		}
		return true;
	}

	void Close()
	{
		// prevent a race condition here.
		bool mo = m_opened;
		m_opened = false;

		if(mo)
			SocketOps::CloseSocket(m_socket);
	}

	bool IsOpen() { return m_opened; }

private:
	SOCKET				m_socket;
	struct sockaddr_in	m_address;
	struct sockaddr_in	m_tempAddress;
	volatile bool		m_opened;
	uint32				m_len;
	SOCKET				aSocket;
	T *					socket;
	HANDLE				m_cp;
};

#endif
#endif