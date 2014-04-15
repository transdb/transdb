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


//#include "Network.h"
#include "../Defines.h"
#include "CircularBuffer.h"

/** Constructor
 */
CircularBuffer::CircularBuffer(size_t size)
{
	m_buffer            = (uint8*)malloc(size);
	m_bufferSize		= size;
	m_bufferEnd			= m_buffer + size;
	m_regionAPointer	= m_buffer;		// reset A to the start
	m_regionBPointer	= NULL;
	m_regionASize		= 0;
	m_regionBSize		= 0;
}

/** Destructor
 */
CircularBuffer::~CircularBuffer()
{
	free(m_buffer);
}

/** Read bytes from the buffer
 * @param destination pointer to destination where bytes will be written
 * @param bytes number of bytes to read
 * @return true if there was enough data, false otherwise
 */
bool CircularBuffer::Read(void * destination, size_t bytes)
{
	// copy as much out of region a
	size_t cnt = bytes;
	size_t aRead = 0, bRead = 0;
	if((m_regionASize + m_regionBSize) < bytes)
		return false;

	// If we have both region A and region B, always "finish" off region A first, as
	// this will contain the "oldest" data
	if(m_regionASize > 0)
	{
		aRead = (cnt > m_regionASize) ? m_regionASize : cnt;
		memcpy(destination, m_regionAPointer, aRead);
		m_regionASize -= aRead;
		m_regionAPointer += aRead;
		cnt -= aRead;
	}

	// Data left over? read the data from buffer B
	if(cnt > 0 && m_regionBSize > 0)
	{
		bRead = (cnt > m_regionBSize) ? m_regionBSize : cnt;
		memcpy((char*)destination + aRead, m_regionBPointer, bRead);
		m_regionBSize -= bRead;
		m_regionBPointer += bRead;
//		cnt -= bRead;
	}

	// is buffer A empty? move buffer B to buffer A, to increase future performance
	if(m_regionASize == 0)
	{
		if(m_regionBSize > 0)
		{
			// push it all to the start of the buffer.
			if(m_regionBPointer != m_buffer)
			{
				memmove(m_buffer, m_regionBPointer, m_regionBSize);
			}

			m_regionAPointer = m_buffer;
			m_regionASize = m_regionBSize;
			m_regionBPointer = NULL;
			m_regionBSize = 0;
		}
		else
		{
			// no data in region b
			m_regionBPointer = NULL;
			m_regionBSize = 0;
			m_regionAPointer = m_buffer;
			m_regionASize = 0;
		}
	}

	return true;
}

/** Write bytes to the buffer
 * @param data pointer to the data to be written
 * @param bytes number of bytes to be written
 * @return true if was successful, otherwise false
 */
bool CircularBuffer::Write(const void * data, size_t bytes)
{
	// If buffer B exists, write to it.
	if(m_regionBPointer != NULL)
	{
		if(GetBFreeSpace() < bytes)
			return false;

		memcpy(&m_regionBPointer[m_regionBSize], data, bytes);
		m_regionBSize += bytes;
		return true;
	}

	// Otherwise, write to buffer A, or initialize buffer B depending on which has more space.
	if(GetAFreeSpace() < GetSpaceBeforeA())
	{
		AllocateB();
		if(GetBFreeSpace() <bytes)
			return false;

		memcpy(&m_regionBPointer[m_regionBSize], data, bytes);
		m_regionBSize += bytes;
		return true;
	}
	else
	{
		if(GetAFreeSpace() < bytes)
			return false;

		memcpy(&m_regionAPointer[m_regionASize], data, bytes);
		m_regionASize += bytes;
		return true;
	}
}

/** Returns the number of available bytes left.
 */
size_t CircularBuffer::GetSpace()
{
	if(m_regionBPointer != NULL)
	{
		return GetBFreeSpace();
	}
	else
	{
		// would allocating buffer B get us more data?
		if(GetAFreeSpace() < GetSpaceBeforeA())
		{
			AllocateB();
			return GetBFreeSpace();
		}

		// or not?
		return GetAFreeSpace();
	}
}

/** Removes len bytes from the front of the buffer
 * @param len the number of bytes to "cut"
 */
void CircularBuffer::Remove(size_t len)
{
	// remove from A first before we remove from b
	size_t cnt = len;
	size_t aRem, bRem;

	// If we have both region A and region B, always "finish" off region A first, as
	// this will contain the "oldest" data
	if(m_regionASize > 0)
	{
		aRem = (cnt > m_regionASize) ? m_regionASize : cnt;
		m_regionASize -= aRem;
		m_regionAPointer += aRem;
		cnt -= aRem;
	}

	// Data left over? cut the data from buffer B
	if(cnt > 0 && m_regionBSize > 0)
	{
		bRem = (cnt > m_regionBSize) ? m_regionBSize : cnt;
		m_regionBSize -= bRem;
		m_regionBPointer += bRem;
//		cnt -= bRem;
	}

	// is buffer A empty? move buffer B to buffer A, to increase future performance
	if(m_regionASize == 0)
	{
		if(m_regionBSize > 0)
		{
			// push it all to the start of the buffer.
			if(m_regionBPointer != m_buffer)
			{
				memmove(m_buffer, m_regionBPointer, m_regionBSize);
			}
	
			m_regionAPointer = m_buffer;
			m_regionASize = m_regionBSize;
			m_regionBPointer = NULL;
			m_regionBSize = 0;
		}
		else
		{
			// no data in region b
			m_regionBPointer = NULL;
			m_regionBSize = 0;
			m_regionAPointer = m_buffer;
			m_regionASize = 0;
		}
	}
}
