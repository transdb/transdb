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

#ifndef BYTEBUFFER_H
#define BYTEBUFFER_H

class ByteBuffer 
{
public:
	const static size_t DEFAULT_SIZE = 256;

	ByteBuffer(): m_rpos(0), m_wpos(0) 
	{
		m_storage.reserve(DEFAULT_SIZE);
	}

	ByteBuffer(size_t res): m_rpos(0), m_wpos(0) 
	{
		m_storage.reserve(res);
	}

	ByteBuffer(const ByteBuffer &buf): m_rpos(buf.m_rpos), m_wpos(buf.m_wpos), m_storage(buf.m_storage) { }

	virtual ~ByteBuffer() {}

	void clear() 
	{
		m_storage.clear();
		m_rpos = m_wpos = 0;
	}

	template <typename T> void append(T value)
	{
		append((uint8*)&value, sizeof(value));
	}

	template <typename T> void put(const size_t &pos, T value)
	{
		put(pos,(uint8*)&value, sizeof(value));
	}

	// stream like operators for storing data
	ByteBuffer &operator<<(const bool &value)
	{
		append<char>((char)value);
		return *this;
	}

	// unsigned
	ByteBuffer &operator<<(const uint8 &value)
	{
		append<uint8>(value);
		return *this;
	}

	ByteBuffer &operator<<(const uint16 &value)
	{
		append<uint16>(value);
		return *this;
	}

	ByteBuffer &operator<<(const uint32 &value)
	{
		append<uint32>(value);
		return *this;
	}

	ByteBuffer &operator<<(const uint64 &value)
	{
		append<uint64>(value);
		return *this;
	}

	// signed as in 2e complement
	ByteBuffer &operator<<(const int8 &value)
	{
		append<int8>(value);
		return *this;
	}

	ByteBuffer &operator<<(const int16 &value)
	{
		append<int16>(value);
		return *this;
	}

	ByteBuffer &operator<<(const int32 &value)
	{
		append<int32>(value);
		return *this;
	}

	ByteBuffer &operator<<(const int64 &value)
	{
		append<int64>(value);
		return *this;
	}

	ByteBuffer &operator<<(const float &value)
	{
		append<float>(value);
		return *this;
	}

	ByteBuffer &operator<<(const double &value)
	{
		append<double>(value);
		return *this;
	}

	ByteBuffer &operator<<(const std::string &value) 
	{
		append((uint8*)value.c_str(), value.length());
		append((uint8)0);
		return *this;
	}

	ByteBuffer &operator<<(const char *str) 
	{
		append((uint8*)str, strlen(str));
		append((uint8)0);
		return *this;
	}

	// stream like operators for reading data
	ByteBuffer &operator>>(bool &value) 
	{
		value = read<char>() > 0 ? true : false;
		return *this;
	}

	//unsigned
	ByteBuffer &operator>>(uint8 &value) 
	{
		value = read<uint8>();
		return *this;
	}

	ByteBuffer &operator>>(uint16 &value) 
	{
		value = read<uint16>();
		return *this;
	}

	ByteBuffer &operator>>(uint32 &value) 
	{
		value = read<uint32>();
		return *this;
	}

	ByteBuffer &operator>>(uint64 &value) 
	{
		value = read<uint64>();
		return *this;
	}

	//signed as in 2e complement
	ByteBuffer &operator>>(int8 &value) 
	{
		value = read<int8>();
		return *this;
	}

	ByteBuffer &operator>>(int16 &value) 
	{
		value = read<int16>();
		return *this;
	}

	ByteBuffer &operator>>(int32 &value) 
	{
		value = read<int32>();
		return *this;
	}

	ByteBuffer &operator>>(int64 &value) 
	{
		value = read<int64>();
		return *this;
	}

	ByteBuffer &operator>>(float &value) 
	{
		value = read<float>();
		return *this;
	}

	ByteBuffer &operator>>(double &value) 
	{
		value = read<double>();
		return *this;
	}

	ByteBuffer &operator>>(std::string& value) 
	{
		value.clear();
		value.reserve(16);
		for(;;)
		{
			char c = read<char>();
			if(c == 0)
				break;

			value += c;
		}
		return *this;
	}

	uint8 operator[](const size_t &pos)
	{
		return read<uint8>(pos);
	}

	INLINE size_t rpos()
	{
		return m_rpos;
	}

	INLINE size_t rpos(const size_t &rpos)
	{
		m_rpos = rpos;
		return m_rpos;
	}

	INLINE size_t wpos() 
	{
		return m_wpos;
	}

	INLINE size_t wpos(const size_t &wpos)
	{
		m_wpos = wpos;
		return m_wpos;
	}

	template <typename T> T read() 
	{
		T r = read<T>(m_rpos);
		m_rpos += sizeof(T);
		return r;
	}

	template <typename T> T read(const size_t &pos) const
	{
		if(pos + sizeof(T) > size())
		{
			return (T)0;
		} 
		else 
		{
			return *((T*)&m_storage[pos]);
		}
	}

	void read(uint8 *dest, const size_t &len)
	{
		if(m_rpos + len <= size()) 
		{
			memcpy(dest, &m_storage[m_rpos], len);
		} 
		else 
		{
			memset(dest, 0, len);
		}
		m_rpos += len;
	}

