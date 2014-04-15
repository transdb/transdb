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


#ifndef SOCKET_GARBAGE_COLLECTOR_H
#define SOCKET_GARBAGE_COLLECTOR_H

#include "SocketDefines.h"

/* Socket Garbage Collector */
#define SOCKET_GC_TIMEOUT 15

typedef unordered_map<Socket*, time_t> DeletionQueueMap;

class SocketGarbageCollector : public Singleton<SocketGarbageCollector>
{
private:
	volatile DeletionQueueMap	m_deletionQueue;
	Mutex						m_DelLock;

public:
	~SocketGarbageCollector()
	{
		LockingPtr<DeletionQueueMap> pDeletionQueue(m_deletionQueue, m_DelLock);
		for(DeletionQueueMap::iterator itr = pDeletionQueue->begin();itr != pDeletionQueue->end();++itr)
		{
			delete itr->first;
		}
	}

	void Update()
	{
		LockingPtr<DeletionQueueMap> pDeletionQueue(m_deletionQueue, m_DelLock);

		DeletionQueueMap::iterator i, i2;
		time_t t = UNIXTIME;
		for(i = pDeletionQueue->begin(); i != pDeletionQueue->end();)
		{
			i2 = i++;
			if(i2->second <= t)
			{
				delete i2->first;
				pDeletionQueue->erase(i2);
			}
		}
	}

	void QueueSocket(Socket * s)
	{
		LockingPtr<DeletionQueueMap> pDeletionQueue(m_deletionQueue, m_DelLock);
		pDeletionQueue->insert(DeletionQueueMap::value_type(s, UNIXTIME + SOCKET_GC_TIMEOUT));
	}
};

#define sSocketGarbageCollector SocketGarbageCollector::getSingleton()

#endif