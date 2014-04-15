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

#ifndef STACKBUFFER_H
#define STACKBUFFER_H

#include "../Defines.h"

class StackBuffer
{
protected:
	uint8   *m_stackBuffer;
	uint8   *m_bufferPointer;
	uint8   *m_heapBuffer;
    size_t  m_readPos;
    size_t  m_writePos;
	size_t  m_space;

public:

	/** Constructor, sets buffer pointers and zeros write/read positions.
	 */
	StackBuffer(uint8* ptr, size_t sz) : m_stackBuffer(ptr), m_bufferPointer(&m_stackBuffer[0]), m_heapBuffer(0), m_readPos(0), m_writePos(0), m_space(sz) {}

	/** Destructor, frees heap buffer if it exists
	 */
	~StackBuffer() { if(m_heapBuffer) free(m_heapBuffer); }

	/** Re-allocates the buffer on the heap. This allows it to expand past the original specified size.
	 * This is only a failsafe and should be avoided at all costs, as it is quite heavy. 
	 */
	void ReallocateOnHeap()
	{
		printf("!!!!!!! WARNING! STACK BUFFER OVERFLOW !!!!!!!!!!!!!\n");
		if(m_heapBuffer)			// Reallocate with 200 bytes larger size
		{
			m_heapBuffer = (uint8*)realloc(m_heapBuffer, 200 + m_space);
			m_bufferPointer = m_heapBuffer;
			m_space += 200;
		}
		else
		{
			m_heapBuffer = (uint8*)malloc(m_space + 200);
			memcpy(m_heapBuffer, m_stackBuffer, m_writePos);
			m_space += 200;
			m_bufferPointer = m_heapBuffer;
		}
	}

	/** Gets the buffer pointer
	 * @return the buffer pointer
	 */
	INLINE const uint8 * GetBufferPointer() const
    {
        return m_bufferPointer;
    }

	/** Gets the buffer size.
	 */
	INLINE size_t GetBufferSize()
    {
        return m_writePos;
    }

	/** Reads sizeof(T) bytes from the buffer
	 * @return the bytes read
	 */
	template<typename T>
    INLINE T Read()
	{
		if(m_readPos + sizeof(T) <= m_writePos)
			return (T)0;
		T ret = *(T*)&m_bufferPointer[m_readPos];
		m_readPos += sizeof(T);
		return ret;
	}

	/** Writes sizeof(T) bytes to the buffer, while checking for overflows.
	 * @param T data The data to be written
	 */
	template<typename T>
    INLINE void Write(T data)
	{
		if(m_writePos + sizeof(T) > m_space)
		{
			// Whoooops. We have to reallocate on the heap.
			ReallocateOnHeap();
		}

		*(T*)&m_bufferPointer[m_writePos] = data;
		m_writePos += sizeof(T);
	}

	/** writes x bytes to the buffer, while checking for overflows
	 * @param ptr the data to be written
	 * @param size byte count
	 */
	INLINE void Write(uint8 * data, size_t size)
	{
		if(m_writePos + size > m_space)
			ReallocateOnHeap();

		memcpy(&m_bufferPointer[m_writePos], data, size);
		m_writePos += size;
	}

	/** Ensures the buffer is big enough to fit the specified number of bytes.
	 * @param bytes number of bytes to fit
	 */
	void EnsureBufferSize(size_t Bytes)
	{
		if(m_writePos + Bytes > m_space)
			ReallocateOnHeap();
	}

	/** These are the default read/write operators.
	 */
#define DEFINE_BUFFER_READ_OPERATOR(type) void operator >> (type& dest) { dest = Read<type>(); }
#define DEFINE_BUFFER_WRITE_OPERATOR(type) void operator << (type src) { Write<type>(src); }

	/** Fast read/write operators without using the templated read/write functions.
	 */
#define DEFINE_FAST_READ_OPERATOR(type, size) StackBuffer& operator >> (type& dest) { if(m_readPos + size > m_writePos) { dest = (type)0; return *this; } else { dest = *(type*)&m_bufferPointer[m_readPos]; m_readPos += size; return *this; } }
#define DEFINE_FAST_WRITE_OPERATOR(type, size) StackBuffer& operator << (type src) { if(m_writePos + size > m_space) { ReallocateOnHeap(); } *(type*)&m_bufferPointer[m_writePos] = src; m_writePos += size; return *this; }

	/** Integer/float r/w operators
	 */
	DEFINE_FAST_READ_OPERATOR(uint64, sizeof(uint64));
	DEFINE_FAST_READ_OPERATOR(uint32, sizeof(uint32));
	DEFINE_FAST_READ_OPERATOR(uint16, sizeof(uint16));
	DEFINE_FAST_READ_OPERATOR(uint8, sizeof(uint8));
	DEFINE_FAST_READ_OPERATOR(int64, sizeof(int64));
	DEFINE_FAST_READ_OPERATOR(int32, sizeof(int32));
	DEFINE_FAST_READ_OPERATOR(int16, sizeof(int16));
	DEFINE_FAST_READ_OPERATOR(int8, sizeof(int8));
	DEFINE_FAST_READ_OPERATOR(float, sizeof(float));
	DEFINE_FAST_READ_OPERATOR(double, sizeof(double));

	DEFINE_FAST_WRITE_OPERATOR(uint64, sizeof(uint64));
	DEFINE_FAST_WRITE_OPERATOR(uint32, sizeof(uint32));
	DEFINE_FAST_WRITE_OPERATOR(uint16, sizeof(uint16));
	DEFINE_FAST_WRITE_OPERATOR(uint8, sizeof(uint8));
	DEFINE_FAST_WRITE_OPERATOR(int64, sizeof(int64));
	DEFINE_FAST_WRITE_OPERATOR(int32, sizeof(int32));
	DEFINE_FAST_WRITE_OPERATOR(int16, sizeof(int16));
	DEFINE_FAST_WRITE_OPERATOR(int8, sizeof(int8));
	DEFINE_FAST_WRITE_OPERATOR(float, sizeof(float));
	DEFINE_FAST_WRITE_OPERATOR(double, sizeof(double));

	/** boolean (1-byte) read/write operators
	 */
	DEFINE_FAST_WRITE_OPERATOR(bool, 1);
	StackBuffer& operator >> (bool & dst) { dst = (Read<char>() > 0 ? true : false); return *this; }

	/** string (null-terminated) operators
	 */
	StackBuffer& operator << (std::string & value) 
	{ 
		EnsureBufferSize(value.length() + 1); 
		memcpy(&m_bufferPointer[m_writePos], value.c_str(), value.length()+1); m_writePos += (value.length() + 1);
		return *this; 
	}

	StackBuffer& operator >> (std::string & dest)
	{
		dest.clear();
		char c;
		for(;;)
		{
			c = Read<char>();
			if(c == 0) break;
			dest += c;
		}
		return *this;
	}

	StackBuffer& operator << (const char *value)
	{
		size_t len = strlen(value);
		EnsureBufferSize(len + 1);
		memcpy(&m_bufferPointer[m_writePos], value, len+1);
		m_writePos += (len + 1);
		return *this;
	}

	/** Clears the buffer
	 */
	INLINE void Clear()
    {
        m_writePos = m_readPos = 0;
    }

	/** Gets the write position
	 * @return buffer size
	 */
	INLINE size_t GetSize() const
    {
        return m_writePos;
    }
};

#endif
