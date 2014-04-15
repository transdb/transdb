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

#ifndef _LISTENSOCKET_H
#define _LISTENSOCKET_H

template<class T>
class ListenSocket : public BaseSocket
{
private:
	SOCKET			m_new_fd;
	sockaddr_in 	m_new_peer;
	sockaddr_in 	m_address;
	socklen_t		m_len;

public:
	ListenSocket(const char * hostname, u_short port)
	{
		m_fd = socket(AF_INET, SOCK_STREAM, 0);
		SocketOps::ReuseAddr(m_fd);
		if(m_fd < 0)
		{
			printf("ListenSocket constructor: could not create socket() %u (%s)\n", errno, strerror(errno));
			return;
		}	
		
		if(!strcmp(hostname, "0.0.0.0"))
		{
			m_address.sin_addr.s_addr = htonl(INADDR_ANY);
		}
		else
		{
			hostent * h = gethostbyname(hostname);
			if(!h)
			{
				printf("Could not resolve listen address\n");
				return;
			}

			memcpy(&m_address.sin_addr, h->h_addr_list[0], sizeof(in_addr));
		}

		m_address.sin_family = AF_INET;
		m_address.sin_port = ntohs(port);

		if(bind(m_fd, (const sockaddr*)&m_address, sizeof(sockaddr_in)) < 0)
		{
			printf("Could not bind\n");
			return;
		}

		if(listen(m_fd, SOMAXCONN) < 0)
		{
			printf("Could not bind\n");
			return;
		}

		m_len 		= sizeof(sockaddr_in);
		m_connected = true;
		m_deleted	= false;
		
		// add to mgr
		sSocketMgr.AddSocket(this, true);
	}

	~ListenSocket()
	{
		Close();
	}

	void ReadCallback(size_t len)
	{
		if(!m_connected)
			return;

		m_new_fd = accept(m_fd, (sockaddr*)&m_new_peer, &m_len);
		if(m_new_fd > 0)
		{
			T * s = new T(m_new_fd);
			s->Accept(&m_new_peer);
		}
	}

	void OnError(int err) {}
	void OnAccept(void * pointer) {}
	void WriteCallback(size_t len) {}	
	
	bool IsOpen() 		{ return m_connected; }	
	bool Writable() 	{ return false; }

	void Delete()
	{
		Close();
	}

	void Disconnect()
	{
		Close();
	}

	void Close()
	{
		// prevent a race condition here.
		bool mo = m_connected;
		m_connected = false;
		m_deleted = true;

		//remove from socketmgr
		sSocketMgr.RemoveSocket(this);

		if(mo)
		{
			shutdown(m_fd, SD_BOTH);
			SocketOps::CloseSocket(m_fd);
		}
	}
};

#endif

