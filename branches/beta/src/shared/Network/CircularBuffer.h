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


#ifndef CIRCULARBUFFER_H
#define CIRCUALRBUFFER_H

class CircularBuffer
{
	// allocated whole block pointer
	uint8 * m_buffer;
	uint8 * m_bufferEnd;
	size_t m_bufferSize;

	// region A pointer, and size
	uint8 * m_regionAPointer;
	size_t m_regionASize;

	// region size
	uint8 * m_regionBPointer;
	size_t m_regionBSize;

	// pointer magic!
	INLINE size_t GetAFreeSpace()       { return (m_bufferEnd - m_regionAPointer - m_regionASize); }
	INLINE size_t GetSpaceBeforeA()     { return (m_regionAPointer - m_buffer); }
	INLINE size_t GetSpaceAfterA()      { return (m_bufferEnd - m_regionAPointer - m_regionASize); }
	INLINE size_t GetBFreeSpace()
	{ 
		if(m_regionBPointer == NULL) 
			return 0; 

		return (m_regionAPointer - m_regionBPointer - m_regionBSize); 
	}

public:

	/** Constructor
	*/
	CircularBuffer(size_t size);

	/** Destructor
	*/
	~CircularBuffer();

	/** Read bytes from the buffer
	* @param destination pointer to destination where bytes will be written
	* @param bytes number of bytes to read
	* @return true if there was enough data, false otherwise
	*/
	bool Read(void * destination, size_t bytes);
    
	INLINE void AllocateB()
    {
        m_regionBPointer = m_buffer;
    }

	/** Write bytes to the buffer
	* @param data pointer to the data to be written
	* @param bytes number of bytes to be written
	* @return true if was successful, otherwise false
	*/
	bool Write(const void * data, size_t bytes);

	/** Returns the number of available bytes left.
	*/
	size_t GetSpace();

	/** Returns the number of bytes currently stored in the buffer.
	*/
	INLINE size_t GetSize()
    {
        return m_regionASize + m_regionBSize;
    }

	/** Returns the number of contiguous bytes (that can be pushed out in one operation)
	*/
	INLINE size_t GetContiguiousBytes()
	{
		if(m_regionASize)			// A before B
			return m_regionASize;
		else
			return m_regionBSize;
	}

	/** Removes len bytes from the front of the buffer
	* @param len the number of bytes to "cut"
	*/
	void Remove(size_t len);

	/** Returns a pointer at the "end" of the buffer, where new data can be written
	*/
	INLINE void * GetBuffer()
	{
		if(m_regionBPointer != NULL)
			return m_regionBPointer + m_regionBSize;
		else
			return m_regionAPointer + m_regionASize;
	}

	/** Increments the "writen" pointer forward len bytes
	* @param len number of bytes to step
	*/
	INLINE void IncrementWritten(size_t len)			// known as "commit"
	{
		if(m_regionBPointer != NULL)
			m_regionBSize += len;
		else
			m_regionASize += len;
	}

	/** Returns a pointer at the "beginning" of the buffer, where data can be pulled from
	*/
	INLINE void * GetBufferStart()
	{
		if(m_regionASize > 0)
			return m_regionAPointer;
		else
			return m_regionBPointer;
	}
};

#endif		// _NETLIB_CIRCULARBUFFER_H

