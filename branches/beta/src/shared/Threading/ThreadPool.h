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

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include "Mutex.h"
#include "ThreadContext.h"

#ifdef WIN32
class ThreadController
{
private:
	HANDLE hThread;
	uint32 thread_id;

public:
	ThreadController() : hThread(NULL), thread_id(0)
	{
	}

	~ThreadController()
	{
		CloseHandle(hThread);
	}

	void Setup(HANDLE h, uint32 threadid)
	{
		hThread = h;
		thread_id = threadid;
	}

	void Suspend()
	{
		// We can't be suspended by someone else. That is a big-no-no and will lead to crashes.
		assert(GetCurrentThreadId() == thread_id);
        SuspendThread(hThread);
	}

	void Resume()
	{
		// This SHOULD be called by someone else.
		assert(GetCurrentThreadId() != thread_id);
		if(!ResumeThread(hThread))
		{
			DWORD le = GetLastError();
			printf("lasterror: %u\n", le);
		}
	}

	INLINE uint32 GetId() 
	{ 
		return thread_id; 
	}
};

#else

long GenerateThreadId();
	
class ThreadController
{
private:
	long                m_thread_id;
	pthread_cond_t      m_cond;
	pthread_mutex_t     m_mutex;
	pthread_t           m_handle;
    
public:
	void Setup(pthread_t h)
	{
		m_handle = h;
		pthread_mutex_init(&m_mutex, NULL);
		pthread_cond_init(&m_cond, NULL);
		m_thread_id = GenerateThreadId();
	}
	
	~ThreadController()
	{
		pthread_mutex_destroy(&m_mutex);
		pthread_cond_destroy(&m_cond);
	}
	
	void Suspend()
	{
        pthread_mutex_lock(&m_mutex);
		pthread_cond_wait(&m_cond, &m_mutex);
        pthread_mutex_unlock(&m_mutex);
	}
	
	void Resume()
	{
        pthread_mutex_lock(&m_mutex);
		pthread_cond_signal(&m_cond);
        pthread_mutex_unlock(&m_mutex);
	}
	
	INLINE uint32 GetId()
    {
        return (uint32)m_thread_id;
    }
};
#endif

struct Thread
{
	Thread(ThreadContext * ExecTarget) : m_pExecutionTarget(ExecTarget)
	{
	}

	ThreadContext       *m_pExecutionTarget;
	ThreadController    m_rControlInterface;
	Mutex               m_rSetupMutex;
};

typedef std::set<Thread*> ThreadSet;
typedef std::list<Thread*> ThreadList;

class CThreadPool
{
private:
	int GetNumCpus();

	volatile uint32 _threadsRequestedSinceLastCheck;
	volatile uint32 _threadsFreedSinceLastCheck;
	volatile uint32 _threadsExitedSinceLastCheck;
	volatile uint32 _threadsToExit;
	volatile int32 _threadsEaten;
	Mutex _mutex;

    ThreadSet m_activeThreads;
	ThreadSet m_freeThreads;
	
#ifdef WIN32
	HANDLE hHandle;
#endif

public:
	CThreadPool();
	~CThreadPool();

	// call every 2 minutes or so.
	void IntegrityCheck();

	// call at startup
	void Startup();

	// shutdown all threads
	void Shutdown();
	
	// return true - suspend ourselves, and wait for a future task.
	// return false - exit, we're shutting down or no longer needed.
	bool ThreadExit(Thread * t);

	// creates a thread, returns a handle to it.
	Thread * StartThread(ThreadContext * ExecutionTarget);

	// grabs/spawns a thread, and tells it to execute a task.
	void ExecuteTask(ThreadContext * ExecutionTarget);

	// prints some neat debug stats
	void ShowStats();

	// kills x free threads
	void KillFreeThreads(uint32 count);

	// resets the gobble counter
	void Gobble() 
    { 
        _mutex.Acquire();
        _threadsEaten = (int32)m_freeThreads.size(); 
        _mutex.Release();
    }

	// gets active thread count
	uint32 GetActiveThreadCount() 
    { 
        uint32 ret;
        _mutex.Acquire();
        ret = (uint32)m_activeThreads.size();
        _mutex.Release();
        return ret; 
    }

	// gets free thread count
	uint32 GetFreeThreadCount() 
    {
        uint32 ret;
        _mutex.Acquire();
        ret = (uint32)m_freeThreads.size();
        _mutex.Release();
        return ret; 
    }
    
    //gets mutex
    INLINE Mutex &GetMutex()
    {
        return _mutex;
    }
};

extern CThreadPool ThreadPool;

#endif
