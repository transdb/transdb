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

#include "ThreadContext.h"

#ifdef WIN32
class ThreadController
{
private:
	HANDLE                  hThread;
	std::atomic<uint32>     thread_id;

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
			printf("lasterror: %u\n", (uint32)le);
		}
	}

	INLINE uint32 GetId() const
	{ 
		return thread_id; 
	}
};

#else

static INLINE uint32 GenerateThreadId()
{
    static std::atomic<uint32> g_threadid_count(1);
    return ++g_threadid_count;
}

class ThreadController
{
private:
    std::atomic<bool>   m_suspended;
	std::atomic<uint32> m_thread_id;
	pthread_cond_t      m_cond;
	pthread_mutex_t     m_mutex;
	pthread_t           m_handle;
    
public:
    explicit ThreadController() : m_suspended(false), m_thread_id(0), m_handle(0)
    {
        
    }
    
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
	
	void Suspend(std::recursive_mutex &rTH_Mutex)
	{
        //lock suspend lock
        assert(!pthread_mutex_lock(&m_mutex));
        //release threadpool mutex now Resume cannot be called before Suspend
        rTH_Mutex.unlock();
        
        m_suspended = true;
        do
        {
            pthread_cond_wait(&m_cond, &m_mutex);
            //
        }while(m_suspended);
        assert(!pthread_mutex_unlock(&m_mutex));
	}
	
	void Resume()
	{
        assert(!pthread_mutex_lock(&m_mutex));
        m_suspended = false;
		pthread_cond_signal(&m_cond);
        assert(!pthread_mutex_unlock(&m_mutex));
	}
	
	INLINE uint32 GetId() const
    {        
        return m_thread_id;
    }
};
#endif

//class Worker
//{
//public:
//    Worker(ThreadContext *pExecTarget) : m_pThreadContext(pExecTarget)
//    {
//        
//    }
//    
//    void Suspend()
//    {
//        std::unique_lock<std::mutex> rLock(m_rCondMutex);
//        m_rCond.wait(rLock);
//    }
//    
//    void Resume()
//    {
//        m_rCond.notify_one();
//    }
//    
//    std::thread::id GetId() const
//	{
//		return std::this_thread::get_id();
//	}
//    
//private:
//    ThreadContext               *m_pThreadContext;
//    ThreadController            m_rControlInterface;
//    std::condition_variable     m_rCond;
//    std::mutex                  m_rCondMutex;
//};

struct Thread
{
	explicit Thread(ThreadContext * ExecTarget) : m_pExecutionTarget(ExecTarget)
	{
	}
    
    std::atomic<ThreadContext*>     m_pExecutionTarget;
    ThreadController                m_rControlInterface;
    std::mutex                      m_rSetupMutex;
};

typedef std::set<Thread*> ThreadSet;

class CThreadPool
{
private:
    std::atomic<uint32>     m_threadsRequestedSinceLastCheck;
	std::atomic<uint32>     m_threadsFreedSinceLastCheck;
	std::atomic<uint32>     m_threadsExitedSinceLastCheck;
	std::atomic<uint32>     m_threadsToExit;
	std::atomic<int32>      m_threadsEaten;
	std::recursive_mutex    m_mutex;

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
        std::lock_guard<std::recursive_mutex> rGuard(m_mutex);
        m_threadsEaten = (int32)m_freeThreads.size();
    }

	// gets active thread count
	uint32 GetActiveThreadCount()
    {
        std::lock_guard<std::recursive_mutex> rGuard(m_mutex);
        return (uint32)m_activeThreads.size();
    }

	// gets free thread count
	uint32 GetFreeThreadCount() 
    {
        std::lock_guard<std::recursive_mutex> rGuard(m_mutex);
        return (uint32)m_freeThreads.size();
    }
    
    //gets mutex
    INLINE std::recursive_mutex &GetMutex()
    {
        return m_mutex;
    }
};

extern CThreadPool ThreadPool;

#endif
