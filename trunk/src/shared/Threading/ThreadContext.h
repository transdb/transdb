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
	ThreadContext() : m_threadRunning(true)
	{

	}

	virtual ~ThreadContext()
	{
        SetThreadName("");
	}

	virtual bool run() = 0;
    
	virtual void OnShutdown()
	{ 
		Terminate(); 
	}

	void Terminate() 
	{
        std::unique_lock<std::mutex> rLock(m_rCondMutex);
        m_threadRunning = false;
        m_rCond.notify_one();
	}

	bool GetThreadRunning() const
    { 
        return m_threadRunning; 
    }
    
    void Wait(long ms)
    {
        std::unique_lock<std::mutex> rLock(m_rCondMutex);
        m_rCond.wait_for(rLock, std::chrono::milliseconds(ms));
    }

protected:
    std::condition_variable m_rCond;
    std::mutex              m_rCondMutex;
	std::atomic<bool>       m_threadRunning;
};

#endif

