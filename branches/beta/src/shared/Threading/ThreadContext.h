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

#ifndef THREADCONTEXT_H
#define THREADCONTEXT_H

class ThreadContext
{
public:
	ThreadContext() 
	{
        Sync_Set(&m_threadRunning, 1);
#ifdef WIN32
		m_hHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
#else       
        pthread_mutex_init(&m_abortmutex, NULL);
        pthread_cond_init(&m_abortcond, NULL);
#endif
	}

	virtual ~ThreadContext() 
	{
        SetThreadName("");        
#ifdef WIN32		
		CloseHandle(m_hHandle);
#else
        pthread_mutex_destroy(&m_abortmutex);
        pthread_cond_destroy(&m_abortcond);    
#endif
	}

	virtual bool run() = 0;
    
	virtual void OnShutdown()
	{ 
		Terminate(); 
	}

	void Terminate() 
	{
        Sync_Set(&m_threadRunning, 0);
#ifndef WIN32        
        pthread_mutex_lock(&m_abortmutex);
        pthread_cond_signal(&m_abortcond);
        pthread_mutex_unlock(&m_abortmutex);
#else
		SetEvent(m_hHandle);
#endif
	}

	long GetThreadRunning() 
    { 
        return m_threadRunning; 
    }
    
    void Wait(long ms)
    {
#ifdef WIN32    
        WaitForSingleObject(m_hHandle, ms);
#else        
        gettimeofday(&m_tnow, NULL);
        m_tv.tv_sec  = m_tnow.tv_sec + ms / 1000;
        m_tv.tv_nsec = m_tnow.tv_usec*1000 + (ms % 1000)*1000000;
        if(m_tv.tv_nsec >= 1000000000)
        {
            m_tv.tv_nsec -= 1000000000;
            m_tv.tv_sec++;
        }        
        
        pthread_mutex_lock(&m_abortmutex);
        pthread_cond_timedwait(&m_abortcond, &m_abortmutex, &m_tv);
        pthread_mutex_unlock(&m_abortmutex);
#endif        
    }

protected:
#ifdef WIN32
	HANDLE			m_hHandle;
#else
    pthread_cond_t  m_abortcond;
	pthread_mutex_t m_abortmutex;
    struct timeval  m_tnow;
    struct timespec m_tv;
#endif
	volatile long	m_threadRunning;
};

#endif

