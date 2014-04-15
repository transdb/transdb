//
//  Vector.h
//  TransDB
//
//  Created by Miroslav Kudrnac on 20.01.14.
//  Copyright (c) 2014 Miroslav Kudrnac. All rights reserved.
//

#ifndef TransDB_Vector_h
#define TransDB_Vector_h

template<class T, class SizeT = size_t>
class Vector
{
public:
    typedef T*              iterator;
    typedef T*              value_type;
    typedef SizeT           size_type;
    static const size_type	npos = static_cast<size_type>(-1);
	static const size_type	granularity = 16;
    
	//constructor
    Vector() : m_pBuff(NULL), 
			   m_size(0), 
			   m_capacity(0)
    {

    }
    
	//constructor
    Vector(const SizeT &initialSize) : m_pBuff(NULL),
                                       m_size(0),
                                       m_capacity(0)
    {
        reserve(initialSize);
    }
    
	//copy constructor
    Vector(const Vector<T, SizeT> &v) : m_pBuff(NULL), 
										m_size(0), 
										m_capacity(0)
    { 
		*this = v;
    }

	//moveable constructor
	Vector(Vector<T, SizeT> &&v) : m_pBuff(NULL), 
								   m_size(0), 
								   m_capacity(0)
	{
		*this = std::move(v);	
	}

    ~Vector()
    {
        clear();
    }
    
    INLINE iterator begin()
    {
        return m_pBuff;
    }
    
    INLINE iterator end()
    {
        return m_pBuff + m_size;
    }
    
    INLINE void push_back(const T& value)
    {   
        if(m_capacity == m_size)
        {
            size_type newCapacity = m_capacity == 0 ? granularity : m_capacity * 2;
            reserve(newCapacity);
        }
        
        m_pBuff[m_size] = value;
        ++m_size;
    }
    
    INLINE void reserve(const size_type &newCapacity)
    {
        assert(newCapacity >= 0);
        
        //only if new size is bigger than capacity
        if(newCapacity > m_capacity)
        {
            //set capacity
            m_capacity = newCapacity;
            // resize buffer
            m_pBuff = (T*)realloc(m_pBuff, sizeof(T) * m_capacity);
        }
    }
    
    INLINE void resize(const size_type &newSize)
    {
        reserve(newSize);
        m_size = newSize;
    }
    
    INLINE void clear()
    {
        if(m_pBuff)
        {
            free(m_pBuff);
            m_pBuff = NULL;
        }
        m_size      = 0;
        m_capacity  = 0;
    }
        
    INLINE size_type capacity() const
    {
        return m_capacity;
    }
    
    INLINE size_type size() const
    {
        return m_size;
    }
    
    INLINE bool empty()
    {
        return m_size == 0;
    }
    
    INLINE T &front()
    {
        return m_pBuff[0];
    }
    
    INLINE T &back()
    {
        return m_pBuff[m_size - 1];
    }
    
    INLINE void pop_back()
    {
        --m_size;
    }
    
	INLINE T &operator[](const std::size_t &index)
    {
        assert(index >= 0);
        assert(index < m_size);
        return m_pBuff[index];
    }

    INLINE const T &operator[](const std::size_t &index) const
    {
		return const_cast<Vector<T>&>(*this)[index];
    }
    
    INLINE Vector<T, SizeT> &operator=(const Vector<T, SizeT> &v)
    {
		if(this != &v)
		{
			clear();
			//copy
			m_size = v.m_size;
			m_capacity = v.m_capacity;
			//copy buff
			if(m_capacity)
			{
				m_pBuff = (T*)malloc(sizeof(T) * m_capacity);
				assert(m_pBuff);
				memcpy(m_pBuff, v.m_pBuff, sizeof(T) * m_size);
			}
		}
        return *this;
    }

	INLINE Vector<T, SizeT>& operator=(Vector<T, SizeT> &&v)
	{
		if(this != &v)
		{
			clear();
			//swap
			std::swap(m_size, v.m_size);
			std::swap(m_capacity, v.m_capacity);
			std::swap(m_pBuff, v.m_pBuff);
		}
		return *this;
	}
    
private:    
    T           *m_pBuff;
    SizeT       m_size;
    SizeT       m_capacity;
};


template <typename T>
INLINE static T _S_read(Vector<uint8> &rVector, size_t &rpos)
{
    T r;
    if(rpos + sizeof(T) > rVector.size())
        r = (T)0;
    else
        r = *((T*)&rVector[rpos]);
    rpos += sizeof(T);
    return r;
}

#endif
