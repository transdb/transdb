/*
 * Thread Pool Class
 * Copyright (C) Burlex <burlex@gmail.com>
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
#include "../Logs/Log.h"

#ifndef WIN32
	
	volatile long g_threadid_count = 0;
	long GenerateThreadId()
	{
        return Sync_Add(&g_threadid_count);
	}

#endif

#define THREAD_RESERVE 4
CThreadPool ThreadPool;

CThreadPool::CThreadPool()
{
	_threadsExitedSinceLastCheck	= 0;
	_threadsRequestedSinceLastCheck = 0;
	_threadsEaten					= 0;
	_threadsFreedSinceLastCheck		= 0;
	_threadsToExit					= 0;

#ifdef WIN32	
	hHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
#endif
}

CThreadPool::~CThreadPool()
{
#ifdef WIN32		
	CloseHandle(hHandle);
#endif
}

bool CThreadPool::ThreadExit(Thread * t)
{
	_mutex.Acquire();
	
	// we're definitely no longer active
	m_activeThreads.erase(t);

	// do we have to kill off some threads?
	if(_threadsToExit > 0)
	{
		// kill us.
		--_threadsToExit;
		++_threadsExitedSinceLastCheck;

        //delete from freeThreads
        m_freeThreads.erase(t);

		_mutex.Release();
		delete t;
		return false;
	}

	// enter the "suspended" pool
	++_threadsExitedSinceLastCheck;
	++_threadsEaten;

	ThreadSet::iterator itr = m_freeThreads.find(t);
	if(itr != m_freeThreads.end())
	{
		Log.Error("ThreadPool", "Thread %u duplicated with thread %u\n", (*itr)->m_rControlInterface.GetId(), t->m_rControlInterface.GetId());
	}
	m_freeThreads.insert(t);
	
	Log.Debug("ThreadPool", "Thread %u entered the free pool.", t->m_rControlInterface.GetId());
	_mutex.Release();

	return true;
}

void CThreadPool::ExecuteTask(ThreadContext * ExecutionTarget)
{
	Thread * t;
	_mutex.Acquire();
	++_threadsRequestedSinceLastCheck;
	--_threadsEaten;

	// grab one from the pool, if we have any.
	if(m_freeThreads.size())
	{
		t = *m_freeThreads.begin();
		m_freeThreads.erase(t);

		// execute the task on this thread.
		t->m_pExecutionTarget = ExecutionTarget;

		// resume the thread, and it should start working.
		t->m_rControlInterface.Resume();
		Log.Debug("ThreadPool", "Thread %u left the thread pool.", t->m_rControlInterface.GetId());
	}
	else
	{
		// creating a new thread means it heads straight to its task.
		// no need to resume it :)
		t = StartThread(ExecutionTarget);
	}

	// add the thread to the active set
	Log.Debug("ThreadPool", "Thread %u is now executing task at %p.", t->m_rControlInterface.GetId(), ExecutionTarget);

	m_activeThreads.insert(t);
	_mutex.Release();
}

void CThreadPool::Startup()
{
	int i;
	int tcount = THREAD_RESERVE;

	for(i = 0; i < tcount; ++i)
    {
		StartThread(NULL);
    }

	Log.Debug("ThreadPool", "Startup, launched %u threads.", tcount);
}

void CThreadPool::ShowStats()
{
	_mutex.Acquire();
	Log.Debug(__FUNCTION__, "============ ThreadPool Status =============");
	Log.Debug(__FUNCTION__, "Active Threads: %u", m_activeThreads.size());
	Log.Debug(__FUNCTION__, "Suspended Threads: %u", m_freeThreads.size());
	Log.Debug(__FUNCTION__, "Requested-To-Freed Ratio: %.3f%% (%u/%u)", float(float(_threadsRequestedSinceLastCheck+1) / float(_threadsExitedSinceLastCheck+1) * 100.0f), _threadsRequestedSinceLastCheck, _threadsExitedSinceLastCheck);
	Log.Debug(__FUNCTION__, "Eaten Count: %d (negative is bad!)", _threadsEaten);
	Log.Debug(__FUNCTION__, "============================================");
	_mutex.Release();
}

void CThreadPool::IntegrityCheck()
{
	_mutex.Acquire();
	int32 gobbled = _threadsEaten;

    if(gobbled < 0)
	{
		// this means we requested more threads than we had in the pool last time.
        // spawn "gobbled" + THREAD_RESERVE extra threads.
		uint32 new_threads = abs(gobbled) + THREAD_RESERVE;
		_threadsEaten=0;

		for(uint32 i = 0; i < new_threads; ++i)
			StartThread(NULL);

		Log.Debug("ThreadPool", "IntegrityCheck: (gobbled < 0) Spawning %u threads.", new_threads);
	}
	else if(gobbled < THREAD_RESERVE)
	{
        // this means while we didn't run out of threads, we were getting damn low.
		// spawn enough threads to keep the reserve amount up.
		uint32 new_threads = (THREAD_RESERVE - gobbled);
		for(uint32 i = 0; i <new_threads; ++i)
			StartThread(NULL);

		Log.Debug("ThreadPool", "IntegrityCheck: (gobbled <= 5) Spawning %u threads.", new_threads);
	}
	else if(gobbled > THREAD_RESERVE)
	{
		// this means we had "excess" threads sitting around doing nothing.
		// lets kill some of them off.
		uint32 kill_count = (gobbled - THREAD_RESERVE);
		KillFreeThreads(kill_count);
		_threadsEaten -= kill_count;
		Log.Debug("ThreadPool", "IntegrityCheck: (gobbled > 5) Killing %u threads.", kill_count);
	}
	else
	{
		// perfect! we have the ideal number of free threads.
		Log.Debug("ThreadPool", "IntegrityCheck: Perfect!");
	}

	_threadsExitedSinceLastCheck = 0;
	_threadsRequestedSinceLastCheck = 0;
	_threadsFreedSinceLastCheck = 0;

	_mutex.Release();
}

void CThreadPool::KillFreeThreads(uint32 count)
{
	Log.Debug("ThreadPool", "Killing %u excess threads.", count);
	_mutex.Acquire();
	Thread * t;
	ThreadSet::iterator itr;
	uint32 i;
	for(i = 0, itr = m_freeThreads.begin(); i < count && itr != m_freeThreads.end(); ++i, ++itr)
	{
		t = *itr;
		t->m_pExecutionTarget = NULL;
		++_threadsToExit;
		t->m_rControlInterface.Resume();
	}
	_mutex.Release();
}

void CThreadPool::Shutdown()
{
	_mutex.Acquire();
	size_t tcount = m_activeThreads.size() + m_freeThreads.size();		// exit all
	Log.Debug("ThreadPool", "Shutting down %u threads.", tcount);
	KillFreeThreads((uint32)m_freeThreads.size());
	_threadsToExit += (uint32)m_activeThreads.size();

	for(ThreadSet::iterator itr = m_activeThreads.begin(); itr != m_activeThreads.end(); ++itr)
	{
		if((*itr)->m_pExecutionTarget)
		{
			(*itr)->m_pExecutionTarget->OnShutdown();
		}
	}
	_mutex.Release();

	for(;;)
	{
		_mutex.Acquire();
		if(m_activeThreads.size() || m_freeThreads.size())
		{
			Log.Debug("ThreadPool", "%u threads remaining...", m_activeThreads.size() + m_freeThreads.size());
			_mutex.Release();
#ifdef WIN32				
			WaitForSingleObject(hHandle, 100);
#else
			Sleep(100);
#endif
			continue;
		}

		_mutex.Release();
		break;
	}
}

#ifdef WIN32

unsigned int WINAPI thread_proc(void* param)
{
	Thread * t = (Thread*)param;

	t->m_rSetupMutex.Acquire();
	uint32 tid = t->m_rControlInterface.GetId();
	bool ht = (t->m_pExecutionTarget != NULL);
	t->m_rSetupMutex.Release();

	Log.Debug("ThreadPool", "Thread %u started.", t->m_rControlInterface.GetId());

	for(;;)
	{
		if(t->m_pExecutionTarget != NULL)
		{
			if(t->m_pExecutionTarget->run())
			{
				delete t->m_pExecutionTarget;
			}

			t->m_pExecutionTarget = NULL;
		}

		if(!ThreadPool.ThreadExit(t))
		{
			Log.Debug("ThreadPool", "Thread %u exiting.", tid);
			break;
		}
		else
		{
			if(ht)
			{
				Log.Debug("ThreadPool", "Thread %u waiting for a new task.", tid);
			}

			// enter "suspended" state. when we return, the threadpool will either tell us to fuk off, or to execute a new task.
			t->m_rControlInterface.Suspend();
			// after resuming, this is where we will end up. start the loop again, check for tasks, then go back to the threadpool.
		}
	}

	_endthreadex(0);
	return 0;
}

Thread * CThreadPool::StartThread(ThreadContext * ExecutionTarget)
{
	HANDLE h;
	uint32 threadid;
	Thread * t = new Thread(ExecutionTarget);
	
	t->m_rSetupMutex.Acquire();
	h = (HANDLE)_beginthreadex(NULL, 0, &thread_proc, (LPVOID)t, 0, &threadid);
	t->m_rControlInterface.Setup(h, threadid);
	t->m_rSetupMutex.Release();

	return t;
}

#else

static void * thread_proc(void * param)
{
	Thread * t = (Thread*)param;

	t->m_rSetupMutex.Acquire();
	uint32 tid = t->m_rControlInterface.GetId();
	bool ht = (t->m_pExecutionTarget != NULL);
	t->m_rSetupMutex.Release();

	Log.Debug("ThreadPool", "Thread %u started.", t->m_rControlInterface.GetId());
	
	for(;;)
	{
		if(t->m_pExecutionTarget != NULL)
		{
			if(t->m_pExecutionTarget->run())
			{
				delete t->m_pExecutionTarget;
			}

			t->m_pExecutionTarget = NULL;
		}

		if(!ThreadPool.ThreadExit(t))
		{
			Log.Debug("ThreadPool", "Thread %u exiting.", tid);
			break;
		}
		else
		{
			if(ht)
			{
				Log.Debug("ThreadPool", "Thread %u waiting for a new task.", tid);
			}

			// enter "suspended" state. when we return, the threadpool will either tell us to fuk off, or to execute a new task.
			t->m_rControlInterface.Suspend();
			// after resuming, this is where we will end up. start the loop again, check for tasks, then go back to the threadpool.
		}
	}

	pthread_exit(NULL);
    return NULL;
}

Thread * CThreadPool::StartThread(ThreadContext * ExecutionTarget)
{
	pthread_t target;
	Thread * t = new Thread(ExecutionTarget);

	// lock the main mutex, to make sure id generation doesn't get messed up
	_mutex.Acquire();
	t->m_rSetupMutex.Acquire();
	pthread_create(&target, NULL, &thread_proc, (void*)t);
	pthread_detach(target);
	t->m_rControlInterface.Setup(target);
	t->m_rSetupMutex.Release();
	_mutex.Release();
	return t;
}

#endif