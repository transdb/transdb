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

#ifndef GUARD_H
#define GUARD_H

#include "Threading.h"

template <typename T>
class LockingPtr
{
public:
	LockingPtr(volatile T& obj, std::mutex& rMutex) : m_obj(const_cast<T*>(&obj)), m_pMutex(&rMutex)
	{
		m_pMutex->lock();
	}

	LockingPtr(volatile T& obj, std::mutex* pMutex) : m_obj(const_cast<T*>(&obj)), m_pMutex(pMutex)
	{
		
	}

	~LockingPtr()
	{
		if(m_pMutex)
		{
			m_pMutex->unlock();
		}
	}

   T& operator*()
   {    
	   return *m_obj;    
   }

   T* operator->()
   {   
	   return m_obj;   
   }

private:
	T*				m_obj;
    std::mutex*		m_pMutex;
	LockingPtr(const LockingPtr&);
	LockingPtr& operator=(const LockingPtr&);
};

#endif