	INLINE const uint8 *contents() const 
	{ 
		return &m_storage[0]; 
	}

	INLINE size_t size() const
	{ 
		return m_storage.size(); 
	}

	void resize(const size_t &newsize)
	{
		m_storage.resize(newsize);
		m_rpos = 0;
		m_wpos = size();
	}

	void reserve(const size_t &ressize)
	{
		if(ressize > size()) 
		{
			m_storage.reserve(ressize);
		}
	}

	// appending to the end of buffer
	void appendLengthCodedString(const std::string& str) 
	{
		append<uint8>(static_cast<uint8>(str.size()));	//size
		append((uint8*)str.c_str(), str.size());		//non-null terminated string
	}
    
    void appendNonNullTermStr(const char *str, ...)
    {
        char rOut[2048];
        
        va_list ap;
        va_start(ap, str);
        vsnprintf(rOut, sizeof(rOut), str, ap);
        va_end(ap);
        
        append((uint8*)rOut, strlen(rOut));
    }
    
	void append(const std::string & str) 
	{
		append((uint8*)str.c_str(), str.length());
		append<uint8>(0);
	}

	void append(const char * src, const size_t &cnt)
	{
		append((const uint8*)src, cnt);
	}

	void append(const void * src, const size_t &cnt)
	{
		append((const uint8*)src, cnt);
	}

	void append(const uint8 *src, const size_t &cnt)
	{
		if(!cnt) 
			return;

		if(m_storage.size() < m_wpos + cnt)
		{
			m_storage.resize(m_wpos + cnt);
		}

		memcpy(&m_storage[m_wpos], src, cnt);
		m_wpos += cnt;
	}

	void append(const ByteBuffer& buffer) 
	{
		if(buffer.size() > 0) 
		{
			append(buffer.contents(), buffer.size());
		}
	}

	void put(const size_t &pos, const uint8 * src, const size_t &cnt)
	{
		assert(pos + cnt <= size());
		memcpy(&m_storage[pos], src, cnt);
	}

	void hexlike()
	{
		size_t j = 1, k = 1;
		printf("STORAGE_SIZE: %u\n", (unsigned int)size());
		for(size_t i = 0; i < size(); i++)
		{
			if ((i == (j*8)) && ((i != (k*16))))
			{
				if (read<uint8>(i) <= 0x0F)
				{
					printf("| 0%X ", read<uint8>(i));
				}
				else
				{
					printf("| %X ", read<uint8>(i));
				}

				j++;
			}
			else if (i == (k*16))
			{
				rpos(rpos()-16);	// move read pointer 16 places back
				printf(" | ");	  // write split char

				for (int x = 0; x <16; x++)
				{
					printf("%c", read<uint8>(i-16 + x));
				}

				if (read<uint8>(i) <= 0x0F)
				{
					printf("\n0%X ", read<uint8>(i));
				}
				else
				{
					printf("\n%X ", read<uint8>(i));
				}

				k++;
				j++;
			}
			else
			{
				if (read<uint8>(i) <= 0x0F)
				{
					printf("0%X ", read<uint8>(i));
				}
				else
				{
					printf("%X ", read<uint8>(i));
				}
			}
		}
		printf("\n");
	}

	void reverse()
	{
		std::reverse(m_storage.begin(), m_storage.end());
	}

protected:
	// read and write positions
	size_t				m_rpos;
	size_t				m_wpos;
	std::vector<uint8>	m_storage;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename T> ByteBuffer &operator<<(ByteBuffer &b, std::vector<T> v)
{
	b <<(uint32)v.size();
	for (typename std::vector<T>::iterator i = v.begin(); i != v.end(); ++i) {
		b <<*i;
	}
	return b;
}

template <typename T> ByteBuffer &operator>>(ByteBuffer &b, std::vector<T> &v)
{
	uint32 vsize;
	b>> vsize;
	v.clear();
	while(vsize--) {
		T t;
		b>> t;
		v.push_back(t);
	}
	return b;
}

template <typename T> ByteBuffer &operator<<(ByteBuffer &b, std::list<T> v)
{
	b <<(uint32)v.size();
	for (typename std::list<T>::iterator i = v.begin(); i != v.end(); ++i) {
		b <<*i;
	}
	return b;
}

template <typename T> ByteBuffer &operator>>(ByteBuffer &b, std::list<T> &v)
{
	uint32 vsize;
	b>> vsize;
	v.clear();
	while(vsize--) {
		T t;
		b>> t;
		v.push_back(t);
	}
	return b;
}

template <typename K, typename V> ByteBuffer &operator<<(ByteBuffer &b, std::map<K, V> &m)
{
	b <<(uint32)m.size();
	for (typename std::map<K, V>::iterator i = m.begin(); i != m.end(); ++i) {
		b <<i->first <<i->second;
	}
	return b;
}

template <typename K, typename V> ByteBuffer &operator>>(ByteBuffer &b, std::map<K, V> &m)
{
	uint32 msize;
	b>> msize;
	m.clear();
	while(msize--) {
		K k;
		V v;
		b>> k>> v;
		m.insert(make_pair(k, v));
	}
	return b;
}

#endif