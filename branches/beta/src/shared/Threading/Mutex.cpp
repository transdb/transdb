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

#include "Threading.h"

#ifndef WIN32
	/* Linux mutex implementation */
	bool Mutex::attr_initalized = false;
	pthread_mutexattr_t Mutex::attr;	
#endif

#ifdef MAC
    #define recursive_mutex_flag PTHREAD_MUTEX_RECURSIVE
#else
    #define recursive_mutex_flag PTHREAD_MUTEX_RECURSIVE_NP
#endif

Mutex::Mutex()
{
#ifdef WIN32	
	InitializeCriticalSection(&m_cs);
#else
	if(!attr_initalized)
	{
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, recursive_mutex_flag);
		attr_initalized = true;
	}

	assert(!pthread_mutex_init(&mutex, &attr));
#endif
}

Mutex::~Mutex()
{
#ifdef WIN32		
	DeleteCriticalSection(&m_cs);
#else
	assert(!pthread_mutex_destroy(&mutex));
#endif
}

void Mutex::Acquire()
{
#ifdef WIN32	
	EnterCriticalSection(&m_cs);
#else
	assert(!pthread_mutex_lock(&mutex));
#endif
}

void Mutex::Release()
{
#ifdef WIN32		
	LeaveCriticalSection(&m_cs);
#else
	assert(!pthread_mutex_unlock(&mutex));
#endif
}

bool Mutex::AttemptAcquire()
{
#ifdef WIN32	
	return (TryEnterCriticalSection(&m_cs) == TRUE ? true : false);
#else
	return (pthread_mutex_trylock(&mutex) == 0);
#endif
}