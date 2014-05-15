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

class CThreadPool;

static INLINE uint32 GenerateThreadId()
{
    static std::atomic<uint32> g_threadid_count(1);
    return ++g_threadid_count;
}

class Thread
{
    friend class CThreadPool;
    
public:
	explicit Thread(CThreadPool &rThreadPool, ThreadContext *pExecTarget) : m_rThreadPool(rThreadPool),
                                                                            m_pExecutionTarget(pExecTarget),
                                                                            m_signaled(false),
                                                                            m_threadId(GenerateThreadId())
	{

	}
    
    //thread start func
    static void thread_proc(Thread *t);
    
	void Suspend(std::unique_lock<std::mutex> &rTP_Mutex)
	{
        std::unique_lock<std::mutex> rLock(m_rCondMutex);
        
        //release threadpool mutex now Resume cannot be called before Suspend
        rTP_Mutex.unlock();
        
        //used to avoid spurious wakeups
        m_rCond.wait(rLock, [=]{ return m_signaled.load(); });
        m_signaled = false;
	}
	
	void Resume()
	{
        std::unique_lock<std::mutex> rLock(m_rCondMutex);
        m_signaled = true;
        m_rCond.notify_one();
	}
    
    uint32 GetId() const
	{
		return m_threadId;
	}
    
private:
    CThreadPool                     &m_rThreadPool;
    std::atomic<ThreadContext*>     m_pExecutionTarget;
    std::atomic<bool>               m_signaled;
    uint32                          m_threadId;
    std::condition_variable         m_rCond;
    std::mutex                      m_rCondMutex;
};


class CThreadPool
{
    typedef std::set<Thread*>   ThreadSet;

    friend class Thread;
    
private:
    uint32      m_threadsRequestedSinceLastCheck;
	uint32      m_threadsExitedSinceLastCheck;
	uint32      m_threadsToExit;
	int32       m_threadsEaten;
	std::mutex  m_mutex;

    ThreadSet   m_activeThreads;
	ThreadSet   m_freeThreads;
    
public:
	explicit CThreadPool();
	~CThreadPool();

	// call every 2 minutes or so.
	void IntegrityCheck();

	// call at startup
	void Startup();

	// shutdown all threads
	void Shutdown();
	
	// grabs/spawns a thread, and tells it to execute a task.
	void ExecuteTask(ThreadContext *pExecutionTarget);

	// prints some neat debug stats
	void ShowStats();

private:
	// return true - suspend ourselves, and wait for a future task.
	// return false - exit, we're shutting down or no longer needed.
	bool ThreadExit(std::unique_lock<std::mutex> &rGuard, Thread * t);
    
	// creates a thread, returns a handle to it.
	Thread * StartThread(std::unique_lock<std::mutex> &rGuard, ThreadContext *pExecutionTarget);
    
	// kills x free threads
	void KillFreeThreads(std::unique_lock<std::mutex> &rGuard, uint32 count);
};

extern CThreadPool ThreadPool;

#endif
